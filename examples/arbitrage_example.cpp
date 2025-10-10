/**
 * @file arbitrage_example.cpp
 * @brief Example demonstrating ultra-low latency arbitrage detection
 * 
 * Use Cases:
 * 1. Speed Arbitrage: Detect mispriced options faster than competitors
 * 2. Put-Call Parity Arbitrage: Find conversion/reversal opportunities
 * 3. Market Making: Provide liquidity while capturing mispricings
 */

#include "../src/cpp/include/black_scholes.hpp"
#include "../src/cpp/include/order_flow_types.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>

using namespace order_flow::pricing;
using namespace order_flow::types;

void print_separator() {
    std::cout << "\n" << std::string(80, '-') << "\n\n";
}

void example_speed_arbitrage() {
    std::cout << "EXAMPLE 1: SPEED ARBITRAGE - Detect Mispriced Options\n";
    std::cout << "Strategy: Price options faster than market makers, capture mispricings\n";
    print_separator();
    
    // Option parameters: AAPL call option
    OptionParams params{
        .spot_price = 175.00,      // AAPL trading at $175
        .strike_price = 180.00,    // $180 strike call
        .time_to_expiry = 30.0/365.0,  // 30 days to expiration
        .risk_free_rate = 0.05,    // 5% risk-free rate
        .volatility = 0.25         // 25% implied volatility
    };
    
    // Market maker's quoted price (stale quote)
    double market_price = 3.50;
    
    // Detect arbitrage opportunity
    auto start = std::chrono::high_resolution_clock::now();
    ArbitrageSignal signal = BlackScholes::detectArbitrage(
        params, 
        market_price,
        0.5,  // 0.5% threshold
        true  // Call option
    );
    auto end = std::chrono::high_resolution_clock::now();
    auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Theoretical Price: $" << signal.theoretical_price << "\n";
    std::cout << "Market Price:      $" << signal.market_price << "\n";
    std::cout << "Price Difference:  $" << signal.price_diff << " (" 
              << signal.price_diff_pct << "%)\n\n";
    
    if (signal.is_arbitrage) {
        std::cout << "*** ARBITRAGE DETECTED! ***\n";
        std::cout << "Action: " << (signal.should_buy ? "BUY" : "SELL") << " the option\n";
        std::cout << "Expected Profit: $" << signal.expected_profit << " per contract\n";
        std::cout << "Delta Hedge: " << (signal.should_buy ? "SELL" : "BUY") 
                  << " " << signal.delta_hedge_size << " shares of underlying\n";
    } else {
        std::cout << "No arbitrage opportunity (within threshold)\n";
    }
    
    std::cout << "\nExecution Latency: " << latency_ns << " nanoseconds ("
              << latency_ns/1000.0 << " microseconds)\n";
}

void example_put_call_parity() {
    std::cout << "EXAMPLE 2: PUT-CALL PARITY ARBITRAGE\n";
    std::cout << "Strategy: Exploit violations of C - P = S - K*e^(-rT)\n";
    print_separator();
    
    // Option parameters: Same strike, same expiration
    OptionParams params{
        .spot_price = 100.00,
        .strike_price = 100.00,    // ATM options
        .time_to_expiry = 60.0/365.0,
        .risk_free_rate = 0.04,
        .volatility = 0.30
    };
    
    // Market prices (parity violated)
    double call_market_price = 5.50;  // Call overpriced
    double put_market_price = 4.80;   // Put underpriced
    
    auto start = std::chrono::high_resolution_clock::now();
    PutCallParityCheck parity = BlackScholes::checkPutCallParity(
        params,
        call_market_price,
        put_market_price,
        0.10  // $0.10 threshold
    );
    auto end = std::chrono::high_resolution_clock::now();
    auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Put-Call Parity: C - P = S - K*e^(-rT)\n\n";
    std::cout << "Left Side (C - P):  $" << parity.lhs << "\n";
    std::cout << "Right Side (S - PV(K)): $" << parity.rhs << "\n";
    std::cout << "Parity Difference: $" << parity.parity_diff << "\n\n";
    
    if (parity.parity_violated) {
        std::cout << "*** PUT-CALL PARITY VIOLATED! ***\n";
        if (parity.parity_diff > 0) {
            std::cout << "Action: CONVERSION SPREAD\n";
            std::cout << "  1. BUY put\n";
            std::cout << "  2. SELL call\n";
            std::cout << "  3. BUY 100 shares of stock\n";
        } else {
            std::cout << "Action: REVERSAL SPREAD\n";
            std::cout << "  1. SELL put\n";
            std::cout << "  2. BUY call\n";
            std::cout << "  3. SELL SHORT 100 shares of stock\n";
        }
        std::cout << "Arbitrage Profit: $" << parity.arbitrage_profit << " per spread\n";
    } else {
        std::cout << "Put-call parity holds (within threshold)\n";
    }
    
    std::cout << "\nExecution Latency: " << latency_ns << " nanoseconds ("
              << latency_ns/1000.0 << " microseconds)\n";
}

void example_market_making_workflow() {
    std::cout << "EXAMPLE 3: MARKET MAKING WITH ARBITRAGE DETECTION\n";
    std::cout << "Strategy: Quote tight spreads, capture mispricings, manage risk\n";
    print_separator();
    
    OptionParams params{
        .spot_price = 50.00,
        .strike_price = 52.00,
        .time_to_expiry = 45.0/365.0,
        .risk_free_rate = 0.045,
        .volatility = 0.28
    };
    
    // Calculate theoretical value
    PricingResult pricing = BlackScholes::calculate(params);
    
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Theoretical Call Price: $" << pricing.call_price << "\n";
    std::cout << "Theoretical Put Price:  $" << pricing.put_price << "\n\n";
    
    // Market maker quotes (bid/ask spread)
    double bid_call = pricing.call_price - 0.05;  // 5 cent wide market
    double ask_call = pricing.call_price + 0.05;
    
    std::cout << "Market Maker Quotes:\n";
    std::cout << "  Call Bid: $" << bid_call << " | Ask: $" << ask_call << "\n\n";
    
    // Simulate incoming order flow
    std::cout << "Incoming Order Flow:\n";
    
    // Customer wants to buy at ask (fair trade)
    double customer_buy_price = ask_call;
    ArbitrageSignal signal1 = BlackScholes::detectArbitrage(params, customer_buy_price, 0.3, true);
    std::cout << "1. Customer BUY at $" << customer_buy_price << " -> ";
    std::cout << (signal1.is_arbitrage ? "REJECT (overpriced)" : "FILL (fair price)") << "\n";
    
    // Customer wants to sell at bid (fair trade)
    double customer_sell_price = bid_call;
    ArbitrageSignal signal2 = BlackScholes::detectArbitrage(params, customer_sell_price, 0.3, true);
    std::cout << "2. Customer SELL at $" << customer_sell_price << " -> ";
    std::cout << (signal2.is_arbitrage ? "TAKE (underpriced)" : "FILL (fair price)") << "\n";
    
    // Customer tries to sell way below value (arbitrage for market maker)
    double customer_cheap_price = pricing.call_price - 0.30;
    ArbitrageSignal signal3 = BlackScholes::detectArbitrage(params, customer_cheap_price, 0.5, true);
    std::cout << "3. Customer SELL at $" << customer_cheap_price << " -> ";
    if (signal3.is_arbitrage && signal3.should_buy) {
        std::cout << "*** ARBITRAGE! BUY IT! Profit: $" << signal3.expected_profit << "\n";
        std::cout << "   Immediately hedge: SELL " << signal3.delta_hedge_size << " shares\n";
    }
    
    std::cout << "\n*** Position Greeks (after filling customer order) ***\n";
    std::cout << "  Delta: " << pricing.call_delta << " (hedge with " 
              << pricing.call_delta * 100 << " shares)\n";
    std::cout << "  Gamma: " << pricing.gamma << " (delta sensitivity)\n";
    std::cout << "  Vega:  " << pricing.vega << " (volatility risk)\n";
    std::cout << "  Theta: " << pricing.theta << " (time decay per day)\n";
}

int main() {
    std::cout << "\n";
    std::cout << "===============================================================================\n";
    std::cout << "        ULTRA-LOW LATENCY ARBITRAGE DETECTION EXAMPLES                     \n";
    std::cout << "        Target: Sub-microsecond option pricing + arbitrage detection       \n";
    std::cout << "===============================================================================\n";
    
    print_separator();
    example_speed_arbitrage();
    
    print_separator();
    example_put_call_parity();
    
    print_separator();
    example_market_making_workflow();
    
    print_separator();
    std::cout << "*** All examples completed successfully! ***\n\n";
    
    return 0;
}
