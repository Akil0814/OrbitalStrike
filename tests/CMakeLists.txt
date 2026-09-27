add_executable(game_tests game_tests.cpp)
target_link_libraries(game_tests PRIVATE game_lib)

add_test(NAME game_tests COMMAND game_tests)
