add_library(coop_protocol STATIC
    coop/protocol/Messages.cpp
)

target_include_directories(coop_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(coop_protocol PUBLIC coop_shared)
