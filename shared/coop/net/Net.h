// Cross-platform (Windows-first) socket helpers: TCP + UDP, non-blocking.
// The dedicated server runs a single select() loop over all connections.
#pragma once

#include "../Types.h"

#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

namespace coop {

class NetInit {
public:
    NetInit();   // WSAStartup on Windows
    ~NetInit();  // WSACleanup
    NetInit(const NetInit&) = delete;
    NetInit& operator=(const NetInit&) = delete;
};

class TcpListener {
public:
    bool Start(u16 port);
    void Stop();
    // Returns fd of accepted socket or INVALID_SOCKET (non-blocking accept).
    SOCKET Accept();
    SOCKET Fd() const { return fd_; }

private:
    SOCKET fd_ = INVALID_SOCKET;
};

class TcpConn {
public:
    TcpConn() = default;
    explicit TcpConn(SOCKET fd) : fd_(fd) {}
    ~TcpConn() { Close(); }

    bool SetNonBlocking();
    bool Connect(const std::string& ip, u16 port); // non-blocking connect
    bool IsConnected() const { return fd_ != INVALID_SOCKET && connected_; }

    // Returns true when connection is established (for non-blocking connect).
    bool FinishConnect();

    // Send raw bytes; buffers internally. Returns false on fatal error.
    bool Send(const void* data, size_t len);
    // Recv into buffer, returns bytes read (0 = none, -1 = closed).
    int Recv(void* data, size_t len);

    void Close();
    SOCKET Fd() const { return fd_; }

private:
    SOCKET fd_ = INVALID_SOCKET;
    bool connected_ = false;
    std::vector<u8> outBuf_;
};

class UdpSocket {
public:
    bool Open(u16 bindPort);
    void Close();
    bool SendTo(const void* data, size_t len, const std::string& ip, u16 port);
    // Returns bytes read, 0 = nothing, -1 = error.
    int RecvFrom(void* data, size_t len, std::string& fromIp, u16& fromPort);
    SOCKET Fd() const { return fd_; }

private:
    SOCKET fd_ = INVALID_SOCKET;
};

} // namespace coop
