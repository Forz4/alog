#ifndef _LOGPUB_H_
#define _LOGPUB_H_

int log_init();
int log_end();
void log_detach();
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
