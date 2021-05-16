#include "alog.h"
#include "alogfun.h"
#include "alogtypes.h"
/* 
 *  日志模块初始化
 * */
int alog_initContext()
{
    char            *ENV_SHMKEY = NULL;
    key_t           shmkey;
    int             shmid = 0;
    alog_shm_t      *g_shm = NULL;

    /* 加载环境变量 */
    ENV_SHMKEY = getenv("ALOG_SHMKEY");
    if ( ( ENV_SHMKEY = getenv("ALOG_SHMKEY") ) == NULL ){
        ALOG_DEBUG("获取环境变量[ALOG_SHMKEY]失败\n");
        return ALOGERR_GETENV_FAIL;
    }

    /* 挂载共享内存 */
    shmkey = atoi(ENV_SHMKEY);
    if ( ( shmid = shmget( shmkey , sizeof(alog_shm_t) , 0) ) < 0 ){
        ALOG_DEBUG("shmget 失败 , shmkey[%s] , shmid[%d] , errmsg[%s]!\n" , ENV_SHMKEY , shmid  , strerror(errno));
        return ALOGERR_SHMGET_FAIL;
    } 
    if ( (g_shm = (alog_shm_t *)shmat( shmid , NULL , 0 ))  == NULL ){
        ALOG_DEBUG("shmat 失败 , shmkey[%s] , shmid[%d]! , errmsg[%s]\n" , ENV_SHMKEY , shmid  , strerror(errno));
        return ALOGERR_SHMAT_FAIL;
    }
    ALOG_DEBUG("alog_initContext 共享内存挂载完成");
    
    /* 分配上下文 */
    alog_context_t  *ctx = (alog_context_t *)malloc(sizeof(alog_context_t));
    if ( ctx == NULL ) {
        ALOG_DEBUG("malloc [alog_context_t] 失败\n");
        shmdt(g_shm);
        return ALOGERR_MALLOC_FAIL;
    }
    ALOG_DEBUG("alog_initContext 上下文分配完成");

    g_alog_ctx = ctx;

    /* 初始化mutex */
    if ( pthread_mutex_init(&(ctx->mutex), NULL) )
    {
        ALOG_DEBUG("初始化mutex失败\n");
        shmdt(g_shm);
        return ALOGERR_INITMUTEX_FAIL;
    }
    ALOG_DEBUG("alog_initContext 初始化mutex完成");

    /* 初始化条件变量 */
    if ( pthread_cond_init(&(ctx->cond_persist), NULL))
    {
        ALOG_DEBUG("初始化条件变量失败\n");
        shmdt(g_shm);
        return ALOGERR_INITCOND_FAIL;
    }
    ALOG_DEBUG("alog_initContext 初始化cond完成");

    /* 分配本地配置缓存区 */
    if ( (ctx->l_shm = (alog_shm_t *)malloc(sizeof(alog_shm_t))) == NULL ){
        ALOG_DEBUG("malloc [alog_shm_t] 失败\n");
        shmdt(g_shm);
        return ALOGERR_MALLOC_FAIL;
    }
    ALOG_DEBUG("alog_initContext 初始化本地缓存区完成");

    /* 初始化上下文变量 */
    ctx->g_shm = g_shm;
    ctx->bufferNum = 0;
    memset(ctx->buffers , 0x00 , sizeof(ctx->buffers));
    ctx->closeFlag = 0;
    ALOG_DEBUG("alog_initContext 初始化上下文变量完成");
    
    alog_update_timer( );
    ALOG_DEBUG("alog_initContext 初始化timer完成date[%s],time[%s]",g_alog_ctx->timer.date,g_alog_ctx->timer.time);

    /* 同步本地配置 */
    memcpy( g_alog_ctx->l_shm , g_shm , sizeof(alog_shm_t) );

    /* 启动定时更新线程 */
    pthread_create(&(g_alog_ctx->updTid), NULL, alog_update_thread, NULL );

    ALOG_DEBUG("alog_initContext 执行完成");
    return 0;
}
/*
 *   日志模块关闭
 * */
int alog_close()
{
    /* 设置结束标志 */
    alog_lock();
    g_alog_ctx->closeFlag = 1;
    alog_unlock();

    /* 通知所有持久化线程冲刷磁盘 */
    pthread_cond_broadcast(&(g_alog_ctx->cond_persist));
    
    /* 等待持久化线程退出 */
    int i = 0;
    for ( i = 0 ; i < g_alog_ctx->bufferNum ; i ++ ){
        pthread_join(g_alog_ctx->buffers[i].consTid, NULL);
    }
    pthread_join(g_alog_ctx->updTid, NULL);

    ALOG_DEBUG("持久化线程全部退出完毕");

    /* 断开共享内存连接 */
    shmdt(g_alog_ctx->g_shm);

    /* 释放分配的资源 */
    pthread_mutex_destroy(&(g_alog_ctx->mutex));
    pthread_cond_destroy(&(g_alog_ctx->cond_persist));
    free(g_alog_ctx->l_shm);
    free(g_alog_ctx);
    
    return 0;
}
/*
 *  日志打印基础接口
 * */
int alog_writelog_t ( 
        int             logtype,
        enum alog_level level,
        char            *regname,
        char            *cstname,
        char            *modname,
        char            *file,
        int             lineno,
        char            *buf,
        int             len,
        char            *fmt , ...)
{
    int             i = 0 ;
    int             j = 0 ;
    unsigned char   ch = ' ';
    int             ret = 0;
    struct          timeval  tv;

    /* 获取注册名配置 */
    alog_regCfg_t   *regCfg = NULL;
    if ( (regCfg = getRegByName( g_alog_ctx->l_shm , regname ) ) == NULL )
        return ALOGMSG_REG_NOTFOUND;
    /* 判断日志级别 */
    if ( level > regCfg->level )
        return ALOGOK;

    gettimeofday( &tv , NULL );
    alog_lock();
    /* 获取本地缓冲区，若缓冲区不存在则新增缓冲区 */
    alog_buffer_t   *buffer = NULL;
    if( (buffer = getBufferByName( regname , cstname )) == NULL ){
        ret = alog_addBuffer( regname  , cstname , &buffer);
        if ( ret ){
            alog_unlock();
            return ret;
        }
    }
    /* 根据配置文件组装日志内容 */
    int         offset = 0;
    int         max = g_alog_ctx->l_shm->singleBlockSize * 1024;
    char        *temp = (char *)malloc(max);
    int         leftsize = 0 ;
    memset( temp , 0x00 , max );

    /* 日期 */
    if ( regCfg->format[0] == '1' ){
        if ( tv.tv_sec % 86400 != g_alog_ctx->timer.sec % 86400 ){
            /* 当前日期跟缓存timer的日期不同，则更新本地timer缓存 */
            alog_update_timer( );
        }
        offset += sprintf( temp+offset , "[%8s]" , g_alog_ctx->timer.date  );
    }
    /* 时间 */
    if ( regCfg->format[1] == '1' ){
        if ( tv.tv_sec != g_alog_ctx->timer.sec ){
            /* 当前时间跟缓存timer不在同一秒，则更新缓存 */
            alog_update_timer( );
        }
        offset += sprintf( temp+offset ,  "[%8s]" , g_alog_ctx->timer.time );
    }
    /* 微秒 */
    if ( regCfg->format[2] == '1' ){
        offset += sprintf( temp+offset ,"[%06d]" , tv.tv_usec );
    }
    /* 进程号+线程号 */
    if ( regCfg->format[3] == '1' ){
        offset += sprintf( temp+offset ,"[PID:%-10d]" , getpid() );
        offset += sprintf( temp+offset ,"[TID:%-10ld]" , (long)pthread_self() );
    }
    /* 模块名 */
    if ( regCfg->format[4] == '1' ){
        offset += sprintf( temp+offset , "[%s]" , modname );
    }
    /* 日志级别 */
    if ( regCfg->format[5] == '1' ){
        char levelstr[7];
        switch( level ){
            case LOGNON:
                strcpy(levelstr , "LOGNON");
                break;
            case LOGFAT:
                strcpy(levelstr , "LOGNON");
                break;
            case LOGERR:
                strcpy(levelstr , "LOGERR");
                break;
            case LOGWAN:
                strcpy(levelstr , "LOGWAN");
                break;
            case LOGINF:
                strcpy(levelstr , "LOGINF");
                break;
            case LOGADT:
                strcpy(levelstr , "LOGADT");
                break;
            case LOGDBG:
                strcpy(levelstr , "LOGDBG");
                break;
        }
        offset += sprintf( temp+offset , "[%s]" , levelstr);
    }
    /* 文件名+行数 */
    if ( regCfg->format[6] == '1' ){
        offset += sprintf( temp+offset , "[%s:%d]" , file , lineno);
    }

    va_list ap;
    va_start( ap , fmt );

    /* 根据日志类型拼接日志内容 */
    switch ( logtype ){
        case ALOG_TYPE_ASC:
            offset += vsnprintf( temp+offset , max - offset , fmt , ap );
            offset += snprintf( temp+offset , max - offset , "\n");
            va_end(ap);
            break;
        case ALOG_TYPE_HEX:
            offset += snprintf( temp+offset , max - offset , "打印十六进制报文:\n");
offset += snprintf( temp+offset , max - offset , "--------------------------------Hex Message begin------------------------------\n");
            for ( i = 0 ; i <= len/16 ; i ++){
                offset += snprintf( temp+offset , max - offset , "M(%06X)=< " , i);
                for ( j = 0 ; j+i*16 < len && j < 16 ;j ++){
                    ch = (unsigned char)buf[j+i*16];
                    offset += snprintf( temp+offset , max - offset , "%02X " , ch);
                }
                while ( j++ < 16 ){
                    offset += snprintf( temp+offset , max - offset , "   ");
                }
                offset += snprintf( temp+offset , max - offset , " > ");
                for ( j = 0 ; j+i*16 < len && j < 16 ; j ++ ){
                    int pos = j+i*16;
                    if ( (buf[pos] >= 32 && buf[pos] <= 126) ){
                        offset += snprintf( temp+offset , max - offset , "%c" , buf[pos]);
                    } else {
                        offset += snprintf( temp+offset , max - offset , ".");
                    }
                }
                offset += snprintf( temp+offset , max - offset , "\n");
            }
offset += snprintf( temp+offset , max - offset , "--------------------------------Hex Message end--------------------------------\n");
            break;
        case ALOG_TYPE_BIN:
        default:
            leftsize = max - 1 - offset;
            if ( len > leftsize ){
                memcpy( temp + offset , buf , leftsize );
                offset = max;
            } else {
                memcpy( temp + offset , buf , len );
                offset += len;
            }
            temp[max-1] = '\n';
            break;
    }

    /* 计算当前node是否有足够空间存放日志 */
    alog_bufNode_t  *node = buffer->prodPtr;

    /*if (node->usedFlag == ALOG_NODE_FULL || node->offset + offset > g_alog_ctx->l_shm->singleBlockSize*1024){ */
    if (node->usedFlag == ALOG_NODE_FULL || node->offset + offset > node->len ){ 

        ALOG_DEBUG("当前节点[%d]空间不足,修改节点状态为FULL",node->index);
        node->usedFlag = ALOG_NODE_FULL;

        /* 如果下一个节点依然满，则尝试增加新节点 */
        if ( node->next->usedFlag !=  ALOG_NODE_FREE ){
            ALOG_DEBUG("下一个节点[%d]空间也满",node->next->index);
            /* 判断是否超过总内存限制 */
            if ( g_alog_ctx->l_shm->singleBlockSize * (buffer->nodeNum+1) > g_alog_ctx->l_shm->maxMemorySize * 1024){
                ALOG_DEBUG("内存已占满，无法新增节点");
                alog_unlock();
                free(temp);
                return ALOGERR_MEMORY_FULL;
            } else {
                ALOG_DEBUG("开始新增节点");
                /* 新增一个节点 */
                alog_bufNode_t *tempnode = (alog_bufNode_t *)malloc(sizeof(alog_bufNode_t));
                if ( tempnode == NULL){
                    alog_unlock();
                    free(temp);
                    return ALOGERR_MALLOC_FAIL;
                }
                /* 插入节点 */
                tempnode->content = (char *)malloc(g_alog_ctx->l_shm->singleBlockSize*1024);
                if ( tempnode->content == NULL ){
                    free(tempnode);
                    alog_unlock();
                    free(temp);
                    return ALOGERR_MALLOC_FAIL;
                }
                tempnode->next = node->next;
                tempnode->prev = node;
                tempnode->offset = 0;
                tempnode->len = g_alog_ctx->l_shm->singleBlockSize*1024;
                tempnode->usedFlag = ALOG_NODE_FREE;

                node->next->prev = tempnode;
                node->next = tempnode;

                buffer->nodeNum ++;
                tempnode->index = buffer->nodeNum;

                ALOG_DEBUG("新增节点完成,前序节点[%d],新增节点[%d],后序节点[%d]",node->index,node->next->index,node->next->next->index);
            }
        } 
        /* 切换到下一个可用节点 */
        node = node->next;
        buffer->prodPtr = node;
        ALOG_DEBUG("切换到节点[%d]",node->index);
        /* 唤醒持久化线程工作 */
        pthread_cond_signal(&(g_alog_ctx->cond_persist));
    } 

    ALOG_DEBUG("日志准备写入节点[%d],当前偏移量[%d]",node->index,node->offset);
    memcpy( node->content+node->offset , temp , offset );
    node->offset += offset;
    node->usedFlag = ALOG_NODE_USED;
    ALOG_DEBUG("修改节点[%d]状态为USED",node->index);

    alog_unlock();
    free(temp);
    return ALOGOK;
}

