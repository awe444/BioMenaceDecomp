cmake_minimum_required(VERSION 3.16)
project(BioMenace C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find SDL2
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# Source files
set(ENGINE_SOURCES
    SRC/ID_CA.C
    SRC/ID_IN.C
    SRC/ID_MM.C
    SRC/ID_RF.C
    SRC/ID_SD.C
    SRC/ID_US_1.C
    SRC/ID_US_2.C
    SRC/ID_VW.C
    SRC/ID_RF_A.C
    SRC/ID_US_A.C
    SRC/opl.c
)

set(GAME_SOURCES
    SRC/BM_MAIN.C
    SRC/BM_TEXT.C
    SRC/BM_DEMO.C
    SRC/BM_GAME1.C
    SRC/BM_GAME2.C
    SRC/BM_GAME3.C
    SRC/BM_STATE.C
    SRC/BM_PLAY1.C
    SRC/BM_PLAY2.C
    SRC/BM_SPEC.C
    SRC/BM_ACT1.C
    SRC/BM_ACT2.C
    SRC/BM_ACT3.C
    SRC/BM1INTRO.C
)

add_executable(bmenace1 ${ENGINE_SOURCES} ${GAME_SOURCES})

target_include_directories(bmenace1 PRIVATE
    ${CMAKE_SOURCE_DIR}/SRC
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(bmenace1 PRIVATE
    ${SDL2_LIBRARIES}
    m
)

target_compile_definitions(bmenace1 PRIVATE
    SDL_PORT=1
)

# Allow case-insensitive file inclusion
target_compile_options(bmenace1 PRIVATE
    -Wno-multichar
    -Wno-pointer-sign
    ${SDL2_CFLAGS_OTHER}
)

# Install
install(TARGETS bmenace1 DESTINATION bin)
