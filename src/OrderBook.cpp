
// T1: support non-crossing buy insert, cancel, best_bid/ask, depth_at
#include "OrderBook.hpp"   // btw these are manually typed comments
#include <map>             // ordered price -> agg_qty
#include <list>
#include <unordered_map>   // id -> ActiveOrder
#include <optional>


namespace {

    /**
     * @brief Minimal per-order metadata tracked while an order rests.
     *
     * Used to (a) identify the side/price of the order and
     * (b) compute how much to remove from a level on cancel,
     * (c) preserve FIFO via the original submission timestamp.
     */
    struct ActiveOrder {
        Side     side;         ///< Side::Buy or Side::Sell
        int64_t  px_ticks;     ///< Price of the order (in ticks)
        int64_t  remaining_qty;///< Unfilled quantity; removed from level on cancel
        uint64_t ts;           ///< Submission timestamp for price-time priority (FIFO)
    };

    /**
     * @brief A single resting order node within a price level's FIFO queue.
     *
     */
    struct OrderNode {
        uint64_t order_id;     ///< Engine-assigned unique order id
        int64_t  remaining_qty;///< Unfilled quantity for this node
        uint64_t ts;           ///< Submission timestamp (earlier = higher FIFO priority)
    };

    /**
     * @brief One price level on one side of the book.
     *
     * Maintains:
     *  - @c aggregate_qty : sum of all node quantities at this price.
     *  - @c queue         : FIFO of resting orders at this price, oldest at front.
     *
     * @note We use @c std::list for stable iterators and O(1) erase given an iterator.
     *       This property enables true O(1) cancel when coupled with stored iterators.
     */
    struct Level {
        int64_t aggregate_qty = 0;       ///< Sum of remaining_qty for all nodes at this price
        std::list<OrderNode> queue;      ///< FIFO queue of resting orders (price-time priority)
    };

    /**
     * @brief O(1) locator for a specific resting order.
     *
     * Stores the exact iterators needed to:
     *  - locate the price level in the side map, and
     *  - locate the order node inside that level's FIFO queue.
     *
     * With this handle, cancellation avoids both map lookup and queue scans.
     *
     * @warning Iterators are invalidated if the referenced element is erased.
     *          Code that erases must ensure handles to that element are not used afterward.
     */
    struct Handle {
        Side side;                                      ///< Side where the order rests
        std::map<int64_t, Level>::iterator level_it;    ///< Iterator to (price -> Level) entry
        std::list<OrderNode>::iterator      order_it;   ///< Iterator to the specific OrderNode in the Level queue
    };

    /**
     * @brief Entire in-memory state of the order book.
     *
     * Layout:
     *  - @c bids : price -> Level, ordered ascending (highest price is @c std::prev(bids.end()))
     *  - @c asks : price -> aggregate quantity (Level elided until DS step finishes)
     *  - @c id_to_handle : order_id -> Handle for O(1) cancel
     *  - @c next_id : monotonic id generator for new submissions
     *
     * @invariant For every price present in @c bids, @c Level::aggregate_qty equals
     *            the sum of @c remaining_qty over all nodes in @c Level::queue.
     *
     * @complexity
     *  - Level lookup/creation: O(log L) where L is number of price levels on that side.
     *  - Cancel by id: O(1) (amortized), given valid Handle iterators.
     *
     * @iterator_validity
     *  - @c std::map iterators remain valid across insert/erase of different keys
     *    (only the erased element’s iterator is invalidated).
     *  - @c std::list iterators remain valid across insert/erase elsewhere; erasing the
     *    node pointed to by @c order_it invalidates that iterator only.
     */
    struct BookState {
        std::map<int64_t, Level>             bids;          ///< Bid side: price -> Level (aggregate + FIFO)
        std::map<int64_t, int64_t>           asks;          ///< Ask side: price -> aggregate (FIFO added in later DS step)
        std::unordered_map<uint64_t, Handle> id_to_handle;  ///< O(1) jump to a resting order’s exact location
        uint64_t                             next_id = 1;   ///< Next engine-generated order id
    };

    /**
     * @brief Singleton accessor for the process-local book state.
     *
     * @return Reference to the single @c BookState instance.
     *
     * @note Returning by reference avoids copying and ensures mutations persist.
     *       The static instance is constructed on first call (thread-unsafe lazy init).
     */
    BookState& S() {
        static BookState s;   // constructed once on first use
        return s;             // subsequent calls return the same instance
    }

} // end anonymous namespace


AddLimitResult OrderBook::add_limit(Side side, int64_t px_ticks, int64_t qty, uint64_t ts) {
    AddLimitResult out{0, {}}; // order_id = 0, trades = {}. default that reprsents failure
    if (qty <= 0 || px_ticks < 0) return out; // invalid trades
    auto& st = S(); // Access the single BookState (internal memory)

    // non-crossing BUY insert -> create node in a Level
    if (side == Side::Buy) {
        const uint64_t id = st.next_id++;  //engine generated ID
        out.order_id = id;
        // Performs single lookup/insert in O(log L)
        // If p_ticks exists: returns an iterator to existing price->Level pair, sets inserted = false
        // if not: constructs new Level{} at that key, returns iterator to it, sets inserted=true
        auto [level_it, inserted] = st.bids.try_emplace(px_ticks, Level{});
        // grab reference to Level object at that price
        Level& level = level_it -> second; // level is a reference to second
        // append to FIFO
        level.queue.push_back(OrderNode{ id, qty, ts });
        auto order_it = std::prev(level.queue.end());
        // update aggregate
        level.aggregate_qty += qty;
        // record handle for O(1) cancel. id is order id
        st.id_to_handle.emplace(id, Handle{ Side::Buy, level_it, order_it });
        // no trades (non-crossing)
        return out;
    }
    
    // CROSSING sell
    if (side == Side::Sell) {
        // assign order id to incoming sell (taker)
        const uint64_t taker_id = st.next_id++;
        out.order_id = taker_id;
        int64_t remaining = qty;
        // while still have qty and crossable bid exists, cross
        while (remaining > 0 && !st.bids.empty()) {
            // best bid = highest price in bids
            auto best_bid_it = std::prev(st.bids.end());
            const int64_t best_bid_px = best_bid_it->first;
            Level& level = best_bid_it->second;

            if (best_bid_px < px_ticks) break; // no longer crossing

            auto maker_it = level.queue.begin(); // starting at the best bid
            const uint64_t maker_id = maker_it->order_id;
            // amt to fill
            const int64_t fill = std::min<int64_t>(remaining, maker_it->remaining_qty);
            // emit trade at the maker's (resting best bid) price
            out.trades.push_back(Trade{
                maker_id,/*maker_order_id=*/
                taker_id,/*taker_order_id=*/
                best_bid_px,/*px_ticks=*/
                fill,/*qty=*/
                ts/*ts=*/
            });
            // apply the effects of the fill
            maker_it->remaining_qty -= fill;
            level.aggregate_qty     -= fill;
            remaining               -= fill;
            // if maker completed, erase node and its handle (O(1))
            if (maker_it->remaining_qty == 0) {
                st.id_to_handle.erase(maker_id);
                level.queue.erase(maker_it);
            }
            // if the price level is now empty, erase it 
            if (level.aggregate_qty == 0) {
                st.bids.erase(best_bid_it);
            }
        }
    }
    return out;
}

// fn is member of OrderBook, might return TopofBook or null. const function doesn't modify object
std::optional<TopOfBook> OrderBook::best_bid() const {
    const auto& st = S();                       // auto = deduce the type
    if (st.bids.empty()) return std::nullopt;
    // std.bids.end() is iterator one past last elem in map. std::prev gives the last element in the map.
    auto iter = std::prev(st.bids.end());         // highest price
    const Level& level = iter->second;   // level references second, the Level object
    return TopOfBook{iter->first, level.aggregate_qty };  // iter -> key (price)
}

std::optional<TopOfBook> OrderBook::best_ask() const {
    const auto& st = S();
    if (st.asks.empty()) return std::nullopt;
    auto iter = st.asks.begin();                  // lowest price
    return TopOfBook{iter->first, iter->second};
}
// get level size, return 0 if missing
int64_t OrderBook::depth_at(Side side, int64_t px_ticks) const {
    const auto& st = S();
    if (side == Side::Buy){
        auto it = st.bids.find(px_ticks);
        if(it == st.bids.end()) return 0;
        return it->second.aggregate_qty;
    } else {
        auto it = st.asks.find(px_ticks);
        return (it == st.asks.end()) ? 0 : it->second;
    }
}


AddMarketResult OrderBook::add_market(Side side, int64_t qty, uint64_t ts) {
    AddMarketResult out{0, {}};
    if(qty<=0) return out; // reject; taker_id==0 signals invalid for now
    auto& st = S();
    // give submission temp taker id for attribution in trades
    out.taker_order_id = st.next_id++;
    int64_t remaining = qty;
    if (side == Side::Buy) {
        // from best (lowest price) outward
        while (remaining > 0 && !st.asks.empty()) {
            auto best_ask_it = st.asks.begin(); // lowest price
            const int64_t level_px   = best_ask_it->first;
            int64_t&      level_qty  = best_ask_it->second;

            // Fill as much as possible at this level
            const int64_t fill = (remaining < level_qty) ? remaining : level_qty;
            
            // Emit a trade at the maker's price (ask level price).
            // NOTE: we don't yet have real maker order_ids for asks (no per-order queue implemented),
            // so we set maker_order_id = 0 as a placeholder for T3.
            out.trades.push_back(Trade{
                0,/*maker_order_id=*/
                out.taker_order_id,/*taker_order_id=*/
                level_px,/*px_ticks=*/
                fill,/*qty=*/
                ts/*ts=*/
            });
            // apply the fill to the level and the taker
            level_qty  -= fill;
            remaining  -= fill;
            // if level empty then erase
            if (level_qty == 0) {
                st.asks.erase(best_ask_it);
            }
        }
        // market orders never rest, any leftover remaining (ie: other side ran out) goes unfilled
        return out;

    } else {
        // Side::Sell: walk bids from best (highest price) outward
        while (remaining > 0 && !st.bids.empty()) {
            auto best_bid_it = std::prev(st.bids.end()); // highest price
            const int64_t level_px  = best_bid_it->first;
            int64_t&      level_qty = best_bid_it->second;

            const int64_t fill = (remaining < level_qty) ? remaining : level_qty;

            out.trades.push_back(Trade{
                0,/*maker_order_id=*/                 // placeholder until per-order FIFO exists
                out.taker_order_id,/*taker_order_id=*/
                level_px, /*px_ticks=*/           
                fill,/*qty=*/
                ts/*ts=*/
            });

            level_qty -= fill;
            remaining -= fill;

            if (level_qty == 0) st.bids.erase(best_bid_it);
        }
        return out;
    }

}