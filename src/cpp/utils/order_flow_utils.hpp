#ifndef ORDER_FLOW_UTILS_HPP
#define ORDER_FLOW_UTILS_HPP

#include <string>
#include <vector>
#include <chrono>
#include <iostream>

namespace order_flow {
namespace utils {

// Time-related utilities
using TimePoint = std::chrono::system_clock::time_point;

inline TimePoint getCurrentTime() {
    return std::chrono::system_clock::now();
}

// Price formatting
inline std::string formatPrice(double price, int decimals = 2) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.*f", decimals, price);
    return std::string(buffer);
}

// Order book utilities
struct Level {
    double price;
    double quantity;
    
    Level(double p, double q) : price(p), quantity(q) {}
};

using OrderBookSnapshot = std::vector<Level>;

// Market data types
enum class OrderType {
    LIMIT,
    MARKET,
    CANCEL
};

enum class Side {
    BUY,
    SELL
};

} // namespace utils
} // namespace order_flow

#endif // ORDER_FLOW_UTILS_HPP
