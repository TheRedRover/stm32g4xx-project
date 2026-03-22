#######################################################################################
# Firmware signing and image stitching (Dependency Tracked)
#######################################################################################

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(PRIVATE_KEY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../secrets/keys/private_key.pem")
set(FW_ONLY_BIN      "${CMAKE_BINARY_DIR}/${FINAL_NAME}_fw_only.bin")
set(SIGNED_IMAGE     "${CMAKE_BINARY_DIR}/${FINAL_NAME}_signed.bin")

# Write metadata to a file so changes trigger a re-sign even if code didn't change
set(VERSION_STAMP "${CMAKE_BINARY_DIR}/version_stamp.txt")
file(WRITE "${VERSION_STAMP}" "v${FW_VERSION}-${GIT_HASH}-${BUILD_TIMESTAMP}")

# Generate standard HEX and BIN files (Optional, but usually standard for STM32)
add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${HEX_FILE}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${BIN_FILE}
    COMMENT "Building standard HEX and BIN files..."
)

# Formal dependency chain for the Signed Image
add_custom_command(
    OUTPUT "${SIGNED_IMAGE}"
    # Step 1: Extract raw firmware specifically for signing
    COMMAND ${CMAKE_OBJCOPY}
            -O binary
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            "${FW_ONLY_BIN}"

    # Step 2: Run the signing script
    COMMAND ${Python3_EXECUTABLE} "${PYTHON_BUILD_SCRIPT}"
            --fw          "${FW_ONLY_BIN}"
            --key         "${PRIVATE_KEY_PATH}"
            --mem-map     "${MEMORY_MAP_H}"
            --struct-hdr  "${FW_HEADER_H}"
            --version     "${FW_VERSION}"
            --git-hash    "${GIT_HASH}"
            --out         "${SIGNED_IMAGE}"

    # CMake will re-run this command if ANY of these files change
    DEPENDS
        ${CMAKE_PROJECT_NAME}
        "${VERSION_STAMP}"
        "${PYTHON_BUILD_SCRIPT}"
        "${PRIVATE_KEY_PATH}"
        "${MEMORY_MAP_H}"
        "${FW_HEADER_H}"
    COMMENT "Stitching and signing firmware: ${FINAL_NAME}_signed.bin"
    VERBATIM
)

# Drive the custom command by attaching it to the 'ALL' build target
add_custom_target(sign_firmware ALL DEPENDS "${SIGNED_IMAGE}")

message(STATUS "---------------------------------------------------")
message(STATUS "Firmware Version : ${FW_VERSION}")
message(STATUS "Git Hash         : ${GIT_HASH}")
message(STATUS "Signing Key      : ${PRIVATE_KEY_PATH}")
message(STATUS "Signed Output    : ${SIGNED_IMAGE}")
message(STATUS "---------------------------------------------------")