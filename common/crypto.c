#include "crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <wincrypt.h>
#else
    #include <sys/random.h>
#endif

// encripción / desencripción XOR (ya que es simétrico)
void xor_crypt(uint8_t *data, size_t len, const uint8_t *key) {
    for (size_t i = 0; i < len; i++) {
        data[i] ^= key[i % KEY_LENGTH];
    }
}

uint64_t generate_private_key(void) {
    uint64_t key = 0;

#ifdef _WIN32
    HCRYPTPROV hCryptProv;
    if (CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hCryptProv, sizeof(key), (BYTE*)&key);
        CryptReleaseContext(hCryptProv, 0);
    }
#else
    if (getrandom(&key, sizeof(key), 0) != sizeof(key)) {
        // fallback solo si getrandom falla
        FILE *f = fopen("/dev/urandom", "rb");
        if (f) {
            fread(&key, sizeof(key), 1, f);
            fclose(f);
        }
    }
#endif

    // asegurarse de que la clave está en rango válido (2 <= key <= prime-2)
    key = key % (DH_PRIME - 3) + 2;
    return key;
}

uint64_t mod_pow(uint64_t base, uint64_t exp, uint64_t mod) {
    uint64_t result = 1;
    base %= mod;

    while (exp > 0) {
        if (exp & 1) {
            result = (__uint128_t)result * base % mod;
        }
        base = (__uint128_t)base * base % mod;
        exp >>= 1;
    }
    return result;
}

uint64_t compute_public_key(uint64_t private_key) {
    return mod_pow(DH_GENERATOR, private_key, DH_PRIME);
}

uint64_t compute_shared_secret(uint64_t other_public, uint64_t private_key) {
    return mod_pow(other_public, private_key, DH_PRIME);
}
