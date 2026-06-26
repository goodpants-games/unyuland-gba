ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/gba_rules
undefine bin2o

#---------------------------------------------------------------------------------
# the LIBGBA path is defined in gba_rules, but we have to define LIBTONC ourselves
#---------------------------------------------------------------------------------
LIBTONC := $(DEVKITPRO)/libtonc

#---------------------------------------------------------------------------------
# platform-specific sources
#---------------------------------------------------------------------------------
SOURCES := src/gba
INCLUDES := src/gba/include


#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-mthumb -mthumb-interwork
CFLAGS	:=	-DPLATFORM_GBA -mcpu=arm7tdmi -mtune=arm7tdmi -g -O2 $(ARCH) $(CFLAGS)
ASFLAGS	:=	-DPLATFORM_GBA -g $(ARCH) $(ASFLAGS)
LDFLAGS	:=	-g $(ARCH) -Wl,-Map,$(notdir $*.map) $(LDFLAGS)


#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:= -lmm -ltonc


#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib.
# the LIBGBA path should remain in this list if you want to use maxmod
#---------------------------------------------------------------------------------
LIBDIRS	:=	$(LIBGBA) $(LIBTONC)


#---------------------------------------------------------------------------------
# targets
#---------------------------------------------------------------------------------
define CLEAN =
@rm -fr $(BUILD) $(TARGET).elf $(TARGET).gba
endef

define BUILD_TARGETS
$(OUTPUT).gba: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
endef

#---------------------------------------------------------------------------------
# This rule creates assembly source files using grit
# grit takes an image file and a .grit describing how the file is to be processed
# add additional rules like this for each image extension
# you use in the graphics folders
#---------------------------------------------------------------------------------
%_gfx.s %_gfx.h: %.png %.grit
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $*)
	$(SILENTCMD)grit $< -fts -o$*_gfx


#---------------------------------------------------------------------------------
export PLATFORM_MAKEFILE := $(TOPLEVEL)/makefiles/gba.mk
include $(TOPLEVEL)/makefiles/common.mk