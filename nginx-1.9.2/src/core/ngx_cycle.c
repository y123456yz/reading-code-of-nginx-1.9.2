
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


static void ngx_destroy_cycle_pools(ngx_conf_t *conf);
static ngx_int_t ngx_init_zone_pool(ngx_cycle_t *cycle,
    ngx_shm_zone_t *shm_zone);
static ngx_int_t ngx_test_lockfile(u_char *file, ngx_log_t *log);
static void ngx_clean_old_cycles(ngx_event_t *ev);

//初始化参考ngx_init_cycle，最终有一个全局类型的ngx_cycle_s，即ngx_cycle
volatile ngx_cycle_t  *ngx_cycle; //ngx_cycle = cycle;  赋值见main
ngx_array_t            ngx_old_cycles;

static ngx_pool_t     *ngx_temp_pool;
static ngx_event_t     ngx_cleaner_event;

ngx_uint_t             ngx_test_config;
ngx_uint_t             ngx_dump_config;
ngx_uint_t             ngx_quiet_mode;


/* STUB NAME */
static ngx_connection_t  dumb;
/* STUB */

/*
表8-2 ngx_cycle_t结构体支持的主要方法
┏━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━┓
┃    方法名                        ┃    参数含义                          ┃    执行意义                          ┃
┣━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃                                  ┃                                      ┃  返回初始化成功的完整的ngx_cycle_    ┃
┃  ngx_cycle_t *ngx_init_cycle     ┃  old_cycle表示临时的ngx_cycle_t      ┃t结构体，该函数将会负责初始化ngx_     ┃
┃                                  ┃指针，一般仅用来传递ngx_cycle_t结     ┃cycle_t中的数据结构、解析配置文件、   ┃
┃(ngx_cycle_t *old_cycle)          ┃                                      ┃加载所有模块、打开监听端口、初始化    ┃
┃                                  ┃构体中的配置文件路径等参数            ┃                                      ┃
┃                                  ┃                                      ┃进程间通信方式等工作。如果失败，则    ┃
┃                                  ┃                                      ┃返回NULL空指针                        ┃
┣━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃  ngx_int_t ngx_process_          ┃  cycle通常是刚刚分配的ngx_cycle_t    ┃  用运行Nginx时可能携带的目录参数     ┃
┃                                  ┃结构体指针，仅用于传递配置文件路      ┃来初始化cycle，包括初始化运行目录、   ┃
┃options (ngx_cycle_t *cycle)      ┃                                      ┃配置目录，并生成完整的nginx．conf配   ┃
┃                                  ┃径信息                                ┃                                      ┃
┃                                  ┃                                      ┃置文件路径                            ┃
┣━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃                                  ┃                                      ┃  在扰行不重启服务升级Nginx的操       ┃
┃                                  ┃                                      ┃作时，老的Nginx进程会通过环境变量     ┃
┃  ngx_int_t ngx_add_inherited     ┃  cycle是当前进程的ngx_cycle_t结      ┃“NGINX”来传递需要打开的监听端       ┃
┃sockets(ngx_cycle_t幸cycle)       ┃构体指针                              ┃口，新的Nginx进程会通过ngx_add_       ┃
┃                                  ┃                                      ┃inherited- sockets方法来使用已经打开  ┃
┃                                  ┃                                      ┃的TCP监听端口                         ┃
┣━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃  ngxjnt_t ngx_open_listening_    ┃  cycle是当前进程的ngx_cycle_t结      ┃  监听、绑定cycle中listening动态数    ┃
┃sockets (ngx_cycle_t *cycle)      ┃构体指针                              ┃组指定的相应端口                      ┃
┣━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃   void ngx_configure_listening_  ┃  cycle是当前进程的ngx_cycle_t结      ┃  根据nginx.conf中的配置项设置已经    ┃
┃sockets(ngx_cycle_t七cycle)       ┃构体指针                              ┃监听的句柄                            ┃
┣━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃void ngx_close_listening_         ┃    cycle是当前进程的ngx_cycle_t结    ┃  关闭cycle中listening动态数组已经    ┃
┃sockets(ngx_cycle_t *cycle)       ┃构体指针                              ┃打开的句柄                            ┃
┣━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃void ngx_master_process_          ┃    cycle是当前进程的ngx_cycle_t绪    ┃  进入master进程的工作循环            ┃
┃cycle(ngx_cycle_t *cycle)         ┃构体指针                              ┃                                      ┃
┣━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃   void ngx_single_process_       ┃  cycle是当前进程的ngx_cycle_t结      ┃  进入单进程模式（非master、worker    ┃
┃cycle (ngx_cycle_t *cycle)        ┃构体指针                              ┃进程工作模式）的工作循环              ┃
┣━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃                                  ┃  cycle是当前进程的ngx_cycle_t结      ┃                                      ┃
┃                                  ┃构体指针，n是启动子进程的个数，       ┃                                      ┃
┃                                  ┃type是启动方式，它的取值范围有以      ┃                                      ┃
┃                                  ┃下5个：                               ┃                                      ┃
┃   void ngx_start_worker_         ┃    1)  NGX_PROCESS_RESPAWN;          ┃  启动n个worker子进程，并设置好       ┃
┃processes (ngx_cycle_t *cycle,    ┃    2) NGX__ PROCESS NORESPAWN;       ┃每个子进程与master父进程之间使用      ┃
┃                                  ┃    3) NGX—PROCESS_JUST_SPAWN;       ┃socketpair系统调用建立超来的socket    ┃
┃ngx_int_t n, ngx_int_t type)      ┃                                      ┃                                      ┃
┃                                  ┃    4) NGX.PROCESS JUST_RESPAWN;      ┃句柄通信机制                          ┃
┃                                  ┃    5) NGX_PROCESS—DETACHED.         ┃                                      ┃
┃                                  ┃type的值将影响       ngn_process_t    ┃                                      ┃
┃                                  ┃结构体的respawn. detached. just_spawn ┃                                      ┃
┃                                  ┃标志位的值                            ┃                                      ┃
┣━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃                                  ┃                                      ┃  根据是否使用文件缓存模块，也就是    ┃
┃   void ngx_start_cache_          ┃  cycle是当前进程的ngx_cycle_t结      ┃cycle中存储路径的动态数组中是否有     ┃
┃manager_processes(ngx_cycle_t     ┃构体指针，respawn是启动子进程的方     ┃路径的manage标志打开，来决定是否      ┃
┃                                  ┃式，  它与ngx_start_worker_processes  ┃启动cache manage子进程，同样根据      ┃
┃*cycle, ngx_uint_t respawn)       ┃                                      ┃                                      ┃
┃                                  ┃方法中的type参数意义完全相同          ┃loader标志决定是否启动cache loader    ┃
┃                                  ┃                                      ┃子进程                                ┃
┣━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃   void ngx_pass_open_channel     ┃  cycle是当前进程的ngx_cycle_t结      ┃  向所有已经打开的channel（通过       ┃
┃(ngx_cycle_t *cycle, ngx_         ┃构体指针，ch是将要向子进程发送的      ┃socketpair生成的句柄进行通信）发送    ┃
┃channel_t *ch)                    ┃信息                                  ┃ch信息                                ┃
┗━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━┛
┏━━━━━━━━━┳━━━━━━━━┳━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━┓
┃    方法          ┃名              ┃    参数含义                      ┃    执行意义                        ┃
┣━━━━━━━━━┻━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃   void ngx_signal_worker_          ┃                                  ┃                                    ┃
┃processes (ngx_cycle_t *cycle,      ┃  cycle是当前进程的ngx_cycle_t结  ┃  处理worker进程接收到的信号        ┃
┃                                    ┃构体指针，signo是信号             ┃                                    ┃
┃int signo)                          ┃                                  ┃                                    ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃                                    ┃                                  ┃  检查master进程的所有子进程，根    ┃
┃  ngx_uint_t ngx_reap_children      ┃  cycle是当前进程的ngx_cycle_t结  ┃据每个子进程的状态（ngx_process_t结 ┃
┃(ngx_cycle_t *cycle)                ┃构体指针                          ┃构体中的标志位）判断是否要启动子进  ┃
┃                                    ┃                                  ┃程、更改pid文件等                   ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃  voidngx_maste,r_process exit      ┃  cycle是当前进程的ngx_cycle_t结  ┃                                    ┃
┃(ngx_cycle_t *cyc, le)              ┃构体指针                          ┃  退出master进程工作的循环          ┃
┣━━━━━━━━━┳━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃  void ngx_wo?    ┃Lker_process_   ┃  cycle是当前进程的ngx_cycle_t结  ┃                                    ┃
┃cycle (ngx_cycle  ┃-t *cycle, void ┃构体指针，这里还未开始使用data参  ┃  进入worker进程工作的循环          ┃
┃*data)            ┃                ┃数，所以data -般为NULL            ┃                                    ┃
┣━━━━━━━━━┻━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃   void ngx_worker_process_         ┃  cycle悬当前进程的ngx_cycle_t结  ┃  进入worker进程工作循环之前的初    ┃
┃init (ngx_cycle_t *cycle, ngx_      ┃构体指针，priority是worker进程的  ┃始化工作                            ┃
┃uint_t priority)                    ┃系统优先级                        ┃                                    ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃  void ngx_woriker_process_         ┃  cycle是当前进程的ngx_cycle_t结  ┃                                    ┃
┃exit (ngx_cycle_t, *cycle)          ┃构体指针                          ┃  退出worker进程工作的循环          ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃  void ngx_cac:he_manager_          ┃  cycle是当前进程的ngx_cycle_t结  ┃  执行缓存管理工作的循环方法。这与  ┃
┃process_cycle(ngx_cycle_t           ┃构体指针，data是传人的ngx_cache_  ┃文件缓存模块密切相关，在本章中不做  ┃
┃*cycle, void *data)                 ┃manager_ctx_t结构体指针           ┃详细探讨                            ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃   void ngx_process_events_         ┃  cycle是当前进程的ngx_cycle_t结  ┃  使用事件模块处理截止到现在已经收  ┃
┃                                    ┃                                  ┃集到的事件。该函数由事伴模块实现，  ┃
┃and_timers (ngx_cycle_t *cycle)     ┃构体指针                          ┃                                    ┃
┃                                    ┃                                  ┃                                    ┃
┗━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━┛
*/

/*
为什么这里需要有old_cycle???
    旧的ngx_cycle_t对象用于引用上一个ngx_cycle_t对象中的成员。例如ngx_init_cycle方法，在启动初期，需要建
立一个临时的ngx_cycle_t对象保存一些变量(如ngx_process_options中的安装路径，新cycle建立起来前提前写日志的log),
再调用ngx_init_cycle 方法时就可以把旧的ngx_cycle_t对象传进去
*/
ngx_cycle_t *
ngx_init_cycle(ngx_cycle_t *old_cycle)
{
    void                *rv;
    char               **senv, **env;
    ngx_uint_t           i, n;
    ngx_log_t           *log;
    ngx_time_t          *tp;
    ngx_conf_t           conf;
    ngx_pool_t          *pool;
    ngx_cycle_t         *cycle, **old;
    ngx_shm_zone_t      *shm_zone, *oshm_zone;
    ngx_list_part_t     *part, *opart;
    ngx_open_file_t     *file;
    ngx_listening_t     *ls, *nls;
    ngx_core_conf_t     *ccf, *old_ccf;
    ngx_core_module_t   *module; //对应的是核心模块NGX_CORE_MODULE
    char                 hostname[NGX_MAXHOSTNAMELEN];

    ngx_timezone_update();

    /* force localtime update with a new timezone */

    tp = ngx_timeofday();
    tp->sec = 0;

    ngx_time_update();

    //在解析配置文件的error_log前，使用这个旧的默认log,后面解析完配置文件后，会重新按照error_log配置文件来写日志
    log = old_cycle->log;

    pool = ngx_create_pool(NGX_CYCLE_POOL_SIZE, log); /*16K */
    if (pool == NULL) {
        return NULL;
    }
    pool->log = log;

    cycle = ngx_pcalloc(pool, sizeof(ngx_cycle_t));
    if (cycle == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    cycle->pool = pool;
    cycle->log = log;
    cycle->old_cycle = old_cycle;

    cycle->conf_prefix.len = old_cycle->conf_prefix.len;
    cycle->conf_prefix.data = ngx_pstrdup(pool, &old_cycle->conf_prefix);
    if (cycle->conf_prefix.data == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    cycle->prefix.len = old_cycle->prefix.len;
    cycle->prefix.data = ngx_pstrdup(pool, &old_cycle->prefix);
    if (cycle->prefix.data == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    cycle->conf_file.len = old_cycle->conf_file.len;
    cycle->conf_file.data = ngx_pnalloc(pool, old_cycle->conf_file.len + 1);
    if (cycle->conf_file.data == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }
    ngx_cpystrn(cycle->conf_file.data, old_cycle->conf_file.data,
                old_cycle->conf_file.len + 1);

    cycle->conf_param.len = old_cycle->conf_param.len;
    cycle->conf_param.data = ngx_pstrdup(pool, &old_cycle->conf_param);
    if (cycle->conf_param.data == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    n = old_cycle->paths.nelts ? old_cycle->paths.nelts : 10;

    cycle->paths.elts = ngx_pcalloc(pool, n * sizeof(ngx_path_t *));
    if (cycle->paths.elts == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    cycle->paths.nelts = 0;
    cycle->paths.size = sizeof(ngx_path_t *);
    cycle->paths.nalloc = n;

    /*
    // 这里都把内存池给赋值给对象的内存池属性， 因为在内存和资源管理方面，nginx委托内存池统一管理， 这里可以通过具体的对象找到
    它用的内存池对象， 方便实现内存和资源方面的管理。  
    */
    cycle->paths.pool = pool;


    if (ngx_array_init(&cycle->config_dump, pool, 1, sizeof(ngx_conf_dump_t))
        != NGX_OK)
    {
        ngx_destroy_pool(pool);
        return NULL;
    }

    if (old_cycle->open_files.part.nelts) {
        n = old_cycle->open_files.part.nelts;
        for (part = old_cycle->open_files.part.next; part; part = part->next) {
            n += part->nelts;
        }

    } else {
        n = 20;
    }

    if (ngx_list_init(&cycle->open_files, pool, n, sizeof(ngx_open_file_t))
        != NGX_OK)
    {
        ngx_destroy_pool(pool);
        return NULL;
    }


    if (old_cycle->shared_memory.part.nelts) {
        n = old_cycle->shared_memory.part.nelts;
        for (part = old_cycle->shared_memory.part.next; part; part = part->next)
        {
            n += part->nelts;
        }

    } else {
        n = 1;
    }

    if (ngx_list_init(&cycle->shared_memory, pool, n, sizeof(ngx_shm_zone_t)) 
        != NGX_OK)
    {
        ngx_destroy_pool(pool);
        return NULL;
    }

    n = old_cycle->listening.nelts ? old_cycle->listening.nelts : 10;

    cycle->listening.elts = ngx_pcalloc(pool, n * sizeof(ngx_listening_t));
    if (cycle->listening.elts == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    cycle->listening.nelts = 0;
    cycle->listening.size = sizeof(ngx_listening_t);
    cycle->listening.nalloc = n;
    cycle->listening.pool = pool;

    ngx_queue_init(&cycle->reusable_connections_queue);


    cycle->conf_ctx = ngx_pcalloc(pool, ngx_max_module * sizeof(void *));
    if (cycle->conf_ctx == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }


    if (gethostname(hostname, NGX_MAXHOSTNAMELEN) == -1) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno, "gethostname() failed");
        ngx_destroy_pool(pool);
        return NULL;
    }

    /* on Linux gethostname() silently truncates name that does not fit */
    hostname[NGX_MAXHOSTNAMELEN - 1] = '\0';
    cycle->hostname.len = ngx_strlen(hostname);

    cycle->hostname.data = ngx_pnalloc(pool, cycle->hostname.len);
    if (cycle->hostname.data == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    ngx_strlow(cycle->hostname.data, (u_char *) hostname, cycle->hostname.len);

/*
在初始化ngx_cycle_t中的所有容器后，会为读取、解析配置文件做准备工作。因为每个模块都必须有相应的数据
结构来存储配置文件中的各配置项，创建这些数据结构的工作都需要在这一步进行。Nginx框架只关心NGX_CORE_MODULE核
心模块，这也是为了降低框架的复杂度。这里将会调用所有核心模块的create conf方法（也只有核心模块才有这个方法），
这意味着需要所有的核心模块开始构造用于存储配置项的结构体。其他非核心模块怎么办呢？其实很简单。这些模块大都
从属于一个核心模块，如每个HTTP模块都由ngx_http_module管理（如图8-2所示），这样ngx_http_module在解析自己感兴
趣的“http”配置项时，将会调用所有HTTP模块约定的方法来创建存储配置项的结构体（xxx_create_main_conf、xxx_create_srv_conf、xxx_create_loc_conf方法）。
*/
    for (i = 0; ngx_modules[i]; i++) {
        if (ngx_modules[i]->type != NGX_CORE_MODULE) {
            continue;
        }

        module = ngx_modules[i]->ctx;

        if (module->create_conf) {
            rv = module->create_conf(cycle);
            if (rv == NULL) {
                ngx_destroy_pool(pool);
                return NULL;
            }
            
            cycle->conf_ctx[ngx_modules[i]->index] = rv;
        }
    }

    senv = environ;

    ngx_memzero(&conf, sizeof(ngx_conf_t));
    /* STUB: init array ? */
    conf.args = ngx_array_create(pool, 10, sizeof(ngx_str_t));
    if (conf.args == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    conf.temp_pool = ngx_create_pool(NGX_CYCLE_POOL_SIZE, log);
    if (conf.temp_pool == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    conf.ctx = cycle->conf_ctx; //这样下面的ngx_conf_param解析配置的时候，里面对conf.ctx赋值操作，实际上就是对cycle->conf_ctx[i]赋值
    conf.cycle = cycle;
    conf.pool = pool;
    conf.log = log;
    conf.module_type = NGX_CORE_MODULE;
    conf.cmd_type = NGX_MAIN_CONF;

#if 0
    log->log_level = NGX_LOG_DEBUG_ALL;
#endif

    if (ngx_conf_param(&conf) != NGX_CONF_OK) { //这时候的conf指向的是cycle
        environ = senv;
        ngx_destroy_cycle_pools(&conf);
        return NULL;
    }
        
    if (ngx_conf_parse(&conf, &cycle->conf_file) != NGX_CONF_OK) {//这时候的conf指向的是cycle
        environ = senv;
        ngx_destroy_cycle_pools(&conf);
        return NULL;
    }
        
    if (ngx_test_config && !ngx_quiet_mode) {
        ngx_log_stderr(0, "the configuration file %s syntax is ok",
                       cycle->conf_file.data);
    }

    /*
    调用所有NGX_CORE_MODULE核心模块的init_conf方法。这一步骤的目的在于让所有核心模块在解析完配置项后可以做综合性处理，
    */
    for (i = 0; ngx_modules[i]; i++) {
        if (ngx_modules[i]->type != NGX_CORE_MODULE) {
            continue;
        }

        module = ngx_modules[i]->ctx;

        if (module->init_conf) {
            if (module->init_conf(cycle, cycle->conf_ctx[ngx_modules[i]->index])
                == NGX_CONF_ERROR)
            {
                environ = senv;
                ngx_destroy_cycle_pools(&conf);
                return NULL;
            }
        }
    }

    if (ngx_process == NGX_PROCESS_SIGNALLER) {
        return cycle;
    }

    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    if (ngx_test_config) {

        if (ngx_create_pidfile(&ccf->pid, log) != NGX_OK) {
            goto failed;
        }

    } else if (!ngx_is_init_cycle(old_cycle)) {

        /*
         * we do not create the pid file in the first ngx_init_cycle() call
         * because we need to write the demonized process pid
         */

        old_ccf = (ngx_core_conf_t *) ngx_get_conf(old_cycle->conf_ctx,
                                                   ngx_core_module);
        if (ccf->pid.len != old_ccf->pid.len
            || ngx_strcmp(ccf->pid.data, old_ccf->pid.data) != 0)
        {
            /* new pid file name */

            if (ngx_create_pidfile(&ccf->pid, log) != NGX_OK) {
                goto failed;
            }

            ngx_delete_pidfile(old_cycle);
        }
    }


    if (ngx_test_lockfile(cycle->lock_file.data, log) != NGX_OK) {
        goto failed;
    }

    //创建在nginx.conf中涉及到文件路径的文件夹
    if (ngx_create_paths(cycle, ccf->user) != NGX_OK) {
        goto failed;
    }

    if (ngx_log_open_default(cycle) != NGX_OK) {
        goto failed;
    }

    /* open the new files */

    part = &cycle->open_files.part;
    file = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            file = part->elts;
            i = 0;
        }

        if (file[i].name.len == 0) {
            continue;
        }

        file[i].fd = ngx_open_file(file[i].name.data,
                                   NGX_FILE_APPEND,
                                   NGX_FILE_CREATE_OR_OPEN,
                                   NGX_FILE_DEFAULT_ACCESS);

        ngx_log_debug3(NGX_LOG_DEBUG_CORE, log, 0,
                       "log: %p %d \"%s\"",
                       &file[i], file[i].fd, file[i].name.data);

        if (file[i].fd == NGX_INVALID_FILE) {
            ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                          ngx_open_file_n " \"%s\" failed",
                          file[i].name.data);
            goto failed;
        }

#if !(NGX_WIN32)
        if (fcntl(file[i].fd, F_SETFD, FD_CLOEXEC) == -1) {
            ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                          "fcntl(FD_CLOEXEC) \"%s\" failed",
                          file[i].name.data);
            goto failed;
        }
#endif
    }

    /**/
    //在解析配置文件的error_log前，使用这个旧的默认log,解析完配置文件后，会重新按照error_log配置文件来写日志
    //new_log信息通过前面的ngx_log_open_default设置new_log信息，并在上面的part = &cycle->open_files.part;中打开了这个文件获得了fd
    cycle->log = &cycle->new_log;
    pool->log = &cycle->new_log;

    /*  走到这里的时候，所有的配置已经解析完毕，如果配置"zone" proxy_cache_path fastcgi_cache_path等需要创建共享内存，则在下面依次创建对应的共享内存  */

    /* create shared memory */
    part = &cycle->shared_memory.part;
    shm_zone = part->elts;
    //ngx_cycle_t结构体的shared_mem ory链表中将会开始初始化用于进程间通信的共享内存。
    for (i = 0; /* void */ ; i++) {
        if (i >= part->nelts) { //在热启动nginx的时候才会执行下面的相关代码，默认i和part->nelts都等于0
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            shm_zone = part->elts;
            i = 0;
        }

        if (shm_zone[i].shm.size == 0) {
            ngx_log_error(NGX_LOG_EMERG, log, 0,
                          "zero size shared memory zone \"%V\"",
                          &shm_zone[i].shm.name);
            goto failed;
        }

        shm_zone[i].shm.log = cycle->log;

        opart = &old_cycle->shared_memory.part;
        oshm_zone = opart->elts;

        for (n = 0; /* void */ ; n++) {

            if (n >= opart->nelts) {
                if (opart->next == NULL) {
                    break;
                }
                opart = opart->next;
                oshm_zone = opart->elts;
                n = 0;
            }

            if (shm_zone[i].shm.name.len != oshm_zone[n].shm.name.len) {
                continue;
            }

            if (ngx_strncmp(shm_zone[i].shm.name.data,
                            oshm_zone[n].shm.name.data,
                            shm_zone[i].shm.name.len) //
                != 0)
            {
                continue;
            }

            if (shm_zone[i].tag == oshm_zone[n].tag
                && shm_zone[i].shm.size == oshm_zone[n].shm.size
                && !shm_zone[i].noreuse)
            {
                shm_zone[i].shm.addr = oshm_zone[n].shm.addr;
#if (NGX_WIN32)
                shm_zone[i].shm.handle = oshm_zone[n].shm.handle;
#endif

                if (shm_zone[i].init(&shm_zone[i], oshm_zone[n].data)
                    != NGX_OK)
                {
                    goto failed;
                }

                goto shm_zone_found;
            }

            ngx_shm_free(&oshm_zone[n].shm);

            break;
        }

        if (ngx_shm_alloc(&shm_zone[i].shm) != NGX_OK) {
            goto failed;
        }

        if (ngx_init_zone_pool(cycle, &shm_zone[i]) != NGX_OK) {
            goto failed;
        }

        if (shm_zone[i].init(&shm_zone[i], NULL) != NGX_OK) {
            goto failed;
        }

    shm_zone_found:

        continue;
    }


    /* handle the listening sockets */
/*
所有的模块都已经解析出自己需要监听的端口，如HTTP模块已经在解析http{．．．}配置项时得到它要监听的端口，并添加到
listening数组中了。这一步骤就是按照listening数组中的每一个ngx_listening_t元素设置socket句柄并监听端口（实际上，这一步骤的主要工作就是调
用表8-2中的ngx_open_listening_sockets方法）。
*/
    if (old_cycle->listening.nelts) {
        ls = old_cycle->listening.elts; //旧的listen,如热启动继承过来的sock,见ngx_add_inherited_sockets
        for (i = 0; i < old_cycle->listening.nelts; i++) {
            ls[i].remain = 0;
        }

        nls = cycle->listening.elts; //从配置文件nginx.conf中listen的server ip和端口获取到的信息
        for (n = 0; n < cycle->listening.nelts; n++) {

            for (i = 0; i < old_cycle->listening.nelts; i++) {
                if (ls[i].ignore) {
                    continue;
                }

                if (ls[i].remain) {
                    continue;
                }

                if (ngx_cmp_sockaddr(nls[n].sockaddr, nls[n].socklen,
                                     ls[i].sockaddr, ls[i].socklen, 1)
                    == NGX_OK)
                {
                    nls[n].fd = ls[i].fd;
                    nls[n].previous = &ls[i];
                    ls[i].remain = 1;

                    if (ls[i].backlog != nls[n].backlog) {
                        nls[n].listen = 1;
                    }

#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)

                    /*
                     * FreeBSD, except the most recent versions,
                     * could not remove accept filter
                     */
                    nls[n].deferred_accept = ls[i].deferred_accept;

                    if (ls[i].accept_filter && nls[n].accept_filter) {
                        if (ngx_strcmp(ls[i].accept_filter,
                                       nls[n].accept_filter)
                            != 0)
                        {
                            nls[n].delete_deferred = 1;
                            nls[n].add_deferred = 1;
                        }

                    } else if (ls[i].accept_filter) {
                        nls[n].delete_deferred = 1;

                    } else if (nls[n].accept_filter) {
                        nls[n].add_deferred = 1;
                    }
#endif

#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)

                    if (ls[i].deferred_accept && !nls[n].deferred_accept) {
                        nls[n].delete_deferred = 1;

                    } else if (ls[i].deferred_accept != nls[n].deferred_accept)
                    {
                        nls[n].add_deferred = 1;
                    }
#endif

#if (NGX_HAVE_REUSEPORT)
                    if (nls[n].reuseport && !ls[i].reuseport) {
                        nls[n].add_reuseport = 1;
                    }
#endif

                    break;
                }
            }

            if (nls[n].fd == (ngx_socket_t) -1) {
                nls[n].open = 1;
#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
                if (nls[n].accept_filter) {
                    nls[n].add_deferred = 1;
                }
#endif
#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
                if (nls[n].deferred_accept) {
                    nls[n].add_deferred = 1;
                }
#endif
            }
        }

    } else {
        ls = cycle->listening.elts;
        for (i = 0; i < cycle->listening.nelts; i++) {
            ls[i].open = 1;
#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
            if (ls[i].accept_filter) {
                ls[i].add_deferred = 1;
            }
#endif
#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
            if (ls[i].deferred_accept) {
                ls[i].add_deferred = 1;
            }
#endif
        }
    }

    if (ngx_open_listening_sockets(cycle) != NGX_OK) {
        goto failed;
    }

    if (!ngx_test_config) {
        ngx_configure_listening_sockets(cycle);
    }


    /* commit the new cycle configuration */

    if (!ngx_use_stderr) {
        (void) ngx_log_redirect_stderr(cycle);
    }

    pool->log = cycle->log;

    for (i = 0; ngx_modules[i]; i++) {
        if (ngx_modules[i]->init_module) {
            if (ngx_modules[i]->init_module(cycle) != NGX_OK) {
                /* fatal */
                exit(1);
            }
        }
    }


    /* close and delete stuff that lefts from an old cycle */

    /* free the unnecessary shared memory */

    opart = &old_cycle->shared_memory.part;
    oshm_zone = opart->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= opart->nelts) {
            if (opart->next == NULL) {
                goto old_shm_zone_done;
            }
            opart = opart->next;
            oshm_zone = opart->elts;
            i = 0;
        }

        part = &cycle->shared_memory.part;
        shm_zone = part->elts;

        for (n = 0; /* void */ ; n++) {

            if (n >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }
                part = part->next;
                shm_zone = part->elts;
                n = 0;
            }

            if (oshm_zone[i].shm.name.len == shm_zone[n].shm.name.len
                && ngx_strncmp(oshm_zone[i].shm.name.data,
                               shm_zone[n].shm.name.data,
                               oshm_zone[i].shm.name.len)
                == 0)
            {
                goto live_shm_zone;
            }
        }

        ngx_shm_free(&oshm_zone[i].shm);

    live_shm_zone:

        continue;
    }

old_shm_zone_done:


    /* close the unnecessary listening sockets */

    ls = old_cycle->listening.elts;
    for (i = 0; i < old_cycle->listening.nelts; i++) {

        if (ls[i].remain || ls[i].fd == (ngx_socket_t) -1) {
            continue;
        }

        if (ngx_close_socket(ls[i].fd) == -1) {
            ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                          ngx_close_socket_n " listening socket on %V failed",
                          &ls[i].addr_text);
        }

#if (NGX_HAVE_UNIX_DOMAIN)

        if (ls[i].sockaddr->sa_family == AF_UNIX) {
            u_char  *name;

            name = ls[i].addr_text.data + sizeof("unix:") - 1;

            ngx_log_error(NGX_LOG_WARN, cycle->log, 0,
                          "deleting socket %s", name);

            if (ngx_delete_file(name) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_socket_errno,
                              ngx_delete_file_n " %s failed", name);
            }
        }

#endif
    }


    /* close the unnecessary open files */

    part = &old_cycle->open_files.part;
    file = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            file = part->elts;
            i = 0;
        }

        if (file[i].fd == NGX_INVALID_FILE || file[i].fd == ngx_stderr) {
            continue;
        }

        if (ngx_close_file(file[i].fd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                          ngx_close_file_n " \"%s\" failed",
                          file[i].name.data);
        }
    }

    ngx_destroy_pool(conf.temp_pool);
//如果是第一次加载，则满足ngx_is_init_cycle。如果是reload热启动，则原来的nginx进程的ngx_process == NGX_PROCESS_MASTER    
    if (ngx_process == NGX_PROCESS_MASTER || ngx_is_init_cycle(old_cycle)) { 

        /*
         * perl_destruct() frees environ, if it is not the same as it was at
         * perl_construct() time, therefore we save the previous cycle
         * environment before ngx_conf_parse() where it will be changed.
         */
        env = environ;
        environ = senv;

        ngx_destroy_pool(old_cycle->pool);
        cycle->old_cycle = NULL;

        environ = env;

        return cycle;
    }


    if (ngx_temp_pool == NULL) {
        ngx_temp_pool = ngx_create_pool(128, cycle->log);
        if (ngx_temp_pool == NULL) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                          "could not create ngx_temp_pool");
            exit(1);
        }

        n = 10;
        ngx_old_cycles.elts = ngx_pcalloc(ngx_temp_pool,
                                          n * sizeof(ngx_cycle_t *));
        if (ngx_old_cycles.elts == NULL) {
            exit(1);
        }
        ngx_old_cycles.nelts = 0;
        ngx_old_cycles.size = sizeof(ngx_cycle_t *);
        ngx_old_cycles.nalloc = n;
        ngx_old_cycles.pool = ngx_temp_pool;

        ngx_cleaner_event.handler = ngx_clean_old_cycles;
        ngx_cleaner_event.log = cycle->log;
        ngx_cleaner_event.data = &dumb;
        dumb.fd = (ngx_socket_t) -1;
    }

    ngx_temp_pool->log = cycle->log;

    old = ngx_array_push(&ngx_old_cycles);
    if (old == NULL) {
        exit(1);
    }
    *old = old_cycle;

    if (!ngx_cleaner_event.timer_set) {
        ngx_add_timer(&ngx_cleaner_event, 30000, NGX_FUNC_LINE);
        ngx_cleaner_event.timer_set = 1;
    }

    return cycle;


failed:

    if (!ngx_is_init_cycle(old_cycle)) {
        old_ccf = (ngx_core_conf_t *) ngx_get_conf(old_cycle->conf_ctx,
                                                   ngx_core_module);
        if (old_ccf->environment) {
            environ = old_ccf->environment;
        }
    }

    /* rollback the new cycle configuration */

    part = &cycle->open_files.part;
    file = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            file = part->elts;
            i = 0;
        }

        if (file[i].fd == NGX_INVALID_FILE || file[i].fd == ngx_stderr) {
            continue;
        }

        if (ngx_close_file(file[i].fd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                          ngx_close_file_n " \"%s\" failed",
                          file[i].name.data);
        }
    }

    if (ngx_test_config) {
        ngx_destroy_cycle_pools(&conf);
        return NULL;
    }

    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {
        if (ls[i].fd == (ngx_socket_t) -1 || !ls[i].open) {
            continue;
        }

        if (ngx_close_socket(ls[i].fd) == -1) {
            ngx_log_error(NGX_LOG_EMERG, log, ngx_socket_errno,
                          ngx_close_socket_n " %V failed",
                          &ls[i].addr_text);
        }
    }

    ngx_destroy_cycle_pools(&conf);

    return NULL;
}


static void
ngx_destroy_cycle_pools(ngx_conf_t *conf)
{
    ngx_destroy_pool(conf->temp_pool);
    ngx_destroy_pool(conf->pool);
}


static ngx_int_t
ngx_init_zone_pool(ngx_cycle_t *cycle, ngx_shm_zone_t *zn)
{
    u_char           *file;
    ngx_slab_pool_t  *sp;

    //共享内存的起始地址开始的sizeof(ngx_slab_pool_t)字节是用来存储管理共享内存的slab poll的
    sp = (ngx_slab_pool_t *) zn->shm.addr; //共享内存起始地址

    if (zn->shm.exists) {

        if (sp == sp->addr) {
            return NGX_OK;
        }

#if (NGX_WIN32)

        /* remap at the required address */

        if (ngx_shm_remap(&zn->shm, sp->addr) != NGX_OK) {
            return NGX_ERROR;
        }

        sp = (ngx_slab_pool_t *) zn->shm.addr;

        if (sp == sp->addr) {
            return NGX_OK;
        }

#endif

        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "shared zone \"%V\" has no equal addresses: %p vs %p",
                      &zn->shm.name, sp->addr, sp);
        return NGX_ERROR;
    }

    sp->end = zn->shm.addr + zn->shm.size;
    sp->min_shift = 3;
    sp->addr = zn->shm.addr;

#if (NGX_HAVE_ATOMIC_OPS)

    file = NULL;

#else

    file = ngx_pnalloc(cycle->pool, cycle->lock_file.len + zn->shm.name.len);
    if (file == NULL) {
        return NGX_ERROR;
    }

    (void) ngx_sprintf(file, "%V%V%Z", &cycle->lock_file, &zn->shm.name);

#endif

    //创建共享内存锁
    if (ngx_shmtx_create(&sp->mutex, &sp->lock, file) != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_slab_init(sp);

    return NGX_OK;
}


ngx_int_t
ngx_create_pidfile(ngx_str_t *name, ngx_log_t *log)
{
    size_t      len;
    ngx_uint_t  create;
    ngx_file_t  file;
    u_char      pid[NGX_INT64_LEN + 2];

    if (ngx_process > NGX_PROCESS_MASTER) {
        return NGX_OK;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.name = *name;
    file.log = log;

    create = ngx_test_config ? NGX_FILE_CREATE_OR_OPEN : NGX_FILE_TRUNCATE;

    file.fd = ngx_open_file(file.name.data, NGX_FILE_RDWR,
                            create, NGX_FILE_DEFAULT_ACCESS);

    if (file.fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", file.name.data);
        return NGX_ERROR;
    }

    if (!ngx_test_config) {
        len = ngx_snprintf(pid, NGX_INT64_LEN + 2, "%P%N", ngx_pid) - pid;

        if (ngx_write_file(&file, pid, len, 0) == NGX_ERROR) {
            return NGX_ERROR;
        }
    }

    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", file.name.data);
    }

    return NGX_OK;
}


void
ngx_delete_pidfile(ngx_cycle_t *cycle)
{
    u_char           *name;
    ngx_core_conf_t  *ccf;

    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    name = ngx_new_binary ? ccf->oldpid.data : ccf->pid.data;

    if (ngx_delete_file(name) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      ngx_delete_file_n " \"%s\" failed", name);
    }
}

/*
该函数作用：
读取ngx_core_module模块的配置结构ngx_core_conf_t；
根据配置结构找到其工作进程文件，如"/usr/local/nginx/logs/nginx.pid"(该文件保存nginx进程ID，即pid)；
打开该文件，读取pid；
调用ngx_os_signal_process()发送信号；
*/
ngx_int_t
ngx_signal_process(ngx_cycle_t *cycle, char *sig)
{
    ssize_t           n;
    ngx_int_t         pid;
    ngx_file_t        file;
    ngx_core_conf_t  *ccf;
    u_char            buf[NGX_INT64_LEN + 2];

    ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "signal process started");

    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    ngx_memzero(&file, sizeof(ngx_file_t));

    //打开存放master进程的文件NGX_PID_PATH
    file.name = ccf->pid;
    file.log = cycle->log;

    file.fd = ngx_open_file(file.name.data, NGX_FILE_RDONLY,
                            NGX_FILE_OPEN, NGX_FILE_DEFAULT_ACCESS);

    if (file.fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", file.name.data);
        return 1;
    }

    n = ngx_read_file(&file, buf, NGX_INT64_LEN + 2, 0);

    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", file.name.data);
    }

    if (n == NGX_ERROR) {
        return 1;
    }

    while (n-- && (buf[n] == CR || buf[n] == LF)) { /* void */ }

    pid = ngx_atoi(buf, ++n); //master进程id

    if (pid == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                      "invalid PID number \"%*s\" in \"%s\"",
                      n, buf, file.name.data);
        return 1;
    }

    return ngx_os_signal_process(cycle, sig, pid);

}


static ngx_int_t
ngx_test_lockfile(u_char *file, ngx_log_t *log)
{
#if !(NGX_HAVE_ATOMIC_OPS)
    ngx_fd_t  fd;

    fd = ngx_open_file(file, NGX_FILE_RDWR, NGX_FILE_CREATE_OR_OPEN,
                       NGX_FILE_DEFAULT_ACCESS);

    if (fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", file);
        return NGX_ERROR;
    }

    if (ngx_close_file(fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", file);
    }

    if (ngx_delete_file(file) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                      ngx_delete_file_n " \"%s\" failed", file);
    }

#endif

    return NGX_OK;
}

//ngx_reopen标志位，如果为1，则表示需要重新打开所有文件
void
ngx_reopen_files(ngx_cycle_t *cycle, ngx_uid_t user)
{
    ngx_fd_t          fd;
    ngx_uint_t        i;
    ngx_list_part_t  *part;
    ngx_open_file_t  *file;

    part = &cycle->open_files.part;
    file = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            file = part->elts;
            i = 0;
        }

        if (file[i].name.len == 0) {
            continue;
        }

        if (file[i].flush) {
            file[i].flush(&file[i], cycle->log);
        }

        fd = ngx_open_file(file[i].name.data, NGX_FILE_APPEND,
                           NGX_FILE_CREATE_OR_OPEN, NGX_FILE_DEFAULT_ACCESS);

        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                       "reopen file \"%s\", old:%d new:%d",
                       file[i].name.data, file[i].fd, fd);

        if (fd == NGX_INVALID_FILE) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                          ngx_open_file_n " \"%s\" failed", file[i].name.data);
            continue;
        }

#if !(NGX_WIN32)
        if (user != (ngx_uid_t) NGX_CONF_UNSET_UINT) {
            ngx_file_info_t  fi;

            if (ngx_file_info((const char *) file[i].name.data, &fi)
                == NGX_FILE_ERROR)
            {
                ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                              ngx_file_info_n " \"%s\" failed",
                              file[i].name.data);

                if (ngx_close_file(fd) == NGX_FILE_ERROR) {
                    ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                                  ngx_close_file_n " \"%s\" failed",
                                  file[i].name.data);
                }

                continue;
            }

            if (fi.st_uid != user) {
                if (chown((const char *) file[i].name.data, user, -1) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                                  "chown(\"%s\", %d) failed",
                                  file[i].name.data, user);

                    if (ngx_close_file(fd) == NGX_FILE_ERROR) {
                        ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                                      ngx_close_file_n " \"%s\" failed",
                                      file[i].name.data);
                    }

                    continue;
                }
            }

            if ((fi.st_mode & (S_IRUSR|S_IWUSR)) != (S_IRUSR|S_IWUSR)) {

                fi.st_mode |= (S_IRUSR|S_IWUSR);

                if (chmod((const char *) file[i].name.data, fi.st_mode) == -1) {
                    ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                                  "chmod() \"%s\" failed", file[i].name.data);

                    if (ngx_close_file(fd) == NGX_FILE_ERROR) {
                        ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                                      ngx_close_file_n " \"%s\" failed",
                                      file[i].name.data);
                    }

                    continue;
                }
            }
        }

        if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                          "fcntl(FD_CLOEXEC) \"%s\" failed",
                          file[i].name.data);

            if (ngx_close_file(fd) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                              ngx_close_file_n " \"%s\" failed",
                              file[i].name.data);
            }

            continue;
        }
#endif

        if (ngx_close_file(file[i].fd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                          ngx_close_file_n " \"%s\" failed",
                          file[i].name.data);
        }

        file[i].fd = fd;
    }

    (void) ngx_log_redirect_stderr(cycle);
}

//先创建链表结构，用来存储配置文件中指定的需要的共享内存空间信息到cycle->shared_memory，然后在ngx_init_cycle中解析完配置文件后，把配置文件中
//指定的需要共享内存信息从cycle->shared_memory链表取出来，在ngx_init_cycle解析完配置文件后统一进行共享内存分配创建
ngx_shm_zone_t *
ngx_shared_memory_add(ngx_conf_t *cf, ngx_str_t *name, size_t size, void *tag)
{
    ngx_uint_t        i;
    ngx_shm_zone_t   *shm_zone;
    ngx_list_part_t  *part;

    part = &cf->cycle->shared_memory.part;
    shm_zone = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            shm_zone = part->elts;
            i = 0;
        }

        if (name->len != shm_zone[i].shm.name.len) {
            continue;
        }

        if (ngx_strncmp(name->data, shm_zone[i].shm.name.data, name->len)
            != 0)
        {
            continue;
        }

        if (tag != shm_zone[i].tag) { //例如proxy_cache abc, fastcgi abc；同时配置，就会报错
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                            "the shared memory zone \"%V\" is "
                            "already declared for a different use",
                            &shm_zone[i].shm.name);
            return NULL;
        }

        if (shm_zone[i].shm.size == 0) {//名字相同，大小不一致，错误。  例如在该配置之前有配置proxy_cache xxx，则xxx共享内存的大小就是0
            shm_zone[i].shm.size = size;
        }

        if (size && size != shm_zone[i].shm.size) {//之前已经有了name共享内存，但是新来的name需要创建的共享内存大小与之前的不相等，冲突了
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                            "the size %uz of shared memory zone \"%V\" "
                            "conflicts with already declared size %uz",
                            size, &shm_zone[i].shm.name, shm_zone[i].shm.size);
            return NULL;
        }

        //找到了一个相同名字的，并且现在知道的共享内存大小与之前的一样，则直接使用该共享内存，返回其在cf->cycle->shared_memory.part中的下标。
        return &shm_zone[i];
    }

    /* 需要创建一个新的ngx_shm_zone_t来在ngx_init_cycle来真正创建共享内存 */
    shm_zone = ngx_list_push(&cf->cycle->shared_memory);

    if (shm_zone == NULL) {
        return NULL;
    }

    shm_zone->data = NULL;
    shm_zone->shm.log = cf->cycle->log;
    shm_zone->shm.size = size;
    shm_zone->shm.name = *name;
    shm_zone->shm.exists = 0;
    shm_zone->init = NULL;
    shm_zone->tag = tag;
    shm_zone->noreuse = 0;

    return shm_zone;
}


static void
ngx_clean_old_cycles(ngx_event_t *ev)
{
    ngx_uint_t     i, n, found, live;
    ngx_log_t     *log;
    ngx_cycle_t  **cycle;

    log = ngx_cycle->log;
    ngx_temp_pool->log = log;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, log, 0, "clean old cycles");

    live = 0;

    cycle = ngx_old_cycles.elts;
    for (i = 0; i < ngx_old_cycles.nelts; i++) {

        if (cycle[i] == NULL) {
            continue;
        }

        found = 0;

        for (n = 0; n < cycle[i]->connection_n; n++) {
            if (cycle[i]->connections[n].fd != (ngx_socket_t) -1) {
                found = 1;

                ngx_log_debug1(NGX_LOG_DEBUG_CORE, log, 0, "live fd:%d", n);

                break;
            }
        }

        if (found) {
            live = 1;
            continue;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_CORE, log, 0, "clean old cycle: %d", i);

        ngx_destroy_pool(cycle[i]->pool);
        cycle[i] = NULL;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, log, 0, "old cycles status: %d", live);

    if (live) {
        ngx_add_timer(ev, 30000, NGX_FUNC_LINE);

    } else {
        ngx_destroy_pool(ngx_temp_pool);
        ngx_temp_pool = NULL;
        ngx_old_cycles.nelts = 0;
    }
}
