
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct { //创建及初始化赋值见ngx_http_upstream_init_ip_hash_peer
    /* the round robin data must be first */ 
    //必须是第一个成员，参考ngx_http_upstream_get_ip_hash_peer
    ngx_http_upstream_rr_peer_data_t   rrp;//r->upstream->peer.data = &iphp->rrp; ngx_http_upstream_init_round_robin_peer中赋值

    ngx_uint_t                         hash; //初始化为89，见ngx_http_upstream_init_ip_hash_peer

    u_char                             addrlen;//iphp->addrlen = 3;//转储IPv4只用到了前3个字节，因为在后面的hash计算过程中只用到了3个字节  
    u_char                            *addr; //客户端IP地址iphp->addr = (u_char *) &sin->sin_addr.s_addr;

    u_char                             tries;//尝试连接的次数   最大失败次数20次，超过了则直接使用rr获取peer

    ngx_event_get_peer_pt              get_rr_peer;//ngx_http_upstream_get_round_robin_peer
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

/*
负载均衡相关配置:
upstream
server
ip_hash:根据客户端的IP来做hash,不过如果squid -- nginx -- server(s)则，ip永远是squid服务器ip,因此不管用,需要ngx_http_realip_module或者第三方模块
keepalive:配置到后端的最大连接数，保持长连接，不必为每一个客户端都重新建立nginx到后端PHP等服务器的连接，需要保持和后端
    长连接，例如fastcgi:fastcgi_keep_conn on;       proxy:  proxy_http_version 1.1;  proxy_set_header Connection "";
least_conn:根据其权重值，将请求发送到活跃连接数最少的那台服务器
hash:可以按照uri  ip 等参数进行做hash

参考http://tengine.taobao.org/nginx_docs/cn/docs/http/ngx_http_upstream_module.html#ip_hash
*/


/*
ip_hash做负载均衡问题:
squid -- nginx -- server(s)
    前端使用squid做缓存，后端用多台服务器，但多台服务器间的SESSION不共享，为了做负载均衡，使用nginx的ip_hash来做，使得来源机器的会话是持续的。
于是便引起来了一个问题，使用nginx的ip_hash规则来做负载均衡时，得到的IP则始终是squid机器的IP，于是负载均衡便失效了。
解决办法:参考http://bbs.chinaunix.net/thread-1985674-1-1.html
ngx_http_realip_module
*/
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

轮询策略和IP哈希策略对比
加权轮询策略
优点：适用性更强，不依赖于客户端的任何信息，完全依靠后端服务器的情况来进行选择。能把客户端请求更合理更均匀地分配到各个后端服务器处理。
缺点：同一个客户端的多次请求可能会被分配到不同的后端服务器进行处理，无法满足做会话保持的应用的需求。

IP哈希策略
优点：能较好地把同一个客户端的多次请求分配到同一台服务器处理，避免了加权轮询无法适用会话保持的需求。
缺点：当某个时刻来自某个IP地址的请求特别多，那么将导致某台后端服务器的压力可能非常大，而其他后端服务器却空闲的不均衡情况、

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
    //调用了RR算法中的初始化函数
    if (ngx_http_upstream_init_round_robin_peer(r, us) != NGX_OK) {
        return NGX_ERROR;
    }

    r->upstream->peer.get = ngx_http_upstream_get_ip_hash_peer;  

    switch (r->connection->sockaddr->sa_family) {

    case AF_INET:
        sin = (struct sockaddr_in *) r->connection->sockaddr;
        iphp->addr = (u_char *) &sin->sin_addr.s_addr;
        iphp->addrlen = 3;//转储IPv4只用到了前3个字节，因为在后面的hash计算过程中只用到了3个字节  
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

轮询策略和IP哈希策略对比
加权轮询策略
优点：适用性更强，不依赖于客户端的任何信息，完全依靠后端服务器的情况来进行选择。能把客户端请求更合理更均匀地分配到各个后端服务器处理。
缺点：同一个客户端的多次请求可能会被分配到不同的后端服务器进行处理，无法满足做会话保持的应用的需求。

IP哈希策略
优点：能较好地把同一个客户端的多次请求分配到同一台服务器处理，避免了加权轮询无法适用会话保持的需求。
缺点：当某个时刻来自某个IP地址的请求特别多，那么将导致某台后端服务器的压力可能非常大，而其他后端服务器却空闲的不均衡情况、

*/
/*ngx_http_upstream_get_round_robin_peer ngx_http_upstream_get_least_conn_peer ngx_http_upstream_get_hash_peer  
ngx_http_upstream_get_ip_hash_peer ngx_http_upstream_get_keepalive_peer等 */
static ngx_int_t
ngx_http_upstream_get_ip_hash_peer(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_ip_hash_peer_data_t  *iphp = data; 
    //本来data指向的是ngx_http_upstream_ip_hash_peer_data_t->rrp,因为rrp是该结构中的第一个成员，因此也就直接可以获取该结构，所以rrp必须是第一个成员

    time_t                        now;
    ngx_int_t                     w;
    uintptr_t                     m;
    ngx_uint_t                    i, n, p, hash;
    ngx_http_upstream_rr_peer_t  *peer;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "get ip hash peer, try: %ui", pc->tries);

    /* TODO: cached */

    ngx_http_upstream_rr_peers_wlock(iphp->rrp.peers);

    //如果失败次数太多，或者只有一个后端服务，那么直接做RR选择
    if (iphp->tries > 20 || iphp->rrp.peers->single) {
        ngx_http_upstream_rr_peers_unlock(iphp->rrp.peers);
        return iphp->get_rr_peer(pc, &iphp->rrp);
    }

    now = ngx_time();

    pc->cached = 0;
    pc->connection = NULL;

    hash = iphp->hash;

    for ( ;; ) {
        //计算IP的hash值 
        /*
        1）由IP计算哈希值的算法如下， 其中公式中hash初始值为89，iphp->addr[i]表示客户端的IP， 通过三次哈希计算得出一个IP的哈希值：
            for (i = 0; i < 3; i++) {
                  hash = (hash * 113 + iphp->addr[i]) % 6271;
            }
         
        2）在选择下一个server时，ip_hash的选择策略是这样的：
           它在上一次哈希值的基础上，再次哈希，就会得到一个全新的哈希值，再根据哈希值选择另外一个后台的服务器。
            哈希算法仍然是
            for (i = 0; i < 3; i++) {
                  hash = (hash * 113 + iphp->addr[i]) % 6271;
            }
          */
        for (i = 0; i < (ngx_uint_t) iphp->addrlen; i++) { //iphp->hash默认89，如果是同一个客户端来的请求，则下面计算出的hash肯定相同
            //113质数，可以让哈希结果更散列  
            hash = (hash * 113 + iphp->addr[i]) % 6271; //根据IP地址的前三位和
        }

        w = hash % iphp->rrp.peers->total_weight;
        peer = iphp->rrp.peers->peer;
        p = 0;

        //根据哈希结果得到被选中的后端服务器  
        while (w >= peer->weight) {
            w -= peer->weight;
            peer = peer->next;
            p++;
        }

        //服务器对应在位图中的位置计算  
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

/*
   fail_timeout时间内访问后端出现错误的次数大于等于max_fails，则认为该服务器不可用，那么如果不可用了，后端该服务器有恢复了怎么判断检测呢?
   答:当这个fail_timeout时间段过了后，会重置peer->checked，那么有可以试探该服务器了，参考ngx_http_upstream_get_peer
   //checked用来检测时间，例如某个时间段fail_timeout这段时间后端失效了，那么这个fail_timeout过了后，也可以试探使用该服务器
 */ 
        if (peer->max_fails
            && peer->fails >= peer->max_fails
            && now - peer->checked <= peer->fail_timeout) //失败次数已达上限  
        {
            goto next;
        }

        break;

    next:

        /*
            在这种ip_hash策略，如果一个后台服务器不能提供提服务（连接超时或读超时），该服务器的失败次数就会加一，当一个服务器的失败次
        数达到max_fails所设置的值，就会在fail_timeout所设置的时间段内不能对外提供服务，这点和RR是一致的。
            如果当前server不能提供服务，就会根据当前的哈希值再哈希出一个新哈希值，选择另一个服务器继续尝试，尝试的最大次是upstream中
        server的个数，如果server的个数超过20，也就是要最大尝试次数在20次以上，当尝试次数达到20次，仍然找不到一个合适的服务器，
        ip_hah策略不再尝试ip哈希值来选择server, 而在剩余的尝试中，它会转而使用RR的策略，使用轮循的方法，选择新的server。
          */
        if (++iphp->tries > 20) {//已经尝试了20个后端服务器都还没找到一个可用的服务器，则直接在剩余的服务器中采用轮询算法
            ngx_http_upstream_rr_peers_unlock(iphp->rrp.peers);
            return iphp->get_rr_peer(pc, &iphp->rrp);
        }
    }

    //当前服务索引  
    iphp->rrp.current = peer;

    //服务器地址及名字保存
    pc->sockaddr = peer->sockaddr;
    pc->socklen = peer->socklen;
    pc->name = &peer->name;

    peer->conns++;

    if (now - peer->checked > peer->fail_timeout) {
        peer->checked = now;
    }

    ngx_http_upstream_rr_peers_unlock(iphp->rrp.peers);

    iphp->rrp.tried[n] |= m; //位图更新   
    iphp->hash = hash;//保留种子，使下次get_ip_hash_peer的时候能够选到同一个peer上  

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
