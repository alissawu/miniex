* T1 — Non-crossing limit add updates top of book (no trades)
Given: empty book
Start with brand new OrderBook
R1 = add_limit(buy, px=10, qty=5, ts=1)

R1.trades -> [] bc nothing crosses
best_bid() -> { px_ticks: 10, agg_qty: 5 }
- represents best price w/ what quantity
best_ask() -> null
depth_at(bid, 10) -> 5
cancel(R1,order_id) -> true
- the order is active, cancel removes it
After cancelling the order:
best_bid() -> null
- no bids remain
- also verify no "ghosts" (level 10 is removed once empty)

* T2 — Crossing limit sell matches at maker prices (single level)
Given: empty OrderBook,
R1 = add_limit(buy, px=10, qty=5, ts=1)
returned order_id = A
When: R2 = add_limit(sell, px=10, qty=3, ts=2)

R2.trades -> 1 trade { maker_order_id = A, taker_order_id = R2.order_id, px_ticks = 10, qty = 3}
best_bid() -> { px_ticks: 10, agg_qty: 2 }
best_ask() -> null
depth_at(bid, 10) -> 2
cancel(A) -> true
- order A is still active w 2 remaining, cancel removes
cancel(R2.order_id) -> false
- no longer active

* T3 — Market buy walks multiple ask levels (FIFO + multiple trades)
Given:
add_limit(sell, 11, qty=2, ts=1) → id = S1
add_limit(sell, 12, qty=4, ts=2) → id = S2
When:
R = add_market(buy, qty=5, ts=3)

R.trades:
{ maker_order_id = S1, taker_order_id = R.order_id, px_ticks = 11, qty = 2 }
{ maker_order_id = S2, taker_order_id = R.order_id, px_ticks = 12, qty = 3 }
- the maker is the resting order
best_ask -> {px_ticks:  12, agg_qty: 1}
best_bid -> null
depth_at(sell, 11) -> null
depth_at(sell, 12) -> 1
cancel(R.taker_order_id) = false bc it's no longer active?

* T4 — FIFO within a level (price–time priority)
Given: add_limit(buy, 10×2, ts=1) then add_limit(buy, 10×3, ts=2)
When: add_market(sell, qty=3, ts=3)
Then (assert):
trades = [ {maker=id_first, px=10, qty=2}, {maker=id_second, px=10, qty=1} ]
Remaining depth_at(bid,10) = 2 (belonging to second order)
best_bid = {px=10, agg_qty=2}
Why: proves FIFO inside a level, and partial fills.

* T5 — Cancel: non-existent vs terminal vs active
Given:
id=A: add_limit(buy, 10×2, ts=1) (active)
id=B: add_limit(sell, 10×2, ts=2) (will be terminal on match)
When: immediately after the add_limit(sell) call (which executes fully)

cancel(B.order_id) returns false bc no active order
cancel(123456789) returns false bc we can't find it.
cancel(A.order_id) returns false bc it's not active
best_bid returns null 
depth_at(bid, 10) returns 0 bc empty level (level is gone)

* T6 — Best-of-book consistency & no crossed book
Given: add_limit(buy, 9×5, ts=1); add_limit(sell, 11×2, ts=2)
When: query top-of-book
Then (assert):
best_bid = {px=9, agg_qty=5}
best_ask = {px=11, agg_qty=2}
invariant check: best_bid.px < best_ask.px (no cross)
Why: codifies the “no crossed book” invariant and top-of-book O(1).

* T7 — Timestamp tie broken by arrival sequence
Given: two buys at px=10 with identical ts=100:
add_limit(buy, 10×1, ts=100) // earlier arrival_seq
add_limit(buy, 10×1, ts=100) // later arrival_seq
When: add_market(sell, qty=1, ts=101)
Then (assert):
first trade’s maker is the first of the two (arrival_seq order)
Remaining depth_at(bid,10) = 1 (the second order)
Why: verifies your tie-breaker policy (ts, arrival_seq).

* T8 — Validation & overflow rejections
Given: empty book
When/Then (assert each independently):
add_limit(buy, px=-1, qty=1, ts=1) → reject (no state change)
add_limit(buy, px=1, qty=0, ts=1) → reject (no state change)
add_limit(buy, px=1, qty=MAX_ORDER_QTY+1, ts=1) → reject
Construct a level near int64 limit and attempt to add that would overflow aggregate → reject, no aggregate change.
Why: enforces non-negativity and overflow policies.
What you’ll need observable from the API
Return value of add_limit / add_market: the list of Trade plus the assigned order_id / taker_id.
best_bid() / best_ask() returning null or {px_ticks, agg_qty}.
depth_at(side, px) for a given price (simple for testing).
cancel(order_id)` → bool.
(If you didn’t plan depth_at, add it—it makes assertions easy and isn’t implementation-leaky.)