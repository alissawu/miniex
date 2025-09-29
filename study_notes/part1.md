Notes:

### Vocab:
side: buy (bid), sell (ask)  
tick: smallest price increment.  
for this proj will store price as an int number of ticks for simplicity  
price level: all orders on one side at one identical price  
limit order: buy/sell at this price or better. fill what's available at the price threshold.  
market order: buy/sell now  
price-time priority: at a price level, earlier orders fill before later ones (FIFO)  
best bid/best ask: highest bid/lowest ask  
spread = ask - bid  
cross happens if buy price ≥ best ask OR sell price ≤ best bid  
- immediate trading  

EXERCISES:  

### Thought exercise A — simulate by hand
You have an empty book. Tick = 1. Process these events in order. Don’t do any fancy math—just keep track of levels and FIFO.  
add buy id=1 px=10 qty=5 ts=1  
add buy id=2 px=9 qty=4 ts=2  
add sell id=3 px=11 qty=2 ts=3  
add sell id=4 px=10 qty=3 ts=4 ← does this cross? if so, who trades with whom, at what price, and what quantities remain in the book afterward?  

Your task:  
After each step, state: best bid, best ask.  
On step 4, list the trades (taker id, maker id, price, qty) and the remaining quantities at each level.  

Work:  
syntax: event #id qty@price  

Step 1: add buy #1 @10x5  
best bid = 10, best ask = none  

Step 2: add buy #2 @9x4  
best bid = 10, best ask = none  

Step 3: add sell #3 @11x2  
best bid = 10, best ask = 11  

Step 4: add sell #4 @10x3  
best bid = 10, best ask = 10  
Creates a cross - because sell_px(10) ≤ best_bid(10)  
Taker: order #4 (incoming). Maker: order #1 (resting)  
Trade price: maker's price = 10  
Fill: 3 out of #1's 4 -> #1 remains 2  

Post trade book:  
bids: #1 @10x2, #2 @9x4  
asks: #3 @11x2  
best bid = 10, best ask = 11  

---

### Thought exercise B — operations we must support (you propose
Based on the vocabulary, list the minimum operations your order-book must implement to behave like the simulation above. Think in verbs. (e.g., “add ___”, “cancel ___”, “query ___”, etc.)
Answer:
- add_limit (side: buy/sell, id, px, qty, ts)
    - inserts limit order, if crosses, matching happens internally, call returns list of trades
    - remaining qty rests in the book.
- add_market (side, qty, ts)
    - consumes opposite side at best available prices until qty is done or book empties
    - returns Trades
- cancel(id) -> bool
    - removes active order w that id if it exists, returns success or failure
- best_bid() / best_ask() (or top_of_book())
    - read-only queries for current top prices, maybe sizes
key idea: fills and removals aren't api calls, they're effects of these 4 operations, 
API shld return trade records so caller can observe what happened.

### Thought exercise C — invariants you will enforce
Without naming data structures yet, write 3–5 invariants your implementation will always maintain (examples to get you thinking, not to copy: “asks sorted ascending by price,” “within a level, order order = arrival order,” …).
invariant: a fact that must be true after every operation. safety rules that never break.
should be checkable from outside
Answer:
1. Within a price level, arrival order must determine execution order (FIFO)
2. Every active order id must be unique
3. If best_ask and best_bid both exist, best_bid < best_ask
    - best_bid: highest amount someone wants to buy for
    - best_ask: lowest amount someone wants to sell for
    - bid must be lower than ask, otherwise trade will happen
4. Empty orders (qty=0) must be removed immediately. Price level w/qty=0 don't exist. (prevent ghost state)
5. Each trade prints at the maker's price (price of resting / prev order it matched)
    - gives concrete check on emitted trades
6. All quantities are positive integers; prices are integer ticks.
7. Bids are ordered descending by price; Asks are ascending 
    - Allows testing that top-of-book equals extreme present on both sides

### Thought exercise D - complexity targets
Let’s set goals before picking data structures. Use:
M = number of active orders,
L = number of active price levels,
k = number of levels touched in an operation,
t = number of trades emitted.
Propose target time complexities (Big-O) for each, and justify briefly:
add_limit (non-crossing)
add_limit (crossing)
add_market(qty)
cancel(id)
best_bid() / best_ask()
Hints to guide your thinking (not answers):
Getting the extreme price on a side should be very cheap.
Any operation that consumes k levels / emits t trades can’t be cheaper than Ω(k) / Ω(t).
Fast cancel(id) usually implies you kept a direct handle somewhere.
Write your targets + one-line rationale for each. Then we’ll choose concrete data structures that can actually hit those targets.
1. add_limit (non-crossing)
    - O(L) to traverse and add in the limit at the correct order? (Wrong)
2. add_limit (crossing)
    - O(1) bc it's just adding at the top and gets consumed? if it's crossing it's the highest (wrong)
3. add_market(qty)
    - O(k) because you consume levels until the quantity is matched
4. cancel(id)
    - O(1) for deletion? I'm not sure what structure this would be
5. best_bid() / best_ask()
    - O(1) bc it's at the top right?