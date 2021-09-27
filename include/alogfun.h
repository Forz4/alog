#ifndef _ALOGFUN_H_
#define _ALOGFUN_H_

#include "alogtypes.h"
#include "alogerror.h"
#include "alog.h"

alog_regCfg_t   *getRegByName(alog_shm_t *shm , char *regname);
alog_buffer_t   *getBufferByName( char *regname , char *cstname , char *logfilepath);
int             alog_lock();
int             alog_unlock();
void            alog_update_timer( struct timeval tv);
int             alog_addBuffer( char *regname  , char *cstname , char *logfilepath , alog_buffer_t **retbuffer);
int             alog_mkdir( char *dir );
void            *alog_update_thread(void *arg);
void            *alog_persist_thread(void *arg);
int             alog_persist( char *regname , char *cstname , char *logbasepath , alog_bufNode_t *node);
void            alog_backupLog( alog_regCfg_t *cfg , char *regname , char *cstname , char *logbasepath);
void            getFileNameFromFormat( int type  , alog_regCfg_t *cfg , char *regname , char *cstname , char *logbasepath , char filePath[ALOG_FILEPATH_LEN] );
int             get_bracket(const char *line , int no , char *value , int val_size);
alog_shm_t      *alog_loadCfg( char *filepath );
void            alog_cleanContext();
void            alog_atfork_prepare();
void            alog_atfork_after();
time_t 			alog_getFileMtime(char *filepath);
#endif

