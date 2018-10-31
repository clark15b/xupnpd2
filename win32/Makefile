CFLAGS  = /EHsc /I. /I.. /I..\sqlite3 /DSQLITE_THREADSAFE=1 /DSQLITE_OMIT_LOAD_EXTENSION /I..\lua-5.1.5 /nologo 
OBJS    = ..\ssdp.obj ..\http.obj ..\db_sqlite.obj ..\scan.obj ..\common.obj ..\mime.obj ..\soap.obj ..\soap_int.obj ..\charset.obj ..\compat.obj \
 ..\scripting.obj ..\live.obj ..\md5.obj ..\luajson.obj ..\sqlite3\sqlite3.obj ..\plugin_hls_common.obj ..\plugin_hls.obj ..\plugin_hls_new.obj ..\plugin_udprtp.obj ..\plugin_lua.obj ..\plugin_tsfilter.obj xupnpd.res
LIBS    = user32.lib Ws2_32.lib Shell32.lib ..\lua-5.1.5\liblua.lib

all: ..\lua-5.1.5\liblua.lib version $(OBJS)
    cl /c $(CFLAGS) /Fomain.obj main.cpp
    link /nologo /out:..\xupnpd.exe /SUBSYSTEM:WINDOWS main.obj $(OBJS) $(LIBS)

console: ..\lua-5.1.5\liblua.lib version $(OBJS)
    cl /c $(CFLAGS) /D_CONSOLE /Fomain.obj main.cpp
    link /nologo /out:..\xupnpd.exe /SUBSYSTEM:CONSOLE main.obj $(OBJS) $(LIBS)

clean: lua-clean
    del $(OBJS) main.obj xupnpd.db version.h ..\xupnpd.exe xupnpd.uid xupnpd.log

..\lua-5.1.5\liblua.lib:
    cd ..\lua-5.1.5
    nmake /F Makefile.win
    cd ..\win32

lua-clean:
    cd ..\lua-5.1.5
    nmake /F Makefile.win clean
    cd ..\win32

version:
    ver.bat

.cpp.obj:
    cl /c $(CFLAGS) /Fo$@ $<

.c.obj:
    cl /c $(CFLAGS) /Fo$@ $<

.rc.res:
    rc /fo $@ $<
