PLATFORM ?= $(PLATFORM)
OUT ?= $(OUT)
TOP ?= $(TOP)
Q ?= @

include $(TOP)/boot.mk

ARMV7 := armv7

arch :
	@echo "Compiling $(MCPUNAME_MK) $@..."

	@mkdir -p ./.output
	@mkdir -p ./.output/obj

ifeq ($(MCPUNAME_MK), armv7-m)
	@cp ./Arm/m7/src/gcc/* ./.output/
else
	@echo "Unknown arch model [$(MCPUNAME_MK)]"
endif
	$(Q) $(AS) $(ASFLAGS_MK) -c ./.output/*.s

	@mv ./a.out ./.output/obj/asm.o
	@cp ./.output/obj/*.o $(OUT)

clean :
	@rm -rf ./.output