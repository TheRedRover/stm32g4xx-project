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
set(CMAKE_PROJECT_NAME nucleo-g4xx-firmware)

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

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${SHARED_LIBS_ABS}/Src/fw_header.c
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${SHARED_LIBS_ABS}/Inc
    # Add user defined include paths
)

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
# Generate Binary and Hex files automatically
#######################################################################################

# This creates the .bin file
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating binary image: ${CMAKE_PROJECT_NAME}.bin"
)

# This creates the .hex file
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
    COMMENT "Generating Intel Hex file: ${CMAKE_PROJECT_NAME}.hex"
)

# Optional: show the size in the terminal every time you build
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Final binary size:"
)

#######################################################################################
# OTA Image Generation (Header + Firmware)
#######################################################################################

# Paths to required files
set(PYTHON_BUILD_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/build_image.py")
set(MEMORY_MAP_H "${CMAKE_CURRENT_SOURCE_DIR}/../shared/Inc/memory_map.h")
set(FW_HEADER_H "${CMAKE_CURRENT_SOURCE_DIR}/../shared/Inc/fw_header.h")

# The raw binary produced by objcopy (we depend on it)
set(FW_RAW_BIN "${CMAKE_PROJECT_NAME}.bin")
set(OTA_OUTPUT_IMAGE "${CMAKE_PROJECT_NAME}_OTA.bin")

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND python3 ${PYTHON_BUILD_SCRIPT}
            --fw ${FW_RAW_BIN}
            --mem-map ${MEMORY_MAP_H}
            --struct-hdr ${FW_HEADER_H}
            --out ${OTA_OUTPUT_IMAGE}
            --version 1  # You can replace this with a CMake variable
            --git-auto   # Automatically fetch the git hash
    COMMENT "Generating OTA update image (Header + Firmware)..."
    VERBATIM
)