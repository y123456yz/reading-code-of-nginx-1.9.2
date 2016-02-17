#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct
{
    ngx_str_t		stock[6];
} ngx_http_subrequest_mytest_ctx_t;

static char *ngx_http_subrequest_mytest(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_subrequest_mytest_handler(ngx_http_request_t *r);
static ngx_int_t mytest_subrequest_post_handler(ngx_http_request_t *r, void *data, ngx_int_t rc);
static void
mytest_post_handler(ngx_http_request_t * r);

static ngx_command_t  ngx_http_subrequest_mytest_commands[] =
{
    {
        ngx_string("subrequest_yyz"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS,
        ngx_http_subrequest_mytest,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },

    ngx_null_command
};

static ngx_http_module_t  ngx_http_subrequest_mytest_module_ctx =
{
    NULL,                              /* preconfiguration */
    NULL,                  		/* postconfiguration */

    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */

    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */

    NULL,       			/* create location configuration */
    NULL         			/* merge location configuration */
};

ngx_module_t  ngx_http_subrequest_mytest_module =
{
    NGX_MODULE_V1,
    &ngx_http_subrequest_mytest_module_ctx,           /* module context */
    ngx_http_subrequest_mytest_commands,              /* module directives */
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

static ngx_int_t mytest_subrequest_post_handler(ngx_http_request_t *r,
                                                void *data, ngx_int_t rc)
{
    //当前请求r是子请求，它的parent成员就指向父请求
    ngx_http_request_t          *pr = r->parent;
    //注意，上下文是保存在父请求中的，所以要由pr中取上下文。
//其实有更简单的方法，即参数data就是上下文，初始化subrequest时
//我们就对其进行设置了的，这里仅为了说明如何获取到父请求的上下文
    ngx_http_subrequest_mytest_ctx_t* myctx = ngx_http_get_module_ctx(pr, ngx_http_subrequest_mytest_module);

    pr->headers_out.status = r->headers_out.status;
    //如果返回NGX_HTTP_OK（也就是200）意味着访问新浪服务器成功，接着将开始解析http包体
    if (r->headers_out.status == NGX_HTTP_OK)
    {
        int flag = 0;

        //在不转发响应时，buffer中会保存着上游服务器的响应。特别是在使用
//反向代理模块访问上游服务器时，如果它使用upstream机制时没有重定义
//input_filter方法，upstream机制默认的input_filter方法会试图
//把所有的上游响应全部保存到buffer缓冲区中
        ngx_buf_t* pRecvBuf = &r->upstream->buffer;

        //以下开始解析上游服务器的响应，并将解析出的值赋到上下文结构体myctx->stock数组中
        for (; pRecvBuf->pos != pRecvBuf->last; pRecvBuf->pos++)
        {
            if (*pRecvBuf->pos == ',' || *pRecvBuf->pos == '\"')
            {
                if (flag > 0)
                {
                    myctx->stock[flag - 1].len = pRecvBuf->pos - myctx->stock[flag - 1].data;
                }
                flag++;
                myctx->stock[flag - 1].data = pRecvBuf->pos + 1;
            }

            if (flag > 6)
                break;
        }
    }

    
    //这一步很重要，设置接下来父请求的回调方法
    pr->write_event_handler = mytest_post_handler;

    return NGX_OK;
}

static void
mytest_post_handler(ngx_http_request_t * r)
{
    //如果没有返回200则直接把错误码发回用户
    if (r->headers_out.status != NGX_HTTP_OK)
    {
        ngx_http_finalize_request(r, r->headers_out.status);
        return;
    }

    //当前请求是父请求，直接取其上下文
    ngx_http_subrequest_mytest_ctx_t* myctx = ngx_http_get_module_ctx(r, ngx_http_subrequest_mytest_module);

    //定义发给用户的http包体内容，格式为：
//stock[…],Today current price: …, volumn: …
    ngx_str_t output_format = ngx_string("stock[%V],Today current price: %V, volumn: %V");

    //计算待发送包体的长度
    int bodylen = output_format.len + myctx->stock[0].len
                  + myctx->stock[1].len + myctx->stock[4].len - 6;
    r->headers_out.content_length_n = bodylen;

    //在内存池上分配内存保存将要发送的包体
    ngx_buf_t* b = ngx_create_temp_buf(r->pool, bodylen);
    ngx_snprintf(b->pos, bodylen, (char*)output_format.data,
                 &myctx->stock[0], &myctx->stock[1], &myctx->stock[4]);
    b->last = b->pos + bodylen;
    b->last_buf = 1;

    ngx_chain_t out;
    out.buf = b;
    out.next = NULL;
    //设置Content-Type，注意汉字编码新浪服务器使用了GBK
    static ngx_str_t type = ngx_string("text/plain; charset=GBK");
    r->headers_out.content_type = type;
    r->headers_out.status = NGX_HTTP_OK;

    r->connection->buffered |= NGX_HTTP_WRITE_BUFFERED;
    ngx_int_t ret = ngx_http_send_header(r);
    ret = ngx_http_output_filter(r, &out);

    //注意，这里发送完响应后必须手动调用ngx_http_finalize_request
//结束请求，因为这时http框架不会再帮忙调用它
    ngx_http_finalize_request(r, ret);
}

static char *
ngx_http_subrequest_mytest(ngx_conf_t * cf, ngx_command_t * cmd, void * conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    //首先找到mytest配置项所属的配置块，clcf貌似是location块内的数据
//结构，其实不然，它可以是main、srv或者loc级别配置项，也就是说在每个
//http{}和server{}内也都有一个ngx_http_core_loc_conf_t结构体
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    //http框架在处理用户请求进行到NGX_HTTP_CONTENT_PHASE阶段时，如果
//请求的主机域名、URI与mytest配置项所在的配置块相匹配，就将调用我们
//实现的ngx_http_mytest_handler方法处理这个请求
    clcf->handler = ngx_http_subrequest_mytest_handler;

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_subrequest_mytest_handler(ngx_http_request_t * r)
{
    //创建http上下文
    ngx_http_subrequest_mytest_ctx_t* myctx = ngx_http_get_module_ctx(r, ngx_http_subrequest_mytest_module);
    if (myctx == NULL)
    {
        myctx = ngx_palloc(r->pool, sizeof(ngx_http_subrequest_mytest_ctx_t));
        if (myctx == NULL)
        {
            return NGX_ERROR;
        }

        //将上下文设置到原始请求r中
        ngx_http_set_ctx(r, myctx, ngx_http_subrequest_mytest_module);
    }

    // ngx_http_post_subrequest_t结构体会决定子请求的回调方法   
    ngx_http_post_subrequest_t *psr = ngx_palloc(r->pool, sizeof(ngx_http_post_subrequest_t));
    if (psr == NULL)
    {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    //设置子请求回调方法为mytest_subrequest_post_handler
    psr->handler = mytest_subrequest_post_handler;

    //data设为myctx上下文，这样回调mytest_subrequest_post_handler时传入的data参数就是myctx
    psr->data = myctx;

    //子请求的URI前缀是/list，这是因为访问新浪服务器的请求必须是类
//似/list=s_sh000001这样的URI，
    ngx_str_t sub_prefix = ngx_string("/list=");
    ngx_str_t sub_location;
    sub_location.len = sub_prefix.len + r->args.len;
    sub_location.data = ngx_palloc(r->pool, sub_location.len);
    ngx_snprintf(sub_location.data, sub_location.len,
                 "%V%V", &sub_prefix, &r->args);

    //sr就是子请求
    ngx_http_request_t *sr;
    //调用ngx_http_subrequest创建子请求，它只会返回NGX_OK
//或者NGX_ERROR。返回NGX_OK时，sr就已经是合法的子请求。注意，这里
//的NGX_HTTP_SUBREQUEST_IN_MEMORY参数将告诉upstream模块把上
//游服务器的响应全部保存在子请求的sr->upstream->buffer内存缓冲区中
    ngx_int_t rc = ngx_http_subrequest(r, &sub_location, NULL, &sr, psr, NGX_HTTP_SUBREQUEST_IN_MEMORY);
    if (rc != NGX_OK)
    {
        return NGX_ERROR;
    }

    //必须返回NGX_DONE，理由同upstream
    return NGX_DONE;
}

