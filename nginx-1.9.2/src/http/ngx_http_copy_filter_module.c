
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {//真正判断生效在ngx_output_chain
    ngx_bufs_t  bufs;  //output_buffers  5   3K  默认值output_buffers 1 32768
} ngx_http_copy_filter_conf_t;


#if (NGX_HAVE_FILE_AIO)
static void ngx_http_copy_aio_handler(ngx_output_chain_ctx_t *ctx,
    ngx_file_t *file);
static void ngx_http_copy_aio_event_handler(ngx_event_t *ev);
#if (NGX_HAVE_AIO_SENDFILE)
static ssize_t ngx_http_copy_aio_sendfile_preload(ngx_buf_t *file);
static void ngx_http_copy_aio_sendfile_event_handler(ngx_event_t *ev);
#endif
#endif
#if (NGX_THREADS)
static ngx_int_t ngx_http_copy_thread_handler(ngx_thread_task_t *task,
    ngx_file_t *file);
static void ngx_http_copy_thread_event_handler(ngx_event_t *ev);
#endif

static void *ngx_http_copy_filter_create_conf(ngx_conf_t *cf);
static char *ngx_http_copy_filter_merge_conf(ngx_conf_t *cf,
    void *parent, void *child);
static ngx_int_t ngx_http_copy_filter_init(ngx_conf_t *cf);


static ngx_command_t  ngx_http_copy_filter_commands[] = {

    { ngx_string("output_buffers"), //真正判断生效在ngx_output_chain
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_bufs_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_copy_filter_conf_t, bufs),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_copy_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_copy_filter_init,             /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_copy_filter_create_conf,      /* create location configuration */
    ngx_http_copy_filter_merge_conf        /* merge location configuration */
};

/*
表6-1  默认即编译进Nginx的HTTP过滤模块
┏━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃默认即编译进Nginx的HTTP过滤模块     ┃    功能                                                          ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP头部做处理。在返回200成功时，根据请求中If-              ┃
┃                                    ┃Modified-Since或者If-Unmodified-Since头部取得浏览器缓存文件的时   ┃
┃ngx_http_not_modified_filter_module ┃                                                                  ┃
┃                                    ┃间，再分析返回用户文件的最后修改时间，以此决定是否直接发送304     ┃
┃                                    ┃ Not Modified响应给用户                                           ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  处理请求中的Range信息，根据Range中的要求返回文件的一部分给      ┃
┃ngx_http_range_body_filter_module   ┃                                                                  ┃
┃                                    ┃用户                                                              ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP包体做处理。将用户发送的ngx_chain_t结构的HTTP包         ┃
┃                                    ┃体复制到新的ngx_chain_t结构中（都是各种指针的复制，不包括实际     ┃
┃ngx_http_copy_filter_module         ┃                                                                  ┃
┃                                    ┃HTTP响应内容），后续的HTTP过滤模块处埋的ngx_chain_t类型的成       ┃
┃                                    ┃员都是ngx_http_copy_filter_module模块处理后的变量                 ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP头部做处理。允许通过修改nginx.conf配置文件，在返回      ┃
┃ngx_http_headers_filter_module      ┃                                                                  ┃
┃                                    ┃给用户的响应中添加任意的HTTP头部                                  ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP头部做处理。这就是执行configure命令时提到的http_        ┃
┃ngx_http_userid_filter_module       ┃                                                                  ┃
┃                                    ┃userid module模块，它基于cookie提供了简单的认证管理功能           ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  可以将文本类型返回给用户的响应包，按照nginx．conf中的配置重新   ┃
┃ngx_http_charset_filter_module      ┃                                                                  ┃
┃                                    ┃进行编码，再返回给用户                                            ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  支持SSI（Server Side Include，服务器端嵌入）功能，将文件内容包  ┃
┃ngx_http_ssi_filter_module          ┃                                                                  ┃
┃                                    ┃含到网页中并返回给用户                                            ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP包体做处理。5.5.2节详细介绍过该过滤模块。它仅应用于     ┃
┃ngx_http_postpone_filter_module     ┃subrequest产生的子请求。它使得多个子请求同时向客户端发送响应时    ┃
┃                                    ┃能够有序，所谓的“有序”是揩按照构造子请求的顺序发送响应            ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  对特定的HTTP响应包体（如网页或者文本文件）进行gzip压缩，再      ┃
┃ngx_http_gzip_filter_module         ┃                                                                  ┃
┃                                    ┃把压缩后的内容返回给用户                                          ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_http_range_header_filter_module ┃  支持range协议                                                   ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_http_chunked_filter_module      ┃  支持chunk编码                                                   ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP头部做处理。该过滤模块将会把r->headers out结构体        ┃
┃                                    ┃中的成员序列化为返回给用户的HTTP响应字符流，包括响应行(如         ┃
┃ngx_http_header_filter_module       ┃                                                                  ┃
┃                                    ┃HTTP/I.1 200 0K)和响应头部，并通过调用ngx_http_write filter       ┃
┃                                    ┃ module过滤模块中的过滤方法直接将HTTP包头发送到客户端             ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_http_write_filter_module        ┃  仅对HTTP包体做处理。该模块负责向客户端发送HTTP响应              ┃
┗━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

*/

ngx_module_t  ngx_http_copy_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_copy_filter_module_ctx,      /* module context */
    ngx_http_copy_filter_commands,         /* module directives */
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


static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


/*
2015/12/31 06:36:20[ ngx_event_pipe_write_to_downstream,   600]  [debug] 21359#21359: *1 pipe write downstream, write ready: 1
2015/12/31 06:36:20[ ngx_event_pipe_write_to_downstream,   620]  [debug] 21359#21359: *1 pipe write downstream flush out
2015/12/31 06:36:20[             ngx_http_output_filter,  3338]  [debug] 21359#21359: *1 http output filter "/test2.php?"
2015/12/31 06:36:20[               ngx_http_copy_filter,   160]  [debug] 21359#21359: *1 http copy filter: "/test2.php?", r->aio:0
2015/12/31 06:36:20[                   ngx_output_chain,    67][yangya  [debug] 21359#21359: *1 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2015/12/31 06:36:20[                  ngx_file_aio_read,   251]  [debug] 21359#21359: *1 aio complete:0 @206:215 /var/yyz/cache_xxx/temp/1/00/0000000001
2015/12/31 06:36:20[               ngx_http_copy_filter,   231]  [debug] 21359#21359: *1 http copy filter: -2 "/test2.php?"
2015/12/31 06:36:20[ ngx_event_pipe_write_to_downstream,   658]  [debug] 21359#21359: *1 pipe write downstream done
2015/12/31 06:36:20[                ngx_event_del_timer,    39]  [debug] 21359#21359: *1 <           ngx_event_pipe,    87>  event timer del: 14: 4111061275
2015/12/31 06:36:20[         ngx_http_file_cache_update,  1508]  [debug] 21359#21359: *1 http file cache update, c->body_start:206
2015/12/31 06:36:20[         ngx_http_file_cache_update,  1521]  [debug] 21359#21359: *1 http file cache rename: "/var/yyz/cache_xxx/temp/1/00/0000000001" to "/var/yyz/cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f", expire time:1800
2015/12/31 06:36:20[                     ngx_shmtx_lock,   168]  [debug] 21359#21359: shmtx lock
2015/12/31 06:36:20[                   ngx_shmtx_unlock,   249]  [debug] 21359#21359: shmtx unlock
2015/12/31 06:36:20[  ngx_http_upstream_process_request,  4250]  [debug] 21359#21359: *1 http upstream exit: 00000000
2015/12/31 06:36:20[ ngx_http_upstream_finalize_request,  4521]  [debug] 21359#21359: *1 finalize http upstream request rc: 0
2015/12/31 06:36:20[  ngx_http_fastcgi_finalize_request,  3215]  [debug] 21359#21359: *1 finalize http fastcgi request
2015/12/31 06:36:20[ngx_http_upstream_free_round_robin_peer,   887]  [debug] 21359#21359: *1 free rr peer 1 0
2015/12/31 06:36:20[ ngx_http_upstream_finalize_request,  4574]  [debug] 21359#21359: *1 close http upstream connection: 14
2015/12/31 06:36:20[               ngx_close_connection,  1120]  [debug] 21359#21359: *1 delete posted event AE8FD098
2015/12/31 06:36:20[            ngx_reusable_connection,  1177]  [debug] 21359#21359: *1 reusable connection: 0
2015/12/31 06:36:20[               ngx_close_connection,  1139][yangya  [debug] 21359#21359: close socket:14
2015/12/31 06:36:20[ ngx_http_upstream_finalize_request,  4588]  [debug] 21359#21359: *1 http upstream temp fd: 15
2015/12/31 06:36:20[              ngx_http_send_special,  3843][yangya  [debug] 21359#21359: *1 ngx http send special, flags:1
2015/12/31 06:36:20[             ngx_http_output_filter,  3338]  [debug] 21359#21359: *1 http output filter "/test2.php?"
2015/12/31 06:36:20[               ngx_http_copy_filter,   160]  [debug] 21359#21359: *1 http copy filter: "/test2.php?", r->aio:1
2015/12/31 06:36:20[                   ngx_output_chain,    67][yangya  [debug] 21359#21359: *1 ctx->sendfile:0, ctx->aio:1, ctx->directio:0
2015/12/31 06:36:20[               ngx_http_copy_filter,   231]  [debug] 21359#21359: *1 http copy filter: -2 "/test2.php?"
2015/12/31 06:36:20[          ngx_http_finalize_request,  2592]  [debug] 21359#21359: *1 http finalize request rc: -2, "/test2.php?" a:1, c:1
2015/12/31 06:36:20[                ngx_event_add_timer,   100]  [debug] 21359#21359: *1 <ngx_http_set_write_handler,  3013>  event timer add fd:13, expire-time:60 s, timer.key:4111061277
2015/12/31 06:36:20[           ngx_trylock_accept_mutex,   405]  [debug] 21359#21359: accept mutex locked
2015/12/31 06:36:20[           ngx_epoll_process_events,  1713]  [debug] 21359#21359: epoll: fd:10 EPOLLIN  (ev:0001) d:080E2520
2015/12/31 06:36:20[           ngx_epoll_process_events,  1759]  [debug] 21359#21359: post event 080E24E0
2015/12/31 06:36:20[           ngx_event_process_posted,    65]  [debug] 21359#21359: begin to run befor posted event 080E24E0

*/

/*
如果是aio on | thread_pool方式，则会两次执行该函数，并且所有参数几乎一样，只是aio标记取值会变化，日志如下:
2016/01/07 18:47:27[ ngx_event_pipe_write_to_downstream,   604]  [debug] 20923#20923: *1 pipe write downstream, write ready: 1
2016/01/07 18:47:27[ ngx_event_pipe_write_to_downstream,   649]  [debug] 20923#20923: *1 pipe write downstream flush out
2016/01/07 18:47:27[             ngx_http_output_filter,  3377]  [debug] 20923#20923: *1 http output filter "/test2.php?"
2016/01/07 18:47:27[               ngx_http_copy_filter,   206]  [debug] 20923#20923: *1 http copy filter: "/test2.php?", r->aio:0
2016/01/07 18:47:27[                   ngx_output_chain,    67][yangya  [debug] 20923#20923: *1 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2016/01/07 18:47:27[             ngx_output_chain_as_is,   309][yangya  [debug] 20923#20923: ngx_output_chain_as_is--- buf_special:0, in_file:1, directio:0, buf_in_mem:0,need_in_memory:0, need_in_temp:0, memory:0, mmap:0
2016/01/07 18:47:27[             ngx_output_chain_as_is,   309][yangya  [debug] 20923#20923: ngx_output_chain_as_is--- buf_special:0, in_file:1, directio:0, buf_in_mem:0,need_in_memory:0, need_in_temp:0, memory:0, mmap:0

注意第一次走ngx_thread_read，打印信息和第一次完全一样
2016/01/07 18:47:27[                    ngx_thread_read,   147]  [debug] 20923#20923: *1 thread read: fd:14, buf:08115A90, size:1220, offset:206
2016/01/07 18:47:27[              ngx_thread_mutex_lock,   145]  [debug] 20923#20923: pthread_mutex_lock(080F0458) enter
2016/01/07 18:47:27[               ngx_thread_task_post,   280][yangya  [debug] 20923#20923: ngx add task to thread, task id:158
2016/01/07 18:47:27[             ngx_thread_cond_signal,    54]  [debug] 20923#20923: pthread_cond_signal(080F047C)
2016/01/07 18:47:27[               ngx_thread_cond_wait,    96]  [debug] 20923#20928: pthread_cond_wait(080F047C) exit
2016/01/07 18:47:27[            ngx_thread_mutex_unlock,   171]  [debug] 20923#20928: pthread_mutex_unlock(080F0458) exit
2016/01/07 18:47:27[              ngx_thread_pool_cycle,   370]  [debug] 20923#20928: run task #158 in thread pool name:"yang_pool"
2016/01/07 18:47:27[            ngx_thread_read_handler,   201]  [debug] 20923#20928: thread read handler
2016/01/07 18:47:27[            ngx_thread_read_handler,   219]  [debug] 20923#20928: pread: 1220 (err: 0) of 1220 @206
2016/01/07 18:47:27[              ngx_thread_pool_cycle,   376]  [debug] 20923#20928: complete task #158 in thread pool name: "yang_pool"
2016/01/07 18:47:27[              ngx_thread_mutex_lock,   145]  [debug] 20923#20928: pthread_mutex_lock(080F0458) enter
2016/01/07 18:47:27[               ngx_thread_cond_wait,    70]  [debug] 20923#20928: pthread_cond_wait(080F047C) enter
2016/01/07 18:47:27[            ngx_thread_mutex_unlock,   171]  [debug] 20923#20923: pthread_mutex_unlock(080F0458) exit
2016/01/07 18:47:27[               ngx_thread_task_post,   297]  [debug] 20923#20923: task #158 added to thread pool name: "yang_pool" complete
2016/01/07 18:47:27[               ngx_http_copy_filter,   284]  [debug] 20923#20923: *1 http copy filter rc: -2, buffered:4 "/test2.php?"
2016/01/07 18:47:27[ ngx_event_pipe_write_to_downstream,   688]  [debug] 20923#20923: *1 pipe write downstream done
2016/01/07 18:47:27[                ngx_event_del_timer,    39]  [debug] 20923#20923: *1 <           ngx_event_pipe,    91>  event timer del: 12: 464761188
2016/01/07 18:47:27[  ngx_http_upstream_process_request,  4233][yangya  [debug] 20923#20923: *1 ngx http cache, p->length:-1, u->headers_in.content_length_n:-1, tf->offset:1426, r->cache->body_start:206
2016/01/07 18:47:27[         ngx_http_file_cache_update,  1557]  [debug] 20923#20923: *1 http file cache update, c->body_start:206
2016/01/07 18:47:27[         ngx_http_file_cache_update,  1570]  [debug] 20923#20923: *1 http file cache rename: "/var/yyz/cache_xxx/temp/2/00/0000000002" to "/var/yyz/cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f", expire time:1800
2016/01/07 18:47:27[                     ngx_shmtx_lock,   168]  [debug] 20923#20923: shmtx lock
2016/01/07 18:47:27[                   ngx_shmtx_unlock,   249]  [debug] 20923#20923: shmtx unlock
2016/01/07 18:47:27[  ngx_http_upstream_process_request,  4270]  [debug] 20923#20923: *1 http upstream exit: 00000000
2016/01/07 18:47:27[ ngx_http_upstream_finalize_request,  4541]  [debug] 20923#20923: *1 finalize http upstream request rc: 0
2016/01/07 18:47:27[  ngx_http_fastcgi_finalize_request,  3215]  [debug] 20923#20923: *1 finalize http fastcgi request
2016/01/07 18:47:27[ngx_http_upstream_free_round_robin_peer,   887]  [debug] 20923#20923: *1 free rr peer 1 0
2016/01/07 18:47:27[ ngx_http_upstream_finalize_request,  4594]  [debug] 20923#20923: *1 close http upstream connection: 12
2016/01/07 18:47:27[               ngx_close_connection,  1120]  [debug] 20923#20923: *1 delete posted event AEA6B098
2016/01/07 18:47:27[            ngx_reusable_connection,  1177]  [debug] 20923#20923: *1 reusable connection: 0
2016/01/07 18:47:27[               ngx_close_connection,  1139][yangya  [debug] 20923#20923: close socket:12
2016/01/07 18:47:27[ ngx_http_upstream_finalize_request,  4608]  [debug] 20923#20923: *1 http upstream temp fd: 14
2016/01/07 18:47:27[              ngx_http_send_special,  3871][yangya  [debug] 20923#20923: *1 ngx http send special, flags:1, last_buf:1, sync:0, last_in_chain:0, flush:0
2016/01/07 18:47:27[             ngx_http_output_filter,  3377]  [debug] 20923#20923: *1 http output filter "/test2.php?"
2016/01/07 18:47:27[               ngx_http_copy_filter,   206]  [debug] 20923#20923: *1 http copy filter: "/test2.php?", r->aio:1
2016/01/07 18:47:27[                   ngx_output_chain,    67][yangya  [debug] 20923#20923: *1 ctx->sendfile:0, ctx->aio:1, ctx->directio:0
2016/01/07 18:47:27[                   ngx_output_chain,   117][yangya  [debug] 20923#20923: *1 ctx->aio = 1, wait kernel complete read
2016/01/07 18:47:27[               ngx_http_copy_filter,   284]  [debug] 20923#20923: *1 http copy filter rc: -2, buffered:4 "/test2.php?"
2016/01/07 18:47:27[          ngx_http_finalize_request,  2598]  [debug] 20923#20923: *1 http finalize request rc: -2, "/test2.php?" a:1, c:1
2016/01/07 18:47:27[                ngx_event_add_timer,   100]  [debug] 20923#20923: *1 <ngx_http_set_write_handler,  3029>  event timer add fd:13, expire-time:60 s, timer.key:464761210
2016/01/07 18:47:27[           ngx_trylock_accept_mutex,   405]  [debug] 20923#20923: accept mutex locked
2016/01/07 18:47:27[           ngx_epoll_process_events,  1725]  [debug] 20923#20923: epoll: fd:9 EPOLLIN  (ev:0001) d:080E36C0
2016/01/07 18:47:27[           ngx_epoll_process_events,  1771]  [debug] 20923#20923: post event 080E3680
2016/01/07 18:47:27[           ngx_event_process_posted,    65]  [debug] 20923#20923: begin to run befor posted event 080E3680
2016/01/07 18:47:27[           ngx_event_process_posted,    67]  [debug] 20923#20923: delete posted event 080E3680
2016/01/07 18:47:27[            ngx_thread_pool_handler,   401]  [debug] 20923#20923: thread pool handler
2016/01/07 18:47:27[            ngx_thread_pool_handler,   422]  [debug] 20923#20923: run completion handler for task #158
2016/01/07 18:47:27[ ngx_http_copy_thread_event_handler,   429][yangya  [debug] 20923#20923: *1 ngx http aio thread event handler
2016/01/07 18:47:27[           ngx_http_request_handler,  2407]  [debug] 20923#20923: *1 http run request(ev->write:1): "/test2.php?"
2016/01/07 18:47:27[                    ngx_http_writer,  3058]  [debug] 20923#20923: *1 http writer handler: "/test2.php?"
2016/01/07 18:47:27[             ngx_http_output_filter,  3377]  [debug] 20923#20923: *1 http output filter "/test2.php?"
2016/01/07 18:47:27[               ngx_http_copy_filter,   206]  [debug] 20923#20923: *1 http copy filter: "/test2.php?", r->aio:0
2016/01/07 18:47:27[                   ngx_output_chain,    67][yangya  [debug] 20923#20923: *1 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2016/01/07 18:47:27[             ngx_output_chain_as_is,   309][yangya  [debug] 20923#20923: ngx_output_chain_as_is--- buf_special:0, in_file:1, directio:0, buf_in_mem:0,need_in_memory:0, need_in_temp:0, memory:0, mmap:0

注意第二次走ngx_thread_read，打印信息和第一次完全一样
2016/01/07 18:47:27[                    ngx_thread_read,   147]  [debug] 20923#20923: *1 thread read: fd:14, buf:08115A90, size:1220, offset:206

2016/01/07 18:47:27[             ngx_output_chain_as_is,   314][yangya  [debug] 20923#20923: ngx_output_chain_as_is--- buf_special:1, in_file:0, buf_in_mem:0,need_in_memory:0, need_in_temp:0, memory:0, mmap:0
2016/01/07 18:47:27[           ngx_http_postpone_filter,   176]  [debug] 20923#20923: *1 http postpone filter "/test2.php?" 080F3E94
2016/01/07 18:47:27[       ngx_http_chunked_body_filter,   212]  [debug] 20923#20923: *1 http chunk: 1220
2016/01/07 18:47:27[       ngx_http_chunked_body_filter,   212]  [debug] 20923#20923: *1 http chunk: 0
2016/01/07 18:47:27[       ngx_http_chunked_body_filter,   273]  [debug] 20923#20923: *1 yang test ..........xxxxxxxx ################## lstbuf:1
2016/01/07 18:47:27[              ngx_http_write_filter,   151]  [debug] 20923#20923: *1 write old buf t:1 f:0 080F3B60, pos 080F3B60, size: 180 file: 0, size: 0
2016/01/07 18:47:27[              ngx_http_write_filter,   208]  [debug] 20923#20923: *1 write new buf temporary:1 buf-in-file:0, buf->start:080F3EE0, buf->pos:080F3EE0, buf_size: 5 file_pos: 0, in_file_size: 0
2016/01/07 18:47:27[              ngx_http_write_filter,   208]  [debug] 20923#20923: *1 write new buf temporary:1 buf-in-file:0, buf->start:08115A90, buf->pos:08115A90, buf_size: 1220 file_pos: 0, in_file_size: 0
2016/01/07 18:47:27[              ngx_http_write_filter,   208]  [debug] 20923#20923: *1 write new buf temporary:0 buf-in-file:0, buf->start:00000000, buf->pos:080CF058, buf_size: 7 file_pos: 0, in_file_size: 0
2016/01/07 18:47:27[              ngx_http_write_filter,   248]  [debug] 20923#20923: *1 http write filter: last:1 flush:1 size:1412
2016/01/07 18:47:27[              ngx_http_write_filter,   380]  [debug] 20923#20923: *1 http write filter limit 0
2016/01/07 18:47:27[           ngx_linux_sendfile_chain,   201][yangya  [debug] 20923#20923: *1 @@@@@@@@@@@@@@@@@@@@@@@begin ngx_linux_sendfile_chain @@@@@@@@@@@@@@@@@@@
2016/01/07 18:47:27[                         ngx_writev,   238]  [debug] 20923#20923: *1 writev: 1412 of 1412
2016/01/07 18:47:27[              ngx_http_write_filter,   386]  [debug] 20923#20923: *1 http write filter 00000000
2016/01/07 18:47:27[               ngx_http_copy_filter,   284]  [debug] 20923#20923: *1 http copy filter rc: 0, buffered:0 "/test2.php?"
2016/01/07 18:47:27[                    ngx_http_writer,  3124]  [debug] 20923#20923: *1 http writer output filter: 0, "/test2.php?"
2016/01/07 18:47:27[                    ngx_http_writer,  3156]  [debug] 20923#20923: *1 http writer done: "/test2.php?"
2016/01/07 18:47:27[          ngx_http_finalize_request,  2598]  [debug] 20923#20923: *1 http finalize request rc: 0, "/test2.php?" a:1, c:1
*/
//如果是aio on | thread_pool方式，则会两次执行该函数，并且所有参数机会一样，参考上面日志。大文件下载和下文件获取过程机会一样，只是在ngx_http_writer后面有判断
//是否写完成，通过r->buffered是否为0来区分


/*
通过这里发送触发在ngx_http_write_filter->ngx_linux_sendfile_chain(如果文件通过sendfile发送)，
如果是普通写发送，则在ngx_http_write_filter->ngx_writev(一般chain->buf在内存中的情况下用该方式)，
或者ngx_http_copy_filter->ngx_output_chain中的if (ctx->aio) { return NGX_AGAIN;}(如果文件通过aio发送)，然后由aio异步事件epoll触发
读取文件内容超过，然后在继续发送文件
*/

/* 注意:到这里的in实际上是已经指向数据内容部分，或者如果发送的数据需要从文件中读取，in中也会指定文件file_pos和file_last已经文件fd等,
   可以参考ngx_http_cache_send ngx_http_send_header ngx_http_output_filter */
//in为需要发送的chain链，上面存储的是实际要发送的数据
static ngx_int_t
ngx_http_copy_filter(ngx_http_request_t *r, ngx_chain_t *in)
{ //实际上在接受完后端数据后，在想客户端发送包体部分的时候，会两次调用该函数，一次是ngx_event_pipe_write_to_downstream-> p->output_filter(), 
//另一次是ngx_http_upstream_finalize_request->ngx_http_send_special,可以参考上面的日志打印注释
    ngx_int_t                     rc;
    ngx_connection_t             *c;
    ngx_output_chain_ctx_t       *ctx;
    ngx_http_core_loc_conf_t     *clcf;
    ngx_http_copy_filter_conf_t  *conf;
    int aio = r->aio;

    c = r->connection;

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http copy filter: \"%V?%V\", r->aio:%d", &r->uri, &r->args, aio);

    ctx = ngx_http_get_module_ctx(r, ngx_http_copy_filter_module);

    if (ctx == NULL) {
        ctx = ngx_pcalloc(r->pool, sizeof(ngx_output_chain_ctx_t));
        if (ctx == NULL) {
            return NGX_ERROR;
        }

        ngx_http_set_ctx(r, ctx, ngx_http_copy_filter_module);

        conf = ngx_http_get_module_loc_conf(r, ngx_http_copy_filter_module);
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    /*
        和后端的ngx_connection_t在ngx_event_connect_peer这里置为1，但在ngx_http_upstream_connect中c->sendfile &= r->connection->sendfile;，
        和客户端浏览器的ngx_connextion_t的sendfile需要在ngx_http_update_location_config中判断，因此最终是由是否在configure的时候是否有加
        sendfile选项来决定是置1还是置0
     */
        ctx->sendfile = c->sendfile;
        ctx->need_in_memory = r->main_filter_need_in_memory
                              || r->filter_need_in_memory;
        ctx->need_in_temp = r->filter_need_temporary;

        ctx->alignment = clcf->directio_alignment;

        ctx->pool = r->pool;
        ctx->bufs = conf->bufs; // 默认值output_buffers 1 32768
        ctx->tag = (ngx_buf_tag_t) &ngx_http_copy_filter_module;

        ctx->output_filter = (ngx_output_chain_filter_pt)
                                  ngx_http_next_body_filter;
        ctx->filter_ctx = r;

#if (NGX_HAVE_FILE_AIO)
        if (ngx_file_aio && clcf->aio == NGX_HTTP_AIO_ON) { //./configure的时候加上--with-file-aio并且配置文件中aio on的时候才有效
            ctx->aio_handler = ngx_http_copy_aio_handler;
#if (NGX_HAVE_AIO_SENDFILE) //只有freebsd系统才有效 auto/os/freebsd:        have=NGX_HAVE_AIO_SENDFILE . auto/have
            ctx->aio_preload = ngx_http_copy_aio_sendfile_preload;
#endif
        }
#endif

#if (NGX_THREADS) 
        if (clcf->aio == NGX_HTTP_AIO_THREADS) {
            //ngx_output_chain_as_is中赋值给buf->file->thread_handler 
            ctx->thread_handler = ngx_http_copy_thread_handler;
        }
#endif

        //一般在调用filter函数的源头，会在in中指定需要发送的数据长度，可以参考ngx_http_cache_send
        if (in && in->buf && ngx_buf_size(in->buf)) { //判断in链中是否有数据
            r->request_output = 1;
        }
    }

#if (NGX_HAVE_FILE_AIO || NGX_THREADS)
    //实际上在接受完后端数据后，在想客户端发送包体部分的时候，会两次调用该函数，一次是ngx_event_pipe_write_to_downstream-> p->output_filter(),
    //另一次是ngx_http_upstream_finalize_request->ngx_http_send_special,
    //如果是aio方式，则第一次该值为0，但是第二次从ngx_http_send_special走到这里的时候已经在ngx_output_chain->ngx_file_aio_read->ngx_http_copy_aio_handler置1
    //aio方式，当aio读事件完成，会通过ngx_http_copy_aio_event_handler->ngx_http_writer再次走到这里，这时候ngx_http_copy_aio_event_handler已经把r->aio置0
    //可以参考上面的日志备注信息
    ctx->aio = r->aio; 
#endif

    rc = ngx_output_chain(ctx, in);//aio on | thread_pool，这里肯定返回NGX_AGAIN,因为他们是由对应的epoll触发读取数据完毕，然后发送。
    //sendfile或者内存数据这里返回NGX_OK，通过后面的ngx_linux_sendfile_chain->(ngx_linux_sendfile,ngx_writev)把数据发送出去

    if (ctx->in == NULL) { //ctx->in链中的数据，没发送一部分就会从ctx->in链中摘除，见ngx_output_chain，当in链中数据发送完毕，则为NULL
        r->buffered &= ~NGX_HTTP_COPY_BUFFERED;

    } else {//说明还有数据未发送到客户端r
    //ngx_http_finalize_request->ngx_http_set_write_handler->ngx_http_writer通过这种方式把未发送完毕的响应报文发送出去
        r->buffered |= NGX_HTTP_COPY_BUFFERED; //说明ctx->in上还有未发送的数据，函数参数in中指向在ngx_output_chain中已经赋值给了ctx->in
    }

    ngx_int_t buffered = r->buffered;
    ngx_log_debug4(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http copy filter rc: %i, buffered:%i \"%V?%V\"", rc, buffered, &r->uri, &r->args);

    return rc;
}


#if (NGX_HAVE_FILE_AIO)
//执行在ngx_output_chain_copy_buf->ngx_http_copy_aio_handler
static void
ngx_http_copy_aio_handler(ngx_output_chain_ctx_t *ctx, ngx_file_t *file)
{ //注意aio内核读取完毕后，是放在ngx_output_chain_ctx_t->buf中的，见ngx_output_chain_copy_buf->ngx_file_aio_read
    ngx_http_request_t *r;

    r = ctx->filter_ctx;

    file->aio->data = r;
    file->aio->handler = ngx_http_copy_aio_event_handler;

    r->main->blocked++;
    r->aio = 1;
    ctx->aio = 1;
}

//注意aio内核读取完毕后，是放在ngx_output_chain_ctx_t->buf中的，见ngx_output_chain_copy_buf->ngx_file_aio_read

//ngx_file_aio_event_handler中执行
static void
ngx_http_copy_aio_event_handler(ngx_event_t *ev)
{
    ngx_event_aio_t     *aio;
    ngx_http_request_t  *r;

    aio = ev->data;
    r = aio->data;

    r->main->blocked--;  
    r->aio = 0;//aio on方式下，异步读取数据读取完毕，则置0

    ngx_log_debugall(r->connection->log, 0, "ngx http aio on event handler");

    //ngx_http_request_handler -> ngx_http_writer   ngx_http_set_write_handler中设置为ngx_http_writer
    r->connection->write->handler(r->connection->write); //触发一次write->handler，从而可以把从ngx_file_aio_read读到的数据发送出去
}


#if (NGX_HAVE_AIO_SENDFILE)

static ssize_t
ngx_http_copy_aio_sendfile_preload(ngx_buf_t *file)
{
    ssize_t              n;
    static u_char        buf[1];
    ngx_event_aio_t     *aio;
    ngx_http_request_t  *r;

    n = ngx_file_aio_read(file->file, buf, 1, file->file_pos, NULL);

    if (n == NGX_AGAIN) {
        aio = file->file->aio;
        aio->handler = ngx_http_copy_aio_sendfile_event_handler;

        r = aio->data;
        r->main->blocked++;
        r->aio = 1;
    }

    return n;
}


static void
ngx_http_copy_aio_sendfile_event_handler(ngx_event_t *ev)
{
    ngx_event_aio_t     *aio;
    ngx_http_request_t  *r;

    aio = ev->data;
    r = aio->data;

    r->main->blocked--;
    r->aio = 0;
    ev->complete = 0;

    r->connection->write->handler(r->connection->write);
}

#endif
#endif


#if (NGX_THREADS)

static ngx_int_t
ngx_http_copy_thread_handler(ngx_thread_task_t *task, ngx_file_t *file)
{
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
    task->event.handler = ngx_http_copy_thread_event_handler;

    if (ngx_thread_task_post(tp, task) != NGX_OK) {
        return NGX_ERROR;
    }

    r->main->blocked++;
    r->aio = 1;

    return NGX_OK;
}


static void
ngx_http_copy_thread_event_handler(ngx_event_t *ev)
{
    ngx_http_request_t  *r;

    r = ev->data;

    r->main->blocked--;
    r->aio = 0;

    ngx_log_debugall(r->connection->log, 0, "ngx http aio thread event handler");
    //ngx_http_request_handler -> ngx_http_writer
    r->connection->write->handler(r->connection->write);
}

#endif


static void *
ngx_http_copy_filter_create_conf(ngx_conf_t *cf)
{
    ngx_http_copy_filter_conf_t *conf;

    conf = ngx_palloc(cf->pool, sizeof(ngx_http_copy_filter_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->bufs.num = 0;

    return conf;
}


static char *
ngx_http_copy_filter_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_copy_filter_conf_t *prev = parent;
    ngx_http_copy_filter_conf_t *conf = child;

    ngx_conf_merge_bufs_value(conf->bufs, prev->bufs, 1, 32768);

    return NULL;
}


static ngx_int_t
ngx_http_copy_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_copy_filter;

    return NGX_OK;
}

