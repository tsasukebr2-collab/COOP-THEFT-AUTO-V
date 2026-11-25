// CRC32 (IEEE 802.3, standard zlib polynomial). Used for save integrity and
// UDP packet sanity checks. No external dependencies.
#pragma once

#include "Types.h"

namespace coop {

class Crc32 {
public:
    Crc32() { InitTable(); }

    void Reset() { value_ = 0xffffffffu; }

    void Update(const void* data, size_t len) {
        const u8* p = static_cast<const u8*>(data);
        while (len--) {
            value_ = (value_ >> 8) ^ table_[(value_ ^ *p++) & 0xff];
        }
    }

    u32 Finalize() const { return value_ ^ 0xffffffffu; }

    static u32 Compute(const void* data, size_t len) {
        Crc32 c;
        c.Reset();
        c.Update(data, len);
        return c.Finalize();
    }

private:
    void InitTable() {
        for (u32 i = 0; i < 256; ++i) {
            u32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
            table_[i] = c;
        }
        Reset();
    }

    u32 table_[256] = {};
    u32 value_ = 0xffffffffu;
};

} // namespace coop
