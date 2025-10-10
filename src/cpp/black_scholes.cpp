#include "black_scholes.hpp"
#include <cmath>
#include <vector>
#include <limits>
#include <cstdio>
#include <chrono>

namespace order_flow{
namespace pricing{


double reference_cdf(double x){
    double CDF = 0.5 * (1.0 + erf(x/(sqrt(2.0))));
    return CDF;
}   

// aprox via taylor series
double BlackScholes::approx_CDF(double x, CDF_precompute CDF){
    double abs_x = fabs(x);
    double t = 1.0 /(1.0 + CDF.p * abs_x);
    double t2 = t*t;
    double t3 = t2 * t;
    double t4 = t3 * t;
    double t5 = t4 * t;

    //aproximate via a polynomial to fit the CDF
    double poly = CDF.a1 * t + CDF.a2 *t2 + CDF.a3 * t3 + CDF.a4 * t4 + CDF.a5 * t5;
    double exp_part = exp(-0.5 * abs_x * abs_x);
    double inv_sqrt_2pi = 1.0 / sqrt(2.0 * M_PI);

    double result = 1.0 - inv_sqrt_2pi * exp_part * poly;

    if (x < 0){
        result = 1.0 - result;
    }
    return result;
}

double BlackScholes::calculate_error(const std::vector<double>& x_values, CDF_precompute CDF){
    double total_error = 0.0;
    for (double x : x_values){
        double true_cdf = reference_cdf(x);
        double approx = approx_CDF(x, CDF);
        double error = true_cdf - approx;
        total_error += error * error;
    }
    return total_error;
}

void BlackScholes::gradient_descent(const std::vector<double>& x_values, CDF_precompute& CDF){
    double initial_learning_rate = 0.001;
    const double beta1 = 0.9;
    const double beta2 = 0.999;
    const double epsilon = 1e-8;
    const int max_iterations = 2000;

    double m_a1 = 0, m_a2 = 0, m_a3 = 0, m_a4 = 0, m_a5 = 0, m_p = 0;
    double v_a1 = 0, v_a2 = 0, v_a3 = 0, v_a4 = 0, v_a5 = 0, v_p = 0;

    double best_error = std::numeric_limits<double>::max();
    CDF_precompute best_CDF = CDF;

    for (int i = 0; i < max_iterations; i++) {
        double error = calculate_error(x_values, CDF);

        if (error < best_error) {
            best_error = error;
            best_CDF = CDF;
        }
        double grad_a1 = 0, grad_a2 = 0, grad_a3 = 0, grad_a4 = 0, grad_a5 = 0, grad_p = 0;
        const double h = 1e-5;

        // Compute gradients with proper error linkage
        for (double x : x_values) {
            double true_cdf = reference_cdf(x);
            double approx = approx_CDF(x, CDF);
            double error = true_cdf - approx;
            
            CDF.a1 += h;
            double approx_a1 = approx_CDF(x, CDF);
            grad_a1 += -2.0 * error * (approx_a1 - approx) / h;
            CDF.a1 -= h;
            
            CDF.a2 += h;
            double approx_a2 = approx_CDF(x, CDF);
            grad_a2 += -2.0 * error * (approx_a2 - approx) / h;
            CDF.a2 -= h;

            CDF.a3 += h;
            double approx_a3 = approx_CDF(x, CDF);
            grad_a3 += -2.0 * error * (approx_a3 - approx) / h;
            CDF.a3 -= h;

            CDF.a4 += h;
            double approx_a4 = approx_CDF(x, CDF);
            grad_a4 += -2.0 * error * (approx_a4 - approx) / h;
            CDF.a4 -= h;

            CDF.a5 += h;
            double approx_a5 = approx_CDF(x, CDF);
            grad_a5 += -2.0 * error * (approx_a5 - approx) / h;
            CDF.a5 -= h;

            CDF.p += h;
            double approx_p = approx_CDF(x, CDF);
            grad_p += -2.0 * error * (approx_p - approx) / h;
            CDF.p -= h;
        }
        m_a1 = beta1 * m_a1 + (1 - beta1) * grad_a1;
        m_a2 = beta1 * m_a2 + (1 - beta1) * grad_a2;
        m_a3 = beta1 * m_a3 + (1 - beta1) * grad_a3;
        m_a4 = beta1 * m_a4 + (1 - beta1) * grad_a4;
        m_a5 = beta1 * m_a5 + (1 - beta1) * grad_a5;
        m_p = beta1 * m_p + (1 - beta1) * grad_p;

        v_a1 = beta2 * v_a1 + (1 - beta2) * grad_a1 * grad_a1;
        v_a2 = beta2 * v_a2 + (1 - beta2) * grad_a2 * grad_a2;
        v_a3 = beta2 * v_a3 + (1 - beta2) * grad_a3 * grad_a3;
        v_a4 = beta2 * v_a4 + (1 - beta2) * grad_a4 * grad_a4;
        v_a5 = beta2 * v_a5 + (1 - beta2) * grad_a5 * grad_a5;
        v_p = beta2 * v_p + (1 - beta2) * grad_p * grad_p;

        double m_a1_hat = m_a1 / (1 - pow(beta1, i + 1));
        double m_a2_hat = m_a2 / (1 - pow(beta1, i + 1));
        double m_a3_hat = m_a3 / (1 - pow(beta1, i + 1));
        double m_a4_hat = m_a4 / (1 - pow(beta1, i + 1));
        double m_a5_hat = m_a5 / (1 - pow(beta1, i + 1));
        double m_p_hat = m_p / (1 - pow(beta1, i + 1));

        double v_a1_hat = v_a1 / (1 - pow(beta2, i + 1));
        double v_a2_hat = v_a2 / (1 - pow(beta2, i + 1));
        double v_a3_hat = v_a3 / (1 - pow(beta2, i + 1));
        double v_a4_hat = v_a4 / (1 - pow(beta2, i + 1));
        double v_a5_hat = v_a5 / (1 - pow(beta2, i + 1));
        double v_p_hat = v_p / (1 - pow(beta2, i + 1));

        CDF.a1 -= initial_learning_rate * m_a1_hat / (sqrt(v_a1_hat) + epsilon);
        CDF.a2 -= initial_learning_rate * m_a2_hat / (sqrt(v_a2_hat) + epsilon);
        CDF.a3 -= initial_learning_rate * m_a3_hat / (sqrt(v_a3_hat) + epsilon);
        CDF.a4 -= initial_learning_rate * m_a4_hat / (sqrt(v_a4_hat) + epsilon);
        CDF.a5 -= initial_learning_rate * m_a5_hat / (sqrt(v_a5_hat) + epsilon);
        CDF.p -= initial_learning_rate * m_p_hat / (sqrt(v_p_hat) + epsilon);
        
        // Add bounds checking to keep parameters reasonable
        CDF.p = std::max(0.01, std::min(10.0, CDF.p));
        CDF.a1 = std::max(-10.0, std::min(10.0, CDF.a1));
        CDF.a2 = std::max(-10.0, std::min(10.0, CDF.a2));
        CDF.a3 = std::max(-10.0, std::min(10.0, CDF.a3));
        CDF.a4 = std::max(-10.0, std::min(10.0, CDF.a4));
        CDF.a5 = std::max(-10.0, std::min(10.0, CDF.a5));
        
        // Progress monitoring
        if (i % 100 == 0 || i == 0) {
            printf("Iteration %d: Error = %.10e\n", i, error);
            printf("  p=%.9f, a1=%.9f, a2=%.9f, a3=%.9f, a4=%.9f, a5=%.9f\n", 
                   CDF.p, CDF.a1, CDF.a2, CDF.a3, CDF.a4, CDF.a5);
        }

        // Early stopping condition
        if (error < 1e-15) {
            printf("Converged at iteration %d with error %.10e\n", i, error);
            break;
        }
    }
    
    // Use best parameters found
    CDF = best_CDF;
    printf("Final error: %.10e\n", best_error);
}

CDF_precompute BlackScholes::computeCoefficents(){
    //gives approx for CDF using method from Abramowitz and Stegun
    CDF_precompute CDF;
    std::vector<double> x_values;

    for (double x = -3.0; x < 3.0; x += 0.01){
        x_values.push_back(x);
    }
    for (double x = -8.0; x < -3.0; x += 0.1) {
        x_values.push_back(x);
    }
    for (double x = 3.0; x <= 8.0; x += 0.1) {
        x_values.push_back(x);
    }    

    //init guesses close to the actual solutions
    CDF.p = 0.2316419;
    CDF.a1 = 0.31938153;
    CDF.a2 = -0.356563782;
    CDF.a3 = 1.781477937;
    CDF.a4 = -1.821255978;
    CDF.a5 = 1.330274429;

    //gradient descent
    gradient_descent(x_values, CDF);
    
    return CDF;// Added return statement to match double return type
}



double BlackScholes::normalCDF(double x){
    // Compute coefficients once and cache them
    static const CDF_precompute coeffs = computeCoefficents();
    
    // Use your optimized approximation
    return approx_CDF(x, coeffs);
}

void BlackScholes::computeD1D2(const OptionParams& params, double& d1, double& d2) {
    // TODO: Calculate Black-Scholes d1 and d2 parameters
    // OPTIMIZATION HINTS:
    // - Use fast log approximation instead of std::log() for ln(S/K)
    // - Pre-compute sigma_sqrt_T = sigma * sqrt(T) to avoid redundant sqrt() calls
    // - Use polynomial approximation for log when S/K is close to 1.0
    // - Consider lookup tables for commonly used strike/spot ratios
    // - Use FMA (fused multiply-add) instructions: r_plus_half_sigma2_T = r*T + 0.5*sigma*sigma*T
    // - Handle edge cases: T->0, sigma->0, S/K->0 or infinity
    // - Use reciprocal approximation (1/x) instead of division for sigma_sqrt_T
    
    // FORMULAS:
    // d1 = [ln(S/K) + (r + sigma^2/2)*T] / (sigma*sqrt(T))
    // d2 = d1 - sigma*sqrt(T)
    
    // EXPECTED INPUTS via OptionParams:
    // - S: current stock/underlying price (params.spot_price)
    // - K: strike price (params.strike_price)
    // - T: time to expiration in years (params.time_to_expiry)
    // - r: risk-free interest rate (params.risk_free_rate)
    // - sigma: implied volatility (params.volatility)
    
    // PERFORMANCE CRITICAL: This function called for every option pricing
    // Target: < 50 nanoseconds per call
    // OPTIMIZED: Using FastMath lookup tables for ultra-fast computation
    
    double S = params.spot_price;
    double K = params.strike_price;
    double T = params.time_to_expiry;
    double r = params.risk_free_rate;
    double sigma = params.volatility;
    
    // Ultra-fast lookups using pre-computed arrays (~1-2 CPU cycles each)
    int days = FastMath::years_to_days(T);
    double sqrt_T = FastMath::fast_sqrt_days(days);        // ~1-2 cycles vs ~20-50 for std::sqrt
    double ln_S_over_K = FastMath::fast_log_ratio(S / K);  // ~1-2 cycles vs ~30-60 for std::log
    
    // Fast arithmetic operations
    double sigma_sqrt_T = sigma * sqrt_T;
    double half_sigma2_T = 0.5 * sigma * sigma * T;
    double r_T = r * T;
    
    // Black-Scholes d1 and d2 formulas
    d1 = (ln_S_over_K + r_T + half_sigma2_T) / sigma_sqrt_T;
    d2 = d1 - sigma_sqrt_T;
}

PricingResult BlackScholes::calculate(const OptionParams& params) {
    // TODO: Main Black-Scholes pricing function
    // OPTIMIZATION HINTS:
    // - Use SIMD instructions (AVX2/AVX-512) for parallel computation of call/put prices
    // - Calculate all Greeks (delta, gamma, theta, vega, rho) in single pass to avoid redundant calculations
    // - Pre-compute common subexpressions: sqrt(T), sigma*sqrt(T), log(S/K)
    // - Use fast exp() approximation or lookup tables for exp(-rT)
    // - Align PricingResult struct to 64-byte boundaries for cache efficiency
    // - Avoid branches in critical path - use conditional moves instead
    // - Consider using restrict pointers if processing arrays of options
    
    // ALGORITHM STEPS:
    // 1. Call computeD1D2() to get d1, d2 parameters
    // 2. Calculate N(d1), N(d2), N(-d1), N(-d2) using your optimized normalCDF()
    // 3. Compute call_price = S*N(d1) - K*exp(-r*T)*N(d2)
    // 4. Compute put_price = K*exp(-r*T)*N(-d2) - S*N(-d1)
    // 5. Calculate Greeks using partial derivatives
    // 6. Pack results into PricingResult struct
    
    // PERFORMANCE TARGET: < 1 microsecond total
    // BREAKDOWN: computeD1D2 (50ns) + 4*normalCDF (200ns) + pricing (100ns) + Greeks (650ns)
    
    // GREEKS FORMULAS (for reference):
    // Delta_call = N(d1), Delta_put = N(d1) - 1
    // Gamma = phi(d1) / (S * sigma * sqrt(T))  where phi(x) = (1/sqrt(2*pi)) * exp(-x^2/2)
    // Theta_call = -S*phi(d1)*sigma/(2*sqrt(T)) - r*K*exp(-r*T)*N(d2)
    // Vega = S * phi(d1) * sqrt(T)
    // Rho_call = K * T * exp(-r*T) * N(d2)
    
    // MEMORY LAYOUT: Pack results efficiently to minimize cache misses
    // Use aligned loads/stores for SIMD operations


    
    PricingResult result{};
    
    // Step 1: Calculate d1 and d2 using optimized FastMath
    double d1, d2;
    computeD1D2(params, d1, d2);
    
    // Step 2: Calculate normal CDF values using optimized approximation
    double nd1 = normalCDF(d1);        // N(d1)
    double nd2 = normalCDF(d2);        // N(d2)  
    double n_neg_d1 = 1 - nd1;  // N(-d1)
    double n_neg_d2 = 1 - nd2;  // N(-d2)
    
    // Step 3: Extract parameters for pricing
    double S = params.spot_price;
    double K = params.strike_price;
    double T = params.time_to_expiry;
    double r = params.risk_free_rate;
    double sigma = params.volatility;
    
    // Step 4: Calculate discount factor
    double r_T = r * T;
    double discount_factor = exp(-r_T);  // Could be optimized with fast exp() approximation
    
    // Step 5: Black-Scholes option prices
    result.call_price = S * nd1 - K * discount_factor * nd2;
    result.put_price = K * discount_factor * n_neg_d2 - S * n_neg_d1;
    
    // Step 6: Calculate Greeks for risk management
    // Get optimized sqrt(T) from FastMath for Greeks calculations
    int days = FastMath::years_to_days(T);
    double sqrt_T = FastMath::fast_sqrt_days(days);
    double sigma_sqrt_T = sigma * sqrt_T;
    
    // Standard normal PDF at d1: phi(d1) = (1/sqrt(2*pi)) * exp(-d1^2/2)
    static constexpr double INV_SQRT_2PI = 0.39894228040143267794;
    double phi_d1 = INV_SQRT_2PI * exp(-0.5 * d1 * d1);
    
    // Greeks calculations
    result.call_delta = nd1;                    // dC/dS
    result.put_delta = nd1 - 1.0;              // dP/dS
    result.gamma = phi_d1 / (S * sigma_sqrt_T); // d^2C/dS^2 (same for calls and puts)
    result.vega = S * phi_d1 * sqrt_T;          // dC/d(sigma)
    result.theta = -S * phi_d1 * sigma / (2.0 * sqrt_T) - r * K * discount_factor * nd2; // dC/dT (call)
    result.rho_call = K * T * discount_factor * nd2;     // dC/dr
    result.rho_put = -K * T * discount_factor * n_neg_d2; // dP/dr
    
    return result;
}

ArbitrageSignal BlackScholes::detectArbitrage(const OptionParams& params,
                                              double market_price,
                                              double threshold_pct,
                                              bool is_call) {
    // ULTRA-LOW LATENCY ARBITRAGE DETECTION
    // Target: < 1.5 microseconds total
    // Use case: Detect mispriced options faster than competition
    
    ArbitrageSignal signal{};
    
    // Step 1: Calculate theoretical price using optimized Black-Scholes
    PricingResult pricing = calculate(params);
    
    // Step 2: Get theoretical price for the specific option type
    double theoretical_price = is_call ? pricing.call_price : pricing.put_price;
    double delta = is_call ? pricing.call_delta : pricing.put_delta;
    
    // Step 3: Calculate price differences
    signal.theoretical_price = theoretical_price;
    signal.market_price = market_price;
    signal.price_diff = market_price - theoretical_price;
    signal.price_diff_pct = (signal.price_diff / theoretical_price) * 100.0;
    
    // Step 4: Determine arbitrage opportunity
    double abs_diff_pct = fabs(signal.price_diff_pct);
    signal.is_arbitrage = abs_diff_pct > threshold_pct;
    
    // Step 5: Generate trading signals
    if (signal.is_arbitrage) {
        // Market undervalued -> BUY option
        signal.should_buy = signal.price_diff < 0;
        
        // Market overvalued -> SELL option
        signal.should_sell = signal.price_diff > 0;
        
        // Calculate expected profit per contract (100 shares per contract)
        signal.expected_profit = fabs(signal.price_diff) * 100.0;
        
        // Delta hedge size (shares to trade in opposite direction)
        signal.delta_hedge_size = fabs(delta) * 100.0;
    } else {
        signal.should_buy = false;
        signal.should_sell = false;
        signal.expected_profit = 0.0;
        signal.delta_hedge_size = 0.0;
    }
    
    // Step 6: Timestamp for latency tracking (nanoseconds since epoch)
    auto now = std::chrono::high_resolution_clock::now();
    signal.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();
    
    return signal;
}

PutCallParityCheck BlackScholes::checkPutCallParity(const OptionParams& params,
                                                     double call_market_price,
                                                     double put_market_price,
                                                     double threshold) {
    // ULTRA-FAST PUT-CALL PARITY CHECK
    // Target: < 100 nanoseconds (no CDF calculations needed)
    // Formula: C - P = S - K*e^(-rT)
    // If violated -> arbitrage via conversion/reversal spreads
    
    PutCallParityCheck check{};
    
    // Store input prices
    check.call_price = call_market_price;
    check.put_price = put_market_price;
    check.spot_price = params.spot_price;
    
    // Calculate present value of strike price
    double r_T = params.risk_free_rate * params.time_to_expiry;
    check.pv_strike = params.strike_price * exp(-r_T);
    
    // Put-Call Parity: C - P = S - K*e^(-rT)
    check.lhs = call_market_price - put_market_price;  // Left side
    check.rhs = params.spot_price - check.pv_strike;   // Right side
    check.parity_diff = check.lhs - check.rhs;
    
    // Check if parity is violated beyond threshold
    check.parity_violated = fabs(check.parity_diff) > threshold;
    
    if (check.parity_violated) {
        // Arbitrage profit per spread trade
        check.arbitrage_profit = fabs(check.parity_diff) * 100.0; // Per contract
    } else {
        check.arbitrage_profit = 0.0;
    }
    
    return check;
}

//ending namespace
    }
}