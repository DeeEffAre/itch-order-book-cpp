add_library(matching_engine STATIC
    bitset.cpp
    book.cpp
    engine.cpp
)

myproject_target_setup(matching_engine)

add_executable(myapp main.cpp)

myproject_target_setup(myapp)
target_link_libraries(myapp PRIVATE matching_engine)
