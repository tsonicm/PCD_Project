#include "des.h"
#include <stdlib.h>
#include <string.h>

void genSubKeys(uint8_t *key, keyStruct *subKeys) {
    int i, j, shift;
    uint8_t shiftByte, _1Shift, _2Shift, _3Shift, _4Shift;

    for(i = 0; i < 8; i++) subKeys[0].k[i] = 0;

    for (i = 0; i < 56; i++) {
        shift = initKeyPerm[i];
        shiftByte = 0x80 >> ((shift - 1) % 8);
        shiftByte &= key[(shift - 1) / 8];
        shiftByte <<= ((shift - 1) % 8);

        subKeys[0].k[i / 8] |= (shiftByte >> i % 8);
    }

    for(i = 0; i < 3; i++) subKeys[0].c[i] = subKeys[0].k[i];

    subKeys[0].c[3] = subKeys[0].k[3] & 0xF0;

    for(i = 0; i < 3; i++) {
        subKeys[0].d[i] = (subKeys[0].k[i + 3] & 0x0F) << 4;
        subKeys[0].d[i] |= (subKeys[0].k[i + 4] & 0xF0) >> 4;
    }

    subKeys[0].d[3] = (subKeys[0].k[6] & 0x0F) << 4;

    for(i = 1; i < 17; i++) {
        for(j = 0; j < 4; j++) {
            subKeys[i].c[j] = subKeys[i - 1].c[j];
            subKeys[i].d[j] = subKeys[i - 1].d[j];
        }

        shift = keyShiftSize[i];
        if (shift == 1) shiftByte = 0x80;
        else shiftByte = 0xC0;

        _1Shift = shiftByte & subKeys[i].c[0];
        _2Shift = shiftByte & subKeys[i].c[1];
        _3Shift = shiftByte & subKeys[i].c[2];
        _4Shift = shiftByte & subKeys[i].c[3];

        // Process C (left half)
        subKeys[i].c[0] <<= shift;
        subKeys[i].c[0] |= (_2Shift >> (8 - shift));

        subKeys[i].c[1] <<= shift;
        subKeys[i].c[1] |= (_3Shift >> (8 - shift));

        subKeys[i].c[2] <<= shift;
        subKeys[i].c[2] |= (_4Shift >> (8 - shift));

        subKeys[i].c[3] <<= shift;
        subKeys[i].c[3] |= (_1Shift >> (4 - shift));

        // Process D (right half)
        _1Shift = shiftByte & subKeys[i].d[0];
        _2Shift = shiftByte & subKeys[i].d[1];
        _3Shift = shiftByte & subKeys[i].d[2];
        _4Shift = shiftByte & subKeys[i].d[3];

        subKeys[i].d[0] <<= shift;
        subKeys[i].d[0] |= (_2Shift >> (8 - shift));

        subKeys[i].d[1] <<= shift;
        subKeys[i].d[1] |= (_3Shift >> (8 - shift));

        subKeys[i].d[2] <<= shift;
        subKeys[i].d[2] |= (_4Shift >> (8 - shift));

        subKeys[i].d[3] <<= shift;
        subKeys[i].d[3] |= (_1Shift >> (4 - shift));

        for (j = 0; j < 48; j++) {
            shift = subKeyPerm[j];
            if (shift <= 28) {
                shiftByte = 0x80 >> ((shift - 1) % 8);
                shiftByte &= subKeys[i].c[(shift - 1) / 8];
                shiftByte <<= ((shift - 1) % 8);
            } else {
                shiftByte = 0x80 >> ((shift - 29) % 8);
                shiftByte &= subKeys[i].d[(shift - 29) / 8];
                shiftByte <<= ((shift - 29) % 8);
            }

            subKeys[i].k[j / 8] |= (shiftByte >> j % 8);
        }
    }
}

void des(uint8_t *msg, uint8_t *output, keyStruct *keys, bool mode) {
    int i, k;
    int shift;
    uint8_t shiftByte, initPerm[8];

    memset(initPerm, 0, 8);
    memset(output, 0, 8);

    for (i = 0; i < 64; i++) {
        shift = initMsgPerm[i];
        shiftByte = 0x80 >> ((shift - 1) % 8);
        shiftByte &= msg[(shift - 1) / 8];
        shiftByte <<= ((shift - 1) % 8);

        initPerm[i / 8] |= (shiftByte >> i % 8);
    }

    // Split the message into two halves
    uint8_t LeftHalf[4], RightHalf[4];
    for (i = 0; i < 4; i++) {
        LeftHalf[i] = initPerm[i];
        RightHalf[i] = initPerm[i + 4];
    }

    uint8_t leftHalf[4], rightHalf[4], expandedR[6], sboxExpR[4];

    int keyIndex;
    for(k = 1; k <= 16; k++) {
        memcpy(leftHalf, RightHalf, 4);
        memset(expandedR, 0, 6);

        for(i = 0; i < 48; i++) {
            shift = msgExp[i];
            shiftByte = 0x80 >> ((shift - 1) % 8);
            shiftByte &= RightHalf[(shift - 1) / 8];
            shiftByte <<= ((shift - 1) % 8);

            expandedR[i / 8] |= (shiftByte >> i % 8);
        }

        if(mode) keyIndex = k;
        else keyIndex = 17 - k;

        for(i = 0; i < 6; i++) expandedR[i] ^= keys[keyIndex].k[i];

        uint8_t row, col;

        for(i = 0; i < 4; i++) sboxExpR[i] = 0;

        // 0000 0000 0000 0000 0000 0000
        // rccc crrc cccr rccc crrc cccr

        // Byte 1
        row = 0;
        row |= ((expandedR[0] & 0x80) >> 6);
        row |= ((expandedR[0] & 0x04) >> 2);

        col = 0;
        col |= ((expandedR[0] & 0x78) >> 3);

        sboxExpR[0] |= ((uint8_t)Sbox1[row * 16 + col] << 4);

        row = 0;
        row |= (expandedR[0] & 0x02);
        row |= ((expandedR[1] & 0x10) >> 4);

        col = 0;
        col |= ((expandedR[0] & 0x01) << 3);
        col |= ((expandedR[1] & 0xE0) >> 5);

        sboxExpR[0] |= (uint8_t)Sbox2[row * 16 + col];

        // Byte 2
        row = 0;
        row |= ((expandedR[1] & 0x08) >> 2);
        row |= ((expandedR[2] & 0x40) >> 6);

        col = 0;
        col |= ((expandedR[1] & 0x07) << 1);
        col |= ((expandedR[2] & 0x80) >> 7);

        sboxExpR[1] |= ((uint8_t)Sbox3[row * 16 + col] << 4);

        row = 0;
        row |= ((expandedR[2] & 0x20) >> 4);
        row |= (expandedR[2] & 0x01);

        col = 0;
        col |= ((expandedR[2] & 0x1E) >> 1);

        sboxExpR[1] |= (uint8_t)Sbox4[row * 16 + col];

        // Byte 3
        row = 0;
        row |= ((expandedR[3] & 0x80) >> 6);
        row |= ((expandedR[3] & 0x04) >> 2);

        col = 0;
        col |= ((expandedR[3] & 0x78) >> 3);

        sboxExpR[2] |= ((uint8_t)Sbox5[row * 16 + col] << 4);

        row = 0;
        row |= (expandedR[3] & 0x02);
        row |= ((expandedR[4] & 0x10) >> 4);

        col = 0;
        col |= ((expandedR[3] & 0x01) << 3);
        col |= ((expandedR[4] & 0xE0) >> 5);

        sboxExpR[2] |= (uint8_t)Sbox6[row * 16 + col];

        // Byte 4
        row = 0;
        row |= ((expandedR[4] & 0x08) >> 2);
        row |= ((expandedR[5] & 0x40) >> 6);

        col = 0;
        col |= ((expandedR[4] & 0x07) << 1);
        col |= ((expandedR[5] & 0x80) >> 7);

        sboxExpR[3] |= ((uint8_t)Sbox7[row * 16 + col] << 4);

        row = 0;
        row |= ((expandedR[5] & 0x20) >> 4);
        row |= (expandedR[5] & 0x01);

        col = 0;
        col |= ((expandedR[5] & 0x1E) >> 1);

        sboxExpR[3] |= (uint8_t)Sbox8[row * 16 + col];

        for (i = 0; i < 4; i++) rightHalf[i] = 0;

        for (i = 0; i < 32; i++) {
            shift = rightSubMsgPerm[i];
            shiftByte = 0x80 >> ((shift - 1) % 8);
            shiftByte &= sboxExpR[(shift - 1) / 8];
            shiftByte <<= ((shift - 1) % 8);

            rightHalf[i / 8] |= (shiftByte >> i % 8);
        }

        for (i = 0; i < 4; i++) rightHalf[i] ^= LeftHalf[i];

        for (i = 0; i < 4; i++) {
            LeftHalf[i] = leftHalf[i];
            RightHalf[i] = rightHalf[i];
        }
    }

    uint8_t preFinal[8];
    for (i = 0; i < 4; i++) {
        preFinal[i] = RightHalf[i];
        preFinal[i + 4] = LeftHalf[i];
    }

    for (i = 0; i < 64; i++) {
        shift = finalMsgPerm[i];
        shiftByte = 0x80 >> ((shift - 1) % 8);
        shiftByte &= preFinal[(shift - 1) / 8];
        shiftByte <<= ((shift - 1) % 8);

        output[i / 8] |= (shiftByte >> i % 8);
    }
}
