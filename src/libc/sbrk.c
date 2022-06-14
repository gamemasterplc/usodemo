#include <ultra64.h>

//Linker script export
extern char __heap_start[];

//Copied from libdragon
void *sbrk(int incr)
{
    static char *heap_end = 0;
    static char *heap_top = 0;
    char *prev_heap_end;
	
	//Disable interrupts
	OSIntMask prev_mask = osSetIntMask(OS_IM_NONE);
	
    if(heap_end == 0)
    {
		//Initialize heap ranges
        heap_end = __heap_start;
        heap_top = OS_PHYSICAL_TO_K0(osMemSize);
    }
	//Increment heap
    prev_heap_end = heap_end;
    heap_end += incr;

    // check if out of memory
    if(heap_end > heap_top)
    {
        heap_end -= incr;
        prev_heap_end = (char *)-1;
    }
	//Reenable interrupts
	osSetIntMask(prev_mask);
	
    return (void *)prev_heap_end;
}