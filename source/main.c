#include <nds.h>
#include <filesystem.h>

#include "scenes.h"


#define BLDCNT            (*(u16*)0x04000050)
#define BLDALPHA          (*(u16*)0x04000052)
#define BLDY              (*(u16*)0x04000054)

#define DB_BLDCNT         (*(u16*)0x04001050)
#define DB_BLDALPHA       (*(u16*)0x04001052)
#define DB_BLDY           (*(u16*)0x04001054)

static int g_fadeFrame = 0;
static int g_fadingOut = 0;
static int g_fadingIn = 0;

int g_levelId = 0;
int g_frameCount = 0;
int g_scene = SCENE_TITLE;
void (*sceneFuncs[SCENE_MAX + 1]) ();

int main(void){
	/*
	
Gravity Well

The game is basically:
	- A level containing a ball, a goal, and a few pre-positioned planets
	- For each level the ball has a starting velocity
	- The goal functions as a planet, except that the level is won when collided with
	- When colliding with a planet that isn't the goal, you lose
	- You can place extra planets on the level to guide the ball to the goal
	- You can only place planets before the ball begins movement
	- You can start the motion by clicking the "start" button
	- The motion only ends when the ball makes a collision, reaches the goal, or the user stops it
	- The user can stop motion at any time to rework the level
	- If the user modifies the level while stopped, they cannot continue from where it was paused, it must be restarted
	
	*/
	
	nitroFSInit(NULL);
	sceneFuncs[SCENE_TITLE] = titleTickProc;
	sceneFuncs[SCENE_SELECT] = selectTickProc;
	sceneFuncs[SCENE_GAME] = gameTickProc;
	sceneFuncs[SCENE_RULES] = rulesTickProc;
	
	swiWaitForVBlank();
	while(1){
		sceneFuncs[g_scene]();
		
		g_frameCount++;
		
		//check for screen fade
		if(g_fadingIn) {
			BLDY = 16 - g_fadeFrame;
			DB_BLDY = 16 - g_fadeFrame;
			
			g_fadeFrame++;
			if(g_fadeFrame > 16) {
				g_fadingIn = 0;
				g_fadeFrame = 0;
				BLDCNT = 0;
				DB_BLDCNT = 0;
			}
		} else if(g_fadingOut) {
			BLDY = g_fadeFrame;
			DB_BLDY = g_fadeFrame;
			
			g_fadeFrame++;
			if(g_fadeFrame > 16) {
				g_fadingOut = 0;
				g_fadeFrame = 0;
			}
		}
		swiWaitForVBlank();
	}
	
	return 0;
}

void sceneFadeOut() {
	//if not fading out, setup BLDCNT registers.
	if(!g_fadingOut) {
		BLDY = 0;
		DB_BLDY = 0;
		BLDCNT = 0x3F | (3 << 6);
		DB_BLDCNT = 0x3F | (3 << 6);
		g_fadingOut = 1;
	}
	
	//if fading in, set the frame accordingly, for a 16-frame fade.
	if(g_fadingIn) {
		g_fadingIn = 0;
		g_fadeFrame = 16 - g_fadeFrame;
	} else {
		g_fadeFrame = 0;
	}
}

void sceneFadeIn() {
	//if not fading in, setup BLDCNT registers.
	if(!g_fadingIn) {
		BLDY = 16;
		DB_BLDY = 16;
		BLDCNT = 0x3F | (3 << 6);
		DB_BLDCNT = 0x3F | (3 << 6);
		g_fadingIn = 1;
	}
	
	//if fading out, set the frame accordingly, for a 16-frame fade.
	if(g_fadingOut) {
		g_fadingOut = 0;
		g_fadeFrame = 16 - g_fadeFrame;
	} else {
		g_fadeFrame = 0;
	}
}

int sceneFadingIn() {
	return g_fadingIn;
}

int sceneFadingOut() {
	return g_fadingOut;
}
