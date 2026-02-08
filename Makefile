#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/gba_rules

PYTHON ?= python
ASEPRITE ?= aseprite

#---------------------------------------------------------------------------------
# the LIBGBA path is defined in gba_rules, but we have to define LIBTONC ourselves
#---------------------------------------------------------------------------------
LIBTONC := $(DEVKITPRO)/libtonc

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
# DATA is a list of directories containing binary data
# GRAPHICS is a list of directories containing files to be processed by grit
#
# All directories are specified relative to the project directory where
# the makefile is found
#
#---------------------------------------------------------------------------------
TARGET		:= $(notdir $(CURDIR))
BUILD		:= build
SOURCES		:= src
INCLUDES	:= include
DATA		:= data/bin
MUSIC		:=
GRAPHICS	:= data/graphics
MAPS        := data/maps
SPRITES     := data/sprites

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-mthumb -mthumb-interwork

CFLAGS	:=	-g -Wall -O2\
		-mcpu=arm7tdmi -mtune=arm7tdmi\
		$(ARCH)

CFLAGS	+=	$(INCLUDE)

CXXFLAGS	:=	$(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-g $(ARCH) -Wl,-Map,$(notdir $*.map)

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
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------


ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export TOPLEVEL :=  $(CURDIR)
export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
			$(foreach dir,$(MAPS),$(CURDIR)/$(dir)) \
			$(foreach dir,$(SPRITES),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PNGFILES	:=	$(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.png)))
MAPFILES	:=	$(foreach dir,$(MAPS),$(notdir $(wildcard $(dir)/*.tmx)))
SPRFILES	:=	$(foreach dir,$(SPRITES),$(notdir $(wildcard $(dir)/*.sprdb)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifneq ($(strip $(MUSIC)),)
	export AUDIOFILES	:=	$(foreach dir,$(notdir $(wildcard $(MUSIC)/*.*)),$(CURDIR)/$(MUSIC)/$(dir))
	BINFILES += soundbank.bin
endif

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_BIN := $(addsuffix .o,$(BINFILES))

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_GRAPHICS := $(addsuffix _gfx.o,$(PNGFILES:.png=))

export OFILES_MAPS := $(addsuffix .map.o,$(MAPFILES:.tmx=))

export OFILES_SPRITES := $(addsuffix _sprdb_data.o,$(SPRFILES:.sprdb=)) $(addsuffix _sprdb_gfx.o,$(SPRFILES:.sprdb=))

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES) $(OFILES_GRAPHICS) $(OFILES_MAPS) $(OFILES_SPRITES)

# export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES))) $(addsuffix _map.h,$(MAPFILES:.tmx=)) $(PNGFILES:.png=.h)
export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES))) \
                 $(addsuffix _gfx.h,$(PNGFILES:.png=)) \
				 $(addsuffix _map.h,$(MAPFILES:.tmx=)) \
				 $(addsuffix _sprdb.h,$(SPRFILES:.sprdb=))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean sprites

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).gba

#---------------------------------------------------------------------------------
else

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------

$(OUTPUT).gba	:	$(OUTPUT).elf

$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SOURCES) : $(HFILES)

#---------------------------------------------------------------------------------
# The bin2o rule should be copied and modified
# for each extension used in the data directories
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
# rule to build soundbank from music files
#---------------------------------------------------------------------------------
soundbank.bin soundbank.h : $(AUDIOFILES)
#---------------------------------------------------------------------------------
	@mmutil $^ -osoundbank.bin -hsoundbank.h

#---------------------------------------------------------------------------------
# This rule links in binary data with the .bin extension
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
# This rule creates assembly source files using grit
# grit takes an image file and a .grit describing how the file is to be processed
# add additional rules like this for each image extension
# you use in the graphics folders
#---------------------------------------------------------------------------------
%_gfx.s %_gfx.h: %.png %.grit
#---------------------------------------------------------------------------------
	@echo "grit $<"
	@grit $< -fts -o$*_gfx


#---------------------------------------------------------------------------------
# These rules convert Tiled level files to a more efficient format readable by
# the game.
#---------------------------------------------------------------------------------
%.map.o %_map.h: %.map
	@echo $(notdir $<)
	@$(bin2o)
%.map: %.tmx
	@$(PYTHON) ../tools/mapc.py $< $@

#---------------------------------------------------------------------------------
# This rule compiles .sprdb files to a sprdb binary data and image file, using
# Aseprite with the sprdb.lua script. The image file is then compiled with grit.
#---------------------------------------------------------------------------------
%_sprdb_data.o %_sprdb.h %_sprdb_data.h %_sprdb_gfx.h: %.sprdb
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	
	$(eval _tmpdir = $(shell mktemp -d))
	$(eval _tmppng = $(_tmpdir)/$(notdir $(basename $@))_gfx.png)
	$(eval _tmpbin = $(_tmpdir)/$(notdir $(basename $@)).data)

	@$(ASEPRITE) --batch \
	            --script-param sprdb="$<" \
	            --script-param output_dat="$(_tmpbin)" \
	            --script-param output_img="$(_tmppng)" \
	            --script-param output_h="`(echo $(<F) | tr . _)`.h" \
	            --script-param out_dep="$(DEPSDIR)/$(notdir $(<:.sprdb=.sd))" \
	            --script-param artifact_name="$@" \
	            --script       "$(TOPLEVEL)/tools/sprdb.lua"

	@grit $(_tmppng) -fts -gB 4 -p!
	
	$(eval _tmpasm := $(shell mktemp))
	@bin2s -a 4 -H `(echo $(<F) | tr . _)`_data.h $(_tmpbin) > $(_tmpasm)
	@$(CC) -x assembler-with-cpp $(CPPFLAGS) $(ASFLAGS) -c $(_tmpasm) -o `(echo $(<F) | tr . _)`_data.o
	@rm $(_tmpasm)
	@rm $(_tmppng)
	@rm $(_tmpbin)
	@rmdir $(_tmpdir)

# make likes to delete intermediate files. This prevents it from deleting the
# files generated by grit after building the GBA ROM.
.SECONDARY:

-include $(DEPSDIR)/*.d
-include $(DEPSDIR)/*.sd
#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
