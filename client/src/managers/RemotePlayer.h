// Remote player representation: a local ped driven by interpolated snapshots.
#pragma once

#include "../../shared/coop/Types.h"
#include "../../protocol/coop/protocol/Messages.h"
#include "../natives/ShvCompat.h"   // coop::ScrPed

#include <deque>

namespace coop {

class RemotePlayer {
public:
    // Interpolation delay: render position this far behind newest snapshot.
    static constexpr u32 kInterpDelayMs = 100;
    // Beyond this snap distance we teleport instead of interpolating.
    static constexpr f32 kSnapDistance = 10.0f;

    void ApplyMove(const proto::Move& move, u64 nowMs);
    void ApplyClothes(const u8 drawables[12], const u8 textures[12]);

    // Called every script tick while in session.
    void Tick(u64 nowMs);

    u32 ClientId() const { return clientId_; }
    const std::string& Name() const { return name_; }
    void SetName(const std::string& n) { name_ = n; }
    // Ped skin override (0 = protagonist model for the current slot). If a
    // ped already exists, it is recreated with the new model at next tick.
    void SetModel(u64 hash) {
        if (hash == modelHash_) return;
        modelHash_ = hash;
        modelDirty_ = true;
    }
    ScrPed Ped() const { return ped_; }
    bool Valid() const { return ped_ != 0; }
    u64 LastUpdateMs() const { return lastUpdateMs_; }
    PlayerSlot Slot() const { return slot_; }
    u16 Health() const { return health_; }
    u32 PingMs() const { return pingMs_; }
    void SetPingMs(u32 p) { pingMs_ = p; }

private:
    struct Sample {
        u64 t;
        Vec3 pos;
        f32 yaw;
        Vec3 vel;
    };

    void EnsurePed(const Vec3& pos);
    void DestroyPed();

    u32 clientId_ = 0;
    std::string name_;
    PlayerSlot slot_ = PlayerSlot::NONE;
    ScrPed ped_ = 0;
    u64 modelHash_ = 0;      // skin override (MSG_PLAYER_MODEL / PlayerInfo)
    bool modelDirty_ = false;
    u64 lastUpdateMs_ = 0;
    u32 tick_ = 0;
    u16 health_ = 100;
    u8 armour_ = 0;
    u8 dead_ = 0;
    u32 pingMs_ = 0;
    bool inVehicle_ = false;
    u64 vehicleGuid_ = 0;
    u8 seat_ = 255;

    std::deque<Sample> samples_;
    u8 drawables_[12] = {};
    u8 textures_[12] = {};
    bool clothesDirty_ = false;
};

} // namespace coop
