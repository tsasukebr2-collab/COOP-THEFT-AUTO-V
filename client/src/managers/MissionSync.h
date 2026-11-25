// Mission State Manager (cooperative).
//
// State machine per project spec: IDLE -> STARTING -> ACTIVE -> CHECKPOINT ->
// COMPLETED / FAILED (plus UNSUPPORTED for missions we refuse to sync).
//
// Authority: the host runs the original mission script exactly as the game
// intends. Clients never start mission scripts themselves. The host detects
// lifecycle transitions (mission flag, completion via screen/script heuristics
// + per-mission config) and broadcasts MSG_MISSION_STATE. Clients mirror the
// state: they freeze their own mission-flag polling while mirroring and show
// objective text.
//
// Mission identity: u64 hash of the mission's script/blip name. The table
// below marks missions as supported/unsupported. A mission that changes the
// protagonist, uses solo scripted actors or cutscene takeovers is marked
// UNSUPPORTED: those keep single-player flow for the host while clients are
// told to stand down (documented honest handling, no fake sync).
#pragma once

#include "../../shared/coop/Types.h"
#include "../../protocol/coop/protocol/Messages.h"

#include <map>
#include <string>

namespace coop {

class MissionSync {
public:
    struct MissionConfig {
        std::string name;      // human-readable
        bool supported = true; // false => UNSUPPORTED handling
        u8 maxCheckpoints = 0;
    };

    void SetHost(bool host) { isHost_ = host; }

    // Called every tick (script thread). Polls local mission state and, on
    // the host, broadcasts transitions. Returns current mirrored state.
    MissionState Tick(u64 nowMs);

    // Network apply (client side mirrors host state).
    void ApplyNetwork(const proto::MissionStateMsg& msg, u64 nowMs);

    // Host-side API used by the F9 menu ("start coop mission" flow).
    void HostBeginMission(u64 missionHash, PlayerSlot slot);
    void HostCheckpoint(u64 missionHash, u8 checkpoint);
    void HostFinishMission(u64 missionHash, bool passed);

    MissionState Current() const { return state_; }
    u64 CurrentMissionHash() const { return missionHash_; }
    u8 CurrentCheckpoint() const { return checkpoint_; }

    // Static registry: which missions the coop layer supports.
    static const std::map<u64, MissionConfig>& Registry();
    static bool IsSupported(u64 missionHash);
    static const char* MissionName(u64 missionHash);

private:
    void Transition(MissionState next, u64 nowMs);

    bool isHost_ = true;
    MissionState state_ = MissionState::IDLE;
    u64 missionHash_ = 0;
    u8 checkpoint_ = 0;
    PlayerSlot slot_ = PlayerSlot::NONE;
    u64 lastTransitionMs_ = 0;
    bool hadMissionFlag_ = false;
};

} // namespace coop
