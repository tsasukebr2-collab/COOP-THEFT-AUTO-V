#include "PlayerManager.h"
#include "../natives/Natives.h"

#include <cstring>

namespace coop {

proto::Move PlayerManager::CaptureLocalMove(u32 tick, u64 vehicleGuid, u8 seat) {
    proto::Move m;
    ScrPed ped = PLAYER_PED_ID();
    ScrPlayer pl = PLAYER_ID();

    m.clientId = local_.clientId;
    m.tick = tick;
    ScrVec3 c = GET_ENTITY_COORDS(ped, 0);
    m.pos = Vec3{ c.x, c.y, c.z };
    m.yaw = GET_ENTITY_HEADING(ped);
    ScrVec3 v = GET_ENTITY_VELOCITY(ped);
    m.vel = Vec3{ v.x, v.y, v.z };
    m.health = u16(GET_ENTITY_HEALTH(ped) > 0 ? GET_ENTITY_HEALTH(ped) : 0);
    m.armour = u8(GET_PED_ARMOUR(ped));
    m.slot = u8(local_.slot);
    m.inVehicle = IS_PED_IN_ANY_VEHICLE(ped) ? 1 : 0;
    m.vehicleGuid = vehicleGuid;
    m.seat = seat;
    m.weaponHash = GET_SELECTED_PED_WEAPON(ped);
    m.ammo = u16(GET_AMMO_IN_PED_WEAPON(ped, ScrHash(m.weaponHash)));
    m.dead = IS_ENTITY_DEAD(ped) ? 1 : 0;
    return m;
}

void PlayerManager::CaptureClothes(u8 outDrawables[12], u8 outTextures[12]) {
    ScrPed ped = PLAYER_PED_ID();
    for (int c = 0; c < 12; ++c) {
        outDrawables[c] = u8(GET_PED_DRAWABLE_VARIATION(ped, c));
        outTextures[c] = u8(GET_PED_TEXTURE_VARIATION(ped, c));
    }
}

RemotePlayer* PlayerManager::GetRemote(u32 clientId) {
    std::lock_guard<std::mutex> lk(remoteMutex_);
    auto it = remotes_.find(clientId);
    return it == remotes_.end() ? nullptr : it->second;
}

void PlayerManager::ApplyMove(const proto::Move& move, u64 nowMs) {
    RemotePlayer* rp = nullptr;
    {
        std::lock_guard<std::mutex> lk(remoteMutex_);
        auto it = remotes_.find(move.clientId);
        if (it == remotes_.end()) {
            // Unknown sender: create implicitly so late joiners still render.
            rp = new RemotePlayer();
            remotes_[move.clientId] = rp;
        } else {
            rp = it->second;
        }
    }
    rp->ApplyMove(move, nowMs);
}

void PlayerManager::RemoveRemote(u32 clientId) {
    RemotePlayer* rp = nullptr;
    {
        std::lock_guard<std::mutex> lk(remoteMutex_);
        auto it = remotes_.find(clientId);
        if (it == remotes_.end()) return;
        rp = it->second;
        remotes_.erase(it);
        pendingModels_.erase(clientId);
    }
    delete rp;
}

size_t PlayerManager::RemoteCount() {
    std::lock_guard<std::mutex> lk(remoteMutex_);
    return remotes_.size();
}

std::vector<RemotePlayer*> PlayerManager::AllRemotes() {
    std::lock_guard<std::mutex> lk(remoteMutex_);
    std::vector<RemotePlayer*> out;
    out.reserve(remotes_.size());
    for (auto& kv : remotes_) out.push_back(kv.second);
    return out;
}

void PlayerManager::AddRemote(u32 clientId, const std::string& name, u64 modelHash) {
    RemotePlayer* rp;
    {
        std::lock_guard<std::mutex> lk(remoteMutex_);
        auto it = remotes_.find(clientId);
        if (it != remotes_.end()) {
            it->second->SetName(name);
            if (modelHash != 0) it->second->SetModel(modelHash);
            auto pending = pendingModels_.find(clientId);
            if (pending != pendingModels_.end()) {
                it->second->SetModel(pending->second);
                pendingModels_.erase(pending);
            }
            return;
        }
        rp = new RemotePlayer();
        rp->SetName(name);
        remotes_[clientId] = rp;
    }
    // Skin order: explicit modelHash (server-stamped PLAYER_JOINED) wins,
    // then any skin announced before the join message arrived.
    if (modelHash != 0) rp->SetModel(modelHash);
    auto pending = pendingModels_.find(clientId);
    if (pending != pendingModels_.end()) {
        rp->SetModel(pending->second);
        pendingModels_.erase(pending);
    }
}

void PlayerManager::SetRemoteModel(u32 clientId, u64 modelHash) {
    std::lock_guard<std::mutex> lk(remoteMutex_);
    auto it = remotes_.find(clientId);
    if (it != remotes_.end()) {
        it->second->SetModel(modelHash);
    } else {
        // PLAYER_JOINED hasn't reached us yet (or raced it): stash.
        pendingModels_[clientId] = modelHash;
    }
}

void PlayerManager::Tick(u64 nowMs) {
    // Copy list under lock, tick outside (natives, no lock held).
    std::vector<RemotePlayer*> list = AllRemotes();
    for (RemotePlayer* rp : list) rp->Tick(nowMs);
}

} // namespace coop
