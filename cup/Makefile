###########################################
# Acorn
# -------------------------------------
# file       : Makefile
# author     : Ben Kietzman
# begin      : 2018-12-17
# copyright  : kietzman.org
# email      : ben@kietzman.org
###########################################

MAKEFLAGS="-j ${C}"

all: bin/addrInfo bin/format bin/gateway bin/jwt bin/logger bin/mssql bin/mysql bin/password bin/ping bin/router

bin/addrInfo: ../../common/libcommon.a obj/addrInfo.o bin
	g++ -o bin/addrInfo obj/addrInfo.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lnsl -lpthread -lssl -ltar -lz

bin/format: ../../common/libcommon.a obj/format.o bin
	g++ -o bin/format obj/format.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lnsl -lpthread -lssl -ltar -lz

bin/gateway: ../../common/libcommon.a obj/gateway.o bin
	g++ -o bin/gateway obj/gateway.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lnsl -lpthread -lssl -ltar -lz

bin/jwt: ../../common/libcommon.a obj/jwt.o bin
	g++ -o bin/jwt obj/jwt.o $(LDFLAGS) -L/usr/local/lib -L../../common -lcommon -lb64 -lcrypto -lexpat -ljwt -lmjson -lnsl -lpthread -lssl -ltar -lz

bin/logger: ../../common/libcommon.a obj/logger.o bin
	g++ -o bin/logger obj/logger.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lnsl -lpthread -lssl -ltar -lz

bin/mssql: ../../common/libcommon.a obj/mssql.o bin
	g++ -o bin/mssql obj/mssql.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lmysqlclient -lnsl -lpthread -lssl -lsybdb -ltar -lz

bin/mysql: ../../common/libcommon.a obj/mysql.o obj/Mysql.o bin
	g++ -o bin/mysql obj/mysql.o obj/Mysql.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lmysqlclient -lnsl -lpthread -lssl -ltar -lz

bin/password: ../../common/libcommon.a obj/password.o bin
	g++ -o bin/password obj/password.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lnsl -lpthread -lssl -ltar -lz

bin/ping: ../../common/libcommon.a obj/ping.o bin
	g++ -o bin/ping obj/ping.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lnsl -lpthread -lssl -ltar -lz

bin/router: ../../common/libcommon.a obj/router.o bin
	g++ -o bin/router obj/router.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lnsl -lpthread -lssl -ltar -lz

bin:
	if [ ! -d bin ]; then mkdir bin; fi;

../../common/libcommon.a:
	cd ../../common; ./configure; make;

obj/addrInfo.o: addrInfo.cpp obj
	g++ -g -std=c++14 -Wall -c addrInfo.cpp -o obj/addrInfo.o $(CPPFLAGS) -I../../common;

obj/format.o: format.cpp obj
	g++ -g -std=c++14 -Wall -c format.cpp -o obj/format.o $(CPPFLAGS) -I../../common;

obj/gateway.o: gateway.cpp obj
	g++ -g -std=c++14 -Wall -c gateway.cpp -o obj/gateway.o $(CPPFLAGS) -I../../common;

obj/jwt.o: jwt.cpp obj
	g++ -g -std=c++14 -Wall -c jwt.cpp -o obj/jwt.o $(CPPFLAGS) -I../../common;

obj/logger.o: logger.cpp obj
	g++ -g -std=c++14 -Wall -c logger.cpp -o obj/logger.o $(CPPFLAGS) -I../../common;

obj/mssql.o: mssql.cpp obj
	g++ -g -std=c++14 -Wall -c mssql.cpp -o obj/mssql.o $(CPPFLAGS) -I../../common;

obj/mysql.o: mysql.cpp obj
	g++ -g -std=c++14 -Wall -c mysql.cpp -o obj/mysql.o $(CPPFLAGS) -I../../common;

obj/password.o: password.cpp obj
	g++ -g -std=c++14 -Wall -c password.cpp -o obj/password.o $(CPPFLAGS) -I../../common;

obj/ping.o: ping.cpp obj
	g++ -g -std=c++14 -Wall -c ping.cpp -o obj/ping.o $(CPPFLAGS) -I../../common;

obj/router.o: router.cpp obj
	g++ -g -std=c++14 -Wall -c router.cpp -o obj/router.o $(CPPFLAGS) -I../../common;

obj/Mysql.o: include/Mysql.cpp obj
	g++ -g -std=c++14 -Wall -c include/Mysql.cpp -o obj/Mysql.o $(CPPFLAGS);

obj:
	if [ ! -d obj ]; then mkdir obj; fi;

clean:
	-rm -fr obj bin
