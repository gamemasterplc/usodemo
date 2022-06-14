#include <nusys.h>
#include <malloc.h>
#include <gfx.h>
#include <debug.h>
#include <symbol_attr.h>

EXPORT_SYMBOL Gfx *gfx_dlistp;
EXPORT_SYMBOL int gfx_cfb_w, gfx_cfb_h;
static GfxRenderMode render_mode;
static u8 clear_r, clear_g, clear_b;
static u16 *cfb_table[NUM_CFBS];
static OSViMode vi_mode;
static Gfx glist[GLIST_SIZE];

static Vp viewport = {
	320 * 2, 240 * 2, G_MAXZ / 2, 0,
	320 * 2, 240 * 2, G_MAXZ / 2, 0,
};

static Gfx rspinit_dl[] = {
	gsSPViewport(&viewport),
	gsSPClearGeometryMode(G_SHADE | G_SHADING_SMOOTH | G_CULL_BOTH |
							G_FOG | G_LIGHTING | G_TEXTURE_GEN |
							G_TEXTURE_GEN_LINEAR | G_LOD),
	gsSPTexture(0, 0, 0, 0, G_OFF),
	gsSPEndDisplayList(),
};

static Gfx rdpinit_dl[] = {
	gsDPSetCycleType(G_CYC_1CYCLE),
	gsDPPipelineMode(G_PM_1PRIMITIVE),
	gsDPSetCombineMode(G_CC_PRIMITIVE, G_CC_PRIMITIVE),
	gsDPSetCombineKey(G_CK_NONE),
	gsDPSetAlphaCompare(G_AC_NONE),
	gsDPSetRenderMode(G_RM_NOOP, G_RM_NOOP2),
	gsDPSetColorDither(G_CD_DISABLE),
	gsDPSetAlphaDither(G_AD_DISABLE),
	gsDPSetTextureFilter(G_TF_POINT),
	gsDPSetTextureConvert(G_TC_FILT),
	gsDPSetTexturePersp(G_TP_NONE),
	gsDPSetTextureLUT(G_TT_NONE),
	gsDPPipeSync(),
	gsSPEndDisplayList(),
};

static void PreNMICB()
{
	//Prevent Reset Crash
	nuGfxDisplayOff();
	osViSetYScale(1);
}

void GfxInit()
{
	nuGfxInit();
	GfxSetScreenSize(320, 240);
	nuPreNMIFuncSet(PreNMICB);
	nuGfxDisplayOn();
}

static void SetVIModeSize(OSViMode *mode, int width, int height)
{
	//Set VI width and x scale
	mode->comRegs.width = width;
	mode->comRegs.xScale = (width*512)/320;
	if(height > 240) {
		//Interlace mode
		mode->comRegs.ctrl |= 0x40; //Forces Serrate Bit On
		mode->comRegs.vSync &= ~0x1; //Force Even Vsync Count
		//Offset even fields by 1 line and odd fields by 2 lines
		mode->fldRegs[0].origin = width*2;
		mode->fldRegs[1].origin = width*4;
		//Calculate Y scale and offset by half a pixel
		mode->fldRegs[0].yScale = 0x2000000|((height*1024)/240);
		mode->fldRegs[1].yScale = 0x2000000|((height*1024)/240);
		//Fix VI start for even fields
		mode->fldRegs[0].vStart = mode->fldRegs[1].vStart-0x20002;
	} else {
		//Progressive scan mode
		mode->comRegs.vSync |= 0x1; //Force Odd Vsync Count
		//Offset fields by 1 line
		mode->fldRegs[0].origin = width*2;
		mode->fldRegs[1].origin = width*2;
		//Calculate Y scale
		mode->fldRegs[0].yScale = ((height*1024)/240);
		mode->fldRegs[1].yScale = ((height*1024)/240);
		mode->fldRegs[0].vStart = mode->fldRegs[1].vStart;
	}
}

EXPORT_SYMBOL void GfxSetScreenSize(int width, int height)
{
	//Clamp resolution to 640x480
	if(width > 640) {
		width = 640;
	}
	if(height > 480) {
		height = 480;
	}
	if(gfx_cfb_w == width && gfx_cfb_h == height) {
		//Screen is already the same size
		return;
	}
	nuGfxTaskAllEndWait(); //Wait for any pending tasks to get done
	//Free framebuffers
	for(int i=0; i<NUM_CFBS; i++) {
		if(cfb_table[i]) {
			free(cfb_table[i]);
		}
	}
	//Allocate and clear framebuffers
	for(int i=0; i<NUM_CFBS; i++) {
		cfb_table[i] = memalign(64, width*height*2); //Framebuffer must be 64-byte aligned
		for(size_t j=0; j<width*height; j++) {
			cfb_table[i][j] = 0x1; //Clear to black
		}
	}
	nuGfxSetCfb(cfb_table, NUM_CFBS);
	//Copy appropriate VI mode
	switch(osTvType) {
		case 0: //PAL
			vi_mode = osViModeTable[OS_VI_FPAL_LPN1];
			break;

		case 1: //NTSC
			vi_mode = osViModeTable[OS_VI_NTSC_LPN1];
			break;

		case 2: //MPAL
			vi_mode = osViModeTable[OS_VI_MPAL_LPN1];
			break;
			
		default:
		//Invalid mode is reated as NTSC
			vi_mode = osViModeTable[OS_VI_NTSC_LPN1];
			break;
	}
	//Change screen size for VI mode
	SetVIModeSize(&vi_mode, width, height);
	osViSetMode(&vi_mode);
	//Disable Gamma
	osViSetSpecialFeatures(OS_VI_GAMMA_OFF);
	//Scale VI vertically on PAL
	//This must be done on every mode change
	if(osTvType == 0) {
		osViSetYScale(0.833);
	}
	gfx_cfb_w = width;
	gfx_cfb_h = height;
	viewport.vp.vscale[0] = viewport.vp.vtrans[0] = width*2;
	viewport.vp.vscale[1] = viewport.vp.vtrans[1] = height*2;
}

EXPORT_SYMBOL void GfxSetClearColor(u8 r, u8 g, u8 b)
{
	clear_r = r;
	clear_g = g;
	clear_b = b;
}

EXPORT_SYMBOL  void GfxSetScissor(int x, int y, int w, int h)
{
	//Don't set fully offscreen scissor
	if(x < -w || y < -h) {
		return;
	}
	//Clamp rect to screen
	if(x < 0) {
		w += x;
		x = 0;
	}
	if(y < 0) {
		h += y;
		y = 0;
	}
	gDPSetScissor(gfx_dlistp++, G_SC_NON_INTERLACE, x, y, x+w, y+h); 
}

EXPORT_SYMBOL  void GfxResetScissor()
{
	gDPSetScissor(gfx_dlistp++, G_SC_NON_INTERLACE, 0, 0, gfx_cfb_w, gfx_cfb_h);
}

EXPORT_SYMBOL void GfxStartFrame()
{
	gfx_dlistp = glist;
	gSPSegment(gfx_dlistp++, 0, 0); //Setup segment 0 as direct segment
	//Call initialization display lists
	gSPDisplayList(gfx_dlistp++, rspinit_dl);
	gSPDisplayList(gfx_dlistp++, rdpinit_dl);
	gDPSetScissor(gfx_dlistp++, G_SC_NON_INTERLACE, 0, 0, gfx_cfb_w, gfx_cfb_h); //Reset scissor for drawing
	//Start screen clear
	gDPSetCycleType(gfx_dlistp++, G_CYC_FILL);
	gDPSetRenderMode(gfx_dlistp++, G_RM_NOOP, G_RM_NOOP2);
	//Start framebuffer drawing
	gDPSetColorImage(gfx_dlistp++, G_IM_FMT_RGBA, G_IM_SIZ_16b, gfx_cfb_w, nuGfxCfb_ptr);
	//Clear Framebuffer
	u16 clear_col = GPACK_RGBA5551(clear_r, clear_g, clear_b, 1);
	gDPSetFillColor(gfx_dlistp++, (clear_col << 16)|clear_col);
	gDPFillRectangle(gfx_dlistp++, 0, 0, gfx_cfb_w - 1, gfx_cfb_h - 1);
	gDPPipeSync(gfx_dlistp++);
	//Start 2D drawing
	gDPSetCycleType(gfx_dlistp++, G_CYC_1CYCLE);
	render_mode = GFX_RENDER_IMAGE;
	gDPSetCombineMode(gfx_dlistp++, G_CC_DECALRGBA, G_CC_DECALRGBA);
	gDPSetRenderMode(gfx_dlistp++, G_RM_XLU_SURF, G_RM_XLU_SURF);
}

EXPORT_SYMBOL void GfxSetRenderMode(GfxRenderMode mode)
{
	//Only mode switch if required
	if(render_mode != mode) {
		switch(mode) {
			case GFX_RENDER_RECT:
				gDPSetCombineMode(gfx_dlistp++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
				gDPSetRenderMode(gfx_dlistp++, G_RM_OPA_SURF, G_RM_OPA_SURF);
				break;
				
			case GFX_RENDER_RECT_ALPHA:
				gDPSetCombineMode(gfx_dlistp++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
				gDPSetRenderMode(gfx_dlistp++, G_RM_XLU_SURF, G_RM_XLU_SURF);
				break;
				
			case GFX_RENDER_IMAGE:
				gDPSetCombineMode(gfx_dlistp++, G_CC_DECALRGBA, G_CC_DECALRGBA);
				gDPSetRenderMode(gfx_dlistp++, G_RM_XLU_SURF, G_RM_XLU_SURF);
				break;
				
			case GFX_RENDER_IMAGETINT:
				gDPSetCombineMode(gfx_dlistp++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
				gDPSetRenderMode(gfx_dlistp++, G_RM_XLU_SURF, G_RM_XLU_SURF);
				break;
				
			default:
				break;
		}
		render_mode = mode;
	}
}

EXPORT_SYMBOL void GfxPutRect(int x, int y, int w, int h, u32 color)
{
	if(x < -w || y < -h) {
		return;
	}
	//Get color channels
	u8 r = (color >> 24) & 0xFF;
	u8 g = (color >> 16) & 0xFF;
	u8 b = (color >> 8) & 0xFF;
	u8 a = color & 0xFF;
	if(a == 0xFF) {
		//No alpha
		GfxSetRenderMode(GFX_RENDER_RECT);
	} else {
		//Alpha
		GfxSetRenderMode(GFX_RENDER_RECT_ALPHA);
	}
	//Render rectangle
	gDPSetPrimColor(gfx_dlistp++, 0, 0, r, g, b, a);
	gSPScisTextureRectangle(gfx_dlistp++, x*4, y*4, (x+w)*4, (y+h)*4, 0, 0, 0, 0, 0);
}

EXPORT_SYMBOL void GfxEndFrame()
{
	//End display list
	gDPFullSync(gfx_dlistp++);
	gSPEndDisplayList(gfx_dlistp++);
	debug_assert((s32)(gfx_dlistp - glist) < GLIST_SIZE && "Display list overflow.\n");
	//Send display list to RSP
	nuGfxTaskStart(glist, (s32)(gfx_dlistp - glist) * sizeof(Gfx), NU_GFX_UCODE_F3DEX2, NU_SC_SWAPBUFFER);
	//Wait for frame end
	nuGfxRetraceWait(1);
	nuGfxTaskAllEndWait();
}