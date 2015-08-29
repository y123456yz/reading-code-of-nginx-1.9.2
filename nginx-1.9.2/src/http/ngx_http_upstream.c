
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#if (NGX_HTTP_CACHE)
static ngx_int_t ngx_http_upstream_cache(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static ngx_int_t ngx_http_upstream_cache_get(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_http_file_cache_t **cache);
static ngx_int_t ngx_http_upstream_cache_send(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static ngx_int_t ngx_http_upstream_cache_status(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_upstream_cache_last_modified(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_upstream_cache_etag(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
#endif

static void ngx_http_upstream_init_request(ngx_http_request_t *r);
static void ngx_http_upstream_resolve_handler(ngx_resolver_ctx_t *ctx);
static void ngx_http_upstream_rd_check_broken_connection(ngx_http_request_t *r);
static void ngx_http_upstream_wr_check_broken_connection(ngx_http_request_t *r);
static void ngx_http_upstream_check_broken_connection(ngx_http_request_t *r,
    ngx_event_t *ev);
static void ngx_http_upstream_connect(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static ngx_int_t ngx_http_upstream_reinit(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static void ngx_http_upstream_send_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_uint_t do_write);
static ngx_int_t ngx_http_upstream_send_request_body(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_uint_t do_write);
static void ngx_http_upstream_send_request_handler(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static void ngx_http_upstream_read_request_handler(ngx_http_request_t *r);
static void ngx_http_upstream_process_header(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static ngx_int_t ngx_http_upstream_test_next(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static ngx_int_t ngx_http_upstream_intercept_errors(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static ngx_int_t ngx_http_upstream_test_connect(ngx_connection_t *c);
static ngx_int_t ngx_http_upstream_process_headers(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static void ngx_http_upstream_process_body_in_memory(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static void ngx_http_upstream_send_response(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static void ngx_http_upstream_upgrade(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static void ngx_http_upstream_upgraded_read_downstream(ngx_http_request_t *r);
static void ngx_http_upstream_upgraded_write_downstream(ngx_http_request_t *r);
static void ngx_http_upstream_upgraded_read_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static void ngx_http_upstream_upgraded_write_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static void ngx_http_upstream_process_upgraded(ngx_http_request_t *r,
    ngx_uint_t from_upstream, ngx_uint_t do_write);
static void
    ngx_http_upstream_process_non_buffered_downstream(ngx_http_request_t *r);
static void
    ngx_http_upstream_process_non_buffered_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static void
    ngx_http_upstream_process_non_buffered_request(ngx_http_request_t *r,
    ngx_uint_t do_write);
static ngx_int_t ngx_http_upstream_non_buffered_filter_init(void *data);
static ngx_int_t ngx_http_upstream_non_buffered_filter(void *data,
    ssize_t bytes);
static void ngx_http_upstream_process_downstream(ngx_http_request_t *r);
static void ngx_http_upstream_process_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static void ngx_http_upstream_process_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static void ngx_http_upstream_store(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static void ngx_http_upstream_dummy_handler(ngx_http_request_t *r,
    ngx_http_upstream_t *u);
static void ngx_http_upstream_next(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_uint_t ft_type);
static void ngx_http_upstream_cleanup(void *data);
static void ngx_http_upstream_finalize_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_int_t rc);

static ngx_int_t ngx_http_upstream_process_header_line(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_process_content_length(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_process_last_modified(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_process_set_cookie(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t
    ngx_http_upstream_process_cache_control(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_ignore_header_line(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_process_expires(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_process_accel_expires(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_process_limit_rate(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_process_buffering(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_process_charset(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_process_connection(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t
    ngx_http_upstream_process_transfer_encoding(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_process_vary(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_copy_header_line(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t
    ngx_http_upstream_copy_multi_header_lines(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_copy_content_type(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_copy_last_modified(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_rewrite_location(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_rewrite_refresh(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_rewrite_set_cookie(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_upstream_copy_allow_ranges(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);

#if (NGX_HTTP_GZIP)
static ngx_int_t ngx_http_upstream_copy_content_encoding(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
#endif

static ngx_int_t ngx_http_upstream_add_variables(ngx_conf_t *cf);
static ngx_int_t ngx_http_upstream_addr_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_upstream_status_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_upstream_response_time_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_upstream_response_length_variable(
    ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);

static char *ngx_http_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy);
static char *ngx_http_upstream_server(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static ngx_addr_t *ngx_http_upstream_get_local(ngx_http_request_t *r,
    ngx_http_upstream_local_t *local);

static void *ngx_http_upstream_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_upstream_init_main_conf(ngx_conf_t *cf, void *conf);

#if (NGX_HTTP_SSL)
static void ngx_http_upstream_ssl_init_connection(ngx_http_request_t *,
    ngx_http_upstream_t *u, ngx_connection_t *c);
static void ngx_http_upstream_ssl_handshake(ngx_connection_t *c);
static ngx_int_t ngx_http_upstream_ssl_name(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_connection_t *c);
#endif

//通过ngx_http_upstream_init_main_conf把所有ngx_http_upstream_headers_in成员做hash运算，放入ngx_http_upstream_main_conf_t的headers_in_hash中
//这些成员最终会赋值给ngx_http_request_t->upstream->headers_in
ngx_http_upstream_header_t  ngx_http_upstream_headers_in[] = { //后端应答的头部行匹配这里面的字段后，最终由ngx_http_upstream_headers_in_t里面的成员指向

    { ngx_string("Status"),
                 ngx_http_upstream_process_header_line, //通过该handler函数把从后端服务器解析到的头部行字段赋值给ngx_http_upstream_headers_in_t->status
                 offsetof(ngx_http_upstream_headers_in_t, status),
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("Content-Type"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, content_type),
                 ngx_http_upstream_copy_content_type, 0, 1 },

    { ngx_string("Content-Length"),
                 ngx_http_upstream_process_content_length, 0,
                 ngx_http_upstream_ignore_header_line, 0, 0 },

    { ngx_string("Date"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, date),
                 ngx_http_upstream_copy_header_line,
                 offsetof(ngx_http_headers_out_t, date), 0 },

    { ngx_string("Last-Modified"),
                 ngx_http_upstream_process_last_modified, 0,
                 ngx_http_upstream_copy_last_modified, 0, 0 },

    { ngx_string("ETag"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, etag),
                 ngx_http_upstream_copy_header_line,
                 offsetof(ngx_http_headers_out_t, etag), 0 },

    { ngx_string("Server"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, server),
                 ngx_http_upstream_copy_header_line,
                 offsetof(ngx_http_headers_out_t, server), 0 },

    { ngx_string("WWW-Authenticate"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, www_authenticate),
                 ngx_http_upstream_copy_header_line, 0, 0 },

//只有在配置了location /uri {mytest;}后，HTTP框架才会在某个请求匹配了/uri后调用它处理请求
    { ngx_string("Location"),  //后端应答这个，表示需要重定向
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, location),
                 ngx_http_upstream_rewrite_location, 0, 0 },

    { ngx_string("Refresh"),
                 ngx_http_upstream_ignore_header_line, 0,
                 ngx_http_upstream_rewrite_refresh, 0, 0 },

    { ngx_string("Set-Cookie"),
                 ngx_http_upstream_process_set_cookie,
                 offsetof(ngx_http_upstream_headers_in_t, cookies),
                 ngx_http_upstream_rewrite_set_cookie, 0, 1 },

    { ngx_string("Content-Disposition"),
                 ngx_http_upstream_ignore_header_line, 0,
                 ngx_http_upstream_copy_header_line, 0, 1 },

    { ngx_string("Cache-Control"),
                 ngx_http_upstream_process_cache_control, 0,
                 ngx_http_upstream_copy_multi_header_lines,
                 offsetof(ngx_http_headers_out_t, cache_control), 1 },

    { ngx_string("Expires"),
                 ngx_http_upstream_process_expires, 0,
                 ngx_http_upstream_copy_header_line,
                 offsetof(ngx_http_headers_out_t, expires), 1 },

    { ngx_string("Accept-Ranges"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, accept_ranges),
                 ngx_http_upstream_copy_allow_ranges,
                 offsetof(ngx_http_headers_out_t, accept_ranges), 1 },

    { ngx_string("Connection"),
                 ngx_http_upstream_process_connection, 0,
                 ngx_http_upstream_ignore_header_line, 0, 0 },

    { ngx_string("Keep-Alive"),
                 ngx_http_upstream_ignore_header_line, 0,
                 ngx_http_upstream_ignore_header_line, 0, 0 },

    { ngx_string("Vary"),
                 ngx_http_upstream_process_vary, 0,
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("X-Powered-By"),
                 ngx_http_upstream_ignore_header_line, 0,
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("X-Accel-Expires"),
                 ngx_http_upstream_process_accel_expires, 0,
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("X-Accel-Redirect"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, x_accel_redirect),
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("X-Accel-Limit-Rate"),
                 ngx_http_upstream_process_limit_rate, 0,
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("X-Accel-Buffering"),
                 ngx_http_upstream_process_buffering, 0,
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("X-Accel-Charset"),
                 ngx_http_upstream_process_charset, 0,
                 ngx_http_upstream_copy_header_line, 0, 0 },

    { ngx_string("Transfer-Encoding"),
                 ngx_http_upstream_process_transfer_encoding, 0,
                 ngx_http_upstream_ignore_header_line, 0, 0 },

#if (NGX_HTTP_GZIP)
    { ngx_string("Content-Encoding"),
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, content_encoding),
                 ngx_http_upstream_copy_content_encoding, 0, 0 },
#endif

    { ngx_null_string, NULL, 0, NULL, 0, 0 }
};

/*
关于nginx upstream的几种配置方式
发表于2011 年 06 月 16 日由edwin 
平时一直依赖硬件来作load blance，最近研究Nginx来做负载设备，记录下upstream的几种配置方式。

第一种：轮询

upstream test{
    server 192.168.0.1:3000;
    server 192.168.0.1:3001;
}第二种：权重

upstream test{
    server 192.168.0.1 weight=2;
    server 192.168.0.2 weight=3;
}这种模式可解决服务器性能不等的情况下轮询比率的调配

第三种：ip_hash

upstream test{
    ip_hash;
    server 192.168.0.1;
    server 192.168.0.2;
}这种模式会根据来源IP和后端配置来做hash分配，确保固定IP只访问一个后端

第四种：fair

需要安装Upstream Fair Balancer Module

upstream test{
    server 192.168.0.1;
    server 192.168.0.2;
    fair;
}这种模式会根据后端服务的响应时间来分配，响应时间短的后端优先分配

第五种：自定义hash

需要安装Upstream Hash Module

upstream test{
    server 192.168.0.1;
    server 192.168.0.2;
    hash $request_uri;
}这种模式可以根据给定的字符串进行Hash分配

具体应用：

server{
    listen 80;
    server_name .test.com;
    charset utf-8;
    
    location / {
        proxy_pass http://test/;
    } 
}此外upstream每个后端的可设置参数为：

1.down: 表示此台server暂时不参与负载。
2.weight: 默认为1，weight越大，负载的权重就越大。
3.max_fails: 允许请求失败的次数默认为1.当超过最大次数时，返回proxy_next_upstream模块定义的错误。
4.fail_timeout: max_fails次失败后，暂停的时间。
5.backup: 其它所有的非backup机器down或者忙的时候，请求backup机器，应急措施。
*/
static ngx_command_t  ngx_http_upstream_commands[] = {
/*
语法：upstream name { ... } 
默认值：none 
使用字段：http 
这个字段设置一群服务器，可以将这个字段放在proxy_pass和fastcgi_pass指令中作为一个单独的实体，它们可以可以是监听不同端口的服务器，
并且也可以是同时监听TCP和Unix socket的服务器。
服务器可以指定不同的权重，默认为1。
示例配置

upstream backend {
  server backend1.example.com weight=5;
  server 127.0.0.1:8080       max_fails=3  fail_timeout=30s;
  server unix:/tmp/backend3;

  server backup1.example.com:8080 backup; 
}请求将按照轮询的方式分发到后端服务器，但同时也会考虑权重。
在上面的例子中如果每次发生7个请求，5个请求将被发送到backend1.example.com，其他两台将分别得到一个请求，如果有一台服务器不可用，那么
请求将被转发到下一台服务器，直到所有的服务器检查都通过。如果所有的服务器都无法通过检查，那么将返回给客户端最后一台工作的服务器产生的结果。

max_fails=number

  设置在fail_timeout参数设置的时间内最大失败次数，如果在这个时间内，所有针对该服务器的请求
  都失败了，那么认为该服务器会被认为是停机了，停机时间是fail_timeout设置的时间。默认情况下，
  不成功连接数被设置为1。被设置为零则表示不进行链接数统计。那些连接被认为是不成功的可以通过
  proxy_next_upstream, fastcgi_next_upstream，和memcached_next_upstream指令配置。http_404
  状态不会被认为是不成功的尝试。

fail_time=time
  设置 多长时间内失败次数达到最大失败次数会被认为服务器停机了服务器会被认为停机的时间长度 默认情况下，超时时间被设置为10S
*/
    { ngx_string("upstream"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_TAKE1,
      ngx_http_upstream,
      0,
      0,
      NULL },

    /*
    语法：server name [parameters];
    配置块：upstream
    server配置项指定了一台上游服务器的名字，这个名字可以是域名、IP地址端口、UNIX句柄等，在其后还可以跟下列参数。
    weight=number：设置向这台上游服务器转发的权重，默认为1。 weigth参数表示权值，权值越高被分配到的几率越大 
    max_fails=number：该选项与fail_timeout配合使用，指在fail_timeout时间段内，如果向当前的上游服务器转发失败次数超过number，则认为在当前的fail_timeout时间段内这台上游服务器不可用。max_fails默认为1，如果设置为0，则表示不检查失败次数。
    fail_timeout=time：fail_timeout表示该时间段内转发失败多少次后就认为上游服务器暂时不可用，用于优化反向代理功能。它与向上游服务器建立连接的超时时间、读取上游服务器的响应超时时间等完全无关。fail_timeout默认为10秒。
    down：表示所在的上游服务器永久下线，只在使用ip_hash配置项时才有用。
    backup：在使用ip_hash配置项时它是无效的。它表示所在的上游服务器只是备份服务器，只有在所有的非备份上游服务器都失效后，才会向所在的上游服务器转发请求。
    例如：
    upstream  backend  {
      server   backend1.example.com    weight=5;
      server   127.0.0.1:8080          max_fails=3  fail_timeout=30s;
      server   unix:/tmp/backend3;
    }

    
    语法：server name [parameters] 
    默认值：none 
    使用字段：upstream 
    指定后端服务器的名称和一些参数，可以使用域名，IP，端口，或者unix socket。如果指定为域名，则首先将其解析为IP。
    ·weight = NUMBER - 设置服务器权重，默认为1。
    ·max_fails = NUMBER - 在一定时间内（这个时间在fail_timeout参数中设置）检查这个服务器是否可用时产生的最多失败请求数，默认为1，将其设置为0可以关闭检查，这些错误在proxy_next_upstream或fastcgi_next_upstream（404错误不会使max_fails增加）中定义。
    ·fail_timeout = TIME - 在这个时间内产生了max_fails所设置大小的失败尝试连接请求后这个服务器可能不可用，同样它指定了服务器不可用的时间（在下一次尝试连接请求发起之前），默认为10秒，fail_timeout与前端响应时间没有直接关系，不过可以使用proxy_connect_timeout和proxy_read_timeout来控制。
    ·down - 标记服务器处于离线状态，通常和ip_hash一起使用。
    ·backup - (0.6.7或更高)如果所有的非备份服务器都宕机或繁忙，则使用本服务器（无法和ip_hash指令搭配使用）。
    示例配置
    
    upstream  backend  {
      server   backend1.example.com    weight=5;
      server   127.0.0.1:8080          max_fails=3  fail_timeout=30s;
      server   unix:/tmp/backend3;
    }注意：如果你只使用一台上游服务器，nginx将设置一个内置变量为1，即max_fails和fail_timeout参数不会被处理。
    结果：如果nginx不能连接到上游，请求将丢失。
    解决：使用多台上游服务器。
    */
    { ngx_string("server"),
      NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
      ngx_http_upstream_server,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_upstream_module_ctx = {
    ngx_http_upstream_add_variables,       /* preconfiguration */
    NULL,                                  /* postconfiguration */

    ngx_http_upstream_create_main_conf,    /* create main configuration */
    ngx_http_upstream_init_main_conf,      /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};

/*
Nginx不仅仅可以用做Web服务器。upstream机制其实是由ngx_http_upstream_module模块实现的，它是一个HTTP模块，使用upstream机制时客
户端的请求必须基于HTTP。

既然upstream是用于访问“上游”服务器的，那么，Nginx需要访问什么类型的“上游”服务器呢？是Apache、Tomcat这样的Web服务器，还
是memcached、cassandra这样的Key-Value存储系统，又或是mongoDB、MySQL这样的数据库？这就涉及upstream机制的范围了。基于事件驱动
架构的upstream机制所要访问的就是所有支持TCP的上游服务器。因此，既有ngx_http_proxy_module模块基于upstream机制实现了HTTP的反向
代理功能，也有类似ngx_http_memcached_module的模块基于upstream机制使得请求可以访问memcached服务器。

当nginx接收到一个连接后，读取完客户端发送出来的Header，然后就会进行各个处理过程的调用。之后就是upstream发挥作用的时候了，
upstream在客户端跟后端比如FCGI/PHP之间，接收客户端的HTTP body，发送给FCGI，然后接收FCGI的结果，发送给客户端。作为一个桥梁的作用。
同时，upstream为了充分显示其灵活性，至于后端具体是什么协议，什么系统他都不care，我只实现主体的框架，具体到FCGI协议的发送，接收，
解析，这些都交给后面的插件来处理，比如有fastcgi,memcached,proxy等插件

http://chenzhenianqing.cn/articles/category/%e5%90%84%e7%a7%8dserver/nginx
upstream和FastCGI memcached  uwsgi  scgi proxy的关系参考:http://chenzhenianqing.cn/articles/category/%e5%90%84%e7%a7%8dserver/nginx
*/
ngx_module_t  ngx_http_upstream_module = { //该模块是访问上游服务器相关模块的基础(例如 FastCGI memcached  uwsgi  scgi proxy都会用到upstream模块  ngx_http_proxy_module  ngx_http_memcached_module)
    NGX_MODULE_V1,
    &ngx_http_upstream_module_ctx,            /* module context */
    ngx_http_upstream_commands,               /* module directives */
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


static ngx_http_variable_t  ngx_http_upstream_vars[] = {

    { ngx_string("upstream_addr"), NULL,
      ngx_http_upstream_addr_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 }, //前端服务器处理请求的服务器地址

    { ngx_string("upstream_status"), NULL,
      ngx_http_upstream_status_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 }, //前端服务器的响应状态。

    { ngx_string("upstream_connect_time"), NULL,
      ngx_http_upstream_response_time_variable, 2,
      NGX_HTTP_VAR_NOCACHEABLE, 0 }, 

    { ngx_string("upstream_header_time"), NULL,
      ngx_http_upstream_response_time_variable, 1,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("upstream_response_time"), NULL,
      ngx_http_upstream_response_time_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },//前端服务器的应答时间，精确到毫秒，不同的应答以逗号和冒号分开。

    { ngx_string("upstream_response_length"), NULL,
      ngx_http_upstream_response_length_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

#if (NGX_HTTP_CACHE)

    { ngx_string("upstream_cache_status"), NULL,
      ngx_http_upstream_cache_status, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("upstream_cache_last_modified"), NULL,
      ngx_http_upstream_cache_last_modified, 0,
      NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    { ngx_string("upstream_cache_etag"), NULL,
      ngx_http_upstream_cache_etag, 0,
      NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

#endif

    { ngx_null_string, NULL, NULL, 0, 0, 0 }
};


static ngx_http_upstream_next_t  ngx_http_upstream_next_errors[] = {
    { 500, NGX_HTTP_UPSTREAM_FT_HTTP_500 },
    { 502, NGX_HTTP_UPSTREAM_FT_HTTP_502 },
    { 503, NGX_HTTP_UPSTREAM_FT_HTTP_503 },
    { 504, NGX_HTTP_UPSTREAM_FT_HTTP_504 },
    { 403, NGX_HTTP_UPSTREAM_FT_HTTP_403 },
    { 404, NGX_HTTP_UPSTREAM_FT_HTTP_404 },
    { 0, 0 }
};


ngx_conf_bitmask_t  ngx_http_upstream_cache_method_mask[] = {
   { ngx_string("GET"),  NGX_HTTP_GET},
   { ngx_string("HEAD"), NGX_HTTP_HEAD },
   { ngx_string("POST"), NGX_HTTP_POST },
   { ngx_null_string, 0 }
};


ngx_conf_bitmask_t  ngx_http_upstream_ignore_headers_masks[] = {
    { ngx_string("X-Accel-Redirect"), NGX_HTTP_UPSTREAM_IGN_XA_REDIRECT },
    { ngx_string("X-Accel-Expires"), NGX_HTTP_UPSTREAM_IGN_XA_EXPIRES },
    { ngx_string("X-Accel-Limit-Rate"), NGX_HTTP_UPSTREAM_IGN_XA_LIMIT_RATE },
    { ngx_string("X-Accel-Buffering"), NGX_HTTP_UPSTREAM_IGN_XA_BUFFERING },
    { ngx_string("X-Accel-Charset"), NGX_HTTP_UPSTREAM_IGN_XA_CHARSET },
    { ngx_string("Expires"), NGX_HTTP_UPSTREAM_IGN_EXPIRES },
    { ngx_string("Cache-Control"), NGX_HTTP_UPSTREAM_IGN_CACHE_CONTROL },
    { ngx_string("Set-Cookie"), NGX_HTTP_UPSTREAM_IGN_SET_COOKIE },
    { ngx_string("Vary"), NGX_HTTP_UPSTREAM_IGN_VARY },
    { ngx_null_string, 0 }
};


ngx_int_t
ngx_http_upstream_create(ngx_http_request_t *r)//创建一个ngx_http_upstream_t结构，放到r->upstream里面去。
{
    ngx_http_upstream_t  *u;

    u = r->upstream;

    if (u && u->cleanup) {
        r->main->count++;
        ngx_http_upstream_cleanup(r);
    }

    u = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_t));
    if (u == NULL) {
        return NGX_ERROR;
    }

    r->upstream = u;

    u->peer.log = r->connection->log;
    u->peer.log_error = NGX_ERROR_ERR;

#if (NGX_HTTP_CACHE)
    r->cache = NULL;
#endif

    u->headers_in.content_length_n = -1;
    u->headers_in.last_modified_time = -1;

    return NGX_OK;
}

/*
    1)调用ngx_http_up stream_init方法启动upstream。
    2) upstream模块会去检查文件缓存，如果缓存中已经有合适的响应包，则会直接返回缓存（当然必须是在使用反向代理文件缓存的前提下）。
    为了让读者方便地理解upstream机制，本章将不再提及文件缓存。
    3)回调mytest模块已经实现的create_request回调方法。
    4) mytest模块通过设置r->upstream->request_bufs已经决定好发送什么样的请求到上游服务器。
    5) upstream模块将会检查5.1.3节中介绍过的resolved成员，如果有resolved成员的话，就根据它设置好上游服务器的地址r->upstream->peer成员。
    6)用无阻塞的TCP套接字建立连接。
    7)无论连接是否建立成功，负责建立连接的connect方法都会立刻返回。
*/
//ngx_http_upstream_init方法将会根据ngx_http_upstream_conf_t中的成员初始化upstream，同时会开始连接上游服务器，以此展开整个upstream处理流程
void ngx_http_upstream_init(ngx_http_request_t *r) //在读取完浏览器发送来的请求头部字段后，会通过proxy fastcgi等模块读取包体，读取完后执行该函数
{
    ngx_connection_t     *c;

    c = r->connection;//得到其连接结构。

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http init upstream, client timer: %d", c->read->timer_set);

#if (NGX_HTTP_SPDY)
    if (r->spdy_stream) {
        ngx_http_upstream_init_request(r);
        return;
    }
#endif

    /*
        首先检查请求对应于客户端的连接，这个连接上的读事件如果在定时器中，也就是说，读事件的timer_ set标志位为1，那么调用ngx_del_timer
    方法把这个读事件从定时器中移除。为什么要做这件事呢？因为一旦启动upstream机制，就不应该对客户端的读操作带有超时时间的处理(超时会关闭客户端连接)，
    请求的主要触发事件将以与上游服务器的连接为主。
     */
    if (c->read->timer_set) {
        ngx_del_timer(c->read, NGX_FUNC_LINE);
    }

    if (ngx_event_flags & NGX_USE_CLEAR_EVENT) { //如果epoll使用边缘触发

/*
2025/04/24 05:31:47[             ngx_http_upstream_init,   654]  [debug] 15507#15507: *1 <   ngx_http_upstream_init,   653> epoll NGX_WRITE_EVENT(et) read add
2025/04/24 05:31:47[                ngx_epoll_add_event,  1400]  [debug] 15507#15507: *1 epoll modify read and write event: fd:11 op:3 ev:80002005
025/04/24 05:31:47[           ngx_epoll_process_events,  1624]  [debug] 15507#15507: begin to epoll_wait, epoll timer: 60000 
2025/04/24 05:31:47[           ngx_epoll_process_events,  1709]  [debug] 15507#15507: epoll: fd:11 epoll-out(ev:0004) d:B26A00E8
实际上是通过ngx_http_upstream_init中的mod epoll_ctl添加读写事件触发的，当本次循环退回到ngx_worker_process_cycle ..->ngx_epoll_process_events
的时候，就会触发epoll_out,从而执行ngx_http_upstream_wr_check_broken_connection
*/
        //这里实际上是触发执行ngx_http_upstream_check_broken_connection
        if (!c->write->active) {//要增加可写事件通知，为啥?因为待会可能就能写了,可能会转发上游服务器的内容给浏览器等客户端
            //实际上是检查和客户端的连接是否异常了
            char tmpbuf[256];
            
            snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> epoll NGX_WRITE_EVENT(et) read add", NGX_FUNC_LINE);
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, 0, tmpbuf);
            if (ngx_add_event(c->write, NGX_WRITE_EVENT, NGX_CLEAR_EVENT)
                == NGX_ERROR)
            {
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }
        }
    }

    ngx_http_upstream_init_request(r);
}

//ngx_http_upstream_init调用这里，此时客户端发送的数据都已经接收完毕了。
/*
1. 调用create_request创建fcgi或者proxy的数据结构。
2. 调用ngx_http_upstream_connect连接下游服务器。
*/
static void
ngx_http_upstream_init_request(ngx_http_request_t *r)
{
    ngx_str_t                      *host;
    ngx_uint_t                      i;
    ngx_resolver_ctx_t             *ctx, temp;
    ngx_http_cleanup_t             *cln;
    ngx_http_upstream_t            *u;
    ngx_http_core_loc_conf_t       *clcf;
    ngx_http_upstream_srv_conf_t   *uscf, **uscfp;
    ngx_http_upstream_main_conf_t  *umcf;

    if (r->aio) {
        return;
    }

    u = r->upstream;//ngx_http_upstream_create里面设置的

#if (NGX_HTTP_CACHE)

    if (u->conf->cache) {
        ngx_int_t  rc;

        rc = ngx_http_upstream_cache(r, u);

        if (rc == NGX_BUSY) {
            r->write_event_handler = ngx_http_upstream_init_request;
            return;
        }

        r->write_event_handler = ngx_http_request_empty_handler;

        if (rc == NGX_DONE) {
            return;
        }

        if (rc == NGX_ERROR) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        if (rc != NGX_DECLINED) {
            ngx_http_finalize_request(r, rc);
            return;
        }
    }

#endif

    u->store = u->conf->store;

    /*
    设置Nginx与下游客户端之间TCP连接的检查方法
    实际上，这两个方法都会通过ngx_http_upstream_check_broken_connection方法检查Nginx与下游的连接是否正常，如果出现错误，就会立即终止连接。
     */
/*
2025/04/24 05:31:47[             ngx_http_upstream_init,   654]  [debug] 15507#15507: *1 <   ngx_http_upstream_init,   653> epoll NGX_WRITE_EVENT(et) read add
2025/04/24 05:31:47[                ngx_epoll_add_event,  1400]  [debug] 15507#15507: *1 epoll modify read and write event: fd:11 op:3 ev:80002005
2025/04/24 05:31:47[           ngx_epoll_process_events,  1624]  [debug] 15507#15507: begin to epoll_wait, epoll timer: 60000 
2025/04/24 05:31:47[           ngx_epoll_process_events,  1709]  [debug] 15507#15507: epoll: fd:11 epoll-out(ev:0004) d:B26A00E8
实际上是通过ngx_http_upstream_init中的mod epoll_ctl添加读写事件触发的，当本次循环退回到ngx_worker_process_cycle ..->ngx_epoll_process_events
的时候，就会触发epoll_out,从而执行ngx_http_upstream_wr_check_broken_connection
*/
    if (!u->store && !r->post_action && !u->conf->ignore_client_abort) {
        //注意这时候的r还是客户端的连接，与上游服务器的连接r还没有建立
        r->read_event_handler = ngx_http_upstream_rd_check_broken_connection; //设置回调需要检测连接是否有问题。
        r->write_event_handler = ngx_http_upstream_wr_check_broken_connection;
    }
    

    //有接收到客户端包体，则把包体结构赋值给u->request_bufs，在后面的if (u->create_request(r) != NGX_OK) {会用到
    if (r->request_body) {//客户端发送过来的POST数据存放在此,ngx_http_read_client_request_body放的
        u->request_bufs = r->request_body->bufs; //记录客户端发送的数据，下面在create_request的时候拷贝到发送缓冲链接表里面的。
    }

    /*
     调用请求中ngx_http_upstream_t结构体里由某个HTTP模块实现的create_request方法，构造发往上游服务器的请求
     （请求中的内容是设置到request_bufs缓冲区链表中的）。如果create_request方法没有返回NGX_OK，则upstream结束

     如果是FCGI。下面组建好FCGI的各种头部，包括请求开始头，请求参数头，请求STDIN头。存放在u->request_bufs链接表里面。
	如果是Proxy模块，ngx_http_proxy_create_request组件反向代理的头部啥的,放到u->request_bufs里面
	FastCGI memcached  uwsgi  scgi proxy都会用到upstream模块
     */
    if (u->create_request(r) != NGX_OK) { //ngx_http_XXX_create_request
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    
    /* 获取ngx_http_upstream_t结构中主动连接结构peer的local本地地址信息 */
    u->peer.local = ngx_http_upstream_get_local(r, u->conf->local);

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    
    /* 初始化ngx_http_upstream_t结构中成员output向下游发送响应的方式 */
    u->output.alignment = clcf->directio_alignment; //
    u->output.pool = r->pool;
    u->output.bufs.num = 1;
    u->output.bufs.size = clcf->client_body_buffer_size;

    if (u->output.output_filter == NULL) {
        //设置过滤模块的开始过滤函数为writer。也就是output_filter。在ngx_output_chain被调用已进行数据的过滤
        u->output.output_filter = ngx_chain_writer; 
        u->output.filter_ctx = &u->writer; //参考ngx_chain_writer，里面会将输出buf一个个连接到这里。
    }

    u->writer.pool = r->pool;
    
    /* 添加用于表示上游响应的状态，例如：错误编码、包体长度等 */
    if (r->upstream_states == NULL) {//数组upstream_states，保留upstream的状态信息。

        r->upstream_states = ngx_array_create(r->pool, 1,
                                            sizeof(ngx_http_upstream_state_t));
        if (r->upstream_states == NULL) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

    } else {

        u->state = ngx_array_push(r->upstream_states);
        if (u->state == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        ngx_memzero(u->state, sizeof(ngx_http_upstream_state_t));
    }

    cln = ngx_http_cleanup_add(r, 0);//环形链表，申请一个新的元素。
    if (cln == NULL) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    cln->handler = ngx_http_upstream_cleanup; //当请求结束时，一定会调用ngx_http_upstream_cleanup方法
    cln->data = r;//指向所指的请求结构体。
    u->cleanup = &cln->handler;

    /*
    http://www.pagefault.info/?p=251
    然后就是这个函数最核心的处理部分，那就是根据upstream的类型来进行不同的操作，这里的upstream就是我们通过XXX_pass传递进来的值，
    这里的upstream有可能下面几种情况。 
    Ngx_http_fastcgi_module.c (src\http\modules):    { ngx_string("fastcgi_pass"),
    Ngx_http_memcached_module.c (src\http\modules):    { ngx_string("memcached_pass"),
    Ngx_http_proxy_module.c (src\http\modules):    { ngx_string("proxy_pass"),
    Ngx_http_scgi_module.c (src\http\modules):    { ngx_string("scgi_pass"),
    Ngx_http_uwsgi_module.c (src\http\modules):    { ngx_string("uwsgi_pass"),
    Ngx_stream_proxy_module.c (src\stream):    { ngx_string("proxy_pass"),
    1 XXX_pass中不包含变量。
    2 XXX_pass传递的值包含了一个变量($开始).这种情况也就是说upstream的url是动态变化的，因此需要每次都解析一遍.
    而第二种情况又分为2种，一种是在进入upstream之前，也就是 upstream模块的handler之中已经被resolve的地址(请看ngx_http_XXX_eval函数)，
    一种是没有被resolve，此时就需要upstream模块来进行resolve。接下来的代码就是处理这部分的东西。
    */
    if (u->resolved == NULL) {//上游的IP地址是否被解析过，ngx_http_fastcgi_handler调用ngx_http_fastcgi_eval会解析。 为NULL说明没有解析过，也就是fastcgi_pas xxx中的xxx参数没有变量
        uscf = u->conf->upstream; //upstream赋值在ngx_http_fastcgi_pass
        printf("yang test ............. resoleved NULL\n");
    } else { //fastcgi_pass xxx的xxx中有变量，说明后端服务器是会根据请求动态变化的，参考ngx_http_fastcgi_handler

#if (NGX_HTTP_SSL)
        u->ssl_name = u->resolved->host;
#endif
    //ngx_http_fastcgi_handler 会调用 ngx_http_fastcgi_eval函数，进行fastcgi_pass 后面的URL的简析，解析出unix域，或者socket.
         printf("yang test ............. resoleved NO NULL, host:%s\n", u->resolved->host.data);
        // 如果已经是ip地址格式了，就不需要再进行解析
        if (u->resolved->sockaddr) {//如果地址已经被resolve过了，我IP地址，此时创建round robin peer就行

            if (ngx_http_upstream_create_round_robin_peer(r, u->resolved)
                != NGX_OK)
            {
                ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            ngx_http_upstream_connect(r, u);//连接 

            return;
        }

        //下面开始查找域名，因为fcgi_pass后面不是ip:port，而是url；
        host = &u->resolved->host;//获取host信息。 
        // 接下来就要开始查找域名

        umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);

        uscfp = umcf->upstreams.elts;

        for (i = 0; i < umcf->upstreams.nelts; i++) {//遍历所有的上游模块，根据其host进行查找，找到host,port相同的。

            uscf = uscfp[i];//找一个IP一样的上流模块

            if (uscf->host.len == host->len
                && ((uscf->port == 0 && u->resolved->no_port)
                     || uscf->port == u->resolved->port)
                && ngx_strncasecmp(uscf->host.data, host->data, host->len) == 0)
            {
                goto found;//这个host正好相等
            }
        }

        if (u->resolved->port == 0) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "no port in upstream \"%V\"", host);
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        //没办法了，url不在upstreams数组里面，也就是不是我们配置的，那么初始化域名解析器
        temp.name = *host;
        
        // 初始化域名解析器
        ctx = ngx_resolve_start(clcf->resolver, &temp);//进行域名解析，带缓存的。申请相关的结构，返回上下文地址。
        if (ctx == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        if (ctx == NGX_NO_RESOLVER) {//无法进行域名解析。 
            // 返回NGX_NO_RESOLVER表示无法进行域名解析
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "no resolver defined to resolve %V", host);

            ngx_http_upstream_finalize_request(r, u, NGX_HTTP_BAD_GATEWAY);
            return;
        }
        
        // 设置需要解析的域名的类型与信息
        ctx->name = *host;
        ctx->handler = ngx_http_upstream_resolve_handler;//设置域名解析完成后的回调函数。
        ctx->data = r;
        ctx->timeout = clcf->resolver_timeout;

        u->resolved->ctx = ctx;

        //开始域名解析，没有完成也会返回的。
        if (ngx_resolve_name(ctx) != NGX_OK) {
            u->resolved->ctx = NULL;
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        return;
        // 域名还没有解析完成，则直接返回
    }

found:

    if (uscf == NULL) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "no upstream configuration");
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

#if (NGX_HTTP_SSL)
    u->ssl_name = uscf->host;
#endif

    if (uscf->peer.init(r, uscf) != NGX_OK) {//ngx_http_upstream_init_round_XX_peer(ngx_http_upstream_init_round_robin_peer)
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    u->peer.start_time = ngx_current_msec;

    if (u->conf->next_upstream_tries
        && u->peer.tries > u->conf->next_upstream_tries)
    {
        u->peer.tries = u->conf->next_upstream_tries;
    }

    ngx_http_upstream_connect(r, u);//调用ngx_http_upstream_connect方法向上游服务器发起连接
}


#if (NGX_HTTP_CACHE)

static ngx_int_t
ngx_http_upstream_cache(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_int_t               rc;
    ngx_http_cache_t       *c;
    ngx_http_file_cache_t  *cache;

    c = r->cache;

    if (c == NULL) {

        if (!(r->method & u->conf->cache_methods)) {
            return NGX_DECLINED;
        }

        rc = ngx_http_upstream_cache_get(r, u, &cache);

        if (rc != NGX_OK) {
            return rc;
        }

        if (r->method & NGX_HTTP_HEAD) {
            u->method = ngx_http_core_get_method;
        }

        if (ngx_http_file_cache_new(r) != NGX_OK) {
            return NGX_ERROR;
        }

        if (u->create_key(r) != NGX_OK) {
            return NGX_ERROR;
        }

        /* TODO: add keys */

        ngx_http_file_cache_create_key(r);

        if (r->cache->header_start + 256 >= u->conf->buffer_size) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "%V_buffer_size %uz is not enough for cache key, "
                          "it should be increased to at least %uz",
                          &u->conf->module, u->conf->buffer_size,
                          ngx_align(r->cache->header_start + 256, 1024));

            r->cache = NULL;
            return NGX_DECLINED;
        }

        u->cacheable = 1;

        c = r->cache;

        c->body_start = u->conf->buffer_size;
        c->min_uses = u->conf->cache_min_uses;
        c->file_cache = cache;

        switch (ngx_http_test_predicates(r, u->conf->cache_bypass)) {

        case NGX_ERROR:
            return NGX_ERROR;

        case NGX_DECLINED:
            u->cache_status = NGX_HTTP_CACHE_BYPASS;
            return NGX_DECLINED;

        default: /* NGX_OK */
            break;
        }

        c->lock = u->conf->cache_lock;
        c->lock_timeout = u->conf->cache_lock_timeout;
        c->lock_age = u->conf->cache_lock_age;

        u->cache_status = NGX_HTTP_CACHE_MISS;
    }

    rc = ngx_http_file_cache_open(r);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream cache: %i", rc);

    switch (rc) {

    case NGX_HTTP_CACHE_UPDATING:

        if (u->conf->cache_use_stale & NGX_HTTP_UPSTREAM_FT_UPDATING) {
            u->cache_status = rc;
            rc = NGX_OK;

        } else {
            rc = NGX_HTTP_CACHE_STALE;
        }

        break;

    case NGX_OK:
        u->cache_status = NGX_HTTP_CACHE_HIT;
    }

    switch (rc) {

    case NGX_OK:

        rc = ngx_http_upstream_cache_send(r, u);

        if (rc != NGX_HTTP_UPSTREAM_INVALID_HEADER) {
            return rc;
        }

        break;

    case NGX_HTTP_CACHE_STALE:

        c->valid_sec = 0;
        u->buffer.start = NULL;
        u->cache_status = NGX_HTTP_CACHE_EXPIRED;

        break;

    case NGX_DECLINED:

        if ((size_t) (u->buffer.end - u->buffer.start) < u->conf->buffer_size) {
            u->buffer.start = NULL;

        } else {
            u->buffer.pos = u->buffer.start + c->header_start;
            u->buffer.last = u->buffer.pos;
        }

        break;

    case NGX_HTTP_CACHE_SCARCE:

        u->cacheable = 0;

        break;

    case NGX_AGAIN:

        return NGX_BUSY;

    case NGX_ERROR:

        return NGX_ERROR;

    default:

        /* cached NGX_HTTP_BAD_GATEWAY, NGX_HTTP_GATEWAY_TIME_OUT, etc. */

        u->cache_status = NGX_HTTP_CACHE_HIT;

        return rc;
    }

    r->cached = 0;

    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_upstream_cache_get(ngx_http_request_t *r, ngx_http_upstream_t *u,
    ngx_http_file_cache_t **cache)
{
    ngx_str_t               *name, val;
    ngx_uint_t               i;
    ngx_http_file_cache_t  **caches;

    if (u->conf->cache_zone) {
        *cache = u->conf->cache_zone->data;
        return NGX_OK;
    }

    if (ngx_http_complex_value(r, u->conf->cache_value, &val) != NGX_OK) {
        return NGX_ERROR;
    }

    if (val.len == 0
        || (val.len == 3 && ngx_strncmp(val.data, "off", 3) == 0))
    {
        return NGX_DECLINED;
    }

    caches = u->caches->elts;

    for (i = 0; i < u->caches->nelts; i++) {
        name = &caches[i]->shm_zone->shm.name;

        if (name->len == val.len
            && ngx_strncmp(name->data, val.data, val.len) == 0)
        {
            *cache = caches[i];
            return NGX_OK;
        }
    }

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "cache \"%V\" not found", &val);

    return NGX_ERROR;
}


static ngx_int_t
ngx_http_upstream_cache_send(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_int_t          rc;
    ngx_http_cache_t  *c;

    r->cached = 1;
    c = r->cache;

    if (c->header_start == c->body_start) {
        r->http_version = NGX_HTTP_VERSION_9;
        return ngx_http_cache_send(r);
    }

    /* TODO: cache stack */

    u->buffer = *c->buf;
    u->buffer.pos += c->header_start;

    ngx_memzero(&u->headers_in, sizeof(ngx_http_upstream_headers_in_t));
    u->headers_in.content_length_n = -1;
    u->headers_in.last_modified_time = -1;

    if (ngx_list_init(&u->headers_in.headers, r->pool, 8,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    rc = u->process_header(r);

    if (rc == NGX_OK) {

        if (ngx_http_upstream_process_headers(r, u) != NGX_OK) {
            return NGX_DONE;
        }

        return ngx_http_cache_send(r);
    }

    if (rc == NGX_ERROR) {
        return NGX_ERROR;
    }

    /* rc == NGX_HTTP_UPSTREAM_INVALID_HEADER */

    /* TODO: delete file */

    return rc;
}

#endif


static void
ngx_http_upstream_resolve_handler(ngx_resolver_ctx_t *ctx)
{
    ngx_connection_t              *c;
    ngx_http_request_t            *r;
    ngx_http_upstream_t           *u;
    ngx_http_upstream_resolved_t  *ur;

    r = ctx->data;
    c = r->connection;

    u = r->upstream;
    ur = u->resolved;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream resolve: \"%V?%V\"", &r->uri, &r->args);

    if (ctx->state) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "%V could not be resolved (%i: %s)",
                      &ctx->name, ctx->state,
                      ngx_resolver_strerror(ctx->state));

        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_BAD_GATEWAY);
        goto failed;
    }

    ur->naddrs = ctx->naddrs;
    ur->addrs = ctx->addrs;

#if (NGX_DEBUG)
    {
    u_char      text[NGX_SOCKADDR_STRLEN];
    ngx_str_t   addr;
    ngx_uint_t  i;

    addr.data = text;

    for (i = 0; i < ctx->naddrs; i++) {
        addr.len = ngx_sock_ntop(ur->addrs[i].sockaddr, ur->addrs[i].socklen,
                                 text, NGX_SOCKADDR_STRLEN, 0);

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "name was resolved to %V", &addr);
    }
    }
#endif

    if (ngx_http_upstream_create_round_robin_peer(r, ur) != NGX_OK) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        goto failed;
    }

    ngx_resolve_name_done(ctx);
    ur->ctx = NULL;

    u->peer.start_time = ngx_current_msec;

    if (u->conf->next_upstream_tries
        && u->peer.tries > u->conf->next_upstream_tries)
    {
        u->peer.tries = u->conf->next_upstream_tries;
    }

    ngx_http_upstream_connect(r, u);

failed:

    ngx_http_run_posted_requests(c);
}

//和后端服务器的读写事件触发后走到这里
static void
ngx_http_upstream_handler(ngx_event_t *ev)
{
    ngx_connection_t     *c;
    ngx_http_request_t   *r;
    ngx_http_upstream_t  *u;
    int writef = ev->write;
    
    c = ev->data;
    r = c->data; 

    u = r->upstream;
    c = r->connection;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0, "http upstream request(ev->write:%d): \"%V?%V\"", writef, &r->uri, &r->args);

    //当ev为ngx_connection_t->write 默认write为1；当ev为ngx_connection_t->read 默认write为0
    if (ev->write) { //说明是c->write事件
        u->write_event_handler(r, u);//ngx_http_upstream_send_request_handler

    } else { //说明是c->read事件
        u->read_event_handler(r, u); //ngx_http_upstream_process_header
        
    }

    ngx_http_run_posted_requests(c);
}

/*
2025/04/24 05:31:47[             ngx_http_upstream_init,   654]  [debug] 15507#15507: *1 <   ngx_http_upstream_init,   653> epoll NGX_WRITE_EVENT(et) read add
2025/04/24 05:31:47[                ngx_epoll_add_event,  1400]  [debug] 15507#15507: *1 epoll modify read and write event: fd:11 op:3 ev:80002005
025/04/24 05:31:47[           ngx_epoll_process_events,  1624]  [debug] 15507#15507: begin to epoll_wait, epoll timer: 60000 
2025/04/24 05:31:47[           ngx_epoll_process_events,  1709]  [debug] 15507#15507: epoll: fd:11 epoll-out(ev:0004) d:B26A00E8
实际上是通过ngx_http_upstream_init中的mod epoll_ctl添加读写事件触发的
*/
static void
ngx_http_upstream_rd_check_broken_connection(ngx_http_request_t *r)
{
    ngx_http_upstream_check_broken_connection(r, r->connection->read);
}

/*
2025/04/24 05:31:47[             ngx_http_upstream_init,   654]  [debug] 15507#15507: *1 <   ngx_http_upstream_init,   653> epoll NGX_WRITE_EVENT(et) read add
2025/04/24 05:31:47[                ngx_epoll_add_event,  1400]  [debug] 15507#15507: *1 epoll modify read and write event: fd:11 op:3 ev:80002005
025/04/24 05:31:47[           ngx_epoll_process_events,  1624]  [debug] 15507#15507: begin to epoll_wait, epoll timer: 60000 
2025/04/24 05:31:47[           ngx_epoll_process_events,  1709]  [debug] 15507#15507: epoll: fd:11 epoll-out(ev:0004) d:B26A00E8
实际上是通过ngx_http_upstream_init中的mod epoll_ctl添加读写事件触发的
*/
static void
ngx_http_upstream_wr_check_broken_connection(ngx_http_request_t *r)
{
    ngx_http_upstream_check_broken_connection(r, r->connection->write);
}

/*
2025/04/24 05:31:47[             ngx_http_upstream_init,   654]  [debug] 15507#15507: *1 <   ngx_http_upstream_init,   653> epoll NGX_WRITE_EVENT(et) read add
2025/04/24 05:31:47[                ngx_epoll_add_event,  1400]  [debug] 15507#15507: *1 epoll modify read and write event: fd:11 op:3 ev:80002005
025/04/24 05:31:47[           ngx_epoll_process_events,  1624]  [debug] 15507#15507: begin to epoll_wait, epoll timer: 60000 
2025/04/24 05:31:47[           ngx_epoll_process_events,  1709]  [debug] 15507#15507: epoll: fd:11 epoll-out(ev:0004) d:B26A00E8
实际上是通过ngx_http_upstream_init中的mod epoll_ctl添加读写事件触发的，当本次循环退回到ngx_worker_process_cycle ..->ngx_epoll_process_events
的时候，就会触发epoll_out,从而执行ngx_http_upstream_wr_check_broken_connection
*/
static void
ngx_http_upstream_check_broken_connection(ngx_http_request_t *r,
    ngx_event_t *ev)
{
    int                  n;
    char                 buf[1];
    ngx_err_t            err;
    ngx_int_t            event;
    ngx_connection_t     *c;
    ngx_http_upstream_t  *u;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                   "http upstream check client, write event:%d, \"%V\"",
                   ev->write, &r->uri);

    c = r->connection;
    u = r->upstream;

    if (c->error) {
        if ((ngx_event_flags & NGX_USE_LEVEL_EVENT) && ev->active) {

            event = ev->write ? NGX_WRITE_EVENT : NGX_READ_EVENT;

            if (ngx_del_event(ev, event, 0) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }
        }

        if (!u->cacheable) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_CLIENT_CLOSED_REQUEST);
        }

        return;
    }

#if (NGX_HTTP_SPDY)
    if (r->spdy_stream) {
        return;
    }
#endif

#if (NGX_HAVE_KQUEUE)

    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {

        if (!ev->pending_eof) {
            return;
        }

        ev->eof = 1;
        c->error = 1;

        if (ev->kq_errno) {
            ev->error = 1;
        }

        if (!u->cacheable && u->peer.connection) {
            ngx_log_error(NGX_LOG_INFO, ev->log, ev->kq_errno,
                          "kevent() reported that client prematurely closed "
                          "connection, so upstream connection is closed too");
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_CLIENT_CLOSED_REQUEST);
            return;
        }

        ngx_log_error(NGX_LOG_INFO, ev->log, ev->kq_errno,
                      "kevent() reported that client prematurely closed "
                      "connection");

        if (u->peer.connection == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_CLIENT_CLOSED_REQUEST);
        }

        return;
    }

#endif

#if (NGX_HAVE_EPOLLRDHUP)

    if ((ngx_event_flags & NGX_USE_EPOLL_EVENT) && ev->pending_eof) {
        socklen_t  len;

        ev->eof = 1;
        c->error = 1;

        err = 0;
        len = sizeof(ngx_err_t);

        /*
         * BSDs and Linux return 0 and set a pending error in err
         * Solaris returns -1 and sets errno
         */

        if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len)
            == -1)
        {
            err = ngx_socket_errno;
        }

        if (err) {
            ev->error = 1;
        }

        if (!u->cacheable && u->peer.connection) {
            ngx_log_error(NGX_LOG_INFO, ev->log, err,
                        "epoll_wait() reported that client prematurely closed "
                        "connection, so upstream connection is closed too");
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_CLIENT_CLOSED_REQUEST);
            return;
        }

        ngx_log_error(NGX_LOG_INFO, ev->log, err,
                      "epoll_wait() reported that client prematurely closed "
                      "connection");

        if (u->peer.connection == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_CLIENT_CLOSED_REQUEST);
        }

        return;
    }

#endif

    n = recv(c->fd, buf, 1, MSG_PEEK);

    err = ngx_socket_errno;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, ev->log, err,
                   "http upstream recv() size: %d, fd:%d", n, c->fd);

    if (ev->write && (n >= 0 || err == NGX_EAGAIN)) {
        return;
    }

    if ((ngx_event_flags & NGX_USE_LEVEL_EVENT) && ev->active) {

        event = ev->write ? NGX_WRITE_EVENT : NGX_READ_EVENT;

        if (ngx_del_event(ev, event, 0) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    if (n > 0) {
        return;
    }

    if (n == -1) {
        if (err == NGX_EAGAIN) {
            return;
        }

        ev->error = 1;

    } else { /* n == 0 */
        err = 0;
    }

    ev->eof = 1;
    c->error = 1;

    if (!u->cacheable && u->peer.connection) {
        ngx_log_error(NGX_LOG_INFO, ev->log, err,
                      "client prematurely closed connection, "
                      "so upstream connection is closed too");
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_CLIENT_CLOSED_REQUEST);
        return;
    }

    ngx_log_error(NGX_LOG_INFO, ev->log, err,
                  "client prematurely closed connection");

    if (u->peer.connection == NULL) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_CLIENT_CLOSED_REQUEST);
    }
}

/*
upstream机制与上游服务器是通过TCP建立连接的，众所周知，建立TCP连接需要三次握手，而三次握手消耗的时间是不可控的。为了保证建立TCP
连接这个操作不会阻塞进程，Nginx使用无阻塞的套接字来连接上游服务器。调用的ngx_http_upstream_connect方法就是用来连接上游服务器的，
由于使用了非阻塞的套接字，当方法返回时与上游之间的TCP连接未必会成功建立，可能还需要等待上游服务器返回TCP的SYN/ACK包。因此，
ngx_http_upstream_connect方法主要负责发起建立连接这个动作，如果这个方法没有立刻返回成功，那么需要在epoll中监控这个套接字，当
它出现可写事件时，就说明连接已经建立成功了。

//调用socket,connect连接一个后端的peer,然后设置读写事件回调函数，进入发送数据的ngx_http_upstream_send_request里面
//这里负责连接后端服务，然后设置各个读写事件回调。最后如果连接建立成功，会调用ngx_http_upstream_send_request进行数据发送。
*/
static void
ngx_http_upstream_connect(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_int_t          rc;
    ngx_connection_t  *c;

    r->connection->log->action = "connecting to upstream";

    if (u->state && u->state->response_time) {
        u->state->response_time = ngx_current_msec - u->state->response_time;
    }

    u->state = ngx_array_push(r->upstream_states);
    if (u->state == NULL) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    ngx_memzero(u->state, sizeof(ngx_http_upstream_state_t));

    u->state->response_time = ngx_current_msec;
    u->state->connect_time = (ngx_msec_t) -1;
    u->state->header_time = (ngx_msec_t) -1;
    //初始赋值见ngx_http_upstream_connect->ngx_event_connect_peer(&u->peer);
    //可以看出有多少个客户端连接，nginx就要与php服务器建立多少个连接，为什么nginx和php服务器不只建立一个连接呢????????????????
    rc = ngx_event_connect_peer(&u->peer); //建立一个TCP套接字，同时，这个套接字需要设置为非阻塞模式。

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream connect: %i", rc);

    if (rc == NGX_ERROR) {//
    //若 rc = NGX_ERROR，表示发起连接失败，则调用ngx_http_upstream_finalize_request 方法关闭连接请求，并 return 从当前函数返回；
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    u->state->peer = u->peer.name;

    if (rc == NGX_BUSY) {
    //若 rc = NGX_BUSY，表示当前上游服务器处于不活跃状态，则调用 ngx_http_upstream_next 方法根据传入的参数尝试重新发起连接请求，并 return 从当前函数返回；
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "no live upstreams");
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_NOLIVE);
        return;
    }

    if (rc == NGX_DECLINED) {
    //若 rc = NGX_DECLINED，表示当前上游服务器负载过重，则调用 ngx_http_upstream_next 方法尝试与其他上游服务器建立连接，并 return 从当前函数返回；
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
        return;
    }

    /* rc == NGX_OK || rc == NGX_AGAIN || rc == NGX_DONE */

    c = u->peer.connection;

    c->data = r;
/*
设置上游连接 ngx_connection_t 结构体的读事件、写事件的回调方法 handler 都为 ngx_http_upstream_handler，设置 ngx_http_upstream_t 
结构体的写事件 write_event_handler 的回调为 ngx_http_upstream_send_request_handler，读事件 read_event_handler 的回调方法为 
ngx_http_upstream_process_header；
*/
    c->write->handler = ngx_http_upstream_handler; 
    c->read->handler = ngx_http_upstream_handler;

    //这一步骤实际上决定了向上游服务器发送请求的方法是ngx_http_upstream_send_request_handler.
//由写事件(写数据或者客户端连接返回成功)触发c->write->handler = ngx_http_upstream_handler;然后在ngx_http_upstream_handler中执行ngx_http_upstream_send_request_handler
    u->write_event_handler = ngx_http_upstream_send_request_handler; //如果ngx_event_connect_peer返回NGX_AGAIN也通过该函数触发连接成功
//设置upstream机制的read_event_handler方法为ngx_http_upstream_process_header，也就是由ngx_http_upstream_process_header方法接收上游服务器的响应。
    u->read_event_handler = ngx_http_upstream_process_header;

    c->sendfile &= r->connection->sendfile;
    u->output.sendfile = c->sendfile;

    if (c->pool == NULL) {

        /* we need separate pool here to be able to cache SSL connections */

        c->pool = ngx_create_pool(128, r->connection->log);
        if (c->pool == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    c->log = r->connection->log;
    c->pool->log = c->log;
    c->read->log = c->log;
    c->write->log = c->log;

    /* init or reinit the ngx_output_chain() and ngx_chain_writer() contexts */

    u->writer.out = NULL;
    u->writer.last = &u->writer.out;
    u->writer.connection = c;
    u->writer.limit = 0;

    if (u->request_sent) {
        if (ngx_http_upstream_reinit(r, u) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    if (r->request_body
        && r->request_body->buf
        && r->request_body->temp_file
        && r == r->main) 
    //客户端包体存入了临时文件后，则使用r->request_body->bufs链表中的ngx_buf_t结构的file_pos和file_last指向，所以r->request_body->buf可以继续读取包体
    {
        /*
         * the r->request_body->buf can be reused for one request only,
         * the subrequests should allocate their own temporary bufs
         */

        u->output.free = ngx_alloc_chain_link(r->pool);
        if (u->output.free == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        u->output.free->buf = r->request_body->buf;
        u->output.free->next = NULL;
        u->output.allocated = 1;

        r->request_body->buf->pos = r->request_body->buf->start;
        r->request_body->buf->last = r->request_body->buf->start;
        r->request_body->buf->tag = u->output.tag;
    }

    u->request_sent = 0;
    
    if (rc == NGX_AGAIN) { //这里的定时器在ngx_http_upstream_send_request会删除
            /*
            2025/04/24 02:54:29[             ngx_event_connect_peer,    32]  [debug] 14867#14867: *1 socket 12
2025/04/24 02:54:29[           ngx_epoll_add_connection,  1486]  [debug] 14867#14867: *1 epoll add connection: fd:12 ev:80002005
2025/04/24 02:54:29[             ngx_event_connect_peer,   125]  [debug] 14867#14867: *1 connect to 127.0.0.1:3666, fd:12 #2
2025/04/24 02:54:29[          ngx_http_upstream_connect,  1549]  [debug] 14867#14867: *1 http upstream connect: -2 //返回NGX_AGAIN
2025/04/24 02:54:29[                ngx_event_add_timer,    88]  [debug] 14867#14867: *1 <ngx_http_upstream_connect,  1665>  event timer add: 12: 60000:1677807811 //这里添加
2025/04/24 02:54:29[          ngx_http_finalize_request,  2526]  [debug] 14867#14867: *1 http finalize request: -4, "/test.php?" a:1, c:2
2025/04/24 02:54:29[             ngx_http_close_request,  3789]  [debug] 14867#14867: *1 http request count:2 blk:0
2025/04/24 02:54:29[           ngx_worker_process_cycle,  1110]  [debug] 14867#14867: worker(14867) cycle again
2025/04/24 02:54:29[           ngx_trylock_accept_mutex,   405]  [debug] 14867#14867: accept mutex locked
2025/04/24 02:54:29[           ngx_epoll_process_events,  1614]  [debug] 14867#14867: begin to epoll_wait, epoll timer: 60000 
2025/04/24 02:54:29[           ngx_epoll_process_events,  1699]  [debug] 14867#14867: epoll: fd:11 epoll-out(ev:0004) d:B27440E8
2025/04/24 02:54:29[           ngx_epoll_process_events,  1772]  [debug] 14867#14867: *1 post event AEB44068
2025/04/24 02:54:29[           ngx_epoll_process_events,  1699]  [debug] 14867#14867: epoll: fd:12 epoll-out(ev:0004) d:B2744158
2025/04/24 02:54:29[           ngx_epoll_process_events,  1772]  [debug] 14867#14867: *1 post event AEB44098
2025/04/24 02:54:29[      ngx_process_events_and_timers,   371]  [debug] 14867#14867: epoll_wait timer range(delta): 2
2025/04/24 02:54:29[           ngx_event_process_posted,    65]  [debug] 14867#14867: posted event AEB44068
2025/04/24 02:54:29[           ngx_event_process_posted,    67]  [debug] 14867#14867: *1 delete posted event AEB44068
2025/04/24 02:54:29[           ngx_http_request_handler,  2400]  [debug] 14867#14867: *1 http run request: "/test.php?"
2025/04/24 02:54:29[ngx_http_upstream_check_broken_connection,  1335]  [debug] 14867#14867: *1 http upstream check client, write event:1, "/test.php"
2025/04/24 02:54:29[ngx_http_upstream_check_broken_connection,  1458]  [debug] 14867#14867: *1 http upstream recv(): -1 (11: Resource temporarily unavailable)
2025/04/24 02:54:29[           ngx_event_process_posted,    65]  [debug] 14867#14867: posted event AEB44098
2025/04/24 02:54:29[           ngx_event_process_posted,    67]  [debug] 14867#14867: *1 delete posted event AEB44098
2025/04/24 02:54:29[          ngx_http_upstream_handler,  1295]  [debug] 14867#14867: *1 http upstream request: "/test.php?"
2025/04/24 02:54:29[ngx_http_upstream_send_request_handler,  2210]  [debug] 14867#14867: *1 http upstream send request handler
2025/04/24 02:54:29[     ngx_http_upstream_send_request,  2007]  [debug] 14867#14867: *1 http upstream send request
2025/04/24 02:54:29[ngx_http_upstream_send_request_body,  2095]  [debug] 14867#14867: *1 http upstream send request body
2025/04/24 02:54:29[                   ngx_chain_writer,   690]  [debug] 14867#14867: *1 chain writer buf fl:0 s:968
2025/04/24 02:54:29[                   ngx_chain_writer,   704]  [debug] 14867#14867: *1 chain writer in: 080EC838
2025/04/24 02:54:29[                         ngx_writev,   192]  [debug] 14867#14867: *1 writev: 968 of 968
2025/04/24 02:54:29[                   ngx_chain_writer,   740]  [debug] 14867#14867: *1 chain writer out: 00000000
2025/04/24 02:54:29[                ngx_event_del_timer,    39]  [debug] 14867#14867: *1 <ngx_http_upstream_send_request,  2052>  event timer del: 12: 1677807811//这里删除
2025/04/24 02:54:29[                ngx_event_add_timer,    88]  [debug] 14867#14867: *1 <ngx_http_upstream_send_request,  2075>  event timer add: 12: 60000:1677807813
           */
        /*
          若 rc = NGX_AGAIN，表示当前已经发起连接，但是没有收到上游服务器的确认应答报文，即上游连接的写事件不可写，则需调用 ngx_add_timer 
          方法将上游连接的写事件添加到定时器中，管理超时确认应答；
            
          这一步处理非阻塞的连接尚未成功建立时的动作。实际上，在ngx_event_connect_peer中，套接字已经加入到epoll中监控了，因此，
          这一步将调用ngx_add_timer方法把写事件添加到定时器中，超时时间为ngx_http_upstream_conf_t结构体中的connect_timeout
          成员，这是在设置建立TCP连接的超时时间。
          */
        ngx_add_timer(c->write, u->conf->connect_timeout, NGX_FUNC_LINE);
        return;
    }

    
//若 rc = NGX_OK，表示成功建立连接，则调用 ngx_http_upsream_send_request 方法向上游服务器发送请求；
#if (NGX_HTTP_SSL)

    if (u->ssl && c->ssl == NULL) {
        ngx_http_upstream_ssl_init_connection(r, u, c);
        return;
    }

#endif

    printf("yang test ...........c->write->active:%u\n", c->write->active);
    //如呆已经成功建立连接，则调用ngx_http_upstream_send_request方法向上游服务器发送请求
    ngx_http_upstream_send_request(r, u, 1);
}


#if (NGX_HTTP_SSL)

static void
ngx_http_upstream_ssl_init_connection(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_connection_t *c)
{
    int                        tcp_nodelay;
    ngx_int_t                  rc;
    ngx_http_core_loc_conf_t  *clcf;

    if (ngx_http_upstream_test_connect(c) != NGX_OK) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
        return;
    }

    if (ngx_ssl_create_connection(u->conf->ssl, c,
                                  NGX_SSL_BUFFER|NGX_SSL_CLIENT)
        != NGX_OK)
    {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    c->sendfile = 0;
    u->output.sendfile = 0;

    if (u->conf->ssl_server_name || u->conf->ssl_verify) {
        if (ngx_http_upstream_ssl_name(r, u, c) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
    }

    if (u->conf->ssl_session_reuse) {
        if (u->peer.set_session(&u->peer, u->peer.data) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        /* abbreviated SSL handshake may interact badly with Nagle */

        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        if (clcf->tcp_nodelay && c->tcp_nodelay == NGX_TCP_NODELAY_UNSET) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "tcp_nodelay");

            tcp_nodelay = 1;

            if (setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY,
                           (const void *) &tcp_nodelay, sizeof(int)) == -1)
            {
                ngx_connection_error(c, ngx_socket_errno,
                                     "setsockopt(TCP_NODELAY) failed");
                ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            c->tcp_nodelay = NGX_TCP_NODELAY_SET;
        }
    }

    r->connection->log->action = "SSL handshaking to upstream";

    rc = ngx_ssl_handshake(c);

    if (rc == NGX_AGAIN) {

        if (!c->write->timer_set) {
            ngx_add_timer(c->write, u->conf->connect_timeout, NGX_FUNC_LINE);
        }

        c->ssl->handler = ngx_http_upstream_ssl_handshake;
        return;
    }

    ngx_http_upstream_ssl_handshake(c);
}


static void
ngx_http_upstream_ssl_handshake(ngx_connection_t *c)
{
    long                  rc;
    ngx_http_request_t   *r;
    ngx_http_upstream_t  *u;

    r = c->data;
    u = r->upstream;

    ngx_http_set_log_request(c->log, r);

    if (c->ssl->handshaked) {

        if (u->conf->ssl_verify) {
            rc = SSL_get_verify_result(c->ssl->connection);

            if (rc != X509_V_OK) {
                ngx_log_error(NGX_LOG_ERR, c->log, 0,
                              "upstream SSL certificate verify error: (%l:%s)",
                              rc, X509_verify_cert_error_string(rc));
                goto failed;
            }

            if (ngx_ssl_check_host(c, &u->ssl_name) != NGX_OK) {
                ngx_log_error(NGX_LOG_ERR, c->log, 0,
                              "upstream SSL certificate does not match \"%V\"",
                              &u->ssl_name);
                goto failed;
            }
        }

        if (u->conf->ssl_session_reuse) {
            u->peer.save_session(&u->peer, u->peer.data);
        }

        c->write->handler = ngx_http_upstream_handler;
        c->read->handler = ngx_http_upstream_handler;

        c = r->connection;

        ngx_http_upstream_send_request(r, u, 1);

        ngx_http_run_posted_requests(c);
        return;
    }

failed:

    c = r->connection;

    ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);

    ngx_http_run_posted_requests(c);
}


static ngx_int_t
ngx_http_upstream_ssl_name(ngx_http_request_t *r, ngx_http_upstream_t *u,
    ngx_connection_t *c)
{
    u_char     *p, *last;
    ngx_str_t   name;

    if (u->conf->ssl_name) {
        if (ngx_http_complex_value(r, u->conf->ssl_name, &name) != NGX_OK) {
            return NGX_ERROR;
        }

    } else {
        name = u->ssl_name;
    }

    if (name.len == 0) {
        goto done;
    }

    /*
     * ssl name here may contain port, notably if derived from $proxy_host
     * or $http_host; we have to strip it
     */

    p = name.data;
    last = name.data + name.len;

    if (*p == '[') {
        p = ngx_strlchr(p, last, ']');

        if (p == NULL) {
            p = name.data;
        }
    }

    p = ngx_strlchr(p, last, ':');

    if (p != NULL) {
        name.len = p - name.data;
    }

    if (!u->conf->ssl_server_name) {
        goto done;
    }

#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME

    /* as per RFC 6066, literal IPv4 and IPv6 addresses are not permitted */

    if (name.len == 0 || *name.data == '[') {
        goto done;
    }

    if (ngx_inet_addr(name.data, name.len) != INADDR_NONE) {
        goto done;
    }

    /*
     * SSL_set_tlsext_host_name() needs a null-terminated string,
     * hence we explicitly null-terminate name here
     */

    p = ngx_pnalloc(r->pool, name.len + 1);
    if (p == NULL) {
        return NGX_ERROR;
    }

    (void) ngx_cpystrn(p, name.data, name.len + 1);

    name.data = p;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "upstream SSL server name: \"%s\"", name.data);

    if (SSL_set_tlsext_host_name(c->ssl->connection, name.data) == 0) {
        ngx_ssl_error(NGX_LOG_ERR, r->connection->log, 0,
                      "SSL_set_tlsext_host_name(\"%s\") failed", name.data);
        return NGX_ERROR;
    }

#endif

done:

    u->ssl_name = name;

    return NGX_OK;
}

#endif


static ngx_int_t
ngx_http_upstream_reinit(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    off_t         file_pos;
    ngx_chain_t  *cl;

    if (u->reinit_request(r) != NGX_OK) {
        return NGX_ERROR;
    }

    u->keepalive = 0;
    u->upgrade = 0;

    ngx_memzero(&u->headers_in, sizeof(ngx_http_upstream_headers_in_t));
    u->headers_in.content_length_n = -1;
    u->headers_in.last_modified_time = -1;

    if (ngx_list_init(&u->headers_in.headers, r->pool, 8,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    /* reinit the request chain */

    file_pos = 0;

    for (cl = u->request_bufs; cl; cl = cl->next) {
        cl->buf->pos = cl->buf->start;

        /* there is at most one file */

        if (cl->buf->in_file) {
            cl->buf->file_pos = file_pos;
            file_pos = cl->buf->file_last;
        }
    }

    /* reinit the subrequest's ngx_output_chain() context */

    if (r->request_body && r->request_body->temp_file
        && r != r->main && u->output.buf)
    {
        u->output.free = ngx_alloc_chain_link(r->pool);
        if (u->output.free == NULL) {
            return NGX_ERROR;
        }

        u->output.free->buf = u->output.buf;
        u->output.free->next = NULL;

        u->output.buf->pos = u->output.buf->start;
        u->output.buf->last = u->output.buf->start;
    }

    u->output.buf = NULL;
    u->output.in = NULL;
    u->output.busy = NULL;

    /* reinit u->buffer */

    u->buffer.pos = u->buffer.start;

#if (NGX_HTTP_CACHE)

    if (r->cache) {
        u->buffer.pos += r->cache->header_start;
    }

#endif

    u->buffer.last = u->buffer.pos;

    return NGX_OK;
}


static void
ngx_http_upstream_send_request(ngx_http_request_t *r, ngx_http_upstream_t *u,
    ngx_uint_t do_write) //向上游服务器发送请求
{
    ngx_int_t          rc;
    ngx_connection_t  *c;

    c = u->peer.connection; //向上游服务器的连接信息

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream send request");

    if (u->state->connect_time == (ngx_msec_t) -1) {
        u->state->connect_time = ngx_current_msec - u->state->response_time;
    }

    //通过getsockopt测试与上游服务器的tcp连接是否异常
    if (!u->request_sent && ngx_http_upstream_test_connect(c) != NGX_OK) {//测试连接失败
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);//如果测试失败，调用ngx_http_upstream_next函数，这个函数可能再次调用peer.get调用别的连接。
        return;
    }

    c->log->action = "sending request to upstream";

    rc = ngx_http_upstream_send_request_body(r, u, do_write);

    if (rc == NGX_ERROR) {
        /*  若返回值rc=NGX_ERROR，表示当前连接上出错， 将错误信息传递给ngx_http_upstream_next方法， 该方法根据错误信息决定
        是否重新向上游其他服务器发起连接； 并return从当前函数返回； */
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
        return;
    }

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        ngx_http_upstream_finalize_request(r, u, rc);
        return;
    }

    /* 
         若返回值rc = NGX_AGAIN，表示请求数据并未完全发送， 即有剩余的请求数据保存在output中，但此时，写事件已经不可写， 
         则调用ngx_add_timer方法把当前连接上的写事件添加到定时器机制， 并调用ngx_handle_write_event方法将写事件注册到epoll事件机制中； 
     */
    if (rc == NGX_AGAIN) {
        if (!c->write->ready) {
            ngx_add_timer(c->write, u->conf->send_timeout, NGX_FUNC_LINE);

        } else if (c->write->timer_set) { //例如ngx_http_upstream_send_request_body发送了三次返回NGX_AGAIN,那么第二次就需要把第一次上面的超时定时器关了，表示发送正常
            ngx_del_timer(c->write, NGX_FUNC_LINE);
        }

        //在连接后端服务器conncet前，有设置ngx_add_conn，里面已经将fd添加到了读写事件中，因此这里实际上只是简单执行下ngx_send_lowat
        if (ngx_handle_write_event(c->write, u->conf->send_lowat, NGX_FUNC_LINE) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        return;
    }

    /* rc == NGX_OK */

    if (c->write->timer_set) { //这里的定时器是ngx_http_upstream_connect中connect返回NGX_AGAIN的时候添加的定时器
        /*
            2025/04/24 02:54:29[             ngx_event_connect_peer,    32]  [debug] 14867#14867: *1 socket 12
2025/04/24 02:54:29[           ngx_epoll_add_connection,  1486]  [debug] 14867#14867: *1 epoll add connection: fd:12 ev:80002005
2025/04/24 02:54:29[             ngx_event_connect_peer,   125]  [debug] 14867#14867: *1 connect to 127.0.0.1:3666, fd:12 #2
2025/04/24 02:54:29[          ngx_http_upstream_connect,  1549]  [debug] 14867#14867: *1 http upstream connect: -2 //返回NGX_AGAIN
2025/04/24 02:54:29[                ngx_event_add_timer,    88]  [debug] 14867#14867: *1 <ngx_http_upstream_connect,  1665>  event timer add: 12: 60000:1677807811 //这里添加
2025/04/24 02:54:29[          ngx_http_finalize_request,  2526]  [debug] 14867#14867: *1 http finalize request: -4, "/test.php?" a:1, c:2
2025/04/24 02:54:29[             ngx_http_close_request,  3789]  [debug] 14867#14867: *1 http request count:2 blk:0
2025/04/24 02:54:29[           ngx_worker_process_cycle,  1110]  [debug] 14867#14867: worker(14867) cycle again
2025/04/24 02:54:29[           ngx_trylock_accept_mutex,   405]  [debug] 14867#14867: accept mutex locked
2025/04/24 02:54:29[           ngx_epoll_process_events,  1614]  [debug] 14867#14867: begin to epoll_wait, epoll timer: 60000 
2025/04/24 02:54:29[           ngx_epoll_process_events,  1699]  [debug] 14867#14867: epoll: fd:11 epoll-out(ev:0004) d:B27440E8
2025/04/24 02:54:29[           ngx_epoll_process_events,  1772]  [debug] 14867#14867: *1 post event AEB44068
2025/04/24 02:54:29[           ngx_epoll_process_events,  1699]  [debug] 14867#14867: epoll: fd:12 epoll-out(ev:0004) d:B2744158
2025/04/24 02:54:29[           ngx_epoll_process_events,  1772]  [debug] 14867#14867: *1 post event AEB44098
2025/04/24 02:54:29[      ngx_process_events_and_timers,   371]  [debug] 14867#14867: epoll_wait timer range(delta): 2
2025/04/24 02:54:29[           ngx_event_process_posted,    65]  [debug] 14867#14867: posted event AEB44068
2025/04/24 02:54:29[           ngx_event_process_posted,    67]  [debug] 14867#14867: *1 delete posted event AEB44068
2025/04/24 02:54:29[           ngx_http_request_handler,  2400]  [debug] 14867#14867: *1 http run request: "/test.php?"
2025/04/24 02:54:29[ngx_http_upstream_check_broken_connection,  1335]  [debug] 14867#14867: *1 http upstream check client, write event:1, "/test.php"
2025/04/24 02:54:29[ngx_http_upstream_check_broken_connection,  1458]  [debug] 14867#14867: *1 http upstream recv(): -1 (11: Resource temporarily unavailable)
2025/04/24 02:54:29[           ngx_event_process_posted,    65]  [debug] 14867#14867: posted event AEB44098
2025/04/24 02:54:29[           ngx_event_process_posted,    67]  [debug] 14867#14867: *1 delete posted event AEB44098
2025/04/24 02:54:29[          ngx_http_upstream_handler,  1295]  [debug] 14867#14867: *1 http upstream request: "/test.php?"
2025/04/24 02:54:29[ngx_http_upstream_send_request_handler,  2210]  [debug] 14867#14867: *1 http upstream send request handler
2025/04/24 02:54:29[     ngx_http_upstream_send_request,  2007]  [debug] 14867#14867: *1 http upstream send request
2025/04/24 02:54:29[ngx_http_upstream_send_request_body,  2095]  [debug] 14867#14867: *1 http upstream send request body
2025/04/24 02:54:29[                   ngx_chain_writer,   690]  [debug] 14867#14867: *1 chain writer buf fl:0 s:968
2025/04/24 02:54:29[                   ngx_chain_writer,   704]  [debug] 14867#14867: *1 chain writer in: 080EC838
2025/04/24 02:54:29[                         ngx_writev,   192]  [debug] 14867#14867: *1 writev: 968 of 968
2025/04/24 02:54:29[                   ngx_chain_writer,   740]  [debug] 14867#14867: *1 chain writer out: 00000000
2025/04/24 02:54:29[                ngx_event_del_timer,    39]  [debug] 14867#14867: *1 <ngx_http_upstream_send_request,  2052>  event timer del: 12: 1677807811//这里删除
2025/04/24 02:54:29[                ngx_event_add_timer,    88]  [debug] 14867#14867: *1 <ngx_http_upstream_send_request,  2075>  event timer add: 12: 60000:1677807813
           */
        ngx_del_timer(c->write, NGX_FUNC_LINE);
    }

    /* 若返回值 rc = NGX_OK，表示已经发送完全部请求数据， 准备接收来自上游服务器的响应报文，则执行以下程序；  */ 
    if (c->tcp_nopush == NGX_TCP_NOPUSH_SET) {
        if (ngx_tcp_push(c->fd) == NGX_ERROR) {
            ngx_log_error(NGX_LOG_CRIT, c->log, ngx_socket_errno,
                          ngx_tcp_push_n " failed");
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        c->tcp_nopush = NGX_TCP_NOPUSH_UNSET;
    }

    u->write_event_handler = ngx_http_upstream_dummy_handler; //数据已经在前面全部发往后端服务器了，所以不需要再做写处理

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "send out chain data to uppeer server OK");
    //在连接后端服务器conncet前，有设置ngx_add_conn，里面已经将fd添加到了读写事件中，因此这里实际上只是简单执行下ngx_send_lowat
    if (ngx_handle_write_event(c->write, 0, NGX_FUNC_LINE) != NGX_OK) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    //这回数据已经发送了，可以准备接收了，设置接收后端应答的超时定时器。
    ngx_add_timer(c->read, u->conf->read_timeout, NGX_FUNC_LINE);

    if (c->read->ready) {
        ngx_http_upstream_process_header(r, u);
        return;
    }
}

//向后端发送请求的调用过程ngx_http_upstream_send_request_body->ngx_output_chain->ngx_chain_writer
static ngx_int_t
ngx_http_upstream_send_request_body(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_uint_t do_write)
{
    int                        tcp_nodelay;
    ngx_int_t                  rc;
    ngx_chain_t               *out, *cl, *ln;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream send request body");

    if (!r->request_body_no_buffering) {

       /* buffered request body */

       if (!u->request_sent) {
           u->request_sent = 1;
           out = u->request_bufs; //如果是fastcgi这里面为实际发往后端的数据(包括fastcgi格式头部+客户端包体等)

       } else {
           out = NULL;
       }

       return ngx_output_chain(&u->output, out);
    }

    if (!u->request_sent) {
        u->request_sent = 1;
        out = u->request_bufs;

        if (r->request_body->bufs) {
            for (cl = out; cl->next; cl = out->next) { /* void */ }
            cl->next = r->request_body->bufs;
            r->request_body->bufs = NULL;
        }

        c = u->peer.connection;
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        if (clcf->tcp_nodelay && c->tcp_nodelay == NGX_TCP_NODELAY_UNSET) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "tcp_nodelay");

            tcp_nodelay = 1;

            if (setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY,
                           (const void *) &tcp_nodelay, sizeof(int)) == -1)
            {
                ngx_connection_error(c, ngx_socket_errno,
                                     "setsockopt(TCP_NODELAY) failed");
                return NGX_ERROR;
            }

            c->tcp_nodelay = NGX_TCP_NODELAY_SET;
        }

        r->read_event_handler = ngx_http_upstream_read_request_handler;

    } else {
        out = NULL;
    }

    for ( ;; ) {

        if (do_write) {
            rc = ngx_output_chain(&u->output, out);

            if (rc == NGX_ERROR) {
                return NGX_ERROR;
            }

            while (out) {
                ln = out;
                out = out->next;
                ngx_free_chain(r->pool, ln);
            }

            if (rc == NGX_OK && !r->reading_body) {
                break;
            }
        }

        if (r->reading_body) {
            /* read client request body */

            rc = ngx_http_read_unbuffered_request_body(r);

            if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
                return rc;
            }

            out = r->request_body->bufs;
            r->request_body->bufs = NULL;
        }

        /* stop if there is nothing to send */

        if (out == NULL) {
            rc = NGX_AGAIN;
            break;
        }

        do_write = 1;
    }

    if (!r->reading_body) {
        if (!u->store && !r->post_action && !u->conf->ignore_client_abort) {
            r->read_event_handler =
                                  ngx_http_upstream_rd_check_broken_connection;
        }
    }

    return rc;
}


static void
ngx_http_upstream_send_request_handler(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_connection_t  *c;

    c = u->peer.connection;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream send request handler");

    if (c->write->timedout) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_TIMEOUT);
        return;
    }

#if (NGX_HTTP_SSL)

    if (u->ssl && c->ssl == NULL) {
        ngx_http_upstream_ssl_init_connection(r, u, c);
        return;
    }

#endif

    if (u->header_sent) {
        u->write_event_handler = ngx_http_upstream_dummy_handler;

        (void) ngx_handle_write_event(c->write, 0, NGX_FUNC_LINE);

        return;
    }

    ngx_http_upstream_send_request(r, u, 1);
}


static void
ngx_http_upstream_read_request_handler(ngx_http_request_t *r)
{
    ngx_connection_t     *c;
    ngx_http_upstream_t  *u;

    c = r->connection;
    u = r->upstream;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream read request handler");

    if (c->read->timedout) {
        c->timedout = 1;
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_REQUEST_TIME_OUT);
        return;
    }

    ngx_http_upstream_send_request(r, u, 0);
}

//ngx_http_upstream_handler中执行
static void
ngx_http_upstream_process_header(ngx_http_request_t *r, ngx_http_upstream_t *u)
{//读取FCGI头部数据，或者proxy头部数据。ngx_http_upstream_send_request发送完数据后，
//会调用这里，或者有可写事件的时候会调用这里。
//ngx_http_upstream_connect函数连接fastcgi后，会设置这个回调函数为fcgi连接的可读事件回调。
    ssize_t            n;
    ngx_int_t          rc;
    ngx_connection_t  *c;

    c = u->peer.connection;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process header, fd:%u", c->fd);

    c->log->action = "reading response header from upstream";

    if (c->read->timedout) {//读超时了，轮询下一个。 ngx_event_expire_timers超时后走到这里
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_TIMEOUT);
        return;
    }

    if (!u->request_sent && ngx_http_upstream_test_connect(c) != NGX_OK) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
        return;
    }

    if (u->buffer.start == NULL) { //分配一块缓存，用来存放接受回来的数据。
        u->buffer.start = ngx_palloc(r->pool, u->conf->buffer_size);
        if (u->buffer.start == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        u->buffer.pos = u->buffer.start;
        u->buffer.last = u->buffer.start;
        u->buffer.end = u->buffer.start + u->conf->buffer_size;
        u->buffer.temporary = 1;

        u->buffer.tag = u->output.tag;

        //初始化headers_in存放头部信息，后端FCGI,proxy解析后的HTTP头部将放入这里
        if (ngx_list_init(&u->headers_in.headers, r->pool, 8,
                          sizeof(ngx_table_elt_t))
            != NGX_OK)
        {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

#if (NGX_HTTP_CACHE)

        if (r->cache) {
            u->buffer.pos += r->cache->header_start;
            u->buffer.last = u->buffer.pos;
        }
#endif
    }

    for ( ;; ) {
        //recv 为 ngx_unix_recv，读取数据放在u->buffer.last的位置，返回读到的大小。
        n = c->recv(c, u->buffer.last, u->buffer.end - u->buffer.last);

        if (n == NGX_AGAIN) { //内核缓冲区已经没数据了
#if 0 
            ngx_add_timer(rev, u->read_timeout);
#endif

            if (ngx_handle_read_event(c->read, 0, NGX_FUNC_LINE) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            return;
        }

        if (n == 0) {
            ngx_log_error(NGX_LOG_ERR, c->log, 0,
                          "upstream prematurely closed connection");
        }

        if (n == NGX_ERROR || n == 0) {
            ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
            return;
        }

        u->buffer.last += n;

#if 0
        u->valid_header_in = 0;

        u->peer.cached = 0;
#endif
        //ngx_http_xxx_process_header
        rc = u->process_header(r);//ngx_http_fastcgi_process_header等，进行数据处理，比如后端返回的数据头部解析，body读取等。

        if (rc == NGX_AGAIN) {

            if (u->buffer.last == u->buffer.end) { //分配的用来存储fastcgi STDOUT头部行包体的buf已经用完了头部行都还没有解析完成，
                ngx_log_error(NGX_LOG_ERR, c->log, 0,
                              "upstream sent too big header");

                ngx_http_upstream_next(r, u,
                                       NGX_HTTP_UPSTREAM_FT_INVALID_HEADER);
                return;
            }

            continue;
        }

        break;
    }

    if (rc == NGX_HTTP_UPSTREAM_INVALID_HEADER) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_INVALID_HEADER);
        return;
    }

    if (rc == NGX_ERROR) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    /* rc == NGX_OK */

    u->state->header_time = ngx_current_msec - u->state->response_time;

    if (u->headers_in.status_n >= NGX_HTTP_SPECIAL_RESPONSE) {

        if (ngx_http_upstream_test_next(r, u) == NGX_OK) {
            return;
        }

        if (ngx_http_upstream_intercept_errors(r, u) == NGX_OK) {
            return;
        }
    }

    //到这里，FCGI等格式的数据已经解析为标准HTTP的表示形式了(除了BODY)，所以可以进行upstream的process_headers。
	//上面的 u->process_header(r)已经进行FCGI等格式的解析了。下面将头部数据拷贝到headers_out.headers数组中。
    if (ngx_http_upstream_process_headers(r, u) != NGX_OK) {
        return;
    }

    if (!r->subrequest_in_memory) {//如果没有子请求了，那就直接发送响应给客户端吧。
        ngx_http_upstream_send_response(r, u);//给客户端发送响应，里面会处理header,body分开发送的情况的
        return;
    }

    /* subrequest content in memory */
    //如果还有子请求的话。子请求不是标准HTTP。

    if (u->input_filter == NULL) {
        u->input_filter_init = ngx_http_upstream_non_buffered_filter_init;
        u->input_filter = ngx_http_upstream_non_buffered_filter;
        u->input_filter_ctx = r;
    }

    if (u->input_filter_init(u->input_filter_ctx) == NGX_ERROR) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    n = u->buffer.last - u->buffer.pos;

    if (n) {
        u->buffer.last = u->buffer.pos;

        u->state->response_length += n;

        if (u->input_filter(u->input_filter_ctx, n) == NGX_ERROR) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }
    }

    if (u->length == 0) {
        ngx_http_upstream_finalize_request(r, u, 0);
        return;
    }

    u->read_event_handler = ngx_http_upstream_process_body_in_memory;//设置body部分的读事件回调。

    ngx_http_upstream_process_body_in_memory(r, u);
}


static ngx_int_t
ngx_http_upstream_test_next(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_uint_t                 status;
    ngx_http_upstream_next_t  *un;

    status = u->headers_in.status_n;

    for (un = ngx_http_upstream_next_errors; un->status; un++) {

        if (status != un->status) {
            continue;
        }

        if (u->peer.tries > 1 && (u->conf->next_upstream & un->mask)) {
            ngx_http_upstream_next(r, u, un->mask);
            return NGX_OK;
        }

#if (NGX_HTTP_CACHE)

        if (u->cache_status == NGX_HTTP_CACHE_EXPIRED
            && (u->conf->cache_use_stale & un->mask))
        {
            ngx_int_t  rc;

            rc = u->reinit_request(r);

            if (rc == NGX_OK) {
                u->cache_status = NGX_HTTP_CACHE_STALE;
                rc = ngx_http_upstream_cache_send(r, u);
            }

            ngx_http_upstream_finalize_request(r, u, rc);
            return NGX_OK;
        }

#endif
    }

#if (NGX_HTTP_CACHE)

    if (status == NGX_HTTP_NOT_MODIFIED
        && u->cache_status == NGX_HTTP_CACHE_EXPIRED
        && u->conf->cache_revalidate)
    {
        time_t     now, valid;
        ngx_int_t  rc;

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http upstream not modified");

        now = ngx_time();
        valid = r->cache->valid_sec;

        rc = u->reinit_request(r);

        if (rc != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u, rc);
            return NGX_OK;
        }

        u->cache_status = NGX_HTTP_CACHE_REVALIDATED;
        rc = ngx_http_upstream_cache_send(r, u);

        if (valid == 0) {
            valid = r->cache->valid_sec;
        }

        if (valid == 0) {
            valid = ngx_http_file_cache_valid(u->conf->cache_valid,
                                              u->headers_in.status_n);
            if (valid) {
                valid = now + valid;
            }
        }

        if (valid) {
            r->cache->valid_sec = valid;
            r->cache->date = now;

            ngx_http_file_cache_update_header(r);
        }

        ngx_http_upstream_finalize_request(r, u, rc);
        return NGX_OK;
    }

#endif

    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_upstream_intercept_errors(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_int_t                  status;
    ngx_uint_t                 i;
    ngx_table_elt_t           *h;
    ngx_http_err_page_t       *err_page;
    ngx_http_core_loc_conf_t  *clcf;

    status = u->headers_in.status_n;

    if (status == NGX_HTTP_NOT_FOUND && u->conf->intercept_404) {
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_NOT_FOUND);
        return NGX_OK;
    }

    if (!u->conf->intercept_errors) {
        return NGX_DECLINED;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (clcf->error_pages == NULL) {
        return NGX_DECLINED;
    }

    err_page = clcf->error_pages->elts;
    for (i = 0; i < clcf->error_pages->nelts; i++) {

        if (err_page[i].status == status) {

            if (status == NGX_HTTP_UNAUTHORIZED
                && u->headers_in.www_authenticate)
            {
                h = ngx_list_push(&r->headers_out.headers);

                if (h == NULL) {
                    ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                    return NGX_OK;
                }

                *h = *u->headers_in.www_authenticate;

                r->headers_out.www_authenticate = h;
            }

#if (NGX_HTTP_CACHE)

            if (r->cache) {
                time_t  valid;

                valid = ngx_http_file_cache_valid(u->conf->cache_valid, status);

                if (valid) {
                    r->cache->valid_sec = ngx_time() + valid;
                    r->cache->error = status;
                }

                ngx_http_file_cache_free(r->cache, u->pipe->temp_file);
            }
#endif
            ngx_http_upstream_finalize_request(r, u, status);

            return NGX_OK;
        }
    }

    return NGX_DECLINED;
}

//检查和c->fd的tcp连接是否有异常
static ngx_int_t
ngx_http_upstream_test_connect(ngx_connection_t *c)
{
    int        err;
    socklen_t  len;

#if (NGX_HAVE_KQUEUE)

    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT)  {
        if (c->write->pending_eof || c->read->pending_eof) {
            if (c->write->pending_eof) {
                err = c->write->kq_errno;

            } else {
                err = c->read->kq_errno;
            }

            c->log->action = "connecting to upstream";
            (void) ngx_connection_error(c, err,
                                    "kevent() reported that connect() failed");
            return NGX_ERROR;
        }

    } else
#endif
    {
        err = 0;
        len = sizeof(int);

        /*
         * BSDs and Linux return 0 and set a pending error in err
         * Solaris returns -1 and sets errno
         */

        if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len)
            == -1)
        {
            err = ngx_socket_errno;
        }

        if (err) {
            c->log->action = "connecting to upstream";
            (void) ngx_connection_error(c, err, "connect() failed");
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}

/*
解析请求的头部字段。每行HEADER回调其copy_handler，然后拷贝一下状态码等。拷贝头部字段到headers_out
*/ //ngx_http_upstream_process_header和ngx_http_upstream_process_headers很像哦，函数名，注意
static ngx_int_t //把从后端返回过来的头部行信息拷贝到r->headers_out中，以备往客户端发送用
ngx_http_upstream_process_headers(ngx_http_request_t *r, ngx_http_upstream_t *u) 
{
    ngx_str_t                       uri, args;
    ngx_uint_t                      i, flags;
    ngx_list_part_t                *part;
    ngx_table_elt_t                *h;
    ngx_http_upstream_header_t     *hh;
    ngx_http_upstream_main_conf_t  *umcf;

    umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);
    
    if (u->headers_in.x_accel_redirect
        && !(u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_XA_REDIRECT))
    {//如果头部中使用了X-Accel-Redirect特性，也就是下载文件的特性，则在这里进行文件下载。，重定向。
    /*nginx X-Accel-Redirect实现文件下载权限控制 
    对文件下载的权限进行精确控制在很多地方都需要，例如有偿的下载服务、网络硬盘、个人相册、防止本站内容被外站盗链等
    步骤0，client请求http://downloaddomain.com/download/my.iso，此请求被CGI程序解析（对于 nginx应该是fastcgi）。
    步骤1，CGI程序根据访问者的身份和所请求的资源其是否有下载权限来判定是否有打开的权限。如果有，那么根据此请求得到对应文件的磁盘存放路径，例如是 /var/data/my.iso。
        那么程序返回时在HTTP header加入X-Accel-Redirect: /protectfile/data/my.iso，并加上head Content-Type:application/octet-stream。
    步骤2，nginx得到cgi程序的回应后发现带有X-Accel-Redirect的header，那么根据这个头记录的路径信息打开磁盘文件。
    步骤3，nginx把打开文件的内容返回给client端。
    这样所有的权限检查都可以在步骤1内完成，而且cgi返回带X-Accel-Redirect的头后，其执行已经终止，剩下的传输文件的工作由nginx 来接管，
        同时X-Accel-Redirect头的信息被nginx删除，也隐藏了文件实际存储目录，并且由于nginx在打开静态文件上使用了 sendfile(2)，其IO效率非常高。
    */
        ngx_http_upstream_finalize_request(r, u, NGX_DECLINED);

        part = &u->headers_in.headers.part; //后端服务器应答的头部行信息全部在该headers链表数组中
        h = part->elts;

        for (i = 0; /* void */; i++) {

            if (i >= part->nelts) { //headers上面的下一个链表
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                h = part->elts;
                i = 0;
            }

            hh = ngx_hash_find(&umcf->headers_in_hash, h[i].hash,
                               h[i].lowcase_key, h[i].key.len);  

            if (hh && hh->redirect) { 
            //如果后端服务器有返回ngx_http_upstream_headers_in中的头部行字段，如果该数组中的成员redirect为1，则执行成员的对应的copy_handler
                if (hh->copy_handler(r, &h[i], hh->conf) != NGX_OK) {
                    ngx_http_finalize_request(r,
                                              NGX_HTTP_INTERNAL_SERVER_ERROR);
                    return NGX_DONE;
                }
            }
        }

        uri = u->headers_in.x_accel_redirect->value; //需要内部重定向的新的uri，通过后面的ngx_http_internal_redirect从新走13 phase阶段流程

        if (uri.data[0] == '@') {
            ngx_http_named_location(r, &uri);

        } else {
            ngx_str_null(&args);
            flags = NGX_HTTP_LOG_UNSAFE;

            if (ngx_http_parse_unsafe_uri(r, &uri, &args, &flags) != NGX_OK) {
                ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
                return NGX_DONE;
            }

            if (r->method != NGX_HTTP_HEAD) {
                r->method = NGX_HTTP_GET;
            }

            ngx_http_internal_redirect(r, &uri, &args);//使用内部重定向，巧妙的下载。里面又会走到各种请求处理阶段。
        }

        ngx_http_finalize_request(r, NGX_DONE);
        return NGX_DONE;
    }

    part = &u->headers_in.headers.part;
    h = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            h = part->elts;
            i = 0;
        }

        if (ngx_hash_find(&u->conf->hide_headers_hash, h[i].hash,
                          h[i].lowcase_key, h[i].key.len)) //这些头部不需要发送给客户端，隐藏
        {
            continue;
        }

        hh = ngx_hash_find(&umcf->headers_in_hash, h[i].hash,
                           h[i].lowcase_key, h[i].key.len);

        if (hh) {//一个个拷贝到请求的headers_out里面
            if (hh->copy_handler(r, &h[i], hh->conf) != NGX_OK) { //从u->headers_in.headers复制到r->headers_out.headers用于发送
                ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_DONE;
            }

            continue; 
        }

        //如果没有注册句柄(在ngx_http_upstream_headers_in找不到该成员)，拷贝后端服务器返回的一行一行的头部信息(u->headers_in.headers中的头部行赋值给r->headers_out.headers)
        if (ngx_http_upstream_copy_header_line(r, &h[i], 0) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_DONE;
        }
    }

    if (r->headers_out.server && r->headers_out.server->value.data == NULL) {
        r->headers_out.server->hash = 0;
    }

    if (r->headers_out.date && r->headers_out.date->value.data == NULL) {
        r->headers_out.date->hash = 0;
    }

    //拷贝状态行，因为这个不是存在headers_in里面的。
    r->headers_out.status = u->headers_in.status_n;
    r->headers_out.status_line = u->headers_in.status_line;

    r->headers_out.content_length_n = u->headers_in.content_length_n;

    r->disable_not_modified = !u->cacheable;

    if (u->conf->force_ranges) {
        r->allow_ranges = 1;
        r->single_range = 1;

#if (NGX_HTTP_CACHE)
        if (r->cached) {
            r->single_range = 0;
        }
#endif
    }

    u->length = -1;

    return NGX_OK;
}


static void
ngx_http_upstream_process_body_in_memory(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    size_t             size;
    ssize_t            n;
    ngx_buf_t         *b;
    ngx_event_t       *rev;
    ngx_connection_t  *c;

    c = u->peer.connection;
    rev = c->read;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process body on memory");

    if (rev->timedout) {
        ngx_connection_error(c, NGX_ETIMEDOUT, "upstream timed out");
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_GATEWAY_TIME_OUT);
        return;
    }

    b = &u->buffer;

    for ( ;; ) {

        size = b->end - b->last;

        if (size == 0) {
            ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                          "upstream buffer is too small to read response");
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }

        n = c->recv(c, b->last, size);

        if (n == NGX_AGAIN) {
            break;
        }

        if (n == 0 || n == NGX_ERROR) {
            ngx_http_upstream_finalize_request(r, u, n);
            return;
        }

        u->state->response_length += n;

        if (u->input_filter(u->input_filter_ctx, n) == NGX_ERROR) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }

        if (!rev->ready) {
            break;
        }
    }

    if (u->length == 0) {
        ngx_http_upstream_finalize_request(r, u, 0);
        return;
    }

    if (ngx_handle_read_event(rev, 0, NGX_FUNC_LINE) != NGX_OK) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    if (rev->active) {
        ngx_add_timer(rev, u->conf->read_timeout, NGX_FUNC_LINE);

    } else if (rev->timer_set) {
        ngx_del_timer(rev, NGX_FUNC_LINE);
    }
}

//发送请求数据给客户端。里面会处理header,body分开发送的情况的 
static void
ngx_http_upstream_send_response(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    int                        tcp_nodelay;
    ssize_t                    n;
    ngx_int_t                  rc;
    ngx_event_pipe_t          *p;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    rc = ngx_http_send_header(r);//先发header，再发body //调用每一个filter过滤，处理头部数据。最后将数据发送给客户端。调用ngx_http_top_header_filter

    if (rc == NGX_ERROR || rc > NGX_OK || r->post_action) {
        ngx_http_upstream_finalize_request(r, u, rc);
        return;
    }

    u->header_sent = 1;//标记已经发送了头部字段，至少是已经挂载出去，经过了filter了。

    if (u->upgrade) {
        ngx_http_upstream_upgrade(r, u);
        return;
    }

    c = r->connection;

    if (r->header_only) {//如果只需要发送头部数据，比如客户端用curl -I 访问的。返回204状态码即可。

        if (!u->buffering) {
            ngx_http_upstream_finalize_request(r, u, rc);
            return;
        }

        if (!u->cacheable && !u->store) {
            ngx_http_upstream_finalize_request(r, u, rc);
            return;
        }

        u->pipe->downstream_error = 1;
    }

    if (r->request_body && r->request_body->temp_file) {
        ngx_pool_run_cleanup_file(r->pool, r->request_body->temp_file->file.fd);
        r->request_body->temp_file->file.fd = NGX_INVALID_FILE;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (!u->buffering) {

        if (u->input_filter == NULL) {
            u->input_filter_init = ngx_http_upstream_non_buffered_filter_init;
            u->input_filter = ngx_http_upstream_non_buffered_filter;
            u->input_filter_ctx = r;
        }

        u->read_event_handler = ngx_http_upstream_process_non_buffered_upstream;
        r->write_event_handler =
                             ngx_http_upstream_process_non_buffered_downstream;

        r->limit_rate = 0;

        if (u->input_filter_init(u->input_filter_ctx) == NGX_ERROR) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }

        if (clcf->tcp_nodelay && c->tcp_nodelay == NGX_TCP_NODELAY_UNSET) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "tcp_nodelay");

            tcp_nodelay = 1;

            if (setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY,
                               (const void *) &tcp_nodelay, sizeof(int)) == -1)
            {
                ngx_connection_error(c, ngx_socket_errno,
                                     "setsockopt(TCP_NODELAY) failed");
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

            c->tcp_nodelay = NGX_TCP_NODELAY_SET;
        }

        n = u->buffer.last - u->buffer.pos;

        if (n) {
            u->buffer.last = u->buffer.pos;

            u->state->response_length += n;

            if (u->input_filter(u->input_filter_ctx, n) == NGX_ERROR) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

            ngx_http_upstream_process_non_buffered_downstream(r);

        } else {
            u->buffer.pos = u->buffer.start;
            u->buffer.last = u->buffer.start;

            if (ngx_http_send_special(r, NGX_HTTP_FLUSH) == NGX_ERROR) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

            if (u->peer.connection->read->ready || u->length == 0) {
                ngx_http_upstream_process_non_buffered_upstream(r, u);
            }
        }

        return;
    }

    /* TODO: preallocate event_pipe bufs, look "Content-Length" */

#if (NGX_HTTP_CACHE)

    if (r->cache && r->cache->file.fd != NGX_INVALID_FILE) {
        ngx_pool_run_cleanup_file(r->pool, r->cache->file.fd);
        r->cache->file.fd = NGX_INVALID_FILE;
    }

    switch (ngx_http_test_predicates(r, u->conf->no_cache)) {

    case NGX_ERROR:
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;

    case NGX_DECLINED:
        u->cacheable = 0;
        break;

    default: /* NGX_OK */

        if (u->cache_status == NGX_HTTP_CACHE_BYPASS) {

            /* create cache if previously bypassed */

            if (ngx_http_file_cache_create(r) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }
        }

        break;
    }

    if (u->cacheable) {
        time_t  now, valid;

        now = ngx_time();

        valid = r->cache->valid_sec;

        if (valid == 0) {
            valid = ngx_http_file_cache_valid(u->conf->cache_valid,
                                              u->headers_in.status_n);
            if (valid) {
                r->cache->valid_sec = now + valid;
            }
        }

        if (valid) {
            r->cache->date = now;
            r->cache->body_start = (u_short) (u->buffer.pos - u->buffer.start);

            if (u->headers_in.status_n == NGX_HTTP_OK
                || u->headers_in.status_n == NGX_HTTP_PARTIAL_CONTENT)
            {
                r->cache->last_modified = u->headers_in.last_modified_time;

                if (u->headers_in.etag) {
                    r->cache->etag = u->headers_in.etag->value;

                } else {
                    ngx_str_null(&r->cache->etag);
                }

            } else {
                r->cache->last_modified = -1;
                ngx_str_null(&r->cache->etag);
            }

            if (ngx_http_file_cache_set_header(r, u->buffer.start) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

        } else {
            u->cacheable = 0;
        }
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http cacheable: %d", u->cacheable);

    if (u->cacheable == 0 && r->cache) {
        ngx_http_file_cache_free(r->cache, u->pipe->temp_file);
    }

#endif

    p = u->pipe;

    p->output_filter = (ngx_event_pipe_output_filter_pt) ngx_http_output_filter;
    p->output_ctx = r;
    p->tag = u->output.tag;
    p->bufs = u->conf->bufs;
    p->busy_size = u->conf->busy_buffers_size;
    p->upstream = u->peer.connection;
    p->downstream = c;
    p->pool = r->pool;
    p->log = c->log;
    p->limit_rate = u->conf->limit_rate;
    p->start_sec = ngx_time();

    p->cacheable = u->cacheable || u->store;

    p->temp_file = ngx_pcalloc(r->pool, sizeof(ngx_temp_file_t));
    if (p->temp_file == NULL) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    p->temp_file->file.fd = NGX_INVALID_FILE;
    p->temp_file->file.log = c->log;
    p->temp_file->path = u->conf->temp_path;
    p->temp_file->pool = r->pool;

    if (p->cacheable) {
        p->temp_file->persistent = 1;

#if (NGX_HTTP_CACHE)
        if (r->cache && r->cache->file_cache->temp_path) {
            p->temp_file->path = r->cache->file_cache->temp_path;
        }
#endif

    } else {
        p->temp_file->log_level = NGX_LOG_WARN;
        p->temp_file->warn = "an upstream response is buffered "
                             "to a temporary file";
    }

    p->max_temp_file_size = u->conf->max_temp_file_size;
    p->temp_file_write_size = u->conf->temp_file_write_size;

    p->preread_bufs = ngx_alloc_chain_link(r->pool);
    if (p->preread_bufs == NULL) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    p->preread_bufs->buf = &u->buffer;
    p->preread_bufs->next = NULL;
    u->buffer.recycled = 1;

    p->preread_size = u->buffer.last - u->buffer.pos;

    if (u->cacheable) {

        p->buf_to_file = ngx_calloc_buf(r->pool);
        if (p->buf_to_file == NULL) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }

        p->buf_to_file->start = u->buffer.start;
        p->buf_to_file->pos = u->buffer.start;
        p->buf_to_file->last = u->buffer.pos;
        p->buf_to_file->temporary = 1;
    }

    if (ngx_event_flags & NGX_USE_IOCP_EVENT) {
        /* the posted aio operation may corrupt a shadow buffer */
        p->single_buf = 1;
    }

    /* TODO: p->free_bufs = 0 if use ngx_create_chain_of_bufs() */
    p->free_bufs = 1;

    /*
     * event_pipe would do u->buffer.last += p->preread_size
     * as though these bytes were read
     */
    u->buffer.last = u->buffer.pos;

    if (u->conf->cyclic_temp_file) {

        /*
         * we need to disable the use of sendfile() if we use cyclic temp file
         * because the writing a new data may interfere with sendfile()
         * that uses the same kernel file pages (at least on FreeBSD)
         */

        p->cyclic_temp_file = 1;
        c->sendfile = 0;

    } else {
        p->cyclic_temp_file = 0;
    }

    p->read_timeout = u->conf->read_timeout;
    p->send_timeout = clcf->send_timeout;
    p->send_lowat = clcf->send_lowat;

    p->length = -1;

    if (u->input_filter_init
        && u->input_filter_init(p->input_ctx) != NGX_OK)
    {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    u->read_event_handler = ngx_http_upstream_process_upstream;
    r->write_event_handler = ngx_http_upstream_process_downstream;

    ngx_http_upstream_process_upstream(r, u);
}


static void
ngx_http_upstream_upgrade(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    int                        tcp_nodelay;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    /* TODO: prevent upgrade if not requested or not possible */

    r->keepalive = 0;
    c->log->action = "proxying upgraded connection";

    u->read_event_handler = ngx_http_upstream_upgraded_read_upstream;
    u->write_event_handler = ngx_http_upstream_upgraded_write_upstream;
    r->read_event_handler = ngx_http_upstream_upgraded_read_downstream;
    r->write_event_handler = ngx_http_upstream_upgraded_write_downstream;

    if (clcf->tcp_nodelay) {
        tcp_nodelay = 1;

        if (c->tcp_nodelay == NGX_TCP_NODELAY_UNSET) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "tcp_nodelay");

            if (setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY,
                           (const void *) &tcp_nodelay, sizeof(int)) == -1)
            {
                ngx_connection_error(c, ngx_socket_errno,
                                     "setsockopt(TCP_NODELAY) failed");
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

            c->tcp_nodelay = NGX_TCP_NODELAY_SET;
        }

        if (u->peer.connection->tcp_nodelay == NGX_TCP_NODELAY_UNSET) {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, u->peer.connection->log, 0,
                           "tcp_nodelay");

            if (setsockopt(u->peer.connection->fd, IPPROTO_TCP, TCP_NODELAY,
                           (const void *) &tcp_nodelay, sizeof(int)) == -1)
            {
                ngx_connection_error(u->peer.connection, ngx_socket_errno,
                                     "setsockopt(TCP_NODELAY) failed");
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

            u->peer.connection->tcp_nodelay = NGX_TCP_NODELAY_SET;
        }
    }

    if (ngx_http_send_special(r, NGX_HTTP_FLUSH) == NGX_ERROR) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    if (u->peer.connection->read->ready
        || u->buffer.pos != u->buffer.last)
    {
        ngx_post_event(c->read, &ngx_posted_events);
        ngx_http_upstream_process_upgraded(r, 1, 1);
        return;
    }

    ngx_http_upstream_process_upgraded(r, 0, 1);
}


static void
ngx_http_upstream_upgraded_read_downstream(ngx_http_request_t *r)
{
    ngx_http_upstream_process_upgraded(r, 0, 0);
}


static void
ngx_http_upstream_upgraded_write_downstream(ngx_http_request_t *r)
{
    ngx_http_upstream_process_upgraded(r, 1, 1);
}


static void
ngx_http_upstream_upgraded_read_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_http_upstream_process_upgraded(r, 1, 0);
}


static void
ngx_http_upstream_upgraded_write_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_http_upstream_process_upgraded(r, 0, 1);
}


static void
ngx_http_upstream_process_upgraded(ngx_http_request_t *r,
    ngx_uint_t from_upstream, ngx_uint_t do_write)
{
    size_t                     size;
    ssize_t                    n;
    ngx_buf_t                 *b;
    ngx_connection_t          *c, *downstream, *upstream, *dst, *src;
    ngx_http_upstream_t       *u;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;
    u = r->upstream;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process upgraded, fu:%ui", from_upstream);

    downstream = c;
    upstream = u->peer.connection;

    if (downstream->write->timedout) {
        c->timedout = 1;
        ngx_connection_error(c, NGX_ETIMEDOUT, "client timed out");
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_REQUEST_TIME_OUT);
        return;
    }

    if (upstream->read->timedout || upstream->write->timedout) {
        ngx_connection_error(c, NGX_ETIMEDOUT, "upstream timed out");
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_GATEWAY_TIME_OUT);
        return;
    }

    if (from_upstream) {
        src = upstream;
        dst = downstream;
        b = &u->buffer;

    } else {
        src = downstream;
        dst = upstream;
        b = &u->from_client;

        if (r->header_in->last > r->header_in->pos) {
            b = r->header_in;
            b->end = b->last;
            do_write = 1;
        }

        if (b->start == NULL) {
            b->start = ngx_palloc(r->pool, u->conf->buffer_size);
            if (b->start == NULL) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

            b->pos = b->start;
            b->last = b->start;
            b->end = b->start + u->conf->buffer_size;
            b->temporary = 1;
            b->tag = u->output.tag;
        }
    }

    for ( ;; ) {

        if (do_write) {

            size = b->last - b->pos;

            if (size && dst->write->ready) {

                n = dst->send(dst, b->pos, size);

                if (n == NGX_ERROR) {
                    ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                    return;
                }

                if (n > 0) {
                    b->pos += n;

                    if (b->pos == b->last) {
                        b->pos = b->start;
                        b->last = b->start;
                    }
                }
            }
        }

        size = b->end - b->last;

        if (size && src->read->ready) {

            n = src->recv(src, b->last, size);

            if (n == NGX_AGAIN || n == 0) {
                break;
            }

            if (n > 0) {
                do_write = 1;
                b->last += n;

                continue;
            }

            if (n == NGX_ERROR) {
                src->read->eof = 1;
            }
        }

        break;
    }

    if ((upstream->read->eof && u->buffer.pos == u->buffer.last)
        || (downstream->read->eof && u->from_client.pos == u->from_client.last)
        || (downstream->read->eof && upstream->read->eof))
    {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "http upstream upgraded done");
        ngx_http_upstream_finalize_request(r, u, 0);
        return;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (ngx_handle_write_event(upstream->write, u->conf->send_lowat, NGX_FUNC_LINE)
        != NGX_OK)
    {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    if (upstream->write->active && !upstream->write->ready) {
        ngx_add_timer(upstream->write, u->conf->send_timeout, NGX_FUNC_LINE);

    } else if (upstream->write->timer_set) {
        ngx_del_timer(upstream->write, NGX_FUNC_LINE);
    }

    if (ngx_handle_read_event(upstream->read, 0, NGX_FUNC_LINE) != NGX_OK) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    if (upstream->read->active && !upstream->read->ready) {
        ngx_add_timer(upstream->read, u->conf->read_timeout, NGX_FUNC_LINE);

    } else if (upstream->read->timer_set) {
        ngx_del_timer(upstream->read, NGX_FUNC_LINE);
    }

    if (ngx_handle_write_event(downstream->write, clcf->send_lowat, NGX_FUNC_LINE)
        != NGX_OK)
    {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    if (ngx_handle_read_event(downstream->read, 0, NGX_FUNC_LINE) != NGX_OK) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    if (downstream->write->active && !downstream->write->ready) {
        ngx_add_timer(downstream->write, clcf->send_timeout, NGX_FUNC_LINE);

    } else if (downstream->write->timer_set) {
        ngx_del_timer(downstream->write, NGX_FUNC_LINE);
    }
}


static void
ngx_http_upstream_process_non_buffered_downstream(ngx_http_request_t *r)
{
    ngx_event_t          *wev;
    ngx_connection_t     *c;
    ngx_http_upstream_t  *u;

    c = r->connection;
    u = r->upstream;
    wev = c->write;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process non buffered downstream");

    c->log->action = "sending to client";

    if (wev->timedout) {
        c->timedout = 1;
        ngx_connection_error(c, NGX_ETIMEDOUT, "client timed out");
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_REQUEST_TIME_OUT);
        return;
    }

    ngx_http_upstream_process_non_buffered_request(r, 1);
}


static void
ngx_http_upstream_process_non_buffered_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_connection_t  *c;

    c = u->peer.connection;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process non buffered upstream");

    c->log->action = "reading upstream";

    if (c->read->timedout) {
        ngx_connection_error(c, NGX_ETIMEDOUT, "upstream timed out");
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_GATEWAY_TIME_OUT);
        return;
    }

    ngx_http_upstream_process_non_buffered_request(r, 0);
}


static void
ngx_http_upstream_process_non_buffered_request(ngx_http_request_t *r,
    ngx_uint_t do_write)
{
    size_t                     size;
    ssize_t                    n;
    ngx_buf_t                 *b;
    ngx_int_t                  rc;
    ngx_connection_t          *downstream, *upstream;
    ngx_http_upstream_t       *u;
    ngx_http_core_loc_conf_t  *clcf;

    u = r->upstream;
    downstream = r->connection;
    upstream = u->peer.connection;

    b = &u->buffer;

    do_write = do_write || u->length == 0;

    for ( ;; ) {

        if (do_write) {

            if (u->out_bufs || u->busy_bufs) {
                rc = ngx_http_output_filter(r, u->out_bufs);

                if (rc == NGX_ERROR) {
                    ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                    return;
                }

                ngx_chain_update_chains(r->pool, &u->free_bufs, &u->busy_bufs,
                                        &u->out_bufs, u->output.tag);
            }

            if (u->busy_bufs == NULL) {

                if (u->length == 0
                    || (upstream->read->eof && u->length == -1))
                {
                    ngx_http_upstream_finalize_request(r, u, 0);
                    return;
                }

                if (upstream->read->eof) {
                    ngx_log_error(NGX_LOG_ERR, upstream->log, 0,
                                  "upstream prematurely closed connection");

                    ngx_http_upstream_finalize_request(r, u,
                                                       NGX_HTTP_BAD_GATEWAY);
                    return;
                }

                if (upstream->read->error) {
                    ngx_http_upstream_finalize_request(r, u,
                                                       NGX_HTTP_BAD_GATEWAY);
                    return;
                }

                b->pos = b->start;
                b->last = b->start;
            }
        }

        size = b->end - b->last;

        if (size && upstream->read->ready) {

            n = upstream->recv(upstream, b->last, size);

            if (n == NGX_AGAIN) {
                break;
            }

            if (n > 0) {
                u->state->response_length += n;

                if (u->input_filter(u->input_filter_ctx, n) == NGX_ERROR) {
                    ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                    return;
                }
            }

            do_write = 1;

            continue;
        }

        break;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (downstream->data == r) {
        if (ngx_handle_write_event(downstream->write, clcf->send_lowat, NGX_FUNC_LINE)
            != NGX_OK)
        {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }
    }

    if (downstream->write->active && !downstream->write->ready) {
        ngx_add_timer(downstream->write, clcf->send_timeout, NGX_FUNC_LINE);

    } else if (downstream->write->timer_set) {
        ngx_del_timer(downstream->write, NGX_FUNC_LINE);
    }

    if (ngx_handle_read_event(upstream->read, 0, NGX_FUNC_LINE) != NGX_OK) {
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;
    }

    if (upstream->read->active && !upstream->read->ready) {
        ngx_add_timer(upstream->read, u->conf->read_timeout, NGX_FUNC_LINE);

    } else if (upstream->read->timer_set) {
        ngx_del_timer(upstream->read, NGX_FUNC_LINE);
    }
}


static ngx_int_t
ngx_http_upstream_non_buffered_filter_init(void *data)
{
    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_non_buffered_filter(void *data, ssize_t bytes)
{
    ngx_http_request_t  *r = data;

    ngx_buf_t            *b;
    ngx_chain_t          *cl, **ll;
    ngx_http_upstream_t  *u;

    u = r->upstream;

    for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
        ll = &cl->next;
    }

    cl = ngx_chain_get_free_buf(r->pool, &u->free_bufs);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    *ll = cl;

    cl->buf->flush = 1;
    cl->buf->memory = 1;

    b = &u->buffer;

    cl->buf->pos = b->last;
    b->last += bytes;
    cl->buf->last = b->last;
    cl->buf->tag = u->output.tag;

    if (u->length == -1) {
        return NGX_OK;
    }

    u->length -= bytes;

    return NGX_OK;
}


static void
ngx_http_upstream_process_downstream(ngx_http_request_t *r)
{
    ngx_event_t          *wev;
    ngx_connection_t     *c;
    ngx_event_pipe_t     *p;
    ngx_http_upstream_t  *u;

    c = r->connection;
    u = r->upstream;
    p = u->pipe;
    wev = c->write;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process downstream");

    c->log->action = "sending to client";

    if (wev->timedout) {

        if (wev->delayed) {

            wev->timedout = 0;
            wev->delayed = 0;

            if (!wev->ready) {
                ngx_add_timer(wev, p->send_timeout, NGX_FUNC_LINE);

                if (ngx_handle_write_event(wev, p->send_lowat, NGX_FUNC_LINE) != NGX_OK) {
                    ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                }

                return;
            }

            if (ngx_event_pipe(p, wev->write) == NGX_ABORT) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

        } else {
            p->downstream_error = 1;
            c->timedout = 1;
            ngx_connection_error(c, NGX_ETIMEDOUT, "client timed out");
        }

    } else {

        if (wev->delayed) {

            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http downstream delayed");

            if (ngx_handle_write_event(wev, p->send_lowat, NGX_FUNC_LINE) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            }

            return;
        }

        if (ngx_event_pipe(p, 1) == NGX_ABORT) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }
    }

    ngx_http_upstream_process_request(r, u);
}


static void
ngx_http_upstream_process_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_event_t       *rev;
    ngx_event_pipe_t  *p;
    ngx_connection_t  *c;

    c = u->peer.connection;
    p = u->pipe;
    rev = c->read;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process upstream");

    c->log->action = "reading upstream";

    if (rev->timedout) {

        if (rev->delayed) {

            rev->timedout = 0;
            rev->delayed = 0;

            if (!rev->ready) {
                ngx_add_timer(rev, p->read_timeout, NGX_FUNC_LINE);

                if (ngx_handle_read_event(rev, 0, NGX_FUNC_LINE) != NGX_OK) {
                    ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                }

                return;
            }

            if (ngx_event_pipe(p, 0) == NGX_ABORT) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

        } else {
            p->upstream_error = 1;
            ngx_connection_error(c, NGX_ETIMEDOUT, "upstream timed out");
        }

    } else {

        if (rev->delayed) {

            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http upstream delayed");

            if (ngx_handle_read_event(rev, 0, NGX_FUNC_LINE) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            }

            return;
        }

        if (ngx_event_pipe(p, 0) == NGX_ABORT) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }
    }

    ngx_http_upstream_process_request(r, u);
}


static void
ngx_http_upstream_process_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_temp_file_t   *tf;
    ngx_event_pipe_t  *p;

    p = u->pipe;

    if (u->peer.connection) {

        if (u->store) {

            if (p->upstream_eof || p->upstream_done) {

                tf = p->temp_file;

                if (u->headers_in.status_n == NGX_HTTP_OK
                    && (p->upstream_done || p->length == -1)
                    && (u->headers_in.content_length_n == -1
                        || u->headers_in.content_length_n == tf->offset))
                {
                    ngx_http_upstream_store(r, u);
                }
            }
        }

#if (NGX_HTTP_CACHE)

        if (u->cacheable) {

            if (p->upstream_done) {
                ngx_http_file_cache_update(r, p->temp_file);

            } else if (p->upstream_eof) {

                tf = p->temp_file;

                if (p->length == -1
                    && (u->headers_in.content_length_n == -1
                        || u->headers_in.content_length_n
                           == tf->offset - (off_t) r->cache->body_start))
                {
                    ngx_http_file_cache_update(r, tf);

                } else {
                    ngx_http_file_cache_free(r->cache, tf);
                }

            } else if (p->upstream_error) {
                ngx_http_file_cache_free(r->cache, p->temp_file);
            }
        }

#endif

        if (p->upstream_done || p->upstream_eof || p->upstream_error) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http upstream exit: %p", p->out);

            if (p->upstream_done
                || (p->upstream_eof && p->length == -1))
            {
                ngx_http_upstream_finalize_request(r, u, 0);
                return;
            }

            if (p->upstream_eof) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "upstream prematurely closed connection");
            }

            ngx_http_upstream_finalize_request(r, u, NGX_HTTP_BAD_GATEWAY);
            return;
        }
    }

    if (p->downstream_error) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http upstream downstream error");

        if (!u->cacheable && !u->store && u->peer.connection) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        }
    }
}


static void
ngx_http_upstream_store(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    size_t                  root;
    time_t                  lm;
    ngx_str_t               path;
    ngx_temp_file_t        *tf;
    ngx_ext_rename_file_t   ext;

    tf = u->pipe->temp_file;

    if (tf->file.fd == NGX_INVALID_FILE) {

        /* create file for empty 200 response */

        tf = ngx_pcalloc(r->pool, sizeof(ngx_temp_file_t));
        if (tf == NULL) {
            return;
        }

        tf->file.fd = NGX_INVALID_FILE;
        tf->file.log = r->connection->log;
        tf->path = u->conf->temp_path;
        tf->pool = r->pool;
        tf->persistent = 1;

        if (ngx_create_temp_file(&tf->file, tf->path, tf->pool,
                                 tf->persistent, tf->clean, tf->access)
            != NGX_OK)
        {
            return;
        }

        u->pipe->temp_file = tf;
    }

    ext.access = u->conf->store_access;
    ext.path_access = u->conf->store_access;
    ext.time = -1;
    ext.create_path = 1;
    ext.delete_file = 1;
    ext.log = r->connection->log;

    if (u->headers_in.last_modified) {

        lm = ngx_parse_http_time(u->headers_in.last_modified->value.data,
                                 u->headers_in.last_modified->value.len);

        if (lm != NGX_ERROR) {
            ext.time = lm;
            ext.fd = tf->file.fd;
        }
    }

    if (u->conf->store_lengths == NULL) {

        if (ngx_http_map_uri_to_path(r, &path, &root, 0) == NULL) {
            return;
        }

    } else {
        if (ngx_http_script_run(r, &path, u->conf->store_lengths->elts, 0,
                                u->conf->store_values->elts)
            == NULL)
        {
            return;
        }
    }

    path.len--;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "upstream stores \"%s\" to \"%s\"",
                   tf->file.name.data, path.data);

    (void) ngx_ext_rename_file(&tf->file.name, &path, &ext);

    u->store = 0;
}


static void
ngx_http_upstream_dummy_handler(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream dummy handler");
}

//如果测试失败，调用ngx_http_upstream_next函数，这个函数可能再次调用peer.get调用别的后端服务器进行连接。
static void // ngx_http_upstream_next 方法尝试与其他上游服务器建立连接
ngx_http_upstream_next(ngx_http_request_t *r, ngx_http_upstream_t *u,
    ngx_uint_t ft_type)
{
    ngx_msec_t  timeout;
    ngx_uint_t  status, state;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http next upstream, %xi", ft_type);

    if (u->peer.sockaddr) {

        if (ft_type == NGX_HTTP_UPSTREAM_FT_HTTP_403
            || ft_type == NGX_HTTP_UPSTREAM_FT_HTTP_404)
        {
            state = NGX_PEER_NEXT;

        } else {
            state = NGX_PEER_FAILED;
        }

        u->peer.free(&u->peer, u->peer.data, state);
        u->peer.sockaddr = NULL;
    }

    if (ft_type == NGX_HTTP_UPSTREAM_FT_TIMEOUT) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, NGX_ETIMEDOUT,
                      "upstream timed out");
    }

    if (u->peer.cached && ft_type == NGX_HTTP_UPSTREAM_FT_ERROR
        && (!u->request_sent || !r->request_body_no_buffering))
    {
        status = 0;

        /* TODO: inform balancer instead */

        u->peer.tries++;

    } else {
        switch (ft_type) {

        case NGX_HTTP_UPSTREAM_FT_TIMEOUT:
            status = NGX_HTTP_GATEWAY_TIME_OUT;
            break;

        case NGX_HTTP_UPSTREAM_FT_HTTP_500:
            status = NGX_HTTP_INTERNAL_SERVER_ERROR;
            break;

        case NGX_HTTP_UPSTREAM_FT_HTTP_403:
            status = NGX_HTTP_FORBIDDEN;
            break;

        case NGX_HTTP_UPSTREAM_FT_HTTP_404:
            status = NGX_HTTP_NOT_FOUND;
            break;

        /*
         * NGX_HTTP_UPSTREAM_FT_BUSY_LOCK and NGX_HTTP_UPSTREAM_FT_MAX_WAITING
         * never reach here
         */

        default:
            status = NGX_HTTP_BAD_GATEWAY;
        }
    }

    if (r->connection->error) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_CLIENT_CLOSED_REQUEST);
        return;
    }

    if (status) {
        u->state->status = status;
        timeout = u->conf->next_upstream_timeout;

        if (u->peer.tries == 0
            || !(u->conf->next_upstream & ft_type)
            || (u->request_sent && r->request_body_no_buffering)
            || (timeout && ngx_current_msec - u->peer.start_time >= timeout))
        {
#if (NGX_HTTP_CACHE)

            if (u->cache_status == NGX_HTTP_CACHE_EXPIRED
                && (u->conf->cache_use_stale & ft_type))
            {
                ngx_int_t  rc;

                rc = u->reinit_request(r);

                if (rc == NGX_OK) {
                    u->cache_status = NGX_HTTP_CACHE_STALE;
                    rc = ngx_http_upstream_cache_send(r, u);
                }

                ngx_http_upstream_finalize_request(r, u, rc);
                return;
            }
#endif

            ngx_http_upstream_finalize_request(r, u, status);
            return;
        }
    }

    if (u->peer.connection) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "close http upstream connection: %d",
                       u->peer.connection->fd);
#if (NGX_HTTP_SSL)

        if (u->peer.connection->ssl) {
            u->peer.connection->ssl->no_wait_shutdown = 1;
            u->peer.connection->ssl->no_send_shutdown = 1;

            (void) ngx_ssl_shutdown(u->peer.connection);
        }
#endif

        if (u->peer.connection->pool) {
            ngx_destroy_pool(u->peer.connection->pool);
        }

        ngx_close_connection(u->peer.connection);
        u->peer.connection = NULL;
    }

    ngx_http_upstream_connect(r, u);//再次发起连接
}


static void
ngx_http_upstream_cleanup(void *data)
{
    ngx_http_request_t *r = data;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "cleanup http upstream request: \"%V\"", &r->uri);

    ngx_http_upstream_finalize_request(r, r->upstream, NGX_DONE);
}


static void
ngx_http_upstream_finalize_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_int_t rc)
{
    ngx_uint_t  flush;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "finalize http upstream request: %i", rc);

    if (u->cleanup == NULL) {
        /* the request was already finalized */
        ngx_http_finalize_request(r, NGX_DONE);
        return;
    }

    *u->cleanup = NULL;
    u->cleanup = NULL;

    if (u->resolved && u->resolved->ctx) {
        ngx_resolve_name_done(u->resolved->ctx);
        u->resolved->ctx = NULL;
    }

    if (u->state && u->state->response_time) {
        u->state->response_time = ngx_current_msec - u->state->response_time;

        if (u->pipe && u->pipe->read_length) {
            u->state->response_length = u->pipe->read_length;
        }
    }

    u->finalize_request(r, rc);

    if (u->peer.free && u->peer.sockaddr) {
        u->peer.free(&u->peer, u->peer.data, 0);
        u->peer.sockaddr = NULL;
    }

    if (u->peer.connection) {

#if (NGX_HTTP_SSL)

        /* TODO: do not shutdown persistent connection */

        if (u->peer.connection->ssl) {

            /*
             * We send the "close notify" shutdown alert to the upstream only
             * and do not wait its "close notify" shutdown alert.
             * It is acceptable according to the TLS standard.
             */

            u->peer.connection->ssl->no_wait_shutdown = 1;

            (void) ngx_ssl_shutdown(u->peer.connection);
        }
#endif

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "close http upstream connection: %d",
                       u->peer.connection->fd);

        if (u->peer.connection->pool) {
            ngx_destroy_pool(u->peer.connection->pool);
        }

        ngx_close_connection(u->peer.connection);
    }

    u->peer.connection = NULL;

    if (u->pipe && u->pipe->temp_file) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http upstream temp fd: %d",
                       u->pipe->temp_file->file.fd);
    }

    if (u->store && u->pipe && u->pipe->temp_file
        && u->pipe->temp_file->file.fd != NGX_INVALID_FILE)
    {
        if (ngx_delete_file(u->pipe->temp_file->file.name.data)
            == NGX_FILE_ERROR)
        {
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                          ngx_delete_file_n " \"%s\" failed",
                          u->pipe->temp_file->file.name.data);
        }
    }

#if (NGX_HTTP_CACHE)

    if (r->cache) {

        if (u->cacheable) {

            if (rc == NGX_HTTP_BAD_GATEWAY || rc == NGX_HTTP_GATEWAY_TIME_OUT) {
                time_t  valid;

                valid = ngx_http_file_cache_valid(u->conf->cache_valid, rc);

                if (valid) {
                    r->cache->valid_sec = ngx_time() + valid;
                    r->cache->error = rc;
                }
            }
        }

        ngx_http_file_cache_free(r->cache, u->pipe->temp_file);
    }

#endif

    if (r->subrequest_in_memory
        && u->headers_in.status_n >= NGX_HTTP_SPECIAL_RESPONSE)
    {
        u->buffer.last = u->buffer.pos;
    }

    if (rc == NGX_DECLINED) {
        return;
    }

    r->connection->log->action = "sending to client";

    if (!u->header_sent
        || rc == NGX_HTTP_REQUEST_TIME_OUT
        || rc == NGX_HTTP_CLIENT_CLOSED_REQUEST)
    {
        ngx_http_finalize_request(r, rc);
        return;
    }

    flush = 0;

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        rc = NGX_ERROR;
        flush = 1;
    }

    if (r->header_only) {
        ngx_http_finalize_request(r, rc);
        return;
    }

    if (rc == 0) {
        rc = ngx_http_send_special(r, NGX_HTTP_LAST);

    } else if (flush) {
        r->keepalive = 0;
        rc = ngx_http_send_special(r, NGX_HTTP_FLUSH);
    }

    ngx_http_finalize_request(r, rc);
}


static ngx_int_t
ngx_http_upstream_process_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  **ph;

    ph = (ngx_table_elt_t **) ((char *) &r->upstream->headers_in + offset);

    if (*ph == NULL) { //这个就是坚持r->upstream->headers_in中的offset成员处是否有存放该ngx_table_elt_t的空间
        *ph = h;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_ignore_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_content_length(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_http_upstream_t  *u;

    u = r->upstream;

    u->headers_in.content_length = h;
    u->headers_in.content_length_n = ngx_atoof(h->value.data, h->value.len);

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_last_modified(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_http_upstream_t  *u;

    u = r->upstream;

    u->headers_in.last_modified = h;

#if (NGX_HTTP_CACHE)

    if (u->cacheable) {
        u->headers_in.last_modified_time = ngx_parse_http_time(h->value.data,
                                                               h->value.len);
    }

#endif

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_set_cookie(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_array_t           *pa;
    ngx_table_elt_t      **ph;
    ngx_http_upstream_t   *u;

    u = r->upstream;
    pa = &u->headers_in.cookies;

    if (pa->elts == NULL) {
        if (ngx_array_init(pa, r->pool, 1, sizeof(ngx_table_elt_t *)) != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    ph = ngx_array_push(pa);
    if (ph == NULL) {
        return NGX_ERROR;
    }

    *ph = h;

#if (NGX_HTTP_CACHE)
    if (!(u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_SET_COOKIE)) {
        u->cacheable = 0;
    }
#endif

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_cache_control(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_array_t          *pa;
    ngx_table_elt_t     **ph;
    ngx_http_upstream_t  *u;

    u = r->upstream;
    pa = &u->headers_in.cache_control;

    if (pa->elts == NULL) {
       if (ngx_array_init(pa, r->pool, 2, sizeof(ngx_table_elt_t *)) != NGX_OK)
       {
           return NGX_ERROR;
       }
    }

    ph = ngx_array_push(pa);
    if (ph == NULL) {
        return NGX_ERROR;
    }

    *ph = h;

#if (NGX_HTTP_CACHE)
    {
    u_char     *p, *start, *last;
    ngx_int_t   n;

    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_CACHE_CONTROL) {
        return NGX_OK;
    }

    if (r->cache == NULL) {
        return NGX_OK;
    }

    if (r->cache->valid_sec != 0 && u->headers_in.x_accel_expires != NULL) {
        return NGX_OK;
    }

    start = h->value.data;
    last = start + h->value.len;

    if (ngx_strlcasestrn(start, last, (u_char *) "no-cache", 8 - 1) != NULL
        || ngx_strlcasestrn(start, last, (u_char *) "no-store", 8 - 1) != NULL
        || ngx_strlcasestrn(start, last, (u_char *) "private", 7 - 1) != NULL)
    {
        u->cacheable = 0;
        return NGX_OK;
    }

    p = ngx_strlcasestrn(start, last, (u_char *) "s-maxage=", 9 - 1);
    offset = 9;

    if (p == NULL) {
        p = ngx_strlcasestrn(start, last, (u_char *) "max-age=", 8 - 1);
        offset = 8;
    }

    if (p == NULL) {
        return NGX_OK;
    }

    n = 0;

    for (p += offset; p < last; p++) {
        if (*p == ',' || *p == ';' || *p == ' ') {
            break;
        }

        if (*p >= '0' && *p <= '9') {
            n = n * 10 + *p - '0';
            continue;
        }

        u->cacheable = 0;
        return NGX_OK;
    }

    if (n == 0) {
        u->cacheable = 0;
        return NGX_OK;
    }

    r->cache->valid_sec = ngx_time() + n;
    }
#endif

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_expires(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_http_upstream_t  *u;

    u = r->upstream;
    u->headers_in.expires = h;

#if (NGX_HTTP_CACHE)
    {
    time_t  expires;

    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_EXPIRES) {
        return NGX_OK;
    }

    if (r->cache == NULL) {
        return NGX_OK;
    }

    if (r->cache->valid_sec != 0) {
        return NGX_OK;
    }

    expires = ngx_parse_http_time(h->value.data, h->value.len);

    if (expires == NGX_ERROR || expires < ngx_time()) {
        u->cacheable = 0;
        return NGX_OK;
    }

    r->cache->valid_sec = expires;
    }
#endif

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_accel_expires(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_http_upstream_t  *u;

    u = r->upstream;
    u->headers_in.x_accel_expires = h;

#if (NGX_HTTP_CACHE)
    {
    u_char     *p;
    size_t      len;
    ngx_int_t   n;

    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_XA_EXPIRES) {
        return NGX_OK;
    }

    if (r->cache == NULL) {
        return NGX_OK;
    }

    len = h->value.len;
    p = h->value.data;

    if (p[0] != '@') {
        n = ngx_atoi(p, len);

        switch (n) {
        case 0:
            u->cacheable = 0;
            /* fall through */

        case NGX_ERROR:
            return NGX_OK;

        default:
            r->cache->valid_sec = ngx_time() + n;
            return NGX_OK;
        }
    }

    p++;
    len--;

    n = ngx_atoi(p, len);

    if (n != NGX_ERROR) {
        r->cache->valid_sec = n;
    }
    }
#endif

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_limit_rate(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_int_t             n;
    ngx_http_upstream_t  *u;

    u = r->upstream;
    u->headers_in.x_accel_limit_rate = h;

    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_XA_LIMIT_RATE) {
        return NGX_OK;
    }

    n = ngx_atoi(h->value.data, h->value.len);

    if (n != NGX_ERROR) {
        r->limit_rate = (size_t) n;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_buffering(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    u_char                c0, c1, c2;
    ngx_http_upstream_t  *u;

    u = r->upstream;

    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_XA_BUFFERING) {
        return NGX_OK;
    }

    if (u->conf->change_buffering) {

        if (h->value.len == 2) {
            c0 = ngx_tolower(h->value.data[0]);
            c1 = ngx_tolower(h->value.data[1]);

            if (c0 == 'n' && c1 == 'o') {
                u->buffering = 0;
            }

        } else if (h->value.len == 3) {
            c0 = ngx_tolower(h->value.data[0]);
            c1 = ngx_tolower(h->value.data[1]);
            c2 = ngx_tolower(h->value.data[2]);

            if (c0 == 'y' && c1 == 'e' && c2 == 's') {
                u->buffering = 1;
            }
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_charset(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    if (r->upstream->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_XA_CHARSET) {
        return NGX_OK;
    }

    r->headers_out.override_charset = &h->value;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_connection(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    r->upstream->headers_in.connection = h;

    if (ngx_strlcasestrn(h->value.data, h->value.data + h->value.len,
                         (u_char *) "close", 5 - 1)
        != NULL)
    {
        r->upstream->headers_in.connection_close = 1;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_transfer_encoding(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    r->upstream->headers_in.transfer_encoding = h;

    if (ngx_strlcasestrn(h->value.data, h->value.data + h->value.len,
                         (u_char *) "chunked", 7 - 1)
        != NULL)
    {
        r->upstream->headers_in.chunked = 1;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_process_vary(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_http_upstream_t  *u;

    u = r->upstream;
    u->headers_in.vary = h;

#if (NGX_HTTP_CACHE)

    if (u->conf->ignore_headers & NGX_HTTP_UPSTREAM_IGN_VARY) {
        return NGX_OK;
    }

    if (r->cache == NULL) {
        return NGX_OK;
    }

    if (h->value.len > NGX_HTTP_CACHE_VARY_LEN
        || (h->value.len == 1 && h->value.data[0] == '*'))
    {
        u->cacheable = 0;
    }

    r->cache->vary = h->value;

#endif

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_copy_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  *ho, **ph;

    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    *ho = *h;

    if (offset) {
        ph = (ngx_table_elt_t **) ((char *) &r->headers_out + offset);
        *ph = ho;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_copy_multi_header_lines(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_array_t      *pa;
    ngx_table_elt_t  *ho, **ph;

    pa = (ngx_array_t *) ((char *) &r->headers_out + offset);

    if (pa->elts == NULL) {
        if (ngx_array_init(pa, r->pool, 2, sizeof(ngx_table_elt_t *)) != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    ph = ngx_array_push(pa);
    if (ph == NULL) {
        return NGX_ERROR;
    }

    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    *ho = *h;
    *ph = ho;

    return NGX_OK;
}

//Content-Type:text/html;charset=ISO-8859-1解析编码方式存到r->headers_out.charset
static ngx_int_t
ngx_http_upstream_copy_content_type(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    u_char  *p, *last;

    r->headers_out.content_type_len = h->value.len;
    r->headers_out.content_type = h->value;
    r->headers_out.content_type_lowcase = NULL;

    for (p = h->value.data; *p; p++) {

        if (*p != ';') {
            continue;
        }

        last = p;

        while (*++p == ' ') { /* void */ }

        if (*p == '\0') {
            return NGX_OK;
        }

        if (ngx_strncasecmp(p, (u_char *) "charset=", 8) != 0) {
            continue;
        }

        p += 8;

        r->headers_out.content_type_len = last - h->value.data;

        if (*p == '"') {
            p++;
        }

        last = h->value.data + h->value.len;

        if (*(last - 1) == '"') {
            last--;
        }

        r->headers_out.charset.len = last - p;
        r->headers_out.charset.data = p;

        return NGX_OK;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_copy_last_modified(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  *ho;

    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    *ho = *h;

    r->headers_out.last_modified = ho;

#if (NGX_HTTP_CACHE)

    if (r->upstream->cacheable) {
        r->headers_out.last_modified_time =
                                    r->upstream->headers_in.last_modified_time;
    }

#endif

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_rewrite_location(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_int_t         rc;
    ngx_table_elt_t  *ho;

    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    *ho = *h;

    if (r->upstream->rewrite_redirect) {
        rc = r->upstream->rewrite_redirect(r, ho, 0);

        if (rc == NGX_DECLINED) {
            return NGX_OK;
        }

        if (rc == NGX_OK) {
            r->headers_out.location = ho;

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "rewritten location: \"%V\"", &ho->value);
        }

        return rc;
    }

    if (ho->value.data[0] != '/') {
        r->headers_out.location = ho;
    }

    /*
     * we do not set r->headers_out.location here to avoid the handling
     * the local redirects without a host name by ngx_http_header_filter()
     */

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_rewrite_refresh(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    u_char           *p;
    ngx_int_t         rc;
    ngx_table_elt_t  *ho;

    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    *ho = *h;

    if (r->upstream->rewrite_redirect) {

        p = ngx_strcasestrn(ho->value.data, "url=", 4 - 1);

        if (p) {
            rc = r->upstream->rewrite_redirect(r, ho, p + 4 - ho->value.data);

        } else {
            return NGX_OK;
        }

        if (rc == NGX_DECLINED) {
            return NGX_OK;
        }

        if (rc == NGX_OK) {
            r->headers_out.refresh = ho;

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "rewritten refresh: \"%V\"", &ho->value);
        }

        return rc;
    }

    r->headers_out.refresh = ho;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_rewrite_set_cookie(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_int_t         rc;
    ngx_table_elt_t  *ho;

    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    *ho = *h;

    if (r->upstream->rewrite_cookie) {
        rc = r->upstream->rewrite_cookie(r, ho);

        if (rc == NGX_DECLINED) {
            return NGX_OK;
        }

#if (NGX_DEBUG)
        if (rc == NGX_OK) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "rewritten cookie: \"%V\"", &ho->value);
        }
#endif

        return rc;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_copy_allow_ranges(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_table_elt_t  *ho;

    if (r->upstream->conf->force_ranges) {
        return NGX_OK;
    }

#if (NGX_HTTP_CACHE)

    if (r->cached) {
        r->allow_ranges = 1;
        return NGX_OK;
    }

    if (r->upstream->cacheable) {
        r->allow_ranges = 1;
        r->single_range = 1;
        return NGX_OK;
    }

#endif

    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    *ho = *h;

    r->headers_out.accept_ranges = ho;

    return NGX_OK;
}


#if (NGX_HTTP_GZIP)

static ngx_int_t
ngx_http_upstream_copy_content_encoding(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_table_elt_t  *ho;

    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    *ho = *h;

    r->headers_out.content_encoding = ho;

    return NGX_OK;
}

#endif


static ngx_int_t
ngx_http_upstream_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var, *v;

    for (v = ngx_http_upstream_vars; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_addr_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                     *p;
    size_t                      len;
    ngx_uint_t                  i;
    ngx_http_upstream_state_t  *state;

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    if (r->upstream_states == NULL || r->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    len = 0;
    state = r->upstream_states->elts;

    for (i = 0; i < r->upstream_states->nelts; i++) {
        if (state[i].peer) {
            len += state[i].peer->len + 2;

        } else {
            len += 3;
        }
    }

    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;

    i = 0;

    for ( ;; ) {
        if (state[i].peer) {
            p = ngx_cpymem(p, state[i].peer->data, state[i].peer->len);
        }

        if (++i == r->upstream_states->nelts) {
            break;
        }

        if (state[i].peer) {
            *p++ = ',';
            *p++ = ' ';

        } else {
            *p++ = ' ';
            *p++ = ':';
            *p++ = ' ';

            if (++i == r->upstream_states->nelts) {
                break;
            }

            continue;
        }
    }

    v->len = p - v->data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_status_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                     *p;
    size_t                      len;
    ngx_uint_t                  i;
    ngx_http_upstream_state_t  *state;

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    if (r->upstream_states == NULL || r->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    len = r->upstream_states->nelts * (3 + 2);

    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;

    i = 0;
    state = r->upstream_states->elts;

    for ( ;; ) {
        if (state[i].status) {
            p = ngx_sprintf(p, "%ui", state[i].status);

        } else {
            *p++ = '-';
        }

        if (++i == r->upstream_states->nelts) {
            break;
        }

        if (state[i].peer) {
            *p++ = ',';
            *p++ = ' ';

        } else {
            *p++ = ' ';
            *p++ = ':';
            *p++ = ' ';

            if (++i == r->upstream_states->nelts) {
                break;
            }

            continue;
        }
    }

    v->len = p - v->data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_response_time_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                     *p;
    size_t                      len;
    ngx_uint_t                  i;
    ngx_msec_int_t              ms;
    ngx_http_upstream_state_t  *state;

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    if (r->upstream_states == NULL || r->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    len = r->upstream_states->nelts * (NGX_TIME_T_LEN + 4 + 2);

    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;

    i = 0;
    state = r->upstream_states->elts;

    for ( ;; ) {
        if (state[i].status) {

            if (data == 1 && state[i].header_time != (ngx_msec_t) -1) {
                ms = state[i].header_time;

            } else if (data == 2 && state[i].connect_time != (ngx_msec_t) -1) {
                ms = state[i].connect_time;

            } else {
                ms = state[i].response_time;
            }

            ms = ngx_max(ms, 0);
            p = ngx_sprintf(p, "%T.%03M", (time_t) ms / 1000, ms % 1000);

        } else {
            *p++ = '-';
        }

        if (++i == r->upstream_states->nelts) {
            break;
        }

        if (state[i].peer) {
            *p++ = ',';
            *p++ = ' ';

        } else {
            *p++ = ' ';
            *p++ = ':';
            *p++ = ' ';

            if (++i == r->upstream_states->nelts) {
                break;
            }

            continue;
        }
    }

    v->len = p - v->data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_response_length_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                     *p;
    size_t                      len;
    ngx_uint_t                  i;
    ngx_http_upstream_state_t  *state;

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    if (r->upstream_states == NULL || r->upstream_states->nelts == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    len = r->upstream_states->nelts * (NGX_OFF_T_LEN + 2);

    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;

    i = 0;
    state = r->upstream_states->elts;

    for ( ;; ) {
        p = ngx_sprintf(p, "%O", state[i].response_length);

        if (++i == r->upstream_states->nelts) {
            break;
        }

        if (state[i].peer) {
            *p++ = ',';
            *p++ = ' ';

        } else {
            *p++ = ' ';
            *p++ = ':';
            *p++ = ' ';

            if (++i == r->upstream_states->nelts) {
                break;
            }

            continue;
        }
    }

    v->len = p - v->data;

    return NGX_OK;
}


ngx_int_t
ngx_http_upstream_header_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    if (r->upstream == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    return ngx_http_variable_unknown_header(v, (ngx_str_t *) data,
                                         &r->upstream->headers_in.headers.part,
                                         sizeof("upstream_http_") - 1);
}


ngx_int_t
ngx_http_upstream_cookie_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_str_t  *name = (ngx_str_t *) data;

    ngx_str_t   cookie, s;

    if (r->upstream == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    s.len = name->len - (sizeof("upstream_cookie_") - 1);
    s.data = name->data + sizeof("upstream_cookie_") - 1;

    if (ngx_http_parse_set_cookie_lines(&r->upstream->headers_in.cookies,
                                        &s, &cookie)
        == NGX_DECLINED)
    {
        v->not_found = 1;
        return NGX_OK;
    }

    v->len = cookie.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = cookie.data;

    return NGX_OK;
}


#if (NGX_HTTP_CACHE)

ngx_int_t
ngx_http_upstream_cache_status(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_uint_t  n;

    if (r->upstream == NULL || r->upstream->cache_status == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    n = r->upstream->cache_status - 1;

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->len = ngx_http_cache_status[n].len;
    v->data = ngx_http_cache_status[n].data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_cache_last_modified(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    if (r->upstream == NULL
        || !r->upstream->conf->cache_revalidate
        || r->upstream->cache_status != NGX_HTTP_CACHE_EXPIRED
        || r->cache->last_modified == -1)
    {
        v->not_found = 1;
        return NGX_OK;
    }

    p = ngx_pnalloc(r->pool, sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->len = ngx_http_time(p, r->cache->last_modified) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_cache_etag(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    if (r->upstream == NULL
        || !r->upstream->conf->cache_revalidate
        || r->upstream->cache_status != NGX_HTTP_CACHE_EXPIRED
        || r->cache->etag.len == 0)
    {
        v->not_found = 1;
        return NGX_OK;
    }

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->len = r->cache->etag.len;
    v->data = r->cache->etag.data;

    return NGX_OK;
}

#endif

/*
upstream backend {
    server backend1.example.com weight=5;
    server backend2.example.com:8080;
    server unix:/tmp/backend3;
}

server {
    location / {
        proxy_pass http://backend;
    }
}

max_fails=number

  设置在fail_timeout参数设置的时间内最大失败次数，如果在这个时间内，所有针对该服务器的请求
  都失败了，那么认为该服务器会被认为是停机了，停机时间是fail_timeout设置的时间。默认情况下，
  不成功连接数被设置为1。被设置为零则表示不进行链接数统计。那些连接被认为是不成功的可以通过
  proxy_next_upstream, fastcgi_next_upstream，和memcached_next_upstream指令配置。http_404
  状态不会被认为是不成功的尝试。

fail_time=time
  设置 多长时间内失败次数达到最大失败次数会被认为服务器停机了服务器会被认为停机的时间长度 默认情况下，超时时间被设置为10S

*/
static char *
ngx_http_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{//当碰到upstream{}指令的时候调用这里。
    char                          *rv;
    void                          *mconf;
    ngx_str_t                     *value;
    ngx_url_t                      u;
    ngx_uint_t                     m;
    ngx_conf_t                     pcf;
    ngx_http_module_t             *module;
    ngx_http_conf_ctx_t           *ctx, *http_ctx;
    ngx_http_upstream_srv_conf_t  *uscf;

    ngx_memzero(&u, sizeof(ngx_url_t));

    value = cf->args->elts;
    u.host = value[1]; //upstream backend { }中的backend
    u.no_resolve = 1;
    u.no_port = 1;

    //下面将u代表的数据设置到umcf->upstreams里面去。然后返回对应的upstream{}结构数据指针。
    uscf = ngx_http_upstream_add(cf, &u, NGX_HTTP_UPSTREAM_CREATE
                                         |NGX_HTTP_UPSTREAM_WEIGHT
                                         |NGX_HTTP_UPSTREAM_MAX_FAILS
                                         |NGX_HTTP_UPSTREAM_FAIL_TIMEOUT
                                         |NGX_HTTP_UPSTREAM_DOWN
                                         |NGX_HTTP_UPSTREAM_BACKUP);
    if (uscf == NULL) {
        return NGX_CONF_ERROR;
    }


    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    http_ctx = cf->ctx;
    ctx->main_conf = http_ctx->main_conf; //获取该upstream xxx{}所处的http{}

    /* the upstream{}'s srv_conf */

    ctx->srv_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->srv_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    ctx->srv_conf[ngx_http_upstream_module.ctx_index] = uscf;

    uscf->srv_conf = ctx->srv_conf;


    /* the upstream{}'s loc_conf */
    ctx->loc_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->loc_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    //该upstream{}中可以配置所有的loc级别模块的配置信息，因此为每个模块创建对应的存储空间
    for (m = 0; ngx_modules[m]; m++) {
        if (ngx_modules[m]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = ngx_modules[m]->ctx;

        if (module->create_srv_conf) {
            mconf = module->create_srv_conf(cf);
            if (mconf == NULL) {
                return NGX_CONF_ERROR;
            }

            ctx->srv_conf[ngx_modules[m]->ctx_index] = mconf;
        }

        if (module->create_loc_conf) {
            mconf = module->create_loc_conf(cf);
            if (mconf == NULL) {
                return NGX_CONF_ERROR;
            }

            ctx->loc_conf[ngx_modules[m]->ctx_index] = mconf;
        }
    }

    uscf->servers = ngx_array_create(cf->pool, 4,
                                     sizeof(ngx_http_upstream_server_t));
    if (uscf->servers == NULL) {
        return NGX_CONF_ERROR;
    }


    /* parse inside upstream{} */

    pcf = *cf;   //保存upstream{}所处的ctx
    cf->ctx = ctx;//临时切换ctx，进入upstream{}块中进行解析。
    cf->cmd_type = NGX_HTTP_UPS_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pcf; //upstream{}内部配置解析完毕后，恢复到之前的cf

    if (rv != NGX_CONF_OK) {
        return rv;
    }

    if (uscf->servers->nelts == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "no servers are inside upstream");
        return NGX_CONF_ERROR;
    }

    return rv;
}

//server   127.0.0.1:8080          max_fails=3  fail_timeout=30s;
static char *
ngx_http_upstream_server(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_upstream_srv_conf_t  *uscf = conf;

    time_t                       fail_timeout;
    ngx_str_t                   *value, s;
    ngx_url_t                    u;
    ngx_int_t                    weight, max_fails;
    ngx_uint_t                   i;
    ngx_http_upstream_server_t  *us;

    us = ngx_array_push(uscf->servers);
    if (us == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memzero(us, sizeof(ngx_http_upstream_server_t));

    value = cf->args->elts;

    weight = 1;
    max_fails = 1;
    fail_timeout = 10;

    for (i = 2; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "weight=", 7) == 0) {

            if (!(uscf->flags & NGX_HTTP_UPSTREAM_WEIGHT)) {
                goto not_supported;
            }

            weight = ngx_atoi(&value[i].data[7], value[i].len - 7);

            if (weight == NGX_ERROR || weight == 0) {
                goto invalid;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "max_fails=", 10) == 0) {

            if (!(uscf->flags & NGX_HTTP_UPSTREAM_MAX_FAILS)) {
                goto not_supported;
            }

            max_fails = ngx_atoi(&value[i].data[10], value[i].len - 10);

            if (max_fails == NGX_ERROR) {
                goto invalid;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "fail_timeout=", 13) == 0) {

            if (!(uscf->flags & NGX_HTTP_UPSTREAM_FAIL_TIMEOUT)) {
                goto not_supported;
            }

            s.len = value[i].len - 13;
            s.data = &value[i].data[13];

            fail_timeout = ngx_parse_time(&s, 1);

            if (fail_timeout == (time_t) NGX_ERROR) {
                goto invalid;
            }

            continue;
        }

        if (ngx_strcmp(value[i].data, "backup") == 0) {

            if (!(uscf->flags & NGX_HTTP_UPSTREAM_BACKUP)) {
                goto not_supported;
            }

            us->backup = 1;

            continue;
        }

        if (ngx_strcmp(value[i].data, "down") == 0) {

            if (!(uscf->flags & NGX_HTTP_UPSTREAM_DOWN)) {
                goto not_supported;
            }

            us->down = 1;

            continue;
        }

        goto invalid;
    }

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url = value[1];
    u.default_port = 80;

    if (ngx_parse_url(cf->pool, &u) != NGX_OK) {
        if (u.err) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "%s in upstream \"%V\"", u.err, &u.url);
        }

        return NGX_CONF_ERROR;
    }

    us->name = u.url;
    us->addrs = u.addrs;
    us->naddrs = u.naddrs;
    us->weight = weight;
    us->max_fails = max_fails;
    us->fail_timeout = fail_timeout;

    return NGX_CONF_OK;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\"", &value[i]);

    return NGX_CONF_ERROR;

not_supported:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "balancing method does not support parameter \"%V\"",
                       &value[i]);

    return NGX_CONF_ERROR;
}


ngx_http_upstream_srv_conf_t *
ngx_http_upstream_add(ngx_conf_t *cf, ngx_url_t *u, ngx_uint_t flags)
{
    ngx_uint_t                      i;
    ngx_http_upstream_server_t     *us;
    ngx_http_upstream_srv_conf_t   *uscf, **uscfp;
    ngx_http_upstream_main_conf_t  *umcf;

    if (!(flags & NGX_HTTP_UPSTREAM_CREATE)) {

        if (ngx_parse_url(cf->pool, u) != NGX_OK) { //解析uri，如果uri是IP:PORT形式则获取他们，如果是域名www.xxx.com形式，则解析域名
            if (u->err) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "%s in upstream \"%V\"", u->err, &u->url);
            }

            return NULL;
        }
    }

    umcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_upstream_module);

    uscfp = umcf->upstreams.elts;

    //遍历当前的upstream，如果有重复的，则比较其相关的字段，并打印日志。如果找到相同的，则返回对应指针。没找到则在后面创建
    for (i = 0; i < umcf->upstreams.nelts; i++) {

        if (uscfp[i]->host.len != u->host.len
            || ngx_strncasecmp(uscfp[i]->host.data, u->host.data, u->host.len)
               != 0)
        {
            continue;
        }

        if ((flags & NGX_HTTP_UPSTREAM_CREATE)
             && (uscfp[i]->flags & NGX_HTTP_UPSTREAM_CREATE))
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "duplicate upstream \"%V\"", &u->host);
            return NULL;
        }

        if ((uscfp[i]->flags & NGX_HTTP_UPSTREAM_CREATE) && !u->no_port) {
            ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                               "upstream \"%V\" may not have port %d",
                               &u->host, u->port);
            return NULL;
        }

        if ((flags & NGX_HTTP_UPSTREAM_CREATE) && !uscfp[i]->no_port) {
            ngx_log_error(NGX_LOG_WARN, cf->log, 0,
                          "upstream \"%V\" may not have port %d in %s:%ui",
                          &u->host, uscfp[i]->port,
                          uscfp[i]->file_name, uscfp[i]->line);
            return NULL;
        }

        if (uscfp[i]->port && u->port
            && uscfp[i]->port != u->port)
        {
            continue;
        }

        if (uscfp[i]->default_port && u->default_port
            && uscfp[i]->default_port != u->default_port)
        {
            continue;
        }

        if (flags & NGX_HTTP_UPSTREAM_CREATE) {
            uscfp[i]->flags = flags;
        }

        return uscfp[i];//找到相同的配置数据了，直接返回它的指针。
    }

    uscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_srv_conf_t));
    if (uscf == NULL) {
        return NULL;
    }

    uscf->flags = flags;
    uscf->host = u->host;
    uscf->file_name = cf->conf_file->file.name.data; //配置文件名称
    uscf->line = cf->conf_file->line;
    uscf->port = u->port;
    uscf->default_port = u->default_port;
    uscf->no_port = u->no_port;

    //比如: server xx.xx.xx.xx:xx weight=2 max_fails=3;  刚开始，ngx_http_upstream会调用本函数。但是其naddres=0.
    if (u->naddrs == 1 && (u->port || u->family == AF_UNIX)) {
        uscf->servers = ngx_array_create(cf->pool, 1,
                                         sizeof(ngx_http_upstream_server_t));
        if (uscf->servers == NULL) {
            return NULL;
        }

        us = ngx_array_push(uscf->servers);//记录本upstream{}块的所有server指令。
        if (us == NULL) {
            return NULL;
        }

        ngx_memzero(us, sizeof(ngx_http_upstream_server_t));

        us->addrs = u->addrs;
        us->naddrs = 1;
    }

    uscfp = ngx_array_push(&umcf->upstreams);
    if (uscfp == NULL) {
        return NULL;
    }

    *uscfp = uscf;

    return uscf;
}


char *
ngx_http_upstream_bind_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    char  *p = conf;

    ngx_int_t                           rc;
    ngx_str_t                          *value;
    ngx_http_complex_value_t            cv;
    ngx_http_upstream_local_t         **plocal, *local;
    ngx_http_compile_complex_value_t    ccv;

    plocal = (ngx_http_upstream_local_t **) (p + cmd->offset);

    if (*plocal != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "off") == 0) {
        *plocal = NULL;
        return NGX_CONF_OK;
    }

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &cv;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    local = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_local_t));
    if (local == NULL) {
        return NGX_CONF_ERROR;
    }

    *plocal = local;

    if (cv.lengths) {
        local->value = ngx_palloc(cf->pool, sizeof(ngx_http_complex_value_t));
        if (local->value == NULL) {
            return NGX_CONF_ERROR;
        }

        *local->value = cv;

        return NGX_CONF_OK;
    }

    local->addr = ngx_palloc(cf->pool, sizeof(ngx_addr_t));
    if (local->addr == NULL) {
        return NGX_CONF_ERROR;
    }

    rc = ngx_parse_addr(cf->pool, local->addr, value[1].data, value[1].len);

    switch (rc) {
    case NGX_OK:
        local->addr->name = value[1];
        return NGX_CONF_OK;

    case NGX_DECLINED:
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid address \"%V\"", &value[1]);
        /* fall through */

    default:
        return NGX_CONF_ERROR;
    }
}


static ngx_addr_t *
ngx_http_upstream_get_local(ngx_http_request_t *r,
    ngx_http_upstream_local_t *local)
{
    ngx_int_t    rc;
    ngx_str_t    val;
    ngx_addr_t  *addr;

    if (local == NULL) {
        return NULL;
    }

    if (local->value == NULL) {
        return local->addr;
    }

    if (ngx_http_complex_value(r, local->value, &val) != NGX_OK) {
        return NULL;
    }

    if (val.len == 0) {
        return NULL;
    }

    addr = ngx_palloc(r->pool, sizeof(ngx_addr_t));
    if (addr == NULL) {
        return NULL;
    }

    rc = ngx_parse_addr(r->pool, addr, val.data, val.len);

    switch (rc) {
    case NGX_OK:
        addr->name = val;
        return addr;

    case NGX_DECLINED:
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "invalid local address \"%V\"", &val);
        /* fall through */

    default:
        return NULL;
    }
}

//fastcgi_param  Params数据包，用于传递执行页面所需要的参数和环境变量。
char *
ngx_http_upstream_param_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf) //uboot.din
{
    char  *p = conf;

    ngx_str_t                   *value;
    ngx_array_t                **a;
    ngx_http_upstream_param_t   *param;

    //fastcgi_param设置的传送到FastCGI服务器的相关参数都添加到该数组中，见ngx_http_upstream_param_set_slot
    a = (ngx_array_t **) (p + cmd->offset);//ngx_http_fastcgi_loc_conf_t->params_source

    if (*a == NULL) {
        *a = ngx_array_create(cf->pool, 4, sizeof(ngx_http_upstream_param_t));
        if (*a == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    param = ngx_array_push(*a);
    if (param == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    param->key = value[1];
    param->value = value[2];
    param->skip_empty = 0;

    if (cf->args->nelts == 4) {
        if (ngx_strcmp(value[3].data, "if_not_empty") != 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid parameter \"%V\"", &value[3]);
            return NGX_CONF_ERROR;
        }

        param->skip_empty = 1; //和ngx_http_fastcgi_init_params配合阅读，如果设置了该值，并且value部分为0，则直接不使用此变量
    }

    return NGX_CONF_OK;
}


ngx_int_t
ngx_http_upstream_hide_headers_hash(ngx_conf_t *cf,
    ngx_http_upstream_conf_t *conf, ngx_http_upstream_conf_t *prev,
    ngx_str_t *default_hide_headers, ngx_hash_init_t *hash)
{
    ngx_str_t       *h;
    ngx_uint_t       i, j;
    ngx_array_t      hide_headers;
    ngx_hash_key_t  *hk;

    if (conf->hide_headers == NGX_CONF_UNSET_PTR
        && conf->pass_headers == NGX_CONF_UNSET_PTR)
    {
        conf->hide_headers = prev->hide_headers;
        conf->pass_headers = prev->pass_headers;

        conf->hide_headers_hash = prev->hide_headers_hash;

        if (conf->hide_headers_hash.buckets
#if (NGX_HTTP_CACHE)
            && ((conf->cache == 0) == (prev->cache == 0))
#endif
           )
        {
            return NGX_OK;
        }

    } else {
        if (conf->hide_headers == NGX_CONF_UNSET_PTR) {
            conf->hide_headers = prev->hide_headers;
        }

        if (conf->pass_headers == NGX_CONF_UNSET_PTR) {
            conf->pass_headers = prev->pass_headers;
        }
    }

    if (ngx_array_init(&hide_headers, cf->temp_pool, 4, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    for (h = default_hide_headers; h->len; h++) {
        hk = ngx_array_push(&hide_headers);
        if (hk == NULL) {
            return NGX_ERROR;
        }

        hk->key = *h;
        hk->key_hash = ngx_hash_key_lc(h->data, h->len);
        hk->value = (void *) 1;
    }

    if (conf->hide_headers != NGX_CONF_UNSET_PTR) {

        h = conf->hide_headers->elts;

        for (i = 0; i < conf->hide_headers->nelts; i++) {

            hk = hide_headers.elts;

            for (j = 0; j < hide_headers.nelts; j++) {
                if (ngx_strcasecmp(h[i].data, hk[j].key.data) == 0) {
                    goto exist;
                }
            }

            hk = ngx_array_push(&hide_headers);
            if (hk == NULL) {
                return NGX_ERROR;
            }

            hk->key = h[i];
            hk->key_hash = ngx_hash_key_lc(h[i].data, h[i].len);
            hk->value = (void *) 1;

        exist:

            continue;
        }
    }

    if (conf->pass_headers != NGX_CONF_UNSET_PTR) {

        h = conf->pass_headers->elts;
        hk = hide_headers.elts;

        for (i = 0; i < conf->pass_headers->nelts; i++) {
            for (j = 0; j < hide_headers.nelts; j++) {

                if (hk[j].key.data == NULL) {
                    continue;
                }

                if (ngx_strcasecmp(h[i].data, hk[j].key.data) == 0) {
                    hk[j].key.data = NULL;
                    break;
                }
            }
        }
    }

    hash->hash = &conf->hide_headers_hash;
    hash->key = ngx_hash_key_lc;
    hash->pool = cf->pool;
    hash->temp_pool = NULL;

    return ngx_hash_init(hash, hide_headers.elts, hide_headers.nelts);
}


static void *
ngx_http_upstream_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_upstream_main_conf_t  *umcf;

    umcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_main_conf_t));
    if (umcf == NULL) {
        return NULL;
    }

    if (ngx_array_init(&umcf->upstreams, cf->pool, 4,
                       sizeof(ngx_http_upstream_srv_conf_t *))
        != NGX_OK)
    {
        return NULL;
    }

    return umcf;
}


static char *
ngx_http_upstream_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_upstream_main_conf_t  *umcf = conf;

    ngx_uint_t                      i;
    ngx_array_t                     headers_in;
    ngx_hash_key_t                 *hk;
    ngx_hash_init_t                 hash;
    ngx_http_upstream_init_pt       init;
    ngx_http_upstream_header_t     *header;
    ngx_http_upstream_srv_conf_t  **uscfp;

    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {

        init = uscfp[i]->peer.init_upstream ? uscfp[i]->peer.init_upstream:
                                            ngx_http_upstream_init_round_robin;

        if (init(cf, uscfp[i]) != NGX_OK) { //执行init_upstream
            return NGX_CONF_ERROR;
        }
    }


    /* upstream_headers_in_hash */

    if (ngx_array_init(&headers_in, cf->temp_pool, 32, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    for (header = ngx_http_upstream_headers_in; header->name.len; header++) {
        hk = ngx_array_push(&headers_in);
        if (hk == NULL) {
            return NGX_CONF_ERROR;
        }

        hk->key = header->name;
        hk->key_hash = ngx_hash_key_lc(header->name.data, header->name.len);
        hk->value = header;
    }

    hash.hash = &umcf->headers_in_hash;
    hash.key = ngx_hash_key_lc;
    hash.max_size = 512;
    hash.bucket_size = ngx_align(64, ngx_cacheline_size);
    hash.name = "upstream_headers_in_hash";
    hash.pool = cf->pool;
    hash.temp_pool = NULL;

    if (ngx_hash_init(&hash, headers_in.elts, headers_in.nelts) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

