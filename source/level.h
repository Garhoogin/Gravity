#pragma once

#include <nds.h>

typedef struct MASS_ {
	s32 x;		//X position as Fx32
	s32 y;		//Y position as Fx32
	s32 vx;		//Velocity X as Fx32
	s32 vy;		//Velocity Y as Fx32
	s32 r;		//Radius as Fx32
	s32 m;		//Mass as Fx32
} MASS; //size: 0x18

//Do not use unless as a pointer! Functions expect these are created by level lib!
typedef struct LEVEL_ {
	char magic[4];					//magic 'GLVL' Gravity LeVeL
	u32 fileSize;					//size in bytes of the file
	u8 nTextures;					//number of textures to load
	u8 nPlanets;					//number of fixed planets
	u8 nMaxObjects;					//max number of placed planets
	u8 firstPlanetTexture;			//first texture ID for planet
	u8 nPlanetTextures;				//number of planet textures
	u8 userPlacedTexture;			//index of user-placed texture
	u8 movableObjectTexture;		//texture ID of moving object
	u8 goalTexture;					//texture ID of goal
	s32 startX;						//moving object start X
	s32 startY;						//moving object start Y
	s32 startVx;					//moving object start vX
	s32 startVy;					//moving object start vY
	MASS *pPlanetData;				//pointer to planet data
	void *pTextureReferences;		//pointer to texture reference data
	int *pTextureIds;				//set to NULL in-file, points to texture ID array
} LEVEL;

#define TEXIMAGE_PARAM_TEXFMT_SHIFT   26
#define TEXIMAGE_PARAM_TEXFMT_MASK    0x1C000000

#define TEXIMAGE_PARAM_T_SIZE_SHIFT   23
#define TEXIMAGE_PARAM_T_SIZE_MASK    0x03800000

#define TEXIMAGE_PARAM_V_SIZE_SHIFT   20
#define TEXIMAGE_PARAM_V_SIZE_MASK    0x00700000

//
// Load texture given texel, index, and palette file names, size, and format. Index path is only 
// required for 4x4 textures, and the palette file name is not required for direct color textures.
//
void LL_LoadTexture(const char *txel, const char *pidx, const char *pcol, int id, int width, int height, int format, int *textureIds);

//
// Load a level from path.
//
LEVEL *LL_LoadLevel(const char *path);

//
// Unload a level.
//
void LL_UnloadLevel(LEVEL *level);