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


    // --- T3: market BUY walks multiple ask levels ---
    // Seed ask side:
    //  - 11 x 2 (older)
    //  - 12 x 4 (newer)
    auto s1 = ob.add_limit(Side::Sell, /*px=*/11, /*qty=*/2, /*ts=*/30);
    auto s2 = ob.add_limit(Side::Sell, /*px=*/12, /*qty=*/4, /*ts=*/40);
    // (In the current minimal engine (matches as much as possible against best opposite price), 
    //  these resting sells don't yet create per-order nodes,
    //  but their aggregates appear in st.asks via add_limit's "rest remainder" path. For T3,
    //  we only need the aggregates and prices.)
    // Action: market BUY qty=5
    auto r = ob.add_market(Side::Buy, /*qty=*/5, /*ts=*/50);
    // Then: two trades at maker prices 11 (qty 2) and 12 (qty 3)
    assert(r.taker_order_id != 0);
    assert(r.trades.size() == 2);
    assert(r.trades[0].px_ticks == 11);
    assert(r.trades[0].qty      == 2);
    assert(r.trades[1].px_ticks == 12);
    assert(r.trades[1].qty      == 3);
    // Then: top-of-book now 12 x 1 (4 - 3), bids empty
    auto ba = ob.best_ask();
    assert(ba.has_value());
    assert(ba->px_ticks == 12);
    assert(ba->agg_qty  == 1);
    assert(!ob.best_bid().has_value());
    // Then: depth reflects no ghosts
    assert(ob.depth_at(Side::Sell, 11) == 0);
    assert(ob.depth_at(Side::Sell, 12) == 1);
    // Then: market taker cannot be canceled
    assert(ob.cancel(r.taker_order_id) == false);

    // --- T3b: market SELL walks multiple bid levels ---
    // Seed bid side:
    //  - 9 x 3 (older)
    //  - 8 x 4 (newer)
    auto b1 = ob.add_limit(Side::Buy, /*px=*/9, /*qty=*/3, /*ts=*/60);
    auto b2 = ob.add_limit(Side::Buy, /*px=*/8, /*qty=*/4, /*ts=*/70);
    auto bb_before = ob.best_bid();
    assert(bb_before.has_value() && bb_before->px_ticks == 9 && bb_before->agg_qty == 3);
    // Action: market SELL qty=5
    auto rS = ob.add_market(Side::Sell, /*qty=*/5, /*ts=*/80);
    // Then: trades at maker prices 9 (qty 3) then 8 (qty 2)
    assert(rS.taker_order_id != 0);
    assert(rS.trades.size() == 2);
    assert(rS.trades[0].px_ticks == 9);
    assert(rS.trades[0].qty      == 3);
    assert(rS.trades[1].px_ticks == 8);
    assert(rS.trades[1].qty      == 2);
    // Then: top-of-book now 8 x 2 (4 - 2), asks unchanged from previous tests
    auto bb_after = ob.best_bid();
    assert(bb_after.has_value());
    assert(bb_after->px_ticks == 8);
    assert(bb_after->agg_qty  == 2);
    // Depth: check no ghosts on bids
    assert(ob.depth_at(Side::Buy, 9) == 0);
    assert(ob.depth_at(Side::Buy, 8) == 2);
    // Market taker cannot be canceled
    assert(ob.cancel(rS.taker_order_id) == false);



    return 0; // success

}