#pragma once
// Time the EE thread loses to synchronous GPU-worker round trips (Present,
// Sync, Flush, VRAM reads). Header-only inline atomics so the worker and the
// host session sampler see the same counters.
#include <atomic>
#include <chrono>
namespace DisneyGpuStats {
inline std::atomic<unsigned long long> syncWaitNs{0}, syncCalls{0};
inline std::atomic<unsigned long long> presentWaitNs{0}, presentCalls{0};
inline std::atomic<unsigned long long> flushWaitNs{0}, syncOpWaitNs{0}, readWaitNs{0}, clearWaitNs{0};
inline std::atomic<unsigned long long> queueBlockNs{0}, queueBlocks{0};
// Wall time the worker thread spends executing queued commands (not idle on the condition variable).
inline std::atomic<unsigned long long> workerBusyNs{0}, workerOps{0};
template <class Sink> struct Timer {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    Sink &sink;
    explicit Timer(Sink &s) : sink(s) {}
    ~Timer() { sink.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count(), std::memory_order_relaxed); }
};
}
