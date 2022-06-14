//Shut up visual studio
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <elfio/elfio.hpp>

struct USOSymbol {
    std::string name;
    size_t section;
    ELFIO::Elf64_Addr addr;
};

ELFIO::elfio elf_reader;
std::vector<USOSymbol> uso_symbols;
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
        && elf_reader.get_type() == ELFIO::ET_EXEC; //Check for non-relocatable ELF
}

bool CompareSymbol(USOSymbol& first, USOSymbol& second)
{
    return first.name < second.name;
}

bool CollectSymbols()
{
    ELFIO::symbol_section_accessor sym_accessor(elf_reader, elf_reader.sections[elf_symbol_sec_index]);
    for (ELFIO::Elf_Xword i = 0; i < sym_accessor.get_symbols_num(); i++) {
        USOSymbol tmp_symbol;
        std::string name;
        ELFIO::Elf64_Addr value;
        ELFIO::Elf_Xword size;
        unsigned char bind;
        unsigned char type;
        ELFIO::Elf_Half section_index;
        unsigned char other;
        sym_accessor.get_symbol(i, name, value, size, bind, type, section_index, other);
        //Skip local and non-default visibility symbols
        if (bind == ELFIO::STB_LOCAL || other != ELFIO::STV_DEFAULT) {
            continue;
        }
		if(value == 0) {
			std::cout << "Null symbol address disallowed." << std::endl;
			return false;
		}
        //Add symbol
        tmp_symbol.name = name;
        tmp_symbol.section = 0;
        tmp_symbol.addr = value;
        uso_symbols.push_back(tmp_symbol);
    }
    std::sort(uso_symbols.begin(), uso_symbols.end(), CompareSymbol);
	return true;
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

bool WriteSymbols(char *path)
{
    FILE* file = fopen(path, "wb");
    if (!file) {
        std::cout << "Failed to open " << path << " for writing." << std::endl;
        return false;
    }
    //Write symbol count and string size
    WriteU32(file, uso_symbols.size());
    uint32_t name_ofs = 0xC * uso_symbols.size(); //Base name offset
    //Write symbol data
    for (uint32_t i = 0; i < uso_symbols.size(); i++) {
        WriteU32(file, name_ofs);
        WriteU32(file, uso_symbols[i].addr);
		WriteU32(file, uso_symbols[i].section);
		name_ofs += uso_symbols[i].name.length() + 1;
    }
    //Write symbol names
    for (uint32_t i = 0; i < uso_symbols.size(); i++) {
        fwrite(uso_symbols[i].name.c_str(), 1, uso_symbols[i].name.length() + 1, file);
    }
	//Write padding byte
	if(ftell(file) % 2 != 0) {
		uint8_t zero = 0;
		fwrite(&zero, 1, 1, file);
	}
    fclose(file);
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " elf_input symbol_out" << std::endl;
        return 1;
    }
    if (!elf_reader.load(argv[1])) {
        std::cout << "Failed to load input ELF file." << std::endl;
        return 1;
    }
    //Do elf sanity checks
    if (!IsElfValid()) {
        std::cout << "Input ELF file is not valid non-relocatable N64 ELF file." << std::endl;
        return 1;
    }
    if(!CollectSymbols()) {
		return 1;
	}
    return !WriteSymbols(argv[2]);
}