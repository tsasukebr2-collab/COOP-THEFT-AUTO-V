// CO-OP save layer. PRIORITY-1 subsystem (client side).
//
// The authoritative save lives on the server (SaveStore). The client:
//  - receives the full blob at WELCOME (initial load),
//  - pulls local game state (weapons, health stats, position, mission
//    progress markers) and pushes a full snapshot to the host at autosave
//    intervals or on demand (menu / mission end),
//  - applies incoming SAVE_SYNC blobs by validating format + version.
//
// The game's native savefiles are never read or written by this layer.
// What we persist (v1): per-slot weapons/ammo, money mirror (informational;
// SP wallet has no stable native - see LIMITATIONS.md), mission progress
// bitset, world prefs (weather/clock), session meta.
#pragma once

#include "../../shared/coop/Types.h"
#include "../../shared/coop/save/SaveStore.h"
#include "../../protocol/coop/protocol/Messages.h"

#include <string>

namespace coop {

struct CoopSaveState {
    // Per protagonist slot.
    struct SlotData {
        i64 money = 0;
        struct Weapon { u32 hash; u16 ammo; };
        Weapon weapons[16] = {};
        u8 weaponCount = 0;
        u16 health = 100;
        u8 armour = 0;
        u8 unlocked = 0; // bit0 Michael, bit1 Franklin, bit2 Trevor
    };
    SlotData slots[3];

    // Story progress: bitset of mission registry indices (0..255 supported).
    u8 missionBits[32] = {};      // 256 missions max
    u8 collectibleBits[16] = {};  // 128 flags

    u8 weatherSel = 255;
    i32 clockOffsetSec = 0;
    u64 playtimeSec = 0;

    // Seamless join: last host position when the save was written. Joining
    // clients spawn here (Seamless Co-op model: no menus, appear in-world).
    Vec3 lastPos{ 0, 0, 0 };
    bool lastPosValid = false;
};

class SaveManager {
public:
    void Init(const std::string& localPath);
    void Shutdown();

    // Called once after WELCOME with the server's blob.
    void AdoptServerBlob(const std::vector<u8>& blob, u64 version);

    // Pull current game state into the coop state (script thread).
    void PullFromGame();

    // Apply coop state into the game (weapons etc.; script thread).
    void ApplyToGame();

    // Serialize current state; pushes to host via net layer (CoopContext does
    // the send). Returns serialized blob.
    std::vector<u8> Serialize(u64 newVersion) const;

    // Validate + merge an incoming shared save (host broadcast).
    bool ApplyIncoming(const proto::SaveSync& msg);

    // Autosave bookkeeping.
    bool ShouldAutosave(u64 nowMs, u64 intervalMs) const;
    void MarkSaved(u64 nowMs);

    u64 Version() const { return version_; }
    const CoopSaveState& State() const { return state_; }
    CoopSaveState& MutableState() { return state_; }

    // Seamless join: spawn position stored by the last save (host pos).
    bool JoinPosition(Vec3& out) const {
        if (!state_.lastPosValid) return false;
        out = state_.lastPos;
        return true;
    }

    // Mission progress helpers.
    bool IsMissionDone(u8 index) const;
    void SetMissionDone(u8 index);

private:
    CoopSaveState state_;
    u64 version_ = 0;
    u64 lastSaveMs_ = 0;
    std::string localPath_;
};

} // namespace coop
