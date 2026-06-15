#include "DiffieHellman.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <openssl/hmac.h>
#include <openssl/sha.h>


static BIGNUM* dh_prime = nullptr;
static BIGNUM* dh_generator = nullptr;

void Retchat::DH::init() {
    const char* prime_hex =
        "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
        "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
        "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
        "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
        "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
        "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
        "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
        "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
        "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
        "DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
        "15728E5A8AACAA68FFFFFFFFFFFFFFFF";
    dh_prime = BN_new();
    if (!dh_prime || BN_hex2bn(&dh_prime, prime_hex) == 0) {
        Logger::error("invalid DH prime");
        exit(1);
    }
    dh_generator = BN_new();
    BN_set_word(dh_generator, 2);
}

void Retchat::DH::free() {
    BN_free(dh_prime);
    BN_free(dh_generator);
}

void Retchat::DH::generatePrivateKey(BIGNUM* priv) {
    do { BN_rand(priv, 256, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY); }
    while (BN_is_zero(priv));
}

void Retchat::DH::computePublicKey(const BIGNUM* priv, BIGNUM* pub) {
    BN_CTX* ctx = BN_CTX_new();
    BN_mod_exp(pub, dh_generator, priv, dh_prime, ctx);
    BN_CTX_free(ctx);
}

void Retchat::DH::computeSharedSecret(const BIGNUM* peerPub, const BIGNUM* priv, BIGNUM* secret) {
    BN_CTX* ctx = BN_CTX_new();
    BN_mod_exp(secret, peerPub, priv, dh_prime, ctx);
    BN_CTX_free(ctx);
}

void Retchat::DH::deriveEncKey(const BIGNUM* sharedSecret, uint8_t outKey[32]) {
    int len = BN_num_bytes(sharedSecret);
    uint8_t* buf = new uint8_t[len];
    BN_bn2bin(sharedSecret, buf);
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, buf, len);
    SHA256_Final(outKey, &ctx);
    delete[] buf;
}

void Retchat::DH::deriveKeystream(uint8_t* keystream, size_t len, const uint8_t* baseKey, uint64_t counter) {
    uint8_t counterBytes[8];
    for (int i = 0; i < 8; i++) counterBytes[i] = (counter >> (i * 8)) & 0xFF;
    uint8_t digest[32];
    unsigned int digestLen;
    HMAC_CTX* hmac = HMAC_CTX_new();
    HMAC_Init_ex(hmac, baseKey, 32, EVP_sha256(), nullptr);
    HMAC_Update(hmac, counterBytes, 8);
    HMAC_Final(hmac, digest, &digestLen);
    HMAC_CTX_free(hmac);
    size_t copied = 0;
    while (copied < len) {
        size_t chunk = std::min(len - copied, size_t(32));
        memcpy(keystream + copied, digest, chunk);
        copied += chunk;
        if (copied < len) {
            HMAC_CTX* hmac2 = HMAC_CTX_new();
            HMAC_Init_ex(hmac2, baseKey, 32, EVP_sha256(), nullptr);
            HMAC_Update(hmac2, digest, 32);
            HMAC_Final(hmac2, digest, &digestLen);
            HMAC_CTX_free(hmac2);
        }
    }
}

void Retchat::DH::xorCrypt(uint8_t* data, size_t len, const uint8_t* key, uint64_t counter) {
    uint8_t keystream[4096]; // MAX_MSG_LEN
    deriveKeystream(keystream, len, key, counter);
    for (size_t i = 0; i < len; i++) data[i] ^= keystream[i];
}