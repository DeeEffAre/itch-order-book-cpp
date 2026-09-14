find_package(benchmark CONFIG REQUIRED)

add_executable(matching_engine_bench
    main.cpp
    itch_replay_bench.cpp
    order_book_ops_bench.cpp
)

myproject_target_setup(matching_engine_bench)

target_link_libraries(matching_engine_bench PRIVATE benchmark::benchmark matching_engine)
target_include_directories(matching_engine_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
