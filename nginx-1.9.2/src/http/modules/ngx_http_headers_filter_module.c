
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct ngx_http_header_val_s  ngx_http_header_val_t;

typedef ngx_int_t (*ngx_http_set_header_pt)(ngx_http_request_t *r,
    ngx_http_header_val_t *hv, ngx_str_t *value);


typedef struct { //ngx_http_set_headers中会用
    ngx_str_t                  name;
    ngx_uint_t                 offset;
    ngx_http_set_header_pt     handler;
} ngx_http_set_header_t;


struct ngx_http_header_val_s { //创建空间和赋值见ngx_http_headers_add
    ngx_http_complex_value_t   value;//add_header name value中的value
    ngx_str_t                  key;//add_header name value;中的name
    //默认ngx_http_add_header  ，如果add_header name value;中的name和ngx_http_set_headers中的配对，则对应hander为ngx_http_set_headers中的handler
    ngx_http_set_header_pt     handler; 
    //如果add_header name value;中的name和ngx_http_set_headers中的配对，则对应hander为ngx_http_set_headers中的handler
    ngx_uint_t                 offset; //对应ngx_http_set_headers中的offset
    ////add_header name value always;
    ngx_uint_t                 always;  /* unsigned  always:1 */
}; 


typedef enum { //生效见ngx_http_headers_expires  ngx_http_parse_expires
    NGX_HTTP_EXPIRES_OFF,  //expire modified off
    NGX_HTTP_EXPIRES_EPOCH, //expire modified epoch
    NGX_HTTP_EXPIRES_MAX,   //expire modified max
    NGX_HTTP_EXPIRES_ACCESS,
    NGX_HTTP_EXPIRES_MODIFIED,
    NGX_HTTP_EXPIRES_DAILY,
    NGX_HTTP_EXPIRES_UNSET
} ngx_http_expires_t;

//expires xx配置存储函数为ngx_http_headers_expires，真正组包生效函数为ngx_http_set_expires
typedef struct {//真正发送给客户端的头部组装在ngx_http_headers_filter
    //expires time如果不带有变量类型则存储在expires 和 expires_time中
    ngx_http_expires_t         expires; //expires time类型，赋值为ngx_http_expires_t  ，见ngx_http_set_expires
    time_t                     expires_time; //expires time中的time，见ngx_http_set_expires

    //expires time如果带有变量类型则存储在expires_value中
    ngx_http_complex_value_t  *expires_value;

    //add_header name value;配置          真正发送给客户端的头部组装在ngx_http_headers_filter
    ngx_array_t               *headers; //见ngx_http_headers_add   成员类型ngx_http_header_val_t
} ngx_http_headers_conf_t;


static ngx_int_t ngx_http_set_expires(ngx_http_request_t *r,
    ngx_http_headers_conf_t *conf);
static ngx_int_t ngx_http_parse_expires(ngx_str_t *value,
    ngx_http_expires_t *expires, time_t *expires_time, char **err);
static ngx_int_t ngx_http_add_cache_control(ngx_http_request_t *r,
    ngx_http_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_add_header(ngx_http_request_t *r,
    ngx_http_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_set_last_modified(ngx_http_request_t *r,
    ngx_http_header_val_t *hv, ngx_str_t *value);
static ngx_int_t ngx_http_set_response_header(ngx_http_request_t *r,
    ngx_http_header_val_t *hv, ngx_str_t *value);

static void *ngx_http_headers_create_conf(ngx_conf_t *cf);
static char *ngx_http_headers_merge_conf(ngx_conf_t *cf,
    void *parent, void *child);
static ngx_int_t ngx_http_headers_filter_init(ngx_conf_t *cf);
static char *ngx_http_headers_expires(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_headers_add(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

//add_header name value;中的name可以为下面这几个
static ngx_http_set_header_t  ngx_http_set_headers[] = {

    { ngx_string("Cache-Control"), 0, ngx_http_add_cache_control },
/*
nginx配置expire后会在应答头部行中添加这几个Cache-Control、Last-Modified、expire头部行,客户端再次请求后会携带If-Modified-Since(内容就是nginx之前传送的Last-Modified内容)
服务器收到这个If-Modified-Since后进行判断，看文件是否有更改(expire设置的时间)，没有则直接返回304 Not modified 参考http://www.fastcache.com.cn/html/3264072438.html
 */
    { ngx_string("Last-Modified"),
                 offsetof(ngx_http_headers_out_t, last_modified),
                 ngx_http_set_last_modified }, 

    { ngx_string("ETag"),
                 offsetof(ngx_http_headers_out_t, etag),
                 ngx_http_set_response_header },

    { ngx_null_string, 0, NULL }
};

/*
ngx_http_headers_module模块提供了两个重要的指令add_header和expires，来添加 “Expires” 和 “Cache-Control” 头字段，对响应头添
加任何域字段。add_header可以用来标示请求访问到哪台服务器上，这个也可以通过nginx模块nginx-http-footer-filter研究使用来实现。
expires指令用来对浏览器本地缓存的控制。
*/

static ngx_command_t  ngx_http_headers_filter_commands[] = {
/*
对于站点中不经常修改的静态内容（如图片，js，css），可以在服务器中设置expires过期时间，控制浏览器缓存，达到有效减小带宽流量，降低服务器压力的目的。 

以nginx服务器为例：

location ~ .*\.(gif|jpg|jpeg|png|bmp|swf)$ 
{ 
#过期时间为30天， #图片文件不怎么更新，过期可以设大一点， #如果频繁更新，则可以设置得小一点。 
expires 30d; } 

location ~ .*\.(js|css)$ 
{ expires 10d; }【背景】：expires是web服务器响应消息头字段，在响应http请求时告诉浏览器在过期时间前浏览器可以直接从浏览器缓存取数据，
而无需再次请求。 

【相关资料】1、cache-control策略cache-control与expires的作用一致，都是指明当前资源的有效期，控制浏览器是否直接从浏览器缓存取数据还是
重新发请求到服务器取数据。只不过cache-control的选择更多，设置更细致，如果同时设置的话，其优先级高于expires。 


语法: expires [modified] time;
expires epoch | max | off;
默认值: expires off;
配置段: http, server, location, if in location
在对响应代码为200，201，204，206，301，302，303，304，或307头部中是否开启对“Expires”和“Cache-Control”的增加和修改操作。
可以指定一个正或负的时间值，Expires头中的时间根据目前时间和指令中指定的时间的和来获得。

epoch表示自1970年一月一日00:00:01 GMT的绝对时间，max指定Expires的值为2037年12月31日23:59:59，Cache-Control的值为10 years。
Cache-Control头的内容随预设的时间标识指定：
·设置为负数的时间值:Cache-Control: no-cache。
·设置为正数或0的时间值：Cache-Control: max-age = #，这里#的单位为秒，在指令中指定。
参数off禁止修改应答头中的"Expires"和"Cache-Control"。

实例一：对图片，flash文件在浏览器本地缓存30天

location ~ .*\.(gif|jpg|jpeg|png|bmp|swf)$
 {
           expires 30d;
 }

实例二：对js，css文件在浏览器本地缓存1小时

location ~ .*\.(js|css)$
 {
            expires 1h;
 }

Expires是给一个资源设定一个过期时间，也就是说无需去服务端验证，直接通过浏览器自身确认是否过期即可，所以不会产生额外的流量。此种方法非常适合
不经常变动的资源。如果文件变动较频繁，不要使用Expires来缓存。

syntax:  add_header name value;
default:  —  
context:  http, server, location
 
Adds the specified field to a response header provided that the response code equals 200, 204, 206, 301, 302, 303, 304, or 307. 
    A value can contain variables. 

syntax:  expires [modified] time;
expires epoch | max | off;
 
default:  expires off;
 
context:  http, server, location
 

Enables or disables adding or modifying the “Expires” and “Cache-Control” response header fields. A parameter can be a positive 
or negative time. 

A time in the “Expires” field is computed as a sum of the current time and time specified in the directive. If the modified parameter 
is used (0.7.0, 0.6.32) then time is computed as a sum of the file’s modification time and time specified in the directive. 

In addition, it is possible to specify a time of the day using the “@” prefix (0.7.9, 0.6.34): 

expires @15h30m;


The epoch parameter corresponds to the absolute time “Thu, 01 Jan 1970 00:00:01 GMT”. The contents of the “Cache-Control” field 
depends on the sign of the specified time: 
? time is negative — “Cache-Control: no-cache”. 
? time is positive or zero — “Cache-Control: max-age=t”, where t is a time specified in the directive, in seconds. 


The max parameter sets “Expires” to the value “Thu, 31 Dec 2037 23:55:55 GMT”, and “Cache-Control” to 10 years. 
The off parameter disables adding or modifying the “Expires” and “Cache-Control” response header fields. 



*/  //该配置会修改应答头中的Cache-Control头部行，从而浏览器可以获取到该文件的缓存时间，如果在这段时间内点击浏览器再次获取该文件，
//如果浏览器支持expires，则自己判断没有过期，浏览器不会发送请求到nginx，浏览器使用本地缓存。也就是浏览器根据该时间段内直接获取本地浏览器缓存，而不是从nginx从新获取，从而提高效率 
//如果浏览器不支持，携带请求过来，我们可以直接回应304 Not Modified，表示没变动。这样浏览器就判断直接使用浏览器本地缓存

    /*
    nginx配置expire后会在应答头部行中添加这几个Cache-Control、Last-Modified、expire头部行,客户端再次请求后会携带If-Modified-Since(内容就是nginx之前传送的Last-Modified内容)
    服务器收到这个If-Modified-Since后进行判断，看文件是否有更改(expire设置的时间)，没有则直接返回304 Not modified 
    参考http://www.fastcache.com.cn/html/3264072438.html
     */
    { ngx_string("expires"), //也就是在响应中是否携带头部行:Expires: Thu, 01 Dec 2010 16:00:00 GMT等信息
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE12,
      ngx_http_headers_expires,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL},

/*
add_header指令
语法: add_header name value;
默认值: —
配置段: http, server, location, if in location
对响应代码为200，201，204，206，301，302，303，304，或307的响应报文头字段添加任意域。如：
add_header From ttlsa.com


syntax:  add_header name value;
 
default:  —  
context:  http, server, location

Adds the specified field to a response header provided that the response code equals 200, 204, 206, 301, 302, 303, 304, or 307. A value can contain variables. 
*/
    { ngx_string("add_header"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE23,
      ngx_http_headers_add,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL},

      ngx_null_command
};


static ngx_http_module_t  ngx_http_headers_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_headers_filter_init,          /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_headers_create_conf,          /* create location configuration */
    ngx_http_headers_merge_conf            /* merge location configuration */
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

/*
ngx_http_headers_module模块提供了两个重要的指令add_header和expires，来添加 “Expires” 和 “Cache-Control” 头字段，对响应头添
加任何域字段。add_header可以用来标示请求访问到哪台服务器上，这个也可以通过nginx模块nginx-http-footer-filter研究使用来实现。
expires指令用来对浏览器本地缓存的控制。
*/
ngx_module_t  ngx_http_headers_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_headers_filter_module_ctx,   /* module context */
    ngx_http_headers_filter_commands,      /* module directives */
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


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;


static ngx_int_t
ngx_http_headers_filter(ngx_http_request_t *r)
{
    ngx_str_t                 value;
    ngx_uint_t                i, safe_status;
    ngx_http_header_val_t    *h;
    ngx_http_headers_conf_t  *conf;

    conf = ngx_http_get_module_loc_conf(r, ngx_http_headers_filter_module);

    if ((conf->expires == NGX_HTTP_EXPIRES_OFF && conf->headers == NULL)
        || r != r->main) //expires off并且没有配置add_header，或者为子请求
    {
        return ngx_http_next_header_filter(r);
    }

    switch (r->headers_out.status) {

    case NGX_HTTP_OK:
    case NGX_HTTP_CREATED:
    case NGX_HTTP_NO_CONTENT:
    case NGX_HTTP_PARTIAL_CONTENT:
    case NGX_HTTP_MOVED_PERMANENTLY:
    case NGX_HTTP_MOVED_TEMPORARILY:
    case NGX_HTTP_SEE_OTHER:
    case NGX_HTTP_NOT_MODIFIED:
    case NGX_HTTP_TEMPORARY_REDIRECT:
        safe_status = 1;
        break;

    default:
        safe_status = 0;
        break;
    }

    if (conf->expires != NGX_HTTP_EXPIRES_OFF && safe_status) {
        if (ngx_http_set_expires(r, conf) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (conf->headers) {
        h = conf->headers->elts;
        for (i = 0; i < conf->headers->nelts; i++) {

            if (!safe_status && !h[i].always) {
                continue;
            }

            if (ngx_http_complex_value(r, &h[i].value, &value) != NGX_OK) {
                return NGX_ERROR;
            }

            if (h[i].handler(r, &h[i], &value) != NGX_OK) {
                return NGX_ERROR;
            }
        }
    }

    return ngx_http_next_header_filter(r);
}

//expires配置解析，会触发创建设置r->headers_out.expires r->headers_out.cache_control
static ngx_int_t //expires xx配置存储函数为ngx_http_headers_expires，真正组包生效函数为ngx_http_set_expires
ngx_http_set_expires(ngx_http_request_t *r, ngx_http_headers_conf_t *conf)
{
    char                *err;
    size_t               len;
    time_t               now, expires_time, max_age;
    ngx_str_t            value;
    ngx_int_t            rc;
    ngx_uint_t           i;
    ngx_table_elt_t     *e, *cc, **ccp;
    ngx_http_expires_t   expires;

    expires = conf->expires;
    expires_time = conf->expires_time;

    if (conf->expires_value != NULL) { //说明expires配置的是变量类型

        if (ngx_http_complex_value(r, conf->expires_value, &value) != NGX_OK) {
            return NGX_ERROR;
        }

        rc = ngx_http_parse_expires(&value, &expires, &expires_time, &err);

        if (rc != NGX_OK) {
            return NGX_OK;
        }

        if (expires == NGX_HTTP_EXPIRES_OFF) {
            return NGX_OK;
        }
    }

    e = r->headers_out.expires;

    if (e == NULL) {

        e = ngx_list_push(&r->headers_out.headers);
        if (e == NULL) {
            return NGX_ERROR;
        }

        r->headers_out.expires = e;

        e->hash = 1;
        ngx_str_set(&e->key, "Expires");
    }

    len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT");
    e->value.len = len - 1;

    ccp = r->headers_out.cache_control.elts;

    if (ccp == NULL) {

        if (ngx_array_init(&r->headers_out.cache_control, r->pool,
                           1, sizeof(ngx_table_elt_t *))
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        ccp = ngx_array_push(&r->headers_out.cache_control);
        if (ccp == NULL) {
            return NGX_ERROR;
        }

        cc = ngx_list_push(&r->headers_out.headers);
        if (cc == NULL) {
            return NGX_ERROR;
        }

        cc->hash = 1;
        ngx_str_set(&cc->key, "Cache-Control");
        *ccp = cc;

    } else {
        for (i = 1; i < r->headers_out.cache_control.nelts; i++) {
            ccp[i]->hash = 0;
        }

        cc = ccp[0];
    }

    if (expires == NGX_HTTP_EXPIRES_EPOCH) {
        e->value.data = (u_char *) "Thu, 01 Jan 1970 00:00:01 GMT";
        ngx_str_set(&cc->value, "no-cache");//设置r->headers_out.cache_control
        return NGX_OK;
    }

    if (expires == NGX_HTTP_EXPIRES_MAX) {
        e->value.data = (u_char *) "Thu, 31 Dec 2037 23:55:55 GMT";
        /* 10 years */
        ngx_str_set(&cc->value, "max-age=315360000");//设置r->headers_out.cache_control
        return NGX_OK;
    }

    e->value.data = ngx_pnalloc(r->pool, len);
    if (e->value.data == NULL) {
        return NGX_ERROR;
    }

    if (expires_time == 0 && expires != NGX_HTTP_EXPIRES_DAILY) {
        ngx_memcpy(e->value.data, ngx_cached_http_time.data,
                   ngx_cached_http_time.len + 1);
        ngx_str_set(&cc->value, "max-age=0");//设置r->headers_out.cache_control
        return NGX_OK;
    }

    now = ngx_time();

    if (expires == NGX_HTTP_EXPIRES_DAILY) {
        expires_time = ngx_next_time(expires_time);
        max_age = expires_time - now;

    } else if (expires == NGX_HTTP_EXPIRES_ACCESS
               || r->headers_out.last_modified_time == -1)
    {
        max_age = expires_time;
        expires_time += now;

    } else {
        expires_time += r->headers_out.last_modified_time;
        max_age = expires_time - now;
    }

    ngx_http_time(e->value.data, expires_time);

    if (conf->expires_time < 0 || max_age < 0) {
        ngx_str_set(&cc->value, "no-cache");//设置r->headers_out.cache_control
        return NGX_OK;
    }

    //设置r->headers_out.cache_control
    cc->value.data = ngx_pnalloc(r->pool,
                                 sizeof("max-age=") + NGX_TIME_T_LEN + 1);
    if (cc->value.data == NULL) {
        return NGX_ERROR;
    }

    cc->value.len = ngx_sprintf(cc->value.data, "max-age=%T", max_age)
                    - cc->value.data;

    return NGX_OK;
}

//获取expires配置的时间
static ngx_int_t
ngx_http_parse_expires(ngx_str_t *value, ngx_http_expires_t *expires,
    time_t *expires_time, char **err)
{
    ngx_uint_t  minus;

    if (*expires != NGX_HTTP_EXPIRES_MODIFIED) {

        if (value->len == 5 && ngx_strncmp(value->data, "epoch", 5) == 0) {
            *expires = NGX_HTTP_EXPIRES_EPOCH; //expire modified epoch
            return NGX_OK;
        }

        if (value->len == 3 && ngx_strncmp(value->data, "max", 3) == 0) {
            *expires = NGX_HTTP_EXPIRES_MAX; //expire modified max
            return NGX_OK;
        }

        if (value->len == 3 && ngx_strncmp(value->data, "off", 3) == 0) {
            *expires = NGX_HTTP_EXPIRES_OFF;  //expire modified off
            return NGX_OK;
        }
    }

    if (value->len && value->data[0] == '@') {
        value->data++;
        value->len--;
        minus = 0;

        if (*expires == NGX_HTTP_EXPIRES_MODIFIED) {
            *err = "daily time cannot be used with \"modified\" parameter";
            return NGX_ERROR;
        }

        *expires = NGX_HTTP_EXPIRES_DAILY;

    } else if (value->len && value->data[0] == '+') {
        value->data++;
        value->len--;
        minus = 0;

    } else if (value->len && value->data[0] == '-') {
        value->data++;
        value->len--;
        minus = 1;

    } else {
        minus = 0;
    }

    *expires_time = ngx_parse_time(value, 1);

    if (*expires_time == (time_t) NGX_ERROR) {
        *err = "invalid value";
        return NGX_ERROR;
    }

    if (*expires == NGX_HTTP_EXPIRES_DAILY
        && *expires_time > 24 * 60 * 60)
    {
        *err = "daily time value must be less than 24 hours";
        return NGX_ERROR;
    }

    if (minus) { //时间为负数，转为整数
        *expires_time = - *expires_time;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_add_header(ngx_http_request_t *r, ngx_http_header_val_t *hv,
    ngx_str_t *value)
{
    ngx_table_elt_t  *h;

    if (value->len) {
        h = ngx_list_push(&r->headers_out.headers);
        if (h == NULL) {
            return NGX_ERROR;
        }

        h->hash = 1;
        h->key = hv->key;
        h->value = *value;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_add_cache_control(ngx_http_request_t *r, ngx_http_header_val_t *hv,
    ngx_str_t *value)
{
    ngx_table_elt_t  *cc, **ccp;

    if (value->len == 0) {
        return NGX_OK;
    }

    ccp = r->headers_out.cache_control.elts;

    if (ccp == NULL) {

        if (ngx_array_init(&r->headers_out.cache_control, r->pool,
                           1, sizeof(ngx_table_elt_t *))
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    ccp = ngx_array_push(&r->headers_out.cache_control);
    if (ccp == NULL) {
        return NGX_ERROR;
    }

    cc = ngx_list_push(&r->headers_out.headers);
    if (cc == NULL) {
        return NGX_ERROR;
    }

    cc->hash = 1;
    ngx_str_set(&cc->key, "Cache-Control");
    cc->value = *value;

    *ccp = cc;

    return NGX_OK;
}


static ngx_int_t
ngx_http_set_last_modified(ngx_http_request_t *r, ngx_http_header_val_t *hv,
    ngx_str_t *value)
{
    if (ngx_http_set_response_header(r, hv, value) != NGX_OK) {
        return NGX_ERROR;
    }

    r->headers_out.last_modified_time =
        (value->len) ? ngx_parse_http_time(value->data, value->len) : -1;

    return NGX_OK;
}


static ngx_int_t
ngx_http_set_response_header(ngx_http_request_t *r, ngx_http_header_val_t *hv,
    ngx_str_t *value)
{
    ngx_table_elt_t  *h, **old;

    old = (ngx_table_elt_t **) ((char *) &r->headers_out + hv->offset);

    if (value->len == 0) {
        if (*old) {
            (*old)->hash = 0;
            *old = NULL;
        }

        return NGX_OK;
    }

    if (*old) {
        h = *old;

    } else {
        h = ngx_list_push(&r->headers_out.headers);
        if (h == NULL) {
            return NGX_ERROR;
        }

        *old = h;
    }

    h->hash = 1;
    h->key = hv->key;
    h->value = *value;

    return NGX_OK;
}


static void *
ngx_http_headers_create_conf(ngx_conf_t *cf)
{
    ngx_http_headers_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_headers_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->headers = NULL;
     *     conf->expires_time = 0;
     *     conf->expires_value = NULL;
     */

    conf->expires = NGX_HTTP_EXPIRES_UNSET;

    return conf;
}


static char *
ngx_http_headers_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_headers_conf_t *prev = parent;
    ngx_http_headers_conf_t *conf = child;

    if (conf->expires == NGX_HTTP_EXPIRES_UNSET) {
        conf->expires = prev->expires;
        conf->expires_time = prev->expires_time;
        conf->expires_value = prev->expires_value;

        if (conf->expires == NGX_HTTP_EXPIRES_UNSET) {
            conf->expires = NGX_HTTP_EXPIRES_OFF;
        }
    }

    if (conf->headers == NULL) {
        conf->headers = prev->headers;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_headers_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_headers_filter;

    return NGX_OK;
}


/*
expires
语法： expires [time|epoch|max|off]

默认值： expires off

作用域： http, server, location

使用本指令可以控制HTTP应答中的“Expires”和“Cache-Control”的头标，（起到控制页面缓存的作用）。

可以在time值中使用正数或负数。“Expires”头标的值将通过当前系统时间加上您设定的 time 值来获得。

epoch 指定“Expires”的值为 1 January, 1970, 00:00:01 GMT。

max 指定“Expires”的值为 31 December 2037 23:59:59 GMT，“Cache-Control”的值为10年。

-1 指定“Expires”的值为 服务器当前时间 -1s,即永远过期

“Cache-Control”头标的值由您指定的时间来决定：

负数：Cache-Control: no-cache
正数或零：Cache-Control: max-age = #, # 为您指定时间的秒数。
"off" 表示不修改“Expires”和“Cache-Control”的值
*/

/*
syntax:  add_header name value;
default:  —  
context:  http, server, location
 
Adds the specified field to a response header provided that the response code equals 200, 204, 206, 301, 302, 303, 304, or 307. 
    A value can contain variables. 

syntax:  expires [modified] time;
expires epoch | max | off;
 
default:  expires off;
 
context:  http, server, location
 

Enables or disables adding or modifying the “Expires” and “Cache-Control” response header fields. A parameter can be a positive 
or negative time. 

A time in the “Expires” field is computed as a sum of the current time and time specified in the directive. If the modified parameter 
is used (0.7.0, 0.6.32) then time is computed as a sum of the file’s modification time and time specified in the directive. 

In addition, it is possible to specify a time of the day using the “@” prefix (0.7.9, 0.6.34): 

expires @15h30m;


The epoch parameter corresponds to the absolute time “Thu, 01 Jan 1970 00:00:01 GMT”. The contents of the “Cache-Control” field 
depends on the sign of the specified time: 
? time is negative — “Cache-Control: no-cache”. 
? time is positive or zero — “Cache-Control: max-age=t”, where t is a time specified in the directive, in seconds. 


The max parameter sets “Expires” to the value “Thu, 31 Dec 2037 23:55:55 GMT”, and “Cache-Control” to 10 years. 
The off parameter disables adding or modifying the “Expires” and “Cache-Control” response header fields. 
*/
static char *
ngx_http_headers_expires(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{ //expires xx配置存储函数为ngx_http_headers_expires，真正组包生效函数为ngx_http_set_expires
    ngx_http_headers_conf_t *hcf = conf;

    char                              *err;
    ngx_str_t                         *value;
    ngx_int_t                          rc;
    ngx_uint_t                         n;
    ngx_http_complex_value_t           cv;
    ngx_http_compile_complex_value_t   ccv;

    if (hcf->expires != NGX_HTTP_EXPIRES_UNSET) { //说明之前设置过
        return "is duplicate";
    }

    value = cf->args->elts;

    if (cf->args->nelts == 2) { //expires后面直接跟了时间

        hcf->expires = NGX_HTTP_EXPIRES_ACCESS;

        n = 1;

    } else { /* cf->args->nelts == 3 */  //expires modified xxx   参考ngx_http_parse_expires
 
        if (ngx_strcmp(value[1].data, "modified") != 0) {
            return "invalid value";
        }

        hcf->expires = NGX_HTTP_EXPIRES_MODIFIED;

        n = 2;
    }

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[n];
    ccv.complex_value = &cv;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    if (cv.lengths != NULL) {

        hcf->expires_value = ngx_palloc(cf->pool,
                                        sizeof(ngx_http_complex_value_t));
        if (hcf->expires_value == NULL) {
            return NGX_CONF_ERROR;
        }

        *hcf->expires_value = cv;

        return NGX_CONF_OK;
    }

    rc = ngx_http_parse_expires(&value[n], &hcf->expires, &hcf->expires_time,
                                &err);
    if (rc != NGX_OK) {
        return err;
    }

    return NGX_CONF_OK;
}

//add_header name value;
static char *
ngx_http_headers_add(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_headers_conf_t *hcf = conf;

    ngx_str_t                         *value;
    ngx_uint_t                         i;
    ngx_http_header_val_t             *hv;
    ngx_http_set_header_t             *set;
    ngx_http_compile_complex_value_t   ccv;

    value = cf->args->elts;

    if (hcf->headers == NULL) {
        hcf->headers = ngx_array_create(cf->pool, 1,
                                        sizeof(ngx_http_header_val_t));
        if (hcf->headers == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    hv = ngx_array_push(hcf->headers);
    if (hv == NULL) {
        return NGX_CONF_ERROR;
    }

    hv->key = value[1]; //add_header name value;中的name
    hv->handler = ngx_http_add_header;
    hv->offset = 0;
    hv->always = 0;

    set = ngx_http_set_headers;
    for (i = 0; set[i].name.len; i++) {
        if (ngx_strcasecmp(value[1].data, set[i].name.data) != 0) {
            continue;
        }

        //如果add_header name value;中的name和ngx_http_set_headers中的配对，则对应offset为ngx_http_set_headers中的offset
        hv->offset = set[i].offset;
        //如果add_header name value;中的name和ngx_http_set_headers中的配对，则对应hander为ngx_http_set_headers中的handler
        hv->handler = set[i].handler;

        break;
    }

    if (value[2].len == 0) {
        ngx_memzero(&hv->value, sizeof(ngx_http_complex_value_t));
        return NGX_CONF_OK;
    }

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[2];
    ccv.complex_value = &hv->value; //把//add_header name value;中的value解析出来付给hv->value

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    if (cf->args->nelts == 3) { //add_header name value;
        return NGX_CONF_OK;
    }

    //add_header name value always;
    if (ngx_strcmp(value[3].data, "always") != 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[3]);
        return NGX_CONF_ERROR;
    }

    hv->always = 1;

    return NGX_CONF_OK;
}
