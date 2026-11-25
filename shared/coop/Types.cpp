#include "Types.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <random>

namespace coop {

std::string SessionId::ToString() const {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%016llx%016llx",
                  (unsigned long long)hi, (unsigned long long)lo);
    return buf;
}

SessionId SessionId::FromString(const std::string& s) {
    SessionId out;
    if (s.size() != 32) return out;
    auto hexval = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    u64 hi = 0, lo = 0;
    bool ok = true;
    for (int i = 0; i < 32; ++i) {
        int v = hexval(s[i]);
        if (v < 0) { ok = false; break; }
        if (i < 16) hi = (hi << 4) | v; else lo = (lo << 4) | v;
    }
    if (!ok) return out;
    out.hi = hi;
    out.lo = lo;
    return out;
}

SessionId SessionId::Generate() {
    // std::random_device + address/time mixing: good enough for a session id.
    static std::atomic<u64> counter{0};
    u64 t = (u64)std::chrono::steady_clock::now().time_since_epoch().count();
    u64 seed = (u64)std::random_device{}();
    SessionId out;
    out.hi = t ^ (seed * 0x9e3779b97f4a7c15ull);
    out.lo = t ^ (seed * 0xc2b2ae3d27d4eb4full) ^ (++counter);
    return out;
}

const char* MissionStateName(MissionState s) {
    switch (s) {
        case MissionState::IDLE:        return "IDLE";
        case MissionState::STARTING:    return "STARTING";
        case MissionState::ACTIVE:      return "ACTIVE";
        case MissionState::CHECKPOINT:  return "CHECKPOINT";
        case MissionState::COMPLETED:   return "COMPLETED";
        case MissionState::FAILED:      return "FAILED";
        case MissionState::UNSUPPORTED: return "UNSUPPORTED";
    }
    return "?";
}

} // namespace coop
