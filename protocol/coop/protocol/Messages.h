// Wire message definitions shared by client, server and tests.
#pragma once

#include "../../shared/coop/Types.h"

#include <string>
#include <vector>

namespace coop {
namespace proto {

// ---- Message IDs -----------------------------------------------------------
// C2S_* : client -> server.  S2C_* : server -> client (relay or own state).
enum MsgId : u8 {
    // TCP control channel
    MSG_HELLO          = 1,  // C2S: handshake {protoVer, name, clientVersion}
    MSG_WELCOME        = 2,  // S2C: {clientId, sessionToken, saveVersion, saveBlob}
    MSG_JOIN_REJECT    = 3,  // S2C: {reason, text}
    MSG_PLAYER_JOINED  = 4,  // S2C: {clientId, name}
    MSG_PLAYER_LEFT    = 5,  // S2C: {clientId, reason}
    MSG_CHAT           = 6,  // C2S/S2C: {clientId, text}
    MSG_MISSION_STATE  = 7,  // C2S/S2C: {missionHash, state, checkpoint, resultSlot}
    MSG_SAVE_SYNC      = 8,  // S2C broadcast / C2S upload: {version, blob}
    MSG_SPAWN_REQUEST  = 9,  // C2S: {entityType, modelHash, pos, yaw, requestTag}
    MSG_SPAWN          = 10, // S2C: {guid, entityType, modelHash, pos, yaw}
    MSG_DESPAWN        = 11, // C2S/S2C: {guid, reason}
    MSG_CLIENT_SUMMARY = 12, // C2S: {missionFlag, wanted, paused, cutscene}
    MSG_SESSION_STATE  = 13, // S2C: {playerCount, hostId, saveVersion}
    MSG_KICK           = 14, // S2C: {reason}
    MSG_EVENT_EXPLOSION= 15, // C2S/S2C: {pos, radius, type, ownerPed}
    MSG_PLAYER_MODEL   = 16, // C2S/S2C: {clientId, modelHash} (ped skin)

    // UDP state channel
    MSG_MOVE           = 32, // C2S/S2C: client movement snapshot
    MSG_PING           = 33, // C2S: {nonce}
    MSG_PONG           = 34, // S2C: {nonce, serverTick}
    MSG_VEHICLE        = 35, // C2S/S2C: vehicle state (driver is authority)
    MSG_NPC            = 36, // C2S/S2C: shared NPC state (owner is authority)
    MSG_WORLD          = 37, // C2S(host)->S2C relay: clock+weather+wanted
};

// ---- Payload structs -------------------------------------------------------

struct Hello {
    u32 protoVer = kProtocolVersion;
    u32 clientVersion = 1;
    std::string name;

    void Write(Writer& w) const {
        w.U32(protoVer);
        w.U32(clientVersion);
        w.Str8(name);
    }
    bool Read(Reader& r) {
        return r.U32(protoVer) && r.U32(clientVersion) && r.Str8(name);
    }
};

struct Welcome {
    u32 clientId = 0;
    u32 hostId = 0;           // clientId of current host (first joiner)
    u64 sessionToken = 0;     // reconnection claim ticket
    u64 saveVersion = 0;
    std::vector<u8> saveBlob;

    void Write(Writer& w) const {
        w.U32(clientId);
        w.U32(hostId);
        w.U64(sessionToken);
        w.U64(saveVersion);
        w.U32(u32(saveBlob.size()));
        w.Raw(saveBlob.data(), saveBlob.size());
    }
    bool Read(Reader& r) {
        u32 len = 0;
        if (!r.U32(clientId) || !r.U32(hostId) || !r.U64(sessionToken) ||
            !r.U64(saveVersion) || !r.U32(len))
            return false;
        if (len > 8 * 1024 * 1024) return false;
        saveBlob.resize(len);
        return r.Raw(saveBlob.data(), len);
    }
};

struct JoinReject {
    u8 reason = 0;
    std::string text;

    void Write(Writer& w) const { w.U8(reason); w.Str8(text); }
    bool Read(Reader& r) { return r.U8(reason) && r.Str8(text); }
};

struct PlayerInfo {
    u32 clientId = 0;
    std::string name;
    u64 modelHash = 0;        // current ped skin (0 = protagonist default)

    void Write(Writer& w) const { w.U32(clientId); w.Str8(name); w.U64(modelHash); }
    bool Read(Reader& r) { return r.U32(clientId) && r.Str8(name) && r.U64(modelHash); }
};

struct PlayerLeft {
    u32 clientId = 0;
    u8 reason = 0;

    void Write(Writer& w) const { w.U32(clientId); w.U8(reason); }
    bool Read(Reader& r) { return r.U32(clientId) && r.U8(reason); }
};

// Ped skin selection (client menu). The server stamps clientId from the
// sending connection before relaying, so skins cannot be spoofed.
struct PlayerModel {
    u32 clientId = 0;
    u64 modelHash = 0;       // 0 = protagonist default

    void Write(Writer& w) const { w.U32(clientId); w.U64(modelHash); }
    bool Read(Reader& r) { return r.U32(clientId) && r.U64(modelHash); }
};

struct Chat {
    u32 clientId = 0;
    std::string text;

    void Write(Writer& w) const { w.U32(clientId); w.Str8(text); }
    bool Read(Reader& r) { return r.U32(clientId) && r.Str8(text); }
};

struct MissionStateMsg {
    u64 missionHash = 0;   // stable id: hash of mission/script name
    u8 state = 0;          // MissionState
    u8 checkpoint = 0;
    u8 slot = 255;         // protagonist slot owning the mission (PlayerSlot)

    void Write(Writer& w) const { w.U64(missionHash); w.U8(state); w.U8(checkpoint); w.U8(slot); }
    bool Read(Reader& r) { return r.U64(missionHash) && r.U8(state) && r.U8(checkpoint) && r.U8(slot); }
};

struct SaveSync {
    u64 version = 0;
    std::vector<u8> blob;

    void Write(Writer& w) const {
        w.U64(version);
        w.U32(u32(blob.size()));
        w.Raw(blob.data(), blob.size());
    }
    bool Read(Reader& r) {
        u32 len = 0;
        if (!r.U64(version) || !r.U32(len)) return false;
        if (len > 8 * 1024 * 1024) return false;
        blob.resize(len);
        return r.Raw(blob.data(), len);
    }
};

struct SpawnRequest {
    u8 entityType = 0;      // 0=ped 1=vehicle
    u64 modelHash = 0;
    u32 requestTag = 0;     // caller-side correlation id
    Vec3 pos;
    f32 yaw = 0;

    void Write(Writer& w) const {
        w.U8(entityType); w.U64(modelHash); w.U32(requestTag);
        QuantizedPos p = PackPos(pos);
        w.U32(u32(p.x)); w.U32(u32(p.y)); w.U32(u32(p.z));
        w.U16(PackYaw(yaw));
    }
    bool Read(Reader& r) {
        u32 x, y, z; u16 yaw;
        if (!r.U8(entityType) || !r.U64(modelHash) || !r.U32(requestTag) ||
            !r.U32(x) || !r.U32(y) || !r.U32(z) || !r.U16(yaw))
            return false;
        pos = UnpackPos({ (i32)x, (i32)y, (i32)z });
        yaw = UnpackYaw(yaw);
        return true;
    }
};

struct SpawnMsg {
    u64 guid = 0;
    u8 entityType = 0;
    u64 modelHash = 0;
    u32 requestTag = 0;
    Vec3 pos;
    f32 yaw = 0;

    void Write(Writer& w) const {
        w.U64(guid); w.U8(entityType); w.U64(modelHash); w.U32(requestTag);
        QuantizedPos p = PackPos(pos);
        w.U32(u32(p.x)); w.U32(u32(p.y)); w.U32(u32(p.z));
        w.U16(PackYaw(yaw));
    }
    bool Read(Reader& r) {
        u32 x, y, z; u16 yaw;
        if (!r.U64(guid) || !r.U8(entityType) || !r.U64(modelHash) || !r.U32(requestTag) ||
            !r.U32(x) || !r.U32(y) || !r.U32(z) || !r.U16(yaw))
            return false;
        pos = UnpackPos({ (i32)x, (i32)y, (i32)z });
        yaw = UnpackYaw(yaw);
        return true;
    }
};

struct Despawn {
    u64 guid = 0;
    u8 reason = 0;

    void Write(Writer& w) const { w.U64(guid); w.U8(reason); }
    bool Read(Reader& r) { return r.U64(guid) && r.U8(reason); }
};

struct ClientSummary {
    u8 missionFlag = 0;   // GET_MISSION_FLAG
    u8 wanted = 0;
    u8 paused = 0;        // pause menu open
    u8 cutscene = 0;      // IS_CUTSCENE_ACTIVE
    u8 slot = 255;        // current protagonist

    void Write(Writer& w) const { w.U8(missionFlag); w.U8(wanted); w.U8(paused); w.U8(cutscene); w.U8(slot); }
    bool Read(Reader& r) { return r.U8(missionFlag) && r.U8(wanted) && r.U8(paused) && r.U8(cutscene) && r.U8(slot); }
};

struct SessionState {
    u32 playerCount = 0;
    u32 hostId = 0;
    u64 saveVersion = 0;

    void Write(Writer& w) const { w.U32(playerCount); w.U32(hostId); w.U64(saveVersion); }
    bool Read(Reader& r) { return r.U32(playerCount) && r.U32(hostId) && r.U64(saveVersion); }
};

struct ExplosionEvent {
    Vec3 pos;
    f32 radius = 0;
    u8 type = 0;
    u64 ownerGuid = 0;

    void Write(Writer& w) const {
        QuantizedPos p = PackPos(pos);
        w.U32(u32(p.x)); w.U32(u32(p.y)); w.U32(u32(p.z));
        w.F32(radius); w.U8(type); w.U64(ownerGuid);
    }
    bool Read(Reader& r) {
        u32 x, y, z;
        if (!r.U32(x) || !r.U32(y) || !r.U32(z) || !r.F32(radius) || !r.U8(type) || !r.U64(ownerGuid))
            return false;
        pos = UnpackPos({ (i32)x, (i32)y, (i32)z });
        return true;
    }
};

// Movement snapshot (UDP). ~40 bytes.
struct Move {
    u32 clientId = 0;
    u32 tick = 0;
    Vec3 pos;
    f32 yaw = 0;
    Vec3 vel;
    u16 health = 100;
    u8 armour = 0;
    u8 slot = 255;          // protagonist in use
    u8 inVehicle = 0;
    u64 vehicleGuid = 0;
    u8 seat = 255;
    u64 weaponHash = 0;
    u16 ammo = 0;
    u8 dead = 0;

    void Write(Writer& w) const {
        w.U32(clientId); w.U32(tick);
        QuantizedPos p = PackPos(pos);
        w.U32(u32(p.x)); w.U32(u32(p.y)); w.U32(u32(p.z));
        w.U16(PackYaw(yaw));
        QuantizedVel v = PackVel(vel);
        w.U16(u16(v.x)); w.U16(u16(v.y)); w.U16(u16(v.z));
        w.U16(health); w.U8(armour); w.U8(slot); w.U8(inVehicle);
        w.U64(vehicleGuid); w.U8(seat); w.U64(weaponHash); w.U16(ammo); w.U8(dead);
    }
    bool Read(Reader& r) {
        u32 x, y, z; u16 yaw; u16 vx, vy, vz;
        if (!r.U32(clientId) || !r.U32(tick) ||
            !r.U32(x) || !r.U32(y) || !r.U32(z) || !r.U16(yaw) ||
            !r.U16(vx) || !r.U16(vy) || !r.U16(vz) ||
            !r.U16(health) || !r.U8(armour) || !r.U8(slot) || !r.U8(inVehicle) ||
            !r.U64(vehicleGuid) || !r.U8(seat) || !r.U64(weaponHash) || !r.U16(ammo) || !r.U8(dead))
            return false;
        pos = UnpackPos({ (i32)x, (i32)y, (i32)z });
        yaw = UnpackYaw(yaw);
        vel = UnpackVel({ (i16)vx, (i16)vy, (i16)vz });
        return true;
    }
};

// Vehicle state (UDP). Driver is physics authority. ~55 bytes.
struct VehicleState {
    u64 guid = 0;
    u32 ownerId = 0;
    Vec3 pos;
    f32 yaw = 0;
    f32 pitch = 0;
    f32 roll = 0;
    Vec3 vel;
    f32 engineHealth = 1000;
    f32 bodyHealth = 1000;
    u8 flags = 0;         // bit0 lightsOn bit1 sirenOn
    u8 tyreMask = 0;      // bit i => wheel i burst
    u64 driverGuid = 0;   // guid of driver player (0 = empty)

    void Write(Writer& w) const {
        w.U64(guid); w.U32(ownerId);
        QuantizedPos p = PackPos(pos);
        w.U32(u32(p.x)); w.U32(u32(p.y)); w.U32(u32(p.z));
        w.U16(PackYaw(yaw));
        w.F32(pitch); w.F32(roll);
        QuantizedVel v = PackVel(vel);
        w.U16(u16(v.x)); w.U16(u16(v.y)); w.U16(u16(v.z));
        w.F32(engineHealth); w.F32(bodyHealth);
        w.U8(flags); w.U8(tyreMask); w.U64(driverGuid);
    }
    bool Read(Reader& r) {
        u32 x, y, z; u16 yaw; u16 vx, vy, vz;
        if (!r.U64(guid) || !r.U32(ownerId) ||
            !r.U32(x) || !r.U32(y) || !r.U32(z) || !r.U16(yaw) ||
            !r.F32(pitch) || !r.F32(roll) ||
            !r.U16(vx) || !r.U16(vy) || !r.U16(vz) ||
            !r.F32(engineHealth) || !r.F32(bodyHealth) ||
            !r.U8(flags) || !r.U8(tyreMask) || !r.U64(driverGuid))
            return false;
        pos = UnpackPos({ (i32)x, (i32)y, (i32)z });
        yaw = UnpackYaw(yaw);
        vel = UnpackVel({ (i16)vx, (i16)vy, (i16)vz });
        return true;
    }
};

// Shared NPC state (UDP). ~25 bytes.
struct NpcState {
    u64 guid = 0;
    u32 ownerId = 0;
    Vec3 pos;
    f32 yaw = 0;
    u16 health = 100;
    u8 dead = 0;
    u8 npcType = 0;    // 0=ambient 1=police 2=missionActor

    void Write(Writer& w) const {
        w.U64(guid); w.U32(ownerId);
        QuantizedPos p = PackPos(pos);
        w.U32(u32(p.x)); w.U32(u32(p.y)); w.U32(u32(p.z));
        w.U16(PackYaw(yaw));
        w.U16(health); w.U8(dead); w.U8(npcType);
    }
    bool Read(Reader& r) {
        u32 x, y, z; u16 yaw;
        if (!r.U64(guid) || !r.U32(ownerId) ||
            !r.U32(x) || !r.U32(y) || !r.U32(z) || !r.U16(yaw) ||
            !r.U16(health) || !r.U8(dead) || !r.U8(npcType))
            return false;
        pos = UnpackPos({ (i32)x, (i32)y, (i32)z });
        yaw = UnpackYaw(yaw);
        return true;
    }
};

// World state (UDP). Host -> everyone.
struct WorldState {
    u32 tick = 0;
    u8 clockH = 0, clockM = 0, clockS = 0;
    u8 weatherSel = 0;   // index into fixed weather table
    u8 wanted = 0;
    u8 paused = 0;

    void Write(Writer& w) const {
        w.U32(tick); w.U8(clockH); w.U8(clockM); w.U8(clockS);
        w.U8(weatherSel); w.U8(wanted); w.U8(paused);
    }
    bool Read(Reader& r) {
        return r.U32(tick) && r.U8(clockH) && r.U8(clockM) && r.U8(clockS) &&
               r.U8(weatherSel) && r.U8(wanted) && r.U8(paused);
    }
};

// ---- Framing ---------------------------------------------------------------

// TCP frame: [u8 protoVer][u8 msgId][u16 payloadLen][payload]
// UDP datagram: identical header, payload follows.
constexpr u8 kFrameProtoVer = 1;
constexpr size_t kFrameHeaderSize = 4;
constexpr size_t kMaxPayload = 8 * 1024 * 1024;

void WriteFrameHeader(std::vector<u8>& out, MsgId id, u16 payloadLen);

// Incremental TCP frame parser. Feed bytes; when a full frame is available,
// returns true and sets id/payload. Call again for the next frame.
class FrameParser {
public:
    bool Feed(const u8* data, size_t len, MsgId& outId, std::vector<u8>& outPayload);
    void Reset() { buf_.clear(); }

private:
    std::vector<u8> buf_;
};

} // namespace proto
} // namespace coop
