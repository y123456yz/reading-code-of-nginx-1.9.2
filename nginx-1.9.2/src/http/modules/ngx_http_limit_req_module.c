
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/*
size = offsetof(ngx_rbtree_node_t, color)
           + offsetof(ngx_http_limit_req_node_t, data)
           + key->len;
红黑树分配空间大小，见ngx_http_limit_req_lookup
*/
typedef struct {
    u_char                       color;
    u_char                       dummy;
    u_short                      len;
    ngx_queue_t                  queue;
    ngx_msec_t                   last;
    /* integer value, 1 corresponds to 0.001 r/s */
    ngx_uint_t                   excess;
    ngx_uint_t                   count;
    u_char                       data[1];
} ngx_http_limit_req_node_t; //用于限速的该客户端一些状态存入这里面，例如该客户端当前的excess等，通过红黑树管理


typedef struct { //所有客户端请求的限速处理，可以参考ngx_http_limit_req_lookup，每个客户端对应一个红黑树节点，用与存储每次请求后用于限速的各种临时信息
    ngx_rbtree_t                  rbtree;
    ngx_rbtree_node_t             sentinel;
    ngx_queue_t                   queue;
} ngx_http_limit_req_shctx_t;


typedef struct { //创建空间和赋值见ngx_http_limit_req_init_zone
    ngx_http_limit_req_shctx_t  *sh;
    ngx_slab_pool_t             *shpool; //用来管理limit req zone的共享内存块
    /* integer value, 1 corresponds to 0.001 r/s */
    ngx_uint_t                   rate; //rate实际上扩大了1000倍，例如1r/s，则这里为1000
    //limit_req_zone  $binary_remote_addr  zone=req_one:10m rate=3000r/s;中的$binary_remote_addr对应的客户端地址
    ngx_http_complex_value_t     key; 
    ngx_http_limit_req_node_t   *node;
} ngx_http_limit_req_ctx_t;


typedef struct {
    ngx_shm_zone_t              *shm_zone;
    /* integer value, 1 corresponds to 0.001 r/s */
    ngx_uint_t                   burst; //实际扩大1000倍，如果配置捅大小为1，这里为1000
    ngx_uint_t                   nodelay; /* unsigned  nodelay:1 */
} ngx_http_limit_req_limit_t;


typedef struct {
    ngx_array_t                  limits; //成员类型ngx_http_limit_req_limit_t
    ngx_uint_t                   limit_log_level;
    ngx_uint_t                   delay_log_level;
    ngx_uint_t                   status_code;
} ngx_http_limit_req_conf_t;


static void ngx_http_limit_req_delay(ngx_http_request_t *r);
static ngx_int_t ngx_http_limit_req_lookup(ngx_http_limit_req_limit_t *limit,
    ngx_uint_t hash, ngx_str_t *key, ngx_uint_t *ep, ngx_uint_t account);
static ngx_msec_t ngx_http_limit_req_account(ngx_http_limit_req_limit_t *limits,
    ngx_uint_t n, ngx_uint_t *ep, ngx_http_limit_req_limit_t **limit);
static void ngx_http_limit_req_expire(ngx_http_limit_req_ctx_t *ctx,
    ngx_uint_t n);

static void *ngx_http_limit_req_create_conf(ngx_conf_t *cf);
static char *ngx_http_limit_req_merge_conf(ngx_conf_t *cf, void *parent,
    void *child);
static char *ngx_http_limit_req_zone(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_limit_req(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static ngx_int_t ngx_http_limit_req_init(ngx_conf_t *cf);


static ngx_conf_enum_t  ngx_http_limit_req_log_levels[] = {
    { ngx_string("info"), NGX_LOG_INFO },
    { ngx_string("notice"), NGX_LOG_NOTICE },
    { ngx_string("warn"), NGX_LOG_WARN },
    { ngx_string("error"), NGX_LOG_ERR },
    { ngx_null_string, 0 }
};


static ngx_conf_num_bounds_t  ngx_http_limit_req_status_bounds = {
    ngx_conf_check_num_bounds, 400, 599
};

/* ngx_http_limit_conn_module限定同一时刻客户端的连接数， ngx_http_limit_req_commands限制请求处理的频率，也就是单位时间最多有多少个连接 */
/*
ngx_http_limit_req_module模块可以通过定义的 键值来限制请求处理的频率。特别的，它可以限制来自单个IP地址的请求处理频率。 限制的
方法是通过一种“漏桶”的方法——固定每秒处理的请求数，推迟过多的请求处理
*/
static ngx_command_t  ngx_http_limit_req_commands[] = {

/*
语法:  limit_req_zone $variable zone=name:size rate=rate;
默认值:  —  
上下文:  http

设置一块共享内存限制域的参数，它可以用来保存键值的状态。 它特别保存了当前超出请求的数量。 键的值就是指定的变量（空值不会被计算）。 示例用法： 
limit_req_zone $binary_remote_addr zone=one:10m rate=1r/s;

这里，状态被存在名为“one”，最大10M字节的共享内存里面。对于这个限制域来说 平均处理的请求频率不能超过每秒一次。 

键值是客户端的IP地址。 如果不使用$remote_addr变量，而用$binary_remote_addr变量， 可以将每条状态记录的大小减少到64个字节，这样1M
的内存可以保存大约1万6千个64字节的记录。 如果限制域的存储空间耗尽了，对于后续所有请求，服务器都会返回 503 (Service Temporarily Unavailable)错误。 
请求频率可以设置为每秒几次（r/s）。如果请求的频率不到每秒一次， 你可以设置每分钟几次(r/m)。比如每秒半次就是30r/m。
*/
    { ngx_string("limit_req_zone"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE3,
      ngx_http_limit_req_zone,
      0,
      0,
      NULL },

/*
语法:  limit_req zone=name [burst=number] [nodelay];
默认值:  —  
上下文:  http, server, location
 
设置对应的共享内存限制域和允许被处理的最大请求数阈值。 如果请求的频率超过了限制域配置的值，请求处理会被延迟，所以 所有的请求都
是以定义的频率被处理的。 超过频率限制的请求会被延迟，直到被延迟的请求数超过了定义的阈值 这时，这个请求会被终止，并返回
503 (Service Temporarily Unavailable) 错误。这个阈值的默认值等于0。 比如这些指令： 
limit_req_zone $binary_remote_addr zone=one:10m rate=1r/s;

server {
    location /search/ {
        limit_req zone=one burst=5;
    }
限制平均每秒不超过一个请求，同时允许超过频率限制的请求数不多于5个。 
如果不希望超过的请求被延迟，可以用nodelay参数： 
limit_req zone=one burst=5 nodelay;
*/
    { ngx_string("limit_req"), //limit_rate限制包体的发送速度，limit_req限制连接请求连理速度
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE123,
      ngx_http_limit_req,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
设置你所希望的日志级别，当服务器因为频率过高拒绝或者延迟处理请求时可以记下相应级别的日志。 延迟记录的日志级别比拒绝的低一个级别；
比如， 如果设置“limit_req_log_level notice”， 延迟的日志就是info级别。 
*/
    { ngx_string("limit_req_log_level"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_limit_req_conf_t, limit_log_level),
      &ngx_http_limit_req_log_levels },

    { ngx_string("limit_req_status"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_limit_req_conf_t, status_code),
      &ngx_http_limit_req_status_bounds },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_limit_req_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_limit_req_init,               /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_limit_req_create_conf,        /* create location configuration */
    ngx_http_limit_req_merge_conf          /* merge location configuration */
};

//参考https://my.oschina.net/kone/blog/88713
ngx_module_t  ngx_http_limit_req_module = {
    NGX_MODULE_V1,
    &ngx_http_limit_req_module_ctx,        /* module context */
    ngx_http_limit_req_commands,           /* module directives */
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
ngx_http_limit_req_handler(ngx_http_request_t *r)
{
    uint32_t                     hash;
    ngx_str_t                    key;
    ngx_int_t                    rc;
    ngx_uint_t                   n, excess;
    ngx_msec_t                   delay;
    ngx_http_limit_req_ctx_t    *ctx;
    ngx_http_limit_req_conf_t   *lrcf;
    ngx_http_limit_req_limit_t  *limit, *limits;

    if (r->main->limit_req_set) {
        return NGX_DECLINED;
    }

    lrcf = ngx_http_get_module_loc_conf(r, ngx_http_limit_req_module);
    limits = lrcf->limits.elts;

    excess = 0;

    rc = NGX_DECLINED;

#if (NGX_SUPPRESS_WARN)
    limit = NULL;
#endif

    for (n = 0; n < lrcf->limits.nelts; n++) {

        limit = &limits[n];

        ctx = limit->shm_zone->data;

        if (ngx_http_complex_value(r, &ctx->key, &key) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        if (key.len == 0) {
            continue;
        }

        if (key.len > 65535) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "the value of the \"%V\" key "
                          "is more than 65535 bytes: \"%V\"",
                          &ctx->key.value, &key);
            continue;
        }

        hash = ngx_crc32_short(key.data, key.len);

        ngx_shmtx_lock(&ctx->shpool->mutex);

        rc = ngx_http_limit_req_lookup(limit, hash, &key, &excess,
                                       (n == lrcf->limits.nelts - 1));

        ngx_shmtx_unlock(&ctx->shpool->mutex);

        ngx_log_debug4(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "limit_req[%ui]: %i %ui.%03ui",
                       n, rc, excess / 1000, excess % 1000);

        if (rc != NGX_AGAIN) {
            break;
        }
    }

    if (rc == NGX_DECLINED) {
        return NGX_DECLINED;
    }

    r->main->limit_req_set = 1;

    if (rc == NGX_BUSY || rc == NGX_ERROR) {

        if (rc == NGX_BUSY) {
            ngx_log_error(lrcf->limit_log_level, r->connection->log, 0,
                          "limiting requests, excess: %ui.%03ui by zone \"%V\"",
                          excess / 1000, excess % 1000,
                          &limit->shm_zone->shm.name);
        }

        while (n--) {
            ctx = limits[n].shm_zone->data;

            if (ctx->node == NULL) {
                continue;
            }

            ngx_shmtx_lock(&ctx->shpool->mutex);

            ctx->node->count--;

            ngx_shmtx_unlock(&ctx->shpool->mutex);

            ctx->node = NULL;
        }

        return lrcf->status_code;
    }

    /* rc == NGX_AGAIN || rc == NGX_OK */

    if (rc == NGX_AGAIN) {
        excess = 0;
    }

    delay = ngx_http_limit_req_account(limits, n, &excess, &limit);

    if (!delay) {
        return NGX_DECLINED;
    }

    ngx_log_error(lrcf->delay_log_level, r->connection->log, 0,
                  "delaying request, excess: %ui.%03ui, by zone \"%V\"",
                  excess / 1000, excess % 1000, &limit->shm_zone->shm.name);

    if (ngx_handle_read_event(r->connection->read, 0, NGX_FUNC_LINE) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    r->read_event_handler = ngx_http_test_reading;
    r->write_event_handler = ngx_http_limit_req_delay;
    ngx_add_timer(r->connection->write, delay, NGX_FUNC_LINE);

    return NGX_AGAIN;
}


static void
ngx_http_limit_req_delay(ngx_http_request_t *r)
{
    ngx_event_t  *wev;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "limit_req delay");

    wev = r->connection->write;

    if (!wev->timedout) {

        if (ngx_handle_write_event(wev, 0, NGX_FUNC_LINE) != NGX_OK) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        }

        return;
    }

    wev->timedout = 0;

    if (ngx_handle_read_event(r->connection->read, 0, NGX_FUNC_LINE) != NGX_OK) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    r->read_event_handler = ngx_http_block_reading;
    r->write_event_handler = ngx_http_core_run_phases;

    ngx_http_core_run_phases(r);
}


static void
ngx_http_limit_req_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t          **p;
    ngx_http_limit_req_node_t   *lrn, *lrnt;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            lrn = (ngx_http_limit_req_node_t *) &node->color;
            lrnt = (ngx_http_limit_req_node_t *) &temp->color;

            p = (ngx_memn2cmp(lrn->data, lrnt->data, lrn->len, lrnt->len) < 0)
                ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


static ngx_int_t
ngx_http_limit_req_lookup(ngx_http_limit_req_limit_t *limit, ngx_uint_t hash,
    ngx_str_t *key, ngx_uint_t *ep, ngx_uint_t account)
{
    size_t                      size;
    ngx_int_t                   rc, excess;
    ngx_time_t                 *tp;
    ngx_msec_t                  now;
    ngx_msec_int_t              ms;
    ngx_rbtree_node_t          *node, *sentinel;
    ngx_http_limit_req_ctx_t   *ctx;
    ngx_http_limit_req_node_t  *lr;

    tp = ngx_timeofday();
    now = (ngx_msec_t) (tp->sec * 1000 + tp->msec);

    ctx = limit->shm_zone->data;

    node = ctx->sh->rbtree.root;
    sentinel = ctx->sh->rbtree.sentinel;

    while (node != sentinel) {

        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        /* hash == node->key */

        lr = (ngx_http_limit_req_node_t *) &node->color;

        rc = ngx_memn2cmp(key->data, lr->data, key->len, (size_t) lr->len);

        if (rc == 0) {
            ngx_queue_remove(&lr->queue);
            ngx_queue_insert_head(&ctx->sh->queue, &lr->queue);

            ms = (ngx_msec_int_t) (now - lr->last);

            /*  https://my.oschina.net/kone/blog/88713
            假设设置rate为500，则rate=500000  配置burst为10，则burst值为10000
            假设1ms一个请求，则excess取值为
            第几个请求        excess值           lr->excess值  是否更新lr->last   是否超过burst阈值
              1                 0-500+1000=500         500             是           否    
              2                 500-500+1000=1000      1000            是           否
              3                 1000-500+1000=1500     1500            是           否
              ...............
              10                9500-500+1000=10000    10000           是           否
              11                10000-500+1000=10500   10000           否           是   最后excess没有保存，还是保存上次的，所以还是10000
              12                10000-2*500+1000=10000 10000           是           否
              13                10000-2*500+1000=10500 10000           否           是
              14                10000-2*500+1000=10000 10000           是           否
              ..............
              后面循环每隔2ms满足条件





            该算法的核心是:有一个桶，他最多容纳MAX令牌，我们往该桶中扔令牌，如果桶中令牌达到了max，则不能往里面扔了，就会拒绝接收该令牌，
            同时每隔1ms会从令牌桶中淘汰掉部分令牌。然后就可以继续扔令牌进去了

            这个算法应该属于漏桶算法:
            假设配置限速rate大小为10000每秒，burst业务10000每秒，配置worker工作进程就为1，则令牌桶允许最大容量令牌数为10000 * 1000，同时每毫秒自动从令牌桶中删除的
            令牌数为10000。如果在1秒钟时间段的第500ms的时候，已经接受了10000个连接，则令牌桶中令牌数为：10000 * 1000-500 * 1000，也就是令牌
            桶还可以扔500 * 1000个令牌才能达到最大容量，这样下一个1ms(也就是500ms到501ms这1ms内)可以接受500个请求。501ms到后，又会删除10000个
            令牌，又可以接收10个请求，依次继续循环下去。

            实际运行的请求数为rate+burst 
            之前的理解有误，可以参考这里:https://github.com/alibaba/tengine/issues/855
            */
            /* 
            lr->excess:桶中的令牌总数
            ctx->rate * ngx_abs(ms) / 1000:每隔1ms从令牌桶中自动清除的令牌数
            +1000 : 表示每来一个请求，就向令牌桶中添加1000个令牌，因为1个请求对应1000个令牌

            ???????
            如果rate=10000,burst=1,如果高流量过来，则1ms只允许过一个请求，1分钟才1000个，如果rate大于1000，
            则if ((ngx_uint_t) excess > limit->burst) 是不是应该改为if ((ngx_uint_t) excess > ctx->rate / 1000) {
            */
            excess = lr->excess - ctx->rate * ngx_abs(ms) / 1000 + 1000;

            if (excess < 0) {
                excess = 0;
            }

            *ep = excess;

            
            if ((ngx_uint_t) excess > limit->burst) {
                return NGX_BUSY;
            }

            if (account) {
                lr->excess = excess;
                lr->last = now;
                return NGX_OK;
            }

            lr->count++;

            ctx->node = lr;

            return NGX_AGAIN;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    *ep = 0;

    size = offsetof(ngx_rbtree_node_t, color)
           + offsetof(ngx_http_limit_req_node_t, data)
           + key->len;

    ngx_http_limit_req_expire(ctx, 1);

    node = ngx_slab_alloc_locked(ctx->shpool, size);

    if (node == NULL) {
        ngx_http_limit_req_expire(ctx, 0);

        node = ngx_slab_alloc_locked(ctx->shpool, size);
        if (node == NULL) {
            ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0,
                          "could not allocate node%s", ctx->shpool->log_ctx);
            return NGX_ERROR;
        }
    }

    node->key = hash;

    lr = (ngx_http_limit_req_node_t *) &node->color;

    lr->len = (u_short) key->len;
    lr->excess = 0;

    ngx_memcpy(lr->data, key->data, key->len);

    ngx_rbtree_insert(&ctx->sh->rbtree, node);

    ngx_queue_insert_head(&ctx->sh->queue, &lr->queue);

    if (account) {
        lr->last = now;
        lr->count = 0;
        return NGX_OK;
    }

    lr->last = 0;
    lr->count = 1;

    ctx->node = lr;

    return NGX_AGAIN;
}


static ngx_msec_t
ngx_http_limit_req_account(ngx_http_limit_req_limit_t *limits, ngx_uint_t n,
    ngx_uint_t *ep, ngx_http_limit_req_limit_t **limit)
{
    ngx_int_t                   excess;
    ngx_time_t                 *tp;
    ngx_msec_t                  now, delay, max_delay;
    ngx_msec_int_t              ms;
    ngx_http_limit_req_ctx_t   *ctx;
    ngx_http_limit_req_node_t  *lr;

    excess = *ep;

    if (excess == 0 || (*limit)->nodelay) {
        max_delay = 0;

    } else {
        ctx = (*limit)->shm_zone->data;
        max_delay = excess * 1000 / ctx->rate;
    }

    while (n--) {
        ctx = limits[n].shm_zone->data;
        lr = ctx->node;

        if (lr == NULL) {
            continue;
        }

        ngx_shmtx_lock(&ctx->shpool->mutex);

        tp = ngx_timeofday();

        now = (ngx_msec_t) (tp->sec * 1000 + tp->msec);
        ms = (ngx_msec_int_t) (now - lr->last);

        excess = lr->excess - ctx->rate * ngx_abs(ms) / 1000 + 1000;

        if (excess < 0) {
            excess = 0;
        }

        lr->last = now;
        lr->excess = excess;
        lr->count--;

        ngx_shmtx_unlock(&ctx->shpool->mutex);

        ctx->node = NULL;

        if (limits[n].nodelay) {
            continue;
        }

        delay = excess * 1000 / ctx->rate;

        if (delay > max_delay) {
            max_delay = delay;
            *ep = excess;
            *limit = &limits[n];
        }
    }

    return max_delay;
}


static void
ngx_http_limit_req_expire(ngx_http_limit_req_ctx_t *ctx, ngx_uint_t n)
{
    ngx_int_t                   excess;
    ngx_time_t                 *tp;
    ngx_msec_t                  now;
    ngx_queue_t                *q;
    ngx_msec_int_t              ms;
    ngx_rbtree_node_t          *node;
    ngx_http_limit_req_node_t  *lr;

    tp = ngx_timeofday();

    now = (ngx_msec_t) (tp->sec * 1000 + tp->msec);

    /*
     * n == 1 deletes one or two zero rate entries
     * n == 0 deletes oldest entry by force
     *        and one or two zero rate entries
     */

    while (n < 3) {

        if (ngx_queue_empty(&ctx->sh->queue)) {
            return;
        }

        q = ngx_queue_last(&ctx->sh->queue);

        lr = ngx_queue_data(q, ngx_http_limit_req_node_t, queue);

        if (lr->count) {

            /*
             * There is not much sense in looking further,
             * because we bump nodes on the lookup stage.
             */

            return;
        }

        if (n++ != 0) {

            ms = (ngx_msec_int_t) (now - lr->last);
            ms = ngx_abs(ms);

            if (ms < 60000) {
                return;
            }

            excess = lr->excess - ctx->rate * ms / 1000;

            if (excess > 0) {
                return;
            }
        }

        ngx_queue_remove(q);

        node = (ngx_rbtree_node_t *)
                   ((u_char *) lr - offsetof(ngx_rbtree_node_t, color));

        ngx_rbtree_delete(&ctx->sh->rbtree, node);

        ngx_slab_free_locked(ctx->shpool, node);
    }
}

/*
shm_zone->init = ngx_http_limit_req_init_zone;
shm_zone->data = ctx; //ngx_http_limit_req_init_zone的data参数
*/
static ngx_int_t
ngx_http_limit_req_init_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_http_limit_req_ctx_t  *octx = data;

    size_t                     len;
    ngx_http_limit_req_ctx_t  *ctx;

    ctx = shm_zone->data;

    if (octx) {
        if (ctx->key.value.len != octx->key.value.len
            || ngx_strncmp(ctx->key.value.data, octx->key.value.data,
                           ctx->key.value.len)
               != 0)
        {
            ngx_log_error(NGX_LOG_EMERG, shm_zone->shm.log, 0,
                          "limit_req \"%V\" uses the \"%V\" key "
                          "while previously it used the \"%V\" key",
                          &shm_zone->shm.name, &ctx->key.value,
                          &octx->key.value);
            return NGX_ERROR;
        }

        ctx->sh = octx->sh;
        ctx->shpool = octx->shpool;

        return NGX_OK;
    }

    ctx->shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (shm_zone->shm.exists) {
        ctx->sh = ctx->shpool->data;

        return NGX_OK;
    }

    ctx->sh = ngx_slab_alloc(ctx->shpool, sizeof(ngx_http_limit_req_shctx_t));
    if (ctx->sh == NULL) {
        return NGX_ERROR;
    }

    ctx->shpool->data = ctx->sh;

    ngx_rbtree_init(&ctx->sh->rbtree, &ctx->sh->sentinel,
                    ngx_http_limit_req_rbtree_insert_value);

    ngx_queue_init(&ctx->sh->queue);

    len = sizeof(" in limit_req zone \"\"") + shm_zone->shm.name.len;

    ctx->shpool->log_ctx = ngx_slab_alloc(ctx->shpool, len);
    if (ctx->shpool->log_ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_sprintf(ctx->shpool->log_ctx, " in limit_req zone \"%V\"%Z",
                &shm_zone->shm.name);

    ctx->shpool->log_nomem = 0;

    return NGX_OK;
}


static void *
ngx_http_limit_req_create_conf(ngx_conf_t *cf)
{
    ngx_http_limit_req_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_limit_req_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->limits.elts = NULL;
     */

    conf->limit_log_level = NGX_CONF_UNSET_UINT;
    conf->status_code = NGX_CONF_UNSET_UINT;

    return conf;
}


static char *
ngx_http_limit_req_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_limit_req_conf_t *prev = parent;
    ngx_http_limit_req_conf_t *conf = child;

    if (conf->limits.elts == NULL) {
        conf->limits = prev->limits;
    }

    ngx_conf_merge_uint_value(conf->limit_log_level, prev->limit_log_level,
                              NGX_LOG_ERR);

    conf->delay_log_level = (conf->limit_log_level == NGX_LOG_INFO) ?
                                NGX_LOG_INFO : conf->limit_log_level + 1;

    ngx_conf_merge_uint_value(conf->status_code, prev->status_code,
                              NGX_HTTP_SERVICE_UNAVAILABLE);

    return NGX_CONF_OK;
}


static char *
ngx_http_limit_req_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    u_char                            *p;
    size_t                             len;
    ssize_t                            size;
    ngx_str_t                         *value, name, s;
    ngx_int_t                          rate, scale;
    ngx_uint_t                         i;
    ngx_shm_zone_t                    *shm_zone;
    ngx_http_limit_req_ctx_t          *ctx;
    ngx_http_compile_complex_value_t   ccv;

    value = cf->args->elts;

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_limit_req_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &ctx->key;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    size = 0;
    rate = 1;
    scale = 1;
    name.len = 0;

    for (i = 2; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "zone=", 5) == 0) {

            name.data = value[i].data + 5;

            p = (u_char *) ngx_strchr(name.data, ':');

            if (p == NULL) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid zone size \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            name.len = p - name.data;

            s.data = p + 1;
            s.len = value[i].data + value[i].len - s.data;

            size = ngx_parse_size(&s);

            if (size == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid zone size \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            if (size < (ssize_t) (8 * ngx_pagesize)) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "zone \"%V\" is too small", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "rate=", 5) == 0) {

            len = value[i].len;
            p = value[i].data + len - 3;

            if (ngx_strncmp(p, "r/s", 3) == 0) {
                scale = 1;
                len -= 3;

            } else if (ngx_strncmp(p, "r/m", 3) == 0) {
                scale = 60;
                len -= 3;
            }

            rate = ngx_atoi(value[i].data + 5, len - 5);
            if (rate <= 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid rate \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    if (name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"zone\" parameter",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    ctx->rate = rate * 1000 / scale; //默认1000r/s

    shm_zone = ngx_shared_memory_add(cf, &name, size,
                                     &ngx_http_limit_req_module);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    if (shm_zone->data) {
        ctx = shm_zone->data;

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "%V \"%V\" is already bound to key \"%V\"",
                           &cmd->name, &name, &ctx->key.value);
        return NGX_CONF_ERROR;
    }

    shm_zone->init = ngx_http_limit_req_init_zone;
    shm_zone->data = ctx;

    return NGX_CONF_OK;
}


static char *
ngx_http_limit_req(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_limit_req_conf_t  *lrcf = conf;

    ngx_int_t                    burst;
    ngx_str_t                   *value, s;
    ngx_uint_t                   i, nodelay;
    ngx_shm_zone_t              *shm_zone;
    ngx_http_limit_req_limit_t  *limit, *limits;

    value = cf->args->elts;

    shm_zone = NULL;
    burst = 0;
    nodelay = 0;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "zone=", 5) == 0) {

            s.len = value[i].len - 5;
            s.data = value[i].data + 5;

            shm_zone = ngx_shared_memory_add(cf, &s, 0,
                                             &ngx_http_limit_req_module);
            if (shm_zone == NULL) {
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "burst=", 6) == 0) {

            burst = ngx_atoi(value[i].data + 6, value[i].len - 6);
            if (burst <= 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid burst rate \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strcmp(value[i].data, "nodelay") == 0) {
            nodelay = 1;
            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    if (shm_zone == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"zone\" parameter",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    limits = lrcf->limits.elts;

    if (limits == NULL) {
        if (ngx_array_init(&lrcf->limits, cf->pool, 1,
                           sizeof(ngx_http_limit_req_limit_t))
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    for (i = 0; i < lrcf->limits.nelts; i++) {
        if (shm_zone == limits[i].shm_zone) {
            return "is duplicate";
        }
    }

    limit = ngx_array_push(&lrcf->limits);
    if (limit == NULL) {
        return NGX_CONF_ERROR;
    }
    
    limit->shm_zone = shm_zone;
    limit->burst = burst * 1000;
    limit->nodelay = nodelay;

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_limit_req_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_limit_req_handler;

    return NGX_OK;
}
