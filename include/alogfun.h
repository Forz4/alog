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
void            *alog_update_thread(void *arg);
void            *alog_persist_thread(void *arg);
int             alog_persist( char *regname , char *cstname , alog_bufNode_t *node);
int             alog_addBuffer( char *regname  , char *cstname , alog_buffer_t **retbuffer);
alog_shm_t      *alog_loadCfg( char *filepath );
int             get_bracket(const char *line , int no , char *value , int val_size);
#endif

