cmake_minimum_required(VERSION 3.16)
project(BioMenace C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================================
# SDL2 — built from source placed at android/app/jni/SDL2/
# ============================================================================
set(SDL2_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDL2)

if(NOT EXISTS "${SDL2_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "SDL2 source not found at ${SDL2_SOURCE_DIR}.\n"
        "Download SDL2 source and extract it there. For example:\n"
        "  cd android/app/jni\n"
        "  wget https://github.com/libsdl-org/SDL/releases/download/release-2.30.12/SDL2-2.30.12.tar.gz\n"
        "  tar xf SDL2-2.30.12.tar.gz\n"
        "  mv SDL2-2.30.12 SDL2\n")
endif()

add_subdirectory(${SDL2_SOURCE_DIR} SDL2-build)

# ============================================================================
# Game source files (paths relative to repository root)
# ============================================================================
set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

set(ENGINE_SOURCES
    ${REPO_ROOT}/SRC/ID_CA.c
    ${REPO_ROOT}/SRC/ID_IN.c
    ${REPO_ROOT}/SRC/ID_MM.c
    ${REPO_ROOT}/SRC/ID_RF.c
    ${REPO_ROOT}/SRC/ID_SD.c
    ${REPO_ROOT}/SRC/ID_US_1.c
    ${REPO_ROOT}/SRC/ID_US_2.c
    ${REPO_ROOT}/SRC/ID_VW.c
    ${REPO_ROOT}/SRC/ID_RF_A.c
    ${REPO_ROOT}/SRC/ID_US_A.c
    ${REPO_ROOT}/SRC/ID_TEXTSCR.c
    ${REPO_ROOT}/SRC/opl.c
    ${REPO_ROOT}/SRC/opl/opl3.c
)

set(GAME_SOURCES
    ${REPO_ROOT}/SRC/BM_MAIN.c
    ${REPO_ROOT}/SRC/BM_TEXT.c
    ${REPO_ROOT}/SRC/BM_DEMO.c
    ${REPO_ROOT}/SRC/BM_GAME1.c
    ${REPO_ROOT}/SRC/BM_GAME2.c
    ${REPO_ROOT}/SRC/BM_GAME3.c
    ${REPO_ROOT}/SRC/BM_STATE.c
    ${REPO_ROOT}/SRC/BM_PLAY1.c
    ${REPO_ROOT}/SRC/BM_PLAY2.c
    ${REPO_ROOT}/SRC/BM_SPEC.c
    ${REPO_ROOT}/SRC/BM_ACT1.c
    ${REPO_ROOT}/SRC/BM_ACT2.c
    ${REPO_ROOT}/SRC/BM_ACT3.c
    ${REPO_ROOT}/SRC/BM1INTRO.c
)

# ============================================================================
# Build the game as a shared library named "main" (loaded by SDLActivity)
# ============================================================================
add_library(main SHARED ${ENGINE_SOURCES} ${GAME_SOURCES})

target_include_directories(main PRIVATE
    ${REPO_ROOT}/SRC
    ${REPO_ROOT}/SRC/opl
)

target_link_libraries(main PRIVATE
    SDL2
    m
    log
)

target_compile_definitions(main PRIVATE
    SDL_PORT=1
)

target_compile_options(main PRIVATE
    -Wno-multichar
    -Wno-pointer-sign
    -Wno-incompatible-function-pointer-types
    -Wno-implicit-function-declaration
    -Wno-implicit-int
)
