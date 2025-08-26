#ifndef ORDER_FLOW_TYPES_HPP
#define ORDER_FLOW_TYPES_HPP

#include <string>
#include <chrono>
#include <array>
#include "order_flow_utils.hpp"

namespace order_flow {
namespace types {

using namespace utils;
/**
 * @brief Core order data structure for the order book system
 * Performance Requirements:
 * - Memory footprint: <= 64 bytes to fit in a cache line
 * - Move constructor: O(1), no heap allocations
 * - Copy constructor: Avoid usage, use references where possible
 */
struct Order{
    //use alignas to force cpu to allign cache
};

struct OptionParams{

};

struct PricingResult{

};
/**
 * @brief Trade data structure representing a matched order pair
 * Performance Requirements:
 * - Memory footprint: <= 64 bytes to fit in a cache line
 * - Move operations: O(1), no heap allocations
 * - Construction time: < 100ns
 */
struct Trade{
    
};

} // namespace types


} // namespace order_flow

#endif // ORDER_FLOW_TYPES_HPP
