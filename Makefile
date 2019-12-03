
PLATFORM ?= $(PLATFORM)
TOP ?= $(TOP)

include $(TOP)/configs/$(PLATFORM)/boot.mk

CCFLAGS := $(CCFLAGS_MK)
CCDEFS := $(CCDEFS_MK)

export CCINCPUB := -I$(TOP)/ulib/pub \
				   -I$(TOP)/ulib/arch

CCINC := -I$(TOP)/main/Inc \
		-I$(TOP)/common/Utilities/JPEG \
		-I$(TOP)/ulib/boot/inc \
		-I$(TOP)/ulib/gui \
		$(CCINCPUB) \
		$(HALINC_MK)

CCINC += -I$(TOP)/ulib/io/fs/$(IOFS_MK)/src

ulib :
	mkdir -p ./.output

	echo !!!
	echo $(TOP)/ulib/io/fs/$(IOFS)/src

	cp -r ./lib/*.c ./.output/
	cp -r ./pub/*.c ./.output/

	cp -r ./audio/*.c ./.output/
	cp -r ./hdmi/hdmi_mem.c ./.output
	cp -r ./mem/*.c ./.output/
	cp -r ./misc/*.c ./.output/
	cp -r ./boot/*.c ./.output/
	cp -r ./screen/*.c ./.output/
	cp -r ./gfx/*.c ./.output/
	cp -r ./gui/*.c ./.output/
	cp -r ./drv/*.c ./.output/

	cp -r ./io/*.c ./.output/

	cp -r ./usb/$(MACHNAME_MK)/*.c ./.output/
	cp -r ./usb/$(MACHNAME_MK)/*.h ./.output/

	cp -r ./screen/$(MACHNAME_MK)/*.c ./.output/


	cp -r ./arch ./.output/
	cp -r ./io ./.output/

	cp ./Makefile ./.output/

	$(MAKE) $(MCPUNAME_MK) TOP=$(TOP) -C ./arch
	$(MAKE) io TOP=$(TOP) PLATFORM=$(PLATFORM) IOFS=$(IOFS_MK) -C ./io/fs

	$(MAKE) _ulib TOP=$(TOP) PLATFORM=$(PLATFORM) -C ./.output

	mv ./arch/.output/obj/*.out ./.output/obj/
	mv ./io/fs/.output/obj/*.o ./.output/obj/
	mv ./.output/*.o ./.output/obj/
	$(AR) rcs ./.output/lib/ulib_$(ARCHNAME_MK).a ./.output/obj/*.o ./.output/obj/*.out

_ulib :
	mkdir -p ./lib
	mkdir -p ./obj

	$(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./*.c

clean :
	rm -rf ./.output

	$(MAKE) clean TOP=$(TOP) -C ./io/fs/
	$(MAKE) clean TOP=$(TOP) -C ./arch/