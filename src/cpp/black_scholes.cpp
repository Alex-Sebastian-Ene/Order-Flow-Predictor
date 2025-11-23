#include "black_scholes.hpp"
#include <cmath>
#include <vector>
#include <limits>
#include <cstdio>
#include <chrono>
#include <stdexcept>
#include <algorithm>
#include <immintrin.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wpsabi"
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace order_flow{
namespace pricing{

namespace {
constexpr double INV_SQRT_2PI = 0.39894228040143267794;
constexpr bool LOG_CDF_CALIBRATION = false;
inline double normal_pdf(double x) {
    return INV_SQRT_2PI * std::exp(-0.5 * x * x);
}
}

#if defined(__GNUC__) || defined(__clang__)
#define OFP_TARGET_AVX2 __attribute__((target("avx2,fma")))
#define OFP_TARGET_AVX512 __attribute__((target("avx512f")))
#define OFP_CAN_COMPILE_AVX2 1
#define OFP_CAN_COMPILE_AVX512 1
#elif defined(_MSC_VER) && defined(__AVX2__)
#define OFP_TARGET_AVX2
#define OFP_TARGET_AVX512
#define OFP_CAN_COMPILE_AVX2 1
#define OFP_CAN_COMPILE_AVX512 0
#else
#define OFP_TARGET_AVX2
#define OFP_TARGET_AVX512
#define OFP_CAN_COMPILE_AVX2 0
#define OFP_CAN_COMPILE_AVX512 0
#endif


#if OFP_CAN_COMPILE_AVX2
template <typename Fn>
OFP_TARGET_AVX2 inline __m256d apply_scalar_fn_256(__m256d v, Fn&& fn) {
    alignas(32) double buf[4];
    _mm256_storeu_pd(buf, v);
    for (int lane = 0; lane < 4; ++lane) {
        buf[lane] = fn(buf[lane]);
    }
    return _mm256_loadu_pd(buf);
}

OFP_TARGET_AVX2 static void calculateAVX2Batch(const OptionBatchView& batch, PricingBatchView& out) {
    const std::size_t lanes = 4;
    const __m256d one = _mm256_set1_pd(1.0);
    const __m256d half = _mm256_set1_pd(0.5);
    const __m256d neg_half = _mm256_set1_pd(-0.5);
    const __m256d inv_sqrt = _mm256_set1_pd(INV_SQRT_2PI);
    const __m256d eps = _mm256_set1_pd(1e-12);
    std::size_t i = 0;
    for (; i + lanes <= batch.count; i += lanes) {
        __m256d spot = _mm256_loadu_pd(batch.spot_ptr + i);
        __m256d strike = _mm256_loadu_pd(batch.strike_ptr + i);
        __m256d T = _mm256_loadu_pd(batch.time_ptr + i);
        __m256d r = _mm256_loadu_pd(batch.rate_ptr + i);
        __m256d sigma = _mm256_loadu_pd(batch.vol_ptr + i);

        __m256d ratio = _mm256_div_pd(spot, strike);
        __m256d ln_ratio = apply_scalar_fn_256(ratio, [](double v) {
            return std::log(v);
        });

        __m256d sqrt_T = _mm256_sqrt_pd(T);
        __m256d sigma_sqrt_T = _mm256_mul_pd(sigma, sqrt_T);
        __m256d sigma_sq = _mm256_mul_pd(sigma, sigma);
        __m256d half_sigma2_T = _mm256_mul_pd(half, _mm256_mul_pd(sigma_sq, T));
        __m256d rT = _mm256_mul_pd(r, T);
        __m256d numerator = _mm256_add_pd(_mm256_add_pd(ln_ratio, rT), half_sigma2_T);
        __m256d d1 = _mm256_div_pd(numerator, _mm256_max_pd(sigma_sqrt_T, eps));
        __m256d d2 = _mm256_sub_pd(d1, sigma_sqrt_T);

        __m256d nd1 = apply_scalar_fn_256(d1, [](double v) {
            return BlackScholes::normalCDF(v);
        });
        __m256d nd2 = apply_scalar_fn_256(d2, [](double v) {
            return BlackScholes::normalCDF(v);
        });
        __m256d n_neg_d1 = _mm256_sub_pd(one, nd1);
        __m256d n_neg_d2 = _mm256_sub_pd(one, nd2);

        __m256d discount = apply_scalar_fn_256(_mm256_sub_pd(_mm256_setzero_pd(), rT), [](double v) {
            return std::exp(v);
        });

        __m256d neg_half_d1_sq = _mm256_mul_pd(neg_half, _mm256_mul_pd(d1, d1));
        __m256d exp_term = apply_scalar_fn_256(neg_half_d1_sq, [](double v) {
            return std::exp(v);
        });
        __m256d phi = _mm256_mul_pd(inv_sqrt, exp_term);

        __m256d call_price = _mm256_sub_pd(_mm256_mul_pd(spot, nd1), _mm256_mul_pd(_mm256_mul_pd(strike, discount), nd2));
        __m256d put_price = _mm256_sub_pd(_mm256_mul_pd(_mm256_mul_pd(strike, discount), n_neg_d2), _mm256_mul_pd(spot, n_neg_d1));

        __m256d call_delta = nd1;
        __m256d put_delta = _mm256_sub_pd(nd1, one);
        __m256d denom = _mm256_max_pd(_mm256_mul_pd(spot, sigma_sqrt_T), eps);
        __m256d gamma = _mm256_div_pd(phi, denom);
        __m256d vega = _mm256_mul_pd(_mm256_mul_pd(spot, phi), sqrt_T);
        __m256d theta_first = _mm256_div_pd(_mm256_mul_pd(_mm256_mul_pd(spot, phi), _mm256_mul_pd(sigma, _mm256_set1_pd(-1.0))), _mm256_mul_pd(_mm256_set1_pd(2.0), _mm256_max_pd(sqrt_T, eps)));
        __m256d theta_second = _mm256_mul_pd(_mm256_mul_pd(_mm256_mul_pd(r, strike), discount), nd2);
        __m256d theta = _mm256_sub_pd(theta_first, theta_second);
        __m256d rho_call = _mm256_mul_pd(_mm256_mul_pd(_mm256_mul_pd(strike, T), discount), nd2);
        __m256d rho_put = _mm256_mul_pd(_mm256_set1_pd(-1.0), _mm256_mul_pd(_mm256_mul_pd(_mm256_mul_pd(strike, T), discount), n_neg_d2));

        __m256d safe_sigma = _mm256_max_pd(sigma, eps);
        __m256d inv_sigma = _mm256_div_pd(one, safe_sigma);
        __m256d vanna = _mm256_mul_pd(phi, _mm256_sub_pd(sqrt_T, _mm256_mul_pd(d1, inv_sigma)));
        __m256d vomma = _mm256_mul_pd(vega, _mm256_mul_pd(d1, _mm256_mul_pd(d2, inv_sigma)));

        _mm256_storeu_pd(out.call_ptr + i, call_price);
        _mm256_storeu_pd(out.put_ptr + i, put_price);
        _mm256_storeu_pd(out.delta_ptr + i, call_delta);
        _mm256_storeu_pd(out.put_delta_ptr + i, put_delta);
        _mm256_storeu_pd(out.gamma_ptr + i, gamma);
        _mm256_storeu_pd(out.vega_ptr + i, vega);
        _mm256_storeu_pd(out.theta_ptr + i, theta);
        _mm256_storeu_pd(out.rho_call_ptr + i, rho_call);
        _mm256_storeu_pd(out.rho_put_ptr + i, rho_put);
        _mm256_storeu_pd(out.vanna_ptr + i, vanna);
        _mm256_storeu_pd(out.vomma_ptr + i, vomma);
    }

    if (i < batch.count) {
        for (std::size_t j = i; j < batch.count; ++j) {
            OptionParams params{};
            params.spot_price = batch.spot_ptr[j];
            params.strike_price = batch.strike_ptr[j];
            params.time_to_expiry = batch.time_ptr[j];
            params.risk_free_rate = batch.rate_ptr[j];
            params.volatility = batch.vol_ptr[j];
            PricingResult res = BlackScholes::calculate(params);
            out.call_ptr[j] = res.call_price;
            out.put_ptr[j] = res.put_price;
            out.delta_ptr[j] = res.call_delta;
            out.put_delta_ptr[j] = res.put_delta;
            out.gamma_ptr[j] = res.gamma;
            out.vega_ptr[j] = res.vega;
            out.theta_ptr[j] = res.theta;
            out.rho_call_ptr[j] = res.rho_call;
            out.rho_put_ptr[j] = res.rho_put;
            out.vanna_ptr[j] = res.vanna;
            out.vomma_ptr[j] = res.vomma;
        }
    }
}
#endif

#if OFP_CAN_COMPILE_AVX512
template <typename Fn>
OFP_TARGET_AVX512 inline __m512d apply_scalar_fn_512(__m512d v, Fn&& fn) {
    alignas(64) double buf[8];
    _mm512_storeu_pd(buf, v);
    for (int lane = 0; lane < 8; ++lane) {
        buf[lane] = fn(buf[lane]);
    }
    return _mm512_loadu_pd(buf);
}

OFP_TARGET_AVX512 static void calculateAVX512Batch(const OptionBatchView& batch, PricingBatchView& out) {
    const std::size_t lanes = 8;
    const __m512d one = _mm512_set1_pd(1.0);
    const __m512d half = _mm512_set1_pd(0.5);
    const __m512d neg_half = _mm512_set1_pd(-0.5);
    const __m512d inv_sqrt = _mm512_set1_pd(INV_SQRT_2PI);
    const __m512d eps = _mm512_set1_pd(1e-12);
    std::size_t i = 0;
    for (; i + lanes <= batch.count; i += lanes) {
        __m512d spot = _mm512_loadu_pd(batch.spot_ptr + i);
        __m512d strike = _mm512_loadu_pd(batch.strike_ptr + i);
        __m512d T = _mm512_loadu_pd(batch.time_ptr + i);
        __m512d r = _mm512_loadu_pd(batch.rate_ptr + i);
        __m512d sigma = _mm512_loadu_pd(batch.vol_ptr + i);

        __m512d ratio = _mm512_div_pd(spot, strike);
        __m512d ln_ratio = apply_scalar_fn_512(ratio, [](double v) {
            return std::log(v);
        });

        __m512d sqrt_T = _mm512_sqrt_pd(T);
        __m512d sigma_sqrt_T = _mm512_mul_pd(sigma, sqrt_T);
        __m512d sigma_sq = _mm512_mul_pd(sigma, sigma);
        __m512d half_sigma2_T = _mm512_mul_pd(half, _mm512_mul_pd(sigma_sq, T));
        __m512d rT = _mm512_mul_pd(r, T);
        __m512d numerator = _mm512_add_pd(_mm512_add_pd(ln_ratio, rT), half_sigma2_T);
        __m512d d1 = _mm512_div_pd(numerator, _mm512_max_pd(sigma_sqrt_T, eps));
        __m512d d2 = _mm512_sub_pd(d1, sigma_sqrt_T);

        __m512d nd1 = apply_scalar_fn_512(d1, [](double v) {
            return BlackScholes::normalCDF(v);
        });
        __m512d nd2 = apply_scalar_fn_512(d2, [](double v) {
            return BlackScholes::normalCDF(v);
        });
        __m512d n_neg_d1 = _mm512_sub_pd(one, nd1);
        __m512d n_neg_d2 = _mm512_sub_pd(one, nd2);

        __m512d discount = apply_scalar_fn_512(_mm512_sub_pd(_mm512_setzero_pd(), rT), [](double v) {
            return std::exp(v);
        });

        __m512d neg_half_d1_sq = _mm512_mul_pd(neg_half, _mm512_mul_pd(d1, d1));
        __m512d exp_term = apply_scalar_fn_512(neg_half_d1_sq, [](double v) {
            return std::exp(v);
        });
        __m512d phi = _mm512_mul_pd(inv_sqrt, exp_term);

        __m512d call_price = _mm512_sub_pd(_mm512_mul_pd(spot, nd1), _mm512_mul_pd(_mm512_mul_pd(strike, discount), nd2));
        __m512d put_price = _mm512_sub_pd(_mm512_mul_pd(_mm512_mul_pd(strike, discount), n_neg_d2), _mm512_mul_pd(spot, n_neg_d1));

        __m512d call_delta = nd1;
        __m512d put_delta = _mm512_sub_pd(nd1, one);
        __m512d denom = _mm512_max_pd(_mm512_mul_pd(spot, sigma_sqrt_T), eps);
        __m512d gamma = _mm512_div_pd(phi, denom);
        __m512d vega = _mm512_mul_pd(_mm512_mul_pd(spot, phi), sqrt_T);
        __m512d theta_first = _mm512_div_pd(_mm512_mul_pd(_mm512_mul_pd(spot, phi), _mm512_mul_pd(sigma, _mm512_set1_pd(-1.0))), _mm512_mul_pd(_mm512_set1_pd(2.0), _mm512_max_pd(sqrt_T, eps)));
        __m512d theta_second = _mm512_mul_pd(_mm512_mul_pd(_mm512_mul_pd(r, strike), discount), nd2);
        __m512d theta = _mm512_sub_pd(theta_first, theta_second);
        __m512d rho_call = _mm512_mul_pd(_mm512_mul_pd(_mm512_mul_pd(strike, T), discount), nd2);
        __m512d rho_put = _mm512_mul_pd(_mm512_set1_pd(-1.0), _mm512_mul_pd(_mm512_mul_pd(_mm512_mul_pd(strike, T), discount), n_neg_d2));

        __m512d safe_sigma = _mm512_max_pd(sigma, eps);
        __m512d inv_sigma = _mm512_div_pd(one, safe_sigma);
        __m512d vanna = _mm512_mul_pd(phi, _mm512_sub_pd(sqrt_T, _mm512_mul_pd(d1, inv_sigma)));
        __m512d vomma = _mm512_mul_pd(vega, _mm512_mul_pd(d1, _mm512_mul_pd(d2, inv_sigma)));

        _mm512_storeu_pd(out.call_ptr + i, call_price);
        _mm512_storeu_pd(out.put_ptr + i, put_price);
        _mm512_storeu_pd(out.delta_ptr + i, call_delta);
        _mm512_storeu_pd(out.put_delta_ptr + i, put_delta);
        _mm512_storeu_pd(out.gamma_ptr + i, gamma);
        _mm512_storeu_pd(out.vega_ptr + i, vega);
        _mm512_storeu_pd(out.theta_ptr + i, theta);
        _mm512_storeu_pd(out.rho_call_ptr + i, rho_call);
        _mm512_storeu_pd(out.rho_put_ptr + i, rho_put);
        _mm512_storeu_pd(out.vanna_ptr + i, vanna);
        _mm512_storeu_pd(out.vomma_ptr + i, vomma);
    }

    if (i < batch.count) {
        for (std::size_t j = i; j < batch.count; ++j) {
            OptionParams params{};
            params.spot_price = batch.spot_ptr[j];
            params.strike_price = batch.strike_ptr[j];
            params.time_to_expiry = batch.time_ptr[j];
            params.risk_free_rate = batch.rate_ptr[j];
            params.volatility = batch.vol_ptr[j];
            PricingResult res = BlackScholes::calculate(params);
            out.call_ptr[j] = res.call_price;
            out.put_ptr[j] = res.put_price;
            out.delta_ptr[j] = res.call_delta;
            out.put_delta_ptr[j] = res.put_delta;
            out.gamma_ptr[j] = res.gamma;
            out.vega_ptr[j] = res.vega;
            out.theta_ptr[j] = res.theta;
            out.rho_call_ptr[j] = res.rho_call;
            out.rho_put_ptr[j] = res.rho_put;
            out.vanna_ptr[j] = res.vanna;
            out.vomma_ptr[j] = res.vomma;
        }
    }
}
#endif


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
        if constexpr (LOG_CDF_CALIBRATION) {
            if (i % 100 == 0 || i == 0) {
                printf("Iteration %d: Error = %.10e\n", i, error);
                printf("  p=%.9f, a1=%.9f, a2=%.9f, a3=%.9f, a4=%.9f, a5=%.9f\n",
                       CDF.p, CDF.a1, CDF.a2, CDF.a3, CDF.a4, CDF.a5);
            }
            if (error < 1e-15) {
                printf("Converged at iteration %d with error %.10e\n", i, error);
                break;
            }
        } else if (error < 1e-15) {
            break;
        }
    }

    // Use best parameters found
    CDF = best_CDF;
    if constexpr (LOG_CDF_CALIBRATION) {
        printf("Final error: %.10e\n", best_error);
    }
}

CDF_precompute BlackScholes::computeCoefficents(){
    return CDF_precompute{
        0.2316419,
        0.31938153,
        -0.356563782,
        1.781477937,
        -1.821255978,
        1.330274429
    };
}



double BlackScholes::normalCDF(double x){
    return 0.5 * std::erfc(-x * M_SQRT1_2);
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
    
    const double safe_T = std::max(T, 0.0);
    const double sqrt_T = std::sqrt(safe_T);
    double ratio = S / K;
    if (!(ratio > 0.0)) {
        ratio = std::numeric_limits<double>::min();
    }
    const double ln_S_over_K = std::log(ratio);

    const double sigma_sqrt_T = sigma * sqrt_T;
    const double eps = 1e-8;
    if (sigma_sqrt_T < eps) {
        const double discount = std::exp(-r * safe_T);
        const double intrinsic = S - K * discount;
        if (intrinsic > 0.0) {
            d1 = std::numeric_limits<double>::infinity();
        } else {
            d1 = -std::numeric_limits<double>::infinity();
        }
        d2 = d1;
        return;
    }
    const double safe_sigma_sqrt_T = sigma_sqrt_T;
    const double half_sigma2_T = 0.5 * sigma * sigma * safe_T;
    const double r_T = r * safe_T;

    d1 = (ln_S_over_K + r_T + half_sigma2_T) / safe_sigma_sqrt_T;
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
    const double sqrt_T = std::sqrt(std::max(T, 0.0));
    double sigma_sqrt_T = sigma * sqrt_T;
    
    // Standard normal PDF at d1: phi(d1) = (1/sqrt(2*pi)) * exp(-d1^2/2)
    double phi_d1 = normal_pdf(d1);
    
    // Greeks calculations
    result.call_delta = nd1;                    // dC/dS
    result.put_delta = nd1 - 1.0;              // dP/dS
    result.gamma = phi_d1 / (S * sigma_sqrt_T); // d^2C/dS^2 (same for calls and puts)
    result.vega = S * phi_d1 * sqrt_T;          // dC/d(sigma)
    result.theta = -S * phi_d1 * sigma / (2.0 * sqrt_T) - r * K * discount_factor * nd2; // dC/dT (call)
    result.rho_call = K * T * discount_factor * nd2;     // dC/dr
    result.rho_put = -K * T * discount_factor * n_neg_d2; // dP/dr
    double safe_sigma = sigma > 1e-12 ? sigma : 1e-12;
    double inv_sigma = 1.0 / safe_sigma;
    result.vanna = phi_d1 * (sqrt_T - d1 * inv_sigma);
    result.vomma = result.vega * d1 * d2 * inv_sigma;
    
    return result;
}

TaylorGreeks BlackScholes::snapshotTaylorGreeks(const OptionParams& params,
                                                const PricingResult& pricing) {
    TaylorGreeks state{};
    state.spot = params.spot_price;
    state.strike = params.strike_price;
    state.sigma = params.volatility;
    int days = FastMath::years_to_days(params.time_to_expiry);
    state.sqrt_time = FastMath::fast_sqrt_days(std::clamp(days, 0, 1825));
    state.call_price = pricing.call_price;
    state.put_price = pricing.put_price;
    state.call_delta = pricing.call_delta;
    state.put_delta = pricing.put_delta;
    state.gamma = pricing.gamma;
    state.vega = pricing.vega;
    state.theta = pricing.theta;
    state.rho_call = pricing.rho_call;
    state.rho_put = pricing.rho_put;
    state.vanna = pricing.vanna;
    state.vomma = pricing.vomma;
    return state;
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

void BlackScholes::taylor_update(const TaylorGreeks& base_state,
                                 double dS,
                                 double dSigma,
                                 PricingResult& out) {
    double ds2 = dS * dS;
    double dsigma2 = dSigma * dSigma;
    double cross = dS * dSigma;
    double gamma_term = 0.5 * base_state.gamma * ds2;
    double vomma_term = 0.5 * base_state.vomma * dsigma2;
    double vega_term = base_state.vega * dSigma;
    double cross_term = base_state.vanna * cross;

    out.call_price = base_state.call_price + base_state.call_delta * dS +
                     gamma_term + vega_term + vomma_term + cross_term;
    out.put_price = base_state.put_price + base_state.put_delta * dS +
                    gamma_term + vega_term + vomma_term + cross_term;

    double delta_shift = base_state.gamma * dS + base_state.vanna * dSigma;
    out.call_delta = base_state.call_delta + delta_shift;
    out.put_delta = base_state.put_delta + delta_shift;
    out.gamma = base_state.gamma;
    out.vega = base_state.vega + base_state.vanna * dS + base_state.vomma * dSigma;
    out.theta = base_state.theta;
    out.rho_call = base_state.rho_call;
    out.rho_put = base_state.rho_put;
    out.vanna = base_state.vanna;
    out.vomma = base_state.vomma;
}

void BlackScholes::taylor_update_batch(const TaylorGreeks* base_states,
                                       const double* dS,
                                       const double* dSigma,
                                       std::size_t count,
                                       PricingBatchView& out) {
    if (count == 0) {
        return;
    }
    if (!base_states || !dS || !dSigma) {
        throw std::invalid_argument("Taylor batch inputs cannot be null");
    }
    if (out.count < count) {
        throw std::invalid_argument("Result view smaller than Taylor batch");
    }
    std::size_t stride = out.stride == 0 ? 1 : out.stride;
    if (!out.call_ptr || !out.put_ptr || !out.delta_ptr || !out.put_delta_ptr || !out.gamma_ptr ||
        !out.vega_ptr || !out.theta_ptr || !out.rho_call_ptr || !out.rho_put_ptr ||
        !out.vanna_ptr || !out.vomma_ptr) {
        throw std::invalid_argument("Taylor batch output pointers cannot be null");
    }
    PricingResult tmp{};
    for (std::size_t i = 0; i < count; ++i) {
        taylor_update(base_states[i], dS[i], dSigma[i], tmp);
        std::size_t idx = i * stride;
        out.call_ptr[idx] = tmp.call_price;
        out.put_ptr[idx] = tmp.put_price;
        out.delta_ptr[idx] = tmp.call_delta;
        out.put_delta_ptr[idx] = tmp.put_delta;
        out.gamma_ptr[idx] = tmp.gamma;
        out.vega_ptr[idx] = tmp.vega;
        out.theta_ptr[idx] = tmp.theta;
        out.rho_call_ptr[idx] = tmp.rho_call;
        out.rho_put_ptr[idx] = tmp.rho_put;
        out.vanna_ptr[idx] = tmp.vanna;
        out.vomma_ptr[idx] = tmp.vomma;
    }
}

void BlackScholes::calculate_batch(const OptionBatchView& batch, PricingBatchView& out) {
    ensureBatchView(batch, out);
    if (batch.count == 0) {
        return;
    }
    KernelCaps caps = queryCaps();
    bool has_avx512 = caps.avx512 && OFP_CAN_COMPILE_AVX512;
    bool has_avx2 = caps.avx2 && OFP_CAN_COMPILE_AVX2;
    bool can_vec = (batch.stride <= 1 && out.stride <= 1) && (has_avx512 || has_avx2);
    if (can_vec) {
        calculateSIMD(batch, out);
    } else {
        calculateScalarBatch(batch, out);
    }
}

void BlackScholes::calculateScalarBatch(const OptionBatchView& batch, PricingBatchView& out) {
    std::size_t in_stride = batch.stride == 0 ? 1 : batch.stride;
    std::size_t out_stride = out.stride == 0 ? 1 : out.stride;
    for (std::size_t i = 0; i < batch.count; ++i) {
        std::size_t in_idx = i * in_stride;
        OptionParams params{};
        params.spot_price = batch.spot_ptr[in_idx];
        params.strike_price = batch.strike_ptr[in_idx];
        params.time_to_expiry = batch.time_ptr[in_idx];
        params.risk_free_rate = batch.rate_ptr[in_idx];
        params.volatility = batch.vol_ptr[in_idx];
        PricingResult pricing = calculate(params);
        std::size_t out_idx = i * out_stride;
        out.call_ptr[out_idx] = pricing.call_price;
        out.put_ptr[out_idx] = pricing.put_price;
        out.delta_ptr[out_idx] = pricing.call_delta;
        out.put_delta_ptr[out_idx] = pricing.put_delta;
        out.gamma_ptr[out_idx] = pricing.gamma;
        out.vega_ptr[out_idx] = pricing.vega;
        out.theta_ptr[out_idx] = pricing.theta;
        out.rho_call_ptr[out_idx] = pricing.rho_call;
        out.rho_put_ptr[out_idx] = pricing.rho_put;
        out.vanna_ptr[out_idx] = pricing.vanna;
        out.vomma_ptr[out_idx] = pricing.vomma;
    }
}

void BlackScholes::calculateSIMD(const OptionBatchView& batch, PricingBatchView& out) {
    KernelCaps caps = queryCaps();
#if OFP_CAN_COMPILE_AVX512
    if (caps.avx512) {
        calculateAVX512Batch(batch, out);
        return;
    }
#endif
#if OFP_CAN_COMPILE_AVX2
    if (caps.avx2) {
        calculateAVX2Batch(batch, out);
        return;
    }
#endif
    calculateScalarBatch(batch, out);
}

BlackScholes::KernelCaps BlackScholes::queryCaps() {
    static const KernelCaps caps = []() {
        KernelCaps detected{};
#if defined(_MSC_VER)
        int cpu_info[4] = {0};
        __cpuidex(cpu_info, 0, 0);
        int max_leaf = cpu_info[0];
        if (max_leaf >= 7) {
            __cpuidex(cpu_info, 7, 0);
            detected.avx2 = (cpu_info[1] & (1 << 5)) != 0;
            detected.avx512 = (cpu_info[1] & (1 << 16)) != 0;
        }
#else
        unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
        unsigned int max_leaf = __get_cpuid_max(0, nullptr);
        if (max_leaf >= 7) {
            __cpuid_count(7, 0, eax, ebx, ecx, edx);
            detected.avx2 = (ebx & (1u << 5)) != 0;
            detected.avx512 = (ebx & (1u << 16)) != 0;
        }
#endif
        return detected;
    }();
    return caps;
}

void BlackScholes::ensureBatchView(const OptionBatchView& batch, const PricingBatchView& out) {
    if (batch.count != out.count) {
        throw std::invalid_argument("Batch input/output mismatch");
    }
    if (batch.count == 0) {
        return;
    }
    if (!batch.spot_ptr || !batch.strike_ptr || !batch.time_ptr ||
        !batch.rate_ptr || !batch.vol_ptr) {
        throw std::invalid_argument("Batch option pointers cannot be null");
    }
    if (!out.call_ptr || !out.put_ptr || !out.delta_ptr || !out.put_delta_ptr || !out.gamma_ptr ||
        !out.vega_ptr || !out.theta_ptr || !out.rho_call_ptr || !out.rho_put_ptr ||
        !out.vanna_ptr || !out.vomma_ptr) {
        throw std::invalid_argument("Batch output pointers cannot be null");
    }
}

//ending namespace
    }
}