all:  alogcmd libalog.a alogtest

clean:
	@-rm *.o
	@> .debug.tmp

debug:
	@echo $(DEBUG) > .debug.tmp

DEBUG = -D ALOG_DEBUG_MODE
INCPATH = -I ../include
LIBS = -lpthread
CFLAGS = -Wall -std=gnu99

alogcmd:	alogcmd.o alogfun.o
	@gcc -o alogcmd alogcmd.o alogfun.o $(LIBS)
	@mv alogcmd ../bin/

alogcmd.o:	alogcmd.c
	@gcc -c alogcmd.c $(INCPATH) $(CFLAGS) `cat .debug.tmp`

alogfun.o:	alogfun.c
	@gcc -c alogfun.c $(INCPATH) $(CFLAGS) `cat .debug.tmp`

alog.o:	alog.c
	@gcc -c alog.c $(INCPATH) $(CFLAGS) `cat .debug.tmp`

libalog.a: alog.o alogfun.o alogcmd.o 
	@ar -ruc libalog.a alog.o alogfun.o alogcmd.o
	@mv libalog.a ../lib/

alogtest:	alogtest.c
	@gcc -o alogtest alogtest.c $(INCPATH) -lalog -L ../lib $(LIBS) $(CFLAGS)  `cat .debug.tmp`
	@mv alogtest ../bin/
