#ifndef _ALOGERROR_H_
#define _ALOGERROR_H_

#ifdef  ALOG_DEBUG_MODE
#define     DEBUG                               1
#else
#define     DEBUG                               0
#endif

#define     ALOGMSG_BUF_NOTFOUND                2
#define     ALOGMSG_REG_NOTFOUND                1
#define     ALOGOK                              0
#define     ALOGERR_GETENV_FAIL                 -1
#define     ALOGERR_SHMGET_FAIL                 -2
#define     ALOGERR_SHMAT_FAIL                  -3
#define     ALOGERR_MALLOC_FAIL                 -4
#define     ALOGERR_INITMUTEX_FAIL              -5
#define     ALOGERR_INITCOND_FAIL               -6
#define     ALOGERR_MEMORY_FULL                 -7
#define     ALOGERR_REGCST_INVALID              -8
#define     ALOGERR_BUFFERNUM_OVERFLOW          -9
#define     ALOGERR_MKDIR_FAIL                  -10
#define     ALOGERR_LOADCFG_FAIL                -11

#define     ALOG_DEBUG( fmt , ... ) \
    do{ \
        if(DEBUG) \
            printf("[%-10s:%-6d][PID %-8d][TID %6lu]"fmt"\n",__FILE__,__LINE__,getpid(),(unsigned long)pthread_self()%1000000,##__VA_ARGS__ );\
    }while(0)

#endif

