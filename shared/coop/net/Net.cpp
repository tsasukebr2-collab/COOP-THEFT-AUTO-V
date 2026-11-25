#include "Net.h"
#include "../Log.h"

#include <cstring>

namespace coop {

NetInit::NetInit() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        Log::Error("WSAStartup failed");
#endif
}

NetInit::~NetInit() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// --- TcpListener ------------------------------------------------------------

bool TcpListener::Start(u16 port) {
    fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd_ == INVALID_SOCKET) return false;

    BOOL reuse = TRUE;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(fd_, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(fd_);
        fd_ = INVALID_SOCKET;
        return false;
    }
    if (listen(fd_, 8) != 0) {
        closesocket(fd_);
        fd_ = INVALID_SOCKET;
        return false;
    }

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(fd_, FIONBIO, &mode);
#else
    int flags = fcntl(fd_, F_GETFL, 0);
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
#endif
    return true;
}

void TcpListener::Stop() {
    if (fd_ != INVALID_SOCKET) { closesocket(fd_); fd_ = INVALID_SOCKET; }
}

SOCKET TcpListener::Accept() {
    if (fd_ == INVALID_SOCKET) return INVALID_SOCKET;
    sockaddr_in peer{};
#ifdef _WIN32
    int plen = sizeof(peer);
#else
    socklen_t plen = sizeof(peer);
#endif
    SOCKET s = accept(fd_, (sockaddr*)&peer, &plen);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    BOOL nodelay = TRUE;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
    return s;
}

// --- TcpConn ----------------------------------------------------------------

bool TcpConn::SetNonBlocking() {
    if (fd_ == INVALID_SOCKET) return false;
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd_, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd_, F_GETFL, 0);
    return fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool TcpConn::Connect(const std::string& ip, u16 port) {
    Close();
    fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd_ == INVALID_SOCKET) return false;

    BOOL nodelay = TRUE;
    setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
    SetNonBlocking();

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        Close();
        return false;
    }
    if (connect(fd_, (sockaddr*)&addr, sizeof(addr)) != 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) { Close(); return false; }
#else
        if (errno != EINPROGRESS) { Close(); return false; }
#endif
    } else {
        connected_ = true;
    }
    return true;
}

bool TcpConn::FinishConnect() {
    if (connected_) return true;
    if (fd_ == INVALID_SOCKET) return false;

    fd_set wset, eset;
    FD_ZERO(&wset); FD_ZERO(&eset);
    FD_SET(fd_, &wset);
    FD_SET(fd_, &eset);
    timeval tv{0, 0};
    int ret = select(int(fd_) + 1, nullptr, &wset, &eset, &tv);
    if (ret <= 0) return false;
#ifdef _WIN32
    if (FD_ISSET(fd_, &eset)) { Close(); return false; }
    if (FD_ISSET(fd_, &wset)) { connected_ = true; return true; }
#else
    int soerr = 0;
    socklen_t slen = sizeof(soerr);
    getsockopt(fd_, SOL_SOCKET, SO_ERROR, (char*)&soerr, &slen);
    if (soerr != 0) { Close(); return false; }
    connected_ = true;
    return true;
#endif
    return false;
}

bool TcpConn::Send(const void* data, size_t len) {
    if (fd_ == INVALID_SOCKET || !connected_) return false;
    outBuf_.insert(outBuf_.end(), (const u8*)data, (const u8*)data + len);

    while (!outBuf_.empty()) {
#ifdef _WIN32
        int sent = send(fd_, (const char*)outBuf_.data(), int(outBuf_.size()), 0);
#else
        ssize_t sent = ::send(fd_, (const char*)outBuf_.data(), outBuf_.size(), MSG_NOSIGNAL);
#endif
        if (sent == SOCKET_ERROR) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) return true; // retry next tick
            if (err == WSAECONNRESET || err == WSAENOTCONN) { Close(); }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
            Close();
#endif
            return false;
        }
        outBuf_.erase(outBuf_.begin(), outBuf_.begin() + sent);
    }
    return true;
}

int TcpConn::Recv(void* data, size_t len) {
    if (fd_ == INVALID_SOCKET || !connected_) return 0;
#ifdef _WIN32
    int got = recv(fd_, (char*)data, int(len), 0);
#else
    ssize_t got = ::recv(fd_, (char*)data, len, 0);
#endif
    if (got == SOCKET_ERROR) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return 0;
        Close();
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        Close();
#endif
        return -1;
    }
    if (got == 0) { Close(); return -1; }
    return int(got);
}

void TcpConn::Close() {
    if (fd_ != INVALID_SOCKET) {
        closesocket(fd_);
        fd_ = INVALID_SOCKET;
    }
    connected_ = false;
    outBuf_.clear();
}

// --- UdpSocket --------------------------------------------------------------

bool UdpSocket::Open(u16 bindPort) {
    Close();
    fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd_ == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(bindPort);
    if (bind(fd_, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(fd_);
        fd_ = INVALID_SOCKET;
        return false;
    }
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(fd_, FIONBIO, &mode);
#else
    int flags = fcntl(fd_, F_GETFL, 0);
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
#endif
    return true;
}

void UdpSocket::Close() {
    if (fd_ != INVALID_SOCKET) { closesocket(fd_); fd_ = INVALID_SOCKET; }
}

bool UdpSocket::SendTo(const void* data, size_t len, const std::string& ip, u16 port) {
    if (fd_ == INVALID_SOCKET) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) return false;
#ifdef _WIN32
    int sent = sendto(fd_, (const char*)data, int(len), 0, (sockaddr*)&addr, sizeof(addr));
#else
    ssize_t sent = sendto(fd_, (const char*)data, len, 0, (sockaddr*)&addr, sizeof(addr));
#endif
    return sent != SOCKET_ERROR;
}

int UdpSocket::RecvFrom(void* data, size_t len, std::string& fromIp, u16& fromPort) {
    if (fd_ == INVALID_SOCKET) return 0;
    sockaddr_in addr{};
#ifdef _WIN32
    int alen = sizeof(addr);
    int got = recvfrom(fd_, (char*)data, int(len), 0, (sockaddr*)&addr, &alen);
#else
    socklen_t alen = sizeof(addr);
    ssize_t got = recvfrom(fd_, (char*)data, len, 0, (sockaddr*)&addr, &alen);
#endif
    if (got == SOCKET_ERROR) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK || err == WSAECONNRESET) return 0;
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
#endif
        return -1;
    }
    char ipbuf[64];
    inet_ntop(AF_INET, &addr.sin_addr, ipbuf, sizeof(ipbuf));
    fromIp = ipbuf;
    fromPort = ntohs(addr.sin_port);
    return int(got);
}

} // namespace coop
