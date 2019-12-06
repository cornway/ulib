PLATFORM ?= $(PLATFORM)
TOP ?= $(TOP)
OUT ?= $(OUT)
Q ?= @

include $(TOP)/configs/$(PLATFORM)/boot.mk

CCFLAGS := $(CCFLAGS_MK)
CCDEFS := $(CCDEFS_MK)

CCINC := -I$(TOP)/ulib/pub \
		 -I$(TOP)/configs/$(PLATFORM) \
		 -I$(TOP)/ulib/arch

iofs :
	@echo "Compiling $(IOFS_MK) $@..."

	@mkdir -p ./.output/obj
	@mkdir -p ./.output/src

	@cp $(TOP)/configs/$(PLATFORM)/ffconf.h ./.output/src/

	@cp -r ./$(IOFS_MK)/src/*.c ./.output/src/ 
	@cp -r ./$(IOFS_MK)/src/*.h ./.output/src/ 

	$(Q) $(CC) $(CCFLAGS) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./.output/src/*.c

	@mv ./*.o ./.output/obj/
	@cp ./.output/obj/*.o $(OUT)
	
clean :
	@rm -rf ./.output