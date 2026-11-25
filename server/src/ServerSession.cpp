#include "ServerSession.h"
#include "../../shared/coop/Log.h"
#include "../../shared/coop/Crc32.h"

#include <algorithm>
#include <cstring>
#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace coop {

namespace {
u64 NowMs() {
    return (u64)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

bool CoopServer::Start(const ServerConfig& cfg) {
    cfg_ = cfg;
    Log::Init("coop_server.log", LogLevel(cfg_.logLevel));

    if (!listener_.Start(cfg_.tcpPort)) {
        Log::Error("TCP bind failed on port %u", cfg_.tcpPort);
        return false;
    }
    if (!udp_.Open(cfg_.udpPort)) {
        Log::Error("UDP bind failed on port %u", cfg_.udpPort);
        return false;
    }

    sessionId_ = SessionId::Generate();
    sessionTokenSeed_ = SessionId::Generate().lo;

    // Seamless group identity: one shared save per group. An explicitly
    // configured savePath always wins; otherwise it derives from groupId.
    if (cfg_.savePath.empty())
        cfg_.savePath = std::string("saves/") + cfg_.groupId + ".coop";

    // Ensure the save directory exists (fresh install scenario).
    {
        size_t slash = cfg_.savePath.find_last_of("/\\");
        if (slash != std::string::npos) {
            std::string dir = cfg_.savePath.substr(0, slash);
#ifdef _WIN32
            CreateDirectoryA(dir.c_str(), nullptr); // succeeds silently if exists
#else
            mkdir(dir.c_str(), 0755);
#endif
        }
    }

    saveStore_ = SaveStore(cfg_.savePath);

    // Load existing shared save if present (resume scenario).
    SaveLoadResult load = saveStore_.Load(save_);
    if (load.ok) {
        saveVersion_ = load.version;
        Log::Info("Loaded shared save v%llu from %s",
                  (unsigned long long)load.version, load.source.c_str());
    } else {
        // Fresh session: create META so versioning starts at 1.
        SaveData fresh;
        u8 meta[8];
        u64 v1 = 1;
        std::memcpy(meta, &v1, sizeof(v1));
        fresh.SetSection(SEC_META, meta, sizeof(meta));
        saveVersion_ = 1;
        saveStore_.Write(fresh, saveVersion_);
        Log::Info("New session %s: fresh save v1", sessionId_.ToString().c_str());
    }

    running_ = true;
    startMs_ = NowMs();
    Log::Info("Server up: tcp=%u udp=%u maxPlayers=%u session=%s",
              cfg_.tcpPort, cfg_.udpPort, cfg_.maxPlayers,
              sessionId_.ToString().c_str());
    return true;
}

void CoopServer::Stop() { running_ = false; }

std::vector<u8> CoopServer::FrameMessage(proto::MsgId id, const std::vector<u8>& payload) const {
    std::vector<u8> frame;
    frame.reserve(payload.size() + 4);
    proto::WriteFrameHeader(frame, id, u16(payload.size()));
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

void CoopServer::BroadcastTcp(proto::MsgId id, const std::vector<u8>& payload, u32 except) {
    std::vector<u8> frame = FrameMessage(id, payload);
    for (auto& kv : clients_) {
        if (kv.first == except) continue;
        if (kv.second.tcp) kv.second.tcp->Send(frame.data(), frame.size());
    }
}

void CoopServer::SendToClient(u32 clientId, proto::MsgId id, const std::vector<u8>& payload) {
    auto it = clients_.find(clientId);
    if (it == clients_.end() || !it->second.tcp) return;
    std::vector<u8> frame = FrameMessage(id, payload);
    it->second.tcp->Send(frame.data(), frame.size());
}

void CoopServer::RelayUdp(const std::vector<u8>& datagram, u32 except) {
    for (auto& kv : clients_) {
        if (kv.first == except || !kv.second.udpBound) continue;
        udp_.SendTo(datagram.data(), datagram.size(), kv.second.udpIp, kv.second.udpPort);
    }
}

void CoopServer::AcceptNewClients(u64 nowMs) {
    for (;;) {
        SOCKET s = listener_.Accept();
        if (s == INVALID_SOCKET) break;
        if (clients_.size() >= cfg_.maxPlayers) {
            // Politely reject then close.
            TcpConn rej(s);
            rej.SetNonBlocking();
            proto::JoinReject r;
            r.reason = 1;
            r.text = "server full";
            Writer w;
            r.Write(w);
            std::vector<u8> f = FrameMessage(MSG_JOIN_REJECT, w.Data());
            rej.Send(f.data(), f.size());
            continue;
        }
        ServerClient c;
        c.clientId = ++nextClientId_;
        c.sessionToken = sessionTokenSeed_ ^ (u64(s) * 0x9e3779b97f4a7c15ull) ^ nowMs;
        c.lastSeenMs = nowMs;
        c.tcp = std::make_unique<TcpConn>(s);
        c.tcp->SetNonBlocking();
        clients_[c.clientId] = std::move(c);
        Log::Info("conn: fd=%d assigned clientId=%u", int(s), c.clientId);
    }
}

void CoopServer::HandleHello(ServerClient& c, const std::vector<u8>& payload) {
    Reader r(payload.data(), payload.size());
    proto::Hello hello;
    if (!hello.Read(r)) { DropClient(c.clientId, 1, "bad HELLO"); return; }

    if (hello.protoVer != kProtocolVersion) {
        proto::JoinReject rej;
        rej.reason = 2;
        rej.text = "protocol version mismatch";
        Writer w;
        rej.Write(w);
        std::vector<u8> f = FrameMessage(MSG_JOIN_REJECT, w.Data());
        c.tcp->Send(f.data(), f.size());
        DropClient(c.clientId, 2, "version mismatch");
        return;
    }

    c.name = hello.name.substr(0, 31);

    // First client to complete the handshake becomes host (slot 0 authority).
    if (hostClientId_ == 0) hostClientId_ = c.clientId;

    // WELCOME carries the shared save so the joiner resumes campaign state.
    proto::Welcome welcome;
    welcome.clientId = c.clientId;
    welcome.hostId = hostClientId_;
    welcome.sessionToken = c.sessionToken;
    welcome.saveVersion = saveVersion_;
    welcome.saveBlob = SaveStore::Serialize(save_);
    Writer w;
    welcome.Write(w);

    std::vector<u8> wf = FrameMessage(MSG_WELCOME, w.Data());
    c.tcp->Send(wf.data(), wf.size());

    proto::PlayerInfo info;
    info.clientId = c.clientId;
    info.name = c.name;
    info.modelHash = c.modelHash; // skin known for late joiners
    Writer iw;
    info.Write(iw);
    BroadcastTcp(MSG_PLAYER_JOINED, iw.Data(), c.clientId);

    Log::Info("join: clientId=%u name=%s (players=%u)", c.clientId, c.name.c_str(),
              u32(clients_.size()));
}

void CoopServer::HandleSaveUpload(ServerClient& c, const std::vector<u8>& payload) {
    // Only the host may push save updates.
    if (c.clientId != hostClientId_) {
        Log::Warn("save upload denied from non-host clientId=%u", c.clientId);
        return;
    }
    Reader r(payload.data(), payload.size());
    proto::SaveSync sync;
    if (!sync.Read(r)) return;

    SaveData incoming;
    std::string err;
    if (!SaveStore::Parse(sync.blob, incoming, err)) {
        Log::Error("host save upload invalid: %s", err.c_str());
        return;
    }
    if (sync.version <= saveVersion_) {
        Log::Warn("host save upload stale (v%llu <= v%llu), ignored",
                  (unsigned long long)sync.version, (unsigned long long)saveVersion_);
        return;
    }

    save_ = incoming;
    saveVersion_ = sync.version;
    saveStore_.Write(save_, saveVersion_);
    Log::Info("shared save updated to v%llu by host", (unsigned long long)saveVersion_);

    // Broadcast to everyone except the host that sent it.
    BroadcastTcp(MSG_SAVE_SYNC, payload, c.clientId);
}

void CoopServer::HandleMissionState(ServerClient& c, const std::vector<u8>& payload) {
    // Mission authority is the host. Non-host mission messages are logged and
    // dropped so a desynced client cannot complete a mission for everyone.
    if (c.clientId != hostClientId_) {
        Reader r(payload.data(), payload.size());
        proto::MissionStateMsg m;
        if (m.Read(r)) {
            Log::Warn("mission state from non-host %s state=%u dropped",
                      c.name.c_str(), m.state);
        }
        return;
    }
    BroadcastTcp(MSG_MISSION_STATE, payload, c.clientId);
}

void CoopServer::HandlePlayerModel(ServerClient& c, const std::vector<u8>& payload) {
    // Skin selection: the server owns clientId (anti-spoof) and remembers the
    // choice so players who join later receive it in PLAYER_JOINED.
    Reader r(payload.data(), payload.size());
    proto::PlayerModel m;
    if (!m.Read(r)) return;
    c.modelHash = m.modelHash;

    proto::PlayerModel stamped;
    stamped.clientId = c.clientId;
    stamped.modelHash = m.modelHash;
    Writer w;
    stamped.Write(w);
    BroadcastTcp(MSG_PLAYER_MODEL, w.Data(), c.clientId);
    Log::Info("skin: clientId=%u model=%016llx", c.clientId,
              (unsigned long long)m.modelHash);
}

void CoopServer::HandleFrame(ServerClient& c, proto::MsgId id, const std::vector<u8>& payload) {
    c.lastSeenMs = NowMs();
    switch (id) {
        case MSG_HELLO:          HandleHello(c, payload); break;
        case MSG_CHAT:           BroadcastTcp(MSG_CHAT, payload, c.clientId); break;
        case MSG_MISSION_STATE:  HandleMissionState(c, payload); break;
        case MSG_SAVE_SYNC:      HandleSaveUpload(c, payload); break;
        case MSG_SPAWN_REQUEST:  // spawns authorized by host only
            if (c.clientId == hostClientId_) BroadcastTcp(MSG_SPAWN, payload, 0xffffffffu);
            break;
        case MSG_SPAWN:
            if (c.clientId == hostClientId_) BroadcastTcp(MSG_SPAWN, payload, c.clientId);
            break;
        case MSG_DESPAWN:        BroadcastTcp(MSG_DESPAWN, payload, c.clientId); break;
        case MSG_CLIENT_SUMMARY: BroadcastTcp(MSG_CLIENT_SUMMARY, payload, c.clientId); break;
        case MSG_EVENT_EXPLOSION:BroadcastTcp(MSG_EVENT_EXPLOSION, payload, c.clientId); break;
        case MSG_PLAYER_MODEL:   HandlePlayerModel(c, payload); break;
        default: break; // TCP-only protocol: UDP messages arriving on TCP are dropped
    }
}

void CoopServer::HandleClientData(ServerClient& c) {
    u8 chunk[4096];
    for (;;) {
        int got = c.tcp->Recv(chunk, sizeof(chunk));
        if (got < 0) { DropClient(c.clientId, 3, "tcp closed"); return; }
        if (got == 0) return;

        proto::MsgId id;
        std::vector<u8> payload;
        bool parsed = c.parser.Feed(chunk, size_t(got), id, payload);
        while (parsed) {
            HandleFrame(c, id, payload);
            if (!c.tcp->IsConnected()) return;
            parsed = c.parser.Feed(nullptr, 0, id, payload); // drain buffered frames
        }
    }
}

void CoopServer::DropClient(u32 clientId, u8 reason, const char* why) {
    auto it = clients_.find(clientId);
    if (it == clients_.end()) return;
    Log::Info("drop: clientId=%u name=%s reason=%u (%s)",
              clientId, it->second.name.c_str(), reason, why);

    proto::PlayerLeft left;
    left.clientId = clientId;
    left.reason = reason;
    Writer w;
    left.Write(w);
    BroadcastTcp(MSG_PLAYER_LEFT, w.Data(), clientId);

    clients_.erase(it);

    // Host migration: first remaining client becomes host.
    if (clientId == hostClientId_) {
        hostClientId_ = clients_.empty() ? 0 : clients_.begin()->first;
        if (!clients_.empty()) {
            proto::SessionState st;
            st.playerCount = u32(clients_.size());
            st.hostId = hostClientId_;
            st.saveVersion = saveVersion_;
            Writer sw;
            st.Write(sw);
            BroadcastTcp(MSG_SESSION_STATE, sw.Data());
            Log::Info("host migrated to clientId=%u", hostClientId_);
        } else {
            Log::Info("no clients left; host slot open");
        }
    }
}

void CoopServer::TickTcp(u64 nowMs) {
    AcceptNewClients(nowMs);

    std::vector<u32> dead;
    for (auto& kv : clients_) {
        ServerClient& c = kv.second;
        if (!c.tcp) { dead.push_back(kv.first); continue; }
        HandleClientData(c);
        if (!c.tcp->IsConnected() && nowMs - c.lastSeenMs > 5000)
            dead.push_back(kv.first);
    }
    for (u32 id : dead) DropClient(id, 3, "timeout");
}

void CoopServer::TickUdp(u64 nowMs) {
    u8 buf[2048];
    std::string fromIp;
    u16 fromPort;
    for (;;) {
        int got = udp_.RecvFrom(buf, sizeof(buf), fromIp, fromPort);
        if (got <= 0) break;

        // [u8 ver][u8 id][u32 clientId][u64 token][body]
        if (got < 14 || buf[0] != proto::kFrameProtoVer) continue;
        u8 id = buf[1];
        u32 clientId;
        u64 token;
        std::memcpy(&clientId, buf + 2, 4);
        std::memcpy(&token, buf + 6, 8);
        auto it = clients_.find(clientId);
        if (it == clients_.end()) continue;
        ServerClient& c = *it;
        if (token != c.sessionToken) {
            Log::Warn("udp token mismatch clientId=%u (spoof or stale token)", clientId);
            continue;
        }
        c.lastSeenMs = nowMs;

        const u8* body = buf + 14;
        size_t bodyLen = size_t(got) - 14;

        if (!c.udpBound) {
            c.udpIp = fromIp;
            c.udpPort = fromPort;
            c.udpBound = true;
            Log::Info("udp bound: clientId=%u %s:%u", clientId, fromIp.c_str(), fromPort);
        } else if (c.udpIp != fromIp || c.udpPort != fromPort) {
            // Endpoint changed (network switch): accept the new one.
            c.udpIp = fromIp;
            c.udpPort = fromPort;
        }

        // PING/PONG latency measurement, everything else relayed.
        if (id == MSG_PING) {
            if (bodyLen < 8) continue;
            u8 pong[30];
            pong[0] = proto::kFrameProtoVer;
            pong[1] = MSG_PONG;
            std::memcpy(pong + 2, &clientId, 4);
            std::memcpy(pong + 6, &token, 8);
            std::memcpy(pong + 14, body, 8);            // nonce passthrough
            u32 serverTick = u32(nowMs - startMs_);
            std::memcpy(pong + 22, &serverTick, 4);
            udp_.SendTo(pong, 26, fromIp, fromPort);
            continue;
        }

        if (got < 14) continue;

        // World state: host authority only.
        if (id == MSG_WORLD && clientId != hostClientId_) continue;

        // Relay verbatim (header already carries sender identity).
        RelayUdp(std::vector<u8>(buf, buf + got), clientId);
    }
}

void CoopServer::TickHousekeeping(u64 nowMs) {
    // Timed autosave: the host pushes SAVE_SYNC; the server version-guards it.
    // Here we only remind clients the interval elapsed.
    if (cfg_.autosaveIntervalMs > 0 && nowMs - lastAutosaveMs_ > cfg_.autosaveIntervalMs) {
        lastAutosaveMs_ = nowMs;
        proto::SessionState st;
        st.playerCount = u32(clients_.size());
        st.hostId = hostClientId_;
        st.saveVersion = saveVersion_;
        Writer w;
        st.Write(w);
        BroadcastTcp(MSG_SESSION_STATE, w.Data());
    }

    // Timeout detection for clients that never bound UDP.
    for (auto& kv : clients_) {
        if (nowMs - kv.second.lastSeenMs > cfg_.clientTimeoutMs) {
            Log::Warn("clientId=%u exceeded timeout", kv.first);
            DropClient(kv.first, 4, "heartbeat timeout");
            break;
        }
    }
}

void CoopServer::Run() {
    while (running_) {
        fd_set readset;
        FD_ZERO(&readset);
        SOCKET maxFd = 0;

        if (listener_.Fd() != INVALID_SOCKET) {
            FD_SET(listener_.Fd(), &readset);
            maxFd = listener_.Fd();
        }
        if (udp_.Fd() != INVALID_SOCKET) {
            FD_SET(udp_.Fd(), &readset);
            maxFd = std::max(maxFd, udp_.Fd());
        }
        for (auto& kv : clients_) {
            if (kv.second.tcp && kv.second.tcp->Fd() != INVALID_SOCKET) {
                FD_SET(kv.second.tcp->Fd(), &readset);
                maxFd = std::max(maxFd, kv.second.tcp->Fd());
            }
        }

        timeval tv{0, 50 * 1000}; // 50 ms
        int ready = select(int(maxFd) + 1, &readset, nullptr, nullptr, &tv);
        u64 nowMs = NowMs();
        if (ready > 0) {
            TickTcp(nowMs);
            TickUdp(nowMs);
        }
        TickHousekeeping(nowMs);
    }
    listener_.Stop();
    udp_.Close();
    Log::Info("server stopped");
}

} // namespace coop
