#include <ultra64.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <usoload.h>
#include <fs.h>
#include <debug.h>
#include <symbol_attr.h>

#define R_MIPS_32 2
#define R_MIPS_26 4
#define R_MIPS_HI16 5
#define R_MIPS_LO16 6

#define USO_STRBUF_SIZE 255

//Convert Pointer to Memory Space Including NULL
#define PATCH_PTR_NULL(ptr, base) ((ptr) = ((ptr) ? (typeof(ptr))((u32)(base)+(u32)(ptr)) : NULL))
#define PATCH_PTR(ptr, base) ((ptr) = ((typeof(ptr))((u32)(base)+(u32)(ptr))))

typedef void (*FuncPtr)();

typedef struct uso_relocation {
	u32 offset;
	u32 info;
	u32 sym_offset;
} USORelocation;

typedef struct uso_section {
	void *data;
	u32 size;
	u32 align;
	u32 num_internal_relocs;
	USORelocation *internal_relocs;
	u32 num_external_relocs;
	USORelocation *external_relocs;
} USOSection;

typedef struct uso_symbol {
	const char *name;
	void *ptr;
	u32 section;
} USOSymbol;

typedef struct uso_header {
	u32 num_sections;
	USOSection *sections;
	u32 num_import_symbols;
	USOSymbol *import_symbols;
	u32 num_export_symbols;
	USOSymbol *export_symbols;
	u16 ctor_section;
	u16 dtor_section;
} USOHeader;

struct uso_load_info {
	u32 align;
	u32 size;
	u32 noload_align;
	u32 noload_size;
};

struct uso_handle_data {
	USOHandle next;
	USOHandle prev;
	USOHeader *uso;
	u32 uso_size;
	int ref_count;
	char name[1];
};

static USOHandle uso_handle_list_head;
static USOHandle uso_handle_list_tail;
static u32 num_global_syms ALIGN_SYMBOL(8);
static USOSymbol *global_syms;
static USOSection dummy_section_table = { NULL, 0, 0, 0, NULL, 0, NULL };

static void FixupExportSymbols(USOSymbol *export_symbol_table, u32 num_symbols, USOSection *section_table, u32 num_sections)
{
	//Patch all symbol name pointers to be absolute
	for(u32 i=0; i<num_symbols; i++) {
		USOSymbol *symbol = &export_symbol_table[i];
		void *section_base = NULL; //Pointer is relative to nowhere section by default
		if(symbol->section < num_sections) {
			section_base = section_table[symbol->section].data;
		}
		PATCH_PTR(symbol->name, export_symbol_table);
		PATCH_PTR(symbol->ptr, section_base);
	}
}

static void FixupImportSymbolsNames(USOSymbol *import_symbol_table, u32 num_symbols)
{
	//Patch all symbol name pointers to be absolute
	for(u32 i=0; i<num_symbols; i++) {
		PATCH_PTR(import_symbol_table[i].name, import_symbol_table);
	}
}

static void LinkToUSOList(USOHandle handle)
{
	USOHandle prev = uso_handle_list_tail;
	if(!prev) {
		//Link handle to head of list without any previous nodes
		uso_handle_list_head = handle;
	} else {
		//Link handle to last node of list
		prev->next = handle;
	}
	//Set handle links
	handle->prev = prev;
	handle->next = NULL;
	//Link this module to end of list
	uso_handle_list_tail = handle;
}

static void UnlinkFromUSOList(USOHandle handle)
{
	USOHandle next = handle->next;
	USOHandle prev = handle->prev;
	if(!next) {
		//Link previous node to tail if no next node exists
		uso_handle_list_tail = prev;
	} else {
		//Relink next node's previous node
		next->prev = prev;
	}
	if(!prev) {
		//Relink linked list head
		uso_handle_list_head = next;
	} else {
		//Relink previous node's next
		prev->next = next;
	}
}

//Only works properly with power of 2 alignment
static u32 AlignTo(u32 value, u32 to)
{
	return (value+to-1) & ~(to-1);
}

static void PatchSectionTable(USOHeader *header, u8 *bss)
{
	//Fixup general section pointer
	PATCH_PTR(header->sections, header);
	//Fixup all section pointers
	for(u32 i=0; i<header->num_sections; i++) {
		USOSection *section = &header->sections[i];
		if(section->data == NULL) {
			if(section->size != 0) {
				//Align BSS section pointer
				bss = (u8 *)AlignTo((u32)bss, section->align);
				section->data = bss;//Fixup BSS section pointer
				bss += section->size; //Set next BSS section pointer
			}
		} else {
			//Update section pointer
			PATCH_PTR(section->data, header);
		}
		//Patch relocation list pointers
		PATCH_PTR_NULL(section->internal_relocs, header);
		PATCH_PTR_NULL(section->external_relocs, header);
	}
}

static int CompareSymbol(const void *arg1, const void *arg2)
{
	const USOSymbol *symbol1 = arg1;
	const USOSymbol *symbol2 = arg2;
	//Compare symbol names
	return strcmp(symbol1->name, symbol2->name);
}

static USOSymbol *SearchSymbolTable(USOSymbol *symbol_table, u32 num_symbols, const char *name)
{
	//Don't search empty symbol tables
	if(num_symbols == 0) {
		return NULL;
	}
	USOSymbol temp = { name, NULL, 0 }; //Symbol to search for
	//Search for wanted symbol
	return bsearch(&temp, symbol_table, num_symbols, sizeof(USOSymbol), CompareSymbol);
}

static USOSymbol *SearchExportSymbolUSO(USOHeader *header, const char *name)
{
	//Search for symbol in this USO's export table
	return SearchSymbolTable(header->export_symbols, header->num_export_symbols, name);
}

static void *SearchSymbol(const char *name, bool search_global)
{
	USOSymbol *symbol = NULL;
	//Iterate over USO list
	USOHandle curr_handle = uso_handle_list_head;
	while(curr_handle) {
		symbol = SearchExportSymbolUSO(curr_handle->uso, name);
		if(symbol) {
			//Symbol found, return pointer
			return symbol->ptr;
		}
		//Go to next USO
		curr_handle = curr_handle->next;
	}
	if(search_global) {
		//Search global symbols if required
		symbol = SearchSymbolTable(global_syms, num_global_syms, name);
	}
	//Return NULL if symbol is not found
	if(!symbol) {
		return NULL;
	}
	//Symbol found, return pointer
	return symbol->ptr;
}

static void PatchImportSymTable(USOHeader *header)
{
	//Fix import symbol table pointer and names
	PATCH_PTR(header->import_symbols, header);
	FixupImportSymbolsNames(header->import_symbols, header->num_import_symbols);
	//Loop through import symbol table
	for(u32 i=0; i<header->num_import_symbols; i++) {
		USOSymbol *symbol = &header->import_symbols[i];
		//Search for symbol including in global symbol tables
		symbol->ptr = SearchSymbol(symbol->name, true);
		if(!symbol->ptr) {
			//Abort with message if symbol was not found
			debug_printf("Could not find symbol %s in any symbol table.\n", symbol->name);
			abort();
		}
	}
}

static void PatchExportSymTable(USOHeader *header)
{
	PATCH_PTR(header->export_symbols, header); //Fixup export symbol table pointer
	//Fixup export symbol table
	FixupExportSymbols(header->export_symbols, header->num_export_symbols, header->sections, header->num_sections);
}

static void ApplySectionRelocations(USOHeader *header, u32 section, bool internal)
{
	USOSection *section_ptr = &header->sections[section];
	USORelocation *relocs;
	u32 num_relocs;
	if(internal) {
		//Reference internal relocation list
		relocs = section_ptr->internal_relocs;
		num_relocs = section_ptr->num_internal_relocs;
	} else {
		//Reference external relocation list
		relocs = section_ptr->external_relocs;
		num_relocs = section_ptr->num_external_relocs;
	}
	if(!relocs || !num_relocs) {
		//Skip if no relocations or relocation table to process
		return;
	}
	//Loop through reloccations
	for(u32 i=0; i<num_relocs; i++) {
		//Calculate target address
		u32 *target_addr = (u32 *)((u8 *)section_ptr->data+relocs[i].offset);
		u32 reloc_target_idx = relocs[i].info & 0x3FFFFFF;
		u32 sym_addr;
		//Get relocation type
		u8 type = relocs[i].info >> 26;
		if(internal) {
			//Use section table and symbol offset to calculte internal symbol address
			sym_addr = (u32)header->sections[reloc_target_idx].data+relocs[i].sym_offset;
		} else {
			//Use import symbol table to calculte external symbol address
			sym_addr = (u32)header->import_symbols[reloc_target_idx].ptr;
		}
		switch(type) {
			case R_MIPS_32:
			//Direct relocation
				*target_addr += sym_addr;
				break;
				
			case R_MIPS_26:
			//Jump relocation
			{
				//Calculate jump target from instruction and address
				u32 target = ((*target_addr & 0x3FFFFFF) << 2)|((u32)target_addr & 0xF0000000);
				target += sym_addr; //Relocate jump target
				//Update jump target
				*target_addr = (*target_addr & 0xFC000000)|((target & 0xFFFFFFC) >> 2);
			}
				break;
				
			case R_MIPS_HI16:
			{
				u16 hi_orig = *target_addr & 0xFFFF; //Read high half from instruction
				//Set parameters for address calculation
				u32 addr = hi_orig << 16;
				u16 hi = hi_orig;
				//Loop over further relocations for R_MIPS_LO16 relocation
				for(u32 j=i+1; j<num_relocs; j++) {
					u8 next_type = relocs[j].info >> 26;
					if(next_type == R_MIPS_LO16) {
						//Found R_MIPS_LO16 relocation
						//Calculate low half target address
						u32 *lo_target_addr = (u32 *)((u8 *)section_ptr->data+relocs[j].offset);
						u32 lo_target_idx = relocs[j].info & 0x3FFFFFF;
						u32 lo_sym_addr;
						u16 lo = *lo_target_addr & 0xFFFF; //Read low half from instruction
						//Calculate symbol address
						if(internal) {
							//Use section table and symbol offset to calculte internal symbol address
							lo_sym_addr = (u32)header->sections[lo_target_idx].data+relocs[j].sym_offset;
						} else {
							//Use import symbol table to calculte external symbol address
							lo_sym_addr = (u32)header->import_symbols[lo_target_idx].ptr;
						}
						addr += lo-((lo & 0x8000) << 1); //Calculate new address
						//Calculate new address and hi
						addr += lo_sym_addr;
						hi = (addr >> 16)+((addr & 0x8000) >> 15);
						break;
					}
				}
				//Update instruction
				*target_addr = (*target_addr & 0xFFFF0000)|hi;
			}
				break;
				
			case R_MIPS_LO16:
			{
				u16 lo = *target_addr & 0xFFFF; //Read lo from instruction
				lo += sym_addr; //Calculate new lo
				//Update instruction
				*target_addr = (*target_addr & 0xFFFF0000)|lo;
			}
				break;
				
			default:
				debug_printf("Invalid relocation type %d.\n", type);
				abort();
				break;
		}
	}
	osWritebackDCache(section_ptr->data, section_ptr->size);
	osInvalICache(section_ptr->data, section_ptr->size);
}

static void RunConstructors(USOHeader *header)
{
	USOSection *ctor_section = &header->sections[header->ctor_section];
	//Run only if ctors exist
	if(ctor_section->data) {
		//Get start/end ctor pointers
		FuncPtr *ctor_start = ctor_section->data;
		FuncPtr *ctor_end = (FuncPtr *)((u8 *)ctor_section->data+ctor_section->size);
		FuncPtr *ctor_curr = ctor_end-1; //Ctors start running at end-1
		//Loop through ctors
		while(ctor_curr >= ctor_start) {
			(*ctor_curr)();
			ctor_curr--;
		}
	}
}

static void RunDestructors(USOHeader *header)
{
	USOSection *dtor_section = &header->sections[header->dtor_section];
	//Run only if dtors exist
	if(dtor_section->data) {
		//Get start/end dtor pointers
		FuncPtr *dtor_start = dtor_section->data;
		FuncPtr *dtor_end = (FuncPtr *)((u8 *)dtor_section->data+dtor_section->size);
		FuncPtr *dtor_curr = dtor_start; //Dtors start from beginning
		//Loop through dtors
		while(dtor_curr < dtor_end) {
			(*dtor_curr)();
			dtor_curr++;
		}
	}
}

static void LinkUSO(USOHeader *header, u8 *bss)
{
	PatchSectionTable(header, bss);
	PatchExportSymTable(header);
	PatchImportSymTable(header);
	//Loop through section relocations
	for(u32 i=0; i<header->num_sections; i++) {
		ApplySectionRelocations(header, i, false);
		ApplySectionRelocations(header, i, true);
	}
}

void USOInit()
{
	//Open global symbols
	FSFile file;
	if(!FSOpen(&file, "uso/global_syms.bin")) {
		debug_printf("Failed to open uso/global_syms.bin.\n");
		abort();
		return;
	}
	//Read global symbol count
	FSRead(&file, &num_global_syms, 4);
	//Read rest of global symbols
	s32 size = FSGetSize(&file)-4;
	global_syms = malloc(size);
	FSRead(&file, global_syms, size);
	//Fixup global symbols using dummy section table
	FixupExportSymbols(global_syms, num_global_syms, &dummy_section_table, 1);
	//Initialize handle list
	uso_handle_list_head = uso_handle_list_tail = NULL;
}

EXPORT_SYMBOL USOHandle USOGetHandle(const char *name)
{
	USOHandle handle = uso_handle_list_head;
	//Loop through handles for one with name of name
	while(handle) {
		if(!strcmp(handle->name, name)) {
			//USO Handle with name found
			return handle;
		}
		//Go to next handle
		handle = handle->next;
	}
	//Handle not found
	return NULL;
}

EXPORT_SYMBOL void USOPin(USOHandle handle)
{
	debug_assert(handle && "Invalid USO handle.\n");
	//Increment reference count to pin
	handle->ref_count++;
}

EXPORT_SYMBOL USOHandle USOGetAddrHandle(void *ptr)
{
	USOHandle handle = uso_handle_list_head;
	//Loop through handles for one with ptr inside address range
	while(handle) {
		void *min_addr = handle->uso;
		void *max_addr = (u8 *)min_addr+handle->uso_size;
		if(ptr >= min_addr && ptr < max_addr) {
			//Found USO handle with valid range
			return handle;
		}
		//Go to next handle
		handle = handle->next;
	}
	//Handle not found
	return NULL;
}

EXPORT_SYMBOL USOHandle USOOpen(const char *name)
{
	USOHandle handle = USOGetHandle(name);
	if(handle) {
		//Increment reference count if handle exists
		handle->ref_count++;
		return handle;
	}
	FSFile file;
	//Generate path for USO file
	char path[USO_STRBUF_SIZE+1];
	path[USO_STRBUF_SIZE] = 0;
	snprintf(path, USO_STRBUF_SIZE-1, "uso/%s.uso", name);
	//Open USO file
	if(!FSOpen(&file, path)) {
		debug_printf("Failed to open shared object %s.\n", path);
		return NULL;
	}
	struct uso_load_info load_info ALIGN_SYMBOL(8); 
	//Allocate handle
	handle = malloc(sizeof(struct uso_handle_data)-1+strlen(name)+1);
	debug_assert(handle != NULL); //Make sure USO handle is allocated successfully
	strcpy(handle->name, name); //Set USO name
	FSRead(&file, &load_info, sizeof(struct uso_load_info)); //Read load info
	u32 noload_offset = AlignTo(load_info.size, load_info.noload_align); //At load_info.size aligned properly
	u32 total_size = load_info.noload_size+noload_offset; //At end of noload part
	//Get maximum align of noload and non-noload part
	u32 align = load_info.align;
	if(load_info.noload_align > align) {
		align = load_info.noload_align;
	}
	handle->uso_size = total_size; //Mark USO size
	//Allocate and clear USO
	handle->uso = memalign(align, total_size);
	debug_assert(handle->uso != NULL); //Make sure USO is allocated successfully
	memset(handle->uso, 0, total_size);
	FSRead(&file, handle->uso, load_info.size); //Read loaded part of USO
	LinkUSO(handle->uso, (u8 *)handle->uso+noload_offset); //Link USO
	RunConstructors(handle->uso); //Run global constructors
	handle->ref_count = 1; //Initialize reference count
	LinkToUSOList(handle); //Add USO handle to list
	return handle; //Return USO handle
}

EXPORT_SYMBOL void *USOFindSymbol(USOHandle handle, const char *name)
{
	if(!handle) {
		//Search all USOs
		return SearchSymbol(name, false);
	} else {
		//Search USO specified by handle
		USOSymbol *symbol = SearchExportSymbolUSO(handle->uso, name);
		if(!symbol) {
			//Symbol not found
			return NULL;
		}
		//Return symbol pointer
		return symbol->ptr;
	}
}

EXPORT_SYMBOL void USOCloseNow(USOHandle handle)
{
	debug_assert(handle && "Invalid USO handle.\n");
	RunDestructors(handle->uso); //Call USO destructors
	UnlinkFromUSOList(handle); //Remove USO handle from list
	//Free USO and handle
	free(handle->uso);
	free(handle);
}

EXPORT_SYMBOL void USOClose(USOHandle handle)
{
	debug_assert(handle && "Invalid USO handle.\n");
	//Close USO for real if reference count is zero
	if(--handle->ref_count == 0) {
		USOCloseNow(handle);
	}
}

EXPORT_SYMBOL USOHandle USOGetFirstHandle()
{
	//Return head of USO handle list
	return uso_handle_list_head;
}

EXPORT_SYMBOL USOHandle USOGetNextHandle(USOHandle handle)
{
	debug_assert(handle && "Invalid USO handle.\n");
	//Return next handle in USO handle list
	return handle->next;
}

EXPORT_SYMBOL USOHandleInfo USOGetHandleInfo(USOHandle handle)
{
	USOHandleInfo handle_info;
	debug_assert(handle && "Invalid USO handle.\n");
	//Get USO handle info
	handle_info.name = handle->name;
	handle_info.handle = handle;
	handle_info.ref_count = handle->ref_count;
	handle_info.min_addr = handle->uso;
	handle_info.max_addr = (u8 *)handle_info.min_addr+handle->uso_size;
	return handle_info; //Return USO handle info
}