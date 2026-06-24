export TOPLEVEL := $(CURDIR)

gba:
	@$(MAKE) -f makefiles/gba.mk $(word 2, $(MAKECMDGOALS))

pc:
	@$(MAKE) -f makefiles/pc.mk $(word 2, $(MAKECMDGOALS))

clean:

.PHONY: gba pc clean