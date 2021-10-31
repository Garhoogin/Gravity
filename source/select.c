#include <nds.h>

#include "scenes.h"
#include "io.h"
#include "mi.h"

static int g_selectLastTick = 0;
static int g_scroll = 0; //in pixels
static int g_focus = 0;
static int g_fadeStart = 0;
static int g_fadingOut = 0;

#define NLEVELS    10

#define SWIZZLE(ind) ((((ind)&0xF)>>1)|(((ind)>>4)<<6))

void sceneInit(){
	sceneFadeIn();
	
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
	bgSetPriority(bg0s, 3);
	
	//setup OAM
	oamInit(&oamSub, SpriteMapping_2D, FALSE);
	
	//load graphics, start with palettes
	u32 mainBgPalSize, subBgPalSize, subObjPalSize;
	void *mainBgPal = IO_ReadEntireFile("select/gfx/select_m_b.ncl.bin", &mainBgPalSize);
	void *subBgPal = IO_ReadEntireFile("select/gfx/select_s_b.ncl.bin", &subBgPalSize);
	void *subObjPal = IO_ReadEntireFile("select/gfx/select_text_s_o.ncl.bin", &subObjPalSize);
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
	void *chr = IO_ReadEntireFile("select/gfx/select_m_b.ncg.bin", &charSize);
	MI_UncompressLZ16(chr, bgGetGfxPtr(bg0));
	free(chr);
	chr = IO_ReadEntireFile("select/gfx/select_s_b.ncg.bin", &charSize);
	MI_UncompressLZ16(chr, bgGetGfxPtr(bg0s));
	free(chr);
	chr = IO_ReadEntireFile("select/gfx/select_text_s_o.ncg.bin", &charSize);
	MI_UncompressLZ16(chr, oamGetGfxPtr(&oamSub, 0));
	free(chr);
	
	//screen
	u32 scrSize;
	void *scr = IO_ReadEntireFile("select/gfx/select_m_b.nsc.bin", &scrSize);
	MI_UncompressLZ16(scr, bgGetMapPtr(bg0));
	free(scr);
	scr = IO_ReadEntireFile("select/gfx/select_s_b.nsc.bin", &scrSize);
	MI_UncompressLZ16(scr, bgGetMapPtr(bg0s));
	free(scr);
	
	//setup globals
	g_scroll = 0;
	g_focus = -1;
	g_fadingOut = 0;
	
	//based on g_levelId (last played level), set the vertical scroll.
	int selY = g_levelId * 24 + 16;
	int listHeight = 24 + 24 * NLEVELS;
	if(selY > 96) {
		g_scroll = selY - 96;
	}
	if(g_scroll > listHeight - 192){
		g_scroll = listHeight - 192;
	}
	g_focus = g_levelId;
}

void sceneTick() {
	//draw level options
	
	int vOfs = -g_scroll;
	int oamOffset = 0;
	int i;
	oamClear(&oamSub, 0, 0);
	for(i = 0; i < NLEVELS; i++){
		int y = i * 24 + 16;
		if(y + vOfs < -16) continue;
		if(y + vOfs >= 192) break;
		
		int pal = (i == g_focus) ? 1 : 0;
		oamSet(&oamSub, oamOffset, 16, y + vOfs, 1, pal, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(0)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
		oamSet(&oamSub, oamOffset + 1, 48, y + vOfs, 1, pal, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(2)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
		oamSet(&oamSub, oamOffset + 2, 80, y + vOfs, 1, pal, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(2)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
		oamSet(&oamSub, oamOffset + 3, 112, y + vOfs, 1, pal, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(4)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
		oamOffset += 4;
	}
	
	//draw text
	for(i = 0; i < NLEVELS; i++){
		int y = i * 24 + 16;
		if(y + vOfs < -13) continue;
		if(y + vOfs >= 188) break;
		
		int pal = (i == g_focus) ? 1 : 0;
		oamSet(&oamSub, oamOffset, 16 + 4, y + vOfs, 0, pal, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(64)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
		
		if(i < 9) {
			int ofs = 68 + 2 * (i + 1);
			oamSet(&oamSub, oamOffset + 1, 60, y + vOfs, 0, pal, SpriteSize_16x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(ofs)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
			oamOffset++;
		} else {
			int lowDigit = (i + 1) % 10;
			int highDigit = (i + 1) / 10;
			int ofs1 = 68 + 2 * highDigit;
			int ofs2 = 68 + 2 * lowDigit;
			oamSet(&oamSub, oamOffset + 1, 60, y + vOfs, 0, pal, SpriteSize_16x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(ofs1)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
			oamSet(&oamSub, oamOffset + 2, 66, y + vOfs, 0, pal, SpriteSize_16x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(ofs2)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
			oamOffset += 2;
		}
		
		oamOffset++;
	}
	
	//scan keys
	scanKeys();
	u32 keys = keysCurrent();
	
	int listHeight = 24 + 24 * NLEVELS;
	if(keys & KEY_DOWN){
		g_scroll += 2;
		if(g_scroll > listHeight - 192) g_scroll = listHeight - 192;
	}
	if(keys & KEY_UP){
		g_scroll -= 2;
		if(g_scroll < 0) g_scroll = 0;
	}
	
	//read touch position
	touchPosition touchPos;
	touchRead(&touchPos);
	
	if(!sceneFadingOut() && !sceneFadingIn()){
		if(keys & KEY_TOUCH){
			if(touchPos.px >= 16 && touchPos.px < (16 + 32 + 32 + 32 + 32)){
				int y = touchPos.py + g_scroll;
				if(y >= 12 && y < listHeight - 12){
					int sel = (y - 12) / 24;
					g_focus = sel;
					g_fadeStart = g_frameCount;
					g_fadingOut = 1;
					g_levelId = sel;
					sceneFadeOut();
				}
			}
		}
	}
	
	oamUpdate(&oamSub);
}

void selectTickProc() {
	if(g_selectLastTick + 1 != g_frameCount){
		sceneInit();
	}
	
	if(g_fadingOut && (g_frameCount - g_fadeStart) >= 16){
		g_scene = SCENE_GAME;
	}
	
	sceneTick();
	
	g_selectLastTick = g_frameCount;
}