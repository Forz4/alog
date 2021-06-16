#include "alogtypes.h"
#include "alog.h"
#include "logpub.h"

int log_init()
{
    return alog_initContext();
}
int log_end()
{
    return alog_close();
}

void writelog_t(char *regname,\
                char *cstname,\
                char *modname,\
                char *filenm,\
                int  lineno,\
                int  nloglevel,\
                char *logfilepath,\
                char *fmt,...)
{
    char fmt1[10240] = { 0x00 };
    va_list ap;
    va_start(ap , fmt);
    vsnprintf(fmt1 , sizeof(fmt1) , fmt , ap);
    alog_writelog_t(ALOG_TYPE_ASC, nloglevel , regname , cstname , modname , filenm , lineno , logfilepath , NULL , 0  , fmt1);
    return;
}

void writelogbin_t(char *regname,\
                char *cstname,\
                char *modname,\
                char *filenm,\
                int  lineno,\
                int  nloglevel,\
                char *logfilepath,\
                char *buf,\
                int  buflen)
{
    alog_writelog_t(ALOG_TYPE_BIN, nloglevel , regname , cstname , modname , filenm , lineno , logfilepath , buf , buflen , NULL);
    return;
}

void writeloghex_t(char *regname,\
                char *cstname,\
                char *modname,\
                char *filenm,\
                int  lineno,\
                int  nloglevel,\
                char *logfilepath,\
                char *buf,\
                int  buflen)
{
    alog_writelog_t(ALOG_TYPE_HEX, nloglevel , regname , cstname , modname , filenm , lineno , logfilepath , buf , buflen , NULL);
    return;
}
