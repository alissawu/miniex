Part 2 Notes:

A. Side index (price -> level)
Ordered associative container keyed by price
std::map<int, Level> RB-tree
O(log L) lookup/insert/erase; O(1) access to min(asks) via begin() and max(bids) via std::prev(end()).
so top of book is O(1)

B. Per level FIFO (orders at one price)
double linked list
has O(1) pushback / arrivals, O(1) pop-front fills, O(1) erase at iterator (cancel an order), stable iterators (so handles don't break)
