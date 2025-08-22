#ifndef ORDER_FLOW_PREDICTOR_HPP
#define ORDER_FLOW_PREDICTOR_HPP

#include <string>
#include <vector>
#include <memory>

namespace order_flow {

/**
 * High-performance order book implementation.
 * Target: Process orders in nanosecond range.
 */
class OrderBook {
public:
    /**
     * @brief Add new order to the book
     * Should:
     * - Complete in < 100 nanoseconds
     * - Use lock-free operations
     * - Pre-allocate memory
     * - Maintain price-time priority
     * - Support concurrent access
     */
    virtual void addOrder(double price, double quantity, bool isBuy) = 0;
    
    /**
     * @brief Remove order from the book
     * Should:
     * - O(1) lookup using order ID
     * - Lock-free removal
     * - No memory deallocation in critical path
     * - Update price levels atomically
     */
    virtual void removeOrder(const std::string& orderId) = 0;
    
    /**
     * @brief Get best bid/ask prices
     * Should:
     * - O(1) access time
     * - Cache-optimized lookup
     * - Thread-safe reading
     * - No locks or synchronization
     */
    virtual double getBestBid() const = 0;
    virtual double getBestAsk() const = 0;
    
    /**
     * @brief Get order book depth
     * Should:
     * - Use pre-allocated vectors
     * - SIMD for price level aggregation
     * - Cache-aligned data structures
     * - Snapshot consistency
     */
    virtual std::vector<std::pair<double, double>> getDepth(size_t levels) const = 0;
};

/**
 * @brief Create order book instance
 * Should:
 * - Pre-allocate memory pool
 * - Configure huge pages
 * - Set up lock-free data structures
 * - Initialize price level cache
 */
std::unique_ptr<OrderBook> createOrderBook();

} // namespace order_flow

#endif // ORDER_FLOW_PREDICTOR_HPP
