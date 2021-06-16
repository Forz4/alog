DEBUG = -D ALOG_DEBUG_MODE
INCPATH = -I ../include
LIBPATH = ../lib
BINPATH = ../bin
LIBS = -lpthread
CFLAGS = -Wall -std=gnu99 -fPIC -g
CC = @gcc
MV = @mv
AR = @ar -ruc
RM = @-rm -f
ECHO = @echo

all:  libalog.a alogtest alogcmd

clean:
	$(RM) *.o
	$(RM) $(BINPATH)/alogtest $(BINPATH)/alogcmd $(LIBPATH)/libalog.a
	$(ECHO) "" > .debug.tmp

debug:
	$(ECHO) $(DEBUG) > .debug.tmp

.c.o:
	$(CC) -c $< $(INCPATH) $(CFLAGS) `cat .debug.tmp`

alogcmd:	alogcmd.o alogfun.o
	$(CC) -o $@ $^ $(LIBS)
	$(MV) $@ $(BINPATH)

libalog.a: alog.o alogfun.o alogcmd.o logpub.o
	$(AR) $@ $^ 
	$(MV) $@ $(LIBPATH)

alogtest:	alogtest.o
	$(CC) -o $@ $< -L $(LIBPATH) -lalog $(LIBS)
	$(MV) $@ $(BINPATH) 
