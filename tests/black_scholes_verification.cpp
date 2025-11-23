#include "black_scholes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace order_flow::pricing;
using namespace order_flow::types;

constexpr double kInvSqrt2Pi = 0.39894228040143267794;

double phi(double x) {
    return kInvSqrt2Pi * std::exp(-0.5 * x * x);
}

double Phi(double x) {
    return 0.5 * std::erfc(-x * M_SQRT1_2);
}

struct Reference {
    double call = 0.0;
    double put = 0.0;
    double call_delta = 0.0;
    double put_delta = 0.0;
    double gamma = 0.0;
    double vega = 0.0;
};

Reference reference_price(const OptionParams& params) {
    Reference ref{};
    const double S = params.spot_price;
    const double K = params.strike_price;
    const double r = params.risk_free_rate;
    const double T = std::max(params.time_to_expiry, 0.0);
    const double sigma = std::max(params.volatility, 0.0);
    const double sqrtT = std::sqrt(T);
    const double sigs = sigma * sqrtT;
    const double df = std::exp(-r * T);

    if (sigs < 1e-12) {
        const double call_intrinsic = std::max(S - K * df, 0.0);
        const double put_intrinsic = std::max(K * df - S, 0.0);
        ref.call = call_intrinsic;
        ref.put = put_intrinsic;
        ref.call_delta = (call_intrinsic > 0.0) ? 1.0 : 0.0;
        ref.put_delta = ref.call_delta - 1.0;
        ref.gamma = 0.0;
        ref.vega = 0.0;
        return ref;
    }

    const double lnSK = std::log(S / K);
    const double d1 = (lnSK + (r + 0.5 * sigma * sigma) * T) / sigs;
    const double d2 = d1 - sigs;
    const double Nd1 = Phi(d1);
    const double Nd2 = Phi(d2);
    const double Nmd1 = Phi(-d1);
    const double Nmd2 = Phi(-d2);
    const double pdf_d1 = phi(d1);

    ref.call = S * Nd1 - K * df * Nd2;
    ref.put = K * df * Nmd2 - S * Nmd1;
    ref.call_delta = Nd1;
    ref.put_delta = Nd1 - 1.0;
    ref.gamma = pdf_d1 / (S * sigs);
    ref.vega = S * pdf_d1 * sqrtT;
    return ref;
}

bool approx_equal(double lhs, double rhs, double rel = 1e-7, double abs = 1e-10) {
    const double diff = std::abs(lhs - rhs);
    return diff <= abs || diff <= rel * std::max({1.0, std::abs(lhs), std::abs(rhs)});
}

[[noreturn]] void fail(const std::string& msg) {
    throw std::runtime_error(msg);
}

void expect(bool cond, const std::string& msg) {
    if (!cond) {
        fail(msg);
    }
}

void expect_close(double lhs, double rhs, double rel, double abs, const std::string& label) {
    if (!approx_equal(lhs, rhs, rel, abs)) {
        std::ostringstream oss;
        oss.setf(std::ios::fixed, std::ios::floatfield);
        oss << std::setprecision(12);
        oss << label << " mismatch: lhs=" << lhs
            << " rhs=" << rhs
            << " diff=" << std::abs(lhs - rhs)
            << " rel_tol=" << rel
            << " abs_tol=" << abs;
        fail(oss.str());
    }
}

struct Scenario {
    double spot;
    double strike;
    double time;
    double rate;
    double vol;
};

void run_parity_and_bounds_suite() {
    const std::array<Scenario, 6> cases{{
        {100.0, 100.0, 0.50, 0.00, 0.20},
        {100.0, 100.0, 1.00, 0.01, 0.30},
        {100.0, 120.0, 2.00, 0.02, 0.15},
        {100.0, 80.00, 0.25, 0.03, 0.50},
        {50.00, 100.0, 3.00, 0.00, 0.80},
        {150.0, 50.00, 0.05, 0.00, 0.10}
    }};

    int case_idx = 0;
    for (const auto& c : cases) {
        OptionParams params{};
        params.spot_price = c.spot;
        params.strike_price = c.strike;
        params.time_to_expiry = c.time;
        params.risk_free_rate = c.rate;
        params.volatility = c.vol;

        const PricingResult engine = BlackScholes::calculate(params);
        const Reference ref = reference_price(params);
        const double df = std::exp(-params.risk_free_rate * params.time_to_expiry);
        std::ostringstream ctx;
        ctx.setf(std::ios::fixed, std::ios::floatfield);
        ctx << std::setprecision(6)
            << "[case=" << case_idx
            << ", S=" << params.spot_price
            << ", K=" << params.strike_price
            << ", r=" << params.risk_free_rate
            << ", T=" << params.time_to_expiry
            << ", sigma=" << params.volatility << "] ";
        auto label = [&](std::string base) {
            return ctx.str() + base;
        };

        expect_close(engine.call_price, ref.call, 5e-7, 5e-9, label("call price"));
        expect_close(engine.put_price, ref.put, 5e-7, 5e-9, label("put price"));
        expect_close(engine.call_price - engine.put_price,
             params.spot_price - params.strike_price * df,
                 5e-7,
                 5e-9,
                 label("put-call parity"));

        expect(engine.call_price >= -1e-9, "call negative");
        expect(engine.put_price >= -1e-9, "put negative");
        expect(engine.call_price <= params.spot_price + 1e-9, label("call exceeds spot"));
        expect(engine.put_price <= params.strike_price * df + 1e-9, label("put exceeds PV strike"));

        expect_close(engine.call_delta, ref.call_delta, 3e-6, 1e-8, label("delta (call)"));
        expect_close(engine.put_delta, ref.put_delta, 3e-6, 1e-8, label("delta (put)"));
        expect_close(engine.gamma, ref.gamma, 1e-5, 1e-8, label("gamma"));
        expect_close(engine.vega, ref.vega, 1e-5, 1e-8, label("vega"));
        ++case_idx;
    }
}

void run_extreme_limit_suite() {
    // T -> 0
    {
        OptionParams params{};
        params.spot_price = 100.0;
        params.strike_price = 95.0;
        params.time_to_expiry = 1e-9;
        params.risk_free_rate = 0.0;
        params.volatility = 0.2;
        const PricingResult out = BlackScholes::calculate(params);
        const double intrinsic = std::max(params.spot_price - params.strike_price, 0.0);
        expect(std::abs(out.call_price - intrinsic) < 1e-6, "T->0 price should be intrinsic");
        expect(out.vega < 1e-6, "T->0 vega collapse");
    }

    // sigma -> 0
    {
        OptionParams params{};
        params.spot_price = 100.0;
        params.strike_price = 100.0;
        params.time_to_expiry = 1.0;
        params.risk_free_rate = 0.0;
        params.volatility = 1e-12;
        const PricingResult out = BlackScholes::calculate(params);
        const double df = std::exp(-params.risk_free_rate * params.time_to_expiry);
        const double intrinsic = std::max(params.spot_price - params.strike_price * df, 0.0);
        if (std::abs(out.call_price - intrinsic) >= 1e-6) {
            std::ostringstream oss;
            oss << "sigma->0 price mismatch: got=" << out.call_price << " intrinsic=" << intrinsic;
            fail(oss.str());
        }
        if (!(out.vega < 1e-6)) {
            std::ostringstream oss;
            oss << "sigma->0 vega collapse: got=" << out.vega;
            fail(oss.str());
        }
    }
}

struct BatchStorage {
    explicit BatchStorage(std::size_t size)
        : call(size),
          put(size),
          delta(size),
          put_delta(size),
          gamma(size),
          vega(size),
          theta(size),
          rho_call(size),
          rho_put(size),
          vanna(size),
          vomma(size) {}

    PricingBatchView view(std::size_t count, std::size_t stride) {
        return PricingBatchView{
            call.data(),
            put.data(),
            delta.data(),
            put_delta.data(),
            gamma.data(),
            vega.data(),
            theta.data(),
            rho_call.data(),
            rho_put.data(),
            vanna.data(),
            vomma.data(),
            count,
            stride
        };
    }

    std::vector<double> call;
    std::vector<double> put;
    std::vector<double> delta;
    std::vector<double> put_delta;
    std::vector<double> gamma;
    std::vector<double> vega;
    std::vector<double> theta;
    std::vector<double> rho_call;
    std::vector<double> rho_put;
    std::vector<double> vanna;
    std::vector<double> vomma;
};

void run_batch_vs_scalar_suite() {
    constexpr std::size_t N = 128;
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> spot_dist(25.0, 200.0);
    std::uniform_real_distribution<double> strike_dist(50.0, 220.0);
    std::uniform_real_distribution<double> time_dist(1.0 / 365.0, 2.0);
    std::uniform_real_distribution<double> rate_dist(0.0, 0.05);
    std::uniform_real_distribution<double> sigma_dist(0.05, 0.9);

    std::vector<double> spot(N), strike(N), expiry(N), rate(N), sigma(N);
    for (std::size_t i = 0; i < N; ++i) {
        spot[i] = spot_dist(rng);
        strike[i] = strike_dist(rng);
        expiry[i] = time_dist(rng);
        rate[i] = rate_dist(rng);
        sigma[i] = sigma_dist(rng);
    }

    OptionBatchView simd_batch{
        spot.data(),
        strike.data(),
        expiry.data(),
        rate.data(),
        sigma.data(),
        N,
        1
    };

    BatchStorage simd_storage(N);
    PricingBatchView simd_view = simd_storage.view(N, 1);
    BlackScholes::calculate_batch(simd_batch, simd_view);

    std::vector<double> spot_scalar(N * 2, 0.0);
    std::vector<double> strike_scalar(N * 2, 0.0);
    std::vector<double> expiry_scalar(N * 2, 0.0);
    std::vector<double> rate_scalar(N * 2, 0.0);
    std::vector<double> sigma_scalar(N * 2, 0.0);
    for (std::size_t i = 0; i < N; ++i) {
        const std::size_t idx = i * 2;
        spot_scalar[idx] = spot[i];
        strike_scalar[idx] = strike[i];
        expiry_scalar[idx] = expiry[i];
        rate_scalar[idx] = rate[i];
        sigma_scalar[idx] = sigma[i];
    }

    OptionBatchView scalar_batch{
        spot_scalar.data(),
        strike_scalar.data(),
        expiry_scalar.data(),
        rate_scalar.data(),
        sigma_scalar.data(),
        N,
        2
    };

    BatchStorage scalar_storage(N * 2);
    PricingBatchView scalar_view = scalar_storage.view(N, 2);
    BlackScholes::calculate_batch(scalar_batch, scalar_view);

    auto lane = [&](const std::vector<double>& simd, const std::vector<double>& scalar, const std::string& name) {
        for (std::size_t i = 0; i < N; ++i) {
            const double lhs = simd[i];
            const double rhs = scalar[i * 2];
            if (!approx_equal(lhs, rhs, 5e-7, 5e-9)) {
                std::string msg = "SIMD vs scalar mismatch in " + name + " at idx=" + std::to_string(i);
                fail(msg);
            }
        }
    };

    lane(simd_storage.call, scalar_storage.call, "call");
    lane(simd_storage.put, scalar_storage.put, "put");
    lane(simd_storage.delta, scalar_storage.delta, "call_delta");
    lane(simd_storage.put_delta, scalar_storage.put_delta, "put_delta");
    lane(simd_storage.gamma, scalar_storage.gamma, "gamma");
    lane(simd_storage.vega, scalar_storage.vega, "vega");
    lane(simd_storage.theta, scalar_storage.theta, "theta");
    lane(simd_storage.rho_call, scalar_storage.rho_call, "rho_call");
    lane(simd_storage.rho_put, scalar_storage.rho_put, "rho_put");
    lane(simd_storage.vanna, scalar_storage.vanna, "vanna");
    lane(simd_storage.vomma, scalar_storage.vomma, "vomma");
}

} // namespace

int main() {
    try {
        run_parity_and_bounds_suite();
        run_extreme_limit_suite();
        run_batch_vs_scalar_suite();
        std::cout << "Black-Scholes verification suite passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Verification failure: " << ex.what() << "\n";
        return 1;
    }
}
