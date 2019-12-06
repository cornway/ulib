PLATFORM ?= $(PLATFORM)
TOP ?= $(TOP)
OUT ?= $(OUT)
Q ?= @

include $(TOP)/configs/$(PLATFORM)/boot.mk

CCFLAGS = $(CCFLAGS_MK)
CCDEFS = $(CCDEFS_MK)

CCINC = -I$(TOP)/common/Utilities/JPEG \
		-I$(TOP)/ulib/boot/inc \
		-I$(TOP)/ulib/gui \
		-I$(TOP)/ulib/arch \
		-I$(TOP)/ulib/pub \
		-I$(TOP)/configs/$(PLATFORM) \
		-I$(TOP)/main/Inc \
		$(HALINC_MK)

CCINC += -I$(TOP)/ulib/io/fs/$(IOFS_MK)/src

OUT_OBJ := .output/obj

ARCH_OBJ := arch/.output/obj
IOFS_OBJ := io/fs/.output/obj
ULIB_OBJ := .output/obj

ulib : ulib/arch ulib/iofs ulib/ulib ulib/all

ulib/arch : $(ARCH_OBJ)/*.o
ulib/iofs : $(IOFS_OBJ)/*.o
ulib/ulib : $(ULIB_OBJ)/*.o

ulib/all :
	$(Q)cp ./.output/obj/*.o $(OUT)/

$(ARCH_OBJ)/*.o :
	$(Q)mkdir -p ./.output/obj
	$(MAKE) TOP=$(TOP) OUT=../$(OUT_OBJ) Q=$(Q) -C ./arch

$(IOFS_OBJ)/*.o :
	$(Q)mkdir -p ./.output/obj
	$(MAKE) iofs TOP=$(TOP) PLATFORM=$(PLATFORM) OUT=../../$(OUT_OBJ) Q=$(Q) -C ./io/fs

$(ULIB_OBJ)/*.o :
	@echo "Compiling $@..."

	$(Q)mkdir -p ./.output/obj

	$(Q)cp -r ./lib/*.c ./.output/
	$(Q)cp -r ./pub/*.c ./.output/

	$(Q)cp -r ./audio/*.c ./.output/
	$(Q)cp -r ./hdmi/*.c ./.output
	$(Q)cp -r ./mem/*.c ./.output/
	$(Q)cp -r ./misc/*.c ./.output/
	$(Q)cp -r ./boot/*.c ./.output/
	$(Q)cp -r ./screen/*.c ./.output/
	$(Q)cp -r ./gfx/*.c ./.output/
	$(Q)cp -r ./gui/*.c ./.output/
	$(Q)cp -r ./drv/*.c ./.output/

	$(Q)cp -r ./io/*.c ./.output/

	$(Q)cp -r ./usb/$(MACHNAME_MK)/*.c ./.output/
	$(Q)cp -r ./usb/$(MACHNAME_MK)/*.h ./.output/
	$(Q)cp -r ./screen/$(MACHNAME_MK)/*.c ./.output/

	$(Q) $(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./.output/*.c

	$(Q)mv ./*.o ./.output/obj/
	$(Q)cp -r ./.output/obj/*.o $(OUT)

clean :
	$(Q)rm -rf ./.output

	$(MAKE) clean TOP=$(TOP) -C ./io/fs/
	$(MAKE) clean TOP=$(TOP) -C ./arch/