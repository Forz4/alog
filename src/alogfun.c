#include "alogfun.h"
/*
 *  通过注册名获取配置
 * */
alog_regCfg_t *getRegByName(alog_shm_t *shm , char *regname)
{
    ALOG_DEBUG( "进入getRegByName");
    if ( strlen(regname) == 0 || strlen(regname) > ALOG_REGNAME_LEN ){
        return NULL;
    }
    int i = 0 ;
    for ( i = 0 ; i < shm->regNum ; i ++ ){
        if ( strcmp( shm->regCfgs[i].regName , regname ) == 0 ){
            return &(shm->regCfgs[i]);
        }
    }
    ALOG_DEBUG( "getRegByName完成");
    return NULL;
}
/* 
 * 通过注册名获取缓冲区 
 * */
alog_buffer_t *getBufferByName( char *regname , char *cstname)
{
    ALOG_DEBUG( "进入getBufferByName,regname[%s],cstname[%s]",regname,cstname);
    if ( strlen(regname) == 0 || strlen(regname) > ALOG_REGNAME_LEN ||\
            strlen(cstname) == 0 || strlen(cstname) > ALOG_CSTNAME_LEN ){
        return NULL;
    }
    int i = 0;
    for ( i = 0 ; i < g_alog_ctx->bufferNum ; i ++){
    ALOG_DEBUG( "ctx->buffers[%d],regname[%s],cstname[%s]",i,g_alog_ctx->buffers[i].regName,g_alog_ctx->buffers[i].cstName);
        if ( strcmp( g_alog_ctx->buffers[i].regName , regname ) == 0 && \
                strcmp( g_alog_ctx->buffers[i].cstName , cstname ) == 0 ){
            ALOG_DEBUG("找到buffer配置项[%s][%s]" , regname , cstname);
            /* 
             * 判断持久化线程是否存在
             * 主要针对父进程fork之后，buffer信息存在但是持久化线程不在的情况
             * */
            if ( pthread_kill(g_alog_ctx->buffers[i].consTid,0) ){
                ALOG_DEBUG("buffer存在但是持久化线程已退出");
                alog_persist_arg_t  *arg = (alog_persist_arg_t *)malloc(sizeof(alog_persist_arg_t));
                strcpy( arg->regName , regname);
                strcpy( arg->cstName , cstname);
                pthread_create(&(g_alog_ctx->buffers[i].consTid), NULL, alog_persist_thread, (void *)arg );
                ALOG_DEBUG("重新启动持久化线程");
            }
            if ( pthread_kill(g_alog_ctx->updTid,0) ){
                ALOG_DEBUG("持久化更新线程不存在");
                pthread_create(&(g_alog_ctx->updTid), NULL, alog_update_thread, NULL );
                ALOG_DEBUG("重新启动持久化更新线程");
            }
            return &(g_alog_ctx->buffers[i]);
        }
    }
    ALOG_DEBUG( "getBufferByName完成");
    return NULL;
}
/*
 * 上锁操作
 * */
int alog_lock()
{
    ALOG_DEBUG("尝试加锁");
    pthread_mutex_lock(&(g_alog_ctx->mutex));
    ALOG_DEBUG("加锁成功");
    return 0;
}
/* 
 * 解锁操作
 * */
int alog_unlock()
{
    ALOG_DEBUG("尝试解锁");
    pthread_mutex_unlock(&(g_alog_ctx->mutex));
    ALOG_DEBUG("解锁成功");
    return 0;
}
/* 
 * 更新timer时间 
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
 *  为注册名初始化缓冲区(调用前需要完成加锁操作)
 * */
int alog_addBuffer( char *regname  , char *cstname , alog_buffer_t **retbuffer)
{
    if ( g_alog_ctx->bufferNum >= ALOG_BUFFER_NUM ){
        return ALOGERR_BUFFERNUM_OVERFLOW;
    }

    if ( strlen(regname) == 0 || strlen(regname) > ALOG_REGNAME_LEN ||\
            strlen(cstname) == 0 || strlen(cstname) > ALOG_CSTNAME_LEN ){
        return ALOGERR_REGCST_INVALID;
    }
    /* 获取配置项信息 */
    alog_regCfg_t       *regCfg;
    if ( (regCfg = getRegByName( g_alog_ctx->l_shm , regname )) == NULL ){
        ALOG_DEBUG("未找到注册名[%s]" , regname);
        return ALOGMSG_REG_NOTFOUND;
    }
    /* 检查缓冲区是否已经打开 */
    alog_buffer_t       *buffer;
    if ( (buffer = getBufferByName( regname , cstname )) != NULL ){
        ALOG_DEBUG("缓冲区已经存在 regname[%s] cstname[%s]" , regname , cstname);
        return ALOGOK;
    }

    /* 新增缓冲区*/
    buffer = &(g_alog_ctx->buffers[g_alog_ctx->bufferNum]);
    g_alog_ctx->bufferNum ++;
    strncpy( buffer->regName , regname , sizeof(buffer->regName) );
    strncpy( buffer->cstName , cstname , sizeof(buffer->cstName) );

    /* 初始化第一个内存块 */
    buffer->nodeNum = 1;
    alog_bufNode_t    *node = NULL;
    node = ( alog_bufNode_t *)malloc( sizeof(alog_bufNode_t) );
    if ( node == NULL )
    {
        ALOG_DEBUG("malloc [alog_bufNode_t] 失败");
        return ALOGERR_MALLOC_FAIL;
    }
    buffer->prodPtr = node;
    buffer->consPtr = node;

    node->index = buffer->nodeNum;
    node->usedFlag = ALOG_NODE_FREE;
    node->content = (char *)malloc(g_alog_ctx->l_shm->singleBlockSize*1024);
    if( node->content == NULL ){
        ALOG_DEBUG("malloc 失败");
        return ALOGERR_MALLOC_FAIL;
    }
    node->offset = 0;
    node->next = node;
    node->prev = node;

    /* 启动后台持久化线程*/
    alog_persist_arg_t  *arg = (alog_persist_arg_t *)malloc(sizeof(alog_persist_arg_t));
    strcpy( arg->regName , regname);
    strcpy( arg->cstName , cstname);
    pthread_create(&(buffer->consTid), NULL, alog_persist_thread, (void *)arg );
    
    *retbuffer = buffer;
    return ALOGOK;
}
/*
 * 定时更新线程函数
 * */
void *alog_update_thread(void *arg)
{
    ALOG_DEBUG("进入alog_update_thread");
    struct          timeval  tv;
    int             shmid = 0;
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
        /* 如果共享内存key不存在则放弃更新本地缓存 */
        if ( (shmid = shmget( g_alog_ctx->l_shm->shmKey , sizeof(alog_shm_t) , 0 )) > 0 ){
            ALOG_DEBUG("共享内存key存在");
            /* 如果共享内存id发生变化，相当于key被重新加载过，需要重新挂载内存*/
            if ( shmid != g_alog_ctx->l_shm->shmId ){
                ALOG_DEBUG("共享内存id发生变化，重新挂载共享内存地址");
                g_alog_ctx->g_shm = (alog_shm_t *)shmat( shmid , NULL , 0 );
            }
            /* 判断全局配置更新标志 */
            if ( g_alog_ctx->l_shm->updTime != g_alog_ctx->g_shm->updTime ){
                ALOG_DEBUG("共享内存有更新，同步配置，updTime[%ld]" , g_alog_ctx->g_shm->updTime);
                memcpy( g_alog_ctx->l_shm , g_alog_ctx->g_shm , sizeof( alog_shm_t) ) ;
            }
        }
        alog_unlock();
        sleep(g_alog_ctx->l_shm->checkInterval);
    }
    return NULL;
}
/*
 *  持久化线程函数
 * */
void *alog_persist_thread(void *arg)
{
    ALOG_DEBUG("进入 alog_persist_thread 函数");

    /* 获取参数 */
    alog_persist_arg_t  *myarg = ( alog_persist_arg_t *)arg;
    ALOG_DEBUG("arg->regname[%s]" , myarg->regName);
    ALOG_DEBUG("arg->cstname[%s]" , myarg->cstName);

    char                myregname[20+1];
    char                mycstname[20+1];
    strcpy( myregname , myarg->regName );
    strcpy( mycstname , myarg->cstName );

    /* 信号处理 */
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

    /* 主循环逻辑 */
    alog_buffer_t       *buffer = getBufferByName( myregname  , mycstname);
    if ( buffer == NULL ){
        ALOG_DEBUG("获取buffer指针失败");
        return NULL;
    }
    alog_bufNode_t      *node = NULL;
    struct timespec     abstime;
    struct timeval      timeval;
    int                 ret = 0;
    while ( 1 ){
        alog_lock();
        node = buffer->consPtr;
        ALOG_DEBUG("持久化线程处于节点[%d]" , node->index);
        /* 1. 当前节点未写满，则等待1s或被唤醒 , 如果检查到结束标志则直接进行持久化操作 */
        if ( g_alog_ctx->closeFlag != 1 && node->usedFlag != ALOG_NODE_FULL ){
            ALOG_DEBUG("当前节点不满");
            gettimeofday(&timeval , NULL);
            abstime.tv_sec = timeval.tv_sec + g_alog_ctx->l_shm->flushInterval;
            abstime.tv_nsec =  timeval.tv_usec * 1000;
            ALOG_DEBUG("开始执行 pthread_cond_timedwait 函数");
            ret = pthread_cond_timedwait(&(g_alog_ctx->cond_persist) , &(g_alog_ctx->mutex) , &abstime);
            ALOG_DEBUG("pthread_cond_timedwait 执行结束 ret[%d]" , ret);
            /* 2. 超时或者被唤醒，尝试进行持久化 */
        }
        /* 3. 再次判断节点状态，如仍为空则解锁重试 */
        if ( node->usedFlag == ALOG_NODE_FREE ){
            ALOG_DEBUG("当前节点为空");
            alog_unlock();
            /* 当前节点为空且结束标志为1时退出线程 */
            if ( g_alog_ctx->closeFlag == 1 ){
                ALOG_DEBUG("监测到结束标志，线程开始退出");
                break;
            } else {
                ALOG_DEBUG("未监测到结束标志，线程继续循环处理");
                continue;
            }
        }
        /* 4. 如节点中有日志，则将节点状态设置为FULL，并开始持久化操作 */

        ALOG_DEBUG("设置节点[%d]状态为FULL",node->index);
        node->usedFlag = ALOG_NODE_FULL;
        alog_unlock();

        /* 5. 将日志块输出到文件 */
        ALOG_DEBUG("开始对节点[%d]进行持久化",node->index);
        alog_persist( myregname , mycstname , node );
        ALOG_DEBUG("节点[%d]持久化完成",node->index);

        /* 6. 更新节点状态 */
        alog_lock();
        ALOG_DEBUG("设置节点[%d]状态为FREE",node->index);
        node->usedFlag = ALOG_NODE_FREE;
        memset( node->content , 0x00 ,  g_alog_ctx->l_shm->singleBlockSize*1024 );
        node->offset = 0;
        buffer->consPtr = node->next;
        ALOG_DEBUG("持久化线程切换到节点[%d]",buffer->consPtr->index);
        if ( buffer->prodPtr == node ){
            ALOG_DEBUG("生产者线程指针切换到节点[%d]",buffer->consPtr->index);
            buffer->prodPtr = node->next;
        }
        alog_unlock();
    }
    /* 持久化线程退出清理工作 */
    ALOG_DEBUG("线程开始退出清理流程");
    int i = 0 ;
    alog_bufNode_t  *temp = node;
    for( i = 0 ; i < buffer->nodeNum ; i ++ ){
        temp = node->next;
        free(node->content);
        free(node);
        node = temp;
    }
    ALOG_DEBUG("持久化线程退出");
    return NULL;
}
/*
 * 持久化操作
 * */
int alog_persist( char *regname , char *cstname , alog_bufNode_t *node)
{
    /* 获取当前配置信息 */
    alog_regCfg_t *cfg = getRegByName( g_alog_ctx->l_shm , regname );
    if ( cfg == NULL ){
        return -1;
    }
    char filepath[ALOG_FILEPATH_LEN];
    memset(filepath , 0x00 , sizeof(filepath));

    /* 日志名格式REGNAME.CSTNAME.YYYYMMDD */
    alog_update_timer();
    sprintf( filepath , "%s/%s.%s.%s" , cfg->filePath , regname , cstname , g_alog_ctx->timer.date);
    FILE *fp = fopen( filepath , "a+");
    fwrite(node->content , node->offset , 1 , fp);

    /* 判断文件大小 */
    long filesize = ftell(fp);
    if ( filesize > cfg->maxSize*1024*1024 ){
        char bak_filepath[ALOG_FILEPATH_LEN];
        char command[ALOG_COMMAND_LEN];
        memset(bak_filepath , 0x00 , sizeof(bak_filepath));
        memset(command , 0x00 , sizeof(command));
        sprintf( bak_filepath , "%s.%02d%02d%02d%06d" ,\
                filepath ,\
                g_alog_ctx->timer.tmst.tm_hour,\
                g_alog_ctx->timer.tmst.tm_min,\
                g_alog_ctx->timer.tmst.tm_sec,\
                g_alog_ctx->timer.tv.tv_usec);
        sprintf( command , "mv %s %s" , filepath , bak_filepath);
        system( command );
    }
    fclose(fp);
    return ALOGOK;
}
/*
 * 读取中括号内容
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
 * 从配置文件和环境变量中加载
 * */
alog_shm_t *alog_loadCfg( char *filepath )
{
    char    line[ALOG_FILEPATH_LEN];
    char    buf[ALOG_CFGBUF_LEN];
    int     temp = 0;

    /* 分配空间 */
    alog_shm_t *l_shm = (alog_shm_t *)malloc(sizeof(alog_shm_t));
    if ( l_shm == NULL ){
        return l_shm;
    }
    l_shm->regNum = 0;

    /* 打开日志文件 */
    FILE *fp = fopen( filepath , "r");
    if ( fp == NULL ){
        perror("打开配置文件[%s]失败\n");
        return NULL;
    }
    /* 读配置文件 */
    while (fgets(line , sizeof(line), fp) != NULL){

        if ( line[0] == '\n' || line[0] == '#' )
            continue;
        
        /* 超出配置项个数限制 */
        if ( l_shm->regNum >= ALOG_REG_NUM ){
            break;
        }
        
        alog_regCfg_t   *cfg = &(l_shm->regCfgs[l_shm->regNum]);
        
        /* 注册名 */
        if (get_bracket(line , 1 , buf , ALOG_CFGBUF_LEN)){
            fclose(fp);
            return NULL;
        }
        strncpy( cfg->regName , buf , ALOG_REGNAME_LEN);

        /* 日志级别 */
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

        /* 单个文件最大尺寸 */
        if (get_bracket(line , 3 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        cfg->maxSize = atoi(buf);

        /* 打印格式 */
        if (get_bracket(line , 4 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        strncpy( cfg->format , buf , 7 );

        /* 日志文件路径 */
        if (get_bracket(line , 5 , buf , ALOG_CFGBUF_LEN )){
            fclose(fp);
            return NULL;
        }
        /* 处理环境变量 */
        char cmd[ALOG_COMMAND_LEN];
        memset( cmd , 0x00 , ALOG_COMMAND_LEN);
        sprintf( cmd , "echo %s" , buf);
        FILE *fp_cmd = popen( cmd , "r");
        if ( fp_cmd ){
            fgets(cfg->filePath , ALOG_FILEPATH_LEN , fp_cmd);
            if ( cfg->filePath[strlen(cfg->filePath)-1] == '\n' )
                cfg->filePath[strlen(cfg->filePath)-1] = '\0';
            pclose(fp_cmd);
        } else {
            strncpy(cfg->filePath ,buf , ALOG_CFGBUF_LEN);
        }
        if ( access( cfg->filePath , F_OK ) ){
            return NULL;
        }
        l_shm->regNum ++;
    }
    fclose(fp);

    l_shm->maxMemorySize = ALOG_DEF_MAXMEMORYSIZE;
    l_shm->singleBlockSize = ALOG_DEF_SINGLEBLOCKSIZE;
    l_shm->flushInterval = ALOG_DEF_FLUSHINTERVAL; 
    l_shm->checkInterval = ALOG_DEF_CHECKINTERVAL; 
    /* 从环境变量加载 */
    if ( getenv("ALOG_MAXMEMORYSIZE") ){
        temp = atoi(getenv("ALOG_MAXMEMORYSIZE"));
        if ( temp > 0 && temp <= 20 ){
            l_shm->maxMemorySize = temp;
        } 
    }
    if ( getenv("ALOG_SINGLEBLOCKSIZE") ){
        temp = atoi(getenv("ALOG_SINGLEBLOCKSIZE"));
        if ( temp > 0 && temp <= 8 ){
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
