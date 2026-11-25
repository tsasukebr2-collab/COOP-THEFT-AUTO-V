// CoopContext: the client's single orchestrator. Owns every manager, pumps
// network messages into them, runs the per-tick capture/broadcast loop and
// hosts the F9 menu.
#pragma once

#include "net/ClientNet.h"
#include "managers/PlayerManager.h"
#include "managers/VehicleSync.h"
#include "managers/WorldSync.h"
#include "managers/MissionSync.h"
#include "managers/SaveManager.h"
#include "managers/SkinManager.h"
#include "ui/CoopMenu.h"

#include <string>

namespace coop {

class CoopContext {
public:
    static CoopContext& Instance();

    // Lifecycle (script thread).
    bool StartSession(const std::string& ip, u16 tcpPort, const std::string& name);
    void StopSession();
    bool InSession() const;

    // Per-frame pump (script thread, every frame).
    void Tick();

    // Menu state for the UI.
    CoopMenu& Menu() { return menu_; }
    SkinManager& Skin() { return skin_; }
    u8 LocalSlot() const { return u8(players_.Local().slot); }
    u32 MenuPing() const { return net_.PingMs(); }

    // Config access (loaded once from coop_client.ini next to the .asi).
    struct Config {
        std::string serverIp = "127.0.0.1";
        u16 tcpPort = kDefaultTcpPort;
        u16 udpPort = kDefaultUdpPort;
        std::string playerName = "Player";
        u64 autosaveMs = 120000;   // 2 min
        u32 moveRateHz = 20;
        bool autoConnect = true;   // seamless: connect as soon as story mode loads
    };
    const Config& ConfigData() const { return config_; }
    void LoadConfig();
    void SaveConfig();

private:
    CoopContext() = default;

    void PumpTcp();
    void PumpUdp();
    void BroadcastMovement(u64 nowMs);
    void BroadcastOwnedVehicles(u64 nowMs);
    void BroadcastWorldIfHost(u64 nowMs);
    void PushSaveIfDue(u64 nowMs);
    void PushSaveNow(const char* reason); // immediate shared-save push (host)
    void CheckVehicleEnterExit();

    ClientNet net_;
    PlayerManager players_;
    VehicleSync vehicles_;
    WorldSync world_;
    MissionSync missions_;
    SaveManager save_;
    SkinManager skin_;
    CoopMenu menu_;
    Config config_;

    u32 tick_ = 0;
    u64 tickStartMs_ = 0;
    u64 lastMoveSentMs_ = 0;
    u64 lastWorldSentMs_ = 0;
    u64 lastVehicleSentMs_ = 0;
    u64 lastInVehicle_ = 0;
    u64 lastVehicleGuid_ = 0;
    u8 lastSeat_ = 255;
    bool saveAdopted_ = false;
    bool configDirty_ = false;
};

} // namespace coop
