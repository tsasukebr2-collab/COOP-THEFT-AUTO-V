// Fixed-point quantization for wire efficiency.
// Positions: mm resolution (1e-3 m), i32 -> +-2147 km, well beyond map.
// Velocities: 1/64 m/s, i16 -> +-512 m/s.
// Yaw: u16 full circle -> 0.0055 degrees.
#pragma once

#include "Types.h"

#include <cmath>

namespace coop {

struct QuantizedPos { i32 x, y, z; };
struct QuantizedVel { i16 x, y, z; };
struct QuantizedYaw { u16 yaw; };

inline QuantizedPos PackPos(const Vec3& v) {
    return { (i32)std::lround(v.x * 1000.0f),
             (i32)std::lround(v.y * 1000.0f),
             (i32)std::lround(v.z * 1000.0f) };
}
inline Vec3 UnpackPos(const QuantizedPos& q) {
    return { q.x / 1000.0f, q.y / 1000.0f, q.z / 1000.0f };
}

inline QuantizedVel PackVel(const Vec3& v) {
    auto clamp16 = [](f32 x) -> i16 {
        f32 q = std::lround(x * 64.0f);
        if (q > 32767.0f) q = 32767.0f;
        if (q < -32768.0f) q = -32768.0f;
        return (i16)q;
    };
    return { clamp16(v.x), clamp16(v.y), clamp16(v.z) };
}
inline Vec3 UnpackVel(const QuantizedVel& q) {
    return { q.x / 64.0f, q.y / 64.0f, q.z / 64.0f };
}

inline u16 PackYaw(f32 deg) {
    f32 norm = std::fmod(deg, 360.0f);
    if (norm < 0) norm += 360.0f;
    return u16(std::lround(norm * (65535.0f / 360.0f)));
}
inline f32 UnpackYaw(u16 p) { return p * (360.0f / 65535.0f); }

} // namespace coop
