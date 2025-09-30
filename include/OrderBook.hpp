// creating the contract the tests & any app will rely on
// types and method signatures
// no maps/lists/handles here - encapsulation - keep implementation choices private to change data structures w/o breaking users or recompiling 
// tests call API, internals can solve. data struct can be restructured


// C++ headers - avoid double inclusion, declare only what is needed

/*
compiler directive, only include this header file once per compilatio
avoids double inclusion problem (same header gets pulled in multiple times)
*/
#pragma once 

#include <cstdint>   // fixed-width ints
#include <optional>  // std::optional for “maybe a value”
#include <vector>    // std::vector for trade lists

// STRUCT - default to public members/inheritance, used for plain data carriers where all fields r public

// type safety and exhaustiveness; enum class keeps values scoped and comparisons cheap
enum class Side { Buy, Sell }; // must write Side::Buy; no implicit int cast

/**
 * Top of the book — px_ticks: best price, agg_qty: total resting size at that price
 */
struct TopOfBook{
    int64_t px_ticks;  /**< price in integer ticks (no floats) */
    int64_t agg_qty;   /**< total resting quantity at that price */
};

// the engine emits events when matching happens
// each event says who traded w/who, at what price, how much, when
struct Trade {
    uint64_t maker_order_id;  // resting order’s id
    uint64_t taker_order_id;  // incoming (market or limit) order’s id 
    int64_t  px_ticks;        // prints at maker price
    int64_t  qty;             // filled quantity for this slice
    uint64_t ts;              // trade timestamp (monotonic)
};

// when submitting an action
// need engine-generated id and list of trades it caused
struct AddLimitResult {
    uint64_t           order_id; // assigned by engine; 0 means “rejected”
    std::vector<Trade> trades;   // may be empty (non-crossing)
};

struct AddMarketResult {
    uint64_t           taker_order_id; // ephemeral / transient / doesn't "rest"
    std::vector<Trade> trades;         // may be empty
};

/* 
 * public API the tests will call  
 * defines the functions/etc from part1.md Part B  
 * documentation link: part1.md#thought-exercise-b--operations-we-must-support-you-propose
 */
class OrderBook {
public:
    // order_id is engine generated for uniqueness, avoids client races
    // engine owns lifecycle, can attribute trades determinstically etc 
    // it makes logical sense that the client does not generate their own ids
    // can still return id so caller can cancel later.
    AddLimitResult  add_limit (Side side, int64_t px_ticks, int64_t qty, uint64_t ts);
    // no px_ticks bc has no price limit - can trade across multiple price levels, no single order price to pass in
    AddMarketResult add_market(Side side, int64_t qty, uint64_t ts);

    bool cancel(uint64_t order_id);

    //
    std::optional<TopOfBook> best_bid() const;
    std::optional<TopOfBook> best_ask() const;

    int64_t depth_at(Side side, int64_t px_ticks) const;

private:
    // Intentionally opaque: no internals leak into the header.
    // (We’ll define data structures in OrderBook.cpp.)
};

//