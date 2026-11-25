#include "SaveStore.h"

#include "../Crc32.h"
#include "../Log.h"

#include <cstdio>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace coop {

namespace {
constexpr u32 kMagic = 0x504F4356; // little-endian 'PCOP' -> 'COOP' bytes on disk

void PutU32(std::vector<u8>& v, u32 x) {
    v.push_back(u8(x)); v.push_back(u8(x >> 8));
    v.push_back(u8(x >> 16)); v.push_back(u8(x >> 24));
}
void PutU16(std::vector<u8>& v, u16 x) { v.push_back(u8(x)); v.push_back(u8(x >> 8)); }
u32 GetU32(const u8* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (u32(p[3]) << 24); }
u16 GetU16(const u8* p) { return u16(p[0] | (p[1] << 8)); }

bool ReadWholeFile(const std::string& path, std::vector<u8>& out) {
    FILE* f = nullptr;
#ifdef _WIN32
    if (fopen_s(&f, path.c_str(), "rb") != 0) return false;
#else
    f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
#endif
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 32 * 1024 * 1024) { std::fclose(f); return false; }
    out.resize(size_t(sz));
    size_t rd = std::fread(out.data(), 1, size_t(sz), f);
    std::fclose(f);
    return rd == size_t(sz);
}
} // namespace

const SaveSection* SaveData::Find(u32 type) const {
    for (const auto& s : sections)
        if (s.type == type) return &s;
    return nullptr;
}

void SaveData::SetSection(u32 type, const void* data, size_t len) {
    for (auto& s : sections) {
        if (s.type == type) {
            s.payload.assign((const u8*)data, (const u8*)data + len);
            return;
        }
    }
    sections.push_back({type, std::vector<u8>((const u8*)data, (const u8*)data + len)});
}

std::vector<u8> SaveStore::Serialize(const SaveData& data) {
    std::vector<u8> out;
    out.reserve(1024 + data.sections.size() * 256);

    PutU32(out, kMagic);
    PutU32(out, data.formatVersion);
    PutU16(out, u16(data.sections.size()));
    PutU16(out, 0); // reserved

    std::vector<u8> body;
    for (const auto& s : data.sections) {
        PutU32(body, s.type);
        PutU32(body, u32(s.payload.size()));
        PutU32(body, Crc32::Compute(s.payload.data(), s.payload.size()));
        body.insert(body.end(), s.payload.begin(), s.payload.end());
    }

    PutU32(out, u32(out.size() + body.size() + 4)); // totalLen incl. headerCrc field
    PutU32(out, Crc32::Compute(out.data(), out.size())); // header CRC over header bytes
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

bool SaveStore::Parse(const std::vector<u8>& raw, SaveData& out, std::string& error) {
    if (raw.size() < 20) { error = "too small"; return false; }
    const u8* p = raw.data();

    if (GetU32(p) != kMagic) { error = "bad magic"; return false; }
    out.formatVersion = GetU32(p + 4);
    if (out.formatVersion != 1) { error = "unsupported formatVersion"; return false; }
    u16 sectionCount = GetU16(p + 8);
    u32 totalLen = GetU32(p + 12);
    u32 headerCrc = GetU32(p + 16);

    if (totalLen != raw.size()) { error = "totalLen mismatch"; return false; }

    Crc32 hc;
    hc.Update(p, 16);
    if (hc.Finalize() != headerCrc) { error = "header CRC mismatch"; return false; }

    size_t off = 20;
    out.sections.clear();
    for (u16 i = 0; i < sectionCount; ++i) {
        if (off + 12 > raw.size()) { error = "truncated section header"; return false; }
        u32 type = GetU32(p + off);
        u32 len = GetU32(p + off + 4);
        u32 crc = GetU32(p + off + 8);
        off += 12;
        if (off + len > raw.size()) { error = "truncated section payload"; return false; }
        if (Crc32::Compute(p + off, len) != crc) { error = "section CRC mismatch"; return false; }
        SaveSection s;
        s.type = type;
        s.payload.assign(p + off, p + off + len);
        out.sections.push_back(std::move(s));
        off += len;
    }
    return true;
}

SaveStore::SaveStore(std::string basePath) : basePath_(std::move(basePath)) {}

std::string SaveStore::TempPath(const std::string& basePath) {
    return basePath + ".tmp";
}

std::vector<std::string> SaveStore::BackupPaths(const std::string& basePath, int count) {
    std::vector<std::string> paths;
    paths.reserve(count);
    for (int i = 0; i < count; ++i)
        paths.push_back(basePath + ".bak" + std::to_string(i));
    return paths;
}

SaveWriteResult SaveStore::Write(const SaveData& data, u64 newVersion) {
    SaveWriteResult result;
    result.version = newVersion;

    // 1) Guard against version regression: refuse to overwrite a newer save.
    SaveData current;
    std::string err;
    std::vector<u8> raw;
    if (ReadWholeFile(basePath_, raw) && Parse(raw, current, err)) {
        // The stored version lives in SEC_META (first 8 bytes of payload).
        if (const SaveSection* meta = current.Find(SEC_META)) {
            if (meta->payload.size() >= 8) {
                u64 storedVersion;
                std::memcpy(&storedVersion, meta->payload.data(), sizeof(storedVersion));
                if (storedVersion >= newVersion) {
                    result.error = "refusing to overwrite newer save (stored " +
                                   std::to_string(storedVersion) + " >= new " +
                                   std::to_string(newVersion) + ")";
                    return result;
                }
            }
        }
    }

    std::vector<u8> bytes = Serialize(data);
    const std::string tmp = TempPath(basePath_);

    // 2) Atomic transaction: write temp, flush, rename over the target.
    FILE* f = nullptr;
#ifdef _WIN32
    if (fopen_s(&f, tmp.c_str(), "wb") != 0) f = nullptr;
#else
    f = std::fopen(tmp.c_str(), "wb");
#endif
    if (!f) { result.error = "cannot open temp file"; return result; }
    size_t written = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fflush(f);
    std::fclose(f);
    if (written != bytes.size()) { result.error = "short write to temp file"; return result; }

#ifdef _WIN32
    // Rotate backups: newest is bak0.
    std::vector<std::string> backups = BackupPaths(basePath_);
    std::string prev = basePath_;
    for (size_t i = 0; i < backups.size(); ++i) {
        // Copy (not rename) so a mid-write crash still leaves every bak intact.
        CopyFileA(prev.c_str(), backups[i].c_str(), FALSE);
        prev = backups[i];
    }
    if (!MoveFileExA(tmp.c_str(), basePath_.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        result.error = "rename failed (Win32 error " + std::to_string(GetLastError()) + ")";
        return result;
    }
#else
    // POSIX path (future dedicated-server port).
    std::vector<std::string> backups = BackupPaths(basePath_);
    std::string prev = basePath_;
    for (size_t i = 0; i < backups.size(); ++i) {
        std::remove(backups[i].c_str());
        std::rename(prev.c_str(), backups[i].c_str()); // best effort
        prev = backups[i];
    }
    if (std::rename(tmp.c_str(), basePath_.c_str()) != 0) {
        result.error = "rename failed";
        return result;
    }
#endif

    result.ok = true;
    Log::Info("SaveStore: wrote %s (v%llu, %u bytes)", basePath_.c_str(),
              (unsigned long long)newVersion, (unsigned)bytes.size());
    return result;
}

SaveLoadResult SaveStore::Load(SaveData& out) const {
    SaveLoadResult result;
    std::vector<u8> raw;
    std::string err;

    if (ReadWholeFile(basePath_, raw) && Parse(raw, out, err)) {
        result.ok = true;
        result.source = "file";
        if (const SaveSection* meta = out.Find(SEC_META))
            if (meta->payload.size() >= 8)
                std::memcpy(&result.version, meta->payload.data(), sizeof(result.version));
        return result;
    }
    result.error = "base: " + err;

    for (int i = 0; i < kBackupCount; ++i) {
        std::string path = basePath_ + ".bak" + std::to_string(i);
        if (ReadWholeFile(path, raw) && Parse(raw, out, err)) {
            result.ok = true;
            result.source = "backup:" + std::to_string(i);
            if (const SaveSection* meta = out.Find(SEC_META))
                if (meta->payload.size() >= 8)
                    std::memcpy(&result.version, meta->payload.data(), sizeof(result.version));
            Log::Warn("SaveStore: base save damaged, recovered from %s", path.c_str());
            return result;
        }
    }
    result.error += "; all backups unusable";
    return result;
}

} // namespace coop
