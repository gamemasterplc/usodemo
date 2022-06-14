#pragma once

#define GLIST_SIZE 8192
#define NUM_CFBS 3

#include <ultra64.h>
#include <cextern.h>

//Render mode defines
typedef enum gfx_render_mode {
	GFX_RENDER_RECT,
	GFX_RENDER_RECT_ALPHA,
	GFX_RENDER_IMAGE,
	GFX_RENDER_IMAGETINT
} GfxRenderMode;

//Forces alpha to 255
#define GFX_MAKE_COLOR(r, g, b) ((((u8)(r)) << 24)|(((u8)(g)) << 16)|(((u8)(b)) << 8)|255)
//Used for when alpha can be not 255
#define GFX_MAKE_COLOR_ALPHA(r, g, b, a) ((((u8)(r)) << 24)|(((u8)(g)) << 16)|(((u8)(b)) << 8)|((u8)(a)))

//Color defines
#define GFX_COLOR_BLACK GFX_MAKE_COLOR(0, 0, 0)
#define GFX_COLOR_WHITE GFX_MAKE_COLOR(255, 255, 255)
#define GFX_COLOR_RED GFX_MAKE_COLOR(255, 0, 0)
#define GFX_COLOR_GREEN GFX_MAKE_COLOR(0, 255, 0)
#define GFX_COLOR_BLUE GFX_MAKE_COLOR(0, 0, 255)
#define GFX_COLOR_YELLOW GFX_MAKE_COLOR(255, 255, 0)
#define GFX_COLOR_MAGENTA GFX_MAKE_COLOR(255, 0, 255)
#define GFX_COLOR_CYAN GFX_MAKE_COLOR(0, 255, 255)

//Externs
extern Gfx *gfx_dlistp;
extern int gfx_cfb_w, gfx_cfb_h; //Screen size

//Function definitions
EXTERN_C void GfxInit();
EXTERN_C void GfxSetScreenSize(int width, int height);
EXTERN_C void GfxSetClearColor(u8 r, u8 g, u8 b);
EXTERN_C void GfxSetScissor(int x, int y, int w, int h); //Set scissor clamped to screen
EXTERN_C void GfxResetScissor(); //Sets scissor to fullscreen
EXTERN_C void GfxStartFrame(); //Initialize rendering and clear screen
EXTERN_C void GfxSetRenderMode(GfxRenderMode mode);
EXTERN_C void GfxPutRect(int x, int y, int w, int h, u32 color);
EXTERN_C void GfxEndFrame(); //Ends task, does vsync, and waits for task done