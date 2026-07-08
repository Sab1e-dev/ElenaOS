/**
 * @file eos_sha256.h
 * @brief Lightweight SHA-256 hash implementation
 */

#ifndef EOS_SHA256_H
#define EOS_SHA256_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ---------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

/* Public macros ----------------------------------------------*/

#define EOS_SHA256_DIGEST_SIZE 32 /**< SHA-256 digest size in bytes */
#define EOS_SHA256_HEX_STR_SIZE 65 /**< Hex string size (64 chars + null) */

/* Public typedefs --------------------------------------------*/

/* Public function prototypes --------------------------------*/

/**
 * @brief One-shot SHA-256 hash computation
 * @param data Input data buffer
 * @param len Input data length in bytes
 * @param out Output digest buffer (must be 32 bytes)
 */
void eos_sha256(const uint8_t *data, size_t len, uint8_t out[EOS_SHA256_DIGEST_SIZE]);

/**
 * @brief Convert binary SHA-256 hash to lowercase hex string
 * @param hash 32-byte hash digest
 * @param out_hex Output buffer for hex string (must be >= 65 bytes)
 * @param out_size Size of output buffer
 */
void eos_sha256_to_hex(const uint8_t hash[EOS_SHA256_DIGEST_SIZE], char *out_hex, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* EOS_SHA256_H */
