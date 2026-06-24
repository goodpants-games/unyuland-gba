PYTHON ?= python3
ASEPRITE ?= aseprite
DEVDEBUG ?= yes

#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

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
ifeq ($(origin TARGET), undefined)
    TARGET	:= $(notdir $(CURDIR))
endif

ifeq ($(origin BUILD), undefined)
    BUILD	:= build
endif

SOURCES		:= $(SOURCES) src/main
INCLUDES	:= include
DATA		:= data/bin
MUSIC		:= data/music
GRAPHICS	:= data/graphics
MAPS        := data/maps
SPRITES     := data/sprites

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS += -Wall -DPRINTF_INCLUDE_CONFIG_H -std=gnu11
CFLAGS += $(INCLUDE)

ifeq ($(DEVDEBUG),yes)
  CFLAGS += -DDEVDEBUG
endif

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS += -g
LDFLAGS += -g


ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

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
TMXFILES	:=	$(addsuffix .tmx,$(shell grep -v "^#" data/room_list.txt))
SPRFILES	:=	$(foreach dir,$(SPRITES),$(notdir $(wildcard $(dir)/*.sprdb)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifneq ($(strip $(MUSIC)),)
	export AUDIOFILES := $(foreach dir,$(notdir $(wildcard $(MUSIC)/*.*)),$(CURDIR)/$(MUSIC)/$(dir))
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

export MAPFILES := $(addsuffix .map,$(TMXFILES:.tmx=))

export OFILES_BIN := $(addsuffix .o,$(BINFILES))

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_GRAPHICS := $(addsuffix _gfx.o,$(PNGFILES:.png=))

export OFILES_MAPS := $(addsuffix .o,$(MAPFILES)) world.o automap.bin.o

export OFILES_SPRITES := $(addsuffix _sprdb.bin.o,$(SPRFILES:.sprdb=))\
                         $(addsuffix _sprdb_gfx.o,$(SPRFILES:.sprdb=))

export OFILES_INTERMEDIATE := sinelut.bin.o dlg.bin.o pitchlut.bin.o\
                              wave_tri.bin.o wave_noise.bin.o

export OFILES := $(OFILES_BIN) $(OFILES_GRAPHICS) $(OFILES_INTERMEDIATE)\
                 $(OFILES_MAPS) $(OFILES_SPRITES) $(OFILES_SOURCES)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) \
                    $(foreach dir,$(SOURCES),-I$(CURDIR)/$(dir))

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
$(BUILD):
	$(SILENTCMD)[ -d $@ ] || mkdir -p $@
	$(SILENTCMD)$(MAKE) --no-print-directory -C $(BUILD) -f $(PLATFORM_MAKEFILE)

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	$(CLEAN)

#---------------------------------------------------------------------------------
else

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------

$(eval $(BUILD_TARGETS))

#---------------------------------------------------------------------------------
# allow seeing compiler command lines with make V=1 (similar to autotools' silent)
#---------------------------------------------------------------------------------
ifeq ($(V),1)
    SILENTMSG := @true
    SILENTCMD :=
else
    SILENTMSG := @echo
    SILENTCMD := @
endif

#---------------------------------------------------------------------------------
# Generate compile commands
#---------------------------------------------------------------------------------
ADD_COMPILE_COMMAND ?= @true

#---------------------------------------------------------------------------------
# rules to compile .cpp, .c, and .s files
#---------------------------------------------------------------------------------

%.o: %.cpp
	$(SILENTMSG) $(notdir $<)
	$(ADD_COMPILE_COMMAND) add $(CXX) "$(_EXTRADEFS) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@" $<
	$(SILENTCMD)$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(_EXTRADEFS) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@ $(ERROR_FILTER)

%.o: %.c
	$(SILENTMSG) $(notdir $<)
	$(ADD_COMPILE_COMMAND) add $(CC) "$(_EXTRADEFS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@" $<
	$(SILENTCMD)$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(_EXTRADEFS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@ $(ERROR_FILTER)

%.o: %.s
	$(SILENTMSG) $(notdir $<)
	$(ADD_COMPILE_COMMAND) add $(CC) "-x assembler-with-cpp $(_EXTRADEFS) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@" $<
	$(SILENTCMD)$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d -x assembler-with-cpp $(_EXTRADEFS) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@ $(ERROR_FILTER)

%.o: %.S
	$(SILENTMSG) $(notdir $<)
	$(ADD_COMPILE_COMMAND) add $(CC) "-x assembler-with-cpp $(_EXTRADEFS) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@" $<
	$(SILENTCMD)$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d -x assembler-with-cpp $(_EXTRADEFS) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@ $(ERROR_FILTER)


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
# These rules convert Tiled level files to a more efficient format readable by
# the game.
#---------------------------------------------------------------------------------
%.map.o %_map.h: %.map
	@echo $(notdir $<)
	@$(bin2o)

%.map: %.tmx $(TOPLEVEL)/tools/mapc.py
	@$(PYTHON) $(TOPLEVEL)/tools/mapc.py $< $@

#---------------------------------------------------------------------------------
# This rule compiles .sprdb files to a sprdb binary data and image file, using
# Aseprite with the sprdb.lua script. The image file should then compiled with
# grit.
#---------------------------------------------------------------------------------
%_sprdb.bin %_sprdb.png %_sprdb.grit %_sprdb.h: %.sprdb
#---------------------------------------------------------------------------------
	@echo $(notdir $<)

	@$(ASEPRITE) --batch \
	             --script-param sprdb="$<" \
	             --script-param output_dat="$*_sprdb.bin" \
	             --script-param output_img="$*_sprdb.png" \
	             --script-param output_h="`(echo $(<F) | tr . _)`.h" \
	             --script-param out_dep="$(DEPSDIR)/$(notdir $(<:.sprdb=.sd))" \
	             --script-param artifact_name="$@" \
	             --script       "$(TOPLEVEL)/tools/sprdb.lua"

	@echo -fts -gB 4 -p! > $*_sprdb.grit

#---------------------------------------------------------------------------------
# This rule compiles the positions of each room in the world matrix, as well
# as the room order. This data is then read by mapc.
# Also creates a pointer list to each map file, as well as the world matrix.
#---------------------------------------------------------------------------------
world.json world.c world.h automap.bin: $(MAPFILES) $(TOPLEVEL)/tools/worldproc.py
#---------------------------------------------------------------------------------
	@$(PYTHON) $(TOPLEVEL)/tools/worldproc.py \
	           $(TOPLEVEL)/data/maps/unyuland.world \
	           $(TOPLEVEL)/data/room_list.txt \
	           --json world.json \
			   --c world.c \
			   --automap automap.bin

#---------------------------------------------------------------------------------
# This rule creates the sine look-up table.
#---------------------------------------------------------------------------------
sinelut.bin:
#---------------------------------------------------------------------------------
	@$(PYTHON) $(TOPLEVEL)/tools/sinelut.py \
	           $@

#---------------------------------------------------------------------------------
# This rule creates the dialogue file.
#---------------------------------------------------------------------------------
dlg.bin: $(TOPLEVEL)/data/dialogue.json $(TOPLEVEL)/tools/dlgc.py
#---------------------------------------------------------------------------------
	@$(PYTHON) $(TOPLEVEL)/tools/dlgc.py \
	           $< $@

#---------------------------------------------------------------------------------
# This rule creates the pitch look-up table.
#---------------------------------------------------------------------------------
pitchlut.bin: $(TOPLEVEL)/tools/pitchlut.py
#---------------------------------------------------------------------------------
	@$(PYTHON) $(TOPLEVEL)/tools/pitchlut.py \
	           $@

#---------------------------------------------------------------------------------
# This rule precomputes the PSG triangle wave table.
#---------------------------------------------------------------------------------
wave_tri.bin: $(TOPLEVEL)/tools/wavetable.py
#---------------------------------------------------------------------------------
	@$(PYTHON) $(TOPLEVEL)/tools/wavetable.py \
	           -w triangle $@

#---------------------------------------------------------------------------------
# This rule precomputes the PSG noise wave table.
#---------------------------------------------------------------------------------
wave_noise.bin: $(TOPLEVEL)/tools/wavetable.py
#---------------------------------------------------------------------------------
	@$(PYTHON) $(TOPLEVEL)/tools/wavetable.py \
	           -w noise $@

# make likes to delete intermediate files. This prevents it from deleting the
# files generated by grit after building the GBA ROM.
.SECONDARY:

-include $(DEPSDIR)/*.d
-include $(DEPSDIR)/*.sd
#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
