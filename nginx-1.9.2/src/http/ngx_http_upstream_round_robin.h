
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_UPSTREAM_ROUND_ROBIN_H_INCLUDED_
#define _NGX_HTTP_UPSTREAM_ROUND_ROBIN_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct ngx_http_upstream_rr_peer_s   ngx_http_upstream_rr_peer_t;

/*
例如upstream {
    server ip1;
    server ip2;
}
则ngx_http_upstream_rr_peers_s会包含两个ngx_http_upstream_rr_peer_s信息
*/

/* //server backend1.example.com weight=5;
·weight = NUMBER - 设置服务器权重，默认为1。
·max_fails = NUMBER - 在一定时间内（这个时间在fail_timeout参数中设置）检查这个服务器是否可用时产生的最多失败请求数，默认为1，将其设置为0可以关闭检查，这些错误在proxy_next_upstream或fastcgi_next_upstream（404错误不会使max_fails增加）中定义。
·fail_timeout = TIME - 在这个时间内产生了max_fails所设置大小的失败尝试连接请求后这个服务器可能不可用，同样它指定了服务器不可用的时间（在下一次尝试连接请求发起之前），默认为10秒，fail_timeout与前端响应时间没有直接关系，不过可以使用proxy_connect_timeout和proxy_read_timeout来控制。
·down - 标记服务器处于离线状态，通常和ip_hash一起使用。
·backup - (0.6.7或更高)如果所有的非备份服务器都宕机或繁忙，则使用本服务器（无法和ip_hash指令搭配使用）。
*/
//ngx_http_upstream_rr_peers_s中包含多个后端服务器信息，最终是存到ngx_http_upstream_srv_conf_t->peer.data，见ngx_http_upstream_init_round_robin
struct ngx_http_upstream_rr_peer_s {

    struct sockaddr                *sockaddr; //初始赋值见ngx_http_upstream_init_round_robin
    socklen_t                       socklen;  //初始赋值见ngx_http_upstream_init_round_robin//赋值见ngx_http_upstream_init_round_robin
    ngx_str_t                       name; //初始赋值见ngx_http_upstream_init_round_robin
    ngx_str_t                       server; //初始赋值见ngx_http_upstream_init_round_robin

    ngx_int_t                       current_weight; //rr算法权重 //当前权重，nginx会在运行过程中调整此权重
    ngx_int_t                       effective_weight; //rr算法权重 //初始赋值见ngx_http_upstream_init_round_robin
    ngx_int_t                       weight;//配置的权重

    ngx_uint_t                      conns; //该后端peer上面的成功连接数
    /*
        当fails达到最大上限次数max_fails，则当fail_timeout时间过后会再次选择该后端服务器，
        选择后端服务器成功后ngx_http_upstream_free_round_robin_peer会把fails置0
     */
    ngx_uint_t                      fails;//已尝试失败次数  赋值见ngx_http_upstream_free_XXX_peer(ngx_http_upstream_free_round_robin_peer)
    //选取的后端服务器异常则跟新accessed时间为当前选取后端服务器的时候检测到异常的时间，见ngx_http_upstream_free_round_robin_peer
    time_t                          accessed;//检测失败时间，用于计算超时

    /*
       fail_timeout事件内访问后端出现错误的次数大于等于max_fails，则认为该服务器不可用，那么如果不可用了，后端该服务器有恢复了怎么判断检测呢?
       答:当这个fail_timeout时间段过了后，会重置peer->checked，那么有可以试探该服务器了，参考ngx_http_upstream_get_peer
       //checked用来检测时间，例如某个时间段fail_timeout这段时间后端失效了，那么这个fail_timeout过了后，也可以试探使用该服务器

       1）如果server的失败次数（peers->peer[i].fails）没有达到了max_fails所设置的最大失败次数，则该server是有效的。
      2）如果server已经达到了max_fails所设置的最大失败次数，从这一时刻开始算起，在fail_timeout 所设置的时间段内， server是无效的。
      3）当server的失败次数（peers->peer[i].fails）为最大的失败次数，当距离现在的时间(最近一次选举该服务器失败)超过了fail_timeout 所设置的时间段， 则令peers->peer[i].fails =0，使得该server重新有效。
    */ 
    //checked用来检测时间，例如某个时间段fail_timeout这段时间后端失效了，那么这个fail_timeout过了后，也可以试探使用该服务器
    time_t                          checked;//和fail_timeout配合阅读  一个fail_timeout时间段到了，则跟新checeked为当前时间

    ngx_uint_t                      max_fails;//最大失败次数
    time_t                          fail_timeout;//多长时间内出现max_fails次失败便认为后端down掉了  参考上面的checked

    //是否处于离线不可用状态 赋值见ngx_http_upstream_init_round_robin   
    //只有在server xxxx down;加上down配置，该服务器才不会被轮询到。一般都是人为指定后端某个服务器挂了，则修改配置文件加上down，然后重新reload nginx进程
    ngx_uint_t                      down;          /* unsigned  down:1; *///指定某后端是否挂了

#if (NGX_HTTP_SSL)
    void                           *ssl_session;
    int                             ssl_session_len;
#endif
    
    //所有同一类服务器(非backup或者backup)服务器信息直接通过ngx_http_upstream_rr_peer_s->next连接，
    //backup服务器和非backup服务器通过ngx_http_upstream_rr_peers_s->next连接在一起，见ngx_http_upstream_init_round_robin
    ngx_http_upstream_rr_peer_t    *next; 
    
#if (NGX_HTTP_UPSTREAM_ZONE)
    ngx_atomic_t                    lock;
#endif
};


typedef struct ngx_http_upstream_rr_peers_s  ngx_http_upstream_rr_peers_t;

//此函数会创建后端服务器列表，并且将非后备服务器与后备服务器分开进行各自单独的链表。每一个后端服务器用一个结构体
//ngx_http_upstream_rr_peer_t与之对应（ngx_http_upstream_round_robin.h）： 

//ngx_http_upstream_init_round_robin中赋值和创建空间
struct ngx_http_upstream_rr_peers_s { //每个upstream节点的信息  
    ngx_uint_t                      number;//服务器数量  为后端配置了多少个服务器  赋值见ngx_http_upstream_init_round_robin

#if (NGX_HTTP_UPSTREAM_ZONE)
    ngx_slab_pool_t                *shpool;
    ngx_atomic_t                    rwlock;
#endif

    ngx_uint_t                      total_weight; //所有服务器的权重和

    unsigned                        single:1;//是否只有一个服务器，例如upstrem xxx {server ip}这种情况下就一个ngx_http_upstream_init_round_robin
    ////为1表示总的权重值等于服务器数量
    unsigned                        weighted:1; //如果为1，表示后端服务器全部一样  ngx_http_upstream_init_round_robin

    ngx_str_t                      *name; //upstrem xxx {}中的xxx，如果是fastcgi_pass IP:PORT,则没有name

    //所有同一类服务器(非backup或者backup)服务器信息直接通过ngx_http_upstream_rr_peer_s->next连接，
    //backup服务器和非backup服务器通过ngx_http_upstream_rr_peers_s->next连接在一起，见ngx_http_upstream_init_round_robin
    ngx_http_upstream_rr_peers_t   *next; //下个upstream节点，例如所有非backup服务器的peers->next会指向所有的backup服务器信息ngx_http_upstream_init_round_robin
    
    /*
    例如upstream {
        server ip1;
        server ip2;
    }
    则ngx_http_upstream_rr_peers_s会包含两个ngx_http_upstream_rr_peer_s信息
    */
    ngx_http_upstream_rr_peer_t    *peer;//服务器信息 //所有的peer[]服务器信息通过peers->peer连接在一起ngx_http_upstream_init_round_robin
}; //


#if (NGX_HTTP_UPSTREAM_ZONE)

#define ngx_http_upstream_rr_peers_rlock(peers)                               \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_rlock(&peers->rwlock);                                     \
    }

#define ngx_http_upstream_rr_peers_wlock(peers)                               \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_wlock(&peers->rwlock);                                     \
    }

#define ngx_http_upstream_rr_peers_unlock(peers)                              \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_unlock(&peers->rwlock);                                    \
    }


#define ngx_http_upstream_rr_peer_lock(peers, peer)                           \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_wlock(&peer->lock);                                        \
    }

#define ngx_http_upstream_rr_peer_unlock(peers, peer)                         \
                                                                              \
    if (peers->shpool) {                                                      \
        ngx_rwlock_unlock(&peer->lock);                                       \
    }

#else

#define ngx_http_upstream_rr_peers_rlock(peers)
#define ngx_http_upstream_rr_peers_wlock(peers)
#define ngx_http_upstream_rr_peers_unlock(peers)
#define ngx_http_upstream_rr_peer_lock(peers, peer)
#define ngx_http_upstream_rr_peer_unlock(peers, peer)

#endif

/*
current_weight,effective_weight,weight三者的意义是不同的，三者一起来计算优先级，见下面链接：
http://blog.sina.com.cn/s/blog_7303a1dc01014i0j.html
tried字段实现了一个位图。对于后端服务器小于32台时，使用一个32位int来标识各个服务器是否尝试连接过。当后端服务器大于32台时，
会在内存池中新申请内存来存储该位图。data是该字段实际存储的位置。current是当前尝试到哪台服务器。
*/
typedef struct {
    ngx_http_upstream_rr_peers_t   *peers;//所有服务器信息   ngx_http_upstream_init_round_robin_peer)
    ngx_http_upstream_rr_peer_t    *current; //当前服务器 current是当前尝试到哪台服务器。
    uintptr_t                      *tried;//服务器位图指针，用于记录服务器当前的状态  赋值见 ngx_http_upstream_init_round_robin_peer)
    uintptr_t                       data;//tried位图实际存储位置  rrp->tried = &rrp->data; 赋值见ngx_http_upstream_init_round_robin_peer
} ngx_http_upstream_rr_peer_data_t;//ngx_http_upstream_get_peer和ngx_http_upstream_init_round_robin_peer配合阅读


ngx_int_t ngx_http_upstream_init_round_robin(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *us);
ngx_int_t ngx_http_upstream_init_round_robin_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);
ngx_int_t ngx_http_upstream_create_round_robin_peer(ngx_http_request_t *r,
    ngx_http_upstream_resolved_t *ur);
ngx_int_t ngx_http_upstream_get_round_robin_peer(ngx_peer_connection_t *pc,
    void *data);
void ngx_http_upstream_free_round_robin_peer(ngx_peer_connection_t *pc,
    void *data, ngx_uint_t state);

#if (NGX_HTTP_SSL)
ngx_int_t
    ngx_http_upstream_set_round_robin_peer_session(ngx_peer_connection_t *pc,
    void *data);
void ngx_http_upstream_save_round_robin_peer_session(ngx_peer_connection_t *pc,
    void *data);
#endif


#endif /* _NGX_HTTP_UPSTREAM_ROUND_ROBIN_H_INCLUDED_ */
