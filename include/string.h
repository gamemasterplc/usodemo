#pragma once

#include <cextern.h>
#include <ultra64.h>

EXTERN_C void *memcpy(void *dst, const void *src, size_t size);
EXTERN_C void *memset(void *str, int c, size_t n);
EXTERN_C int memcmp(const void *str1, const void *str2, size_t n);
EXTERN_C int strncmp(const unsigned char *str1, const unsigned char *str2, size_t n);
EXTERN_C int strcmp(const unsigned char *str1, const unsigned char *str2);
EXTERN_C size_t strlen(const char *str);
EXTERN_C char *strchr(const char *str, int ch);
EXTERN_C char *strcpy(char *dest, const char *src);