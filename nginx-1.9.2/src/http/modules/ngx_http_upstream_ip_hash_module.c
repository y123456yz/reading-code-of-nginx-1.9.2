
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    /* the round robin data must be first */
    ngx_http_upstream_rr_peer_data_t   rrp;

    ngx_uint_t                         hash;

    u_char                             addrlen;
    u_char                            *addr;

    u_char                             tries;

    ngx_event_get_peer_pt              get_rr_peer;
} ngx_http_upstream_ip_hash_peer_data_t;


static ngx_int_t ngx_http_upstream_init_ip_hash_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);
static ngx_int_t ngx_http_upstream_get_ip_hash_peer(ngx_peer_connection_t *pc,
    void *data);
static char *ngx_http_upstream_ip_hash(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


static ngx_command_t  ngx_http_upstream_ip_hash_commands[] = {
/*
ip_hash
语法：ip_hash;
配置块：upstream
在有些场景下，我们可能会希望来自某一个用户的请求始终落到固定的一台上游服务器中。例如，假设上游服务器会缓存一些信息，如果同一个用户的请求任意
地转发到集群中的任一台上游服务器中，那么每一台上游服务器都有可能会缓存同一份信息，这既会造成资源的浪费，也会难以有效地管理缓存信息。
ip_hash就是用以解决上述问题的，它首先根据客户端的IP地址计算出一个key，将key按照upstream集群里的上游服务器数量进行取模，然后以取模后的结果把
请求转发到相应的上游服务器中。这样就确保了同一个客户端的请求只会转发到指定的上游服务器中。
ip_hash与weight（权重）配置不可同时使用。如果upstream集群中有一台上游服务器暂时不可用，不能直接删除该配置，而是要down参数标识，确保转发策略的一贯性。例如：
upstream backend {
  ip_hash;
  server   backend1.example.com;
  server   backend2.example.com;
  server   backend3.example.com  down;
  server   backend4.example.com;
}

指定nginx负载均衡器组使用基于客户端ip的负载均衡算法。IPV4的前3个八进制位和所有的IPV6地址被用作一个hash key。这个方法确保了
相同客户端的请求一直发送到相同的服务器上除非这个服务器被认为是停机。这种情况下，请求会被发送到其他主机上，然后可能会一直发 
送到这个主机上。
如果nginx负载均衡器组里面的一个服务器要临时移除，它应该用参数down标记，来防止之前的客户端IP还往这个服务器上发请求。
例子：
[cpp] view plaincopyprint?

1.upstream backend {  
2.    ip_hash;  
3.   
4.    server backend1.example.com;  
5.    server backend2.example.com;  
6.    server backend3.example.com down;  
7.    server backend4.example.com;  
8.}  
*/
    { ngx_string("ip_hash"),
      NGX_HTTP_UPS_CONF|NGX_CONF_NOARGS,
      ngx_http_upstream_ip_hash,
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_upstream_ip_hash_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


ngx_module_t  ngx_http_upstream_ip_hash_module = {
    NGX_MODULE_V1,
    &ngx_http_upstream_ip_hash_module_ctx, /* module context */
    ngx_http_upstream_ip_hash_commands,    /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static u_char ngx_http_upstream_ip_hash_pseudo_addr[3];

/*
Load-blance模块中4个关键回调函数：
回调指针                  函数功能                          round_robin模块                     IP_hash模块
 
uscf->peer.init_upstream
解析配置文件过程中调用，根据upstream里各个server配置项做初始准备工作，另外的核心工作是设置回调指针us->peer.init。配置文件解析完后不再被调用

ngx_http_upstream_init_round_robin
设置：us->peer.init = ngx_http_upstream_init_round_robin_peer;
 
ngx_http_upstream_init_ip_hash
设置：us->peer.init = ngx_http_upstream_init_ip_hash_peer;
 


us->peer.init
在每一次Nginx准备转发客户端请求到后端服务器前都会调用该函数。该函数为本次转发选择合适的后端服务器做初始准备工作，另外的核心工
作是设置回调指针r->upstream->peer.get和r->upstream->peer.free等
 
ngx_http_upstream_init_round_robin_peer
设置：r->upstream->peer.get = ngx_http_upstream_get_round_robin_peer;
r->upstream->peer.free = ngx_http_upstream_free_round_robin_peer;
 
ngx_http_upstream_init_ip_hash_peer
设置：r->upstream->peer.get = ngx_http_upstream_get_ip_hash_peer;
r->upstream->peer.free为空
 


r->upstream->peer.get
在每一次Nginx准备转发客户端请求到后端服务器前都会调用该函数。该函数实现具体的位本次转发选择合适的后端服务器的算法逻辑，即
完成选择获取合适后端服务器的功能
 
ngx_http_upstream_get_round_robin_peer
加权选择当前权值最高的后端服务器
 
ngx_http_upstream_get_ip_hash_peer
根据IP哈希值选择后端服务器
 




r->upstream->peer.free
在每一次Nginx完成与后端服务器之间的交互后都会调用该函数。
ngx_http_upstream_free_round_robin_peer
更新相关数值，比如rrp->current
*/

static ngx_int_t
ngx_http_upstream_init_ip_hash(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us)
{
    if (ngx_http_upstream_init_round_robin(cf, us) != NGX_OK) {
        return NGX_ERROR;
    }

    us->peer.init = ngx_http_upstream_init_ip_hash_peer;

    return NGX_OK;
}

/*
Load-blance模块中4个关键回调函数：
回调指针                  函数功能                          round_robin模块                     IP_hash模块
 
uscf->peer.init_upstream (默认为ngx_http_upstream_init_round_robin 在ngx_http_upstream_init_main_conf中执行)
解析配置文件过程中调用，根据upstream里各个server配置项做初始准备工作，另外的核心工作是设置回调指针us->peer.init。配置文件解析完后不再被调用

ngx_http_upstream_init_round_robin
设置：us->peer.init = ngx_http_upstream_init_round_robin_peer;
 
ngx_http_upstream_init_ip_hash
设置：us->peer.init = ngx_http_upstream_init_ip_hash_peer;
 


us->peer.init
在每一次Nginx准备转发客户端请求到后端服务器前都会调用该函数。该函数为本次转发选择合适的后端服务器做初始准备工作，另外的核心工
作是设置回调指针r->upstream->peer.get和r->upstream->peer.free等
 
ngx_http_upstream_init_round_robin_peer
设置：r->upstream->peer.get = ngx_http_upstream_get_round_robin_peer;
r->upstream->peer.free = ngx_http_upstream_free_round_robin_peer;
 
ngx_http_upstream_init_ip_hash_peer
设置：r->upstream->peer.get = ngx_http_upstream_get_ip_hash_peer;
r->upstream->peer.free为空
 


r->upstream->peer.get
在每一次Nginx准备转发客户端请求到后端服务器前都会调用该函数。该函数实现具体的位本次转发选择合适的后端服务器的算法逻辑，即
完成选择获取合适后端服务器的功能
 
ngx_http_upstream_get_round_robin_peer
加权选择当前权值最高的后端服务器
 
ngx_http_upstream_get_ip_hash_peer
根据IP哈希值选择后端服务器
 




r->upstream->peer.free
在每一次Nginx完成与后端服务器之间的交互后都会调用该函数。
ngx_http_upstream_free_round_robin_peer
更新相关数值，比如rrp->current
*/
//轮询负债均衡算法ngx_http_upstream_init_round_robin_peer  iphash负载均衡算法ngx_http_upstream_init_ip_hash_peer
static ngx_int_t
ngx_http_upstream_init_ip_hash_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us)
{
    struct sockaddr_in                     *sin;
#if (NGX_HAVE_INET6)
    struct sockaddr_in6                    *sin6;
#endif
    ngx_http_upstream_ip_hash_peer_data_t  *iphp;

    iphp = ngx_palloc(r->pool, sizeof(ngx_http_upstream_ip_hash_peer_data_t));
    if (iphp == NULL) {
        return NGX_ERROR;
    }

    r->upstream->peer.data = &iphp->rrp;

    if (ngx_http_upstream_init_round_robin_peer(r, us) != NGX_OK) {
        return NGX_ERROR;
    }

    r->upstream->peer.get = ngx_http_upstream_get_ip_hash_peer;

    switch (r->connection->sockaddr->sa_family) {

    case AF_INET:
        sin = (struct sockaddr_in *) r->connection->sockaddr;
        iphp->addr = (u_char *) &sin->sin_addr.s_addr;
        iphp->addrlen = 3;
        break;

#if (NGX_HAVE_INET6)
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) r->connection->sockaddr;
        iphp->addr = (u_char *) &sin6->sin6_addr.s6_addr;
        iphp->addrlen = 16;
        break;
#endif

    default:
        iphp->addr = ngx_http_upstream_ip_hash_pseudo_addr;
        iphp->addrlen = 3;
    }

    iphp->hash = 89;
    iphp->tries = 0;
    iphp->get_rr_peer = ngx_http_upstream_get_round_robin_peer;

    return NGX_OK;
}

/*
Load-blance模块中4个关键回调函数：
回调指针                  函数功能                          round_robin模块                     IP_hash模块
 
uscf->peer.init_upstream (默认为ngx_http_upstream_init_round_robin 在ngx_http_upstream_init_main_conf中执行)
解析配置文件过程中调用，根据upstream里各个server配置项做初始准备工作，另外的核心工作是设置回调指针us->peer.init。配置文件解析完后不再被调用

ngx_http_upstream_init_round_robin
设置：us->peer.init = ngx_http_upstream_init_round_robin_peer;
 
ngx_http_upstream_init_ip_hash
设置：us->peer.init = ngx_http_upstream_init_ip_hash_peer;
 


us->peer.init
在每一次Nginx准备转发客户端请求到后端服务器前都会调用该函数。该函数为本次转发选择合适的后端服务器做初始准备工作，另外的核心工
作是设置回调指针r->upstream->peer.get和r->upstream->peer.free等
 
ngx_http_upstream_init_round_robin_peer
设置：r->upstream->peer.get = ngx_http_upstream_get_round_robin_peer;
r->upstream->peer.free = ngx_http_upstream_free_round_robin_peer;
 
ngx_http_upstream_init_ip_hash_peer
设置：r->upstream->peer.get = ngx_http_upstream_get_ip_hash_peer;
r->upstream->peer.free为空
 


r->upstream->peer.get
在每一次Nginx准备转发客户端请求到后端服务器前都会调用该函数。该函数实现具体的位本次转发选择合适的后端服务器的算法逻辑，即
完成选择获取合适后端服务器的功能
 
ngx_http_upstream_get_round_robin_peer
加权选择当前权值最高的后端服务器
 
ngx_http_upstream_get_ip_hash_peer
根据IP哈希值选择后端服务器

r->upstream->peer.free
在每一次Nginx完成与后端服务器之间的交互后都会调用该函数。
ngx_http_upstream_free_round_robin_peer
更新相关数值，比如rrp->current
*/

static ngx_int_t
ngx_http_upstream_get_ip_hash_peer(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_ip_hash_peer_data_t  *iphp = data;

    time_t                        now;
    ngx_int_t                     w;
    uintptr_t                     m;
    ngx_uint_t                    i, n, p, hash;
    ngx_http_upstream_rr_peer_t  *peer;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "get ip hash peer, try: %ui", pc->tries);

    /* TODO: cached */

    ngx_http_upstream_rr_peers_wlock(iphp->rrp.peers);

    if (iphp->tries > 20 || iphp->rrp.peers->single) {
        ngx_http_upstream_rr_peers_unlock(iphp->rrp.peers);
        return iphp->get_rr_peer(pc, &iphp->rrp);
    }

    now = ngx_time();

    pc->cached = 0;
    pc->connection = NULL;

    hash = iphp->hash;

    for ( ;; ) {

        for (i = 0; i < (ngx_uint_t) iphp->addrlen; i++) {
            hash = (hash * 113 + iphp->addr[i]) % 6271;
        }

        w = hash % iphp->rrp.peers->total_weight;
        peer = iphp->rrp.peers->peer;
        p = 0;

        while (w >= peer->weight) {
            w -= peer->weight;
            peer = peer->next;
            p++;
        }

        n = p / (8 * sizeof(uintptr_t));
        m = (uintptr_t) 1 << p % (8 * sizeof(uintptr_t));

        if (iphp->rrp.tried[n] & m) {
            goto next;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                       "get ip hash peer, hash: %ui %04XA", p, m);

        if (peer->down) {
            goto next;
        }

        if (peer->max_fails
            && peer->fails >= peer->max_fails
            && now - peer->checked <= peer->fail_timeout)
        {
            goto next;
        }

        break;

    next:

        if (++iphp->tries > 20) {
            ngx_http_upstream_rr_peers_unlock(iphp->rrp.peers);
            return iphp->get_rr_peer(pc, &iphp->rrp);
        }
    }

    iphp->rrp.current = peer;

    pc->sockaddr = peer->sockaddr;
    pc->socklen = peer->socklen;
    pc->name = &peer->name;

    peer->conns++;

    if (now - peer->checked > peer->fail_timeout) {
        peer->checked = now;
    }

    ngx_http_upstream_rr_peers_unlock(iphp->rrp.peers);

    iphp->rrp.tried[n] |= m;
    iphp->hash = hash;

    return NGX_OK;
}


static char *
ngx_http_upstream_ip_hash(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_upstream_srv_conf_t  *uscf;

    uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);

    if (uscf->peer.init_upstream) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "load balancing method redefined");
    }

    uscf->peer.init_upstream = ngx_http_upstream_init_ip_hash;

    uscf->flags = NGX_HTTP_UPSTREAM_CREATE
                  |NGX_HTTP_UPSTREAM_WEIGHT
                  |NGX_HTTP_UPSTREAM_MAX_FAILS
                  |NGX_HTTP_UPSTREAM_FAIL_TIMEOUT
                  |NGX_HTTP_UPSTREAM_DOWN;

    return NGX_CONF_OK;
}
