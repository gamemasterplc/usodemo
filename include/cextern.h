#pragma once

//All of these are only needed for functions

#ifdef __cplusplus
#define START_EXTERN_C extern "C" {
#else
#define START_EXTERN_C
#endif

#ifdef __cplusplus
#define END_EXTERN_C extern "C" {
#else
#define END_EXTERN_C
#endif

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif