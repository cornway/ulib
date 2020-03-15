PLATFORM ?= $(PLATFORM)
OUT ?= $(OUT)
TOP ?= $(TOP)
Q ?= @

include $(TOP)/boot.mk

arch :
	@echo "[ $@... ]"

	@mkdir -p ./.output
	@mkdir -p ./.output/obj

ifeq ($(TARGET_CPU), armv7-m)
	@cp ./arm/m7/src/gcc/* ./.output/
else
	@echo "Unknown : [$(TARGET_CPU)]"
endif
	$(Q) $(AS) $(ASFLAGS) -c ./.output/*.s

	@mv ./a.out ./.output/obj/asm.o
	@cp ./.output/obj/*.o $(OUT)

clean :
	@rm -rf ./.output