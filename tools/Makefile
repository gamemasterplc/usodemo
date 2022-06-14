HOST_ENV := $(shell uname 2>/dev/null || echo Unknown)

CC := gcc
CXX := g++
CFLAGS := -std=c++17 -I. -O2 -s
LDFLAGS := -lstdc++ -lstdc++fs
ALL_PROGRAMS := maken64fs elf2uso makeglobalsym

BUILD_PROGRAMS := $(ALL_PROGRAMS)

default: all

elf2uso_SOURCES := elf2uso.cpp
maken64fs_SOURCES := maken64fs.cpp
makeglobalsym_SOURCES := makeglobalsym.cpp

all: $(BUILD_PROGRAMS)

clean:
	$(RM) $(ALL_PROGRAMS)

define COMPILE
$(1): $($1_SOURCES)
	$$(CXX) $(CFLAGS) $($1_CFLAGS) $$^ -o $$@ $($1_LDFLAGS) $(LDFLAGS)
endef

$(foreach p,$(BUILD_PROGRAMS),$(eval $(call COMPILE,$(p))))

.PHONY: all clean default
