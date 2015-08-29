
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

//in为需要发送的chain链，上面存储的是实际要发送的数据
static ngx_int_t
ngx_http_copy_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
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
            ctx->thread_handler = ngx_http_copy_thread_handler;
        }
#endif

        if (in && in->buf && ngx_buf_size(in->buf)) { //判断in链中是否有数据
            r->request_output = 1;
        }
    }

#if (NGX_HAVE_FILE_AIO || NGX_THREADS)
    ctx->aio = r->aio;
#endif

    rc = ngx_output_chain(ctx, in);

    if (ctx->in == NULL) {
        r->buffered &= ~NGX_HTTP_COPY_BUFFERED;

    } else {
        r->buffered |= NGX_HTTP_COPY_BUFFERED;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http copy filter: %i \"%V?%V\"", rc, &r->uri, &r->args);

    return rc;
}


#if (NGX_HAVE_FILE_AIO)
//执行在ngx_output_chain_copy_buf->ngx_http_copy_aio_handler
static void
ngx_http_copy_aio_handler(ngx_output_chain_ctx_t *ctx, ngx_file_t *file)
{
    ngx_http_request_t *r;

    r = ctx->filter_ctx;

    file->aio->data = r;
    file->aio->handler = ngx_http_copy_aio_event_handler;

    r->main->blocked++;
    r->aio = 1;
    ctx->aio = 1;
}

//ngx_file_aio_event_handler中执行
static void
ngx_http_copy_aio_event_handler(ngx_event_t *ev)
{
    ngx_event_aio_t     *aio;
    ngx_http_request_t  *r;

    aio = ev->data;
    r = aio->data;

    r->main->blocked--;
    r->aio = 0;

    //ngx_http_request_handler->ngx_http_upstream_process_non_buffered_downstream
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

