// Little-endian byte buffer with hard bounds checking.
// All network deserialization goes through Reader: a malformed or hostile
// packet can never cause an out-of-bounds read, it just fails cleanly.
#pragma once

#include "Types.h"

#include <vector>
#include <string>

namespace coop {

class Writer {
public:
    explicit Writer(size_t reserve = 512) { buf_.reserve(reserve); }

    void U8(u8 v)  { buf_.push_back(v); }
    void U16(u16 v){ buf_.push_back(u8(v)); buf_.push_back(u8(v >> 8)); }
    void U32(u32 v){ U16(u16(v)); U16(u16(v >> 16)); }
    void U64(u64 v){ U32(u32(v)); U32(u32(v >> 32)); }
    void I64(i64 v){ U64(u64(v)); }
    void F32(f32 v){ u32 b; std::memcpy(&b, &v, 4); U32(b); }

    void Raw(const void* p, size_t len) {
        const u8* b = (const u8*)p;
        buf_.insert(buf_.end(), b, b + len);
    }
    // u8-length-prefixed string, truncated to fit 255 bytes.
    void Str8(const std::string& s) {
        u8 n = u8(s.size() < 255 ? s.size() : 255);
        U8(n);
        Raw(s.data(), n);
    }

    const std::vector<u8>& Data() const { return buf_; }
    std::vector<u8>& Data() { return buf_; }
    size_t Size() const { return buf_.size(); }

private:
    std::vector<u8> buf_;
};

class Reader {
public:
    Reader(const u8* data, size_t len) : data_(data), len_(len) {}
    explicit Reader(const std::vector<u8>& v) : data_(v.data()), len_(v.size()) {}

    bool U8(u8& v)  { if (off_ + 1 > len_) return false; v = data_[off_++]; return true; }
    bool U16(u16& v){ u8 a,b; if (!U8(a)||!U8(b)) return false; v=u16(a|(u16(b)<<8)); return true; }
    bool U32(u32& v){ u16 a,b; if (!U16(a)||!U16(b)) return false; v=u32(a|(u32(b)<<16)); return true; }
    bool U64(u64& v){ u32 a,b; if (!U32(a)||!U32(b)) return false; v=u64(a)|(u64(b)<<32); return true; }
    bool I64(i64& v){ u64 t; if (!U64(t)) return false; std::memcpy(&v,&t,8); return true; }
    bool F32(f32& v){ u32 b; if (!U32(b)) return false; std::memcpy(&v,&b,4); return true; }

    bool Raw(void* p, size_t len) {
        if (off_ + len > len_) return false;
        std::memcpy(p, data_ + off_, len);
        off_ += len;
        return true;
    }
    bool Str8(std::string& s) {
        u8 n;
        if (!U8(n)) return false;
        if (off_ + n > len_) return false;
        s.assign((const char*)data_ + off_, n);
        off_ += n;
        return true;
    }

    size_t Remaining() const { return len_ - off_; }
    size_t Offset() const { return off_; }

private:
    const u8* data_;
    size_t len_;
    size_t off_ = 0;
};

} // namespace coop
