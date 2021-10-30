#pragma once

//compression functions

u32 MI_GetUncompressedSize(const void *srcp);

void MI_UncompressLZ16(const void *srcp, const void *destp);

void MI_UncompressLZ8(const void *srcp, const void *destp);

//DMA functions

void MI_DmaClear16(u32 dmaNo, void *dest, u32 size);
void MI_DmaClear32(u32 dmaNo, void *dest, u32 size);

void MI_DmaFill16(u32 dmaNo, void *dest, u32 data, u32 size);
void MI_DmaFill32(u32 dmaNo, void *dest, u32 data, u32 size);

void MI_WaitDma(u32 dmaNo);