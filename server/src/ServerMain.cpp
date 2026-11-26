// GTAV-Coop dedicated server entry point.
#include "../src/ServerSession.h"
#include "../../shared/coop/KeyValueConfig.h"
#include "../../shared/coop/Log.h"

#include <csignal>
#include <iostream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static coop::CoopServer* g_server = nullptr;
static BOOL WINAPI ConsoleCtrl(DWORD /*type*/) {
    if (g_server) g_server->Stop();
    return TRUE;
}
#endif

int main(int argc, char** argv) {
    using namespace coop;

    std::string configPath = "coop_server.ini";
    if (argc > 1) configPath = argv[1];

    ServerConfig cfg;
    KeyValueConfig file;
    if (file.Load(configPath)) {
        cfg.tcpPort = u16(file.GetInt("tcpPort", cfg.tcpPort));
        cfg.udpPort = u16(file.GetInt("udpPort", cfg.udpPort));
        cfg.maxPlayers = u32(file.GetInt("maxPlayers", int(cfg.maxPlayers)));
        cfg.groupId = file.Get("groupId", cfg.groupId);
        cfg.savePath = file.Get("savePath", cfg.savePath);
        cfg.password = file.Get("password", cfg.password);
        cfg.autosaveIntervalMs = u64(file.GetInt("autosaveIntervalMs", int(cfg.autosaveIntervalMs)));
        cfg.clientTimeoutMs = u32(file.GetInt("clientTimeoutMs", int(cfg.clientTimeoutMs)));
        cfg.logLevel = u32(file.GetInt("logLevel", int(cfg.logLevel)));
        std::cout << "config loaded from " << configPath << "\n";
    } else {
        std::cout << "no config at " << configPath << ", using defaults\n";
    }

    static CoopServer server;
    g_server = &server;
#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleCtrl, TRUE);
#endif

    if (!server.Start(cfg)) {
        Log::Error("server failed to start");
        return 1;
    }
    server.Run();
    return 0;
}
