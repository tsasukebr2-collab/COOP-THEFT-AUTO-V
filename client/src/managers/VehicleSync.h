// Vehicle synchronization. The driver is the physics authority: whoever
// occupies the driver seat broadcasts state at 20 Hz; everyone else applies
// it with interpolation. A vehicle with no driver inside our session is
// frozen in place on remote clients (no physics drift).
#pragma once

#include "../../shared/coop/Types.h"
#include "../../protocol/coop/protocol/Messages.h"

#include <map>

namespace coop {

class VehicleSync {
public:
    struct Entry {
        ScrVehicle vehicle = 0;
        u64 guid = 0;
        u64 lastUpdateMs = 0;
        proto::VehicleState last;
    };

    // Local capture (driver or spawn owner).
    proto::VehicleState Capture(ScrVehicle v, u64 guid, u32 ownerId);

    void ApplyState(const proto::VehicleState& st, u64 nowMs);

    // Creates the local vehicle for a guid announced by the network.
    // Returns 0 while the model is still streaming (retry next tick).
    ScrVehicle EnsureSpawned(u64 guid, ScrHash model, const Vec3& pos, f32 yaw);

    void Forget(u64 guid);

    Entry* Find(u64 guid);
    const std::map<u64, Entry>& All() const { return entries_; }

    // Called every script tick while in session: interpolates remote
    // vehicles toward their latest networked state.
    void Tick(u64 nowMs);

    // Marks guid as locally-owned (we are driver/owner => we broadcast it).
    void SetOwned(u64 guid, bool owned) { owned_[guid] = owned; }
    bool IsOwned(u64 guid) const {
        auto it = owned_.find(guid);
        return it != owned_.end() && it->second;
    }

private:
    std::map<u64, Entry> entries_;
    std::map<u64, bool> owned_;
};

} // namespace coop
