#include "WorldSync.h"
#include "../natives/Natives.h"

#include <cmath>

namespace coop {

proto::WorldState WorldSync::Capture(u32 tick) {
    proto::WorldState w;
    w.tick = tick;
    w.clockH = u8(GET_CLOCK_HOURS());
    w.clockM = u8(GET_CLOCK_MINUTES());
    w.clockS = u8(GET_CLOCK_SECONDS());
    w.wanted = u8(GET_PLAYER_WANTED_LEVEL(PLAYER_ID()));
    w.paused = 0;
    // Weather: host tracks the selection it set / received (game has no
    // plain "current weather" getter); 255 = untouched.
    w.weatherSel = lastWeather_;
    return w;
}

void WorldSync::Apply(const proto::WorldState& st, u64 nowMs) {
    lastApplyMs_ = u32(nowMs);

    if (st.clockH != lastClockH_ || st.clockM != lastClockM_ || st.clockS != lastClockS_) {
        i32 h = GET_CLOCK_HOURS();
        i32 m = GET_CLOCK_MINUTES();
        i32 s = GET_CLOCK_SECONDS();
        // Snap when drift >= 2 seconds on any wall-clock mapping.
        i32 localSec = h * 3600 + m * 60 + s;
        i32 hostSec = st.clockH * 3600 + st.clockM * 60 + st.clockS;
        i32 diff = localSec - hostSec;
        if (diff < -1080 || diff > 1080 || st.clockH != lastClockH_) { // >18 min drift or hour jump
            SET_CLOCK_TIME(st.clockH, st.clockM, st.clockS);
        }
        lastClockH_ = st.clockH;
        lastClockM_ = st.clockM;
        lastClockS_ = st.clockS;
    }

    if (st.weatherSel < kWeatherCount && st.weatherSel != lastWeather_) {
        SET_WEATHER_TYPE_NOW_PERSIST(kWeatherTable[st.weatherSel]);
        lastWeather_ = st.weatherSel;
    }

    if (st.wanted != lastWanted_) {
        ScrPlayer me = PLAYER_ID();
        i32 cur = GET_PLAYER_WANTED_LEVEL(me);
        if (st.wanted > cur) {
            SET_PLAYER_WANTED_LEVEL(me, st.wanted, 0);
            SET_PLAYER_WANTED_LEVEL_NOW(me, 0);
        } else if (st.wanted < cur) {
            // Local player's wanted level is only lowered by host authority
            // when the local one is "stale" (cop died remotely etc.).
            SET_PLAYER_WANTED_LEVEL(me, st.wanted, 0);
            SET_PLAYER_WANTED_LEVEL_NOW(me, 0);
        }
        lastWanted_ = st.wanted;
    }
}

void WorldSync::Tick(u64) {
    // Reserved for future drift smoothing (interpolated clock).
}

} // namespace coop
