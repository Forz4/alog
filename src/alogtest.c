#include "alog.h"
#include <stdlib.h>
#include <stdio.h>

int     COUNT = 100;
int     INTERVAL = 5;
int     THREADNUM = 1;
int     LENGTH = 50;
char    *message;
char    REGNAME[20+1] = "TEST0";
int     MODE = 1;
char    logfilepath[ALOG_FILEPATH_LEN];

void print_help()
{
    printf("alogtest [-r regname][-p direct_log_path][-c count][-i interval][-l length][-t thread number][-A|H|B][-h]\n");
    exit(0);
}
void *func(void *arg)
{
    int i = 0 ;
    char cstname[20+1];
    memset( cstname , 0x00 , sizeof(cstname) );
    sprintf(cstname , "%ld" , (long)getpid() );
    for ( i = 0 ; i < COUNT ; i ++){
        if ( MODE == 1 ){
            if ( strlen(logfilepath) )
                ALOG_INFASC( REGNAME , cstname , "" , logfilepath , "%s" , message);
            else
                ALOG_INFASC( REGNAME , cstname , "" , NULL , "%s" , message);
        }
        else if ( MODE == 2 )
            ALOG_INFHEX( REGNAME , cstname , "" , logfilepath , message , LENGTH);
        else if ( MODE == 3 )
            ALOG_INFBIN( REGNAME , cstname , "" , logfilepath , message , LENGTH);
        if ( INTERVAL)  usleep(INTERVAL);
    }
    return NULL;
}
int main(int argc , char *argv[])
{
    int op = 0;
    while ( (op = getopt(argc , argv , "r:p:c:i:l:t:AHBh") ) > 0 ){
        switch(op){
            case 'r':
                strcpy( REGNAME , optarg);
                break;
            case 'p':
                strcpy( logfilepath , optarg);
                break;
            case 'c':
                COUNT = atoi(optarg);
                break;
            case 'i':
                INTERVAL = atoi(optarg);
                break;
            case 'l':
                LENGTH = atoi(optarg);
                break;
            case 't':
                THREADNUM = atoi(optarg);
                break;
            case 'A':
                MODE = 1;
                break;
            case 'H':
                MODE = 2;
                break;
            case 'B':
                MODE = 3;
                break;
            case 'h':
                print_help();
                break;
        }
    }

    if ( alog_initContext() ) {
        exit(0);
    }

    message = (char *)malloc(LENGTH+1);
    int             i = 0;
    pthread_t       tids[20];

    for( i = 0 ; i < LENGTH ; i ++){
        message[i] = 'A' + i % 26;
    }
    message[i] = '\0';

    for( i = 0 ; i < THREADNUM ; i ++ ){
        pthread_create( &tids[i] , NULL , func , 0 );
    }
    for( i = 0 ; i < THREADNUM ; i ++ ){
        pthread_join( tids[i] ,  NULL );
    }
    free(message);

    alog_close();
    return 0;
}
