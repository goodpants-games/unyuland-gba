PKGCONF ?= pkg-config
CC ?= gcc

TARGET := unyuland
BUILD  := buildpc

#---------------------------------------------------------------------------------
# platform-specific sources
#---------------------------------------------------------------------------------
SOURCES := src/pc/tonc src/pc/maxmod src/pc


#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBNAMES := sdl3
LIBS     := $(shell $(PKGCONF) --libs $(LIBNAMES))


#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS  := -DPLATFORM_PC -gdwarf-4 -Og \
           $(shell $(PKGCONF) --cflags $(LIBNAMES)) $(CFLAGS)
ASFLAGS := -DPLATFORM_PC $(ASFLAGS)
LDFLAGS := -gdwarf-4 $(LDFLAGS)

ifeq ($(OS), Windows_NT)
  LIBS += -mconsole
endif


#---------------------------------------------------------------------------------
# targets
#---------------------------------------------------------------------------------
define CLEAN =
@rm -fr $(BUILD) $(OUTPUT)
endef

define BUILD_TARGETS
$(OUTPUT): $(OFILES)
	$(SILENTCMD)$(CC) $(OFILES) $(LDFLAGS) $(CFLAGS) $(LIBS) -o $(OUTPUT)
endef

#---------------------------------------------------------------------------------
# This rule creates C source files using grit
# grit takes an image file and a .grit describing how the file is to be processed
# add additional rules like this for each image extension
# you use in the graphics folders
#---------------------------------------------------------------------------------
%_gfx.c %_gfx.h: %.png %.grit
#---------------------------------------------------------------------------------
	@echo "grit $<"
	@grit $< -ftc -o$*_gfx


#---------------------------------------------------------------------------------
export PLATFORM_MAKEFILE := $(TOPLEVEL)/makefiles/pc.mk
include $(TOPLEVEL)/makefiles/common.mk