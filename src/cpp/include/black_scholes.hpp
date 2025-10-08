#ifndef BLACK_SCHOLES_HPP
#define BLACK_SCHOLES_HPP

#include "order_flow_types.hpp"

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

private:
    /**
     * @brief Approximates normal CDF using Abramowitz & Stegun polynomial method
     * @param x Input value for which to calculate CDF
     * @param CDF Precomputed coefficients (p, a1-a5) for the approximation
     * @return Normal CDF value Φ(x) with accuracy < 7.5×10⁻⁸
     * 
     * Uses formula: Φ(x) = 1 - (1/√(2π)) * e^(-x²/2) * P(t)
     * where t = 1/(1+px) and P(t) is polynomial of degree 5
     * Handles negative values using symmetry: Φ(-x) = 1 - Φ(x)
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
     * Error metric: Σ(Φ_true(x) - Φ_approx(x))²
     */
    static double calculate_error(const std::vector<double>& x_values, CDF_precompute CDF);
    
    /**
     * @brief Calculate d1, d2 parameters for Black-Scholes formula
     * @param params Option parameters (S, K, T, r, σ)
     * @param d1 Output: d1 = [ln(S/K) + (r + σ²/2)T] / (σ√T)
     * @param d2 Output: d2 = d1 - σ√T
     * 
     * Should:
     * - Optimize log calculation using fast approximations
     * - Use pre-computed values where possible (e.g., σ√T)
     * - Implement SIMD operations for vectorized calculations
     * - Minimize cache misses with aligned data access
     * - Handle edge cases (T≈0, σ≈0) gracefully
     */
    static void computeD1D2(const OptionParams& params, double& d1, double& d2);
};

} // namespace pricing
} // namespace order_flow

#endif // BLACK_SCHOLES_HPP
