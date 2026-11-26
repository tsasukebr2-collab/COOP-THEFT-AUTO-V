#include "CoopContext.h"
#include "natives/Natives.h"
#include "../../shared/coop/Log.h"
#include "../../shared/coop/KeyValueConfig.h"

#include <chrono>
#include <cstring>

namespace coop {

namespace {
u64 NowMs() {
    return (u64)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
constexpr u32 kHostIdNone = 0xffffffffu;
} // namespace

CoopContext& CoopContext::Instance() {
    static CoopContext ctx;
    return ctx;
}

void CoopContext::LoadConfig() {
    KeyValueConfig cfg;
    if (cfg.Load("coop_client.ini")) {
        config_.serverIp = cfg.Get("serverIp", config_.serverIp);
        config_.tcpPort = u16(cfg.GetInt("tcpPort", config_.tcpPort));
        config_.udpPort = u16(cfg.GetInt("udpPort", config_.udpPort));
        config_.playerName = cfg.Get("playerName", config_.playerName);
        config_.autosaveMs = u64(cfg.GetInt("autosaveMs", int(config_.autosaveMs)));
        config_.moveRateHz = u32(cfg.GetInt("moveRateHz", config_.moveRateHz));
        config_.autoConnect = cfg.GetBool("autoConnect", config_.autoConnect);
    }
    save_.Init("coop_local.coop");
    configDirty_ = false;
}

void CoopContext::SaveConfig() {
    KeyValueConfig cfg;
    cfg.Set("serverIp", config_.serverIp);
    cfg.Set("tcpPort", std::to_string(config_.tcpPort));
    cfg.Set("udpPort", std::to_string(config_.udpPort));
    cfg.Set("playerName", config_.playerName);
    cfg.Set("autosaveMs", std::to_string(config_.autosaveMs));
    cfg.Set("moveRateHz", std::to_string(config_.moveRateHz));
    cfg.Set("autoConnect", config_.autoConnect ? "1" : "0");
    cfg.Save("coop_client.ini");
}

bool CoopContext::StartSession(const std::string& ip, u16 tcpPort, const std::string& name) {
    if (InSession()) StopSession();
    bool ok = net_.Start(ip, tcpPort, config_.udpPort, name);
    if (ok) Log::Info("session start: %s:%u as %s", ip.c_str(), tcpPort, name.c_str());
    return ok;
}

void CoopContext::StopSession() {
    net_.Stop();
    players_.Local().clientId = 0;
    // Remove remote representations.
    for (RemotePlayer* rp : players_.AllRemotes())
        players_.RemoveRemote(rp->ClientId());
    saveAdopted_ = false;
    Log::Info("session stopped");
}

bool CoopContext::InSession() const {
    return net_.State() == NetState::IN_SESSION;
}

void CoopContext::PumpTcp() {
    IncomingMsg msg;
    while (net_.PollTcp(msg)) {
        Reader r(msg.payload.data(), msg.payload.size());
        switch (msg.id) {
            case proto::MSG_PLAYER_JOINED: {
                proto::PlayerInfo info;
                if (info.Read(r)) {
                    players_.AddRemote(info.clientId, info.name, info.modelHash);
                    Log::Info("player joined: %s (%u)", info.name.c_str(), info.clientId);
                }
                break;
            }
            case proto::MSG_PLAYER_LEFT: {
                proto::PlayerLeft left;
                if (left.Read(r)) {
                    players_.RemoveRemote(left.clientId);
                    Log::Info("player left: %u", left.clientId);
                }
                break;
            }
            case proto::MSG_CHAT: {
                proto::Chat chat;
                if (chat.Read(r)) menu_.PushChat(chat.clientId, chat.text);
                break;
            }
            case proto::MSG_MISSION_STATE: {
                proto::MissionStateMsg m;
                if (m.Read(r)) missions_.ApplyNetwork(m, NowMs());
                break;
            }
            case proto::MSG_SAVE_SYNC: {
                proto::SaveSync sync;
                if (sync.Read(r)) save_.ApplyIncoming(sync);
                break;
            }
            case proto::MSG_SESSION_STATE: {
                proto::SessionState st;
                if (st.Read(r)) net_.SetHost(st.hostId == net_.ClientId());
                break;
            }
            case proto::MSG_SPAWN: {
                proto::SpawnMsg sp;
                if (sp.Read(r) && sp.entityType == 1) {
                    vehicles_.EnsureSpawned(sp.guid, ScrHash(sp.modelHash), sp.pos, sp.yaw);
                }
                break;
            }
            case proto::MSG_DESPAWN: {
                proto::Despawn d;
                if (d.Read(r)) vehicles_.Forget(d.guid);
                break;
            }
            case proto::MSG_EVENT_EXPLOSION: {
                proto::ExplosionEvent ex;
                if (ex.Read(r))
                    ADD_EXPLOSION(ex.pos.x, ex.pos.y, ex.pos.z, ex.type,
                                  ex.radius, 1, 0, 1.0f, 0);
                break;
            }
            case proto::MSG_PLAYER_MODEL: {
                proto::PlayerModel pm;
                if (pm.Read(r) && pm.clientId != net_.ClientId())
                    players_.SetRemoteModel(pm.clientId, pm.modelHash);
                break;
            }
            default: break;
        }
    }
}

void CoopContext::PumpUdp() {
    IncomingMsg msg;
    u64 now = NowMs();
    while (net_.PollUdp(msg)) {
        Reader r(msg.payload.data(), msg.payload.size());
        switch (msg.id) {
            case proto::MSG_MOVE: {
                proto::Move m;
                if (m.Read(r)) players_.ApplyMove(m, now);
                break;
            }
            case proto::MSG_VEHICLE: {
                proto::VehicleState st;
                if (st.Read(r)) vehicles_.ApplyState(st, now);
                break;
            }
            case proto::MSG_WORLD: {
                proto::WorldState w;
                if (w.Read(r) && !net_.IsHost()) world_.Apply(w, now);
                break;
            }
            default: break;
        }
    }
}

void CoopContext::CheckVehicleEnterExit() {
    ScrPed ped = PLAYER_PED_ID();
    bool inVeh = IS_PED_IN_ANY_VEHICLE(ped, 0) != 0;
    if (inVeh != (lastInVehicle_ != 0)) {
        lastInVehicle_ = inVeh ? 1 : 0;
        if (inVeh) {
            ScrVehicle v = GET_VEHICLE_PED_IS_IN(ped, 0);
            if (v != 0) {
                // Adopt ownership of this vehicle for sync purposes.
                u64 guid = MakeGuid(net_.ClientId(), u32(v));
                vehicles_.SetOwned(guid, true);
                lastVehicleGuid_ = guid;
            }
        } else {
            if (lastVehicleGuid_) {
                vehicles_.SetOwned(lastVehicleGuid_, false);
                lastVehicleGuid_ = 0;
            }
        }
        lastSeat_ = inVeh ? 0 : 255; // driver assumption refined by seat check
    }
}

void CoopContext::BroadcastMovement(u64 nowMs) {
    u32 interval = 1000 / (config_.moveRateHz ? config_.moveRateHz : 20);
    if (nowMs - lastMoveSentMs_ < interval) return;
    lastMoveSentMs_ = nowMs;

    u8 seat = 255;
    u64 vehGuid = lastVehicleGuid_;
    if (lastInVehicle_ && vehGuid) seat = lastSeat_;

    proto::Move m = players_.CaptureLocalMove(++tick_, vehGuid, seat);
    Writer w;
    m.Write(w);
    net_.SendUdp(proto::MSG_MOVE, w.Data());
}

void CoopContext::BroadcastOwnedVehicles(u64 nowMs) {
    if (nowMs - lastVehicleSentMs_ < 50) return; // 20 Hz
    lastVehicleSentMs_ = nowMs;

    for (auto& kv : vehicles_.All()) {
        if (!vehicles_.IsOwned(kv.first)) continue;
        if (kv.second.vehicle == 0 || !DOES_ENTITY_EXIST(kv.second.vehicle)) continue;
        proto::VehicleState st = vehicles_.Capture(kv.second.vehicle, kv.first, net_.ClientId());
        Writer w;
        st.Write(w);
        net_.SendUdp(proto::MSG_VEHICLE, w.Data());
    }
}

void CoopContext::BroadcastWorldIfHost(u64 nowMs) {
    if (!net_.IsHost()) return;
    if (nowMs - lastWorldSentMs_ < 500) return; // 2 Hz
    lastWorldSentMs_ = nowMs;

    proto::WorldState w = world_.Capture(tick_);
    Writer wr;
    w.Write(wr);
    net_.SendUdp(proto::MSG_WORLD, wr.Data());
}

void CoopContext::PushSaveNow(const char* reason) {
    save_.PullFromGame();

    // Push our snapshot to the server; the server version-guards it.
    // Only meaningful for the host in v1 (server rejects non-host saves);
    // non-host clients still refresh their local mirror.
    u64 nextVersion = save_.Version() + 1;
    proto::SaveSync sync;
    sync.version = nextVersion;
    sync.blob = save_.Serialize(nextVersion);
    Writer w;
    sync.Write(w);
    net_.SendTcp(proto::MSG_SAVE_SYNC, w.Data());
    save_.MarkSaved(NowMs());
    Log::Info("save pushed (%s, v%llu)", reason, (unsigned long long)nextVersion);
}

void CoopContext::PushSaveIfDue(u64 nowMs) {
    if (!save_.ShouldAutosave(nowMs, config_.autosaveMs)) return;
    PushSaveNow("autosave");
}

void CoopContext::Tick() {
    tickStartMs_ = NowMs();

    // Skin streaming runs whether or not a session is active (offline skin
    // changes are allowed); the broadcast only happens when connected.
    skin_.Tick(tickStartMs_);
    u64 appliedHash = 0;
    if (skin_.ConsumeApplied(appliedHash)) {
        // Model swaps reset loadouts: restore saved weapons.
        save_.ApplyToGame();
        if (InSession()) {
            proto::PlayerModel pm;
            pm.clientId = net_.ClientId();
            pm.modelHash = appliedHash;
            Writer w;
            pm.Write(w);
            net_.SendTcp(proto::MSG_PLAYER_MODEL, w.Data());
        }
    }

    // First WELCOME handling: adopt save + identify.
    if (net_.State() == NetState::IN_SESSION) {
        if (!saveAdopted_ && net_.SaveVersion() > 0) {
            players_.Local().clientId = net_.ClientId();
            players_.Local().isHost = net_.IsHost();
            u64 serverVerBefore = net_.SaveVersion();
            save_.AdoptServerBlob(net_.SaveBlob(), net_.SaveVersion());
            missions_.SetHost(net_.IsHost());
            world_.SetHost(net_.IsHost());
            saveAdopted_ = true;
            // Seamless crash recovery: our local mirror is NEWER than the
            // server's (server lost its save / fresh restart). Keep ours and
            // push immediately so the whole group continues from this state.
            if (save_.Version() > serverVerBefore) {
                PushSaveNow("crash recovery");
            }
        }

        PumpTcp();
        PumpUdp();

        players_.Tick(tickStartMs_);
        vehicles_.Tick(tickStartMs_);

        MissionState missionBefore = missions_.Current();
        missions_.Tick(tickStartMs_);
        // Seamless: the shared save follows story progress automatically.
        // When the host's mission ends, push the shared save right away.
        if (net_.IsHost() && missions_.Current() != missionBefore &&
            (missions_.Current() == MissionState::COMPLETED ||
             missions_.Current() == MissionState::FAILED)) {
            PushSaveNow("mission end");
        }
        world_.Tick(tickStartMs_);

        CheckVehicleEnterExit();
        BroadcastMovement(tickStartMs_);
        BroadcastOwnedVehicles(tickStartMs_);
        BroadcastWorldIfHost(tickStartMs_);
        PushSaveIfDue(tickStartMs_);
    }

    menu_.Tick(*this, tickStartMs_);
}

} // namespace coop
