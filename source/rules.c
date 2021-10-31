#include <nds.h>

#include "scenes.h"
#include "io.h"
#include "mi.h"

#define VCOUNT            (*(vu16*)0x04000006)

static int g_fadeFrames = 0;
static int g_rulesLastTick = 0;
static int g_effectY = -32;

void rulesInit(){
	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);
	vramSetBankB(VRAM_B_MAIN_SPRITE);
	vramSetBankD(VRAM_D_SUB_SPRITE);
	lcdMainOnTop();
	
	//setup BGs
	int bg0 = bgInit(0, BgType_Text4bpp, BgSize_T_256x256, 0, 4);
	int bg0s = bgInitSub(0, BgType_Text4bpp, BgSize_T_256x256, 0, 4);
	int bg1s = bgInitSub(1, BgType_Text4bpp, BgSize_T_256x256, 1, 6);
	
	oamInit(&oamSub, SpriteMapping_1D_32, FALSE);
	
	//load graphics, start with palettes
	u32 mainBgPalSize, subBgPalSize, subObjPalSize;
	void *mainBgPal = IO_ReadEntireFile("rules/gfx/rules_m_b.ncl.bin", &mainBgPalSize);
	void *subBgPal = IO_ReadEntireFile("select/gfx/select_s_b.ncl.bin", &subBgPalSize);
	void *subObjPal = IO_ReadEntireFile("rules/gfx/back_s_o.ncl.bin", &subObjPalSize);
	DC_FlushRange(mainBgPal, mainBgPalSize);
	DC_FlushRange(subBgPal, subBgPalSize);
	DC_FlushRange(subObjPal, subObjPalSize);
	swiWaitForVBlank();
	dmaCopy(mainBgPal, BG_PALETTE, mainBgPalSize);
	dmaCopy(subBgPal, BG_PALETTE_SUB, subBgPalSize);
	dmaCopy(subObjPal, SPRITE_PALETTE_SUB, subObjPalSize);
	free(mainBgPal);
	free(subBgPal);
	free(subObjPal);
	
	//character
	u32 charSize;
	void *chr = IO_ReadEntireFile("rules/gfx/rules_m_b.ncg.bin", &charSize);
	MI_UncompressLZ16(chr, bgGetGfxPtr(bg0));
	free(chr);
	chr = IO_ReadEntireFile("select/gfx/select_s_b.ncg.bin", &charSize);
	MI_UncompressLZ16(chr, bgGetGfxPtr(bg1s));
	free(chr);
	chr = IO_ReadEntireFile("rules/gfx/rules_s_b.ncg.bin", &charSize);
	MI_UncompressLZ16(chr, bgGetGfxPtr(bg0s));
	free(chr);
	chr = IO_ReadEntireFile("rules/gfx/back_s_o.ncg.bin", &charSize);
	MI_UncompressLZ16(chr, oamGetGfxPtr(&oamSub, 0));
	free(chr);
	
	//screen
	u32 scrSize;
	void *scr = IO_ReadEntireFile("rules/gfx/rules_m_b.nsc.bin", &scrSize);
	MI_UncompressLZ16(scr, bgGetMapPtr(bg0));
	free(scr);
	scr = IO_ReadEntireFile("select/gfx/select_s_b.nsc.bin", &scrSize);
	MI_UncompressLZ16(scr, bgGetMapPtr(bg1s));
	free(scr);
	scr = IO_ReadEntireFile("rules/gfx/rules_s_b.nsc.bin", &scrSize);
	MI_UncompressLZ16(scr, bgGetMapPtr(bg0s));
	free(scr);
	
	oamSet(&oamSub, 0, 5, 171, 0, 0, SpriteSize_16x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, 0), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
	oamUpdate(&oamSub);
	
	g_effectY = -32;
	g_fadeFrames = 0;
}

void rulesTick(){
	
	//check input
	scanKeys();
	u32 keys = keysDown();
	touchPosition touchPos;
	touchRead(&touchPos);
	
	if(!sceneFadingOut() && !sceneFadingIn()){
		if(keys & KEY_TOUCH){
			if(touchPos.px >= 5 && touchPos.px < 5 + 16 && touchPos.py >= 171 && touchPos.py < 171 + 16){
				oamSet(&oamSub, 0, 5, 171, 0, 1, SpriteSize_16x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, 0), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
				oamUpdate(&oamSub);
				
				sceneFadeOut();
			}
		} else if(keys & KEY_B){
			oamSet(&oamSub, 0, 5, 171, 0, 1, SpriteSize_16x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, 0), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
			oamUpdate(&oamSub);
		
			sceneFadeOut();
		}
	}
	
	while(VCOUNT & 0xFF);
	
	//HDMA color shift effect
	u32 dummy[2];
	int i;
	while(DMA_CR(0) & DMA_BUSY);
	for(i = 0; i < 192; i++){
		DMA_DEST(0) = (u32) dummy;
		DMA_SRC(0) = (u32) (dummy + 1);
		DMA_CR(0) = DMA_COPY_HALFWORDS | DMA_START_HBL | 1;
		while(DMA_CR(0) & DMA_BUSY);
	
		//update palette for next scanline
		int line = (VCOUNT & 0xFF) + 1;
		
		if(line >= g_effectY && line < g_effectY + 32){
			//from RGB(31, 31, 31) -> RGB(15, 31, 31) -> RGB(31, 31, 31)
			u16 clr = 0x7FFF & ~(0x1F);
			
			int strength = line - (g_effectY + 16);
			if(strength < 0) strength = -strength; //0-15
			strength = 15 - strength;
			
			int r = (0 /*15*/ * strength + 31 * (15 - strength)) / 15;
			if(r > 31) r= 31;
			clr |= r;
			
			((vu16 *) BG_PALETTE_SUB)[15 + 8 * 16] = clr;
			((vu16 *) SPRITE_PALETTE_SUB)[15] = clr;
		} else {
			((vu16 *) BG_PALETTE_SUB)[15 + 8 * 16] = 0x7FFF;
			((vu16 *) SPRITE_PALETTE_SUB)[15] = 0x7FFF;
		}
	}
	
	((vu16 *) BG_PALETTE_SUB)[15 + 8 * 16] = 0x7FFF;
	((vu16 *) SPRITE_PALETTE_SUB)[15] = 0x7FFF;
	
	g_effectY++;
	if(g_effectY > 187) g_effectY = -27;
	
	if(g_fadeFrames >= 16){
		g_scene = SCENE_TITLE;
	}
}

void rulesTickProc() {
	if(g_rulesLastTick + 1 != g_frameCount){
		sceneFadeIn();
		rulesInit();
	}
	rulesTick();
	
	if(sceneFadingOut()) g_fadeFrames++;
	
	g_rulesLastTick = g_frameCount;
}