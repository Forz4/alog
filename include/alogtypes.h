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

/* 节点状态 */
#define ALOG_NODE_FREE              0
#define ALOG_NODE_FULL              1
#define ALOG_NODE_USED              2
/* 默认配置 */
#define ALOG_DEF_MAXMEMORYSIZE      4
#define ALOG_DEF_SINGLEBLOCKSIZE    4
#define ALOG_DEF_FLUSHINTERVAL      2
#define ALOG_DEF_CHECKINTERVAL      5
/* 日志打印类型 */
#define ALOG_TYPE_ASC               1
#define ALOG_TYPE_BIN               2
#define ALOG_TYPE_HEX               3
/* 长度宏定义 */
#define ALOG_REGNAME_LEN            20
#define ALOG_CSTNAME_LEN            20
#define ALOG_FORMAT_LEN             7
#define ALOG_FILEPATH_LEN           300
#define ALOG_CFGBUF_LEN             100
#define ALOG_COMMAND_LEN            600
#define ALOG_REG_NUM                20
#define ALOG_BUFFER_NUM             20

/* 日志类型定义 */
enum alog_level{
    LOGNON = 0,
    LOGFAT,
    LOGERR,
    LOGWAN,
    LOGINF,
    LOGADT,
    LOGDBG
};

/* 日期时间字符串缓存 */
typedef struct alog_timer{
    char            date[8+1];                                  /* 日期字符串                       */
    char            time[8+1];                                  /* 时间字符串 HH:MM:SS              */
    struct tm       tmst;                                       /* 时间结构体                       */
    struct timeval  tv;
    int             sec;                                        /* 实际秒数                         */
} alog_timer_t;

/* 持久化线程参数结构体 */
typedef struct alog_persist_arg{
    struct alog_context *ctx;                                   /* 上下文指针                       */
    char                regName[ALOG_REGNAME_LEN+1];            /* 注册名                           */
    char                cstName[ALOG_CSTNAME_LEN+1];            /* 识别名                           */
} alog_persist_arg_t;

/* 缓存日志块节点 */
typedef struct alog_bufNode{
    int                 index;                                  /* 内存快序号                       */
    char                usedFlag;                               /* 使用标志                         */
    char                *content;                               /* 日志存放大数组                   */
    int                 offset;                                 /* 当前偏移量                       */
    struct alog_bufNode *next;                                  /* 指向下一个节点                   */
    struct alog_bufNode *prev;                                  /* 指向上一个节点                   */
} alog_bufNode_t;

/* 本地日志缓冲区 */
typedef struct alog_buffer{
    char                regName[ALOG_REGNAME_LEN+1];            /* 注册名                           */
    char                cstName[ALOG_CSTNAME_LEN+1];            /* 识别名                           */
    pthread_t           consTid;                                /* 消费者线程号                     */
    int                 nodeNum;                                /* 日志块个数                       */
    struct alog_bufNode *prodPtr;                               /* 生产者指针                       */
    struct alog_bufNode *consPtr;                               /* 消费者指针                       */
}alog_buffer_t;

/* 注册名配置项结构 */
typedef struct alog_regCfg{
    char                regName[ALOG_REGNAME_LEN+1];            /* 注册名                           */
    short               level;                                  /* 日志级别                         */
    int                 maxSize;                                /* 单个文件大小                     */
    char                format[ALOG_FORMAT_LEN+1];                            /* 打印格式                         */
    char                filePath[ALOG_FILEPATH_LEN+1];          /* 日志文件路径                     */
} alog_regCfg_t;

/* 线程上下文 */
typedef struct alog_context{
    pthread_mutex_t     mutex;                                  /* 互斥信号量保护缓冲区读写         */
    pthread_cond_t      cond_persist;                           /* 条件变量用于唤醒持久化线程       */
    struct alog_shm     *l_shm;                                 /* 本地配置缓存                     */
    struct alog_shm     *g_shm;                                 /* 共享内存配置                     */
    int                 bufferNum;                              /* 缓冲区个数                       */
    struct alog_buffer  buffers[ALOG_BUFFER_NUM];               /* 缓冲区组                         */
    struct alog_timer   timer;                                  /* 日期和时间缓冲区                 */
    int                 closeFlag;                              /* 结束标志                         */
    pthread_t           updTid;
}alog_context_t;

/* 共享内存结构 */
typedef struct alog_shm{
    key_t               shmKey;                                 /* 共享内存key值                    */
    int                 shmId;                                  /* 共享内存id值                     */
    int                 maxMemorySize;                          /* 缓冲区最大使用内存(MB)           */
    int                 singleBlockSize;                        /* 单个缓存日志块数组大小(KB)       */
    int                 flushInterval;                          /* 持久化线程写日志间隔             */
    int                 checkInterval;                          /* 配置更新检查间隔                 */
    time_t              updTime;                                /* 上次更新时间                     */
    int                 regNum;                                 /* 配置项个数                       */
    struct alog_regCfg  regCfgs[ALOG_REG_NUM];                  /* 配置项数组                       */
} alog_shm_t;

#endif

