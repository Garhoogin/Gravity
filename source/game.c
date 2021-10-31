#include <nds.h>

#include "level.h"
#include "scenes.h"
#include "mi.h"
#include "io.h"

static int g_gameLastTick = 0;

//static int textureIds[2];
static int *textureIds = NULL;

#define MODE_PLAY     1 /* mode for physics in action*/
#define MODE_LOSE     2 /* mode for losing */
#define MODE_WIN      3 /* mode for winning */
#define MODE_FADEIN   4 /* mode for screen fade-in */
#define MODE_FADEOUT  5 /* mode for screen fade-out */
#define MODE_PAUSE    6 /* mode for pausing */

#define NMASS_MAX     64

#define SWIZZLE(ind) ((((ind)&0xF)>>1)|(((ind)>>4)<<6))

static MASS g_planets[NMASS_MAX];
static MASS g_ball;
static int g_nPlanets = 0;
static int g_mode = MODE_PLAY;
static int g_modeFrame = 0;
static int g_outOfBounds = FALSE;
static int g_outOfBoundsFrames = 0;
static int g_selection = 0;
static int g_fadingOut = 0;
static int g_fadeTo = SCENE_SELECT;
static int g_fadeFrame = 0;

static LEVEL *g_currentLevel = NULL;

//adds a mass to the global list of masses.
void addMass(MASS *mass){
	//dmaCopy(mass, g_planets + g_nPlanets, sizeof(MASS));
	memcpy(g_planets + g_nPlanets, mass, sizeof(MASS));
	g_nPlanets++;
}

void clearMasses(){
	g_nPlanets = 0;
}

void accelerate(MASS *movingObject) {
	int i = 0;
	for(i = 0; i < g_nPlanets; i++){
		MASS *p = g_planets + i;
		
		s32 dx = p->x - movingObject->x;
		s32 dy = p->y - movingObject->y;
		s32 r2 = (dx >> 6) * (dx >> 6) + (dy >> 6) * (dy >> 6);
		s32 r = swiSqrt(r2);
		s32 a = 16 * (p->m << 12) / (r2 + 1);
		dx = (dx << 3) / r;
		dy = (dy << 3) / r;
		
		dx = (dx * a) >> 9;
		dy = (dy * a) >> 9;
		movingObject->vx += dx;
		movingObject->vy += dy;
	}
}

void snap(MASS *movingObject, int planetIndex) {
	MASS *p = g_planets + planetIndex;
		
	s32 dx = p->x - movingObject->x;
	s32 dy = p->y - movingObject->y;
	s32 r2 = (dx >> 6) * (dx >> 6) + (dy >> 6) * (dy >> 6);
	s32 r = swiSqrt(r2);
	s32 sumR = p->r + movingObject->r;
	dx = (dx << 3) / r;
	dy = (dy << 3) / r;
	
	dx = (dx * sumR) >> 9;
	dy = (dy * sumR) >> 9;
	movingObject->x = p->x - dx;
	movingObject->y = p->y - dy;
}

//return the index of the mass that the ball is colliding with.
int ballIsCollidingWith(){
	int i;
	for(i = 0; i < g_nPlanets; i++){
		MASS *mass = g_planets + i;
		s32 dx = mass->x - g_ball.x;
		s32 dy = mass->y - g_ball.y;
		s32 d2 = (dx >> 6) * (dx >> 6) + (dy >> 6) * (dy >> 6);
		
		//if(d2 >= (ball.r + mass->r)^2)
		s32 minR = g_ball.r + mass->r;
		s32 minR2 = (minR >> 6) * (minR >> 6);
		
		if(d2 < minR2){
			return i;
		}
	}
	return -1;
}

void gameInit(){
	videoSetMode(MODE_0_3D);
	videoSetModeSub(MODE_0_2D);
	lcdMainOnBottom();
	oamInit(&oamMain, SpriteMapping_2D, FALSE);
	oamInit(&oamSub, SpriteMapping_2D, FALSE);
	
	glInit();
	glResetTextures(); //clean up the last textures, if any
	
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ANTIALIAS);
	glEnable(GL_BLEND);
	
	glClearColor(0, 0, 0, 31);
	glClearPolyID(63);
	glClearDepth(0x7FFF);
	glViewport(0, 0, 255, 191);
	
	vramSetBankA(VRAM_A_TEXTURE);
	vramSetBankB(VRAM_B_TEXTURE);
	vramSetBankC(VRAM_C_SUB_BG);
	vramSetBankD(VRAM_D_MAIN_BG_0x06000000);
	vramSetBankE(VRAM_E_MAIN_SPRITE);
	vramSetBankF(VRAM_F_TEX_PALETTE_SLOT0);
	vramSetBankG(VRAM_G_TEX_PALETTE_SLOT5);
	vramSetBankI(VRAM_I_SUB_SPRITE);
	
	//init BG and OAM
	oamInit(&oamSub, SpriteMapping_2D, FALSE);
	int sub0 = bgInitSub(0, BgType_Text4bpp, BgSize_T_256x256, 0, 4);
	int bg1 = bgInit(1, BgType_Text4bpp, BgSize_T_256x256, 0, 4);
	bgHide(bg1);
	bgSetPriority(bg1, 0);
	bgSetPriority(0, 1);
	
	//load graphics, starting with palettes
	u32 palSize = 0;
	void *subBgPal = IO_ReadEntireFile("game/gfx/top_s_b.ncl.bin", &palSize);
	swiWaitForVBlank();
	dmaCopy(subBgPal, BG_PALETTE_SUB, palSize);
	free(subBgPal);
	void *subObjPal = IO_ReadEntireFile("game/gfx/status_s_o.ncl.bin", &palSize);
	swiWaitForVBlank();
	dmaCopy(subObjPal, SPRITE_PALETTE_SUB, palSize);
	free(subObjPal);
	void *mainBgPal = IO_ReadEntireFile("game/gfx/pause_m_b.ncl.bin", &palSize);
	swiWaitForVBlank();
	dmaCopy(mainBgPal, BG_PALETTE, palSize);
	free(mainBgPal);
	
	//character 
	u32 charSize;
	void *subBgChar = IO_ReadEntireFile("game/gfx/top_s_b.ncg.bin", &charSize);
	MI_UncompressLZ16(subBgChar, bgGetGfxPtr(sub0));
	free(subBgChar);
	void *subObjChar = IO_ReadEntireFile("game/gfx/status_s_o.ncg.bin", &charSize);
	MI_UncompressLZ16(subObjChar, oamGetGfxPtr(&oamSub, 0));
	free(subObjChar);
	void *mainBgChar = IO_ReadEntireFile("game/gfx/pause_m_b.ncg.bin", &charSize);
	MI_UncompressLZ16(mainBgChar, bgGetGfxPtr(bg1));
	free(mainBgChar);
	
	//screen
	u32 subBgScrSize;
	void *subBgScr = IO_ReadEntireFile("game/gfx/top_s_b.nsc.bin", &subBgScrSize);
	MI_UncompressLZ16(subBgScr, bgGetMapPtr(sub0));
	free(subBgScr);
	void *mainBgScr = IO_ReadEntireFile("game/gfx/pause_m_b.nsc.bin", &subBgScrSize);
	MI_UncompressLZ16(mainBgScr, bgGetMapPtr(bg1));
	free(mainBgScr);
	
	//load level
	char levelNameBuffer[] = "game/levels/00.glv";
	levelNameBuffer[12] = '0' + (g_levelId / 10);
	levelNameBuffer[13] = '0' + (g_levelId % 10);
	g_currentLevel = LL_LoadLevel(levelNameBuffer);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(70, 256.0 / 192.0, 0.1, 40);
	
	gluLookAt(	0.0, 0.0, 0.0,		//camera possition 
				0.0, 0.0, -1.0,		//look at
				0.0, 1.0, 0.0);		//up
	
	swiWaitForVBlank();
	
	textureIds = g_currentLevel->pTextureIds;
	g_ball.m = 410; //0.1
	g_ball.r = 5 << 12;
	g_ball.x = g_currentLevel->startX;
	g_ball.y = g_currentLevel->startY;
	g_ball.vx = g_currentLevel->startVx;
	g_ball.vy = g_currentLevel->startVy;
	
	clearMasses();
	g_nPlanets = g_currentLevel->nPlanets;
	memcpy(g_planets, g_currentLevel->pPlanetData, g_nPlanets * sizeof(MASS));
	
	g_mode = MODE_FADEIN;
	g_fadingOut = 0;
	g_fadeTo = SCENE_SELECT;
	g_fadeFrame = 0;
}

void resetLevel() {
	g_ball.m = 410;
	g_ball.r = 5 << 12;
	g_ball.m = 410; //0.1
	g_ball.r = 5 << 12;
	g_ball.x = g_currentLevel->startX;
	g_ball.y = g_currentLevel->startY;
	g_ball.vx = g_currentLevel->startVx;
	g_ball.vy = g_currentLevel->startVy;
	
	clearMasses();
	g_nPlanets = g_currentLevel->nPlanets;
	memcpy(g_planets, g_currentLevel->pPlanetData, g_nPlanets * sizeof(MASS));
	
	g_mode = MODE_PLAY;
	g_modeFrame = 0;
}

void drawPlanet(int planetX, int planetY, int planetRadius, int offsetAngle, int nTris, int texId){
	glBindTexture(0, g_currentLevel->pTextureIds[texId]);
	glBegin(GL_TRIANGLE);
	glColor3b(255, 255, 255);
	glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(2));
	
	planetX = planetX * 30 / 8233;
	planetY = planetY * 30 / 8233;
	planetRadius = planetRadius * 30 / 8233;
	
	int i;
	for(i = 0; i < nTris; i++){
		int angle = i * 32768 / nTris;
		int angle2 = (i + 1) * 32768 / nTris;
		int cosAngle = cosLerp(angle), sinAngle = sinLerp(angle);
		int cosAngle2 = cosLerp(angle2), sinAngle2 = sinLerp(angle2);
		int cosAngleOffset = cosLerp(angle + offsetAngle), sinAngleOffset = sinLerp(angle + offsetAngle);
		int cosAngleOffset2 = cosLerp(angle2 + offsetAngle), sinAngleOffset2 = sinLerp(angle2 + offsetAngle);
		
		int x1 = planetX, y1 = planetY;
		int x2 = planetX + planetRadius * cosAngleOffset / 4096, 
			y2 = planetY + planetRadius * sinAngleOffset / 4096;
		int x3 = planetX + planetRadius * cosAngleOffset2 / 4096, 
			y3 = planetY + planetRadius * sinAngleOffset2 / 4096;
		
		GFX_TEX_COORD = TEXTURE_PACK(inttot16(32), inttot16(32));
		glVertex3v16(x1, y1, -floattov16(0.5));
		GFX_TEX_COORD = TEXTURE_PACK(inttot16(32 + 31 * cosAngle / 4096), inttot16(32 + 31 * sinAngle / 4096));
		glVertex3v16(x2, y2, -floattov16(0.5));
		GFX_TEX_COORD = TEXTURE_PACK(inttot16(32 + 31 * cosAngle2 / 4096), inttot16(32 + 31 * sinAngle2 / 4096));
		glVertex3v16(x3, y3, -floattov16(0.5));
	}
	glEnd();
}

void gameTick(){
	glPushMatrix();
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	
	glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1));
	glBindTexture(0, textureIds[0]);
	
	glBegin(GL_QUAD);
	glColor3f(1.0, 1.0, 1.0);

	int angle = g_frameCount;
	int sinX = sinLerp(angle) * 5 / 4;
	int cosX = cosLerp(angle) * 5 / 4;
	
	glTexParameter(0, GL_TEXTURE_WRAP_S | GL_TEXTURE_WRAP_T);
	GFX_TEX_COORD = TEXTURE_PACK(0, inttot16(512));
	glVertex3v16(-cosX + sinX, -sinX - cosX, -floattov16(1.0));		//left down
	GFX_TEX_COORD = TEXTURE_PACK(inttot16(512),inttot16(512));
	glVertex3v16(cosX + sinX, sinX - cosX, -floattov16(1.0));		//right down
	GFX_TEX_COORD = TEXTURE_PACK(inttot16(512), 0);
	glVertex3v16(cosX - sinX, sinX + cosX, -floattov16(1.0));		//right up
	GFX_TEX_COORD = TEXTURE_PACK(0, 0);
	glVertex3v16(-cosX - sinX, -sinX + cosX, -floattov16(1.0));		//left up
	
	glEnd();
	
	//make some tris
	int nTris = 16;
	
	//if not in the win display phase, draw planets like normal.
	if(g_mode != MODE_WIN) {
		int i;
		for(i = 0; i < g_nPlanets; i++){
			MASS *planet = g_planets + i;
			int texId = (i == 0) ? g_currentLevel->goalTexture : (g_currentLevel->firstPlanetTexture + ((i - 1) % g_currentLevel->nPlanetTextures));
			drawPlanet(planet->x, planet->y, planet->r, -g_frameCount * 16, nTris, texId);
		}
		
		drawPlanet(g_ball.x, g_ball.y, g_ball.r, g_frameCount * 16, nTris, g_currentLevel->movableObjectTexture);
	} else {
		//otherwise, we start zooming into the planet.
		int factor = g_modeFrame;
		if(factor > 32) factor = 32;
		
		int scales[] = {32, 33, 35, 36, 38, 40, 41, 43, 45, 47, 49, 52, 54, 56, 59, 61, 64, 67, 70, 73, 76, 79, 83, 87, 91, 95, 99, 103, 108, 112, 117, 123, 128};
		int scale = scales[factor]; //exponential zoom from 1x to 4x
		
		int i;
		for(i = 0; i < g_nPlanets; i++){
			MASS *planet = g_planets + i;
			int texId = (i == 0) ? g_currentLevel->goalTexture : (g_currentLevel->firstPlanetTexture + ((i - 1) % g_currentLevel->nPlanetTextures));
			
			int effectiveX = planet->x * scale / 32;
			int effectiveY = planet->y * scale / 32;
			drawPlanet(effectiveX, effectiveY, planet->r * scale / 32, -g_frameCount * 16, nTris, texId);
		}
		
		int effectiveX = g_ball.x * scale / 32;
		int effectiveY = g_ball.y * scale / 32;
		drawPlanet(effectiveX, effectiveY, g_ball.r * scale / 32, g_frameCount * 16, nTris, g_currentLevel->movableObjectTexture);
	}
	
	glPopMatrix(1);
	glFlush(0);
	
	//check for touch input
	touchPosition touchPos;
	scanKeys();
	u32 keys = keysDown(); //don't want continued presses, now do we?
	touchRead(&touchPos);
	
	//get touch input
	if(keys & KEY_TOUCH && g_mode == MODE_PLAY) {
		//place an object
		
		int nPlacedObjects = g_nPlanets - g_currentLevel->nPlanets;
		if(nPlacedObjects < g_currentLevel->nMaxObjects) {
			MASS m;
			m.vx = 0;
			m.vy = 0;
			m.x = (touchPos.px - 128) << 12;
			m.y = (96 - touchPos.py) << 12;
			m.r = 10 << 12;
			m.m = 5 << 12;
			addMass(&m);
		}
	}
	
	//check pausing?
	if(keys & KEY_START){
		if(g_mode == MODE_PLAY) {
			g_mode = MODE_PAUSE;
			g_selection = 0;
			
			//show the pause menu
			bgShow(1);
		} else if(g_mode == MODE_PAUSE) {
			g_mode = MODE_PLAY;
			
			//hide pause menu
			bgHide(1);
		}
	}
	
	//if paused, then update pause menu
	if(g_mode == MODE_PAUSE) {
		int i;
		vu16 *refPal = BG_PALETTE;
		
		for(i = 1; i < 5; i++){
			vu16 *pal = BG_PALETTE + (i * 16);
			
			int j;
			for(j = 0; j < 16; j++){
				u16 src = refPal[j];
				
				if(i - 1 != g_selection) pal[j] = src;
				else {
					src &= (0x1F << 5) | (0x1F << 10);
					pal[j] = src;
				}
			}
		}
	}
	
	//if paused, get pause input
	if(g_mode == MODE_PAUSE) {
		if(keys & KEY_DOWN) {
			g_selection++;
			if(g_selection == 4) g_selection = 0;
		} else if(keys & KEY_UP) {
			g_selection--;
			if(g_selection == -1) g_selection = 3;
		} else if(keys & KEY_A) {
			//act on the selection
			switch(g_selection) {
				case 0: //resume
					g_mode = MODE_PLAY;
					bgHide(1);
					break;
				case 1: //reset
					g_mode = MODE_LOSE;
					bgHide(1);
					break;
				case 2: //level select
					sceneFadeOut();
					g_fadingOut = 1;
					g_fadeTo = SCENE_SELECT;
					break;
				case 3: //main menu
					sceneFadeOut();
					g_fadingOut = 1;
					g_fadeTo = SCENE_TITLE;
					break;
			}
		}
	}
	
	//tick objects
	if(g_mode == MODE_PLAY){
		g_ball.x += g_ball.vx;
		g_ball.y += g_ball.vy;
		
		//is ball colliding?
		int index = ballIsCollidingWith();
		if(index != -1){
			//is goal? 
			if(index == 0) {
				//yay
				g_mode = MODE_WIN;
				g_modeFrame = 0;
			} else {
				//nope
				
				g_mode = MODE_LOSE;
				g_modeFrame = 0;
			}
			
			//adjust ball position so that it isn't imbedded in the planet
			snap(&g_ball, index);
		}
		
		accelerate(&g_ball);
		
		//check out of bounds
		int maxX = (g_ball.x + g_ball.r) >> 12;
		int minX = (g_ball.x - g_ball.r) >> 12;
		int maxY = (g_ball.y + g_ball.r) >> 12;
		int minY = (g_ball.y - g_ball.r) >> 12;
		
		int oamOffset = 0;
		oamClear(&oamSub, 0, 0);
		if(maxX < -128 || maxY < -96 || minX > 128 || minY > 96) {
			g_outOfBounds = TRUE;
			g_outOfBoundsFrames++;
			
			int x = 88, y = 136;
			oamSet(&oamSub, 0, x, y, 0, 0, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, 0), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
			oamSet(&oamSub, 1, x + 32, y, 0, 0, SpriteSize_32x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(4)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
			oamSet(&oamSub, 2, x + 64, y, 0, 0, SpriteSize_16x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(8)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
			
			//countdown timer
			int seconds = (300 - g_outOfBoundsFrames + 59) / 60;
			oamSet(&oamSub, 3, 120, 150, 0, 0, SpriteSize_16x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(64 + 2 * seconds)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
			oamOffset += 4;
		} else {
			g_outOfBounds = FALSE;
			g_outOfBoundsFrames = 0;
			
			oamClear(&oamSub, 0, 4);
		}
		
		//draw usable planet indicator
		int i;
		for(i = 0; i < g_currentLevel->nMaxObjects - (g_nPlanets - g_currentLevel->nPlanets); i++){
			oamSet(&oamSub, oamOffset, 10 + i * 20, 166, 0, 1, SpriteSize_16x16, SpriteColorFormat_16Color, oamGetGfxPtr(&oamSub, SWIZZLE(10)), -1, FALSE, FALSE, FALSE, FALSE, FALSE);
			oamOffset++;
		}
		
		//if out of bounds too long? (>5s)
		if(g_outOfBoundsFrames > 300) {
			g_mode = MODE_LOSE;
			g_modeFrame = 0;
		}
	}
	
	//if losing for too long? reset
	if(g_mode == MODE_LOSE && g_modeFrame > 60) {
		resetLevel();
	}
	
	//after frame 16 of win loop, fade out.
	if(g_mode == MODE_WIN && g_modeFrame == 16) {
		sceneFadeOut();
		g_fadingOut = 1;
		g_fadeTo = SCENE_SELECT;
	}
	
	if(g_fadingOut) g_fadeFrame++;
	if(g_fadeFrame == 16) {
		g_scene = g_fadeTo;
		LL_UnloadLevel(g_currentLevel);
	}
	
	oamUpdate(&oamSub);
}

void gameTickProc(){
	if(g_gameLastTick + 1 != g_frameCount){
		sceneFadeIn();
		gameInit();
	}
	
	//after frame 16, switch to gameplay.
	if(g_modeFrame > 16 && g_mode == MODE_FADEIN) {
		g_mode = MODE_PLAY;
		g_modeFrame = 0;
	}
	gameTick();
	
	g_gameLastTick = g_frameCount;
	g_modeFrame++;
}