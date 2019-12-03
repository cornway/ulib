PLATFORM ?= $(PLATFORM)
TOP ?= $(TOP)
IOFS ?= $(IOFS)

include $(TOP)/configs/$(PLATFORM)/boot.mk

CCFLAGS := $(CCFLAGS_MK)
CCDEFS := $(CCDEFS_MK)

CCINC := -I./src \
		$$CCINCPUB

io :
	mkdir -p ./.output/obj
	mkdir -p ./.output/lib
	mkdir -p ./.output/bin
	mkdir -p ./.output/src
	mkdir -p ./.output/src/option

	echo $(CCDEFS)
	echo !!!

	cp -r ./*.c ./.output/src/
	cp -r ./$(IOFS)/src/*.c ./.output/src/ 
	cp -r ./$(IOFS)/src/*.h ./.output/src/ 
	cp $(TOP)/main/Inc/ffconf.h ./.output/src/

	cp ./Makefile ./.output/

	$(MAKE) _io -C ./.output/


_io :
	$(CC) $(CCFLAGS) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./src/*.c

	mv ./*.o ./obj/

clean :
	rm -rf ./.output