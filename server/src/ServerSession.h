// Dedicated co-op server: session management + relay + save hosting.
// The server never runs game logic: it owns session state and the shared
// save; gameplay authority stays with the host client (slot 0).
#pragma once

#include "../../shared/coop/Types.h"
#include "../../shared/coop/net/Net.h"
#include "../../shared/coop/save/SaveStore.h"
#include "../../protocol/coop/protocol/Messages.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace coop {

struct ServerClient {
    u32 clientId = 0;
    std::string name;
    u64 sessionToken = 0;      // issued at WELCOME, required for reconnect
    u64 modelHash = 0;         // last announced ped skin (stamped into PLAYER_JOINED)
    std::unique_ptr<TcpConn> tcp;
    proto::FrameParser parser;
    std::string udpIp;
    u16 udpPort = 0;           // learned from first UDP ping
    u64 lastSeenMs = 0;        // last TCP/UDP activity
    u64 pingMs = 0;
    bool udpBound = false;
};

struct ServerConfig {
    u16 tcpPort = kDefaultTcpPort;
    u16 udpPort = kDefaultUdpPort;
    u32 maxPlayers = 4;
    // Seamless group identity: each group has its own shared save. When
    // savePath is empty it resolves to saves/<groupId>.coop.
    std::string groupId = "default";
    std::string savePath;                 // empty = saves/<groupId>.coop
    std::string password;                 // empty = open
    u64 autosaveIntervalMs = 120000;      // 2 min
    u32 clientTimeoutMs = 15000;
    u32 logLevel = 1;                     // 0 debug 1 info 2 warn 3 error
};

class CoopServer {
public:
    bool Start(const ServerConfig& cfg);
    void Run();          // blocks until Stop() or fatal error
    void Stop();

    u32 PlayerCount() const { return u32(clients_.size()); }

private:
    void TickTcp(u64 nowMs);
    void TickUdp(u64 nowMs);
    void TickHousekeeping(u64 nowMs);

    void AcceptNewClients(u64 nowMs);
    void HandleClientData(ServerClient& c);
    void HandleFrame(ServerClient& c, proto::MsgId id, const std::vector<u8>& payload);
    void DropClient(u32 clientId, u8 reason, const char* why);

    void BroadcastTcp(proto::MsgId id, const std::vector<u8>& payload, u32 exceptClientId = 0xffffffffu);
    void SendToClient(u32 clientId, proto::MsgId id, const std::vector<u8>& payload);
    void RelayUdp(const std::vector<u8>& datagram, u32 exceptClientId);
    std::vector<u8> FrameMessage(proto::MsgId id, const std::vector<u8>& payload) const;

    void HandleHello(ServerClient& c, const std::vector<u8>& payload);
    void HandleSaveUpload(ServerClient& c, const std::vector<u8>& payload);
    void HandleMissionState(ServerClient& c, const std::vector<u8>& payload);

    void HandlePlayerModel(ServerClient& c, const std::vector<u8>& payload);

    ServerConfig cfg_;
    NetInit netInit_;
    TcpListener listener_;
    UdpSocket udp_;
    std::map<u32, ServerClient> clients_;
    u32 nextClientId_ = 0;

    SessionId sessionId_;
    u64 sessionTokenSeed_ = 0;
    SaveStore saveStore_;
    SaveData save_;
    u64 saveVersion_ = 0;
    u32 hostClientId_ = 0;   // first joiner; rebased if it leaves
    u64 lastAutosaveMs_ = 0;
    bool running_ = false;
    u64 startMs_ = 0;
};

} // namespace coop
