find_package(Catch2 3 CONFIG REQUIRED)

list(APPEND CMAKE_MODULE_PATH ${Catch2_DIR})
include(Catch)

add_executable(unit_tests test_bitset.cpp test_order_book.cpp)

myproject_target_setup(unit_tests)

target_link_libraries(unit_tests PRIVATE Catch2::Catch2WithMain matching_engine)

catch_discover_tests(unit_tests)
