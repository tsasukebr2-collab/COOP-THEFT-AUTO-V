# GTAV-Coop native hash + wrapper generator.
# Reads tools/natives.json (alloc8or gta5-nativedb-data) and emits:
#   client/src/natives/NativeHashes.h  (hash constants)
#   client/src/natives/NativeWrappers.h (typed inline wrappers with EXACT
#    parameter lists from the database - no guessed signatures)
# Fails loudly on unknown names so we never guess a hash or a signature.

param(
    [string]$JsonPath  = (Join-Path $PSScriptRoot "natives.json"),
    [string]$HashesOut = (Join-Path $PSScriptRoot "..\client\src\natives\NativeHashes.h"),
    [string]$WrapOut   = (Join-Path $PSScriptRoot "..\client\src\natives\NativeWrappers.h")
)

$ErrorActionPreference = "Stop"

$Natives = @(
    "PLAYER_ID","PLAYER_PED_ID","GET_PLAYER_NAME","IS_PLAYER_PLAYING",
    "GET_PLAYER_WANTED_LEVEL","SET_PLAYER_WANTED_LEVEL","SET_PLAYER_WANTED_LEVEL_NOW",
    "SET_MAX_WANTED_LEVEL","SET_PLAYER_CONTROL","IS_PLAYER_DEAD",

    "GET_PED_DRAWABLE_VARIATION","GET_PED_TEXTURE_VARIATION","GET_PED_PALETTE_VARIATION",
    "SET_PED_COMPONENT_VARIATION","SET_PED_DEFAULT_COMPONENT_VARIATION",
    "SET_PED_ARMOUR","GET_PED_ARMOUR",
    "GET_CURRENT_PED_WEAPON","GET_SELECTED_PED_WEAPON","GIVE_WEAPON_TO_PED",
    "REMOVE_ALL_PED_WEAPONS","SET_PED_AMMO","GET_AMMO_IN_PED_WEAPON",
    "CREATE_PED","DELETE_PED","IS_PED_IN_ANY_VEHICLE","GET_VEHICLE_PED_IS_IN",
    "SET_PED_INTO_VEHICLE","RESURRECT_PED","CLEAR_PED_TASKS_IMMEDIATELY",
    "IS_PED_DEAD_OR_DYING","SET_PED_CAN_RAGDOLL","SET_PED_ACCURACY",
    "GET_PED_SOURCE_OF_DEATH","SET_PED_RELATIONSHIP_GROUP_HASH","GET_RELATIONSHIP_BETWEEN_PEDS",

    "DOES_ENTITY_EXIST","DELETE_ENTITY","SET_ENTITY_AS_MISSION_ENTITY",
    "GET_ENTITY_COORDS","SET_ENTITY_COORDS","GET_ENTITY_ROTATION","SET_ENTITY_ROTATION",
    "GET_ENTITY_HEADING","SET_ENTITY_HEADING","GET_ENTITY_VELOCITY","SET_ENTITY_VELOCITY",
    "GET_ENTITY_HEALTH","SET_ENTITY_HEALTH","GET_ENTITY_MAX_HEALTH","SET_ENTITY_MAX_HEALTH",
    "IS_ENTITY_DEAD","GET_ENTITY_MODEL","SET_ENTITY_INVINCIBLE",
    "GET_ENTITY_PITCH","GET_ENTITY_ROLL","GET_ENTITY_SPEED",
    "GET_GROUND_Z_FOR_3D_COORD","SET_ENTITY_COLLISION","FREEZE_ENTITY_POSITION",

    "CREATE_VEHICLE","SET_VEHICLE_ENGINE_ON","GET_IS_VEHICLE_ENGINE_RUNNING","SET_VEHICLE_FIXED",
    "SET_VEHICLE_TYRE_BURST","IS_VEHICLE_TYRE_BURST","SET_VEHICLE_DOOR_OPEN","SET_VEHICLE_DOOR_SHUT",
    "GET_PED_IN_VEHICLE_SEAT","IS_VEHICLE_SEAT_FREE","GET_VEHICLE_MAX_NUMBER_OF_PASSENGERS",
    "SET_VEHICLE_LIGHTS","GET_VEHICLE_LIGHTS_STATE","SET_VEHICLE_SIREN",
    "GET_VEHICLE_BODY_HEALTH","SET_VEHICLE_BODY_HEALTH","GET_VEHICLE_ENGINE_HEALTH",
    "SET_VEHICLE_ENGINE_HEALTH","GET_VEHICLE_PETROL_TANK_HEALTH","SET_VEHICLE_PETROL_TANK_HEALTH",
    "GET_VEHICLE_NUMBER_PLATE_TEXT","SET_VEHICLE_NUMBER_PLATE_TEXT","SET_VEHICLE_ON_GROUND_PROPERLY",
    "SET_VEHICLE_HAS_BEEN_OWNED_BY_PLAYER","SET_VEHICLE_NEEDS_TO_BE_HOTWIRED",

    "SET_CLOCK_TIME","GET_CLOCK_HOURS","GET_CLOCK_MINUTES","GET_CLOCK_SECONDS","PAUSE_CLOCK",
    "SET_WEATHER_TYPE_NOW_PERSIST","SET_WEATHER_TYPE_NOW",
    "ADD_EXPLOSION",
    "GET_STATE_OF_CLOSEST_DOOR_OF_TYPE","SET_STATE_OF_CLOSEST_DOOR_OF_TYPE",

    "DRAW_RECT","SET_TEXT_FONT","SET_TEXT_SCALE","SET_TEXT_COLOUR","SET_TEXT_OUTLINE",
    "SET_TEXT_CENTRE","BEGIN_TEXT_COMMAND_DISPLAY_TEXT","ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME",
    "END_TEXT_COMMAND_DISPLAY_TEXT","GET_SCREEN_RESOLUTION",

    "GET_HASH_KEY","GET_MISSION_FLAG","IS_CUTSCENE_ACTIVE","DISPLAY_HUD","TIMESTEP",
    "STAT_GET_INT","STAT_SET_INT",
    "REQUEST_MODEL","HAS_MODEL_LOADED","SET_MODEL_AS_NO_LONGER_NEEDED",
    "IS_MODEL_IN_CDIMAGE","IS_MODEL_VALID","SET_PLAYER_MODEL"
)

$json = Get-Content -Raw -Path $JsonPath | ConvertFrom-Json

# name -> object (hash key, params, return_type)
$map = @{}
foreach ($ns in $json.PSObject.Properties) {
    foreach ($n in $ns.Value.PSObject.Properties) {
        $name = $n.Value.name
        if (-not $map.ContainsKey($name)) { $map[$name] = @{ Hash = $n.Name; Def = $n.Value } }
    }
}

# JSON type -> C++ parameter type + how to pass it
function ParamType([string]$t) {
    switch ($t) {
        "Ped"          { return @{ cpp = "ScrPed";     pass = "val" } }
        "Vehicle"      { return @{ cpp = "ScrVehicle"; pass = "val" } }
        "Entity"       { return @{ cpp = "ScrEntity";  pass = "val" } }
        "Player"       { return @{ cpp = "ScrPlayer";  pass = "val" } }
        "Hash"         { return @{ cpp = "ScrHash";    pass = "val" } }
        "BOOL"         { return @{ cpp = "BOOL_";      pass = "val" } }
        "int"          { return @{ cpp = "i32";        pass = "val" } }
        "Any"          { return @{ cpp = "u32";        pass = "val" } }
        "float"        { return @{ cpp = "f32";        pass = "val" } }
        "const char*"  { return @{ cpp = "const char*";pass = "val" } }
        "char*"        { return @{ cpp = "char*";      pass = "ptr" } }
        default {
            if ($t.EndsWith("*")) { return @{ cpp = "void*"; pass = "ptr" } }
            if ($t -eq "Vector3") { return @{ cpp = "ScrVec3"; pass = "val" } }
            return @{ cpp = "u64"; pass = "val" }  # opaque fallback
        }
    }
}
function RetType([string]$t) {
    switch ($t) {
        "void"  { return "void" }
        "Ped"   { return "ScrPed" }
        "Vehicle"{ return "ScrVehicle" }
        "Entity"{ return "ScrEntity" }
        "Player"{ return "ScrPlayer" }
        "Hash"  { return "ScrHash" }
        "BOOL"  { return "BOOL_" }
        "int"   { return "i32" }
        "Any"   { return "u32" }
        "float" { return "f32" }
        "const char*" { return "const char*" }
        "char*" { return "const char*" }
        "Vector3" { return "ScrVec3" }
        default { return "u64" }
    }
}

$missing = @()
$hashLines = New-Object System.Collections.Generic.List[string]
$hashLines.Add("// Auto-generated by tools/gen_natives.ps1 - DO NOT EDIT BY HAND.")
$hashLines.Add("// Source: alloc8or/gta5-nativedb-data natives.json")
$hashLines.Add("#pragma once")
$hashLines.Add("")
$hashLines.Add("#include <cstdint>")
$hashLines.Add("")
$hashLines.Add("namespace coop {")
$hashLines.Add("enum : std::uint64_t {")

$wrapLines = New-Object System.Collections.Generic.List[string]
$wrapLines.Add("// Auto-generated by tools/gen_natives.ps1 - DO NOT EDIT BY HAND.")
$wrapLines.Add("// Signatures are taken verbatim from the natives database.")
$wrapLines.Add("#pragma once")
$wrapLines.Add("")
$wrapLines.Add("#include `"ShvCompat.h`"")
$wrapLines.Add("#include `"NativeHashes.h`"")
$wrapLines.Add("")
$wrapLines.Add("namespace coop {")

foreach ($n in $Natives) {
    if (-not $map.ContainsKey($n)) { $missing += $n; continue }
    $hash = ($map[$n].Hash).TrimStart("0x")
    $def  = $map[$n].Def
    $hashLines.Add("    NATIVE_$n = 0x${hash}ull,")

    $ret = RetType $def.return_type
    $params = New-Object System.Collections.Generic.List[string]
    $argNames = New-Object System.Collections.Generic.List[string]
    $i = 0
    foreach ($p in $def.params) {
        $i++
        $pt = ParamType $p.type
        $pname = "a$i"
        $params.Add("$($pt.cpp) $pname")
        $argNames.Add($pname)
    }
    $paramStr = ($params -join ", ")
    if ($paramStr -eq "") { $paramStr = "void" }
    $argStr = ($argNames -join ", ")

    if ($ret -eq "void") {
        if ($argStr) {
            $wrapLines.Add("inline void $n($paramStr) { InvokeVoid(NATIVE_$n, $argStr); }")
        } else {
            $wrapLines.Add("inline void $n(void) { InvokeVoid(NATIVE_$n); }")
        }
    } else {
        if ($argStr) {
            $wrapLines.Add("inline $ret $n($paramStr) { return Invoke<$ret>(NATIVE_$n, $argStr); }")
        } else {
            $wrapLines.Add("inline $ret $n(void) { return Invoke<$ret>(NATIVE_$n); }")
        }
    }
}

$hashLines.Add("};")
$hashLines.Add("} // namespace coop")
$hashLines.Add("")
$wrapLines.Add("} // namespace coop")
$wrapLines.Add("")

if ($missing.Count -gt 0) {
    Write-Error ("MISSING NATIVES (fix names, do not guess): " + ($missing -join ", "))
}

foreach ($pair in @(@($HashesOut, $hashLines), @($WrapOut, $wrapLines))) {
    $dir = Split-Path $pair[0] -Parent
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    Set-Content -Path $pair[0] -Value $pair[1] -Encoding UTF8
}
Write-Output ("Generated {0} natives x2 files." -f $Natives.Count)
