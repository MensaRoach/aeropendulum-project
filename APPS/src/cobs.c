#include "cobs.h"

size_t cobs_encode(const void *ptr, size_t length, uint8_t *dst) {
    const uint8_t *buffer = (const uint8_t *)ptr;
    uint8_t *code_ptr = dst++;
    uint8_t code = 0x01;
    size_t encoded_length = 1;

    while (length--) {
        if (*buffer == 0) {
            *code_ptr = code;
            code_ptr = dst++;
            code = 0x01;
        } else {
            *dst++ = *buffer;
            code++;
            if (code == 0xFF) {
                *code_ptr = code;
                code_ptr = dst++;
                code = 0x01;
            }
        }
        buffer++;
        encoded_length++;
    }

    *code_ptr = code;
    return encoded_length;
}

size_t cobs_decode(const uint8_t *ptr, size_t length, void *dst) {
    const uint8_t *source = ptr;
    uint8_t *dest = (uint8_t *)dst;
    size_t decoded_length = 0;

    while (length) {
        uint8_t code = *source++;
        length--;
        
        if (code == 0) {
            return 0; // Error: zero byte found in encoded data
        }

        uint8_t len = code - 1;
        if (len > length) {
            return 0; // Error: truncated data
        }

        for (uint8_t i = 0; i < len; i++) {
            *dest++ = *source++;
        }
        
        length -= len;
        decoded_length += len;

        if (code < 0xFF && length > 0) {
            *dest++ = 0;
            decoded_length++;
        }
    }

    return decoded_length;
}
