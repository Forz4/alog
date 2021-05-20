#include "alogfun.h"
/*
 *  get config by regname
 * */
alog_regCfg_t *getRegByName(alog_shm_t *shm , char *regname)
{
    if ( strlen(regname) == 0 || strlen(regname) > ALOG_REGNAME_LEN ){
        return NULL;
    }
    int i = 0 ;
    for ( i = 0 ; i < shm->regNum ; i ++ ){
        if ( strcmp( shm->regCfgs[i].regName , regname ) == 0 ){
            return &(shm->regCfgs[i]);
        }
    }
    return NULL;
}
/* 
 *  get buffer by regname+cstname
 *  if buffer not found , then try to create a new buffer and a new persist thread
 * */
alog_buffer_t *getBufferByName( char *regname , char *cstname)
{
    if ( strlen(regname) == 0 || strlen(regname) > ALOG_REGNAME_LEN ||\
            strlen(cstname) == 0 || strlen(cstname) > ALOG_CSTNAME_LEN ){
        return NULL;
    }
    int i = 0;
    for ( i = 0 ; i < g_alog_ctx->bufferNum ; i ++){
        if ( strcmp( g_alog_ctx->buffers[i].regName , regname ) == 0 && \
                strcmp( g_alog_ctx->buffers[i].cstName , cstname ) == 0 ){
            /*
             * judge if persist thread exist especially in the case of fork situation 
             * after fork , children process get buffer memory but no persist thread
             * */
            if ( pthread_kill(g_alog_ctx->buffers[i].consTid,0) ){
                ALOG_DEBUG("buffer exist but no persist thread lost , start to recreate persist thread");
                alog_persist_arg_t  *arg = (alog_persist_arg_t *)malloc(sizeof(alog_persist_arg_t));
                strcpy( arg->regName , regname);
                strcpy( arg->cstName , cstname);
                pthread_create(&(g_alog_ctx->buffers[i].consTid), NULL, alog_persist_thread, (void *)arg );
                free(arg);
            }
            if ( pthread_kill(g_alog_ctx->updTid,0) ){
                ALOG_DEBUG("update thread lost , start to recreate update thread");
                pthread_create(&(g_alog_ctx->updTid), NULL, alog_update_thread, NULL );
            }
            return &(g_alog_ctx->buffers[i]);
        }
    }
    return NULL;
}
/*
 * locking operation
 * */
int alog_lock()
{
    pthread_mutex_lock(&(g_alog_ctx->mutex));
    return 0;
}
/* 
 * unlocking operation
 * */
int alog_unlock()
{
    pthread_mutex_unlock(&(g_alog_ctx->mutex));
    return 0;
}
/* 
 *  update timer
 * */
void alog_update_timer()
{
    struct timeval  tv;
    time_t t;

    gettimeofday(&tv , NULL);
    time(&t);

    struct tm *temp = localtime( &t );
    sprintf( g_alog_ctx->timer.date , "%4d%02d%02d" , temp->tm_year+1900 , temp->tm_mon + 1 , temp->tm_mday);
    sprintf( g_alog_ctx->timer.time , "%02d:%02d:%02d" ,temp->tm_hour ,temp->tm_min ,temp->tm_sec);

    g_alog_ctx->timer.sec = tv.tv_sec;
    memcpy(&(g_alog_ctx->timer.tv) , &tv , sizeof(struct timeval) );
    memcpy(&(g_alog_ctx->timer.tmst) , temp , sizeof( struct tm) );
    return ;
}
/* 
 *  alloc space for a new buffer
 * */
int alog_addBuffer( char *regname  , char *cstname , alog_buffer_t **retbuffer)
{
    /* judge if total number exceeds limit */
    if ( g_alog_ctx->bufferNum >= ALOG_BUFFER_NUM ){
        return ALOGERR_BUFFERNUM_OVERFLOW;
    }

    /* check input */
    if ( strlen(regname) == 0 || strlen(regname) > ALOG_REGNAME_LEN ||\
            strlen(cstname) == 0 || strlen(cstname) > ALOG_CSTNAME_LEN ){
        return ALOGERR_REGCST_INVALID;
    }
    /* get config by regname */
    alog_regCfg_t       *regCfg;
    if ( (regCfg = getRegByName( g_alog_ctx->l_shm , regname )) == NULL ){
        ALOG_DEBUG("regname[%s] not found" , regname);
        return ALOGMSG_REG_NOTFOUND;
    }
    /* check if buffer already exists */
    alog_buffer_t       *buffer;
    if ( (buffer = getBufferByName( regname , cstname )) != NULL ){
        ALOG_DEBUG("buffer regname[%s] cstname[%s] alread exists" , regname , cstname);
        return ALOGOK;
    }

    /* add buffer */
    ALOG_DEBUG("start to create buffer for regname[%s] cstname[%s]" , regname , cstname);
    buffer = &(g_alog_ctx->buffers[g_alog_ctx->bufferNum]);
    g_alog_ctx->bufferNum ++;
    strncpy( buffer->regName , regname , sizeof(buffer->regName) );
    strncpy( buffer->cstName , cstname , sizeof(buffer->cstName) );

    /* alloc space for the first node */
    buffer->nodeNum = 1;
    alog_bufNode_t    *node = NULL;
    node = ( alog_bufNode_t *)malloc( sizeof(alog_bufNode_t) );
    if ( node == NULL )
    {
        ALOG_DEBUG("malloc [alog_bufNode_t] fail");
        return ALOGERR_MALLOC_FAIL;
    }
    buffer->prodPtr = node;
    buffer->consPtr = node;

    node->index = buffer->nodeNum;
    node->usedFlag = ALOG_NODE_FREE;
    node->len = g_alog_ctx->l_shm->singleBlockSize*1024;
    node->content = (char *)malloc(node->len);
    if( node->content == NULL ){
        return ALOGERR_MALLOC_FAIL;
    }
    node->offset = 0;
    node->next = node;
    node->prev = node;

    /* create persist thread */
    alog_persist_arg_t  *arg = (alog_persist_arg_t *)malloc(sizeof(alog_persist_arg_t));
    strcpy( arg->regName , regname);
    strcpy( arg->cstName , cstname);
    pthread_create(&(buffer->consTid), NULL, alog_persist_thread, (void *)arg );
    
    *retbuffer = buffer;
    return ALOGOK;
}
/*
 *  update thread func
 *  check and update local memory every $CHECKINTERVAL seconds
 * */
void *alog_update_thread(void *arg)
{
    struct          timeval  tv;
    int             shmid = 0;

    /* block signals */
    sigset_t            sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    sigaddset(&sigset, SIGUSR1);
    sigaddset(&sigset, SIGUSR2);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGKILL);
    sigaddset(&sigset, SIGSEGV);
    pthread_sigmask(SIG_BLOCK , &sigset , NULL);
    
    while ( g_alog_ctx->closeFlag != 1 ){
        alog_lock();
        gettimeofday( &tv , NULL );
        /* if share memory key not exists , give up updating */
        if ( (shmid = shmget( g_alog_ctx->l_shm->shmKey , sizeof(alog_shm_t) , 0 )) > 0 ){
            /* if shmid changes , it means share memory was recreated , then reattach to share memory */
            if ( shmid != g_alog_ctx->l_shm->shmId ){
                ALOG_DEBUG("shmid changes , reattach to share memory");
                g_alog_ctx->g_shm = (alog_shm_t *)shmat( shmid , NULL , 0 );
            }
            /* check updTime */
            if ( g_alog_ctx->l_shm->updTime != g_alog_ctx->g_shm->updTime ){
                ALOG_DEBUG("share memory was updated , sync config , updTime[%ld]" , g_alog_ctx->g_shm->updTime);
                memcpy( g_alog_ctx->l_shm , g_alog_ctx->g_shm , sizeof( alog_shm_t) ) ;
            }
        }
        alog_unlock();
        sleep(g_alog_ctx->l_shm->checkInterval);
    }
    return NULL;
}
/*
 *  persist thread func
 *  read from buffer and write to file
 * */
void *alog_persist_thread(void *arg)
{
    ALOG_DEBUG("persist thread start");

    /* 获取参数 */
    alog_persist_arg_t  *myarg = ( alog_persist_arg_t *)arg;
    ALOG_DEBUG("arg->regname[%s]" , myarg->regName);
    ALOG_DEBUG("arg->cstname[%s]" , myarg->cstName);

    char                myregname[20+1];
    char                mycstname[20+1];
    strcpy( myregname , myarg->regName );
    strcpy( mycstname , myarg->cstName );

    /* block signals */
    sigset_t            sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    sigaddset(&sigset, SIGUSR1);
    sigaddset(&sigset, SIGUSR2);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGKILL);
    sigaddset(&sigset, SIGSEGV);
    pthread_sigmask(SIG_BLOCK , &sigset , NULL);

    /* get buffer by regname+cstname */
    alog_buffer_t       *buffer = getBufferByName( myregname  , mycstname);
    if ( buffer == NULL ){
        ALOG_DEBUG("fail to get buffer");
        return NULL;
    }
    alog_bufNode_t      *node = NULL;
    alog_bufNode_t      *node_start = NULL;
    struct timespec     abstime;
    struct timeval      timeval;
    int                 ret = 0;
    while ( 1 ){
        alog_lock();
        node = buffer->consPtr;
        ALOG_DEBUG("persist thread at node[%d]" , node->index);
        /* 
         * 1. if current node not FULL , then sleep ${FLUSHINTERVAL} seconds , quit when closeFlag is set
         * */
        if ( g_alog_ctx->closeFlag != 1 && node->usedFlag != ALOG_NODE_FULL ){

            ALOG_DEBUG("current node [%d] not FULL" , node->index);
            gettimeofday(&timeval , NULL);
            abstime.tv_sec = timeval.tv_sec + g_alog_ctx->l_shm->flushInterval;
            abstime.tv_nsec =  timeval.tv_usec * 1000;
            ALOG_DEBUG("start to  pthread_cond_timedwait");
            ret = pthread_cond_timedwait(&(g_alog_ctx->cond_persist) , &(g_alog_ctx->mutex) , &abstime);
            ALOG_DEBUG("wake from pthread_cond_timedwait ret[%d]" , ret);
            /* 
             * 2. time out or wake by other threads , then try to persist current node
             * */
        }

        ALOG_DEBUG("check node[%d] again" , node->index);
        /*
         * 3. check current node again , if still FREE  then unlock and retry
         * */
        if ( node->usedFlag == ALOG_NODE_FREE ){
            ALOG_DEBUG("node[%d] is FREE" , node->index);
            alog_unlock();
            /*
             * if current node is FREE and closeFlag is set , then quit
             * */
            if ( g_alog_ctx->closeFlag == 1 ){
                ALOG_DEBUG("closeFlag set , persist thread quitting");
                break;
            } else {
                ALOG_DEBUG("closeFlag not set , continue ");
                continue;
            }
        }
        /*
         * 4. if current node not FREE , then iterate all nodes and find all nodes which are not FREE,
         *    set those nodes to FULL and start to persist
         * */
        int i = 0 ;
        int count = 0;
        node_start = node;
        ALOG_DEBUG("start to iterate all [%d] nodes" , buffer->nodeNum);
        for ( i = 0 ; i < buffer->nodeNum ; i ++){
            ALOG_DEBUG("check node[%d]" , node->index);
            if ( node->usedFlag == ALOG_NODE_USED ){
                ALOG_DEBUG("node[%d] is USED" , node->index);
                node->usedFlag = ALOG_NODE_FULL;
                ALOG_DEBUG("modify node[%d] to FULL" , node->index);
                count ++;
                break;
            } else if ( node->usedFlag == ALOG_NODE_FULL ){
                ALOG_DEBUG("node[%d] is FULL" , node->index);
                count ++;
                node = node->next;
                ALOG_DEBUG("continue to iterate , count is [%d]",count);
                continue;
            } else if ( node->usedFlag == ALOG_NODE_FREE ){
                ALOG_DEBUG("node[%d] is FREE , quit" , node->index);
                break;
            }
        }
        alog_unlock();

        /* 
         * 5. persist all FULL nodes
         * */
        node = node_start;
        ALOG_DEBUG("start to persist from node[%d] , total[%d] nodes" , node->index , count);
        for ( i = 0 ; i < count ; i ++ ){
            ALOG_DEBUG("persisting node[%d]",node->index);
            alog_persist( myregname , mycstname , node );
            ALOG_DEBUG("node[%d] persistence done",node->index);
            node = node->next;
        }

        /* 
         * 6. update node status
         * */
        alog_lock();
        node = node_start;
        ALOG_DEBUG("start to update node status from node[%d] , total[%d] nodes" , node->index , count);
        for ( i = 0 ; i < count ; i ++ ){
            ALOG_DEBUG("set node[%d] to FREE",node->index);
            node->usedFlag = ALOG_NODE_FREE;
            memset( node->content , 0x0 ,  node->len );
            node->offset = 0;
            node = node->next;
        }
        alog_unlock();
        buffer->consPtr = node;
/*
        if ( buffer->prodPtr == node ){
            ALOG_DEBUG("生产者线程指针切换到节点[%d]",buffer->consPtr->index);
            buffer->prodPtr = node->next;
        }
*/
    }

    /* rename log file before quit */
    alog_regCfg_t *cfg = getRegByName( g_alog_ctx->l_shm , myregname );
    if ( cfg != NULL ){
        char filePath[ALOG_FILEPATH_LEN];
        char bak_filePath[ALOG_FILEPATH_LEN];
        char command[ALOG_COMMAND_LEN];
        memset(filePath , 0x00 , sizeof(filePath));
        memset(bak_filePath , 0x00 , sizeof(bak_filePath));
        memset(command , 0x00 , sizeof(command));
        getFileNameFromFormat( ALOG_CURFILEFORMAT , cfg , myregname , mycstname , filePath);
        getFileNameFromFormat( ALOG_BAKFILEFORMAT , cfg , myregname , mycstname , bak_filePath);
        sprintf( command , "mv %s %s" , filePath , bak_filePath);
        system( command );
    }

    ALOG_DEBUG("persist thread clean up");
    int i = 0 ;
    alog_bufNode_t  *temp = node;
    for( i = 0 ; i < buffer->nodeNum ; i ++ ){
        temp = node->next;
        free(node->content);
        free(node);
        node = temp;
    }
    return NULL;
}
/*
 *  persisting operation
 * */
int alog_persist( char *regname , char *cstname , alog_bufNode_t *node)
{
    /* get config by regname */
    alog_regCfg_t *cfg = getRegByName( g_alog_ctx->l_shm , regname );
    if ( cfg == NULL ){
        return -1;
    }
    char filePath[ALOG_FILEPATH_LEN];
    memset(filePath , 0x00 , sizeof(filePath));

    alog_update_timer();

    /* get log file name */
    getFileNameFromFormat( ALOG_CURFILEFORMAT , cfg , regname , cstname , filePath);

    FILE *fp = fopen( filePath , "a+");
    fwrite(node->content , node->offset , 1 , fp);

    /* judge if file size over limit */
    long filesize = ftell(fp);
    if ( filesize > cfg->maxSize*1024*1024 ){
        char bak_filePath[ALOG_FILEPATH_LEN];
        char command[ALOG_COMMAND_LEN];
        memset(bak_filePath , 0x00 , sizeof(bak_filePath));
        memset(command , 0x00 , sizeof(command));
        getFileNameFromFormat( ALOG_BAKFILEFORMAT , cfg , regname , cstname , bak_filePath);
        sprintf( command , "mv %s %s" , filePath , bak_filePath);
        system( command );
    }
    fclose(fp);
    return ALOGOK;
}
/*
 *  get log file name 
 * */
void getFileNameFromFormat( int type  , alog_regCfg_t *cfg , char *regname , char *cstname , char filePath[ALOG_FILEPATH_LEN] )
{
    char        *p =  NULL;
    int         i = 0;
    int         len = 0;

    if ( type == ALOG_CURFILEFORMAT ){
        p = cfg->curFilePath;
    } else if ( type == ALOG_BAKFILEFORMAT ){
        p = cfg->bakFilePath;
    }
    len = strlen(p);

    for ( ; ; ) {
        if ( *p == '\0' ){
            break;
        } else if ( *p == '%' ){
            p++;
            switch( *p ){
                case 'Y':
                    memcpy( filePath+i , g_alog_ctx->timer.date , 4);
                    i += 4;
                    break;
                case 'M':
                    memcpy( filePath+i , g_alog_ctx->timer.date+4 , 2);
                    i += 2;
                    break;
                case 'D':
                    memcpy( filePath+i , g_alog_ctx->timer.date+6 , 2);
                    i += 2;
                    break;
                case 'h':
                    memcpy( filePath+i , g_alog_ctx->timer.time , 2);
                    i += 2;
                    break;
                case 'm':
                    memcpy( filePath+i , g_alog_ctx->timer.time+3 , 2);
                    i += 2;
                    break;
                case 's':
                    memcpy( filePath+i , g_alog_ctx->timer.time+6 , 2);
                    i += 2;
                    break;
                case 'R':
                    memcpy( filePath+i , regname , strlen(regname));
                    i += strlen(regname);
                    break;
                case 'C':
                    memcpy( filePath+i , cstname , strlen(cstname));
                    i += strlen(cstname);
                    break;
                case 'T':
                    i = i + sprintf( filePath+i , "%d" , getpid());
                    break;
                default:
                    filePath[i] = *p;
                    i += 1;
            }
        } else {
            filePath[i] = *p;
            i ++;
        }
        p ++;
    }
    return ;
}
/*
 *  read from brackets
 * */
int get_bracket(const char *line , int no , char *value , int val_size)
{
	int i = 0;
	int j = 0;
	int count = 0;
	memset(value , 0x00 , val_size);
	while ( count != no){
		if ( line[i] == '\0' || line[i] == '\n' )
			break;
		if ( line[i] == '[' )
			count ++;
		i ++;
	}
	if ( count != no )
		return 1;
	j = 0;
	while ( line[i] != ']' ){
		if ( line[i] == '\0' || line[i] == '\n' )
			break;
		value[j] = line[i];
		j ++;
		i ++;
	}
	if ( line[i] != ']' )
		return 1;

	return 0;
}
/*
 *  load configs from environment and file
 * */
alog_shm_t *alog_loadCfg( char *filepath )
{
    char    line[ALOG_FILEPATH_LEN];
    char    buf[ALOG_CFGBUF_LEN];
    int     temp = 0;

    /* allocate space for share memory */
    alog_shm_t *l_shm = (alog_shm_t *)malloc(sizeof(alog_shm_t));
    if ( l_shm == NULL ){
        return l_shm;
    }
    l_shm->regNum = 0;

    FILE *fp = fopen( filepath , "r");
    if ( fp == NULL ){
        perror("fail to open config file\n");
        return NULL;
    }

    while (fgets(line , sizeof(line), fp) != NULL){

        if ( line[0] == '\n' || line[0] == '#' )
            continue;
        
        if ( l_shm->regNum >= ALOG_REG_NUM ){
            break;
        }
        
        alog_regCfg_t   *cfg = &(l_shm->regCfgs[l_shm->regNum]);
        
        /* regname */
        if (get_bracket(line , 1 , buf , ALOG_CFGBUF_LEN)){
            fclose(fp);
            return NULL;
        }
        strncpy( cfg->regName , buf , ALOG_REGNAME_LEN);

        /* log level */
        if (get_bracket(line , 2 , buf , ALOG_CFGBUF_LEN)){
            fclose(fp);
            return NULL;
        }
        if ( strcmp( buf , "LOGNON") == 0 ){
            cfg->level = LOGNON;
        } else if ( strcmp( buf , "LOGFAT") == 0 ){
            cfg->level = LOGFAT;
        } else if ( strcmp( buf , "LOGERR") == 0 ){
            cfg->level = LOGERR;
        } else if ( strcmp( buf , "LOGWAN") == 0 ){
            cfg->level = LOGWAN;
        } else if ( strcmp( buf , "LOGINF") == 0 ){
            cfg->level = LOGINF;
        } else if ( strcmp( buf , "LOGADT") == 0 ){
            cfg->level = LOGADT;
        } else if ( strcmp( buf , "LOGDBG") == 0 ){
            cfg->level = LOGDBG;
        } else {
            cfg->level = LOGNON;
        }

        /* file size limit */
        if (get_bracket(line , 3 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        cfg->maxSize = atoi(buf);

        /* prefix format */
        if (get_bracket(line , 4 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        strncpy( cfg->format , buf , 7 );

        char cmd[ALOG_COMMAND_LEN];
        FILE *fp_cmd = NULL;

        /*  current file name patterm */
        if (get_bracket(line , 5 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        strcpy( cfg->curFilePath_r , buf );
        memset( cmd , 0x00 , ALOG_COMMAND_LEN);
        sprintf( cmd , "echo %s" , buf);
        fp_cmd = popen( cmd , "r");
        if ( fp_cmd ){
            fgets(cfg->curFilePath , ALOG_FILEPATH_LEN , fp_cmd);
            if ( cfg->curFilePath[strlen(cfg->curFilePath)-1] == '\n' )
                cfg->curFilePath[strlen(cfg->curFilePath)-1] = '\0';
            pclose(fp_cmd);
        } else {
            strncpy(cfg->curFilePath ,buf , ALOG_CFGBUF_LEN);
        }

        /*  backup file name patterm */
        if (get_bracket(line , 6 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        strcpy( cfg->bakFilePath_r , buf );
        memset( cmd , 0x00 , ALOG_COMMAND_LEN);
        sprintf( cmd , "echo %s" , buf);
        fp_cmd = popen( cmd , "r");
        if ( fp_cmd ){
            fgets(cfg->bakFilePath , ALOG_FILEPATH_LEN , fp_cmd);
            if ( cfg->bakFilePath[strlen(cfg->bakFilePath)-1] == '\n' )
                cfg->bakFilePath[strlen(cfg->bakFilePath)-1] = '\0';
            pclose(fp_cmd);
        } else {
            strncpy(cfg->bakFilePath ,buf , ALOG_CFGBUF_LEN);
        }
        l_shm->regNum ++;
    }
    fclose(fp);

    l_shm->maxMemorySize = ALOG_DEF_MAXMEMORYSIZE;
    l_shm->singleBlockSize = ALOG_DEF_SINGLEBLOCKSIZE;
    l_shm->flushInterval = ALOG_DEF_FLUSHINTERVAL; 
    l_shm->checkInterval = ALOG_DEF_CHECKINTERVAL; 

    /* load from environment */
    if ( getenv("ALOG_MAXMEMORYSIZE") ){
        temp = atoi(getenv("ALOG_MAXMEMORYSIZE"));
        if ( temp > 0 && temp <= 4096 ){
            l_shm->maxMemorySize = temp;
        } 
    }
    if ( getenv("ALOG_SINGLEBLOCKSIZE") ){
        temp = atoi(getenv("ALOG_SINGLEBLOCKSIZE"));
        if ( temp > 0 && temp <= 1024 ){
            l_shm->singleBlockSize = temp;
        }
    }
    if ( getenv("ALOG_FLUSHINTERVAL") ){
        temp = atoi(getenv("ALOG_FLUSHINTERVAL"));
        if ( temp > 0 && temp <= 10 ){
            l_shm->flushInterval = temp;
        }
    }
    if ( getenv("ALOG_CHECKINTERVAL") ){
        temp = atoi(getenv("ALOG_CHECKINTERVAL"));
        if ( temp > 0 && temp <= 10 ){
            l_shm->checkInterval = temp;
        }
    }
    return l_shm;
} 
/*
 *  Clean Up context
 * */
void alog_cleanContext()
{
    if ( g_alog_ctx == NULL ){
        return ;
    }
    shmdt(g_alog_ctx->g_shm);

    int i = 0;
    int j = 0;
    alog_bufNode_t  *temp;
    alog_bufNode_t  *node;
    for ( i = 0 ; i < g_alog_ctx->bufferNum ; i ++ ){
        node = g_alog_ctx->buffers[i].prodPtr;;
        for( j = 0 ; j < g_alog_ctx->buffers[i].nodeNum ; j ++ ){
            temp = node->next;
            free(node->content);
            free(node);
            node = temp;
        }
    }
    pthread_mutex_destroy(&(g_alog_ctx->mutex));
    pthread_cond_destroy(&(g_alog_ctx->cond_persist));
    free(g_alog_ctx->l_shm);
    free(g_alog_ctx);
    return ;
}
