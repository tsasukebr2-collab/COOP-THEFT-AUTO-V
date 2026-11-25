// CO-OP shared save store. PRIORITY-1 subsystem.
//
// Design rules (from project spec):
//  - The save is versioned; a monotonic u64 version guards every write.
//    A write that would regress the version is rejected.
//  - Every write is an atomic transaction: write temp file, flush, rename.
//  - Rolling backups (default 5) exist before any destructive operation.
//  - Every section carries a CRC32; on load, a bad section falls back to
//    the most recent good backup.
//  - The game's native savefiles are NEVER touched. This store is a separate
//    layer; the client applies/pulls game state through natives explicitly.
//
// On-disk layout (little-endian):
//   Header : magic 'COOP' u32 | formatVersion u32 | sectionCount u16 |
//            reserved u16 | totalLen u32 (header+sections) | headerCrc u32
//   Section: type u32 | len u32 | crc u32 | payload[len]
#pragma once

#include "Types.h"

#include <string>
#include <vector>
#include <map>

namespace coop {

enum SectionType : u32 {
    SEC_META     = 1, // session id, save version, timestamps, counters
    SEC_PROGRESS = 2, // completed missions bitset + story flags
    SEC_PLAYER   = 3, // per-slot player data (money, weapons)
    SEC_WORLD    = 4, // weather selection, clock offset, collectibles
    SEC_STATS    = 5, // gameplay counters
    SEC_VEHICLES = 6, // acquired vehicles (garage contents)
};

struct SaveSection {
    u32 type = 0;
    std::vector<u8> payload;
};

struct SaveData {
    u32 formatVersion = 1;
    std::vector<SaveSection> sections;

    const SaveSection* Find(u32 type) const;
    void SetSection(u32 type, const void* data, size_t len);
};

struct SaveWriteResult {
    bool ok = false;
    u64 version = 0;    // version written (valid when ok)
    std::string error;  // reason (valid when !ok)
};

struct SaveLoadResult {
    bool ok = false;
    u64 version = 0;
    std::string source;  // "file" or "backup:N"
    std::string error;
};

class SaveStore {
public:
    // Number of rolling backups kept per save file.
    static constexpr int kBackupCount = 5;

    SaveStore() = default;
    explicit SaveStore(std::string basePath);

    // Serialize + validate + atomic-write + rotate backups.
    SaveWriteResult Write(const SaveData& data, u64 newVersion);

    // Load base file; on corruption/absence, walk backups newest-first.
    SaveLoadResult Load(SaveData& out) const;

    // Directory snapshot: absolute paths of existing backups, newest first.
    static std::vector<std::string> BackupPaths(const std::string& basePath, int count = kBackupCount);

    static std::string TempPath(const std::string& basePath);
    static bool Parse(const std::vector<u8>& raw, SaveData& out, std::string& error);
    static std::vector<u8> Serialize(const SaveData& data);

private:
    std::string basePath_;
};

} // namespace coop
