
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_CONNECT_H_INCLUDED_
#define _NGX_EVENT_CONNECT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


#define NGX_PEER_KEEPALIVE           1
#define NGX_PEER_NEXT                2
#define NGX_PEER_FAILED              4 //使用赋值地方在ngx_http_upstream_next


typedef struct ngx_peer_connection_s  ngx_peer_connection_t;

//当使用长连接与上游服务器通信时，可通过该方法由连接池中获取一个新连接
typedef ngx_int_t (*ngx_event_get_peer_pt)(ngx_peer_connection_t *pc,
    void *data);
//当使用长连接与上游服务器通信时，通过该方法将使用完毕的连接释放给连接池
typedef void (*ngx_event_free_peer_pt)(ngx_peer_connection_t *pc, void *data,
    ngx_uint_t state);
#if (NGX_SSL)

typedef ngx_int_t (*ngx_event_set_peer_session_pt)(ngx_peer_connection_t *pc,
    void *data);
typedef void (*ngx_event_save_peer_session_pt)(ngx_peer_connection_t *pc,
    void *data);
#endif

/*
Nginx中定义了基本的数据结构ngx_connection_t来表示连接，这个连接表示是客户端主动发起的、Nginx服务器被动接受的TCP连接，我们可以简单称
其为被动连接。同时，在有些请求的处理过程中，Nginx会试图主动向其他上游服务器建立连接，并以此连接与上游服务器通信，因此，这样的
连接与ngx_connection_t又是不同的，Nginx定义了}ngx_peer_connection_t结构体来表示主动连接，当然，ngx_peer_connection_t主动连接是
以ngx_connection-t结构体为基础实现的。本节将说明这两种连接中各字段的意义，同时需要注意的是，这两种连接都不可以随意创建，必须从
连接池中获取，
*/
//被动连接(客户端连接nginx)对应的数据结构是ngx_connection_s，主动连接(nginx连接后端服务器)对应的数据结构是ngx_peer_connection_s
struct ngx_peer_connection_s {
    /* 一个主动连接实际上也需要ngx_connection_t结构体中的大部分成员，并且出于重用的考虑而定义了connection成员 */
    ngx_connection_t                *connection; //赋初值见ngx_event_connect_peer  连接的fd保存在这里面

    struct sockaddr                 *sockaddr;//远端服务器的socket地址
    socklen_t                        socklen; //sockaddr地址的长度
    ngx_str_t                       *name; //远端服务器的名称 

    //表示在连接一个远端服务器时，当前连接出现异常失败后可以重试的次数，也就是允许的最多失败次数
    ngx_uint_t                       tries; //赋值见ngx_http_upstream_init_xxx_peer(例如ngx_http_upstream_init_round_robin_peer)
    ngx_msec_t                       start_time;//向后端服务器发起连接的时间ngx_http_upstream_init_request

    /*
       fail_timeout事件内访问后端出现错误的次数大于等于max_fails，则认为该服务器不可用，那么如果不可用了，后端该服务器有恢复了怎么判断检测呢?
       答:当这个fail_timeout时间段过了后，会重置peer->checked，那么有可以试探该服务器了，参考ngx_http_upstream_get_peer
       //checked用来检测时间，例如某个时间段fail_timeout这段时间后端失效了，那么这个fail_timeout过了后，也可以试探使用该服务器
     */ 
    //ngx_event_connect_peer中执行 获取连接的方法，如果使用长连接构成的连接池，那么必须要实现get方法
    //ngx_http_upstream_get_round_robin_peer ngx_http_upstream_get_least_conn_peer 
    //ngx_http_upstream_get_hash_peer  ngx_http_upstream_get_ip_hash_peer ngx_http_upstream_get_keepalive_peer等
    ngx_event_get_peer_pt            get; //赋值见ngx_http_upstream_init_xxx_peer(例如ngx_http_upstream_init_round_robin_peer)
    ngx_event_free_peer_pt           free; //与get方法对应的释放连接的方法 ngx_http_upstream_next或者ngx_http_upstream_finalize_request中执行

    /*
     这个data指针仅用于和上面的get、free方法配合传递参数，它的具体含义与实现get方法、free
     方法的模块相关，可参照ngx_event_get_peer_pt和ngx_event_free_pee r_pt方法原型中的data参数
     */ //如果采用iphash，则data=ngx_http_upstream_ip_hash_peer_data_t->rrp,见ngx_http_upstream_init_ip_hash_peer
    void                            *data; //例如rr算法,对应结构ngx_http_upstream_rr_peer_data_t，创建空间在ngx_http_upstream_create_round_robin_peer

#if (NGX_SSL)
    ngx_event_set_peer_session_pt    set_session;
    ngx_event_save_peer_session_pt   save_session;
#endif

    ngx_addr_t                      *local; //本机地址信息 //proxy_bind  fastcgi_bind 设置的本地IP端口地址，有可能设备有好几个eth，只用其中一个

    int                              rcvbuf; //套接字的接收缓冲区大小

    ngx_log_t                       *log; //记录日志的ngx_log_t对象

    unsigned                         cached:1; //标志位，为1时表示上面的connection连接已经缓存

    /* ngx_connection_log_error_e */
    /*NGX_ERROR_IGNORE_EINVAL  ngx_connection_log_error_e
  与ngx_connection_t里的log_error惫义是相同的，区别在于这里的log_error只有两位，只能表达4种错误，NGX_ERROR_IGNORE_EINVAL错误无法表达
     */
    unsigned                         log_error:2;
};


ngx_int_t ngx_event_connect_peer(ngx_peer_connection_t *pc);
ngx_int_t ngx_event_get_peer(ngx_peer_connection_t *pc, void *data);


#endif /* _NGX_EVENT_CONNECT_H_INCLUDED_ */

