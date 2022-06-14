#pragma once

#include <PR/ultratypes.h>
#include <cextern.h>
#include <stdbool.h>

#define FS_SEEK_SET 0
#define FS_SEEK_CUR 1
#define FS_SEEK_END 2

typedef struct fs_file {
	u32 rom_ofs;
	s32 pos;
	s32 size;
} FSFile;

EXTERN_C void FSInit();
EXTERN_C bool FSOpen(FSFile *file, const char *path); //Opens the file at path
EXTERN_C s32 FSRead(FSFile *file, void *dst, s32 len); //Reads up to len bytes from file from current position
EXTERN_C u32 FSGetRomOfs(FSFile *file);
EXTERN_C s32 FSGetRomSize(FSFile *file);
EXTERN_C s32 FSGetSize(FSFile *file);
EXTERN_C void FSSeek(FSFile *file, s32 offset, u32 origin); //Set seek position relative to FS_SEEK origin
EXTERN_C s32 FSTell(FSFile *file);