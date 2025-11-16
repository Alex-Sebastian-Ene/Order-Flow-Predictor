#ifndef ORDER_FLOW_TYPES_HPP
#define ORDER_FLOW_TYPES_HPP

#include <string>
#include <chrono>
#include <array>
#include <cstdint>
#include <cstddef>

namespace order_flow {
namespace types {
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
    double volatility;      // sigma - Implied volatility (annualized)
};

struct PricingResult{
    // Option prices
    double call_price;      // Black-Scholes call option price
    double put_price;       // Black-Scholes put option price
    
    // Greeks - risk sensitivities
    double call_delta;      // dC/dS - Call delta (sensitivity to spot price)
    double put_delta;       // dP/dS - Put delta
    double gamma;           // d^2C/dS^2 - Gamma (sensitivity to delta changes)
    double theta;           // dC/dT - Theta (time decay)
    double vega;            // dC/d(sigma) - Vega (sensitivity to volatility)
    double rho_call;        // dC/dr - Call rho (sensitivity to interest rate)
    double rho_put;         // dP/dr - Put rho
    double vanna;           // d^2C/dS d(sigma) - cross sensitivity
    double vomma;           // d^2C/d(sigma)^2 - convexity in volatility
};

struct alignas(64) OptionBatchView{
    const double* spot_ptr;
    const double* strike_ptr;
    const double* time_ptr;
    const double* rate_ptr;
    const double* vol_ptr;
    std::size_t count;
    std::size_t stride;
};

struct alignas(64) PricingBatchView{
    double* call_ptr;
    double* put_ptr;
    double* delta_ptr;
    double* put_delta_ptr;
    double* gamma_ptr;
    double* vega_ptr;
    double* theta_ptr;
    double* rho_call_ptr;
    double* rho_put_ptr;
    double* vanna_ptr;
    double* vomma_ptr;
    std::size_t count;
    std::size_t stride;
};

struct alignas(64) TaylorGreeks{
    double spot;
    double strike;
    double sqrt_time;
    double sigma;
    double call_price;
    double put_price;
    double call_delta;
    double put_delta;
    double gamma;
    double vega;
    double theta;
    double rho_call;
    double rho_put;
    double vanna;
    double vomma;
};

struct ArbitrageSignal{
    // Arbitrage detection for ultra-low latency trading
    double theoretical_price;   // Black-Scholes theoretical value
    double market_price;        // Observed market price
    double price_diff;          // market_price - theoretical_price
    double price_diff_pct;      // (price_diff / theoretical_price) * 100
    
    // Action signals
    bool is_arbitrage;          // true if |price_diff_pct| > threshold
    bool should_buy;            // true if market undervalued (price_diff < 0)
    bool should_sell;           // true if market overvalued (price_diff > 0)
    
    // Risk metrics for position sizing
    double delta_hedge_size;    // Number of shares to hedge (delta * position_size)
    double expected_profit;     // Estimated profit per contract
    
    // Timing (for latency tracking)
    int64_t timestamp_ns;       // Nanosecond timestamp when signal generated
};

struct PutCallParityCheck{
    // Put-Call Parity: C - P = S - K*e^(-rT)
    double call_price;
    double put_price;
    double spot_price;
    double pv_strike;           // K * e^(-rT) - present value of strike
    
    double lhs;                 // C - P
    double rhs;                 // S - K*e^(-rT)
    double parity_diff;         // lhs - rhs (should be ~0)
    
    bool parity_violated;       // true if arbitrage exists
    double arbitrage_profit;    // Profit per spread trade
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
