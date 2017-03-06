
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define ngx_http_upstream_tries(p) ((p)->number                               \
                                    + ((p)->next ? (p)->next->number : 0))


static ngx_http_upstream_rr_peer_t *ngx_http_upstream_get_peer(
    ngx_http_upstream_rr_peer_data_t *rrp);

#if (NGX_HTTP_SSL)

static ngx_int_t ngx_http_upstream_empty_set_session(ngx_peer_connection_t *pc,
    void *data);
static void ngx_http_upstream_empty_save_session(ngx_peer_connection_t *pc,
    void *data);

#endif

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

ngx_int_t
ngx_http_upstream_init_round_robin(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *us)//在ngx_http_upstream_init_main_conf中执行
{
    ngx_url_t                      u;
    ngx_uint_t                     i, j, n, w;
    ngx_http_upstream_server_t    *server;
    ngx_http_upstream_rr_peer_t   *peer, **peerp;
    ngx_http_upstream_rr_peers_t  *peers, *backup;

    us->peer.init = ngx_http_upstream_init_round_robin_peer;

    if (us->servers) {
        server = us->servers->elts;

        n = 0; //所有服务器数量
        w = 0; //所有服务器的权重之和
        

        for (i = 0; i < us->servers->nelts; i++) {
            if (server[i].backup) { //备份服务器不算在内
                continue;
            }

            n += server[i].naddrs;
            w += server[i].naddrs * server[i].weight;
        }

        if (n == 0) {
            ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                          "no servers in upstream \"%V\" in %s:%ui",
                          &us->host, us->file_name, us->line);
            return NGX_ERROR;
        }

        peers = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_rr_peers_t));
        if (peers == NULL) {
            return NGX_ERROR;
        }

        peer = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_rr_peer_t) * n); //为所有的非backup服务器分配相关存储空间
        if (peer == NULL) {
            return NGX_ERROR;
        }

        peers->single = (n == 1); //n=1，表示只为后端配置了一个服务器
        peers->number = n;
        peers->weighted = (w != n); //w=n表示权重都相等，都是1
        peers->total_weight = w;
        peers->name = &us->host;

        n = 0;
        peerp = &peers->peer;
        
        //初始化每个peer节点的信息
        for (i = 0; i < us->servers->nelts; i++) {
            if (server[i].backup) {
                continue;
            }

            for (j = 0; j < server[i].naddrs; j++) {
                peer[n].sockaddr = server[i].addrs[j].sockaddr;
                peer[n].socklen = server[i].addrs[j].socklen;
                peer[n].name = server[i].addrs[j].name;
                peer[n].weight = server[i].weight;
                peer[n].effective_weight = server[i].weight;
                peer[n].current_weight = 0;
                peer[n].max_fails = server[i].max_fails;
                peer[n].fail_timeout = server[i].fail_timeout;
                peer[n].down = server[i].down;
                peer[n].server = server[i].name;

                *peerp = &peer[n]; //所有的peer[]服务器信息通过peers->peer连接在一起
                peerp = &peer[n].next;
                n++;
            }
        }

        us->peer.data = peers;

        
        //初始化backup servers
        /* backup servers */

        n = 0;
        w = 0;

        for (i = 0; i < us->servers->nelts; i++) {
            if (!server[i].backup) {
                continue;
            }

            n += server[i].naddrs;
            w += server[i].naddrs * server[i].weight;
        }

        if (n == 0) {
            return NGX_OK;
        }

        backup = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_rr_peers_t));
        if (backup == NULL) {
            return NGX_ERROR;
        }

        peer = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_rr_peer_t) * n);
        if (peer == NULL) {
            return NGX_ERROR;
        }

        peers->single = 0;
        backup->single = 0;
        backup->number = n;
        backup->weighted = (w != n);
        backup->total_weight = w;
        backup->name = &us->host;

        n = 0;
        peerp = &backup->peer;

        for (i = 0; i < us->servers->nelts; i++) {
            if (!server[i].backup) {
                continue;
            }

            for (j = 0; j < server[i].naddrs; j++) {
                peer[n].sockaddr = server[i].addrs[j].sockaddr;
                peer[n].socklen = server[i].addrs[j].socklen;
                peer[n].name = server[i].addrs[j].name;
                peer[n].weight = server[i].weight;
                peer[n].effective_weight = server[i].weight;
                peer[n].current_weight = 0;
                peer[n].max_fails = server[i].max_fails;
                peer[n].fail_timeout = server[i].fail_timeout;
                peer[n].down = server[i].down;
                peer[n].server = server[i].name;

                *peerp = &peer[n]; //所有的backup服务器通过next连接在一起
                peerp = &peer[n].next;
                n++;
            }
        }

        peers->next = backup; //所有backup服务器的信息都连接在上面的非backup服务器的next后面，这样所有的服务器(包括backup和非backup)都会连接到us->peer.data

        return NGX_OK;
    }


    /* an upstream implicitly defined by proxy_pass, etc. */
    //us参数中服务器指针为空，例如用户直接在proxy_pass等指令后配置后端服务器地址
    if (us->port == 0) {
        ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                      "no port in upstream \"%V\" in %s:%ui",
                      &us->host, us->file_name, us->line);
        return NGX_ERROR;
    }

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.host = us->host;
    u.port = us->port;

    if (ngx_inet_resolve_host(cf->pool, &u) != NGX_OK) {
        if (u.err) {
            ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                          "%s in upstream \"%V\" in %s:%ui",
                          u.err, &us->host, us->file_name, us->line);
        }

        return NGX_ERROR;
    }

    n = u.naddrs;

    peers = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_rr_peers_t));
    if (peers == NULL) {
        return NGX_ERROR;
    }

    peer = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_rr_peer_t) * n);
    if (peer == NULL) {
        return NGX_ERROR;
    }

    peers->single = (n == 1);
    peers->number = n;
    peers->weighted = 0;
    peers->total_weight = n;
    peers->name = &us->host;

    peerp = &peers->peer;

    for (i = 0; i < u.naddrs; i++) {
        peer[i].sockaddr = u.addrs[i].sockaddr;
        peer[i].socklen = u.addrs[i].socklen;
        peer[i].name = u.addrs[i].name;
        peer[i].weight = 1;
        peer[i].effective_weight = 1;
        peer[i].current_weight = 0;
        peer[i].max_fails = 1;
        peer[i].fail_timeout = 10;
        *peerp = &peer[i];
        peerp = &peer[i].next;
    }

    us->peer.data = peers;

    /* implicitly defined upstream has no backup servers */

    return NGX_OK;
}

/*
Load-blance模块中4个关键回调函数：
回调指针                  函数功能                          round_robin模块                     IP_hash模块
 
uscf->peer.init_upstream默认为ngx_http_upstream_init_round_robin 在ngx_http_upstream_init_main_conf中执行
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

轮询策略和IP哈希策略对比
加权轮询策略
优点：适用性更强，不依赖于客户端的任何信息，完全依靠后端服务器的情况来进行选择。能把客户端请求更合理更均匀地分配到各个后端服务器处理。
缺点：同一个客户端的多次请求可能会被分配到不同的后端服务器进行处理，无法满足做会话保持的应用的需求。

IP哈希策略
优点：能较好地把同一个客户端的多次请求分配到同一台服务器处理，避免了加权轮询无法适用会话保持的需求。
缺点：当某个时刻来自某个IP地址的请求特别多，那么将导致某台后端服务器的压力可能非常大，而其他后端服务器却空闲的不均衡情况、
*/
//如果没有手动设置访问后端服务器的算法，则默认用robin方式  //轮询负债均衡算法ngx_http_upstream_init_round_robin_peer  iphash负载均衡算法ngx_http_upstream_init_ip_hash_peer
ngx_int_t //ngx_http_upstream_init_request准备好FCGI数据，buffer后，会调用这里进行一个peer的初始化，此处是轮询peer的初始化。
ngx_http_upstream_init_round_robin_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us) // (默认为ngx_http_upstream_init_round_robin 在ngx_http_upstream_init_main_conf中执行)
{//ngx_http_upstream_get_peer和ngx_http_upstream_init_round_robin_peer配合阅读  
    ngx_uint_t                         n;
    ngx_http_upstream_rr_peer_data_t  *rrp;

    rrp = r->upstream->peer.data;

    if (rrp == NULL) {
        rrp = ngx_palloc(r->pool, sizeof(ngx_http_upstream_rr_peer_data_t));
        if (rrp == NULL) {
            return NGX_ERROR;
        }

        r->upstream->peer.data = rrp;
    }

    rrp->peers = us->peer.data; //该upstream {}所在的
    rrp->current = NULL; //当前是第0个

    n = rrp->peers->number;

    if (rrp->peers->next && rrp->peers->next->number > n) {
        n = rrp->peers->next->number; //获取backup和非backup这两组服务器中最大的个数，例如backup服务器个数为5，非backup服务器为3，则这里为5
    }

    if (n <= 8 * sizeof(uintptr_t)) { //n<32直接用data来表示位图信息
        rrp->tried = &rrp->data;
        rrp->data = 0;

    } else { //超过32则分配相应的多个INT来存储位图
        n = (n + (8 * sizeof(uintptr_t) - 1)) / (8 * sizeof(uintptr_t));

        rrp->tried = ngx_pcalloc(r->pool, n * sizeof(uintptr_t));
        if (rrp->tried == NULL) {
            return NGX_ERROR;
        }
    }

    r->upstream->peer.get = ngx_http_upstream_get_round_robin_peer;
    r->upstream->peer.free = ngx_http_upstream_free_round_robin_peer;
    r->upstream->peer.tries = ngx_http_upstream_tries(rrp->peers);
#if (NGX_HTTP_SSL)
    r->upstream->peer.set_session =
                               ngx_http_upstream_set_round_robin_peer_session;
    r->upstream->peer.save_session =
                               ngx_http_upstream_save_round_robin_peer_session;
#endif

    return NGX_OK;
}


ngx_int_t
ngx_http_upstream_create_round_robin_peer(ngx_http_request_t *r,
    ngx_http_upstream_resolved_t *ur)
{
    u_char                            *p;
    size_t                             len;
    socklen_t                          socklen;
    ngx_uint_t                         i, n;
    struct sockaddr                   *sockaddr;
    ngx_http_upstream_rr_peer_t       *peer, **peerp;
    ngx_http_upstream_rr_peers_t      *peers;
    ngx_http_upstream_rr_peer_data_t  *rrp;

    rrp = r->upstream->peer.data;

    if (rrp == NULL) {
        rrp = ngx_palloc(r->pool, sizeof(ngx_http_upstream_rr_peer_data_t));
        if (rrp == NULL) {
            return NGX_ERROR;
        }

        r->upstream->peer.data = rrp;
    }

    peers = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_rr_peers_t));
    if (peers == NULL) {
        return NGX_ERROR;
    }

    peer = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_rr_peer_t)
                                * ur->naddrs);
    if (peer == NULL) {
        return NGX_ERROR;
    }

    peers->single = (ur->naddrs == 1);
    peers->number = ur->naddrs;
    peers->name = &ur->host;

    if (ur->sockaddr) {
        peer[0].sockaddr = ur->sockaddr;
        peer[0].socklen = ur->socklen;
        peer[0].name = ur->host;
        peer[0].weight = 1;
        peer[0].effective_weight = 1;
        peer[0].current_weight = 0;
        peer[0].max_fails = 1;
        peer[0].fail_timeout = 10;
        peers->peer = peer;

    } else {
        peerp = &peers->peer;

        for (i = 0; i < ur->naddrs; i++) {

            socklen = ur->addrs[i].socklen;

            sockaddr = ngx_palloc(r->pool, socklen);
            if (sockaddr == NULL) {
                return NGX_ERROR;
            }

            ngx_memcpy(sockaddr, ur->addrs[i].sockaddr, socklen);

            switch (sockaddr->sa_family) {
#if (NGX_HAVE_INET6)
            case AF_INET6:
                ((struct sockaddr_in6 *) sockaddr)->sin6_port = htons(ur->port);
                break;
#endif
            default: /* AF_INET */
                ((struct sockaddr_in *) sockaddr)->sin_port = htons(ur->port);
            }

            p = ngx_pnalloc(r->pool, NGX_SOCKADDR_STRLEN);
            if (p == NULL) {
                return NGX_ERROR;
            }

            len = ngx_sock_ntop(sockaddr, socklen, p, NGX_SOCKADDR_STRLEN, 1);

            peer[i].sockaddr = sockaddr;
            peer[i].socklen = socklen;
            peer[i].name.len = len;
            peer[i].name.data = p;
            peer[i].weight = 1;
            peer[i].effective_weight = 1;
            peer[i].current_weight = 0;
            peer[i].max_fails = 1;
            peer[i].fail_timeout = 10;
            *peerp = &peer[i];
            peerp = &peer[i].next;
        }
    }

    rrp->peers = peers;
    rrp->current = NULL;

    if (rrp->peers->number <= 8 * sizeof(uintptr_t)) {
        rrp->tried = &rrp->data;
        rrp->data = 0;

    } else {
        n = (rrp->peers->number + (8 * sizeof(uintptr_t) - 1))
                / (8 * sizeof(uintptr_t));

        rrp->tried = ngx_pcalloc(r->pool, n * sizeof(uintptr_t));
        if (rrp->tried == NULL) {
            return NGX_ERROR;
        }
    }

    r->upstream->peer.get = ngx_http_upstream_get_round_robin_peer;
    r->upstream->peer.free = ngx_http_upstream_free_round_robin_peer;
    r->upstream->peer.tries = ngx_http_upstream_tries(rrp->peers);
#if (NGX_HTTP_SSL)
    r->upstream->peer.set_session = ngx_http_upstream_empty_set_session;
    r->upstream->peer.save_session = ngx_http_upstream_empty_save_session;
#endif

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

轮询策略和IP哈希策略对比
加权轮询策略
优点：适用性更强，不依赖于客户端的任何信息，完全依靠后端服务器的情况来进行选择。能把客户端请求更合理更均匀地分配到各个后端服务器处理。
缺点：同一个客户端的多次请求可能会被分配到不同的后端服务器进行处理，无法满足做会话保持的应用的需求。

IP哈希策略
优点：能较好地把同一个客户端的多次请求分配到同一台服务器处理，避免了加权轮询无法适用会话保持的需求。
缺点：当某个时刻来自某个IP地址的请求特别多，那么将导致某台后端服务器的压力可能非常大，而其他后端服务器却空闲的不均衡情况、

轮询策略和IP哈希策略对比
加权轮询策略
优点：适用性更强，不依赖于客户端的任何信息，完全依靠后端服务器的情况来进行选择。能把客户端请求更合理更均匀地分配到各个后端服务器处理。
缺点：同一个客户端的多次请求可能会被分配到不同的后端服务器进行处理，无法满足做会话保持的应用的需求。

IP哈希策略
优点：能较好地把同一个客户端的多次请求分配到同一台服务器处理，避免了加权轮询无法适用会话保持的需求。
缺点：当某个时刻来自某个IP地址的请求特别多，那么将导致某台后端服务器的压力可能非常大，而其他后端服务器却空闲的不均衡情况、
*/

/*
 判断server 是否有效的方法是：
 1）如果server的失败次数（peers->peer[i].fails）没有达到了max_fails所设置的最大失败次数，则该server是有效的。
 2）如果server已经达到了max_fails所设置的最大失败次数，从这一时刻开始算起，在fail_timeout 所设置的时间段内， server是无效的。
 3）当server的失败次数（peers->peer[i].fails）为最大的失败次数，当距离现在的时间超过了fail_timeout 所设置的时间段， 则令peers->peer[i].fails =0，使得该server重新有效。

2.2.2.1
如果peers中所有的server都是无效的; 就会尝试去backup的数组中找一个有效的server, 如果找到， 跳转到2.2.3; 如果仍然找不到，表示此时
upstream中无server可以使用。就会清空所有peers数组中所有的失败次数的记录，使所有server都变成了有效。这样做的目的是为了防止下次再
有请求访问时，仍找不到一个有效的server.
    for (i = 0; i < peers->number; i++) {
            peers->peer[i].fails2 = 0;
    }
    并返回错误码给nginx,  nginx得到此错误码后，就不再向后台server发请求，而是在nginx的错误日志中输出“no live upstreams while connecting to upstream”
    的记录（这就是no live产生的真正原因），并直接返回给请求的客户端一个502的错误。
*/
/*ngx_http_upstream_get_round_robin_peer ngx_http_upstream_get_least_conn_peer ngx_http_upstream_get_hash_peer  
ngx_http_upstream_get_ip_hash_peer ngx_http_upstream_get_keepalive_peer等 */
ngx_int_t
ngx_http_upstream_get_round_robin_peer(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_rr_peer_data_t  *rrp = data;

    ngx_int_t                      rc;
    ngx_uint_t                     i, n;
    ngx_http_upstream_rr_peer_t   *peer;
    ngx_http_upstream_rr_peers_t  *peers;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "get rr peer, try: %ui", pc->tries);

    pc->cached = 0;
    pc->connection = NULL;

    peers = rrp->peers;
    ngx_http_upstream_rr_peers_wlock(peers);

    if (peers->single) {
        peer = peers->peer;

        if (peer->down) {  //该服务器已经失效，不能在使用该服务器了
            goto failed;
        }

        rrp->current = peer;

    } else {

        /* there are several peers */

        peer = ngx_http_upstream_get_peer(rrp);
        //get_peer函数返回优先级最大的服务器

        if (peer == NULL) {
            goto failed;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                       "get rr peer, current: %p %i",
                       peer, peer->current_weight);
    }
    
    //pc中记录了选取的服务器的信息
    pc->sockaddr = peer->sockaddr;
    pc->socklen = peer->socklen;
    pc->name = &peer->name;

    peer->conns++;

    ngx_http_upstream_rr_peers_unlock(peers);

    return NGX_OK;

failed: //选择失败，转向后备服务器

    if (peers->next) {

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, pc->log, 0, "backup servers");

        rrp->peers = peers->next;

        n = (rrp->peers->number + (8 * sizeof(uintptr_t) - 1))
                / (8 * sizeof(uintptr_t));

        for (i = 0; i < n; i++) {
             rrp->tried[i] = 0;
        }

        ngx_http_upstream_rr_peers_unlock(peers);

        rc = ngx_http_upstream_get_round_robin_peer(pc, rrp);

        if (rc != NGX_BUSY) {
            return rc;
        }

        ngx_http_upstream_rr_peers_wlock(peers);
    }

    /* all peers failed, mark them as live for quick recovery */
    /*
         表示此时upstream中无server可以使用。就会清空所有peers数组中所有的失败次数的记录，使所有server都变成了有效。这样
         做的目的是为了防止下次再有请求访问时，仍找不到一个有效的server.
     */
    for (peer = peers->peer; peer; peer = peer->next) {
        peer->fails = 0;
    }

    ngx_http_upstream_rr_peers_unlock(peers);

    pc->name = peers->name;

    return NGX_BUSY;
}

/*
   fail_timeout事件内访问后端出现错误的次数大于等于max_fails，则认为该服务器不可用，那么如果不可用了，后端该服务器有恢复了怎么判断检测呢?
   答:当这个fail_timeout时间段过了后，会重置peer->checked，那么有可以试探该服务器了，参考ngx_http_upstream_get_peer
   //checked用来检测时间，例如某个时间段fail_timeout这段时间后端失效了，那么这个fail_timeout过了后，也可以试探使用该服务器
 */ 
//get_peer函数返回优先级最大的服务器 //按照当前各服务器权值进行选择
static ngx_http_upstream_rr_peer_t *
ngx_http_upstream_get_peer(ngx_http_upstream_rr_peer_data_t *rrp)//ngx_http_upstream_get_peer和ngx_http_upstream_init_round_robin_peer配合阅读
{
    time_t                        now;
    uintptr_t                     m;
    ngx_int_t                     total;
    ngx_uint_t                    i, n, p;
    ngx_http_upstream_rr_peer_t  *peer, *best;

    now = ngx_time();

    best = NULL;
    total = 0;

#if (NGX_SUPPRESS_WARN)
    p = 0;
#endif

    for (peer = rrp->peers->peer, i = 0;
         peer;
         peer = peer->next, i++) //ngx_http_upstream_get_peer和ngx_http_upstream_init_round_robin_peer配合阅读
    {
        //计算当前服务器的标记位在位图中的位置
        n = i / (8 * sizeof(uintptr_t));
        m = (uintptr_t) 1 << i % (8 * sizeof(uintptr_t));

        if (rrp->tried[n] & m) {//已经选择过，跳过
            continue;
        }

        if (peer->down) {//当前服务器已宕机，排除
            continue;
        }

      /*
       fail_timeout事件内访问后端出现错误的次数大于等于max_fails，则认为该服务器不可用，那么如果不可用了，后端该服务器有恢复了怎么判断检测呢?
       答:当这个fail_timeout时间段过了后，会重置peer->checked，那么有可以试探该服务器了
       */
        if (peer->max_fails
            && peer->fails >= peer->max_fails
            && now - peer->checked <= peer->fail_timeout)//根据指定一段时间内最大失败次数做判断
        {
            continue;
        }
        /*
          foreach peer in peers {
            peer->current_weight += peer->effective_weight;
                total += peer->effective_weight;
             
                if (best == NULL || peer->current_weight > best->current_weight) {
                    best = peer;
            }
            }
            best->current_weight -= total;
            这个算法应该说就是毒化的加权动态优先级算法，最大的特点有两点：一是优先级current_weight的变化量是权effective_weight，二是对所选server的优先级进行大规模毒化，毒化程度是所有server的权值之和。这种算法的结果特点一定是权高的server一定先被选中，并且更频繁的被选中，而权低的server也会慢慢的提升优先级而被选中。对于上面的边界情况，这种算法得到的序列是a, a, b, a, c, a, a，均匀程度提升非常显著。
            对于我们自己的例子，这里也演算一下：
            selected server current_weight          before selected           current_weight after selected
                    a                                   { 5, 1, 2 }                 { -3, 1, 2 }
                    b                                   { 2, 2, 4 }                 { 2, 2, -4 }
                    a                                    { 7, 3, -2 }            { -1, 3, -2 }
                    a                                    { 4, 4, 0 }            { -4, 4, 0 }
                    b                                    { 1, 5, 2 }            { 1, -3, 2 }
                    a                                    { 6, -2, 4 }            { -2, -2, 4 }
                    b                                    { 3, -1, 6 }            { 3, -1, -2 }
                    a                                   { 8, 0, 0 }              { 0, 0, 0 }
            经过一轮选择以后，优先级恢复到初始状态。这个性质使得代码得以缩短。Cool!
          */
        //加权轮训算法可以参考:http://blog.sina.com.cn/s/blog_7303a1dc01014i0j.html
        peer->current_weight += peer->effective_weight;
        total += peer->effective_weight;

        if (peer->effective_weight < peer->weight) {//服务正常，effective_weight 逐渐恢复正常    
            peer->effective_weight++;
        }

        if (best == NULL || peer->current_weight > best->current_weight) {
            best = peer;
            p = i;
        }
    }

    if (best == NULL) {
        return NULL;
    }

    rrp->current = best;
    
    //所选择的服务器在服务器列表中的位置
    n = p / (8 * sizeof(uintptr_t));
    m = (uintptr_t) 1 << p % (8 * sizeof(uintptr_t));

    rrp->tried[n] |= m; //位图相应位置置位

    best->current_weight -= total;

    /*
       fail_timeout事件内访问后端出现错误的次数大于等于max_fails，则认为该服务器不可用，那么如果不可用了，后端该服务器有恢复了怎么判断检测呢?
       答:当这个fail_timeout时间段过了后，会重置peer->checked，那么有可以试探该服务器了
     */
    if (now - best->checked > best->fail_timeout) {
        best->checked = now;
    }

    return best;
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

轮询策略和IP哈希策略对比
加权轮询策略
优点：适用性更强，不依赖于客户端的任何信息，完全依靠后端服务器的情况来进行选择。能把客户端请求更合理更均匀地分配到各个后端服务器处理。
缺点：同一个客户端的多次请求可能会被分配到不同的后端服务器进行处理，无法满足做会话保持的应用的需求。

IP哈希策略
优点：能较好地把同一个客户端的多次请求分配到同一台服务器处理，避免了加权轮询无法适用会话保持的需求。
缺点：当某个时刻来自某个IP地址的请求特别多，那么将导致某台后端服务器的压力可能非常大，而其他后端服务器却空闲的不均衡情况、

*/

//ngx_http_upstream_free_round_robin_peer函数将服务器的标志字段都恢复到初始状态，以便后续使用
void
ngx_http_upstream_free_round_robin_peer(ngx_peer_connection_t *pc, void *data,
    ngx_uint_t state)
{
    ngx_http_upstream_rr_peer_data_t  *rrp = data;

    time_t                       now;
    ngx_http_upstream_rr_peer_t  *peer;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "free rr peer %ui %ui", pc->tries, state);

    /* TODO: NGX_PEER_KEEPALIVE */

    peer = rrp->current;

    ngx_http_upstream_rr_peers_rlock(rrp->peers);
    ngx_http_upstream_rr_peer_lock(rrp->peers, peer);

    if (rrp->peers->single) {

        peer->conns--;

        ngx_http_upstream_rr_peer_unlock(rrp->peers, peer);
        ngx_http_upstream_rr_peers_unlock(rrp->peers);

        pc->tries = 0;
        return;
    }

    if (state & NGX_PEER_FAILED) { //从ngx_http_upstream_next走到这里
        now = ngx_time();

        peer->fails++;
        peer->accessed = now; //选取的后端服务器异常则跟新accessed时间为当前选取后端服务器的时候检测到异常的时间
        peer->checked = now;

        if (peer->max_fails) {//服务发生异常时，调低effective_weight
            peer->effective_weight -= peer->weight / peer->max_fails;//服务发生异常时，调低effective_weight

            if (peer->fails >= peer->max_fails) { //超过允许的最大失败次数
                ngx_log_error(NGX_LOG_WARN, pc->log, 0,
                              "upstream server temporarily disabled");
            }
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                       "free rr peer failed: %p %i",
                       peer, peer->effective_weight);

        if (peer->effective_weight < 0) {
            peer->effective_weight = 0;
        }

    } else {

        /* mark peer live if check passed */
        //一个fail_timeout时间段到了，则跟新checeked为当前时间
        if (peer->accessed < peer->checked) { 
        //从获取后端失败并且判断达到最大上限失败次数，则会复活该后端服务器，同时再次选择该服务器，如果该服务器又有效了，则会设置checked为当前时间
        //这时候peer->accessed < peer->checked满足条件，把fails清0
            peer->fails = 0;
        }
    }

    peer->conns--;

    ngx_http_upstream_rr_peer_unlock(rrp->peers, peer);
    ngx_http_upstream_rr_peers_unlock(rrp->peers);

    if (pc->tries) {
        pc->tries--;
    }
}

#if (NGX_HTTP_SSL)

ngx_int_t
ngx_http_upstream_set_round_robin_peer_session(ngx_peer_connection_t *pc,
    void *data)
{
    ngx_http_upstream_rr_peer_data_t  *rrp = data;

    ngx_int_t                      rc;
    ngx_ssl_session_t             *ssl_session;
    ngx_http_upstream_rr_peer_t   *peer;
#if (NGX_HTTP_UPSTREAM_ZONE)
    int                            len;
#if OPENSSL_VERSION_NUMBER >= 0x0090707fL
    const
#endif
    u_char                        *p;
    ngx_http_upstream_rr_peers_t  *peers;
    u_char                         buf[NGX_SSL_MAX_SESSION_SIZE];
#endif

    peer = rrp->current;

#if (NGX_HTTP_UPSTREAM_ZONE)
    peers = rrp->peers;

    if (peers->shpool) {
        ngx_http_upstream_rr_peers_rlock(peers);
        ngx_http_upstream_rr_peer_lock(peers, peer);

        if (peer->ssl_session == NULL) {
            ngx_http_upstream_rr_peer_unlock(peers, peer);
            ngx_http_upstream_rr_peers_unlock(peers);
            return NGX_OK;
        }

        len = peer->ssl_session_len;

        ngx_memcpy(buf, peer->ssl_session, len);

        ngx_http_upstream_rr_peer_unlock(peers, peer);
        ngx_http_upstream_rr_peers_unlock(peers);

        p = buf;
        ssl_session = d2i_SSL_SESSION(NULL, &p, len);

        rc = ngx_ssl_set_session(pc->connection, ssl_session);

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                       "set session: %p", ssl_session);

        ngx_ssl_free_session(ssl_session);

        return rc;
    }
#endif

    ssl_session = peer->ssl_session;

    rc = ngx_ssl_set_session(pc->connection, ssl_session);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "set session: %p", ssl_session);

    return rc;
}


void
ngx_http_upstream_save_round_robin_peer_session(ngx_peer_connection_t *pc,
    void *data)
{
    ngx_http_upstream_rr_peer_data_t  *rrp = data;

    ngx_ssl_session_t             *old_ssl_session, *ssl_session;
    ngx_http_upstream_rr_peer_t   *peer;
#if (NGX_HTTP_UPSTREAM_ZONE)
    int                            len;
    u_char                        *p;
    ngx_http_upstream_rr_peers_t  *peers;
    u_char                         buf[NGX_SSL_MAX_SESSION_SIZE];
#endif

#if (NGX_HTTP_UPSTREAM_ZONE)
    peers = rrp->peers;

    if (peers->shpool) {

        ssl_session = SSL_get0_session(pc->connection->ssl->connection);

        if (ssl_session == NULL) {
            return;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                       "save session: %p", ssl_session);

        len = i2d_SSL_SESSION(ssl_session, NULL);

        /* do not cache too big session */

        if (len > NGX_SSL_MAX_SESSION_SIZE) {
            return;
        }

        p = buf;
        (void) i2d_SSL_SESSION(ssl_session, &p);

        peer = rrp->current;

        ngx_http_upstream_rr_peers_rlock(peers);
        ngx_http_upstream_rr_peer_lock(peers, peer);

        if (len > peer->ssl_session_len) {
            ngx_shmtx_lock(&peers->shpool->mutex);

            if (peer->ssl_session) {
                ngx_slab_free_locked(peers->shpool, peer->ssl_session);
            }

            peer->ssl_session = ngx_slab_alloc_locked(peers->shpool, len);

            ngx_shmtx_unlock(&peers->shpool->mutex);

            if (peer->ssl_session == NULL) {
                peer->ssl_session_len = 0;

                ngx_http_upstream_rr_peer_unlock(peers, peer);
                ngx_http_upstream_rr_peers_unlock(peers);
                return;
            }

            peer->ssl_session_len = len;
        }

        ngx_memcpy(peer->ssl_session, buf, len);

        ngx_http_upstream_rr_peer_unlock(peers, peer);
        ngx_http_upstream_rr_peers_unlock(peers);

        return;
    }
#endif

    ssl_session = ngx_ssl_get_session(pc->connection);

    if (ssl_session == NULL) {
        return;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "save session: %p", ssl_session);

    peer = rrp->current;

    old_ssl_session = peer->ssl_session;
    peer->ssl_session = ssl_session;

    if (old_ssl_session) {

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                       "old session: %p", old_ssl_session);

        /* TODO: may block */

        ngx_ssl_free_session(old_ssl_session);
    }
}


static ngx_int_t
ngx_http_upstream_empty_set_session(ngx_peer_connection_t *pc, void *data)
{
    return NGX_OK;
}


static void
ngx_http_upstream_empty_save_session(ngx_peer_connection_t *pc, void *data)
{
    return;
}

#endif
