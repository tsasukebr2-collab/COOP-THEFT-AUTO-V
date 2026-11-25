#include "Messages.h"

namespace coop {
namespace proto {

void WriteFrameHeader(std::vector<u8>& out, MsgId id, u16 payloadLen) {
    out.push_back(kFrameProtoVer);
    out.push_back(u8(id));
    out.push_back(u8(payloadLen & 0xff));
    out.push_back(u8((payloadLen >> 8) & 0xff));
}

bool FrameParser::Feed(const u8* data, size_t len, MsgId& outId, std::vector<u8>& outPayload) {
    if (len > 0 && data != nullptr)
        buf_.insert(buf_.end(), data, data + len);

    if (buf_.size() < kFrameHeaderSize) return false;

    u8 ver = buf_[0];
    u8 id = buf_[1];
    u16 payloadLen = u16(buf_[2] | (u16(buf_[3]) << 8));

    if (ver != kFrameProtoVer) {
        // Protocol desync - drop the buffer; caller treats this as fatal.
        buf_.clear();
        return false;
    }
    if (payloadLen > kMaxPayload) {
        buf_.clear();
        return false;
    }
    if (buf_.size() < kFrameHeaderSize + payloadLen) return false;

    outId = MsgId(id);
    outPayload.assign(buf_.begin() + kFrameHeaderSize,
                      buf_.begin() + kFrameHeaderSize + payloadLen);
    buf_.erase(buf_.begin(), buf_.begin() + kFrameHeaderSize + payloadLen);
    return true;
}

} // namespace proto
} // namespace coop
