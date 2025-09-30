// if an assert fails, the program aborts w a non-zero exit code, makes the build red

#include "OrderBook.hpp"   // the public api we defined
#include <cassert>         // assert() for simple checks. if any fails, test exits w/ nonzero

int main() {
    OrderBook ob;

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

    return 0; // success

}