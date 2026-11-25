#include "ClientNet.h"
#include "../../shared/coop/Log.h"
#include "../../shared/coop/Crc32.h"

#include <chrono>
#include <cstring>

namespace coop {

namespace {
u64 NowMs() {
    return (u64)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
constexpr size_t kUdpBufSize = 2048;
} // namespace

ClientNet::~ClientNet() { Stop(); }

bool ClientNet::Start(const std::string& serverIp, u16 tcpPort, u16 udpPort, const std::string& playerName) {
    Stop();
    parser_.Reset();
    serverIp_ = serverIp;
    tcpPort_ = tcpPort;
    udpPort_ = udpPort;
    playerName_ = playerName.substr(0, 31);

    if (!udp_.Open(0)) { // ephemeral local port
        Log::Error("UDP socket open failed");
        return false;
    }

    // Non-blocking connect; state flips to CONNECTED by FinishConnect.
    if (!tcp_.Connect(serverIp_, tcpPort_)) {
        Log::Error("TCP connect failed to %s:%u", serverIp_.c_str(), tcpPort_);
        udp_.Close();
        return false;
    }

    state_ = NetState::CONNECTING;
    running_ = true;
    lastRecvMs_ = NowMs();
    thread_ = std::thread(&ClientNet::NetworkThreadMain, this);
    return true;
}

void ClientNet::Stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    tcp_.Close();
    udp_.Close();
    state_ = NetState::DISCONNECTED;
}

void ClientNet::SendTcp(proto::MsgId id, const std::vector<u8>& payload) {
    std::lock_guard<std::mutex> lk(outMutex_);
    outTcp_.push_back({id, payload});
}

void ClientNet::SendUdp(proto::MsgId id, const std::vector<u8>& payload) {
    if (state_ != NetState::IN_SESSION) return;
    std::vector<u8> frame;
    frame.reserve(payload.size() + 14);
    frame.push_back(proto::kFrameProtoVer);
    frame.push_back(u8(id));
    u32 cid = clientId_;
    u64 tok = sessionToken_;
    frame.insert(frame.end(), (u8*)&cid, (u8*)&cid + 4);
    frame.insert(frame.end(), (u8*)&tok, (u8*)&tok + 8);
    frame.insert(frame.end(), payload.begin(), payload.end());
    udp_.SendTo(frame.data(), frame.size(), serverIp_, udpPort_);
}

void ClientNet::BuildFrame(std::vector<u8>& out, proto::MsgId id, const std::vector<u8>& payload) {
    out.clear();
    proto::WriteFrameHeader(out, id, u16(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
}

bool ClientNet::PollTcp(IncomingMsg& out) {
    std::lock_guard<std::mutex> lk(inMutex_);
    if (inTcp_.empty()) return false;
    out = inTcp_.front();
    inTcp_.pop_front();
    return true;
}

bool ClientNet::PollUdp(IncomingMsg& out) {
    std::lock_guard<std::mutex> lk(inMutex_);
    if (inUdp_.empty()) return false;
    out = inUdp_.front();
    inUdp_.pop_front();
    return true;
}

void ClientNet::UpdateLatency(u64 nonce, u32 serverTick) {
    (void)serverTick;
    // Ping RTT = now - send time (send time encoded in nonce for simplicity).
    u64 sentAt = nonce & 0xffffffffull;
    u32 rtt = u32(NowMs() - sentAt);
    pingMs_ = rtt;
}

void ClientNet::SendPing() {
    u64 nonce = (NowMs() & 0xffffffffull) | (u64(0xC00C) << 32);
    std::vector<u8> frame;
    frame.reserve(22);
    frame.push_back(proto::kFrameProtoVer);
    frame.push_back(proto::MSG_PING);
    u32 cid = clientId_;
    u64 tok = sessionToken_;
    frame.insert(frame.end(), (u8*)&cid, (u8*)&cid + 4);
    frame.insert(frame.end(), (u8*)&tok, (u8*)&tok + 8);
    frame.insert(frame.end(), (u8*)&nonce, (u8*)&nonce + 8);
    udp_.SendTo(frame.data(), frame.size(), serverIp_, udpPort_);
    lastPingMs_ = NowMs();
}

void ClientNet::ProcessUdp() {
    u8 buf[kUdpBufSize];
    std::string fromIp;
    u16 fromPort;
    for (;;) {
        int got = udp_.RecvFrom(buf, sizeof(buf), fromIp, fromPort);
        if (got <= 0) break;
        if (got < 14 || buf[0] != proto::kFrameProtoVer) continue;
        if (fromIp != serverIp_) continue; // ignore spoofed sources

        lastRecvMs_ = NowMs();
        u8 id = buf[1];
        const u8* body = buf + 14;
        size_t bodyLen = size_t(got) - 14;

        if (id == proto::MSG_PONG) {
            u64 nonce;
            std::memcpy(&nonce, body, sizeof(nonce));
            UpdateLatency(nonce, 0);
            continue;
        }

        IncomingMsg msg;
        msg.id = proto::MsgId(id);
        msg.payload.assign(body, body + bodyLen);
        std::lock_guard<std::mutex> lk(inMutex_);
        inUdp_.push_back(std::move(msg));
    }
}

void ClientNet::NetworkThreadMain() {
    NetInit netInit;
    u64 lastPing = NowMs();

    while (running_) {
        u64 now = NowMs();

        // TCP connect completion (CONNECTING state).
        if (state_ == NetState::CONNECTING) {
            if (tcp_.FinishConnect()) {
                state_ = NetState::CONNECTED;
                Log::Info("TCP connected, sending HELLO");
                proto::Hello hello;
                hello.name = playerName_;
                Writer w;
                hello.Write(w);
                std::vector<u8> frame;
                BuildFrame(frame, proto::MSG_HELLO, w.Data());
                tcp_.Send(frame.data(), frame.size());
            } else if (now - lastRecvMs_ > 10000) {
                Log::Error("TCP connect timeout");
                state_ = NetState::DISCONNECTED;
                running_ = false;
                break;
            }
        }

        // Outbound TCP queue.
        if (state_ == NetState::CONNECTED || state_ == NetState::IN_SESSION) {
            std::deque<IncomingMsg> local;
            {
                std::lock_guard<std::mutex> lk(outMutex_);
                local.swap(outTcp_);
            }
            for (auto& m : local) {
                std::vector<u8> frame;
                BuildFrame(frame, m.id, m.payload);
                tcp_.Send(frame.data(), frame.size());
            }
        }

        // Inbound TCP.
        u8 chunk[8192];
        for (;;) {
            int got = tcp_.Recv(chunk, sizeof(chunk));
            if (got < 0) {
                if (state_ == NetState::IN_SESSION) {
                    Log::Warn("TCP lost - session dropped, save layer keeps local state");
                }
                state_ = NetState::DISCONNECTED;
                break;
            }
            if (got == 0) break;
            lastRecvMs_ = NowMs();

            proto::MsgId id;
            std::vector<u8> payload;
            bool parsed = parser_.Feed(chunk, size_t(got), id, payload);
            while (parsed) {
                if (id == proto::MSG_WELCOME) {
                    Reader r(payload.data(), payload.size());
                    proto::Welcome welcome;
                    if (welcome.Read(r)) {
                        clientId_ = welcome.clientId;
                        sessionToken_ = welcome.sessionToken;
                        saveVersion_ = welcome.saveVersion;
                        {
                            std::lock_guard<std::mutex> lk(inMutex_);
                            saveBlob_ = welcome.saveBlob;
                        }
                        isHost_ = (welcome.hostId == welcome.clientId);
                        state_ = NetState::IN_SESSION;
                        Log::Info("WELCOME: clientId=%u host=%u save v%llu",
                                  welcome.clientId, welcome.hostId,
                                  (unsigned long long)welcome.saveVersion);
                    }
                } else {
                    IncomingMsg msg;
                    msg.id = id;
                    msg.payload = payload;
                    std::lock_guard<std::mutex> lk(inMutex_);
                    inTcp_.push_back(std::move(msg));
                }
                parsed = parser_.Feed(nullptr, 0, id, payload);
            }
            if (state_ == NetState::DISCONNECTED) break;
        }

        // UDP: server pongs + relays.
        ProcessUdp();

        // Ping loop (only meaningful once UDP endpoint is known server-side;
        // the first pings establish it).
        if (state_ == NetState::IN_SESSION && now - lastPing > 500) {
            SendPing();
            lastPing = now;
        }

        if (state_ == NetState::DISCONNECTED && !running_) break;
        Sleep(10);
    }
}

} // namespace coop
