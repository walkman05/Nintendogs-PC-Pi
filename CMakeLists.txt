cmake_minimum_required(VERSION 3.20)
project(nintendogs_pc C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The game code assumes 32-bit pointers and fixed DS addresses, so the host build is 32-bit.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(WARNING "nintendogs_pc must be built as a 32-bit program (Linux: -DCMAKE_C_FLAGS=-m32, "
                    "Windows: -A Win32). Continuing so the shell can be checked, but the game will not run.")
endif()

option(NDOGS_BUILD_GAME "Compile the decompiled game code (milestone 2)" OFF)

add_executable(nintendogs_pc
    port/src/main.c
    port/src/platform_mem.c
    port/src/host_cpu.c
    port/src/host_link.c
)
target_include_directories(nintendogs_pc PRIVATE port/include include)

if(MSVC)
    target_compile_options(nintendogs_pc PRIVATE /W3)
else()
    # Signed overflow wraps like the DS hardware; see CLAUDE.md.
    target_compile_options(nintendogs_pc PRIVATE -Wall -fwrapv -fno-strict-aliasing)
endif()

if(NDOGS_BUILD_GAME)
    file(GLOB_RECURSE NDOGS_GAME_SOURCES CONFIGURE_DEPENDS src/*.c)
    target_sources(nintendogs_pc PRIVATE ${NDOGS_GAME_SOURCES})
    target_compile_definitions(nintendogs_pc PRIVATE NDOGS_PC=1)
endif()

# Milestone 1: `cmake --build build --target extract` with -DNDOGS_ROM=<baserom.nds> -DNDOGS_DSD=<dsd executable>
set(NDOGS_ROM "" CACHE FILEPATH "Your own Nintendogs (Europe) kiosk demo ROM")
set(NDOGS_DSD "" CACHE FILEPATH "ds-decomp (dsd) executable")
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_custom_target(extract
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/port/tools/extract_rom.py --rom "${NDOGS_ROM}" --dsd "${NDOGS_DSD}"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Extracting game data from your ROM into extracted/ (git-ignored)"
        VERBATIM)
endif()
