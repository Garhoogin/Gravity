#include <nds.h>

#include "scenes.h"
#include "io.h"
#include "mi.h"

static int g_titleLastTick = 0;

#define VCOUNT            (*(vu16*)0x04000006)
#define BG3HOFS           (*(vs16*)0x0400001C)
#define BG3VOFS           (*(vs16*)0x0400001E)

#define N_PALETTES 32

#define SWIZZLE(ind) ((((ind)&0xF)>>1)|(((ind)>>4)<<6))

static u16 *tintPalettes = NULL;
static int effectY = 0;
static int effectTimes = 0;
static int bgVy = 8 << 8;
static int bgY = -64 << 8;
static int g_fadeFrames = 0;
static int g_fadeTo = 0;

void titleInit() {
	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);
	vramSetBankB(VRAM_B_MAIN_SPRITE);
	vramSetBankD(VRAM_D_SUB_SPRITE);
	lcdMainOnTop();
	
	//hide BGs
	int i = 0;
	for(i = 0; i < 8; i++) bgHide(i);
	((vu16 *) BG_PALETTE)[0] = 0;
	((vu16 *) BG_PALETTE_SUB)[0] = 0;
	
	//setup BGs
	int bg3 = bgInit(3, BgType_Text4bpp, BgSize_T_256x256, 0, 4);
	
	bgSetPriority(bg3, 0);
	BG3VOFS = 64;
	
	oamInit(&oamMain, SpriteMapping_1D_32, FALSE);
	oamInit(&oamSub, SpriteMapping_2D, FALSE);
	tintPalettes = (u16 *) malloc(2 * 128 * (1 + N_PALETTES));
	
	//load graphics, start with palettes
	u32 palSize;
	void *pal = IO_ReadEntireFile("title/gfx/logo_m_b.ncl.bin", &palSize);
	swiWaitForVBlank();
	dmaCopy(pal, BG_PALETTE, palSize);
	free(pal);
	pal = IO_ReadEntireFile("title/gfx/planet_m_o.ncl.bin", &palSize);
	swiWaitForVBlank();
	dmaCopy(pal, SPRITE_PALETTE, palSize);
	dmaCopy(pal, tintPalettes, palSize);
	free(pal);
	pal = IO_ReadEntireFile("title/gfx/buttons_s_o.ncl.bin", &palSize);
	swiWaitForVBlank();
	dmaCopy(pal, SPRITE_PALETTE_SUB, palSize);
	free(pal);
	
	//character
	u32 charSize;
	void *chr = IO_ReadEntireFile("title/gfx/logo_m_b.ncg.bin", &charSize);
	MI_UncompressLZ16(chr, bgGetGfxPtr(bg3));
	free(chr);
	chr = IO_ReadEntireFile("title/gfx/planet_m_o.ncg.bin", &charSize);
	MI_UncompressLZ16(chr, oamGetGfxPtr(&oamMain, 0));
	free(chr);
	chr = IO_ReadEntireFile("title/gfx/buttons_s_o.ncg.bin", &charSize);
	MI_UncompressLZ16(chr, oamGetGfxPtr(&oamSub, 0));
	free(chr);
	
	//screen
	u32 scrSize;
	void *scr = IO_ReadEntireFile("title/gfx/logo_m_b.nsc.bin", &scrSize);
	MI_UncompressLZ16(scr, bgGetMapPtr(bg3));
	free(scr);
	
	
	*(vu16 *) 0x07000000 = (32) | (1 << 13) | (1 << 8) | (1 << 9);
	*(vu16 *) 0x07000002 = (64) | (3 << 14);
	*(vu16 *) 0x07000004 = (0) | (3 << 10);
	
	//apply transformation to palette
	for(i = 0; i < 128; i++){
		u16 c1 = tintPalettes[i];
		int or = c1 & 0x1F;
		int og = (c1 >> 5) & 0x1F;
		int ob = (c1 >> 10) & 0x1F;
		
		int j = 0;
		for(j = 0; j < N_PALETTES; j++){
			//morph the blue channel into the average of red and green
			int rgAvg = (or + og + 1);
			if(or < 2 && og < 2) rgAvg = 0; //hack
			int b = (ob * (N_PALETTES - j - 1) + rgAvg * (j + 1) + (N_PALETTES / 2)) / N_PALETTES;
			int r = or, g = og;
			
			if(r > 31) r = 31;
			if(g > 31) g = 31;
			if(b > 31) b = 31;
			
			u16 c2 = r | (g << 5) | (b << 10);
			tintPalettes[i + 128 * (j + 1)] = c2;
		}
	}
	
	effectY = 0;
	effectTimes = 0;
	g_fadeFrames = 0;
	bgVy = 8 << 8;
	bgY = -64 << 8;
	
	oamSet(&oamSub, 0, 80, 32, 2, 0, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, 0), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
	oamSet(&oamSub, 1, 80 + 32, 32, 2, 0, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(2)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
	oamSet(&oamSub, 2, 80 + 64, 32, 2, 0, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, 0), -1, FALSE, FALSE, TRUE, TRUE, FALSE);
	
	oamSet(&oamSub, 3, 80, 96, 2, 0, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, 0), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
	oamSet(&oamSub, 4, 80 + 32, 96, 2, 0, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(2)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
	oamSet(&oamSub, 5, 80 + 64, 96, 2, 0, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, 0), -1, FALSE, FALSE, TRUE, TRUE, FALSE);
	
	oamSet(&oamSub, 6, 112, 32, 1, 0, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(6)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
	oamSet(&oamSub, 7, 112, 96, 1, 0, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(10)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
	
	oamUpdate(&oamSub);
}

void titleTick(){
	
	int i;
	u32 dummy[2];
	
	//makes performance better on emulator ;)
	if(effectTimes < 2 && !sceneFadingOut()){
		while(DMA_CR(0) & DMA_BUSY);
		for(i = 0; i < 192; i++){
			DMA_DEST(0) = (u32) dummy;
			DMA_SRC(0) = (u32) (dummy + 1);
			DMA_CR(0) = DMA_COPY_HALFWORDS | DMA_START_HBL | 1;
			while(DMA_CR(0) & DMA_BUSY);
		
			//update palette for next scanline
			int line = (VCOUNT & 0xFF) + 1;
			
			//add palette effect
			//in theory a 128-color transfer should take around 7 microseconds, of the allotted 17 microseconds for hblank
			
			if(line >= effectY && line < effectY + N_PALETTES) {
				dmaCopy(tintPalettes + 128 * (line + 1 - effectY), SPRITE_PALETTE, 256);
			} else if(line >= effectY + N_PALETTES && line < effectY + 2 * N_PALETTES) {
				int pal = N_PALETTES - (line - effectY - N_PALETTES + 1);
				dmaCopy(tintPalettes + 128 * pal, SPRITE_PALETTE, 256);
			}
			
		}
		dmaCopy(tintPalettes, SPRITE_PALETTE, 256);
	}
	
	//button fadeout effect
	if(sceneFadingOut()){
		int shiftBy = g_fadeFrames * g_fadeFrames / 2;
		
		int lineStart = g_fadeTo == SCENE_SELECT ? 32 : 96;
		
		while(DMA_CR(0) & DMA_BUSY);
		for(i = 0; i < 128; i++){
			DMA_DEST(0) = (u32) dummy;
			DMA_SRC(0) = (u32) (dummy + 1);
			DMA_CR(0) = DMA_COPY_HALFWORDS | DMA_START_HBL | 1;
			while(DMA_CR(0) & DMA_BUSY);
			
			if(i >= lineStart + 16) break;
			if(i < lineStart - 2) continue;
			
			//shift lines alternating by g_fadeFrames
			int j;
			vu16 *attr1 = (vu16 *) (0x07000402);
			for(j = 0; j < 8; j++){
				int x = *attr1 & 0x1FF;
				
				//start 2 lines before to make sure we're where we want to be.
				if(i == lineStart - 2) x += shiftBy;
				
				//odd lines, shift left 2x, even lines shift right 2x.
				else {
					if(i & 1) x -= 2 * shiftBy;
					else x += 2 * shiftBy;
				}
				
				*attr1 = (*attr1 & 0xFE00) | (x & 0x1FF);
				attr1 += 4;
			}
			
		}
		//reset
		int j;
		vu16 *attr1 = (vu16 *) (0x07000402);
		for(j = 0; j < 8; j++){
			int x = *attr1 & 0x1FF;
			x += shiftBy;
			*attr1 = (*attr1 & 0xFE00) | (x & 0x1FF);
			attr1 += 4;
		}
	}
	
	//effectY += 2;
	if(effectTimes < 2) effectY += 8;
	
	if(effectY > 128) {
		effectY = 0;
		effectTimes++;
	}
	
	if(effectTimes >= 1){
		bgY += bgVy;
		bgVy += 1 << 6;
		
		if(bgY > 64 << 8) {
			bgY = 64 << 8;
			bgVy = -bgVy / 3;
		}
		BG3VOFS = -bgY >> 8;
	}
	
	scanKeys();
	u32 keys = keysDown();
	touchPosition touchPos;
	touchRead(&touchPos);
	
	if(!sceneFadingOut() && !sceneFadingIn()){
		if(keys & KEY_TOUCH){
			if(touchPos.px >= 80 && touchPos.py >= 32 && touchPos.px < 80 + 64 + 32 && touchPos.py < 48){
				sceneFadeOut();
				g_fadeTo = SCENE_SELECT;
			}
			if(touchPos.px >= 80 && touchPos.py >= 96 && touchPos.px < 80 + 64 + 32 && touchPos.py < 96 + 16){
				sceneFadeOut();
				g_fadeTo = SCENE_RULES;
			}
		}
	}
	
	if(sceneFadingOut()){
		if(g_fadeFrames >= 16) {
			free(tintPalettes);
			g_scene = g_fadeTo;
		}
	}
}

void titleTickProc(){
	if(g_titleLastTick + 1 != g_frameCount){
		sceneFadeIn();
		titleInit();
	}
	titleTick();
	
	if(sceneFadingOut()) g_fadeFrames++;
	
	g_titleLastTick = g_frameCount;
}