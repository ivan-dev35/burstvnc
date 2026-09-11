cmake_minimum_required(VERSION 3.16)
project(burstvnc CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-O3 -mavx2 -mfma -Wall -Wextra -Wno-unused-parameter -pthread)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale)
pkg_check_modules(LIBX11 REQUIRED IMPORTED_TARGET x11 xext xtst xfixes)
pkg_check_modules(LIBPULSE REQUIRED IMPORTED_TARGET libpulse-simple libpulse)
pkg_check_modules(LIBOPUS REQUIRED IMPORTED_TARGET opus)
pkg_check_modules(LIBSDL2 REQUIRED IMPORTED_TARGET sdl2)

add_executable(burst_srv
    src/main.cpp
    src/cap.cpp
    src/cur.cpp
    src/enc.cpp
    src/inp.cpp
    src/aud.cpp
    src/srv.cpp
)

target_link_libraries(burst_srv PRIVATE
    Boost::system
    Boost::thread
    PkgConfig::LIBAV
    PkgConfig::LIBX11
    PkgConfig::LIBPULSE
    PkgConfig::LIBOPUS
    pthread
)

add_executable(burst_cli
    src/cli.cpp
)

target_link_libraries(burst_cli PRIVATE
    Boost::system
    Boost::thread
    PkgConfig::LIBAV
    PkgConfig::LIBSDL2
    pthread
)

install(TARGETS burst_srv burst_cli DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/bin)
