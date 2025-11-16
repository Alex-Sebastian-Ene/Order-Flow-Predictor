#include "black_scholes.hpp"
#include "execution_affinity.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__)
#include <x86intrin.h>
#endif

#if defined(__linux__)
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace {

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define BS_BENCH_HAS_TSC 1
#else
#define BS_BENCH_HAS_TSC 0
#endif

constexpr bool kHasTSC = BS_BENCH_HAS_TSC != 0;

inline uint64_t rdtsc_begin() {
#if BS_BENCH_HAS_TSC
    _mm_mfence();
    _mm_lfence();
    return __rdtsc();
#else
    return 0;
#endif
}

inline uint64_t rdtsc_end() {
#if BS_BENCH_HAS_TSC
    unsigned int aux = 0;
    uint64_t value = __rdtscp(&aux);
    _mm_lfence();
    _mm_mfence();
    return value;
#else
    return 0;
#endif
}

struct BenchConfig {
    std::size_t batch = 64;
    std::size_t samples = 20000;
    std::size_t warmup = 2000;
    double cpu_ghz = 3.5;
    int core = -1;
    bool realtime = false;
};

struct StatBlock {
    double mean = 0.0;
    double median = 0.0;
    double p90 = 0.0;
    double p99 = 0.0;
    double min = 0.0;
    double max = 0.0;
};

BenchConfig parse_args(int argc, char** argv) {
    BenchConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        auto pos = arg.find('=');
        std::string_view key = pos == std::string_view::npos ? arg : arg.substr(0, pos);
        std::string value = pos == std::string_view::npos ? std::string{} : std::string(arg.substr(pos + 1));
        if (key == "--batch") {
            cfg.batch = std::max<std::size_t>(1, std::stoull(value));
        } else if (key == "--samples") {
            cfg.samples = std::max<std::size_t>(1, std::stoull(value));
        } else if (key == "--warmup") {
            cfg.warmup = std::stoull(value);
        } else if (key == "--cpu-ghz") {
            cfg.cpu_ghz = std::stod(value);
        } else if (key == "--core") {
            cfg.core = std::stoi(value);
        } else if (key == "--realtime") {
            cfg.realtime = (value == "1" || value == "true" || value == "on");
        }
    }
    return cfg;
}

StatBlock summarize(const std::vector<double>& samples) {
    StatBlock stats{};
    if (samples.empty()) {
        return stats;
    }
    stats.mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
    std::vector<double> sorted(samples);
    std::sort(sorted.begin(), sorted.end());
    auto percentile = [&](double pct) {
        if (sorted.empty()) return 0.0;
        double rank = pct * (sorted.size() - 1);
        std::size_t idx = static_cast<std::size_t>(rank);
        double frac = rank - idx;
        if (idx + 1 < sorted.size()) {
            return sorted[idx] + (sorted[idx + 1] - sorted[idx]) * frac;
        }
        return sorted.back();
    };
    stats.median = percentile(0.5);
    stats.p90 = percentile(0.9);
    stats.p99 = percentile(0.99);
    stats.min = sorted.front();
    stats.max = sorted.back();
    return stats;
}

#if defined(__linux__)
class PerfEventGroup {
public:
    PerfEventGroup() {
        cycles_fd_ = open_counter(PERF_COUNT_HW_CPU_CYCLES);
        instructions_fd_ = open_counter(PERF_COUNT_HW_INSTRUCTIONS);
        available_ = cycles_fd_ >= 0 && instructions_fd_ >= 0;
    }

    ~PerfEventGroup() {
        if (cycles_fd_ >= 0) close(cycles_fd_);
        if (instructions_fd_ >= 0) close(instructions_fd_);
    }

    void begin() {
        if (!available_) return;
        ioctl(cycles_fd_, PERF_EVENT_IOC_RESET, 0);
        ioctl(instructions_fd_, PERF_EVENT_IOC_RESET, 0);
        ioctl(cycles_fd_, PERF_EVENT_IOC_ENABLE, 0);
        ioctl(instructions_fd_, PERF_EVENT_IOC_ENABLE, 0);
    }

    std::pair<uint64_t, uint64_t> end() {
        if (!available_) return {0, 0};
        ioctl(cycles_fd_, PERF_EVENT_IOC_DISABLE, 0);
        ioctl(instructions_fd_, PERF_EVENT_IOC_DISABLE, 0);
        uint64_t cycles = 0;
        uint64_t inst = 0;
        read(cycles_fd_, &cycles, sizeof(cycles));
        read(instructions_fd_, &inst, sizeof(inst));
        return {cycles, inst};
    }

    bool available() const { return available_; }

private:
    static int open_counter(uint64_t config) {
        perf_event_attr attr{};
        memset(&attr, 0, sizeof(attr));
        attr.type = PERF_TYPE_HARDWARE;
        attr.size = sizeof(attr);
        attr.config = config;
        attr.disabled = 1;
        attr.exclude_kernel = 0;
        attr.exclude_hv = 0;
        return static_cast<int>(syscall(__NR_perf_event_open, &attr, 0, -1, -1, 0));
    }

    int cycles_fd_ = -1;
    int instructions_fd_ = -1;
    bool available_ = false;
};
#else
class PerfEventGroup {
public:
    void begin() {}
    std::pair<uint64_t, uint64_t> end() { return {0, 0}; }
    bool available() const { return false; }
};
#endif

} // namespace

int main(int argc, char** argv) {
    using namespace order_flow;
    using namespace order_flow::pricing;
    using namespace order_flow::types;

    BenchConfig cfg = parse_args(argc, argv);

    if (cfg.batch == 0) {
        std::cerr << "Batch size must be > 0" << std::endl;
        return 1;
    }

    utils::PinningConfig pin_cfg;
    pin_cfg.core_id = cfg.core;
    pin_cfg.realtime_priority = cfg.realtime;
    utils::ScopedThreadAffinity pin_guard(pin_cfg);

    std::vector<double> spots(cfg.batch);
    std::vector<double> strikes(cfg.batch);
    std::vector<double> expiries(cfg.batch);
    std::vector<double> rates(cfg.batch);
    std::vector<double> sigmas(cfg.batch);

    std::vector<double> call(cfg.batch);
    std::vector<double> put(cfg.batch);
    std::vector<double> delta(cfg.batch);
    std::vector<double> put_delta(cfg.batch);
    std::vector<double> gamma(cfg.batch);
    std::vector<double> vega(cfg.batch);
    std::vector<double> theta(cfg.batch);
    std::vector<double> rho_call(cfg.batch);
    std::vector<double> rho_put(cfg.batch);
    std::vector<double> vanna(cfg.batch);
    std::vector<double> vomma(cfg.batch);

    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> spot_dist(50.0, 200.0);
    std::uniform_real_distribution<double> strike_dist(50.0, 200.0);
    std::uniform_real_distribution<double> time_dist(1.0 / 365.0, 2.0);
    std::uniform_real_distribution<double> rate_dist(0.0, 0.05);
    std::uniform_real_distribution<double> sigma_dist(0.1, 0.8);

    for (std::size_t i = 0; i < cfg.batch; ++i) {
        spots[i] = spot_dist(rng);
        strikes[i] = strike_dist(rng);
        expiries[i] = time_dist(rng);
        rates[i] = rate_dist(rng);
        sigmas[i] = sigma_dist(rng);
    }

    OptionBatchView options{
        spots.data(),
        strikes.data(),
        expiries.data(),
        rates.data(),
        sigmas.data(),
        cfg.batch,
        1
    };

    PricingBatchView results{
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
        cfg.batch,
        1
    };

    for (std::size_t i = 0; i < cfg.warmup; ++i) {
        BlackScholes::calculate_batch(options, results);
        sigmas[i % cfg.batch] = std::clamp(sigmas[i % cfg.batch] * 1.0001, 0.05, 1.0);
    }

    std::vector<double> cycles_per_price;
    std::vector<double> ns_per_price;
    cycles_per_price.reserve(cfg.samples);
    ns_per_price.reserve(cfg.samples);

    PerfEventGroup pmu;
    pmu.begin();
    auto wall_begin = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < cfg.samples; ++i) {
        sigmas[i % cfg.batch] = std::clamp(sigmas[i % cfg.batch] * 0.9999 + 1e-4, 0.05, 1.2);
        uint64_t t0 = rdtsc_begin();
        auto c0 = std::chrono::high_resolution_clock::now();
        BlackScholes::calculate_batch(options, results);
        auto c1 = std::chrono::high_resolution_clock::now();
        uint64_t t1 = rdtsc_end();
        double ns = std::chrono::duration<double, std::nano>(c1 - c0).count() / static_cast<double>(cfg.batch);
        ns_per_price.push_back(ns);
        if (kHasTSC) {
            double cycles = static_cast<double>(t1 - t0) / static_cast<double>(cfg.batch);
            cycles_per_price.push_back(cycles);
        }
    }

    auto wall_end = std::chrono::steady_clock::now();
    auto pmu_counts = pmu.end();

    double total_wall_ns = std::chrono::duration<double, std::nano>(wall_end - wall_begin).count();
    double total_prices = static_cast<double>(cfg.batch) * static_cast<double>(cfg.samples);
    double throughput_mops = (total_prices / total_wall_ns) * 1e3;

    StatBlock ns_stats = summarize(ns_per_price);
    StatBlock cycle_stats = summarize(cycles_per_price);

    auto format_stats = [](const StatBlock& stats, double scale) {
        return std::array<double, 6>{
            stats.mean * scale,
            stats.median * scale,
            stats.p90 * scale,
            stats.p99 * scale,
            stats.min * scale,
            stats.max * scale
        };
    };

    auto ns_values = format_stats(ns_stats, 1.0);
    auto cycle_values = format_stats(cycle_stats, 1.0);
    auto derived_ns_from_cycles = format_stats(cycle_stats, cfg.cpu_ghz > 0 ? (1.0 / cfg.cpu_ghz) : 0.0);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "{\n";
    std::cout << "  \"batch_size\": " << cfg.batch << ",\n";
    std::cout << "  \"samples\": " << cfg.samples << ",\n";
    std::cout << "  \"cpu_ghz\": " << cfg.cpu_ghz << ",\n";
    std::cout << "  \"throughput_mops\": " << throughput_mops << ",\n";
    std::cout << "  \"wall_time_ms\": " << total_wall_ns / 1e6 << ",\n";
    if (kHasTSC) {
        std::cout << "  \"cycles_per_price\": {";
        std::cout << "\"mean\": " << cycle_values[0] << ", "
                  << "\"p50\": " << cycle_values[1] << ", "
                  << "\"p90\": " << cycle_values[2] << ", "
                  << "\"p99\": " << cycle_values[3] << ", "
                  << "\"min\": " << cycle_values[4] << ", "
                  << "\"max\": " << cycle_values[5] << "},\n";
        std::cout << "  \"ns_from_cycles\": {";
        std::cout << "\"mean\": " << derived_ns_from_cycles[0] << ", "
                  << "\"p50\": " << derived_ns_from_cycles[1] << ", "
                  << "\"p90\": " << derived_ns_from_cycles[2] << ", "
                  << "\"p99\": " << derived_ns_from_cycles[3] << ", "
                  << "\"min\": " << derived_ns_from_cycles[4] << ", "
                  << "\"max\": " << derived_ns_from_cycles[5] << "},\n";
    }
    std::cout << "  \"measured_ns_per_price\": {";
    std::cout << "\"mean\": " << ns_values[0] << ", "
              << "\"p50\": " << ns_values[1] << ", "
              << "\"p90\": " << ns_values[2] << ", "
              << "\"p99\": " << ns_values[3] << ", "
              << "\"min\": " << ns_values[4] << ", "
              << "\"max\": " << ns_values[5] << "},\n";
    std::cout << "  \"pmu\": {\n";
    std::cout << "    \"available\": " << (pmu.available() ? "true" : "false") << ",\n";
    std::cout << "    \"cycles\": " << pmu_counts.first << ",\n";
    std::cout << "    \"instructions\": " << pmu_counts.second << "\n";
    std::cout << "  }\n";
    std::cout << "}\n";

    return 0;
}
