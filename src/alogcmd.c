/*
 *   ALOG控制台程序
 * */
#include "alogcmd.h"
int main(int argc , char **argv)
{
    /* 加载环境变量 */
    ENV_ALOG_HOME = getenv("ALOG_HOME");
    if ( ENV_ALOG_HOME == NULL ){
        perror("环境变量[ALOG_HOME]不存在\n");
        exit(1);
    }
    ENV_ALOG_SHMKEY = getenv("ALOG_SHMKEY");
    if ( ENV_ALOG_SHMKEY == NULL ){
        perror("环境变量[ALOG_SHMKEY]不存在\n");
        exit(1);
    }

    /* 判断参数类型 */
    if ( argc != 2 ){
        alogcmd_help();
        exit(1);
    }
    if ( strcmp( argv[1] , "init" ) == 0  || strcmp( argv[1] , "reload" ) == 0 ){
        if ( alogcmd_load(argv[1]) ){
            printf("ALOG %s失败\n",argv[1]);
            exit(1);
        } else {
            alogcmd_print();
        }
    } else if ( strcmp( argv[1] , "print" ) == 0 ){
        alogcmd_print();
    } else if ( strcmp( argv[1] , "close" ) == 0 ){
        if ( alogcmd_close() ){
            printf("ALOG close失败\n");
            exit(1);
        } else {
            printf("ALOG close成功\n");
        }
    } else {
        alogcmd_help();
        exit(1);
    }
    return 0;
}
void alogcmd_help()
{
    printf("alogcmd <init/reload/print/close>\n");
    return;
}
int alogcmd_load(char *type)
{
    int shmkey = atoi(ENV_ALOG_SHMKEY);
    int shmid = 0;

    if ( strcmp( type , "init") == 0 )
        shmid = shmget( shmkey , sizeof(alog_shm_t) , IPC_CREAT|IPC_EXCL|0660);
    else if ( strcmp( type , "reload") == 0 )
        shmid = shmget( shmkey , sizeof(alog_shm_t) , 0);
    if ( shmid < 0 ){
        perror("shmget error");
        return -1;
    }
    alog_shm_t *g_shm = (alog_shm_t *)shmat( shmid , NULL , 0 );
    if ( g_shm == NULL ){
        perror("shmat error");
        return -1;
    }

    /* 从配置文件和环境变量加载配置到内存 */
    char filepath[ALOG_FILEPATH_LEN];
    sprintf( filepath , "%s/cfg/alog.cfg" , ENV_ALOG_HOME);
    alog_shm_t *l_shm = alog_loadCfg( filepath );
    if ( l_shm == NULL ){
        printf("加载配置文件失败\n");
        return -1;
    }
    l_shm->shmKey = shmkey;
    l_shm->shmId = shmid;
    time(&(l_shm->updTime));

    memcpy( g_shm , l_shm , sizeof(alog_shm_t));
    free(l_shm);
    return 0;

}
int alogcmd_print()
{
    int shmkey = atoi(ENV_ALOG_SHMKEY);
    char *format_head = "|%-20s|%-8s|%-11s|%-8s|%-40s|%-40s|\n";
    char *format_body = "|%-20s|%-8s|%-11d|%-8s|%-40s|%-40s|\n";

    int shmid = shmget( shmkey , sizeof(alog_shm_t) , 0);
    if ( shmid < 0 ){
        perror("shmget error");
        return -1;
    }
    alog_shm_t *g_shm = (alog_shm_t *)shmat( shmid , NULL , 0 );
    if ( g_shm == NULL ){
        perror("shmat error");
        return -1;
    }

    struct tm *tm = localtime(&(g_shm->updTime));
    printf("ALOG_HOME Directory:    %s\n",ENV_ALOG_HOME);
    printf("share memory key   :    %d\n",g_shm->shmKey);
    printf("share memory id    :    %d\n",g_shm->shmId);
    printf("缓冲区最大使用量:       %d(MB)\n",g_shm->maxMemorySize);
    printf("单个日志块大小:         %d(KB)\n",g_shm->singleBlockSize);
    printf("持久化线程写入间隔:     %d(s)\n",g_shm->flushInterval);
    printf("配置更新检查间隔:       %d(s)\n",g_shm->checkInterval);
    printf("更新时间戳:             %4d%02d%02d %02d:%02d:%02d\n",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
    printf("配置项个数:             %d\n",g_shm->regNum);
    printf("======================================================================================================================================\n");
    printf( format_head ,\
            "regname",\
            "level",\
            "maxsize(MB)",\
            "format",\
            "curfilepath",\
            "bakfilepath");
    int i = 0;
    printf("======================================================================================================================================\n");
    for ( i = 0 ; i < g_shm->regNum ; i ++ ){
        alog_regCfg_t *cfg = &(g_shm->regCfgs[i]);
        printf( format_body ,\
            cfg->regName,\
            alog_level_string[cfg->level],\
            cfg->maxSize,\
            cfg->format,\
            cfg->curFilePath_r,\
            cfg->bakFilePath_r);
    }
    printf("======================================================================================================================================\n");
    return 0;
}
/* 
 * 关闭共享内存区
 * */
int alogcmd_close()
{
    ALOG_DEBUG( "进入alogcmd_close()" );
    key_t shmkey = (key_t)atoi(ENV_ALOG_SHMKEY);
    int shmid = 0;
    struct shmid_ds buf;

    shmid = shmget( shmkey , sizeof(alog_shm_t) , 0);
    if ( shmid < 0 ){
        perror("shmget error");
        return -1;
    }

    /* 判断共享内存挂载情况 */
    if ( shmctl( shmid , IPC_STAT , &buf ) ) {
        perror("shmctl error");
        return -1;
    }
    if ( buf.shm_nattch > 0 ){
        printf("共享内存仍有[%d]个进程挂载，确定继续吗？(y/n)\n" , buf.shm_nattch);
        char ch = getchar();
        if ( ch == 'n' ){
            return 0;
        }
    }

    alog_shm_t *g_shm = (alog_shm_t *)shmat( shmid , NULL , 0 );
    if ( g_shm == NULL ){
        perror("shmat error");
        return -1;
    }
    shmdt((void *)g_shm);
    if ( shmctl( shmid , IPC_RMID , 0) ){
        perror("shmdt error");
        return -1;
    }

    ALOG_DEBUG( "alogcmd_close()完成" );
    return 0;
}
