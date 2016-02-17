#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct
{
    ngx_http_status_t           status;
    ngx_str_t					backendServer;
} ngx_http_mytest_upstream_ctx_t;

typedef struct
{
    ngx_http_upstream_conf_t upstream;
} ngx_http_mytest_upstream_conf_t;


static char *
ngx_http_mytest_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_int_t ngx_http_mytest_upstream_handler(ngx_http_request_t *r);
static void* ngx_http_mytest_upstream_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_mytest_upstream_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

static ngx_int_t
mytest_upstream_process_header(ngx_http_request_t *r);
static ngx_int_t
mytest_process_status_line(ngx_http_request_t *r);


static ngx_str_t  ngx_http_proxy_hide_headers[] =
{
    ngx_string("Date"),
    ngx_string("Server"),
    ngx_string("X-Pad"),
    ngx_string("X-Accel-Expires"),
    ngx_string("X-Accel-Redirect"),
    ngx_string("X-Accel-Limit-Rate"),
    ngx_string("X-Accel-Buffering"),
    ngx_string("X-Accel-Charset"),
    ngx_null_string
};

static ngx_command_t  ngx_http_mytest_upstream_commands[] =
{

    {
        ngx_string("mytest_upstream"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS,
        ngx_http_mytest_upstream,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },

    ngx_null_command
};

static ngx_http_module_t  ngx_http_mytest_upstream_module_ctx =
{
    NULL,                              /* preconfiguration */
    NULL,                  		/* postconfiguration */

    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */

    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */

    ngx_http_mytest_upstream_create_loc_conf,       			/* create location configuration */
    ngx_http_mytest_upstream_merge_loc_conf         			/* merge location configuration */
};

ngx_module_t  ngx_http_mytest_upstream_module =
{
    NGX_MODULE_V1,
    &ngx_http_mytest_upstream_module_ctx,     /* module context */
    ngx_http_mytest_upstream_commands,        /* module directives */
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


static void* ngx_http_mytest_upstream_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_mytest_upstream_conf_t  *mycf;

    mycf = (ngx_http_mytest_upstream_conf_t  *)ngx_pcalloc(cf->pool, sizeof(ngx_http_mytest_upstream_conf_t));
    if (mycf == NULL)
    {
        return NULL;
    }

    //以下简单的硬编码ngx_http_upstream_conf_t结构中的各成员，例如
//超时时间都设为1分钟。这也是http反向代理模块的默认值
    mycf->upstream.connect_timeout = 60000;
    mycf->upstream.send_timeout = 60000;
    mycf->upstream.read_timeout = 60000;
    mycf->upstream.store_access = 0600;
    
//实际上buffering已经决定了将以固定大小的内存作为缓冲区来转发上游的
//响应包体，这块固定缓冲区的大小就是buffer_size。如果buffering为1
//就会使用更多的内存缓存来不及发往下游的响应，例如最多使用bufs.num个
//缓冲区、每个缓冲区大小为bufs.size，另外还会使用临时文件，临时文件的
//最大长度为max_temp_file_size
    mycf->upstream.buffering = 0;
    mycf->upstream.bufs.num = 8;
    mycf->upstream.bufs.size = ngx_pagesize;
    mycf->upstream.buffer_size = ngx_pagesize;
    mycf->upstream.busy_buffers_size = 2 * ngx_pagesize;
    mycf->upstream.temp_file_write_size = 2 * ngx_pagesize;
    mycf->upstream.max_temp_file_size = 1024 * 1024 * 1024;

//upstream模块要求hide_headers成员必须要初始化（upstream在解析完上游服务器返回的包头时，会调用ngx_http_upstream_process_headers
//方法按照hide_headers成员将本应转发给下游的一些http头部隐藏），这里将它赋为NGX_CONF_UNSET_PTR ，是为了在merge合并配置项方法中使用
//upstream模块提供的ngx_http_upstream_hide_headers_hash方法初始化hide_headers 成员
    mycf->upstream.hide_headers = NGX_CONF_UNSET_PTR;
    mycf->upstream.pass_headers = NGX_CONF_UNSET_PTR;

    return mycf;
}


static char *ngx_http_mytest_upstream_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_mytest_upstream_conf_t *prev = (ngx_http_mytest_upstream_conf_t *)parent;
    ngx_http_mytest_upstream_conf_t *conf = (ngx_http_mytest_upstream_conf_t *)child;

    ngx_hash_init_t             hash;
    hash.max_size = 100;
    hash.bucket_size = 1024;
    hash.name = "proxy_headers_hash";
    if (ngx_http_upstream_hide_headers_hash(cf, &conf->upstream,
                                            &prev->upstream, ngx_http_proxy_hide_headers, &hash)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

/* 
这里定义的mytest_upstream_create_request方法用于创建发送给上游服务器的HTTP请求，upstream模块将会回调它 
*/
static ngx_int_t
mytest_upstream_create_request(ngx_http_request_t *r)
{
    //发往google上游服务器的请求很简单，就是模仿正常的搜索请求，
//以/search?q=…的URL来发起搜索请求。backendQueryLine中的%V等转化
    static ngx_str_t backendQueryLine =
        ngx_string("GET /search?q=%V HTTP/1.1\r\nHost: www.sina.com\r\nConnection: close\r\n\r\n");
    ngx_int_t queryLineLen = backendQueryLine.len + r->args.len - 2;
    //必须由内存池中申请内存，这有两点好处：在网络情况不佳的情况下，向上游
//服务器发送请求时，可能需要epoll多次调度send发送才能完成，
//这时必须保证这段内存不会被释放；请求结束时，这段内存会被自动释放，
//降低内存泄漏的可能
   
    ngx_buf_t* b = ngx_create_temp_buf(r->pool, queryLineLen);
    if (b == NULL)
        return NGX_ERROR;
    //last要指向请求的末尾
    b->last = b->pos + queryLineLen;

    ngx_snprintf(b->pos, queryLineLen, (char*)backendQueryLine.data, &r->args); 
              
    // r->upstream->request_bufs是一个ngx_chain_t结构，它包含着要发送给上游服务器的请求
    r->upstream->request_bufs = ngx_alloc_chain_link(r->pool);
    if (r->upstream->request_bufs == NULL)
        return NGX_ERROR;

    // request_bufs这里只包含1个ngx_buf_t缓冲区
    r->upstream->request_bufs->buf = b;
    r->upstream->request_bufs->next = NULL;

    r->upstream->request_sent = 0;
    r->upstream->header_sent = 0;
    // header_hash不可以为0
    r->header_hash = 1;
     
    return NGX_OK;
}

/*
process_header负责解析上游服务器发来的基于TCP的包头，在本例中，就是解析
HTTP响应行和HTTP头部，因此，这里使用mytest_process_status line方法解析HTTP响
应行，使用mytest_upstream_proces s_header方法解析http响应头部。之所以使用两个方法
解析包头，这也是HTTP的复杂性造成的，因为无论是响应行还是响应头部都是不定长的，
都需要使用状态机来解析。实际上，这两个方法也是通用的，它们适用于解析所有的HTTP
响应包，而且这两个方法的代码与ngx_http_proxy_module模块的实现几乎是完全一致的。
*/
static ngx_int_t
mytest_process_status_line(ngx_http_request_t *r)
{
    size_t                 len;
    ngx_int_t              rc;
    ngx_http_upstream_t   *u;

    //上下文中才会保存多次解析http响应行的状态，首先取出请求的上下文
    ngx_http_mytest_upstream_ctx_t* ctx = ngx_http_get_module_ctx(r, ngx_http_mytest_upstream_module);
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    u = r->upstream;
    
    ngx_log_debugall(r->connection->log, 0,
                       "%*s", (size_t) (u->buffer.last - u->buffer.pos), u->buffer.pos);
    //http框架提供的ngx_http_parse_status_line方法可以解析http响应行，它的输入就是收到的字符流和上下文中的ngx_http_status_t结构
    rc = ngx_http_parse_status_line(r, &u->buffer, &ctx->status);

    //返回NGX_AGAIN表示还没有解析出完整的http响应行，需要接收更多的字符流再来解析
    if (rc == NGX_AGAIN)
    {
        return rc;
    }
    //返回NGX_ERROR则没有接收到合法的http响应行
    if (rc == NGX_ERROR)
    {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent no valid HTTP/1.0 header");

        r->http_version = NGX_HTTP_VERSION_9;
        u->state->status = NGX_HTTP_OK;

        return NGX_OK;
    }

/* 
以下表示解析到完整的http响应行，这时会做一些简单的赋值操作，将解析出的信息设置到r->upstream->headers_in结构体中，upstream解析完所
有的包头时，就会把headers_in中的成员设置到将要向下游发送的r->headers_out结构体中，也就是说，现在我们向headers_in中设置的信息，
最终都会发往下游客户端。为什么不是直接设置r->headers_out而要这样多此一举呢？这是因为upstream希望能够按照ngx_http_upstream_conf_t配
置结构体中的hide_headers等成员对发往下游的响应头部做统一处理 
*/
    if (u->state)
    {
        u->state->status = ctx->status.code;
    }

    u->headers_in.status_n = ctx->status.code;

    len = ctx->status.end - ctx->status.start;
    u->headers_in.status_line.len = len;

    u->headers_in.status_line.data = ngx_pnalloc(r->pool, len);
    if (u->headers_in.status_line.data == NULL)
    {
        return NGX_ERROR;
    }

    ngx_memcpy(u->headers_in.status_line.data, ctx->status.start, len);

    //下一步将开始解析http头部，设置process_header回调方法为
//mytest_upstream_process_header，
//之后再收到的新字符流将由mytest_upstream_process_header解析
    u->process_header = mytest_upstream_process_header;

    //如果本次收到的字符流除了http响应行外，还有多余的字符，
//将由mytest_upstream_process_header方法解析
    return mytest_upstream_process_header(r);
}

//ngx_http_parse_status_line解析应答行，mytest_upstream_process_header解析头部行中的其中一行
static ngx_int_t
mytest_upstream_process_header(ngx_http_request_t *r)
{
    ngx_int_t                       rc;
    ngx_table_elt_t                *h;
    ngx_http_upstream_header_t     *hh;
    ngx_http_upstream_main_conf_t  *umcf;

    //这里将upstream模块配置项ngx_http_upstream_main_conf_t取了
    //出来，目的只有1个，对将要转发给下游客户端的http响应头部作统一
    //处理。该结构体中存储了需要做统一处理的http头部名称和回调方法
    umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);

    //循环的解析所有的http头部
    for ( ;; ) {
        // http框架提供了基础性的ngx_http_parse_header_line方法，它用于解析http头部
        rc = ngx_http_parse_header_line(r, &r->upstream->buffer, 1);
        //返回NGX_OK表示解析出一行http头部
        if (rc == NGX_OK)
        {
            //向headers_in.headers这个ngx_list_t链表中添加http头部
            h = ngx_list_push(&r->upstream->headers_in.headers);
            if (h == NULL)
            {
                return NGX_ERROR;
            }
            //以下开始构造刚刚添加到headers链表中的http头部
            h->hash = r->header_hash;

            h->key.len = r->header_name_end - r->header_name_start;
            h->value.len = r->header_end - r->header_start;
            
            //必须由内存池中分配解析出的这一行信息，存放解析出的这一行http头部的内存
            h->key.data = ngx_pnalloc(r->pool,
                                      h->key.len + 1 + h->value.len + 1 + h->key.len);
            if (h->key.data == NULL)
            {
                return NGX_ERROR;
            }

            /* key + value + 小写key */
            h->value.data = h->key.data + h->key.len + 1; //value存放在key后面
            h->lowcase_key = h->key.data + h->key.len + 1 + h->value.len + 1; //最后存放key的小写字符串

            ngx_memcpy(h->key.data, r->header_name_start, h->key.len);
            h->key.data[h->key.len] = '\0';
            ngx_memcpy(h->value.data, r->header_start, h->value.len);
            h->value.data[h->value.len] = '\0';

            if (h->key.len == r->lowcase_index)
            {
                ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);
            }
            else
            {
                ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
            }

            //upstream模块会对一些http头部做特殊处理
            hh = ngx_hash_find(&umcf->headers_in_hash, h->hash,
                               h->lowcase_key, h->key.len);

            if (hh && hh->handler(r, h, hh->offset) != NGX_OK)
            {
                return NGX_ERROR;
            }

            continue;
        }

        //返回NGX_HTTP_PARSE_HEADER_DONE表示响应中所有的http头部都解析
//完毕，接下来再接收到的都将是http包体
        if (rc == NGX_HTTP_PARSE_HEADER_DONE)
        {
            //如果之前解析http头部时没有发现server和date头部，以下会
            //根据http协议添加这两个头部
            if (r->upstream->headers_in.server == NULL)
            {
                h = ngx_list_push(&r->upstream->headers_in.headers);
                if (h == NULL)
                {
                    return NGX_ERROR;
                }

                h->hash = ngx_hash(ngx_hash(ngx_hash(ngx_hash(
                                                         ngx_hash('s', 'e'), 'r'), 'v'), 'e'), 'r');

                ngx_str_set(&h->key, "Server");
                ngx_str_null(&h->value);
                h->lowcase_key = (u_char *) "server";
            }

            if (r->upstream->headers_in.date == NULL)
            {
                h = ngx_list_push(&r->upstream->headers_in.headers);
                if (h == NULL)
                {
                    return NGX_ERROR;
                }

                h->hash = ngx_hash(ngx_hash(ngx_hash('d', 'a'), 't'), 'e');

                ngx_str_set(&h->key, "Date");
                ngx_str_null(&h->value);
                h->lowcase_key = (u_char *) "date";
            }

            return NGX_OK;
        }

        //如果返回NGX_AGAIN则表示状态机还没有解析到完整的http头部，
//要求upstream模块继续接收新的字符流再交由process_header
//回调方法解析
        if (rc == NGX_AGAIN)
        {
            return NGX_AGAIN;
        }

        //其他返回值都是非法的
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent invalid header");

        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }
}

static void
mytest_upstream_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                  "mytest_upstream_finalize_request");
}


static char *
ngx_http_mytest_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;
    
    //首先找到mytest配置项所属的配置块，clcf貌似是location块内的数据
//结构，其实不然，它可以是main、srv或者loc级别配置项，也就是说在每个
//http{}和server{}内也都有一个ngx_http_core_loc_conf_t结构体
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    //http框架在处理用户请求进行到NGX_HTTP_CONTENT_PHASE阶段时，如果
//请求的主机域名、URI与mytest配置项所在的配置块相匹配，就将调用我们
//实现的ngx_http_mytest_handler方法处理这个请求
    clcf->handler = ngx_http_mytest_upstream_handler;

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_mytest_upstream_handler(ngx_http_request_t *r)
{
    
    //首先建立http上下文结构体ngx_http_mytest_ctx_t
    ngx_http_mytest_upstream_ctx_t* myctx = ngx_http_get_module_ctx(r, ngx_http_mytest_upstream_module);
    if (myctx == NULL)
    {
        myctx = ngx_palloc(r->pool, sizeof(ngx_http_mytest_upstream_ctx_t));
        if (myctx == NULL)
        {
            return NGX_ERROR;
        }
        
        //将新建的上下文与请求关联起来
        ngx_http_set_ctx(r, myctx, ngx_http_mytest_upstream_module);
    }
    
    //对每1个要使用upstream的请求，必须调用且只能调用1次
    //ngx_http_upstream_create方法，它会初始化r->upstream成员
    if (ngx_http_upstream_create(r) != NGX_OK)
    {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngx_http_upstream_create() failed");
        return NGX_ERROR;
    }

    //得到配置结构体ngx_http_mytest_conf_t
    ngx_http_mytest_upstream_conf_t  *mycf = (ngx_http_mytest_upstream_conf_t  *) ngx_http_get_module_loc_conf(r, ngx_http_mytest_upstream_module);
    ngx_http_upstream_t *u = r->upstream;
    //这里用配置文件中的结构体来赋给r->upstream->conf成员
    u->conf = &mycf->upstream;//把我们设置好的upstream配置信息赋值给ngx_http_request_t->upstream->conf
    //决定转发包体时使用的缓冲区
    u->buffering = mycf->upstream.buffering;

    //以下代码开始初始化resolved结构体，用来保存上游服务器的地址
    u->resolved = (ngx_http_upstream_resolved_t*) ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_resolved_t));
    if (u->resolved == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngx_pcalloc resolved error. %s.", strerror(errno));
        return NGX_ERROR;
    }

    //这里的上游服务器就是www.google.com
    static struct sockaddr_in backendSockAddr;
    struct hostent *pHost = gethostbyname((char*) "www.sina.com");
    if (pHost == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "gethostbyname fail. %s", strerror(errno));
        
        ngx_log_debugall(r->connection->log, 0, "yang test ############################MYTEST upstream gethostbyname error\n");
        return NGX_ERROR;
    }
    
    //访问上游服务器的80端口
    backendSockAddr.sin_family = AF_INET;
    backendSockAddr.sin_port = htons((in_port_t) 80);
    char* pDmsIP = inet_ntoa(*(struct in_addr*) (pHost->h_addr_list[0]));
    //char* pDmsIP = inet_ntoa(*(struct in_addr*) ("10.10.0.2"));
    backendSockAddr.sin_addr.s_addr = inet_addr(pDmsIP);
    myctx->backendServer.data = (u_char*)pDmsIP;
    myctx->backendServer.len = strlen(pDmsIP);
    ngx_log_debugall(r->connection->log, 0, "yang test ############################MYTEST upstream gethostbyname OK, addr:%s\n", pDmsIP);

    //将地址设置到resolved成员中
    u->resolved->sockaddr = (struct sockaddr *)&backendSockAddr;
    u->resolved->socklen = sizeof(struct sockaddr_in);
    u->resolved->naddrs = 1;

    u->create_request = mytest_upstream_create_request; //构造http请求行和头部行
    u->process_header = mytest_process_status_line;
    u->finalize_request = mytest_upstream_finalize_request;

/*
这里还需要执行r->main->count++，这是在告诉HTTP框架将当前请求的引用计数加1，即告诉ngx_http_mytest_handler方法暂时不要销
毁请求，因为HTTP框架只有在引用计数为0时才能真正地销毁请求。这样的话，upstream机制接下来才能接管请求的处理工作。
*/
    r->main->count++;
    //启动upstream
    ngx_http_upstream_init(r);
    //必须返回NGX_DONE
    return NGX_DONE; //这时要通过返回NGX DONE告诉HTTP框架暂停执行请求的下一个阶段
}

