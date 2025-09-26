Notes:

Vocab:
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
***Thought exercise A — simulate by hand
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

Thought exercise B — operations we must support (you propose)
Based on the vocabulary, list the minimum operations your order-book must implement to behave like the simulation above. Think in verbs. (e.g., “add ___”, “cancel ___”, “query ___”, etc.