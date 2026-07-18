PYTHON ?= python3
ASEPRITE ?= aseprite
DEVDEBUG ?= yes

# TODO: why the fuck does doing make -B cause the map files to be built twice.
#       wtf??

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

ifeq ($(origin TOPLEVEL), undefined)
  # the extra slash is important, so that scripts can do
  # $(TOPLEVEL)makefiles/common.mk
  export TOPLEVEL := $(CURDIR)/
endif

SOURCES		:= $(SOURCES) src/main
INCLUDES	:= $(INCLUDES) src/main src/include
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


ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(CURDIR)
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
SFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.s))
PNGFILES	:=	$(foreach dir,$(GRAPHICS),$(wildcard $(dir)/*.png))
TMXFILES	:=	$(addprefix data/maps/,$(addsuffix .tmx,$(shell grep -v "^#" data/room_list.txt)))
SPRFILES	:=	$(foreach dir,$(SPRITES),$(wildcard $(dir)/*.sprdb))
BINFILES	:=	$(foreach dir,$(DATA),$(wildcard $(dir)/*.*))

ifneq ($(strip $(MUSIC)),)
  MODEXTS := .mod .xm
  export AUDIOFILES := $(foreach dir,$(MUSIC),\
                         $(foreach ext,$(MODEXTS),\
				           $(wildcard $(dir)/*$(ext))))

  ifeq ($(AUDIO_DRIVER),mm)
    SOUNDBANK := data/mm_soundbank.bin.o
  else
    export AUDIO_BINFILES := $(addsuffix .bin,$(basename $(AUDIOFILES)))
	SOUNDBANK := data/mplay_data.o
  endif
endif

BINFILES += data/sinelut.bin data/dlg.bin data/pitchlut.bin data/wave_tri.bin\
            data/wave_noise.bin data/color_qlut.bin

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

export OFILES_AUDIO := $(addsuffix .o,$(AUDIO_BINFILES))

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_GRAPHICS := $(addsuffix _gfx.o,$(PNGFILES:.png=))

export OFILES_MAPS := $(addsuffix .o,$(MAPFILES))

export OFILES_WORLD := data/world.o data/automap.bin.o

export OFILES_SPRITES := $(addsuffix _sprdb.bin.o,$(SPRFILES:.sprdb=))\
                         $(addsuffix _sprdb_gfx.o,$(SPRFILES:.sprdb=))

export OFILES := $(OFILES_BIN) $(OFILES_AUDIO) $(SOUNDBANK) $(OFILES_GRAPHICS)\
                 $(OFILES_MAPS) $(OFILES_WORLD) $(OFILES_SPRITES)\
				 $(OFILES_SOURCES)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

# the first makefile *should* be the  makefile that included this one; i.e., the
# platform makefile.
PLATFORM_MAKEFILE := $(abspath $(firstword $(MAKEFILE_LIST)))

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
	@mkdir -p $(dir $@)
	$(ADD_COMPILE_COMMAND) add $(CXX) "$(_EXTRADEFS) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@" $<
	$(SILENTCMD)$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(_EXTRADEFS) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@ $(ERROR_FILTER)

%.o: %.c
	$(SILENTMSG) $(notdir $<)
	@mkdir -p $(dir $@)
	$(ADD_COMPILE_COMMAND) add $(CC) "$(_EXTRADEFS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@" $<
	$(SILENTCMD)$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(_EXTRADEFS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@ $(ERROR_FILTER)

%.o: %.s
	$(SILENTMSG) $(notdir $<)
	@mkdir -p $(dir $@)
	$(ADD_COMPILE_COMMAND) add $(CC) "-x assembler-with-cpp $(_EXTRADEFS) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@" $<
	$(SILENTCMD)$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d -x assembler-with-cpp $(_EXTRADEFS) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@ $(ERROR_FILTER)

%.o: %.S
	$(SILENTMSG) $(notdir $<)
	@mkdir -p $(dir $@)
	$(ADD_COMPILE_COMMAND) add $(CC) "-x assembler-with-cpp $(_EXTRADEFS) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@" $<
	$(SILENTCMD)$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d -x assembler-with-cpp $(_EXTRADEFS) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@ $(ERROR_FILTER)


#---------------------------------------------------------------------------------
# canned command sequence for binary data
# The bin2o rule should be copied and modified
# for each extension used in the data directories
#---------------------------------------------------------------------------------
ifeq ($(origin bin2o), undefined)
  define bin2o
    $(eval _tmpasm := $(shell mktemp))
    $(SILENTCMD)$(PYTHON) $(TOPLEVEL)/tools/bin2s.py \
	  -a 4 -H `(echo "$<" | tr . _)`.h $(BIN2S_FLAGS) "$<" \
	  > $(_tmpasm)
    $(SILENTCMD)$(CC) -x assembler-with-cpp $(CPPFLAGS) $(ASFLAGS) -c $(_tmpasm) -o $<.o
    @rm $(_tmpasm)
  endef
endif


#---------------------------------------------------------------------------------
# control evaluation order for parallelized make
#---------------------------------------------------------------------------------
$(OFILES_SOURCES): | $(OFILES_BIN) $(SOUNDBANK) $(OFILES_GRAPHICS)\
                   $(OFILES_INTERMEDIATE) $(OFILES_MAPS) $(OFILES_WORLD)\
                   $(OFILES_SPRITES)
#---------------------------------------------------------------------------------


ifeq ($(AUDIO_DRIVER),mm)
#---------------------------------------------------------------------------------
# rule to build soundbank from music files
#---------------------------------------------------------------------------------
data/mm_soundbank.bin data/mm_soundbank.h data/music.h : $(AUDIOFILES)
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $@)
	$(SILENTCMD)mmutil $^ -odata/mm_soundbank.bin -hdata/mm_soundbank.h
	$(SILENTCMD)$(PYTHON) $(TOPLEVEL)/tools/modidx.py\
	  -t mm $(AUDIOFILES) -o data/music.h

else
#---------------------------------------------------------------------------------
# rule to build mod index from module blobs
#---------------------------------------------------------------------------------
data/mplay_data.c data/music.h : $(AUDIOFILES) $(TOPLEVEL)/tools/modidx.py
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $@)
	$(SILENTCMD)$(PYTHON) $(TOPLEVEL)/tools/modidx.py\
	  --mod-bank data/mplay_data.c $(AUDIOFILES) -o data/music.h

#---------------------------------------------------------------------------------
# mplay data depends on the binary files of each module
#---------------------------------------------------------------------------------
data/mplay_data.o: $(OFILES_AUDIO)
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
# rules to copy music files to the build directory with the .bin extension
#---------------------------------------------------------------------------------
%.bin: %.xm
	@mkdir -p $(dir $@)
	$(SILENTCMD)cp $< $@

%.bin: %.mod
	@mkdir -p $(dir $@)
	$(SILENTCMD)cp $< $@
#---------------------------------------------------------------------------------
endif


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
	@echo $<
	$(bin2o)

%.map: %.tmx $(TOPLEVEL)/tools/mapc.py
	@mkdir -p $(dir $@)
	$(SILENTCMD)$(PYTHON) $(TOPLEVEL)/tools/mapc.py $< $@

#---------------------------------------------------------------------------------
# This rule compiles .sprdb files to a sprdb binary data and image file, using
# Aseprite with the sprdb.lua script. The image file should then compiled with
# grit.
#---------------------------------------------------------------------------------
%_sprdb.bin %_sprdb.png %_sprdb.grit %_sprdb.h: %.sprdb
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@mkdir -p $(dir $@)

	$(SILENTCMD)$(ASEPRITE) --batch \
	  --script-param sprdb="$<" \
	  --script-param output_dat="$*_sprdb.bin" \
	  --script-param output_img="$*_sprdb.png" \
	  --script-param output_h="`(echo "$(basename $@)" | tr . _)`.h" \
	  --script-param out_dep="$(DEPSDIR)/$(basename $@).sd" \
	  --script-param artifact_name="$@" \
	  --script       "$(TOPLEVEL)/tools/sprdb.lua"

	@echo -fts -gB 4 -p! > $*_sprdb.grit

#---------------------------------------------------------------------------------
# This rule compiles the positions of each room in the world matrix, as well
# as the room order. This data is then read by mapc.
# Also creates a pointer list to each map file, as well as the world matrix.
#---------------------------------------------------------------------------------
data/world.c data/world.h data/automap.bin:\
    $(OFILES_MAPS) $(TOPLEVEL)/data/maps/unyuland.world\
	$(TOPLEVEL)/data/room_list.txt $(TOPLEVEL)/tools/worldproc.py
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $@)
	
	$(SILENTCMD)$(PYTHON) $(TOPLEVEL)/tools/worldproc.py \
	  $(TOPLEVEL)/data/maps/unyuland.world \
	  $(TOPLEVEL)/data/room_list.txt \
	  --c data/world.c \
	  --automap data/automap.bin

#---------------------------------------------------------------------------------
# Ensure world.o properly gets rebuilt whenever its dependencies change
#---------------------------------------------------------------------------------
data/world.o: data/music.h $(SOUNDBANK) $(AUDIO_BINFILES) $(OFILES_MAPS)
#---------------------------------------------------------------------------------


#---------------------------------------------------------------------------------
# This rule creates the sine look-up table.
#---------------------------------------------------------------------------------
data/sinelut.bin:
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $@)
	$(SILENTCMD)$(PYTHON) $(TOPLEVEL)/tools/sinelut.py $@

#---------------------------------------------------------------------------------
# This rule creates the dialogue file.
#---------------------------------------------------------------------------------
data/dlg.bin: $(TOPLEVEL)/data/dialogue.json $(TOPLEVEL)/tools/dlgc.py
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $@)
	$(SILENTCMD)$(PYTHON) $(TOPLEVEL)/tools/dlgc.py $< $@

#---------------------------------------------------------------------------------
# This rule creates the pitch look-up table.
#---------------------------------------------------------------------------------
data/pitchlut.bin: $(TOPLEVEL)/tools/pitchlut.py
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $@)
	$(SILENTCMD)$(PYTHON) $(TOPLEVEL)/tools/pitchlut.py $@

#---------------------------------------------------------------------------------
# This rule precomputes the PSG triangle wave table.
#---------------------------------------------------------------------------------
data/wave_tri.bin: $(TOPLEVEL)/tools/wavetable.py
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $@)
	$(SILENTCMD)$(PYTHON) $(TOPLEVEL)/tools/wavetable.py -w triangle $@

#---------------------------------------------------------------------------------
# This rule precomputes the PSG noise wave table.
#---------------------------------------------------------------------------------
data/wave_noise.bin: $(TOPLEVEL)/tools/wavetable.py
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $@)
	$(SILENTCMD)$(PYTHON) $(TOPLEVEL)/tools/wavetable.py -w noise $@

#---------------------------------------------------------------------------------
# This rule precomputes the color quantization look-up table
#---------------------------------------------------------------------------------
data/color_qlut.bin: $(TOPLEVEL)/tools/color.py
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $@)
	$(SILENTCMD)$(PYTHON) $(TOPLEVEL)/tools/color.py qlut -o $@


# make likes to delete intermediate files. This prevents it from deleting the
# files generated by grit after building the GBA ROM.
.SECONDARY:

CDEPS := $(shell find $(DEPSDIR) -name '*.d')
SDEPS := $(shell find $(DEPSDIR) -name '*.sd')
-include $(CDEPS)
-include $(SDEPS)
#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
