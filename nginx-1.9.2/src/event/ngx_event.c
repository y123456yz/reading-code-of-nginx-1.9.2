
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_log.h>

#define DEFAULT_CONNECTIONS  512


extern ngx_module_t ngx_kqueue_module;
extern ngx_module_t ngx_eventport_module;
extern ngx_module_t ngx_devpoll_module;
extern ngx_module_t ngx_epoll_module;
extern ngx_module_t ngx_select_module;


static char *ngx_event_init_conf(ngx_cycle_t *cycle, void *conf);
static ngx_int_t ngx_event_module_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_event_process_init(ngx_cycle_t *cycle);
static char *ngx_events_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static char *ngx_event_connections(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_event_use(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_event_debug_connection(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static void *ngx_event_core_create_conf(ngx_cycle_t *cycle);
static char *ngx_event_core_init_conf(ngx_cycle_t *cycle, void *conf);


/* 
nginx提供参数timer_resolution，设置缓存时间更新的间隔；
配置该项后，nginx将使用中断机制，而非使用定时器红黑树中的最小时间为epoll_wait的超时时间，即此时定时器将定期被中断。
timer_resolution指令的使用将会设置epoll_wait超时时间为-1，这表示epoll_wait将永远阻塞直至读写事件发生或信号中断。 

如果配置文件中使用了timer_ resolution配置项，也就是ngx_timer_resolution值大于0，则说明用；户希望服务器时间精确度为ngx_timer_resolution毫秒

*/ //ngx_timer_signal_handler定时器超时通过该值设置       如果设置了这个，则epoll_wait的返回是由定时器中断引起
//定时器设置在ngx_event_process_init，定时器生效在ngx_process_events_and_timers -> ngx_process_events
//从timer_resolution全局配置中解析到的参数,表示多少ms执行定时器中断，然后epoll_wail会返回跟新内存时间
static ngx_uint_t     ngx_timer_resolution; //不配置timer_resolution参数，该值为0   参考ngx_process_events_and_timers  单位是ms
sig_atomic_t          ngx_event_timer_alarm; //ngx_event_timer_alarm只是个全局变量，当它设为l时，表示需要更新时间。

static ngx_uint_t     ngx_event_max_module;//gx_event_max_module是编译进Nginx的所有事件模块的总个数。

ngx_uint_t            ngx_event_flags; //位图表示，见NGX_USE_FD_EVENT等  初始化见ngx_epoll_init
ngx_event_actions_t   ngx_event_actions; //ngx_event_actions = ngx_epoll_module_ctx.actions;


static ngx_atomic_t   connection_counter = 1;

//原子变量类型的ngx_connection_counter将统计所有建立过的连接数（包括主动发起的连接） 是总的连接数，不是某个进程的，是所有进程的，因为他们是共享内存的
ngx_atomic_t         *ngx_connection_counter = &connection_counter;
ngx_atomic_t         *ngx_accept_mutex_ptr; //指向共享的内存空间，见ngx_event_module_init
//ngx_accept_mutex为共享内存互斥锁  //获取到该锁的进程才会接受客户端的accept请求
ngx_shmtx_t           ngx_accept_mutex; //共享内存的空间  在建连接的时候，为了避免惊群，在accept的时候，只有获取到该原子锁，才把accept添加到epoll事件中，见ngx_trylock_accept_mutex


/*
注意:上面的ngx_accept_mutex_ptr和下面的其他全局变量ngx_use_accept_mutex等的区别?
ngx_accept_mutex_ptr是共享内存空间，所有进程共享，而下面的ngx_use_accept_mutex等全局变量是本进程可见的，其他进程不能使用该空间，不可见。
*/



//ngx_use_accept_mutex表示是否需要通过对accept加锁来解决惊群问题。当nginx worker进程数>1时且配置文件中打开accept_mutex时，这个标志置为1   
//具体实现:在创建子线程的时候，在执行ngx_event_process_init时并没有添加到epoll读事件中，worker抢到accept互斥体后，再放入epoll
//ccf->master && ccf->worker_processes > 1 && ecf->accept_mutex 条件满足才会置该标记为1
ngx_uint_t            ngx_use_accept_mutex; //那么会把ngx_use_accept_mutex置为1，可以避免惊群。赋值在ngx_event_process_init 
ngx_uint_t            ngx_accept_events; //只有eventport会用到该变量
/* ngx_accept_mutex_held是当前进程的一个全局变量，如果为l，则表示这个进程已经获取到了ngx_accept_mutex锁；如果为0，则表示没有获取到锁 */
//见ngx_process_events_and_timers会置位该位  如果flag置为该位，则ngx_epoll_process_events会延后处理epoll事件ngx_post_event
ngx_uint_t            ngx_accept_mutex_held; //1表示当前获取了ngx_accept_mutex锁   0表示当前并没有获取到ngx_accept_mutex锁   
//默认0.5s，可以由accept_mutex_delay进行配置
ngx_msec_t            ngx_accept_mutex_delay; //如果没获取到mutex锁，则延迟这么多毫秒重新获取。accept_mutex_delay配置，单位500ms

/*
ngx_accept_disabled表示此时满负荷，没必要再处理新连接了，我们在nginx.conf曾经配置了每一个nginx worker进程能够处理的最大连接数，
当达到最大数的7/8时，ngx_accept_disabled为正，说明本nginx worker进程非常繁忙，将不再去处理新连接，这也是个简单的负载均衡
*/
ngx_int_t             ngx_accept_disabled; //赋值地方在ngx_event_accept

/*
   作为Web服务器，Nginx具有统计整个服务器中HTTP连接状况的功能（不是某一个Nginx worker进程的状况，而是所有worker进程连接状况的总和）。
例如，可以用于统计某一时刻下Nginx巳经处理过的连接状况。下面定义的6个原子变量就是用于统计ngx_http_stub_status_module模块连接状况的，如下所示。

NGX_STAT_STUB选项通过下面的编译过程使能:
该模块在 auto/options文件中，通过下面的config选项把模块编译到nginx
    HTTP_STUB_STATUS=NO
    --with-http_stub_status_module)  HTTP_STUB_STATUS=YES       ;; 

*/
#if (NGX_STAT_STUB)

//已经建立成功过的TCP连接数
ngx_atomic_t   ngx_stat_accepted0;
ngx_atomic_t  *ngx_stat_accepted = &ngx_stat_accepted0;

/*
连接建立成功且获取到ngx_connection t结构体后，已经分配过内存池，并且在表示初始化了读/写事件后的连接数
*/
ngx_atomic_t   ngx_stat_handled0;
ngx_atomic_t  *ngx_stat_handled = &ngx_stat_handled0;

//已经由HTTP模块处理过的连接数
ngx_atomic_t   ngx_stat_requests0;
ngx_atomic_t  *ngx_stat_requests = &ngx_stat_requests0;

/*
已经从ngx_cycle_t核心结构体的free_connections连接池中获取到ngx_connection_t对象的活跃连接数
*/
ngx_atomic_t   ngx_stat_active0;
ngx_atomic_t  *ngx_stat_active = &ngx_stat_active0;

//正在接收TCP流的连接数
ngx_atomic_t   ngx_stat_reading0;
ngx_atomic_t  *ngx_stat_reading = &ngx_stat_reading0;

//正在发送TCP流的连接数
ngx_atomic_t   ngx_stat_writing0;
ngx_atomic_t  *ngx_stat_writing = &ngx_stat_writing0;

ngx_atomic_t   ngx_stat_waiting0;
ngx_atomic_t  *ngx_stat_waiting = &ngx_stat_waiting0;

#endif



static ngx_command_t  ngx_events_commands[] = {

    { ngx_string("events"),
      NGX_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_events_block,
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_core_module_t  ngx_events_module_ctx = {
    ngx_string("events"),
    NULL,
    ngx_event_init_conf
};

/*
可以看到，ngx_events_module_ctx实现的接口只是定义了模块名字而已，ngx_core_module_t接口中定义的create_onf方法没有实现（NULL空指针即为不实现），
为什么呢？这是因为ngx_events_module模块并不会解析配置项的参数，只是在出现events配置项后会调用各事件模块去解析eventso()块内的配置项，
自然就不需要实现create_conf方法来创建存储配置项参数的结构体. 
*/
//一旦在nginx.conf配置文件中找到ngx_events_module感兴趣的“events{}，ngx_events_module模块就开始工作了
//除了对events配置项的解析外，该模块没有做其他任何事情
ngx_module_t  ngx_events_module = {
    NGX_MODULE_V1,
    &ngx_events_module_ctx,                /* module context */
    ngx_events_commands,                   /* module directives */
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


static ngx_str_t  event_core_name = ngx_string("event_core");

//相关配置见ngx_event_core_commands ngx_http_core_commands ngx_stream_commands ngx_http_core_commands ngx_core_commands  ngx_mail_commands
static ngx_command_t  ngx_event_core_commands[] = {
    //每个worker进程可以同时处理的最大连接数
    //连接池的大小，也就是每个worker进程中支持的TCP最大连接数，它与下面的connections配置项的意义是重复的，可参照9.3.3节理解连接池的概念
    { ngx_string("worker_connections"),
      NGX_EVENT_CONF|NGX_CONF_TAKE1,
      ngx_event_connections,
      0,
      0,
      NULL },

    //设置事件模型。 use [kqueue | rtsig | epoll | dev/poll | select | poll | eventport] linux系统中只支持select poll epoll三种 
    //freebsd里的kqueue,LINUX中没有
    //确定选择哪一个事件模块作为事件驱动机制
    { ngx_string("use"),
      NGX_EVENT_CONF|NGX_CONF_TAKE1,
      ngx_event_use,
      0,
      0,
      NULL },
    //当事件模块通知有TCP连接时，尽可能在本次调度中对所有的客户端TCP连接请求都建立连接
    //对应于事件定义的available字段。对于epoll事件驱动模式来说，意味着在接收到一个新连接事件时，调用accept以尽可能多地接收连接
    { ngx_string("multi_accept"),
      NGX_EVENT_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_event_conf_t, multi_accept),
      NULL },

    //accept_mutex on|off是否打开accept进程锁，是为了实现worker进程接收连接的负载均衡、打开后让多个worker进程轮流的序列号的接收TCP连接
    //默认是打开的，如果关闭的话TCP连接会更快，但worker间的连接不会那么均匀。
    { ngx_string("accept_mutex"),
      NGX_EVENT_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_event_conf_t, accept_mutex),
      NULL },
    //accept_mutex_delay time，如果设置为accpt_mutex on，则worker同一时刻只有一个进程能个获取accept锁，这个accept锁不是阻塞的，如果娶不到会
    //立即返回，然后等待time时间重新获取。
    //启用accept_mutex负载均衡锁后，延迟accept_mutex_delay毫秒后再试图处理新连接事件
    { ngx_string("accept_mutex_delay"),
      NGX_EVENT_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      0,
      offsetof(ngx_event_conf_t, accept_mutex_delay),
      NULL },
    //debug_connection 1.2.2.2则在收到该IP地址请求的时候，使用debug级别打印。其他的还是沿用error_log中的设置
    //需要对来自指定IP的TCP连接打印debug级别的调斌日志
    { ngx_string("debug_connection"),
      NGX_EVENT_CONF|NGX_CONF_TAKE1,
      ngx_event_debug_connection,
      0,
      0,
      NULL },

      ngx_null_command
};

//ngx_event_core_module模块则仅实现了create_conf方法和init_conf方法，这是因为它并不真正负责TCP网络事件的驱动，
//所以不会实现ngx_event_actions_t中的方法
ngx_event_module_t  ngx_event_core_module_ctx = {
    &event_core_name,
    ngx_event_core_create_conf,            /* create configuration */
    ngx_event_core_init_conf,              /* init configuration */

    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

/*
Nginx定义了一系列（目前为9个）运行在不同操作系统、不同内核版本上的事件驱动模块，包括：ngx_epoll_module、ngx_kqueue_module、
ngx_poll_module、ngx_select_module、ngx_devpoll_module、ngx_eventport_module、ngx_aio_module、ngx_rtsig_module
和基于Windows的ngx_select_module模块。在ngx_event_core_module模块的初始化过程中，将会从以上9个模块中选取1个作为Nginx进程的事件驱动模块。
*/
ngx_module_t  ngx_event_core_module = {
    NGX_MODULE_V1,
    &ngx_event_core_module_ctx,            /* module context */
    ngx_event_core_commands,               /* module directives */
    NGX_EVENT_MODULE,                      /* module type */
    NULL,                                  /* init master */
    ngx_event_module_init,                 /* init module */ //解析完配置文件后执行
    ngx_event_process_init,                /* init process */ //在创建子进程的里面执行  ngx_worker_process_init
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


//当一次处理客户端请求结束后，会把ngx_http_process_request_line添加到定时器中，如果等client_header_timeout还没有信的请求数据过来，
//则会走到ngx_http_read_request_header中的ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);从而关闭连接

/*
在说nginx前，先来看看什么是“惊群”？简单说来，多线程/多进程（linux下线程进程也没多大区别）等待同一个socket事件，当这个事件发生时，
这些线程/进程被同时唤醒，就是惊群。可以想见，效率很低下，许多进程被内核重新调度唤醒，同时去响应这一个事件，当然只有一个进程能处理
事件成功，其他的进程在处理该事件失败后重新休眠（也有其他选择）。这种性能浪费现象就是惊群。

nginx就是这样，master进程监听端口号（例如80），所有的nginx worker进程开始用epoll_wait来处理新事件（linux下），如果不加任何保护，一个
新连接来临时，会有多个worker进程在epoll_wait后被唤醒，然后发现自己accept失败。现在，我们可以看看nginx是怎么处理这个惊群问题了。
*/

void
ngx_process_events_and_timers(ngx_cycle_t *cycle)  
{
    ngx_uint_t  flags;
    ngx_msec_t  timer, delta;
    
    /*nginx提供参数timer_resolution，设置缓存时间更新的间隔；
    配置该项后，nginx将使用中断机制，而非使用定时器红黑树中的最小时间为epoll_wait的超时时间，即此时定时器将定期被中断。
    timer_resolution指令的使用将会设置epoll_wait超时时间为-1，这表示epoll_wait将永远阻塞直至读写事件发生或信号中断。
    
    1.设置timer_resolution时，flags=0，只有当ngx_event_timer_alarm=1时epoll_wait()返回时才执行ngx_time_update（更新后会把ngx_event_timer_alarm置零）
    2.没有设置timer_resolution，flags = NGX_UPDATE_TIME，timer为定时器红黑树中最小定时时间，将作为epoll_wait的超时时间(timeout) */
    
    if (ngx_timer_resolution) {
        timer = NGX_TIMER_INFINITE; //如果设置了timer_resolution参数，timer为-1,也就是epoll_wait只有通过事件触发返回，定时器定时触发epoll_wait返回
        flags = 0;

    } else { //
        //如果没有设置timer_resolution定时器，则每次epoll_wait后跟新时间，否则每隔timer_resolution配置跟新一次时间，见ngx_epoll_process_events
        //获取离现在最近的超时定时器时间
        timer = ngx_event_find_timer();//例如如果一次accept的时候失败，则在ngx_event_accept中会把ngx_event_conf_t->accept_mutex_delay加入到红黑树定时器中
        flags = NGX_UPDATE_TIME; 
        
#if (NGX_WIN32)

        /* handle signals from master in case of network inactivity */

        if (timer == NGX_TIMER_INFINITE || timer > 500) {
            timer = 500;
        }

#endif
    }

    ngx_use_accept_mutex = 1;
   //ngx_use_accept_mutex表示是否需要通过对accept加锁来解决惊群问题。当nginx worker进程数>1时且配置文件中打开accept_mutex时，这个标志置为1   
    if (ngx_use_accept_mutex) {
        /*
              ngx_accept_disabled表示此时满负荷，没必要再处理新连接了，我们在nginx.conf曾经配置了每一个nginx worker进程能够处理的最大连接数，
          当达到最大数的7/8时，ngx_accept_disabled为正，说明本nginx worker进程非常繁忙，将不再去处理新连接，这也是个简单的负载均衡
              在当前使用的连接到达总连接数的7/8时，就不会再处理新连接了，同时，在每次调用process_events时都会将ngx_accept_disabled减1，
          直到ngx_accept_disabled降到总连接数的7/8以下时，才会调用ngx_trylock_accept_mutex试图去处理新连接事件。
          */
        if (ngx_accept_disabled > 0) { //为正说明可用连接用了超过八分之七,则让其他的进程在下面的else中来accept
            ngx_accept_disabled--;

        } else {
            /*
                 如果ngx_trylock_accept_mutex方法没有获取到锁，接下来调用事件驱动模块的process_events方法时只能处理已有的连接上的事件；
                 如果获取到了锁，调用process_events方法时就会既处理已有连接上的事件，也处理新连接的事件。
              
                如何用锁来避免惊群?
                   尝试锁accept mutex，只有成功获取锁的进程，才会将listen  
                   套接字放入epoll中。因此，这就保证了只有一个进程拥有  
                   监听套接口，故所有进程阻塞在epoll_wait时，不会出现惊群现象。  
                   这里的ngx_trylock_accept_mutex函数中，如果顺利的获取了锁，那么它会将监听端口注册到当前worker进程的epoll当中   

               获得accept锁，多个worker仅有一个可以得到这把锁。获得锁不是阻塞过程，都是立刻返回，获取成功的话ngx_accept_mutex_held被置为1。
               拿到锁，意味着监听句柄被放到本进程的epoll中了，如果没有拿到锁，则监听句柄会被从epoll中取出。 
              */
        /*
           如果ngx_use_accept_mutex为0也就是未开启accept_mutex锁，则在ngx_worker_process_init->ngx_event_process_init 中把accept连接读事件统计到epoll中
           否则在ngx_process_events_and_timers->ngx_process_events_and_timers->ngx_trylock_accept_mutex中把accept连接读事件统计到epoll中
           */
            if (ngx_trylock_accept_mutex(cycle) == NGX_ERROR) { //不管是获取到锁还是没获取到锁都是返回NGX_OK
                return;
            }

            /*
                拿到锁的话，置flag为NGX_POST_EVENTS，这意味着ngx_process_events函数中，任何事件都将延后处理，会把accept事件都放到
                ngx_posted_accept_events链表中，epollin|epollout事件都放到ngx_posted_events链表中 
               */
            if (ngx_accept_mutex_held) {
                flags |= NGX_POST_EVENTS;

            } else {
                /*
                    拿不到锁，也就不会处理监听的句柄，这个timer实际是传给epoll_wait的超时时间，修改为最大ngx_accept_mutex_delay意味
                    着epoll_wait更短的超时返回，以免新连接长时间没有得到处理   
                    */
                if (timer == NGX_TIMER_INFINITE
                    || timer > ngx_accept_mutex_delay)
                {   //如果没获取到锁，则延迟这么多ms重新获取说，继续循环，也就是技术锁被其他进程获得，本进程最多在epoll_wait中睡眠0.5s,然后返回
                    timer = ngx_accept_mutex_delay; //保证这么多时间超时的时候出发epoll_wait返回，从而可以更新内存时间
                }
            }
        }
    }

    delta = ngx_current_msec;

    /*
    1.如果进程获的锁，并获取到锁，则该进程在epoll事件发生后会触发返回，然后得到对应的事件handler，加入延迟队列中，然后释放锁，然
    后在执行对应handler，同时更新时间，判断该进程对应的红黑树中是否有定时器超时，
    2.如果没有获取到锁，则默认传给epoll_wait的超时时间是0.5s，表示过0.5s继续获取锁，0.5s超时后，会跟新当前时间，同时判断是否有过期的
      定时器，有则指向对应的定时器函数
    */

/*
1.ngx_event_s可以是普通的epoll读写事件(参考ngx_event_connect_peer->ngx_add_conn或者ngx_add_event)，通过读写事件触发

2.也可以是普通定时器事件(参考ngx_cache_manager_process_handler->ngx_add_timer(ngx_event_add_timer))，通过ngx_process_events_and_timers中的
epoll_wait返回，可以是读写事件触发返回，也可能是因为没获取到共享锁，从而等待0.5s返回重新获取锁来跟新事件并执行超时事件来跟新事件并且判断定
时器链表中的超时事件，超时则执行从而指向event的handler，然后进一步指向对应r或者u的->write_event_handler  read_event_handler

3.也可以是利用定时器expirt实现的读写事件(参考ngx_http_set_write_handler->ngx_add_timer(ngx_event_add_timer)),触发过程见2，只是在handler中不会执行write_event_handler  read_event_handler
*/
    
    //linux下，普通网络套接字调用ngx_epoll_process_events函数开始处理，异步文件i/o设置事件的回调方法为ngx_epoll_eventfd_handler
    (void) ngx_process_events(cycle, timer, flags);

    delta = ngx_current_msec - delta; //(void) ngx_process_events(cycle, timer, flags)中epoll等待事件触发过程花费的时间

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0, "epoll_wait timer range(delta): %M", delta);
             
    //当感应到来自于客户端的accept事件，epoll_wait返回后加入到post队列，执行完所有accpet连接事件后，立马释放ngx_accept_mutex锁，这样其他进程就可以立马获得锁accept客户端连接
    ngx_event_process_posted(cycle, &ngx_posted_accept_events); //一般执行ngx_event_accept
    
    //释放锁后再处理下面的EPOLLIN EPOLLOUT请求   
    if (ngx_accept_mutex_held) {
        ngx_shmtx_unlock(&ngx_accept_mutex);
    }

    if (delta) {
        ngx_event_expire_timers(); //处理红黑树队列中的超时事件handler
    }
    
    /*
     然后再处理正常的数据读写请求。因为这些请求耗时久，所以在ngx_process_events里NGX_POST_EVENTS标志将事件都放入ngx_posted_events
     链表中，延迟到锁释放了再处理。 
     */
    ngx_event_process_posted(cycle, &ngx_posted_events); //普通读写事件放在释放ngx_accept_mutex锁后执行，提高客户端accept性能
}

/*
ET（Edge Triggered）与LT（Level Triggered）的主要区别可以从下面的例子看出 
eg： 
1． 标示管道读者的文件句柄注册到epoll中； 
2． 管道写者向管道中写入2KB的数据； 
3． 调用epoll_wait可以获得管道读者为已就绪的文件句柄； 
4． 管道读者读取1KB的数据 
5． 一次epoll_wait调用完成 
如果是ET模式，管道中剩余的1KB被挂起，再次调用epoll_wait，得不到管道读者的文件句柄，除非有新的数据写入管道。如果是LT模式，
只要管道中有数据可读，每次调用epoll_wait都会触发。


epoll的两种模式LT和ET
二者的差异在于level-trigger模式下只要某个socket处于readable/writable状态，无论什么时候进行epoll_wait都会返回该socket；
而edge-trigger模式下只有某个socket从unreadable变为readable或从unwritable变为writable时，epoll_wait才会返回该socket。

所以，在epoll的ET模式下，正确的读写方式为:
读：只要可读，就一直读，直到返回0，或者 errno = EAGAIN
写:只要可写，就一直写，直到数据发送完，或者 errno = EAGAIN



ngx_handle_read_event方法会将读事件添加到事件驱动模块中，这样该事件对应的TCP连接上一旦出现可读事件（如接收到TCP连接另一
端发送来的字符流）就会回调该事件的handler方法。
    下面看一下ngx_handle_read_event的参数和返回值。参数rev是要操作的事件，flags将会指定事件的驱动方式。对于不同的事件驱动
模块，flags的取值范围并不同，本书以Linux下的epoll为例，对于ngx_epoll_module来说，flags的取值范围可以是0或者NGX_CLOSE_EVENT
（NGX_CLOSE_EVENT仅在epoll的LT水平触发模式下有效），Nginx主要工作在ET模式下，一般可以忽略flags这个参数。该方法返回NGX_0K表示成功，返回
NGX—ERRO耳表示失败。

简述Linux Epoll ET模式EPOLLOUT和EPOLLIN触发时刻 ET模式称为边缘触发模式，顾名思义，不到边缘情况，是死都不会触发的。

EPOLLOUT事件：
EPOLLOUT事件只有在连接时触发一次，表示可写，其他时候想要触发，那你要先准备好下面条件：
1.某次write，写满了发送缓冲区，返回错误码为EAGAIN。
2.对端读取了一些数据，又重新可写了，此时会触发EPOLLOUT。
简单地说：EPOLLOUT事件只有在不可写到可写的转变时刻，才会触发一次，所以叫边缘触发，这叫法没错的！

其实，如果你真的想强制触发一次，也是有办法的，直接调用epoll_ctl重新设置一下event就可以了，event跟原来的设置一模一样都行（但必须包含EPOLLOUT），关键是重新设置，就会马上触发一次EPOLLOUT事件。

EPOLLIN事件：
EPOLLIN事件则只有当对端有数据写入时才会触发，所以触发一次后需要不断读取所有数据直到读完EAGAIN为止。否则剩下的数据只有在下次对端有写入时才能一起取出来了。



关于epoll边缘触发模式（ET）下的EPOLLOUT触发，

ET模式下，EPOLLOUT触发条件有：
1.缓冲区满-->缓冲区非满；
2.同时监听EPOLLOUT和EPOLLIN事件时，当有IN 事件发生，都会顺带一个OUT事件；
3.一个客户端connect过来，accept成功后会触发一次OUT事件。

其中2最令人费解，内核代码这块有注释，说是一般有IN 时候都能OUT，就顺带一个，多给了个事件。。 (15.11.30验证了下，确实是这样)


以上，当只监听IN事件，读完数据后改为监听OUT事件，有时候会发现触发OUT事件并不方便，想要强制触发，可以重新设置一次要监听的events，带上EPOLLOUT即可。

另：
一道腾讯后台开发的面试题
使用Linux epoll模型，水平触发模式；当socket可写时，会不停的触发socket
可写的事件，如何处理？


第一种最普遍的方式：
需要向socket写数据的时候才把socket加入epoll，等待可写事件。接受到可写事件后，调用write或者send发送数据。当所有数据都写完后，把socket移出epoll。
这种方式的缺点是，即使发送很少的数据，也要把socket加入epoll，写完后在移出epoll，有一定操作代价。


一种改进的方式：
开始不把socket加入epoll，需要向socket写数据的时候，直接调用write或者send发送数据。如果返回EAGAIN，把socket加入epoll，在epoll
的驱动下写数据，全部数据发送完毕后，再移出epoll。
这种方式的优点是：数据不多的时候可以避免epoll的事件处理，提高效率。
*/
ngx_int_t
ngx_handle_read_event(ngx_event_t *rev, ngx_uint_t flags, const char* func, int line) //recv读取返回NGX_AGAIN后，需要再次ngx_handle_read_event来检测该fd在epoll上面的读事件
{
    char tmpbuf[128];
    
    if (ngx_event_flags & NGX_USE_CLEAR_EVENT) { //epoll边沿触发et模式

        /* kqueue, epoll */

        if (!rev->active && !rev->ready) {
            snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> epoll NGX_USE_CLEAR_EVENT(et) read add", func, line);
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, rev->log, 0, tmpbuf);
                       
            if (ngx_add_event(rev, NGX_READ_EVENT, NGX_CLEAR_EVENT)
                == NGX_ERROR)
            {
                return NGX_ERROR;
            }
        }

        return NGX_OK;

    } else if (ngx_event_flags & NGX_USE_LEVEL_EVENT) { //epoll水平促发模式

        /* select, poll, /dev/poll */

        if (!rev->active && !rev->ready) {
        
            snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> epoll NGX_USE_LEVEL_EVENT read add", func, line);
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, rev->log, 0, tmpbuf);
            
            if (ngx_add_event(rev, NGX_READ_EVENT, NGX_LEVEL_EVENT)
                == NGX_ERROR)
            {
                return NGX_ERROR;
            }

            return NGX_OK;
        }

        if (rev->active && (rev->ready || (flags & NGX_CLOSE_EVENT))) {
        
            snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> epoll NGX_USE_LEVEL_EVENT read del", func, line);
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, rev->log, 0, tmpbuf);
            
            if (ngx_del_event(rev, NGX_READ_EVENT, NGX_LEVEL_EVENT | flags)
                == NGX_ERROR)
            {
                return NGX_ERROR;
            }

            return NGX_OK;
        }

    } else if (ngx_event_flags & NGX_USE_EVENTPORT_EVENT) {

        /* event ports */

        if (!rev->active && !rev->ready) {
            if (ngx_add_event(rev, NGX_READ_EVENT, 0) == NGX_ERROR) {
                return NGX_ERROR;
            }

            return NGX_OK;
        }

        if (rev->oneshot && !rev->ready) {
            if (ngx_del_event(rev, NGX_READ_EVENT, 0) == NGX_ERROR) {
                return NGX_ERROR;
            }

            return NGX_OK;
        }
    }

    /* iocp */

    return NGX_OK;
}

/*
ngx_handle—write—event方法会将写辜件添加到事件驱动模块中。wev是要操作的事件，而lowat则表示只有当连接对应的套接字缓冲区中必须有
lowat大小的可用空间时，事件收集器（如select或者epoll_wait调用）才能处理这个可写事件（lowat参数为0时表示不考虑
可写缓冲区的大小）。该方法返回NGX一OK表示成功，返回NGX ERROR表示失败。

EOIKK EPOLLOUT事件只有在连接时触发一次，表示可写，其他时候想要触发，那你要先准备好下面条件：
1.某次write，写满了发送缓冲区，返回错误码为EAGAIN。
2.对端读取了一些数据，又重新可写了，此时会触发EPOLLOUT。

简述Linux Epoll ET模式EPOLLOUT和EPOLLIN触发时刻 ET模式称为边缘触发模式，顾名思义，不到边缘情况，是死都不会触发的。

EPOLLOUT事件：
EPOLLOUT事件只有在连接时触发一次，表示可写，其他时候想要触发，那你要先准备好下面条件：
1.某次write，写满了发送缓冲区，返回错误码为EAGAIN。
2.对端读取了一些数据，又重新可写了，此时会触发EPOLLOUT。
简单地说：EPOLLOUT事件只有在不可写到可写的转变时刻，才会触发一次，所以叫边缘触发，这叫法没错的！

其实，如果你真的想强制触发一次，也是有办法的，直接调用epoll_ctl重新设置一下event就可以了，event跟原来的设置一模一样都行（但必须包含EPOLLOUT），关键是重新设置，就会马上触发一次EPOLLOUT事件。

EPOLLIN事件：
EPOLLIN事件则只有当对端有数据写入时才会触发，所以触发一次后需要不断读取所有数据直到读完EAGAIN为止。否则剩下的数据只有在下次对端有写入时才能一起取出来了。


*/ ////write读取返回NGX_AGAIN后，需要再次ngx_handle_write_event来检测该fd在epoll上面的读事件
ngx_int_t
ngx_handle_write_event(ngx_event_t *wev, size_t lowat, const char* func, int line) 
{
    ngx_connection_t  *c;
    char tmpbuf[256];
    
    if (lowat) {
        c = wev->data;

        if (ngx_send_lowat(c, lowat) == NGX_ERROR) {
            return NGX_ERROR;
        }
    }

    if (ngx_event_flags & NGX_USE_CLEAR_EVENT) {

        /* kqueue, epoll */

        if (!wev->active && !wev->ready) {
            snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> epoll NGX_USE_CLEAR_EVENT write add", func, line);
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, wev->log, 0, tmpbuf);
            
            if (ngx_add_event(wev, NGX_WRITE_EVENT,
                              NGX_CLEAR_EVENT | (lowat ? NGX_LOWAT_EVENT : 0))
                == NGX_ERROR)
            {
                return NGX_ERROR;
            }
        }

        return NGX_OK;

    } else if (ngx_event_flags & NGX_USE_LEVEL_EVENT) {

        /* select, poll, /dev/poll */

        if (!wev->active && !wev->ready) {

            snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> epoll NGX_USE_LEVEL_EVENT write add", func, line);
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, wev->log, 0, tmpbuf);
            
            if (ngx_add_event(wev, NGX_WRITE_EVENT, NGX_LEVEL_EVENT)
                == NGX_ERROR)
            {
                return NGX_ERROR;
            }

            return NGX_OK;
        }

        if (wev->active && wev->ready) {

            snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> epoll NGX_USE_LEVEL_EVENT write del", func, line);
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, wev->log, 0, tmpbuf);
            
            if (ngx_del_event(wev, NGX_WRITE_EVENT, NGX_LEVEL_EVENT)
                == NGX_ERROR)
            {
                return NGX_ERROR;
            }

            return NGX_OK;
        }

    } else if (ngx_event_flags & NGX_USE_EVENTPORT_EVENT) {

        /* event ports */

        if (!wev->active && !wev->ready) {
            if (ngx_add_event(wev, NGX_WRITE_EVENT, 0) == NGX_ERROR) {
                return NGX_ERROR;
            }

            return NGX_OK;
        }

        if (wev->oneshot && wev->ready) {
            if (ngx_del_event(wev, NGX_WRITE_EVENT, 0) == NGX_ERROR) {
                return NGX_ERROR;
            }

            return NGX_OK;
        }
    }

    /* iocp */

    return NGX_OK;
}


static char *
ngx_event_init_conf(ngx_cycle_t *cycle, void *conf)
{
    if (ngx_get_conf(cycle->conf_ctx, ngx_events_module) == NULL) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "no \"events\" section in configuration");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

/*
ngx_event_module_init方法其实很简单，它主要初始化了一些变量，尤其是ngx_http_stub_status_module统计模块使用的一些原子性的统计变量
*/
static ngx_int_t
ngx_event_module_init(ngx_cycle_t *cycle)
{
    void              ***cf;
    u_char              *shared;
    size_t               size, cl;
    ngx_shm_t            shm;
    ngx_time_t          *tp;
    ngx_core_conf_t     *ccf;
    ngx_event_conf_t    *ecf;

    cf = ngx_get_conf(cycle->conf_ctx, ngx_events_module);
    ecf = (*cf)[ngx_event_core_module.ctx_index];

    if (!ngx_test_config && ngx_process <= NGX_PROCESS_MASTER) {
        ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0,
                      "using the \"%s\" event method", ecf->name);
    }

    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);

    ngx_timer_resolution = ccf->timer_resolution;

#if !(NGX_WIN32)
    {
        ngx_int_t      limit;
        struct rlimit  rlmt;

        if (getrlimit(RLIMIT_NOFILE, &rlmt) == -1) { // 每个进程能打开的最多文件数。 
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          "getrlimit(RLIMIT_NOFILE) failed, ignored");

        } else {
            if (ecf->connections > (ngx_uint_t) rlmt.rlim_cur
                && (ccf->rlimit_nofile == NGX_CONF_UNSET
                    || ecf->connections > (ngx_uint_t) ccf->rlimit_nofile))
            {
                limit = (ccf->rlimit_nofile == NGX_CONF_UNSET) ?
                             (ngx_int_t) rlmt.rlim_cur : ccf->rlimit_nofile;

                ngx_log_error(NGX_LOG_WARN, cycle->log, 0,
                              "%ui worker_connections exceed "
                              "open file resource limit: %i",
                              ecf->connections, limit);
            }
        }
    }
#endif /* !(NGX_WIN32) */


    if (ccf->master == 0) {
        return NGX_OK;
    }

    if (ngx_accept_mutex_ptr) {
        return NGX_OK;
    }


    /* cl should be equal to or greater than cache line size */

/*
计算出需要使用的共享内存的大小。为什么每个统计成员需要使用128字节呢？这似乎太大了，看上去，每个ngx_atomic_t原子变量最多需要8字
节而已。其实是因为Nginx充分考虑了CPU的二级缓存。在目前许多CPU架构下缓存行的大小都是128字节，而下面需要统计的变量都是访问非常频
繁的成员，同时它们占用的内存又非常少，所以采用了每个成员都使用128字节存放的形式，这样速度更快
*/
    cl = 128;

    size = cl            /* ngx_accept_mutex */
           + cl          /* ngx_connection_counter */
           + cl;         /* ngx_temp_number */

#if (NGX_STAT_STUB)

    size += cl           /* ngx_stat_accepted */
           + cl          /* ngx_stat_handled */
           + cl          /* ngx_stat_requests */
           + cl          /* ngx_stat_active */
           + cl          /* ngx_stat_reading */
           + cl          /* ngx_stat_writing */
           + cl;         /* ngx_stat_waiting */

#endif

    shm.size = size;
    shm.name.len = sizeof("nginx_shared_zone") - 1;
    shm.name.data = (u_char *) "nginx_shared_zone";
    shm.log = cycle->log;

    //开辟一块共享内存，共享内存的大小为shm.size
    if (ngx_shm_alloc(&shm) != NGX_OK) {
        return NGX_ERROR;
    }

    //共享内存的首地址就在shm.addr成员中
    shared = shm.addr;

    //原子变量类型的accept锁使用了128字节的共享内存
    ngx_accept_mutex_ptr = (ngx_atomic_t *) shared;

    /*
     ngx_accept_mutex就是负载均衡锁，spin值为-1则是告诉Nginx这把锁不可以使进程进入睡眠状态
     */
    ngx_accept_mutex.spin = (ngx_uint_t) -1;

    if (ngx_shmtx_create(&ngx_accept_mutex, (ngx_shmtx_sh_t *) shared,
                         cycle->lock_file.data)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    //原予变量类型的ngx_connection counter将统计所有建立过的连接数（包括主动发起的连接）
    ngx_connection_counter = (ngx_atomic_t *) (shared + 1 * cl);

    (void) ngx_atomic_cmp_set(ngx_connection_counter, 0, 1);

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                   "counter: %p, %d",
                   ngx_connection_counter, *ngx_connection_counter);

    ngx_temp_number = (ngx_atomic_t *) (shared + 2 * cl);

    tp = ngx_timeofday();

    ngx_random_number = (tp->msec << 16) + ngx_pid;

#if (NGX_STAT_STUB)
    //依次初始化需要统计的6个原子变量，也就是使用共享内存作为原子变量
    ngx_stat_accepted = (ngx_atomic_t *) (shared + 3 * cl);
    ngx_stat_handled = (ngx_atomic_t *) (shared + 4 * cl);
    ngx_stat_requests = (ngx_atomic_t *) (shared + 5 * cl);
    ngx_stat_active = (ngx_atomic_t *) (shared + 6 * cl);
    ngx_stat_reading = (ngx_atomic_t *) (shared + 7 * cl);
    ngx_stat_writing = (ngx_atomic_t *) (shared + 8 * cl);
    ngx_stat_waiting = (ngx_atomic_t *) (shared + 9 * cl);

#endif

    return NGX_OK;
}


#if !(NGX_WIN32)

//ngx_event_timer_alarm只是个全局变量，当它设为l时，表示需要更新时间。
/*
在ngx_event_ actions t的process_events方法中，每一个事件驱动模块都需要在ngx_event_timer_alarm为1时调
用ngx_time_update方法（）更新系统时间，在更新系统结束后需要将ngx_event_timer_alarm设为0。
*/ //定时器超时触发epoll_wait返回，返回处理后才会执行timer超时handler  ngx_timer_signal_handler
static void
ngx_timer_signal_handler(int signo)
{
    ngx_event_timer_alarm = 1;
#if 1
    ngx_log_debug0(NGX_LOG_DEBUG_EVENT, ngx_cycle->log, 0, "timer signal");
#endif
}

#endif
//在创建子进程的里面执行  ngx_worker_process_init，
static ngx_int_t
ngx_event_process_init(ngx_cycle_t *cycle)
{
    ngx_uint_t           m, i;
    ngx_event_t         *rev, *wev;
    ngx_listening_t     *ls;
    ngx_connection_t    *c, *next, *old;
    ngx_core_conf_t     *ccf;
    ngx_event_conf_t    *ecf;
    ngx_event_module_t  *module;

    ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);
    ecf = ngx_event_get_conf(cycle->conf_ctx, ngx_event_core_module);

    /*
         当打开accept_mutex负载均衡锁，同时使用了master模式并且worker迸程数量大于1时，才正式确定了进程将使用accept_mutex负载均衡锁。
     因此，即使我们在配置文件中指定打开accept_mutex锁，如果没有使用master模式或者worker进程数量等于1，进程在运行时还是不会使用
     负载均衡锁（既然不存在多个进程去抢一个监听端口上的连接的情况，那么自然不需要均衡多个worker进程的负载）。
         这时会将ngx_use_accept_mutex全局变量置为1，ngx_accept_mutex_held标志设为0，ngx_accept_mutex_delay则设为在配置文件中指定的最大延迟时间。
     */
    if (ccf->master && ccf->worker_processes > 1 && ecf->accept_mutex) {
        ngx_use_accept_mutex = 1;
        ngx_accept_mutex_held = 0;
        ngx_accept_mutex_delay = ecf->accept_mutex_delay;

    } else {
        ngx_use_accept_mutex = 0;
    }

#if (NGX_WIN32)

    /*
     * disable accept mutex on win32 as it may cause deadlock if
     * grabbed by a process which can't accept connections
     */

    ngx_use_accept_mutex = 0;

#endif

    ngx_queue_init(&ngx_posted_accept_events);
    ngx_queue_init(&ngx_posted_events);

    //初始化红黑树实现的定时器。
    if (ngx_event_timer_init(cycle->log) == NGX_ERROR) {
        return NGX_ERROR;
    }

    //在调用use配置项指定的事件模块中，在ngx_event_module_t接口下，ngx_event_actions_t中的init方法进行这个事件模块的初始化工作。
    for (m = 0; ngx_modules[m]; m++) {
        if (ngx_modules[m]->type != NGX_EVENT_MODULE) {
            continue;
        }

        if (ngx_modules[m]->ctx_index != ecf->use) { //找到epoll或者select的module模块
            continue;
        }

        module = ngx_modules[m]->ctx;

        if (module->actions.init(cycle, ngx_timer_resolution) != NGX_OK) { //执行epoll module中的ngx_epoll_init
            /* fatal */
            exit(2);
        }

        break; /*跳出循环，只可能使用一个具体的事件模型*/  
    }

#if !(NGX_WIN32)
    /*
    如果nginx.conf配置文件中设置了timer_resolution酡置项，即表明需要控制时间精度，这时会调用setitimer方法，设置时间间隔
    为timer_resolution毫秒来回调ngx_timer_signal_handler方法
     */
    if (ngx_timer_resolution && !(ngx_event_flags & NGX_USE_TIMER_EVENT)) {
        struct sigaction  sa;
        struct itimerval  itv;
        
        //设置定时器
        /*
            在ngx_event_ actions t的process_events方法中，每一个事件驱动模块都需要在ngx_event_timer_alarm为1时调
            用ngx_time_update方法（）更新系统时间，在更新系统结束后需要将ngx_event_timer_alarm设为0。
          */
        ngx_memzero(&sa, sizeof(struct sigaction)); //每隔ngx_timer_resolution ms会超时执行handle
        sa.sa_handler = ngx_timer_signal_handler;
        sigemptyset(&sa.sa_mask);

        if (sigaction(SIGALRM, &sa, NULL) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          "sigaction(SIGALRM) failed");
            return NGX_ERROR;
        }

        itv.it_interval.tv_sec = ngx_timer_resolution / 1000;
        itv.it_interval.tv_usec = (ngx_timer_resolution % 1000) * 1000;
        itv.it_value.tv_sec = ngx_timer_resolution / 1000;
        itv.it_value.tv_usec = (ngx_timer_resolution % 1000 ) * 1000;

        if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          "setitimer() failed");
        }
    }

    /* 
     如果使用了epoll事件驱动模式，那么会为ngx_cycle_t结构体中的files成员预分配旬柄。
     */
    if (ngx_event_flags & NGX_USE_FD_EVENT) {
        struct rlimit  rlmt;

        if (getrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          "getrlimit(RLIMIT_NOFILE) failed");
            return NGX_ERROR;
        }

        cycle->files_n = (ngx_uint_t) rlmt.rlim_cur; //每个进程能够打开的最多文件数

        cycle->files = ngx_calloc(sizeof(ngx_connection_t *) * cycle->files_n,
                                  cycle->log);
        if (cycle->files == NULL) {
            return NGX_ERROR;
        }
    }

#endif

    cycle->connections =
        ngx_alloc(sizeof(ngx_connection_t) * cycle->connection_n, cycle->log);
    if (cycle->connections == NULL) {
        return NGX_ERROR;
    }

    c = cycle->connections;

    cycle->read_events = ngx_alloc(sizeof(ngx_event_t) * cycle->connection_n,
                                   cycle->log);
    if (cycle->read_events == NULL) {
        return NGX_ERROR;
    }

    rev = cycle->read_events;
    for (i = 0; i < cycle->connection_n; i++) {
        rev[i].closed = 1;
        rev[i].instance = 1;
    }

    cycle->write_events = ngx_alloc(sizeof(ngx_event_t) * cycle->connection_n,
                                    cycle->log);
    if (cycle->write_events == NULL) {
        return NGX_ERROR;
    }

    wev = cycle->write_events;
    for (i = 0; i < cycle->connection_n; i++) {
        wev[i].closed = 1;
    }

    i = cycle->connection_n;
    next = NULL;

    /*
    接照序号，将上述3个数组相应的读/写事件设置到每一个ngx_connection_t连接对象中，同时把这些连接以ngx_connection_t中的data成员
    作为next指针串联成链表，为下一步设置空闲连接链表做好准备
     */
    do {
        i--;

        c[i].data = next;
        c[i].read = &cycle->read_events[i];
        c[i].write = &cycle->write_events[i];
        c[i].fd = (ngx_socket_t) -1;

        next = &c[i];
    } while (i);

    /*
    将ngx_cycle_t结构体中的空闲连接链表free_connections指向connections数组的最后1个元素，也就是第10步所有ngx_connection_t连
    接通过data成员组成的单链表的首部。
     */
    cycle->free_connections = next;
    cycle->free_connection_n = cycle->connection_n;

    /* for each listening socket */
    /*
     在刚刚建立好的连接池中，为所有ngx_listening_t监听对象中的connection成员分配连接，同时对监听端口的读事件设置处理方法
     为ngx_event_accept，也就是说，有新连接事件时将调用ngx_event_accept方法建立新连接（）。
     */
    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) { 

#if (NGX_HAVE_REUSEPORT)
        //master进程执行ngx_clone_listening中如果配置了多worker，监听80端口会有worker个listen赋值，master进程在ngx_open_listening_sockets
        //中会监听80端口worker次，那么子进程创建起来后，不是每个字进程都关注这worker多个 listen事件了吗?为了避免这个问题，nginx通过
        //在子进程运行ngx_event_process_init函数的时候，通过ngx_add_event来控制子进程关注的listen，最终实现只关注master进程中创建的一个listen事件
        if (ls[i].reuseport && ls[i].worker != ngx_worker) {
            continue;
        }
#endif
        
        c = ngx_get_connection(ls[i].fd, cycle->log); //从连接池中获取一个ngx_connection_t

        if (c == NULL) {
            return NGX_ERROR;
        }

        c->log = &ls[i].log;

        c->listening = &ls[i]; //把解析到listen配置项信息赋值给ngx_connection_s中的listening中
        ls[i].connection = c;

        rev = c->read;

        rev->log = c->log;
        rev->accept = 1;

#if (NGX_HAVE_DEFERRED_ACCEPT)
        rev->deferred_accept = ls[i].deferred_accept;
#endif

        if (!(ngx_event_flags & NGX_USE_IOCP_EVENT)) {
            if (ls[i].previous) {

                /*
                 * delete the old accept events that were bound to
                 * the old cycle read events array
                 */

                old = ls[i].previous->connection;

                if (ngx_del_event(old->read, NGX_READ_EVENT, NGX_CLOSE_EVENT)
                    == NGX_ERROR)
                {
                    return NGX_ERROR;
                }

                old->fd = (ngx_socket_t) -1;
            }
        }

#if (NGX_WIN32)

        if (ngx_event_flags & NGX_USE_IOCP_EVENT) {
            ngx_iocp_conf_t  *iocpcf;
            rev->handler = ngx_event_acceptex;

            if (ngx_use_accept_mutex) {
                continue;
            }

            if (ngx_add_event(rev, 0, NGX_IOCP_ACCEPT) == NGX_ERROR) {
                return NGX_ERROR;
            }

            ls[i].log.handler = ngx_acceptex_log_error;

            iocpcf = ngx_event_get_conf(cycle->conf_ctx, ngx_iocp_module);
            if (ngx_event_post_acceptex(&ls[i], iocpcf->post_acceptex)
                == NGX_ERROR)
            {
                return NGX_ERROR;
            }

        } else {
            rev->handler = ngx_event_accept;

            if (ngx_use_accept_mutex) {
                continue;
            }

            if (ngx_add_event(rev, NGX_READ_EVENT, 0) == NGX_ERROR) {
                return NGX_ERROR;
            }
        }

#else
        /*
        对监听端口的读事件设置处理方法
        为ngx_event_accept，也就是说，有新连接事件时将调用ngx_event_accept方法建立新连接
          */
        rev->handler = ngx_event_accept; 

        /* 
          使用了accept_mutex，暂时不将监听套接字放入epoll中, 而是等到worker抢到accept互斥体后，再放入epoll，避免惊群的发生。 
          */ //在建连接的时候，为了避免惊群，在accept的时候，只有获取到该原子锁，才把accept添加到epoll事件中，见ngx_process_events_and_timers->ngx_trylock_accept_mutex
        if (ngx_use_accept_mutex
#if (NGX_HAVE_REUSEPORT)
            && !ls[i].reuseport
#endif
           ) //如果是单进程方式
        {
            continue;
        }

        /*
          将监听对象连接的读事件添加到事件驱动模块中，这样，epoll等事件模块就开始检测监听服务，并开始向用户提供服务了。
          */ //如果ngx_use_accept_mutex为0也就是未开启accept_mutex锁，则在ngx_worker_process_init->ngx_event_process_init 中把accept连接读事件统计到epoll中
          //否则在ngx_process_events_and_timers->ngx_process_events_and_timers->ngx_trylock_accept_mutex中把accept连接读事件统计到epoll中

        char tmpbuf[256];
        
        snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> epoll NGX_READ_EVENT(et) read add", NGX_FUNC_LINE);
        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, cycle->log, 0, tmpbuf);
        if (ngx_add_event(rev, NGX_READ_EVENT, 0) == NGX_ERROR) { //如果是epoll则为ngx_epoll_add_event
            return NGX_ERROR;
        }

#endif  

    }

    return NGX_OK;
}


ngx_int_t
ngx_send_lowat(ngx_connection_t *c, size_t lowat)
{
    int  sndlowat;

#if (NGX_HAVE_LOWAT_EVENT)

    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {
        c->write->available = lowat;
        return NGX_OK;
    }

#endif

    if (lowat == 0 || c->sndlowat) {
        return NGX_OK;
    }

    sndlowat = (int) lowat;
    /*
     SO_RCVLOWAT SO_SNDLOWAT 
     每个套接口都有一个接收低潮限度和一个发送低潮限度。
     接收低潮限度：对于TCP套接口而言，接收缓冲区中的数据必须达到规定数量，内核才通知进程“可读”。比如触发select或者epoll，返回“套接口可读”。
     发送低潮限度：对于TCP套接口而言，和接收低潮限度一个道理。
     */
    if (setsockopt(c->fd, SOL_SOCKET, SO_SNDLOWAT,
                   (const void *) &sndlowat, sizeof(int))
        == -1)
    {
        ngx_connection_error(c, ngx_socket_errno,
                             "setsockopt(SO_SNDLOWAT) failed");
        return NGX_ERROR;
    }

    c->sndlowat = 1;

    return NGX_OK;
}

static char *
ngx_events_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char                 *rv;
    void               ***ctx;
    ngx_uint_t            i;
    ngx_conf_t            pcf;
    ngx_event_module_t   *m;

    if (*(void **) conf) {
        return "is duplicate";
    }

    /* count the number of the event modules and set up their indices */

    ngx_event_max_module = 0;
    for (i = 0; ngx_modules[i]; i++) {
        if (ngx_modules[i]->type != NGX_EVENT_MODULE) {
            continue;
        }

        ngx_modules[i]->ctx_index = ngx_event_max_module++;
    }

    ctx = ngx_pcalloc(cf->pool, sizeof(void *));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    *ctx = ngx_pcalloc(cf->pool, ngx_event_max_module * sizeof(void *));
    if (*ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    //conf为ngx_conf_handler中的conf = confp[ngx_modules[i]->ctx_index];也就是conf指向的是ngx_cycle_s->conf_ctx[]，
    //所以对conf赋值就是对ngx_cycle_s中的conf_ctx赋值,最终就是ngx_cycle_s中的conf_ctx[ngx_events_module=>index]指向了ctx
    *(void **) conf = ctx;

    for (i = 0; ngx_modules[i]; i++) {
        if (ngx_modules[i]->type != NGX_EVENT_MODULE) {
            continue;
        }

        m = ngx_modules[i]->ctx;

        if (m->create_conf) {
            (*ctx)[ngx_modules[i]->ctx_index] = m->create_conf(cf->cycle);
            if ((*ctx)[ngx_modules[i]->ctx_index] == NULL) {
                return NGX_CONF_ERROR;
            }
        }
    }

    //零时保存之前的cf,在下面解析完event{}配置后，在恢复
    pcf = *cf;
    cf->ctx = ctx;
    cf->module_type = NGX_EVENT_MODULE;
    cf->cmd_type = NGX_EVENT_CONF;

    rv = ngx_conf_parse(cf, NULL);//这时候cf里面的上下文ctx为NGX_EVENT_MODULE模块create_conf的用于存储event{}的空间

    *cf = pcf; //解析完event{}配置后，恢复

    if (rv != NGX_CONF_OK) {
        return rv;
    }

    for (i = 0; ngx_modules[i]; i++) {
        if (ngx_modules[i]->type != NGX_EVENT_MODULE) {
            continue;
        }

        m = ngx_modules[i]->ctx;

        if (m->init_conf) {
            rv = m->init_conf(cf->cycle, (*ctx)[ngx_modules[i]->ctx_index]);
            if (rv != NGX_CONF_OK) {
                return rv;
            }
        }
    }

    return NGX_CONF_OK;
}


static char *
ngx_event_connections(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_event_conf_t  *ecf = conf;

    ngx_str_t  *value;

    if (ecf->connections != NGX_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elts;
    ecf->connections = ngx_atoi(value[1].data, value[1].len);
    if (ecf->connections == (ngx_uint_t) NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid number \"%V\"", &value[1]);

        return NGX_CONF_ERROR;
    }

    cf->cycle->connection_n = ecf->connections;

    return NGX_CONF_OK;
}


static char *
ngx_event_use(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_event_conf_t  *ecf = conf;

    ngx_int_t             m;
    ngx_str_t            *value;
    ngx_event_conf_t     *old_ecf;
    ngx_event_module_t   *module;

    if (ecf->use != NGX_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (cf->cycle->old_cycle->conf_ctx) {
        old_ecf = ngx_event_get_conf(cf->cycle->old_cycle->conf_ctx,
                                     ngx_event_core_module);
    } else {
        old_ecf = NULL;
    }


    for (m = 0; ngx_modules[m]; m++) {
        if (ngx_modules[m]->type != NGX_EVENT_MODULE) {
            continue;
        }

        module = ngx_modules[m]->ctx;
        if (module->name->len == value[1].len) {
            if (ngx_strcmp(module->name->data, value[1].data) == 0) {
                ecf->use = ngx_modules[m]->ctx_index;
                ecf->name = module->name->data;

                if (ngx_process == NGX_PROCESS_SINGLE
                    && old_ecf
                    && old_ecf->use != ecf->use)
                {
                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "when the server runs without a master process "
                               "the \"%V\" event type must be the same as "
                               "in previous configuration - \"%s\" "
                               "and it cannot be changed on the fly, "
                               "to change it you need to stop server "
                               "and start it again",
                               &value[1], old_ecf->name);

                    return NGX_CONF_ERROR;
                }

                return NGX_CONF_OK;
            }
        }
    }

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid event type \"%V\"", &value[1]);

    return NGX_CONF_ERROR;
}


static char *
ngx_event_debug_connection(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
#if (NGX_DEBUG)
    ngx_event_conf_t  *ecf = conf;

    ngx_int_t             rc;
    ngx_str_t            *value;
    ngx_url_t             u;
    ngx_cidr_t            c, *cidr;
    ngx_uint_t            i;
    struct sockaddr_in   *sin;
#if (NGX_HAVE_INET6)
    struct sockaddr_in6  *sin6;
#endif

    value = cf->args->elts;

#if (NGX_HAVE_UNIX_DOMAIN)

    if (ngx_strcmp(value[1].data, "unix:") == 0) {
        cidr = ngx_array_push(&ecf->debug_connection);
        if (cidr == NULL) {
            return NGX_CONF_ERROR;
        }

        cidr->family = AF_UNIX;
        return NGX_CONF_OK;
    }

#endif

    rc = ngx_ptocidr(&value[1], &c);

    if (rc != NGX_ERROR) {
        if (rc == NGX_DONE) {
            ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                               "low address bits of %V are meaningless",
                               &value[1]);
        }

        cidr = ngx_array_push(&ecf->debug_connection);
        if (cidr == NULL) {
            return NGX_CONF_ERROR;
        }

        *cidr = c;

        return NGX_CONF_OK;
    }

    ngx_memzero(&u, sizeof(ngx_url_t));
    u.host = value[1];

    if (ngx_inet_resolve_host(cf->pool, &u) != NGX_OK) {
        if (u.err) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "%s in debug_connection \"%V\"",
                               u.err, &u.host);
        }

        return NGX_CONF_ERROR;
    }

    cidr = ngx_array_push_n(&ecf->debug_connection, u.naddrs);
    if (cidr == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memzero(cidr, u.naddrs * sizeof(ngx_cidr_t));

    for (i = 0; i < u.naddrs; i++) {
        cidr[i].family = u.addrs[i].sockaddr->sa_family;

        switch (cidr[i].family) {

#if (NGX_HAVE_INET6)
        case AF_INET6:
            sin6 = (struct sockaddr_in6 *) u.addrs[i].sockaddr;
            cidr[i].u.in6.addr = sin6->sin6_addr;
            ngx_memset(cidr[i].u.in6.mask.s6_addr, 0xff, 16);
            break;
#endif

        default: /* AF_INET */
            sin = (struct sockaddr_in *) u.addrs[i].sockaddr;
            cidr[i].u.in.addr = sin->sin_addr.s_addr;
            cidr[i].u.in.mask = 0xffffffff;
            break;
        }
    }

#else

    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "\"debug_connection\" is ignored, you need to rebuild "
                       "nginx using --with-debug option to enable it");

#endif

    return NGX_CONF_OK;
}


static void *
ngx_event_core_create_conf(ngx_cycle_t *cycle)
{
    ngx_event_conf_t  *ecf;

    ecf = ngx_palloc(cycle->pool, sizeof(ngx_event_conf_t));
    if (ecf == NULL) {
        return NULL;
    }

    ecf->connections = NGX_CONF_UNSET_UINT;
    ecf->use = NGX_CONF_UNSET_UINT;
    ecf->multi_accept = NGX_CONF_UNSET;
    ecf->accept_mutex = NGX_CONF_UNSET;
    ecf->accept_mutex_delay = NGX_CONF_UNSET_MSEC;
    ecf->name = (void *) NGX_CONF_UNSET;

#if (NGX_DEBUG)

    if (ngx_array_init(&ecf->debug_connection, cycle->pool, 4,
                       sizeof(ngx_cidr_t)) == NGX_ERROR)
    {
        return NULL;
    }

#endif

    return ecf;
}


static char *
ngx_event_core_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_event_conf_t  *ecf = conf;

#if (NGX_HAVE_EPOLL) && !(NGX_TEST_BUILD_EPOLL)
    int                  fd;
#endif
    ngx_int_t            i;
    ngx_module_t        *module;
    ngx_event_module_t  *event_module;

    module = NULL;

#if (NGX_HAVE_EPOLL) && !(NGX_TEST_BUILD_EPOLL)

    fd = epoll_create(100);

    if (fd != -1) {
        (void) close(fd);
        module = &ngx_epoll_module;

    } else if (ngx_errno != NGX_ENOSYS) {
        module = &ngx_epoll_module;
    }

#endif

#if (NGX_HAVE_DEVPOLL)

    module = &ngx_devpoll_module;

#endif

#if (NGX_HAVE_KQUEUE)

    module = &ngx_kqueue_module;

#endif

#if (NGX_HAVE_SELECT)

    if (module == NULL) {
        module = &ngx_select_module;
    }

#endif

    //ngx_event_core_module后的第一个NGX_EVENT_MODULE也就是ngx_epoll_module默认作为第一个event模块
    if (module == NULL) {
        for (i = 0; ngx_modules[i]; i++) {

            if (ngx_modules[i]->type != NGX_EVENT_MODULE) {
                continue;
            }

            event_module = ngx_modules[i]->ctx;

            if (ngx_strcmp(event_module->name->data, event_core_name.data) == 0) //不能为ngx_event_core_module
            {
                continue;
            }

            module = ngx_modules[i];
            break;
        }
    }

    if (module == NULL) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0, "no events module found");
        return NGX_CONF_ERROR;
    }

    ngx_conf_init_uint_value(ecf->connections, DEFAULT_CONNECTIONS);
    cycle->connection_n = ecf->connections;

    ngx_conf_init_uint_value(ecf->use, module->ctx_index);

    event_module = module->ctx;
    ngx_conf_init_ptr_value(ecf->name, event_module->name->data);

    ngx_conf_init_value(ecf->multi_accept, 0);
    ngx_conf_init_value(ecf->accept_mutex, 1);
    ngx_conf_init_msec_value(ecf->accept_mutex_delay, 500);

    return NGX_CONF_OK;
}
