
// T1: support non-crossing buy insert, cancel, best_bid/ask, depth_at
#include "OrderBook.hpp"   // btw these are manually typed comments
#include <map>             // ordered price -> agg_qty
#include <unordered_map>   // id -> ActiveOrder
#include <optional>


// namespace - way to group names together to avoid collisions w name from other code
namespace {
    // minimal info we need ab single resting order to cancel it correctly
    struct ActiveOrder {
        Side     side;
        int64_t  px_ticks;
        int64_t  remaining_qty;    // how much to subtract from aggregate on cancel
        uint64_t ts;               // needed for FIFO
    };

    // BookState is the whole book, prices are keys inside bids/asks maps
    struct BookState {
        std::map<int64_t, int64_t>                 bids;        // price → aggregate qty
        std::map<int64_t, int64_t>                 asks;        // these hold the aggregates per side
        // map order id -> active order
        std::unordered_map<uint64_t, ActiveOrder>  id_to_order; 
        // each time we accept new resting order, we hand out fresh order_id and increment
        uint64_t next_id = 1; // engine-generated IDs
    };
    // if we returned by value, we'd get a copy each time, edits don't persist
    // returning reference gives direct access. we mutate the real state
    BookState& S() { // & = reference type
        static BookState s; // constructed once on first call
        return s;           // always return this 
    } 
}

AddLimitResult OrderBook::add_limit(Side side, int64_t px_ticks, int64_t qty, uint64_t ts) {
    AddLimitResult out{0, {}}; // order_id = 0, trades = {}. default that reprsents failure
    if (qty <= 0 || px_ticks < 0) return out; // invalid trades
    auto& st = S(); // Access the single BookState (internal memory)

    // T1 - non-crossing BUY inserts
    if (side == Side::Buy) {
        const uint64_t id = st.next_id++;  //engine generated ID
        out.order_id = id;
        // Update the aggregate at this bid price
        st.bids[px_ticks] += qty;
        // Record the order so cancel() can find & remove it later
        st.id_to_order.emplace(id, ActiveOrder{ // emplace - constructs value in place (no temp obj needed unlike insert), doesn't overwrite (unlike operator[](key))
            Side::Buy,      // side
            px_ticks,       // price level
            qty,            // remaining_qty
            ts              // timestamp (for FIFO)
        });
        // Non-crossing: no trades to emit (out.trades stays empty)
        return out;
    }
    
    // T2 - Sell, match against one price level, one resting order at that level
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
            if (best_bid_px < px_ticks) break; // no longer crossing
            // find a maker order at this price w/earliest ts (FIFO stand-in)
            // (Temporary: linear scan of id_to_order; OK for T2 where 1 order exists.)
            uint64_t maker_id = 0;
            uint64_t maker_ts = UINT64_MAX;
            // oid, ao iterates over st.id_to_order
            for (const auto& [oid, ao] : st.id_to_order) {
                if (ao.side == Side::Buy && ao.px_ticks == best_bid_px && ao.remaining_qty > 0) {
                    if (ao.ts < maker_ts) { maker_ts = ao.ts; maker_id = oid; }
                }
            }
            if (maker_id == 0) {
                // No concrete maker found at this level (shouldn't happen if aggregate > 0).
                break; // avoid infinite loop.
            }
            // Compute fill qty
            auto& maker = st.id_to_order[maker_id];
            const int64_t fill = (remaining < maker.remaining_qty) ? remaining : maker.remaining_qty;
            
            // Emit/do trade at maker's price
            out.trades.push_back(Trade{
                maker_id,/*maker_order_id=*/
                taker_id,/*taker_order_id=*/
                best_bid_px,/*px_ticks=, maker price*/ 
                fill,/*qty=*/
                ts/*ts= use incoming order's ts for now*/              
            });
            // apply fills: reduce maker, reduce level aggregate, reduce taker remaining
            maker.remaining_qty-= fill;
            best_bid_it->second-= fill;
            remaining-= fill;

            // if maker order is done, erase it
            if (maker.remaining_qty == 0) {
                st.id_to_order.erase(maker_id);
            }
            // if price level aggregate = 0 / empty, erase the level
            if (best_bid_it->second == 0) {
                st.bids.erase(best_bid_it);
            }
        }

        // if any incoming sell remains, didn't cross (ie limit order not fulfilled), rest on the ask side
        if (remaining > 0) {
            st.asks[px_ticks] += remaining;
            st.id_to_order.emplace(taker_id, ActiveOrder{Side::Sell, px_ticks, remaining, ts});
        } else {
            // fully filled on entry, taker never rests; nothing to record in id_to_order
        }
        return out;

    }
    return out; // order_id==0 means “not accepted” in this minimal step
}


bool OrderBook::cancel(uint64_t order_id) {
    auto& st = S();
    // Look up order_id in id_to_order; if missing -> false
    // .find() returns an iterator to that element with that key
    auto iter = st.id_to_order.find(order_id);
    // .find() returns end() if not found
    if (iter == st.id_to_order.end()) return false;

    // -> dereferences iter to get the element, to read fields w/o copying 
    const ActiveOrder& ao = iter->second; // second = VALUE IN KEY, VALUE PAIR (ActiveOrder)

    // reference st.bids or st.asks (price->aggregate map)
    auto& side_map = (ao.side == Side::Buy) ? st.bids : st.asks;
    // price iterator
    auto px_iter = side_map.find(ao.px_ticks);
    // if order was buy, we subtract from bids
    // if order was sell, we subtract from asks
    // if level exists, subtract from aggregate, px_iter -> second is the aggregate quantity after subtraction
    if (px_iter != side_map.end()) {
        px_iter->second -= ao.remaining_qty;
        if (px_iter->second <= 0) side_map.erase(px_iter);  // no ghosts
    }
    st.id_to_order.erase(iter);
    return true;
}

// fn is member of OrderBook, might return TopofBook or null
std::optional<TopOfBook> OrderBook::best_bid() const {
    const auto& st = S();                       // auto = deduce the type
    if (st.bids.empty()) return std::nullopt;
    auto iter = std::prev(st.bids.end());         // highest price
    return TopOfBook{iter->first, iter->second};    // move the pointer
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
    const auto& side_map = (side == Side::Buy) ? st.bids : st.asks;
    // find returns an iterator to the key if found; otherwise side_map.end()
    auto iter = side_map.find(px_ticks);
    // not found, return 0; else return the aggregate qty. second is the value
    return (iter == side_map.end()) ? 0 : iter->second;
}


