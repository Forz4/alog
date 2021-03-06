#ifndef _ALOGCLIENT_H_
#define _ALOGCLIENT_H_
#include "alogtypes.h"

/* global context pointer */
alog_context_t      *g_alog_ctx;

/* interfaces */
int alog_initContext();
void alog_close();
int alog_writelog_t ( 
        int             logtype,
        enum alog_level level,
        char            *regname,
        char            *cstname,
        char            *modname,
        char            *file,
        int             lineno,
        char            *logfilepath,
        char            *buf,
        int             len,
        char            *fmt , ...);

/* macros */
#define ALOG_FATASC(regname,cstname,modname,logfilepath,fmt,...)\
    alog_writelog_t( ALOG_TYPE_ASC ,LOGFAT , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , NULL , 0 , fmt , ##__VA_ARGS__)
#define ALOG_ERRASC(regname,cstname,modname,logfilepath,fmt,...)\
    alog_writelog_t( ALOG_TYPE_ASC ,LOGERR , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , NULL , 0 , fmt , ##__VA_ARGS__)
#define ALOG_WANASC(regname,cstname,modname,logfilepath,fmt,...)\
    alog_writelog_t( ALOG_TYPE_ASC ,LOGWAN , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , NULL , 0 , fmt , ##__VA_ARGS__)
#define ALOG_INFASC(regname,cstname,modname,logfilepath,fmt,...)\
    alog_writelog_t( ALOG_TYPE_ASC ,LOGINF , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , NULL , 0 , fmt , ##__VA_ARGS__)
#define ALOG_ADTASC(regname,cstname,modname,logfilepath,fmt,...)\
    alog_writelog_t( ALOG_TYPE_ASC ,LOGADT , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , NULL , 0 , fmt , ##__VA_ARGS__)
#define ALOG_DBGASC(regname,cstname,modname,logfilepath,fmt,...)\
    alog_writelog_t( ALOG_TYPE_ASC ,LOGDBG , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , NULL , 0 , fmt , ##__VA_ARGS__)

#define ALOG_FATHEX(regname,cstname,modname,logfilepath,buf,size)\
    alog_writelog_t( ALOG_TYPE_HEX ,LOGFAT , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , buf , size , NULL )
#define ALOG_ERRHEX(regname,cstname,modname,logfilepath,buf,size)\
    alog_writelog_t( ALOG_TYPE_HEX ,LOGERR , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , buf , size , NULL )
#define ALOG_WANHEX(regname,cstname,modname,logfilepath,buf,size)\
    alog_writelog_t( ALOG_TYPE_HEX ,LOGWAN , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , buf , size , NULL )
#define ALOG_INFHEX(regname,cstname,modname,logfilepath,buf,size)\
    alog_writelog_t( ALOG_TYPE_HEX ,LOGINF , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , buf , size , NULL )
#define ALOG_ADTHEX(regname,cstname,logfilepath,modname,buf,size)\
    alog_writelog_t( ALOG_TYPE_HEX ,LOGADT , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , buf , size , NULL )
#define ALOG_DBGHEX(regname,cstname,modname,logfilepath,buf,size)\
    alog_writelog_t( ALOG_TYPE_HEX ,LOGDBG , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , buf , size , NULL )

#define ALOG_FATBIN(regname,cstname,modname,logfilepath,buf,size)\
    alog_writelog_t( ALOG_TYPE_BIN ,LOGFAT , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , buf , size , NULL )
#define ALOG_ERRBIN(regname,cstname,modname,logfilepath,buf,size)\
    alog_writelog_t( ALOG_TYPE_BIN ,LOGERR , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , buf , size , NULL )
#define ALOG_WANBIN(regname,cstname,modname,logfilepath,buf,size)\
    alog_writelog_t( ALOG_TYPE_BIN ,LOGWAN , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , buf , size , NULL )
#define ALOG_INFBIN(regname,cstname,modname,logfilepath,buf,size)\
    alog_writelog_t( ALOG_TYPE_BIN ,LOGINF , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , buf , size , NULL )
#define ALOG_ADTBIN(regname,cstname,modname,logfilepath,buf,size)\
    alog_writelog_t( ALOG_TYPE_BIN ,LOGADT , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , buf , size , NULL )
#define ALOG_DBGBIN(regname,cstname,modname,logfilepath,buf,size)\
    alog_writelog_t( ALOG_TYPE_BIN ,LOGDBG , regname , cstname , modname , __FILE__ , __LINE__ , logfilepath , buf , size , NULL )

#endif

