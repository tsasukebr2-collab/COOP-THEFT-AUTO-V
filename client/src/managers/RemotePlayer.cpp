#include "RemotePlayer.h"
#include "../natives/Natives.h"
#include "../../shared/coop/Log.h"

#include <cmath>
#include <cstring>

namespace coop {

namespace {
// Protagonist model hashes (story mode characters).
constexpr u64 kModelMichael  = 0x9C9CF888ull; // player_zero
constexpr u64 kModelFranklin = 0xD34F2B07ull; // player_one
constexpr u64 kModelTrevor   = 0x4F25A626ull; // player_two

ScrHash ModelForSlot(PlayerSlot slot) {
    switch (slot) {
        case PlayerSlot::MICHAEL:  return ScrHash(kModelMichael);
        case PlayerSlot::FRANKLIN: return ScrHash(kModelFranklin);
        case PlayerSlot::TREVOR:   return ScrHash(kModelTrevor);
        default: return ScrHash(kModelFranklin);
    }
}
} // namespace

void RemotePlayer::ApplyMove(const proto::Move& move, u64 nowMs) {
    clientId_ = move.clientId;
    tick_ = move.tick;
    slot_ = PlayerSlot(move.slot);
    health_ = move.health;
    armour_ = move.armour;
    dead_ = move.dead;
    inVehicle_ = move.inVehicle != 0;
    vehicleGuid_ = move.vehicleGuid;
    seat_ = move.seat;

    Sample s;
    s.t = nowMs;
    s.pos = move.pos;
    s.yaw = move.yaw;
    s.vel = move.vel;

    // Snap on first sample or big jumps.
    bool snap = samples_.empty();
    if (!snap) {
        const Sample& last = samples_.back();
        f32 dx = s.pos.x - last.pos.x, dy = s.pos.y - last.pos.y, dz = s.pos.z - last.pos.z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) > kSnapDistance) snap = true;
    }

    if (snap) {
        samples_.clear();
        samples_.push_back(s);
    } else {
        samples_.push_back(s);
        while (samples_.size() > 4) samples_.pop_front();
    }
    lastUpdateMs_ = nowMs;
}

void RemotePlayer::ApplyClothes(const u8 drawables[12], const u8 textures[12]) {
    std::memcpy(drawables_, drawables, 12);
    std::memcpy(textures_, textures, 12);
    clothesDirty_ = true;
}

void RemotePlayer::EnsurePed(const Vec3& pos) {
    if (ped_ != 0 && DOES_ENTITY_EXIST(ped_)) return;
    ped_ = 0;

    // Skin override wins; slot protagonist is the fallback.
    ScrHash model = modelHash_ != 0 ? ScrHash(modelHash_) : ModelForSlot(slot_);
    REQUEST_MODEL(model);
    if (!HAS_MODEL_LOADED(model)) return; // retry next tick while streaming

    // CREATE_PED(pedType, model, x, y, z, heading, isNetwork, keepTask)
    ScrPed p = CREATE_PED(4, model, pos.x, pos.y, pos.z, 0.f, 0, 1);
    if (p == 0) return;

    SET_MODEL_AS_NO_LONGER_NEEDED(model);
    SET_ENTITY_AS_MISSION_ENTITY(p, 1, 1);
    SET_ENTITY_INVINCIBLE(p, 0, 0);
    SET_PED_CAN_RAGDOLL(p, 1);
    SET_PED_ACCURACY(p, 100);
    ped_ = p;
    clothesDirty_ = true;
    Log::Info("remote player ped created (slot %u)", u8(slot_));
}

void RemotePlayer::Tick(u64 nowMs) {
    if (samples_.empty()) return;

    if (nowMs - lastUpdateMs_ > 3000) {
        // Player vanished (disconnect): remove representation.
        DestroyPed();
        return;
    }

    // Interpolated position: render kInterpDelayMs behind newest sample.
    u64 renderT = nowMs >= kInterpDelayMs ? nowMs - kInterpDelayMs : 0;
    const Sample* a = &samples_.back();
    const Sample* b = &samples_.back();
    for (size_t i = 0; i + 1 < samples_.size(); ++i) {
        if (samples_[i].t <= renderT && samples_[i + 1].t >= renderT) {
            a = &samples_[i];
            b = &samples_[i + 1];
            break;
        }
    }
    f32 alpha = 0.f;
    if (b->t > a->t) {
        alpha = f32(double(renderT - a->t) / double(b->t - a->t));
        if (alpha > 1.f) alpha = 1.f;
        if (alpha < 0.f) alpha = 0.f;
    }
    Vec3 pos{ a->pos.x + (b->pos.x - a->pos.x) * alpha,
              a->pos.y + (b->pos.y - a->pos.y) * alpha,
              a->pos.z + (b->pos.z - a->pos.z) * alpha };
    f32 yaw = a->yaw + (b->yaw - a->yaw) * alpha;

    // Skin change: recreate the ped in place (samples/interpolation are kept
    // so the transition is seamless for the observer).
    if (modelDirty_) {
        if (ped_ != 0 && DOES_ENTITY_EXIST(ped_)) {
            ScrPed p = ped_;
            DELETE_PED(&p);
        }
        ped_ = 0;
        modelDirty_ = false;
        clothesDirty_ = true;
    }

    if (ped_ == 0 || !DOES_ENTITY_EXIST(ped_)) {
        EnsurePed(pos);
        if (ped_ == 0) return;
    }

    SET_ENTITY_COORDS(ped_, pos.x, pos.y, pos.z, 0, 0, 0, 1);
    SET_ENTITY_HEADING(ped_, yaw);

    if (dead_ && !IS_ENTITY_DEAD(ped_, 0)) SET_ENTITY_HEALTH(ped_, 0, 0, 0);
    if (!dead_ && IS_ENTITY_DEAD(ped_, 0)) SET_ENTITY_HEALTH(ped_, health_, 0, 0);

    if (clothesDirty_) {
        for (int c = 0; c < 12; ++c)
            SET_PED_COMPONENT_VARIATION(ped_, c, drawables_[c], textures_[c], 0);
        clothesDirty_ = false;
    }
}

void RemotePlayer::DestroyPed() {
    if (ped_ != 0 && DOES_ENTITY_EXIST(ped_)) {
        ScrPed p = ped_;
        DELETE_PED(&p);
    }
    ped_ = 0;
    samples_.clear();
}

} // namespace coop
