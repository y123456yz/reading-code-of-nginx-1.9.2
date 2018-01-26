
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CONNECTION_H_INCLUDED_
#define _NGX_CONNECTION_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_listening_s  ngx_listening_t;

//初始化赋值等可以参考ngx_event_process_init
//ngx_listening_t结构体代表着Nginx服务器监听的一个端口
//实际上这些ngx_listening_s结构体是从 cycle->listening.elts中来的，见ngx_event_process_init
struct ngx_listening_s { //初始化及赋值见ngx_http_add_listening    热升级nginx的时候，继承源master listen fd在ngx_set_inherited_sockets
    ngx_socket_t        fd; //socket套接字句柄   //赋值见ngx_open_listening_sockets

    struct sockaddr    *sockaddr; //监听sockaddr地址
    socklen_t           socklen;    /* size of sockaddr */ //sockaddr地址长度

    //存储IP地址的字符串addr_text最大长度，即它指定了addr_text所分配的内存大小
    size_t              addr_text_max_len; //
    //如果listen 80;或者listen *:80;则该地址为0.0.0.0
    ngx_str_t           addr_text;//以字符串形式存储IP地址和端口 例如 A.B.C.D:E     3.3.3.3:23  赋值见ngx_set_inherited_sockets

    int                 type;//套接字类型。例如，当type是SOCK_STREAM时，表示TCP

    //TCP实现监听时的backlog队列，它表示允许正在通过三次握手建立TCP连接但还没有任何进程开始处理的连接最大个数，默认NGX_LISTEN_BACKLOG
    int                 backlog; //
    int                 rcvbuf;//内核中对于这个套接字的接收缓冲区大小
    int                 sndbuf;//内核中对于这个套接字的发送缓冲区大小
#if (NGX_HAVE_KEEPALIVE_TUNABLE)
    int                 keepidle;
    int                 keepintvl;
    int                 keepcnt;
#endif

    /* handler of accepted connection */
    //当新的TCP accept连接成功建立后的处理方法  ngx_connection_handler_pt粪型的handler成员表示在这个监听端口上成功建立新的TCP连接后，就会回调handler方法
    ngx_connection_handler_pt   handler; //赋值为ngx_http_init_connection，见ngx_http_add_listening。该handler在ngx_event_accept中执行
    /*
    实际上框架并不使用servers指针，它更多是作为一个保留指针，目前主要用于HTTP或者mail等模块，用于保存当前监听端口对应着的所有主机名
    */ 
    void               *servers;  /* array of ngx_http_in_addr_t  ngx_http_port_t, for example */ //赋值见ngx_http_init_listening，指向ngx_http_port_t结构

    //log和logp都是可用的日志对象的指针
    ngx_log_t           log; //见ngx_http_add_listening
    ngx_log_t          *logp;

    size_t              pool_size;//如果为新的TCP连接创建内存池，则内存池的初始大小应该是pool_size      见ngx_http_add_listening
    /* should be here because of the AcceptEx() preread */
    
    size_t              post_accept_buffer_size;
    /* should be here because of the deferred accept */
    /*
    TCP_DEFER ACCEPT选项将在建立TCP连接成功且接收到用户的请求数据后，才向对监听套接字感兴趣的进程发送事件通知，而连接建立成功后，
    如果post_accept_timeout秒后仍然没有收到的用户数据，则内核直接丢弃连接
    */ //ls->post_accept_timeout = cscf->client_header_timeout;  "client_header_timeout"设置
    ngx_msec_t          post_accept_timeout; //见ngx_http_add_listening

    /* 前一个ngx_listening_t结构，多个ngx_listening_t结构体之间由previous指针组成单链表 */
    ngx_listening_t    *previous; //这个表示在热启动nginx进程的时候，在最新启动前的上一个nginx的所有listen信息
    //当前监听句柄对应着的ngx_connection_t结构体
    ngx_connection_t   *connection; 

    ngx_uint_t          worker;

    //下面这些标志位一般在ngx_init_cycle中初始化赋值
    /*
    标志位，为1则表示在当前监听句柄有效，且执行ngx- init—cycle时不关闭监听端口，为0时则正常关闭。该标志位框架代码会自动设置
    */
    unsigned            open:1;
    /*
    标志位，为1表示使用已有的ngx_cycle_t来初始化新的ngx_cycle_t结构体时，不关闭原先打开的监听端口，这对运行中升级程序很有用，
    remaln为o时，表示正常关闭曾经打开的监听端口。该标志位框架代码会自动设置，参见ngx_init_cycle方法
    */
    unsigned            remain:1;
    /*
    标志位，为1时表示跳过设置当前ngx_listening_t结构体中的套接字，为o时正常初始化套接字。该标志位框架代码会自动设置
    */
    unsigned            ignore:1;

    //表示是否已经绑定。实际上目前该标志位没有使用
    unsigned            bound:1;       /* already bound */
    /* 表示当前监听句柄是否来自前一个进程（如升级Nginx程序），如果为1，则表示来自前一个进程。一般会保留之前已经设置好的套接字，不做改变 */
    unsigned            inherited:1;   /* inherited from previous process */ //说明是热升级过程
    unsigned            nonblocking_accept:1;  //目前未使用
    //lsopt.bind = 1;这里面存的是bind为1的配置才会有创建ngx_http_port_t
    unsigned            listen:1; //标志位，为1时表示当前结构体对应的套接字已经监听  赋值见ngx_open_listening_sockets
    unsigned            nonblocking:1;//表素套接字是否阻塞，目前该标志位没有意义
    unsigned            shared:1;    /* shared between threads or processes */ //目前该标志位没有意义
    
    //标志位，为1时表示Nginx会将网络地址转变为字符串形式的地址  见addr_text 赋值见ngx_http_add_listening,当在ngx_create_listening把listen的IP地址转换为字符串地址后置1
    unsigned            addr_ntop:1;
#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
    unsigned            ipv6only:1;
#endif
#if (NGX_HAVE_REUSEPORT)
    //赋值见ngx_http_add_listening
    //master进程执行ngx_clone_listening中如果配置了多worker，监听80端口会有worker个listen赋值，master进程在ngx_open_listening_sockets
    //中会监听80端口worker次，那么子进程创建起来后，不是每个字进程都关注这worker多个 listen事件了吗?为了避免这个问题，nginx通过
    //在子进程运行ngx_event_process_init函数的时候，通过ngx_add_event来控制子进程关注的listen，最终实现只关注master进程中创建的一个listen事件
    unsigned            reuseport:1;
    unsigned            add_reuseport:1;
#endif
    unsigned            keepalive:2;

#if (NGX_HAVE_DEFERRED_ACCEPT)
    unsigned            deferred_accept:1;//SO_ACCEPTFILTER(freebsd所用)设置  TCP_DEFER_ACCEPT(LINUX系统所用)
    unsigned            delete_deferred:1;

    unsigned            add_deferred:1; //SO_ACCEPTFILTER(freebsd所用)设置  TCP_DEFER_ACCEPT(LINUX系统所用)
#ifdef SO_ACCEPTFILTER
    char               *accept_filter;
#endif
#endif
#if (NGX_HAVE_SETFIB)
    int                 setfib;
#endif

#if (NGX_HAVE_TCP_FASTOPEN)
    int                 fastopen;
#endif

};

//本连接记录日志时的级别，它占用了3位，取值范围是0-7，但实际上目前只定义了5个值。见ngx_connection_s->log_error
typedef enum {
     NGX_ERROR_ALERT = 0,
     NGX_ERROR_ERR,
     NGX_ERROR_INFO,
     NGX_ERROR_IGNORE_ECONNRESET,
     NGX_ERROR_IGNORE_EINVAL
} ngx_connection_log_error_e;

/*
tcp_nopush
官方:

tcp_nopush
Syntax: tcp_nopush on | off
Default: off
Context: http
server
location
Reference: tcp_nopush
This directive permits or forbids the use of thesocket options TCP_NOPUSH on FreeBSD or TCP_CORK on Linux. This option is 
onlyavailable when using sendfile.
Setting this option causes nginx to attempt to sendit’s HTTP response headers in one packet on Linux and FreeBSD 4.x
You can read more about the TCP_NOPUSH and TCP_CORKsocket options here.

 
linux 下是tcp_cork,上面的意思就是说，当使用sendfile函数时，tcp_nopush才起作用，它和指令tcp_nodelay是互斥的。tcp_cork是linux下
tcp/ip传输的一个标准了，这个标准的大概的意思是，一般情况下，在tcp交互的过程中，当应用程序接收到数据包后马上传送出去，不等待，
而tcp_cork选项是数据包不会马上传送出去，等到数据包最大时，一次性的传输出去，这样有助于解决网络堵塞，已经是默认了。

也就是说tcp_nopush = on 会设置调用tcp_cork方法，这个也是默认的，结果就是数据包不会马上传送出去，等到数据包最大时，一次性的传输出去，
这样有助于解决网络堵塞。

以快递投递举例说明一下（以下是我的理解，也许是不正确的），当快递东西时，快递员收到一个包裹，马上投递，这样保证了即时性，但是会
耗费大量的人力物力，在网络上表现就是会引起网络堵塞，而当快递收到一个包裹，把包裹放到集散地，等一定数量后统一投递，这样就是tcp_cork的
选项干的事情，这样的话，会最大化的利用网络资源，虽然有一点点延迟。

对于nginx配置文件中的tcp_nopush，默认就是tcp_nopush,不需要特别指定，这个选项对于www，ftp等大文件很有帮助

tcp_nodelay
        TCP_NODELAY和TCP_CORK基本上控制了包的“Nagle化”，Nagle化在这里的含义是采用Nagle算法把较小的包组装为更大的帧。 John Nagle是Nagle算法的发明人，后者就是用他的名字来命名的，他在1984年首次用这种方法来尝试解决福特汽车公司的网络拥塞问题（欲了解详情请参看IETF RFC 896）。他解决的问题就是所谓的silly window syndrome，中文称“愚蠢窗口症候群”，具体含义是，因为普遍终端应用程序每产生一次击键操作就会发送一个包，而典型情况下一个包会拥有一个字节的数据载荷以及40个字节长的包头，于是产生4000%的过载，很轻易地就能令网络发生拥塞,。 Nagle化后来成了一种标准并且立即在因特网上得以实现。它现在已经成为缺省配置了，但在我们看来，有些场合下把这一选项关掉也是合乎需要的。

       现在让我们假设某个应用程序发出了一个请求，希望发送小块数据。我们可以选择立即发送数据或者等待产生更多的数据然后再一次发送两种策略。如果我们马上发送数据，那么交互性的以及客户/服务器型的应用程序将极大地受益。如果请求立即发出那么响应时间也会快一些。以上操作可以通过设置套接字的TCP_NODELAY = on 选项来完成，这样就禁用了Nagle 算法。

       另外一种情况则需要我们等到数据量达到最大时才通过网络一次发送全部数据，这种数据传输方式有益于大量数据的通信性能，典型的应用就是文件服务器。应用 Nagle算法在这种情况下就会产生问题。但是，如果你正在发送大量数据，你可以设置TCP_CORK选项禁用Nagle化，其方式正好同 TCP_NODELAY相反（TCP_CORK和 TCP_NODELAY是互相排斥的）。



tcp_nopush
语法：tcp_nopush on | off;
默认：tcp_nopush off;
配置块：http、server、location
在打开sendfile选项时，确定是否开启FreeBSD系统上的TCP_NOPUSH或Linux系统上的TCP_CORK功能。打开tcp_nopush后，将会在发送响应时把整个响应包头放到一个TCP包中发送。
*/

//表示如何使用TCP的nodelay特性
typedef enum {
     NGX_TCP_NODELAY_UNSET = 0,
     NGX_TCP_NODELAY_SET,
     NGX_TCP_NODELAY_DISABLED
} ngx_connection_tcp_nodelay_e;

//表示如何使用TCP的nopush特性
typedef enum {
     NGX_TCP_NOPUSH_UNSET = 0,
     NGX_TCP_NOPUSH_SET,
     NGX_TCP_NOPUSH_DISABLED
} ngx_connection_tcp_nopush_e;


#define NGX_LOWLEVEL_BUFFERED  0x0f
#define NGX_SSL_BUFFERED       0x01
#define NGX_SPDY_BUFFERED      0x02
#define NGX_HTTP_V2_BUFFERED   0x02

/*
Nginx中定义了基本的数据结构ngx_connection_t来表示连接，这个连接表示是客户端主动发起的、Nginx服务器被动接受的TCP连接，我们可以简单称
其为被动连接。同时，在有些请求的处理过程中，Nginx会试图主动向其他上游服务器建立连接，并以此连接与上游服务器通信，因此，这样的
连接与ngx_connection_t又是不同的，Nginx定义了}ngx_peer_connection_t结构体来表示主动连接，当然，ngx_peer_connection_t主动连接是
以ngx_connection-t结构体为基础实现的。本节将说明这两种连接中各字段的意义，同时需要注意的是，这两种连接都不可以随意创建，必须从
连接池中获取，
*/

/*
在使用连接池时，Nginx也封装了两个方法，见表9-1。
    如果我们开发的模块直接使用了连接池，那么就可以用这两个方法来获取、释放ngx_connection_t结构体。
表9-1  连接池的使用方法
┏━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━┓
┃    连接池操作方法名                  ┃    参数含义                ┃    执行意义                          ┃
┣━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃npc_connection_t *ngx_get_connection  ┃  s是这条连接的套接字句柄， ┃  从连接池中获取一个ngx_connection_t  ┃
┃(ngx_socket_t s, ngx_log_t *log)      ┃log则是记录日志的对象       ┃结构体，同时获取相应的读／写事件      ┃
┣━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃void ngx_free_connection              ┃  c是需要回收的连接         ┃  将这个连接回收到连接池中            ┃
┃(ngx_connection_t)                    ┃                            ┃                                      ┃
┗━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━┛
*/
/*一个ngx_connection_s对应一个ngx_event_s read和一个ngx_event_s write,其中事件的fd是从ngx_connection_s->fd获取，他们
在ngx_worker_process_init->ngx_event_process_init中关联起来 */
//ngx_event_t事件和ngx_connection_t连接是处理TCP连接的基础数据结构, 通过ngx_get_connection从连接池中获取一个ngx_connection_s结构，
//被动连接(客户端连接nginx)对应的数据结构是ngx_connection_s，主动连接(nginx连接后端服务器)对应的数据结构是ngx_peer_connection_s
struct ngx_connection_s {  //cycle->read_events和cycle->write_events这两个数组存放的是ngx_event_s,他们是对应的，见ngx_event_process_init
    /*
    连接未使用时，data成员用于充当连接池中空闲连接链表中的next指针(ngx_event_process_init)。当连接被使用时，data的意义由使用它的Nginx模块而定，
    如在HTTP框架中，data指向ngx_http_request_t请求

    在服务器端accept客户端连接成功(ngx_event_accept)后，会通过ngx_get_connection从连接池获取一个ngx_connection_t结构，也就是每个客户端连接对于一个ngx_connection_t结构，
    并且为其分配一个ngx_http_connection_t结构，ngx_connection_t->data = ngx_http_connection_t，见ngx_http_init_connection
     */ 
 
 /*
    在子请求处理过程中，上层父请求r的data指向第一个r下层的子请求，例如第二层的r->connection->data指向其第三层的第一个
 创建的子请求r，c->data = sr见ngx_http_subrequest,在subrequest往客户端发送数据的时候，只有data指向的节点可以先发送出去
    listen过程中，指向原始请求ngx_http_connection_t(ngx_http_init_connection ngx_http_ssl_handshake),接收到客户端数据后指向ngx_http_request_t(ngx_http_wait_request_handler)
    http2协议的过程中，在ngx_http_v2_connection_t(ngx_http_v2_init)
 */
    void               *data; /* 如果是subrequest，则data最终指向最下层子请求r,见ngx_http_subrequest */
    //如果是文件异步i/o中的ngx_event_aio_t，则它来自ngx_event_aio_t->ngx_event_t(只有读),如果是网络事件中的event,则为ngx_connection_s中的event(包括读和写)
    ngx_event_t        *read;//连接对应的读事件   赋值在ngx_event_process_init，空间是从ngx_cycle_t->read_event池子中获取的
    ngx_event_t        *write; //连接对应的写事件  赋值在ngx_event_process_init 一般在ngx_handle_write_event中添加些事件，空间是从ngx_cycle_t->read_event池子中获取的

    ngx_socket_t        fd;//套接字句柄
    /* 如果启用了ssl,则发送和接收数据在ngx_ssl_recv ngx_ssl_write ngx_ssl_recv_chain ngx_ssl_send_chain */
    //服务端通过ngx_http_wait_request_handler读取数据
    ngx_recv_pt         recv; //直接接收网络字符流的方法  见ngx_event_accept或者ngx_http_upstream_connect   赋值为ngx_os_io  在接收到客户端连接或者向上游服务器发起连接后赋值
    ngx_send_pt         send; //直接发送网络字符流的方法  见ngx_event_accept或者ngx_http_upstream_connect   赋值为ngx_os_io  在接收到客户端连接或者向上游服务器发起连接后赋值

    /* 如果启用了ssl,则发送和接收数据在ngx_ssl_recv ngx_ssl_write ngx_ssl_recv_chain ngx_ssl_send_chain */
    //以ngx_chain_t链表为参数来接收网络字符流的方法  ngx_recv_chain
    ngx_recv_chain_pt   recv_chain;  //赋值见ngx_event_accept     ngx_event_pipe_read_upstream中执行
    //以ngx_chain_t链表为参数来发送网络字符流的方法    ngx_send_chain
    //当http2头部帧发送的时候，会在ngx_http_v2_header_filter把ngx_http_v2_send_chain.send_chain=ngx_http_v2_send_chain
    ngx_send_chain_pt   send_chain; //赋值见ngx_event_accept   ngx_http_write_filter和ngx_chain_writer中执行

    //这个连接对应的ngx_listening_t监听对象,通过listen配置项配置，此连接由listening监听端口的事件建立,赋值在ngx_event_process_init
    //接收到客户端连接后会冲连接池分配一个ngx_connection_s结构，其listening成员指向服务器接受该连接的listen信息结构，见ngx_event_accept
    ngx_listening_t    *listening; //实际上是从cycle->listening.elts中的一个ngx_listening_t   

    off_t               sent;//这个连接上已经发送出去的字节数 //ngx_linux_sendfile_chain和ngx_writev_chain没发送多少字节就加多少字节

    ngx_log_t          *log;//可以记录日志的ngx_log_t对象 其实就是ngx_listening_t中获取的log //赋值见ngx_event_accept

    /*
    内存池。一般在accept -个新连接时，会创建一个内存池，而在这个连接结束时会销毁内存池。注意，这里所说的连接是指成功建立的
    TCP连接，所有的ngx_connection_t结构体都是预分配的。这个内存池的大小将由listening监听对象中的pool_size成员决定
     */
    ngx_pool_t         *pool; //在accept返回成功后创建poll,见ngx_event_accept， 连接上游服务区的时候在ngx_http_upstream_connect创建

    struct sockaddr    *sockaddr; //连接客户端的sockaddr结构体  客户端的，本端的为下面的local_sockaddr 赋值见ngx_event_accept
    socklen_t           socklen; //sockaddr结构体的长度  //赋值见ngx_event_accept
    ngx_str_t           addr_text; //连接客户端字符串形式的IP地址  

    ngx_str_t           proxy_protocol_addr;

#if (NGX_SSL)
    ngx_ssl_connection_t  *ssl; //赋值见ngx_ssl_create_connection
#endif

    //本机的监听端口对应的sockaddr结构体，也就是listening监听对象中的sockaddr成员
    struct sockaddr    *local_sockaddr; //赋值见ngx_event_accept
    socklen_t           local_socklen;

    /*
    用于接收、缓存客户端发来的字符流，每个事件消费模块可自由决定从连接池中分配多大的空间给buffer这个接收缓存字段。
    例如，在HTTP模块中，它的大小决定于client_header_buffer_size配置项
     */
    ngx_buf_t          *buffer; //ngx_http_request_t->header_in指向该缓冲去

    /*
    该字段用来将当前连接以双向链表元素的形式添加到ngx_cycle_t核心结构体的reusable_connections_queue双向链表中，表示可以重用的连接
     */
    ngx_queue_t         queue;

    /*
    连接使用次数。ngx_connection t结构体每次建立一条来自客户端的连接，或者用于主动向后端服务器发起连接时（ngx_peer_connection_t也使用它），
    number都会加l
     */
    ngx_atomic_uint_t   number; //这个应该是记录当前连接是整个连接中的第几个连接，见ngx_event_accept  ngx_event_connect_peer

    ngx_uint_t          requests; //处理的请求次数

    /*
    缓存中的业务类型。任何事件消费模块都可以自定义需要的标志位。这个buffered字段有8位，最多可以同时表示8个不同的业务。第三方模
    块在自定义buffered标志位时注意不要与可能使用的模块定义的标志位冲突。目前openssl模块定义了一个标志位：
        #define NGX_SSL_BUFFERED    Ox01
        
        HTTP官方模块定义了以下标志位：
        #define NGX HTTP_LOWLEVEL_BUFFERED   0xf0
        #define NGX_HTTP_WRITE_BUFFERED       0x10
        #define NGX_HTTP_GZIP_BUFFERED        0x20
        #define NGX_HTTP_SSI_BUFFERED         0x01
        #define NGX_HTTP_SUB_BUFFERED         0x02
        #define NGX_HTTP_COPY_BUFFERED        0x04
        #define NGX_HTTP_IMAGE_BUFFERED       Ox08
    同时，对于HTTP模块而言，buffered的低4位要慎用，在实际发送响应的ngx_http_write_filter_module过滤模块中，低4位标志位为1则惫味着
    Nginx会一直认为有HTTP模块还需要处理这个请求，必须等待HTTP模块将低4位全置为0才会正常结束请求。检查低4位的宏如下：
        #define NGX_LOWLEVEL_BUFFERED  OxOf
     */
    unsigned            buffered:8; //不为0，表示有数据没有发送完毕，ngx_http_request_t->out中还有未发送的报文

    /*
     本连接记录日志时的级别，它占用了3位，取值范围是0-7，但实际上目前只定义了5个值，由ngx_connection_log_error_e枚举表示，如下：
    typedef enum{
        NGXi ERROR—AIERT=0，
        NGX' ERROR ERR,
        NGX  ERROR_INFO，
        NGX, ERROR IGNORE ECONNRESET,
        NGX ERROR—IGNORE EIb:fVAL
     }
     ngx_connection_log_error_e ;
     */
    unsigned            log_error:3;     /* ngx_connection_log_error_e */

    //标志位，为1时表示不期待字符流结束，目前无意义
    unsigned            unexpected_eof:1;

    //每次处理完一个客户端请求后，都会ngx_add_timer(rev, c->listening->post_accept_timeout);
    /*读客户端连接的数据，在ngx_http_init_connection(ngx_connection_t *c)中的ngx_add_timer(rev, c->listening->post_accept_timeout)把读事件添加到定时器中，如果超时则置1
      每次ngx_unix_recv把内核数据读取完毕后，在重新启动add epoll，等待新的数据到来，同时会启动定时器ngx_add_timer(rev, c->listening->post_accept_timeout);
      如果在post_accept_timeout这么长事件内没有数据到来则超时，开始处理关闭TCP流程*/
      //当ngx_event_t->timedout置1的时候，该置也同时会置1，参考ngx_http_process_request_line  ngx_http_process_request_headers
      //在ngx_http_free_request中如果超时则会设置SO_LINGER来减少time_wait状态
    unsigned            timedout:1; //标志位，为1时表示连接已经超时,也就是过了post_accept_timeout多少秒还没有收到客户端的请求
    unsigned            error:1; //标志位，为1时表示连接处理过程中出现错误

    /*
     标志位，为1时表示连接已经销毁。这里的连接指是的TCP连接，而不是ngx_connection t结构体。当destroyed为1时，ngx_connection_t结
     构体仍然存在，但其对应的套接字、内存池等已经不可用
     */
    unsigned            destroyed:1; //ngx_http_close_connection中置1

    unsigned            idle:1; //为1时表示连接处于空闲状态，如keepalive请求中丽次请求之间的状态
    unsigned            reusable:1; //为1时表示连接可重用，它与上面的queue字段是对应使用的
    unsigned            close:1; //为1时表示连接关闭
    /*
        和后端的ngx_connection_t在ngx_event_connect_peer这里置为1，但在ngx_http_upstream_connect中c->sendfile &= r->connection->sendfile;，
        和客户端浏览器的ngx_connextion_t的sendfile需要在ngx_http_update_location_config中判断，因此最终是由是否在configure的时候是否有加
        sendfile选项来决定是置1还是置0
     */
    //赋值见ngx_http_update_location_config
    unsigned            sendfile:1; //标志位，为1时表示正在将文件中的数据发往连接的另一端

    /*
    标志位，如果为1，则表示只有在连接套接字对应的发送缓冲区必须满足最低设置的大小阅值时，事件驱动模块才会分发该事件。这与上文
    介绍过的ngx_handle_write_event方法中的lowat参数是对应的
     */
    unsigned            sndlowat:1; //ngx_send_lowat

    /*
    标志位，表示如何使用TCP的nodelay特性。它的取值范围是下面这个枚举类型ngx_connection_tcp_nodelay_e。
    typedef enum{
    NGX_TCP_NODELAY_UNSET=O,
    NGX_TCP_NODELAY_SET,
    NGX_TCP_NODELAY_DISABLED
    )  ngx_connection_tcp_nodelay_e;
     */
    unsigned            tcp_nodelay:2;   /* ngx_connection_tcp_nodelay_e */ //域套接字默认是disable的,

    /*
    标志位，表示如何使用TCP的nopush特性。它的取值范围是下面这个枚举类型ngx_connection_tcp_nopush_e：
    typedef enum{
    NGX_TCP_NOPUSH_UNSET=0,
    NGX_TCP_NOPUSH_SET,
    NGX_TCP_NOPUSH_DISABLED
    )  ngx_connection_tcp_nopush_e
     */ //域套接字默认是disable的,
    unsigned            tcp_nopush:2;    /* ngx_connection_tcp_nopush_e */

    unsigned            need_last_buf:1;

#if (NGX_HAVE_IOCP)
    unsigned            accept_context_updated:1;
#endif

/*
#if (NGX HAVE AIO- SENDFILE)
    ／／标志位，为1时表示使用异步I/O的方式将磁盘上文件发送给网络连接的另一端
    unsigned aio一sendfile:l;
    ／／使用异步I/O方式发送的文件，busy_sendfile缓冲区保存待发送文件的信息
    ngx_buf_t   *busy_sendf ile;
#endif
*/
#if (NGX_HAVE_AIO_SENDFILE)
    unsigned            busy_count:2;
#endif

#if (NGX_THREADS)
    ngx_thread_task_t  *sendfile_task;
#endif
};


#define ngx_set_connection_log(c, l)                                         \
                                                                             \
    c->log->file = l->file;                                                  \
    c->log->next = l->next;                                                  \
    c->log->writer = l->writer;                                              \
    c->log->wdata = l->wdata;                                                \
    if (!(c->log->log_level & NGX_LOG_DEBUG_CONNECTION)) {                   \
        c->log->log_level = l->log_level;                                    \
    }


ngx_listening_t *ngx_create_listening(ngx_conf_t *cf, void *sockaddr,
    socklen_t socklen);
ngx_int_t ngx_clone_listening(ngx_conf_t *cf, ngx_listening_t *ls);
ngx_int_t ngx_set_inherited_sockets(ngx_cycle_t *cycle);
ngx_int_t ngx_open_listening_sockets(ngx_cycle_t *cycle);
void ngx_configure_listening_sockets(ngx_cycle_t *cycle);
void ngx_close_listening_sockets(ngx_cycle_t *cycle);
void ngx_close_connection(ngx_connection_t *c);
ngx_int_t ngx_connection_local_sockaddr(ngx_connection_t *c, ngx_str_t *s,
    ngx_uint_t port);
ngx_int_t ngx_connection_error(ngx_connection_t *c, ngx_err_t err, char *text);

ngx_connection_t *ngx_get_connection(ngx_socket_t s, ngx_log_t *log);
void ngx_free_connection(ngx_connection_t *c);

void ngx_reusable_connection(ngx_connection_t *c, ngx_uint_t reusable);

#endif /* _NGX_CONNECTION_H_INCLUDED_ */
