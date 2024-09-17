###########################################
# Acorn
# -------------------------------------
# file       : Makefile
# author     : Ben Kietzman
# begin      : 2018-12-17
# copyright  : kietzman.org
# email      : ben@kietzman.org
###########################################
# {{{ prep work
prefix=/usr/local
all: bin/addrInfo bin/format bin/gateway bin/jwt bin/logger bin/mssql bin/mysql bin/password bin/ping bin/router
install: bin/addrInfo bin/format bin/gateway bin/jwt bin/logger bin/mssql bin/mysql bin/password bin/ping bin/router
	install --mode=755 bin/addrInfo $(prefix)/acorn/cup/
	install --mode=755 bin/format $(prefix)/acorn/cup/
	install --mode=755 bin/gateway $(prefix)/acorn/cup/
	install --mode=755 bin/jwt $(prefix)/acorn/cup/
	install --mode=755 bin/logger $(prefix)/acorn/cup/
	install --mode=755 bin/mssql $(prefix)/acorn/cup/
	install --mode=755 bin/mysql $(prefix)/acorn/cup/
	install --mode=755 bin/password $(prefix)/acorn/cup/
	install --mode=755 bin/ping $(prefix)/acorn/cup/
	install --mode=755 bin/router $(prefix)/acorn/cup/
# }}}

# {{{ addrInfo
addrInfo: bin/addrInfo
	install --mode=755 bin/addrInfo $(prefix)/acorn/cup/
bin/addrInfo: ../../common/libcommon.a obj/addrInfo.o bin
	g++ -o bin/addrInfo obj/addrInfo.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/addrInfo.o: addrInfo.cpp obj
	g++ -g -Wall -c addrInfo.cpp -o obj/addrInfo.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ format
format: bin/format
	install --mode=755 bin/format $(prefix)/acorn/cup/
bin/format: ../../common/libcommon.a obj/format.o bin
	g++ -o bin/format obj/format.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/format.o: format.cpp obj
	g++ -g -Wall -c format.cpp -o obj/format.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ gateway
gateway: bin/gateway
	install --mode=755 bin/gateway $(prefix)/acorn/cup/
bin/gateway: ../../common/libcommon.a obj/gateway.o bin
	g++ -o bin/gateway obj/gateway.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/gateway.o: gateway.cpp obj
	g++ -g -Wall -c gateway.cpp -o obj/gateway.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ jwt
jwt: bin/jwt
	install --mode=755 bin/jwt $(prefix)/acorn/cup/
bin/jwt: ../../common/libcommon.a obj/jwt.o bin
	g++ -o bin/jwt obj/jwt.o $(LDFLAGS) -L/usr/local/lib -L../../common -lcommon -lb64 -lcrypto -lexpat -ljwt -lmjson -lpthread -lssl -ltar -lz
obj/jwt.o: jwt.cpp obj
	g++ -g -Wall -c jwt.cpp -o obj/jwt.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ logger
logger: bin/logger
	install --mode=755 bin/logger $(prefix)/acorn/cup/
bin/logger: ../../common/libcommon.a obj/logger.o bin
	g++ -o bin/logger obj/logger.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/logger.o: logger.cpp obj
	g++ -g -Wall -c logger.cpp -o obj/logger.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ mssql
mssql: bin/mssql
	install --mode=755 bin/mssql $(prefix)/acorn/cup/
bin/mssql: ../../common/libcommon.a obj/mssql.o bin
	g++ -o bin/mssql obj/mssql.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lmysqlclient -lpthread -lssl -lsybdb -ltar -lz
obj/mssql.o: mssql.cpp obj
	g++ -g -Wall -c mssql.cpp -o obj/mssql.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ mysql
mysql: bin/mysql
	install --mode=755 bin/mysql $(prefix)/acorn/cup/
bin/mysql: ../../common/libcommon.a obj/mysql.o obj/Mysql.o bin
	g++ -o bin/mysql obj/mysql.o obj/Mysql.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lmysqlclient -lpthread -lssl -ltar -lz
obj/mysql.o: mysql.cpp obj
	g++ -g -Wall -c mysql.cpp -o obj/mysql.o $(CPPFLAGS) -I../../common;
obj/Mysql.o: include/Mysql.cpp obj
	g++ -g -Wall -c include/Mysql.cpp -o obj/Mysql.o $(CPPFLAGS);
# }}}
# {{{ password
password: bin/password
	install --mode=755 bin/password $(prefix)/acorn/cup/
bin/password: ../../common/libcommon.a obj/password.o bin
	g++ -o bin/password obj/password.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/password.o: password.cpp obj
	g++ -g -Wall -c password.cpp -o obj/password.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ ping
ping: bin/ping
	install --mode=755 bin/ping $(prefix)/acorn/cup/
bin/ping: ../../common/libcommon.a obj/ping.o bin
	g++ -o bin/ping obj/ping.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/ping.o: ping.cpp obj
	g++ -g -Wall -c ping.cpp -o obj/ping.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ router
router: bin/router
	install --mode=755 bin/router $(prefix)/acorn/cup/
bin/router: ../../common/libcommon.a obj/router.o bin
	g++ -o bin/router obj/router.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/router.o: router.cpp obj
	g++ -g -Wall -c router.cpp -o obj/router.o $(CPPFLAGS) -I../../common;
# }}}

# {{{ post work
bin:
	if [ ! -d bin ]; then mkdir bin; fi;
../../common/libcommon.a: ../../common/Makefile
	cd ../../common; make;
../../common/Makefile: ../../common/configure
	cd ../../common; ./configure;
../../common/configure:
	cd ../../; git clone https://github.com/benkietzman/common.git
obj:
	if [ ! -d obj ]; then mkdir obj; fi;
clean:
	-rm -fr obj bin
# }}}
