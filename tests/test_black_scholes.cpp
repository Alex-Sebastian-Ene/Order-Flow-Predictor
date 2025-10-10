// g++ -O3 -march=native -DNDEBUG -std=c++20 test_black_scholes.cpp black_scholes.cpp -o test_bs
#includ            CHECK_        const double df = std::exp(-c.r*c.T);
        CHECK( approx_equal(y.call_price - y.put_price, c.S - c.K*df, 5e-7, 5e-9), "put-call parity");
        CHECK( y.call_price >= 0.0 && y.put_price >= 0.0, "non-negativity");
        CHECK( y.call_price <= c.S+1e-9, "call <= S");
        CHECK( y.put_price  <= c.K*df+1e-9, "put <= K*e^(-rT)");

        // Greeks vs reference (vega/gamma/theta tolerances a bit looser; they're sensitive to CDF/pdf accuracy)
        CHECK_CLOSE(y.call_delta, r.delta_c, 3e-6, 1e-8, "delta(call)");call_price, r.call, 5e-7, 5e-10, "call price");
        CHECK_CLOSE(y.put_price,  r.put,  5e-7, 5e-10, "put  price");

        // Parity & bounds
        const double df = std::exp(-c.r*c.T);
        CHECK( approx_equal(y.call_price - y.put_price, c.S - c.K*df, 5e-7, 5e-9), "put-call parity");
        CHECK( y.call_price >= 0.0 && y.put_price >= 0.0, "non-negativity");
        CHECK( y.call_price <= c.S+1e-9, "call <= S");
        CHECK( y.put_price  <= c.K*df+1e-9, "put <= K*e^(-rT)");

        // Greeks vs reference (vega/gamma/theta tolerances a bit looser; they're sensitive to CDF/pdf accuracy)ty & bounds
        const double df = std::exp(-c.r*c.T);
        CHECK( approx_equal(y.call_price - y.put_price, c.S - c.K*df, 5e-7, 5e-9), "put-call parity");
        CHECK( y.call_price >= 0.0 && y.put_price >= 0.0, "non-negativity");
        CHECK( y.call_price <= c.S+1e-9, "call <= S");
        CHECK( y.put_price  <= c.K*df+1e-9, "put <= K e^{-rT}");

        // Greeks vs reference (vega/gamma/theta tolerances a bit looser; they're sensitive to CDF/pdf accuracy)>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <limits>
#include <cassert>


#include "black_scholes.hpp"

using std::cout;
using std::endl;

namespace ref {
// High-precision reference building blocks (no approximations)
inline double Phi(double x) { return 0.5 * std::erfc(-x * M_SQRT1_2); }
inline double phi(double x) { static constexpr double INV_SQRT_2PI = 0.39894228040143267794; return INV_SQRT_2PI * std::exp(-0.5 * x*x); }

struct Out {
    double call, put;
    double d1, d2;
    double delta_c, delta_p;
    double gamma, vega, theta_c, rho_c, rho_p;
};

inline Out bs(double S,double K,double r,double T,double sigma) {
    Out o{};
    const double sqrtT = (T>0.0)? std::sqrt(T) : 0.0;
    const double sigs = sigma * sqrtT;
    const double lnSK = std::log(S / K);
    const double muT  = (r + 0.5 * sigma * sigma) * T;
    const double inv  = (sigs>0.0)? 1.0 / sigs : 0.0;
    o.d1 = (lnSK + muT) * inv;
    o.d2 = o.d1 - sigs;

    const double Nd1 = Phi(o.d1), Nd2 = Phi(o.d2);
    const double Nmd1 = Phi(-o.d1), Nmd2 = Phi(-o.d2);
    const double df = std::exp(-r*T);

    // Prices
    o.call = S * Nd1 - K * df * Nd2;
    o.put  = K * df * Nmd2 - S * Nmd1;

    // Greeks (call)
    const double pdf_d1 = phi(o.d1);
    o.delta_c = Nd1;
    o.delta_p = Nd1 - 1.0;
    o.gamma   = (sigs>0.0)? pdf_d1 / (S * sigs) : std::numeric_limits<double>::infinity();
    o.vega    = S * pdf_d1 * sqrtT;
    o.theta_c = -S * pdf_d1 * sigma / (2.0 * sqrtT + (T==0.0)) - r * K * df * Nd2;  // guard T=0 in denom
    o.rho_c   = K * T * df * Nd2;
    o.rho_p   = -K * T * df * Nmd2;
    return o;
}
} // namespace ref

// Tiny test helpers
inline bool approx_equal(double a, double b, double rel=1e-7, double abs=1e-10) {
    double diff = std::abs(a - b);
    return diff <= abs || diff <= rel * std::max({1.0, std::abs(a), std::abs(b)});
}
#define CHECK(cond, msg) do { if(!(cond)) { std::cerr << "FAIL: " << msg << "\n"; std::exit(1);} } while(0)
#define CHECK_CLOSE(a,b,rel,abs,msg) CHECK(approx_equal((a),(b),(rel),(abs)), msg << "  got=" << (a) << " ref=" << (b))

// Finite-difference utility
template<class Fn>
double central_diff(Fn f, double x, double h){ return (f(x+h)-f(x-h))/(2.0*h); }

int main() {
    using namespace order_flow::pricing;

    // -------- 1) Deterministic unit tests on a small grid --------
    struct Case { double S,K,r,T,sigma; };
    std::vector<Case> cases = {
        {100,100, 0.00, 0.5, 0.20},
        {100,100, 0.01, 1.0, 0.30},
        {100,120, 0.02, 2.0, 0.15},
        {100, 80, 0.03, 0.25,0.50},
        {50,  100, 0.00, 3.0, 0.80},    // deep OTM
        {150, 50,  0.00, 0.05,0.10},   // deep ITM, near expiry
    };

    BlackScholes engine; // Provides calculate(), normalCDF(), etc. (your code)  // NOLINT

    for (const auto& c : cases) {
        // Prepare params in your struct
        OptionParams p{};
        p.spot_price      = c.S;
        p.strike_price    = c.K;
        p.risk_free_rate  = c.r;
        p.time_to_expiry  = c.T;
        p.volatility      = c.sigma;

        // Library result (your implementation)
        PricingResult y = engine.calculate(p);

        // Reference
        auto r = ref::bs(c.S,c.K,c.r,c.T,c.sigma);

        // Prices within ~1e-7 relative is good for doubles; relax to 1e-6 because your CDF uses a fitted approx.
        CHECK_CLOSE(y.call_price, r.call, 5e-7, 5e-10, "call price");
        CHECK_CLOSE(y.put_price,  r.put,  5e-7, 5e-10, "put  price");

        // Parity & bounds
        const double df = std::exp(-c.r*c.T);
        CHECK( approx_equal(y.call_price - y.put_price, c.S - c.K*df, 5e-7, 5e-9), "put-call parity");
        CHECK( y.call_price >= 0.0 && y.put_price >= 0.0, "non-negativity");
        CHECK( y.call_price <= c.S+1e-9, "call ≤ S");
        CHECK( y.put_price  <= c.K*df+1e-9, "put ≤ K e^{-rT}");

        // Greeks vs reference (vega/gamma/theta tolerances a bit looser; they’re sensitive to CDF/pdf accuracy)
        CHECK_CLOSE(y.call_delta, r.delta_c, 3e-6, 1e-8, "delta(call)");
        CHECK_CLOSE(y.put_delta , r.delta_p, 3e-6, 1e-8, "delta(put)");
        CHECK_CLOSE(y.gamma     , r.gamma  , 1e-5, 1e-8, "gamma");
        CHECK_CLOSE(y.vega      , r.vega   , 1e-5, 1e-8, "vega");
        CHECK_CLOSE(y.theta     , r.theta_c, 2e-5, 1e-7, "theta(call)");
        CHECK_CLOSE(y.rho_call  , r.rho_c  , 2e-5, 1e-8, "rho(call)");
        CHECK_CLOSE(y.rho_put   , r.rho_p  , 2e-5, 1e-8, "rho(put)");
    }

    // -------- 2) Finite-difference self-consistency checks --------
    {
        OptionParams p{ .spot_price=100, .strike_price=100, .risk_free_rate=0.01, .time_to_expiry=0.5, .volatility=0.25 };
        BlackScholes engine;
        auto priceS = [&](double S){ p.spot_price=S; return engine.calculate(p).call_price; };
        auto priceSig=[&](double s){ p.volatility=s; return engine.calculate(p).call_price; };
        auto priceT = [&](double T){ p.time_to_expiry=T; return engine.calculate(p).call_price; };

        const double dS=1e-4*100, ds=1e-4*0.25, dT=1e-4*0.5;

        p.spot_price=100;  double delta_fd = central_diff(priceS, 100, dS);
        auto y = engine.calculate(p);
        CHECK_CLOSE(y.call_delta, delta_fd, 3e-4, 1e-8, "FD delta");

        p.volatility=0.25; double vega_fd = central_diff(priceSig, 0.25, ds);
        y = engine.calculate(p);
        CHECK_CLOSE(y.vega, vega_fd, 5e-4, 1e-8, "FD vega");

        p.time_to_expiry=0.5; double theta_fd = central_diff(priceT, 0.5, dT); // dC/dT
        y = engine.calculate(p);
        CHECK_CLOSE(y.theta, theta_fd, 8e-4, 1e-7, "FD theta");
    }

    // -------- 3) Edge cases: T->0, sigma->0 --------
    {
        OptionParams p{ .spot_price=100, .strike_price=95, .risk_free_rate=0.0, .time_to_expiry=1e-9, .volatility=0.2 };
        BlackScholes engine;
        auto y = engine.calculate(p);
        double intrinsic = std::max(p.spot_price - p.strike_price, 0.0);
        CHECK( std::abs(y.call_price - intrinsic) < 1e-6, "T->0 price -> intrinsic");
        CHECK( y.vega < 1e-6, "T->0 vega -> 0");
    }
    {
        OptionParams p{ .spot_price=100, .strike_price=100, .risk_free_rate=0.0, .time_to_expiry=1.0, .volatility=1e-12 };
        BlackScholes engine;
        auto y = engine.calculate(p);
        // sigma->0: price approx= discounted intrinsic; vega->0
        double df = std::exp(-p.risk_free_rate*p.time_to_expiry);
        double approx = std::max(p.spot_price - p.strike_price*df, 0.0);
        CHECK( std::abs(y.call_price - approx) < 1e-6, "sigma->0 price");
        CHECK( y.vega < 1e-6, "sigma->0 vega -> 0");
    }

    // -------- 4) Micro-benchmark (sanity throughput) --------
    {
        BlackScholes engine;
        const int N = 200000; // adjust
        OptionParams p{ .spot_price=100, .strike_price=100, .risk_free_rate=0.01, .time_to_expiry=1.0, .volatility=0.2 };

        auto t0 = std::chrono::high_resolution_clock::now();
        volatile double sink = 0.0;
        for (int i=0;i<N;++i) {
            p.spot_price += 0.00001; // avoid identical inputs
            auto y = engine.calculate(p);
            sink += y.call_price;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ns = std::chrono::duration<double,std::nano>(t1-t0).count()/N;
        cout << "[Benchmark] ~" << ns << " ns/call (N="<<N<<")  sink="<<sink<<"\n";
    }

    cout << "All tests PASSED\n";
    return 0;
}
