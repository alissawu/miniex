
// T1: support non-crossing buy insert, cancel, best_bid/ask, depth_at
#include "OrderBook.hpp"   // btw these are manually typed comments
#include <map>             // ordered price -> agg_qty
#include <unordered_map>   // id -> ActiveOrder

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

    // Non-negativity. No neg qty, no neg price
    if (qty <= 0 || px_ticks < 0) return out;

    // Access the single BookState (internal memory)
    auto& st = S();

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
    // others aren't implemented yet
    return out; // order_id==0 means “not accepted” in this minimal step
}

bool OrderBook::cancel(uint64_t order_id) {
    auto& st = S();
    auto it = st.id_to_order.find(order_id);
    if (it == st.id_to_order.end()) return false;

    const ActiveOrder& ao = it->second;

    auto& side_map = (ao.side == Side::Buy) ? st.bids : st.asks;
    auto pit = side_map.find(ao.px_ticks);
    if (pit != side_map.end()) {
        pit->second -= ao.remaining_qty;
        if (pit->second <= 0) side_map.erase(pit);  // no ghosts
    }

    st.id_to_order.erase(it);
    return true;
}

