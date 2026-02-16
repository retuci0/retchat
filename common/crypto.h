#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>

// primo 2048-bit
#define DH_PRIME 0xFFFFFFFFFFFFFFC5ULL
#define DH_GENERATOR 5
#define KEY_LENGTH 32

void xor_crypt(uint8_t *data, size_t len, const uint8_t *key);

// Diffie-Hellman
uint64_t generate_private_key(void);
uint64_t compute_public_key(uint64_t private_key);
uint64_t compute_shared_secret(uint64_t other_public, uint64_t private_key);

#endif
