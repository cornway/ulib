
include $(TOP)/configs/.armv7xx

ulib_stm32f7xx :
	mkdir -p ./.output

	cp -r ./lib/*.c ./.output/
	cp -r ./pub/*.c ./.output/

	cp -r ./audio/*.c ./.output/
	cp -r ./hdmi/hdmi_mem.c ./.output
	cp -r ./mem/*.c ./.output/
	cp -r ./misc/*.c ./.output/
	cp -r ./usb/stm32/*.c ./.output/
	cp -r ./boot/*.c ./.output/
	cp -r ./screen/stm32/*.c ./.output/
	cp -r ./screen/*.c ./.output/
	cp -r ./gfx/*.c ./.output/
	cp -r ./gui/*.c ./.output/
	cp -r ./drv/*.c ./.output/

	cp -r ./arch ./.output/
	cp -r ./io ./.output/

	cp ./Makefile ./.output/

	$(MAKE) _ulib_stm32f7xx TOP=$(TOP) -C ./.output

CC := $$CCBIN_TOP

CCFLAGS := $$CCFLAGS_TOP
CCDEFS := $$CCDEFS_TOP
CCINC := \
	-I../usb/stm32 \
	-I../boot/inc \
	-I../gui \
	$(CCINC_TOP)


_ulib_stm32f7xx :
	mkdir -p ./lib
	mkdir -p ./obj

	$(MAKE) armv7xx TOP=$(TOP) -C ./arch
	$(MAKE) io TOP=$(TOP) CCDEFS=$(CCDEFS_ARMV7XX) FSSRC=FatFs -C ./io/fs

	$(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) $(CCINC_ARMV7XX) $(CCFLAGS_ARMV7XX) $(CCDEFS_ARMV7XX) -c ./*.c

	mv ./arch/.output/obj/*.out ./obj/
	mv ./io/fs/.output/obj/*.o ./obj/
	mv ./*.o ./obj/
	ar rcs ./lib/ulib_stm32f7xx.a ./obj/*.o ./obj/*.out

clean :
	rm -rf ./.output

	$(MAKE) clean TOP=$(TOP) -C ./io/fs/
	$(MAKE) clean TOP=$(TOP) -C ./arch/