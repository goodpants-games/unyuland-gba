ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/gba_rules

#---------------------------------------------------------------------------------
# the LIBGBA path is defined in gba_rules, but we have to define LIBTONC ourselves
#---------------------------------------------------------------------------------
LIBTONC := $(DEVKITPRO)/libtonc

#---------------------------------------------------------------------------------
# platform-specific sources
#---------------------------------------------------------------------------------
SOURCES		:= src/gba


#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-mthumb -mthumb-interwork
CFLAGS	:=	-mcpu=arm7tdmi -mtune=arm7tdmi $(ARCH)
ASFLAGS	:=	$(ARCH)
LDFLAGS	:=	$(ARCH) -Wl,-Map,$(notdir $*.map)


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

export PLATFORM_MAKEFILE := $(TOPLEVEL)/makefiles/gba.mk
include $(TOPLEVEL)/makefiles/common.mk