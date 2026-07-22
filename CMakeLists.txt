cmake_minimum_required(VERSION 3.12)
project(ElenixOS C)

# Define a compile-time constant for the ElenixOS system root directory
if(EOS_IS_WASM)
    add_compile_definitions(EOS_SYS_ROOT_DIR="/")
elseif(EOS_PLATFORM STREQUAL "Native")
    add_compile_definitions(EOS_SYS_ROOT_DIR="${CMAKE_SOURCE_DIR}/fs/")
endif()

# Recursively gather all source files
file(GLOB_RECURSE ELENIXOS_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.c
)
# Include RemixIcon.c (icon font used as fallback in all modes)
file(GLOB ELENIXOS_RES_FONT_ICON_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/font/RemixIcon.c
)
list(APPEND ELENIXOS_SRC ${ELENIXOS_RES_FONT_ICON_SRC})

# Conditionally include Chinese bitmap fonts when EOS_FONT_TYPE == EOS_FONT_C_MULTI
# Read the generated Kconfig header to determine the font type
set(_eos_font_type_gen_h "${CMAKE_BINARY_DIR}/generated/eos_config_gen.h")
if(EXISTS "${_eos_font_type_gen_h}")
    file(READ "${_eos_font_type_gen_h}" _eos_gen_config_content)
    string(REGEX MATCH "#define EOS_FONT_TYPE ([0-9]+)" _eos_font_type_match "${_eos_gen_config_content}")
    if(CMAKE_MATCH_1 EQUAL 0)
        message(STATUS "ElenixOS: EOS_FONT_TYPE=C_MULTI — including bitmap font sources")
        file(GLOB ELENIXOS_RES_FONT_BITMAP_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/font/source_han_sans_22.c
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/font/source_han_sans_26.c
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/font/source_han_sans_30.c
        )
        list(APPEND ELENIXOS_SRC ${ELENIXOS_RES_FONT_BITMAP_SRC})
    else()
        message(STATUS "ElenixOS: EOS_FONT_TYPE=TTF — skipping bitmap font sources")
    endif()
else()
    message(WARNING "ElenixOS: generated config header not found — skipping bitmap font sources")
endif()

# Automatically collect all directories that contain header files
file(GLOB_RECURSE HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.h
)

set(ELENIXOS_INC_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/resources")
foreach(h ${HEADER_FILES})
    get_filename_component(dir ${h} DIRECTORY)
    list(APPEND ELENIXOS_INC_DIRS ${dir})
endforeach()
list(REMOVE_DUPLICATES ELENIXOS_INC_DIRS)

# Define library target
add_library(elenixos_lib STATIC ${ELENIXOS_SRC})

target_include_directories(elenixos_lib
    PUBLIC
    ${ELENIXOS_INC_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jerryscript/jerry-core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jerryscript/jerry-ext/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cJSON
)

# Link against necessary libraries

# cJSON
add_library(cjson STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(cjson PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(elenixos_lib PRIVATE cjson)

# JerryScript
add_compile_options(-Wno-gnu-folding-constant)
set(JERRY_VM_HALT ON CACHE INTERNAL "Enable VM halt feature")
set(JERRY_LINE_INFO ON CACHE INTERNAL "Enable line info feature")
set(JERRY_ERROR_MESSAGES ON CACHE INTERNAL "Enable error message strings")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(JERRY_MEM_STATS ON CACHE INTERNAL "Enable memory stats")
else()
    set(JERRY_MEM_STATS OFF CACHE INTERNAL "Enable memory stats")
endif()
set(JERRY_CMDLINE OFF CACHE BOOL "Build jerry command line tool?" FORCE)
set(JERRY_CMDLINE_TEST OFF CACHE BOOL "Build jerry test command line tool?" FORCE)
set(JERRY_CMDLINE_SNAPSHOT OFF CACHE BOOL "Build jerry snapshot command line tool?" FORCE)
set(JERRY_LIBFUZZER OFF CACHE BOOL "Build jerry with libfuzzer support?" FORCE)
set(UNITTESTS OFF CACHE BOOL "Build unit tests?" FORCE)
set(DOCTESTS OFF CACHE BOOL "Build doc tests?" FORCE)
set(ENABLE_AMALGAM OFF CACHE BOOL "Enable amalgamated build?" FORCE)
set(JERRY_PORT ON CACHE BOOL "Build default jerry port implementation?" FORCE)

if(EOS_IS_WASM)
    add_compile_options(-Wno-enum-enum-conversion -Wno-unterminated-string-initialization)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/jerryscript)
target_link_libraries(elenixos_lib PRIVATE jerry-core jerry-ext jerry-port)

# LVGL
target_link_libraries(elenixos_lib PRIVATE lvgl)
