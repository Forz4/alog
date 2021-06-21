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
alog_buffer_t *getBufferByName( char *regname , char *cstname , char *logfilepath)
{
    if ( strlen(regname) == 0 || strlen(regname) > ALOG_REGNAME_LEN ||\
            strlen(cstname) == 0 || strlen(cstname) > ALOG_CSTNAME_LEN ){
        return NULL;
    }
    int i = 0;
    for ( i = 0 ; i < g_alog_ctx->bufferNum ; i ++){
        if ( strcmp( g_alog_ctx->buffers[i].regName , regname ) == 0 && \
                strcmp( g_alog_ctx->buffers[i].cstName , cstname ) == 0 ){

            /* if input logfilepath , then input path with current logBasePath */
            if ( logfilepath && strlen(logfilepath) ) {
                if ( strcmp( logfilepath , g_alog_ctx->buffers[i].logBasePath ) ){
                    continue;
                }
            } else {
                /* if no path input , then decide if default log path is created */
                if ( g_alog_ctx->buffers[i].isDefaultPath == 0 ) {
                    continue;
                }
            }
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
int alog_addBuffer( char *regname  , char *cstname , char *logfilepath , alog_buffer_t **retbuffer)
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
    if ( (buffer = getBufferByName( regname , cstname , logfilepath)) != NULL ){
        ALOG_DEBUG("buffer regname[%s] cstname[%s] logfilepath[%s] alread exists" , regname , cstname , logfilepath==NULL?"":logfilepath);
        return ALOGOK;
    }

    /* ensure logfilepath exists */
    if ( (logfilepath  && strlen(logfilepath) && alog_mkdir( logfilepath )) ||\
            ( alog_mkdir( regCfg->defLogBasePath) ) ){
        return ALOGERR_MKDIR_FAIL;
    }

    /* add buffer */
    ALOG_DEBUG("start to create buffer for regname[%s] cstname[%s]" , regname , cstname);
    buffer = &(g_alog_ctx->buffers[g_alog_ctx->bufferNum]);
    g_alog_ctx->bufferNum ++;
    strncpy( buffer->regName , regname , sizeof(buffer->regName) );
    strncpy( buffer->cstName , cstname , sizeof(buffer->cstName) );
    if ( logfilepath  && strlen(logfilepath) ){
        buffer->isDefaultPath = 0;
        strcpy( buffer->logBasePath , logfilepath );
    }
    else {
        buffer->isDefaultPath = 1;
        strcpy( buffer->logBasePath , regCfg->defLogBasePath );
    }

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
    alog_persist_arg_t  *arg = &(buffer->arg);
    strcpy( arg->regName , regname);
    strcpy( arg->cstName , cstname);
    strcpy( arg->logBasePath , buffer->logBasePath);

    pthread_create(&(buffer->consTid), NULL, alog_persist_thread, (void *)arg );
    
    *retbuffer = buffer;
    return ALOGOK;
}
/*
 * check if directory is valid , if not exists then try to create path
 * */
int alog_mkdir( char *dir )
{
    int len = strlen( dir );
    int i = 0;
    char DirName[ALOG_FILEPATH_LEN];

    if ( access( dir , R_OK ) == 0 ) return ALOGOK;

    memset( DirName , 0x00 , sizeof(DirName) );
    strncpy( DirName , dir , sizeof( DirName ) - 1);
    len = strlen( DirName);

    if( DirName[len-1] != '/' ){
        DirName[len] = '/';
        len ++;
    }

    for( i = 1 ; i < len ; i++){
        if ( DirName[i] == '/' ){
            DirName[i] = 0;
            if ( access(DirName , R_OK) != 0 ){
                if( mkdir( DirName , 0777 ) == -1 ) return ALOGERR_MKDIR_FAIL;
            }
            DirName[i] = '/';
        }
    }

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
        if ( g_alog_ctx->statusless == 1 ){
            char cfgFile[ALOG_FILEPATH_LEN+1];
            memset( cfgFile , 0x00 , sizeof(cfgFile) );
            sprintf( cfgFile , "%s/cfg/alog.cfg" , getenv("ALOG_HOME"));
            struct stat buf;
            if ( stat( cfgFile , &buf) == 0 ){
                if ( buf.st_mtime != g_alog_ctx->l_shm->updTime ){
                    alog_lock();
                    g_alog_ctx->l_shm = alog_loadCfg( cfgFile );
                    if ( g_alog_ctx->l_shm == NULL ){
                        ALOG_DEBUG("fail to load config\n");
                    } else {
                        g_alog_ctx->l_shm->updTime = buf.st_mtime;
                    }
                    alog_unlock();
                }
            }
        } else {
            alog_lock();
            gettimeofday( &tv , NULL );
            /* if share memory key not exists , give up updating */
            if ( g_alog_ctx->statusless == 0 ){
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
            }
        }
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
    char                mylogbasepath[ALOG_FILEPATH_LEN+1]; 
    memset( myregname , 0x00 , sizeof(myregname));
    memset( mycstname , 0x00 , sizeof(mycstname));
    memset( mylogbasepath , 0x00 , sizeof(mylogbasepath));
    strcpy( myregname , myarg->regName );
    strcpy( mycstname , myarg->cstName );
    strcpy( mylogbasepath , myarg->logBasePath );

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
    alog_buffer_t       *buffer = getBufferByName( myregname  , mycstname , mylogbasepath);
    if ( buffer == NULL ){
        ALOG_DEBUG("fail to get buffer");
        return NULL;
    }
    alog_bufNode_t      *node = NULL;
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

        ALOG_DEBUG("check node[%d]" , node->index);
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
        } else if ( node->usedFlag == ALOG_NODE_USED ){
            /*
            * 4. if current node is USED , then set current node to FULL and do persistence
            * */
            ALOG_DEBUG("node[%d] is USED , set status to FULL" , node->index);
            node->usedFlag = ALOG_NODE_FULL;
        } else {
            ALOG_DEBUG("node[%d] is FULL" , node->index);
        }
        alog_unlock();

        /* 
         * 5. persist current node
         * */
        ALOG_DEBUG("persisting node[%d]" , node->index );
        alog_persist( myregname , mycstname , buffer->logBasePath , node );
        ALOG_DEBUG("node[%d] has been written to file" , node->index );

        /* 
         * 6. update node status
         * */
        alog_lock();
        ALOG_DEBUG("set node[%d] to FREE",node->index);
        node->usedFlag = ALOG_NODE_FREE;
        memset( node->content , 0x0 ,  node->len );
        node->offset = 0;
        buffer->consPtr = node->next;
        if ( buffer->prodPtr == node ){
            buffer->prodPtr = node->next;
        }
        alog_unlock();
    }

    /* rename log file before quit */
    alog_regCfg_t *cfg = getRegByName( g_alog_ctx->l_shm , myregname );
    if ( cfg->backupAfterQuit ){
        if ( cfg != NULL ){
            alog_backupLog( cfg , myregname , mycstname  , buffer->logBasePath);
        }
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
int alog_persist( char *regname , char *cstname , char *logbasepath , alog_bufNode_t *node)
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
    getFileNameFromFormat( ALOG_CURFILEFORMAT , cfg , regname , cstname , logbasepath , filePath);

    FILE *fp = fopen( filePath , "a+");
    fwrite(node->content , node->offset , 1 , fp);

    /* judge if file size over limit */
    long filesize = ftell(fp);
    if ( filesize > cfg->maxSize*1024*1024 ){
        alog_backupLog( cfg , regname , cstname , logbasepath);
    }
    fclose(fp);
    return ALOGOK;
}
/*
 *
 * */
void alog_backupLog( alog_regCfg_t *cfg , char *regname , char *cstname , char *logbasepath)
{
    char filePath[ALOG_FILEPATH_LEN];
    char bak_filePath[ALOG_FILEPATH_LEN];
    char command[ALOG_COMMAND_LEN];
    memset(filePath , 0x00 , sizeof(filePath));
    memset(bak_filePath , 0x00 , sizeof(bak_filePath));
    memset(command , 0x00 , sizeof(command));
    getFileNameFromFormat( ALOG_CURFILEFORMAT , cfg , regname , cstname , logbasepath , filePath);
    getFileNameFromFormat( ALOG_BAKFILEFORMAT , cfg , regname , cstname , logbasepath , bak_filePath);
    sprintf( command , "mv %s %s" , filePath , bak_filePath);
    system( command );
    return ;
}
/*
 *  get log file name 
 * */
void getFileNameFromFormat( int type  , alog_regCfg_t *cfg , char *regname , char *cstname , char *logbasepath , char filePath[ALOG_FILEPATH_LEN] )
{
    char        *p =  NULL;
    int         i = 0;
    int         len = 0;

    strcpy( filePath , logbasepath );
    len = strlen( logbasepath );
    if ( filePath[ len - 1 ] != '/' ) {
        filePath[ len ] = '/';
        i = len + 1;
    } else {
        i = len;
    }

    if ( type == ALOG_CURFILEFORMAT ){
        p = cfg->curFileNamePattern;
    } else if ( type == ALOG_BAKFILEFORMAT ){
        p = cfg->bakFileNamePattern;
    }

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
        
        /* col1 : regname */
        if (get_bracket(line , 1 , buf , ALOG_CFGBUF_LEN)){
            fclose(fp);
            return NULL;
        }
        strncpy( cfg->regName , buf , ALOG_REGNAME_LEN);

        /* col2 : log level */
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

        /* col3 : file size limit */
        if (get_bracket(line , 3 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        cfg->maxSize = atoi(buf);

        /* col4 : log prefix pattern */
        if (get_bracket(line , 4 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        strncpy( cfg->format , buf , 7 );

        char cmd[ALOG_COMMAND_LEN];
        FILE *fp_cmd = NULL;

        /* col5 :  default log base path */
        if (get_bracket(line , 5 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        strcpy( cfg->defLogBasePath_r , buf );
        memset( cmd , 0x00 , ALOG_COMMAND_LEN);
        sprintf( cmd , "echo %s" , buf);
        fp_cmd = popen( cmd , "r");
        if ( fp_cmd ){
            fgets(cfg->defLogBasePath , ALOG_FILEPATH_LEN , fp_cmd);
            if ( cfg->defLogBasePath[strlen(cfg->defLogBasePath)-1] == '\n' )
                cfg->defLogBasePath[strlen(cfg->defLogBasePath)-1] = '\0';
            pclose(fp_cmd);
        } else {
            strncpy(cfg->defLogBasePath ,buf , ALOG_CFGBUF_LEN);
        }

        /*  col6 : current file name patterm */
        if (get_bracket(line , 6 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        strcpy( cfg->curFileNamePattern_r , buf );
        memset( cmd , 0x00 , ALOG_COMMAND_LEN);
        sprintf( cmd , "echo %s" , buf);
        fp_cmd = popen( cmd , "r");
        if ( fp_cmd ){
            fgets(cfg->curFileNamePattern , ALOG_FILEPATH_LEN , fp_cmd);
            if ( cfg->curFileNamePattern[strlen(cfg->curFileNamePattern)-1] == '\n' )
                cfg->curFileNamePattern[strlen(cfg->curFileNamePattern)-1] = '\0';
            pclose(fp_cmd);
        } else {
            strncpy(cfg->curFileNamePattern ,buf , ALOG_CFGBUF_LEN);
        }

        /*  col7 : backup file name patterm */
        if (get_bracket(line , 7 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        strcpy( cfg->bakFileNamePattern_r , buf );
        memset( cmd , 0x00 , ALOG_COMMAND_LEN);
        sprintf( cmd , "echo %s" , buf);
        fp_cmd = popen( cmd , "r");
        if ( fp_cmd ){
            fgets(cfg->bakFileNamePattern , ALOG_FILEPATH_LEN , fp_cmd);
            if ( cfg->bakFileNamePattern[strlen(cfg->bakFileNamePattern)-1] == '\n' )
                cfg->bakFileNamePattern[strlen(cfg->bakFileNamePattern)-1] = '\0';
            pclose(fp_cmd);
        } else {
            strncpy(cfg->bakFileNamePattern ,buf , ALOG_CFGBUF_LEN);
        }

        /*  col8 : force backup after quit */
        if (get_bracket(line , 8 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        cfg->backupAfterQuit = atoi(buf);

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
