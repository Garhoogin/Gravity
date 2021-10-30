#pragma once

#define SCENE_TITLE        0
#define SCENE_GAME         1
#define SCENE_SELECT       2
#define SCENE_RULES        3
#define SCENE_MAX          3

extern void (*sceneFuncs[SCENE_MAX + 1])();

extern int g_scene;
extern int g_frameCount;
extern int g_levelId;

void gameTickProc();

void selectTickProc();

void titleTickProc();

void rulesTickProc();


void sceneFadeOut();

void sceneFadeIn();

int sceneFadingIn();

int sceneFadingOut();
