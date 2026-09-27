# Copyright (C) 2026  pkhead
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
LIBS	     := -lmm -ltonc
AUDIO_DRIVER := mm


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
include $(TOPLEVEL)makefiles/common.mk