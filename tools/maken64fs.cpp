#define _CRT_SECURE_NO_WARNINGS //Required to compile on Visual Stud
#include <stdio.h>
#if __cplusplus >= 201703L && __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error "No filesystem libraries exist"
#endif
#include <string>
#include <algorithm>
#include <vector>
#include <iostream>

struct FileInfo {
    std::string path;
    uint8_t* file_buf;
    size_t size;
};

std::vector<FileInfo> files_list;

bool CompareFile(FileInfo& first, FileInfo& second)
{
    return first.path < second.path;
}

void CollectFiles(std::string root_dir)
{
    for (auto const& entry : fs::recursive_directory_iterator(root_dir)) {
        if (fs::is_regular_file(entry.status())) {
            FileInfo file_info;
            std::string temp_path = entry.path().string();
            file_info.size = fs::file_size(entry.path()); //Get file size
            //Read file into allocated buffer
            FILE* file = fopen(temp_path.c_str(), "rb");
            file_info.file_buf = new uint8_t[file_info.size];
            fread(file_info.file_buf, 1, file_info.size, file);
            fclose(file);
            //Remove initial part of path
            if (root_dir.length() != 0) {
                temp_path = temp_path.substr(root_dir.length() + 1);
            }
            //Fix slashes in path
            std::replace(temp_path.begin(), temp_path.end(), '\\', '/');
            file_info.path = temp_path;
            //Add file data
            files_list.push_back(file_info);
        }
    }
    std::sort(files_list.begin(), files_list.end(), CompareFile);
}

void CleanFiles()
{
    for (size_t i = 0; i < files_list.size(); i++) {
        delete[] files_list[i].file_buf;
    }
}

uint32_t GetStringTableSize()
{
    uint32_t size = 0;
    for (size_t i = 0; i < files_list.size(); i++) {
        size += files_list[i].path.length() + 1;
    }
    return (size + 1) & ~1;
}

void WriteU32(FILE* file, uint32_t value)
{
    uint8_t temp[4];
    temp[0] = value >> 24;
    temp[1] = (value >> 16) & 0xFF;
    temp[2] = (value >> 8) & 0xFF;
    temp[3] = value & 0xFF;
    fwrite(temp, 4, 1, file);
}

bool WriteFilesystem(std::string root_dir, char* dest)
{
    FILE* file = fopen(dest, "wb");
    if (!file) {
        std::cout << "Failed to open " << dest << " for writing." << std::endl;
        CleanFiles();
        return false;
    }
    uint32_t str_offset = 12 * files_list.size(); //After last file but relative to after 8-byte header
    uint32_t rom_data_ofs = 12 + str_offset + GetStringTableSize();
	WriteU32(file, 0x46533634); //Write Magic
    WriteU32(file, files_list.size());
    WriteU32(file, GetStringTableSize());
    for (size_t i = 0; i < files_list.size(); i++) {
        WriteU32(file, str_offset);
        WriteU32(file, rom_data_ofs);
        WriteU32(file, files_list[i].size);
        rom_data_ofs += (files_list[i].size + 1) & ~1;
		str_offset += files_list[i].path.length() + 1;
    }
    for (size_t i = 0; i < files_list.size(); i++) {
        fwrite(files_list[i].path.c_str(), 1, files_list[i].path.length() + 1, file);
    }
    if (ftell(file) % 2 != 0) {
        uint8_t zero = 0;
        fwrite(&zero, 1, 1, file);
    }
    for (size_t i = 0; i < files_list.size(); i++) {
        fwrite(files_list[i].file_buf, 1, files_list[i].size, file);
        if (ftell(file) % 2 != 0) {
            uint8_t zero = 0;
            fwrite(&zero, 1, 1, file);
        }
    }
    CleanFiles();
    fclose(file);
    return true;
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " src_dir dst_file" << std::endl;
        return 1;
    }
    CollectFiles(argv[1]);
    return !WriteFilesystem(argv[1], argv[2]);
}