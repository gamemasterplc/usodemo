#pragma once

#include <ultra64.h>
#include <cextern.h>
#include <stdarg.h>

EXTERN_C int vsnprintf(char *str, size_t n, const char *format, va_list arg);
EXTERN_C int snprintf(char *str, size_t n, const char *format, ...);
EXTERN_C int vsprintf(char *str, const char *format, va_list arg);