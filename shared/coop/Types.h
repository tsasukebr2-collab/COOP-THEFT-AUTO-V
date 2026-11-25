// GTAV-Coop shared fundamental types.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace coop {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;

// Max players supported by the wire protocol (architecturally expandable:
// widen fields in Messages.h and bump PROTOCOL_VERSION).
constexpr u32 kMaxPlayers = 8;

// Network protocol version. Clients and server must agree exactly.
constexpr u32 kProtocolVersion = 1;

// Default ports (overridable by server config / client menu).
constexpr u16 kDefaultTcpPort = 31999;
constexpr u16 kDefaultUdpPort = 31998;

struct Vec3 {
    f32 x = 0.f, y = 0.f, z = 0.f;
    Vec3() = default;
    Vec3(f32 px, f32 py, f32 pz) : x(px), y(py), z(pz) {}
};

struct Quat {
    f32 x = 0.f, y = 0.f, z = 0.f, w = 1.f;
};

// Persistent entity identifier.
// High 32 bits: owner slot id (0 = host). Low 32: owner-local counter.
inline u64 MakeGuid(u32 ownerId, u32 localId) { return (u64(ownerId) << 32) | localId; }
inline u32 GuidOwner(u64 guid) { return u32(guid >> 32); }
inline u32 GuidLocal(u64 guid) { return u32(guid & 0xffffffffull); }

// Session identifier (random, generated when a session is created).
struct SessionId {
    u64 hi = 0, lo = 0;
    bool operator==(const SessionId& o) const { return hi == o.hi && lo == o.lo; }
    std::string ToString() const;
    static SessionId FromString(const std::string& s);
    static SessionId Generate();
};

// Mission lifecycle state machine, shared client/server.
enum class MissionState : u8 {
    IDLE = 0,
    STARTING = 1,
    ACTIVE = 2,
    CHECKPOINT = 3,
    COMPLETED = 4,
    FAILED = 5,
    UNSUPPORTED = 6, // detected but not coop-safe; treated specially
};

const char* MissionStateName(MissionState s);

enum class PlayerSlot : u8 {
    MICHAEL = 0,
    FRANKLIN = 1,
    TREVOR = 2,
    NONE = 255,
};

} // namespace coop
