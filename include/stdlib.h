#pragma once

#include <cextern.h>
#include <ultra64.h>
#include <malloc.h>

EXTERN_C void *bsearch(const void *key, const void *base0, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
EXTERN_C void abort();