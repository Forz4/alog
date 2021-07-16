#include "alogfun.h"
/**
 * [getRegByName   Get configs]
 * @param  shm     [find regname in shm->regCfgs]
 * @param  regname [regname to find]
 * @return         [pointer to regcfg]
 */
alog_regCfg_t *getRegByName(alog_shm_t *shm , char *regname)
{
    if ( regname == NULL || strlen(regname) == 0 || strlen(regname) > ALOG_REGNAME_LEN ){
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
/**
 * [getBufferByName    Get buffer by regname+cstname+logfilepath]
 * @param  regname     [input regname]
 * @param  cstname     [input cstname]
 * @param  logfilepath [input logfilepath]
 * @return             [pointer to buffer]
 */
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

            /**
             * if input logfilepath , then input path with current logBasePath
             */
            if ( logfilepath && strlen(logfilepath) ) {
                if ( strcmp( logfilepath , g_alog_ctx->buffers[i].logBasePath ) ){
                    continue;
                }
            } else {
                /**
                 * if no path input , then decide if default log path is created
                 */
                if ( g_alog_ctx->buffers[i].isDefaultPath == 0 ) {
                    continue;
                }
            }
            /**
             * if regname+cstname+logfilepath locates a buffer , then 
             * judge if persist thread exist , especially in the case of fork situation
             * because after fork , children process only get buffer memory but no 
             * persist thread.
             */
            if ( pthread_kill(g_alog_ctx->buffers[i].consTid,0) ){
                ALOG_DEBUG("buffer exist but persist thread lost , then empty current buffer");
                alog_bufNode_t  *temp;
                alog_bufNode_t  *node;
                node = g_alog_ctx->buffers[i].prodPtr;;
                int j = 0;
                for( j = 0 ; j < g_alog_ctx->buffers[i].nodeNum ; j ++ ){
                    temp = node->next;
                    free(node->content);
                    free(node);
                    node = temp;
                }
                ALOG_DEBUG("delete current buffer by set regname and cstname to 0x00 ");
                memset( g_alog_ctx->buffers[i].regName , 0x0 , ALOG_REGNAME_LEN + 1 );
                memset( g_alog_ctx->buffers[i].cstName , 0x0 , ALOG_CSTNAME_LEN + 1 );
                /**
                 * check if update thread exists
                 */
                if ( pthread_kill(g_alog_ctx->updTid,0) ){
                    ALOG_DEBUG("update thread lost , start to recreate update thread");
                    pthread_create(&(g_alog_ctx->updTid), NULL, alog_update_thread, NULL );
                }
                /**
                 * break out for loop
                 */
                break;
            }

            return &(g_alog_ctx->buffers[i]);
        }
    }
    /**
     * if buffer not found then create a new one
     */
    alog_buffer_t *buffer = NULL;
    if ( alog_addBuffer( regname , cstname , logfilepath , &buffer) == 0 ){
        return buffer;
    } else {
        return NULL;
    }
}
/**
 * [alog_lock Lock mutex]
 * @return [0]
 */
int alog_lock()
{
    pthread_mutex_lock(&(g_alog_ctx->mutex));
    return 0;
}
/**
 * [alog_unlock Unlock mutex]
 * @return [0]
 */
int alog_unlock()
{
    pthread_mutex_unlock(&(g_alog_ctx->mutex));
    return 0;
}
/**
 * [alog_update_timer Update global timer]
 */
void alog_update_timer( struct timeval tv )
{
    time_t t = tv.tv_sec;
    struct tm *temp = localtime( &t );

    sprintf( g_alog_ctx->timer.date , "%4d%02d%02d" , temp->tm_year+1900 , temp->tm_mon + 1 , temp->tm_mday);
    sprintf( g_alog_ctx->timer.time , "%02d:%02d:%02d" ,temp->tm_hour ,temp->tm_min ,temp->tm_sec);

    g_alog_ctx->timer.sec = tv.tv_sec;
    memcpy(&(g_alog_ctx->timer.tv) , &tv , sizeof(struct timeval) );
    memcpy(&(g_alog_ctx->timer.tmst) , temp , sizeof( struct tm) );
    return ;
}
/**
 * [alog_addBuffer Add a new buffer]
 * @param  regname     [input regname]
 * @param  cstname     [input cstname]
 * @param  logfilepath [input logfilepath]
 * @param  retbuffer   [pointer to new buffer]
 * @return             [0 for success]
 */
int alog_addBuffer( char *regname  , char *cstname , char *logfilepath  , alog_buffer_t **retbuffer)
{
    /**
     * judge if total number exceeds limit
     */
    if ( g_alog_ctx->bufferNum + 1 > ALOG_BUFFER_NUM ){
        return ALOGERR_BUFFERNUM_OVERFLOW;
    }

    /**
     * check input
     */
    if ( regname == NULL || strlen(regname) == 0 || strlen(regname) > ALOG_REGNAME_LEN ||\
            strlen(cstname) == 0 || strlen(cstname) > ALOG_CSTNAME_LEN ){
        return ALOGERR_REGCST_INVALID;
    }
    /**
     * get config by regname
     */
    alog_regCfg_t       *regCfg;
    if ( (regCfg = getRegByName( g_alog_ctx->l_shm , regname )) == NULL ){
        ALOG_DEBUG("regname[%s] not found" , regname);
        return ALOGMSG_REG_NOTFOUND;
    }

    /**
     * ensure logfilepath exists
     */
    if ( (logfilepath  && strlen(logfilepath) && alog_mkdir( logfilepath )) ||\
            ( alog_mkdir( regCfg->defLogBasePath) ) ){
        return ALOGERR_MKDIR_FAIL;
    }

    /**
     * add new buffer
     */
    ALOG_DEBUG("start to create buffer for regname[%s] cstname[%s]" , regname , cstname);
    alog_buffer_t *buffer = &(g_alog_ctx->buffers[g_alog_ctx->bufferNum]);
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

    /**
     * alloc space for the first node
     */
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

    /**
     * create persist thread
     */
    alog_persist_arg_t  *arg = &(buffer->arg);
    strcpy( arg->regName , regname);
    strcpy( arg->cstName , cstname);
    strcpy( arg->logBasePath , buffer->logBasePath);

    pthread_create(&(buffer->consTid), NULL, alog_persist_thread, (void *)arg );
    *retbuffer = buffer;
    
    return ALOGOK;
}
/**
 * [alog_mkdir Create directory if not exists]
 * @param  dir [input direcotry]
 * @return     [0 for success]
 */
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
                if( mkdir( DirName , 0777 ) == -1 )
                    return ALOGERR_MKDIR_FAIL;
            }
            DirName[i] = '/';
        }
    }

    return ALOGOK;
}
/**
 * [alog_update_thread Background thread to check config update]
 * @param  arg [NULL]
 * @return     [NULL]
 */
void *alog_update_thread(void *arg)
{

#ifdef ALOG_ASYNC_MODE
    ALOG_DEBUG("update thread start in NON-SYNC MODE");
    pthread_detach(pthread_self());
#else
    ALOG_DEBUG("update thread start in SYNC MODE");
#endif

    struct          timeval  tv;
    int             shmid = 0;

    /**
     * block signals
     */
    sigset_t            sigset;
    sigfillset(&sigset);
    pthread_sigmask(SIG_BLOCK , &sigset , NULL);
    
    while ( g_alog_ctx->closeFlag != 1 ){
        /**
         * if in statusless mode , then check configfile last modified time
         */
        if ( g_alog_ctx->statusless == 1 ){
            alog_shm_t *shm = NULL;
            char cfgFile[ALOG_FILEPATH_LEN+1];
            memset( cfgFile , 0x00 , sizeof(cfgFile) );
            sprintf( cfgFile , "%s/cfg/alog.cfg" , getenv("ALOG_HOME"));
            struct stat buf;
            if ( stat( cfgFile , &buf) == 0 ){
                if ( buf.st_mtime != g_alog_ctx->l_shm->updTime ){
                    alog_lock();
                    shm = alog_loadCfg( cfgFile );
                    if ( shm == NULL ){
                        ALOG_DEBUG("fail to load config");
                    } else {
                        memcpy( g_alog_ctx->l_shm , shm , sizeof(alog_shm_t) );
                        g_alog_ctx->l_shm->updTime = buf.st_mtime;
                        free(shm);
                    }
                    alog_unlock();
                }
            } else {
                ALOG_DEBUG("fail to stat file %s" , cfgFile);
            }
        } else {
            /**
             * if not in statusless mode , then check updTime with share memory
             */
            alog_lock();
            gettimeofday( &tv , NULL );
            /**
             * if share memory key not exists , give up updating
             */
            if ( (shmid = shmget( g_alog_ctx->l_shm->shmKey , sizeof(alog_shm_t) , 0 )) > 0 ){
                /**
                 * if shmid changes , it means share memory was recreated ,
                 * then reattach to share memory
                 */
                if ( shmid != g_alog_ctx->l_shm->shmId ){
                    ALOG_DEBUG("shmid changes , reattach to share memory");
                    g_alog_ctx->g_shm = (alog_shm_t *)shmat( shmid , NULL , 0 );
                }
                /**
                 * check and update updTime
                 */
                if ( g_alog_ctx->l_shm->updTime != g_alog_ctx->g_shm->updTime ){
                    ALOG_DEBUG("share memory was updated , sync config , updTime[%ld]" , g_alog_ctx->g_shm->updTime);
                    memcpy( g_alog_ctx->l_shm , g_alog_ctx->g_shm , sizeof( alog_shm_t) ) ;
                }
            }
            alog_unlock();
        }
        sleep(g_alog_ctx->l_shm->checkInterval);
    }
    return NULL;
}
/**
 * [alog_persist_thread Background thread for persisting]
 * @param  arg [regname , cstname , logfilepath]
 * @return     [NULL]
 */
void *alog_persist_thread(void *arg)
{

#ifdef ALOG_ASYNC_MODE
    ALOG_DEBUG("persist thread start in NON-SYNC MODE");
    pthread_detach(pthread_self());
#else
    ALOG_DEBUG("persist thread start in SYNC MODE");
#endif

    /**
     * get parameter
     */
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

    /**
     * block signals
     */
    sigset_t            sigset;
    sigfillset(&sigset);
    pthread_sigmask(SIG_BLOCK , &sigset , NULL);

    /**
     * get buffer by regname+cstname+logfilepath
     */
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
        /**
         *  if current node not FULL , 
         *  then sleep ${FLUSHINTERVAL} seconds 
         *  quit when closeFlag is set
         */
        if ( g_alog_ctx->closeFlag != 1 && node->usedFlag != ALOG_NODE_FULL ){

            ALOG_DEBUG("current node [%d] not FULL" , node->index);
            gettimeofday(&timeval , NULL);
            abstime.tv_sec = timeval.tv_sec + g_alog_ctx->l_shm->flushInterval;
            abstime.tv_nsec =  timeval.tv_usec * 1000;
            ALOG_DEBUG("start to  pthread_cond_timedwait");
            ret = pthread_cond_timedwait(&(g_alog_ctx->cond_persist) , &(g_alog_ctx->mutex) , &abstime);
            ALOG_DEBUG("wake from pthread_cond_timedwait ret[%d]" , ret);
            /**
             * thread wait timeout or wake by other threads
             * then try to persist current node
             ad*/
        }

        ALOG_DEBUG("check node[%d]" , node->index);
        /**
         * check current node again , if still FREE  then unlock and retry
         */
        if ( node->usedFlag == ALOG_NODE_FREE ){
            ALOG_DEBUG("node[%d] is FREE" , node->index);
            alog_unlock();
            /**
             * if current node is FREE and closeFlag is set , then quit
             */
            if ( g_alog_ctx->closeFlag == 1 ){
                ALOG_DEBUG("closeFlag set , persist thread quitting");
                break;
            } else {
                ALOG_DEBUG("closeFlag not set , continue ");
                continue;
            }
        } else if ( node->usedFlag == ALOG_NODE_USED ){
            /**
             * if current node is USED , then set current node to FULL and do persistence
             */
            ALOG_DEBUG("node[%d] is USED , set status to FULL" , node->index);
            node->usedFlag = ALOG_NODE_FULL;
        } else {
            ALOG_DEBUG("node[%d] is FULL" , node->index);
        }
        alog_unlock();

        /**
         * persist current node
         */
        ALOG_DEBUG("persisting node[%d]" , node->index );
        alog_persist( myregname , mycstname , buffer->logBasePath , node );
        ALOG_DEBUG("node[%d] has been written to file" , node->index );

        /**
         * update node status
         */
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

    /**
     * rename log file before quit
     */
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
/**
 * [alog_persist Write log to file]
 * @param  regname     [input regname]
 * @param  cstname     [input cstname]
 * @param  logbasepath [input logbasepath]
 * @param  node        [node to persist]
 * @return             [0 for success
 */
int alog_persist( char *regname , char *cstname , char *logbasepath , alog_bufNode_t *node)
{
    /**
     * get config by regname
     */
    alog_regCfg_t *cfg = getRegByName( g_alog_ctx->l_shm , regname );
    if ( cfg == NULL ){
        return -1;
    }
    char filePath[ALOG_FILEPATH_LEN];
    memset(filePath , 0x00 , sizeof(filePath));

    /**
     * get log file name
     */
    getFileNameFromFormat( ALOG_CURFILEFORMAT , cfg , regname , cstname , logbasepath , filePath);

    FILE *fp = fopen( filePath , "a+");
    fwrite(node->content , node->offset , 1 , fp);

    /**
     * judge if file size over limit
     */
    long filesize = ftell(fp);
    if ( filesize > cfg->maxSize*1024*1024 ){
        alog_backupLog( cfg , regname , cstname , logbasepath);
    }
    fclose(fp);
    return ALOGOK;
}
/**
 * [alog_backupLog Back up current log file]
 * @param cfg         [regcfg]
 * @param regname     [input regname]
 * @param cstname     [input cstname]
 * @param logbasepath [logbasepath]
 */
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
/**
 * get log file name 
 */
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
/**
 * read from brackets
 */
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
/**
 * load configs from environment and file
 */
alog_shm_t *alog_loadCfg( char *filepath )
{
    char    line[ALOG_FILEPATH_LEN];
    char    buf[ALOG_CFGBUF_LEN];
    int     temp = 0;

    /**
     * allocate space for share memory
     */
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
        
        /**
         * col1 : regname
         */
        if (get_bracket(line , 1 , buf , ALOG_CFGBUF_LEN)){
            fclose(fp);
            return NULL;
        }
        strncpy( cfg->regName , buf , ALOG_REGNAME_LEN);

        /**
         * col2 : log level
         */
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

        /**
         * col3 : file size limit
         */
        if (get_bracket(line , 3 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        cfg->maxSize = atoi(buf);

        /**
         * col4 : log prefix pattern
         */
        if (get_bracket(line , 4 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        strncpy( cfg->format , buf , 7 );

        char cmd[ALOG_COMMAND_LEN];
        FILE *fp_cmd = NULL;

        /**
         * col5 : default log base path
         */
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

        /**
         * col6 : current file name patterm
         */
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

        /**
         * col7 : backup file name patterm
         */
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

        /**
         * col8 : force backup after quit
         */
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

    /**
     * load from environment
     */
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
/**
 * [alog_cleanContext Clean up context]
 */
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
