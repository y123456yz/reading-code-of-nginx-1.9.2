
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


static char *ngx_error_log(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_log_set_levels(ngx_conf_t *cf, ngx_log_t *log);
static void ngx_log_insert(ngx_log_t *log, ngx_log_t *new_log);


#if (NGX_DEBUG)

static void ngx_log_memory_writer(ngx_log_t *log, ngx_uint_t level,
    u_char *buf, size_t len);
static void ngx_log_memory_cleanup(void *data);


typedef struct {
    u_char        *start;
    u_char        *end;
    u_char        *pos;
    ngx_atomic_t   written;
} ngx_log_memory_buf_t;

#endif

/*
Nginx的日志模块（这里所说的日志模块是ngx_errlog_module模块，而ngx_http_log_module模块是用于记录HTTP请求的访问日志的，
两者功能不同，在实现上也没有任何关系）为其他模块提供了基本的记录日志功能
*/
//error_log path level path路径 level打印级别，只有比level级别高的才打印  值越小优先级越高
static ngx_command_t  ngx_errlog_commands[] = { //    error_log file [ debug | info | notice | warn | error | crit ] 
    {ngx_string("error_log"), //ngx_errlog_module中的error_log配置只能全局配置，ngx_http_core_module在http{} server{} local{}中配置
     NGX_MAIN_CONF|NGX_CONF_1MORE,
     ngx_error_log,
     0,
     0,
     NULL},

    ngx_null_command
};


static ngx_core_module_t  ngx_errlog_module_ctx = {
    ngx_string("errlog"),
    NULL,
    NULL
};

/*
Nginx的日志模块（这里所说的日志模块是ngx_errlog_module模块，而ngx_http_log_module模块是用于记录HTTP请求的访问日志的，
两者功能不同，在实现上也没有任何关系）为其他模块提供了基本的记录日志功能
*/
ngx_module_t  ngx_errlog_module = {
    NGX_MODULE_V1,
    &ngx_errlog_module_ctx,                /* module context */
    ngx_errlog_commands,                   /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_log_t        ngx_log;//指向的是ngx_log_file，见ngx_log_init
static ngx_open_file_t  ngx_log_file;//NGX_ERROR_LOG_PATH文件 ngx_log_init
ngx_uint_t              ngx_use_stderr = 1;


static ngx_str_t err_levels[] = { //对应日志级别NGX_LOG_STDERR--NGX_LOG_DEBUG，参考ngx_log_set_levels
    ngx_null_string,
    ngx_string("emerg"),
    ngx_string("alert"),
    ngx_string("crit"),
    ngx_string("error"),
    ngx_string("warn"),
    ngx_string("notice"),
    ngx_string("info"),
    ngx_string("debug")
};

//debug_levels代表的是日志类型     err_levels代表的是日志级别  
static const char *debug_levels[] = { //对应位图NGX_LOG_DEBUG_CORE---NGX_LOG_DEBUG_LAST  参考ngx_log_set_levels
    "debug_core", "debug_alloc", "debug_mutex", "debug_event",
    "debug_http", "debug_mail", "debug_mysql", "debug_stream"
};

void ngx_str_t_2buf(char *buf, ngx_str_t *str)
{
    if(buf == NULL || str == NULL)
        return;
    
    if(str->data != NULL && str->len != 0) {
        strncpy(buf, (char*)str->data, ngx_min(str->len, NGX_STR2BUF_LEN - 1));
        buf[str->len] = '\0';
    }
}

/*
ngx_log_error宏和ngx_log_debug宏都包括参数level、log、err、fmt，下面分别解释这
4个参数的意义。
    (1) level参数
    对于ngx_log_error宏来说，level表示当前这条日志的级别。它的取值范围见表4-6。
	

┏━━━━━━━━┳━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    级别名称    ┃  值  ┃    意义                                                            ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                ┃      ┃    最高级别日志，日志的内容不会再写入log参数指定的文件，而是会直接 ┃
┃NGX_LOG_STDERR  ┃    O ┃                                                                    ┃
┃                ┃      ┃将日志输出到标准错误设备，如控制台屏幕                              ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                ┃      ┃  大于NGX—LOG ALERT级别，而小于或等于NGX LOG EMERG级别的            ┃
┃NGX_LOG:EMERG   ┃  1   ┃                                                                    ┃
┃                ┃      ┃日志都会输出到log参数指定的文件中                                   ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG ALERT   ┃    2 ┃    大干NGX LOG CRIT级别                                            ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  NGX LOG CRIT  ┃    3 ┃    大干NGX LOG ERR级别                                             ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  NGX LOG ERR   ┃    4 ┃    大干NGX—LOG WARN级别                                            ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  NGX LOG WARN  ┃    5 ┃    大于NGX LOG NOTICE级别                                          ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG NOTICE  ┃  6   ┃  大于NGX__ LOG INFO级别                                            ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  NGX LOG INFO  ┃    7 ┃    大于NGX—LOG DEBUG级别                                           ┃
┣━━━━━━━━╋━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX LOG DEBUG   ┃    8 ┃    调试级别，最低级别日志                                          ┃
┗━━━━━━━━┻━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
    使用ngx_log_error宏记录日志时，如果传人的level级别小于或等于log参数中的日志
级别（通常是由nginx.conf配置文件中指定），就会输出日志内容，否则这条日志会被忽略。
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
    当HTTP模块调用ngx_log_debug宏记录日志时，传人的level参数是NGX_LOG_DEBUG HTTP，
这时如果1og参数不属于HTTP模块，如使周了event事件模块的log，则
不会输出任何日志。它正是ngx_log_debug拥有level参数的意义所在。
    (2) log参数
    实际上，在开发HTTP模块时我们并不用关心log参数的构造，因为在处理请求时ngx_
http_request_t结构中的connection成员就有一个ngx_log_t类型的log成员，可以传给ngx_
log_error宏和ngx_log_debug宏记录日志。在读取配置阶段，ngx_conf_t结构也有log成员
可以用来记录日志（读取配置阶段时的日志信息都将输出到控制台屏幕）。下面简单地看一
下ngx_log_t的定义。
typedef struct ngx_log_s ngx_log_t;
typedef u_char * (*ngx_log_handler_pt)   (ngx_log_t  *log,  u_char *buf,  size_t  len) ;

struct ngx_log_s  {
    ／／日志级别或者日志类型
    ngx_uint_t log_level;
    f，日志文件
    ngx_open_file_t★file;
    ／／连接数，不为O时会输出到日志中
    ngx_atomic_uint_t connection;
    ／★记录日志时的回调方法。当handler已经实现（不为NULL），并且不是DEBUG调试级别时，才会调
用handler钩子方法★／
    ngx_log_handler_pt handler;
    ／★每个模块都可以自定义data的使用方法。通常，data参数都是在实现了上面的handler回调方法后
才使用的。例如，HTTP框架就定义了handler方法，并在data中放入了这个请求的上下文信息，这样每次输出日
志时都会把这个请求URI输出到日志的尾部+／
    void★data;
    ／★表示当前的动作。实际上，action与data足一样的，只有在实现了handler回调方法后才会使
用。例如，HTTP框架就在handler方法中检查action是否为NULL，如果不为NULL，就会在日志后加入“while
”+action，以此表示当前日志是在进行什么操作，帮助定位问题+／
    char *action;
}

*/
#if (NGX_HAVE_VARIADIC_MACROS)
void
ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, const char* filename, int lineno, ngx_err_t err,
    const char *fmt, ...) 
//这里打印一定要注意，例如位标记用%d %u打印就会出现段错误，例如用%d打印ngx_event_t->write;
//例如打印ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0, "http upstream request(ev->write:%u %u)  %V", ngx_event_t->write, ngx_event_t->write); 段错误
#else

void
ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, const char* filename, int lineno, ngx_err_t err,
    const char *fmt, va_list args)

#endif
{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list      args;
#endif
    u_char      *p, *last, *msg;
    ssize_t      n;
    ngx_uint_t   wrote_stderr, debug_connection;
    u_char       errstr[NGX_MAX_ERROR_STR];
    char filebuf[52];

    last = errstr + NGX_MAX_ERROR_STR;
    
    p = ngx_cpymem(errstr, ngx_cached_err_log_time.data,
                   ngx_cached_err_log_time.len);

    snprintf(filebuf, sizeof(filebuf), "[%40s, %5d]", filename, lineno);

    p = ngx_slprintf(p, last, "%s ", filebuf);  
    
    p = ngx_slprintf(p, last, " [%V] ", &err_levels[level]);

    /* pid#tid */
    p = ngx_slprintf(p, last, "%P#" NGX_TID_T_FMT ": ", ngx_log_pid, ngx_log_tid);
    
    if (log->connection) {
        p = ngx_slprintf(p, last, "*%uA ", log->connection);
    }

    msg = p;

#if (NGX_HAVE_VARIADIC_MACROS)

    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

#else

    p = ngx_vslprintf(p, last, fmt, args);

#endif

    if (err) {
        p = ngx_log_errno(p, last, err);
    }

    if (level != NGX_LOG_DEBUG && log->handler) {
        p = log->handler(log, p, last - p); //非NGX_LOG_DEBUG的情况执行handler,里面会添加新的信息到打印的信息buf中一起打印
    }

    if (p > last - NGX_LINEFEED_SIZE) {
        p = last - NGX_LINEFEED_SIZE;
    }

    ngx_linefeed(p);

    wrote_stderr = 0;
    debug_connection = (log->log_level & NGX_LOG_DEBUG_CONNECTION) != 0;

    while (log) {

        if (log->log_level < level && !debug_connection) { //只有log_level大于level则会输出到error_log配置中
            break;
        }

        if (log->writer) {
            log->writer(log, level, errstr, p - errstr);
            goto next;
        }

        if (ngx_time() == log->disk_full_time) {

            /*
             * on FreeBSD writing to a full filesystem with enabled softupdates
             * may block process for much longer time than writing to non-full
             * filesystem, so we skip writing to a log for one second
             */

            goto next;
        }

        n = ngx_write_fd(log->file->fd, errstr, p - errstr); //写到log文件中

        if (n == -1 && ngx_errno == NGX_ENOSPC) {
            log->disk_full_time = ngx_time();
        }

        if (log->file->fd == ngx_stderr) {
            wrote_stderr = 1;
        }

    next:

        log = log->next;
    }

    if (!ngx_use_stderr
        || level > NGX_LOG_WARN
        || wrote_stderr) /* 如果满足这些条件，则不会输出打印到前台，只会写到errlog文件中 */
    {
        return;
    }

    msg -= (7 + err_levels[level].len + 3);

    (void) ngx_sprintf(msg, "nginx: [%V] ", &err_levels[level]);
    (void) ngx_write_console(ngx_stderr, msg, p - msg);
}


#if (NGX_HAVE_VARIADIC_MACROS)
void
ngx_log_error_coreall(ngx_uint_t level, ngx_log_t *log, const char* filename, int lineno, ngx_err_t err,
    const char *fmt, ...) 
//这里打印一定要注意，例如位标记用%d %u打印就会出现段错误，例如用%d打印ngx_event_t->write;
//例如打印ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0, "http upstream request(ev->write:%u %u)  %V", ngx_event_t->write, ngx_event_t->write); 段错误
#else

void
ngx_log_error_coreall(ngx_uint_t level, ngx_log_t *log, const char* filename, int lineno, ngx_err_t err,
    const char *fmt, va_list args)

#endif

{
#if (NGX_HAVE_VARIADIC_MACROS)
    va_list      args;
#endif
    u_char      *p, *last, *msg;
    ssize_t      n;
    ngx_uint_t   wrote_stderr;//, debug_connection;
    u_char       errstr[NGX_MAX_ERROR_STR];
    char filebuf[52];

    last = errstr + NGX_MAX_ERROR_STR;
    
    p = ngx_cpymem(errstr, ngx_cached_err_log_time.data,
                   ngx_cached_err_log_time.len);

    snprintf(filebuf, sizeof(filebuf), "[%35s, %5d][yangyazhou @@@ test]", filename, lineno);

    p = ngx_slprintf(p, last, "%s ", filebuf);  
    
    p = ngx_slprintf(p, last, " [%V] ", &err_levels[level]);

    /* pid#tid */
    p = ngx_slprintf(p, last, "%P#" NGX_TID_T_FMT ": ", ngx_log_pid, ngx_log_tid); //进程ID和线程ID(在开启线程池的时候线程ID和进程ID不同)
    
    if (log->connection) {
        p = ngx_slprintf(p, last, "*%uA ", log->connection);
    }

    msg = p;

#if (NGX_HAVE_VARIADIC_MACROS)

    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

#else

    p = ngx_vslprintf(p, last, fmt, args);

#endif

    if (err) {
        p = ngx_log_errno(p, last, err);
    }

    if (level != NGX_LOG_DEBUG && log->handler) {
        p = log->handler(log, p, last - p);
    }

    if (p > last - NGX_LINEFEED_SIZE) {
        p = last - NGX_LINEFEED_SIZE;
    }

    ngx_linefeed(p);

    wrote_stderr = 0;
    //debug_connection = (log->log_level & NGX_LOG_DEBUG_CONNECTION) != 0;

    while (log) {

        if (log->writer) {
            log->writer(log, level, errstr, p - errstr);
            goto next;
        }

        if (ngx_time() == log->disk_full_time) {

            /*
             * on FreeBSD writing to a full filesystem with enabled softupdates
             * may block process for much longer time than writing to non-full
             * filesystem, so we skip writing to a log for one second
             */

            goto next;
        }

        n = ngx_write_fd(log->file->fd, errstr, p - errstr); //写到log文件中

        if (n == -1 && ngx_errno == NGX_ENOSPC) {
            log->disk_full_time = ngx_time();
        }

        if (log->file->fd == ngx_stderr) {
            wrote_stderr = 1;
        }

    next:

        log = log->next;
    }

    if (!ngx_use_stderr
        || level > NGX_LOG_WARN
        || wrote_stderr) /* 如果满足这些条件，则不会输出打印到前台，只会写到errlog文件中 */
    {
        return;
    }

    msg -= (7 + err_levels[level].len + 3);

    (void) ngx_sprintf(msg, "nginx: [%V] ", &err_levels[level]);
    (void) ngx_write_console(ngx_stderr, msg, p - msg);
}


#if !(NGX_HAVE_VARIADIC_MACROS)

void ngx_cdecl
ngx_log_error(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...)
{
    va_list  args;

    if (log->log_level >= level) {
        va_start(args, fmt);
        ngx_log_error_core(level, log,__FUNCTION__, __LINE__, err, fmt, args);
        va_end(args);
    }
}

void ngx_cdecl
ngx_log_errorall(ngx_uint_t level, ngx_log_t *log, ngx_err_t err,
    const char *fmt, ...)
{
    va_list  args;

    va_start(args, fmt);
    ngx_log_error_core(level, log,__FUNCTION__, __LINE__, err, fmt, args);
    va_end(args);
}



void ngx_cdecl
ngx_log_debug_core(ngx_log_t *log, ngx_err_t err, const char *fmt, ...)
{
    va_list  args;

    va_start(args, fmt);
    ngx_log_error_core(NGX_LOG_DEBUG, log,__FUNCTION__, __LINE__, err, fmt, args);
    va_end(args);
}

#endif


void ngx_cdecl
ngx_log_abort(ngx_err_t err, const char *fmt, ...)
{
    u_char   *p;
    va_list   args;
    u_char    errstr[NGX_MAX_CONF_ERRSTR];

    va_start(args, fmt);
    p = ngx_vsnprintf(errstr, sizeof(errstr) - 1, fmt, args);
    va_end(args);

    ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, err,
                  "%*s", p - errstr, errstr);
}


void ngx_cdecl
ngx_log_stderr(ngx_err_t err, const char *fmt, ...)
{
    u_char   *p, *last;
    va_list   args;
    u_char    errstr[NGX_MAX_ERROR_STR];

    last = errstr + NGX_MAX_ERROR_STR;

    p = ngx_cpymem(errstr, "nginx: ", 7);

    va_start(args, fmt);
    p = ngx_vslprintf(p, last, fmt, args);
    va_end(args);

    if (err) {
        p = ngx_log_errno(p, last, err);
    }

    if (p > last - NGX_LINEFEED_SIZE) {
        p = last - NGX_LINEFEED_SIZE;
    }

    ngx_linefeed(p);

    (void) ngx_write_console(ngx_stderr, errstr, p - errstr);
}


u_char *
ngx_log_errno(u_char *buf, u_char *last, ngx_err_t err)
{
    if (buf > last - 50) {

        /* leave a space for an error code */

        buf = last - 50;
        *buf++ = '.';
        *buf++ = '.';
        *buf++ = '.';
    }

#if (NGX_WIN32)
    buf = ngx_slprintf(buf, last, ((unsigned) err < 0x80000000)
                                       ? " (%d: " : " (%Xd: ", err);
#else
    buf = ngx_slprintf(buf, last, " (%d: ", err);
#endif

    buf = ngx_strerror(err, buf, last - buf);

    if (buf < last) {
        *buf++ = ')';
    }

    return buf;
}

//打开NGX_ERROR_LOG_PATH文件
ngx_log_t *ngx_log_init(u_char *prefix)
{
    u_char  *p, *name;
    size_t   nlen, plen;

    ngx_log.file = &ngx_log_file;
    ngx_log.log_level = NGX_LOG_NOTICE;

    name = (u_char *) NGX_ERROR_LOG_PATH;

    /*
     * we use ngx_strlen() here since BCC warns about
     * condition is always false and unreachable code
     */

    nlen = ngx_strlen(name);

    if (nlen == 0) {
        ngx_log_file.fd = ngx_stderr;
        return &ngx_log;
    }

    p = NULL;

#if (NGX_WIN32)
    if (name[1] != ':') {
#else
    if (name[0] != '/') {
#endif

        if (prefix) {
            plen = ngx_strlen(prefix);

        } else {
#ifdef NGX_PREFIX
            prefix = (u_char *) NGX_PREFIX;
            plen = ngx_strlen(prefix);
#else
            plen = 0;
#endif
        }

        if (plen) {
            name = malloc(plen + nlen + 2); //"NGX_PREFIX/NGX_ERROR_LOG_PATH"
            if (name == NULL) {
                return NULL;
            }

            p = ngx_cpymem(name, prefix, plen);

            if (!ngx_path_separator(*(p - 1))) {
                *p++ = '/';
            }

            ngx_cpystrn(p, (u_char *) NGX_ERROR_LOG_PATH, nlen + 1);

            p = name;
        }
    }

    ngx_log_file.fd = ngx_open_file(name, NGX_FILE_APPEND,
                                    NGX_FILE_CREATE_OR_OPEN,
                                    NGX_FILE_DEFAULT_ACCESS);//打开logs/error.log文件

    if (ngx_log_file.fd == NGX_INVALID_FILE) {
        ngx_log_stderr(ngx_errno,
                       "[alert] could not open error log file: "
                       ngx_open_file_n " \"%s\" failed", name);
#if (NGX_WIN32)
        ngx_event_log(ngx_errno,
                       "could not open error log file: "
                       ngx_open_file_n " \"%s\" failed", name);
#endif

        ngx_log_file.fd = ngx_stderr;
    }

    if (p) {
        ngx_free(p);
    }

    return &ngx_log;
}

/*
如果配置文件中没有error_log配置项，在配置文件解析完后调用errlog模块的ngx_log_open_default函数将日志等级默认置为NGX_LOG_ERR，
日志文件设置为NGX_ERROR_LOG_PATH（该宏是在configure时指定的）。
*/
ngx_int_t
ngx_log_open_default(ngx_cycle_t *cycle)
{
    ngx_log_t         *log;
    static ngx_str_t   error_log = ngx_string(NGX_ERROR_LOG_PATH);

    /* 配置文件中不存在生效的error_log配置项时new_log.file就为空 */  
    if (ngx_log_get_file_log(&cycle->new_log) != NULL) {
        return NGX_OK;
    }

    if (cycle->new_log.log_level != 0) {
        /* there are some error logs, but no files */

        log = ngx_pcalloc(cycle->pool, sizeof(ngx_log_t));
        if (log == NULL) {
            return NGX_ERROR;
        }

    } else {
        /* no error logs at all */
        log = &cycle->new_log;
    }

    log->log_level = NGX_LOG_ERR;

    log->file = ngx_conf_open_file(cycle, &error_log);
    if (log->file == NULL) {
        return NGX_ERROR;
    }

    if (log != &cycle->new_log) {
        ngx_log_insert(&cycle->new_log, log);
    }

    return NGX_OK;
}

//把cycle->log fd设置为STDERR_FILENO
ngx_int_t ngx_log_redirect_stderr(ngx_cycle_t *cycle)
{
    ngx_fd_t  fd;

    if (cycle->log_use_stderr) {
        return NGX_OK;
    }

    /* file log always exists when we are called */
    fd = ngx_log_get_file_log(cycle->log)->file->fd;

    if (fd != ngx_stderr) {
        if (ngx_set_stderr(fd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          ngx_set_stderr_n " failed");

            return NGX_ERROR;
        }
    }

    return NGX_OK;
}

ngx_log_t *
ngx_log_get_file_log(ngx_log_t *head)
{
    ngx_log_t  *log;

    for (log = head; log; log = log->next) {
        if (log->file != NULL) {
            return log;
        }
    }

    return NULL;
}

/*

语法:  error_log file | stderr [debug | info | notice | warn | error | crit | alert | emerg];
 
默认值:  error_log logs/error.log error;
 
上下文:  main, http, server, location
 
配置日志。 
第一个参数定义了存放日志的文件。 如果设置为特殊值stderr，nginx会将日志输出到标准错误输出。 
第二个参数定义日志级别。 日志级别在上面已经按严重性由轻到重的顺序列出。 设置为某个日志级别将会使指定级别和更高级别的日志都被记录下来。 
比如，默认级别error会使nginx记录所有error、crit、alert和emerg级别的消息。 如果省略这个参数，nginx会使用error。 
为了使debug日志工作，需要添加--with-debug编译选项。 



error_log file [ debug | info | notice | warn | error | crit ]  | [{  debug_core | debug_alloc | debug_mutex | debug_event | debug_http | debug_mail | debug_mysql } ]
日志级别 = 错误日志级别 | 调试日志级别; 或者
日志级别 = 错误日志级别;
错误日志的级别: emerg, alert, crit, error, warn, notic, info, debug, 
调试日志的级别: debug_core, debug_alloc, debug_mutex, debug_event, debug_http, debug_mail, debug_mysql,

 error_log 指令的日志级别配置分为 错误日志级别和调试日志级别
 且 错误日志只能设置一个级别 且 错误日志必须书写在调试日志级别的前面 且 调试日志可以设置多个级别
 其他配置方法可能达不到你的预期. 
*/
static char *
ngx_log_set_levels(ngx_conf_t *cf, ngx_log_t *log)
{
    ngx_uint_t   i, n, d, found;
    ngx_str_t   *value;

    if (cf->args->nelts == 2) {
        log->log_level = NGX_LOG_ERR;
        return NGX_CONF_OK;
    }

    value = cf->args->elts;

    for (i = 2; i < cf->args->nelts; i++) {
        found = 0;

        for (n = 1; n <= NGX_LOG_DEBUG; n++) {
            if (ngx_strcmp(value[i].data, err_levels[n].data) == 0) {

                if (log->log_level != 0) {
                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                       "duplicate log level \"%V\"",
                                       &value[i]);
                    return NGX_CONF_ERROR;
                }

                log->log_level = n;
                found = 1;
                break;
            }
        }

        for (n = 0, d = NGX_LOG_DEBUG_FIRST; d <= NGX_LOG_DEBUG_LAST; d <<= 1) {
            if (ngx_strcmp(value[i].data, debug_levels[n++]) == 0) {
                if (log->log_level & ~NGX_LOG_DEBUG_ALL) {
                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                       "invalid log level \"%V\"",
                                       &value[i]);
                    return NGX_CONF_ERROR;
                }

                log->log_level |= d; //设置调试开关
                found = 1;
                break;
            }
        }


        if (!found) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid log level \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }
    }

    if (log->log_level == NGX_LOG_DEBUG) {
        log->log_level = NGX_LOG_DEBUG_ALL;
    }

    
    return NGX_CONF_OK;
}

/* 全局中配置的error_log xxx存储在ngx_cycle_s->new_log，http{}、server{}、local{}配置的error_log保存在ngx_http_core_loc_conf_t->error_log,
   见ngx_log_set_log,如果只配置全局error_log，不配置http{}、server{}、local{}则在ngx_http_core_merge_loc_conf conf->error_log = &cf->cycle->new_log;  */ 
static char *
ngx_error_log(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_log_t  *dummy;

    dummy = &cf->cycle->new_log; 

    return ngx_log_set_log(cf, &dummy);
}



/*

语法:  error_log file | stderr [debug | info | notice | warn | error | crit | alert | emerg];
 
默认值:  error_log logs/error.log error;
 
上下文:  main, http, server, location
 
配置日志。 
第一个参数定义了存放日志的文件。 如果设置为特殊值stderr，nginx会将日志输出到标准错误输出。 
第二个参数定义日志级别。 日志级别在上面已经按严重性由轻到重的顺序列出。 设置为某个日志级别将会使指定级别和更高级别的日志都被记录下来。 
比如，默认级别error会使nginx记录所有error、crit、alert和emerg级别的消息。 如果省略这个参数，nginx会使用error。 
为了使debug日志工作，需要添加--with-debug编译选项。 



error_log file [ debug | info | notice | warn | error | crit ]  | [{  debug_core | debug_alloc | debug_mutex | debug_event | debug_http | debug_mail | debug_mysql } ]
日志级别 = 错误日志级别 | 调试日志级别; 或者
日志级别 = 错误日志级别;
错误日志的级别: emerg, alert, crit, error, warn, notic, info, debug, 
调试日志的级别: debug_core, debug_alloc, debug_mutex, debug_event, debug_http, debug_mail, debug_mysql,

 error_log 指令的日志级别配置分为 错误日志级别和调试日志级别
 且 错误日志只能设置一个级别 且 错误日志必须书写在调试日志级别的前面 且 调试日志可以设置多个级别
 其他配置方法可能达不到你的预期. 
*/  /* 全局中配置的error_log xxx存储在ngx_cycle_s->new_log，http{}、server{}、local{}配置的error_log保存在ngx_http_core_loc_conf_t->error_log,
    见ngx_log_set_log,如果只配置全局error_log，不配置http{}、server{}、local{}则在ngx_http_core_merge_loc_conf conf->error_log = &cf->cycle->new_log;  */
    
char *
ngx_log_set_log(ngx_conf_t *cf, ngx_log_t **head)
{
    ngx_log_t          *new_log;
    ngx_str_t          *value, name;
    ngx_syslog_peer_t  *peer;

    if (*head != NULL && (*head)->log_level == 0) {
        new_log = *head;

    } else {

        new_log = ngx_pcalloc(cf->pool, sizeof(ngx_log_t));
        if (new_log == NULL) {
            return NGX_CONF_ERROR;
        }

        if (*head == NULL) {
            *head = new_log;
        }
    }

    value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "stderr") == 0) {
        ngx_str_null(&name);
        cf->cycle->log_use_stderr = 1;

        new_log->file = ngx_conf_open_file(cf->cycle, &name);
        if (new_log->file == NULL) {
            return NGX_CONF_ERROR;
        }

     } else if (ngx_strncmp(value[1].data, "memory:", 7) == 0) {

#if (NGX_DEBUG)
        size_t                 size, needed;
        ngx_pool_cleanup_t    *cln;
        ngx_log_memory_buf_t  *buf;

        value[1].len -= 7;
        value[1].data += 7;

        needed = sizeof("MEMLOG  :" NGX_LINEFEED)
                 + cf->conf_file->file.name.len
                 + NGX_SIZE_T_LEN
                 + NGX_INT_T_LEN
                 + NGX_MAX_ERROR_STR;

        size = ngx_parse_size(&value[1]);

        if (size == (size_t) NGX_ERROR || size < needed) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid buffer size \"%V\"", &value[1]);
            return NGX_CONF_ERROR;
        }

        buf = ngx_pcalloc(cf->pool, sizeof(ngx_log_memory_buf_t));
        if (buf == NULL) {
            return NGX_CONF_ERROR;
        }

        buf->start = ngx_pnalloc(cf->pool, size);
        if (buf->start == NULL) {
            return NGX_CONF_ERROR;
        }

        buf->end = buf->start + size;

        buf->pos = ngx_slprintf(buf->start, buf->end, "MEMLOG %uz %V:%ui%N",
                                size, &cf->conf_file->file.name,
                                cf->conf_file->line);

        ngx_memset(buf->pos, ' ', buf->end - buf->pos);

        cln = ngx_pool_cleanup_add(cf->pool, 0);
        if (cln == NULL) {
            return NGX_CONF_ERROR;
        }

        cln->data = new_log;
        cln->handler = ngx_log_memory_cleanup;

        new_log->writer = ngx_log_memory_writer;
        new_log->wdata = buf;

#else
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "nginx was built without debug support");
        return NGX_CONF_ERROR;
#endif

     } else if (ngx_strncmp(value[1].data, "syslog:", 7) == 0) {
        peer = ngx_pcalloc(cf->pool, sizeof(ngx_syslog_peer_t));
        if (peer == NULL) {
            return NGX_CONF_ERROR;
        }

        if (ngx_syslog_process_conf(cf, peer) != NGX_CONF_OK) {
            return NGX_CONF_ERROR;
        }

        new_log->writer = ngx_syslog_writer;
        new_log->wdata = peer;

    } else {
        new_log->file = ngx_conf_open_file(cf->cycle, &value[1]);
        if (new_log->file == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    if (ngx_log_set_levels(cf, new_log) != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    if (*head != new_log) {
        ngx_log_insert(*head, new_log);
    }

    return NGX_CONF_OK;
}

//日志对象队列按日志等级从低到高排序
static void
ngx_log_insert(ngx_log_t *log, ngx_log_t *new_log)
{
    ngx_log_t  tmp;

    if (new_log->log_level > log->log_level) {

        /*
         * list head address is permanent, insert new log after
         * head and swap its contents with head
         */

        tmp = *log;
        *log = *new_log;
        *new_log = tmp;

        log->next = new_log;
        return;
    }

    while (log->next) {
        if (new_log->log_level > log->next->log_level) {
            new_log->next = log->next;
            log->next = new_log;
            return;
        }

        log = log->next;
    }

    log->next = new_log;
}


#if (NGX_DEBUG)

static void
ngx_log_memory_writer(ngx_log_t *log, ngx_uint_t level, u_char *buf,
    size_t len)
{
    u_char                *p;
    size_t                 avail, written;
    ngx_log_memory_buf_t  *mem;

    mem = log->wdata;

    if (mem == NULL) {
        return;
    }

    written = ngx_atomic_fetch_add(&mem->written, len);

    p = mem->pos + written % (mem->end - mem->pos);

    avail = mem->end - p;

    if (avail >= len) {
        ngx_memcpy(p, buf, len);

    } else {
        ngx_memcpy(p, buf, avail);
        ngx_memcpy(mem->pos, buf + avail, len - avail);
    }
}


static void
ngx_log_memory_cleanup(void *data)
{
    ngx_log_t *log = data;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, log, 0, "destroy memory log buffer");

    log->wdata = NULL;
}

#endif
