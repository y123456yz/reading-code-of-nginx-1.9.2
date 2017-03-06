
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <nginx.h>


static ngx_int_t ngx_add_inherited_sockets(ngx_cycle_t *cycle);
static ngx_int_t ngx_get_options(int argc, char *const *argv);
static ngx_int_t ngx_process_options(ngx_cycle_t *cycle);
static ngx_int_t ngx_save_argv(ngx_cycle_t *cycle, int argc, char *const *argv);
static void *ngx_core_module_create_conf(ngx_cycle_t *cycle);
static char *ngx_core_module_init_conf(ngx_cycle_t *cycle, void *conf);
static char *ngx_set_user(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_set_env(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_set_priority(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_set_cpu_affinity(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_set_worker_processes(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


static ngx_conf_enum_t  ngx_debug_points[] = {
    { ngx_string("stop"), NGX_DEBUG_POINTS_STOP },
    { ngx_string("abort"), NGX_DEBUG_POINTS_ABORT },
    { ngx_null_string, 0 }
};
//相关配置见ngx_event_core_commands ngx_http_core_commands ngx_stream_commands ngx_http_core_commands ngx_core_commands  ngx_mail_commands

//对应的存放参数的值的结构体为ngx_core_conf_t
static ngx_command_t  ngx_core_commands[] = {
    //daemon on|off 是否已守护进程方式运行，守护进程是脱离终端在后台运行的进程，脱离终端是避免进程执行过程中的打印在任何终端上面显示
    { ngx_string("daemon"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_core_conf_t, daemon),
      NULL },

    //是否以master/slave方式运行  master_process on | off,如果以master/slave方式运行将以slave来接收连接，否则以master接收连接
    { ngx_string("master_process"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_core_conf_t, master),
      NULL },

    //timer_resolution t表示至少t秒后才调用一次gettimeofday
    /*
     如果nginx.conf配置文件中设置了timer_resolution酡置项，即表明需要控制时间精度，这时会调用setitimer方法，设置时间间隔
     为timer_resolution毫秒来回调ngx_timer_signal_handler方法
     */ //timer_resolution这个参数加上可以保证定时器每个这么多秒中断一次，从而可以从epoll中返回，并跟新时间，判断哪些事件有超时，执行超时事件，例如客户端继上次
     //发请求过来，隔了client_header_timeout时间后还没有新请求过来，这会关闭连接
    { ngx_string("timer_resolution"), //单位是s
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      0,
      offsetof(ngx_core_conf_t, timer_resolution),
      NULL },

    //设置pid文件路径
    { ngx_string("pid"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_core_conf_t, pid),
      NULL },
    //lock_file logs/nginx.lock，如果不打开lock_file，则该nginx.lock文件不生效，没作用，如果打开，则开操作系统是否支持原子锁，如果不支持则用文件锁实现
    //一般linux是支持原子锁的，所以该文件没有意义
    /*
      见ngx_trylock_fd
     */
    { ngx_string("lock_file"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_core_conf_t, lock_file),
      NULL },

    //worker_processes 4设置进程个数
    { ngx_string("worker_processes"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_set_worker_processes,
      0,
      0,
      NULL },
    //debug_points [stop|abort] nginx在一些关键逻辑错误处设置了调试点，如果设置为stop,nginx在执行到这些调试点讲发出SIGSTOP信号以用以调试。
    //如果设置为abort则在这些调试点会产生coredump文件，从而可以使用gdb查看nginx当时的各种信息
    { ngx_string("debug_points"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      0,
      offsetof(ngx_core_conf_t, debug_points),
      &ngx_debug_points },
    //worker进程运行的用户和用户组  user username [groupname],不设置groupname则group默认为username
    { ngx_string("user"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE12,
      ngx_set_user,
      0,
      0,
      NULL },

    //进程优先级，取值范围-20 - 19 越小优先权越高
    { ngx_string("worker_priority"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_set_priority,
      0,
      0,
      NULL },

     //worker_processes 4设置进程个数  worker_cpu_affinity 1000 0100 0010 0001
    { ngx_string("worker_cpu_affinity"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_1MORE,
      ngx_set_cpu_affinity,
      0,
      0,
      NULL },
    //设置worker进程打开文件描述符的最大个数
    { ngx_string("worker_rlimit_nofile"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      0,
      offsetof(ngx_core_conf_t, rlimit_nofile),
      NULL },
    //设置coredump文件的大小
    { ngx_string("worker_rlimit_core"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_off_slot,
      0,
      offsetof(ngx_core_conf_t, rlimit_core),
      NULL },
    //设置coredump path文件的产生路径
    { ngx_string("working_directory"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      0,
      offsetof(ngx_core_conf_t, working_directory),
      NULL },

    //设置系统环境变量
    { ngx_string("env"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_set_env,
      0,
      0,
      NULL },

      ngx_null_command
};

//http{}外的配置见ngx_core_module_ctx， http{}内的配置见ngx_http_module
static ngx_core_module_t  ngx_core_module_ctx = {
    ngx_string("core"),
    ngx_core_module_create_conf,
    ngx_core_module_init_conf
};

ngx_module_t  ngx_core_module = {
    NGX_MODULE_V1,
    &ngx_core_module_ctx,                    /* module context */
    ngx_core_commands,                       /* module directives */
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

ngx_uint_t          ngx_max_module; //模块总数

static ngx_uint_t   ngx_show_help;
static ngx_uint_t   ngx_show_version;
static ngx_uint_t   ngx_show_configure;
static u_char      *ngx_prefix; //赋值见ngx_log_init，值为NGX_PREFIX 或者为nginx -p参数携带的参数
static u_char      *ngx_conf_file; //nginx -c参数携带的参数
static u_char      *ngx_conf_params;

/*
如果加了-s 参数  这个记录的是-s参数带的参数，在main中的
if (ngx_signal) { //加了-S参数
        return ngx_signal_process(cycle, ngx_signal);
    }
*/ //信号发送ngx_signal_process
static char        *ngx_signal; //stop, quit, reopen, reload


static char **ngx_os_environ;

//1.时间、正则、错误日志、ssl等初始化
//2.读入命令行参数
//3.OS相关初始化
//4.读入并解析配置
//5.核心模块初始化
//6.创建各种临时文件和目录
//7.创建共享内存
//8.打开listen的端口
//9.所有模块初始化
//10.启动worker进程
int ngx_cdecl
main(int argc, char *const *argv)
{
    ngx_buf_t        *b;
    ngx_log_t        *log;
    ngx_uint_t        i;
    ngx_cycle_t      *cycle, init_cycle;
    ngx_conf_dump_t  *cd;
    ngx_core_conf_t  *ccf;

    ngx_debug_init();

    if (ngx_strerror_init() != NGX_OK) {
        return 1;
    }

    //获取参数和配置参数，比如命令是nginx -v 那么ngx_show_version就设置为1
    if (ngx_get_options(argc, argv) != NGX_OK) { //解析命令参数
        return 1;
    }

    if (ngx_show_version) {
        ngx_write_stderr("nginx version: " NGINX_VER_BUILD NGX_LINEFEED);

        if (ngx_show_help) {
            ngx_write_stderr(
                "Usage: nginx [-?hvVtTq] [-s signal] [-c filename] "
                             "[-p prefix] [-g directives]" NGX_LINEFEED
                             NGX_LINEFEED
                "Options:" NGX_LINEFEED
                "  -?,-h         : this help" NGX_LINEFEED
                "  -v            : show version and exit" NGX_LINEFEED
                "  -V            : show version and configure options then exit" //除了version外还可以显示操作系统和configure阶段等相关信息
                                   NGX_LINEFEED
                "  -t            : test configuration and exit" NGX_LINEFEED  //不启动nginx进程，只是测试配置文件是否有错误
                "  -T            : test configuration, dump it and exit"
                                   NGX_LINEFEED
                "  -q            : suppress non-error messages "
                                   "during configuration testing" NGX_LINEFEED //通过-t测试配置文件是否错误的时候,nginx -t -q可以把error级别以下的日志不输出的屏幕
                "  -s signal     : send signal to a master process: "
                                   "stop, quit, reopen, reload" NGX_LINEFEED
#ifdef NGX_PREFIX
                "  -p prefix     : set prefix path (default: "
                                   NGX_PREFIX ")" NGX_LINEFEED  //指定安装目录  
#else
                "  -p prefix     : set prefix path (default: NONE)" NGX_LINEFEED
#endif
                "  -c filename   : set configuration file (default: "
                                   NGX_CONF_PATH ")" NGX_LINEFEED //指定配置文件
                "  -g directives : set global directives out of configuration "
                                   "file" NGX_LINEFEED NGX_LINEFEED
                );
        }

        if (ngx_show_configure) {

#ifdef NGX_COMPILER
            ngx_write_stderr("built by " NGX_COMPILER NGX_LINEFEED);
#endif

#if (NGX_SSL)
            if (SSLeay() == SSLEAY_VERSION_NUMBER) {
                ngx_write_stderr("built with " OPENSSL_VERSION_TEXT
                                 NGX_LINEFEED);
            } else {
                ngx_write_stderr("built with " OPENSSL_VERSION_TEXT
                                 " (running with ");
                ngx_write_stderr((char *) (uintptr_t)
                                 SSLeay_version(SSLEAY_VERSION));
                ngx_write_stderr(")" NGX_LINEFEED);
            }
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
            ngx_write_stderr("TLS SNI support enabled" NGX_LINEFEED);
#else
            ngx_write_stderr("TLS SNI support disabled" NGX_LINEFEED);
#endif
#endif

            ngx_write_stderr("configure arguments:" NGX_CONFIGURE NGX_LINEFEED);
        }

        if (!ngx_test_config) {
            return 0;
        }
    }

    /* TODO */ ngx_max_sockets = -1;

    ngx_time_init(); //初始化nginx环境的当前时间

#if (NGX_PCRE)
    ngx_regex_init();
#endif

    ngx_pid = ngx_getpid();

    /*
    主进程启动的时候，此时还没有读取配置文件，即没有指定日志打印在哪里。nginx这时候虽然可以将一些出错内容或者结果输到标准输出，但是如果要记录一些系统初始化情况，
socket监听状况，还是需要写到日志文件中去的。在nginx的main函数中，首先会调用ngx_log_init 函数，默认日志文件为：安装路径/logs/error.log，如果这个文件没有权限访问的话，
会直接报错退出。在mian函数结尾处，在ngx_master_process_cycle函数调用之前，会close掉这个日志文件。
     */
    log = ngx_log_init(ngx_prefix);
    if (log == NULL) {
        return 1;
    }

    /* STUB */
#if (NGX_OPENSSL)
    ngx_ssl_init(log);
#endif

    /*
     * init_cycle->log is required for signal handlers and
     * ngx_process_options()
     */

    ngx_memzero(&init_cycle, sizeof(ngx_cycle_t));
    init_cycle.log = log;
    ngx_cycle = &init_cycle;

    init_cycle.pool = ngx_create_pool(1024, log);
    if (init_cycle.pool == NULL) {
        return 1;
    }

    if (ngx_save_argv(&init_cycle, argc, argv) != NGX_OK) {
        return 1;
    }

    if (ngx_process_options(&init_cycle) != NGX_OK) {
        return 1;
    }

    if (ngx_os_init(log) != NGX_OK) {
        return 1;
    }

    /*
     * ngx_crc32_table_init() requires ngx_cacheline_size set in ngx_os_init()
     */

    if (ngx_crc32_table_init() != NGX_OK) {
        return 1;
    }

    if (ngx_add_inherited_sockets(&init_cycle) != NGX_OK) {
        return 1;
    }

    ngx_max_module = 0;
    for (i = 0; ngx_modules[i]; i++) {
        ngx_modules[i]->index = ngx_max_module++;
    }

    cycle = ngx_init_cycle(&init_cycle);
    if (cycle == NULL) {
        if (ngx_test_config) {
            ngx_log_stderr(0, "configuration file %s test failed",
                           init_cycle.conf_file.data);
        }

        return 1;
    }

    if (ngx_test_config) {
        if (!ngx_quiet_mode) {
            ngx_log_stderr(0, "configuration file %s test is successful",
                           cycle->conf_file.data);
        }

        if (ngx_dump_config) {
            cd = cycle->config_dump.elts;

            for (i = 0; i < cycle->config_dump.nelts; i++) {

                ngx_write_stdout("# configuration file ");
                (void) ngx_write_fd(ngx_stdout, cd[i].name.data,
                                    cd[i].name.len);
                ngx_write_stdout(":" NGX_LINEFEED);

                b = cd[i].buffer;

                (void) ngx_write_fd(ngx_stdout, b->pos, b->last - b->pos);
                ngx_write_stdout(NGX_LINEFEED);
            }
        }

        return 0;
    }

    if (ngx_signal) { //加了-S参数
        return ngx_signal_process(cycle, ngx_signal);
    }


    ngx_os_status(cycle->log);

    ngx_cycle = cycle;

    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    if (ccf->master && ngx_process == NGX_PROCESS_SINGLE) { //在这里会把进程模式设置为MASTER模式
        ngx_process = NGX_PROCESS_MASTER;
    }

#if !(NGX_WIN32)

    if (ngx_init_signals(cycle->log) != NGX_OK) {
        return 1;
    }

    if (!ngx_inherited && ccf->daemon) {
        if (ngx_daemon(cycle->log) != NGX_OK) {
            return 1;
        }

        ngx_daemonized = 1;
    }

    if (ngx_inherited) {
        ngx_daemonized = 1;
    }

#endif

    if (ngx_create_pidfile(&ccf->pid, cycle->log) != NGX_OK) {
        return 1;
    }

    if (ngx_log_redirect_stderr(cycle) != NGX_OK) {
        return 1;
    }

    if (log->file->fd != ngx_stderr) {
        //前面的log = ngx_log_init(ngx_prefix);
        if (ngx_close_file(log->file->fd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          ngx_close_file_n " built-in log failed");
        }
    }

    ngx_use_stderr = 0;

    /*
    如果hginx.conf中配置为单进程工作模式，这时将会调用ngx_single_process_cycle方法进入单迸程工作模式。
    */
    if (ngx_process == NGX_PROCESS_SINGLE) { //如果配置的是单进程工作模式，好像不会走到这里
        ngx_single_process_cycle(cycle);
        
    } else { //一般都是走到这里，master方式
        ngx_master_process_cycle(cycle);

    }

    return 0;
}

/*
在执行不重启服务升级Nginx的操作时，老的Nginx进程会通过环境变量“NGINX”来传递需要打开的监听端口，
新的Nginx进程会通过ngx_add_inherited_sockets方法来使用已经打开的TCP监听端口,不采用这种方式的话会报错，说该端口已经bind   

ngx_add_inherited_sockets 函数通过环境变量NGINX完成socket的继承，继承来的socket将会放到init_cycle的listening数组中。在NGINX环
境变量中，每个socket中间用冒号或分号隔开。完成继承同时设置全局变量ngx_inherited为1
*/
/*  
Nginx在不重启服务升级时，也就是我们说过的平滑升级时，它会不重启master进程而启动新版本的Nginx程序。这样，旧版本的
master进程会通过execve系统调用来启动新版本的master进程（先fork出子进程再调用exec来运行新程序），这时旧版本的master
进程必须要通过一种方式告诉新版本的master进程这是在平滑升级，并且传递一些必要的信息。Nginx是通过环境变量来传递这些
信息的，新版本的master进程通过ngx_add_inherited_sockets方法由环境变量里读取平滑升级信息，并对旧版本Nginx服务监听的句柄做继承处理。
*/
static ngx_int_t
ngx_add_inherited_sockets(ngx_cycle_t *cycle)  //ngx_add_inherited_sockets和ngx_exec_new_binary对应
{
    u_char           *p, *v, *inherited;
    ngx_int_t         s;
    ngx_listening_t  *ls;

    //getenv()用来取得参数envvar环境变量的内容。参数envvar为环境变量的名称，如果该变量存在则会返回指向该内容的指针
    inherited = (u_char *) getenv(NGINX_VAR); //获取环境变量 这里的"NGINX_VAR"是宏定义，值为"NGINX"   
    if (inherited == NULL) {
        return NGX_OK;
    }
    ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0,
                  "using inherited sockets from \"%s\"", inherited);

    /* 如果是热升级nginx的时候inherit不为NULL，走到这里，配合ngx_exec_new_binary阅读 */

    //初始化ngx_cycle.listening数组，并且数组中包含10个元素   
    if (ngx_array_init(&cycle->listening, cycle->pool, 10,
                       sizeof(ngx_listening_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    for (p = inherited, v = p; *p; p++) { //遍历环境变量   
        if (*p == ':' || *p == ';') {//环境变量的值以':'or';'分开   
            s = ngx_atoi(v, p - v); //转换十进制sockets   
            if (s == NGX_ERROR) {
                ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                              "invalid socket number \"%s\" in " NGINX_VAR
                              " environment variable, ignoring the rest"
                              " of the variable", v);
                break;
            }

            v = p + 1;

            ls = ngx_array_push(&cycle->listening); //返回新分配的数组指针地址(在参考的blog里面这里解释可能有点错误)   
            if (ls == NULL) {
                return NGX_ERROR;
            }

            ngx_memzero(ls, sizeof(ngx_listening_t));

            ls->fd = (ngx_socket_t) s;//保存socket文件描述符到数组中
        }
    }

    ngx_inherited = 1;  //表示已经的得到要继承的socket   

    return ngx_set_inherited_sockets(cycle);
}


char **
ngx_set_environment(ngx_cycle_t *cycle, ngx_uint_t *last)
{
    char             **p, **env;
    ngx_str_t         *var;
    ngx_uint_t         i, n;
    ngx_core_conf_t   *ccf;

    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    if (last == NULL && ccf->environment) {
        return ccf->environment;
    }

    var = ccf->env.elts;
    
    for (i = 0; i < ccf->env.nelts; i++) {
        if (ngx_strcmp(var[i].data, "TZ") == 0
            || ngx_strncmp(var[i].data, "TZ=", 3) == 0)
        {
            goto tz_found;
        }
    }

    /* 先添加TZ到数组ccf->env */
    var = ngx_array_push(&ccf->env);
    if (var == NULL) {
        return NULL;
    }

    var->len = 2;
    var->data = (u_char *) "TZ";

    var = ccf->env.elts;

tz_found:

    n = 0;

    /* 计算出有多少个环境变量，然后存入n变量中 */
    for (i = 0; i < ccf->env.nelts; i++) {
        if (var[i].data[var[i].len] == '=') { //例如kye=   =号后面没有值则continue
            n++;
            continue;
        }

        for (p = ngx_os_environ; *p; p++) {
            
            if (ngx_strncmp(*p, var[i].data, var[i].len) == 0
                && (*p)[var[i].len] == '=') //说明是key=value格式
            {
                n++;
                break;
            }
        }
    }

    if (last) { //last表示需要多分配last个环境变量，+1的原因是末尾是一个NULL，见后面的env[n] = NULL;
        env = ngx_alloc((*last + n + 1) * sizeof(char *), cycle->log);
        *last = n;

    } else {
        env = ngx_palloc(cycle->pool, (n + 1) * sizeof(char *));
    }

    if (env == NULL) {
        return NULL;
    }

    n = 0;

    //把ccf->env数组中的环境变量全部存入env数组中
    for (i = 0; i < ccf->env.nelts; i++) {
        if (var[i].data[var[i].len] == '=') {   //kye=    =号后面没有值
            env[n++] = (char *) var[i].data;
            continue;
        }

        for (p = ngx_os_environ; *p; p++) {

            if (ngx_strncmp(*p, var[i].data, var[i].len) == 0
                && (*p)[var[i].len] == '=') //标准的key=value环境变量
            {
                env[n++] = *p;
                break;
            }
        }
    }

    env[n] = NULL;

    if (last == NULL) {
        ccf->environment = env;
        environ = env;
    }

    return env;
}


/*
由于Nginx只有一个可执行程序，当该可执行程序更新时，就创建一个子进程来执行新的二进制文件。同时要注意的是
老进程通过环境变量将所有的侦听描述字传递给新进程，又因为文件描述字可以跨execve系统调用保留(关于跨exec函
数保留文件描述字，可参考《Unix系统环境高级编程》第二版8.10节：exec函数集)，所以在新进程中这些侦听描述字
可以接受新的连接请求。新进程获取环境变量中的描述字的实现在nginx.c的函数ngx_add_inherited_sockets中


该函数的执行流程如下：
1）构造启动新进程需要的环境变量。该环境变量的内容包括当前进程的环境变量以及当前进程的所有侦听socket描述字。
2）给当前进程的pid文件添上后缀名oldbin。
3）fork一个子进程并触发系统调用execve，该系统调用使用更新后的二进制文件路径作为参数。
*/
ngx_pid_t
ngx_exec_new_binary(ngx_cycle_t *cycle, char *const *argv) //ngx_add_inherited_sockets和ngx_exec_new_binary对应
{
    char             **env, *var;
    u_char            *p;
    ngx_uint_t         i, n;
    ngx_pid_t          pid;
    ngx_exec_ctx_t     ctx;
    ngx_core_conf_t   *ccf;
    ngx_listening_t   *ls;

    ngx_memzero(&ctx, sizeof(ngx_exec_ctx_t));

    ctx.path = argv[0]; //原来启动nginx的时候的路径
    ctx.name = "new binary process";
    ctx.argv = argv; //原来启动nginx时候所带的参数

    n = 2;
    env = ngx_set_environment(cycle, &n);
    if (env == NULL) {
        return NGX_INVALID_PID;
    }

    //把旧master监听的fd写入环境变量NGINX
    var = ngx_alloc(sizeof(NGINX_VAR)
                    + cycle->listening.nelts * (NGX_INT32_LEN + 1) + 2,
                    cycle->log);
    if (var == NULL) {
        ngx_free(env);
        return NGX_INVALID_PID;
    }

    p = ngx_cpymem(var, NGINX_VAR "=", sizeof(NGINX_VAR));

    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) { //把旧master监听的fd写入环境变量NGINX
        p = ngx_sprintf(p, "%ud;", ls[i].fd);
    }

    *p = '\0';

    env[n++] = var;

#if (NGX_SETPROCTITLE_USES_ENV)

    /* allocate the spare 300 bytes for the new binary process title */

    env[n++] = "SPARE=XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
               "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
               "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
               "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
               "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

#endif

    env[n] = NULL;

//#if (NGX_DEBUG)   yang add 调试
    {
    char  **e;
    for (e = env; *e; e++) {
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, cycle->log, 0, "env: %s", *e);
    }
    }
//#endif

    ctx.envp = (char *const *) env;

    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    if (ngx_rename_file(ccf->pid.data, ccf->oldpid.data) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      ngx_rename_file_n " %s to %s failed "
                      "before executing new binary process \"%s\"",
                      ccf->pid.data, ccf->oldpid.data, argv[0]);

        ngx_free(env);
        ngx_free(var);

        return NGX_INVALID_PID;
    }

    pid = ngx_execute(cycle, &ctx);

    if (pid == NGX_INVALID_PID) {
        if (ngx_rename_file(ccf->oldpid.data, ccf->pid.data)//源master进程PID文件重命名为nginx.pid.oldbin
            == NGX_FILE_ERROR)
        {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          ngx_rename_file_n " %s back to %s failed after "
                          "an attempt to execute new binary process \"%s\"",
                          ccf->oldpid.data, ccf->pid.data, argv[0]);
        }
    }

    ngx_free(env);
    ngx_free(var);

    return pid;
}

/*
Command-line parameters
nginx supports the following command-line parameters: 

?-? | -h — print help for command-line parameters. 
?-c file — use an alternative configuration file instead of a default file. 
?-g directives — set global configuration directives, for example, 
nginx -g "pid /var/run/nginx.pid; worker_processes `sysctl -n hw.ncpu`;"
?-p prefix — set nginx path prefix, i.e. a directory that will keep server files (default value is /usr/local/nginx). 
?-q — suppress non-error messages during configuration testing. 
?-s signal — send a signal to the master process. The argument signal can be one of: 
?stop — shut down quickly 
?quit — shut down gracefully 
?reload — reload configuration, start the new worker process with a new configuration, gracefully shut down old worker processes. 
?reopen — reopen log files 
?-t — test the configuration file: nginx checks the configuration for correct syntax, and then tries to open files referred in the configuration. 
?-T — same as -t, but additionally dump configuration files to standard output (1.9.2). 
?-v — print nginx version. 
?-V — print nginx version, compiler version, and configure parameters. 
*/
static ngx_int_t
ngx_get_options(int argc, char *const *argv)
{
    u_char     *p;
    ngx_int_t   i;

    for (i = 1; i < argc; i++) {

        p = (u_char *) argv[i];

        if (*p++ != '-') {
            ngx_log_stderr(0, "invalid option: \"%s\"", argv[i]);
            return NGX_ERROR;
        }

        while (*p) {

            switch (*p++) {

            case '?':
            case 'h':
                ngx_show_version = 1;
                ngx_show_help = 1;
                break;

            case 'v':
                ngx_show_version = 1;
                break;

            case 'V':
                ngx_show_version = 1;
                ngx_show_configure = 1;
                break;

            case 't':
                ngx_test_config = 1;
                break;

            case 'T':
                ngx_test_config = 1;
                ngx_dump_config = 1;
                break;

            case 'q':
                ngx_quiet_mode = 1;
                break;

            case 'p':
                if (*p) {
                    ngx_prefix = p;
                    goto next;
                }

                if (argv[++i]) {
                    ngx_prefix = (u_char *) argv[i];
                    goto next;
                }

                ngx_log_stderr(0, "option \"-p\" requires directory name");
                return NGX_ERROR;

            case 'c':
                if (*p) {
                    ngx_conf_file = p;
                    goto next;
                }

                if (argv[++i]) {
                    ngx_conf_file = (u_char *) argv[i];
                    goto next;
                }

                ngx_log_stderr(0, "option \"-c\" requires file name");
                return NGX_ERROR;

            case 'g':
                if (*p) {
                    ngx_conf_params = p;
                    goto next;
                }

                if (argv[++i]) {
                    ngx_conf_params = (u_char *) argv[i];
                    goto next;
                }

                ngx_log_stderr(0, "option \"-g\" requires parameter");
                return NGX_ERROR;

            case 's':
                if (*p) {
                    ngx_signal = (char *) p;

                } else if (argv[++i]) {
                    ngx_signal = argv[i];

                } else {
                    ngx_log_stderr(0, "option \"-s\" requires parameter");
                    return NGX_ERROR;
                }

                if (ngx_strcmp(ngx_signal, "stop") == 0
                    || ngx_strcmp(ngx_signal, "quit") == 0
                    || ngx_strcmp(ngx_signal, "reopen") == 0
      /* 
         reload实际上是执行reload的nginx进程向原master+worker中的master进程发送reload信号，源master收到后，启动新的worker进程，同时向源worker
         进程发送quit信号，等他们处理完已有的数据信息后，退出，这样就只有新的worker进程运行。见ngx_signal_handler
      */
                    || ngx_strcmp(ngx_signal, "reload") == 0)
                {
                    ngx_process = NGX_PROCESS_SIGNALLER;
                    goto next;
                }

                ngx_log_stderr(0, "invalid option: \"-s %s\"", ngx_signal);
                return NGX_ERROR;

            default:
                ngx_log_stderr(0, "invalid option: \"%c\"", *(p - 1));
                return NGX_ERROR;
            }
        }

    next:

        continue;
    }

    return NGX_OK;
}

/*调用ngx_save_argv()保存命令行参数至全局变量ngx_os_argv、ngx_argc、ngx_argv中；*/
static ngx_int_t
ngx_save_argv(ngx_cycle_t *cycle, int argc, char *const *argv)
{
#if (NGX_FREEBSD)

    ngx_os_argv = (char **) argv;
    ngx_argc = argc;
    ngx_argv = (char **) argv;

#else
    size_t     len;
    ngx_int_t  i;

    ngx_os_argv = (char **) argv;
    ngx_argc = argc;

    ngx_argv = ngx_alloc((argc + 1) * sizeof(char *), cycle->log);
    if (ngx_argv == NULL) {
        return NGX_ERROR;
    }

    for (i = 0; i < argc; i++) {
        len = ngx_strlen(argv[i]) + 1;

        ngx_argv[i] = ngx_alloc(len, cycle->log);
        if (ngx_argv[i] == NULL) {
            return NGX_ERROR;
        }

        (void) ngx_cpystrn((u_char *) ngx_argv[i], (u_char *) argv[i], len);
    }

    ngx_argv[i] = NULL;

#endif

    ngx_os_environ = environ;

    return NGX_OK;
}

//调用ngx_process_options()初始化ngx_cycle的prefix, conf_prefix, conf_file, conf_param等字段；
static ngx_int_t
ngx_process_options(ngx_cycle_t *cycle)
{
    u_char  *p;
    size_t   len;

    if (ngx_prefix) {
        len = ngx_strlen(ngx_prefix);
        p = ngx_prefix;

        if (len && !ngx_path_separator(p[len - 1])) {
            p = ngx_pnalloc(cycle->pool, len + 1);
            if (p == NULL) {
                return NGX_ERROR;
            }

            ngx_memcpy(p, ngx_prefix, len);
            p[len++] = '/';
        }

        cycle->conf_prefix.len = len;
        cycle->conf_prefix.data = p;
        cycle->prefix.len = len;
        cycle->prefix.data = p;

    } else {

#ifndef NGX_PREFIX

        p = ngx_pnalloc(cycle->pool, NGX_MAX_PATH);
        if (p == NULL) {
            return NGX_ERROR;
        }

        if (ngx_getcwd(p, NGX_MAX_PATH) == 0) {
            ngx_log_stderr(ngx_errno, "[emerg]: " ngx_getcwd_n " failed");
            return NGX_ERROR;
        }

        len = ngx_strlen(p);

        p[len++] = '/';

        cycle->conf_prefix.len = len;
        cycle->conf_prefix.data = p;
        cycle->prefix.len = len;
        cycle->prefix.data = p;

#else

#ifdef NGX_CONF_PREFIX
        ngx_str_set(&cycle->conf_prefix, NGX_CONF_PREFIX);
#else
        ngx_str_set(&cycle->conf_prefix, NGX_PREFIX);
#endif
        ngx_str_set(&cycle->prefix, NGX_PREFIX);

#endif
    }

    if (ngx_conf_file) {
        cycle->conf_file.len = ngx_strlen(ngx_conf_file);
        cycle->conf_file.data = ngx_conf_file;

    } else {
        ngx_str_set(&cycle->conf_file, NGX_CONF_PATH);
    }

    if (ngx_conf_full_name(cycle, &cycle->conf_file, 0) != NGX_OK) {
        return NGX_ERROR;
    }

    for (p = cycle->conf_file.data + cycle->conf_file.len - 1;
         p > cycle->conf_file.data;
         p--)
    {
        if (ngx_path_separator(*p)) {
            cycle->conf_prefix.len = p - ngx_cycle->conf_file.data + 1;
            cycle->conf_prefix.data = ngx_cycle->conf_file.data;
            break;
        }
    }

    if (ngx_conf_params) {
        cycle->conf_param.len = ngx_strlen(ngx_conf_params);
        cycle->conf_param.data = ngx_conf_params;
    }

    if (ngx_test_config) {
        cycle->log->log_level = NGX_LOG_INFO;
    }

    return NGX_OK;
}


static void *
ngx_core_module_create_conf(ngx_cycle_t *cycle)
{
    ngx_core_conf_t  *ccf;

    ccf = ngx_pcalloc(cycle->pool, sizeof(ngx_core_conf_t));
    if (ccf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc()
     *
     *     ccf->pid = NULL;
     *     ccf->oldpid = NULL;
     *     ccf->priority = 0;
     *     ccf->cpu_affinity_n = 0;
     *     ccf->cpu_affinity = NULL;
     */

    ccf->daemon = NGX_CONF_UNSET;
    ccf->master = NGX_CONF_UNSET;
    ccf->timer_resolution = NGX_CONF_UNSET_MSEC;

    ccf->worker_processes = NGX_CONF_UNSET;
    ccf->debug_points = NGX_CONF_UNSET;

    ccf->rlimit_nofile = NGX_CONF_UNSET;
    ccf->rlimit_core = NGX_CONF_UNSET;

    ccf->user = (ngx_uid_t) NGX_CONF_UNSET_UINT;
    ccf->group = (ngx_gid_t) NGX_CONF_UNSET_UINT;

    if (ngx_array_init(&ccf->env, cycle->pool, 1, sizeof(ngx_str_t))
        != NGX_OK)
    {
        return NULL;
    }

    return ccf;
}


static char *
ngx_core_module_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_core_conf_t  *ccf = conf;

    ngx_conf_init_value(ccf->daemon, 1);
    ngx_conf_init_value(ccf->master, 1);
    ngx_conf_init_msec_value(ccf->timer_resolution, 0);

    ngx_conf_init_value(ccf->worker_processes, 1);
    ngx_conf_init_value(ccf->debug_points, 0);

#if (NGX_HAVE_CPU_AFFINITY)

    if (ccf->cpu_affinity_n
        && ccf->cpu_affinity_n != 1
        && ccf->cpu_affinity_n != (ngx_uint_t) ccf->worker_processes)
    {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0,
                      "the number of \"worker_processes\" is not equal to "
                      "the number of \"worker_cpu_affinity\" masks, "
                      "using last mask for remaining worker processes");
    }

#endif


    if (ccf->pid.len == 0) {
        ngx_str_set(&ccf->pid, NGX_PID_PATH);
    }

    if (ngx_conf_full_name(cycle, &ccf->pid, 0) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    ccf->oldpid.len = ccf->pid.len + sizeof(NGX_OLDPID_EXT);

    ccf->oldpid.data = ngx_pnalloc(cycle->pool, ccf->oldpid.len);
    if (ccf->oldpid.data == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memcpy(ngx_cpymem(ccf->oldpid.data, ccf->pid.data, ccf->pid.len),
               NGX_OLDPID_EXT, sizeof(NGX_OLDPID_EXT));


#if !(NGX_WIN32)

    if (ccf->user == (uid_t) NGX_CONF_UNSET_UINT && geteuid() == 0) {
        struct group   *grp;
        struct passwd  *pwd;

        ngx_set_errno(0);
        pwd = getpwnam(NGX_USER);
        if (pwd == NULL) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                          "getpwnam(\"" NGX_USER "\") failed");
            return NGX_CONF_ERROR;
        }

        ccf->username = NGX_USER;
        ccf->user = pwd->pw_uid;

        ngx_set_errno(0);
        grp = getgrnam(NGX_GROUP);
        if (grp == NULL) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                          "getgrnam(\"" NGX_GROUP "\") failed");
            return NGX_CONF_ERROR;
        }

        ccf->group = grp->gr_gid;
    }


    if (ccf->lock_file.len == 0) {
        ngx_str_set(&ccf->lock_file, NGX_LOCK_PATH);
    }

    if (ngx_conf_full_name(cycle, &ccf->lock_file, 0) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    {
    ngx_str_t  lock_file;

    lock_file = cycle->old_cycle->lock_file;

    if (lock_file.len) {
        lock_file.len--;

        if (ccf->lock_file.len != lock_file.len
            || ngx_strncmp(ccf->lock_file.data, lock_file.data, lock_file.len)
               != 0)
        {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                          "\"lock_file\" could not be changed, ignored");
        }

        cycle->lock_file.len = lock_file.len + 1;
        lock_file.len += sizeof(".accept");

        cycle->lock_file.data = ngx_pstrdup(cycle->pool, &lock_file);
        if (cycle->lock_file.data == NULL) {
            return NGX_CONF_ERROR;
        }

    } else {
        cycle->lock_file.len = ccf->lock_file.len + 1;
        cycle->lock_file.data = ngx_pnalloc(cycle->pool,
                                      ccf->lock_file.len + sizeof(".accept"));
        if (cycle->lock_file.data == NULL) {
            return NGX_CONF_ERROR;
        }

        ngx_memcpy(ngx_cpymem(cycle->lock_file.data, ccf->lock_file.data,
                              ccf->lock_file.len),
                   ".accept", sizeof(".accept"));
    }
    }

#endif

    return NGX_CONF_OK;
}


static char *
ngx_set_user(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
#if (NGX_WIN32)

    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "\"user\" is not supported, ignored");

    return NGX_CONF_OK;

#else

    ngx_core_conf_t  *ccf = conf;

    char             *group;
    struct passwd    *pwd;
    struct group     *grp;
    ngx_str_t        *value;

    if (ccf->user != (uid_t) NGX_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    if (geteuid() != 0) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "the \"user\" directive makes sense only "
                           "if the master process runs "
                           "with super-user privileges, ignored");
        return NGX_CONF_OK;
    }

    value = (ngx_str_t *) cf->args->elts;

    ccf->username = (char *) value[1].data;

    ngx_set_errno(0);
    pwd = getpwnam((const char *) value[1].data);
    if (pwd == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                           "getpwnam(\"%s\") failed", value[1].data);
        return NGX_CONF_ERROR;
    }

    ccf->user = pwd->pw_uid;

    group = (char *) ((cf->args->nelts == 2) ? value[1].data : value[2].data);

    ngx_set_errno(0);
    grp = getgrnam(group);
    if (grp == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                           "getgrnam(\"%s\") failed", group);
        return NGX_CONF_ERROR;
    }

    ccf->group = grp->gr_gid;

    return NGX_CONF_OK;

#endif
}


static char *
ngx_set_env(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_core_conf_t  *ccf = conf;

    ngx_str_t   *value, *var;
    ngx_uint_t   i;

    var = ngx_array_push(&ccf->env);
    if (var == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;
    *var = value[1];

    for (i = 0; i < value[1].len; i++) {

        if (value[1].data[i] == '=') {

            var->len = i;

            return NGX_CONF_OK;
        }
    }

    return NGX_CONF_OK;
}


static char *
ngx_set_priority(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_core_conf_t  *ccf = conf;

    ngx_str_t        *value;
    ngx_uint_t        n, minus;

    if (ccf->priority != 0) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (value[1].data[0] == '-') {
        n = 1;
        minus = 1;

    } else if (value[1].data[0] == '+') {
        n = 1;
        minus = 0;

    } else {
        n = 0;
        minus = 0;
    }

    ccf->priority = ngx_atoi(&value[1].data[n], value[1].len - n);
    if (ccf->priority == NGX_ERROR) {
        return "invalid number";
    }

    if (minus) {
        ccf->priority = -ccf->priority;
    }

    return NGX_CONF_OK;
}


/*
worker_processes 4;
worker_cpu_affinity 0001 0010 0100 1000; 四个工作进程分别在四个指定的he上面运行

如果是5he可以这样配置
worker_cpu_affinity 00001 00010 00100 01000 10000; 其他多核类似
*/ 
static char*
ngx_set_cpu_affinity(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
#if (NGX_HAVE_CPU_AFFINITY)
    ngx_core_conf_t  *ccf = conf;

    u_char            ch;
    uint64_t         *mask;
    ngx_str_t        *value;
    ngx_uint_t        i, n;

    if (ccf->cpu_affinity) {
        return "is duplicate";
    }

    //一个64位的空间来存储64个位
    mask = ngx_palloc(cf->pool, (cf->args->nelts - 1) * sizeof(uint64_t));
    if (mask == NULL) {
        return NGX_CONF_ERROR;
    }

    ccf->cpu_affinity_n = cf->args->nelts - 1;
    ccf->cpu_affinity = mask;

    value = cf->args->elts;

    for (n = 1; n < cf->args->nelts; n++) {

        if (value[n].len > 64) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                         "\"worker_cpu_affinity\" supports up to 64 CPUs only");
            return NGX_CONF_ERROR;
        }

        mask[n - 1] = 0;

        for (i = 0; i < value[n].len; i++) {

            ch = value[n].data[i];

            if (ch == ' ') {
                continue;
            }

            mask[n - 1] <<= 1;

            if (ch == '0') {
                continue;
            }

            if (ch == '1') {
                mask[n - 1] |= 1;
                continue;
            }

            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                          "invalid character \"%c\" in \"worker_cpu_affinity\"",
                          ch);
            return NGX_CONF_ERROR;
        }
    }

#else

    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "\"worker_cpu_affinity\" is not supported "
                       "on this platform, ignored");
#endif

    return NGX_CONF_OK;
}


uint64_t
ngx_get_cpu_affinity(ngx_uint_t n)
{
    ngx_core_conf_t  *ccf;

    ccf = (ngx_core_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                           ngx_core_module);

    if (ccf->cpu_affinity == NULL) {
        return 0;
    }

    if (ccf->cpu_affinity_n > n) {
        return ccf->cpu_affinity[n];
    }

    return ccf->cpu_affinity[ccf->cpu_affinity_n - 1];
}


static char *
ngx_set_worker_processes(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t        *value;
    ngx_core_conf_t  *ccf;

    ccf = (ngx_core_conf_t *) conf;

    if (ccf->worker_processes != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = (ngx_str_t *) cf->args->elts;

    if (ngx_strcmp(value[1].data, "auto") == 0) {
        ccf->worker_processes = ngx_ncpu;
        return NGX_CONF_OK;
    }

    ccf->worker_processes = ngx_atoi(value[1].data, value[1].len);

    if (ccf->worker_processes == NGX_ERROR) {
        return "invalid value";
    }

    return NGX_CONF_OK;
}
