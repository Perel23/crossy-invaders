cmake_minimum_required(VERSION 3.31)
project(crossy_invaders)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_executable(crossy_invaders
    main.cpp
    Game.cpp
    Game.h
    Components.h
    bagel.h)

set(SDL_STATIC ON)
set(SDL_SHARED OFF)
add_subdirectory(lib/SDL)
target_link_libraries(crossy_invaders PUBLIC SDL3-static)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(lib/SDL_image)
target_link_libraries(crossy_invaders PUBLIC SDL3_image-static)

add_custom_command(TARGET crossy_invaders POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    "${PROJECT_SOURCE_DIR}/res"
    "$<TARGET_FILE_DIR:crossy_invaders>/res")
