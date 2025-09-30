// if an assert fails, the program aborts w a non-zero exit code, makes the build red

#include "OrderBook.hpp"   // the public api we defined
#include <cassert>         // assert() for simple checks. if any fails, test exits w/ nonzero

int main() {
    OrderBook ob;

    // --- T1: add non-crossing BUY ---
    // Given: empty book
    assert(!ob.best_bid().has_value());
    assert(!ob.best_ask().has_value());
    // When: add non-crossing BUY 10 x 5
    auto r1 = ob.add_limit(Side::Buy, /*px_ticks=*/10, /*qty=*/5, /*ts=*/1);
    // Then: no trades on non-crossing insert
    assert(r1.order_id != 0); // check that engine accepted submission, assigned real id (0 = rejected)
    assert(r1.trades.empty()); // verify no matches happened
    // Then: top-of-book reflects the new bid; asks remain empty
    auto bb = ob.best_bid();
    assert(bb.has_value());
    assert(bb->px_ticks == 10);
    assert(bb->agg_qty  == 5);
    assert(!ob.best_ask().has_value());

    // Then: depth at (bid, 10) is exactly 5
    assert(ob.depth_at(Side::Buy, 10) == 5);

    // Then: cancel the active order succeeds and clears the bid
    assert(ob.cancel(r1.order_id) == true);
    assert(!ob.best_bid().has_value());

    // --- T2: crossing limit SELL against resting BUY ---
    // Seed: put a bid back in (10 x 5)
    auto seed = ob.add_limit(Side::Buy, /*px=*/10, /*qty=*/5, /*ts=*/10);
    assert(seed.order_id != 0);
    assert(ob.best_bid().has_value());
    assert(ob.best_bid()->px_ticks == 10);
    assert(ob.best_bid()->agg_qty  == 5);
    // Action: incoming SELL 10 x 3 (crosses)
    auto r2 = ob.add_limit(Side::Sell, /*px=*/10, /*qty=*/3, /*ts=*/20);
    // Then: one trade at maker price 10 for qty 3
    assert(r2.order_id != 0);                 // taker got an id (even if it never rests)
    assert(r2.trades.size() == 1);
    auto t = r2.trades[0];
    assert(t.px_ticks == 10); // limit order's price is 10
    assert(t.qty      == 3); // limit orderer wants 3 
    assert(t.taker_order_id == r2.order_id);
    // maker should be the resting bid (seed.order_id)
    assert(t.maker_order_id == seed.order_id);
    // Then: best bid reduced to 10 x 2; asks still empty
    auto bb2 = ob.best_bid();
    assert(bb2.has_value());
    assert(bb2->px_ticks == 10);
    assert(bb2->agg_qty  == 2);
    assert(!ob.best_ask().has_value());
    // Then: depth reflects 2 at (bid,10)
    assert(ob.depth_at(Side::Buy, 10) == 2);
    // Then: cancel maker succeeds (still active), cancel taker fails (fully filled on entry)
    assert(ob.cancel(seed.order_id) == true);
    assert(!ob.best_bid().has_value()); // cleared after cancel
    assert(ob.cancel(r2.order_id) == false);


    return 0; // success

}