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

//子请求处理自己负责的后端响应读取，读取处理完毕后，触发父请求进行相应的合并等操作
static ngx_int_t mytest_subrequest_post_handler(ngx_http_request_t *r,
                                                void *data, ngx_int_t rc) //ngx_http_finalize_request中执行
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
                myctx->stock[flag - 1].data = pRecvBuf->pos + 1; //把从后端获取到的部分信息，保存到ctx中，父请求write_event_handler的时候使用
            }

            if (flag > 6)
                break;
        }
    }

    
    //这一步很重要，设置接下来父请求的回调方法   子请求接收完自己负责的那部分后端响应后，就通知父请求进行相应的处理
    //(例如多个不同的子请求分别对应后端多个不同的服务器，各个后端应答回来后就需要通知父请求就行合并等操作，
    pr->write_event_handler = mytest_post_handler; //ngx_http_core_run_phases中执行

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
    /*
    当读取完该子请求后端应答的数据后，会调用ngx_http_upstream_finalize_request->ngx_http_finalize_request，在ngx_http_finalize_request中执行该handler函数
    */
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

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "ngx_http_subrequest_mytest_handler, http subrequest \"%V?%V\"", &sub_prefix, &r->args);

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
    /*
     该函数由ngx_http_core_content_phase调用，然后去判断该返回值，11个阶段执行完后，会通过ngx_http_run_posted_requests
     对客户端请求r对应的所有子请求做重定向处理 
    */

    
    return NGX_DONE; 
}


/*
NGINX配置

location /list {
    proxy_pass http://hq.sinajs.cn;
}

location /query {
  subrequest_yyz;
}

http访问:http://192.168.50.63/query?s_sh000001


NGINX日志:
2017/03/01 15:25:40[                ngx_epoll_process_events,  1715]  [debug] 1562#1562: epoll: fd:6 EPOLLIN  (ev:0001) d:00007FC6EF55E010
2017/03/01 15:25:40[                ngx_epoll_process_events,  1761]  [debug] 1562#1562: post event 00007FC6EB95E010
2017/03/01 15:25:40[           ngx_process_events_and_timers,   395]  [debug] 1562#1562: epoll_wait timer range(delta): 12245
2017/03/01 15:25:40[                ngx_event_process_posted,    62]  [debug] 1562#1562: begin to run befor posted event 00007FC6EB95E010
2017/03/01 15:25:40[                ngx_event_process_posted,    64]  [debug] 1562#1562: delete posted event 00007FC6EB95E010
2017/03/01 15:25:40[                        ngx_event_accept,    72]  [debug] 1562#1562: accept on 0.0.0.0:80, ready: 1
2017/03/01 15:25:40[                        ngx_event_accept,   371]  [debug] 1562#1562: *1 accept: 192.168.50.150:63646 fd:3
2017/03/01 15:25:40[                     ngx_event_add_timer,   100]  [debug] 1562#1562: *1 < ngx_http_init_connection,   402>  event timer add fd:3, expire-time:60 s, timer.key:1488353200893
2017/03/01 15:25:40[                 ngx_reusable_connection,  1177]  [debug] 1562#1562: *1 reusable connection: 1
2017/03/01 15:25:40[                   ngx_handle_read_event,   499]  [debug] 1562#1562: *1 < ngx_http_init_connection,   405> epoll NGX_USE_CLEAR_EVENT(et) read add
2017/03/01 15:25:40[                     ngx_epoll_add_event,  1404]  [debug] 1562#1562: *1 epoll add read event: fd:3 op:1 ev:80002001
2017/03/01 15:25:40[                        ngx_event_accept,   100]  [debug] 1562#1562: accept() not ready (11: Resource temporarily unavailable)
2017/03/01 15:25:40[                ngx_trylock_accept_mutex,   406]  [debug] 1562#1562: accept mutex locked
2017/03/01 15:25:40[                ngx_epoll_process_events,  1715]  [debug] 1562#1562: epoll: fd:6 EPOLLIN  (ev:0001) d:00007FC6EF55E010
2017/03/01 15:25:40[                ngx_epoll_process_events,  1761]  [debug] 1562#1562: post event 00007FC6EB95E010
2017/03/01 15:25:40[           ngx_process_events_and_timers,   395]  [debug] 1562#1562: epoll_wait timer range(delta): 3
2017/03/01 15:25:40[                ngx_event_process_posted,    62]  [debug] 1562#1562: begin to run befor posted event 00007FC6EB95E010
2017/03/01 15:25:40[                ngx_event_process_posted,    64]  [debug] 1562#1562: delete posted event 00007FC6EB95E010
2017/03/01 15:25:40[                        ngx_event_accept,    72]  [debug] 1562#1562: accept on 0.0.0.0:80, ready: 1
2017/03/01 15:25:40[                        ngx_event_accept,   371]  [debug] 1562#1562: *2 accept: 192.168.50.150:63647 fd:12
2017/03/01 15:25:40[                     ngx_event_add_timer,   100]  [debug] 1562#1562: *2 < ngx_http_init_connection,   402>  event timer add fd:12, expire-time:60 s, timer.key:1488353200896
2017/03/01 15:25:40[                 ngx_reusable_connection,  1177]  [debug] 1562#1562: *2 reusable connection: 1
2017/03/01 15:25:40[                   ngx_handle_read_event,   499]  [debug] 1562#1562: *2 < ngx_http_init_connection,   405> epoll NGX_USE_CLEAR_EVENT(et) read add
2017/03/01 15:25:40[                     ngx_epoll_add_event,  1404]  [debug] 1562#1562: *2 epoll add read event: fd:12 op:1 ev:80002001
2017/03/01 15:25:40[                        ngx_event_accept,   100]  [debug] 1562#1562: accept() not ready (11: Resource temporarily unavailable)
2017/03/01 15:25:40[                ngx_trylock_accept_mutex,   406]  [debug] 1562#1562: accept mutex locked
2017/03/01 15:25:40[                ngx_epoll_process_events,  1715]  [debug] 1562#1562: epoll: fd:12 EPOLLIN  (ev:0001) d:00007FC6EF55E370
2017/03/01 15:25:40[                ngx_epoll_process_events,  1761]  [debug] 1562#1562: *2 post event 00007FC6EB95E190
2017/03/01 15:25:40[           ngx_process_events_and_timers,   395]  [debug] 1562#1562: epoll_wait timer range(delta): 6
2017/03/01 15:25:40[                ngx_event_process_posted,    62]  [debug] 1562#1562: begin to run befor posted event 00007FC6EB95E190
2017/03/01 15:25:40[                ngx_event_process_posted,    64]  [debug] 1562#1562: *2 delete posted event 00007FC6EB95E190
2017/03/01 15:25:40[           ngx_http_wait_request_handler,   426]  [debug] 1562#1562: *2 http wait request handler
2017/03/01 15:25:40[                           ngx_unix_recv,   210]  [debug] 1562#1562: *2 recv: fd:12 read-size:463 of 1024, ready:1
2017/03/01 15:25:40[                 ngx_reusable_connection,  1177]  [debug] 1562#1562: *2 reusable connection: 0
2017/03/01 15:25:40[           ngx_http_process_request_line,   971]  [debug] 1562#1562: *2 http process request line
2017/03/01 15:25:40[           ngx_http_process_request_line,  1010]  [debug] 1562#1562: *2 http request line: "GET /query?s_sh000001 HTTP/1.1"
2017/03/01 15:25:40[            ngx_http_process_request_uri,  1237]  [debug] 1562#1562: *2 http uri: "/query"
2017/03/01 15:25:40[            ngx_http_process_request_uri,  1240]  [debug] 1562#1562: *2 http args: "s_sh000001"
2017/03/01 15:25:40[            ngx_http_process_request_uri,  1243]  [debug] 1562#1562: *2 http exten: ""
2017/03/01 15:25:40[        ngx_http_process_request_headers,  1286]  [debug] 1562#1562: *2 http process request header line
2017/03/01 15:25:40[        ngx_http_process_request_headers,  1418]  [debug] 1562#1562: *2 http header: "Host: 192.168.50.63"
2017/03/01 15:25:40[        ngx_http_process_request_headers,  1418]  [debug] 1562#1562: *2 http header: "Connection: keep-alive"
2017/03/01 15:25:40[        ngx_http_process_request_headers,  1418]  [debug] 1562#1562: *2 http header: "Cache-Control: max-age=0"
2017/03/01 15:25:40[        ngx_http_process_request_headers,  1418]  [debug] 1562#1562: *2 http header: "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,* / *;q=0.8"
2017/03/01 15:25:40[        ngx_http_process_request_headers,  1418]  [debug] 1562#1562: *2 http header: "Upgrade-Insecure-Requests: 1"
2017/03/01 15:25:40[        ngx_http_process_request_headers,  1418]  [debug] 1562#1562: *2 http header: "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/48.0.2564.116 Safari/537.36"
2017/03/01 15:25:40[        ngx_http_process_request_headers,  1418]  [debug] 1562#1562: *2 http header: "Accept-Encoding: gzip, deflate, sdch"
2017/03/01 15:25:40[        ngx_http_process_request_headers,  1418]  [debug] 1562#1562: *2 http header: "Accept-Language: zh-CN,zh;q=0.8"
2017/03/01 15:25:40[        ngx_http_process_request_headers,  1418]  [debug] 1562#1562: *2 http header: "Cookie: sinaGlobalRotator_http%3A//192.168.50=98"
2017/03/01 15:25:40[        ngx_http_process_request_headers,  1428]  [debug] 1562#1562: *2 http header done
2017/03/01 15:25:40[                     ngx_event_del_timer,    39]  [debug] 1562#1562: *2 < ngx_http_process_request,  2029>  event timer del: 12: 1488353200896
2017/03/01 15:25:40[             ngx_http_core_rewrite_phase,  1997]  [debug] 1562#1562: *2 rewrite phase: 0 (NGX_HTTP_SERVER_REWRITE_PHASE)
2017/03/01 15:25:40[         ngx_http_core_find_config_phase,  2055]  [debug] 1562#1562: *2 find config phase: 1 (NGX_HTTP_FIND_CONFIG_PHASE), uri:/query
2017/03/01 15:25:40[      ngx_http_core_find_static_location,  2947]  [debug] 1562#1562: *2 test location: "/query"
2017/03/01 15:25:40[             ngx_http_core_find_location,  2883]  [debug] 1562#1562: *2 ngx pcre test location: ~ "\.php$"
2017/03/01 15:25:40[         ngx_http_core_find_config_phase,  2076]  [debug] 1562#1562: *2 using configuration "/query"
2017/03/01 15:25:40[         ngx_http_core_find_config_phase,  2083]  [debug] 1562#1562: *2 http cl:-1 max:1048576, rc:-5
2017/03/01 15:25:40[             ngx_http_core_rewrite_phase,  1997]  [debug] 1562#1562: *2 rewrite phase: 2 (NGX_HTTP_REWRITE_PHASE)
2017/03/01 15:25:40[        ngx_http_core_post_rewrite_phase,  2151]  [debug] 1562#1562: *2 post rewrite phase: 3 (NGX_HTTP_POST_REWRITE_PHASE)
2017/03/01 15:25:40[             ngx_http_core_generic_phase,  1933]  [debug] 1562#1562: *2 generic phase: 4 (NGX_HTTP_PREACCESS_PHASE)
2017/03/01 15:25:40[             ngx_http_core_generic_phase,  1933]  [debug] 1562#1562: *2 generic phase: 5 (NGX_HTTP_PREACCESS_PHASE)
2017/03/01 15:25:40[              ngx_http_core_access_phase,  2249]  [debug] 1562#1562: *2 access phase: 6 (NGX_HTTP_ACCESS_PHASE)
2017/03/01 15:25:40[              ngx_http_core_access_phase,  2249]  [debug] 1562#1562: *2 access phase: 7 (NGX_HTTP_ACCESS_PHASE)
2017/03/01 15:25:40[         ngx_http_core_post_access_phase,  2351]  [debug] 1562#1562: *2 post access phase: 8 (NGX_HTTP_POST_ACCESS_PHASE)
2017/03/01 15:25:40[             ngx_http_core_content_phase,  2673]  [debug] 1562#1562: *2 content phase(content_handler): 9 (NGX_HTTP_CONTENT_PHASE)
2017/03/01 15:25:40[      ngx_http_subrequest_mytest_handler,   219]  [debug] 1562#1562: *2 ngx_http_subrequest_mytest_handler, http subrequest "/list=?s_sh000001"
2017/03/01 15:25:40[                     ngx_http_subrequest,  3999]  [debug] 1562#1562: *2 http subrequest "/list=s_sh000001?"
2017/03/01 15:25:40[               ngx_http_finalize_request,  2593]  [debug] 1562#1562: *2 http finalize request rc: -4, "/query?s_sh000001" a:0, c:2, b:0, p:000000000194AB88
2017/03/01 15:25:40[                  ngx_http_close_request,  3921]  [debug] 1562#1562: *2 http request count:2 blk:0
2017/03/01 15:25:40[            ngx_http_run_posted_requests,  2525]  [debug] 1562#1562: *2 http posted request: "/list=s_sh000001?"
2017/03/01 15:25:40[             ngx_http_core_rewrite_phase,  1997]  [debug] 1562#1562: *2 rewrite phase: 0 (NGX_HTTP_SERVER_REWRITE_PHASE)
2017/03/01 15:25:40[         ngx_http_core_find_config_phase,  2055]  [debug] 1562#1562: *2 find config phase: 1 (NGX_HTTP_FIND_CONFIG_PHASE), uri:/list=s_sh000001
2017/03/01 15:25:40[      ngx_http_core_find_static_location,  2947]  [debug] 1562#1562: *2 test location: "/query"
2017/03/01 15:25:40[      ngx_http_core_find_static_location,  2947]  [debug] 1562#1562: *2 test location: "/list"
2017/03/01 15:25:40[             ngx_http_core_find_location,  2883]  [debug] 1562#1562: *2 ngx pcre test location: ~ "\.php$"
2017/03/01 15:25:40[         ngx_http_core_find_config_phase,  2076]  [debug] 1562#1562: *2 using configuration "/list"
2017/03/01 15:25:40[         ngx_http_core_find_config_phase,  2083]  [debug] 1562#1562: *2 http cl:-1 max:1048576, rc:-5
2017/03/01 15:25:40[             ngx_http_core_rewrite_phase,  1997]  [debug] 1562#1562: *2 rewrite phase: 2 (NGX_HTTP_REWRITE_PHASE)
2017/03/01 15:25:40[        ngx_http_core_post_rewrite_phase,  2151]  [debug] 1562#1562: *2 post rewrite phase: 3 (NGX_HTTP_POST_REWRITE_PHASE)
2017/03/01 15:25:40[             ngx_http_core_generic_phase,  1933]  [debug] 1562#1562: *2 generic phase: 4 (NGX_HTTP_PREACCESS_PHASE)
2017/03/01 15:25:40[           ngx_http_script_copy_var_code,   989]  [debug] 1562#1562: *2 http script var: "括2

                                                                                                                  2017/03/01 15:25:40[                          ngx_shmtx_lock,   168]  [debug] 1562#1562: shmtx lock
2017/03/01 15:25:40[                        ngx_shmtx_unlock,   249]  [debug] 1562#1562: shmtx unlock
2017/03/01 15:25:40[              ngx_http_limit_req_handler,   261]  [debug] 1562#1562: *2 limit_req[0]: 0 0.000
2017/03/01 15:25:40[             ngx_http_core_generic_phase,  1933]  [debug] 1562#1562: *2 generic phase: 5 (NGX_HTTP_PREACCESS_PHASE)
2017/03/01 15:25:40[             ngx_http_core_content_phase,  2673]  [debug] 1562#1562: *2 content phase(content_handler): 9 (NGX_HTTP_CONTENT_PHASE)
2017/03/01 15:25:40[                  ngx_http_upstream_init,   647]  [debug] 1562#1562: *2 http init upstream, client timer: 0
2017/03/01 15:25:40[                  ngx_http_upstream_init,   681]  [debug] 1562#1562: *2 <   ngx_http_upstream_init,   680> epoll NGX_WRITE_EVENT(et) write add
2017/03/01 15:25:40[                     ngx_epoll_add_event,  1400]  [debug] 1562#1562: *2 epoll modify read and write event: fd:12 op:3 ev:80002005
2017/03/01 15:25:40[               ngx_http_script_copy_code,   865]  [debug] 1562#1562: *2 http script copy: "Host: "
2017/03/01 15:25:40[           ngx_http_script_copy_var_code,   989]  [debug] 1562#1562: *2 http script var: "hq.sinajs.cn"
2017/03/01 15:25:40[               ngx_http_script_copy_code,   865]  [debug] 1562#1562: *2 http script copy: "
"
2017/03/01 15:25:40[               ngx_http_script_copy_code,   865]  [debug] 1562#1562: *2 http script copy: "Connection: close
"
2017/03/01 15:25:40[               ngx_http_script_copy_code,   865]  [debug] 1562#1562: *2 http script copy: ""
2017/03/01 15:25:40[               ngx_http_script_copy_code,   865]  [debug] 1562#1562: *2 http script copy: ""
2017/03/01 15:25:40[               ngx_http_script_copy_code,   865]  [debug] 1562#1562: *2 http script copy: ""
2017/03/01 15:25:40[               ngx_http_script_copy_code,   865]  [debug] 1562#1562: *2 http script copy: ""
2017/03/01 15:25:40[           ngx_http_proxy_create_request,  2134]  [debug] 1562#1562: *2 http proxy header: "Cache-Control: max-age=0"
2017/03/01 15:25:40[           ngx_http_proxy_create_request,  2134]  [debug] 1562#1562: *2 http proxy header: "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,* / *;q=0.8"
2017/03/01 15:25:40[           ngx_http_proxy_create_request,  2134]  [debug] 1562#1562: *2 http proxy header: "Upgrade-Insecure-Requests: 1"
2017/03/01 15:25:40[           ngx_http_proxy_create_request,  2134]  [debug] 1562#1562: *2 http proxy header: "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/48.0.2564.116 Safari/537.36"
2017/03/01 15:25:40[           ngx_http_proxy_create_request,  2134]  [debug] 1562#1562: *2 http proxy header: "Accept-Encoding: gzip, deflate, sdch"
2017/03/01 15:25:40[           ngx_http_proxy_create_request,  2134]  [debug] 1562#1562: *2 http proxy header: "Accept-Language: zh-CN,zh;q=0.8"
2017/03/01 15:25:40[           ngx_http_proxy_create_request,  2134]  [debug] 1562#1562: *2 http proxy header: "Cookie: sinaGlobalRotator_http%3A//192.168.50=98"
2017/03/01 15:25:40[           ngx_http_proxy_create_request,  2159]  [debug] 1562#1562: *2 http proxy header:
"GET /list=s_sh000001 HTTP/1.0
Host: hq.sinajs.cn
Connection: close
Cache-Control: max-age=0
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,* / *;q=0.8
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/48.0.2564.116 Safari/537.36
Accept-Encoding: gzip, deflate, sdch
Accept-Language: zh-CN,zh;q=0.8
Cookie: sinaGlobalRotator_http%3A//192.168.50=98

"
2017/03/01 15:25:40[                    ngx_http_cleanup_add,  4261]  [debug] 1562#1562: *2 http cleanup add: 000000000194AC78
2017/03/01 15:25:40[  ngx_http_upstream_get_round_robin_peer,   622]  [debug] 1562#1562: *2 get rr peer, try: 2
2017/03/01 15:25:40[  ngx_http_upstream_get_round_robin_peer,   652]  [debug] 1562#1562: *2 get rr peer, current: 0000000001969A70 -1
2017/03/01 15:25:40[                  ngx_event_connect_peer,    33]  [debug] 1562#1562: *2 socket 13
2017/03/01 15:25:40[                ngx_epoll_add_connection,  1495]  [debug] 1562#1562: *2 epoll add connection(read and write): fd:13 ev:80002005
2017/03/01 15:25:40[                  ngx_event_connect_peer,   131]  [debug] 1562#1562: *2 connect to 202.102.94.120:80, fd:13 #3
2017/03/01 15:25:40[               ngx_http_upstream_connect,  1690]  [debug] 1562#1562: *2 http upstream connect: -2
2017/03/01 15:25:40[                     ngx_event_add_timer,   100]  [debug] 1562#1562: *2 <ngx_http_upstream_connect,  1840>  event timer add fd:13, expire-time:60 s, timer.key:1488353200902
2017/03/01 15:25:40[               ngx_http_finalize_request,  2593]  [debug] 1562#1562: *2 http finalize request rc: -4, "/list=s_sh000001?" a:1, c:2, b:0, p:0000000000000000
2017/03/01 15:25:40[                  ngx_http_close_request,  3921]  [debug] 1562#1562: *2 http request count:2 blk:0
2017/03/01 15:25:40[                ngx_trylock_accept_mutex,   406]  [debug] 1562#1562: accept mutex locked
2017/03/01 15:25:40[                ngx_epoll_process_events,  1715]  [debug] 1562#1562: epoll: fd:12 EPOLLOUT  (ev:0004) d:00007FC6EF55E370
2017/03/01 15:25:40[                ngx_epoll_process_events,  1788]  [debug] 1562#1562: *2 post event 00007FC6E7D5E190
2017/03/01 15:25:40[           ngx_process_events_and_timers,   395]  [debug] 1562#1562: epoll_wait timer range(delta): 1
2017/03/01 15:25:40[                ngx_event_process_posted,    62]  [debug] 1562#1562: begin to run befor posted event 00007FC6E7D5E190
2017/03/01 15:25:40[                ngx_event_process_posted,    64]  [debug] 1562#1562: *2 delete posted event 00007FC6E7D5E190
2017/03/01 15:25:40[                ngx_http_request_handler,  2416]  [debug] 1562#1562: *2 http run request(ev->write:1): "/list=s_sh000001?"
2017/03/01 15:25:40[ngx_http_upstream_check_broken_connection,  1475]  [debug] 1562#1562: *2 http upstream check client, write event:1, "/list=s_sh000001"
2017/03/01 15:25:40[ngx_http_upstream_check_broken_connection,  1598]  [debug] 1562#1562: *2 http upstream recv() size: -1, fd:12 (11: Resource temporarily unavailable)
2017/03/01 15:25:40[                ngx_trylock_accept_mutex,   406]  [debug] 1562#1562: accept mutex locked
2017/03/01 15:25:40[                ngx_epoll_process_events,  1715]  [debug] 1562#1562: epoll: fd:13 EPOLLOUT  (ev:0004) d:00007FC6EF55E448
2017/03/01 15:25:40[                ngx_epoll_process_events,  1788]  [debug] 1562#1562: *2 post event 00007FC6E7D5E1F0
2017/03/01 15:25:40[           ngx_process_events_and_timers,   395]  [debug] 1562#1562: epoll_wait timer range(delta): 8
2017/03/01 15:25:40[                ngx_event_process_posted,    62]  [debug] 1562#1562: begin to run befor posted event 00007FC6E7D5E1F0
2017/03/01 15:25:40[                ngx_event_process_posted,    64]  [debug] 1562#1562: *2 delete posted event 00007FC6E7D5E1F0
2017/03/01 15:25:40[               ngx_http_upstream_handler,  1414]  [debug] 1562#1562: *2 http upstream request(ev->write:1): "/list=s_sh000001?"
2017/03/01 15:25:40[  ngx_http_upstream_send_request_handler,  2445]  [debug] 1562#1562: *2 http upstream send request handler
2017/03/01 15:25:40[          ngx_http_upstream_send_request,  2181]  [debug] 1562#1562: *2 http upstream send request
2017/03/01 15:25:40[     ngx_http_upstream_send_request_body,  2330]  [debug] 1562#1562: *2 http upstream send request body
2017/03/01 15:25:40[                   ngx_output_chain,   161][yangya  [debug] 1562#1562: *2 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2017/03/01 15:25:40[             ngx_output_chain_as_is,   411][yangya  [debug] 1562#1562: ngx_output_chain_as_is--- buf_special:0, in_file:0, buf_in_mem:1,need_in_memory:240518168576, need_in_temp:140733193388032, memory:0, mmap:140733193388032
2017/03/01 15:25:40[                   ngx_output_chain,   185][yangya  [debug] 1562#1562: *2 only one chain buf to output_filter
2017/03/01 15:25:40[                        ngx_chain_writer,  1011]  [debug] 1562#1562: *2 chain writer buf fl:1 s:456
2017/03/01 15:25:40[                        ngx_chain_writer,  1026]  [debug] 1562#1562: *2 chain writer in: 000000000194BB50
2017/03/01 15:25:40[           ngx_linux_sendfile_chain,   201][yangya  [debug] 1562#1562: *2 @@@@@@@@@@@@@@@@@@@@@@@begin ngx_linux_sendfile_chain @@@@@@@@@@@@@@@@@@@
2017/03/01 15:25:40[                              ngx_writev,   238]  [debug] 1562#1562: *2 writev: 456 of 456
2017/03/01 15:25:40[                        ngx_chain_writer,  1065]  [debug] 1562#1562: *2 chain writer out: 0000000000000000
2017/03/01 15:25:40[                     ngx_event_del_timer,    39]  [debug] 1562#1562: *2 <ngx_http_upstream_send_request,  2277>  event timer del: 13: 1488353200902
2017/03/01 15:25:40[          ngx_http_upstream_send_request,  2295]  [debug] 1562#1562: *2 send out chain data to uppeer server OK
2017/03/01 15:25:40[                     ngx_event_add_timer,   100]  [debug] 1562#1562: *2 <ngx_http_upstream_send_request,  2310>  event timer add fd:13, expire-time:60 s, timer.key:1488353200911
2017/03/01 15:25:40[                ngx_trylock_accept_mutex,   406]  [debug] 1562#1562: accept mutex locked
2017/03/01 15:25:40[                ngx_epoll_process_events,  1715]  [debug] 1562#1562: epoll: fd:13 EPOLLIN EPOLLOUT  (ev:2005) d:00007FC6EF55E448
2017/03/01 15:25:40[                ngx_epoll_process_events,  1761]  [debug] 1562#1562: *2 post event 00007FC6EB95E1F0
2017/03/01 15:25:40[                ngx_epoll_process_events,  1788]  [debug] 1562#1562: *2 post event 00007FC6E7D5E1F0
2017/03/01 15:25:40[           ngx_process_events_and_timers,   395]  [debug] 1562#1562: epoll_wait timer range(delta): 11
2017/03/01 15:25:40[                ngx_event_process_posted,    62]  [debug] 1562#1562: begin to run befor posted event 00007FC6EB95E1F0
2017/03/01 15:25:40[                ngx_event_process_posted,    64]  [debug] 1562#1562: *2 delete posted event 00007FC6EB95E1F0
2017/03/01 15:25:40[               ngx_http_upstream_handler,  1414]  [debug] 1562#1562: *2 http upstream request(ev->write:0): "/list=s_sh000001?"
2017/03/01 15:25:40[        ngx_http_upstream_process_header,  2511]  [debug] 1562#1562: *2 http upstream process header, fd:13, buffer_size:4096
2017/03/01 15:25:40[                           ngx_unix_recv,   210]  [debug] 1562#1562: *2 recv: fd:13 read-size:214 of 4096, ready:1
2017/03/01 15:25:40[      ngx_http_proxy_process_status_line,  2466]  [debug] 1562#1562: *2 http proxy status 200 "200 OK"
2017/03/01 15:25:40[           ngx_http_proxy_process_header,  2544]  [debug] 1562#1562: *2 http proxy header: "Cache-Control: no-cache"
2017/03/01 15:25:40[           ngx_http_proxy_process_header,  2544]  [debug] 1562#1562: *2 http proxy header: "Content-Length: 73"
2017/03/01 15:25:40[           ngx_http_proxy_process_header,  2544]  [debug] 1562#1562: *2 http proxy header: "Connection: Keep-Alive"
2017/03/01 15:25:40[           ngx_http_proxy_process_header,  2544]  [debug] 1562#1562: *2 http proxy header: "Content-Type: application/x-javascript; charset=GBK"
2017/03/01 15:25:40[           ngx_http_proxy_process_header,  2554]  [debug] 1562#1562: *2 http proxy header done
2017/03/01 15:25:40[      ngx_http_proxy_process_header,  2622][yangya  [debug] 1562#1562: *2 upstream header recv ok, u->keepalive:0
2017/03/01 15:25:40[           ngx_http_proxy_process_header,  2625]  [debug] 1562#1562: *2 yang test .... body:var hq_str_s_sh000001="上证指数,3246.9335,5.2004,0.16,1906775,22594097";

2017/03/01 15:25:40[        ngx_http_proxy_input_filter_init,  2658]  [debug] 1562#1562: *2 http proxy filter init upstream status:200 is HEAD:0 chunked:0 content_length_n:73
2017/03/01 15:25:40[      ngx_http_upstream_finalize_request,  4568]  [debug] 1562#1562: *2 finalize http upstream request rc: 0
2017/03/01 15:25:40[         ngx_http_proxy_finalize_request,  3076]  [debug] 1562#1562: *2 finalize http proxy request
2017/03/01 15:25:40[ ngx_http_upstream_free_round_robin_peer,   889]  [debug] 1562#1562: *2 free rr peer 2 0
2017/03/01 15:25:40[      ngx_http_upstream_finalize_request,  4621]  [debug] 1562#1562: *2 close http upstream connection: 13
2017/03/01 15:25:40[                     ngx_event_del_timer,    39]  [debug] 1562#1562: *2 <     ngx_close_connection,  1090>  event timer del: 13: 1488353200911
2017/03/01 15:25:40[                    ngx_close_connection,  1120]  [debug] 1562#1562: *2 delete posted event 00007FC6E7D5E1F0
2017/03/01 15:25:40[                 ngx_reusable_connection,  1177]  [debug] 1562#1562: *2 reusable connection: 0
2017/03/01 15:25:40[               ngx_close_connection,  1139][yangya  [debug] 1562#1562: close socket:13
2017/03/01 15:25:40[               ngx_http_finalize_request,  2593]  [debug] 1562#1562: *2 http finalize request rc: 0, "/list=s_sh000001?" a:1, c:1, b:0, p:0000000000000000
2017/03/01 15:25:40[               ngx_http_finalize_request,  2779]  [debug] 1562#1562: *2 http wake parent request: "/query?s_sh000001"
2017/03/01 15:25:40[            ngx_http_run_posted_requests,  2525]  [debug] 1562#1562: *2 http posted request: "/query?s_sh000001"
2017/03/01 15:25:40[               ngx_http_send_header,  3359][yangya  [debug] 1562#1562: *2 ngx http send header
2017/03/01 15:25:40[                  ngx_http_header_filter,   677]  [debug] 1562#1562: *2 HTTP/1.1 200 OK
Server: nginx/1.9.2
Date: Wed, 01 Mar 2017 07:25:40 GMT
Content-Type: text/plain; charset=GBK
Content-Length: 63
Connection: keep-alive

2017/03/01 15:25:40[                   ngx_http_write_filter,   208]  [debug] 1562#1562: *2 write new buf temporary:1 buf-in-file:0, buf->start:000000000194C518, buf->pos:000000000194C518, buf_size: 160 file_pos: 0, in_file_size: 0
2017/03/01 15:25:40[                   ngx_http_write_filter,   248]  [debug] 1562#1562: *2 http write filter: last:0 flush:0 size:160
2017/03/01 15:25:40[              ngx_http_write_filter,   290][yangya  [debug] 1562#1562: *2 send size:160 < min postpone_output:1460, do not send
2017/03/01 15:25:40[                  ngx_http_output_filter,  3412]  [debug] 1562#1562: *2 http output filter "/query?s_sh000001"
2017/03/01 15:25:40[                    ngx_http_copy_filter,   297]  [debug] 1562#1562: *2 http copy filter: "/query?s_sh000001", r->aio:0
2017/03/01 15:25:40[                   ngx_output_chain,   161][yangya  [debug] 1562#1562: *2 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2017/03/01 15:25:40[             ngx_output_chain_as_is,   411][yangya  [debug] 1562#1562: ngx_output_chain_as_is--- buf_special:0, in_file:0, buf_in_mem:1,need_in_memory:140733193388032, need_in_temp:140733193388032, memory:240518168576, mmap:206158430208
2017/03/01 15:25:40[                   ngx_output_chain,   185][yangya  [debug] 1562#1562: *2 only one chain buf to output_filter
2017/03/01 15:25:40[                ngx_http_postpone_filter,   158]  [debug] 1562#1562: *2 http postpone filter "/query?s_sh000001" 00007FFF6BF4D030
2017/03/01 15:25:40[                   ngx_http_write_filter,   151]  [debug] 1562#1562: *2 write old buf t:1 f:0 000000000194C518, pos 000000000194C518, size: 160 file: 0, size: 0
2017/03/01 15:25:40[                   ngx_http_write_filter,   208]  [debug] 1562#1562: *2 write new buf temporary:1 buf-in-file:0, buf->start:000000000194C488, buf->pos:000000000194C488, buf_size: 63 file_pos: 0, in_file_size: 0
2017/03/01 15:25:40[                   ngx_http_write_filter,   248]  [debug] 1562#1562: *2 http write filter: last:1 flush:0 size:223
2017/03/01 15:25:40[                   ngx_http_write_filter,   380]  [debug] 1562#1562: *2 http write filter limit 0
2017/03/01 15:25:40[           ngx_linux_sendfile_chain,   201][yangya  [debug] 1562#1562: *2 @@@@@@@@@@@@@@@@@@@@@@@begin ngx_linux_sendfile_chain @@@@@@@@@@@@@@@@@@@
2017/03/01 15:25:40[                              ngx_writev,   238]  [debug] 1562#1562: *2 writev: 223 of 223
2017/03/01 15:25:40[                   ngx_http_write_filter,   386]  [debug] 1562#1562: *2 http write filter 0000000000000000
2017/03/01 15:25:40[                    ngx_http_copy_filter,   376]  [debug] 1562#1562: *2 http copy filter rc: 0, buffered:0 "/query?s_sh000001"
2017/03/01 15:25:40[               ngx_http_finalize_request,  2593]  [debug] 1562#1562: *2 http finalize request rc: 0, "/query?s_sh000001" a:1, c:0, b:0, p:0000000000000000
2017/03/01 15:25:40[                  ngx_http_close_request,  3921]  [debug] 1562#1562: *2 http request count:0 blk:0
2017/03/01 15:25:40[                  ngx_http_close_request,  3924]  [alert] 1562#1562: *2 http request count is zero while sending to client, client: 192.168.50.150, server: , request: "GET /query?s_sh000001 HTTP/1.1", host: "192.168.50.63"


*/
