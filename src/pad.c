#include <nusys.h>
#include <debug.h>
#include <symbol_attr.h>
#include "pad.h"

static u8 connected_pads;
static NUContData pad_data[NU_CONT_MAXCONTROLLERS];

EXPORT_SYMBOL void PadInit()
{
	connected_pads = nuContInit();
}

EXPORT_SYMBOL void PadUpdate()
{
	nuContDataGetExAll(pad_data);
}

EXPORT_SYMBOL bool PadIsConnected(int pad_num)
{
	debug_assert(pad_num < NU_CONT_MAXCONTROLLERS);
	//Check if requested pad bit is enabled
	return (connected_pads & (1 << pad_num)) != 0;
}

EXPORT_SYMBOL u16 PadGetHeldButtons(int pad_num)
{
	debug_assert(pad_num < NU_CONT_MAXCONTROLLERS);
	return pad_data[pad_num].button;
}

EXPORT_SYMBOL s8 PadGetStickX(int pad_num)
{
	debug_assert(pad_num < NU_CONT_MAXCONTROLLERS);
	return pad_data[pad_num].stick_x;
}

EXPORT_SYMBOL s8 PadGetStickY(int pad_num)
{
	debug_assert(pad_num < NU_CONT_MAXCONTROLLERS);
	return pad_data[pad_num].stick_y;
}

EXPORT_SYMBOL u16 PadGetPressedButtons(int pad_num)
{
	debug_assert(pad_num < NU_CONT_MAXCONTROLLERS);
	return pad_data[pad_num].trigger;
}