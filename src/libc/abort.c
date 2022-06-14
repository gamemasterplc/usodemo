#include <stdlib.h>
#include <symbol_attr.h>
#include <debug.h>

EXPORT_SYMBOL void abort()
{
	debug_printf("Abort called\n");
	while(1); //Freeze system
}