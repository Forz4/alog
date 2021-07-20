#include "alogcmd.h"
int main(int argc , char **argv)
{
    /**
     * get from environment
     */
    ENV_ALOG_HOME = getenv("ALOG_HOME");
    if ( ENV_ALOG_HOME == NULL ){
        printf("environment variable [ALOG_HOME] not found\n");
        exit(1);
    }
    ENV_ALOG_SHMKEY = getenv("ALOG_SHMKEY");
    if ( ENV_ALOG_SHMKEY == NULL ){
        printf("environment variable [ALOG_SHMKEY] not found\n");
        exit(1);
    }

    if ( argc < 2 ){
        alogcmd_help();
        exit(1);
    }

    if ( strcmp( argv[1] , "-h") == 0  || strcmp( argv[1] , "--help") == 0  ){
        alogcmd_help();
        exit(1);
    } else if ( strcmp( argv[1] , "init" ) == 0  || strcmp( argv[1] , "reload" ) == 0 ){
        if ( alogcmd_load(argv[1]) ){
            printf("alogcmd %s fail\n",argv[1]);
            exit(1);
        } else {
            printf("alogcmd %s succeed\n" , argv[1]);
            alogcmd_print();
            exit(0);
        }
    } else if ( strcmp( argv[1] , "print" ) == 0 ){
        alogcmd_print();
        exit(0);
    } else if ( strcmp( argv[1] , "close" ) == 0 ){
        if ( alogcmd_close() ){
            printf("alogcmd close fail\n");
            exit(1);
        } else {
            printf("alogcmd close succeed\n");
            exit(0);
        }
    } else if ( strcmp( argv[1] , "setlevel" ) == 0  ){
        if ( argc != 4 ){
            alogcmd_help();
            exit(1);
        } else {
            if ( alogcmd_setLevel( argv[2] , argv[3] ) == 0  ){
                printf("alogcmd setlevel %s %s succeed\n" , argv[2] , argv[3] );
                exit(0);
            } else {
                printf("alogcmd setlevel %s %s fail\n" , argv[2] , argv[3] );
                exit(1);
            }
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
    printf("\033[0;31mNAME\033[0;39m\n");
    printf("    alogcmd  --  alog module command line tool\n");
    printf("\033[0;31mVERSION\033[0;39m\n");
    printf("    %s\n" , ALOG_VERSION);
    printf("\033[0;31mSYNOPSIS\033[0;39m\n");
    printf("    alogcmd [init|reload|close|print|setlevel]\n");
    printf("\033[0;31mREQUIREMENTS\033[0;39m\n");
    printf("    alogcmd requires two environment variables : ${ALOG_HOME} and ${ALOG_SHMKEY}\n");
    printf("\033[0;31mCOMMANDS\033[0;39m\n");
    printf("    init                Initialize share memory with shmkey ${ALOG_SHMKEY} before any use of alog interface\n");
    printf("    reload              To reload config from file to share memory in real time\n");
    printf("    close               Delete share memory \n");
    printf("    print               Print current share memory \n");
    printf("    setlevel REG LEV    Set the loglevel of REG to LEV\n");
    printf("                        REGname :  1st column of alog.cfg\n");
    printf("                        LEVel   :  LOGNON|LOGFAT|LOGERR|LOGWAN|LOGINF|LOGADT|LOGDBG\n");
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

    if ( strcmp( type , "init") == 0 ){
        shmid = shmget( shmkey , sizeof(alog_shm_t) , IPC_CREAT|IPC_EXCL|0660);
        if ( shmid < 0 ){
            printf("shmkey already exists\n");
            return -1;
        }
    }
    else if ( strcmp( type , "reload") == 0 ){
        shmid = shmget( shmkey , sizeof(alog_shm_t) , 0);
        if ( shmid < 0 ){
            printf("shmkey notfound\n");
            return -1;
        }
    }
    alog_shm_t *g_shm = (alog_shm_t *)shmat( shmid , NULL , 0 );
    if ( g_shm == NULL ){
        printf("shmat error\n");
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
    shmdt(g_shm);
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
        printf("shmget not found\n");
        return -1;
    }
    alog_shm_t *g_shm = (alog_shm_t *)shmat( shmid , NULL , 0 );
    if ( g_shm == NULL ){
        printf("shmat error\n");
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
    shmdt(g_shm);
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
        printf("shmkey not found\n");
        return -1;
    }

    if ( shmctl( shmid , IPC_STAT , &buf ) ) {
        printf("shmctl error\n");
        return -1;
    }
    if ( buf.shm_nattch > 0 ){
        printf("there are [%d] process stil attached to share memory , continue to close ? (y/n)\n" , buf.shm_nattch);
        char ch = getchar();
        if ( ch == 'n' ){
            return 0;
        }
    }

    if ( shmctl( shmid , IPC_RMID , 0) ){
        printf("shmctl error\n");
        return -1;
    }

    return 0;
}
int alogcmd_setLevel( char *regname , char *level )
{
    /**
     * check regname
     */
    if ( regname == NULL || strlen(regname) == 0 || strlen(regname) > ALOG_REGNAME_LEN ){
        printf("invalid regname\n");
        return -1;
    }

    /**
     * check level
     */
    int nlevel = -1;
    if ( level == NULL || strlen(level) == 0 ){
        printf("invalid loglevel\n");
    } else {
        for ( int i = 1 ; i < 8 ; i ++){
            if ( strcmp( level , alog_level_string[i] ) == 0 ){
                nlevel = i;
                break;
            }
        }
        if ( nlevel == -1 ){
            printf("invalid loglevel\n");
            return -1;
        }

    }

    /**
     * attach share memory
     */
    key_t shmkey = (key_t)atoi(ENV_ALOG_SHMKEY);
    int shmid = shmget( shmkey , sizeof(alog_shm_t) , 0);
    if ( shmid < 0 ){
        printf("shmget error\n");
        return -1;
    }
    alog_shm_t *g_shm = (alog_shm_t *)shmat( shmid , NULL , 0 );
    if ( g_shm == NULL ){
        printf("shmat error\n");
        return -1;
    }

    /**
     * find config
     */
    alog_regCfg_t *cfg = getRegByName( g_shm , regname );
    if ( cfg == NULL ){
        printf("regname not found\n");
        return -1;
    }

    /**
     * update level
     */
    cfg->level = (short)nlevel;
    time(&(g_shm->updTime));
    shmdt(g_shm);

    return 0;
}
