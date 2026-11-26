// World state sync: clock, weather, wanted level, shared explosions.
// The host is the authority and broadcasts at 2 Hz; clients apply.
#pragma once

#include "../../shared/coop/Types.h"
#include "../../protocol/coop/protocol/Messages.h"

namespace coop {

// Fixed weather table (index -> native weather type name).
static const char* const kWeatherTable[] = {
    "EXTRASUNNY", "CLEAR", "CLOUDS", "SMOG", "FOGGY", "OVERCAST",
    "RAIN", "THUNDER", "CLEARING", "NEUTRAL", "SNOW", "BLIZZARD", "SNOWLIGHT",
};
constexpr u8 kWeatherCount = 13;

class WorldSync {
public:
    void SetHost(bool host) { isHost_ = host; }

    // Host: capture and broadcast (CoopContext calls at 2 Hz).
    proto::WorldState Capture(u32 tick);

    // Client: apply host world state (script thread).
    void Apply(const proto::WorldState& st, u64 nowMs);

    // Clock drift correction: if local clock differs by >= 2s, snap.
    void Tick(u64 nowMs);

    u32 LastAppliedMs() const { return lastApplyMs_; }

private:
    bool isHost_ = false;
    u32 lastApplyMs_ = 0;
    u8 lastClockH_ = 255;
    u8 lastClockM_ = 255;
    u8 lastClockS_ = 255;
    u8 lastWeather_ = 255;
    u8 lastWanted_ = 255;
};

} // namespace coop
