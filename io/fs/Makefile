
CC := $$CCBIN_TOP

io :
	mkdir -p ./.output/obj
	mkdir -p ./.output/lib
	mkdir -p ./.output/src
	mkdir -p ./.output/src/option

	cp -r ./*.c ./.output/src/
	cp -r ./$(FSSRC)/src/*.c ./.output/src/ 
	cp -r ./$(FSSRC)/src/*.h ./.output/src/ 
	cp -r ./$(FSSRC)/src/option/*.c ./.output/src/option/
	cp $(TOP)/Inc/ffconf.h ./.output/src/

	cp ./Makefile ./.output/

	$(MAKE) _io -C ./.output/

CCINC := \
	-I./src $(CCINC_TOP)

_io :
	$(CC) $(CCFLAGS) $$CCFLAGS_TOP $(CCINC) $$CCDEFS_TOP $(CCDEFS) -c ./src/*.c
#	$(CC) $(CCFLAGS) $$CCFLAGS_TOP $(CCINC) $$CCDEFS_TOP $(CCDEFS) -c ./src/option/*.c

	mv ./*.o ./obj/

clean :
	rm -rf ./.output