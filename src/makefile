all:	alogcmd libalog.a alogtest

clean:
	@-rm *.o
#DEBUG = -D ALOG_DEBUG_MODE

INCPATH = -I ../include
LIBS = -lpthread
CFLAGS = -Wall -std=gnu99

alogcmd:	alogcmd.o alogfun.o
	@gcc -o alogcmd alogcmd.o alogfun.o $(LIBS)
	@mv alogcmd ../bin/

alogcmd.o:	alogcmd.c
	@gcc -c alogcmd.c $(INCPATH) $(DEBUG) $(CFLAGS)

alogfun.o:	alogfun.c
	@gcc -c alogfun.c $(INCPATH) $(DEBUG) $(CFLAGS)

alog.o:	alog.c
	@gcc -c alog.c $(INCPATH) $(DEBUG) $(CFLAGS)

libalog.a: alog.o alogfun.o alogcmd.o 
	@ar -ruc libalog.a alog.o alogfun.o alogcmd.o
	@mv libalog.a ../lib/

alogtest:	alogtest.c
	@gcc -o alogtest alogtest.c $(INCPATH) -lalog -L ../lib $(DEBUG) $(LIBS) $(CFLAGS)
	@mv alogtest ../bin/
