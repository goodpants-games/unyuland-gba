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

PKGCONF ?= pkg-config
CC ?= gcc
DEVDEBUG ?= yes

TARGET       := unyuland
BUILD        := buildpc

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
LIBS         := $(shell $(PKGCONF) $(PKGCONF_FLAGS) --libs $(LIBNAMES))\
                $(SLIBS) -lm


#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS  := -DPLATFORM_PC \
           $(shell $(PKGCONF) $(PKGCONF_FLAGS) --cflags $(LIBNAMES)) $(CFLAGS)
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
include $(TOPLEVEL)makefiles/common.mk
