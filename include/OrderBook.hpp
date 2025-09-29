// creating the contract the tests & any app will rely on
// types and method signatures

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

// top of the book - best price and total resting size at that price
struct TopOfBook{
    int64_t px_ticks;  // price in integer ticks (no floats)
    int64_t agg_qty;   // total resting quantity at that price
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
