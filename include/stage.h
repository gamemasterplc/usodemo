#pragma once

#include <cextern.h>

typedef void (*StageFunc)();

typedef struct stage_funcs {
	StageFunc init;
	StageFunc main;
	StageFunc draw;
	StageFunc destroy;
} StageFuncs;

EXTERN_C void StageToggle(); //Change stage to other stage