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
    double spot_price;      // S - Current price of the underlying asset
    double strike_price;    // K - Strike price of the option
    double time_to_expiry;  // T - Time to expiration in years
    double risk_free_rate;  // r - Risk-free interest rate (annualized)
    double volatility;      // σ - Implied volatility (annualized)
};

struct PricingResult{
    // Option prices
    double call_price;      // Black-Scholes call option price
    double put_price;       // Black-Scholes put option price
    
    // Greeks - risk sensitivities
    double call_delta;      // ∂C/∂S - Call delta (sensitivity to spot price)
    double put_delta;       // ∂P/∂S - Put delta
    double gamma;           // ∂²C/∂S² - Gamma (sensitivity to delta changes)
    double theta;           // ∂C/∂T - Theta (time decay)
    double vega;            // ∂C/∂σ - Vega (sensitivity to volatility)
    double rho_call;        // ∂C/∂r - Call rho (sensitivity to interest rate)
    double rho_put;         // ∂P/∂r - Put rho
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
