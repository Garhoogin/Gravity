#include "level.h"
#include "io.h"
#include "mi.h"

void LL_LoadTexture(const char *txel, const char *pidx, const char *pcol, int id, int width, int height, int format, int *textureIds){
	u32 textureSize = 0, idxSize = 0, paletteSize = 0;
	void *palette = NULL, *texture = NULL, *idx = NULL;
	if(pcol != NULL) {
		palette = IO_ReadEntireFile(pcol, &paletteSize);
		DC_FlushRange(palette, paletteSize);
	}
	texture = IO_ReadEntireFile(txel, &textureSize);
	DC_FlushRange(texture, textureSize);
	if(pidx != NULL) {
		idx = IO_ReadEntireFile(pidx, &idxSize);
		DC_FlushRange(idx, idxSize);
	}

	void *texImage = malloc(textureSize + idxSize);
	swiCopy(texture, texImage, (textureSize >> 2) | COPY_MODE_WORD);
	if(pidx != NULL) swiCopy(idx, (void *) ((u32) texImage + textureSize), (idxSize >> 2) | COPY_MODE_WORD);

	glBindTexture(0, textureIds[id]);
	glTexImage2D(0, 0, format, width, height, 0, TEXGEN_TEXCOORD, (u8 *) texImage);
	if(paletteSize) glColorTableEXT(0, 0, paletteSize >> 1, 0, 0, (u16*) palette);
	
	free(texImage);
	if(palette != NULL) free(palette);
	if(texture != NULL) free(texture);
	if(idx != NULL) free(idx);
}

LEVEL *LL_LoadLevel(const char *path) {
	/*
	Header structure:
	
	Offset      Type         Meaning
	0x00        char[4]      Magic 'GLVL' Gravity LeVeL
	0x04        u32          File size
	0x08        u8           Number of textures
	0x09        u8           Number of planets
	0x0A        u8           Maximum user-placed objects
	0x0B        u8           Index of first texture belonging to planets
	0x0C        u8           Number of textures belonging to planets
	0x0D        u8           Index of texture belonging to user-placed objects
	0x0E        u8           Index of texture belonging to movable objects
	0x0F        u8           Index of texture belonging to goal point
	0x10        fx32         Starting X
	0x14        fx32         Starting Y
	0x18        fx32         Starting vX
	0x1C        fx32         Starting vY
	0x20        void *       Pointer to planet data
	0x24        void *       Pointer to texture references
	0x28        int *        Pointer to texture IDs. Set during execution.      
	
	*/
	
	u32 size;
	void *pFile = IO_ReadEntireFile(path, &size);
	if(pFile == NULL) return NULL;
	
	LEVEL *level = (LEVEL *) pFile;
	
	//fix-up pointers in structure Nintendo style
	level->pPlanetData = (void *) (((u32) level->pPlanetData) + ((u32) level));
	level->pTextureReferences = (void *) (((u32) level->pTextureReferences) + ((u32) level));
	
	//setup VideoGL textures
	level->pTextureIds = (int *) calloc(level->nTextures, sizeof(int));
	glGenTextures(level->nTextures, level->pTextureIds);
	
	//parse out texture dependencies.
	int i;
	char *ptr = (char *) level->pTextureReferences;
	for(i = 0; i < level->nTextures; i++) {
		//align pointer, make sure reads happen correctly!
		if(((u32) ptr) & 3) ptr += 4 - (((u32) ptr) & 3);
		
		//First 4 bytes: texImageParam
		//Following are 0-terminated strings for required files in the order texel, index, palette.
		u32 texImageParam = *(u32 *) ptr;
		ptr += 4;
		
		int fmt = (texImageParam & TEXIMAGE_PARAM_TEXFMT_MASK) >> TEXIMAGE_PARAM_TEXFMT_SHIFT;
		int sizeS = (texImageParam & TEXIMAGE_PARAM_T_SIZE_MASK) >> TEXIMAGE_PARAM_T_SIZE_SHIFT;
		int sizeT = (texImageParam & TEXIMAGE_PARAM_V_SIZE_MASK) >> TEXIMAGE_PARAM_V_SIZE_SHIFT;
		
		u8 hasIdxMask = 0x20;
		u8 hasPltMask = 0x7E;
		
		char *texelPath = NULL, *indexPath = NULL, *palettePath = NULL;
		
		texelPath = ptr;
		ptr += strlen(ptr) + 1;
		if(hasIdxMask & (1 << fmt)) {
			indexPath = ptr;
			ptr += strlen(ptr) + 1;
		}
		if(hasPltMask & (1 << fmt)) {
			palettePath = ptr;
			ptr += strlen(ptr) + 1;
		}
		
		//load texture
		LL_LoadTexture(texelPath, indexPath, palettePath, i, sizeS, sizeT, fmt, level->pTextureIds);
	}
	
	return level;
}

void LL_UnloadLevel(LEVEL *level) {

	//free up pointers
	free(level->pTextureIds);
	free(level);
}