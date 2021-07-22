# **ALOG Reference**

## Table of Contents

* [What is ALOG](##What is ALOG)
* [Version](##Version)
* [Configuration](##Configuration)
  * [Global settings](###Global settings)
  * [Log settings](###Log settings)
* [How to build](##How to build) 
* [How to use](##How to use) 
* [How to handle share memory](##How to handle share memory)
* [Glimpse of implementation](##Glimpse of implementation)
  * [Data structure](###Data structure)
  * [Thread Logic](###Thread Logic)

## What is ALOG

ALOG is a **Light , Thread safe , Asynchronized and Functional** log module for C.

## Version

### 1.0.0(20210722)

First release version.

## Configuration

### Global settings

***${ALOG_HOME}/env/alog.env*** must be loaded before any use of alog interfaces or command tools.

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

***${ALOG_HOME}/cfg/alog.cfg***contains configurations of registries , there should be at least one registry define.

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

## How to build 

```shell
# 1. add following line to you makefile
-I ${ALOG_HOME}/include -L ${ALOG_HOME}/lib -lalog
# 2. for debug version which prints all debug information to stdout , use:
cd ${ALOG_HOME}/src & make clean debug all
# 3. normally you should just use:
cd ${ALOG_HOME}/src & make clean all
# 4. you will get two executive file in ${ALOG_HOME}/bin 
#    alogtest  : for local test
#    alogcmd   : ALOG command line tool for init|reload|print|close share memory
# 5. you will get one static library in ${ALOG_HOME}/lib
#    libalog.a : contains implementations of all ALOG interfaces
```

## How to use

In you program , you can simply call `alog_initContext()` to initialize alog context , normally initialization should be done by caller explicitly , although alog will be self-initialized when `alog_wirte_t` is first called in case that context is missing. 

After initialization , just call `alog_write_t()`or those macros predefined in `alog.h` to generate log message as you want. 

It is recomended that `alog_close()`is called before program exit to ensure that all log messages in buffer is flushed to files on disk. ALOG guarantees that `alog_close`is called before exit() by calling atexit(alog_close) in alog_initContext() , but don't always relay on that. 

## How to handle share memory


1. call `. alog.env` to load environment.
2. use `alogcmd init` to initialize share memory.
3. use `alogcmd reload` to refresh environments and logging configs.
4. use `alogcmd print` to show configs.
5. use `alogcmd setlevel REG LEV` to change log level of certain registry in real time.
6. use `alogcmd close` to destroy share memory.

## Glimpse of implementation

### Data structure

![alog_data_structure](docs/alog_data_structure.png)

### Thread Logic

![thread_logic](docs/thread_logic.png)
