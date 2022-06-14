#pragma once

#include <cextern.h>

typedef struct uso_handle_data *USOHandle;

typedef struct uso_handle_info {
	char *name;
	USOHandle handle;
	int ref_count;
	void *min_addr;
	void *max_addr;
} USOHandleInfo;

EXTERN_C void USOInit(); //Initialize USO manager
EXTERN_C USOHandle USOGetHandle(const char *name); //Find loaded USO handle
EXTERN_C void USOPin(USOHandle handle); //Increment reference count
EXTERN_C USOHandle USOGetAddrHandle(void *ptr); //Find USO address is within
EXTERN_C USOHandle USOOpen(const char *name); //Open USO if not loaded or increment reference count if loaded
EXTERN_C void *USOFindSymbol(USOHandle handle, const char *name); //Find symbol within USO specified by handle
EXTERN_C void USOCloseNow(USOHandle handle); //Unload USO regardless of reference count
EXTERN_C void USOClose(USOHandle handle); //Unload USO if reference count has decremented to zero
EXTERN_C USOHandle USOGetFirstHandle(); //Get first handle of USO handle list
EXTERN_C USOHandle USOGetNextHandle(USOHandle handle); //Get handle after this handle in USO handle list
EXTERN_C USOHandleInfo USOGetHandleInfo(USOHandle handle); //Get information about USO handle