#include <ngx_config.h>  
#include <ngx_core.h>  
#include <ngx_http.h>  

/*
在ngx_http_mytest_handler处理方法传来的ngx_http_request_t对象中就有这个请求的内存池管理对象，我们对内存池的操作都可以基于
它来进行，这样，在这个请求结束的时候，内存池分配的内存也都会被释放。
*/

/*
在ngx_http_mytest_handler的返回值中，如果是正常的HTTP返回码，Nginx就会按照规范构造合法的响应包发送给用户。
例如，假设对于PUT方法暂不支持，那么，在处理方法中发现方法名是PUT时，返回NGX_HTTP_NOT_ALLOWED，这样Nginx也就会构造类似下面的响应包给用户。
*/

/*
Nginx在调用例子中的ngx_http_mytest_handler方法时是阻塞了整个Nginx
进程的，所以ngx_http_mytest_handler或类似的处理方法中是不能有耗时很长的操作的。
*/
static ngx_int_t ngx_http_mytest_handler(ngx_http_request_t *r)  
{  
    printf("yang test:xxxxxxxxx <%s, %u>\n",  __FUNCTION__, __LINE__);
    // Only handle GET/HEAD method  ////必须是GET或者HEAD方法，否则返回405 Not Allowed
    if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD))) {  
        return NGX_HTTP_NOT_ALLOWED;  
    }  

    // Discard request body  
    ngx_int_t rc = ngx_http_discard_request_body(r);  
    //如果不想处理请求中的包体，那么可以调用ngx_http_discard_request_body方法将接收自客户端的HTTP包体丢弃掉。
    if (rc != NGX_OK) {  
        return rc;  
    }  
  
    // Send response header  
    ngx_str_t type = ngx_string("text/plain");  
    ngx_str_t response = ngx_string("Hello World!!!11111111111");  
    r->headers_out.status = NGX_HTTP_OK;  
    r->headers_out.content_length_n = response.len;  
    r->headers_out.content_type = type;  

    /*
    请求处理完毕后，需要向用户发送HTTP响应，告知客户端Nginx的执行结果。HTTP响应主要包括响应行、响应头部、包体三部分。
    发送HTTP响应时需要执行发送HTTP头部（发送HTTP头部时也会发送响应行）和发送HTTP包体两步操作。
    */
    rc = ngx_http_send_header(r);  
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {  
        return rc;  
    }  
    // Send response body  
    ngx_buf_t *b;  
    b = ngx_create_temp_buf(r->pool, response.len);  
    if (b == NULL) {  
        return NGX_HTTP_INTERNAL_SERVER_ERROR;  
    }  
    ngx_memcpy(b->pos, response.data, response.len);  
    b->last = b->pos + response.len;  
    b->last_buf = 1;  
  
    ngx_chain_t out;  
    out.buf = b;  
    out.next = NULL;  

    return ngx_http_output_filter(r, &out);  
}  
  
static char* ngx_http_mytest(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)  
{  
    ngx_http_core_loc_conf_t *clcf;  


    /*
    首先找到mytest配置项所属的配置块，clcf看上去像是location块内的数据结构，其实不然，它可以是main、srv或者loc级别配置项，也就是说，
    在每个http{}和server{}内也都有一个ngx_http_core_loc_conf_t结构体
    */
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);  

    /*HTTP框架在处理用户请求进行到NGX_HTTP_CONTENT_PHASE阶段时，如果请求的主机域名、URI与mytest配置项所在的配置块相匹配，就将调用我们实现的ngx_http_mytest_handler方法处理这个请求*/
    //该函数在ngx_http_core_content_phase中的ngx_http_finalize_request(r, r->content_handler(r));里面的r->content_handler(r)执行
    clcf->handler = ngx_http_mytest_handler;   //HTTP框架在接收完HTTP请求的头部后，会调用handler指向的方法

    return NGX_CONF_OK;  
}  
  
static ngx_command_t ngx_http_mytest_commands[] = {  
    {  
        ngx_string("mytest"),  
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS, 

/*
ngx_http_mytest是ngx_command_t结构体中的set成员（完整定义为char *(*set)(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);），
当在某个配置块中出现mytest配置项时，Nginx将会调用ngx_http_mytest方法。下面看一下如何实现ngx_http_mytest方法。
*/
        ngx_http_mytest,  
        NGX_HTTP_LOC_CONF_OFFSET,  
        0,  
        NULL  
    },  
    ngx_null_command      
};  

//暂且不管如何实现处理请求的ngx_http_mytest_handler方法，如果没有什么工作是必须在HTTP框架初始化时完成的，那就不必实现ngx_http_module_t的8个回调方法，可以像下面这样定义ngx_http_module_t接口。
static ngx_http_module_t ngx_http_mytest_module_ctx = {  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL  
};  

//定义mytest模块： 
ngx_module_t ngx_http_mytest_module = {  
    NGX_MODULE_V1,  
    &ngx_http_mytest_module_ctx,  
    ngx_http_mytest_commands,  
    NGX_HTTP_MODULE,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NULL,  
    NGX_MODULE_V1_PADDING  
};  

