#ifndef _ALOGCMD_H_
#define _ALOGCMD_H_
#include "alogtypes.h"
#include "alogerror.h"
#include "alogfun.h"

const char alog_level_string[8][7]={
    "",
    "LOGNON",
    "LOGFAT",
    "LOGERR",
    "LOGWAN",
    "LOGINF",
    "LOGADT",
    "LOGDBG"
};

void            alogcmd_help();
int             alogcmd_load(char *type);
int             alogcmd_print();
int             alogcmd_close();
int             alogcmd_setLevel( char *regname , char *level );
#endif

