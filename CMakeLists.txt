cmake_minimum_required(VERSION 3.20)
project(GTAV-Coop CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT WIN32)
    message(FATAL_ERROR "GTAV-Coop targets Windows (GTA V). POSIX code exists for a future dedicated-server port but is not built here.")
endif()

add_subdirectory(shared)
add_subdirectory(protocol)
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(tests)
