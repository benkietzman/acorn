###########################################
# Acorn
# -------------------------------------
# file       : Makefile
# author     : Ben Kietzman
# begin      : 2018-12-17
# copyright  : kietzman.org
# email      : ben@kietzman.org
###########################################

#MAKEFLAGS="-j ${C}"
prefix=/usr/local

all: bin/cap bin/load

bin/cap: ../common/libcommon.a obj/cap.o bin
	g++ -o bin/cap obj/cap.o $(LDFLAGS) -L../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lnsl -lpthread -lrt -lssl -ltar -lz

bin/load: ../common/libcommon.a obj/load.o bin
	g++ -o bin/load obj/load.o $(LDFLAGS) -L../common -lcommon -lexpat -lmjson -lncurses -lpthread

bin:
	-if [ ! -d bin ]; then mkdir bin; fi;

../common/libcommon.a: ../common/Makefile
	cd ../common; ./configure; make;

../common/Makefile: ../common/configure
	cd ../common; ./configure;

../common/configure:
	cd ../; git clone https://github.com/benkietzman/common.git

obj/cap.o: cap.cpp obj ../common/Makefile
	g++ -g -Wall -c cap.cpp -o obj/cap.o $(CPPFLAGS) -I../common

obj/load.o: load.cpp obj ../common/Makefile
	g++ -g -Wall -c load.cpp -o obj/load.o $(CPPFLAGS) -I../common

obj:
	-if [ ! -d obj ]; then mkdir obj; fi;

install: bin/cap
	-if [ ! -d $(prefix)/acorn ]; then mkdir $(prefix)/acorn; fi;
	-if [ ! -d $(prefix)/acorn/cup ]; then mkdir $(prefix)/acorn/cup; fi;
	install --mode=777 bin/cap $(prefix)/acorn/cap

clean:
	-rm -fr obj bin

uninstall:
	-rm -fr $(prefix)/acorn
