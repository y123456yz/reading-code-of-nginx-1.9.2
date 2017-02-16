
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_LOG_H_INCLUDED_
#define _NGX_LOG_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

/*
表4-6 ngx_log_error日志接口level参数的取值范围
┏━━━━━━━━┳━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    级别名称    ┃  值  ┃    意义                                                            ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                ┃      ┃    最高级别日志，日志的内容不会再写入log参数指定的文件，而是会直接 ┃
┃NGX_LOG_STDERR  ┃    O ┃                                                                    ┃
┃                ┃      ┃将日志输出到标准错误设备，如控制台屏幕                              ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                ┃      ┃  大于NGX—LOG ALERT级别，而小于或等于NGX LOG EMERG级别的           ┃
┃NGX_LOG_EMERG   ┃   1  ┃                                                                    ┃
┃                ┃      ┃日志都会输出到log参数指定的文件中                                   ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG_ALERT   ┃    2 ┃    大干NGX LOG CRIT级别                                            ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  NGX LOG_CRIT  ┃    3 ┃    大干NGX LOG ERR级别                                             ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  NGX LOG_ERR   ┃    4 ┃    大干NGX—LOG WARN级别                                           ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  NGX LOG_WARN  ┃    5 ┃    大于NGX LOG NOTICE级别                                          ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX_LOG_NOTICE  ┃  6   ┃  大于NGX__ LOG INFO级别                                            ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  NGX_LOG_INFO  ┃    7 ┃    大于NGX—LOG DEBUG级别                                          ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX_LOG_DEBUG   ┃    8 ┃    调试级别，最低级别日志                                          ┃
┗━━━━━━━━┻━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
*/ 
//stderr (0)>= emerg(1) >= alert(2) >= crit(3) >= err(4)>= warn(5) >= notice(6) >= info(7) >= debug(8) 
//debug级别最低，stderr级别最高；圆括号中的数据是对应日志等级的值。
//log->log_level中的低4位取值为NGX_LOG_STDERR等  5-12位取值为位图，表示对应模块的日志   另外NGX_LOG_DEBUG_CONNECTION NGX_LOG_DEBUG_ALL对应connect日志和所有日志
//下面这些通过ngx_log_error输出  对应err_levels[]参考ngx_log_set_levels
#define NGX_LOG_STDERR            0
#define NGX_LOG_EMERG             1
#define NGX_LOG_ALERT             2
#define NGX_LOG_CRIT              3
#define NGX_LOG_ERR               4
#define NGX_LOG_WARN              5 //如果level > NGX_LOG_WARN则不会在屏幕前台打印，见ngx_log_error_core
#define NGX_LOG_NOTICE            6
#define NGX_LOG_INFO              7
#define NGX_LOG_DEBUG             8

/*
在使用ngx_log_debug宏时，level的崽义完全不同，它表达的意义不再是级别（已经
是DEBUG级别），而是日志类型，因为ngx_log_debug宏记录的日志必须是NGX-LOG—
DEBUG调试级别的，这里的level由各子模块定义。level的取值范围参见表4-7。
表4-7 ngx_log_debug日志接口level参数的取值范围
┏━━━━━━━━━━┳━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    级别名称        ┃  值    ┃    意义                                        ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG DEBUG CORE  ┃ Ox010  ┃    Nginx核心模块的调试日志                     ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG DEBUG ALLOC ┃ Ox020  ┃    Nginx在分配内存时使用的调试日志             ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG DEBUG MUTEX ┃ Ox040  ┃    Nginx在使用进程锁时使用的调试日志           ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG DEBUG EVENT ┃ Ox080  ┃    Nginx事件模块的调试日志                     ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG DEBUG HTTP  ┃ Oxl00  ┃    Nginx http模块的调试日志                    ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG DEBUG MAIL  ┃ Ox200  ┃    Nginx邮件模块的调试日志                     ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG_DEBUG_MYSQL ┃ Ox400  ┃    表示与MySQL相关的Nginx模块所使用的调试日志  ┃
┗━━━━━━━━━━┻━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━┛
    当HTTP模块调用ngx_log_debug宏记录日志时，传人的level参数是NGX- LOG—
DEBUG HTTP，这时如果109参数不属于HTTP模块，如使周了event事件模块的log，则
不会输出任何日志。它正是ngx_log_debug拥有level参数的意义所在。
*/ //下面这些是通过与操作判断是否需要打印，可以参考ngx_log_debug7
//log->log_level中的低4位取值为NGX_LOG_STDERR等  5-12位取值为位图，表示对应模块的日志   另外NGX_LOG_DEBUG_CONNECTION NGX_LOG_DEBUG_ALL对应connect日志和所有日志
//如果通过加参数debug_http则会打开NGX_LOG_DEBUG_HTTP开关，见debug_levels  ngx_log_set_levels,如果打开下面开关中的一个，则NGX_LOG_STDERR到NGX_LOG_DEBUG会全部打开，因为log_level很大
//下面这些通过ngx_log_debug0 -- ngx_log_debug8输出  对应debug_levels[] 参考ngx_log_set_levels
#define NGX_LOG_DEBUG_CORE        0x010
#define NGX_LOG_DEBUG_ALLOC       0x020
#define NGX_LOG_DEBUG_MUTEX       0x040
#define NGX_LOG_DEBUG_EVENT       0x080  
#define NGX_LOG_DEBUG_HTTP        0x100
#define NGX_LOG_DEBUG_MAIL        0x200
#define NGX_LOG_DEBUG_MYSQL       0x400
#define NGX_LOG_DEBUG_STREAM      0x800

/*
 * do not forget to update debug_levels[] in src/core/ngx_log.c
 * after the adding a new debug level
 */
//log->log_level中的低4位取值为NGX_LOG_STDERR等  5-12位取值为位图，表示对应模块的日志   另外NGX_LOG_DEBUG_CONNECTION NGX_LOG_DEBUG_ALL对应connect日志和所有日志
#define NGX_LOG_DEBUG_FIRST       NGX_LOG_DEBUG_CORE
#define NGX_LOG_DEBUG_LAST        NGX_LOG_DEBUG_STREAM
#define NGX_LOG_DEBUG_CONNECTION  0x80000000 // --with-debug)                    NGX_DEBUG=YES  会打开连接日志 
#define NGX_LOG_DEBUG_ALL         0x7ffffff0


typedef u_char *(*ngx_log_handler_pt) (ngx_log_t *log, u_char *buf, size_t len);
typedef void (*ngx_log_writer_pt) (ngx_log_t *log, ngx_uint_t level,
    u_char *buf, size_t len);


struct ngx_log_s {  
    //如果设置的log级别为debug，则会在ngx_log_set_levels把level设置为NGX_LOG_DEBUG_ALL
    //赋值见ngx_log_set_levels
    ngx_uint_t           log_level;//日志级别或者日志类型  默认为NGX_LOG_ERR  如果通过error_log  logs/error.log  info;则为设置的等级  比该级别下的日志可以打印
    ngx_open_file_t     *file; //日志文件

    ngx_atomic_uint_t    connection;//连接数，不为O时会输出到日志中

    time_t               disk_full_time;

    /* 记录日志时的回调方法。当handler已经实现（不为NULL），并且不是DEBUG调试级别时，才会调用handler钩子方法 */
    ngx_log_handler_pt   handler; //从连接池获取ngx_connection_t后，c->log->handler = ngx_http_log_error;

    /*
    每个模块都可以自定义data的使用方法。通常，data参数都是在实现了上面的handler回调方法后
    才使用的。例如，HTTP框架就定义了handler方法，并在data中放入了这个请求的上下文信息，这样每次输出日
    志时都会把这个请求URI输出到日志的尾部
    */
    void                *data; //指向ngx_http_log_ctx_t，见ngx_http_init_connection

    ngx_log_writer_pt    writer;
    void                *wdata;

    /*
     * we declare "action" as "char *" because the actions are usually
     * the static strings and in the "u_char *" case we have to override
     * their types all the time
     */
     
    /*
    表示当前的动作。实际上，action与data是一样的，只有在实现了handler回调方法后才会使
    用。例如，HTTP框架就在handler方法中检查action是否为NULL，如果不为NULL，就会在日志后加入“while
    ”+action，以此表示当前日志是在进行什么操作，帮助定位问题
    */
    char                *action;
    //ngx_log_insert插入，在ngx_log_error_core找到对应级别的日志配置进行输出，因为可以配置error_log不同级别的日志存储在不同的日志文件中
    ngx_log_t           *next;
};

#define NGX_STR2BUF_LEN 256
void ngx_str_t_2buf(char *buf, ngx_str_t *str);


#define NGX_MAX_ERROR_STR   2048


/*********************************/

#if (NGX_HAVE_C99_VARIADIC_MACROS)

#define NGX_HAVE_VARIADIC_MACROS  1
/*
表4-6 ngx_log_error日志接口level参数的取值范围
┏━━━━━━━━┳━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    级别名称    ┃  值  ┃    意义                                                            ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                ┃      ┃    最高级别日志，日志的内容不会再写入log参数指定的文件，而是会直接 ┃
┃NGX_LOG_STDERR  ┃    O ┃                                                                    ┃
┃                ┃      ┃将日志输出到标准错误设备，如控制台屏幕                              ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                ┃      ┃  大于NGX—LOG ALERT级别，而小于或等于NGX LOG EMERG级别的           ┃
┃NGX_LOG:EMERG   ┃  1   ┃                                                                    ┃
┃                ┃      ┃日志都会输出到log参数指定的文件中                                   ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG ALERT   ┃    2 ┃    大干NGX LOG CRIT级别                                            ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  NGX LOG CRIT  ┃    3 ┃    大干NGX LOG ERR级别                                             ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  NGX LOG ERR   ┃    4 ┃    大干NGX—LOG WARN级别                                           ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  NGX LOG WARN  ┃    5 ┃    大于NGX LOG NOTICE级别                                          ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG NOTICE  ┃  6   ┃  大于NGX__ LOG INFO级别                                            ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  NGX LOG INFO  ┃    7 ┃    大于NGX—LOG DEBUG级别                                          ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG DEBUG   ┃    8 ┃    调试级别，最低级别日志                                          ┃
┗━━━━━━━━┻━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
    使用ngx_log_error宏记录日志时，如果传人的level级别小于或等于log参数中的日志
级别（通常是由nginx.conf配置文件中指定），就会输出日志内容，否则这条日志会被忽略。
*/
#define ngx_log_error(level, log, ...)                                        \
    if ((log)->log_level >= level) ngx_log_error_core(level, log,__FUNCTION__, __LINE__, __VA_ARGS__)

void ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, const char* filename, int lineno, ngx_err_t err,
    const char *fmt, ...);
    
void ngx_log_error_coreall(ngx_uint_t level, ngx_log_t *log, const char* filename, int lineno, ngx_err_t err,
    const char *fmt, ...);

/*
    在使用ngx_log_debug宏时，level的崽义完全不同，它表达的意义不再是级别（已经
是DEBUG级别），而是日志类型，因为ngx_log_debug宏记录的日志必须是NGX-LOG—
DEBUG调试级别的，这里的level由各子模块定义。level的取值范围参见表4-7。
表4-7 ngx_log_debug日志接口level参数的取值范围
┏━━━━━━━━━━┳━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    级别名称        ┃  值    ┃    意义                                        ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX_LOG_DEBUG_CORE  ┃ Ox010  ┃    Nginx核心模块的调试日志                     ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX_LOG_DEBUG_ALLOC ┃ Ox020  ┃    Nginx在分配内存时使用的调试日志             ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX_LOG_DEBUG_MUTEX ┃ Ox040  ┃    Nginx在使用进程锁时使用的调试日志           ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX_LOG_DEBUG_EVENT ┃ Ox080  ┃    Nginx事件模块的调试日志                     ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX_LOG_DEBUG_HTTP  ┃ Oxl00  ┃    Nginx http模块的调试日志                    ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX_LOG_DEBUG_MAIL  ┃ Ox200  ┃    Nginx邮件模块的调试日志                     ┃
┣━━━━━━━━━━╋━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX_LOG_DEBUG_MYSQL ┃ Ox400  ┃    表示与MySQL相关的Nginx模块所使用的调试日志  ┃
┗━━━━━━━━━━┻━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━┛
    当HTTP模块调用ngx_log_debug宏记录日志时，传人的level参数是NGX_LOG_DEBUG HTTP，
这时如果1og参数不属于HTTP模块，如使周了event事件模块的log，则
不会输出任何日志。它正是ngx_log_debug拥有level参数的意义所在。
*/
#define ngx_log_debug(level, log, ...)                                        \
    if ((log)->log_level & level)                                             \
        ngx_log_error_core(NGX_LOG_DEBUG, log,__FUNCTION__, __LINE__, __VA_ARGS__)
        
#define ngx_log_debugall(log, ...)                                        \
            ngx_log_error_coreall(NGX_LOG_DEBUG, log,__FUNCTION__, __LINE__, __VA_ARGS__)

/*********************************/

#elif (NGX_HAVE_GCC_VARIADIC_MACROS)

#define NGX_HAVE_VARIADIC_MACROS  1

#define ngx_log_error(level, log, args...)                                    \
    if ((log)->log_level >= level) ngx_log_error_core(level, log,__FUNCTION__, __LINE__, args)

void ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, const char* filename, int lineno, ngx_err_t err,
    const char *fmt, ...);
    void ngx_log_error_coreall(ngx_uint_t level, ngx_log_t *log, const char* filename, int lineno, ngx_err_t err,
        const char *fmt, ...);

#define ngx_log_debug(level, log, args...)                                    \
    if ((log)->log_level & level)                                             \
        ngx_log_error_core(NGX_LOG_DEBUG, log,__FUNCTION__, __LINE__, args)

#define ngx_log_debugall(log, args...)                                    \
            ngx_log_error_coreall(NGX_LOG_DEBUG, log,__FUNCTION__, __LINE__, args)

/*********************************/

#else /* no variadic macros */

#define NGX_HAVE_VARIADIC_MACROS  0

void ngx_cdecl ngx_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...);
void ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, const char* filename, int lineno, ngx_err_t err,
    const char *fmt, va_list args);
void ngx_log_error_coreall(ngx_uint_t level, ngx_log_t *log, const char* filename, int lineno, ngx_err_t err,
    const char *fmt, va_list args);
#define ngx_log_debugall(log, args...)                                    \
            ngx_log_error_coreall(NGX_LOG_DEBUG, log,__FUNCTION__, __LINE__, args)

void ngx_cdecl ngx_log_debug_core(ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...);


#endif /* variadic macros */


/*********************************/

#if (NGX_DEBUG)

#if (NGX_HAVE_VARIADIC_MACROS)
/* 注意不能用%V输出，否则会出现段错误 */
#define ngx_log_debug0(level, log, err, fmt)                                  \
        ngx_log_debug(level, log, err, fmt)

#define ngx_log_debug1(level, log, err, fmt, arg1)                            \
        ngx_log_debug(level, log, err, fmt, arg1)

#define ngx_log_debug2(level, log, err, fmt, arg1, arg2)                      \
        ngx_log_debug(level, log, err, fmt, arg1, arg2)

#define ngx_log_debug3(level, log, err, fmt, arg1, arg2, arg3)                \
        ngx_log_debug(level, log, err, fmt, arg1, arg2, arg3)

#define ngx_log_debug4(level, log, err, fmt, arg1, arg2, arg3, arg4)          \
        ngx_log_debug(level, log, err, fmt, arg1, arg2, arg3, arg4)

#define ngx_log_debug5(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5)    \
        ngx_log_debug(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5)

#define ngx_log_debug6(level, log, err, fmt,                                  \
                       arg1, arg2, arg3, arg4, arg5, arg6)                    \
        ngx_log_debug(level, log, err, fmt,                                   \
                       arg1, arg2, arg3, arg4, arg5, arg6)

#define ngx_log_debug7(level, log, err, fmt,                                  \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7)              \
        ngx_log_debug(level, log, err, fmt,                                   \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7)

#define ngx_log_debug8(level, log, err, fmt,                                  \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)        \
        ngx_log_debug(level, log, err, fmt,                                   \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)



#else /* no variadic macros */

#define ngx_log_debug0(level, log, err, fmt)                                  \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt)

#define ngx_log_debug1(level, log, err, fmt, arg1)                            \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt, arg1)

#define ngx_log_debug2(level, log, err, fmt, arg1, arg2)                      \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt, arg1, arg2)

#define ngx_log_debug3(level, log, err, fmt, arg1, arg2, arg3)                \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt, arg1, arg2, arg3)

#define ngx_log_debug4(level, log, err, fmt, arg1, arg2, arg3, arg4)          \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt, arg1, arg2, arg3, arg4)

#define ngx_log_debug5(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5)    \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt, arg1, arg2, arg3, arg4, arg5)

#define ngx_log_debug6(level, log, err, fmt,                                  \
                       arg1, arg2, arg3, arg4, arg5, arg6)                    \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt, arg1, arg2, arg3, arg4, arg5, arg6)

#define ngx_log_debug7(level, log, err, fmt,                                  \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7)              \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt,                                     \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7)

#define ngx_log_debug8(level, log, err, fmt,                                  \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)        \
    if ((log)->log_level & level)                                             \
        ngx_log_debug_core(log, err, fmt,                                     \
                       arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)

#endif

#else /* !NGX_DEBUG */

#define ngx_log_debug0(level, log, err, fmt)
#define ngx_log_debug1(level, log, err, fmt, arg1)
#define ngx_log_debug2(level, log, err, fmt, arg1, arg2)
#define ngx_log_debug3(level, log, err, fmt, arg1, arg2, arg3)
#define ngx_log_debug4(level, log, err, fmt, arg1, arg2, arg3, arg4)
#define ngx_log_debug5(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5)
#define ngx_log_debug6(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5, arg6)
#define ngx_log_debug7(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5,    \
                       arg6, arg7)
#define ngx_log_debug8(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5,    \
                       arg6, arg7, arg8)

#endif

/*********************************/

ngx_log_t *ngx_log_init(u_char *prefix);
void ngx_cdecl ngx_log_abort(ngx_err_t err, const char *fmt, ...);
void ngx_cdecl ngx_log_stderr(ngx_err_t err, const char *fmt, ...);
u_char *ngx_log_errno(u_char *buf, u_char *last, ngx_err_t err);
ngx_int_t ngx_log_open_default(ngx_cycle_t *cycle);
ngx_int_t ngx_log_redirect_stderr(ngx_cycle_t *cycle);
ngx_log_t *ngx_log_get_file_log(ngx_log_t *head);
char *ngx_log_set_log(ngx_conf_t *cf, ngx_log_t **head);

/*
 * ngx_write_stderr() cannot be implemented as macro, since
 * MSVC does not allow to use #ifdef inside macro parameters.
 *
 * ngx_write_fd() is used instead of ngx_write_console(), since
 * CharToOemBuff() inside ngx_write_console() cannot be used with
 * read only buffer as destination and CharToOemBuff() is not needed
 * for ngx_write_stderr() anyway.
 */
static ngx_inline void
ngx_write_stderr(char *text)
{
    (void) ngx_write_fd(ngx_stderr, text, ngx_strlen(text));
}


static ngx_inline void
ngx_write_stdout(char *text)
{
    (void) ngx_write_fd(ngx_stdout, text, ngx_strlen(text));
}


extern ngx_module_t  ngx_errlog_module;
extern ngx_uint_t    ngx_use_stderr;


#endif /* _NGX_LOG_H_INCLUDED_ */
