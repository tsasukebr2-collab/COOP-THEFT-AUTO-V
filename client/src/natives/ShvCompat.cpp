#include "ShvCompat.h"
#include "../shared/coop/Log.h"

namespace coop {

Shv& Shv::Instance() {
    static Shv shv;
    return shv;
}

bool Shv::Load() {
    HMODULE shv = GetModuleHandleA("ScriptHookV.dll");
    if (!shv) {
        Log::Error("ScriptHookV.dll not loaded - co-op plugin requires it (SP only).");
        return false;
    }

#define COOP_LOAD(name) name = reinterpret_cast<decltype(name)>(GetProcAddress(shv, #name)); \
        if (!name) { Log::Error("ScriptHookV.dll missing export " #name); return false; }
    COOP_LOAD(nativeInit)
    COOP_LOAD(nativePush64)
    COOP_LOAD(nativeCall)
    COOP_LOAD(scriptWait)
    COOP_LOAD(scriptRegister)
    COOP_LOAD(scriptUnregister)
    COOP_LOAD(keyboardHandlerRegister)
    COOP_LOAD(keyboardHandlerUnregister)
    COOP_LOAD(worldGetAllPeds)
    COOP_LOAD(worldGetAllVehicles)
    COOP_LOAD(worldGetAllObjects)
    COOP_LOAD(getGameVersion)
#undef COOP_LOAD

    module = shv;
    return true;
}

} // namespace coop
