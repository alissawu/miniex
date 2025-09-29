* T1 — Non-crossing limit add updates top of book (no trades)
Given: empty book
When: add_limit(buy, px=10, qty=5, ts=1)
Then (assert):
trades = []
best_bid = {px=10, agg_qty=5}, best_ask = null
depth_at(bid, 10) = 5
cancel(order_id_from_add) returns true (order was active), and afterwards best_bid = null
Why: exercises add_limit(non-crossing), top-of-book O(1), cancel happy path, “no ghosts”.

* T2 — Crossing limit sell matches at maker prices (single level)
Given: resting bids: add_limit(buy, 10×5, ts=1)
When: add_limit(sell, 10×3, ts=2)
Then (assert):
trades = [ {maker=buy@10 id₁, taker=id₂, px=10, qty=3} ]
remaining: best_bid = {px=10, agg_qty=2}, best_ask = null
cancel(id₁) now returns true and removes the 10×2; best_bid becomes null
cancel(id₂) returns false (fully filled on entry → never active)
Why: matches your “trade at maker price” invariant; verifies terminal vs active cancel.

* T3 — Market buy walks multiple ask levels (FIFO + multiple trades)
Given: resting asks:
add_limit(sell, 11×2, ts=1), add_limit(sell, 12×4, ts=2)
When: add_market(buy, qty=5, ts=3)
Then (assert):
trades = [ {px=11, qty=2}, {px=12, qty=3} ] (maker side = sell)
best_ask becomes {px=12, agg_qty=1}
best_bid = null
cancel(taker_id) returns false (market orders never rest)
No empty ask level at 11 remains (ghost-free)
Why: validates O(k+t) walk, FIFO between levels, ghost cleanup, market order semantics.
T4 — FIFO within a level (price–time priority)
Given: add_limit(buy, 10×2, ts=1) then add_limit(buy, 10×3, ts=2)
When: add_market(sell, qty=3, ts=3)
Then (assert):
trades = [ {maker=id_first, px=10, qty=2}, {maker=id_second, px=10, qty=1} ]
Remaining depth_at(bid,10) = 2 (belonging to second order)
best_bid = {px=10, agg_qty=2}
Why: proves FIFO inside a level, and partial fills.
T5 — Cancel: non-existent vs terminal vs active
Given:
idA: add_limit(buy, 10×2, ts=1) (active)
idB: add_limit(sell, 10×2, ts=2) (will be terminal on match)
When: immediately after the add_limit(sell) call (which executes fully)
Then (assert):
cancel(idB) = false (terminal already)
cancel(unknown_id) = false
cancel(idA) = true and removes residual liquidity (best_bid becomes null)
Why: locks the boolean semantics for cancel.
T6 — Best-of-book consistency & no crossed book
Given: add_limit(buy, 9×5, ts=1); add_limit(sell, 11×2, ts=2)
When: query top-of-book
Then (assert):
best_bid = {px=9, agg_qty=5}
best_ask = {px=11, agg_qty=2}
invariant check: best_bid.px < best_ask.px (no cross)
Why: codifies the “no crossed book” invariant and top-of-book O(1).
T7 — Timestamp tie broken by arrival sequence
Given: two buys at px=10 with identical ts=100:
add_limit(buy, 10×1, ts=100) // earlier arrival_seq
add_limit(buy, 10×1, ts=100) // later arrival_seq
When: add_market(sell, qty=1, ts=101)
Then (assert):
first trade’s maker is the first of the two (arrival_seq order)
Remaining depth_at(bid,10) = 1 (the second order)
Why: verifies your tie-breaker policy (ts, arrival_seq).
T8 — Validation & overflow rejections
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