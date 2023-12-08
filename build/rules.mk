AR      = ar
CC      = gcc
CPP     = g++
STRIP   = strip
RM      = rm
LUA     = lua-5.3.5
OBJS    = main.o common.o ssdp.o http.o soap.o soap_int.o db_sqlite.o scan.o mime.o charset.o scripting.o live.o md5.o luajson.o \
 compat.o ssl.o plugin_hls_common.o plugin_hls.o plugin_hls_new.o plugin_hls_decrypt.o plugin_tsbuf.o plugin_lua.o plugin_udprtp.o plugin_tsfilter.o
SLIBS   = sqlite3/libsqlite3.a $(LUA)/liblua.a

all: version $(SLIBS) $(OBJS)
	$(CPP) $(CFLAGS) -I$(LUA) -o xupnpd $(OBJS) $(SLIBS) $(LIBS)
	$(STRIP) xupnpd

sqlite3/libsqlite3.a:
	$(CC) -O2 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -c sqlite3/sqlite3.c
	$(AR) -cr $@ sqlite3.o

$(LUA)/liblua.a:
	$(MAKE) -C $(LUA) a MYCFLAGS="-DLUA_USE_LINUX -O2"

version:
	./ver.sh

clean:
	$(MAKE) -C $(LUA) clean
	$(RM) -f $(OBJS) xupnpd.db xupnpd version.h sqlite3/libsqlite3.a sqlite3.o xupnpd.uid

.c.o:
	$(CC) -c $(CFLAGS) -I$(LUA) -o $@ $<

.cpp.o:
	$(CPP) -c $(CFLAGS) -I$(LUA) -o $@ $<
