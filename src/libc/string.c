#include <string.h>
#include <symbol_attr.h>

EXPORT_SYMBOL void *memset(void *str, int c, size_t n)
{
	char *ptr = str;
	for(size_t i=0; i<n; i++) {
		*ptr++ = c;
	}
	return ptr;
}

EXPORT_SYMBOL int memcmp(const void *str1, const void *str2, size_t n)
{
	const unsigned char *ptr1 = (const unsigned char *)str1;
	const unsigned char *ptr2 = (const unsigned char *)str2;
	for(size_t i=0; i<n; i++) {
		if(*ptr1 != *ptr2) {
			return *ptr1-*ptr2;
		}
		ptr1++;
		ptr2++;
	}
	return 0;
}

EXPORT_SYMBOL int strncmp(const unsigned char *str1, const unsigned char *str2, size_t n)
{
	for(size_t i=0; i<n; i++) {
		if(*str1 != *str2 || *str1 == 0) {
			return *str1-*str2;
		}
		str1++;
		str2++;
	}
	return 0;
}

EXPORT_SYMBOL int strcmp(const unsigned char *str1, const unsigned char *str2)
{
	while(*str1 == *str2 && *str1 != 0) {
		str1++;
		str2++;
	}
	return *str1-*str2;
}

EXPORT_SYMBOL char *strcpy(char *dest, const char *src)
{
	char *dest_orig = dest;
	while(*dest++ = *src++);
	return dest_orig;
}