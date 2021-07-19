#include "alogcmd.h"
int main(int argc , char **argv)
{
    /**
     * get from environment
     */
    ENV_ALOG_HOME = getenv("ALOG_HOME");
    if ( ENV_ALOG_HOME == NULL ){
        perror("environment variable [ALOG_HOME] not found\n");
        exit(1);
    }
    ENV_ALOG_SHMKEY = getenv("ALOG_SHMKEY");
    if ( ENV_ALOG_SHMKEY == NULL ){
        perror("environment variable [ALOG_SHMKEY] not found\n");
        exit(1);
    }

    if ( argc != 2 ){
        alogcmd_help();
        exit(1);
    }
    if ( strcmp( argv[1] , "init" ) == 0  || strcmp( argv[1] , "reload" ) == 0 ){
        if ( alogcmd_load(argv[1]) ){
            printf("ALOG %s fail\n",argv[1]);
            exit(1);
        } else {
            alogcmd_print();
        }
    } else if ( strcmp( argv[1] , "print" ) == 0 ){
        alogcmd_print();
    } else if ( strcmp( argv[1] , "close" ) == 0 ){
        if ( alogcmd_close() ){
            printf("ALOG close fail\n");
            exit(1);
        } else {
            printf("ALOG close succeed\n");
        }
    } else {
        alogcmd_help();
        exit(1);
    }
    return 0;
}
/**
 * [alogcmd_help print help]
 */
void alogcmd_help()
{
    printf("%s\n" , ALOG_VERSION);
    printf("alogcmd <init/reload/print/close>\n");
    return;
}
/**
 * [alogcmd_load load from file and environment]
 * @param  type [init/reload]
 * @return      [0 for success]
 */
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

    char filepath[ALOG_FILEPATH_LEN];
    sprintf( filepath , "%s/cfg/alog.cfg" , ENV_ALOG_HOME);
    alog_shm_t *l_shm = alog_loadCfg( filepath );
    if ( l_shm == NULL ){
        printf("fail to load config from %s\n" , filepath);
        return -1;
    }
    l_shm->shmKey = shmkey;
    l_shm->shmId = shmid;
    time(&(l_shm->updTime));

    memcpy( g_shm , l_shm , sizeof(alog_shm_t));
    free(l_shm);
    return 0;

}
/**
 * [alogcmd_print print share memory]
 * @return [0 for success]
 */
int alogcmd_print()
{
    int shmkey = atoi(ENV_ALOG_SHMKEY);
    char *format_head = "|%-20s|%-8s|%-11s|%-8s|%-30s|%-30s|%-30s|\n";
    char *format_body = "|%-20s|%-8s|%-11d|%-8s|%-30s|%-30s|%-30s|\n";

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
    printf("ALOG_HOME Directory  :    %s\n",ENV_ALOG_HOME);
    printf("ALOG_SHMKEY          :    %d\n",g_shm->shmKey);
    printf("ALOG SHMID           :    %d\n",g_shm->shmId);
    printf("ALOG_MAXMEMORYSIZE   :    %d(MB)\n",g_shm->maxMemorySize);
    printf("ALOG_SINGLEBLOCKSIZE :    %d(KB)\n",g_shm->singleBlockSize);
    printf("ALOG_FLUSHINTERVAL   :    %d(s)\n",g_shm->flushInterval);
    printf("ALOG_CHECKINTERVAL   :    %d(ms)\n",g_shm->checkInterval);
    printf("UPDATE TIMESTAMP     :    %4d%02d%02d %02d:%02d:%02d\n",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
    printf("REG CONFIG NUMBER    :    %d\n",g_shm->regNum);
    printf("=================================================================================================================================================\n");
    printf( format_head ,\
            "regname",\
            "level",\
            "maxsize(MB)",\
            "format",\
            "default base path",\
            "curfilename",\
            "bakfilename");
    int i = 0;
    printf("=================================================================================================================================================\n");
    for ( i = 0 ; i < g_shm->regNum ; i ++ ){
        alog_regCfg_t *cfg = &(g_shm->regCfgs[i]);
        printf( format_body ,\
            cfg->regName,\
            alog_level_string[cfg->level],\
            cfg->maxSize,\
            cfg->format,\
            cfg->defLogBasePath_r,\
            cfg->curFileNamePattern_r,\
            cfg->bakFileNamePattern_r);
    }
    printf("=================================================================================================================================================\n");
    return 0;
}
/**
 * [alogcmd_close destroy share memory]
 * @return []
 */
int alogcmd_close()
{
    key_t shmkey = (key_t)atoi(ENV_ALOG_SHMKEY);
    int shmid = 0;
    struct shmid_ds buf;

    shmid = shmget( shmkey , sizeof(alog_shm_t) , 0);
    if ( shmid < 0 ){
        perror("shmget error");
        return -1;
    }

    if ( shmctl( shmid , IPC_STAT , &buf ) ) {
        perror("shmctl error");
        return -1;
    }
    if ( buf.shm_nattch > 0 ){
        printf("there are [%d] process stil attached to share memory , continue to close ? (y/n)\n" , buf.shm_nattch);
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

    return 0;
}
