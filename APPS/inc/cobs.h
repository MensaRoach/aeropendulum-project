#ifndef COBS_H
#define COBS_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Encode a byte buffer with the COBS algorithm.
 * 
 * @param ptr Pointer to the unencoded data
 * @param length Length of the unencoded data
 * @param dst Pointer to the destination buffer (should be at least length + 2 bytes long)
 * @return size_t Length of the encoded data (does not include trailing zero)
 */
size_t cobs_encode(const void *ptr, size_t length, uint8_t *dst);

/**
 * @brief Decode a COBS-encoded byte buffer.
 * 
 * @param ptr Pointer to the encoded data (without the trailing zero)
 * @param length Length of the encoded data
 * @param dst Pointer to the destination buffer
 * @return size_t Length of the decoded data, or 0 if error
 */
size_t cobs_decode(const uint8_t *ptr, size_t length, void *dst);

#endif // COBS_H
