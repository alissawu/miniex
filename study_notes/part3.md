Module 1 — Part 3: Public interfaces (headers only)
Goal: design types & method signatures that express our invariants and big-O goals. No implementation, no code dump — you propose; I critique.
A. Value types (think: what data each record must carry)
Design the fields (names + types) for:
1. Order
    - Must uniquely identify an order, its side, price (ticks), quantity, and arrival priority.
    - Think: id, side, px_ticks, qty, ts (monotonic).
    - Constraint: qty > 0, px_ticks ≥ 0.
2. Trade
    - Captures one fill event.
    - Think: taker_id, maker_id, px_ticks, qty, ts (trade time).
    - Invariant to encode: trade price = maker’s price.
For each, write a bullet list of fields you’ll include and one sentence on why each field exists.
1. Order
- order_id (uint64): each order id must be unique, good for indexing and access, uniquely identifies
- side (enum): ask | bid, keeps track of whether it's an ask or a bid
    - feedback: strings are slow/error prone, so use 2-value domain. enums are cheap, typechecked, compare in O(1)
- px_ticks (int > 0): price 
    - integer ticks, no floats, name it ticks so units are explicit, signed int is fine as negatives are invalid
- qty (int > 0): quantity of stocks in this trade, bc every order has a desired amount
- ts (uint64, monotonic): timestamp, break ties in price level
    - monotonic timebase (ns since process start) so comparison is total / cheap
- owner_id (optional?): who made the order, need to track this for trading
    - needed for LIMIT ORDER, but for market orders, they never rest so don't really need?
    - still convenient to tag incoming taker w taker id so fills can say maker_id X traded with taker_id Y
2. Trade:
- maker_id (uint64): who makes the order
- taker_id (uint64): who takes the order, need maker and taker id to track how many stocks to take from who?
- px_ticks (int32/64 ? ): price 
    - trade price = maker price
- qty (int32 > 0): trades involve quantities of stocks that must be kept track of
- ts (uint64, monotonic): trade time, to determine recency?

B. OrderBook API (verbs an external user calls)
Propose method names + inputs + outputs (no bodies). Use our earlier decisions:
- add_limit(...)
add_limit(order_id, side, px_ticks, qty, ts, owner_id) -> list of Trade 
    Input: order_id, side, px_ticks, qty, ts, owner_id
    Output: list of Trade events that happen bc of this call (can be empty, no crossings happened, can have multiple entries if order walked several price levels)
    Postcondition: emits trades at maker prices while crossing, if any qty remains, residual order rests
    book invariants hold (FIFO, no crossed book, no empty levels)
- add_market(...)
add_market(taker_id, side, qty, ts, owner_id) -> list of Trade
    Inputs: taker_id, side, qty, ts, owner_id (possibly optional bc these happen fast, unless we wanna track it for profiles)
    Output: sequence of Trades.
    Behavior: consumes opposite side from best outward; never rests.
    Postcondition: COnsumes from opposite side from best outward, book invariants hold
- cancel(...)
cancel(order_id) -> bool
    Inputs: id.
    Output: bool (success/failure).
        - Success: the order id exists and is active and we successfuly remove it 
        - False if: order doesn't exist, invalid id by policy? (TBD)
    Behavior: O(1) via id→handle; may erase a level if it empties.
    Postcondition: if order is active, it is removed and level erased if empty
- best_bid() / best_ask(
best_bid() -> optional<{px_ticks, agg_qty}>
    Output: “top-of-book” struct (price + size).
    Behavior: O(1) via map extremes.
    (Optional) depth_at(price, side), empty(), size() — only if you think they help testing.
    Postconditon: reports current extreme price and size, O(1)

For each method, add a one-line postcondition tied to your invariants. Example style (don’t copy):
“After add_market, there are no crossed prices; all zero-qty orders removed.”

C. Edge-case policy (you decide)
1. Validation on add_limit
What do we do if qty ≤ 0 or px_ticks < 0? (reject? clamp? error code?)
- Reject w clear status. clear box and show text underneath the input box when it is input (order qty can't be 0!) or something upon trying to submit
What if order_id is already active?
- I think the way we implement this should ensure we don't assign duplicate order_ids. 
- If this does happen,  reject the new order (don't auto cancel the exissting one)
2. Immediate full execution
If add_limit is fully filled immediately (no remainder), does the order ever “exist” in the book afterward? (Should cancel(order_id) succeed or fail later?)
- return false (not found), bc order never became active
3. Market order identifiers
If a market order has taker_id, and it fully executes, do you keep any state for it? (What should cancel(taker_id) do?)
- market orders never rest so it should return false
- can include taker_id in the api so trades point back to the taker order?
4. Cancel semantics
What should cancel(order_id) return when:
    a. the order doesn’t exist,
        - False
    b. it existed but is already fully filled,
        - False
    c. it’s active?
        - True
5. Top-of-book on emptiness
What do best_bid() / best_ask() return if that side is empty?
- Null
6. Timestamp ties
If two orders arrive at the same price and identical ts, how do we break the tie for FIFO? (e.g., secondary key = order_id increasing? reject equal ts? use arrival sequence?)
- Engine-assigned seq breaks ties (strict FIFO)
7. Overflow/limits
Do we enforce a max qty per order or per level? If an addition would overflow aggregate_qty, what’s the policy?
- Choose wide int types for qty and aggregate
- enforce per-order max, reject if qty > max_order>qty
- for level aggregate, check for overflow before add
- no cap needed beyond overflow prevention
8. (Optional) Replace vs. cancel-and-add
If a user wants to modify price/qty, do we support a replace(order_id, …) in Step 1, or do we require cancel + new add?
- require cancel and new add, like fidelity

Answers:
1) Validation on add_limit
Reject if qty ≤ 0 or px_ticks < 0. No side effects.
(Reason: enforces non-negativity invariant.)
Duplicate active order_id cannot occur because IDs are engine-generated.
If an internal bug tries to reuse an active ID → reject and surface an internal error (for us to fix).
(Reason: maintain the “unique active ID” invariant.)
2) Immediate full execution
If the incoming limit fully executes on entry (no remainder), it never becomes active.
Later cancel(order_id) → false (not found).
The only record is the Trade events emitted.
3) Market order identifiers
Market orders never rest; they’re not inserted into the id→handle table.
cancel(taker_id) → false (there is nothing to cancel).
Trades include taker_order_id (ephemeral) for attribution.
4) Cancel semantics
cancel(order_id) returns:
true → order was active and is now removed (erase level if it empties).
false → order does not exist / already terminal (filled or previously canceled).
5) Top-of-book emptiness
best_bid() / best_ask() → null (e.g., std::nullopt) if that side has no liquidity.
6) Timestamp ties (price–time priority)
Engine maintains a monotonic arrival_seq (internal counter).
Priority is ordered by (price, ts, arrival_seq).
If two orders share identical ts at a price level, arrival_seq breaks the tie.
(Reason: strict FIFO independent of wall-clock granularity.)
7) Overflow / limits
Use 64-bit integers for qty and level aggregates.
Enforce max per-order qty (e.g., MAX_ORDER_QTY) → reject if exceeded.
On insert/fill/cancel, check aggregate math for overflow; if aggregate + delta would overflow → reject the insert (no side effects).
No separate “per-level cap” beyond overflow protection in Step 1.
(Reason: correctness first; real risk limits can be layered later.)
8) Modify semantics (replace vs cancel+add)
Step 1 policy: no replace. Users (or a wrapper API) must cancel + add to modify price/qty.
(Reason: keeps core matching logic simple; true replace has tricky priority rules.)