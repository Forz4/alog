#include "alog.h"
#include "alogfun.h"
#include "alogtypes.h"
/*
 * Initialize Context
 * */
int alog_initContext()
{
    /* clean up previous context , especially in fork situation */
    alog_cleanContext();

    char            *ENV_SHMKEY = NULL;
    key_t           shmkey;
    int             shmid = 0;
    alog_shm_t      *g_shm = NULL;

    /* get shmkey from environment */
    ENV_SHMKEY = getenv("ALOG_SHMKEY");
    if ( ( ENV_SHMKEY = getenv("ALOG_SHMKEY") ) == NULL ){
        ALOG_DEBUG("fail to get environment variable [ALOG_SHMKEY]\n");
        return ALOGERR_GETENV_FAIL;
    }

    shmkey = atoi(ENV_SHMKEY);
    if ( ( shmid = shmget( shmkey , sizeof(alog_shm_t) , 0) ) < 0 ){
        ALOG_DEBUG("shmget fail , shmkey[%s] , shmid[%d] , errmsg[%s]!\n" , ENV_SHMKEY , shmid  , strerror(errno));
        return ALOGERR_SHMGET_FAIL;
    } 
    if ( (g_shm = (alog_shm_t *)shmat( shmid , NULL , 0 ))  == NULL ){
        ALOG_DEBUG("shmat fail , shmkey[%s] , shmid[%d]! , errmsg[%s]\n" , ENV_SHMKEY , shmid  , strerror(errno));
        return ALOGERR_SHMAT_FAIL;
    }
    
    /* alloc space for context */
    alog_context_t  *ctx = (alog_context_t *)malloc(sizeof(alog_context_t));
    if ( ctx == NULL ) {
        ALOG_DEBUG("fail to malloc [alog_context_t]\n");
        shmdt(g_shm);
        return ALOGERR_MALLOC_FAIL;
    }

    /* set g_alog_ctx */
    g_alog_ctx = ctx;

    /* initialize mutex */
    if ( pthread_mutex_init(&(ctx->mutex), NULL) )
    {
        ALOG_DEBUG("fail to initialize mutex\n");
        shmdt(g_shm);
        return ALOGERR_INITMUTEX_FAIL;
    }

    /* initialize cond */
    if ( pthread_cond_init(&(ctx->cond_persist), NULL))
    {
        shmdt(g_shm);
        return ALOGERR_INITCOND_FAIL;
    }

    /* initialize space for share memory struct */
    if ( (ctx->l_shm = (alog_shm_t *)malloc(sizeof(alog_shm_t))) == NULL ){
        shmdt(g_shm);
        return ALOGERR_MALLOC_FAIL;
    }

    ctx->g_shm = g_shm;
    ctx->bufferNum = 0;
    memset(ctx->buffers , 0x00 , sizeof(ctx->buffers));
    ctx->closeFlag = 0;
    alog_update_timer();
    memcpy( g_alog_ctx->l_shm , g_shm , sizeof(alog_shm_t) );

    /* create update thread */
    pthread_create(&(g_alog_ctx->updTid), NULL, alog_update_thread, NULL );

    return 0;
}
/*
 *  Clena Up
 * */
int alog_close()
{
    /* set close flag */
    alog_lock();
    g_alog_ctx->closeFlag = 1;
    alog_unlock();

    /* signal all threads */
    pthread_cond_broadcast(&(g_alog_ctx->cond_persist));
    
    /* join all threadsa */
    int i = 0;
    for ( i = 0 ; i < g_alog_ctx->bufferNum ; i ++ ){
        pthread_join(g_alog_ctx->buffers[i].consTid, NULL);
    }
    pthread_join(g_alog_ctx->updTid, NULL);

    ALOG_DEBUG("all threads joined");

    /* detach share memory */
    shmdt(g_alog_ctx->g_shm);

    /* clean up resources */
    pthread_mutex_destroy(&(g_alog_ctx->mutex));
    pthread_cond_destroy(&(g_alog_ctx->cond_persist));
    free(g_alog_ctx->l_shm);
    free(g_alog_ctx);
    
    return 0;
}
/*
 *  main interface for logging
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

    /* get config for current regname in local memory */
    alog_regCfg_t   *regCfg = NULL;
    if ( (regCfg = getRegByName( g_alog_ctx->l_shm , regname ) ) == NULL )
        return ALOGMSG_REG_NOTFOUND;

    /* judge log level */
    if ( level > regCfg->level )
        return ALOGOK;

    /* get current time */
    gettimeofday( &tv , NULL );

    /* lock mutext */
    alog_lock();

    /* get buffer for current regname+cstname , if not exist then create one */
    alog_buffer_t   *buffer = NULL;
    if( (buffer = getBufferByName( regname , cstname )) == NULL ){
        ret = alog_addBuffer( regname  , cstname , &buffer);
        if ( ret ){
            alog_unlock();
            return ret;
        }
    }

    /* packing log message based on config */
    int         offset = 0;
    int         max = g_alog_ctx->l_shm->singleBlockSize * 1024;
    char        *temp = (char *)malloc(max);
    int         leftsize = 0 ;
    memset( temp , 0x00 , max );

    /* date */
    if ( regCfg->format[0] == '1' ){
        if ( tv.tv_sec % 86400 != g_alog_ctx->timer.sec % 86400 ){
            /* update timer if now is a new day */
            alog_update_timer( );
        }
        offset += sprintf( temp+offset , "[%8s]" , g_alog_ctx->timer.date  );
    }
    /* time */
    if ( regCfg->format[1] == '1' ){
        if ( tv.tv_sec != g_alog_ctx->timer.sec ){
            /* update timer if now is a new second */
            alog_update_timer( );
        }
        offset += sprintf( temp+offset ,  "[%8s]" , g_alog_ctx->timer.time );
    }
    /* micro second */
    if ( regCfg->format[2] == '1' ){
        offset += sprintf( temp+offset ,"[%06d]" , tv.tv_usec );
    }
    /* pid+tid */
    if ( regCfg->format[3] == '1' ){
        offset += sprintf( temp+offset ,"[PID:%-8d]" , getpid() );
        offset += sprintf( temp+offset ,"[TID:%-6ld]" , (long)pthread_self()%1000000 );
    }
    /* modname */
    if ( regCfg->format[4] == '1' ){
        offset += sprintf( temp+offset , "[%s]" , modname );
    }
    /* log level */
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
    /* filaname+lineno */
    if ( regCfg->format[6] == '1' ){
        offset += sprintf( temp+offset , "[%s:%d]" , file , lineno);
    }

    va_list ap;
    va_start( ap , fmt );

    /* apppend read log message based on logtype */
    switch ( logtype ){
        case ALOG_TYPE_ASC:
            offset += vsnprintf( temp+offset , max - offset , fmt , ap );
            offset += snprintf( temp+offset , max - offset , "\n");
            va_end(ap);
            break;
        case ALOG_TYPE_HEX:
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

    /* get productor pointer for current buffer */
    alog_bufNode_t  *node = buffer->prodPtr;

    /* judge if there are enough space for current message */
    if (node->usedFlag == ALOG_NODE_FULL || node->offset + offset > node->len ){ 

        ALOG_DEBUG("not enough space in node [%d] , modify status to FULL , check next node",node->index);
        node->usedFlag = ALOG_NODE_FULL;

        /* if the next node is not FREE , then try to add a new node */
        if ( node->next->usedFlag !=  ALOG_NODE_FREE ){
            ALOG_DEBUG("the next node [%d] is not FREE",node->next->index);

            /* judge if total memory use exceeds limit */
            if ( g_alog_ctx->l_shm->singleBlockSize * (buffer->nodeNum+1) > g_alog_ctx->l_shm->maxMemorySize * 1024){
                ALOG_DEBUG("memory use over limit , can not create new nodes");
                alog_unlock();
                free(temp);
                return ALOGERR_MEMORY_FULL;
            } else {
                ALOG_DEBUG("start to add new node");
                /* add a new node */
                alog_bufNode_t *tempnode = (alog_bufNode_t *)malloc(sizeof(alog_bufNode_t));
                if ( tempnode == NULL){
                    alog_unlock();
                    free(temp);
                    return ALOGERR_MALLOC_FAIL;
                }
                /* append new node to chain */
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

                ALOG_DEBUG("new node added ,prev node[%d],current node[%d],next node[%d]",node->index,node->next->index,node->next->next->index);
            }
        } 
        /* switch to next valid node */
        node = node->next;
        buffer->prodPtr = node;
        ALOG_DEBUG("switch to node[%d]",node->index);

        /* signal persist thread to work */
        pthread_cond_signal(&(g_alog_ctx->cond_persist));
    } 

    ALOG_DEBUG("ready to write message to node[%d],current offset[%d]",node->index,node->offset);
    memcpy( node->content+node->offset , temp , offset );
    node->offset += offset;
    node->usedFlag = ALOG_NODE_USED;
    ALOG_DEBUG("modify node[%d] status to USED",node->index);

    alog_unlock();
    free(temp);
    return ALOGOK;
}

