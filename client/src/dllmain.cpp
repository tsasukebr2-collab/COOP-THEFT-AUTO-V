// GTAV-Coop client - ASI entry point.
//
// Host requirements (documented in INSTALL.md): GTA V (single player) with
// ScriptHookV installed. We register one script thread with ScriptHookV and
// route every native call through it. The F9 menu and all session logic run
// there; networking runs on its own thread behind queues (ClientNet).
#include "CoopContext.h"
#include "natives/ShvCompat.h"
#include "../../shared/coop/Log.h"

#include <windows.h>

namespace {

void KeyboardHandler(u32 key, u16 /*repeats*/, u8 /*scan*/, u8 /*extended*/,
                     u8 /*withAlt*/, u8 /*wasDown*/, u8 /*isUpNow*/) {
    coop::CoopContext::Instance().Menu().OnKey(key);
}

void ScriptMain() {
    coop::CoopContext& ctx = coop::CoopContext::Instance();

    ctx.LoadConfig();

    while (true) {
        // SHV script thread loop: every iteration is one game frame.
        coop::CoopContext::Instance().Tick();
        coop::Shv::Instance().scriptWait(0);
    }
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) {
    using namespace coop;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);

        Log::Init("coop_client.log", LogLevel::Info);
        if (!Shv::Instance().Load()) {
            Log::Error("ScriptHookV bridge unavailable; plugin staying idle.");
            return TRUE;
        }

        // Register with ScriptHookV: the script thread starts when the game
        // reaches a state where scripts run (story mode).
        Shv::Instance().keyboardHandlerRegister(KeyboardHandler);
        Shv::Instance().scriptRegister(module, ScriptMain);

        Log::Info("GTAV-Coop client loaded.");
    } else if (reason == DLL_PROCESS_DETACH) {
        CoopContext::Instance().StopSession();
        CoopContext::Instance().SaveConfig();
        Log::Shutdown();
    }
    return TRUE;
}
