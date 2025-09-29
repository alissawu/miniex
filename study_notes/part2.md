Part 2 Notes:

A. Side index (price -> level)
Ordered associative container keyed by price
std::map<int, Level> RB-tree
O(log L) lookup/insert/erase; O(1) access to min(asks) via begin() and max(bids) via std::prev(end()).
so top of book is O(1)

B. Per level FIFO (orders at one price)
double linked list
has O(1) pushback / arrivals, O(1) pop-front fills, O(1) erase at iterator (cancel an order), stable iterators (so handles don't break)
(cache locality - prob switch to intrusive list later)

C. ID index (order id -> handle)
unordered_map<order_id, Handle>
O(1) lookup by id to cancel/inspect
handle: 
- side: bid or ask, know what tree to touch
- price-node iterator: reference to the level in the side index
- order-node iterator: reference to the node in that level's linked list
lets us jump directly to the node to erase without searching

D. Level metadata (what is in a level)
aggregate_qty at that price (fast check for emptiness / depth queries)
list of orders (FIFO)
anything else for later, like arrival counters and stuff

E. Best of book in O(1)
Cached iterators: keep best_bid_it / best_ask_it updated on inserts/erases

TASK / PART 2:
Define your Handle and Level on paper (bullet list, not code):
Handle fields — list the exact fields you will store for O(1) cancel.
Name the side field and why you need it.
Name the two “iterators/pointers” you’ll hold (to what, exactly?).
Level fields — list the minimal state per price level.
What aggregate(s) do you keep and why?
What container holds the orders, and what properties does it guarantee you?
Top-of-book policy — which of the two strategies will you use for O(1) best bid/ask (map extremes or cached iters), and why?
Post those three bullet lists. I’ll critique them. Then we’ll move to Part 3: public interfaces (headers only) that embody these decisions and your invariants.

Handle fields:
To cancel an order in O(1), we need to know what side it's on, which price level node it lives in, and which order node in that level.
side: bid | ask
price-level iterator: iterator/pointer to the price level node inside the side's ordered index (interator into map<int, Level>)
order iterator: iterator/pointer to the order node inside that level's FIFO list (node in a linked list)

Level fields:
- The minimal state per price level should have at least one order in it, otherwise it shouldn't exist. It holds orders at a price and supports FIFO / O(1) cancel
- Aggregates we keep - the qty per price level, so we know when to erase / move to next level
- orders (linked list of ordered nodes): FIFO queue w O(1) push/pop and O(1) erase at iterator - enforces price-time priority and allows O(1) cancel
- Top of book policy, best bid/ask in O(1):
Use map extremes
O(1) on std::map, needs no extra state
aka stored in begin() and prev(end()), still O(1)