// Client network engine.
// Threads: one network thread (blocking select loop) + the game script
// thread. They exchange messages through mutex-protected queues; all game
// natives are called ONLY from the script thread.
#pragma once

#include "../../shared/coop/Types.h"
#include "../../shared/coop/net/Net.h"
#include "../../protocol/coop/protocol/Messages.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace coop {

enum class NetState : u8 {
    DISCONNECTED = 0,
    CONNECTING,
    CONNECTED,   // TCP established, waiting WELCOME
    IN_SESSION,  // WELCOME received, gameplay active
};

struct IncomingMsg {
    proto::MsgId id = proto::MSG_HELLO;
    std::vector<u8> payload;
};

class ClientNet {
public:
    ~ClientNet();

    bool Start(const std::string& serverIp, u16 tcpPort, u16 udpPort, const std::string& playerName);
    void Stop();
    void UpdateLatency(u64 nonce, u32 serverTick);

    // ---- Script-thread API (natives must not be touched here) --------------
    NetState State() const { return state_; }
    u32 ClientId() const { return clientId_; }
    u64 SessionToken() const { return sessionToken_; }
    u32 PingMs() const { return pingMs_; }
    u64 SaveVersion() const { return saveVersion_; }
    const std::vector<u8>& SaveBlob() const { return saveBlob_; }
    bool IsHost() const { return isHost_; }
    void SetHost(bool host) { isHost_ = host; }

    bool PollTcp(IncomingMsg& out);   // one message per call
    bool PollUdp(IncomingMsg& out);

    void SendTcp(proto::MsgId id, const std::vector<u8>& payload);
    void SendUdp(proto::MsgId id, const std::vector<u8>& payload);

private:
    void NetworkThreadMain();
    void ProcessUdp();
    void SendPing();
    void BuildFrame(std::vector<u8>& out, proto::MsgId id, const std::vector<u8>& payload);

    std::thread thread_;
    std::atomic<bool> running_{false};

    std::string serverIp_;
    u16 tcpPort_ = kDefaultTcpPort;
    u16 udpPort_ = kDefaultUdpPort;
    std::string playerName_;

    TcpConn tcp_;
    UdpSocket udp_;
    proto::FrameParser parser_;

    std::atomic<NetState> state_{NetState::DISCONNECTED};
    std::atomic<u32> clientId_{0};
    std::atomic<u64> sessionToken_{0};
    std::atomic<u32> pingMs_{0};
    std::atomic<u64> saveVersion_{0};
    std::atomic<bool> isHost_{false};

    // Written once on WELCOME (net thread), read on script thread after
    // the state becomes IN_SESSION - guarded by the same mutex as the queues.
    std::vector<u8> saveBlob_;

    std::mutex inMutex_;
    std::deque<IncomingMsg> inTcp_;
    std::deque<IncomingMsg> inUdp_;

    std::mutex outMutex_;
    std::deque<IncomingMsg> outTcp_;

    u32 udpNonce_ = 1;
    u64 lastPingMs_ = 0;
    u64 lastRecvMs_ = 0;
};

} // namespace coop
