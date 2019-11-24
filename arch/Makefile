
include $(TOP)/configs/.armv7xx

CC := $$CCBIN_TOP
CCASM := $$CCASM_TOP
CCFLAGS := $$CCFLAGS_TOP
CCDEFS := $$CCDEFS_TOP
CCINC := $$CCINCTOP

ASFLAGS_M7 := $$ASFLAGS_M7_TOP

armv7xx :
	mkdir -p ./.output
	mkdir -p ./.output/obj

	cp ./Arm/m7/src/gcc/* ./.output/
	cp Makefile ./.output

	$(MAKE) _armv7xx TOP=$(TOP) -C ./.output
_armv7xx :
		$(CCASM) $(ASFLAGS_M7) -c ./*.s

		mv ./*.out ./obj/
clean :
	rm -rf ./.output