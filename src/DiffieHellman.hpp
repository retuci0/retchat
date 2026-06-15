#pragma once

#include <openssl/bn.h>
#include <cstdint>


namespace Retchat {

    class DH {
    public:
        static void init();
        static void free();
        static void generatePrivateKey(BIGNUM* priv);
        static void computePublicKey(const BIGNUM* priv, BIGNUM* pub);
        static void computeSharedSecret(const BIGNUM* peerPub, const BIGNUM* priv, BIGNUM* secret);
        static void deriveEncKey(const BIGNUM* sharedSecret, uint8_t outKey[32]);
        static void deriveKeystream(uint8_t* keystream, size_t len, const uint8_t* baseKey, uint64_t counter);
        static void xorCrypt(uint8_t* data, size_t len, const uint8_t* key, uint64_t counter);
    };

}