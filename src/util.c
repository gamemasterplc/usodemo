#include <fs.h>
#include <malloc.h>
#include <symbol_attr.h>

EXPORT_SYMBOL void *FileLoad(const char *path)
{
	FSFile file;
	if(!FSOpen(&file, path)) {
		//Return NULL pointer if file failed to open
		return NULL;
	}
	u32 size = FSGetSize(&file);
	//Allocate file
	void *dst = malloc(size);
	FSRead(&file, dst, size); //Read file
	return dst; //Return allocated space for file
}