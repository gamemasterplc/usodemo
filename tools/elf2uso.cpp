#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string>
#include <iostream>
#include <map>
#include <algorithm>
#include <vector>
#include <elfio/elfio.hpp>

struct USOReloc {
    uint32_t offset;
    uint32_t info; //Upper 6 bits are relocation type, lower 26 bits are either symbol or section index
    uint32_t sym_offset; //Section-relative symbol offset, zero for external relocations
};

struct USOSection {
    ELFIO::Elf_Half reloc_elf_section;
    std::vector<USOReloc> internal_relocs;
    std::vector<USOReloc> external_relocs;
    const char* data;
    size_t size;
    size_t align;
};

struct USOSymbol {
    ELFIO::Elf_Word src_symbol;
    std::string name;
    size_t section;
    ELFIO::Elf64_Addr addr;
};

std::map<ELFIO::Elf_Half, size_t> uso_section_map;
std::vector<USOSection> uso_sections;

std::vector<USOSymbol> uso_import_symbols;
std::vector<USOSymbol> uso_export_symbols;
std::map<ELFIO::Elf_Word, size_t> import_sym_map;

ELFIO::elfio elf_reader;
ELFIO::Elf_Half elf_symbol_sec_index;

ELFIO::Elf_Half FindELFSection(std::string name)
{
    for (ELFIO::Elf_Half i = 0; i < elf_reader.sections.size(); i++) {
        if (elf_reader.sections[i]->get_name() == name) {
            return i;
        }
    }
    //ELFIO::SHN_UNDEF is never a valid section index
    return ELFIO::SHN_UNDEF;
}

bool IsElfValid()
{
    //Check if symbol table can be found
    elf_symbol_sec_index = FindELFSection(".symtab");
    if (elf_symbol_sec_index == ELFIO::SHN_UNDEF) {
        std::cout << "ELF file is missing symbol table." << std::endl;
        return false;
    }
    return elf_reader.get_class() == ELFIO::ELFCLASS32 //Check for 32-bit ELF
        && elf_reader.get_encoding() == ELFIO::ELFDATA2MSB //Check for Big-Endian Platform
        && elf_reader.get_machine() == ELFIO::EM_MIPS //Check for MIPS platform
        && elf_reader.get_type() == ELFIO::ET_REL; //Check for relocatable ELF
}

bool CompareSymbol(USOSymbol& first, USOSymbol& second)
{
    return first.name < second.name;
}

void SortSymbols()
{
    std::sort(uso_import_symbols.begin(), uso_import_symbols.end(), CompareSymbol);
    std::sort(uso_export_symbols.begin(), uso_export_symbols.end(), CompareSymbol);
}

void CollectSymbols()
{
    ELFIO::symbol_section_accessor sym_accessor(elf_reader, elf_reader.sections[elf_symbol_sec_index]);
    for (ELFIO::Elf_Xword i = 0; i < sym_accessor.get_symbols_num(); i++) {
        std::string name;
        ELFIO::Elf64_Addr value;
        ELFIO::Elf_Xword size;
        unsigned char bind;
        unsigned char type;
        ELFIO::Elf_Half section_index;
        unsigned char other;
        sym_accessor.get_symbol(i, name, value, size, bind, type, section_index, other);
        //Skip local symbols
        if (bind == ELFIO::STB_LOCAL) {
            continue;
        }
        if (section_index == ELFIO::SHN_UNDEF) {
            USOSymbol symbol;
            //Populate import symbol
            symbol.name = name;
            symbol.section = 0; //Import symbols have no section
            symbol.addr = value;
            symbol.src_symbol = i;
            uso_import_symbols.push_back(symbol); //Add import symbol
        } else {
            //Only add symbols with default visibility for export
            if (other == ELFIO::STV_DEFAULT) {
                USOSymbol symbol;
                //Populate export symbol
                symbol.name = name;
                if (uso_section_map.find(section_index) == uso_section_map.end()) {
                    symbol.section = 0; //Fallback to section 0 if section index cannot be found
                } else {
                    symbol.section = uso_section_map[section_index]; //Lookup section index in map
                }
                symbol.addr = value;
                uso_export_symbols.push_back(symbol);
            }
        }
    }
    SortSymbols();
	//Generate mapping for import symbols
    for (size_t i = 0; i < uso_import_symbols.size(); i++) {
        import_sym_map[uso_import_symbols[i].src_symbol] = i;
    }
}

void CollectUSOSections()
{
    USOSection section_data;
    //Push absolute fallback section
    section_data.reloc_elf_section = ELFIO::SHN_UNDEF;
    section_data.data = NULL;
    section_data.size = 0;
    section_data.align = 0;
    uso_section_map[ELFIO::SHN_UNDEF] = uso_sections.size();
    uso_sections.push_back(section_data);
    for (ELFIO::Elf_Half i = 0; i < elf_reader.sections.size(); i++) {
        ELFIO::Elf64_Word type = elf_reader.sections[i]->get_type();
        //Push only SHT_PROGBITS or SHT_NOBITS sections
        if (type == ELFIO::SHT_PROGBITS || type == ELFIO::SHT_NOBITS) {
            uso_section_map[i] = uso_sections.size(); //Add section mapping parameter
            //Add section data info
            section_data.size = elf_reader.sections[i]->get_size();
            section_data.align = elf_reader.sections[i]->get_addr_align();
            if (type == ELFIO::SHT_NOBITS) {
                //SHT_NOBITS sections have no relocation data or data
                section_data.reloc_elf_section = ELFIO::SHN_UNDEF;
                section_data.data = NULL;
            } else {
                //Relocation section name is .rel#name for a section with a name of name if one exists
                std::string reloc_sec_name = ".rel" + elf_reader.sections[i]->get_name();
                section_data.reloc_elf_section = FindELFSection(reloc_sec_name);
                section_data.data = elf_reader.sections[i]->get_data();
            }
            uso_sections.push_back(section_data);
        }
    }
}

void GenerateRelocs()
{
    for (size_t i = 0; i < uso_sections.size(); i++) {
        if (uso_sections[i].reloc_elf_section != ELFIO::SHN_UNDEF) {
            ELFIO::relocation_section_accessor reloc_accessor(elf_reader, elf_reader.sections[uso_sections[i].reloc_elf_section]);
            for (ELFIO::Elf_Xword j = 0; j < reloc_accessor.get_entries_num(); j++) {
                USOReloc reloc_tmp;
                //Temporaries for relocation
                ELFIO::Elf64_Addr offset;
                ELFIO::Elf_Word symbol;
                unsigned char type;
                ELFIO::Elf_Sxword addend;
                reloc_accessor.get_entry(j, offset, symbol, type, addend);
                //Write known fields
                reloc_tmp.offset = offset;
                reloc_tmp.info = (type << 26);
                {
                    //Read symbol relocation is accessing
                    ELFIO::symbol_section_accessor sym_accessor(elf_reader, elf_reader.sections[elf_symbol_sec_index]);
                    //Temporaries for symbol
                    std::string sym_name;
                    ELFIO::Elf64_Addr sym_value;
                    ELFIO::Elf_Xword sym_size;
                    unsigned char sym_bind;
                    unsigned char sym_type;
                    ELFIO::Elf_Half sym_section;
                    unsigned char sym_other;
                    sym_accessor.get_symbol(symbol, sym_name, sym_value, sym_size, sym_bind, sym_type, sym_section, sym_other);
                    if (sym_section == ELFIO::SHN_UNDEF) {
                        //Symbol references undefined symbol
                        reloc_tmp.info |= import_sym_map[symbol] & 0x3FFFFFF; //Write import symbol ID
                        reloc_tmp.sym_offset = 0; //Assume 0 symbol offset for these symbols
                        uso_sections[i].external_relocs.push_back(reloc_tmp); //Write external relocation
                    } else {
                        reloc_tmp.info |= uso_section_map[sym_section] & 0x3FFFFFF; //Write section ID
                        reloc_tmp.sym_offset = sym_value; //Use value as symbol offset
                        uso_sections[i].internal_relocs.push_back(reloc_tmp); //Write internal relocation
                    }
                }
            }
        }
    }
}

bool CheckExistCommonSymbols()
{
    ELFIO::symbol_section_accessor sym_accessor(elf_reader, elf_reader.sections[elf_symbol_sec_index]);
    for (ELFIO::Elf_Xword i = 0; i < sym_accessor.get_symbols_num(); i++) {
        std::string name;
        ELFIO::Elf64_Addr value;
        ELFIO::Elf_Xword size;
        unsigned char bind;
        unsigned char type;
        ELFIO::Elf_Half section_index;
        unsigned char other;
        sym_accessor.get_symbol(i, name, value, size, bind, type, section_index, other);
        if (bind != ELFIO::STB_LOCAL && section_index == ELFIO::SHN_COMMON) {
            //Terminate if any non-local symbol is found using section SHN_COMMON
            return true;
        }
    }
    //No non-local symbol found in SHN_COMMON
    return false;
}

bool CheckNullAbsSymbol()
{
    ELFIO::symbol_section_accessor sym_accessor(elf_reader, elf_reader.sections[elf_symbol_sec_index]);
    for (ELFIO::Elf_Xword i = 0; i < sym_accessor.get_symbols_num(); i++) {
        std::string name;
        ELFIO::Elf64_Addr value;
        ELFIO::Elf_Xword size;
        unsigned char bind;
        unsigned char type;
        ELFIO::Elf_Half section_index;
        unsigned char other;
        sym_accessor.get_symbol(i, name, value, size, bind, type, section_index, other);
        //Skip checking local symbols
        if (bind == ELFIO::STB_LOCAL) {
            continue;
        }
        //Check for out of range section with value of 0
        if (section_index >= elf_reader.sections.size()) {
            if (value == 0) {
                return true;
            }
        }
        //Check for non-external symbol in defined non-progbits and non-nobits section with value of 0
        ELFIO::Elf64_Word section_type = elf_reader.sections[section_index]->get_type();
        if (section_index != ELFIO::SHN_UNDEF && section_type != ELFIO::SHT_PROGBITS && section_type != ELFIO::SHT_NOBITS) {
            if (value == 0) {
                return true;
            }
        }
    }
    //No symbol found with 0 value in
    return false;
}

uint32_t AlignU32(uint32_t val, uint32_t to)
{
    //Only supports power of 2 alignment
    return (val + to - 1) & ~(to - 1);
}

void WriteU8(FILE* file, uint8_t value)
{
    //Forward value to fwrite
    fwrite(&value, 1, 1, file);
}

void WriteU16(FILE* file, uint16_t value)
{
    //Write in big-endian order
    uint8_t bytes[2];
    bytes[0] = value >> 8;
    bytes[1] = value & 0xFF;
    fwrite(&bytes, 1, 2, file);
}

void WriteU32(FILE* file, uint32_t value)
{
    //Write in big-endian order
    uint8_t bytes[4];
    bytes[0] = value >> 24;
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = value & 0xFF;
    fwrite(&bytes, 1, 4, file);
}

void PadUSO(FILE* file, uint32_t to)
{
    uint32_t curr_ofs = ftell(file) - 16; //Skip 16 bytes for load info
    //Align current file offset
    while (curr_ofs % to != 0) {
        WriteU8(file, 0);
        curr_ofs++;
    }
}

uint32_t GetImportSymbolsSize()
{
    uint32_t size = 0xC * uso_import_symbols.size(); //Import symbol table size
    //Add sum of name sizes to import symbol table size
    for (size_t i = 0; i < uso_import_symbols.size(); i++) {
        size += uso_import_symbols[i].name.length() + 1;
    }
    return size;
}

uint32_t GetExportSymbolsSize()
{
    uint32_t size = 0xC * uso_export_symbols.size(); //Export symbol table size
    //Add sum of name sizes to export symbol table size
    for (size_t i = 0; i < uso_export_symbols.size(); i++) {
        size += uso_export_symbols[i].name.length() + 1;
    }
    return size;
}

uint32_t GetUSOAlignment()
{
    uint32_t align = 4;
    //Find maximum alignment of section with loaded data
    for (size_t i = 0; i < uso_sections.size(); i++) {
        if (uso_sections[i].data && uso_sections[i].align > align) {
            align = uso_sections[i].align;
        }
    }
    return align;
}

uint32_t GetUSONoloadAlignment()
{
    uint32_t align = 1;
    //Find maximum alignment of section with no loaded data
    for (size_t i = 0; i < uso_sections.size(); i++) {
        if (!uso_sections[i].data && uso_sections[i].size != 0 &&  uso_sections[i].align > align) {
            align = uso_sections[i].align;
        }
    }
    return align;
}

uint32_t GetUSONoloadSize()
{
    uint32_t size = 0;
    //Sum up sizes of sections with no loaded data but with a size
    for (size_t i = 0; i < uso_sections.size(); i++) {
        if (!uso_sections[i].data && uso_sections[i].size != 0) {
            size += uso_sections[i].size;
        }
    }
    return size;
}

int WriteUSO(char *path)
{
    FILE* file = fopen(path, "wb");
    if (!file) {
        std::cout << "Failed to open " << path << " for writing." << std::endl;
        return false;
    }
    uint32_t uso_alignment = GetUSOAlignment();
    fseek(file, 16, SEEK_SET); //Seek
    //Calculate USO offsets
    uint32_t section_ofs = 0x1C;
    uint32_t imports_ofs = section_ofs + (0x1C * uso_sections.size());
    uint32_t exports_ofs = AlignU32(imports_ofs + GetImportSymbolsSize(), 4);
    uint32_t data_ofs = AlignU32(exports_ofs + GetExportSymbolsSize(), uso_alignment);
    //Write USO Header
    WriteU32(file, uso_sections.size());
    WriteU32(file, section_ofs);
    WriteU32(file, uso_import_symbols.size());
    WriteU32(file, imports_ofs);
    WriteU32(file, uso_export_symbols.size());
    WriteU32(file, exports_ofs);
    WriteU16(file, uso_section_map[FindELFSection(".ctors")]);
    WriteU16(file, uso_section_map[FindELFSection(".dtors")]);
    uint32_t data_size = 0;
    //Calculate size of loaded data part
    for (size_t i = 0; i < uso_sections.size(); i++) {
        if (uso_sections[i].data) {
            //Only consider sections with data loaded
            data_size = AlignU32(data_size, uso_sections[i].align);
            data_size += uso_sections[i].size;
        }
    }
    //Align relocation offset to 4 bytes
    uint32_t reloc_ofs = AlignU32(data_ofs + data_size, 4);
    //Write section info
    for (size_t i = 0; i < uso_sections.size(); i++) {
        if (uso_sections[i].data) {
            //Write aligned data offset
            data_ofs = AlignU32(data_ofs, uso_sections[i].align);
            WriteU32(file, data_ofs);
        } else {
            //No loaded data in section
            WriteU32(file, 0);
        }
        //Write size and align fields
        WriteU32(file, uso_sections[i].size);
        WriteU32(file, uso_sections[i].align);
        //Write internal relocation size
        WriteU32(file, uso_sections[i].internal_relocs.size());
        //Write internal relocation offset
        if (uso_sections[i].internal_relocs.size() == 0) {
            WriteU32(file, 0);
        } else {
            WriteU32(file, reloc_ofs);
            reloc_ofs += 0xC * uso_sections[i].internal_relocs.size(); //Advance relocation pointer
        }
        WriteU32(file, uso_sections[i].external_relocs.size());
        //Write external relocation offset
        if (uso_sections[i].external_relocs.size() == 0) {
            WriteU32(file, 0);
        } else {
            WriteU32(file, reloc_ofs);
            reloc_ofs += 0xC * uso_sections[i].external_relocs.size(); //Advance relocation pointer
        }
        //Advance data pointer
        if (uso_sections[i].data) {
            data_ofs += uso_sections[i].size;
        }
    }
    //Write import symbol table
    uint32_t name_ofs = 0xC * uso_import_symbols.size();
    for (size_t i = 0; i < uso_import_symbols.size(); i++) {
        WriteU32(file, name_ofs); //Name offset
        WriteU32(file, uso_import_symbols[i].addr); //Address
        WriteU32(file, uso_import_symbols[i].section); //Section
        name_ofs += uso_import_symbols[i].name.length() + 1; //Advance name offset
    }
    //Write import symbol table names
    for (size_t i = 0; i < uso_import_symbols.size(); i++) {
        fwrite(uso_import_symbols[i].name.c_str(), 1, uso_import_symbols[i].name.length() + 1, file);
    }
    //Align for export table
    PadUSO(file, 4);
    //Write export symbol table
    name_ofs = 0xC * uso_export_symbols.size();
    for (size_t i = 0; i < uso_export_symbols.size(); i++) {
        WriteU32(file, name_ofs); //Name offset
        WriteU32(file, uso_export_symbols[i].addr); //Address
        WriteU32(file, uso_export_symbols[i].section); //Section
        name_ofs += uso_export_symbols[i].name.length() + 1; //Advance name offset
    }
    //Write import symbol table names
    for (size_t i = 0; i < uso_export_symbols.size(); i++) {
        fwrite(uso_export_symbols[i].name.c_str(), 1, uso_export_symbols[i].name.length() + 1, file);
    }
    PadUSO(file, uso_alignment); //Pad for section data
    //Write sections with data to file
    for (size_t i = 0; i < uso_sections.size(); i++) {
        if (uso_sections[i].data) {
            PadUSO(file, uso_sections[i].align); //Align data properly
            fwrite(uso_sections[i].data, 1, uso_sections[i].size, file); //Write data
        }
    }
    //Align for relocations
    PadUSO(file, 4);
    //Write relocations
    for (size_t i = 0; i < uso_sections.size(); i++) {
        for (size_t j = 0; j < uso_sections[i].internal_relocs.size(); j++) {
            WriteU32(file, uso_sections[i].internal_relocs[j].offset);
            WriteU32(file, uso_sections[i].internal_relocs[j].info);
            WriteU32(file, uso_sections[i].internal_relocs[j].sym_offset);
        }
        for (size_t j = 0; j < uso_sections[i].external_relocs.size(); j++) {
            WriteU32(file, uso_sections[i].external_relocs[j].offset);
            WriteU32(file, uso_sections[i].external_relocs[j].info);
            WriteU32(file, uso_sections[i].external_relocs[j].sym_offset);
        }
    }
    uint32_t uso_size = ftell(file) - 16;
    //Write USO load info
    fseek(file, 0, SEEK_SET);
    WriteU32(file, uso_alignment); //Alignment
    WriteU32(file, uso_size); //Size
    WriteU32(file, GetUSONoloadAlignment()); //Noload alignment
    WriteU32(file, GetUSONoloadSize()); //Noload size
    fclose(file);
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " elf_input uso_output" << std::endl;
        return 1;
    }
    if (!elf_reader.load(argv[1])) {
        std::cout << "Failed to read input ELF file." << std::endl;
        return 1;
    }
    //Do elf sanity checks
    if (!IsElfValid()) {
        std::cout << "Input ELF file is not valid relocatable N64 ELF file." << std::endl;
        return 1;
    }
    if (CheckExistCommonSymbols()) {
        std::cout << "Common section symbols disallowed in input ELF file." << std::endl;
        return 1;
    }
    if (CheckNullAbsSymbol()) {
        std::cout << "Symbol not mapped to SHT_PROGBITS, SHT_NOBITS, or SHT_NULL section has value of zero." << std::endl;
        return 1;
    }
    //Prepare for writing USO
    CollectUSOSections();
    CollectSymbols();
    GenerateRelocs();
    //Write USO and return write status
    return !WriteUSO(argv[2]);
}