TARGET_STRING := usodemo
TARGET := $(TARGET_STRING)

# Preprocessor definitions
DEFINES := _FINALROM=1 NDEBUG=1 F3DEX_GBI_2=1

SRC_DIRS :=
USE_DEBUG := 0

TOOLS_DIR := tools

# Whether to hide commands or not
VERBOSE ?= 0
ifeq ($(VERBOSE),0)
  V := @
endif

# Whether to colorize build messages
COLOR ?= 1

ifeq ($(filter clean distclean print-%,$(MAKECMDGOALS)),)
  # Make tools if out of date
  $(info Building tools...)
  DUMMY != $(MAKE) -s -C $(TOOLS_DIR) >&2 || echo FAIL
    ifeq ($(DUMMY),FAIL)
      $(error Failed to build tools)
    endif
  $(info Building ROM...)
endif

#==============================================================================#
# Target Executable and Sources                                                #
#==============================================================================#
# BUILD_DIR is the location where all build artifacts are placed
BUILD_DIR := build
DATA_DIR := data
USO_ELF_DIR := $(BUILD_DIR)/usos
USO_DIR := $(DATA_DIR)/uso
GLOBAL_SYMS := $(USO_DIR)/global_syms.bin
FINAL_ROM := $(TARGET_STRING).z64
MAIN_ELF := $(BUILD_DIR)/$(TARGET_STRING).elf
MAIN_BIN := $(BUILD_DIR)/$(TARGET_STRING).bin
LD_SCRIPT := main.ld
MAIN_LIB_DIRS := -L/usr/lib/n64 -L/usr/lib/n64/nusys -L$(N64_LIBGCCDIR)
MAIN_LIBS := -lmus -lnualstl -lnusys -lultra_rom -lgcc
USO_LD_SCRIPT := uso.ld
FS_DATA := $(BUILD_DIR)/fs_data.bin
ROM_HEADER := rom_header.bin
BOOT := /usr/lib/n64/PR/bootcode/boot.6102
FS_DATA_DIR := data
NUSYS_BOOT_OBJ := /usr/lib/n64/nusys/nusys_rom.o
UCODE_OBJS := /usr/lib/n64/PR/gspF3DEX2.fifo.o /usr/lib/n64/PR/gspF3DEX2.NoN.fifo.o \
/usr/lib/n64/PR/gspF3DEX2.Rej.fifo.o /usr/lib/n64/PR/gspF3DLX2.Rej.fifo.o \
/usr/lib/n64/PR/gspL3DEX2.fifo.o /usr/lib/n64/PR/gspS2DEX2.fifo.o \
/usr/lib/n64/PR/rspboot.o /usr/lib/n64/PR/aspMain.o

# Directories containing source files
SRC_DIRS += src src/libc

C_FILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
CXX_FILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp))
S_FILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.s))

SRC_OBJECTS := $(foreach file,$(C_FILES),$(BUILD_DIR)/$(file:.c=.o)) \
			$(foreach file,$(CXX_FILES),$(BUILD_DIR)/$(file:.cpp=.o)) \
			$(foreach file,$(S_FILES),$(BUILD_DIR)/$(file:.s=.o))
			
# Object files
O_FILES := $(SRC_OBJECTS)
		 
# Automatic dependency files
DEP_FILES := $(SRC_OBJECTS:.o=.d)

#==============================================================================#
# Compiler Options                                                             #
#==============================================================================#

AS        := mips-n64-as
CC        := mips-n64-gcc
CXX := mips-n64-g++
CPP       := cpp
LD        := mips-n64-ld
AR        := mips-n64-ar
OBJDUMP   := mips-n64-objdump
OBJCOPY   := mips-n64-objcopy
ELF2USO := tools/elf2uso
MAKEGLOBALSYM := tools/makeglobalsym
MAKEN64FS := tools/maken64fs

INCLUDE_DIRS += /usr/include/n64 /usr/include/n64/nusys include

C_DEFINES := $(foreach d,$(DEFINES),-D$(d))
DEF_INC_CFLAGS := $(foreach i,$(INCLUDE_DIRS),-I$(i)) $(C_DEFINES)

CFLAGS = -fvisibility=hidden -G 0 -Os -mabi=32 -ffreestanding -mfix4300 $(DEF_INC_CFLAGS)
CXXFLAGS = $(CFLAGS) -std=c++17 -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-threadsafe-statics
ASFLAGS := -march=vr4300 -mabi=32 $(foreach i,$(INCLUDE_DIRS),-I$(i)) $(foreach d,$(DEFINES),--defsym $(d))

# C preprocessor flags
CPPFLAGS := -P -Wno-trigraphs $(DEF_INC_CFLAGS)

# tools
PRINT = printf

ifeq ($(COLOR),1)
NO_COL  := \033[0m
RED     := \033[0;31m
GREEN   := \033[0;32m
BLUE    := \033[0;34m
YELLOW  := \033[0;33m
BLINK   := \033[33;5m
endif

# Common build print status function
define print
  @$(PRINT) "$(GREEN)$(1) $(YELLOW)$(2)$(GREEN) -> $(BLUE)$(3)$(NO_COL)\n"
endef

#==============================================================================#
# Main Targets                                                                 #
#==============================================================================#

# Default target
default: $(FINAL_ROM)

#Must be below the default target for some reason
USO_OBJECTS := 
USO_SRCDIRS :=
USO_NAMES :=

include uso_rules.mak

USO_SRC_ELFS := $(foreach name,$(USO_NAMES),$(USO_ELF_DIR)/$(name).elf)
USOS_ALL := $(foreach name,$(USO_NAMES),$(USO_DIR)/$(name).uso)

DEP_FILES += $(USO_OBJECTS:.o=.d) 

clean:
	$(RM) -r $(BUILD_DIR) $(USOS_ALL) $(GLOBAL_SYMS) $(FINAL_ROM)

distclean: clean
	$(MAKE) -C $(TOOLS_DIR) clean

ALL_DIRS := $(BUILD_DIR) $(USO_ELF_DIR) $(USO_DIR) $(addprefix $(BUILD_DIR)/,$(SRC_DIRS)) $(addprefix $(BUILD_DIR)/,$(USO_SRCDIRS))

# Make sure build directory exists before compiling anything
DUMMY != mkdir -p $(ALL_DIRS)

#==============================================================================#
# Compilation Recipes                                                          #
#==============================================================================#

# Compile C code
$(BUILD_DIR)/%.o: %.c
	$(call print,Compiling:,$<,$@)
	$(V)$(CC) -c -Werror=implicit-function-declaration $(CFLAGS) -MMD -MF $(BUILD_DIR)/$*.d  -o $@ $<
	
$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.c
	$(call print,Compiling:,$<,$@)
	$(V)$(CC) -c -Werror=implicit-function-declaration $(CFLAGS) -MMD -MF $(BUILD_DIR)/$*.d  -o $@ $<

$(BUILD_DIR)/%.o: %.cpp
	$(call print,Compiling:,$<,$@)
	$(V)$(CC) -c $(CXXFLAGS) -MMD -MF $(BUILD_DIR)/$*.d  -o $@ $<
	
# Assemble assembly code
$(BUILD_DIR)/%.o: %.s
	$(call print,Assembling:,$<,$@)
	$(V)$(AS) $(ASFLAGS) -MD $(BUILD_DIR)/$*.d  -o $@ $<

$(BOOT_OBJ): $(BOOT)
	$(V)$(OBJCOPY) -I binary -B mips -O elf32-bigmips $< $@

# Link final ELF file
$(MAIN_ELF): $(O_FILES)
	@$(PRINT) "$(GREEN)Linking ELF file: $(BLUE)$@ $(NO_COL)\n"
	$(V)$(LD) -L $(BUILD_DIR) -T $(LD_SCRIPT) -Map $(BUILD_DIR)/$(TARGET_STRING).map --no-check-sections -o $@ $(O_FILES) $(NUSYS_BOOT_OBJ) $(UCODE_OBJS) $(MAIN_LIB_DIRS) $(MAIN_LIBS)

$(MAIN_BIN): $(MAIN_ELF)
	@$(PRINT) "$(GREEN)Converting to binary: $(BLUE)$@ $(NO_COL)\n"
	$(V)$(OBJCOPY) $< $@ -O binary
	
#Link USO Source ELF file
$(USO_SRC_ELFS):
	@$(PRINT) "$(GREEN)Linking ELF file: $(BLUE)$@ $(NO_COL)\n"
	$(V)$(LD) -Map $@.map -d -r -T $(USO_LD_SCRIPT) -o $@ $^
	
#Generate global symbol table
$(GLOBAL_SYMS): $(MAIN_ELF)
	@$(PRINT) "$(GREEN)Generating global symbol table: $(BLUE)$@ $(NO_COL)\n"
	$(V)$(MAKEGLOBALSYM) $(MAIN_ELF) $(GLOBAL_SYMS)
	
#Build filesystem
$(FS_DATA): $(USOS_ALL) $(GLOBAL_SYMS)
	@$(PRINT) "$(GREEN)Generating filesytem: $(BLUE)$@ $(NO_COL)\n"
	$(V)$(MAKEN64FS) $(FS_DATA_DIR) $(FS_DATA)
	
$(FINAL_ROM): $(ROM_HEADER) $(BOOT) $(MAIN_BIN) $(FS_DATA)
	@$(PRINT) "$(GREEN)Creating final ROM: $(BLUE)$(FINAL_ROM) $(NO_COL)\n"
	$(V)cat $(ROM_HEADER) $(BOOT) $(MAIN_BIN) $(FS_DATA) > $(FINAL_ROM)
	$(V)makemask $(FINAL_ROM)

#Generate USO
$(USO_DIR)/%.uso: $(USO_ELF_DIR)/%.elf
	$(call print,Converting ELF to USO:,$<,$@)
	$(V)$(ELF2USO) $^ $@
	
#Force build filesystem every time
.PHONY: $(FS_DATA)

.PHONY: clean distclean default
# with no prerequisites, .SECONDARY causes no intermediate target to be removed
.SECONDARY:

# Remove built-in rules, to improve performance
MAKEFLAGS += --no-builtin-rules

-include $(DEP_FILES)

print-% : ; $(info $* is a $(flavor $*) variable set to [$($*)]) @true