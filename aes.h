#ifndef AES_H
#define AES_H

#include <stdint.h>

#define AES256 1
// #define AES192 1
// #define AES128 1

#define AES_BLOCKLEN 16 // AES operates on 16 bytes at a time

#if defined(AES256) && (AES256 == 1)
    #define AES_KEYLEN 32
    #define AES_KEY_EXP_SIZE 240
#elif defined(AES192) && (AES192 == 1)
    #define AES_KEYLEN 24
    #define AES_KEY_EXP_SIZE 208
#else
    #define AES_KEYLEN 16
    #define AES_KEY_EXP_SIZE 176
#endif

struct aes_ctx {
    uint8_t roundKey[AES_KEY_EXP_SIZE]; // AES-256 needs 240 bytes of round keys (15 rounds)
};

void aes_init(struct aes_ctx *ctx, const uint8_t *key);

void aes_encrypt(const struct aes_ctx *ctx, uint8_t *buffer);
void aes_decrypt(const struct aes_ctx *ctx, uint8_t *buffer);

#endif
