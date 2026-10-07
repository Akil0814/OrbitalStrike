add_executable(game_tests game_tests.cpp)
target_link_libraries(game_tests PRIVATE game_lib)
elysia_copy_sdl_runtime(game_tests)

add_test(NAME game_tests COMMAND game_tests)

add_executable(game_scene_tests game_scene_tests.cpp)
target_link_libraries(game_scene_tests PRIVATE game_lib)
elysia_copy_sdl_runtime(game_scene_tests)

add_test(NAME game_scene_tests COMMAND game_scene_tests)
