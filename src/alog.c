#include "alog.h"
#include "alogfun.h"
#include "alogtypes.h"
char *ALOG_TRACEID;
/**
 * [alog_initContext Initialize context]
 * @return [0 for success]
 */
int alog_initContext()
{
    /**
     * clean up previous context , especially in fork situation
     */
    alog_cleanContext();

    key_t           shmkey;
    int             shmid = 0;
    alog_shm_t      *g_shm = NULL;

    /**
     * get and load share memory
     */
    ENV_ALOG_SHMKEY = getenv("ALOG_SHMKEY");
    if ( ENV_ALOG_SHMKEY != NULL ){
        shmkey = atoi(ENV_ALOG_SHMKEY);
        if ( ( shmid = shmget( shmkey , sizeof(alog_shm_t) , 0) ) < 0 ){
            ALOG_DEBUG("shmget fail , shmkey[%s] , shmid[%d] , errmsg[%s]!" , ENV_ALOG_SHMKEY , shmid  , strerror(errno));
            return ALOGERR_SHMGET_FAIL;
        } 
        if ( (g_shm = (alog_shm_t *)shmat( shmid , NULL , 0 ))  == NULL ){
            ALOG_DEBUG("shmat fail , shmkey[%s] , shmid[%d]! , errmsg[%s]" , ENV_ALOG_SHMKEY , shmid  , strerror(errno));
            return ALOGERR_SHMAT_FAIL;
        }
    } else {
        ALOG_DEBUG("in noshm mode");
    }
    
    /**
     * allocate space for context
     */
    alog_context_t  *ctx = (alog_context_t *)malloc(sizeof(alog_context_t));
    if ( ctx == NULL ) {
        ALOG_DEBUG("fail to malloc [alog_context_t]");
        shmdt(g_shm);
        return ALOGERR_MALLOC_FAIL;
    }

    /**
     * set g_alog_ctx
     */
    g_alog_ctx = ctx;

    /**
     * initialize mutex
     */
    if ( pthread_mutex_init(&(ctx->mutex), NULL) )
    {
        ALOG_DEBUG("fail to initialize mutex");
        shmdt(g_shm);
        return ALOGERR_INITMUTEX_FAIL;
    }

    /**
     * initialize cond
     */
    if ( pthread_cond_init(&(ctx->cond_persist), NULL))
    {
        shmdt(g_shm);
        return ALOGERR_INITCOND_FAIL;
    }

    /**
     * initialize space for share memory struct
     */
    if ( (ctx->l_shm = (alog_shm_t *)malloc(sizeof(alog_shm_t))) == NULL ){
        shmdt(g_shm);
        return ALOGERR_MALLOC_FAIL;
    }

    /**
     * copy share memory to local memory
     */
    if ( ENV_ALOG_SHMKEY != NULL ){
        ctx->g_shm = g_shm;
        memcpy( g_alog_ctx->l_shm , g_shm , sizeof(alog_shm_t) );
    } else {
        /**
         * load config from file
         */
        char filepath[ALOG_FILEPATH_LEN];
        ENV_ALOG_HOME = getenv("ALOG_HOME");
        if ( ENV_ALOG_HOME != NULL ){
            sprintf( filepath , "%s/cfg/alog.cfg" , ENV_ALOG_HOME);
        } else {
            sprintf( filepath , "%s/alog/cfg/alog.cfg" , getenv("HOME"));
        }
        g_alog_ctx->l_shm = alog_loadCfg( filepath );
        if ( g_alog_ctx->l_shm == NULL ){
            printf("fail to load config from %s\n" , filepath);
            return -1;
        }
        /**
         * get config file mtime
         */
        g_alog_ctx->l_shm->updTime = alog_getFileMtime(filepath);
        if ( g_alog_ctx->l_shm->updTime  < 0 ){
            printf("fail to stat config file %s\n" , filepath);
            return -1;
        }
    }

    ctx->bufferNum = 0;
    memset(ctx->buffers , 0x00 , sizeof(ctx->buffers));
    ctx->closeFlag = 0;
    struct timeval tv;
    gettimeofday( &tv , NULL );
    alog_update_timer( tv );

    /**
     * create update thread
     */
    pthread_create(&(g_alog_ctx->updTid), NULL, alog_update_thread, NULL );

    /**
     * register atexit , ensure alog_close is called before exit
     * */
    atexit(alog_close);

    /**
     * register atfork , ensure that mutex status is normal after fork
     */
    pthread_atfork( alog_atfork_prepare , alog_atfork_after , alog_atfork_after );

    return 0;
}
/**
 * [alog_close close and destroy context]
 * @return [0 for success]
 */
void alog_close()
{
    if ( g_alog_ctx == NULL )   return;
    /**
     * set close flag
     */
    alog_lock();
    g_alog_ctx->closeFlag = 1;
    alog_unlock();

    /**
     * signal all threads
     */
    pthread_cond_broadcast(&(g_alog_ctx->cond_persist));
    
    /**
     * join all threadsa
     */
    int i = 0;
    for ( i = 0 ; i < g_alog_ctx->bufferNum ; i ++ ){
        pthread_join(g_alog_ctx->buffers[i].consTid, NULL);
    }
    pthread_join(g_alog_ctx->updTid, NULL);
    ALOG_DEBUG("in SYNC MODE all threads joined");

    /**
     * detach share memory
     */
    shmdt(g_alog_ctx->g_shm);

    /**
     * clean up resources
     */
    pthread_mutex_destroy(&(g_alog_ctx->mutex));
    //pthread_cond_destroy(&(g_alog_ctx->cond_persist));
    free(g_alog_ctx->l_shm);
    free(g_alog_ctx);

    g_alog_ctx = NULL;
    
    return;
}
/**
 * [alog_writelog_t  main interface for logging]
 * @param  logtype     [ALOG_TYPE_BIN/ALOG_TYPE_ASC/ALOG_TYPE/HEX]
 * @param  level       [LOGNON/LOGFAT/LOGERR/LOGWAN/LOGINF/LOGADT/LOGDBG]
 * @param  regname     [register name , max 20 bytes]
 * @param  cstname     []
 * @param  modname     [module name]
 * @param  file        [__FILE__]
 * @param  lineno      [__LINE__]
 * @param  logfilepath [input log base path]
 * @param  buf         [buffer]
 * @param  len         [length of buffer]
 * @param  fmt         [format for ascii print]
 * @param  ...         []
 * @return             [0 for success]
 */
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
        char            *fmt , ...)
{
    /**
     * check if context is initialized
     */
    if ( g_alog_ctx == NULL && alog_initContext() != 0  ){
        return ALOGERR_CTX_NOTINIT;
    }
    /**
     * check input parameters
     */
    if ( (logtype != ALOG_TYPE_ASC && logtype != ALOG_TYPE_BIN && logtype != ALOG_TYPE_HEX)  ||\
         (level < LOGNON || level > LOGDBG)                                                  ||\
         (regname == NULL || strlen(regname) > ALOG_REGNAME_LEN)                             ||\
         (cstname == NULL || strlen(cstname) > ALOG_CSTNAME_LEN)                             ||\
         (modname == NULL) || (file == NULL) )     
        return ALOGMSG_INVALID_PARAM;
    int             i = 0 ;
    int             j = 0 ;
    unsigned char   ch = ' ';
    struct          timeval  tv;
    sigset_t        n_sigset;
    sigset_t        o_sigset;

    /**
     * get config for current regname in local memory
     */
    alog_regCfg_t   *regCfg = NULL;
    if ( (regCfg = getRegByName( g_alog_ctx->l_shm , regname ) ) == NULL )
        return ALOGMSG_REG_NOTFOUND;

    /**
     * judge log level
     */
    if ( level > regCfg->level )
        return ALOGOK;

    /**
     * block signal
     */
    sigemptyset(&n_sigset);    
    sigemptyset(&o_sigset);
    sigaddset( &n_sigset , SIGCHLD );
    sigaddset( &n_sigset , SIGALRM );
    sigaddset( &n_sigset , SIGTERM );
    sigaddset( &n_sigset , SIGUSR1 );
    pthread_sigmask(SIG_BLOCK , &n_sigset , &o_sigset);

    /**
     * lock mutex
     */
    alog_lock();

    /**
     * if cstname is not set , then set it to current PID
     */
    char realcstname[ALOG_CSTNAME_LEN+1];
    memset(realcstname , 0x00 , ALOG_CSTNAME_LEN + 1);
    if ( strlen(cstname) == 0 ){
        sprintf( realcstname , "%d" , getpid());
    } else {
        strcpy( realcstname , cstname );
    }
    /**
     * get buffer for current regname+cstname+logfilepath
     * if buffer not exists then create one
     */
    alog_buffer_t   *buffer = NULL;
    if( ( buffer = getBufferByName( regname , realcstname , logfilepath )) == NULL ){
        alog_unlock();
        pthread_sigmask(SIG_SETMASK , &o_sigset , NULL);
        return ALOGMSG_BUF_NOTFOUND;
    }

    /**
     * packing log message based on config
     */
    int         offset = 0;
    int         max = g_alog_ctx->l_shm->singleBlockSize * 1024;
    char        *temp = (char *)malloc(max);
    if ( temp == NULL ){
        alog_unlock();
        pthread_sigmask(SIG_SETMASK , &o_sigset , NULL);
        return ALOGERR_MALLOC_FAIL;
    }
    int         leftsize = 0 ;
    memset( temp , 0x00 , max );

    /**
     * get current time
     */
    gettimeofday( &tv , NULL );


    /* traceid */
    if ( strcmp( regname , "TRACE" ) && ALOG_TRACEID ){
        offset += sprintf( temp+offset , "[%s]" , ALOG_TRACEID );
    }
    /**
     * date
     */
    if ( regCfg->format[0] == '1' ){
        if ( tv.tv_sec % 86400 != g_alog_ctx->timer.sec % 86400 ){
            /* update timer if now is a new day */
            alog_update_timer( tv );
        }
        offset += sprintf( temp+offset , "[%8s]" , g_alog_ctx->timer.date  );
    }
    /**
     * time
     */
    if ( regCfg->format[1] == '1' ){
        if ( tv.tv_sec != g_alog_ctx->timer.sec ){
            /* update timer if now is a new second */
            alog_update_timer( tv );
        }
        offset += sprintf( temp+offset ,  "[%8s]" , g_alog_ctx->timer.time );
    }
    /**
     * micro second
     */
    if ( regCfg->format[2] == '1' ){
        offset += sprintf( temp+offset ,"[%06d]" , (int)tv.tv_usec );
    }
    /**
     * pid+tid
     */
    if ( regCfg->format[3] == '1' ){
        offset += sprintf( temp+offset ,"[PID:%-8d]" , getpid() );
        offset += sprintf( temp+offset ,"[TID:%-6ld]" , (long)pthread_self()%1000000 );
    }
    /**
     * modname
     */
    if ( regCfg->format[4] == '1' ){
        if ( modname && strlen(modname) )
            offset += sprintf( temp+offset , "[%-30s]" , modname );
        else
            offset += sprintf( temp+offset , "[]" );

    }
    /**
     * log level
     */
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
    /**
     * filaname+lineno
     */
    if ( regCfg->format[6] == '1' ){
        offset += sprintf( temp+offset , "[%s:%d]" , file , lineno);
    }

    va_list ap;
    va_start( ap , fmt );

    /* apppend read log message based on logtype */
    switch ( logtype ){
        case ALOG_TYPE_ASC:
            if ( strcmp( regname , "TRACE" ) != 0  ){
                offset += snprintf( temp+offset , max - offset , "[");
            }
            offset += vsnprintf( temp+offset , max - offset - 2, fmt , ap );
            if ( strcmp( regname , "TRACE" ) != 0 ){
                offset += snprintf( temp+offset , max - offset , "]\n");
            } else {
                offset += snprintf( temp+offset , max - offset , "\n");
            }
            va_end(ap);
            break;
        case ALOG_TYPE_HEX:
offset += snprintf( temp+offset , max - offset , "\n--------------------------------Hex Message begin------------------------------\n");
            for ( i = 0 ; i <= len/16 ; i ++){
                offset += snprintf( temp+offset , max - offset , "M(%06d)=< " , i*16);
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
            offset += snprintf( temp+offset , max - offset , "[");
            leftsize = max - 2 - offset;
            if ( len > leftsize ){
                memcpy( temp + offset , buf , leftsize );
                offset = max;
            } else {
                memcpy( temp + offset , buf , len );
                offset += len + 2;
            }
            temp[offset-2] = ']';
            temp[offset-1] = '\n';
            break;
    }

    /* get productor pointer for current buffer */
    alog_bufNode_t  *node = buffer->prodPtr;

    ALOG_DEBUG("message packed ok , length[%d] , finding available node...",offset);
    /* judge if message length exceeds singleblocksize */
    if ( offset > g_alog_ctx->l_shm->singleBlockSize*1024 ){
        alog_unlock();
        free(temp);
        pthread_sigmask(SIG_SETMASK , &o_sigset , NULL);
        return ALOGERR_MALLOC_FAIL;
    }

    /* judge if there are enough space for current message */
    if (node->usedFlag == ALOG_NODE_FULL || node->offset + offset > node->len ){ 

        ALOG_DEBUG("current node node [%d] is FULL or dosen't have enough space ",node->index);
        if( node->usedFlag != ALOG_NODE_FULL )
            node->usedFlag = ALOG_NODE_FULL;

        ALOG_DEBUG("checking next node [%d] ",node->next->index);
        /* if the next node is not FREE , then try to add a new node */
        if ( node->next->usedFlag !=  ALOG_NODE_FREE ){
            ALOG_DEBUG("node [%d] is not FREE",node->next->index);

            /* judge if total memory use exceeds limit */
            if ( g_alog_ctx->l_shm->singleBlockSize * (buffer->nodeNum+1) > g_alog_ctx->l_shm->maxMemorySize * 1024){
                ALOG_DEBUG("memory use over limit , can not create new nodes");
                alog_unlock();
                free(temp);
                pthread_sigmask(SIG_SETMASK , &o_sigset , NULL);
                return ALOGERR_MEMORY_FULL;
            } else {
                ALOG_DEBUG("start to add new node");
                /* add a new node */
                alog_bufNode_t *tempnode = (alog_bufNode_t *)malloc(sizeof(alog_bufNode_t));
                if ( tempnode == NULL){
                    alog_unlock();
                    free(temp);
                    pthread_sigmask(SIG_SETMASK , &o_sigset , NULL);
                    return ALOGERR_MALLOC_FAIL;
                }
                /* append new node to chain */
                tempnode->content = (char *)malloc(g_alog_ctx->l_shm->singleBlockSize*1024);
                if ( tempnode->content == NULL ){
                    free(tempnode);
                    alog_unlock();
                    free(temp);
                    pthread_sigmask(SIG_SETMASK , &o_sigset , NULL);
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
    pthread_sigmask(SIG_SETMASK , &o_sigset , NULL);
    return ALOGOK;
}

