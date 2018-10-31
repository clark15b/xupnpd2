CFLAGS  = -Istlport -fno-exceptions -fno-rtti -I. -Os -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DNO_SQLITE -DNO_LIBUUID -Ilua-5.1.5
OBJS    = main.o common.o ssdp.o http.o soap.o soap_int.o db_mem.o scan.o mime.o charset.o scripting.o live.o md5.o luajson.o newdelete.o \
 compat.o plugin_hls_common.o plugin_hls.o plugin_hls_new.o plugin_tsbuf.o plugin_lua.o plugin_udprtp.o plugin_tsfilter.o
LIBS    = lua-5.1.5/liblua.a

all: version $(LIBS) $(OBJS)
	PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR) $(CC) -B $(LIBEXEC) -o xupnpd $(OBJS) $(LIBS) -Wl,-E -lm -ldl
	$(STRIP) xupnpd

lua-5.1.5/liblua.a:
	$(MAKE) -C lua-5.1.5 a CC=$(CC) PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR) MYCFLAGS="-DLUA_USE_LINUX -Os"

version:
	./ver.sh

clean:
	$(MAKE) -C lua-5.1.5 clean
	$(RM) -f $(OBJS) xupnpd.db xupnpd version.h

.c.o:
	PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR) $(CC) -c -o $@ $<

.cpp.o:
	PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR) $(CPP) -c $(CFLAGS) -o $@ $<
