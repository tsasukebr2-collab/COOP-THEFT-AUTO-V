// Protocol roundtrip tests: framing, bounds-checking, quantization,
// message encode/decode with exact byte accounting.
#include "TestMain.h"
#include "../protocol/coop/protocol/Messages.h"
#include "../protocol/coop/protocol/Quantize.h"
#include "../shared/coop/Crc32.h"

using namespace coop;
using namespace coop::proto;

static void TestFraming() {
    // Single frame in one feed.
    FrameParser p;
    MsgId id;
    std::vector<u8> payload;
    u8 chunk[] = { 1, 6, 3, 0, 'a', 'b', 'c' };
    CHECK(p.Feed(chunk, sizeof(chunk), id, payload));
    CHECK_EQ(id, MSG_CHAT);
    CHECK_EQ(payload.size(), size_t(3));

    // Byte-at-a-time feed.
    FrameParser q;
    bool done = false;
    for (size_t i = 0; i < sizeof(chunk); ++i)
        done = q.Feed(chunk + i, 1, id, payload);
    CHECK(done);
    CHECK_EQ(id, MSG_CHAT);

    // Two frames + trailing partial header in one buffer.
    u8 two[] = { 1, 6, 1, 0, 'x',       // frame A: CHAT "x"
                 1, 4, 2, 0, 'y', 'z',  // frame B: PLAYER_JOINED "yz"
                 1, 7 };                // partial header (needs 2 more bytes)
    FrameParser s;
    CHECK(s.Feed(two, 5, id, payload));
    CHECK_EQ(id, MSG_CHAT);
    CHECK(s.Feed(two + 5, 6, id, payload));
    CHECK_EQ(id, MSG_PLAYER_JOINED);
    CHECK_EQ(payload.size(), size_t(2));
    CHECK(!s.Feed(two + 11, 2, id, payload)); // incomplete, no frame yet
}

static void TestFramingRejectsGarbage() {
    FrameParser p;
    MsgId id;
    std::vector<u8> payload;
    u8 bad[] = { 9, 9, 0xff, 0xff }; // wrong proto version
    CHECK(!p.Feed(bad, sizeof(bad), id, payload));
    // buffer cleared; next feed starts clean
    CHECK(!p.Feed(nullptr, 0, id, payload));
}

static void TestReaderBounds() {
    u8 data[4] = { 1, 2, 3, 4 };
    Reader r(data, 4);
    u32 v;
    CHECK(r.U32(v));
    CHECK_EQ(v, u32(0x04030201));
    CHECK(!r.U32(v)); // no more bytes
    u8 x;
    CHECK(!r.U8(x));
}

static void TestQuantization() {
    Vec3 p{ 1234.5678f, -9876.5432f, 15.0f };
    QuantizedPos qp = PackPos(p);
    Vec3 back = UnpackPos(qp);
    CHECK_NEAR(back.x, p.x, 0.001);
    CHECK_NEAR(back.y, p.y, 0.001);
    CHECK_NEAR(back.z, p.z, 0.001);

    Vec3 v{ 30.5f, -0.02f, 100.9f };
    QuantizedVel qv = PackVel(v);
    Vec3 vb = UnpackVel(qv);
    CHECK_NEAR(vb.x, v.x, 0.02);
    CHECK_NEAR(vb.y, v.y, 0.02);
    CHECK_NEAR(vb.z, v.z, 0.02);

    CHECK_NEAR(UnpackYaw(PackYaw(359.9f)), 359.9f, 0.02);
    CHECK_NEAR(UnpackYaw(PackYaw(0.1f)), 0.1f, 0.02);
    CHECK_NEAR(UnpackYaw(PackYaw(-90.0f)), 270.0f, 0.02); // wrap
}

static void TestMoveRoundtrip() {
    proto::Move m;
    m.clientId = 7;
    m.tick = 123456;
    m.pos = Vec3{ 100.5f, -200.25f, 3.75f };
    m.yaw = 123.4f;
    m.vel = Vec3{ 5.5f, -1.25f, 0.0f };
    m.health = 87;
    m.armour = 33;
    m.slot = 2;
    m.inVehicle = 1;
    m.vehicleGuid = MakeGuid(3, 999);
    m.seat = 0;
    m.weaponHash = 0xA1B2C3D4ull;
    m.ammo = 250;
    m.dead = 0;

    Writer w(256);
    m.Write(w);
    Reader r(w.Data().data(), w.Data().size());
    proto::Move out;
    CHECK(out.Read(r));
    CHECK_EQ(out.clientId, m.clientId);
    CHECK_EQ(out.tick, m.tick);
    CHECK_NEAR(out.pos.x, m.pos.x, 0.001);
    CHECK_NEAR(out.pos.y, m.pos.y, 0.001);
    CHECK_NEAR(out.yaw, m.yaw, 0.01);
    CHECK_EQ(out.health, m.health);
    CHECK_EQ(out.armour, m.armour);
    CHECK_EQ(out.slot, m.slot);
    CHECK_EQ(out.inVehicle, m.inVehicle);
    CHECK_EQ(out.vehicleGuid, m.vehicleGuid);
    CHECK_EQ(out.seat, m.seat);
    CHECK_EQ(out.weaponHash, m.weaponHash);
    CHECK_EQ(out.ammo, m.ammo);

    // Wire size sanity: must stay under one UDP datagram with headroom.
    CHECK(w.Data().size() < 80);
}

static void TestVehicleRoundtrip() {
    proto::VehicleState v;
    v.guid = MakeGuid(1, 42);
    v.ownerId = 1;
    v.pos = Vec3{ -50.0f, 300.75f, 1.5f };
    v.yaw = 270.0f;
    v.pitch = -2.5f;
    v.roll = 1.25f;
    v.vel = Vec3{ 20.0f, 0.0f, -0.5f };
    v.engineHealth = 987.5f;
    v.bodyHealth = 1234.0f;
    v.flags = 3;
    v.tyreMask = 0x5A;
    v.driverGuid = MakeGuid(1, 7);

    Writer w(128);
    v.Write(w);
    Reader r(w.Data().data(), w.Data().size());
    proto::VehicleState out;
    CHECK(out.Read(r));
    CHECK_EQ(out.guid, v.guid);
    CHECK_NEAR(out.pos.x, v.pos.x, 0.001);
    CHECK_NEAR(out.yaw, v.yaw, 0.01);
    CHECK_NEAR(out.pitch, v.pitch, 0.01);
    CHECK_NEAR(out.engineHealth, v.engineHealth, 0.01);
    CHECK_EQ(out.tyreMask, v.tyreMask);
    CHECK_EQ(out.driverGuid, v.driverGuid);
}

static void TestPlayerModelRoundtrip() {
    proto::PlayerModel pm;
    pm.clientId = 42;
    pm.modelHash = 0x9C9CF888ull;

    Writer w(64);
    pm.Write(w);
    Reader r(w.Data().data(), w.Data().size());
    proto::PlayerModel out;
    CHECK(out.Read(r));
    CHECK_EQ(out.clientId, u32(42));
    CHECK_EQ(out.modelHash, u64(0x9C9CF888ull));

    // PLAYER_JOINED now carries the skin for late joiners.
    proto::PlayerInfo info;
    info.clientId = 7;
    info.name = "Bob";
    info.modelHash = 0xD34F2B07ull;
    Writer wi(64);
    info.Write(wi);
    Reader ri(wi.Data().data(), wi.Data().size());
    proto::PlayerInfo in;
    CHECK(in.Read(ri));
    CHECK_EQ(in.name, std::string("Bob"));
    CHECK_EQ(in.modelHash, u64(0xD34F2B07ull));
}

static void TestCrc32() {
    // Standard CRC32 test vector: "123456789" -> 0xCBF43926.
    CHECK_EQ(Crc32::Compute("123456789", 9), u32(0xCBF43926));
}

static void TestGuid() {
    u64 g = MakeGuid(5, 1234);
    CHECK_EQ(GuidOwner(g), u32(5));
    CHECK_EQ(GuidLocal(g), u32(1234));
}

void RunProtocolTests() {
    TestFraming();
    TestFramingRejectsGarbage();
    TestReaderBounds();
    TestQuantization();
    TestMoveRoundtrip();
    TestVehicleRoundtrip();
    TestPlayerModelRoundtrip();
    TestCrc32();
    TestGuid();
}
