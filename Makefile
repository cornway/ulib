PLATFORM ?= $(PLATFORM)
TOP ?= $(TOP)
OUT ?= $(OUT)
Q ?= @

include $(TOP)/configs/$(PLATFORM)/boot.mk

CCFLAGS = $(CCFLAGS_MK)
LDFLAGS = $(LDFLAGS_MK)
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

MODULE ?=

ulib/module :
	@echo "Compiling [ $(MODULE) ]..."

	$(Q)mkdir -p ./.output/$(MODULE)
	$(Q)cp -r ./$(MODULE)/* ./.output/$(MODULE)
	
	$(Q)$(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./.output/$(MODULE)/*.c

define module/compile
$(MAKE) ulib/module TOP=$(TOP) PLATFORM=$(PLATFORM) OUT=./$(OUT_OBJ) Q= -C ./
endef


$(ULIB_OBJ)/*.o :
	@echo "Compiling $@..."

	$(Q)mkdir -p ./.output/obj

	$(module/compile) MODULE=lib
	$(module/compile) MODULE=pub
ifeq ($(HAVE_AUDIO), 1)
	$(module/compile) MODULE=audio
endif
ifeq ($(HAVE_HDMI), 1)
	$(module/compile) MODULE=hdmi
endif
	$(module/compile) MODULE=mem
	$(module/compile) MODULE=misc
	$(module/compile) MODULE=boot
ifeq ($(HAVE_LCD), 1)
	$(module/compile) MODULE=screen
endif
	$(module/compile) MODULE=gfx
	$(module/compile) MODULE=gui
	$(module/compile) MODULE=drv
	$(module/compile) MODULE=io
ifeq ($(HAVE_USB), 1)
	$(module/compile) MODULE=usb/$(MACHNAME_MK)
endif
ifeq ($(HAVE_LCD), 1)
	$(module/compile) MODULE=screen/$(MACHNAME_MK)
endif

	$(Q)mv ./*.o ./.output/obj/
	$(Q)cp -r ./.output/obj/*.o $(OUT)

ulib/modules :
	@echo "Compiling [ $(MODULE).so ]..."

	$(Q)mkdir -p ./.output/$(MODULE)
	$(Q)mkdir -p ./.output/$(MODULE)/lib
	$(Q)cp -r ./$(MODULE)/* ./.output/$(MODULE)
	
	$(Q)$(CC) $(CCSHARED) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./.output/$(MODULE)/*.c
	$(Q)cp ./*.o ./.output/$(MODULE)/lib
	$(Q)$(CC) $(LDSHARED) $(LDFLAGS) -o ./.output/$(MODULE)/lib/$(MODULE).so ./.output/$(MODULE)/lib/*.o

ulib/modules/clean :
	$(Q)rm -rf ./.output/$(MODULE)

clean :
	$(Q)rm -rf ./.output

	$(MAKE) clean TOP=$(TOP) -C ./io/fs/
	$(MAKE) clean TOP=$(TOP) -C ./arch/