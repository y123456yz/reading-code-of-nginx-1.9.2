
/*
 * Copyright (C) Maxim Dounin
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    //最大缓存连接个数，由keepalive参数指定（keepalive connection）  默认0，不开启keepalive con-num设置
    ngx_uint_t                         max_cached; //keepalive的第一个参数  开辟max_cached个ngx_http_upstream_keepalive_cache_t

    /*
    //长连接队列，其中cache为缓存连接池，free为空闲连接池。初始化时根据keepalive指令的参数初始化free队列，后续有连接过来从free队列
    //取连接，请求处理结束后将长连接缓存到cache队列，连接被断开（或超时）再从cache队列放入free队列

    //ngx_http_upstream_free_keepalive_peer往kp->conf->cache中添加缓存ngx_connection_t，ngx_http_upstream_get_keepalive_peer从缓存中
    //取出和后端的连接缓存ngx_connection_t，可以避免重复的建立和关闭TCP连接
     */
    ngx_queue_t                        cache; // keepalive的第二个参数
    ngx_queue_t                        free; //初始化创建的max_cached个ngx_http_upstream_keepalive_cache_t会添加到该free队列中

    /*keepalive module是配合hash  ip_hash least_conn使用的，而且在这些模块之后，original_init_upstream和original_init_peer指向这
    几个模块的uscf->peer.init_upstream或者uscf->peer.init*/
    ngx_http_upstream_init_pt          original_init_upstream;
    ngx_http_upstream_init_peer_pt     original_init_peer;

} ngx_http_upstream_keepalive_srv_conf_t;//这个是upstream里的keepalive指令真正填充的数据结构。


typedef struct {//ngx_http_upstream_init_keepalive中开辟max_cached(keepalive设置)个空间
    ngx_http_upstream_keepalive_srv_conf_t  *conf;

    ngx_queue_t                        queue;
    ngx_connection_t                  *connection;

     //缓存连接池中保存的后端服务器的地址，后续就是根据相同的socket地址来找出对应的连接，并使用该连接
    socklen_t                          socklen;
    u_char                             sockaddr[NGX_SOCKADDRLEN];

} ngx_http_upstream_keepalive_cache_t;


typedef struct { //ngx_http_upstream_init_keepalive_peer中开辟空间
    ngx_http_upstream_keepalive_srv_conf_t  *conf;//指向ngx_http_upstream_keepalive_srv_conf_t

    ngx_http_upstream_t               *upstream; //r->upstream

    void                              *data;

    /*
    //保存原始获取peer和释放peer的钩子，它们通常是ngx_http_upstream_get_round_robin_peer和ngx_http_upstream_free_round_robin_peer，
    nginx负载均衡默认是使用轮询算法
     */
     /*keepalive module是配合hash  ip_hash least_conn使用的，而且在这些模块之后，original_init_upstream和original_init_peer指向这
    几个模块的get和free*/
    ngx_event_get_peer_pt              original_get_peer;
    ngx_event_free_peer_pt             original_free_peer;

#if (NGX_HTTP_SSL)
    ngx_event_set_peer_session_pt      original_set_session;
    ngx_event_save_peer_session_pt     original_save_session;
#endif

} ngx_http_upstream_keepalive_peer_data_t;


static ngx_int_t ngx_http_upstream_init_keepalive_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);
static ngx_int_t ngx_http_upstream_get_keepalive_peer(ngx_peer_connection_t *pc,
    void *data);
static void ngx_http_upstream_free_keepalive_peer(ngx_peer_connection_t *pc,
    void *data, ngx_uint_t state);

static void ngx_http_upstream_keepalive_dummy_handler(ngx_event_t *ev);
static void ngx_http_upstream_keepalive_close_handler(ngx_event_t *ev);
static void ngx_http_upstream_keepalive_close(ngx_connection_t *c);

#if (NGX_HTTP_SSL)
static ngx_int_t ngx_http_upstream_keepalive_set_session(
    ngx_peer_connection_t *pc, void *data);
static void ngx_http_upstream_keepalive_save_session(ngx_peer_connection_t *pc,
    void *data);
#endif

static void *ngx_http_upstream_keepalive_create_conf(ngx_conf_t *cf);
static char *ngx_http_upstream_keepalive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

//指定可用于长连接的连接数
static ngx_command_t  ngx_http_upstream_keepalive_commands[] = {
    // 默认0，不开启keepalive con-num设置
    { ngx_string("keepalive"), //缓存多少个长连接，一般和后端有多少个服务器这里就配置为多少，这个nginx可以与每一个后端只建立一个TCP连接
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE1,
      ngx_http_upstream_keepalive,   //keepalive connections
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_upstream_keepalive_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_http_upstream_keepalive_create_conf, /* create server configuration */
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
如果要实现upstream长连接，则每个进程需要另外一个connection pool，里面都是长连接。一旦与后端服务器建立连接，则在当前请求连接结
束之后不会立即关闭连接，而是把用完的连接保存在一个keepalive connection pool里面，以后每次需要建立向后连接的时候，只需要从这个
连接池里面找，如果找到合适的连接的话，就可以直接来用这个连接，不需要重新创建socket或者发起connect()。这样既省下建立连接时在握
手的时间消耗，又可以避免TCP连接的slow start。如果在keepalive连接池找不到合适的连接，那就按照原来的步骤重新建立连接。假设连接查
找时间可以忽略不计，那么这种方法肯定是有益而无害的（当然，需要少量额外的内存）。
*/ 
//如果不配置keepalive  num，则有多少个客户端请求到后端，nginx就会和后端建立多少个tcp连接，如果加了该参数则会把num个connect缓存
//下来，下次有客户端请求就直接使用缓存中的连接，避免不停建立TCP连接
ngx_module_t  ngx_http_upstream_keepalive_module = {
    NGX_MODULE_V1,
    &ngx_http_upstream_keepalive_module_ctx, /* module context */
    ngx_http_upstream_keepalive_commands,    /* module directives */
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


static ngx_int_t
ngx_http_upstream_init_keepalive(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *us)
{
    ngx_uint_t                               i;
    ngx_http_upstream_keepalive_srv_conf_t  *kcf;
    ngx_http_upstream_keepalive_cache_t     *cached;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0,
                   "init keepalive");

    kcf = ngx_http_conf_upstream_srv_conf(us,
                                          ngx_http_upstream_keepalive_module);

    // 先执行原始初始化upstream函数（即ngx_http_upstream_init_round_robin），该函数会根据配置的后端地址解析成socket地址，用
    //于连接后端。并设置us->peer.init钩子为ngx_http_upstream_init_round_robin_peer
    if (kcf->original_init_upstream(cf, us) != NGX_OK) { //默认ngx_http_upstream_init_round_robin
        return NGX_ERROR;
    }

    // 保存原钩子，并用keepalive的钩子覆盖旧钩子，初始化后端请求的时候会调用这个新钩子
    kcf->original_init_peer = us->peer.init;

    us->peer.init = ngx_http_upstream_init_keepalive_peer;

    /* allocate cache items and add to free queue */
     /* 申请缓存项，并添加到free队列中，后续用从free队列里面取 */
    cached = ngx_pcalloc(cf->pool,
                sizeof(ngx_http_upstream_keepalive_cache_t) * kcf->max_cached);
    if (cached == NULL) {
        return NGX_ERROR;
    }

    ngx_queue_init(&kcf->cache);
    ngx_queue_init(&kcf->free);

    /*
    先预创建max_cached个后端连接信息ngx_http_upstream_keepalive_cache_t，，后续有连接过来从free队列
    取连接，请求处理结束后将长连接缓存到cache队列，连接被断开（或超时）再从cache队列放入free队列
    */
    for (i = 0; i < kcf->max_cached; i++) {
        ngx_queue_insert_head(&kcf->free, &cached[i].queue);
        cached[i].conf = kcf;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_init_keepalive_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us)
{
    ngx_http_upstream_keepalive_peer_data_t  *kp;
    ngx_http_upstream_keepalive_srv_conf_t   *kcf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "init keepalive peer");

    kcf = ngx_http_conf_upstream_srv_conf(us,
                                          ngx_http_upstream_keepalive_module);

    kp = ngx_palloc(r->pool, sizeof(ngx_http_upstream_keepalive_peer_data_t));
    if (kp == NULL) {
        return NGX_ERROR;
    }

    /* 
    先执行原始的初始化peer函数，即ngx_http_upstream_init_round_robin_peer。该
    函数内部处理一些与负载均衡相关的操作并分别设置以下四个钩子：
    r->upstream->peer.get和 r->upstream->peer.free
    r->upstream->peer.set_session和 r->upstream->peer.save_session 
    */
    if (kcf->original_init_peer(r, us) != NGX_OK) {
        return NGX_ERROR;
    }

    kp->conf = kcf;
    kp->upstream = r->upstream;

     // keepalive模块则保存上述原始钩子，并使用新的各类钩子覆盖旧钩子
    kp->data = r->upstream->peer.data;
    kp->original_get_peer = r->upstream->peer.get;
    kp->original_free_peer = r->upstream->peer.free;

    r->upstream->peer.data = kp;
    r->upstream->peer.get = ngx_http_upstream_get_keepalive_peer;
    r->upstream->peer.free = ngx_http_upstream_free_keepalive_peer;

#if (NGX_HTTP_SSL)
    kp->original_set_session = r->upstream->peer.set_session;
    kp->original_save_session = r->upstream->peer.save_session;
    r->upstream->peer.set_session = ngx_http_upstream_keepalive_set_session;
    r->upstream->peer.save_session = ngx_http_upstream_keepalive_save_session;
#endif

    return NGX_OK;
}

/*
//ngx_event_connect_peer调用pc->get钩子。如前面所述，若是keepalive upstream，则该钩子是
//ngx_http_upstream_get_keepalive_peer，此时如果存在缓存长连接该函数调用返回的是
//NGX_DONE，直接返回上层调用而不会继续往下执行获取新的连接并创建socket，
//如果不存在缓存的长连接，则会返回NGX_OK.
//若是非keepalive upstream，该钩子是ngx_http_upstream_get_round_robin_peer。
*/

//ngx_http_upstream_free_keepalive_peer往kp->conf->cache中添加缓存ngx_connection_t，ngx_http_upstream_get_keepalive_peer从缓存中
//取出和后端的连接缓存ngx_connection_t，可以避免重复的建立和关闭TCP连接

/*ngx_http_upstream_get_round_robin_peer ngx_http_upstream_get_least_conn_peer ngx_http_upstream_get_hash_peer  
ngx_http_upstream_get_ip_hash_peer ngx_http_upstream_get_keepalive_peer等 */
static ngx_int_t
ngx_http_upstream_get_keepalive_peer(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_keepalive_peer_data_t  *kp = data;
    ngx_http_upstream_keepalive_cache_t      *item;

    ngx_int_t          rc;
    ngx_queue_t       *q, *cache;
    ngx_connection_t  *c;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "get keepalive peer");

    /* ask balancer */
    // 先调用原始getpeer钩子（ngx_http_upstream_get_round_robin_peer）选择后端
    rc = kp->original_get_peer(pc, kp->data); //通过hash  ip_hash rr方式选择后端的服务器peer

    if (rc != NGX_OK) {
        return rc;
    }

    /* 已经选定应该把请求发往后端某个节点，然后下面就选择和这个节点的某个已有的长连接来发送数据 */
    
    /* search cache for suitable connection */

    cache = &kp->conf->cache;
    // 根据socket地址查找连接cache池，找到直接返回NGX_DONE，上层调用就不会获取新的连接
    for (q = ngx_queue_head(cache);
         q != ngx_queue_sentinel(cache);
         q = ngx_queue_next(q))
    {
        item = ngx_queue_data(q, ngx_http_upstream_keepalive_cache_t, queue);
        c = item->connection;

        if (ngx_memn2cmp((u_char *) &item->sockaddr, (u_char *) pc->sockaddr,
                         item->socklen, pc->socklen)
            == 0)
        {
            ngx_queue_remove(q);
            ngx_queue_insert_head(&kp->conf->free, q);

            goto found;
        }
    }

    return NGX_OK;

found:

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "get keepalive peer: using connection %p", c);

    c->idle = 0;
    c->sent = 0;
    c->log = pc->log;
    c->read->log = pc->log;
    c->write->log = pc->log;
    c->pool->log = pc->log;

    pc->connection = c;
    pc->cached = 1;

    return NGX_DONE;
}
/*
分析完ngx_http_upstream_free_keepalive_peer函数后，在回过头去看ngx_http_upstream_get_keepalive_peer就更能理解是如何复用keepalive连接的，
free操作将当前连接缓存到cache队列中，并保存该连接对应后端的socket地址，get操作根据想要连接后端的socket地址，遍历查找cache队列，如果找到
就使用先前缓存的长连接，未找到就重新建立新的连接。

当free操作发现当前所有cache item用完时（即缓存连接达到上限），会关闭最近未被使用的那个连接，用来缓存新的连接。Nginx官方推荐keepalive的
连接数应该配置的尽可能小，以免出现被缓存的连接太多而造成新的连接请求过来时无法获取连接的情况（一个worker进程的总连接池是有限的）。
*/
//在一个HTTP请求处理完毕后，通常会调用ngx_http_upstream_finalize_request结束请求，并调用释放peer的操作
//该钩子会将连接缓存到长连接cache池，并将u->peer.connection设置成空，防止ngx_http_upstream_finalize_request最下面部分代码关闭连接。

//ngx_http_upstream_free_keepalive_peer往kp->conf->cache中添加缓存ngx_connection_t，ngx_http_upstream_get_keepalive_peer从缓存中
//取出和后端的连接缓存ngx_connection_t，可以避免重复的建立和关闭TCP连接
static void
ngx_http_upstream_free_keepalive_peer(ngx_peer_connection_t *pc, void *data,
    ngx_uint_t state)
{
    ngx_http_upstream_keepalive_peer_data_t  *kp = data;
    ngx_http_upstream_keepalive_cache_t      *item;

    ngx_queue_t          *q;
    ngx_connection_t     *c;
    ngx_http_upstream_t  *u;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "free keepalive peer");

    /* cache valid connections */

    u = kp->upstream;
    c = pc->connection;

    if (state & NGX_PEER_FAILED
        || c == NULL
        || c->read->eof
        || c->read->error
        || c->read->timedout
        || c->write->error
        || c->write->timedout)
    {
        goto invalid;
    }

    if (!u->keepalive) { 
    //说明本次和后端的连接使用的是缓存cache(keepalive配置)connection的TCP连接，也就是使用的是之前已经和后端建立好的TCP连接ngx_connection_t
        goto invalid;
    }

    //通常设置keepalive后连接都是由后端web服务发起的，因此需要添加读事件
    if (ngx_handle_read_event(c->read, 0, NGX_FUNC_LINE) != NGX_OK) {
        goto invalid;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "free keepalive peer: saving connection %p", c);

    //如果free队列中可用cache items为空，则从cache队列取一个最近最少使用item，
    //将该item对应的那个连接关闭，该item用于保存当前需要释放的连接
    if (ngx_queue_empty(&kp->conf->free)) { //free中已经没有节点信息了，因此把cache中最少使用的那个取出来，把该连接关闭，重新添加到cache

        q = ngx_queue_last(&kp->conf->cache);
        ngx_queue_remove(q);

        item = ngx_queue_data(q, ngx_http_upstream_keepalive_cache_t, queue);

        ngx_http_upstream_keepalive_close(item->connection);

    } else {
         //free队列不空则直接从队列头取一个item用于保存当前连接
        q = ngx_queue_head(&kp->conf->free);
        ngx_queue_remove(q);

        item = ngx_queue_data(q, ngx_http_upstream_keepalive_cache_t, queue);
    }

    ngx_queue_insert_head(&kp->conf->cache, q);
    
    //缓存当前连接，将item插入cache队列，然后将pc->connection置空，防止上层调用
    //ngx_http_upstream_finalize_request关闭该连接（详见该函数）
    item->connection = c;

    pc->connection = NULL;

    //关闭读写定时器，这样可以避免把后端服务器的tcp连接关闭掉
    if (c->read->timer_set) {
        ngx_del_timer(c->read, NGX_FUNC_LINE);
    }
    if (c->write->timer_set) {
        ngx_del_timer(c->write, NGX_FUNC_LINE);
    }

    
    //设置连接读写钩子。写钩子是一个假钩子（keepalive连接不会由客户端主动关闭）
    //读钩子处理关闭keepalive连接的操作（接收到来自后端web服务器的FIN分节）
    c->write->handler = ngx_http_upstream_keepalive_dummy_handler;
    c->read->handler = ngx_http_upstream_keepalive_close_handler;

    c->data = item;
    c->idle = 1;
    c->log = ngx_cycle->log;
    c->read->log = ngx_cycle->log;
    c->write->log = ngx_cycle->log;
    c->pool->log = ngx_cycle->log;

    // 保存socket地址相关信息，后续就是通过查找相同的socket地址来复用该连接
    item->socklen = pc->socklen;
    ngx_memcpy(&item->sockaddr, pc->sockaddr, pc->socklen);

    if (c->read->ready) {
        ngx_http_upstream_keepalive_close_handler(c->read);
    }

invalid:

    kp->original_free_peer(pc, kp->data, state); //指向原负载均衡算法对应的free
}


static void
ngx_http_upstream_keepalive_dummy_handler(ngx_event_t *ev)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                   "keepalive dummy handler");
}


static void
ngx_http_upstream_keepalive_close_handler(ngx_event_t *ev)
{
    ngx_http_upstream_keepalive_srv_conf_t  *conf;
    ngx_http_upstream_keepalive_cache_t     *item;

    int                n;
    char               buf[1];
    ngx_connection_t  *c;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                   "keepalive close handler");

    c = ev->data;

    if (c->close) {
        goto close;
    }

    n = recv(c->fd, buf, 1, MSG_PEEK);

    if (n == -1 && ngx_socket_errno == NGX_EAGAIN) {
        ev->ready = 0;

        if (ngx_handle_read_event(c->read, 0, NGX_FUNC_LINE) != NGX_OK) {
            goto close;
        }

        return;
    }

close:

    item = c->data;
    conf = item->conf;

    ngx_http_upstream_keepalive_close(c);

    ngx_queue_remove(&item->queue);
    ngx_queue_insert_head(&conf->free, &item->queue);
}


static void
ngx_http_upstream_keepalive_close(ngx_connection_t *c)
{

#if (NGX_HTTP_SSL)

    if (c->ssl) {
        c->ssl->no_wait_shutdown = 1;
        c->ssl->no_send_shutdown = 1;

        if (ngx_ssl_shutdown(c) == NGX_AGAIN) {
            c->ssl->handler = ngx_http_upstream_keepalive_close;
            return;
        }
    }

#endif

    ngx_destroy_pool(c->pool);
    ngx_close_connection(c);
}


#if (NGX_HTTP_SSL)

static ngx_int_t
ngx_http_upstream_keepalive_set_session(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_keepalive_peer_data_t  *kp = data;

    return kp->original_set_session(pc, kp->data);
}


static void
ngx_http_upstream_keepalive_save_session(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_keepalive_peer_data_t  *kp = data;

    kp->original_save_session(pc, kp->data);
    return;
}

#endif


static void *
ngx_http_upstream_keepalive_create_conf(ngx_conf_t *cf)
{
    ngx_http_upstream_keepalive_srv_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool,
                       sizeof(ngx_http_upstream_keepalive_srv_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->original_init_upstream = NULL;
     *     conf->original_init_peer = NULL;
     *     conf->max_cached = 0;
     */

    return conf;
}

//keepalive connections
static char *
ngx_http_upstream_keepalive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_upstream_srv_conf_t            *uscf;
    ngx_http_upstream_keepalive_srv_conf_t  *kcf = conf;

    ngx_int_t    n;
    ngx_str_t   *value;

    if (kcf->max_cached) {
        return "is duplicate";
    }

    /* read options */

    value = cf->args->elts;

    n = ngx_atoi(value[1].data, value[1].len);

    if (n == NGX_ERROR || n == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid value \"%V\" in \"%V\" directive",
                           &value[1], &cmd->name);
        return NGX_CONF_ERROR;
    }

    kcf->max_cached = n;

    uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);

    /*
    When using load balancer methods other than the default round-robin method, it is necessary to activate them before the keepalive directive.
     */

    /*
    &ngx_http_upstream_hash_module,
    &ngx_http_upstream_ip_hash_module,
    &ngx_http_upstream_least_conn_module,
    &ngx_http_upstream_keepalive_module,  keepalive module是配合hash  ip_hash least_conn使用的，而且在这些模块之后，original_init_upstream指向这几个模块的init_upstream
    */
  
    /* 保存原来的初始化upstream的钩子，并设置新的钩子 */ //这个是赋值均衡算法的一些初始化钩子用original_init_upstream保存
    kcf->original_init_upstream = uscf->peer.init_upstream
                                  ? uscf->peer.init_upstream
                                  : ngx_http_upstream_init_round_robin;

    uscf->peer.init_upstream = ngx_http_upstream_init_keepalive; //原始的负债均衡钩子保存在kcf->original_init_upstream

    return NGX_CONF_OK;
}
