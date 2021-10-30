#include <nds.h>

#include "mi.h"

u32 MI_GetUncompressedSize(const void *srcp){
    u32 size = 0;
    if (srcp != NULL && (*(u8 *) srcp) != 0) {
        size = *(u32 *) srcp >> 8;
    }
    return size;
}

void MI_UncompressLZ8(const void *srcp, const void *destp){
    int remainingUncompressed = (int) (*(u32 *) srcp >> 8);
    u8 *currentSection = (u8 *) srcp + 4;
    do {
        if (remainingUncompressed < 1) {
            return;
        }
        u8 head = *currentSection;
        u8 *pos = currentSection + 1;
        int chunksLeft = 8;
        while (currentSection = pos, chunksLeft > 0) {
			int bytesAdded;
            if ((head & 0x80) == 0) {
                currentSection = pos + 1;
                *(u8 *) destp = *pos;
                destp = (u8 *) destp + 1;
                bytesAdded = -1;
            } else {
                int runLength = (*pos >> 4) + 3;
                u8 high = pos[0];
                u8 low = pos[1];
                currentSection = pos + 2;
                bytesAdded = -runLength;
                do {
                    *(u8 *) destp =
                         ((u8 *) destp)[-(((u32) low | ((u32) high & 0xF) << 8) + 1)];
                    destp = (u8 *) destp + 1;
                    runLength--;
                } while (runLength != 0 && runLength >= 0);
            }
            remainingUncompressed = remainingUncompressed + bytesAdded;
            if (remainingUncompressed < 1) break;
            head <<= 1;
            pos = currentSection;
            chunksLeft--;
        }
    } while(1);
}

void MI_UncompressLZ16(const void *srcp, const void *destp){
    u32 tempBuffer = 0;
    int uncompressedRemaining = (int) (*(u32 *) srcp >> 8);
    u32 bufferShift = 0;
    u8 *currentSection = (u8 *) srcp + 4;
    do {
        if (uncompressedRemaining < 1) {
            return;
        }
        u8 head = *currentSection;
        u8 *pos = currentSection + 1;
        u16 *outPos = (u16 *) destp;
        int chunksLeft = 8;
        while (currentSection = pos, destp = outPos, chunksLeft > 0) {
			int bytesAdded;
            if ((head & 0x80) == 0) {
                currentSection = pos + 1;
                tempBuffer |= *pos << bufferShift;
                bytesAdded = -1;
                bufferShift ^= 8;
                if (bufferShift == 0) {
                    destp = outPos + 1;
                    *outPos = (u16) tempBuffer;
                    tempBuffer = 0;
                }
            } else {
                int runLength = (*pos >> 4) + 3;
                currentSection = pos + 2;
                u32 uVar7 = (pos[1] | ((*pos & 0xF) << 8)) + 1;
                u32 uVar11 = 8 - bufferShift ^ (uVar7 & 1) << 3;
                bytesAdded = -runLength;
                do {
                    uVar11 ^= 8;
                    tempBuffer |= ((int)((u32) *(u16 *)
                                                  ((int) outPos -
                                                  (uVar7 + (8 - bufferShift >> 3) & 0xFFFFFFFE)) &
                                          0xFF << (uVar11 & 0xff)) >> (uVar11 & 0xFF)) << bufferShift;
                    bufferShift ^= 8;
                    if (bufferShift == 0) {
                        *outPos = (u16) tempBuffer;
                        tempBuffer = 0;
                        outPos++;
                    }
                    destp = outPos;
                    runLength--;
                } while (runLength != 0 && runLength >= 0);
            }
            uncompressedRemaining = uncompressedRemaining + bytesAdded;
            if (uncompressedRemaining < 1) break;
            head <<= 1;
            pos = currentSection;
            outPos = (u16 *) destp;
            chunksLeft--;
        }
    } while(1);
}