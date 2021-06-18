#ifndef _LOGPUB_H_
#define _LOGPUB_H_
#include "logpub.h"

int log_init();
int log_end();
void log_datech();
void writelog_t(char *regname,\
                char *cstname,\
                char *modname,\
                char *filenm,\
                int  lineno,\
                int  nloglevel,\
                char *logfilepath,\
                char *fmt,...);

void writelogbin_t(char *regname,\
                char *cstname,\
                char *modname,\
                char *filenm,\
                int  lineno,\
                int  nloglevel,\
                char *logfilepath,\
                char *buf,\
                int  buflen);

void writeloghex_t(char *regname,\
                char *cstname,\
                char *modname,\
                char *filenm,\
                int  lineno,\
                int  nloglevel,\
                char *logfilepath,\
                char *buf,\
                int  buflen);
#endif
