#ifndef EXECUTION_AFFINITY_HPP
#define EXECUTION_AFFINITY_HPP

#include <cstdint>
#include <cstddef>
#include <string>

#if !defined(_WIN32)
#include <sched.h>
#endif

namespace order_flow {
namespace utils {

struct PinningConfig {
    int core_id = -1;
    int numa_node = -1;
    bool realtime_priority = false;
    int priority_hint = 0; // platform specific (0 -> default)
    int memory_node = -1;
    bool prefault_memory = false;
    std::size_t memory_alignment = 64;
};

class ScopedThreadAffinity {
public:
    explicit ScopedThreadAffinity(const PinningConfig& cfg);
    ~ScopedThreadAffinity();
    ScopedThreadAffinity(const ScopedThreadAffinity&) = delete;
    ScopedThreadAffinity& operator=(const ScopedThreadAffinity&) = delete;
    ScopedThreadAffinity(ScopedThreadAffinity&&) = delete;
    ScopedThreadAffinity& operator=(ScopedThreadAffinity&&) = delete;

private:
    PinningConfig cfg_;
    bool active_;
#if defined(_WIN32)
    void* thread_handle_;
    unsigned long old_priority_;
    unsigned long long previous_mask_;
#else
    int old_policy_;
    struct sched_param old_sched_;
    cpu_set_t previous_mask_;
    bool have_old_mask_;
#endif
};

void pin_current_thread(const PinningConfig& cfg);
void set_thread_name(const std::string& name);

class NumaBuffer {
public:
    NumaBuffer(std::size_t bytes, std::size_t alignment, const PinningConfig& cfg);
    ~NumaBuffer();
    NumaBuffer(NumaBuffer&&) noexcept;
    NumaBuffer& operator=(NumaBuffer&&) noexcept;
    NumaBuffer(const NumaBuffer&) = delete;
    NumaBuffer& operator=(const NumaBuffer&) = delete;

    void* data() const { return data_; }
    std::size_t size() const { return size_; }

private:
    void* data_;
    std::size_t size_;
    bool os_backed_;
};

void prefault_memory(void* ptr, std::size_t bytes);

} // namespace utils
} // namespace order_flow

#endif // EXECUTION_AFFINITY_HPP
