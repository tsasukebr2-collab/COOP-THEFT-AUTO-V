# ASI plugin: must be a 64-bit DLL renamed to .asi at install time.
add_library(coop_client SHARED
    src/dllmain.cpp
    src/CoopContext.cpp
    src/natives/ShvCompat.cpp
    src/managers/RemotePlayer.cpp
    src/managers/PlayerManager.cpp
    src/managers/VehicleSync.cpp
    src/managers/WorldSync.cpp
    src/managers/MissionSync.cpp
    src/managers/SaveManager.cpp
    src/managers/SkinManager.cpp
    src/ui/CoopMenu.cpp
)

target_link_libraries(coop_client PRIVATE coop_protocol coop_shared ws2_32)
target_compile_definitions(coop_client PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
set_target_properties(coop_client PROPERTIES
    OUTPUT_NAME "GTAVCoop"
    SUFFIX ".asi"
)
