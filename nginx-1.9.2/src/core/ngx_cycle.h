
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CYCLE_H_INCLUDED_
#define _NGX_CYCLE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#ifndef NGX_CYCLE_POOL_SIZE
#define NGX_CYCLE_POOL_SIZE     NGX_DEFAULT_POOL_SIZE
#endif


#define NGX_DEBUG_POINTS_STOP   1
#define NGX_DEBUG_POINTS_ABORT  2


typedef struct ngx_shm_zone_s  ngx_shm_zone_t;

typedef ngx_int_t (*ngx_shm_zone_init_pt) (ngx_shm_zone_t *zone, void *data);

//在ngx_http_upstream_cache_get中获取zone的时候获取的是fastcgi_cache proxy_cache设置的zone，因此必须配置fastcgi_cache (proxy_cache) abc;中的xxx和xxx_cache_path(proxy_cache_path fastcgi_cache_path) xxx keys_zone=abc:10m;一致
//所有的共享内存都通过ngx_http_file_cache_s->shpool进行管理   每个共享内存对应一个ngx_slab_pool_t来管理，见ngx_init_zone_pool
struct ngx_shm_zone_s { //初始化见ngx_shared_memory_add，真正的共享内存创建在ngx_init_cycle->ngx_init_cycle
    void                     *data;//指向ngx_http_file_cache_t，赋值见ngx_http_file_cache_set_slot
    ngx_shm_t                 shm; //ngx_init_cycle->ngx_shm_alloc->ngx_shm_alloc中创建相应的共享内存空间
    //ngx_init_cycle中执行
    ngx_shm_zone_init_pt      init; // "zone" proxy_cache_path fastcgi_cache_path等配置中设置为ngx_http_file_cache_init   ngx_http_upstream_init_zone   
    void                     *tag; //创建的这个共享内存属于哪个模块
    ngx_uint_t                noreuse;  /* unsigned  noreuse:1; */
};

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
┃                                  ┃type的值将影响8.6节中ngn_process_t    ┃                                      ┃
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
┃                                    ┃                                  ┃详见第9章                           ┃
┗━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━┛
*/
//http://tech.uc.cn/?p=300 参数解析相关数据结构参考
//初始化参考ngx_init_cycle
struct ngx_cycle_s {
    /*     保存着所有模块存储配置项的结构体指针，     它首先是一个数组，数组大小为ngx_max_module，正好与Nginx的module个数一样；     
    每个数组成员又是一个指针，指向另一个存储着指针的数组，因此会看到void ****    请见陶辉所著《深入理解Nginx-模块开发与架构解析》
    一书302页插图。    另外，这个图也不错：http://img.my.csdn.net/uploads/201202/9/0_1328799724GTUk.gif    

    每个进程中都有一个唯一的ngx_cycle_t核心结构体，它有一个成员conf_ctx维护着所有模块的配置结构体，
  其类型是void ****conf_ctx。conf_ctx意义为首先指向一个成员皆为指针的数组，其中每个成员指针又指向另外一个
  成员皆为指针的数组，第2个子数组中的成员指针才会指向各模块生成的配置结构体。这正是为了事件模
  块、http模块、mail模块而设计的，第9、10章都有详述，这有利于不同于NGX_CORE_MODULE类型的
  特定模块解析配置项。然而，NGX_CORE_MODULE类型的核心模块解析配置项时，配置项一定是全局的，
  不会从属于任何{}配置块的，它不需要上述这种双数组设计。解析标识为NGX_DIRECT_CONF类型的配
  置项时，会把void****类型的conf_ctx强制转换为void**，也就是说，此时，在conf_ctx指向的指针数组
  中，每个成员指针不再指向其他数组，直接指向核心模块生成的配置鲒构体。因此，NGX_DIRECT_CONF
  仅由NGX_CORE_MODULE类型的核心模块使用，而且配置项只应该出现在全局配置中。
    */ //初始化见ngx_init_cycle，所有为http{} server{} location{}分配的空间都由该指针指向新开辟的空间
    //NGX_CORE_MODULE类型模块赋值在ngx_init_cycle 
    //http{}ngx_http_module相关模块赋值地方在ngx_http_block
    //参考:http://tech.uc.cn/?p=300   ngx_http_conf_ctx_t变量的指针ctx存储在ngx_cycle_t的conf_ctx所指向的指针数组，以ngx_http_module的index为下标的数组元素
    //在ngx_http_block中，conf_ctx中的数组指针会被赋值给ngx_conf_s的ctx，这样ngx_conf_s->ctx就等于conf_ctx的相关空间
    //这里的conf_ctx[]数组成员指针直接指向NGX_CORE_MODULE类型的配置命令中开辟的ngx_http_conf_ctx_t空间，例如ngx_http_block中开辟的空间

    /*
    为什么上面代码中有4个宰？因为它首先指向一个存放指针的数组，这个数组中的指针成员同时又指向了另外一个存放指针的数组，所以是4个4。
    看到conf- ctx的奥秘了吧。只有拥有了这个conf_ctx，才可以看到任意一个模块在create_conf中产生的结构体指针。同理，HTTP模块和mail
    模块也是这样做的，这些模块的通用接口中也有create_conf方法，其产生的指针会以相似的方式存放。
    图形化参考:深入理解NGINX中的图9-2
    */

/*
    在核心结构体ngx_cycle_t的conf_ctx成员指向的指针数组中，第7个指针由ngx_http_module模块使用（ngx_http_module模块的index序号为6，
由于由0开始，所以它在ngx_modules数组中排行第7。在存放全局配置结构体的conf_ctx数组中，第7个成员指向ngx_http_module模块），这个指针
设置为指向解析http{}块时生成的ngx_http_conf_ctx_t结构体，而ngx_http_conf_ctx_t的3个成员则分别指向新分配的3个指针数组。新的指针数组中
成员的意义由每个HTTP模块的ctx_index序号指定（ctx_index在HTTP模块中表明它处于HTTP模块间的序号），例如，第6个HTTP模块的ctx_index是5
（ctx_index同样由0开始计数），那么在ngx_http_conf_ctx_t的3个数组中，第6个成员就指向第6个HTTP模块的create_main_conf、create_srv_conf、
create_loc_conf方法建立的结构体，当然，如果相应的回调方法没有实现，该指针就为NULL空指针。
*/ 
/*
图解参考图10-1  见ngx_init_cycle  conf.ctx = cycle->conf_ctx; //这样下面的ngx_conf_param解析配置的时候，里面对conf.ctx赋值操作，实际上就是对cycle->conf_ctx[i]
可如何由ngx_cycle_t核心结构体中找到main级别的配置结构体呢？Nginx提供的ngx_http_cycle_get_module_main_conf宏可以实现这个功能
*/
    void                  ****conf_ctx; //有多少个模块就会有多少个指向这些模块的指针，见ngx_init_cycle   ngx_max_module
    ngx_pool_t               *pool; // 内存池

    /*    日志模块中提供了生成基本ngx_log_t日志对象的功能，这里的log实际上是在还没有执行ngx_init_cycle方法前，    
    也就是还没有解析配置前，如果有信息需要输出到日志，就会暂时使用log对象，它会输出到屏幕。    
    在ngx_init_cycle方法执行后，将会根据nginx.conf配置文件中的配置项，构造出正确的日志文件，此时会对log重新赋值。    */
    ngx_log_t                *log; //指向ngx_log_init中的ngx_log，如果配置error_log，指向这个配置后面的文件参数，见ngx_error_log。否则在ngx_log_open_default中设置
    
    /* 由nginx.conf配置文件读取到日志文件路径后，将开始初始化error_log日志文件，由于log对象还在用于输出日志到屏幕，    
    这时会用new_log对象暂时性地替代log日志，待初始化成功后，会用new_log的地址覆盖上面的log指针    */
    ngx_log_t                 new_log;//如果配置error_log，指向这个配置后面的文件参数，见ngx_error_log。否则在ngx_log_open_default中设置

    ngx_uint_t                log_use_stderr;  /* unsigned  log_use_stderr:1; */

    /*  对于poll，rtsig这样的事件模块，会以有效文件句柄数来预先建立这些ngx_connection t结构
体，以加速事件的收集、分发。这时files就会保存所有ngx_connection_t的指针组成的数组，files_n就是指
针的总数，而文件句柄的值用来访问files数组成员 */
    ngx_connection_t        **files; //sizeof(ngx_connection_t *) * cycle->files_n  见ngx_event_process_init  ngx_get_connection

    /*
        从图9-1中可以看出，在ngx_cycle_t中的connections和free_connections达两个成员构成了一个连接池，其中connections指向整个连
    接池数组的首部，而free_connections则指向第一个ngx_connection_t空闲连接。所有的空闲连接ngx_connection_t都以data成员（见9.3.1节）作
    为next指针串联成一个单链表，如此，一旦有用户发起连接时就从free_connections指向的链表头获取一个空闲的连接，同时free_connections再指
    向下一个空闲连接。而归还连接时只需把该连接插入到free_connections链表表头即可。
     */ //见ngx_event_process_init, ngx_connection_t空间和它当中的读写ngx_event_t存储空间都在该函数一次性分配好
    ngx_connection_t         *free_connections;// 可用连接池，与free_connection_n配合使用
    ngx_uint_t                free_connection_n;// 可用连接池中连接的总数

    //ngx_connection_s中的queue添加到该链表上
    /*
    通过读操作可以判断连接是否正常，如果不正常的话，就会把该ngx_close_connection->ngx_free_connection释放出来，这样
    如果之前free_connections上没有空余ngx_connection_t，c = ngx_cycle->free_connections;就可以获取到刚才释放出来的ngx_connection_t
    见ngx_drain_connections
    */ 
    ngx_queue_t               reusable_connections_queue;/* 双向链表容器，元素类型是ngx_connection_t结构体，表示可重复使用连接队列 表示可以重用的连接 */

//ngx_http_optimize_servers->ngx_http_init_listening->ngx_http_add_listening->ngx_create_listening把解析到的listen配置项信息添加到cycle->listening中
    //通过"listen"配置创建ngx_listening_t加入到该数组中
    ngx_array_t               listening;// 动态数组，每个数组元素储存着ngx_listening_t成员，表示监听端口及相关的参数

    /*    动态数组容器，它保存着nginx所有要操作的目录。如果有目录不存在，就会试图创建，而创建目录失败就会导致nginx启动失败。    */
    //通过解析配置文件获取到的路径添加到该数组，例如nginx.conf中的client_body_temp_path proxy_temp_path，参考ngx_conf_set_path_slot
    //这些配置可能设置重复的路径，因此不需要重复创建，通过ngx_add_path检测添加的路径是否重复，不重复则添加到paths中
    ngx_array_t               paths;//数组成员 nginx_path_t ，
    ngx_array_t               config_dump;

    /*    单链表容器，元素类型是ngx_open_file_t 结构体，它表示nginx已经打开的所有文件。事实上，nginx框架不会向open_files链表中添加文件。    
    而是由对此感兴趣的模块向其中添加文件路径名，nginx框架会在ngx_init_cycle 方法中打开这些文件    */
    //该链表中所包含的文件的打开在ngx_init_cycle中打开
    ngx_list_t                open_files; //如nginx.conf配置文件中的access_log参数的文件就保存在该链表中，参考ngx_conf_open_file
    //创建ngx_shm_zone_t在ngx_init_cycle，在ngx_shared_memory_add也可能创建新的ngx_shm_zone_t，为每个ngx_shm_zone_t真正分配共享内存空间在ngx_init_cycle
    ngx_list_t                shared_memory;// 单链表容器，元素类型是ngx_shm_zone_t结构体，每个元素表示一块共享内存

    ngx_uint_t                connection_n;// 当前进程中所有链接对象的总数，与connections成员配合使用
    ngx_uint_t                files_n; //每个进程能够打开的最多文件数  赋值见ngx_event_process_init

    /*
    从图9-1中可以看出，在ngx_cycle_t中的connections和free_connections达两个成员构成了一个连接池，其中connections指向整个连接池数组的首部，
    而free_connections则指向第一个ngx_connection_t空闲连接。所有的空闲连接ngx_connection_t都以data成员（见9.3.1节）作为next指针串联成一个
    单链表，如此，一旦有用户发起连接时就从free_connections指向的链表头获取一个空闲的连接，同时free_connections再指向下一个空闲连
    接。而归还连接时只需把该连接插入到free_connections链表表头即可。

    在connections指向的连接池中，每个连接所需要的读/写事件都以相同的数组序号对应着read_events、write_events读/写事件数组，
    相同序号下这3个数组中的元素是配合使用的
     */
    ngx_connection_t         *connections;// 指向当前进程中的所有连接对象，与connection_n配合使用

    /*
    事件是不需要创建的，因为Nginx在启动时已经在ngx_cycle_t的read_events成员中预分配了所有的读事件，并在write_events成员中预分配了所有的写事件

    在connections指向的连接池中，每个连接所需要的读/写事件都以相同的数组序号对应着read_events、write_events读/写事件数组，相同序号下这
    3个数组中的元素是配合使用的。图9-1中还显示了事件池，Nginx认为每一个连接一定至少需要一个读事件和一个写事件，有多少连接就分配多少个读、
    写事件。怎样把连接池中的任一个连接与读事件、写事件对应起来呢？很简单。由于读事件、写事件、连接池是由3个大小相同的数组组成，所以根据数组
    序号就可将每一个连接、读事件、写事件对应起来，这个对应关系在ngx_event_core_module模块的初始化过程中就已经决定了（参见9.5节）。这3个数组
    的大小都是由cycle->connection_n决定。
     */
    ngx_event_t              *read_events;// 指向当前进程中的所有读事件对象，connection_n同时表示所有读事件的总数
    ngx_event_t              *write_events;// 指向当前进程中的所有写事件对象，connection_n同时表示所有写事件的总数

    /*    旧的ngx_cycle_t 对象用于引用上一个ngx_cycle_t 对象中的成员。例如ngx_init_cycle 方法，在启动初期，    
    需要建立一个临时的ngx_cycle_t对象保存一些变量， 
    再调用ngx_init_cycle 方法时就可以把旧的ngx_cycle_t 对象传进去，    而这时old_cycle对象就会保存这个前期的ngx_cycle_t对象。    */
    ngx_cycle_t              *old_cycle;

    ngx_str_t                 conf_file;// 配置文件相对于安装目录的路径名称 默认为安装路径下的NGX_CONF_PATH,见ngx_process_options
    ngx_str_t                 conf_param;// nginx 处理配置文件时需要特殊处理的在命令行携带的参数，一般是-g 选项携带的参数
    ngx_str_t                 conf_prefix;    // nginx配置文件所在目录的路径  ngx_prefix 见ngx_process_options
    ngx_str_t                 prefix; //nginx安装目录的路径 ngx_prefix 见ngx_process_options
    ngx_str_t                 lock_file;// 用于进程间同步的文件锁名称
    ngx_str_t                 hostname; // 使用gethostname系统调用得到的主机名  在ngx_init_cycle中大写字母被转换为小写字母
};

typedef struct { //从ngx_cycle_s中的conf_ctx指向这里
     ngx_flag_t               daemon;
     ngx_flag_t               master; //在ngx_core_module_init_conf中初始化为1  通过参数"master_process"设置

     ngx_msec_t               timer_resolution; //从timer_resolution全局配置中解析到的参数,表示多少ms执行定时器中断，然后epoll_wail会返回跟新内存时间

     ngx_int_t                worker_processes;  //创建的worker进程数，通过nginx配置，默认为1  "worker_processes"设置
     ngx_int_t                debug_points;

     ngx_int_t                rlimit_nofile;
     off_t                    rlimit_core;

     int                      priority;

     ngx_uint_t               cpu_affinity_n;
     uint64_t                *cpu_affinity;

     char                    *username;
     ngx_uid_t                user;
     ngx_gid_t                group;

     ngx_str_t                working_directory;
     ngx_str_t                lock_file;

     ngx_str_t                pid;
     ngx_str_t                oldpid;

     ngx_array_t              env;
     char                   **environment;
} ngx_core_conf_t;


#define ngx_is_init_cycle(cycle)  (cycle->conf_ctx == NULL)


ngx_cycle_t *ngx_init_cycle(ngx_cycle_t *old_cycle);
ngx_int_t ngx_create_pidfile(ngx_str_t *name, ngx_log_t *log);
void ngx_delete_pidfile(ngx_cycle_t *cycle);
ngx_int_t ngx_signal_process(ngx_cycle_t *cycle, char *sig);
void ngx_reopen_files(ngx_cycle_t *cycle, ngx_uid_t user);
char **ngx_set_environment(ngx_cycle_t *cycle, ngx_uint_t *last);
ngx_pid_t ngx_exec_new_binary(ngx_cycle_t *cycle, char *const *argv);
uint64_t ngx_get_cpu_affinity(ngx_uint_t n);
ngx_shm_zone_t *ngx_shared_memory_add(ngx_conf_t *cf, ngx_str_t *name,
    size_t size, void *tag);


extern volatile ngx_cycle_t  *ngx_cycle;
extern ngx_array_t            ngx_old_cycles;
extern ngx_module_t           ngx_core_module;
extern ngx_uint_t             ngx_test_config;
extern ngx_uint_t             ngx_dump_config;
extern ngx_uint_t             ngx_quiet_mode;


#endif /* _NGX_CYCLE_H_INCLUDED_ */
