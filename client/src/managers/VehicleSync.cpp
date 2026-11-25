#include "VehicleSync.h"
#include "../natives/Natives.h"

#include <cmath>
#include <vector>

namespace coop {

namespace {
constexpr f32 kSnapDistance = 15.0f;

u8 PackTyres(ScrVehicle v) {
    u8 mask = 0;
    for (int i = 0; i < 8; ++i)
        if (IS_VEHICLE_TYRE_BURST(v, i)) mask |= u8(1 << i);
    return mask;
}

u8 PackFlags(ScrVehicle v) {
    u8 flags = 0;
    i32 lights = GET_VEHICLE_LIGHTS_STATE(v);
    if (lights == 1 || lights == 2) flags |= 1;   // lights on / high beams
    return flags;
}
} // namespace

proto::VehicleState VehicleSync::Capture(ScrVehicle v, u64 guid, u32 ownerId) {
    proto::VehicleState st;
    st.guid = guid;
    st.ownerId = ownerId;

    ScrVec3 c = GET_ENTITY_COORDS(v, 0);
    st.pos = Vec3{ c.x, c.y, c.z };
    st.yaw = GET_ENTITY_HEADING(v);
    st.pitch = GET_ENTITY_PITCH(v);
    st.roll = GET_ENTITY_ROLL(v);
    ScrVec3 vel = GET_ENTITY_VELOCITY(v);
    st.vel = Vec3{ vel.x, vel.y, vel.z };

    st.engineHealth = GET_VEHICLE_ENGINE_HEALTH(v);
    st.bodyHealth = GET_VEHICLE_BODY_HEALTH(v);

    i32 lights = GET_VEHICLE_LIGHTS_STATE(v);
    st.flags = u8((lights == 1 || lights == 2) ? 1 : 0);
    // Siren state has no getter native; we track it client-side when we set it.
    st.tyreMask = PackTyres(v);

    ScrPed driver = GET_PED_IN_VEHICLE_SEAT(v, -1);
    st.driverGuid = 0; // resolved by caller (PlayerManager guid of driver)
    return st;
}

void VehicleSync::ApplyState(const proto::VehicleState& st, u64 nowMs) {
    Entry& e = entries_[st.guid];
    e.last = st;
    e.lastUpdateMs = nowMs;
    e.guid = st.guid;
}

ScrVehicle VehicleSync::EnsureSpawned(u64 guid, ScrHash model, const Vec3& pos, f32 yaw) {
    auto it = entries_.find(guid);
    if (it != entries_.end() && it->second.vehicle != 0 && DOES_ENTITY_EXIST(it->second.vehicle))
        return it->second.vehicle;

    REQUEST_MODEL(model);
    if (!HAS_MODEL_LOADED(model)) return 0;

    // CREATE_VEHICLE(model, x, y, z, heading, isNetwork, unk7, unk8)
    ScrVehicle v = CREATE_VEHICLE(model, pos.x, pos.y, pos.z, yaw, 0, 1, 1);
    if (v == 0) return 0;

    SET_MODEL_AS_NO_LONGER_NEEDED(model);
    SET_ENTITY_AS_MISSION_ENTITY(v);
    SET_VEHICLE_ON_GROUND_PROPERLY(v);

    Entry& e = entries_[guid];
    e.vehicle = v;
    e.guid = guid;
    return v;
}

void VehicleSync::Forget(u64 guid) {
    auto it = entries_.find(guid);
    if (it == entries_.end()) return;
    if (it->second.vehicle != 0 && DOES_ENTITY_EXIST(it->second.vehicle)) {
        ScrEntity e = it->second.vehicle;
        if (!IsOwned(guid)) DELETE_ENTITY(&e);
    }
    entries_.erase(it);
    owned_.erase(guid);
}

VehicleSync::Entry* VehicleSync::Find(u64 guid) {
    auto it = entries_.find(guid);
    return it == entries_.end() ? nullptr : &it->second;
}

void VehicleSync::Tick(u64 nowMs) {
    const u64 kMaxStaleMs = 3000;
    const f32 kSnapDistance = 15.0f;

    std::vector<u64> stale;
    for (auto& kv : entries_) {
        Entry& e = kv.second;
        if (e.vehicle == 0 || !DOES_ENTITY_EXIST(e.vehicle)) continue;
        if (IsOwned(kv.first)) continue;              // our own vehicle
        if (nowMs - e.lastUpdateMs > kMaxStaleMs) {   // driver vanished
            stale.push_back(kv.first);
            continue;
        }
        // Drive remote vehicle toward the last captured state. v1 uses
        // position+heading lerp each frame; full velocity integration and
        // quaternion blending are Phase-3 work (see LIMITATIONS.md).
        ScrVec3 cur = GET_ENTITY_COORDS(e.vehicle, 0);
        f32 dx = e.last.pos.x - cur.x;
        f32 dy = e.last.pos.y - cur.y;
        f32 dz = e.last.pos.z - cur.z;
        f32 dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist > kSnapDistance) {
            SET_ENTITY_COORDS(e.vehicle, e.last.pos.x, e.last.pos.y, e.last.pos.z, 0, 0, 0, 1);
            SET_ENTITY_HEADING(e.vehicle, e.last.yaw);
        } else {
            const f32 k = 0.35f; // smooth approach per tick
            SET_ENTITY_COORDS(e.vehicle,
                              cur.x + dx * k, cur.y + dy * k, cur.z + dz * k,
                              0, 0, 0, 1);
            SET_ENTITY_HEADING(e.vehicle, e.last.yaw);
        }
        SET_VEHICLE_ENGINE_HEALTH(e.vehicle, e.last.engineHealth);
        SET_VEHICLE_BODY_HEALTH(e.vehicle, e.last.bodyHealth);
    }
    for (u64 guid : stale) Forget(guid);
}

} // namespace coop
