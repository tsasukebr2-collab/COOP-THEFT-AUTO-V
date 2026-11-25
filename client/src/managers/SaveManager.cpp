#include "SaveManager.h"
#include "../natives/Natives.h"
#include "../../shared/coop/Crc32.h"
#include "../../shared/coop/Log.h"

#include <cstring>

namespace coop {

namespace {
// Small serialization helpers keep the format explicit and testable.
// They must precede ParseWorldSection, which uses them.
void WriteU32(std::vector<u8>& v, u32 x) {
    v.push_back(u8(x)); v.push_back(u8(x >> 8));
    v.push_back(u8(x >> 16)); v.push_back(u8(x >> 24));
}
void WriteU64(std::vector<u8>& v, u64 x) {
    WriteU32(v, u32(x)); WriteU32(v, u32(x >> 32));
}
u32 ReadU32(const u8* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (u32(p[3]) << 24); }
u64 ReadU64(const u8* p) { return u64(ReadU32(p)) | (u64(ReadU32(p + 4)) << 32); }

// WORLD payload layout: [i32 clockOffset][u8 weather][u8 posValid][f32 x][f32 y][f32 z]
// = 4 + 1 + 1 + 12 = 18 bytes.
void ParseWorldSection(const SaveSection* s, CoopSaveState& st) {
    if (!s || s->payload.size() < 6 + 12) return;
    const u8* p = s->payload.data();
    st.clockOffsetSec = (i32)ReadU32(p);
    st.weatherSel = p[4];
    st.lastPosValid = p[5] != 0;
    f32 px, py, pz;
    std::memcpy(&px, p + 6, 4);
    std::memcpy(&py, p + 10, 4);
    std::memcpy(&pz, p + 14, 4);
    st.lastPos = Vec3{ px, py, pz };
}
} // namespace

void SaveManager::Init(const std::string& localPath) {
    localPath_ = localPath;

    // Local mirror: resume-from-cache when the server is unreachable.
    SaveStore local(localPath_);
    SaveData data;
    SaveLoadResult res = local.Load(data);
    if (res.ok) {
        if (const SaveSection* s = data.Find(SEC_PLAYER)) {
            if (s->payload.size() >= sizeof(state_.slots)) {
                std::memcpy(state_.slots, s->payload.data(), sizeof(state_.slots));
            }
        }
        if (const SaveSection* s = data.Find(SEC_PROGRESS)) {
            if (s->payload.size() >= sizeof(state_.missionBits)) {
                std::memcpy(state_.missionBits, s->payload.data(), sizeof(state_.missionBits));
            }
        }
        if (const SaveSection* s = data.Find(SEC_META)) {
            if (s->payload.size() >= 8) {
                // META layout: [u64 saveVersion][u64 reserved].
                version_ = ReadU64(s->payload.data());
            }
        }
        Log::Info("local coop save v%llu loaded from %s",
                  (unsigned long long)version_, res.source.c_str());
    }
}

void SaveManager::Shutdown() {
    // Best-effort local mirror write (server may be gone).
    SaveStore local(localPath_);
    SaveData data;
    data.SetSection(SEC_PLAYER, state_.slots, sizeof(state_.slots));
    data.SetSection(SEC_PROGRESS, state_.missionBits, sizeof(state_.missionBits));
    std::vector<u8> meta;
    WriteU64(meta, version_); // version guard field (read by SaveStore::Load/Write)
    WriteU64(meta, 0);        // reserved (session id, stamped by the server)
    data.SetSection(SEC_META, meta.data(), meta.size());
    local.Write(data, version_ + 1);
}

void SaveManager::AdoptServerBlob(const std::vector<u8>& blob, u64 version) {
    SaveData incoming;
    std::string err;
    if (!SaveStore::Parse(blob, incoming, err)) {
        Log::Error("server save blob invalid: %s", err.c_str());
        return;
    }
    if (version < version_) {
        // Crash-recovery case: our local mirror is newer than the server's.
        // We keep ours and will push it to the host on next autosave.
        Log::Warn("server save v%llu older than local v%llu - keeping local",
                  (unsigned long long)version, (unsigned long long)version_);
        return;
    }
    if (const SaveSection* s = incoming.Find(SEC_PLAYER)) {
        if (s->payload.size() >= sizeof(state_.slots))
            std::memcpy(state_.slots, s->payload.data(), sizeof(state_.slots));
    }
    if (const SaveSection* s = incoming.Find(SEC_PROGRESS)) {
        if (s->payload.size() >= sizeof(state_.missionBits))
            std::memcpy(state_.missionBits, s->payload.data(), sizeof(state_.missionBits));
    }
    ParseWorldSection(incoming.Find(SEC_WORLD), state_);
    version_ = version;
    ApplyToGame();
    Log::Info("adopted server save v%llu", (unsigned long long)version);
}

void SaveManager::PullFromGame() {
    // Only the local player's current protagonist data is pulled; the other
    // slots are pulled when a player switches to them.
    ScrPed ped = PLAYER_PED_ID();

    CoopSaveState::SlotData& slot = state_.slots[u8(0)]; // refine: slot by protagonist id
    slot.health = u16(GET_ENTITY_HEALTH(ped));
    slot.armour = u8(GET_PED_ARMOUR(ped));

    // Seamless: host position rides along in every pull.
    ScrVec3 c = GET_ENTITY_COORDS(ped, 0);
    state_.lastPos = Vec3{ c.x, c.y, c.z };
    state_.lastPosValid = true;

    // Current weapon + ammo (v1 captures current weapon only; full inventory
    // snapshot requires per-weapon natives iteration, see LIMITATIONS.md).
    ScrHash wep = GET_SELECTED_PED_WEAPON(ped);
    if (wep != 0 && slot.weaponCount < 16) {
        bool present = false;
        for (u8 i = 0; i < slot.weaponCount; ++i)
            if (slot.weapons[i].hash == wep) { present = true; break; }
        if (!present) {
            slot.weapons[slot.weaponCount].hash = wep;
            ++slot.weaponCount;
        }
        for (u8 i = 0; i < slot.weaponCount; ++i)
            if (slot.weapons[i].hash == wep)
                slot.weapons[i].ammo = u16(GET_AMMO_IN_PED_WEAPON(ped, wep));
    }
}

void SaveManager::ApplyToGame() {
    ScrPed ped = PLAYER_PED_ID();
    // Weapons restore for the current slot.
    for (u8 i = 0; i < state_.slots[0].weaponCount; ++i) {
        const auto& w = state_.slots[0].weapons[i];
        GIVE_WEAPON_TO_PED(ped, ScrHash(w.hash), w.ammo, 0, 0);
        SET_PED_AMMO(ped, ScrHash(w.hash), w.ammo, 0);
    }
    if (state_.slots[0].health > 0)
        SET_ENTITY_HEALTH(ped, state_.slots[0].health);
    if (state_.slots[0].armour > 0)
        SET_PED_ARMOUR(ped, state_.slots[0].armour);
}

std::vector<u8> SaveManager::Serialize(u64 newVersion) const {
    SaveData data;

    std::vector<u8> meta;
    WriteU64(meta, newVersion); // version guard field read by SaveStore (first 8 bytes)
    WriteU64(meta, 0);          // reserved (session id, stamped by the server)
    data.SetSection(SEC_META, meta.data(), meta.size());

    data.SetSection(SEC_PLAYER, state_.slots, sizeof(state_.slots));
    data.SetSection(SEC_PROGRESS, state_.missionBits, sizeof(state_.missionBits));
    data.SetSection(SEC_STATS, &state_.playtimeSec, sizeof(state_.playtimeSec));

    std::vector<u8> world;
    WriteU32(world, u32(state_.clockOffsetSec));
    world.push_back(state_.weatherSel);
    // Seamless join position.
    u8 posValid = state_.lastPosValid ? 1 : 0;
    world.push_back(posValid);
    f32 px = state_.lastPos.x, py = state_.lastPos.y, pz = state_.lastPos.z;
    world.insert(world.end(), (u8*)&px, (u8*)&px + 4);
    world.insert(world.end(), (u8*)&py, (u8*)&py + 4);
    world.insert(world.end(), (u8*)&pz, (u8*)&pz + 4);
    data.SetSection(SEC_WORLD, world.data(), world.size());

    return SaveStore::Serialize(data);
}

bool SaveManager::ApplyIncoming(const proto::SaveSync& msg) {
    SaveData incoming;
    std::string err;
    if (!SaveStore::Parse(msg.blob, incoming, err)) {
        Log::Error("incoming save invalid: %s", err.c_str());
        return false;
    }
    if (msg.version < version_) {
        Log::Warn("incoming save v%llu older than local v%llu - ignored",
                  (unsigned long long)msg.version, (unsigned long long)version_);
        return false;
    }
    if (const SaveSection* s = incoming.Find(SEC_PLAYER)) {
        if (s->payload.size() >= sizeof(state_.slots))
            std::memcpy(state_.slots, s->payload.data(), sizeof(state_.slots));
    }
    if (const SaveSection* s = incoming.Find(SEC_PROGRESS)) {
        if (s->payload.size() >= sizeof(state_.missionBits))
            std::memcpy(state_.missionBits, s->payload.data(), sizeof(state_.missionBits));
    }
    ParseWorldSection(incoming.Find(SEC_WORLD), state_);
    version_ = msg.version;
    ApplyToGame();
    return true;
}

bool SaveManager::ShouldAutosave(u64 nowMs, u64 intervalMs) const {
    return intervalMs > 0 && nowMs - lastSaveMs_ >= intervalMs;
}

void SaveManager::MarkSaved(u64 nowMs) { lastSaveMs_ = nowMs; }

bool SaveManager::IsMissionDone(u8 index) const {
    return (state_.missionBits[index >> 3] >> (index & 7)) & 1;
}

void SaveManager::SetMissionDone(u8 index) {
    state_.missionBits[index >> 3] |= u8(1 << (index & 7));
}

} // namespace coop
