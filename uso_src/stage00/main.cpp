#include <util.h>
#include <pad.h>
#include <gfx.h>
#include <debug.h>
#include <malloc.h>
#include <stage.h>
#include <symbol_attr.h>
#include "zoom_camera.h"

//Globals for stage
static ZoomCamera camera;
static void *bg_buf;

static void init()
{
	//Load background image
	bg_buf = FileLoad("stage00bg.bin");
	debug_assert(bg_buf && "Failed to load stage00bg.bin");
}

static void main()
{
	camera.Update();
	//Wait for A Button to toggle stage
	if(PadGetPressedButtons(0) & A_BUTTON) {
		StageToggle();
	}
}

static void draw()
{
	float x = camera.GetX();
	float y = camera.GetY();
	float zoom = camera.GetZoom();
	//Load background texture as a 32x32 RGBA16 repeating texture
	gDPLoadTextureBlock(gfx_dlistp++, bg_buf, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0, G_TX_WRAP, G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
	//Fill screen with texture zoomed and offsetted in texture space appropriately
	gSPTextureRectangle(gfx_dlistp++, 0, 0, gfx_cfb_w*4, gfx_cfb_h*4, G_TX_RENDERTILE,
		(x-((gfx_cfb_w/2)/zoom))*32, (y-((gfx_cfb_h/2)/zoom))*32, 1024/zoom, 1024/zoom);
	//Tell RDP that texture is done drawing
	gDPPipeSync(gfx_dlistp++);
}

static void destroy()
{
	//Free background image
	free(bg_buf);
}

//Export stage functions list
EXPORT_SYMBOL StageFuncs stage_funcs { init, main, draw, destroy };