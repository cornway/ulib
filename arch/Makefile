
PLATFORM ?= $(PLATFORM)
TOP ?= $(TOP)

include $(TOP)/configs/$(PLATFORM)/boot.mk

$(MCPUNAME_MK) :
	mkdir -p ./.output
	mkdir -p ./.output/obj

	cp ./Arm/m7/src/gcc/* ./.output/
	cp Makefile ./.output

	$(MAKE) _$(MCPUNAME_MK) TOP=$(TOP) -C ./.output

_$(MCPUNAME_MK) :
	$(AS) $(ASFLAGS_MK) -c ./*.s

	mv ./*.out ./obj/

clean :
	rm -rf ./.output