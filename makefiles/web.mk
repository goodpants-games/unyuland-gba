CC := emcc

TARGET := web/game
BUILD  := buildweb

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
LIBS         := -sUSE_SDL=3
AUDIO_DRIVER := mpt


#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS      := -DPLATFORM_PC -DPLATFORM_WEB -O2 $(LIBS) $(CFLAGS)
ASFLAGS     := -DPLATFORM_PC -DPLATFORM_WEB $(ASFLAGS)
BIN2S_FLAGS += --arch wasm


#---------------------------------------------------------------------------------
# targets
#---------------------------------------------------------------------------------
define CLEAN =
@rm -fr $(BUILD) $(OUTPUT).*
endef

define BUILD_TARGETS
$(OUTPUT).js: $(OFILES) $(SLIBS)
	$(SILENTCMD)$(CC) $(OFILES) $(LDFLAGS) $(CFLAGS) $(LIBS) $(SLIBS) -o $(OUTPUT).js

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
export PLATFORM_MAKEFILE := $(TOPLEVEL)/makefiles/web.mk
include $(TOPLEVEL)/makefiles/common.mk