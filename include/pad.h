#pragma once

#include <ultra64.h>
#include <stdbool.h>
#include <cextern.h>

EXTERN_C void PadInit();
EXTERN_C void PadUpdate();
EXTERN_C bool PadIsConnected(int pad_num);
EXTERN_C u16 PadGetHeldButtons(int pad_num);
EXTERN_C s8 PadGetStickX(int pad_num);
EXTERN_C s8 PadGetStickY(int pad_num);
EXTERN_C u16 PadGetPressedButtons(int pad_num);