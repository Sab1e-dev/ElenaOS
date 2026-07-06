/**
 * @file eos_sha256.c
 * @brief Lightweight SHA-256 hash implementation (FIPS 180-4)
 */

#include "eos_sha256.h"

/* Includes ---------------------------------------------------*/
#include <string.h>

/* Macros and Definitions -------------------------------------*/

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHR(x, n) ((x) >> (n))

#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#define SIGMA0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SIGMA1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ SHR(x, 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))

/* Variables --------------------------------------------------*/

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

/* Function Implementations -----------------------------------*/

static void _sha256_transform(uint32_t state[8], const uint8_t block[64])
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;
    int i;

    /* Prepare message schedule */
    for (i = 0; i < 16; i++)
    {
        w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) | ((uint32_t)block[i * 4 + 2] << 8)
               | ((uint32_t)block[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++)
    {
        w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];
    }

    /* Initialize working variables */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    /* Compression */
    for (i = 0; i < 64; i++)
    {
        t1 = h + SIGMA1(e) + CH(e, f, g) + K[i] + w[i];
        t2 = SIGMA0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /* Update state */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void eos_sha256(const uint8_t *data, size_t len, uint8_t out[EOS_SHA256_DIGEST_SIZE])
{
    uint32_t state[8] =
        {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    uint8_t block[64];
    size_t block_idx = 0;
    uint64_t bit_len = (uint64_t)len * 8;
    size_t i;

    /* Process full blocks */
    for (i = 0; i < len; i++)
    {
        block[block_idx++] = data[i];
        if (block_idx == 64)
        {
            _sha256_transform(state, block);
            block_idx = 0;
        }
    }

    /* Padding */
    block[block_idx++] = 0x80;
    if (block_idx > 56)
    {
        /* Need an extra block for padding */
        while (block_idx < 64)
        {
            block[block_idx++] = 0;
        }
        _sha256_transform(state, block);
        block_idx = 0;
    }
    while (block_idx < 56)
    {
        block[block_idx++] = 0;
    }

    /* Append length in bits (big-endian) */
    for (i = 0; i < 8; i++)
    {
        block[63 - i] = (uint8_t)(bit_len >> (i * 8));
    }
    _sha256_transform(state, block);

    /* Output digest (big-endian) */
    for (i = 0; i < 4; i++)
    {
        out[i] = (uint8_t)(state[0] >> (24 - i * 8));
        out[i + 4] = (uint8_t)(state[1] >> (24 - i * 8));
        out[i + 8] = (uint8_t)(state[2] >> (24 - i * 8));
        out[i + 12] = (uint8_t)(state[3] >> (24 - i * 8));
        out[i + 16] = (uint8_t)(state[4] >> (24 - i * 8));
        out[i + 20] = (uint8_t)(state[5] >> (24 - i * 8));
        out[i + 24] = (uint8_t)(state[6] >> (24 - i * 8));
        out[i + 28] = (uint8_t)(state[7] >> (24 - i * 8));
    }
}

void eos_sha256_to_hex(const uint8_t hash[EOS_SHA256_DIGEST_SIZE], char *out_hex, size_t out_size)
{
    static const char hex_chars[] = "0123456789abcdef";
    size_t i;

    if (out_size < EOS_SHA256_HEX_STR_SIZE)
    {
        if (out_size > 0)
        {
            out_hex[0] = '\0';
        }
        return;
    }

    for (i = 0; i < EOS_SHA256_DIGEST_SIZE; i++)
    {
        out_hex[i * 2] = hex_chars[(hash[i] >> 4) & 0x0F];
        out_hex[i * 2 + 1] = hex_chars[hash[i] & 0x0F];
    }
    out_hex[64] = '\0';
}
