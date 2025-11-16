#ifndef EXECUTION_AFFINITY_HPP
#define EXECUTION_AFFINITY_HPP

#include <cstdint>
#include <string>

namespace order_flow {
namespace utils {

struct PinningConfig {
    int core_id = -1;
    int numa_node = -1;
    bool realtime_priority = false;
    int priority_hint = 0; // platform specific (0 -> default)
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

} // namespace utils
} // namespace order_flow

#endif // EXECUTION_AFFINITY_HPP
