#ifndef BLACK_SCHOLES_HPP
#define BLACK_SCHOLES_HPP

#include "order_flow_types.hpp"

namespace order_flow {
namespace pricing {

/**
 * Ultra-low latency Black-Scholes pricing engine.
 * Target: Sub-microsecond computation for option pricing.
 */
class BlackScholes {
public:
    /**
     * @brief Calculate normal CDF using Abramowitz and Stegun approximation
     * Should:
     * - Use polynomial approximation for speed
     * - Implement SIMD vectorization
     * - Avoid branches in critical path
     * - Pre-compute coefficients
     * - Error < 10^-7
     */
    static double normalCDF(double x);

    /**
     * @brief Price European options using Black-Scholes formula
     * Should:
     * - Complete in < 1 microsecond
     * - Use SIMD instructions (AVX2/AVX-512)
     * - Avoid memory allocation
     * - Calculate price and all Greeks in single pass
     * - Use cache-aligned data structures
     */
    static PricingResult calculate(const OptionParams& params);

private:
    /**
     * @brief Calculate d1, d2 parameters for Black-Scholes
     * Should:
     * - Optimize log calculation
     * - Use pre-computed values where possible
     * - Implement SIMD operations
     * - Minimize cache misses
     */
    static void computeD1D2(const OptionParams& params, double& d1, double& d2);
};

} // namespace pricing
} // namespace order_flow

#endif // BLACK_SCHOLES_HPP
