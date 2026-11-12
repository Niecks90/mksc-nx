#include "host_platform.h"

#include <chrono>
#include <cstdio>
#include <thread>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace gbarecomp {

FramePacer::FramePacer(double target_hz) {
    if (target_hz <= 0.0) target_hz = kGbaFrameHz;
    period_ = std::chrono::nanoseconds(
        static_cast<long long>(1e9 / target_hz + 0.5));
    reset();
}

FramePacer::~FramePacer() {}

void FramePacer::reset() {
    next_ = clock::now() + period_;
}

void FramePacer::wait_for_next_frame() {
    if (uncapped_) {
        next_ = clock::now();
        return;
    }
    clock::time_point now = clock::now();
    if (now > next_ + period_) {
        next_ = now + period_;
        return;
    }

#if defined(__SWITCH__)
    // On Nintendo Switch Horizon OS:
    // svcSleepThread has a typical granularity of ~1-2 ms.
    // Leave a 2.5 ms spin tail to prevent the kernel sleep from overshooting the deadline.
    constexpr auto kSwitchSpinTail = std::chrono::microseconds(2500);
    if (next_ - now > kSwitchSpinTail) {
        const auto sleep_dur = std::chrono::duration_cast<std::chrono::nanoseconds>(next_ - now - kSwitchSpinTail);
        svcSleepThread(sleep_dur.count());
    }
    // Precision tail: yield quantum on Switch until target time is reached.
    while (clock::now() < next_) {
        svcSleepThread(0);
    }
    const clock::time_point woke = clock::now();
    // Allow up to 8 ms tolerance for transient hitches so we don't drop cadence to 50 FPS.
    constexpr auto kLateTolerance = std::chrono::microseconds(8000);
    next_ = woke > next_ + kLateTolerance ? woke + period_ : next_ + period_;
#else
    constexpr auto kSpinTail = std::chrono::microseconds(1200);
    if (next_ - now > kSpinTail) {
        std::this_thread::sleep_until(next_ - kSpinTail);
    }
    while (clock::now() < next_) {
        std::this_thread::yield();
    }
    const clock::time_point woke = clock::now();
    constexpr auto kLateTolerance = std::chrono::microseconds(1500);
    next_ = woke > next_ + kLateTolerance ? woke + period_
                                          : next_ + period_;
#endif
}

std::vector<unsigned char> read_file(const std::string& path, std::string* error) {
    std::vector<unsigned char> out;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (error) *error = "open failed: " + path;
        return out;
    }
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz < 0) {
        if (error) *error = "ftell failed: " + path;
        std::fclose(f);
        return out;
    }
    out.resize(static_cast<std::size_t>(sz));
    std::size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    if (got != out.size()) {
        out.clear();
        if (error) *error = "short read: " + path;
    }
    return out;
}

}  // namespace gbarecomp
