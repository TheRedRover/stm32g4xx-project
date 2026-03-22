set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(PUBLIC_KEY_H  "${GENERATED_DIR}/public_key.h")
set(PUBLIC_KEY_C  "${GENERATED_DIR}/public_key.c")
set(PUB_RAW       "${GENERATED_DIR}/pub.raw")
set(OPENSSL "openssl")
set(PRIVATE_KEY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../secrets/keys/private_key.pem")

file(MAKE_DIRECTORY "${GENERATED_DIR}")

execute_process(
    COMMAND bash -lc
            "${OPENSSL} ec -in \"${PRIVATE_KEY_PATH}\" -pubout -conv_form uncompressed -outform DER \
             | tail -c 65 | tail -c +2"
    OUTPUT_FILE "${PUB_RAW}"
    RESULT_VARIABLE RESULT_GEN
)

if(NOT RESULT_GEN EQUAL 0)
    message(FATAL_ERROR "OpenSSL failed to generate raw P-256 public key")
endif()

file(READ "${PUB_RAW}" PUB_RAW_HEX HEX)

string(LENGTH "${PUB_RAW_HEX}" PUB_RAW_HEX_LEN)
math(EXPR PUB_RAW_BYTES "${PUB_RAW_HEX_LEN} / 2")

if(NOT PUB_RAW_BYTES EQUAL 64)
    message(FATAL_ERROR "Public key must be 64 bytes, got ${PUB_RAW_BYTES}")
endif()

set(KEY_BYTES_CLEAN "")
math(EXPR LAST_BYTE "${PUB_RAW_BYTES} - 1")

foreach(i RANGE 0 ${LAST_BYTE})
    math(EXPR idx "${i} * 2")
    string(SUBSTRING "${PUB_RAW_HEX}" ${idx} 2 byte_hex)

    string(APPEND KEY_BYTES_CLEAN "0x${byte_hex}")
    if(NOT i EQUAL LAST_BYTE)
        string(APPEND KEY_BYTES_CLEAN ", ")
    endif()
endforeach()

file(WRITE "${PUBLIC_KEY_H}"
"#ifndef __PUBLIC_KEY_H
#define __PUBLIC_KEY_H

#include <stdint.h>

extern const uint8_t g_public_key[64];

#endif
")

file(WRITE "${PUBLIC_KEY_C}"
"#include \"public_key.h\"

const uint8_t g_public_key[64] = {
${KEY_BYTES_CLEAN}
};
")