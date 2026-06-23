#---------------------------------------------------------------------------------
# platform-specific sources
#---------------------------------------------------------------------------------
SOURCES		:= src/pc/tonc src/pc/maxmod src/pc


#---------------------------------------------------------------------------------
# targets
#---------------------------------------------------------------------------------
define CLEAN =
@rm -fr $(BUILD) $(TARGET).elf $(TARGET).gba
endef

define BUILD_TARGETS
$(OUTPUT).exe: $(OFILES)
	$(SILENTCMD)$(CC) $(OFILES) $(LDFLAGS) -o $(OUTPUT).exe
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
# canned command sequence for binary data
#---------------------------------------------------------------------------------
define bin2o
        $(eval _tmpasm := $(shell mktemp))
        $(SILENTCMD)bin2s -a 4 -H `(echo $(<F) | tr . _)`.h $< > $(_tmpasm)
        $(SILENTCMD)$(CC) -x assembler-with-cpp $(CPPFLAGS) $(ASFLAGS) -c $(_tmpasm) -o $(<F).o
        @rm $(_tmpasm)
endef

#---------------------------------------------------------------------------------
export PLATFORM_MAKEFILE := $(TOPLEVEL)/makefiles/pc.mk
include $(TOPLEVEL)/makefiles/common.mk