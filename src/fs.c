#include <ultra64.h>
#include <stdlib.h>
#include <string.h>
#include <fs.h>
#include <debug.h>
#include <symbol_attr.h>

#define FS_HEADER_MAGIC 0x46533634
#define ROMREAD_BUF_SIZE 1024

extern u8 __fs_rom_start[];

typedef struct fs_entry {
	const char *name;
	u32 rom_ofs;
	u32 size;
} FSEntry;

typedef struct fs_header {
	u32 magic;
	u32 num_files;
	u32 string_size;
} FSHeader;

static u8 read_buf[ROMREAD_BUF_SIZE] ALIGN_SYMBOL(16); //16-byte aligned buffer for unaligned reads
static FSHeader fs_header ALIGN_SYMBOL(8);
static FSEntry *fs_entries;

static int ComparePath(const char *path1, const char *path2)
{
	char c1, c2;
	do {
		//Read character from each string
		c1 = *path1++;
		c2 = *path2++;
		//Convert left slash to right slash
		if(c1 == '\\') {
			c1 = '/';
		}
		//Convert left slash to right slash
		if(c2 == '\\') {
			c2 = '/';
		}
	} while(c1 == c2 && c1 != 0);
	return c1-c2;
}

static int CompareFileEntry(const void *arg1, const void *arg2)
{
	const FSEntry *entry1 = arg1;
	const FSEntry *entry2 = arg2;
	return ComparePath(entry1->name, entry2->name);
}

static OSPiHandle *GetCartHandle()
{
	static OSPiHandle *handle = NULL;
	//Only acquire handle if necessary
	if(!handle) {
		handle = osCartRomInit();
	}
	return handle;
}

static void ReadRom(void *dst, u32 src, u32 len)
{
	OSIoMesg io_mesg;
	OSMesgQueue dma_msg_queue;
	OSMesg dma_msg;
	u32 src_ofs = (u32)src; //Temporary for source offset
	char *dst_ptr = dst; //Use temporary for destination pointer
	//Initialize DMA Status
	osCreateMesgQueue(&dma_msg_queue, &dma_msg, 1);
	io_mesg.hdr.pri = OS_MESG_PRI_NORMAL;
	io_mesg.hdr.retQueue = &dma_msg_queue;
	//Check for direct DMA being possible
	if((((u32)dst_ptr & 0x7) == 0) && (src_ofs & 0x1) == 0 && (len & 0x1) == 0) {
		//Do DMA directly to destination
		osInvalDCache(dst, (len+15) & ~0xF);
		while(len) {
			//Calculate chunk read length
			u32 copy_len = ROMREAD_BUF_SIZE;
			if(copy_len > len) {
				copy_len = len;
			}
			//Setup DMA params
			io_mesg.dramAddr = dst_ptr;
			io_mesg.devAddr = src_ofs;
			io_mesg.size = copy_len;
			//Start reading from ROM
			osEPiStartDma(GetCartHandle(), &io_mesg, OS_READ);
			//Wait for ROM read to finish
			osRecvMesg(&dma_msg_queue, &dma_msg, OS_MESG_BLOCK);
			//Advance data pointers
			src_ofs += copy_len;
			dst_ptr += copy_len;
			len -= copy_len;
		}
	} else {
		u32 read_buf_offset = src_ofs & 0x1; //Source fixup offset for odd source offset DMAs
		src_ofs &= ~0x1; //Round down source offset
		//Writeback invalidate destination buffer for RCP usage
		osWritebackDCache(dst, (len+15) & ~0xF);
		osInvalDCache(dst, (len+15) & ~0xF);
		//DMA to temporary buffer
		while(len) {
			//Calculate chunk copy length
			u32 copy_len = ROMREAD_BUF_SIZE;
			if(copy_len > len) {
				copy_len = len;
			}
			u32 read_len = (copy_len+15) & ~0xF; //Round read length to nearest cache line
			//Simple invalidate works here since the buffer is aligned to 16 bytes
			osInvalDCache(read_buf, read_len);
			//Setup DMA params
			io_mesg.dramAddr = read_buf;
			io_mesg.devAddr = src_ofs;
			io_mesg.size = read_len;
			//Start reading from ROM
			osEPiStartDma(GetCartHandle(), &io_mesg, OS_READ);
			//Wait for ROM read to finish
			osRecvMesg(&dma_msg_queue, &dma_msg, OS_MESG_BLOCK);
			//Copy from temporary buffer
			bcopy(read_buf+read_buf_offset, dst_ptr, copy_len);
			//Advance data pointers
			src_ofs += copy_len;
			dst_ptr += copy_len;
			len -= copy_len;
		}
	}
}

void FSInit()
{
	u32 fs_addr = (u32)__fs_rom_start;
	//Read FS header
	ReadRom(&fs_header, fs_addr, sizeof(FSHeader));
	debug_assert(fs_header.magic == FS_HEADER_MAGIC && "Invalid FS Magic");
	//Calculate size of entries data
	u32 entry_size = sizeof(FSEntry)*fs_header.num_files;
	entry_size += fs_header.string_size;
	//Allocate entries data
	fs_entries = malloc(entry_size);
	//Read entries
	ReadRom(fs_entries, fs_addr+sizeof(FSHeader), entry_size);
	//Fix pointers in entries
	for(u32 i=0; i<fs_header.num_files; i++) {
		//Patch name pointer
		fs_entries[i].name = (const char *)((u32)fs_entries[i].name+(u32)fs_entries);
		//Patch rom offset pointer
		fs_entries[i].rom_ofs += fs_addr;
	}
}

EXPORT_SYMBOL bool FSOpen(FSFile *file, const char *path)
{
	debug_assert(file != NULL);
	//Skip initial slash
	if(*path == '/') {
		path++;
	}
	FSEntry check_entry = { path, 0, 0 };
	FSEntry *entry = bsearch(&check_entry, fs_entries, fs_header.num_files, sizeof(FSEntry), CompareFileEntry);
	if(entry) {
		file->rom_ofs = entry->rom_ofs;
		file->pos = 0;
		file->size = entry->size;
		return true;
	}
	return false;
}

EXPORT_SYMBOL s32 FSRead(FSFile *file, void *dst, s32 len)
{
	debug_assert(file != NULL);
	if(file->pos+len > file->size) {
		len = file->size-file->pos;
	}
	if(len > 0) {
		ReadRom(dst, file->rom_ofs+file->pos, len);
		file->pos += len;
	}
	return len;
}

EXPORT_SYMBOL u32 FSGetRomOfs(FSFile *file)
{
	debug_assert(file != NULL);
	return file->rom_ofs;
}

EXPORT_SYMBOL s32 FSGetRomSize(FSFile *file)
{
	debug_assert(file != NULL);
	return (file->size+1) & ~1;
}

EXPORT_SYMBOL s32 FSGetSize(FSFile *file)
{
	debug_assert(file != NULL);
	return file->size;
}

EXPORT_SYMBOL void FSSeek(FSFile *file, s32 offset, u32 origin)
{
	debug_assert(file != NULL);
	switch(origin) {
		case FS_SEEK_SET:
		//Direct offset
			file->pos = offset;
			break;
			
		case FS_SEEK_CUR:
		//Relative offset
			file->pos += offset;
			break;
			
		case FS_SEEK_END:
		//End-based offset
			file->pos = file->size+offset;
			break;
			
		default:
			debug_printf("Invalid seek origin %d.\n", origin);
			return;
	}
	//Clamp seek to file boundaries
	if(file->pos < 0) {
		file->pos = 0;
	}
	if(file->pos > file->size) {
		file->pos = file->size;
	}
}

EXPORT_SYMBOL s32 FSTell(FSFile *file)
{
	debug_assert(file != NULL);
	return file->pos;
}