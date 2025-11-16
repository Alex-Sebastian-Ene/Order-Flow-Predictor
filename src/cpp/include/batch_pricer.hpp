#ifndef BATCH_PRICER_HPP
#define BATCH_PRICER_HPP

#include "order_flow_types.hpp"
#include "execution_affinity.hpp"
#include "lock_free_ring.hpp"
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <cstdint>

namespace order_flow {
namespace pricing {

struct OptionBatchJob {
    types::OptionBatchView options;
    types::PricingBatchView results;
    std::uint64_t sequence_id;
};

class BatchPricingEngine {
public:
    struct Config {
        utils::PinningConfig pinning;
        std::size_t queue_depth = 1024;
        bool prefer_simd = true;
        std::string thread_name = "bs-engine";
    };

    explicit BatchPricingEngine(const Config& cfg);
    ~BatchPricingEngine();

    bool start();
    void stop();
    bool submit(const OptionBatchJob& job);
    bool try_receive(OptionBatchJob& job);

private:
    void worker_loop();

    Config cfg_;
    utils::LockFreeRing<OptionBatchJob> inbound_;
    utils::LockFreeRing<OptionBatchJob> outbound_;
    std::thread worker_;
    std::atomic<bool> running_;
};

} // namespace pricing
} // namespace order_flow

#endif // BATCH_PRICER_HPP
