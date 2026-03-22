add_executable(example example.cpp)
target_link_libraries(example PRIVATE mempeep)
add_test(NAME example COMMAND example)
