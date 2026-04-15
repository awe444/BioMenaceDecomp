cmake_minimum_required(VERSION 3.16)
project(BioMenace C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find SDL2
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# Source files
set(ENGINE_SOURCES
    SRC/ID_CA.c
    SRC/ID_IN.c
    SRC/ID_MM.c
    SRC/ID_RF.c
    SRC/ID_SD.c
    SRC/ID_US_1.c
    SRC/ID_US_2.c
    SRC/ID_VW.c
    SRC/ID_RF_A.c
    SRC/ID_US_A.c
    SRC/ID_TEXTSCR.c
    SRC/opl.c
)

set(GAME_SOURCES
    SRC/BM_MAIN.c
    SRC/BM_TEXT.c
    SRC/BM_DEMO.c
    SRC/BM_GAME1.c
    SRC/BM_GAME2.c
    SRC/BM_GAME3.c
    SRC/BM_STATE.c
    SRC/BM_PLAY1.c
    SRC/BM_PLAY2.c
    SRC/BM_SPEC.c
    SRC/BM_ACT1.c
    SRC/BM_ACT2.c
    SRC/BM_ACT3.c
    SRC/BM1INTRO.c
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
