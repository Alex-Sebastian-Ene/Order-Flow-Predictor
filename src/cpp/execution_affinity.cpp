#include "execution_affinity.hpp"

#include <stdexcept>
#include <algorithm>

#if defined(_WIN32)
#include <windows.h>
#include <processthreadsapi.h>
#else
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#include <linux/unistd.h>
#endif
#endif

namespace order_flow {
namespace utils {

#if !defined(_WIN32)
static cpu_set_t read_current_affinity() {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
    return mask;
}
#endif

ScopedThreadAffinity::ScopedThreadAffinity(const PinningConfig& cfg)
    : cfg_(cfg), active_(false)
#if defined(_WIN32)
    , thread_handle_(GetCurrentThread()), old_priority_(0), previous_mask_(0)
#else
    , old_policy_(0), old_sched_{}, previous_mask_{}, have_old_mask_(false)
#endif
{
    pin_current_thread(cfg);
#if defined(_WIN32)
    previous_mask_ = SetThreadAffinityMask(thread_handle_, cfg.core_id >= 0 ? (1ull << cfg.core_id) : ~0ull);
    if (previous_mask_ == 0) {
        throw std::runtime_error("Failed to set affinity mask");
    }
    if (cfg.realtime_priority) {
        old_priority_ = GetThreadPriority(thread_handle_);
        int new_priority = THREAD_PRIORITY_TIME_CRITICAL;
        if (cfg.priority_hint > 0) {
            new_priority = std::min<int>(THREAD_PRIORITY_TIME_CRITICAL, cfg.priority_hint);
        }
        SetThreadPriority(thread_handle_, new_priority);
    }
#else
    have_old_mask_ = true;
    previous_mask_ = read_current_affinity();
    cpu_set_t mask;
    CPU_ZERO(&mask);
    if (cfg.core_id >= 0) {
        CPU_SET(cfg.core_id, &mask);
    } else {
        long cpus = sysconf(_SC_NPROCESSORS_ONLN);
        for (long i = 0; i < cpus; ++i) {
            CPU_SET(i, &mask);
        }
    }
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask) != 0) {
        throw std::runtime_error("Failed to set thread affinity");
    }
    old_policy_ = sched_getscheduler(0);
    sched_getparam(0, &old_sched_);
    if (cfg.realtime_priority) {
        sched_param sp{};
        sp.sched_priority = cfg.priority_hint > 0 ? cfg.priority_hint : sched_get_priority_max(SCHED_FIFO);
        sched_setscheduler(0, SCHED_FIFO, &sp);
    }
#endif
    active_ = true;
}

ScopedThreadAffinity::~ScopedThreadAffinity() {
    if (!active_) {
        return;
    }
#if defined(_WIN32)
    if (previous_mask_ != 0) {
        SetThreadAffinityMask(thread_handle_, previous_mask_);
    }
    if (cfg_.realtime_priority) {
        SetThreadPriority(thread_handle_, old_priority_);
    }
#else
    if (have_old_mask_) {
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &previous_mask_);
    }
    if (cfg_.realtime_priority) {
        sched_setscheduler(0, old_policy_, &old_sched_);
    }
#endif
}

void pin_current_thread(const PinningConfig& cfg) {
    if (cfg.core_id < 0 && !cfg.realtime_priority) {
        return;
    }
#if defined(_WIN32)
    HANDLE thread = GetCurrentThread();
    if (cfg.core_id >= 0) {
        SetThreadAffinityMask(thread, 1ull << cfg.core_id);
    }
    if (cfg.realtime_priority) {
        int priority = cfg.priority_hint > 0 ? cfg.priority_hint : THREAD_PRIORITY_HIGHEST;
        SetThreadPriority(thread, priority);
    }
#else
    if (cfg.core_id >= 0) {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(cfg.core_id, &mask);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
    }
    if (cfg.realtime_priority) {
        sched_param sp{};
        sp.sched_priority = cfg.priority_hint > 0 ? cfg.priority_hint : sched_get_priority_max(SCHED_FIFO);
        sched_setscheduler(0, SCHED_FIFO, &sp);
    }
#endif
}

void set_thread_name(const std::string& name) {
#if defined(_WIN32)
    using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    static SetThreadDescriptionFn fn = reinterpret_cast<SetThreadDescriptionFn>(
        GetProcAddress(GetModuleHandleW(L"Kernel32.dll"), "SetThreadDescription"));
    if (fn) {
        std::wstring wname(name.begin(), name.end());
        fn(GetCurrentThread(), wname.c_str());
    }
#else
    pthread_setname_np(pthread_self(), name.c_str());
#endif
}

} // namespace utils
} // namespace order_flow
