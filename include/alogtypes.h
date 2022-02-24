#ifndef _ALOGTYPES_H_
#define _ALOGTYPES_H_
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <math.h>
#include <stdarg.h>
#include "alogversion.h"

/* node status */
#define ALOG_NODE_FREE              0
#define ALOG_NODE_FULL              1
#define ALOG_NODE_USED              2
/* default config */
#define ALOG_DEF_MAXMEMORYSIZE      16
#define ALOG_DEF_SINGLEBLOCKSIZE    16
#define ALOG_DEF_FLUSHINTERVAL      1
#define ALOG_DEF_CHECKINTERVAL      500
/* log type */
#define ALOG_TYPE_ASC               1
#define ALOG_TYPE_BIN               2
#define ALOG_TYPE_HEX               3
/* macro */
#define ALOG_REGNAME_LEN            20
#define ALOG_CSTNAME_LEN            50
#define ALOG_FORMAT_LEN             7
#define ALOG_FILEPATH_LEN           300
#define ALOG_CFGBUF_LEN             100
#define ALOG_COMMAND_LEN            600
#define ALOG_REG_NUM                30
#define ALOG_BUFFER_NUM             20
#define ALOG_CURFILEFORMAT          1
#define ALOG_BAKFILEFORMAT          2

char            *ENV_ALOG_HOME;
char            *ENV_ALOG_SHMKEY; 

/* log level */
enum alog_level{
    LOGNON = 1,
    LOGFAT,
    LOGERR,
    LOGWAN,
    LOGINF,
    LOGADT,
    LOGDBG
};

/* timver */
typedef struct alog_timer{
    char            date[8+1];                                  /* date string                      */
    char            time[8+1];                                  /* time sting HH:MM:SS              */
    struct tm       tmst;                                       /* time struct                      */
    struct timeval  tv;
    int             sec;                                        /* second                           */
} alog_timer_t;

/* persist thread argument */
typedef struct alog_persist_arg{
    struct alog_context *ctx;                                   /* context pointer                  */
    char                regName[ALOG_REGNAME_LEN+1];            /* regname                          */
    char                cstName[ALOG_CSTNAME_LEN+1];            /* cstname                          */
    char                logBasePath[ALOG_FILEPATH_LEN+1];
} alog_persist_arg_t;

/* node */
typedef struct alog_bufNode{
    int                 index;                                  /* internal node index              */
    char                usedFlag;                               /*                                  */
    char                *content;                               /* message string                   */
    int                 len;                                    /* length of content                */
    int                 offset;                                 /* current offset                   */
    struct alog_bufNode *next;                                  /* next node                        */
    struct alog_bufNode *prev;                                  /* previous node                    */
} alog_bufNode_t;

/* buffer */
typedef struct alog_buffer{
    char                logBasePath[ALOG_FILEPATH_LEN+1];       /* input log file path              */
    int                 isDefaultPath;                          /* set to 1 if the base path is def */
    char                regName[ALOG_REGNAME_LEN+1];            /* regname                          */
    char                cstName[ALOG_CSTNAME_LEN+1];            /* cstname                          */
    pthread_t           consTid;                                /* consumer thread id               */
    int                 nodeNum;                                /* number of ndoes                  */
    alog_persist_arg_t  arg;                                    /* argument passwd to thread        */
    struct alog_bufNode *prodPtr;                               /* productor pointer                */
    struct alog_bufNode *consPtr;                               /* consumer pointer                 */
}alog_buffer_t;

/* regname config */
typedef struct alog_regCfg{
    char                regName[ALOG_REGNAME_LEN+1];            /* regname                          */
    short               level;                                  /* log level                        */
    int                 maxSize;                                /* file size limit                  */
    char                format[ALOG_FORMAT_LEN+1];              /* log prefix pattern               */
    char                defLogBasePath_r[ALOG_FILEPATH_LEN+1];  /* default raw log base path        */
    char                defLogBasePath[ALOG_FILEPATH_LEN+1];    /* default log base path            */
    char                curFileNamePattern_r[ALOG_FILEPATH_LEN+1];
    char                curFileNamePattern[ALOG_FILEPATH_LEN+1];/* current file name pattern        */
    char                bakFileNamePattern_r[ALOG_FILEPATH_LEN+1];
    char                bakFileNamePattern[ALOG_FILEPATH_LEN+1];/* backup file name pattern         */
    int                 backupAfterQuit;                        /*                                  */
} alog_regCfg_t;

/* 线程上下文 */
typedef struct alog_context{
    pthread_mutex_t     mutex;                                  /* mutex to protect buffer          */
    pthread_cond_t      cond_persist;                           /* cond to wake up consumer         */
    struct alog_shm     *l_shm;                                 /* local shm pointer                */
    struct alog_shm     *g_shm;                                 /* global share memory pointer      */
    int                 bufferNum;                              /* number of buffers                */
    struct alog_buffer  buffers[ALOG_BUFFER_NUM];               /* buffers                          */
    struct alog_timer   timer;                                  /* timer                            */
    int                 closeFlag;                              /* close flag                       */
    pthread_t           updTid;                                 /* thread id of update thread       */
}alog_context_t;

/* context */
typedef struct alog_shm{
    key_t               shmKey;                                 /* share memory key                 */
    int                 shmId;                                  /* share memory id                  */
    int                 maxMemorySize;                          /* max mermory use(MB)              */
    int                 singleBlockSize;                        /* single Block size(KB)            */
    int                 flushInterval;                          /* flush interval for persist thread*/
    int                 checkInterval;                          /* check interval for update thread */
    time_t              updTime;                                /* update timestamp                 */
    int                 regNum;                                 /* number of register configs       */
    struct alog_regCfg  regCfgs[ALOG_REG_NUM];                  /* register configs                 */
} alog_shm_t;

#endif

