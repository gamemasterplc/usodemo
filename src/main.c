#include <nusys.h>
#include <nualstl.h>
#include <debug.h>
#include <gfx.h>
#include <fs.h>
#include <pad.h>
#include <stdlib.h>
#include <usoload.h>
#include <stage.h>
#include <util.h>
#include <symbol_attr.h>

//Stage manager information
static char *stage_names[] = { "stage00", "stage01" };
static int last_stage_num;
static int stage_num;

static void AudioInit()
{
	musConfig config;
    config.control_flag = 0; //Wave data is streamed from ROM
    config.channels = NU_AU_CHANNELS; //Maximum total number of channels
    config.sched = NULL; //The address of the Scheduler structure. NuSystem uses NULL
    config.thread_priority = NU_AU_MGR_THREAD_PRI; //Thread priority (highest)
    config.heap = memalign(16, NU_AU_HEAP_SIZE); //Heap address
    config.heap_length = NU_AU_HEAP_SIZE; //Heap size
    config.ptr = NULL; //Allows you to set a default ptr file
    config.wbk = NULL; //Allows you to set a default wbk file
    config.default_fxbank = NULL; //Allows you to set a default bfx file
    config.fifo_length = NU_AU_FIFO_LENGTH; //The size of the library's FIFO buffer
    config.syn_updates = NU_AU_SYN_UPDATE_MAX; //Specifies the number of updates usable by the synthesizer.
    config.syn_output_rate = 44100; //Audio output rate. The higher, the better quality
    config.syn_rsp_cmds = NU_AU_CLIST_LEN; //The maximum length of the audio command list.
    config.syn_retraceCount = 1; //The number of frames per retrace message
    config.syn_num_dma_bufs = NU_AU_DMA_BUFFER_NUM; //Specifies number of buffers Audio Manager can use for DMA from ROM.
    config.syn_dma_buf_size = NU_AU_DMA_BUFFER_SIZE; //The length of each DMA buffer

    //Initialize the Audio Manager.
    nuAuStlMgrInit(&config);
    nuAuPreNMIFuncSet(nuAuPreNMIProc);
}

static void MusicStart()
{
	//Try to open wave bank
	FSFile wbk_file;
	debug_assert(FSOpen(&wbk_file, "bank_instr.wbk") && "Failed to open wave bank.");
	//Load pointer bank
	void *ptr_bank = FileLoad("bank_instr.ptr");
	debug_assert(ptr_bank && "Failed to load pointer bank.");
	MusPtrBankInitialize(ptr_bank, FSGetRomOfs(&wbk_file)); //Initialize pointer bank
	//Read and start song
	void *sng_data = FileLoad("sng_menu.bin");
	debug_assert(sng_data && "Failed to load song.");
	MusStartSong(sng_data);
}

static void SystemInit()
{
	FSInit();
	debug_initialize();
	USOInit();
	GfxInit();
	PadInit();
	AudioInit();
}

EXPORT_SYMBOL void StageToggle()
{
	//Change from 0 to 1 and vice versa
	stage_num ^= 1;
}

void mainproc()
{
	SystemInit();
	MusicStart();
	stage_num = 0; //Start at stage 0
	while(1) {
		debug_printf("Loading stage %s.\n", stage_names[stage_num]);
		//Load stage USO
		USOHandle handle = USOOpen(stage_names[stage_num]);
		if(!handle) {
			//Error if it didn't load
			debug_printf("Failed to load stage %s.\n", stage_names[stage_num]);
			abort();
		}
		//Try to find stage functions
		StageFuncs *funcs = USOFindSymbol(handle, "stage_funcs");
		if(!funcs) {
			//Error if not found
			debug_printf("Failed to load stage %s\n.", stage_names[stage_num]);
			abort();
		}
		debug_printf("stage_funcs = %08x\n", funcs); //Print address of stage functions
		//Initialize stage
		funcs->init();
		last_stage_num = stage_num; //Remember previous stage
		//Wait for stage to change
		while(stage_num == last_stage_num) {
			//Update stage
			PadUpdate();
			funcs->main();
			//Update stage graphics
			GfxStartFrame();
			funcs->draw();
			GfxEndFrame();
		}
		//Destroy stage if any function is registered
		if(funcs->destroy) {
			funcs->destroy();
		}
		//Close USO
		USOClose(handle);
	}
}