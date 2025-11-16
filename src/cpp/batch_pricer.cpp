#include "batch_pricer.hpp"
#include "black_scholes.hpp"

#include <chrono>
#include <thread>

namespace order_flow {
namespace pricing {

BatchPricingEngine::BatchPricingEngine(const Config& cfg)
    : cfg_(cfg), inbound_(cfg.queue_depth), outbound_(cfg.queue_depth), running_(false) {}

BatchPricingEngine::~BatchPricingEngine() {
    stop();
}

bool BatchPricingEngine::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return false;
    }
    worker_ = std::thread(&BatchPricingEngine::worker_loop, this);
    return true;
}

void BatchPricingEngine::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool BatchPricingEngine::submit(const OptionBatchJob& job) {
    return inbound_.try_push(job);
}

bool BatchPricingEngine::try_receive(OptionBatchJob& job) {
    return outbound_.try_pop(job);
}

void BatchPricingEngine::worker_loop() {
    utils::ScopedThreadAffinity guard(cfg_.pinning);
    if (!cfg_.thread_name.empty()) {
        utils::set_thread_name(cfg_.thread_name);
    }
    OptionBatchJob job{};
    while (running_.load(std::memory_order_acquire)) {
        if (!inbound_.try_pop(job)) {
            std::this_thread::yield();
            continue;
        }
        BlackScholes::calculate_batch(job.options, job.results);
        while (!outbound_.try_push(job)) {
            std::this_thread::yield();
        }
    }
}

} // namespace pricing
} // namespace order_flow
