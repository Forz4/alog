#ifndef _ALOGFUN_H_
#define _ALOGFUN_H_

#include "alogtypes.h"
#include "alogerror.h"
#include "alog.h"

alog_regCfg_t   *getRegByName(alog_shm_t *shm , char *regname);
alog_buffer_t   *getBufferByName( char *regname , char *cstname);
int             alog_lock();
int             alog_unlock();
void            alog_update_timer();
int             alog_addBuffer( char *regname  , char *cstname , alog_buffer_t **retbuffer);
void            *alog_update_thread(void *arg);
void            *alog_persist_thread(void *arg);
int             alog_persist( char *regname , char *cstname , alog_bufNode_t *node);
void            getFileNameFromFormat( int type  , alog_regCfg_t *cfg , char *regname , char *cstname , char filePath[ALOG_FILEPATH_LEN] );
int             get_bracket(const char *line , int no , char *value , int val_size);
alog_shm_t      *alog_loadCfg( char *filepath );
void            alog_cleanContext();
#endif

