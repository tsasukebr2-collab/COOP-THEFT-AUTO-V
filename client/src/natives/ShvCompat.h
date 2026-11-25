// ScriptHookV compatibility layer - dynamic loading, no SDK .lib needed.
//
// Semantics verified against the public ScriptHookV SDK (Alexander Blade,
// dev-c.com) as used by open-source consumers (MenyooSP nativeCaller.h):
//   nativeCall() returns a pointer to the native's result slot; results are
//   read via *reinterpret_cast<R*>(nativeCall()).
//   Arguments: SHV's nativePush<T> bit-copies T into the low bytes of a
//   zeroed u64 (so float args keep their bits, int handles keep their value).
//   Vector3 results: the game's vector3 has per-component padding
//   (x,pad,y,pad,z,pad) - see ScrVec3 below.
//
// Everything is resolved with GetProcAddress from ScriptHookV.dll, which is
// the standard single-player script host. If it is missing (GTA Online or a
// broken install) the plugin refuses to register and unloads.
#pragma once

#include "../../shared/coop/Types.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>

namespace coop {

using ScriptCallback = void(*)();

using FnNativeInit = void(*)(u64 hash);
using FnNativePush64 = void(*)(u64 value);
using FnNativeCall = void*(*)();
using FnScriptWait = void(*)(u32 ms);
using FnScriptRegister = void(*)(HMODULE, ScriptCallback);
using FnScriptUnregister = void(*)(HMODULE);
using FnKeyboardHandlerRegister = void(*)(void(*handler)(u32 key, u16 repeats, u8 scan, u8 isExtended, u8 withAlt, u8 wasDown, u8 isUpNow));
using FnKeyboardHandlerUnregister = void(*)(void(*handler)(u32 key, u16 repeats, u8 scan, u8 isExtended, u8 withAlt, u8 wasDown, u8 isUpNow));
using FnWorldGetAllPeds = int(*)(int* arr, int cap);
using FnWorldGetAllVehicles = int(*)(int* arr, int cap);
using FnWorldGetAllObjects = int(*)(int* arr, int cap);
using FnGetGameVersion = int(*)();

struct Shv {
    FnNativeInit nativeInit = nullptr;
    FnNativePush64 nativePush64 = nullptr;
    FnNativeCall nativeCall = nullptr;
    FnScriptWait scriptWait = nullptr;
    FnScriptRegister scriptRegister = nullptr;
    FnScriptUnregister scriptUnregister = nullptr;
    FnKeyboardHandlerRegister keyboardHandlerRegister = nullptr;
    FnKeyboardHandlerUnregister keyboardHandlerUnregister = nullptr;
    FnWorldGetAllPeds worldGetAllPeds = nullptr;
    FnWorldGetAllVehicles worldGetAllVehicles = nullptr;
    FnWorldGetAllObjects worldGetAllObjects = nullptr;
    FnGetGameVersion getGameVersion = nullptr;
    HMODULE module = nullptr;

    bool Load();
    static Shv& Instance();
};

// Game script-handle types (see any SHV consumer).
using ScrEntity = i32;
using ScrPed = i32;
using ScrVehicle = i32;
using ScrPlayer = i32;
using ScrHash = u32;

// Game Vector3 as returned by vector natives: padded per component (24 bytes).
struct ScrVec3 {
    f32 x; u32 pad0;
    f32 y; u32 pad1;
    f32 z; u32 pad2;

    ScrVec3() : x(0), pad0(0), y(0), pad1(0), z(0), pad2(0) {}
    ScrVec3(f32 px, f32 py, f32 pz) : x(px), pad0(0), y(py), pad1(0), z(pz), pad2(0) {}
    operator Vec3() const { return Vec3{ x, y, z }; }
};

// Bit-preserving argument push (mirrors SHV nativePush<T>).
template <typename T>
inline u64 ToPush(T v) {
    u64 x = 0;
    std::memcpy(&x, &v, sizeof(T));
    return x;
}

// Non-void invocation.
template <typename R, typename... A>
inline R Invoke(u64 hash, A... args) {
    Shv& shv = Shv::Instance();
    shv.nativeInit(hash);
    (shv.nativePush64(ToPush(args)), ...);
    return *reinterpret_cast<R*>(shv.nativeCall());
}

// Void invocation (explicit; void is not a valid reinterpret result type).
template <typename... A>
inline void InvokeVoid(u64 hash, A... args) {
    Shv& shv = Shv::Instance();
    shv.nativeInit(hash);
    (shv.nativePush64(ToPush(args)), ...);
    shv.nativeCall();
}

// Convenience readers.
inline f32 InvokeF32(u64 hash) { return Invoke<f32>(hash); }
inline i32 InvokeI32(u64 hash) { return Invoke<i32>(hash); }
inline const char* InvokeStr(u64 hash) { return Invoke<const char*>(hash); }
inline ScrVec3 InvokeVec3(u64 hash) { return Invoke<ScrVec3>(hash); }

} // namespace coop
