
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_H_INCLUDED_
#define _NGX_EVENT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#define NGX_INVALID_INDEX  0xd0d0d0d0


#if (NGX_HAVE_IOCP)

typedef struct {
    WSAOVERLAPPED    ovlp;
    ngx_event_t     *event;
    int              error;
} ngx_event_ovlp_t;

#endif

//cycle->read_events和cycle->write_events这两个数组存放的是ngx_event_s,他们是对应的，见ngx_event_process_init  ngx_event_t事件和ngx_connection_t连接是一一对应的
//ngx_event_t事件和ngx_connection_t连接是处理TCP连接的基础数据结构, 在Nginx中，每一个事件都由ngx_event_t结构体来表示

/*
1.ngx_event_s可以是普通的epoll读写事件(参考ngx_event_connect_peer->ngx_add_conn或者ngx_add_event)，通过读写事件触发

2.也可以是普通定时器事件(参考ngx_cache_manager_process_handler->ngx_add_timer(ngx_event_add_timer))，通过ngx_process_events_and_timers中的
epoll_wait返回，可以是读写事件触发返回，也可能是因为没获取到共享锁，从而等待0.5s返回重新获取锁来跟新事件并执行超时事件来跟新事件并且判断定
时器链表中的超时事件，超时则执行从而指向event的handler，然后进一步指向对应r或者u的->write_event_handler  read_event_handler

3.也可以是利用定时器expirt实现的读写事件(参考ngx_http_set_write_handler->ngx_add_timer(ngx_event_add_timer)),触发过程见2，只是在handler中不会执行write_event_handler  read_event_handler
*/

/*一个ngx_connection_s对应一个ngx_event_s read和一个ngx_event_s write,其中事件的fd是从ngx_connection_s->fd获取，他们
在ngx_worker_process_init->ngx_event_process_init中关联起来 */
struct ngx_event_s {
    /*
    事件相关的对象。通常data都是指向ngx_connection_t连接对象,见ngx_get_connection。开启文件异步I/O时，它可能会指向ngx_event_aio_t(ngx_file_aio_init)结构体
     */
    void            *data;  //赋值见ngx_get_connection

    //标志位，为1时表示事件是可写的。通常情况下，它表示对应的TCP连接目前状态是可写的，也就是连接处于可以发送网络包的状态
    unsigned         write:1; //见ngx_get_connection，可写事件ev默认为1  读ev事件应该默认还是0

    //标志位，为1时表示为此事件可以建立新的连接。通常情况下，在ngx_cycle_t中的listening动态数组中，每一个监听对象ngx_listening_t对
    //应的读事件中的accept标志位才会是l  ngx_event_process_init中置1
    unsigned         accept:1;

    /*
    这个标志位用于区分当前事件是否是过期的，它仅仅是给事件驱动模块使用的，而事件消费模块可不用关心。为什么需要这个标志位呢？
    当开始处理一批事件时，处理前面的事件可能会关闭一些连接，而这些连接有可能影响这批事件中还未处理到的后面的事件。这时，
    可通过instance标志位来避免处理后面的已经过期的事件。将详细描述ngx_epoll_module是如何使用instance标志位区分
    过期事件的，这是一个巧妙的设计方法

        instance标志位为什么可以判断事件是否过期？instance标志位的使用其实很简单，它利用了指针的最后一位一定
    是0这一特性。既然最后一位始终都是0，那么不如用来表示instance。这样，在使用ngx_epoll_add_event方法向epoll中添加事件时，就把epoll_event中
    联合成员data的ptr成员指向ngx_connection_t连接的地址，同时把最后一位置为这个事件的instance标志。而在ngx_epoll_process_events方法中取出指向连接的
    ptr地址时，先把最后一位instance取出来，再把ptr还原成正常的地址赋给ngx_connection_t连接。这样，instance究竟放在何处的问题也就解决了。
    那么，过期事件又是怎么回事呢？举个例子，假设epoll_wait -次返回3个事件，在第
        1个事件的处理过程中，由于业务的需要，所以关闭了一个连接，而这个连接恰好对应第3个事件。这样的话，在处理到第3个事件时，这个事件就
    已经是过期辜件了，一旦处理必然出错。既然如此，把关闭的这个连接的fd套接字置为一1能解决问题吗？答案是不能处理所有情况。
        下面先来看看这种貌似不可能发生的场景到底是怎么发生的：假设第3个事件对应的ngx_connection_t连接中的fd套接字原先是50，处理第1个事件
    时把这个连接的套接字关闭了，同时置为一1，并且调用ngx_free_connection将该连接归还给连接池。在ngx_epoll_process_events方法的循环中开始处
    理第2个事件，恰好第2个事件是建立新连接事件，调用ngx_get_connection从连接池中取出的连接非常可能就是刚刚释放的第3个事件对应的连接。由于套
    接字50刚刚被释放，Linux内核非常有可能把刚刚释放的套接字50又分配给新建立的连接。因此，在循环中处理第3个事件时，这个事件就是过期的了！它对应
    的事件是关闭的连接，而不是新建立的连接。
        如何解决这个问题？依靠instance标志位。当调用ngx_get_connection从连接池中获取一个新连接时，instance标志位就会置反
     */
    /* used to detect the stale events in kqueue and epoll */
    unsigned         instance:1; //ngx_get_connection从连接池中获取一个新连接时，instance标志位就会置反  //见ngx_get_connection

    /*
     * the event was passed or would be passed to a kernel;
     * in aio mode - operation was posted.
     */
    /*
    标志位，为1时表示当前事件是活跃的，为0时表示事件是不活跃的。这个状态对应着事件驱动模块处理方式的不同。例如，在添加事件、
    删除事件和处理事件时，active标志位的不同都会对应着不同的处理方式。在使用事件时，一般不会直接改变active标志位
     */  //ngx_epoll_add_event中也会置1  在调用该函数后，该值一直为1，除非调用ngx_epoll_del_event
    unsigned         active:1; //标记是否已经添加到事件驱动中，避免重复添加  在server端accept成功后，
    //或者在client端connect的时候把active置1，见ngx_epoll_add_connection。第一次添加epoll_ctl为EPOLL_CTL_ADD,如果再次添加发
    //现active为1,则epoll_ctl为EPOLL_CTL_MOD

    /*
    标志位，为1时表示禁用事件，仅在kqueue或者rtsig事件驱动模块中有效，而对于epoll事件驱动模块则无意义，这里不再详述
     */
    unsigned         disabled:1;

    /* the ready event; in aio mode 0 means that no operation can be posted */
    /*
    标志位，为1时表示当前事件已经淮备就绪，也就是说，允许这个事件的消费模块处理这个事件。在
    HTTP框架中，经常会检查事件的ready标志位以确定是否可以接收请求或者发送响应
    ready标志位，如果为1，则表示在与客户端的TCP连接上可以发送数据；如果为0，则表示暂不可发送数据。
     */ //如果来自对端的数据内核缓冲区没有数据(返回NGX_EAGAIN)，或者连接断开置0，见ngx_unix_recv
     //在发送数据的时候，ngx_unix_send中的时候，如果希望发送1000字节，但是实际上send只返回了500字节(说明内核协议栈缓冲区满，需要通过epoll再次促发write的时候才能写)，或者链接异常，则把ready置0
    unsigned         ready:1; //在ngx_epoll_process_events中置1,读事件触发并读取数据后ngx_unix_recv中置0

    /*
    该标志位仅对kqueue，eventport等模块有意义，而对于Linux上的epoll事件驱动模块则是无意叉的，限于篇幅，不再详细说明
     */
    unsigned         oneshot:1;

    /* aio operation is complete */
    //aio on | thread_pool方式下，如果读取数据完成，则在ngx_epoll_eventfd_handler(aio on)或者ngx_thread_pool_handler(aio thread_pool)中置1
    unsigned         complete:1; //表示读取数据完成，通过epoll机制返回获取 ，见ngx_epoll_eventfd_handler

    //标志位，为1时表示当前处理的字符流已经结束  例如内核缓冲区没有数据，你去读，则会返回0
    unsigned         eof:1; //见ngx_unix_recv
    //标志位，为1时表示事件在处理过程中出现错误
    unsigned         error:1;

    //标志位，为I时表示这个事件已经超时，用以提示事件的消费模块做超时处理
    /*读客户端连接的数据，在ngx_http_init_connection(ngx_connection_t *c)中的ngx_add_timer(rev, c->listening->post_accept_timeout)把读事件添加到定时器中，如果超时则置1
      每次ngx_unix_recv把内核数据读取完毕后，在重新启动add epoll，等待新的数据到来，同时会启动定时器ngx_add_timer(rev, c->listening->post_accept_timeout);
      如果在post_accept_timeout这么长事件内没有数据到来则超时，开始处理关闭TCP流程*/

    /*
    读超时是指的读取对端数据的超时时间，写超时指的是当数据包很大的时候，write返回NGX_AGAIN，则会添加write定时器，从而判断是否超时，如果发往
    对端数据长度小，则一般write直接返回成功，则不会添加write超时定时器，也就不会有write超时，写定时器参考函数ngx_http_upstream_send_request
     */
    unsigned         timedout:1; //定时器超时标记，见ngx_event_expire_timers
    //标志位，为1时表示这个事件存在于定时器中
    unsigned         timer_set:1; //ngx_event_add_timer ngx_add_timer 中置1   ngx_event_expire_timers置0

    //标志位，delayed为1时表示需要延迟处理这个事件，它仅用于限速功能 
    unsigned         delayed:1; //限速见ngx_http_write_filter  

    /*
     标志位，为1时表示延迟建立TCP连接，也就是说，经过TCP三次握手后并不建立连接，而是要等到真正收到数据包后才会建立TCP连接
     */
    unsigned         deferred_accept:1; //通过listen的时候添加 deferred 参数来确定

    /* the pending eof reported by kqueue, epoll or in aio chain operation */
    //标志位，为1时表示等待字符流结束，它只与kqueue和aio事件驱动机制有关
    //一般在触发EPOLLRDHUP(当对端已经关闭，本端写数据，会引起该事件)的时候，会置1，见ngx_epoll_process_events
    unsigned         pending_eof:1; 

    /*
    if (c->read->posted) { //删除post队列的时候需要检查
        ngx_delete_posted_event(c->read);
    }
     */
    unsigned         posted:1; //表示延迟处理该事件，见ngx_epoll_process_events -> ngx_post_event  标记是否在延迟队列里面
    //标志位，为1时表示当前事件已经关闭，epoll模块没有使用它
    unsigned         closed:1; //ngx_close_connection中置1

    /* to test on worker exit */
    //这两个该标志位目前无实际意义
    unsigned         channel:1;
    unsigned         resolver:1;

    unsigned         cancelable:1;

#if (NGX_WIN32)
    /* setsockopt(SO_UPDATE_ACCEPT_CONTEXT) was successful */
    unsigned         accept_context_updated:1;
#endif

#if (NGX_HAVE_KQUEUE)
    unsigned         kq_vnode:1;

    /* the pending errno reported by kqueue */
    int              kq_errno;
#endif

    /*
     * kqueue only:
     *   accept:     number of sockets that wait to be accepted
     *   read:       bytes to read when event is ready
     *               or lowat when event is set with NGX_LOWAT_EVENT flag
     *   write:      available space in buffer when event is ready
     *               or lowat when event is set with NGX_LOWAT_EVENT flag
     *
     * iocp: TODO
     *
     * otherwise:
     *   accept:     1 if accept many, 0 otherwise
     */

//标志住，在epoll事件驱动机制下表示一次尽可能多地建立TCP连接，它与multi_accept配置项对应，实现原理基见9.8.1节
#if (NGX_HAVE_KQUEUE) || (NGX_HAVE_IOCP)
    int              available;
#else
    unsigned         available:1; //ngx_event_accept中  ev->available = ecf->multi_accept;  
#endif
    /*
    每一个事件最核心的部分是handler回调方法，它将由每一个事件消费模块实现，以此决定这个事件究竟如何“消费”
     */

    /*
    1.event可以是普通的epoll读写事件(参考ngx_event_connect_peer->ngx_add_conn或者ngx_add_event)，通过读写事件触发
    
    2.也可以是普通定时器事件(参考ngx_cache_manager_process_handler->ngx_add_timer(ngx_event_add_timer))，通过ngx_process_events_and_timers中的
    epoll_wait返回，可以是读写事件触发返回，也可能是因为没获取到共享锁，从而等待0.5s返回重新获取锁来跟新事件并执行超时事件来跟新事件并且判断定
    时器链表中的超时事件，超时则执行从而指向event的handler，然后进一步指向对应r或者u的->write_event_handler  read_event_handler
    
    3.也可以是利用定时器expirt实现的读写事件(参考ngx_http_set_write_handler->ngx_add_timer(ngx_event_add_timer)),触发过程见2，只是在handler中不会执行write_event_handler  read_event_handler
    */
     
    //这个事件发生时的处理方法，每个事件消费模块都会重新实现它
    //ngx_epoll_process_events中执行accept
    /*
     赋值为ngx_http_process_request_line     ngx_event_process_init中初始化为ngx_event_accept  如果是文件异步i/o，赋值为ngx_epoll_eventfd_handler
     //当accept客户端连接后ngx_http_init_connection中赋值为ngx_http_wait_request_handler来读取客户端数据  
     在解析完客户端发送来的请求的请求行和头部行后，设置handler为ngx_http_request_handler
     */ //一般与客户端的数据读写是 ngx_http_request_handler;  与后端服务器读写为ngx_http_upstream_handler(如fastcgi proxy memcache gwgi等)
    
    /* ngx_event_accept，ngx_http_ssl_handshake ngx_ssl_handshake_handler ngx_http_v2_write_handler ngx_http_v2_read_handler 
    ngx_http_wait_request_handler  ngx_http_request_handler,ngx_http_upstream_handler ngx_file_aio_event_handler */
    ngx_event_handler_pt  handler; //由epoll读写事件在ngx_epoll_process_events触发
   

#if (NGX_HAVE_IOCP)
    ngx_event_ovlp_t ovlp;
#endif
    //由于epoll事件驱动方式不使用index，所以这里不再说明
    ngx_uint_t       index;
    //可用于记录error_log日志的ngx_log_t对象
    ngx_log_t       *log;  //可以记录日志的ngx_log_t对象 其实就是ngx_listening_t中获取的log //赋值见ngx_event_accept
    //定时器节点，用于定时器红黑树中
    ngx_rbtree_node_t   timer; //见ngx_event_timer_rbtree

    /* the posted queue */
    /*
    post事件将会构成一个队列再统一处理，这个队列以next和prev作为链表指针，以此构成一个简易的双向链表，其中next指向后一个事件的地址，
    prev指向前一个事件的地址
     */
    ngx_queue_t      queue;

#if 0

    /* the threads support */

    /*
     * the event thread context, we store it here
     * if $(CC) does not understand __thread declaration
     * and pthread_getspecific() is too costly
     */

    void            *thr_ctx;

#if (NGX_EVENT_T_PADDING)

    /* event should not cross cache line in SMP */

    uint32_t         padding[NGX_EVENT_T_PADDING];
#endif
#endif
};


#if (NGX_HAVE_FILE_AIO)

struct ngx_event_aio_s { //ngx_file_aio_init中初始化,创建空间和赋值
    void                      *data; //指向ngx_http_request_t  赋值见ngx_http_copy_aio_handler

    //执行地方在ngx_file_aio_event_handler，赋值地方在ngx_http_copy_aio_handler
    ngx_event_handler_pt       handler;//这是真正由业务模块实现的方法，在异步I/O事件完成后被调用
    ngx_file_t                *file;//file为要读取的file文件信息

#if (NGX_HAVE_AIO_SENDFILE)
    ssize_t                  (*preload_handler)(ngx_buf_t *file);
#endif

    ngx_fd_t                   fd;//file为要读取的file文件信息对应的文件描述符

#if (NGX_HAVE_EVENTFD)
    int64_t                    res; //赋值见ngx_epoll_eventfd_handler
#endif

#if !(NGX_HAVE_EVENTFD) || (NGX_TEST_BUILD_EPOLL)
    ngx_err_t                  err;
    size_t                     nbytes;
#endif

    ngx_aiocb_t                aiocb;
    //如果是文件异步i/o中的ngx_event_aio_t，则它来自ngx_event_aio_t->ngx_event_t(只有读),如果是网络事件中的event,则为ngx_connection_s中的event(包括读和写)
    ngx_event_t                event; //只是异步i/o读事件
};

#endif

//ngx_event_module_t中的actions成员是定义事件驱动模块的核心方法，下面重点看一下actions中的这10个抽象方法
typedef struct {
    /* 
    添加事件方法，它将负责把1个感兴趣的事件添加到操作系统提供的事件驱动机制（如epoll、
    kqueue等）中，这样，在事件发生后，将可以在调用下面的process_events时获取这个事件 
    */
    ngx_int_t  (*add)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags); //ngx_add_event中执行

    /*
    删除事件方法，它将把1个已经存在于事件驱动机制中的事件移除，这样以后即使这个事件发生，调用
   process_events方法时也无法再获取这个事件
    */
    ngx_int_t  (*del)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags); //ngx_del_event中执行

    /*
     启用1个事件，目前事件框架不会调用这个方法，大部分事件驱动模块对于该方法的实现都是与上面的add方法完全一致的
     */
    ngx_int_t  (*enable)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);

    //禁用1个事件，目前事件框架不会调用这个方法，大部分事件驱动模块对于该方法的实现都是与上面的del方法完全一致的
    ngx_int_t  (*disable)(ngx_event_t *ev, ngx_int_t event, ngx_uint_t flags);

    //向事件驱动机制中添加一个新的连接，这意味着连接上的读写事件都添加到事件驱动机制中了
    ngx_int_t  (*add_conn)(ngx_connection_t *c); //ngx_add_conn中执行
    //从事件驱动机制中移除一个连接的读写事件
    ngx_int_t  (*del_conn)(ngx_connection_t *c, ngx_uint_t flags); //ngx_del_conn中执行

    ngx_int_t  (*notify)(ngx_event_handler_pt handler); //ngx_notify中执行

    //在正常的工作循环中，将通过调用process_event方法来处理事件。
    ngx_int_t  (*process_events)(ngx_cycle_t *cycle, ngx_msec_t timer,
                   ngx_uint_t flags); //调用见ngx_process_events

    //初始化事件驱动模块的方法
    ngx_int_t  (*init)(ngx_cycle_t *cycle, ngx_msec_t timer); //ngx_event_process_init中执行
    //退出事件驱动模块前调用的方法
    void       (*done)(ngx_cycle_t *cycle);  //ngx_done_events中执行
} ngx_event_actions_t;


extern ngx_event_actions_t   ngx_event_actions;


/*
 * The event filter requires to read/write the whole data:
 * select, poll, /dev/poll, kqueue, epoll.
 */ //#define NGX_LEVEL_EVENT    0  见
#define NGX_USE_LEVEL_EVENT      0x00000001 //epoll的LT模式   见ngx_handle_read_event  nginx默认使用NGX_USE_CLEAR_EVENT边沿触发方式

/*
 * The event filter is deleted after a notification without an additional
 * syscall: kqueue, epoll.
 */
#define NGX_USE_ONESHOT_EVENT    0x00000002

/*
 * The event filter notifies only the changes and an initial level:
 * kqueue, epoll.
 */ //#define NGX_CLEAR_EVENT    EPOLLET  见ngx_handle_read_event
 //默认使用这个,见ngx_epoll_init 
#define NGX_USE_CLEAR_EVENT      0x00000004 ////默认是采用LT模式来使用epoll的，NGX USE CLEAR EVENT宏实际上就是在告诉Nginx使用ET模式

/*
 * The event filter has kqueue features: the eof flag, errno,
 * available data, etc.
 */
#define NGX_USE_KQUEUE_EVENT     0x00000008

/*
 * The event filter supports low water mark: kqueue's NOTE_LOWAT.
 * kqueue in FreeBSD 4.1-4.2 has no NOTE_LOWAT so we need a separate flag.
 */
#define NGX_USE_LOWAT_EVENT      0x00000010

/*
 * The event filter requires to do i/o operation until EAGAIN: epoll.
 */
#define NGX_USE_GREEDY_EVENT     0x00000020  //epoll有添加该标记

/*
 * The event filter is epoll.
 */
#define NGX_USE_EPOLL_EVENT      0x00000040

/*
 * Obsolete.
 */
#define NGX_USE_RTSIG_EVENT      0x00000080

/*
 * Obsolete.
 */
#define NGX_USE_AIO_EVENT        0x00000100

/*
 * Need to add socket or handle only once: i/o completion port.
 */
#define NGX_USE_IOCP_EVENT       0x00000200

/*
 * The event filter has no opaque data and requires file descriptors table:
 * poll, /dev/poll.
 */
#define NGX_USE_FD_EVENT         0x00000400

/*
 * The event module handles periodic or absolute timer event by itself:
 * kqueue in FreeBSD 4.4, NetBSD 2.0, and MacOSX 10.4, Solaris 10's event ports.
 */
#define NGX_USE_TIMER_EVENT      0x00000800

/*
 * All event filters on file descriptor are deleted after a notification:
 * Solaris 10's event ports.
 */
#define NGX_USE_EVENTPORT_EVENT  0x00001000

/*
 * The event filter support vnode notifications: kqueue.
 */
#define NGX_USE_VNODE_EVENT      0x00002000


/*
 * The event filter is deleted just before the closing file.
 * Has no meaning for select and poll.
 * kqueue, epoll, eventport:         allows to avoid explicit delete,
 *                                   because filter automatically is deleted
 *                                   on file close,
 *
 * /dev/poll:                        we need to flush POLLREMOVE event
 *                                   before closing file.
 */
#define NGX_CLOSE_EVENT    1

/*
 * disable temporarily event filter, this may avoid locks
 * in kernel malloc()/free(): kqueue.
 */
#define NGX_DISABLE_EVENT  2

/*
 * event must be passed to kernel right now, do not wait until batch processing.
 */
#define NGX_FLUSH_EVENT    4


/* these flags have a meaning only for kqueue */
#define NGX_LOWAT_EVENT    0
#define NGX_VNODE_EVENT    0


#if (NGX_HAVE_EPOLL) && !(NGX_HAVE_EPOLLRDHUP)
#define EPOLLRDHUP         0
#endif


#if (NGX_HAVE_KQUEUE)

#define NGX_READ_EVENT     EVFILT_READ
#define NGX_WRITE_EVENT    EVFILT_WRITE

#undef  NGX_VNODE_EVENT
#define NGX_VNODE_EVENT    EVFILT_VNODE

/*
 * NGX_CLOSE_EVENT, NGX_LOWAT_EVENT, and NGX_FLUSH_EVENT are the module flags
 * and they must not go into a kernel so we need to choose the value
 * that must not interfere with any existent and future kqueue flags.
 * kqueue has such values - EV_FLAG1, EV_EOF, and EV_ERROR:
 * they are reserved and cleared on a kernel entrance.
 */
#undef  NGX_CLOSE_EVENT
#define NGX_CLOSE_EVENT    EV_EOF

#undef  NGX_LOWAT_EVENT
#define NGX_LOWAT_EVENT    EV_FLAG1

#undef  NGX_FLUSH_EVENT
#define NGX_FLUSH_EVENT    EV_ERROR

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  EV_ONESHOT
#define NGX_CLEAR_EVENT    EV_CLEAR

#undef  NGX_DISABLE_EVENT
#define NGX_DISABLE_EVENT  EV_DISABLE


#elif (NGX_HAVE_DEVPOLL || NGX_HAVE_EVENTPORT)

#define NGX_READ_EVENT     POLLIN
#define NGX_WRITE_EVENT    POLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1


#elif (NGX_HAVE_EPOLL)

#define NGX_READ_EVENT     (EPOLLIN|EPOLLRDHUP)
#define NGX_WRITE_EVENT    EPOLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_CLEAR_EVENT    EPOLLET
#define NGX_ONESHOT_EVENT  0x70000000
#if 0
#define NGX_ONESHOT_EVENT  EPOLLONESHOT
#endif


#elif (NGX_HAVE_POLL)

#define NGX_READ_EVENT     POLLIN
#define NGX_WRITE_EVENT    POLLOUT

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1


#else /* select */

#define NGX_READ_EVENT     0
#define NGX_WRITE_EVENT    1

#define NGX_LEVEL_EVENT    0
#define NGX_ONESHOT_EVENT  1

#endif /* NGX_HAVE_KQUEUE */


#if (NGX_HAVE_IOCP)
#define NGX_IOCP_ACCEPT      0
#define NGX_IOCP_IO          1
#define NGX_IOCP_CONNECT     2
#endif


#ifndef NGX_CLEAR_EVENT
#define NGX_CLEAR_EVENT    0    /* dummy declaration */
#endif

#define NGX_FUNC_LINE __FUNCTION__, __LINE__

/*
ngx_event_actions = ngx_epoll_module_ctx.actions; ngx_event_actions为具体的模块action，如ngx_event_actions = ngx_poll_module_ctx.actions;
ngx_event_actions = ngx_select_module_ctx.actions;
*/
#define ngx_process_events   ngx_event_actions.process_events //ngx_process_events_and_timers中执行
#define ngx_done_events      ngx_event_actions.done

#define ngx_add_event        ngx_event_actions.add // ngx_epoll_add_event会调用该函数
#define ngx_del_event        ngx_event_actions.del

#define ngx_add_conn         ngx_event_actions.add_conn //connect和accept返回的时候用到  已经channel读的时候用
#define ngx_del_conn         ngx_event_actions.del_conn

//ngx_notify为事件模块的通知函数，主要是使用该通知函数触发某任务已经执行完成；
//ngx_notify通告主线程，该任务处理完毕，ngx_thread_pool_handler由主线程执行，也就是进程cycle{}通过epoll_wait返回执行，而不是由线程池中的线程执行
#define ngx_notify           ngx_event_actions.notify

#define ngx_add_timer        ngx_event_add_timer //在ngx_process_events_and_timers中，当有事件使epoll_wait返回，则会执行超时的定时器
#define ngx_del_timer        ngx_event_del_timer


extern ngx_os_io_t  ngx_io;

#define ngx_recv             ngx_io.recv
#define ngx_recv_chain       ngx_io.recv_chain
#define ngx_udp_recv         ngx_io.udp_recv
#define ngx_send             ngx_io.send
#define ngx_send_chain       ngx_io.send_chain //epoll方式ngx_io = ngx_linux_io;


//所有的核心模块NGX_CORE_MODULE对应的上下文ctx为ngx_core_module_t，子模块，例如http{} NGX_HTTP_MODULE模块对应的为上下文为ngx_http_module_t
//events{} NGX_EVENT_MODULE模块对应的为上下文为ngx_event_module_t
#define NGX_EVENT_MODULE      0x544E5645  /* "EVNT" */
/*
NGX_MAIN_CONF：配置项可以出现在全局配置中，即不属于任何{}配置块。
NGX_EVET_CONF：配置项可以出现在events{}块内。
NGX_HTTP_MAIN_CONF： 配置项可以出现在http{}块内。
NGX_HTTP_SRV_CONF:：配置项可以出现在server{}块内，该server块必需属于http{}块。
NGX_HTTP_LOC_CONF：配置项可以出现在location{}块内，该location块必需属于server{}块。
NGX_HTTP_UPS_CONF： 配置项可以出现在upstream{}块内，该location块必需属于http{}块。
NGX_HTTP_SIF_CONF：配置项可以出现在server{}块内的if{}块中。该if块必须属于http{}块。
NGX_HTTP_LIF_CONF: 配置项可以出现在location{}块内的if{}块中。该if块必须属于http{}块。
NGX_HTTP_LMT_CONF: 配置项可以出现在limit_except{}块内,该limit_except块必须属于http{}块。
*/
#define NGX_EVENT_CONF        0x02000000


//用于存储从ngx_event_core_commands配置命令中解析出的各种参数
typedef struct {
    ngx_uint_t    connections; //连接池的大小
    //通过"use"选择IO复用方式 select epoll等，然后通过解析赋值 见ngx_event_use
    //默认赋值见ngx_event_core_init_conf，ngx_event_core_module后的第一个NGX_EVENT_MODULE也就是ngx_epoll_module默认作为第一个event模块
    ngx_uint_t    use; //选用的事件模块在所有事件模块中的序号，也就是ctx_index成员 赋值见ngx_event_use

    /*
    事件的available标志位对应着multi_accept配置项。当available为l时，告诉Nginx -次性尽量多地建立新连接，它的实现原理也就在这里
     */ //默认0
    ngx_flag_t    multi_accept; //标志位，如果为1，则表示在接收到一个新连接事件时，一次性建立尽可能多的连接

    /*
     如果ccf->worker_processes > 1 && ecf->accept_mutex，则在创建进程后，调用ngx_event_process_init把accept添加到epoll事件驱动中，
     否则在ngx_process_events_and_timers->ngx_trylock_accept_mutex中把accept添加到epoll事件驱动中
     */
    ngx_flag_t    accept_mutex;//标志位，为1时表示启用负载均衡锁

    /*
    负载均衡锁会使有些worker进程在拿不到锁时延迟建立新连接，accept_mutex_delay就是这段延迟时间的长度。关于它如何影响负载均衡的内容
     */ //默认500ms，也就是0.5s
    ngx_msec_t    accept_mutex_delay; //单位ms  如果没获取到mutex锁，则延迟这么多毫秒重新获取

    u_char       *name;//所选用事件模块的名字，它与use成员是匹配的  epoll select

/*
在-with-debug编译模式下，可以仅针对某些客户端建立的连接输出调试级别的日志，而debug_connection数组用于保存这些客户端的地址信息
*/
#if (NGX_DEBUG)
    ngx_array_t   debug_connection;
#endif
} ngx_event_conf_t;

//所有的核心模块NGX_CORE_MODULE对应的上下文ctx为ngx_core_module_t，子模块，例如http{} NGX_HTTP_MODULE模块对应的为上下文为ngx_http_module_t
//events{} NGX_EVENT_MODULE模块对应的为上下文为ngx_event_module_t

//这里面的函数在解析到event{}后在ngx_events_block中执行,所有的NGX_EVENT_MODULE模块都在ngx_events_block中执行
typedef struct {//ngx_epoll_module_ctx  ngx_select_module_ctx  
    ngx_str_t              *name; //事件模块的名称

    //在解析配置项前，这个回调方法用于创建存储配置项参数的结构体
    void                 *(*create_conf)(ngx_cycle_t *cycle);

    //在解析配置项完成后，init一conf方法会被调用，用以综合处理当前事件模块感兴趣的全部配置项
    char                 *(*init_conf)(ngx_cycle_t *cycle, void *conf);

    //对于事件驱动机制，每个亨件模块需要实现的10个抽象方法
    ngx_event_actions_t     actions;
} ngx_event_module_t;


extern ngx_atomic_t          *ngx_connection_counter;

extern ngx_atomic_t          *ngx_accept_mutex_ptr;
extern ngx_shmtx_t            ngx_accept_mutex;
extern ngx_uint_t             ngx_use_accept_mutex;
extern ngx_uint_t             ngx_accept_events;
extern ngx_uint_t             ngx_accept_mutex_held;
extern ngx_msec_t             ngx_accept_mutex_delay;
extern ngx_int_t              ngx_accept_disabled;


#if (NGX_STAT_STUB)

extern ngx_atomic_t  *ngx_stat_accepted;
extern ngx_atomic_t  *ngx_stat_handled;
extern ngx_atomic_t  *ngx_stat_requests;
extern ngx_atomic_t  *ngx_stat_active;
extern ngx_atomic_t  *ngx_stat_reading;
extern ngx_atomic_t  *ngx_stat_writing;
extern ngx_atomic_t  *ngx_stat_waiting;

#endif


#define NGX_UPDATE_TIME         1

/*
拿到锁的话，置flag为NGX_POST_EVENTS，这意味着ngx_process_events函数中，任何事件都将延后处理，会把accept事件都放到
ngx_posted_accept_events链表中，epollin|epollout事件都放到ngx_posted_events链表中 
*/ //见ngx_process_events_and_timers会置位该位  如果flag置为该位，则ngx_epoll_process_events会延后处理epoll事件ngx_post_event
#define NGX_POST_EVENTS         2 //NGX_POST_EVENTS，这意味着ngx_process_events函数中，任何事件都将延后处理


extern sig_atomic_t           ngx_event_timer_alarm;
extern ngx_uint_t             ngx_event_flags;
extern ngx_module_t           ngx_events_module;
extern ngx_module_t           ngx_event_core_module;


#define ngx_event_get_conf(conf_ctx, module)                                  \
             (*(ngx_get_conf(conf_ctx, ngx_events_module))) [module.ctx_index];



void ngx_event_accept(ngx_event_t *ev);
ngx_int_t ngx_trylock_accept_mutex(ngx_cycle_t *cycle);
u_char *ngx_accept_log_error(ngx_log_t *log, u_char *buf, size_t len);


void ngx_process_events_and_timers(ngx_cycle_t *cycle);
//ngx_int_t ngx_handle_read_event(ngx_event_t *rev, ngx_uint_t flags);
ngx_int_t ngx_handle_read_event(ngx_event_t *rev, ngx_uint_t flags, const char* func, int line);

ngx_int_t ngx_handle_write_event(ngx_event_t *wev, size_t lowat, const char* func, int line);


#if (NGX_WIN32)
void ngx_event_acceptex(ngx_event_t *ev);
ngx_int_t ngx_event_post_acceptex(ngx_listening_t *ls, ngx_uint_t n);
u_char *ngx_acceptex_log_error(ngx_log_t *log, u_char *buf, size_t len);
#endif


ngx_int_t ngx_send_lowat(ngx_connection_t *c, size_t lowat);


/* used in ngx_log_debugX() */
#define ngx_event_ident(p)  ((ngx_connection_t *) (p))->fd


#include <ngx_event_timer.h>
#include <ngx_event_posted.h>

#if (NGX_WIN32)
#include <ngx_iocp_module.h>
#endif


#endif /* _NGX_EVENT_H_INCLUDED_ */
