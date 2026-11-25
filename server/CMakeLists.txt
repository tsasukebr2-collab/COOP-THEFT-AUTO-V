add_executable(coop_server
    src/ServerMain.cpp
    src/ServerSession.cpp
)

target_link_libraries(coop_server PRIVATE coop_protocol coop_shared ws2_32)
