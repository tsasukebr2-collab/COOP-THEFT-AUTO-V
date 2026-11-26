add_library(coop_shared STATIC
    coop/Log.cpp
    coop/Types.cpp
    coop/KeyValueConfig.cpp
    coop/net/Net.cpp
    coop/save/SaveStore.cpp
)

target_include_directories(coop_shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..        # raiz: "shared/coop/...", "protocol/coop/..."
    ${CMAKE_CURRENT_SOURCE_DIR}           # módulos internos: "save/SaveStore.h", "net/Net.h"
    ${CMAKE_CURRENT_SOURCE_DIR}/coop      # headers planos: "Types.h", "Log.h", "Crc32.h"
)
