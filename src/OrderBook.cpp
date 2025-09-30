
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

    // engine internal memory
    // BookState is the whole book, prices are keys inside bids/asks maps
    struct BookState {
        std::map<int64_t, int64_t>                 bids;        // price → aggregate qty
        std::map<int64_t, int64_t>                 asks;        // these hold the aggregates per side
        // mapping type, id -> order, find order w/o searching the book
        std::unordered_map<uint64_t, ActiveOrder>  id_to_order; 
        // each time we accept new resting order, we hand out fresh order_id and increment
        uint64_t next_id = 1; // engine-generated IDs
    };

    // returns reference to this s BookState instance. & = reference type
    // if we returned by value, we'd get a copy each time, edits don't persist
    // returning reference gives direct access. we mutate the real state
    BookState& S() {
        static BookState s; // constructed once on first call
        return s;           // always return this 
    } 
}
