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
    Inputs: side, id, price (ticks), qty, ts.
    Output: a sequence of Trades (possibly empty).
    Behavior: if crossing, emits trades; leftover qty (if any) rests.
add_limit()
- add_market(...)
    Inputs: side, qty, ts.
    Output: sequence of Trades.
    Behavior: consumes opposite side from best outward; never rests.
- cancel(id)
    Inputs: id.
    Output: bool (success/failure).
    Behavior: O(1) via id→handle; may erase a level if it empties.
- best_bid() / best_ask()
    Output: maybe optional<int> (ticks) or a richer “top-of-book” struct (price + size).
    Behavior: O(1) via map extremes.
    (Optional) depth_at(price, side), empty(), size() — only if you think they help testing.

For each method, add a one-line postcondition tied to your invariants. Example style (don’t copy):
“After add_market, there are no crossed prices; all zero-qty orders removed.”