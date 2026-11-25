// Local player state + remote player roster.
#pragma once

#include "RemotePlayer.h"
#include "../natives/ShvCompat.h"

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace coop {

struct LocalPlayerState {
    std::string name = "Player";
    u32 clientId = 0;
    bool isHost = false;
    PlayerSlot slot = PlayerSlot::FRANKLIN; // current protagonist
};

class PlayerManager {
public:
    // ---- local -------------------------------------------------------------
    LocalPlayerState& Local() { return local_; }

    // Snapshot own ped for the wire (script thread only).
    proto::Move CaptureLocalMove(u32 tick, u64 vehicleGuid, u8 seat);

    // ---- remote ------------------------------------------------------------
    RemotePlayer* GetRemote(u32 clientId);
    void ApplyMove(const proto::Move& move, u64 nowMs);
    void RemoveRemote(u32 clientId);
    size_t RemoteCount();
    std::vector<RemotePlayer*> AllRemotes(); // caller must not delete

    // Roster maintenance driven by session messages. `modelHash` comes from
    // PLAYER_JOINED (server-stamped) so late joiners see existing skins.
    void AddRemote(u32 clientId, const std::string& name, u64 modelHash = 0);
    // Live skin update (MSG_PLAYER_MODEL); stashed if the remote ped does
    // not exist yet and applied on AddRemote.
    void SetRemoteModel(u32 clientId, u64 modelHash);

    // Ticks all remote player representations (script thread).
    void Tick(u64 nowMs);

    // Sends local clothes to remote peers (change detection).
    void CaptureClothes(u8 outDrawables[12], u8 outTextures[12]);

private:
    LocalPlayerState local_;

    std::mutex remoteMutex_;
    std::map<u32, RemotePlayer*> remotes_;
    std::map<u32, u64> pendingModels_; // skins for players not spawned yet
    u8 lastDrawables_[12] = {};
    u8 lastTextures_[12] = {};
};

} // namespace coop
