#ifndef BLACK_SCHOLES_HPP
#define BLACK_SCHOLES_HPP

#include "order_flow_types.hpp"
#include <array>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace order_flow {
namespace pricing {
using namespace types;

struct CDF_precompute{
    double p;
    double a1; 
    double a2; 
    double a3; 
    double a4; 
    double a5;
};

/**
 * Ultra-fast math operations using pre-computed lookup tables.
 * All arrays are cache-aligned and compile-time computed for zero runtime overhead.
 */
class FastMath {
private:
    // Maximum 5 years = 1825 days for option expiration
    static constexpr size_t MAX_DAYS = 1825;
    
    // Pre-computed sqrt values for days 0-1825
    alignas(64) static constexpr std::array<double, MAX_DAYS + 1> sqrt_cache = []() {
        std::array<double, MAX_DAYS + 1> cache{};
        for (size_t i = 0; i <= MAX_DAYS; ++i) {
            double years = static_cast<double>(i) / 365.0;
            cache[i] = std::sqrt(years);
        }
        return cache;
    }();

    // Pre-computed log values for common S/K ratios (0.5 to 1.5)
    static constexpr size_t LOG_CACHE_SIZE = 2001;
    static constexpr double LOG_MIN_RATIO = 0.5;
    static constexpr double LOG_MAX_RATIO = 1.5;
    static constexpr double LOG_SCALE = static_cast<double>(LOG_CACHE_SIZE - 1) / (LOG_MAX_RATIO - LOG_MIN_RATIO);
    
    alignas(64) static constexpr std::array<double, LOG_CACHE_SIZE> log_cache = []() {
        std::array<double, LOG_CACHE_SIZE> cache{};
        for (size_t i = 0; i < LOG_CACHE_SIZE; ++i) {
            double ratio = LOG_MIN_RATIO + (static_cast<double>(i) / (LOG_CACHE_SIZE - 1)) * (LOG_MAX_RATIO - LOG_MIN_RATIO);
            cache[i] = std::log(ratio);
        }
        return cache;
    }();

public:
    /**
     * @brief Ultra-fast sqrt lookup for time to expiry in days
     * @param days Days to expiration (0 to 1825)
     * @return sqrt(days/365) - precomputed value
     * Performance: ~1-2 CPU cycles
     */
    static inline double fast_sqrt_days(int days) {
        return sqrt_cache[days]; // Direct array access
    }

    /**
     * @brief Ultra-fast log lookup for S/K ratios
     * @param ratio Stock price / Strike price ratio
     * @return ln(ratio) - precomputed or calculated value
     * Performance: ~1-2 cycles for ratios in [0.5, 1.5], fallback for extremes
     */
    static inline double fast_log_ratio(double ratio) {
        if (ratio >= LOG_MIN_RATIO && ratio <= LOG_MAX_RATIO) {
            size_t index = static_cast<size_t>((ratio - LOG_MIN_RATIO) * LOG_SCALE);
            return log_cache[index]; // Direct array access
        }
        return std::log(ratio); // Fallback for extreme ratios
    }

    /**
     * @brief Convert time in years to days
     * @param years Time to expiry in years
     * @return Days to expiry (rounded to nearest day)
     */
    static inline int years_to_days(double years) {
        return static_cast<int>(years * 365.0 + 0.5);
    }
};

/**
 * Ultra-low latency Black-Scholes pricing engine.
 * Target: Sub-microsecond computation for option pricing.
 */
class BlackScholes {
public:
    static CDF_precompute computeCoefficents();
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

    /**
     * @brief Detect arbitrage opportunity by comparing theoretical vs market price
     * @param params Option parameters (S, K, T, r, sigma)
     * @param market_price Current market price of the option
     * @param threshold_pct Percentage difference to trigger arbitrage signal (default 0.5%)
     * @param is_call true for call option, false for put option
     * @return ArbitrageSignal with pricing difference and action recommendation
     * 
     * Performance: < 1.5 microseconds (includes full Black-Scholes pricing)
     * Use case: High-frequency arbitrage detection for latency-sensitive trading
     */
    static ArbitrageSignal detectArbitrage(const OptionParams& params, 
                                          double market_price,
                                          double threshold_pct = 0.5,
                                          bool is_call = true);

    /**
     * @brief Check put-call parity for arbitrage opportunities
     * @param params Option parameters (same S, K, T, r for both options)
     * @param call_market_price Current market price of call option
     * @param put_market_price Current market price of put option
     * @param threshold Absolute difference threshold to trigger arbitrage (default $0.10)
     * @return PutCallParityCheck with parity violation details
     * 
     * Put-Call Parity: C - P = S - K*e^(-rT)
     * If violated beyond threshold, arbitrage exists via conversion/reversal spreads
     * Performance: < 100 nanoseconds (simple arithmetic, no CDF calculations needed)
     */
    static PutCallParityCheck checkPutCallParity(const OptionParams& params,
                                                 double call_market_price,
                                                 double put_market_price,
                                                 double threshold = 0.10);

private:
    /**
     * @brief Approximates normal CDF using Abramowitz & Stegun polynomial method
     * @param x Input value for which to calculate CDF
     * @param CDF Precomputed coefficients (p, a1-a5) for the approximation
     * @return Normal CDF value Phi(x) with accuracy < 7.5e-8
     * 
     * Uses formula: Phi(x) = 1 - (1/sqrt(2*pi)) * e^(-x^2/2) * P(t)
     * where t = 1/(1+px) and P(t) is polynomial of degree 5
     * Handles negative values using symmetry: Phi(-x) = 1 - Phi(x)
     */
    static double approx_CDF(double x, CDF_precompute CDF);
    
    /**
     * @brief Adam optimizer for finding optimal Abramowitz & Stegun coefficients
     * @param x_values Training data points across normal distribution domain
     * @param CDF Coefficient struct to optimize (modified in-place)
     * 
     * Minimizes squared error between approx_CDF and reference erf-based CDF
     * Uses adaptive learning rates, momentum, and bias correction
     * Includes parameter bounds checking and early stopping
     * Typically converges in < 2000 iterations with error < 1e-15
     */
    static void   gradient_descent(const std::vector<double>& x_values, CDF_precompute& CDF);
    
    /**
     * @brief Calculates total squared error between approximation and reference
     * @param x_values Test points to evaluate error across
     * @param CDF Current coefficient values to test
     * @return Sum of squared errors across all test points
     * 
     * Uses high-precision erf() as reference implementation
     * Error metric: Sum(Phi_true(x) - Phi_approx(x))^2
     */
    static double calculate_error(const std::vector<double>& x_values, CDF_precompute CDF);
    
    /**
     * @brief Calculate d1, d2 parameters for Black-Scholes formula
     * @param params Option parameters (S, K, T, r, sigma)
     * @param d1 Output: d1 = [ln(S/K) + (r + sigma^2/2)T] / (sigma*sqrt(T))
     * @param d2 Output: d2 = d1 - sigma*sqrt(T)
     * 
     * Should:
     * - Optimize log calculation using fast approximations
     * - Use pre-computed values where possible (e.g., sigma*sqrt(T))
     * - Implement SIMD operations for vectorized calculations
     * - Minimize cache misses with aligned data access
     * - Handle edge cases (T~=0, sigma~=0) gracefully
     */
    static void computeD1D2(const OptionParams& params, double& d1, double& d2);
    

};


} // namespace pricing
} // namespace order_flow

#endif // BLACK_SCHOLES_HPP
