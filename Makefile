# TODO: this probably should just be a shell script?
export TOPLEVEL := $(CURDIR)
export DEVDEBUG ?= yes

gba:
	@$(MAKE) -f makefiles/gba.mk $(word 2, $(MAKECMDGOALS))

pc:
	@$(MAKE) -f makefiles/pc.mk $(word 2, $(MAKECMDGOALS))

web:
	@$(MAKE) -f makefiles/web.mk $(word 2, $(MAKECMDGOALS))

clean:

.PHONY: gba pc web clean