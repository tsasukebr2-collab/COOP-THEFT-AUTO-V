// Save system tests: CRC integrity, version guard, atomic write, backup
// rotation, corruption recovery. Runs on plain files in a temp dir.
#include "TestMain.h"
#include "../shared/coop/save/SaveStore.h"
#include "../shared/coop/Log.h"

#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using namespace coop;

namespace {
void MakeMeta(SaveData& data, u64 version) {
    std::vector<u8> meta(8, 0);
    std::memcpy(meta.data(), &version, sizeof(version));
    data.SetSection(SEC_META, meta.data(), meta.size());
}

u64 StoredVersion(const SaveData& data) {
    const SaveSection* s = data.Find(SEC_META);
    if (!s || s->payload.size() < 8) return 0;
    u64 v;
    std::memcpy(&v, s->payload.data(), sizeof(v));
    return v;
}
} // namespace

static void TestRoundtrip(const std::string& path) {
    SaveStore store(path);

    SaveData out;
    MakeMeta(out, 7);
    u8 progress[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
    out.SetSection(SEC_PROGRESS, progress, sizeof(progress));

    SaveWriteResult w = store.Write(out, 7);
    CHECK(w.ok);

    SaveStore reader(path);
    SaveData in;
    SaveLoadResult r = reader.Load(in);
    CHECK(r.ok);
    CHECK_EQ(r.version, u64(7));
    const SaveSection* s = in.Find(SEC_PROGRESS);
    CHECK(s != nullptr);
    CHECK(s->payload.size() == 4);
    CHECK_EQ(s->payload[2], u8(0xCC));
}

static void TestVersionGuard(const std::string& path) {
    SaveStore store(path);

    SaveData v10;
    MakeMeta(v10, 10);
    CHECK(store.Write(v10, 10).ok);

    // Attempt to write v9 over v10: must be REFUSED.
    SaveData v9;
    MakeMeta(v9, 9);
    SaveWriteResult refused = store.Write(v9, 9);
    CHECK(!refused.ok);

    // File still holds v10.
    SaveData loaded;
    SaveLoadResult r = store.Load(loaded);
    CHECK(r.ok);
    CHECK_EQ(StoredVersion(loaded), u64(10));
}

static void TestCorruptionRecovery(const std::string& path) {
    SaveStore store(path);

    SaveData good;
    MakeMeta(good, 5);
    const char* payload = "important-progress";
    good.SetSection(SEC_PROGRESS, payload, 18);
    CHECK(store.Write(good, 5).ok);

    // Corrupt the base file: flip bytes in the middle.
    FILE* f = std::fopen(path.c_str(), "r+b");
    CHECK(f != nullptr);
    std::fseek(f, 30, SEEK_SET);
    u8 junk[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    std::fwrite(junk, 1, 4, f);
    std::fclose(f);

    // Load must fall back to backup and return the good data.
    SaveStore recovery(path);
    SaveData loaded;
    SaveLoadResult r = recovery.Load(loaded);
    CHECK(r.ok);
    const SaveSection* s = loaded.Find(SEC_PROGRESS);
    CHECK(s != nullptr);
    CHECK(s->payload.size() == 18);
    CHECK(std::memcmp(s->payload.data(), payload, 18) == 0);
}

static void TestRejectsGarbage(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "wb");
    CHECK(f != nullptr);
    std::fwrite("GARBAGE-GARBAGE-GARBAGE", 1, 23, f);
    std::fclose(f);

    SaveStore store(path);
    SaveData loaded;
    SaveLoadResult r = store.Load(loaded);
    CHECK(!r.ok); // no valid backup either -> clean failure, no crash
}

void RunSaveTests() {
    Log::Init("", LogLevel::Error); // quiet during tests

    const char* dir = "test_saves";
#ifdef _WIN32
    CreateDirectoryA(dir, nullptr);
#endif

    std::string base1 = std::string(dir) + "/roundtrip.coop";
    std::remove(base1.c_str());
    TestRoundtrip(base1);

    std::string base2 = std::string(dir) + "/version.coop";
    std::remove(base2.c_str());
    TestVersionGuard(base2);

    std::string base3 = std::string(dir) + "/corrupt.coop";
    std::remove(base3.c_str());
    TestCorruptionRecovery(base3);

    std::string base4 = std::string(dir) + "/garbage.coop";
    TestRejectsGarbage(base4);
}
