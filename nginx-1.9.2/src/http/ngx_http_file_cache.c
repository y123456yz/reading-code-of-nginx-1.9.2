
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_md5.h>


static ngx_int_t ngx_http_file_cache_lock(ngx_http_request_t *r,
    ngx_http_cache_t *c);
static void ngx_http_file_cache_lock_wait_handler(ngx_event_t *ev);
static void ngx_http_file_cache_lock_wait(ngx_http_request_t *r,
    ngx_http_cache_t *c);
static ngx_int_t ngx_http_file_cache_read(ngx_http_request_t *r,
    ngx_http_cache_t *c);
static ssize_t ngx_http_file_cache_aio_read(ngx_http_request_t *r,
    ngx_http_cache_t *c);
#if (NGX_HAVE_FILE_AIO)
static void ngx_http_cache_aio_event_handler(ngx_event_t *ev);
#endif
#if (NGX_THREADS)
static ngx_int_t ngx_http_cache_thread_handler(ngx_thread_task_t *task,
    ngx_file_t *file);
static void ngx_http_cache_thread_event_handler(ngx_event_t *ev);
#endif
static ngx_int_t ngx_http_file_cache_exists(ngx_http_file_cache_t *cache,
    ngx_http_cache_t *c);
static ngx_int_t ngx_http_file_cache_name(ngx_http_request_t *r,
    ngx_path_t *path);
static ngx_http_file_cache_node_t *
    ngx_http_file_cache_lookup(ngx_http_file_cache_t *cache, u_char *key);
static void ngx_http_file_cache_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
static void ngx_http_file_cache_vary(ngx_http_request_t *r, u_char *vary,
    size_t len, u_char *hash);
static void ngx_http_file_cache_vary_header(ngx_http_request_t *r,
    ngx_md5_t *md5, ngx_str_t *name);
static ngx_int_t ngx_http_file_cache_reopen(ngx_http_request_t *r,
    ngx_http_cache_t *c);
static ngx_int_t ngx_http_file_cache_update_variant(ngx_http_request_t *r,
    ngx_http_cache_t *c);
static void ngx_http_file_cache_cleanup(void *data);
static time_t ngx_http_file_cache_forced_expire(ngx_http_file_cache_t *cache);
static time_t ngx_http_file_cache_expire(ngx_http_file_cache_t *cache);
static void ngx_http_file_cache_delete(ngx_http_file_cache_t *cache,
    ngx_queue_t *q, u_char *name);
static void ngx_http_file_cache_loader_sleep(ngx_http_file_cache_t *cache);
static ngx_int_t ngx_http_file_cache_noop(ngx_tree_ctx_t *ctx,
    ngx_str_t *path);
static ngx_int_t ngx_http_file_cache_manage_file(ngx_tree_ctx_t *ctx,
    ngx_str_t *path);
static ngx_int_t ngx_http_file_cache_manage_directory(ngx_tree_ctx_t *ctx,
    ngx_str_t *path);
static ngx_int_t ngx_http_file_cache_add_file(ngx_tree_ctx_t *ctx,
    ngx_str_t *path);
static ngx_int_t ngx_http_file_cache_add(ngx_http_file_cache_t *cache,
    ngx_http_cache_t *c);
static ngx_int_t ngx_http_file_cache_delete_file(ngx_tree_ctx_t *ctx,
    ngx_str_t *path);


ngx_str_t  ngx_http_cache_status[] = {
    ngx_string("MISS"),
    ngx_string("BYPASS"),
    ngx_string("EXPIRED"),
    ngx_string("STALE"),
    ngx_string("UPDATING"),
    ngx_string("REVALIDATED"),
    ngx_string("HIT")
};


static u_char  ngx_http_file_cache_key[] = { LF, 'K', 'E', 'Y', ':', ' ' };


static ngx_int_t
ngx_http_file_cache_init(ngx_shm_zone_t *shm_zone, void *data) //ngx_init_cycle中执行
{
    ngx_http_file_cache_t  *ocache = data;

    size_t                  len;
    ngx_uint_t              n;
    ngx_http_file_cache_t  *cache;

    cache = shm_zone->data;

    if (ocache) {
        //如果ocache不是NULL，即有old cache，就比较缓存路径和level等，如果match的话就继承ocache的sh、shpool、bsize等  
        if (ngx_strcmp(cache->path->name.data, ocache->path->name.data) != 0) {
            ngx_log_error(NGX_LOG_EMERG, shm_zone->shm.log, 0,
                          "cache \"%V\" uses the \"%V\" cache path "
                          "while previously it used the \"%V\" cache path",
                          &shm_zone->shm.name, &cache->path->name,
                          &ocache->path->name);

            return NGX_ERROR;
        }

        for (n = 0; n < 3; n++) {
            if (cache->path->level[n] != ocache->path->level[n]) {
                ngx_log_error(NGX_LOG_EMERG, shm_zone->shm.log, 0,
                              "cache \"%V\" had previously different levels",
                              &shm_zone->shm.name);
                return NGX_ERROR;
            }
        }

        cache->sh = ocache->sh;

        cache->shpool = ocache->shpool;
        cache->bsize = ocache->bsize;

        cache->max_size /= cache->bsize;

        if (!cache->sh->cold || cache->sh->loading) {
            cache->path->loader = NULL;
        }

        return NGX_OK;
    }

    cache->shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (shm_zone->shm.exists) {
        cache->sh = cache->shpool->data;
        cache->bsize = ngx_fs_bsize(cache->path->name.data);

        return NGX_OK;
    }

    cache->sh = ngx_slab_alloc(cache->shpool, sizeof(ngx_http_file_cache_sh_t));
    if (cache->sh == NULL) {
        return NGX_ERROR;
    }

    cache->shpool->data = cache->sh;

    ngx_rbtree_init(&cache->sh->rbtree, &cache->sh->sentinel,
                    ngx_http_file_cache_rbtree_insert_value); //红黑树初始化

    ngx_queue_init(&cache->sh->queue);//队列初始化

    cache->sh->cold = 1;
    cache->sh->loading = 0;
    cache->sh->size = 0;

    cache->bsize = ngx_fs_bsize(cache->path->name.data);

    cache->max_size /= cache->bsize;

    len = sizeof(" in cache keys zone \"\"") + shm_zone->shm.name.len;

    cache->shpool->log_ctx = ngx_slab_alloc(cache->shpool, len);
    if (cache->shpool->log_ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_sprintf(cache->shpool->log_ctx, " in cache keys zone \"%V\"%Z",
                &shm_zone->shm.name);

    cache->shpool->log_nomem = 0;

    return NGX_OK;
}

//这里面的keys数组是为了存储proxy_cache_key $scheme$proxy_host$request_uri各个变量对应的value值
ngx_int_t
ngx_http_file_cache_new(ngx_http_request_t *r)
{
    ngx_http_cache_t  *c;

    c = ngx_pcalloc(r->pool, sizeof(ngx_http_cache_t));
    if (c == NULL) {
        return NGX_ERROR;
    }

    if (ngx_array_init(&c->keys, r->pool, 4, sizeof(ngx_str_t)) != NGX_OK) {
        return NGX_ERROR;
    }

    r->cache = c;
    c->file.log = r->connection->log;
    c->file.fd = NGX_INVALID_FILE;

    return NGX_OK;
}

/*ngx_http_upstream_init_request->ngx_http_upstream_cache 客户端获取缓存 后端应答回来数据后在ngx_http_upstream_send_response->ngx_http_file_cache_create
中创建临时文件，然后在ngx_event_pipe_write_chain_to_temp_file把读取的后端数据写入临时文件，最后在
ngx_http_upstream_send_response->ngx_http_upstream_process_request->ngx_http_file_cache_update中把临时文件内容rename(相当于mv)到proxy_cache_path指定
的cache目录下面
*/
    /*后端数据读取完毕，并且全部写入临时文件后才会执行rename过程，为什么需要临时文件的原因是:例如之前的缓存过期了，现在有个请求正在从后端
    获取数据写入临时文件，如果是直接写入缓存文件，则在获取后端数据过程中，如果在来一个客户端请求，如果允许proxy_cache_use_stale updating，则
    后面的请求可以直接获取之前老旧的过期缓存，从而可以避免冲突(前面的请求写文件，后面的请求获取文件内容) 
    */

//为后端应答的数据创建对应的缓存文件
ngx_int_t
ngx_http_file_cache_create(ngx_http_request_t *r)
{
    ngx_http_cache_t       *c;
    ngx_pool_cleanup_t     *cln;
    ngx_http_file_cache_t  *cache;

    c = r->cache;
    cache = c->file_cache;

    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) {
        return NGX_ERROR;
    }

    cln->handler = ngx_http_file_cache_cleanup;
    cln->data = c;

    if (ngx_http_file_cache_exists(cache, c) == NGX_ERROR) {
        return NGX_ERROR;
    }

    if (ngx_http_file_cache_name(r, cache->path) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

/* 生成 md5sum(key) 和 crc32(key)并计算 `c->header_start` 值 */
void
ngx_http_file_cache_create_key(ngx_http_request_t *r)
{
    size_t             len;
    ngx_str_t         *key;
    ngx_uint_t         i;
    ngx_md5_t          md5;
    ngx_http_cache_t  *c;

    c = r->cache;

    len = 0;

    ngx_crc32_init(c->crc32);
    ngx_md5_init(&md5);

    key = c->keys.elts; 
    for (i = 0; i < c->keys.nelts; i++) { //计算 proxy_cache_key $scheme$proxy_host$request_uri对应的变量value值的md5和crc32值
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http cache key: \"%V\"", &key[i]);

        len += key[i].len; //xxx_cache_key配置中的字符串长度和

        ngx_crc32_update(&c->crc32, key[i].data, key[i].len); //xxx_cache_key配置中的字符串进行crc32校验值   ·
        ngx_md5_update(&md5, key[i].data, key[i].len); //xxx_cache_key配置中的字符串进行MD5运算 ·
    }

    ////[ngx_http_file_cache_header_t]["\nKEY: "][orig_key]["\n"][header][body] 封包过程见ngx_http_file_cache_set_header
    c->header_start = sizeof(ngx_http_file_cache_header_t)
                      + sizeof(ngx_http_file_cache_key) + len + 1; //+1是因为key后面有有个'\N'

    ngx_crc32_final(c->crc32);//获取所有key字符串的校验结果
    ngx_md5_final(c->key, &md5);//获取xxx_cache_key配置字符串进行MD5运算的值

    ngx_memcpy(c->main, c->key, NGX_HTTP_CACHE_KEY_LEN);
}

/*
 ngx_http_file_cache_open->ngx_http_file_cache_read->ngx_http_file_cache_aio_read这个流程获取文件中前面的头部信息相关内容，并获取整个
 文件stat信息，例如文件大小等。
 头部部分在ngx_http_cache_send->ngx_http_send_header发送，
 缓存文件后面的包体部分在ngx_http_cache_send后半部代码中触发在filter模块中发送
 */

//调用 ngx_http_file_cache_open 函数查找是否有对应的有效缓存数据 ngx_http_file_cache_open 函数负责缓存文件定位、缓存文件打开和校验等操作
ngx_int_t
ngx_http_file_cache_open(ngx_http_request_t *r)
{//读取缓存文件前面的头部信息数据到r->cache->buf，同时获取文件的相关属性到r->cache的相关字段
    ngx_int_t                  rc, rv;
    ngx_uint_t                 test;
    ngx_http_cache_t          *c;
    ngx_pool_cleanup_t        *cln;
    ngx_open_file_info_t       of;
    ngx_http_file_cache_t     *cache;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->cache;

    /* ngx_http_file_cache_open如果返回NGX_AGAIN，则会在函数外执行下面的代码，也就是等待前面的请求后端返回后，再次触发后面的请求执行ngx_http_upstream_init_request过程
        这时候前面从后端获取的数据肯定已经得到缓存
        r->write_event_handler = ngx_http_upstream_init_request;  //这么触发该write handler呢?因为前面的请求获取到后端数据后，在触发epoll_in的同时
        也会触发epoll_out，从而会执行该函数
        return;  
     */
    if (c->waiting) {  //缓存内容己过期，当前请求正等待其它请求更新此缓存节点。 
        return NGX_AGAIN;
    }

    if (c->reading) {
        return ngx_http_file_cache_read(r, c);
    }

    //通过proxy_cache xxx或者fastcgi_cache xxx来设置的共享内存等信息
    cache = c->file_cache;

    /*
     第一次根据请求信息生成的 key 查找对应缓存节点时，先注册一下请求内存池级别的清理函数
     */
    if (c->node == NULL) { //添加缓存对应的cleanup
        cln = ngx_pool_cleanup_add(r->pool, 0);
        if (cln == NULL) {
            return NGX_ERROR;
        }

        cln->handler = ngx_http_file_cache_cleanup;
        cln->data = c;
    }

    rc = ngx_http_file_cache_exists(cache, c);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache exists, rc: %i if exist:%d", rc, c->exists);

    if (rc == NGX_ERROR) {
        return rc;
    }

    
    if (rc == NGX_AGAIN) { //例如配置Proxy_cache_min_uses 5，则需要客户端请求5才才能从缓存中取，如果现在只有4次，则都需要从后端获取数据
        return NGX_HTTP_CACHE_SCARCE; //函数外层ngx_http_upstream_cache会把 u->cacheable = 0;
    }

    if (rc == NGX_OK) { 
        
        if (c->error) {
            return c->error;
        }

        c->temp_file = 1;
        test = c->exists ? 1 : 0; //是否有达到Proxy_cache_min_uses 5配置的开始缓存文件的请求次数，达到为1，没达到为0
        rv = NGX_DECLINED;//如果返回这个，会把cached置0，返回出去后只有从后端从新获取数据

    } else { /* rc == NGX_DECLINED */ //表示在ngx_http_file_cache_exists中没找到该key对应的node节点，因此按照key重新创建了一个node节点(第一次请求该uri)
        //ngx_http_file_cache_exists没找到对应的ngx_http_file_cache_node_t节点，或者该节点对应缓存过期，返回NGX_DECLINED (第一次请求该uri)
        test = cache->sh->cold ? 1 : 0;//test=0,表示进程起来后缓存文件已经加载完毕，为1表示进程刚起来还没有加载缓存文件，默认值1

        if (c->min_uses > 1) {

            if (!test) { //
                return NGX_HTTP_CACHE_SCARCE;
            }

            rv = NGX_HTTP_CACHE_SCARCE;

        } else {
            c->temp_file = 1;
            rv = NGX_DECLINED; //如果返回这个，会把cached置0，返回出去后只有从后端从新获取数据
        }
    }

    if (ngx_http_file_cache_name(r, cache->path) != NGX_OK) {
        return NGX_ERROR;
    }

    if (!test) {
        //还没达到Proxy_cache_min_uses 5配置的开始缓存文件的请求次数
        //nginx进程起来后，loader进程已经把缓存文件加载完毕，但是在红黑树中没有找到对应的文件node节点(第一次请求该uri)
        goto done;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    ngx_memzero(&of, sizeof(ngx_open_file_info_t));

    of.uniq = c->uniq;
    of.valid = clcf->open_file_cache_valid;  
    of.min_uses = clcf->open_file_cache_min_uses;
    of.events = clcf->open_file_cache_events;
    of.directio = NGX_OPEN_FILE_DIRECTIO_OFF;
    of.read_ahead = clcf->read_ahead;  /* read_ahead配置，默认0 */

    if (ngx_open_cached_file(clcf->open_file_cache, &c->file.name, &of, r->pool)
        != NGX_OK)
    { //一般没有该文件的时候会走到这里面
        ngx_log_debugall(r->connection->log, 0, "ngx_open_cached_file return:NGX_ERROR");
        switch (of.err) {

        case 0:
            return NGX_ERROR;

        case NGX_ENOENT:
        case NGX_ENOTDIR:
            goto done;

        default:
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, of.err,
                          ngx_open_file_n " \"%s\" failed", c->file.name.data);
            return NGX_ERROR;
        }
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache fd: %d", of.fd);

    c->file.fd = of.fd;
    c->file.log = r->connection->log;
    c->uniq = of.uniq;
    c->length = of.size;
    c->fs_size = (of.fs_size + cache->bsize - 1) / cache->bsize; //bsize对齐

    /*
    root@root:/var/yyz# cat cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f   封包过程见ngx_http_file_cache_set_header
     3hwhdBw
     KEY: /test2.php
     
     X-Powered-By: PHP/5.2.13
     Content-type: text/html
    //body_start就是上面这一段内存内容长度
    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    
     <Html> 
     <title>file update</title>
     <body> 
     <form method="post" action="" enctype="multipart/form-data">
     <input type="file" name="file" /> 
     <input type="submit" value="submit" /> 
     </form> 
     </body> 
     </html>
     */ 
    //创建存放缓存文件中前面[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header]部分的内容长度空间,也就是
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@前面的内容
    c->buf = ngx_create_temp_buf(r->pool, c->body_start);
    if (c->buf == NULL) {
        return NGX_ERROR;
    }

//注意这里读取缓存文件中的头部部分的时候，只有aio读取或者缓存方式读取，和sendfile没有关系，因为头部读出来需要重新组装发往客户端的头部行信息，必须从文件读到内存中
    //如果返回这个NGX_DECLINED，会把cached置0，返回出去后只有从后端从新获取数据
    return ngx_http_file_cache_read(r, c);  

done:
    //还没达到Proxy_cache_min_uses 5配置的开始缓存文件的请求次数
    //nginx进程起来后，loader进程已经把缓存文件加载完毕，但是在红黑树中没有找到对应的文件node节点(第一次请求该uri)，同时配置的Proxy_cache_min_uses=1
    if (rv == NGX_DECLINED) {
    //说明没有uri对应的缓存文件，通过ngx_http_cache_t->key[](实际上就是由uri进行MD5计算出的值放到key[]中的)在红黑树中找不到该节点
        return ngx_http_file_cache_lock(r, c);
    }

    return rv;
}


static ngx_int_t
ngx_http_file_cache_lock(ngx_http_request_t *r, ngx_http_cache_t *c)
{
    ngx_msec_t                 now, timer;
    ngx_http_file_cache_t     *cache;

    if (!c->lock) {//默认就是0
        return NGX_DECLINED;
    }

    now = ngx_current_msec;

    cache = c->file_cache;

    ngx_shmtx_lock(&cache->shpool->mutex);

    timer = c->node->lock_time - now;

    if (!c->node->updating || (ngx_msec_int_t) timer <= 0) {
        c->node->updating = 1;
        c->node->lock_time = now + c->lock_age;
        c->updating = 1;
        c->lock_time = c->node->lock_time;
    }

    ngx_shmtx_unlock(&cache->shpool->mutex);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache lock u:%d wt:%M",
                   c->updating, c->wait_time);

    if (c->updating) {
        return NGX_DECLINED;
    }

    if (c->lock_timeout == 0) {
        return NGX_HTTP_CACHE_SCARCE;
    }

    c->waiting = 1;

    if (c->wait_time == 0) {
        c->wait_time = now + c->lock_timeout;

        c->wait_event.handler = ngx_http_file_cache_lock_wait_handler;
        c->wait_event.data = r;
        c->wait_event.log = r->connection->log;
    }

    timer = c->wait_time - now;

    ngx_add_timer(&c->wait_event, (timer > 500) ? 500 : timer, NGX_FUNC_LINE);

    r->main->blocked++;

    return NGX_AGAIN;
}


static void
ngx_http_file_cache_lock_wait_handler(ngx_event_t *ev)
{
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

    r = ev->data;
    c = r->connection;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http file cache wait: \"%V?%V\"", &r->uri, &r->args);

    ngx_http_file_cache_lock_wait(r, r->cache);

    ngx_http_run_posted_requests(c);
}


static void
ngx_http_file_cache_lock_wait(ngx_http_request_t *r, ngx_http_cache_t *c)
{
    ngx_uint_t              wait;
    ngx_msec_t              now, timer;
    ngx_http_file_cache_t  *cache;

    now = ngx_current_msec;

    timer = c->wait_time - now;

    if ((ngx_msec_int_t) timer <= 0) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "cache lock timeout");
        c->lock_timeout = 0;
        goto wakeup;
    }

    cache = c->file_cache;
    wait = 0;

    ngx_shmtx_lock(&cache->shpool->mutex);

    timer = c->node->lock_time - now;

    if (c->node->updating && (ngx_msec_int_t) timer > 0) {
        wait = 1;
    }

    ngx_shmtx_unlock(&cache->shpool->mutex);

    if (wait) {
        ngx_add_timer(&c->wait_event, (timer > 500) ? 500 : timer, NGX_FUNC_LINE);
        return;
    }

wakeup:

    c->waiting = 0;
    r->main->blocked--;
    r->write_event_handler(r);
}


/*
    root@root:/var/yyz# cat cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f   封包过程见ngx_http_file_cache_set_header
     3hwhdBw
     KEY: /test2.php
     
     X-Powered-By: PHP/5.2.13
     Content-type: text/html
    //body_start就是上面这一段内存内容长度
    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    
     <Html> 
     <title>file update</title>
     <body> 
     <form method="post" action="" enctype="multipart/form-data">
     <input type="file" name="file" /> 
     <input type="submit" value="submit" /> 
     </form> 
     </body> 
     </html>
*/ 

/*
     ngx_http_file_cache_open->ngx_http_file_cache_read->ngx_http_file_cache_aio_read这个流程获取文件中前面的头部信息相关内容，并获取整个
     文件stat信息，例如文件大小等。
     头部部分在ngx_http_cache_send->ngx_http_send_header发送，
     缓存文件后面的包体部分在ngx_http_cache_send后半部代码中触发在filter模块中发送
 */

//读取缓存文件中前面[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header]部分的内容长度空间,也就是
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@前面的内容
static ngx_int_t
ngx_http_file_cache_read(ngx_http_request_t *r, ngx_http_cache_t *c)
{
//注意这里读取缓存文件中的头部部分的时候，只有aio读取或者缓存方式读取，和sendfile没有关系，因为头部读出来需要重新组装发往客户端的头部行信息，必须从文件读到内存中
    time_t                         now;
    ssize_t                        n;
    ngx_int_t                      rc;
    ngx_http_file_cache_t         *cache;
    ngx_http_file_cache_header_t  *h;

    /*
     ngx_http_file_cache_open->ngx_http_file_cache_read->ngx_http_file_cache_aio_read这个流程获取文件中前面的头部信息相关内容，并获取整个
     文件stat信息，例如文件大小等。
     头部部分在ngx_http_cache_send->ngx_http_send_header发送，
     缓存文件后面的包体部分在ngx_http_cache_send后半部代码中触发在filter模块中发送
     */
    n = ngx_http_file_cache_aio_read(r, c);//读取缓存文件中的前面头部相关信息部分数据

    if (n < 0) {
        return n;
    }

    //写缓冲区封装过程参考:ngx_http_upstream_process_header
    //缓存文件中前面部分格式:[ngx_http_file_cache_header_t]["\nKEY: "][orig_key]["\n"][header]

    //头部部分读取错误
    if ((size_t) n < c->header_start) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      "cache file \"%s\" is too small", c->file.name.data);
        return NGX_DECLINED; //如果返回这个NGX_DECLINED，会把cached置0，返回出去后只有从后端从新获取数据
    }

    //[ngx_http_file_cache_header_t]["\nKEY: "][orig_key]["\n"][header]
    h = (ngx_http_file_cache_header_t *) c->buf->pos;

    if (h->version != NGX_HTTP_CACHE_VERSION) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "cache file \"%s\" version mismatch", c->file.name.data);
        return NGX_DECLINED; //如果返回这个NGX_DECLINED，会把cached置0，返回出去后只有从后端从新获取数据
    }

    if (h->crc32 != c->crc32) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      "cache file \"%s\" has md5 collision", c->file.name.data);
        return NGX_DECLINED; //如果返回这个NGX_DECLINED，会把cached置0，返回出去后只有从后端从新获取数据
    }

    if ((size_t) h->body_start > c->body_start) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      "cache file \"%s\" has too long header",
                      c->file.name.data);
        return NGX_DECLINED; //如果返回这个NGX_DECLINED，会把cached置0，返回出去后只有从后端从新获取数据
    }

    if (h->vary_len > NGX_HTTP_CACHE_VARY_LEN) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      "cache file \"%s\" has incorrect vary length",
                      c->file.name.data);
        return NGX_DECLINED; //如果返回这个NGX_DECLINED，会把cached置0，返回出去后只有从后端从新获取数据
    }

    if (h->vary_len) {
        ngx_http_file_cache_vary(r, h->vary, h->vary_len, c->variant);

        if (ngx_memcmp(c->variant, h->variant, NGX_HTTP_CACHE_KEY_LEN) != 0) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http file cache vary mismatch");
            return ngx_http_file_cache_reopen(r, c);
        }
    }

    c->buf->last += n; //移动last指针

    c->valid_sec = h->valid_sec;
    c->last_modified = h->last_modified;
    c->date = h->date;
    c->valid_msec = h->valid_msec;
    c->header_start = h->header_start;
    c->body_start = h->body_start;
    c->etag.len = h->etag_len;
    c->etag.data = h->etag;

    r->cached = 1;

    cache = c->file_cache;

    if (cache->sh->cold) {

        ngx_shmtx_lock(&cache->shpool->mutex);

        if (!c->node->exists) {
            c->node->uses = 1;
            c->node->body_start = c->body_start;
            c->node->exists = 1;
            c->node->uniq = c->uniq;
            c->node->fs_size = c->fs_size;

            cache->sh->size += c->fs_size;
        }

        ngx_shmtx_unlock(&cache->shpool->mutex);
    }

    now = ngx_time();

    if (c->valid_sec < now) { //判断该缓存是否过期

        ngx_shmtx_lock(&cache->shpool->mutex);

        if (c->node->updating) {
            rc = NGX_HTTP_CACHE_UPDATING;

        } else { //表示自己是第一个发现该缓存过期的客户端请求，因此自己需要从后端从新获取
            c->node->updating = 1;//客户端请求到nginx后，发现缓存过期，则会重新从后端获取数据，updating置1，见ngx_http_file_cache_read
            c->updating = 1;
            c->lock_time = c->node->lock_time;
            rc = NGX_HTTP_CACHE_STALE;
        }

        ngx_shmtx_unlock(&cache->shpool->mutex);

        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http file cache expired: %i %T %T",
                       rc, c->valid_sec, now);

        return rc;
    }

    return NGX_OK;
}

/*
    root@root:/var/yyz# cat cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f   封包过程见ngx_http_file_cache_set_header
     3hwhdBw
     KEY: /test2.php
     
     X-Powered-By: PHP/5.2.13
     Content-type: text/html
    //body_start就是上面这一段内存内容长度
    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    
     <Html> 
     <title>file update</title>
     <body> 
     <form method="post" action="" enctype="multipart/form-data">
     <input type="file" name="file" /> 
     <input type="submit" value="submit" /> 
     </form> 
     </body> 
     </html>
*/ 
//读取缓存文件中前面[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header]部分的内容长度空间,也就是
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@前面的内容

/*
发送缓存文件中内容到客户端过程:
 ngx_http_file_cache_open->ngx_http_file_cache_read->ngx_http_file_cache_aio_read这个流程获取文件中前面的头部信息相关内容，并获取整个
 文件stat信息，例如文件大小等。
 头部部分在ngx_http_cache_send->ngx_http_send_header发送，
 缓存文件后面的包体部分在ngx_http_cache_send后半部代码中触发在filter模块中发送

 接收后端数据并转发到客户端触发数据发送过程:
 ngx_event_pipe_write_to_downstream中的
 if (p->upstream_eof || p->upstream_error || p->upstream_done) {
    遍历p->in 或者遍历p->out，然后执行输出
    p->output_filter(p->output_ctx, p->out);
 }
 */

/* 读取缓存文件中前面的[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header] */
//注意置读取前面的头部信息，紧跟后面的后端应答回来的缓存包体是没有读取的
static ssize_t
ngx_http_file_cache_aio_read(ngx_http_request_t *r, ngx_http_cache_t *c)
{
//注意这里读取缓存文件中的头部部分的时候，只有aio读取或者缓存方式读取，和sendfile没有关系，因为头部读出来需要重新组装发往客户端的头部行信息，必须从文件读到内存中
#if (NGX_HAVE_FILE_AIO || NGX_THREADS)
    ssize_t                    n;
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
#endif

#if (NGX_HAVE_FILE_AIO)

    if (clcf->aio == NGX_HTTP_AIO_ON && ngx_file_aio) { //aio on这这里  aio on | off | threads[=pool]; 
        n = ngx_file_aio_read(&c->file, c->buf->pos, c->body_start, 0, r->pool);

        if (n != NGX_AGAIN) {
            c->reading = 0;
            return n;
        }

        c->reading = 1;

        c->file.aio->data = r;
        c->file.aio->handler = ngx_http_cache_aio_event_handler;

        r->main->blocked++;
        r->aio = 1;

        return NGX_AGAIN;
    }

#endif

#if (NGX_THREADS)

    if (clcf->aio == NGX_HTTP_AIO_THREADS) { //aio thread配置的时候走这里  aio on | off | threads[=pool]; 
        c->file.thread_handler = ngx_http_cache_thread_handler;
        c->file.thread_ctx = r;

        n = ngx_thread_read(&c->thread_task, &c->file, c->buf->pos,
                            c->body_start, 0, r->pool); //只是读取缓冲区文件中前面的头部信息部分

        c->reading = (n == NGX_AGAIN);

        return n;
    }

#endif

    /*
     ngx_http_file_cache_open->ngx_http_file_cache_read->ngx_http_file_cache_aio_read这个流程获取文件中前面的头部信息相关内容，并获取整个
     文件stat信息，例如文件大小等。
     头部部分在ngx_http_cache_send->ngx_http_send_header发送，
     缓存文件后面的包体部分在ngx_http_cache_send后半部代码中触发在filter模块中发送
     */


    /* 读取缓存文件中前面的[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header] */
    return  ngx_read_file(&c->file, c->buf->pos, c->body_start, 0);
    /*c->buf->last += ret;
    ngx_log_debugall(r->connection->log, 0, "YANG TEST ......@@@@@@@@@......%d, ret:%uz", 
        (int)(c->buf->last - c->buf->pos), ret);
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "%*s", (size_t) (c->buf->last - c->buf->pos), c->buf->pos);

    c->buf->last -= ret;
    return ret;*/
}


#if (NGX_HAVE_FILE_AIO)

static void
ngx_http_cache_aio_event_handler(ngx_event_t *ev)
{
    ngx_event_aio_t     *aio;
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

    aio = ev->data;
    r = aio->data;
    c = r->connection;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http file cache aio on: \"%V?%V\"", &r->uri, &r->args);

    r->main->blocked--;
    r->aio = 0;

    r->write_event_handler(r);

    ngx_http_run_posted_requests(c);
}

#endif


#if (NGX_THREADS)

////aio thread配置的时候走这里  aio on | off | threads[=pool]; 
//这里添加task->event信息到task中，当task->handler指向完后，通过nginx_notify可以继续通过epoll_wait返回执行task->event
static ngx_int_t
ngx_http_cache_thread_handler(ngx_thread_task_t *task, ngx_file_t *file)
{ //由ngx_thread_read触发执行
    ngx_str_t                  name;
    ngx_thread_pool_t         *tp;
    ngx_http_request_t        *r;
    ngx_http_core_loc_conf_t  *clcf;

    r = file->thread_ctx;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    tp = clcf->thread_pool;

    if (tp == NULL) {
        if (ngx_http_complex_value(r, clcf->thread_pool_value, &name)
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        tp = ngx_thread_pool_get((ngx_cycle_t *) ngx_cycle, &name);

        if (tp == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "thread pool \"%V\" not found", &name);
            return NGX_ERROR;
        }
    }

    task->event.data = r;
    task->event.handler = ngx_http_cache_thread_event_handler;

    if (ngx_thread_task_post(tp, task) != NGX_OK) { //该任务的handler函数式task->handler = ngx_thread_read_handler;
        return NGX_ERROR;
    }

    r->main->blocked++;
    r->aio = 1;

    return NGX_OK;
}

static void
ngx_http_cache_thread_event_handler(ngx_event_t *ev)
{//在ngx_notify(ngx_thread_pool_handler); 中的ngx_thread_pool_handler执行该函数，表示线程读文件完成，通过ngx_notify epoll方式触发
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

    r = ev->data;
    c = r->connection;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http file cache aio thread: \"%V?%V\"", &r->uri, &r->args);

    r->main->blocked--;
    r->aio = 0;

    r->write_event_handler(r);

    ngx_http_run_posted_requests(c);  
}

#endif

/*
  同一个客户端请求r只拥有一个r->ngx_http_cache_t和r->ngx_http_cache_t->ngx_http_file_cache_t结构，同一个客户端可能会请求后端的多个uri，
  则在向后端发起请求前，在ngx_http_file_cache_open->ngx_http_file_cache_exists中会按照proxy_cache_key $scheme$proxy_host$request_uri计算出来的
  MD5来创建对应的红黑树节点，然后添加到ngx_http_file_cache_t->sh->rbtree红黑树中。所以不同的客户端uri会有不同的node节点存在于红黑树中
*/

//http://www.tuicool.com/articles/QnMNr23
//查找红黑树cache->sh->rbtree中的节点ngx_http_file_cache_node_t，没找到则创建响应的ngx_http_file_cache_node_t节点添加到红黑树中
static ngx_int_t
ngx_http_file_cache_exists(ngx_http_file_cache_t *cache, ngx_http_cache_t *c)
{
    ngx_int_t                    rc;
    ngx_http_file_cache_node_t  *fcn;

    ngx_shmtx_lock(&cache->shpool->mutex);

    fcn = c->node;//后面没找到则会创建node节点
   
    if (fcn == NULL) {
        fcn = ngx_http_file_cache_lookup(cache, c->key); //以 c->key 为查找条件从缓存中查找缓存节点： 
    }

    if (fcn) { //cache中存在该key
        ngx_queue_remove(&fcn->queue);

        //该客户端在新建连接后，如果之前有缓存该文件，则c->node为NULL，表示这个连接请求第一次走到这里，有一个客户端在获取数据，如果在
        //连接范围内(还没有断开连接)多次获取该缓存文件，则也只会加1，表示当前有多少个客户端连接在获取该缓存
        if (c->node == NULL) { //如果该请求第一次使用此缓存节点，则增加相关引用和使用次数
            fcn->uses++;
            fcn->count++;
        }

        if (fcn->error) {

            if (fcn->valid_sec < ngx_time()) {
                goto renew; //缓存已过期
            }

            rc = NGX_OK;

            goto done;
        }

        if (fcn->exists || fcn->uses >= c->min_uses) { //该请求的缓存已经存在，并且对该缓存的请求次数达到了最低要求次数min_uses
            //表示该缓存文件是否存在，Proxy_cache_min_uses 3，则第3次后开始获取后端数据，获取完毕后在ngx_http_file_cache_update中置1，但是只有在地4次请求的时候才会在ngx_http_file_cache_exists赋值为1
            c->exists = fcn->exists;
            if (fcn->body_start) {
                c->body_start = fcn->body_start;
            }

            rc = NGX_OK;

            goto done;
        }

        //例如配置Proxy_cache_min_uses 5，则需要客户端请求5才才能从缓存中取，如果现在只有4次，则都需要从后端获取数据
        rc = NGX_AGAIN;

        goto done;
    }

    //没找到，则在下面创建node节点，添加到ngx_http_file_cache_t->sh->rbtree红黑树中
    fcn = ngx_slab_calloc_locked(cache->shpool,
                                 sizeof(ngx_http_file_cache_node_t));
    if (fcn == NULL) {
        ngx_shmtx_unlock(&cache->shpool->mutex);

        (void) ngx_http_file_cache_forced_expire(cache);

        ngx_shmtx_lock(&cache->shpool->mutex);

        fcn = ngx_slab_calloc_locked(cache->shpool,
                                     sizeof(ngx_http_file_cache_node_t));
        if (fcn == NULL) {
            ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0,
                          "could not allocate node%s", cache->shpool->log_ctx);
            rc = NGX_ERROR;
            goto failed;
        }
    }

    ngx_memcpy((u_char *) &fcn->node.key, c->key, sizeof(ngx_rbtree_key_t));

    ngx_memcpy(fcn->key, &c->key[sizeof(ngx_rbtree_key_t)],
               NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t));

    ngx_rbtree_insert(&cache->sh->rbtree, &fcn->node); //把该节点添加到红黑树中

    fcn->uses = 1;
    fcn->count = 1;

renew:

    rc = NGX_DECLINED; //uri第一次请求的时候创建node节点，同时返回NGX_DECLINED。或者缓存过期需要把该节点相关信息恢复为默认值

    fcn->valid_msec = 0;
    fcn->error = 0;
    fcn->exists = 0;
    fcn->valid_sec = 0;
    fcn->uniq = 0;
    fcn->body_start = 0;
    fcn->fs_size = 0;

done:

    fcn->expire = ngx_time() + cache->inactive;

    ngx_queue_insert_head(&cache->sh->queue, &fcn->queue); //新创建的node节点添加到cache->sh->queue头部

    c->uniq = fcn->uniq;//文件的uniq  赋值见ngx_http_file_cache_update
    c->error = fcn->error;
    c->node = fcn; //把新创建的fcn赋值给c->node

failed:

    ngx_shmtx_unlock(&cache->shpool->mutex);

    return rc;
}

//为后端应答回来的数据创建缓存文件用该函数获取缓存文件名，客户端请求过来后，也是采用该函数获取缓存文件名，只要
//proxy_cache_key $scheme$proxy_host$request_uri配置中的变量对应的值一样，则获取到的文件名肯定是一样的，即使是不同的客户端r，参考ngx_http_file_cache_name
//因为不同客户端的proxy_cache_key配置的对应变量value一样，则他们计算出来的ngx_http_cache_s->key[]也会一样，他们的在红黑树和queue队列中的
//node节点也会是同一个，参考ngx_http_file_cache_lookup
static ngx_int_t
ngx_http_file_cache_name(ngx_http_request_t *r, ngx_path_t *path) //获取缓存名
{
    u_char            *p;
    ngx_http_cache_t  *c;

    c = r->cache;

    if (c->file.name.len) {
        return NGX_OK;
    }

    c->file.name.len = path->name.len + 1 + path->len
                       + 2 * NGX_HTTP_CACHE_KEY_LEN;

    c->file.name.data = ngx_pnalloc(r->pool, c->file.name.len + 1);
    if (c->file.name.data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(c->file.name.data, path->name.data, path->name.len); //XXX_cache_path 指定的路径

    //跳过level，在后面的ngx_create_hashed_filename添加到内存中
    p = c->file.name.data + path->name.len + 1 + path->len; //   /cache/0/8d/
    p = ngx_hex_dump(p, c->key, NGX_HTTP_CACHE_KEY_LEN); //16进制key转换为字符串拷贝到cache缓存目录file中
    *p = '\0';

    //通过从配置文件中的path，得到完整路径，ngx_create_hashed_filename是填充level路径
    ngx_create_hashed_filename(path, c->file.name.data, c->file.name.len);

    //cache file: "/var/yyz/cache_xxx/c/c1/13cc494353644acaed96a080cac13c1c"
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "cache file: \"%s\"", c->file.name.data);

    return NGX_OK;
}

/*
为后端应答回来的数据创建缓存文件用该函数获取缓存文件名，客户端请求过来后，也是采用该函数获取缓存文件名，只要
proxy_cache_key $scheme$proxy_host$request_uri配置中的变量对应的值一样，则获取到的文件名肯定是一样的，即使是不同的客户端r，参考ngx_http_file_cache_name
因为不同客户端的proxy_cache_key配置的对应变量value一样，则他们计算出来的ngx_http_cache_s->key[]也会一样，他们的在红黑树和queue队列中的
node节点也会是同一个，参考ngx_http_file_cache_lookup  
*/

//参考nginx proxy cache分析 http://blog.csdn.net/xiaolang85/article/details/38260041 图解
static ngx_http_file_cache_node_t *
ngx_http_file_cache_lookup(ngx_http_file_cache_t *cache, u_char *key)
{
    ngx_int_t                    rc;
    ngx_rbtree_key_t             node_key;
    ngx_rbtree_node_t           *node, *sentinel;
    ngx_http_file_cache_node_t  *fcn;

    ngx_memcpy((u_char *) &node_key, key, sizeof(ngx_rbtree_key_t)); //拷贝key的前面4个字符

    node = cache->sh->rbtree.root; //红黑树跟节点
    sentinel = cache->sh->rbtree.sentinel; //哨兵节点

    while (node != sentinel) {

        if (node_key < node->key) {
            node = node->left;
            continue;
        }

        if (node_key > node->key) {
            node = node->right;
            continue;
        }

        /* node_key == node->key */

        fcn = (ngx_http_file_cache_node_t *) node;

        rc = ngx_memcmp(&key[sizeof(ngx_rbtree_key_t)], fcn->key,
                        NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t)); 
                        //比较内容是从key的NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t)开始比较

        if (rc == 0) {
            return fcn;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    /* not found */

    return NULL;
}


static void
ngx_http_file_cache_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t           **p;
    ngx_http_file_cache_node_t   *cn, *cnt;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            cn = (ngx_http_file_cache_node_t *) node;
            cnt = (ngx_http_file_cache_node_t *) temp;

            p = (ngx_memcmp(cn->key, cnt->key,
                            NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t))
                 < 0)
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


static void
ngx_http_file_cache_vary(ngx_http_request_t *r, u_char *vary, size_t len,
    u_char *hash)
{
    u_char     *p, *last;
    ngx_str_t   name;
    ngx_md5_t   md5;
    u_char      buf[NGX_HTTP_CACHE_VARY_LEN];

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache vary: \"%*s\"", len, vary);

    ngx_md5_init(&md5);
    ngx_md5_update(&md5, r->cache->main, NGX_HTTP_CACHE_KEY_LEN);

    ngx_strlow(buf, vary, len);

    p = buf;
    last = buf + len;

    while (p < last) {

        while (p < last && (*p == ' ' || *p == ',')) { p++; }

        name.data = p;

        while (p < last && *p != ',' && *p != ' ') { p++; }

        name.len = p - name.data;

        if (name.len == 0) {
            break;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http file cache vary: %V", &name);

        ngx_md5_update(&md5, name.data, name.len);
        ngx_md5_update(&md5, (u_char *) ":", sizeof(":") - 1);

        ngx_http_file_cache_vary_header(r, &md5, &name);

        ngx_md5_update(&md5, (u_char *) CRLF, sizeof(CRLF) - 1);
    }

    ngx_md5_final(hash, &md5);
}


static void
ngx_http_file_cache_vary_header(ngx_http_request_t *r, ngx_md5_t *md5,
    ngx_str_t *name)
{
    size_t            len;
    u_char           *p, *start, *last;
    ngx_uint_t        i, multiple, normalize;
    ngx_list_part_t  *part;
    ngx_table_elt_t  *header;

    multiple = 0;
    normalize = 0;

    if (name->len == sizeof("Accept-Charset") - 1
        && ngx_strncasecmp(name->data, (u_char *) "Accept-Charset",
                           sizeof("Accept-Charset") - 1) == 0)
    {
        normalize = 1;

    } else if (name->len == sizeof("Accept-Encoding") - 1
        && ngx_strncasecmp(name->data, (u_char *) "Accept-Encoding",
                           sizeof("Accept-Encoding") - 1) == 0)
    {
        normalize = 1;

    } else if (name->len == sizeof("Accept-Language") - 1
        && ngx_strncasecmp(name->data, (u_char *) "Accept-Language",
                           sizeof("Accept-Language") - 1) == 0)
    {
        normalize = 1;
    }

    part = &r->headers_in.headers.part;
    header = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            header = part->elts;
            i = 0;
        }

        if (header[i].hash == 0) {
            continue;
        }

        if (header[i].key.len != name->len) {
            continue;
        }

        if (ngx_strncasecmp(header[i].key.data, name->data, name->len) != 0) {
            continue;
        }

        if (!normalize) {

            if (multiple) {
                ngx_md5_update(md5, (u_char *) ",", sizeof(",") - 1);
            }

            ngx_md5_update(md5, header[i].value.data, header[i].value.len);

            multiple = 1;

            continue;
        }

        /* normalize spaces */

        p = header[i].value.data;
        last = p + header[i].value.len;

        while (p < last) {

            while (p < last && (*p == ' ' || *p == ',')) { p++; }

            start = p;

            while (p < last && *p != ',' && *p != ' ') { p++; }

            len = p - start;

            if (len == 0) {
                break;
            }

            if (multiple) {
                ngx_md5_update(md5, (u_char *) ",", sizeof(",") - 1);
            }

            ngx_md5_update(md5, start, len);

            multiple = 1;
        }
    }
}


static ngx_int_t
ngx_http_file_cache_reopen(ngx_http_request_t *r, ngx_http_cache_t *c)
{
    ngx_http_file_cache_t  *cache;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->file.log, 0,
                   "http file cache reopen");

    if (c->secondary) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      "cache file \"%s\" has incorrect vary hash",
                      c->file.name.data);
        return NGX_DECLINED;
    }

    cache = c->file_cache;

    ngx_shmtx_lock(&cache->shpool->mutex);

    c->node->count--;
    c->node = NULL;

    ngx_shmtx_unlock(&cache->shpool->mutex);

    c->secondary = 1;
    c->file.name.len = 0;
    c->body_start = c->buf->end - c->buf->start;

    ngx_memcpy(c->key, c->variant, NGX_HTTP_CACHE_KEY_LEN);

    return ngx_http_file_cache_open(r);
}

/*
root@root:/var/yyz/cache_xxx# cat b/7d/bf6813c2bc0becb369a8d8367b6b77db 
oV尛oVZ"  
KEY: /test.php
IX-Powered-By: PHP/5.2.13
Content-type: text/html

//下面才是真正的文件内容
<Html> 
<Head> 
<title>Your page Subject and domain name</title>

<Meta NAME="" CONTENT="">
"" your others meta tagB
"" your others meta tag
"" your others meta tag
"" your others meta tag
"" your others meta tag
"" your others meta tag
"" your others meta tag

*/

//初始化缓存文件包头： 
ngx_int_t  //赋值[ngx_http_file_cache_header_t]["\nKEY: "][orig_key]["\n"][header][body]中的[orig_key]["\n"]，注意这时候还没有赋值[header]
ngx_http_file_cache_set_header(ngx_http_request_t *r, u_char *buf)
{
    //实际在接收后端第一个头部行相关信息的时候，会预留u->buffer.pos += r->cache->header_start;字节，见ngx_http_upstream_process_header
    ngx_http_file_cache_header_t  *h = (ngx_http_file_cache_header_t *) buf; 

    u_char            *p;
    ngx_str_t         *key;
    ngx_uint_t         i;
    ngx_http_cache_t  *c;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache set header");

    c = r->cache;

    ngx_memzero(h, sizeof(ngx_http_file_cache_header_t));

    h->version = NGX_HTTP_CACHE_VERSION;
    h->valid_sec = c->valid_sec;
    h->last_modified = c->last_modified;
    h->date = c->date;
    h->crc32 = c->crc32;
    h->valid_msec = (u_short) c->valid_msec;
    h->header_start = (u_short) c->header_start;
    h->body_start = (u_short) c->body_start;

    if (c->etag.len <= NGX_HTTP_CACHE_ETAG_LEN) {
        h->etag_len = (u_char) c->etag.len;
        ngx_memcpy(h->etag, c->etag.data, c->etag.len);
    }

    if (c->vary.len) {
        if (c->vary.len > NGX_HTTP_CACHE_VARY_LEN) {
            /* should not happen */
            c->vary.len = NGX_HTTP_CACHE_VARY_LEN;
        }

        h->vary_len = (u_char) c->vary.len;
        ngx_memcpy(h->vary, c->vary.data, c->vary.len);

        ngx_http_file_cache_vary(r, c->vary.data, c->vary.len, c->variant);
        ngx_memcpy(h->variant, c->variant, NGX_HTTP_CACHE_KEY_LEN);
    }

    if (ngx_http_file_cache_update_variant(r, c) != NGX_OK) {
        return NGX_ERROR;
    }

    p = buf + sizeof(ngx_http_file_cache_header_t);

    //[ngx_http_file_cache_header_t]["\nKEY: "][orig_key]["\n"][header][body]中的KEY
    p = ngx_cpymem(p, ngx_http_file_cache_key, sizeof(ngx_http_file_cache_key)); 

    //proxy_cache_key $scheme$proxy_host$request_uri中的各个字符串解析出来放在KEY: 后面
    key = c->keys.elts;
    for (i = 0; i < c->keys.nelts; i++) { //[ngx_http_file_cache_header_t]["\nKEY: "][orig_key]["\n"][header][body]中的orig_key
        p = ngx_copy(p, key[i].data, key[i].len);
    }

    *p = LF;

    return NGX_OK;
}


static ngx_int_t
ngx_http_file_cache_update_variant(ngx_http_request_t *r, ngx_http_cache_t *c)
{
    ngx_http_file_cache_t  *cache;

    if (!c->secondary) {
        return NGX_OK;
    }

    if (c->vary.len
        && ngx_memcmp(c->variant, c->key, NGX_HTTP_CACHE_KEY_LEN) == 0)
    {
        return NGX_OK;
    }

    /*
     * if the variant hash doesn't match one we used as a secondary
     * cache key, switch back to the original key
     */

    cache = c->file_cache;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache main key");

    ngx_shmtx_lock(&cache->shpool->mutex);

    c->node->count--;
    c->node->updating = 0;
    c->node = NULL;

    ngx_shmtx_unlock(&cache->shpool->mutex);

    c->file.name.len = 0;

    ngx_memcpy(c->key, c->main, NGX_HTTP_CACHE_KEY_LEN);

    if (ngx_http_file_cache_exists(cache, c) == NGX_ERROR) {
        return NGX_ERROR;
    }

    if (ngx_http_file_cache_name(r, cache->path) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

/*ngx_http_upstream_init_request->ngx_http_upstream_cache 客户端获取缓存 后端应答回来数据后在ngx_http_upstream_send_response->ngx_http_file_cache_create
中创建临时文件，然后在ngx_event_pipe_write_chain_to_temp_file把读取的后端数据写入临时文件，最后在
ngx_http_upstream_send_response->ngx_http_upstream_process_request->ngx_http_file_cache_update中把临时文件内容rename(相当于mv)到proxy_cache_path指定
的cache目录下面
*/
void /*后端数据读取完毕，并且全部写入临时文件后才会执行rename过程，为什么需要临时文件的原因是:例如之前的缓存过期了，现在有个请求正在从后端
//获取数据写入临时文件，如果是直接写入缓存文件，则在获取后端数据过程中，如果在来一个客户端请求，如果允许proxy_cache_use_stale updating，则
//后面的请求可以直接获取之前老旧的过期缓存，从而可以避免冲突(前面的请求写文件，后面的请求获取文件内容) */
ngx_http_file_cache_update(ngx_http_request_t *r, ngx_temp_file_t *tf)
{
    off_t                   fs_size;
    ngx_int_t               rc;
    ngx_file_uniq_t         uniq;
    ngx_file_info_t         fi;
    ngx_http_cache_t        *c;
    ngx_ext_rename_file_t   ext;
    ngx_http_file_cache_t  *cache;

    c = r->cache;

    if (c->updated) {
        return;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache update, c->body_start:%uz", c->body_start);

    cache = c->file_cache;

    c->updated = 1;
    c->updating = 0;

    uniq = 0;
    fs_size = 0;

//http file cache rename: "/var/yyz/cache_xxx/temp/1/00/0000000001" to "/var/yyz/cache_xxx/c/c1/13cc494353644acaed96a080cac13c1c"
    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache rename: \"%s\" to \"%s\", expire time:%M",
                   tf->file.name.data, c->file.name.data, r->cache->file_cache->inactive);

    ext.access = NGX_FILE_OWNER_ACCESS;
    ext.path_access = NGX_FILE_OWNER_ACCESS;
    ext.time = -1;
    ext.create_path = 1;
    ext.delete_file = 1;
    ext.log = r->connection->log;

    //临时文件中的内容到指定的cache目录下
    rc = ngx_ext_rename_file(&tf->file.name, &c->file.name, &ext);

    if (rc == NGX_OK) {
        //获取to对应的cache文件的文件状态特性
        if (ngx_fd_info(tf->file.fd, &fi) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                          ngx_fd_info_n " \"%s\" failed", tf->file.name.data);

            rc = NGX_ERROR;

        } else { //获取文件状态信息成功后，获取uniq
            uniq = ngx_file_uniq(&fi); //文件inode节点号
            fs_size = (ngx_file_fs_size(&fi) + cache->bsize - 1) / cache->bsize; //缓存文件内容cache->bsize字节对齐
        }
    }

    ngx_shmtx_lock(&cache->shpool->mutex);

    //在获取后端数据前，首先会会查找缓存是否有缓存该请求数据，如果没有，则会在ngx_http_file_cache_open中创建node
    c->node->count--;
    c->node->uniq = uniq;
    c->node->body_start = c->body_start;

    cache->sh->size += fs_size - c->node->fs_size; //文件中本次从后端读取的数据大小为文件总大小-之前文件中已经缓存的，例如可能多次epoll获取后端数据
    c->node->fs_size = fs_size;

    if (rc == NGX_OK) {
        c->node->exists = 1; //前面rename成功后，该缓存文件肯定就存在了，标识一下
    }

    c->node->updating = 0;

    ngx_shmtx_unlock(&cache->shpool->mutex);
}


void
ngx_http_file_cache_update_header(ngx_http_request_t *r)
{
    ssize_t                        n;
    ngx_err_t                      err;
    ngx_file_t                     file;
    ngx_file_info_t                fi;
    ngx_http_cache_t              *c;
    ngx_http_file_cache_header_t   h;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache update header");

    c = r->cache;

    ngx_memzero(&file, sizeof(ngx_file_t));

    file.name = c->file.name;
    file.log = r->connection->log;
    file.fd = ngx_open_file(file.name.data, NGX_FILE_RDWR, NGX_FILE_OPEN, 0);

    if (file.fd == NGX_INVALID_FILE) {
        err = ngx_errno;

        /* cache file may have been deleted */

        if (err == NGX_ENOENT) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http file cache \"%s\" not found",
                           file.name.data);
            return;
        }

        ngx_log_error(NGX_LOG_CRIT, r->connection->log, err,
                      ngx_open_file_n " \"%s\" failed", file.name.data);
        return;
    }

    /*
     * make sure cache file wasn't replaced;
     * if it was, do nothing
     */

    if (ngx_fd_info(file.fd, &fi) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                      ngx_fd_info_n " \"%s\" failed", file.name.data);
        goto done;
    }

    if (c->uniq != ngx_file_uniq(&fi)
        || c->length != ngx_file_size(&fi))
    {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http file cache \"%s\" changed",
                       file.name.data);
        goto done;
    }

    n = ngx_read_file(&file, (u_char *) &h,
                      sizeof(ngx_http_file_cache_header_t), 0);

    if (n == NGX_ERROR) {
        goto done;
    }

    if ((size_t) n != sizeof(ngx_http_file_cache_header_t)) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      ngx_read_file_n " read only %z of %z from \"%s\"",
                      n, sizeof(ngx_http_file_cache_header_t), file.name.data);
        goto done;
    }

    if (h.version != NGX_HTTP_CACHE_VERSION
        || h.last_modified != c->last_modified
        || h.crc32 != c->crc32
        || h.header_start != c->header_start
        || h.body_start != c->body_start)
    {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http file cache \"%s\" content changed",
                       file.name.data);
        goto done;
    }

    /*
     * update cache file header with new data,
     * notably h.valid_sec and h.date
     */

    ngx_memzero(&h, sizeof(ngx_http_file_cache_header_t));

    h.version = NGX_HTTP_CACHE_VERSION;
    h.valid_sec = c->valid_sec;
    h.last_modified = c->last_modified;
    h.date = c->date;
    h.crc32 = c->crc32;
    h.valid_msec = (u_short) c->valid_msec;
    h.header_start = (u_short) c->header_start;
    h.body_start = (u_short) c->body_start;

    if (c->etag.len <= NGX_HTTP_CACHE_ETAG_LEN) {
        h.etag_len = (u_char) c->etag.len;
        ngx_memcpy(h.etag, c->etag.data, c->etag.len);
    }

    if (c->vary.len) {
        if (c->vary.len > NGX_HTTP_CACHE_VARY_LEN) {
            /* should not happen */
            c->vary.len = NGX_HTTP_CACHE_VARY_LEN;
        }

        h.vary_len = (u_char) c->vary.len;
        ngx_memcpy(h.vary, c->vary.data, c->vary.len);

        ngx_http_file_cache_vary(r, c->vary.data, c->vary.len, c->variant);
        ngx_memcpy(h.variant, c->variant, NGX_HTTP_CACHE_KEY_LEN);
    }

    (void) ngx_write_file(&file, (u_char *) &h,
                          sizeof(ngx_http_file_cache_header_t), 0);

done:

    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", file.name.data);
    }
}

/*
发送缓存文件中内容到客户端过程:
 ngx_http_file_cache_open->ngx_http_file_cache_read->ngx_http_file_cache_aio_read这个流程获取文件中前面的头部信息相关内容，并获取整个
 文件stat信息，例如文件大小等。
 头部部分在ngx_http_cache_send->ngx_http_send_header发送，
 缓存文件后面的包体部分在ngx_http_cache_send后半部代码中触发在filter模块中发送

 接收后端数据并转发到客户端触发数据发送过程:
 ngx_event_pipe_write_to_downstream中的
 if (p->upstream_eof || p->upstream_error || p->upstream_done) {
    遍历p->in 或者遍历p->out，然后执行输出
    p->output_filter(p->output_ctx, p->out);
 }
 */

/*
缓存文件除去文件前面头部部分，剩下的就是实际的包体数据，通过这里发送触发在ngx_http_write_filter->ngx_linux_sendfile_chain(如果文件通过sendfile发送)，
如果是普通写发送，则在ngx_http_write_filter->ngx_writev(一般chain->buf在内存中的情况下用该方式)，
或者ngx_http_copy_filter->ngx_output_chain中的if (ctx->aio) { return NGX_AGAIN;}(如果文件通过aio发送)，然后由aio异步事件epoll触发读取文件内容超过，然后在继续发送文件
*/ngx_int_t
ngx_http_cache_send(ngx_http_request_t *r)
{
    ngx_int_t          rc;
    ngx_buf_t         *b;
    ngx_chain_t        out;
    ngx_http_cache_t  *c;

    c = r->cache;

//http file cache send: /var/yyz/cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http file cache send: %s", c->file.name.data);

    if (r != r->main && c->length - c->body_start == 0) {
        return ngx_http_send_header(r);
    }

    /* we need to allocate all before the header would be sent */

    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
    if (b->file == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    

    rc = ngx_http_send_header(r); //先把头部行发送出去

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    //一下触发包体发送

    b->file_pos = c->body_start; //指向网页包体部分内容
    b->file_last = c->length; //包体末尾处，也就是文件尾部   

    b->in_file = (c->length - c->body_start) ? 1: 0;
    b->last_buf = (r == r->main) ? 1: 0;
    b->last_in_chain = 1;

    b->file->fd = c->file.fd;
    b->file->name = c->file.name;
    b->file->log = r->connection->log;

    out.buf = b;
    out.next = NULL;

    return ngx_http_output_filter(r, &out); //发送包体部分
}


void
ngx_http_file_cache_free(ngx_http_cache_t *c, ngx_temp_file_t *tf)
{
    ngx_http_file_cache_t       *cache;
    ngx_http_file_cache_node_t  *fcn;

    if (c->updated || c->node == NULL) {
        return;
    }

    cache = c->file_cache;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->file.log, 0,
                   "http file cache free, fd: %d", c->file.fd);

    ngx_shmtx_lock(&cache->shpool->mutex);

    fcn = c->node;
    fcn->count--;

    if (c->updating && fcn->lock_time == c->lock_time) {
        fcn->updating = 0;
    }

    if (c->error) {
        fcn->error = c->error;

        if (c->valid_sec) {
            fcn->valid_sec = c->valid_sec;
            fcn->valid_msec = c->valid_msec;
        }

    } else if (!fcn->exists && fcn->count == 0 && c->min_uses == 1) {
        ngx_queue_remove(&fcn->queue);
        ngx_rbtree_delete(&cache->sh->rbtree, &fcn->node);
        ngx_slab_free_locked(cache->shpool, fcn);
        c->node = NULL;
    }

    ngx_shmtx_unlock(&cache->shpool->mutex);

    c->updated = 1;
    c->updating = 0;

    if (c->temp_file) {
        if (tf && tf->file.fd != NGX_INVALID_FILE) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->file.log, 0,
                           "http file cache incomplete: \"%s\"",
                           tf->file.name.data);

            if (ngx_delete_file(tf->file.name.data) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_CRIT, c->file.log, ngx_errno,
                              ngx_delete_file_n " \"%s\" failed",
                              tf->file.name.data);
            }
        }
    }

    if (c->wait_event.timer_set) {
        ngx_del_timer(&c->wait_event, NGX_FUNC_LINE);
    }
}


static void
ngx_http_file_cache_cleanup(void *data)
{
    ngx_http_cache_t  *c = data;

    if (c->updated) {
        return;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->file.log, 0,
                   "http file cache cleanup");

    if (c->updating) {
        ngx_log_error(NGX_LOG_ALERT, c->file.log, 0,
                      "stalled cache updating, error:%ui", c->error);
    }

    ngx_http_file_cache_free(c, NULL);
}

/*
ngx_http_file_cache_expire，一个是ngx_http_file_cache_forced_expire，他们有什么区别呢，主要区别是这样子，前一个只有过期的cache
才会去尝试删除它(引用计数为0)，而后一个不管有没有过期，只要引用计数为0，就会去清理。来详细看这两个函数的实现。
*/
/*
然后是ngx_http_file_cache_forced_expire，顾名思义，就是强制删除cache 节点，它的返回值也是wait time，它的遍历也是从后到前的。
*/

/*
函数ngx_http_file_cache_forced_expire 从 inactive queue 队尾开始扫描，直到找到 
可以被清理的当前未使用节点 ( fcn->count == 0 且不论它是否过期) 或者查找了20 个节点后仍未找到符合条件的节点。 
*/
//删除过期的缓存
static time_t //这个一般是所有缓存文件大小超过最大限制了( xxx_cache_path  path max_size=size中的size),则调用该函数
ngx_http_file_cache_forced_expire(ngx_http_file_cache_t *cache)
{
    u_char                      *name;
    size_t                       len;
    time_t                       wait;
    ngx_uint_t                   tries;
    ngx_path_t                  *path;
    ngx_queue_t                 *q;
    ngx_http_file_cache_node_t  *fcn;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "http file cache forced expire");

    path = cache->path;
    
    //len表示缓存文件对应的全路径名称的长度
     /* 
     全路径的名称形式为:proxy_cache_path+'/'+根据level生成的子路径+16进制表示的MD5码，path->name.len + 1 ：表示proxy_cache_path+'/'的
     长度，path->len 表示根据 level生成的子路径的长度，2 * NGX_HTTP_CACHE_KEY_LEN 表示16进制表示的MD5码的长度，之所以是
     2 * NGX_HTTP_CACHE_KEY_LEN 是因此MD5码是16个字节，一个字节是8位，而一个16进制数字只需要4位表示，因此MD5码所占的位数为     
     16*8,转换成16进制的形式，所表示的字节的个数为16*8/2=16*2 = NGX_HTTP_CACHE_KEY_LEN * 2
     */
    len = path->name.len + 1 + path->len + 2 * NGX_HTTP_CACHE_KEY_LEN;

    name = ngx_alloc(len + 1, ngx_cycle->log);
    if (name == NULL) {
        return 10;
    }

    ngx_memcpy(name, path->name.data, path->name.len);

    wait = 10;
    tries = 20; //删除节点尝试次数

    ngx_shmtx_lock(&cache->shpool->mutex);

    for (q = ngx_queue_last(&cache->sh->queue);
         q != ngx_queue_sentinel(&cache->sh->queue);
         q = ngx_queue_prev(q))
    {
        fcn = ngx_queue_data(q, ngx_http_file_cache_node_t, queue);

        ngx_log_debug6(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                  "http file cache forced expire: #%d %d %02xd%02xd%02xd%02xd",
                  fcn->count, fcn->exists,
                  fcn->key[0], fcn->key[1], fcn->key[2], fcn->key[3]);

        if (fcn->count == 0) {
            //如果引用计数为0则删除cache
            ngx_http_file_cache_delete(cache, q, name);
            wait = 0;

        } else {
            //否则尝试20次
            if (--tries) {
                continue;
            }

            wait = 1;
        }

        break;
    }

    ngx_shmtx_unlock(&cache->shpool->mutex);

    ngx_free(name);

    return wait;
}

//参考nginx proxy cache分析 http://blog.csdn.net/xiaolang85/article/details/38260041 图解
/*
ngx_http_file_cache_expire，一个是ngx_http_file_cache_forced_expire，他们有什么区别呢，主要区别是这样子，前一个只有过期的cache
才会去尝试删除它(引用计数为0)，而后一个不管有没有过期，只要引用计数为0，就会去清理。来详细看这两个函数的实现。
*/

/*
首先是ngx_http_file_cache_expire，这里注意nginx使用了LRU，也就是队列最尾端保存的是最长时间没有被使用的，并且这个函数返回的就是
一个wait值，这个值的计算不知道为什么nginx会设置为10ms，我觉得这个值设置为可调或许更好。
*/

/*
缓存文件stat状态信息ngx_cached_open_file_s(ngx_open_file_cache_t->rbtree(expire_queue)的成员   )在ngx_expire_old_cached_files进行失效判断, 
缓存文件内容信息(实实在在的文件信息)ngx_http_file_cache_node_t(ngx_http_file_cache_s->sh中的成员)在ngx_http_file_cache_expire进行失效判断。
*/

//删除过期缓存 ngx_http_file_cache_expire 函数清除过期缓存条目 (删除其占用的共享内存 和对应的磁盘文件)。 
static time_t //ngx_http_file_cache_expire和ngx_http_file_cache_add对应
ngx_http_file_cache_expire(ngx_http_file_cache_t *cache)
{ //最少返回值是10，也就是最短超时进行老化操作的时间是10s,即使限制缓存中有节点还有5s就过期了，但是我们还是在10s的时候进行清除
//然如果最末尾的缓存文件正在被删除，则返回1
    u_char                      *name, *p;
    size_t                       len;
    time_t                       now, wait;
    ngx_path_t                  *path;
    ngx_queue_t                 *q;
    ngx_http_file_cache_node_t  *fcn;
    u_char                       key[2 * NGX_HTTP_CACHE_KEY_LEN];

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "http file cache expire");

    path = cache->path;
    
    //len表示缓存文件对应的全路径名称的长度
    
    /* 
        全路径的名称形式为:proxy_cache_path+'/'+根据level生成的子路径+16进制表示的MD5码，path->name.len + 1 ：表示
        proxy_cache_path+'/'的长度，path->len 表示根据 level生成的子路径的长度，2 * NGX_HTTP_CACHE_KEY_LEN 表示16进制表示的
        MD5码的长度，之所以是2 * NGX_HTTP_CACHE_KEY_LEN 是因此MD5码是16个字节，一个字节是8位，而一个16进制数字只需要4位表示，
        因此MD5码所占的位数为16*8,转换成16进制的形式，所表示的字节的个数为16*8/2=16*2 = NGX_HTTP_CACHE_KEY_LEN * 2
    */
    len = path->name.len + 1 + path->len + 2 * NGX_HTTP_CACHE_KEY_LEN;

    name = ngx_alloc(len + 1, ngx_cycle->log);
    if (name == NULL) {
        return 10;
    }

    ngx_memcpy(name, path->name.data, path->name.len);

    now = ngx_time();

    ngx_shmtx_lock(&cache->shpool->mutex); //必须加锁，多进程环境避免同时对共享内存操作

    for ( ;; ) {

        if (ngx_quit || ngx_terminate) {
            wait = 1;
            break;
        }

        if (ngx_queue_empty(&cache->sh->queue)) {
            //如果cache队列为空，则直接退出返回
            wait = 10;//最少返回值是10，也就是最短超时进行老化操作的时间是10s,即使限制缓存中有节点还有5s就过期了，但是我们还是在10s的时候进行清除
            break;
        }
        
        //取得过期队列的最后一个节点
        q = ngx_queue_last(&cache->sh->queue);
        
        //获得过期队列节点对应的ngx_http_file_cache_node_t节点的地址
        fcn = ngx_queue_data(q, ngx_http_file_cache_node_t, queue);

        wait = fcn->expire - now;
        
        //表示当前的cache文件没有过期，则直接跳出循环，因为过期队列越是最新的就越靠前存放，最新的缓存存在队列头部
        if (wait > 0) {
            //如果没有超时，则退出循环
            wait = wait > 10 ? 10 : wait;//最少返回值是10，也就是最短超时进行老化操作的时间是10s,即使限制缓存中有节点还有5s就过期了，但是我们还是在10s的时候进行清除
            break;
        }

        ngx_log_debug6(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                       "http file cache expire: #%d %d %02xd%02xd%02xd%02xd",
                       fcn->count, fcn->exists,
                       fcn->key[0], fcn->key[1], fcn->key[2], fcn->key[3]);

        if (fcn->count == 0) { 
            //如果引用计数为0，则删除这个cache节点
            //删除磁盘中缓存的文件
            ngx_http_file_cache_delete(cache, q, name);
            continue;
        }

        if (fcn->deleting) { //配合ngx_http_file_cache_delete阅读
            //如果当前节点正在删除，则退出循环
            wait = 1; //如果最末尾节点正在被删除，则返回1,1s后继续执行该函数
            break;
        }

        
        //将node中字符表示的MD5码，key转换为16进制表示的MD5码并将转换后的16进制表示形式存储在key中，方法执行完后
        //返回转换后的字符串的最后一个字符
        p = ngx_hex_dump(key, (u_char *) &fcn->node.key, 
                         sizeof(ngx_rbtree_key_t)); //ngx_http_file_cache_expire和ngx_http_file_cache_add对应
        len = NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t);
        
        //将fcn->key转换成16进制表示的形式fcn->key中存储的是url的MD5码的后12个字符
        (void) ngx_hex_dump(p, fcn->key, len);
        
        //通过上面的两部转换，就将URL的MD5吗转换成了16进制表示的形式并且存储在了key中
        
        /*
         * abnormally exited workers may leave locked cache entries,
         * and although it may be safe to remove them completely,
         * we prefer to just move them to the top of the inactive queue
         */
        //将当前节点放入队列最前端,如果超时时间到，但是当前还有其他客户端在使用该缓存，则在把缓存时间延迟inactive， 
        ngx_queue_remove(q);
        fcn->expire = ngx_time() + cache->inactive;
        ngx_queue_insert_head(&cache->sh->queue, &fcn->queue);

        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0,
                      "ignore long locked inactive cache entry %*s, count:%d",
                      2 * NGX_HTTP_CACHE_KEY_LEN, key, fcn->count);
    }

    ngx_shmtx_unlock(&cache->shpool->mutex);

    ngx_free(name);

    return wait;
}

/*
缓存文件清理过程均调用了ngx_http_file_cache_delete 函数，并且调用它的前提条 
件是当前函数已经获得了cache->shpool->mutex 锁，同时，当前缓存节点的引用计数为0。
*/
//它主要有2个功能，一个是删除cache文件，一个是删除cache管理节点。
static void
ngx_http_file_cache_delete(ngx_http_file_cache_t *cache, ngx_queue_t *q,
    u_char *name)
{
    u_char                      *p;
    size_t                       len;
    ngx_path_t                  *path;
    ngx_http_file_cache_node_t  *fcn;

    fcn = ngx_queue_data(q, ngx_http_file_cache_node_t, queue);

    if (fcn->exists) {
        cache->sh->size -= fcn->fs_size; //这块共享内存释放了，总共占用的共享内存也就少了这么多

        path = cache->path;
        p = name + path->name.len + 1 + path->len;
        p = ngx_hex_dump(p, (u_char *) &fcn->node.key,
                         sizeof(ngx_rbtree_key_t));
        len = NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t);
        p = ngx_hex_dump(p, fcn->key, len);
        *p = '\0';

        fcn->count++; //count 加 1 以避免其它进程再次尝试清理此节点 (当前代码中还不会有这种情况发生)。 
        
        fcn->deleting = 1; //deleting 标识此缓存节点正在被删除，其它函数或进程因视其为无效节点。 
        

        /*
          由于文件删除操作 ( ngx_delete_file ) 可能发生阻塞，所以进行这个操作期间，函数将缓存锁先释放掉，以免其它进程因为等待这个锁而阻塞。 
          */
        ngx_shmtx_unlock(&cache->shpool->mutex);

        len = path->name.len + 1 + path->len + 2 * NGX_HTTP_CACHE_KEY_LEN;
        ngx_create_hashed_filename(path, name, len);

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                       "http file cache expire: \"%s\"", name);

        if (ngx_delete_file(name) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_CRIT, ngx_cycle->log, ngx_errno,
                          ngx_delete_file_n " \"%s\" failed", name);
        }

        ngx_shmtx_lock(&cache->shpool->mutex);
        fcn->count--;
        fcn->deleting = 0;
    }

    if (fcn->count == 0) {
        ngx_queue_remove(q);
        ngx_rbtree_delete(&cache->sh->rbtree, &fcn->node);
        ngx_slab_free_locked(cache->shpool, fcn);
    }
}

/*
在Nginx中，如果启用了proxy(fastcgi) cache功能，master process会在启动的时候启动管理缓存的两个子进程(区别于处理请求的子进程)来管理内
存和磁盘的缓存个体。第一个进程的功能是定期检查缓存，并将过期的缓存删除；第二个进程的作用是在启动的时候将磁盘中已经缓存的个
体映射到内存中(目前Nginx设定为启动以后60秒)，然后退出。

具体的，在这两个进程的ngx_process_events_and_timers()函数中，会调用ngx_event_expire_timers()。Nginx的ngx_event_timer_rbtree(红黑树)里
面按照执行的时间的先后存放着一系列的事件。每次取执行时间最早的事件，如果当前时间已经到了应该执行该事件，就会调用事件的handler。两个
进程的handler分别是ngx_cache_manager_process_handler和ngx_cache_loader_process_handler
*/

//ngx_cache_manager_process_handler中执行
static time_t //定时执行ngx_cache_manager_process_handler->ngx_http_file_cache_manager从而进行超时(通过定时器实现)清理操作
ngx_http_file_cache_manager(void *data) //每次nginx退出的时候，例如kill nginx都会坚持缓存文件，如果过期，则会删除
{
    ngx_http_file_cache_t  *cache = data;

    off_t   size;
    time_t  next, wait;

    next = ngx_http_file_cache_expire(cache); //先删过期的缓存  

    cache->last = ngx_current_msec; //最后访问时间
    cache->files = 0;

    for ( ;; ) {
        ngx_shmtx_lock(&cache->shpool->mutex);
        
        //获取更新的缓存空间的大小  
        size = cache->sh->size; //获取删除过期缓存后的缓存队列的大小

        ngx_shmtx_unlock(&cache->shpool->mutex);

        //killall -9 nginx的时候打印是:http file cache size: 16, max_size:0 所以后面会把所有缓存清楚
        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                       "http file cache size: %O, max_size:%O", size, cache->max_size);

        //检查缓存磁盘目录是否超过设定大小限制  超过了proxy_cache_path xxx_cache_path  path max_size=size
        if (size < cache->max_size) { //如果空间在指定范围内，不用再删了。return  
            return next;
        }

       /*
        如果超限，调用函数ngx_http_file_cache_forced_expire 从 inactive queue 队尾开始扫描，直到找到 
        可以被清理的当前未使用节点 ( fcn->count == 0 且不论它是否过期) 或者查找了20 个节点后仍未找到符合条件的节点。 
        */
        
        //如果size超过磁盘的使用空间，即size >= cache->max_size 强制把部分缓存删除，以保证缓存使用的空间在指定范围内   
        wait = ngx_http_file_cache_forced_expire(cache);

        if (wait > 0) { //休息一下以后继续删
            return wait;
        }

        if (ngx_quit || ngx_terminate) {
            return next;
        }
    }
}


/*
在nginx启动1分钟之后，会启动一个名为cache loader process的进程，该进程运行了一段时间之后，该进程就会结束消失。

在该进程运行期间主要做了以下事情：遍历配置文件中proxy_cache_path命令指定的路径中的所有的缓存文件，并且针对遍历到的各个缓存文件
的MD5编码先遍历红黑树和相应的ngx_http_file_cache_node_t节点，如果不存在就创建新的ngx_http_file_cache_node_t，并将该对象中的rbnode
和queue分别插入到红黑树和过期队列；如果存在，则更新相应的属性。
*/
//ngx_cache_loader_process_handler->ngx_http_file_cache_loader
static void
ngx_http_file_cache_loader(void *data)
{
    ngx_http_file_cache_t  *cache = data;

    ngx_tree_ctx_t  tree;

    if (!cache->sh->cold || cache->sh->loading) {//表示已经被加载完毕
        return;
    }

    if (!ngx_atomic_cmp_set(&cache->sh->loading, 0, ngx_pid)) {
        return;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "http file cache loader");

    tree.init_handler = NULL;
    tree.file_handler = ngx_http_file_cache_manage_file;
    tree.pre_tree_handler = ngx_http_file_cache_manage_directory;
    tree.post_tree_handler = ngx_http_file_cache_noop;
    tree.spec_handler = ngx_http_file_cache_delete_file;
    //上述的注册方法都会在ngx_walk_tree方法中进行调用
    
    tree.data = cache; //回调数据就是cache
    tree.alloc = 0;
    tree.log = ngx_cycle->log;

    cache->last = ngx_current_msec; //last为最后load时间
    cache->files = 0;

    if (ngx_walk_tree(&tree, &cache->path->name) == NGX_ABORT) { //开始遍历
        cache->sh->loading = 0;
        return;
    }

    cache->sh->cold = 0;
    cache->sh->loading = 0;

    ngx_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0,
                  "http file cache: %V %.3fM, bsize: %uz",
                  &cache->path->name,
                  ((double) cache->sh->size * cache->bsize) / (1024 * 1024),
                  cache->bsize);
}


static ngx_int_t
ngx_http_file_cache_noop(ngx_tree_ctx_t *ctx, ngx_str_t *path)
{
    return NGX_OK;
}

//ngx_http_file_cache_manage_file 将缓存文件信息存入缓存中。 
static ngx_int_t
ngx_http_file_cache_manage_file(ngx_tree_ctx_t *ctx, ngx_str_t *path)
{
    ngx_msec_t              elapsed;
    ngx_http_file_cache_t  *cache;

    cache = ctx->data;

    if (ngx_http_file_cache_add_file(ctx, path) != NGX_OK) {
        //将文件添加进cache
        (void) ngx_http_file_cache_delete_file(ctx, path);
    }

   /* 
        根据配置控制缓存的读取速度 ( loader_files 和 loader_threshold )，以便在缓存文件很多的情况下降低初次启动时对系统资源的消耗。 
    */
    if (++cache->files >= cache->loader_files) {
        //如果文件个数太大，则休眠并清理files计数
        ngx_http_file_cache_loader_sleep(cache);

    } else {
        ngx_time_update();
        //否则看loader时间是不是过长，如果过长则又进入休眠
        elapsed = ngx_abs((ngx_msec_int_t) (ngx_current_msec - cache->last));

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                       "http file cache loader time elapsed: %M", elapsed);

      /* 
        根据配置控制缓存的读取速度 ( loader_files 和 loader_threshold )，以便在缓存文件很多的情况下降低初次启动时对系统资源的消耗。 
        */
        if (elapsed >= cache->loader_threshold) {
            ngx_http_file_cache_loader_sleep(cache);
        }
    }

    return (ngx_quit || ngx_terminate) ? NGX_ABORT : NGX_OK;
}


static ngx_int_t
ngx_http_file_cache_manage_directory(ngx_tree_ctx_t *ctx, ngx_str_t *path)
{
    if (path->len >= 5
        && ngx_strncmp(path->data + path->len - 5, "/temp", 5) == 0)
    {
        return NGX_DECLINED;
    }

    return NGX_OK;
}


static void
ngx_http_file_cache_loader_sleep(ngx_http_file_cache_t *cache)
{
    ngx_msleep(cache->loader_sleep);

    ngx_time_update();

    cache->last = ngx_current_msec;
    cache->files = 0;
}

/*
ngx_http_file_cache_add_file，它主要是通过文件名计算hash，然后调用ngx_http_file_cache_add将这个文件加入到cache管理中(也就是添加红黑树以及队列),
*/
static ngx_int_t
ngx_http_file_cache_add_file(ngx_tree_ctx_t *ctx, ngx_str_t *name)
{
    u_char                 *p;
    ngx_int_t               n;
    ngx_uint_t              i;
    ngx_http_cache_t        c;
    ngx_http_file_cache_t  *cache;

    if (name->len < 2 * NGX_HTTP_CACHE_KEY_LEN) {
        return NGX_ERROR;
    }

    if (ctx->size < (off_t) sizeof(ngx_http_file_cache_header_t)) {
        ngx_log_error(NGX_LOG_CRIT, ctx->log, 0,
                      "cache file \"%s\" is too small", name->data);
        return NGX_ERROR;
    }

    ngx_memzero(&c, sizeof(ngx_http_cache_t));
    cache = ctx->data;

    c.length = ctx->size;
    c.fs_size = (ctx->fs_size + cache->bsize - 1) / cache->bsize;

    p = &name->data[name->len - 2 * NGX_HTTP_CACHE_KEY_LEN];

    for (i = 0; i < NGX_HTTP_CACHE_KEY_LEN; i++) {
        n = ngx_hextoi(p, 2);

        if (n == NGX_ERROR) {
            return NGX_ERROR;
        }

        p += 2;

        c.key[i] = (u_char) n;
    }

    return ngx_http_file_cache_add(cache, &c);
}

//ngx_http_file_cache_add 函数将此节点加入 ngx_http_file_cache_sh_t 类型的缓存管理机制中。 

//按照c->key在红黑树中查找，没有就创建node节点，然后把节点添加到红黑树cache->sh->rbtree和cache->sh->queue队列头
static ngx_int_t //ngx_http_file_cache_expire和ngx_http_file_cache_add对应
ngx_http_file_cache_add(ngx_http_file_cache_t *cache, ngx_http_cache_t *c)
{
    ngx_http_file_cache_node_t  *fcn;

    ngx_shmtx_lock(&cache->shpool->mutex);

    fcn = ngx_http_file_cache_lookup(cache, c->key);//首先查找

    if (fcn == NULL) {
        //如果不存在，则新建结构
        fcn = ngx_slab_calloc_locked(cache->shpool,
                                     sizeof(ngx_http_file_cache_node_t));
        if (fcn == NULL) {
            ngx_shmtx_unlock(&cache->shpool->mutex);
            return NGX_ERROR;
        }

        ngx_memcpy((u_char *) &fcn->node.key, c->key, sizeof(ngx_rbtree_key_t));

        ngx_memcpy(fcn->key, &c->key[sizeof(ngx_rbtree_key_t)],
                   NGX_HTTP_CACHE_KEY_LEN - sizeof(ngx_rbtree_key_t));

        ngx_rbtree_insert(&cache->sh->rbtree, &fcn->node); //插入红黑树

        fcn->uses = 1;
        fcn->exists = 1;
        fcn->fs_size = c->fs_size;

        cache->sh->size += c->fs_size;

    } else {
        //否则删除queue，后续会重新插入
        ngx_queue_remove(&fcn->queue);
    }

    fcn->expire = ngx_time() + cache->inactive;

    ngx_queue_insert_head(&cache->sh->queue, &fcn->queue); //重新插入

    ngx_shmtx_unlock(&cache->shpool->mutex);

    return NGX_OK;
}


static ngx_int_t
ngx_http_file_cache_delete_file(ngx_tree_ctx_t *ctx, ngx_str_t *path)
{
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ctx->log, 0,
                   "http file cache delete: \"%s\"", path->data);

    if (ngx_delete_file(path->data) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_CRIT, ctx->log, ngx_errno,
                      ngx_delete_file_n " \"%s\" failed", path->data);
    }

    return NGX_OK;
}

//获取//proxy_cache_valid xxx 4m;中的4m，根据status查找对应的时间
time_t
ngx_http_file_cache_valid(ngx_array_t *cache_valid, ngx_uint_t status)
{
    ngx_uint_t               i;
    ngx_http_cache_valid_t  *valid;

    if (cache_valid == NULL) {
        return 0;
    }

    valid = cache_valid->elts;
    for (i = 0; i < cache_valid->nelts; i++) {

        if (valid[i].status == 0) {
            return valid[i].valid;
        }

        if (valid[i].status == status) {
            return valid[i].valid;
        }
    }

    return 0;
}

/*
Proxy_cache_path：缓存的存储路径和索引信息；
  path 缓存文件的根目录；
  level=N:N在目录的第几级hash目录缓存数据；
  keys_zone=name:size 缓存索引重建进程建立索引时用于存放索引的内存区域名和大小；
  interval=time强制更新缓存时间，规定时间内没有访问则从内存中删除，默认10s；
  max_size=size硬盘中缓存数据的上限，由cache manager管理，超出则根据LRU策略删除；
  loader_sleep=time索引重建进程在两次遍历间的暂停时长，默认50ms；
  loader_files=number重建索引时每次加载数据元素的上限，进程递归遍历读取硬盘上的缓存目录和文件，对每个文件在内存中建立索引，每
  建立一个索引称为加载一个数据元素，每次遍历时可同时加载多个数据元素，默认100；

   //loader_files这个值也就是一个阈值，当load的文件个数大于这个值之后，load进程会短暂的休眠(时间位loader_sleep)
    //loader_sleep和上面的loader_files配合使用，当文件个数大于loader_files，就会休眠
    //loader_threshold配合上面的last，也就是loader遍历的休眠间隔。
*/
//XXX_cache_path(proxy_cache_path fastcgi_cache_path)等配置走到这里
//XXX_cache缓存是先写在xxx_temp_path再移到xxx_cache_path，所以这两个目录最好在同一个分区
char * //后端应答数据在ngx_http_upstream_process_request->ngx_http_file_cache_update中进行缓存
ngx_http_file_cache_set_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{ //目录不存在会自动创建
    char  *confp = conf;

    off_t                   max_size;
    u_char                 *last, *p;
    time_t                  inactive;
    size_t                  len;
    ssize_t                 size;
    ngx_str_t               s, name, *value;
    ngx_int_t               loader_files;
    ngx_msec_t              loader_sleep, loader_threshold;
    ngx_uint_t              i, n, 
                            use_temp_path; //"use_temp_path= on|off"
    ngx_array_t            *caches;
    ngx_http_file_cache_t  *cache, **ce;

    cache = ngx_pcalloc(cf->pool, sizeof(ngx_http_file_cache_t));
    if (cache == NULL) {
        return NGX_CONF_ERROR;
    }

    cache->path = ngx_pcalloc(cf->pool, sizeof(ngx_path_t));
    if (cache->path == NULL) {
        return NGX_CONF_ERROR;
    }

    use_temp_path = 1;

    inactive = 600;
    loader_files = 100;
    loader_sleep = 50;
    loader_threshold = 200;

    name.len = 0;
    size = 0;
    max_size = NGX_MAX_OFF_T_VALUE;

    value = cf->args->elts;

    cache->path->name = value[1]; //获取path保存到path->name

    if (cache->path->name.data[cache->path->name.len - 1] == '/') {
        cache->path->name.len--; //去掉path后面的/字符
    }

    
    if (ngx_conf_full_name(cf->cycle, &cache->path->name, 0) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

     //loader_files这个值也就是一个阈值，当load的文件个数大于这个值之后，load进程会短暂的休眠(时间位loader_sleep)
    //loader_sleep和上面的loader_files配合使用，当文件个数大于loader_files，就会休眠
    //loader_threshold配合上面的last，也就是loader遍历的休眠间隔。
    for (i = 2; i < cf->args->nelts; i++) {
    /*
 levels=1:2，意思是说使用两级目录，第一级目录名是一个字符，第二级用两个字符。但是nginx最大支持3级目录，即levels=xxx:xxx:xxx。
 那么构成目录名字的字符哪来的呢？假设我们的存储目录为/cache，levels=1:2，那么对于上面的文件 就是这样存储的：
 /cache/0/8d/8ef9229f02c5672c747dc7a324d658d0  注意后面的8d0和cache后面的/0/8d一致
     */
        if (ngx_strncmp(value[i].data, "levels=", 7) == 0) { //level=N:N在目录的第几级hash目录缓存数据；

            p = value[i].data + 7;
            last = value[i].data + value[i].len;

            for (n = 0; n < 3 && p < last; n++) { //levels=x:y;后面的x和y的取值范围是1-2
                //把levels=x:y;中的x和y分别存储在level[0]和level[1]
                if (*p > '0' && *p < '3') { //level[]只能为1和2

                    cache->path->level[n] = *p++ - '0';
                    cache->path->len += cache->path->level[n] + 1;   //levels=x:y最终的结果是path->len = (x+1) + (y+1)
            
                    if (p == last) {
                        break;
                    }

                    if (*p++ == ':' && n < 2 && p != last) {
                        continue;
                    }

                    goto invalid_levels;
                }

                goto invalid_levels;
            }
            
            if (cache->path->len < 10 + 3) { // ??????为什么这里要小于10 + 3  最大不应该是2+1 + 2+1吗
                continue;
            }

        invalid_levels:

            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid \"levels\" \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }

/*
非缓存方式(p->cacheable=0)p->temp_file->path = u->conf->temp_path; 由ngx_http_fastcgi_temp_path指定路径
缓存方式(p->cacheable=1) p->temp_file->path = r->cache->file_cache->temp_path;见proxy_cache_path或者fastcgi_cache_path use_temp_path=指定路径  
见ngx_http_upstream_send_response 

当前fastcgi_buffers 和fastcgi_buffer_size配置的空间都已经用完了，则需要把数据写道临时文件中去，参考ngx_event_pipe_read_upstream
*/   //use_temp_path= on则,则不会用
        if (ngx_strncmp(value[i].data, "use_temp_path=", 14) == 0) {

            if (ngx_strcmp(&value[i].data[14], "on") == 0) {
                use_temp_path = 1;

            } else if (ngx_strcmp(&value[i].data[14], "off") == 0) {
                use_temp_path = 0;

            } else {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid use_temp_path value \"%V\", "
                                   "it must be \"on\" or \"off\"",
                                   &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "keys_zone=", 10) == 0) { //keys_zone=fcgi:10m   

            name.data = value[i].data + 10;

            p = (u_char *) ngx_strchr(name.data, ':');

            if (p) {
                name.len = p - name.data;//keys_zone=fcgi:10m中的fcgi

                p++;//跳过':'指向fcgi

                //fcgi:10m中的10m字符串保存到s中
                s.len = value[i].data + value[i].len - p;
                s.data = p;

                size = ngx_parse_size(&s); //keys_zone=fcgi:xx  xx最小要4K
                if (size > 8191) {
                    continue;
                }
            }

            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid keys zone size \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }

        if (ngx_strncmp(value[i].data, "inactive=", 9) == 0) {

            s.len = value[i].len - 9;
            s.data = value[i].data + 9;

            inactive = ngx_parse_time(&s, 1);
            if (inactive == (time_t) NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid inactive value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "max_size=", 9) == 0) {

            s.len = value[i].len - 9;
            s.data = value[i].data + 9;

            max_size = ngx_parse_offset(&s);
            if (max_size < 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid max_size value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "loader_files=", 13) == 0) {

            loader_files = ngx_atoi(value[i].data + 13, value[i].len - 13);
            if (loader_files == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid loader_files value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "loader_sleep=", 13) == 0) {

            s.len = value[i].len - 13;
            s.data = value[i].data + 13;

            loader_sleep = ngx_parse_time(&s, 0);
            if (loader_sleep == (ngx_msec_t) NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid loader_sleep value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "loader_threshold=", 17) == 0) {

            s.len = value[i].len - 17;
            s.data = value[i].data + 17;

            loader_threshold = ngx_parse_time(&s, 0);
            if (loader_threshold == (ngx_msec_t) NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid loader_threshold value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    if (name.len == 0 || size == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"keys_zone\" parameter",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    cache->path->manager = ngx_http_file_cache_manager;
    cache->path->loader = ngx_http_file_cache_loader;
    cache->path->data = cache;
    cache->path->conf_file = cf->conf_file->file.name.data;
    cache->path->line = cf->conf_file->line;
    cache->loader_files = loader_files;
    cache->loader_sleep = loader_sleep;
    cache->loader_threshold = loader_threshold;

    if (ngx_add_path(cf, &cache->path) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    if (!use_temp_path) {//参数中带有use_temp_path=off则会在配置的path后面创建一层/temp目录  在前面默认use_temp_path = 1;
        cache->temp_path = ngx_pcalloc(cf->pool, sizeof(ngx_path_t));
        if (cache->temp_path == NULL) {
            return NGX_CONF_ERROR;
        }

        len = cache->path->name.len + sizeof("/temp") - 1;//在proxy_cache_path /xxx指定的/xxx后面添加/temp，即/xxx/temp

        p = ngx_pnalloc(cf->pool, len + 1);
        if (p == NULL) {
            return NGX_CONF_ERROR;
        }

        cache->temp_path->name.len = len;
        cache->temp_path->name.data = p;

        p = ngx_cpymem(p, cache->path->name.data, cache->path->name.len);
        ngx_memcpy(p, "/temp", sizeof("/temp"));

        ngx_memcpy(&cache->temp_path->level, &cache->path->level,
                   3 * sizeof(size_t)); //temp的level继承了其父目录的level

        cache->temp_path->len = cache->path->len;
        cache->temp_path->conf_file = cf->conf_file->file.name.data;
        cache->temp_path->line = cf->conf_file->line;

        if (ngx_add_path(cf, &cache->temp_path) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    cache->shm_zone = ngx_shared_memory_add(cf, &name, size, cmd->post);
    if (cache->shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    if (cache->shm_zone->data) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "duplicate zone \"%V\"", &name);
        return NGX_CONF_ERROR;
    }


    cache->shm_zone->init = ngx_http_file_cache_init;
    cache->shm_zone->data = cache;

    cache->inactive = inactive;
    cache->max_size = max_size;

    caches = (ngx_array_t *) (confp + cmd->offset);

    ce = ngx_array_push(caches);
    if (ce == NULL) {
        return NGX_CONF_ERROR;
    }

    *ce = cache;

    return NGX_CONF_OK;
}

/*
Syntax:  proxy_cache_valid [code ...] time;
 
Default:  —  
Context:  http, server, location
 

Sets caching time for different response codes. For example, the following directives 

proxy_cache_valid 200 302 10m;
proxy_cache_valid 404      1m;
set 10 minutes of caching for responses with codes 200 and 302 and 1 minute for responses with code 404. 

If only caching time is specified 

proxy_cache_valid 5m;
then only 200, 301, and 302 responses are cached. 

In addition, the any parameter can be specified to cache any responses: 

proxy_cache_valid 200 302 10m;
proxy_cache_valid 301      1h;
proxy_cache_valid any      1m;

Parameters of caching can also be set directly in the response header. This has higher priority than setting of caching time using the directive. 

?The “X-Accel-Expires” header field sets caching time of a response in seconds. The zero value disables caching for a response. 
If the value starts with the @ prefix, it sets an absolute time in seconds since Epoch, up to which the response may be cached. 
?If the header does not include the “X-Accel-Expires” field, parameters of caching may be set in the header fields “Expires” 
or “Cache-Control”. 
?If the header includes the “Set-Cookie” field, such a response will not be cached. 
?If the header includes the “Vary” field with the special value “*”, such a response will not be cached (1.7.7). If the header 
includes the “Vary” field with another value, such a response will be cached taking into account the corresponding request header fields (1.7.7). 
Processing of one or more of these response header fields can be disabled using the proxy_ignore_headers directive. 
*/
//proxy_cache_valid  fastcgo_cache_valid
char *
ngx_http_file_cache_valid_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    char  *p = conf;

    time_t                    valid;
    ngx_str_t                *value;
    ngx_uint_t                i, n, status;
    ngx_array_t             **a;
    ngx_http_cache_valid_t   *v;
    //如果不带2XX 3XX 4XX 5XX等，直接是proxy_cache_valid 5m，则默认开启200 301 302
    static ngx_uint_t         statuses[] = { 200, 301, 302 }; 

    a = (ngx_array_t **) (p + cmd->offset);

    if (*a == NGX_CONF_UNSET_PTR) {
        *a = ngx_array_create(cf->pool, 1, sizeof(ngx_http_cache_valid_t));
        if (*a == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;
    n = cf->args->nelts - 1;

    valid = ngx_parse_time(&value[n], 1);
    if (valid == (time_t) NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid time value \"%V\"", &value[n]);
        return NGX_CONF_ERROR;
    }

    if (n == 1) {//如果不带2XX 3XX 4XX 5XX等，直接是proxy_cache_valid 5m，则默认开启200 301 302

        for (i = 0; i < 3; i++) {
            v = ngx_array_push(*a);
            if (v == NULL) {
                return NGX_CONF_ERROR;
            }

            v->status = statuses[i];
            v->valid = valid;
        }

        return NGX_CONF_OK;
    }

    for (i = 1; i < n; i++) {

        if (ngx_strcmp(value[i].data, "any") == 0) {

            status = 0;

        } else {

            status = ngx_atoi(value[i].data, value[i].len);
            if (status < 100) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid status \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }
        }

        v = ngx_array_push(*a);
        if (v == NULL) {
            return NGX_CONF_ERROR;
        }

        v->status = status;
        v->valid = valid;
    }

    return NGX_CONF_OK;
}
