#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/*
HTTP过滤模块的地位、作用与正常的HTTP处理模块是不同的，它所做的工作是对发送给用户的HTTP响应包做一些加工
HTTP过滤模块不会去访问第三方服务
*/

typedef struct
{
    ngx_flag_t		enable;
} ngx_http_myfilter_conf_t;

typedef struct
{
    ngx_int_t   	add_prefix;
} ngx_http_myfilter_ctx_t;

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;

//将在包体中添加这个前缀
static ngx_str_t filter_prefix = ngx_string("[my filter prefix]");

static void* ngx_http_myfilter_create_conf(ngx_conf_t *cf);
static char *
ngx_http_myfilter_merge_conf(ngx_conf_t *cf, void *parent, void *child);

static ngx_int_t ngx_http_myfilter_init(ngx_conf_t *cf);
static ngx_int_t
ngx_http_myfilter_header_filter(ngx_http_request_t *r);
static ngx_int_t
ngx_http_myfilter_body_filter(ngx_http_request_t *r, ngx_chain_t *in);

static ngx_command_t  ngx_http_myfilter_commands[] =
{
    {
        ngx_string("add_prefix"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_FLAG,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_myfilter_conf_t, enable),
        NULL
    },

    ngx_null_command
};


static ngx_http_module_t  ngx_http_myfilter_module_ctx =
{
    NULL,                                  /* preconfiguration方法  */
    ngx_http_myfilter_init,                /* postconfiguration方法 */

    NULL,                                  /*create_main_conf 方法 */
    NULL,                                  /* init_main_conf方法 */

    NULL,                                  /* create_srv_conf方法 */
    NULL,                                  /* merge_srv_conf方法 */

    ngx_http_myfilter_create_conf,         /* create_loc_conf方法 */
    ngx_http_myfilter_merge_conf           /* merge_loc_conf方法 */
};


ngx_module_t  ngx_http_myfilter_module =
{
    NGX_MODULE_V1,
    &ngx_http_myfilter_module_ctx,     /* module context */
    ngx_http_myfilter_commands,        /* module directives */
    NGX_HTTP_MODULE,                /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

/*
遍历访问所有的HTTP过滤模块时，这个单链表中的元素是怎么用next指针连接起来的呢？很简单，每个HTTP过滤模块在初始化时，会先找到链表
的首元素ngx_http_top_header_filter指针和ngx_http_top_body_filter指针，再使用static静态类型的ngx_http_next_header_filter和
ngx_http_next_body_filter指针将自己插入到链表的首部，这样就行了。下面来看一下在每个过滤模块中ngx_http_next_ header_ filter和ngx_http_next- body_filter据针的
定义：
static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;
    注意，ngx_http_next_header_filter和ngx_http_next_body_filter都必须是static静态变量，为什么呢？因为static类型可以让上面两
个变量仅在当前文件中生效，这就允许所有的HTTP过滤模块都有各自的ngx_http_next_header_filter和ngx_http_next_body_filter指针。
这样，在每个HTTP过滤模块初始化时，就可以用上面这两个指针指向下一个HTTP过滤模块了。例如，可以像下列代码一样将当前HTTP过滤模块
的处理方法添加到链表首部。
ngx_http_next_header_filter = ngx_http_top_header filter;
ngx_http_top_header filter = ngx_http_myfilter_header_filter;
ngx_ht tp_next_body_f ilter = ngx_http_top_body_f ilter ;
ngx_http_top_body_f ilt er = ngx_ht tp_myf ilter_body_f ilter ;
    速样，在初始化到本模块时，自定义的ngx_http_myfilter_header_filter与ngx_http_myfilter_body_filter方法就暂时加入到了链表的首部，
而且本模块所在文件中static类型的ngx_http_next_header_filter指针和ngx_http_next_body_filter指针也指向了链表中原来的首部。在实际使用中，
如果需要调用下一个HTTP过滤模块，只需要调用ngx_http_next_header_filter(r)或者ngx_http_next_ body_filter(r，chain)就可以了。
*/

//处理流程ngx_http_send_header中执行ngx_http_myfilter_header_filter，然后在ngx_http_myfilter_header_filter中执行下一个filter，也就是
//ngx_http_next_header_filter中存储之前的filter，这样依次循环下去，那么所有的filter函数都会得到执行

//包体通过ngx_http_output_filter循环发送
static ngx_int_t ngx_http_myfilter_init(ngx_conf_t *cf)
{
    //插入到头部处理方法链表的首部
    ngx_http_next_header_filter = ngx_http_top_header_filter; //ngx_http_next_header_filter指针零时保存ngx_http_top_header_filter。
    ngx_http_top_header_filter = ngx_http_myfilter_header_filter; //在ngx_http_myfilter_header_filter中继续处理下一个filter

    //插入到包体处理方法链表的首部
    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_myfilter_body_filter;

    return NGX_OK;
}

static ngx_int_t ngx_http_myfilter_header_filter(ngx_http_request_t *r)
{
    ngx_http_myfilter_ctx_t   *ctx;
    ngx_http_myfilter_conf_t  *conf;

    //如果不是返回成功，这时是不需要理会是否加前缀的，直接交由下一个过滤模块
//处理响应码非200的情形
    if (r->headers_out.status != NGX_HTTP_OK)
    {
        return ngx_http_next_header_filter(r);
    }

//获取http上下文
    ctx = ngx_http_get_module_ctx(r, ngx_http_myfilter_module);
    if (ctx)
    {
        //该请求的上下文已经存在，这说明
// ngx_http_myfilter_header_filter已经被调用过1次，
//直接交由下一个过滤模块处理
        return ngx_http_next_header_filter(r);
    }

//获取存储配置项的ngx_http_myfilter_conf_t结构体
    conf = ngx_http_get_module_loc_conf(r, ngx_http_myfilter_module);

//如果enable成员为0，也就是配置文件中没有配置add_prefix配置项，
//或者add_prefix配置项的参数值是off，这时直接交由下一个过滤模块处理
    if (conf->enable == 0)
    {
        return ngx_http_next_header_filter(r);
    }

//构造http上下文结构体ngx_http_myfilter_ctx_t
    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_myfilter_ctx_t));
    if (ctx == NULL)
    {
        return NGX_ERROR;
    }

//add_prefix为0表示不加前缀
    ctx->add_prefix = 0;

//将构造的上下文设置到当前请求中
    ngx_http_set_ctx(r, ctx, ngx_http_myfilter_module);

//myfilter过滤模块只处理Content-Type是"text/plain"类型的http响应
    if (r->headers_out.content_type.len >= sizeof("text/plain") - 1
        && ngx_strncasecmp(r->headers_out.content_type.data, (u_char *) "text/plain", sizeof("text/plain") - 1) == 0)
    {
        //1表示需要在http包体前加入前缀
        ctx->add_prefix = 1;

//如果处理模块已经在Content-Length写入了http包体的长度，由于
//我们加入了前缀字符串，所以需要把这个字符串的长度也加入到
//Content-Length中
        if (r->headers_out.content_length_n > 0)
            r->headers_out.content_length_n += filter_prefix.len;
    }

//交由下一个过滤模块继续处理
    return ngx_http_next_header_filter(r);
}

/*
本节通过一个简单的例子来说明如何开发HTTP过滤模块。场景是这样的，用户的请求由static静态文件模块进行了处理，它会根据URI返回磁盘
中的文件给用户。而我们开发的过滤模块就会在返回给用户的响应包体前加一段字符串：”[my filter prefix]”。需要实现的功能就是这么
简单，当然，可以在配置文件中决定是否开启此功能。
*/
static ngx_int_t
ngx_http_myfilter_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_http_myfilter_ctx_t   *ctx;
    ctx = ngx_http_get_module_ctx(r, ngx_http_myfilter_module);
    
    //如果获取不到上下文，或者上下文结构体中的add_prefix为0或者2时，都不会添加前缀，这时直接交给下一个http过滤模块处理
    if (ctx == NULL || ctx->add_prefix != 1)
    {
        return ngx_http_next_body_filter(r, in);
    }

//将add_prefix设置为2，这样即使ngx_http_myfilter_body_filter
//再次回调时，也不会重复添加前缀
    ctx->add_prefix = 2;

//从请求的内存池中分配内存，用于存储字符串前缀
    ngx_buf_t* b = ngx_create_temp_buf(r->pool, filter_prefix.len);
//将ngx_buf_t中的指针正确地指向filter_prefix字符串
    b->start = b->pos = filter_prefix.data;
    b->last = b->pos + filter_prefix.len;

//从请求的内存池中生成ngx_chain_t链表，将刚分配的ngx_buf_t设置到其buf成员中，并将它添加到原先待发送的http包体前面
    ngx_chain_t *cl = ngx_alloc_chain_link(r->pool);
    cl->buf = b;
    cl->next = in;

//调用下一个模块的http包体处理方法，注意这时传入的是新生成的cl链表
    return ngx_http_next_body_filter(r, cl);
}

static void* ngx_http_myfilter_create_conf(ngx_conf_t *cf)
{
    ngx_http_myfilter_conf_t  *mycf;

    //创建存储配置项的结构体
    mycf = (ngx_http_myfilter_conf_t  *)ngx_pcalloc(cf->pool, sizeof(ngx_http_myfilter_conf_t));
    if (mycf == NULL)
    {
        return NULL;
    }

    //ngx_flat_t类型的变量，如果使用预设函数ngx_conf_set_flag_slot
//解析配置项参数，必须初始化为NGX_CONF_UNSET
    mycf->enable = NGX_CONF_UNSET;

    return mycf;
}

static char *
ngx_http_myfilter_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_myfilter_conf_t *prev = (ngx_http_myfilter_conf_t *)parent;
    ngx_http_myfilter_conf_t *conf = (ngx_http_myfilter_conf_t *)child;

//合并ngx_flat_t类型的配置项enable
    ngx_conf_merge_value(conf->enable, prev->enable, 0);

    return NGX_CONF_OK;
}

