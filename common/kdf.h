#ifndef KDF_H
#define KDF_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// simula un KDF
static inline void kdf_derive(uint64_t shared_secret, uint8_t *output_key, size_t key_len) {
    uint64_t state[4];

    // inicializar estado con el secreto y constantes
    state[0] = shared_secret;
    state[1] = shared_secret ^ 0x9E3779B97F4A7C15ULL;
    state[2] = shared_secret ^ 0x85EBCA6FC2B2AE35ULL;
    state[3] = shared_secret ^ 0xC6D4D6C9A5F3B2E1ULL;

    // múltiples rondas de mezcla no lineal
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 4; i++) {
            state[i] = (state[i] * 0x9E3779B97F4A7C15ULL) ^ (state[i] >> 31);
            state[i] = (state[i] ^ (state[i] << 17)) * 0x85EBCA6FC2B2AE35ULL;
        }

        // mezclar entre estados
        state[0] ^= state[1] + state[2];
        state[1] ^= state[2] + state[3];
        state[2] ^= state[3] + state[0];
        state[3] ^= state[0] + state[1];
    }

    // extraer bytes de la clave
    for (size_t i = 0; i < key_len; i++) {
        uint64_t word = state[(i / 8) % 4];
        output_key[i] = (word >> ((i % 8) * 8)) & 0xFF;

        // mezcla adicional cada 8 bytes
        if ((i + 1) % 8 == 0) {
            state[0] = (state[0] * 0x9E3779B9) ^ (state[0] >> 17);
        }
    }
}

#endif
