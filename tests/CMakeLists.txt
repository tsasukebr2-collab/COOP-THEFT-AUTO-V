add_executable(coop_tests
    test_main.cpp
    test_protocol.cpp
    test_save.cpp
)

target_link_libraries(coop_tests PRIVATE coop_protocol coop_shared)
