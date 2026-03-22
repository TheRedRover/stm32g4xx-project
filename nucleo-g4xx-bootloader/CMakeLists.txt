cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME nucleo-g4xx-bootloader)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

get_filename_component(SHARED_LIBS_ABS "${CMAKE_CURRENT_SOURCE_DIR}/../shared" ABSOLUTE)
file(GLOB SHARED_INCLUDES_HEADERS "${SHARED_LIBS_ABS}/Inc/*.h")

#######################################################################################
# Public Key Extraction and Generation
#######################################################################################

include(cmake/extract_public_key.cmake)
#######################################################################################

set (TINYCRYPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/tinycrypt")
set (MICROECC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/micro-ecc")
set (TINYCRYPT_LIB ${TINYCRYPT_DIR}/lib/)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/bootloader.c
    Core/Src/first_boot.c
    ${TINYCRYPT_LIB}/source/sha256.c
    ${TINYCRYPT_LIB}/source/utils.c
    ${MICROECC_DIR}/uECC.c
    ${PUBLIC_KEY_C}
    ${SHARED_LIBS_ABS}/Src/helpers.c
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${TINYCRYPT_LIB}/include
    ${MICROECC_DIR}
    ${SHARED_LIBS_ABS}/Inc
    ${GENERATED_DIR}
    # Add user defined include paths
)

# Build micro-ecc (uECC.c) without debug flags to avoid inline-asm
# constraints and improve optimization. This forces higher optimization
# and disables debug info for that translation unit even in Debug builds.
set(MICROECC_SOURCE "${MICROECC_DIR}/uECC.c")
if(EXISTS ${MICROECC_SOURCE})
    set_source_files_properties(${MICROECC_SOURCE} PROPERTIES COMPILE_FLAGS "-O2 -g0 -DNDEBUG")
endif()
# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

#######################################################################################
# Linker script generation from template with memory map header
#######################################################################################


# Force Absolute Paths
get_filename_component(SHARED_LIBS_ABS "${CMAKE_CURRENT_SOURCE_DIR}/../shared" ABSOLUTE)
file(GLOB SHARED_INCLUDES_HEADERS "${SHARED_LIBS_ABS}/Inc/*.h")

set(LINKER_SCRIPT_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/STM32G474XX_FLASH.ld.S")
set(GENERATED_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/STM32G474XX_FLASH.ld")

# Preprocess with explicit absolute include paths
add_custom_command(
    OUTPUT "${GENERATED_LINKER_SCRIPT}"
    COMMAND ${CMAKE_C_COMPILER} -E -P -xc
            "-D__LINKER_SCRIPT__"           # Define a macro to conditionally include content in the template
            "-I${CMAKE_CURRENT_SOURCE_DIR}"
            "-I${SHARED_LIBS_ABS}/Inc"          # Absolute path to shared folder
            "${LINKER_SCRIPT_TEMPLATE}"
            -o "${GENERATED_LINKER_SCRIPT}"
    DEPENDS "${LINKER_SCRIPT_TEMPLATE}" "${SHARED_INCLUDES_HEADERS}"
    COMMENT "Preprocessing linker script..."
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM
)

# Apply to target
target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${GENERATED_LINKER_SCRIPT}")


#######################################################################################
# Auto-tracking for Version and Git
#######################################################################################

# 1. Track Git Hash changes
set(GIT_HEAD_FILE "${CMAKE_CURRENT_SOURCE_DIR}/.git/HEAD")
set(GIT_INDEX_FILE "${CMAKE_CURRENT_SOURCE_DIR}/.git/index")
if(EXISTS "${GIT_HEAD_FILE}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GIT_HEAD_FILE}")
endif()
if(EXISTS "${GIT_INDEX_FILE}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GIT_INDEX_FILE}")
endif()

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_HASH)
    set(GIT_HASH "00000000")
endif()

string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d_%H%M")

# 2. Track Firmware Version changes
if(NOT DEFINED FW_VERSION)
    set(VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt")

    # FORCE CMake reconfigure if this file is touched
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${VERSION_FILE}")

    if(EXISTS "${VERSION_FILE}")
        file(READ "${VERSION_FILE}" FW_VERSION_RAW)
        string(STRIP "${FW_VERSION_RAW}" FW_VERSION)
    else()
        set(FW_VERSION "1.0.0")
    endif()
endif()

# Final build name
set(FINAL_NAME "${CMAKE_PROJECT_NAME}_${GIT_HASH}_${BUILD_TIMESTAMP}_v${FW_VERSION}")
set(BIN_FILE "${CMAKE_CURRENT_BINARY_DIR}/${FINAL_NAME}.bin")
set(HEX_FILE "${CMAKE_CURRENT_BINARY_DIR}/${FINAL_NAME}.hex")

# 3. Post-Build: Ensure these files are actually created with the correct name
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${HEX_FILE}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${BIN_FILE}
    COMMENT "Generating artifacts: ${FINAL_NAME}"
)