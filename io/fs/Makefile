PLATFORM ?= $(PLATFORM)
TOP ?= $(TOP)
OUT ?= $(OUT)
Q ?= @

include $(TOP)/boot.mk

CCINC := -I$(TOP)/ulib/pub \
		 -I$(TOP)/configs/$(PLATFORM) \
		 -I$(TOP)/ulib/arch

iofs :
	@echo "Compiling $(FS_SOURCE) $@..."

	@mkdir -p ./.output/obj
	@mkdir -p ./.output/src

	@cp $(TOP)/configs/$(PLATFORM)/ffconf.h ./.output/src/

	@cp -r ./$(FS_SOURCE)/src/*.c ./.output/src/ 
	@cp -r ./$(FS_SOURCE)/src/*.h ./.output/src/ 

	$(Q) $(CC) $(CFLAGS) $(CCINC) $(CDEFS) -c ./.output/src/*.c

	@mv ./*.o ./.output/obj/
	@cp ./.output/obj/*.o $(OUT)
	
clean :
	@rm -rf ./.output