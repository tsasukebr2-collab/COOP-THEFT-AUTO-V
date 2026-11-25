#include "MissionSync.h"
#include "../natives/Natives.h"
#include "../../shared/coop/Log.h"

namespace coop {

namespace {

// Built-in mission registry. Mission hash = Jenkins hash of the script name
// (GET_HASH_KEY semantics). Story missions that fundamentally require the
// solo protagonist layout (switch-driven, solo interiors, scripted partner
// control) are explicitly marked unsupported rather than fake-synced.
const std::map<u64, MissionSync::MissionConfig>& BuildRegistry() {
    static const std::map<u64, MissionSync::MissionConfig> registry = [] {
        std::map<u64, MissionSync::MissionConfig> r;
        auto add = [&r](const char* script, const char* pretty, bool supported, u8 cps) {
            // Compute Jenkins hash the same way GET_HASH_KEY does.
            u32 h = 0;
            for (const char* p = script; *p; ++p) {
                h += u8(*p);
                h += h << 10;
                h ^= h >> 6;
            }
            h += h << 3;
            h ^= h >> 11;
            h += h << 15;
            MissionSync::MissionConfig cfg;
            cfg.name = pretty;
            cfg.supported = supported;
            cfg.maxCheckpoints = cps;
            r[u64(h)] = cfg;
        };
        // Supported: open-world, multi-actor, position-based missions.
        add("armenian2", "Franklin - Complications", true, 3);
        add("trevor3",   "Trevor - Trevor Philips Industries", true, 3);
        add("michael4",  "Michael - The Long Stretch", true, 2);
        add("franklin2", "Franklin - Marriage Counseling", true, 2);
        add("shrink3",   "Michael - Father/Son", true, 2);
        // Unsupported: heavy cutscene takeover / solo interior scripting.
        add("prologue1", "Prologue", false, 0);
        add("franklin0", "Franklin - Chop (solo vehicle scripting)", false, 0);
        add("michael1",  "Michael - Friend Request (solo office)", false, 0);
        return r;
    }();
    return registry;
}

} // namespace

const std::map<u64, MissionSync::MissionConfig>& MissionSync::Registry() {
    return BuildRegistry();
}

bool MissionSync::IsSupported(u64 missionHash) {
    const auto& r = Registry();
    auto it = r.find(missionHash);
    return it != r.end() && it->second.supported;
}

const char* MissionSync::MissionName(u64 missionHash) {
    const auto& r = Registry();
    auto it = r.find(missionHash);
    return it != r.end() ? it->second.name.c_str() : "Unknown mission";
}

void MissionSync::Transition(MissionState next, u64 nowMs) {
    if (state_ == next) return;
    Log::Info("mission %s: %s -> %s", MissionName(missionHash_),
              MissionStateName(state_), MissionStateName(next));
    state_ = next;
    lastTransitionMs_ = nowMs;
}

MissionState MissionSync::Tick(u64 nowMs) {
    BOOL_ inMission = GET_MISSION_FLAG();

    if (state_ == MissionState::IDLE) {
        if (inMission && isHost_) {
            // Host entered a mission through normal gameplay. Until the
            // mission identifies itself (registry lookup is approximate),
            // track it generically under hash 0.
            missionHash_ = 0;
            slot_ = PlayerSlot::NONE;
            Transition(MissionState::STARTING, nowMs);
        }
    } else if (state_ == MissionState::STARTING) {
        // Give the original script a couple of seconds to spin up.
        if (inMission) Transition(MissionState::ACTIVE, nowMs);
        else if (nowMs - lastTransitionMs_ > 10000) Transition(MissionState::IDLE, nowMs);
    } else if (state_ == MissionState::ACTIVE || state_ == MissionState::CHECKPOINT) {
        if (!inMission) {
            // Mission flag cleared: the host's script ended. Distinguish
            // pass/fail: a passed mission returns to free roam with the
            // character alive; a failed one shows the game's fail banner.
            // v1: host decides via HostFinishMission from the menu or
            // per-mission config; auto-detected endings default to COMPLETED.
            Transition(MissionState::COMPLETED, nowMs);
        }
    } else if (state_ == MissionState::COMPLETED || state_ == MissionState::FAILED) {
        // Cool-down, then back to idle.
        if (nowMs - lastTransitionMs_ > 5000) {
            missionHash_ = 0;
            checkpoint_ = 0;
            Transition(MissionState::IDLE, nowMs);
        }
    }

    if (state_ == MissionState::ACTIVE && checkpoint_ > 0)
        Transition(MissionState::CHECKPOINT, nowMs);

    return state_;
}

void MissionSync::ApplyNetwork(const proto::MissionStateMsg& msg, u64 nowMs) {
    if (isHost_) return; // host is authority; ignore echoes
    missionHash_ = msg.missionHash;
    checkpoint_ = msg.checkpoint;
    slot_ = PlayerSlot(msg.slot);
    Transition(MissionState(msg.state), nowMs);
}

void MissionSync::HostBeginMission(u64 missionHash, PlayerSlot slot) {
    if (!isHost_) return;
    missionHash_ = missionHash;
    slot_ = slot;
    checkpoint_ = 0;
}

void MissionSync::HostCheckpoint(u64 missionHash, u8 checkpoint) {
    if (!isHost_ || missionHash != missionHash_) return;
    checkpoint_ = checkpoint;
}

void MissionSync::HostFinishMission(u64 missionHash, bool passed) {
    if (!isHost_ || missionHash != missionHash_) return;
    Transition(passed ? MissionState::COMPLETED : MissionState::FAILED, 0);
}

} // namespace coop
