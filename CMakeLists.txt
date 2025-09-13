cmake_minimum_required(VERSION 3.16)
project(music_player LANGUAGES CXX)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-3.0)

pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDLMIXER REQUIRED IMPORTED_TARGET SDL2_mixer)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::GTKMM PkgConfig::SDL2 PkgConfig::SDLMIXER)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
