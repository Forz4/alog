# Quick Reference

[TOC]

## What is ALOG

ALOG is a **Light , Thread safe , Asynchronized and Functional** log module for C.

## Configuration

### Global settings

${ALOG_HOME}/env/alog.env

```shell
#######           MANDATORY CONFIGURATIONS                   #######
####################################################################
export ALOG_HOME=~/development/alog
export PATH=${PATH}:${ALOG_HOME}/bin
export ALOG_SHMKEY=1234
####################################################################

#######           OPTIONAL CONFIGURATIONS                    #######
####################################################################
# block size that is malloced in one node
# unit is KB
# default 16KB
# valid scale : ( 0 , 64 ]
#export ALOG_SINGLEBLOCKSIZE=16
####################################################################
# memmory that is allowed to use in total
# unit is MB
# default 16MB
# valid scale : ( 0 , 1024 ]
#export ALOG_MAXMEMORYSIZE=16
####################################################################
# timeout interval for backgroud persist thread
# unit is second
# default 1 second
# valid scale : [ 1 , 5 ]
#export ALOG_FLUSHINTERVAL=1
####################################################################
# interval time for backgroud udpate thread
# unit is microsecond
# default 500 ms
# valid scale : [ 250 , 2000 ]
#export ALOG_CHECKINTERVAL=500
####################################################################
```

### Log settings

${ALOG_HOME}/cfg/alog.cfg

```shell
# ALOG config file
# 1. regname
#    max 20 bytes
# 2. loglevel
#    LOGNON LOGFAT LOGERR LOGWAN LOGINF LOGADT LOGDBG
# 3. file size limit
#    single file size limit(MB)
# 4. prefix pattern
#    7 columns in total , defination as follows:
#    col1: date(YYYYMMDD)
#    col2: time(HH:MM:SS)
#    col3: micro seconds (1/10^6 s) 
#    col4: pid + tid
#    col5: modname
#    col6: log level
#    col7: filename+lineno
# 5. default base path
#    ${} : environment variabale
# 6. current filename pattern
#    ${} : environment variabale
#    %R  : regname
#    %C  ：cstname
#    %Y  ：yead(YYYY)
#    %M  ：month(MM)
#    %D  ：date(DD)
#    %h  ：hour(hh)
#    %m  ：minute(mm)
#    %s  ：second(ss)
#    %P  ：PID
# 7. backup filename pattern
# 8. forcebackup
#    set to 1 if you want to force backup after process quit
[TEST0][LOGINF][1][0111111][${ALOG_HOME}/log][%R.%C.log][%R.%C.log.%Y%M%D%h%m%s][1]
```

## Compiling

```shell
# there are 2 special compiling mode:
# for debug version which prints all debug information to stdout , use:
cd ${ALOG_HOME}/src & make clean debug all
# normally you should just use:
cd ${ALOG_HOME}/src & make clean all
```


## Usage


1. include "alog.h" in your source file
2. call `alog_initContext` before anything else
3. call `alog_write_t` to write whatever you want
4. call `alog_close` before process quit
5. add `-I ${ALOG_HOME}/include` when compiling
6. add `-L ${ALOG_HOME}/lib -lalog` when linking

## Running


1. call `. alog.env` to load environment
2. use `alogcmd init` to initialize share memory
3. use `alogcmd reload` to refresh environments and logging configs
4. use `alogcmd print` to show configs
5. use `alogcmd close` to destroy share memory

## Data Structure

![alog_data_structure](docs/alog_data_structure.png)

## Thread Logic

![thread_logic](docs/thread_logic.png)
