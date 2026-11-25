add_library(coop_shared STATIC
    coop/Log.cpp
    coop/Types.cpp
    coop/KeyValueConfig.cpp
    coop/net/Net.cpp
    coop/save/SaveStore.cpp
)

target_include_directories(coop_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
