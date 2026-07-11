PKGCONF ?= pkg-config
CC ?= gcc

TARGET := unyuland
BUILD  := buildpc

LIBXMP := third_party/libxmp

#---------------------------------------------------------------------------------
# platform-specific sources
#---------------------------------------------------------------------------------
SOURCES := src/pc/tonc src/pc/maxmod src/pc
INCLUDES := src/pc/include $(LIBXMP)/include

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
SLIBS        := libxmp-lite.a
LIBNAMES     := sdl3
LIBS         := $(shell $(PKGCONF) --libs $(LIBNAMES))\
                $(SLIBS)


#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS  := -DPLATFORM_PC \
           $(shell $(PKGCONF) --cflags $(LIBNAMES)) $(CFLAGS)
ASFLAGS := -DPLATFORM_PC $(ASFLAGS)
LDFLAGS := $(LDFLAGS)

ifeq ($(DEVDEBUG),yes)
  CFLAGS += -gdwarf-4 -Og
  LDFLAGS += -gdwarf-4
endif

ifeq ($(OS), Windows_NT)
  ifeq ($(DEVDEBUG),yes)
    LIBS += -mconsole
  endif
endif


#---------------------------------------------------------------------------------
# targets
#---------------------------------------------------------------------------------
define CLEAN =
@rm -fr $(BUILD) $(OUTPUT)
endef

define BUILD_TARGETS
$(OUTPUT): $(OFILES) $(SLIBS)
	$(SILENTCMD)$(CC) $(OFILES) $(LDFLAGS) $(CFLAGS) $(LIBS) -o $(OUTPUT)

libxmp-lite.a: $(TOPLEVEL)/$(LIBXMP)/lib/libxmp-lite.a
	$(SILENTCMD)cp $(TOPLEVEL)/$(LIBXMP)/lib/libxmp-lite.a $(CURDIR)
endef

#---------------------------------------------------------------------------------
# This rule creates C source files using grit
# grit takes an image file and a .grit describing how the file is to be processed
# add additional rules like this for each image extension
# you use in the graphics folders
#---------------------------------------------------------------------------------
%_gfx.c %_gfx.h: %.png %.grit
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $*)
	@grit $< -ftc -o$*_gfx


#---------------------------------------------------------------------------------
export PLATFORM_MAKEFILE := $(TOPLEVEL)/makefiles/pc.mk
include $(TOPLEVEL)/makefiles/common.mk