DEBUG = -D ALOG_DEBUG_MODE
TEMPFLAG = .debug.tmp
INCPATH = ../include
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
	$(ECHO) "" > $(TEMPFLAG)

debug:
	$(ECHO) $(DEBUG) >> $(TEMPFLAG)

.c.o:
	$(CC) -c $< -I$(INCPATH) $(CFLAGS) `cat $(TEMPFLAG)`

alogcmd:	alogcmd.o alogfun.o
	$(CC) -o $@ $^ $(LIBS)
	$(MV) $@ $(BINPATH)

libalog.a: alog.o alogfun.o logpub.o
	$(AR) $@ $^ 
	$(MV) $@ $(LIBPATH)

alogtest:	alogtest.o
	$(CC) -o $@ $< -L $(LIBPATH) -lalog $(LIBS)
	$(MV) $@ $(BINPATH) 
