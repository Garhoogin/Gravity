#include "io.h"

#include <stdio.h>
#include <sys/stat.h>

#include <nds.h>

void *IO_ReadEntireFile(const char *fname, u32 *nSize){
	u32 bufferSize;
	void *bf;
	
	FILE *fp = fopen(fname, "rb");
	if(fp == NULL){
		*nSize = 0;
		return NULL;
	}
	
	struct stat stats;
	stat(fname, &stats);
	bufferSize = stats.st_size;
	
	if(!bufferSize){
		*nSize = 0;
		return NULL;
	}
	
	bf = malloc(bufferSize);
	if(bf == NULL){
		*nSize = 0;
		return NULL;
	}
	
	fread(bf, bufferSize, 1, fp);
	fclose(fp);
	
	*nSize = bufferSize;
	return bf;
	
}

int IO_FileExists(const char *fname){
	struct stat bf;
	int s = stat(fname, &bf);
	return !s;
}