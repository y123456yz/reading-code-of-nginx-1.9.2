
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_log.h>

//(proxy_cache_path)等配置走到这里
typedef struct { //赋值见ngx_http_file_cache_set_slot
    //u->caches = &ngx_http_proxy_main_conf_t->caches;
    ngx_array_t                    caches;  /* ngx_http_file_cache_t * */ //ngx_http_file_cache_t成员，最终被赋值给u->caches
} ngx_http_proxy_main_conf_t;


typedef struct ngx_http_proxy_rewrite_s  ngx_http_proxy_rewrite_t;

typedef ngx_int_t (*ngx_http_proxy_rewrite_pt)(ngx_http_request_t *r,
    ngx_table_elt_t *h, size_t prefix, size_t len,
    ngx_http_proxy_rewrite_t *pr);

/*
//如果没有配置proxy_redirect或者配置为proxy_redirect default,则pattern=plcf->url replacement=location(location /xxxx {}中的/xxx),
见ngx_http_proxy_redirect和ngx_http_proxy_merge_loc_conf
*/
struct ngx_http_proxy_rewrite_s { //ngx_http_proxy_redirect中创建该结构空间
    ngx_http_proxy_rewrite_pt      handler; //ngx_http_proxy_rewrite_complex_handler

    //解析proxy_redirect   http://localhost:8000/    http://$host:$server_port/;中的http://localhost:8000/
    union {
        ngx_http_complex_value_t   complex; 
#if (NGX_PCRE)
        ngx_http_regex_t          *regex;
#endif
    } pattern; 
    
    //解析proxy_redirect   http://localhost:8000/    http://$host:$server_port/;中的 http://$host:$server_port/
    ngx_http_complex_value_t       replacement;
};

/*
  proxy_pass  http://10.10.0.103:8080/tttxxsss; 
  浏览器输入:http://10.2.13.167/proxy1111/yangtest
  实际上到后端的uri会变为/tttxxsss/yangtest
  plcf->location:/proxy1111, ctx->vars.uri:/tttxxsss,  r->uri:/proxy1111/yangtest, r->args:, urilen:19
 */
typedef struct {
    //proxy_pass  http://10.10.0.103:8080/tttxx;中的http://10.10.0.103:8080
    ngx_str_t                      key_start; // 初始状态plcf->vars.key_start = plcf->vars.schema;
    /*  "http://" 或者 "https://"  */ //指向的是uri，当时schema->len="http://" 或者 "https://"字符串长度7或者8，参考ngx_http_proxy_pass
    ngx_str_t                      schema; ////proxy_pass  http://10.10.0.103:8080/tttxx;中的http://
    
    ngx_str_t                      host_header;//proxy_pass  http://10.10.0.103:8080/tttxx; 中的10.10.0.103:8080
    ngx_str_t                      port; //"80"或者"443"，见ngx_http_proxy_set_vars
    //proxy_pass  http://10.10.0.103:8080/tttxx; 中的/tttxx         uri不带http://http://10.10.0.103:8080 url带有http://10.10.0.103:8080
    ngx_str_t                      uri; //ngx_http_proxy_set_vars里面赋值   uri是proxy_pass  http://10.10.0.103:8080/xxx中的/xxx，如果
    //proxy_pass  http://10.10.0.103:8080则uri长度为0，没有uri
} ngx_http_proxy_vars_t;


typedef struct {
    ngx_array_t                   *flushes;
     /* 
    把proxy_set_header与ngx_http_proxy_headers合在一起，然后把其中的key:value字符串和长度信息添加到headers->lengths和headers->values中
    把ngx_http_proxy_cache_headers合在一起，然后把其中的key:value字符串和长度信息添加到headers->lengths和headers->values中
     */
    //1. 里面存储的是ngx_http_proxy_headers和proxy_set_header设置的头部信息头添加到该lengths和values 存入ngx_http_proxy_loc_conf_t->headers
    //2. 里面存储的是ngx_http_proxy_cache_headers设置的头部信息头添加到该lengths和values  存入ngx_http_proxy_loc_conf_t->headers_cache
    ngx_array_t                   *lengths; //创建空间和赋值见ngx_http_proxy_init_headers
    ngx_array_t                   *values;  //创建空间和赋值见ngx_http_proxy_init_headers
    ngx_hash_t                     hash;
} ngx_http_proxy_headers_t;


typedef struct {
    ngx_http_upstream_conf_t       upstream;

    /* 下面这几个和proxy_set_body XXX配置相关 proxy_set_body设置包体后，就不会传送客户端发送来的数据到后端服务器(只传送proxy_set_body设置的包体)，
    如果没有设置，则会发送客户端包体数据到后端， 参考ngx_http_proxy_create_request */
    ngx_array_t                   *body_flushes; //从proxy_set_body XXX配置中获取，见ngx_http_proxy_merge_loc_conf
    ngx_array_t                   *body_lengths; //从proxy_set_body XXX配置中获取，见ngx_http_proxy_merge_loc_conf
    ngx_array_t                   *body_values;  //从proxy_set_body XXX配置中获取，见ngx_http_proxy_merge_loc_conf
    ngx_str_t                      body_source; //proxy_set_body XXX配置中的xxx

    //数据来源是ngx_http_proxy_headers  proxy_set_header，见ngx_http_proxy_init_headers
    ngx_http_proxy_headers_t       headers;//创建空间和赋值见ngx_http_proxy_merge_loc_conf
#if (NGX_HTTP_CACHE)
    //数据来源是ngx_http_proxy_cache_headers，见ngx_http_proxy_init_headers
    ngx_http_proxy_headers_t       headers_cache;//创建空间和赋值见ngx_http_proxy_merge_loc_conf
#endif
    //通过proxy_set_header Host $proxy_host;设置并添加到该数组中
    ngx_array_t                   *headers_source; //创建空间和赋值见ngx_http_proxy_init_headers

    //解析uri中的变量的时候会用到下面两个，见ngx_http_proxy_pass  proxy_pass xxx中如果有变量，下面两个就不为空
    ngx_array_t                   *proxy_lengths;
    ngx_array_t                   *proxy_values;
    //proxy_redirect [ default|off|redirect replacement ]; 
    //成员类型ngx_http_proxy_rewrite_t
    ngx_array_t                   *redirects; //和配置proxy_redirect相关，见ngx_http_proxy_redirect创建空间
    ngx_array_t                   *cookie_domains;
    ngx_array_t                   *cookie_paths;

    /* 此配置项表示转发时的协议方法名。例如设置为：proxy_method POST;那么客户端发来的GET请求在转发时方法名也会改为POST */
    ngx_str_t                      method;
    ngx_str_t                      location; //当前location的名字 location xxx {} 中的xxx

    //proxy_pass  http://10.10.0.103:8080/tttxx; 中的http://10.10.0.103:8080/tttxx
    ngx_str_t                      url; //proxy_pass url名字，见ngx_http_proxy_pass

#if (NGX_HTTP_CACHE)
    ngx_http_complex_value_t       cache_key;//proxy_cache_key为缓存建立索引时使用的关键字；见ngx_http_proxy_cache_key
#endif

    ngx_http_proxy_vars_t          vars;//里面保存proxy_pass uri中的uri信息
    //默认1
    ngx_flag_t                     redirect;//和配置proxy_redirect相关，见ngx_http_proxy_redirect
    //proxy_http_version设置，
    ngx_uint_t                     http_version; //到后端服务器采用的http协议版本，默认NGX_HTTP_VERSION_10

    ngx_uint_t                     headers_hash_max_size;
    ngx_uint_t                     headers_hash_bucket_size;

#if (NGX_HTTP_SSL)
    ngx_uint_t                     ssl;
    ngx_uint_t                     ssl_protocols;
    ngx_str_t                      ssl_ciphers;
    ngx_uint_t                     ssl_verify_depth;
    ngx_str_t                      ssl_trusted_certificate;
    ngx_str_t                      ssl_crl;
    ngx_str_t                      ssl_certificate;
    ngx_str_t                      ssl_certificate_key;
    ngx_array_t                   *ssl_passwords;
#endif
} ngx_http_proxy_loc_conf_t;


typedef struct { //ngx_http_proxy_handler中创建空间
    ngx_http_status_t              status; //HTTP/1.1 200 OK 赋值见ngx_http_proxy_process_status_line
    ngx_http_chunked_t             chunked;
    ngx_http_proxy_vars_t          vars;
    off_t                          internal_body_length; //长度为客户端发送过来的包体长度+proxy_set_body设置的字符包体长度和

    ngx_chain_t                   *free;
    ngx_chain_t                   *busy;

    unsigned                       head:1; //到后端的请求行请求时"HEAD"
    unsigned                       internal_chunked:1;
    unsigned                       header_sent:1;
} ngx_http_proxy_ctx_t; //proxy模块的各种上下文信息，例如读取后端数据的时候会有多次交换，这里面会记录状态信息ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);


static ngx_int_t ngx_http_proxy_eval(ngx_http_request_t *r,
    ngx_http_proxy_ctx_t *ctx, ngx_http_proxy_loc_conf_t *plcf);
#if (NGX_HTTP_CACHE)
static ngx_int_t ngx_http_proxy_create_key(ngx_http_request_t *r);
#endif
static ngx_int_t ngx_http_proxy_create_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_proxy_reinit_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_proxy_body_output_filter(void *data, ngx_chain_t *in);
static ngx_int_t ngx_http_proxy_process_status_line(ngx_http_request_t *r);
static ngx_int_t ngx_http_proxy_process_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_proxy_input_filter_init(void *data);
static ngx_int_t ngx_http_proxy_copy_filter(ngx_event_pipe_t *p,
    ngx_buf_t *buf);
static ngx_int_t ngx_http_proxy_chunked_filter(ngx_event_pipe_t *p,
    ngx_buf_t *buf);
static ngx_int_t ngx_http_proxy_non_buffered_copy_filter(void *data,
    ssize_t bytes);
static ngx_int_t ngx_http_proxy_non_buffered_chunked_filter(void *data,
    ssize_t bytes);
static void ngx_http_proxy_abort_request(ngx_http_request_t *r);
static void ngx_http_proxy_finalize_request(ngx_http_request_t *r,
    ngx_int_t rc);

static ngx_int_t ngx_http_proxy_host_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_proxy_port_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t
    ngx_http_proxy_add_x_forwarded_for_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t
    ngx_http_proxy_internal_body_length_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_proxy_internal_chunked_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_proxy_rewrite_redirect(ngx_http_request_t *r,
    ngx_table_elt_t *h, size_t prefix);
static ngx_int_t ngx_http_proxy_rewrite_cookie(ngx_http_request_t *r,
    ngx_table_elt_t *h);
static ngx_int_t ngx_http_proxy_rewrite_cookie_value(ngx_http_request_t *r,
    ngx_table_elt_t *h, u_char *value, ngx_array_t *rewrites);
static ngx_int_t ngx_http_proxy_rewrite(ngx_http_request_t *r,
    ngx_table_elt_t *h, size_t prefix, size_t len, ngx_str_t *replacement);

static ngx_int_t ngx_http_proxy_add_variables(ngx_conf_t *cf);
static void *ngx_http_proxy_create_main_conf(ngx_conf_t *cf);
static void *ngx_http_proxy_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_proxy_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);
static ngx_int_t ngx_http_proxy_init_headers(ngx_conf_t *cf,
    ngx_http_proxy_loc_conf_t *conf, ngx_http_proxy_headers_t *headers,
    ngx_keyval_t *default_headers);

static char *ngx_http_proxy_pass(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_proxy_redirect(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_proxy_cookie_domain(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_proxy_cookie_path(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_proxy_store(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#if (NGX_HTTP_CACHE)
static char *ngx_http_proxy_cache(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_proxy_cache_key(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#endif
#if (NGX_HTTP_SSL)
static char *ngx_http_proxy_ssl_password_file(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
#endif

static char *ngx_http_proxy_lowat_check(ngx_conf_t *cf, void *post, void *data);

static ngx_int_t ngx_http_proxy_rewrite_regex(ngx_conf_t *cf,
    ngx_http_proxy_rewrite_t *pr, ngx_str_t *regex, ngx_uint_t caseless);

#if (NGX_HTTP_SSL)
static ngx_int_t ngx_http_proxy_set_ssl(ngx_conf_t *cf,
    ngx_http_proxy_loc_conf_t *plcf);
#endif
static void ngx_http_proxy_set_vars(ngx_url_t *u, ngx_http_proxy_vars_t *v);


static ngx_conf_post_t  ngx_http_proxy_lowat_post =
    { ngx_http_proxy_lowat_check };

//这个决定后端返回的status来决定是否寻找下一个服务器进行连接
static ngx_conf_bitmask_t  ngx_http_proxy_next_upstream_masks[] = {
    { ngx_string("error"), NGX_HTTP_UPSTREAM_FT_ERROR },
    { ngx_string("timeout"), NGX_HTTP_UPSTREAM_FT_TIMEOUT },
    { ngx_string("invalid_header"), NGX_HTTP_UPSTREAM_FT_INVALID_HEADER },
    { ngx_string("http_500"), NGX_HTTP_UPSTREAM_FT_HTTP_500 },
    { ngx_string("http_502"), NGX_HTTP_UPSTREAM_FT_HTTP_502 },
    { ngx_string("http_503"), NGX_HTTP_UPSTREAM_FT_HTTP_503 },
    { ngx_string("http_504"), NGX_HTTP_UPSTREAM_FT_HTTP_504 },
    { ngx_string("http_403"), NGX_HTTP_UPSTREAM_FT_HTTP_403 },
    { ngx_string("http_404"), NGX_HTTP_UPSTREAM_FT_HTTP_404 },
    { ngx_string("updating"), NGX_HTTP_UPSTREAM_FT_UPDATING },
    { ngx_string("off"), NGX_HTTP_UPSTREAM_FT_OFF },
    { ngx_null_string, 0 }
};


#if (NGX_HTTP_SSL)

static ngx_conf_bitmask_t  ngx_http_proxy_ssl_protocols[] = {
    { ngx_string("SSLv2"), NGX_SSL_SSLv2 },
    { ngx_string("SSLv3"), NGX_SSL_SSLv3 },
    { ngx_string("TLSv1"), NGX_SSL_TLSv1 },
    { ngx_string("TLSv1.1"), NGX_SSL_TLSv1_1 },
    { ngx_string("TLSv1.2"), NGX_SSL_TLSv1_2 },
    { ngx_null_string, 0 }
};

#endif


static ngx_conf_enum_t  ngx_http_proxy_http_version[] = {
    { ngx_string("1.0"), NGX_HTTP_VERSION_10 },
    { ngx_string("1.1"), NGX_HTTP_VERSION_11 },
    { ngx_null_string, 0 }
};

ngx_module_t  ngx_http_proxy_module;

/*
Nginx的反向代理模块还提供了很多种配置，如设置连接的超时时间、临时文件如何存储，以及最重要的如何缓存上游服务器响应等功能。这些配置可以通过阅读
ngx_http_proxy_module模块的说明了解，只有深入地理解，才能实现一个高性能的反向代理服务器。本节只是介绍反向代理服务器的基本功能，在第12章中我们
将会深入地探索upstream机制，到那时，读者也许会发现ngx_http_proxy_module模块只是使用upstream机制实现了反向代理功能而已。
*/
static ngx_command_t  ngx_http_proxy_commands[] = {
/*
upstream块
语法：upstream name {...}
配置块：http
upstream块定义了一个上游服务器的集群，便于反向代理中的proxy_pass使用。例如：
upstream backend {
  server backend1.example.com;
  server backend2.example.com;
    server backend3.example.com;
}

server {
  location / {
    proxy_pass  http://backend;
  }
}

语法：proxy_pass URL 
默认值：no 
使用字段：location, location中的if字段 
这个指令设置被代理服务器的地址和被映射的URI，地址可以使用主机名或IP加端口号的形式，例如：


proxy_pass http://localhost:8000/uri/;或者一个unix socket：


proxy_pass http://unix:/path/to/backend.socket:/uri/;路径在unix关键字的后面指定，位于两个冒号之间。
注意：HTTP Host头没有转发，它将设置为基于proxy_pass声明，例如，如果你移动虚拟主机example.com到另外一台机器，然后重新配置正常（监听example.com到一个新的IP），同时在旧机器上手动将新的example.comIP写入/etc/hosts，同时使用proxy_pass重定向到http://example.com, 然后修改DNS到新的IP。
当传递请求时，Nginx将location对应的URI部分替换成proxy_pass指令中所指定的部分，但是有两个例外会使其无法确定如何去替换：


■location通过正则表达式指定；

■在使用代理的location中利用rewrite指令改变URI，使用这个配置可以更加精确的处理请求（break）：

location  /name/ {
  rewrite      /name/([^/] +)  /users?name=$1  break;
  proxy_pass   http://127.0.0.1;
}这些情况下URI并没有被映射传递。
此外，需要标明一些标记以便URI将以和客户端相同的发送形式转发，而不是处理过的形式，在其处理期间：


■两个以上的斜杠将被替换为一个： ”//” – ”/”; 

■删除引用的当前目录：”/./” – ”/”; 

■删除引用的先前目录：”/dir /../” – ”/“。
如果在服务器上必须以未经任何处理的形式发送URI，那么在proxy_pass指令中必须使用未指定URI的部分：

location  /some/path/ {
  proxy_pass   http://127.0.0.1;
}在指令中使用变量是一种比较特殊的情况：被请求的URL不会使用并且你必须完全手工标记URL。
这意味着下列的配置并不能让你方便的进入某个你想要的虚拟主机目录，代理总是将它转发到相同的URL（在一个server字段的配置）：


location / {
  proxy_pass   http://127.0.0.1:8080/VirtualHostBase/https/$server_name:443/some/path/VirtualHostRoot;
}解决方法是使用rewrite和proxy_pass的组合：


location / {
  rewrite ^(.*)$ /VirtualHostBase/https/$server_name:443/some/path/VirtualHostRoot$1 break;
  proxy_pass   http://127.0.0.1:8080;
}这种情况下请求的URL将被重写， proxy_pass中的拖尾斜杠并没有实际意义。
如果需要通过ssl信任连接到一个上游服务器组，proxy_pass前缀为 https://，并且同时指定ssl的端口，如：


upstream backend-secure {
  server 10.0.0.20:443;
}
 
server {
  listen 10.0.0.1:443;
  location / {
    proxy_pass https://backend-secure;
  }
}
*/
    { ngx_string("proxy_pass"),
      NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_HTTP_LMT_CONF|NGX_CONF_TAKE1,
      ngx_http_proxy_pass,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
语法：proxy_redirect [ default|off|redirect replacement ];
默认：proxy_redirect default;
配置块：http、server、location
当上游服务器返回的响应是重定向或刷新请求（如HTTP响应码是301或者302）时，proxy_redirect可以重设HTTP头部的location或refresh字段。
例如，如果上游服务器发出的响应是302重定向请求，location字段的URI是http://localhost:8000/two/some/uri/，那么在下面的配置情况下，实际转发给客户端的location是http://frontend/one/some/uri/。
proxy_redirect http://localhost:8000/two/ http://frontend/one/;
这里还可以使用ngx-http-core-module提供的变量来设置新的location字段。例如：
proxy_redirect   http://localhost:8000/    http://$host:$server_port/;
也可以省略replacement参数中的主机名部分，这时会用虚拟主机名称来填充。例如：
proxy_redirect http://localhost:8000/two/ /one/;
使用off参数时，将使location或者refresh字段维持不变。例如：
proxy_redirect off;
使用默认的default参数时，会按照proxy_pass配置项和所属的location配置项重组发往客户端的location头部。例如，下面两种配置效果是一样的：
location /one/ {
  proxy_pass       http://upstream:port/two/;
  proxy_redirect   default;
}
 
location /one/ {
  proxy_pass       http://upstream:port/two/;
  proxy_redirect   http://upstream:port/two/   /one/;
}

语法：proxy_redirect [ default|off|redirect replacement ];
默认值：proxy_redirect default;
使用字段：http, server, location
这个指令为被代理服务器应答中必须改变的应答头：”Location”和”Refresh”设置值。
我们假设被代理的服务器返回的应答头字段为：Location: http://localhost:8000/two/some/uri/。
指令：


proxy_redirect http://localhost:8000/two/ http://frontend/one/;会将其重写为：Location: http://frontend/one/some/uri/。
在重写字段里面可以不使用服务器名：

proxy_redirect http://localhost:8000/two/ /;这样，默认的服务器名和端口将被设置，端口默认80。
默认的重写可以使用参数default，将使用location和proxy_pass的值。
下面两个配置是等价的：


location /one/ {
  proxy_pass       http://upstream:port/two/;
  proxy_redirect   default;
}
 
location /one/ {
  proxy_pass       http://upstream:port/two/;
  proxy_redirect   http://upstream:port/two/   /one/;
}同样，在重写字段中可以使用变量：


proxy_redirect   http://localhost:8000/    http://$host:$server_port/;这个指令可以重复使用：


proxy_redirect   default;
proxy_redirect   http://localhost:8000/    /;
proxy_redirect   http://www.example.com/   /;参数off在本级中禁用所有的proxy_redirect指令：


proxy_redirect   off;
proxy_redirect   default;
proxy_redirect   http://localhost:8000/    /;
proxy_redirect   http://www.example.com/   /;这个指令可以很容易的将被代理服务器的服务器名重写为代理服务器的服务器名：

proxy_redirect   /   /;
*/ //ngx_http_proxy_merge_loc_conf该if里面内容和ngx_http_proxy_redirect里面的proxy_redirect default一样，也就是说proxy_redirect默认为default
    { ngx_string("proxy_redirect"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_proxy_redirect,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("proxy_cookie_domain"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_proxy_cookie_domain,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("proxy_cookie_path"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_proxy_cookie_path,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /*
     语法：proxy_store [on | off | path] 
     默认值：proxy_store off 
     使用字段：http, server, location 
     这个指令设置哪些传来的文件将被存储，参数”on”保持文件与alias或root指令指定的目录一致，参数”off”将关闭存储，路径名中可以使用变量：

     proxy_store   /data/www$original_uri;应答头中的”Last-Modified”字段设置了文件最后修改时间，为了文件的安全，可以使用proxy_temp_path指定一个临时文件目录。
     这个指令为那些不是经常使用的文件做一份本地拷贝。从而减少被代理服务器负载。
     
     location /images/ {
       root                 /data/www;
       error_page           404 = /fetch$uri;
     }
      
     location /fetch {
       internal;
       proxy_pass           http://backend;
       proxy_store          on;
       proxy_store_access   user:rw  group:rw  all:r;
       proxy_temp_path      /data/temp;
       alias                /data/www;
     }或者通过这种方式：
     
     location /images/ {
       root                 /data/www;
       error_page           404 = @fetch;
     }
      
     location @fetch {
       internal;
      
       proxy_pass           http://backend;
       proxy_store          on;
       proxy_store_access   user:rw  group:rw  all:r;
       proxy_temp_path      /data/temp;
      
       root                 /data/www;
     }注意proxy_store不是一个缓存，它更像是一个镜像。

     nginx的存储系统分两类，一类是通过proxy_store开启的，存储方式是按照url中的文件路径，存储在本地。比如/file/2013/0001/en/test.html，
     那么nginx就会在指定的存储目录下依次建立各个目录和文件。另一类是通过proxy_cache开启，这种方式存储的文件不是按照url路径来组织的，
     而是使用一些特殊方式来管理的(这里称为自定义方式)，自定义方式就是我们要重点分析的。那么这两种方式各有什么优势呢？


    按url路径存储文件的方式，程序处理起来比较简单，但是性能不行。首先有的url巨长，我们要在本地文件系统上建立如此深的目录，那么文件的打开
    和查找都很会很慢(回想kernel中通过路径名查找inode的过程吧)。如果使用自定义方式来处理模式，尽管也离不开文件和路径，但是它不会因url长度
    而产生复杂性增加和性能的降低。从某种意义上说这是一种用户态文件系统，最典型的应该算是squid中的CFS。nginx使用的方式相对简单，主要依靠
    url的md5值来管理
     */
     /*
       nginx的存储系统分两类，一类是通过proxy_store开启的，存储方式是按照url中的文件路径，存储在本地。比如/file/2013/0001/en/test.html，
     那么nginx就会在指定的存储目录下依次建立各个目录和文件。另一类是通过proxy_cache开启，这种方式存储的文件不是按照url路径来组织的，
     而是使用一些特殊方式来管理的(这里称为自定义方式)，自定义方式就是我们要重点分析的。那么这两种方式各有什么优势呢？


    按url路径存储文件的方式，程序处理起来比较简单，但是性能不行。首先有的url巨长，我们要在本地文件系统上建立如此深的目录，那么文件的打开
    和查找都很会很慢(回想kernel中通过路径名查找inode的过程吧)。如果使用自定义方式来处理模式，尽管也离不开文件和路径，但是它不会因url长度
    而产生复杂性增加和性能的降低。从某种意义上说这是一种用户态文件系统，最典型的应该算是squid中的CFS。nginx使用的方式相对简单，主要依靠
    url的md5值来管理
     */
    { ngx_string("proxy_store"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_proxy_store,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /*
    语法：proxy_store_access users:permissions [users:permission …] 
    默认值：proxy_store_access user:rw 
    使用字段：http, server, location 
    指定创建文件和目录的相关权限，如：
    proxy_store_access  user:rw  group:rw  all:r;如果正确指定了组和所有的权限，则没有必要去指定用户的权限：
    proxy_store_access  group:rw  all:r;
     */
    { ngx_string("proxy_store_access"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE123,
      ngx_conf_set_access_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.store_access),
      NULL },

/*
语法：proxy_buffering on|off 
默认值：proxy_buffering on 
使用字段：http, server, location 
为后端的服务器启用应答缓冲。
如果启用缓冲，nginx假设被代理服务器能够非常快的传递应答，并将其放入缓冲区，可以使用 proxy_buffer_size和proxy_buffers设置相关参数。
如果响应无法全部放入内存，则将其写入硬盘。
如果禁用缓冲，从后端传来的应答将立即被传送到客户端。
nginx忽略被代理服务器的应答数目和所有应答的大小，接受proxy_buffer_size所指定的值。
对于基于长轮询的Comet应用需要关闭这个指令，否则异步的应答将被缓冲并且Comet无法正常工作。
*/
    { ngx_string("proxy_buffering"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.buffering),
      NULL },

    /*
     Syntax:  proxy_request_buffering on | off;
     Default:  proxy_request_buffering on; 
     Context:  http, server, location

        Enables or disables buffering of a client request body. 
         When buffering is enabled, the entire request body is read from the client before sending the request to a proxied server. 
         When buffering is disabled, the request body is sent to the proxied server immediately as it is received. In this case, 
     the request cannot be passed to the next server if nginx already started sending the request body. 
         When HTTP/1.1 chunked transfer encoding is used to send the original request body, the request body will be buffered 
     regardless of the directive value unless HTTP/1.1 is enabled for proxying. 
     */
    { ngx_string("proxy_request_buffering"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.request_buffering),
      NULL },

    { ngx_string("proxy_ignore_client_abort"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ignore_client_abort),
      NULL },

    /*
     proxy_bind  192.168.1.1;
     在调用connect()前将上游socket绑定到一个本地地址，如果主机有多个网络接口或别名，但是你希望代理的连接通过指定的借口或地址，
     可以使用这个指令。
     */
    { ngx_string("proxy_bind"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_upstream_bind_set_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.local),
      NULL },

/*
默认值：proxy_connect_timeout 60 
使用字段：http, server, location 
指定一个连接到代理服务器的超时时间，需要注意的是这个时间最好不要超过75秒。
这个时间并不是指服务器传回页面的时间（这个时间由proxy_read_timeout声明）。如果你的前端代理服务器是正常运行的，但是遇到一些状况
（例如没有足够的线程去处理请求，请求将被放在一个连接池中延迟处理），那么这个声明无助于服务器去建立连接。
*/
    { ngx_string("proxy_connect_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.connect_timeout),
      NULL },

/*
默认值：proxy_connect_timeout 60 
使用字段：http, server, location 
指定一个连接到代理服务器的超时时间，需要注意的是这个时间最好不要超过75秒。
这个时间并不是指服务器传回页面的时间（这个时间由proxy_read_timeout声明）。如果你的前端代理服务器是正常运行的，但是遇到一些状
况（例如没有足够的线程去处理请求，请求将被放在一个连接池中延迟处理），那么这个声明无助于服务器去建立连接。
*/
    { ngx_string("proxy_send_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.send_timeout),
      NULL },

    { ngx_string("proxy_send_lowat"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.send_lowat),
      &ngx_http_proxy_lowat_post },

    /*
     语法：proxy_intercept_errors [ on|off ] 
     默认值：proxy_intercept_errors off 
     使用字段：http, server, location 
     使nginx阻止HTTP应答代码为400或者更高的应答。
     默认情况下被代理服务器的所有应答都将被传递。 
     如果将其设置为on则nginx会将阻止的这部分代码在一个error_page指令处理，如果在这个error_page中没有匹配的处理方法，则被代理服务器传递的错误应答会按原样传递。
     */
    { ngx_string("proxy_intercept_errors"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.intercept_errors),
      NULL },

    /*
    语法：proxy_set_header header value 
    默认值： Host and Connection
    使用字段：http, server, location 
    这个指令允许将发送到被代理服务器的请求头重新定义或者增加一些字段。
    这个值可以是一个文本，变量或者它们的组合。
    proxy_set_header在指定的字段中没有定义时会从它的上级字段继承。
    默认只有两个字段可以重新定义：

    proxy_set_header Host $proxy_host;
    proxy_set_header Connection Close;未修改的请求头“Host”可以用如下方式传送：

    proxy_set_header Host $http_host;但是如果这个字段在客户端的请求头中不存在，那么不发送数据到被代理服务器。
    这种情况下最好使用$Host变量，它的值等于请求头中的”Host”字段或服务器名：

    proxy_set_header Host $host;此外，可以将被代理的端口与服务器名称一起传递：

    proxy_set_header Host $host:$proxy_port;如果设置为空字符串，则不会传递头部到后端，例如下列设置将禁止后端使用gzip压缩：

    proxy_set_header  Accept-Encoding  "";
     */
    { ngx_string("proxy_set_header"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_keyval_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, headers_source),
      NULL },

    { ngx_string("proxy_headers_hash_max_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, headers_hash_max_size),
      NULL },

    { ngx_string("proxy_headers_hash_bucket_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, headers_hash_bucket_size),
      NULL },

/*
Allows redefining the request body passed to the proxied server. The value can contain text, variables, and their combination. 
*/ //设置通过后端的body的值，这个值可以包含变量。
  /* 下面这几个和proxy_set_body XXX配置相关 proxy_set_body设置包体后，就不会传送客户端发送来的数据到后端服务器(只传送proxy_set_body设置的包体)，
    如果没有设置，则会发送客户端包体数据到后端， 参考ngx_http_proxy_create_request */  
    { ngx_string("proxy_set_body"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, body_source),
      NULL },

/*
proxy_method

语法：proxy_method method;

配置块：http、server、location

此配置项表示转发时的协议方法名。例如设置为：

proxy_method POST;

那么客户端发来的GET请求在转发时方法名也会改为POST
*/
    { ngx_string("proxy_method"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, method),
      NULL },

/*
语法：proxy_pass_request_headers on | off;
默认：proxy_pass_request_headers on;
配置块：http、server、location
作用为确定是否转发HTTP头部。
*/
    { ngx_string("proxy_pass_request_headers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.pass_request_headers),
      NULL },

/*
语法：proxy_pass_request_body on | off;
默认：proxy_pass_request_body on;
配置块：http、server、location
作用为确定是否向上游服务器发送HTTP包体部分。
*/
    { ngx_string("proxy_pass_request_body"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.pass_request_body),
      NULL },

/*
proxy_buffer_size 
语法：proxy_buffer_size the_size 
默认值：proxy_buffer_size 4k/8k 
使用字段：http, server, location 
设置从被代理服务器读取的第一部分应答的缓冲区大小。
通常情况下这部分应答中包含一个小的应答头。
默认情况下这个值的大小为指令proxy_buffers中指定的一个缓冲区的大小，不过可以将其设置为更小。
*/ //指定的空间开辟在ngx_http_upstream_process_header  
    { ngx_string("proxy_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.buffer_size),
      NULL },

    /*
    语法：proxy_read_timeout time 
    默认值：proxy_read_timeout 60s 
    使用字段：http, server, location 
        决定读取后端服务器应答的超时时间，单位为秒，它决定nginx将等待多久时间来取得一个请求的应答。超时时间是指完成了两次握手后并
    且状态为established后等待读取后端数据的事件。
        相对于proxy_connect_timeout，这个时间可以扑捉到一台将你的连接放入连接池延迟处理并且没有数据传送的服务器，注意不要将此值设置太低，
    某些情况下代理服务器将花很长的时间来获得页面应答（例如如当接收一个需要很多计算的报表时），当然你可以在不同的location里面设置不同的值。
    可以通过指定时间单位以免引起混乱，支持的时间单位有”s”(秒), “ms”(毫秒), “y”(年), “M”(月), “w”(周), “d”(日), “h”(小时),和 “m”(分钟)。
    这个值不能大于597小时。
    */
    { ngx_string("proxy_read_timeout"), //读取后端数据的超时时间
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.read_timeout),
      NULL },

    /*
     语法：proxy_buffers the_number is_size; 
     默认值：proxy_buffers 8 4k/8k; 
     使用字段：http, server, location 
     设置用于读取应答（来自被代理服务器）的缓冲区数目和大小，默认情况也为分页大小，根据操作系统的不同可能是4k或者8k。
     */ //该配置配置的空间真正分配空间在//在ngx_event_pipe_read_upstream中创建空间
    { ngx_string("proxy_buffers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_bufs_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.bufs),
      NULL },

    //默认值：proxy_busy_buffers_size ["#proxy buffer size"] * 2; 
    //xxx_buffers指定为接收后端服务器数据最多开辟这么多空间，xxx_busy_buffers_size指定一次发送后有可能数据没有全部发送出去，因此放入busy链中
    //当没有发送出去的busy链中的数据达到xxx_busy_buffers_size就不能从后端读取数据，只有busy链中的数据发送一部分出去后小与xxx_busy_buffers_size才能继续读取
    { ngx_string("proxy_busy_buffers_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.busy_buffers_size_conf),
      NULL },

    { ngx_string("proxy_force_ranges"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.force_ranges),
      NULL },

    { ngx_string("proxy_limit_rate"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.limit_rate),
      NULL },

#if (NGX_HTTP_CACHE)
/*
       nginx的存储系统分两类，一类是通过proxy_store开启的，存储方式是按照url中的文件路径，存储在本地。比如/file/2013/0001/en/test.html，
     那么nginx就会在指定的存储目录下依次建立各个目录和文件。另一类是通过proxy_cache开启，这种方式存储的文件不是按照url路径来组织的，
     而是使用一些特殊方式来管理的(这里称为自定义方式)，自定义方式就是我们要重点分析的。那么这两种方式各有什么优势呢？


    按url路径存储文件的方式，程序处理起来比较简单，但是性能不行。首先有的url巨长，我们要在本地文件系统上建立如此深的目录，那么文件的打开
    和查找都很会很慢(回想kernel中通过路径名查找inode的过程吧)。如果使用自定义方式来处理模式，尽管也离不开文件和路径，但是它不会因url长度
    而产生复杂性增加和性能的降低。从某种意义上说这是一种用户态文件系统，最典型的应该算是squid中的CFS。nginx使用的方式相对简单，主要依靠
    url的md5值来管理
     */

    /*
     语法：proxy_cache zone_name; 
默认值：None 
使用字段：http, server, location 
设置一个缓存区域的名称，一个相同的区域可以在不同的地方使用。
在0.7.48后，缓存遵循后端的"Expires", "Cache-Control: no-cache", "Cache-Control: max-age=XXX"头部字段，0.7.66版本以后，
"Cache-Control:"private"和"no-store"头同样被遵循。nginx在缓存过程中不会处理"Vary"头，为了确保一些私有数据不被所有的用户看到，
后端必须设置 "no-cache"或者"max-age=0"头，或者proxy_cache_key包含用户指定的数据如$cookie_xxx，使用cookie的值作为proxy_cache_key
的一部分可以防止缓存私有数据，所以可以在不同的location中分别指定proxy_cache_key的值以便分开私有数据和公有数据。
缓存指令依赖代理缓冲区(buffers)，如果proxy_buffers设置为off，缓存不会生效。
*/ 
//xxx_cache(proxy_cache fastcgi_cache) abc必须xxx_cache_path(proxy_cache_path fastcgi_cache_path) xxx keys_zone=abc:10m;一起，否则在ngx_http_proxy_merge_loc_conf会失败，因为没有为该abc创建ngx_http_file_cache_t
//fastcgi_cache 指令指定了在当前作用域中使用哪个缓存维护缓存条目，参数对应的缓存必须事先由 fastcgi_cache_path 指令定义。 
//获取该结构ngx_http_upstream_cache_get，实际上是通过proxy_cache xxx或者fastcgi_cache xxx来获取共享内存块名的，因此必须设置proxy_cache或者fastcgi_cache

    { ngx_string("proxy_cache"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_proxy_cache,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
语法：proxy_cache_key line; 
默认值：$scheme$proxy_host$request_uri; 
使用字段：http, server, location 
指令指定了包含在缓存中的缓存关键字。
proxy_cache_key "$host$request_uri$cookie_user";注意默认情况下服务器的主机名并没有包含到缓存关键字中，如果你为你的站点在不同的location中使用二级域，
你可能需要在缓存关键字中包换主机名：

proxy_cache_key "$scheme$host$request_uri";  
*/
    { ngx_string("proxy_cache_key"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_proxy_cache_key,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
proxy_cache_path 
语法：proxy_cache_path path [levels=number] keys_zone=zone_name:zone_size [inactive=time] [max_size=size]; 
默认值：None 
使用字段：http 
指令指定缓存的路径和一些其他参数，缓存的数据存储在文件中，并且使用代理url的哈希值作为关键字与文件名。levels参数指定缓存的子目录数，例如： 
proxy_cache_path  /data/nginx/cache  levels=1:2   keys_zone=one:10m;文件名类似于：

/data/nginx/cache/c/29/b7f54b2df7773722d382f4809d65029c 
可以使用任意的1位或2位数字作为目录结构，如 X, X:X,或X:X:X e.g.: "2", "2:2", "1:1:2"，但是最多只能是三级目录。
所有活动的key和元数据存储在共享的内存池中，这个区域用keys_zone参数指定。
注意每一个定义的内存池必须是不重复的路径，例如：

proxy_cache_path  /data/nginx/cache/one    levels=1      keys_zone=one:10m;
proxy_cache_path  /data/nginx/cache/two    levels=2:2    keys_zone=two:100m;
proxy_cache_path  /data/nginx/cache/three  levels=1:1:2  keys_zone=three:1000m;如果在inactive参数指定的时间内缓存的数据没有被请求则被删除，
默认inactive为10分钟。
一个名为cache manager的进程控制磁盘的缓存大小，它被用来删除不活动的缓存和控制缓存大小，这些都在max_size参数中定义，当目前缓存的值超出max_size
指定的值之后，超过其大小后最少使用数据（LRU替换算法）将被删除。
内存池的大小按照缓存页面数的比例进行设置，一个页面（文件）的元数据大小按照操作系统来定，FreeBSD/i386下为64字节，FreeBSD/amd64下为128字节。
proxy_cache_path和proxy_temp_path应该使用在相同的文件系统上。

Proxy_cache_path：缓存的存储路径和索引信息；
  path 缓存文件的根目录；
  level=N:N在目录的第几级hash目录缓存数据；
  keys_zone=name:size 缓存索引重建进程建立索引时用于存放索引的内存区域名和大小；
  interval=time强制更新缓存时间，规定时间内没有访问则从内存中删除，默认10s；
  max_size=size硬盘中缓存数据的上限，由cache manager管理，超出则根据LRU策略删除；
  loader_sleep=time索引重建进程在两次遍历间的暂停时长，默认50ms；
  loader_files=number重建索引时每次加载数据元素的上限，进程递归遍历读取硬盘上的缓存目录和文件，对每个文件在内存中建立索引，每
  建立一个索引称为加载一个数据元素，每次遍历时可同时加载多个数据元素，默认100；
*/  //XXX_cache缓存是先写在xxx_temp_path再移到xxx_cache_path，所以这两个目录最好在同一个分区
//xxx_cache(proxy_cache fastcgi_cache) abc必须xxx_cache_path(proxy_cache_path fastcgi_cache_path) xxx keys_zone=abc:10m;一起，否则在ngx_http_proxy_merge_loc_conf会失败，因为没有为该abc创建ngx_http_file_cache_t
//fastcgi_cache 指令指定了在当前作用域中使用哪个缓存维护缓存条目，参数对应的缓存必须事先由 fastcgi_cache_path 指令定义。 
//获取该结构ngx_http_upstream_cache_get，实际上是通过proxy_cache xxx或者fastcgi_cache xxx来获取共享内存块名的，因此必须设置proxy_cache或者fastcgi_cache

/*
非缓存方式(p->cacheable=0)p->temp_file->path = u->conf->temp_path; 由ngx_http_fastcgi_temp_path指定路径
缓存方式(p->cacheable=1) p->temp_file->path = r->cache->file_cache->temp_path;见proxy_cache_path或者fastcgi_cache_path指定路径 
见ngx_http_upstream_send_response 

当前fastcgi_buffers 和fastcgi_buffer_size配置的空间都已经用完了，则需要把数据写道临时文件中去，参考ngx_event_pipe_read_upstream
*/  //从ngx_http_file_cache_update可以看出，后端数据先写到临时文件后，在写入xxx_cache_path中，见ngx_http_file_cache_update
    { ngx_string("proxy_cache_path"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_2MORE,
      ngx_http_file_cache_set_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_proxy_main_conf_t, caches),
      &ngx_http_proxy_module },

/*
语法: proxy_cache_bypass line […];
默认值: off
使用字段: http, server, location
这个指令指定不使用缓存返回应答的条件，如果指定的变量中至少有一个为非空，或者不等于“0”，这个应答将不从缓存中返回：

 proxy_cache_bypass $cookie_nocache $ arg_nocache$arg_comment;
 proxy_cache_bypass $http_pragma $http_authorization;可以结合proxy_no_cache使用。

 
Defines conditions under which the response will not be taken from a cache. If at least one value of the string parameters is not 
empty and is not equal to “0” then the response will not be taken from the cache: 
*/ //注意proxy_no_cache和proxy_cache_proxy的区别
    { ngx_string("proxy_cache_bypass"), 
    //proxy_cache_bypass  xx1 xx2设置的xx2不为空或者不为0，则不会从缓存中取，而是直接冲后端读取  但是这些请求的后端响应数据依然可以被 upstream 模块缓存。 
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_set_predicate_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_bypass),
      NULL },

/*
语法：proxy_no_cache variable1 variable2 …; 
默认值：None 
使用字段：http, server, location 
确定在何种情况下缓存的应答将不会使用，示例：

proxy_no_cache $cookie_nocache  $arg_nocache$arg_comment;
proxy_no_cache $http_pragma     $http_authorization;如果为空字符串或者等于0，表达式的值等于false，例如，在上述例子中，如果在
请求中设置了cookie “nocache”，请求将总是穿过缓存而被传送到后端。
注意：来自后端的应答依然有可能符合缓存条件，有一种方法可以快速的更新缓存中的内容，那就是发送一个拥有你自己定义的请求头部字段
的请求。例如：My-Secret-Header，那么在proxy_no_cache指令中可以这样定义：
proxy_no_cache $http_my_secret_header;

Defines conditions under which the response will not be saved to a cache. If at least one value of the string parameters is 
not empty and is not equal to “0” then the response will not be saved(注意是not be saved): 
*/ //注意proxy_no_cache和proxy_cache_proxy的区别

    //proxy_cache_bypass  xx1 xx2设置的xx2不为空或者不为0，则不会从缓存中取，而是直接冲后端读取
    //proxy_no_cache  xx1 xx2设置的xx2不为空或者不为0，则后端回来的数据不会被缓存
    { ngx_string("proxy_no_cache"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_set_predicate_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.no_cache),
      NULL },

/*
语法：proxy_cache_valid reply_code [reply_code ...] time; 
默认值：None 
使用字段：http, server, location 
为不同的应答设置不同的缓存时间，例如： 
  proxy_cache_valid  200 302  10m;
  proxy_cache_valid  404      1m;为应答代码为200和302的设置缓存时间为10分钟，404代码缓存1分钟。
如果只定义时间：
 proxy_cache_valid 5m;那么只对代码为200, 301和302的应答进行缓存。
同样可以使用any参数任何应答。 
  proxy_cache_valid  200 302 10m;
  proxy_cache_valid  301 1h;
  proxy_cache_valid  any 1m;
*/
    { ngx_string("proxy_cache_valid"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_file_cache_valid_set_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_valid),
      NULL },

/*
语法：proxy_cache_min_uses the_number; 
默认值：proxy_cache_min_uses 1; 
使用字段：http, server, location 
多少次的查询后应答将被缓存，默认1。
*/ //Proxy_cache_min_uses number 默认为1，当客户端发送相同请求达到规定次数后，nginx才对响应数据进行缓存；
////例如配置Proxy_cache_min_uses 5，则需要客户端请求5才才能从缓存中取，如果现在只有4次，则都需要从后端获取数据,如果没有达到5次，函数ngx_http_upstream_cache会把 u->cacheable = 0;
    { ngx_string("proxy_cache_min_uses"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_min_uses),
      NULL },

/*
语法：proxy_cache_use_stale [error|timeout|updating|invalid_header|http_500|http_502|http_503|http_504|http_404|off] [...]; 
默认值：proxy_cache_use_stale off; 
使用字段：http, server, location 
这个指令告诉nginx何时从代理缓存中提供一个过期的响应，参数类似于proxy_next_upstream指令。
为了防止缓存失效（在多个线程同时更新本地缓存时），你可以指定'updating'参数，它将保证只有一个线程去更新缓存，并且在这个线程更
新缓存的过程中其他的线程只会响应当前缓存中的过期版本。
*/
/*
例如如果设置了fastcgi_cache_use_stale updating，表示说虽然该缓存文件失效了，已经有其他客户端请求在获取后端数据，但是该客户端请求现在还没有获取完整，
这时候就可以把以前过期的缓存发送给当前请求的客户端 //可以配合ngx_http_upstream_cache阅读
*/
    { ngx_string("proxy_cache_use_stale"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_use_stale),
      &ngx_http_proxy_next_upstream_masks },

/*
语法：proxy_cache_methods [GET HEAD POST]; 
默认值：proxy_cache_methods GET HEAD; 
使用字段：http, server, location 
GET/HEAD用来装饰语句，即你无法禁用GET/HEAD即使你只使用下列语句设置： 
proxy_cache_methods POST;
*/
    { ngx_string("proxy_cache_methods"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_methods),
      &ngx_http_upstream_cache_method_mask },

/*
When enabled, only one request at a time will be allowed to populate a new cache element identified according to the proxy_cache_key 
directive by passing a request to a proxied server. Other requests of the same cache element will either wait for a response to appear 
in the cache or the cache lock for this element to be released, up to the time set by the proxy_cache_lock_timeout directive. 


这个主要解决一个问题:
假设现在又两个客户端，一个客户端正在获取后端数据，并且后端返回了一部分，则nginx会缓存这一部分，并且等待所有后端数据返回继续缓存。
但是在缓存的过程中如果客户端2页来想后端去同样的数据uri等都一样，则会去到客户端缓存一半的数据，这时候就可以通过该配置来解决这个问题，
也就是客户端1还没缓存完全部数据的过程中客户端2只有等客户端1获取完全部后端数据，或者获取到proxy_cache_lock_timeout超时，则客户端2只有从后端获取数据
*/
    { ngx_string("proxy_cache_lock"), //默认关闭
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_lock),
      NULL },

/*
If the last request passed to the proxied server for populating a new cache element has not completed for the specified time, one 
more request may be passed to the proxied server. 
*/
    { ngx_string("proxy_cache_lock_timeout"),  //默认5S
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_lock_timeout),
      NULL },

    { ngx_string("proxy_cache_lock_age"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_lock_age),
      NULL },

    { ngx_string("proxy_cache_revalidate"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.cache_revalidate),
      NULL },

#endif

    /*
     语法：proxy_temp_path dir-path [ level1 [ level2 [ level3 ] ; 
     默认值：在configure时由–http-proxy-temp-path指定 
     使用字段：http, server, location 
     类似于http核心模块中的client_body_temp_path指令，指定一个地址来缓冲比较大的被代理请求。
     */ //XXX_cache缓存是先写在xxx_temp_path再移到xxx_cache_path，所以这两个目录最好在同一个分区

         /*
默认情况下p->temp_file->path = u->conf->temp_path; 也就是由ngx_http_fastcgi_temp_path指定路径，但是如果是缓存方式(p->cacheable=1)并且配置
proxy_cache_path(fastcgi_cache_path) /a/b的时候带有use_temp_path=off(表示不使用ngx_http_fastcgi_temp_path配置的path)，
则p->temp_file->path = r->cache->file_cache->temp_path; 也就是临时文件/a/b/temp。use_temp_path=off表示不使用ngx_http_fastcgi_temp_path
配置的路径，而使用指定的临时路径/a/b/temp   见ngx_http_upstream_send_response 
*/ 
    /*后端数据读取完毕，并且全部写入临时文件后才会执行rename过程，为什么需要临时文件的原因是:例如之前的缓存过期了，现在有个请求正在从后端
    获取数据写入临时文件，如果是直接写入缓存文件，则在获取后端数据过程中，如果在来一个客户端请求，如果允许proxy_cache_use_stale updating，则
    后面的请求可以直接获取之前老旧的过期缓存，从而可以避免冲突(前面的请求写文件，后面的请求获取文件内容) 
    */

//从ngx_http_file_cache_update可以看出，后端数据先写到临时文件后，在写入xxx_cache_path中，见ngx_http_file_cache_update
    { ngx_string("proxy_temp_path"), //从ngx_http_file_cache_update可以看出，后端数据先写到临时文件后，在写入xxx_cache_path中，见ngx_http_file_cache_update
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1234,
      ngx_conf_set_path_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.temp_path),
      NULL },

    /*
     语法：proxy_max_temp_file_size size; 
     默认值：proxy_max_temp_file_size 1G; 
     使用字段：http, server, location, if 
     当代理缓冲区过大时使用一个临时文件的最大值，如果文件大于这个值，将同步传递请求而不写入磁盘进行缓存。
     如果这个值设置为零，则禁止使用临时文件。
     */
    { ngx_string("proxy_max_temp_file_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.max_temp_file_size_conf),
      NULL },

    /*
     语法：proxy_temp_file_write_size size; 
     默认值：proxy_temp_file_write_size [”#proxy buffer size”] * 2; 
     使用字段：http, server, location, if 
     设置在写入proxy_temp_path时数据的大小，在预防一个工作进程在传递文件时阻塞太长。
     */
    { ngx_string("proxy_temp_file_write_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.temp_file_write_size_conf),
      NULL },

/*
语法：proxy_next_upstream [error | timeout | invalid_header | http_500 | http_502 | http_503 | http_504 | http_404 | off ];

默认：proxy_next_upstream error timeout;

配置块：http、server、location

此配置项表示当向一台上游服务器转发请求出现错误时，继续换一台上游服务器处理这个请求。上游服务器一旦开始发送应答，
Nginx反向代理服务器会立刻把应答包转发给客户端。因此，一旦Nginx开始向客户端发送响应包，之后的过程中若出现错误也是不允许换下一台
上游服务器继续处理的。这很好理解，这样才可以更好地保证客户端只收到来自一个上游服务器的应答。proxy_next_upstream的参数用来说明
在哪些情况下会继续选择下一台上游服务器转发请求。

error：当向上游服务器发起连接、发送请求、读取响应时出错。

timeout：发送请求或读取响应时发生超时。
invalid_header：上游服务器发送的响应是不合法的。
http_500：上游服务器返回的HTTP响应码是500。
http_502：上游服务器返回的HTTP响应码是502。
http_503：上游服务器返回的HTTP响应码是503。
http_504：上游服务器返回的HTTP响应码是504。
http_404：上游服务器返回的HTTP响应码是404。
off：关闭proxy_next_upstream功能—出错就选择另一台上游服务器再次转发。
*/
    { ngx_string("proxy_next_upstream"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.next_upstream),
      &ngx_http_proxy_next_upstream_masks },

    { ngx_string("proxy_next_upstream_tries"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.next_upstream_tries),
      NULL },

    { ngx_string("proxy_next_upstream_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.next_upstream_timeout),
      NULL },

/*
proxy_pass_header
语法：proxy_pass_header the_header;
配置块：http、server、location
与proxy_hide_header功能相反，proxy_pass_header会将原来禁止转发的header设置为允许转发。例如：
proxy_pass_header X-Accel-Redirect;
*/
    { ngx_string("proxy_pass_header"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.pass_headers),
      NULL },

/*
proxy_hide_header
语法：proxy_hide_header the_header;
配置块：http、server、location
Nginx会将上游服务器的响应转发给客户端，但默认不会转发以下HTTP头部字段：Date、Server、X-Pad和X-Accel-*。使用proxy_hide_header后可以
任意地指定哪些HTTP头部字段不能被转发。例如：
proxy_hide_header Cache-Control;
proxy_hide_header MicrosoftOfficeWebServer;

语法：proxy_hide_header the_header 
使用字段：http, server, location 
nginx不对从被代理服务器传来的"Date", "Server", "X-Pad"和"X-Accel-..."应答进行转发，这个参数允许隐藏一些其他的头部字段，但是如果
上述提到的头部字段必须被转发，可以使用proxy_pass_header指令，例如：需要隐藏MS-OfficeWebserver和AspNet-Version可以使用如下配置： 
location / {
  proxy_hide_header X-AspNet-Version;
  proxy_hide_header MicrosoftOfficeWebServer;
}当使用X-Accel-Redirect时这个指令非常有用。例如，你可能要在后端应用服务器对一个需要下载的文件设置一个返回头，其中X-Accel-Redirect
字段即为这个文件，同时要有恰当的Content-Type，但是，重定向的URL将指向包含这个文件的文件服务器，而这个服务器传递了它自己的Content-Type，
可能这并不是正确的，这样就忽略了后端应用服务器传递的Content-Type。为了避免这种情况你可以使用这个指令： 
location / {
  proxy_pass http://backend_servers;
}
 
location /files/ {
  proxy_pass http://fileserver;
  proxy_hide_header Content-Type;

*/ //proxy_hide_header和proxy_pass_header功能相反
    { ngx_string("proxy_hide_header"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.hide_headers),
      NULL },

    /*
     语法：proxy_ignore_headers name [name …] 
     默认值：none 
     使用字段：http, server, location 
     这个指令(0.7.54+) 禁止处理来自代理服务器的应答。
     可以指定的字段为”X-Accel-Redirect”, “X-Accel-Expires”, “Expires”或”Cache-Control”。
     */
    { ngx_string("proxy_ignore_headers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ignore_headers),
      &ngx_http_upstream_ignore_headers_masks },

    //到后端服务器采用的http版本号，默认1.0，见ngx_http_proxy_create_request
    { ngx_string("proxy_http_version"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, http_version),
      &ngx_http_proxy_http_version },

#if (NGX_HTTP_SSL)

    /*
     proxy_ssl_session_reuse
     语法：proxy_ssl_session_reuse [ on | off ];
     默认值： proxy_ssl_session_reuse on;
     使用字段：http, server, location
     使用版本：≥ 0.7.11
     当使用https连接到上游服务器时尝试重用ssl会话。
     */
    { ngx_string("proxy_ssl_session_reuse"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ssl_session_reuse),
      NULL },

    { ngx_string("proxy_ssl_protocols"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, ssl_protocols),
      &ngx_http_proxy_ssl_protocols },

    { ngx_string("proxy_ssl_ciphers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, ssl_ciphers),
      NULL },

    { ngx_string("proxy_ssl_name"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_set_complex_value_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ssl_name),
      NULL },

    { ngx_string("proxy_ssl_server_name"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ssl_server_name),
      NULL },

    { ngx_string("proxy_ssl_verify"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, upstream.ssl_verify),
      NULL },

    { ngx_string("proxy_ssl_verify_depth"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, ssl_verify_depth),
      NULL },

    { ngx_string("proxy_ssl_trusted_certificate"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, ssl_trusted_certificate),
      NULL },

    { ngx_string("proxy_ssl_crl"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, ssl_crl),
      NULL },

    { ngx_string("proxy_ssl_certificate"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, ssl_certificate),
      NULL },

    { ngx_string("proxy_ssl_certificate_key"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proxy_loc_conf_t, ssl_certificate_key),
      NULL },

    { ngx_string("proxy_ssl_password_file"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_proxy_ssl_password_file,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

#endif

      ngx_null_command
};


static ngx_http_module_t  ngx_http_proxy_module_ctx = {
    ngx_http_proxy_add_variables,          /* preconfiguration */
    NULL,                                  /* postconfiguration */

    ngx_http_proxy_create_main_conf,       /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_proxy_create_loc_conf,        /* create location configuration */
    ngx_http_proxy_merge_loc_conf          /* merge location configuration */
};


ngx_module_t  ngx_http_proxy_module = {
    NGX_MODULE_V1,
    &ngx_http_proxy_module_ctx,              /* module context */
    ngx_http_proxy_commands,                 /* module directives */
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


static char  ngx_http_proxy_version[] = " HTTP/1.0" CRLF;
static char  ngx_http_proxy_version_11[] = " HTTP/1.1" CRLF;


static ngx_keyval_t  ngx_http_proxy_headers[] = {
    { ngx_string("Host"), ngx_string("$proxy_host") },
    { ngx_string("Connection"), ngx_string("close") },
    { ngx_string("Content-Length"), ngx_string("$proxy_internal_body_length") },
    { ngx_string("Transfer-Encoding"), ngx_string("$proxy_internal_chunked") },
    { ngx_string("TE"), ngx_string("") }, //这里面的value为空，则如果proxy_set_header又重新设置了，则还是会生效，否则该项不生效，见ngx_http_proxy_init_headers
    { ngx_string("Keep-Alive"), ngx_string("") },
    { ngx_string("Expect"), ngx_string("") },
    { ngx_string("Upgrade"), ngx_string("") },
    { ngx_null_string, ngx_null_string }
};

//最终添加到了ngx_http_upstream_conf_t->hide_headers_hash表中 不需要发送给客户端
static ngx_str_t  ngx_http_proxy_hide_headers[] = {
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


#if (NGX_HTTP_CACHE)

static ngx_keyval_t  ngx_http_proxy_cache_headers[] = {
    { ngx_string("Host"), ngx_string("$proxy_host") },
    { ngx_string("Connection"), ngx_string("close") },
    { ngx_string("Content-Length"), ngx_string("$proxy_internal_body_length") },
    { ngx_string("Transfer-Encoding"), ngx_string("$proxy_internal_chunked") },
    { ngx_string("TE"), ngx_string("") },
    { ngx_string("Keep-Alive"), ngx_string("") },
    { ngx_string("Expect"), ngx_string("") },
    { ngx_string("Upgrade"), ngx_string("") },
    { ngx_string("If-Modified-Since"),
      ngx_string("$upstream_cache_last_modified") },
    { ngx_string("If-Unmodified-Since"), ngx_string("") },
    { ngx_string("If-None-Match"), ngx_string("$upstream_cache_etag") },
    { ngx_string("If-Match"), ngx_string("") },
    { ngx_string("Range"), ngx_string("") },
    { ngx_string("If-Range"), ngx_string("") },
    { ngx_null_string, ngx_null_string }
};

#endif


/*
该模块中包含一些内置变量，可以用于proxy_set_header指令中以创建头部。

$proxy_add_x_forwarded_for
包含客户端的请求头“X-Forwarded-For”与$remote_addr，用逗号分开，如果不存在X-Forwarded-For请求头，则$proxy_add_x_forwarded_for等于$remote_addr。

$proxy_host
被代理服务器的主机名与端口号。

$proxy_internal_body_length
通过proxy_set_body设置的代理请求实体的长度。

$proxy_host
被代理服务器的端口号
*/
static ngx_http_variable_t  ngx_http_proxy_vars[] = {

    { ngx_string("proxy_host"), NULL, ngx_http_proxy_host_variable, 0,
      NGX_HTTP_VAR_CHANGEABLE|NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    { ngx_string("proxy_port"), NULL, ngx_http_proxy_port_variable, 0,
      NGX_HTTP_VAR_CHANGEABLE|NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    { ngx_string("proxy_add_x_forwarded_for"), NULL,
      ngx_http_proxy_add_x_forwarded_for_variable, 0, NGX_HTTP_VAR_NOHASH, 0 },

#if 0
    { ngx_string("proxy_add_via"), NULL, NULL, 0, NGX_HTTP_VAR_NOHASH, 0 },
#endif

    { ngx_string("proxy_internal_body_length"), NULL,
      ngx_http_proxy_internal_body_length_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    { ngx_string("proxy_internal_chunked"), NULL,
      ngx_http_proxy_internal_chunked_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    { ngx_null_string, NULL, NULL, 0, 0, 0 }
};


static ngx_path_init_t  ngx_http_proxy_temp_path = {
    ngx_string(NGX_HTTP_PROXY_TEMP_PATH), { 1, 2, 0 }
};

//配置proxy_pass后，在ngx_http_core_content_phase里面指向该函数
/*
那么，当有请求访问到特定的location的时候(假设这个location配置了proxy_pass指令)，
跟其他请求一样，会调用各个phase的checker和handler，到了NGX_HTTP_CONTENT_PHASE的checker，
即ngx_http_core_content_phase()的时候，会调用r->content_handler(r)，即ngx_http_proxy_handler。
*/
static ngx_int_t ngx_http_proxy_handler(ngx_http_request_t *r)
{
    ngx_int_t                    rc;
    ngx_http_upstream_t         *u;
    ngx_http_proxy_ctx_t        *ctx;
    ngx_http_proxy_loc_conf_t   *plcf;
#if (NGX_HTTP_CACHE)
    ngx_http_proxy_main_conf_t  *pmcf;
#endif

    if (ngx_http_upstream_create(r) != NGX_OK) { //生成一个ngx_http_upstream_t结构，赋值到r->upstream
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_proxy_ctx_t));
    if (ctx == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    //将这个上下文放到请求的ctx数组的ngx_http_proxy_module模块中，r->ctx[module.ctx_index] = c;
    ngx_http_set_ctx(r, ctx, ngx_http_proxy_module);

    plcf = ngx_http_get_module_loc_conf(r, ngx_http_proxy_module);

    u = r->upstream;

    if (plcf->proxy_lengths == NULL) {
        ctx->vars = plcf->vars;
        u->schema = plcf->vars.schema;
#if (NGX_HTTP_SSL)
        u->ssl = (plcf->upstream.ssl != NULL);
#endif

    } else {
        if (ngx_http_proxy_eval(r, ctx, plcf) != NGX_OK) { 
        //获取uri中变量的值，从而可以得到完整的uri，和ngx_http_proxy_pass配合阅读
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    u->output.tag = (ngx_buf_tag_t) &ngx_http_proxy_module;

    u->conf = &plcf->upstream;

#if (NGX_HTTP_CACHE)
    pmcf = ngx_http_get_module_main_conf(r, ngx_http_proxy_module);

    u->caches = &pmcf->caches;
    u->create_key = ngx_http_proxy_create_key;
#endif

    //为upstream准备各种请求的回调
    u->create_request = ngx_http_proxy_create_request; //生成发送到上游服务器的请求缓冲（或者一条缓冲链），也就是要发给上游的数据
    u->reinit_request = ngx_http_proxy_reinit_request;
    //处理回调就是第一行的回调，第一行处理完后会设置为ngx_http_proxy_process_header，走下一步
    u->process_header = ngx_http_proxy_process_status_line;
    //一个upstream的u->read_event_handler 读事件回调被设置为ngx_http_upstream_process_header;，他会不断的读取数据，然后
    //调用process_header对于FCGI，当然是调用对应的读取FCGI格式的函数了，对于代理模块，只要处理HTTP格式即可
    u->abort_request = ngx_http_proxy_abort_request;
    u->finalize_request = ngx_http_proxy_finalize_request;
    r->state = 0;

    if (plcf->redirects) {
        u->rewrite_redirect = ngx_http_proxy_rewrite_redirect;
    }

    if (plcf->cookie_domains || plcf->cookie_paths) {
        u->rewrite_cookie = ngx_http_proxy_rewrite_cookie;
    }

    u->buffering = plcf->upstream.buffering;

    u->pipe = ngx_pcalloc(r->pool, sizeof(ngx_event_pipe_t));
    if (u->pipe == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    //设置在有buffering的状态下，nginx读取upstream的回调，如果是FCGI，则是对应的回调ngx_http_fastcgi_input_filter用来解析FCGI协议的数据。
	//如果我们要实现我们自己的协议格式，那就用对应的解析方式。
    u->pipe->input_filter = ngx_http_proxy_copy_filter;
    u->pipe->input_ctx = r;

    //buffering后端响应包体使用ngx_event_pipe_t->input_filter  非buffering方式响应后端包体使用ngx_http_upstream_s->input_filter ,在ngx_http_upstream_send_response分叉

    u->input_filter_init = ngx_http_proxy_input_filter_init;
    u->input_filter = ngx_http_proxy_non_buffered_copy_filter;
    u->input_filter_ctx = r;

    u->accel = 1;

    if (!plcf->upstream.request_buffering
        && plcf->body_values == NULL && plcf->upstream.pass_request_body
        && (!r->headers_in.chunked
            || plcf->http_version == NGX_HTTP_VERSION_11))
    {
        r->request_body_no_buffering = 1;
    }

    /*
在阅读HTTP反向代理模块(ngx_http_proxy_module)源代码时，会发现它并没有调用r->main->count++，其中proxy模块是这样启动upstream机制的：
ngx_http_read_client_request_body(r，ngx_http_upstream_init);，这表示读取完用户请求的HTTP包体后才会调用ngx_http_upstream_init方法
启动upstream机制。由于ngx_http_read_client_request_body的第一行有效语句是r->maln->count++，所以HTTP反向代理模块不能
再次在其代码中执行r->main->count++。

这个过程看起来似乎让人困惑。为什么有时需要把引用计数加1，有时却不需要呢？因为ngx_http_read- client_request_body读取请求包体是
一个异步操作（需要epoll多次调度方能完成的可称其为异步操作），ngx_http_upstream_init方法启用upstream机制也是一个异步操作，因此，
从理论上来说，每执行一次异步操作应该把引用计数加1，而异步操作结束时应该调用ngx_http_finalize_request方法把引用计数减1。另外，
ngx_http_read_client_request_body方法内是加过引用计数的，而ngx_http_upstream_init方法内却没有加过引用计数（或许Nginx将来会修改
这个问题）。在HTTP反向代理模块中，它的ngx_http_proxy_handler方法中用“ngx_http_read- client_request_body(r，ngx_http_upstream_init);”
语句同时启动了两个异步操作，注意，这行语句中只加了一次引用计数。执行这行语句的ngx_http_proxy_handler方法返回时只调用
ngx_http_finalize_request方法一次，这是正确的。对于mytest模块也一样，务必要保证对引用计数的增加和减少是配对进行的。
    */
    rc = ngx_http_read_client_request_body(r, ngx_http_upstream_init);

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    return NGX_DONE;
}


static ngx_int_t
ngx_http_proxy_eval(ngx_http_request_t *r, ngx_http_proxy_ctx_t *ctx,
    ngx_http_proxy_loc_conf_t *plcf)
{
    u_char               *p;
    size_t                add;
    u_short               port;
    ngx_str_t             proxy;
    ngx_url_t             url;
    ngx_http_upstream_t  *u;

    if (ngx_http_script_run(r, &proxy, plcf->proxy_lengths->elts, 0,
                            plcf->proxy_values->elts)
        == NULL)
    {
        return NGX_ERROR;
    }

    if (proxy.len > 7
        && ngx_strncasecmp(proxy.data, (u_char *) "http://", 7) == 0)
    {
        add = 7;
        port = 80;

#if (NGX_HTTP_SSL)

    } else if (proxy.len > 8
               && ngx_strncasecmp(proxy.data, (u_char *) "https://", 8) == 0)
    {
        add = 8;
        port = 443;
        r->upstream->ssl = 1;

#endif

    } else {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "invalid URL prefix in \"%V\"", &proxy);
        return NGX_ERROR;
    }

    u = r->upstream;

    u->schema.len = add;
    u->schema.data = proxy.data;

    ngx_memzero(&url, sizeof(ngx_url_t));

    url.url.len = proxy.len - add;
    url.url.data = proxy.data + add;
    url.default_port = port;
    url.uri_part = 1;
    url.no_resolve = 1;

    if (ngx_parse_url(r->pool, &url) != NGX_OK) {
        if (url.err) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "%s in upstream \"%V\"", url.err, &url.url);
        }

        return NGX_ERROR;
    }

    if (url.uri.len) {
        if (url.uri.data[0] == '?') {
            p = ngx_pnalloc(r->pool, url.uri.len + 1);
            if (p == NULL) {
                return NGX_ERROR;
            }

            *p++ = '/';
            ngx_memcpy(p, url.uri.data, url.uri.len);

            url.uri.len++;
            url.uri.data = p - 1;
        }
    }

    ctx->vars.key_start = u->schema;

    ngx_http_proxy_set_vars(&url, &ctx->vars);

    u->resolved = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_resolved_t));
    if (u->resolved == NULL) {
        return NGX_ERROR;
    }

    if (url.addrs && url.addrs[0].sockaddr) {
        u->resolved->sockaddr = url.addrs[0].sockaddr;
        u->resolved->socklen = url.addrs[0].socklen;
        u->resolved->naddrs = 1;
        u->resolved->host = url.addrs[0].name;

    } else {
        u->resolved->host = url.host;
        u->resolved->port = (in_port_t) (url.no_port ? port : url.port);
        u->resolved->no_port = url.no_port;
    }

    return NGX_OK;
}


#if (NGX_HTTP_CACHE)

//解析proxy_cache_key xxx 参数值到r->cache->keys
static ngx_int_t //ngx_http_upstream_cache中执行
ngx_http_proxy_create_key(ngx_http_request_t *r)
{
    size_t                      len, loc_len;
    u_char                     *p;
    uintptr_t                   escape;
    ngx_str_t                  *key;
    ngx_http_upstream_t        *u;
    ngx_http_proxy_ctx_t       *ctx;
    ngx_http_proxy_loc_conf_t  *plcf;

    u = r->upstream;

    plcf = ngx_http_get_module_loc_conf(r, ngx_http_proxy_module);

    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    key = ngx_array_push(&r->cache->keys);
    if (key == NULL) {
        return NGX_ERROR;
    }

    if (plcf->cache_key.value.data) { //如果设置了proxy_cache_key

        if (ngx_http_complex_value(r, &plcf->cache_key, key) != NGX_OK) { //解析plcf->cache_key中的变量value值到r->cache->keys中
            return NGX_ERROR;
        }

        return NGX_OK;
    }

    //如果没有设置proxy_cache_key，则使用默认Default:  proxy_cache_key $scheme$proxy_host$request_uri; 
    /* 
    
     Syntax:  proxy_cache_key string;
      
     Default:  proxy_cache_key $scheme$proxy_host$request_uri; 
     Context:  http, server, location
     */
    
    *key = ctx->vars.key_start;

    key = ngx_array_push(&r->cache->keys); //下面这些代码是解析默认的 proxy_cache_key $scheme$proxy_host$request_uri; 存到keys数组
    if (key == NULL) {
        return NGX_ERROR;
    }

    if (plcf->proxy_lengths && ctx->vars.uri.len) {

        *key = ctx->vars.uri;
        u->uri = ctx->vars.uri;

        return NGX_OK;

    } else if (ctx->vars.uri.len == 0 && r->valid_unparsed_uri && r == r->main)
    {
        *key = r->unparsed_uri;
        u->uri = r->unparsed_uri;

        return NGX_OK;
    }

    loc_len = (r->valid_location && ctx->vars.uri.len) ? plcf->location.len : 0;

    if (r->quoted_uri || r->internal) {
        escape = 2 * ngx_escape_uri(NULL, r->uri.data + loc_len,
                                    r->uri.len - loc_len, NGX_ESCAPE_URI);
    } else {
        escape = 0;
    }

    len = ctx->vars.uri.len + r->uri.len - loc_len + escape
          + sizeof("?") - 1 + r->args.len;

    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    key->data = p;

    if (r->valid_location) {
        p = ngx_copy(p, ctx->vars.uri.data, ctx->vars.uri.len);
    }

    if (escape) {
        ngx_escape_uri(p, r->uri.data + loc_len,
                       r->uri.len - loc_len, NGX_ESCAPE_URI);
        p += r->uri.len - loc_len + escape;

    } else {
        p = ngx_copy(p, r->uri.data + loc_len, r->uri.len - loc_len);
    }

    if (r->args.len > 0) {
        *p++ = '?';
        p = ngx_copy(p, r->args.data, r->args.len);
    }

    key->len = p - key->data;
    u->uri = *key;

    return NGX_OK;
}

#endif

/*
  proxy_pass  http://10.10.0.103:8080/tttxxsss; 
  浏览器输入:http://10.2.13.167/proxy1111/yangtest
  实际上到后端的uri会变为/tttxxsss/yangtest
  plcf->location:/proxy1111, ctx->vars.uri:/tttxxsss,  r->uri:/proxy1111/yangtest, r->args:, urilen:19
*/

/*
如果是FCGI。下面组建好FCGI的各种头部，包括请求开始头，请求参数头，请求STDIN头。存放在u->request_bufs链接表里面。
如果是Proxy模块，ngx_http_proxy_create_request组件反向代理的头部啥的,放到u->request_bufs里面
FastCGI memcached  uwsgi  scgi proxy都会用到upstream模块
 */
static ngx_int_t
ngx_http_proxy_create_request(ngx_http_request_t *r) //ngx_http_upstream_init_request中执行
{
    size_t                          len, 
                                    uri_len, 
                                    loc_len,  //当前location的名字 location xxx {} 中的xxx的长度3
                                    body_len;
    uintptr_t                     escape;
    ngx_buf_t                    *b;
    ngx_str_t                     method;
    ngx_uint_t                    i, unparsed_uri;
    ngx_chain_t                  *cl, *body;
    ngx_list_part_t              *part;
    ngx_table_elt_t              *header;
    ngx_http_upstream_t          *u;
    ngx_http_proxy_ctx_t         *ctx;
    ngx_http_script_code_pt       code;
    ngx_http_proxy_headers_t     *headers;
    ngx_http_script_engine_t      e, le;
    ngx_http_proxy_loc_conf_t    *plcf;
    ngx_http_script_len_code_pt   lcode;

    u = r->upstream;//得到在ngx_http_proxy_handler前面创建的上游结构

    plcf = ngx_http_get_module_loc_conf(r, ngx_http_proxy_module); //获取proxy模块的相关配置信息

#if (NGX_HTTP_CACHE)
    headers = u->cacheable ? &plcf->headers_cache : &plcf->headers;
#else
    headers = &plcf->headers;
#endif

    if (u->method.len) {
        /* 客户端head请求会被转换为GET到后端 HEAD was changed to GET to cache response */
        method = u->method;
        method.len++;

    } else if (plcf->method.len) {
        method = plcf->method;

    } else {
        method = r->method_name;
        method.len++;
    }

    //获取proxy模块状态等上下文信息，
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (method.len == 5
        && ngx_strncasecmp(method.data, (u_char *) "HEAD ", 5) == 0)
    {
        ctx->head = 1;
    }

    //下面不断的对数据长度进行累加，算出要发送到后端的数据总共有多长，存在len变量里面
    len = method.len + sizeof(ngx_http_proxy_version) - 1 + sizeof(CRLF) - 1; //头部行长度

    escape = 0;
    loc_len = 0;
    unparsed_uri = 0;

    //下面处理一下uri长度以及location
    if (plcf->proxy_lengths && ctx->vars.uri.len) {//proxy_pass xxxx配置
        uri_len = ctx->vars.uri.len;

    }  else if (ctx->vars.uri.len == 0 && r->valid_unparsed_uri && r == r->main) {
        unparsed_uri = 1;
        uri_len = r->unparsed_uri.len;

    } else {
        loc_len = (r->valid_location && ctx->vars.uri.len) ?
                      plcf->location.len : 0;  //当前location的名字 location xxx {} 中的xxx的长度3

        if (r->quoted_uri || r->space_in_uri || r->internal) {
            escape = 2 * ngx_escape_uri(NULL, r->uri.data + loc_len,
                                        r->uri.len - loc_len, NGX_ESCAPE_URI);
        }

        /*
           proxy_pass  http://10.10.0.103:8080/tttxxsss; 
           浏览器输入:http://10.2.13.167/proxy1111/yangtest
           实际上到后端的uri会变为/tttxxsss/yangtest
           plcf->location:/proxy1111, ctx->vars.uri:/tttxxsss,  r->uri:/proxy1111/yangtest, r->args:, urilen:19
          */
        uri_len = ctx->vars.uri.len + r->uri.len - loc_len + escape
                  + sizeof("?") - 1 + r->args.len; //注意sizeof("?")是2字节
    }

    if (uri_len == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "zero length URI to proxy");
        return NGX_ERROR;
    }

    len += uri_len;//累加第一行，如: GET /XXX/Y.html HTTP/1.0

    ngx_memzero(&le, sizeof(ngx_http_script_engine_t));

    ngx_http_script_flush_no_cacheable_variables(r, plcf->body_flushes);
    ngx_http_script_flush_no_cacheable_variables(r, headers->flushes);

    if (plcf->body_lengths) { //计算proxy_set_body 设置的body字符串长度
        le.ip = plcf->body_lengths->elts;
        le.request = r;
        le.flushed = 1;
        body_len = 0;

        while (*(uintptr_t *) le.ip) {
            lcode = *(ngx_http_script_len_code_pt *) le.ip;
            body_len += lcode(&le);
        }

        ctx->internal_body_length = body_len; //如果配置了proxy_set_body，则internal_body_length为proxy_set_body设置的包体长度，否则为客户端请求包体长度
        len += body_len;

    } else if (r->headers_in.chunked && r->reading_body) {
        ctx->internal_body_length = -1;
        ctx->internal_chunked = 1;

    } else {
        //如果配置了proxy_set_body，则internal_body_length为proxy_set_body设置的包体长度，否则为客户端请求包体长度
        ctx->internal_body_length = r->headers_in.content_length_n;
    }

    //ngx_http_proxy_headers和proxy_set_header设置的头部信息里面的所有key:value值长度和
    le.ip = headers->lengths->elts;
    le.request = r;
    le.flushed = 1;

    while (*(uintptr_t *) le.ip) {
        while (*(uintptr_t *) le.ip) {
            lcode = *(ngx_http_script_len_code_pt *) le.ip;
            len += lcode(&le);
        }
        le.ip += sizeof(uintptr_t);
    }

    //把客户端发送过来的头部行key:value也计算进来，注意避免和前面的ngx_http_proxy_headers和proxy_set_header添加的重复
    if (plcf->upstream.pass_request_headers) {//是否要将HTTP请求头部的HEADER发送给后端，已HTTP_为前缀
        part = &r->headers_in.headers.part;
        header = part->elts;

        for (i = 0; /* void */; i++) {

            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                header = part->elts;
                i = 0;
            }

            /* 在ngx_http_proxy_loc_conf_t->headers中查找是否存在客户端请求行中的头部行信息r->headers_in.headers， 找到说明有重复，不继续len*/
            if (ngx_hash_find(&headers->hash, header[i].hash,
                              header[i].lowcase_key, header[i].key.len))
            {
                continue;
            }

            len += header[i].key.len + sizeof(": ") - 1
                + header[i].value.len + sizeof(CRLF) - 1; //计算ngx_http_proxy_headers proxy_set_header设置的头部行以及客户端发送过来的头部行的key:value总长度
        }
    }

    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NGX_ERROR;
    }

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    cl->buf = b;//老办法，将这块缓冲放入连接表头部


    /* 下面这部分把GET /XXX/Y.html HTTP/1.0拷贝到buf内存 */
    /* the request line */
    b->last = ngx_copy(b->last, method.data, method.len); //先把请求行中的GET POST HEAD等方法拷贝的b内存空间

    u->uri.data = b->last;

    if (plcf->proxy_lengths && ctx->vars.uri.len) {
        b->last = ngx_copy(b->last, ctx->vars.uri.data, ctx->vars.uri.len);

    } else if (unparsed_uri) {
        b->last = ngx_copy(b->last, r->unparsed_uri.data, r->unparsed_uri.len);

    } else {
        if (r->valid_location) {
            b->last = ngx_copy(b->last, ctx->vars.uri.data, ctx->vars.uri.len);
        }

        if (escape) { //将空格进行转移，
            ngx_escape_uri(b->last, r->uri.data + loc_len,
                           r->uri.len - loc_len, NGX_ESCAPE_URI);
            b->last += r->uri.len - loc_len + escape;

        } else {
            b->last = ngx_copy(b->last, r->uri.data + loc_len,
                               r->uri.len - loc_len);
        }

        if (r->args.len > 0) {
            *b->last++ = '?';
            b->last = ngx_copy(b->last, r->args.data, r->args.len);
        }
    }

    u->uri.len = b->last - u->uri.data;

    if (plcf->http_version == NGX_HTTP_VERSION_11) {
        b->last = ngx_cpymem(b->last, ngx_http_proxy_version_11,
                             sizeof(ngx_http_proxy_version_11) - 1);

    } else {
        b->last = ngx_cpymem(b->last, ngx_http_proxy_version,
                             sizeof(ngx_http_proxy_version) - 1);
    }


    /* 拷贝ngx_http_proxy_headers  proxy_set_header中设置的头部信息key:value到b内存中 */
    ngx_memzero(&e, sizeof(ngx_http_script_engine_t));

    e.ip = headers->values->elts;
    e.pos = b->last;
    e.request = r;
    e.flushed = 1;

    le.ip = headers->lengths->elts;

    while (*(uintptr_t *) le.ip) {
        lcode = *(ngx_http_script_len_code_pt *) le.ip;

        /* skip the header line name length */
        (void) lcode(&le);

        if (*(ngx_http_script_len_code_pt *) le.ip) {

            for (len = 0; *(uintptr_t *) le.ip; len += lcode(&le)) {
                lcode = *(ngx_http_script_len_code_pt *) le.ip;
            }

            e.skip = (len == sizeof(CRLF) - 1) ? 1 : 0;

        } else {
            e.skip = 0;
        }

        le.ip += sizeof(uintptr_t);

        while (*(uintptr_t *) e.ip) {
            code = *(ngx_http_script_code_pt *) e.ip;
            code((ngx_http_script_engine_t *) &e);
        }
        e.ip += sizeof(uintptr_t);
    }

    b->last = e.pos;


     /* 拷贝客户端请求中头部行信息key:value到b内存中 */
    if (plcf->upstream.pass_request_headers) {
        part = &r->headers_in.headers.part;
        header = part->elts;

        for (i = 0; /* void */; i++) {

            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                header = part->elts;
                i = 0;
            }

            //ngx_http_proxy_headers  proxy_set_header中设置的头部信息和客户端发送过来的重复，就不需要拷贝了
            if (ngx_hash_find(&headers->hash, header[i].hash,
                              header[i].lowcase_key, header[i].key.len))
            {
                continue;
            }

            b->last = ngx_copy(b->last, header[i].key.data, header[i].key.len);

            *b->last++ = ':'; *b->last++ = ' ';

            b->last = ngx_copy(b->last, header[i].value.data,
                               header[i].value.len);

            *b->last++ = CR; *b->last++ = LF;

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http proxy header: \"%V: %V\"",
                           &header[i].key, &header[i].value);
        }
    }


    /* add "\r\n" at the header end */
    *b->last++ = CR; *b->last++ = LF;//以一个空行结束

    //这里是指proxy_set_body额外增加的body数据添加到b内存中
    if (plcf->body_values) {
        e.ip = plcf->body_values->elts;
        e.pos = b->last;
        e.skip = 0;

        while (*(uintptr_t *) e.ip) {
            code = *(ngx_http_script_code_pt *) e.ip;
            code((ngx_http_script_engine_t *) &e);
        }

        b->last = e.pos;
    }

    //把header信息全部打印出来
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http proxy header:%N\"%*s\"",
                   (size_t) (b->last - b->pos), b->pos);

    if (r->request_body_no_buffering) { //不需要缓存，这直接把头部行部分发送出去

        u->request_bufs = cl;

        if (ctx->internal_chunked) {
            u->output.output_filter = ngx_http_proxy_body_output_filter;
            u->output.filter_ctx = r;
        }

    } else if (plcf->body_values == NULL && plcf->upstream.pass_request_body) { 
    //如果没有设置proxy_set_body包体，并且需要传送包体，则把客户端包体也拷贝到内存中

        body = u->request_bufs;
        u->request_bufs = cl;

        while (body) {
            b = ngx_alloc_buf(r->pool);
            if (b == NULL) {
                return NGX_ERROR;
            }

            ngx_memcpy(b, body->buf, sizeof(ngx_buf_t));

            cl->next = ngx_alloc_chain_link(r->pool);
            if (cl->next == NULL) {
                return NGX_ERROR;
            }

            cl = cl->next;
            cl->buf = b;

            body = body->next;
        }

    } else { //说明通过proxy_set_body设置了单独的包体
        u->request_bufs = cl;
    }

    b->flush = 1;
    cl->next = NULL;

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_reinit_request(ngx_http_request_t *r)
{
    ngx_http_proxy_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL) {
        return NGX_OK;
    }

    ctx->status.code = 0;
    ctx->status.count = 0;
    ctx->status.start = NULL;
    ctx->status.end = NULL;
    ctx->chunked.state = 0;

    r->upstream->process_header = ngx_http_proxy_process_status_line;
    r->upstream->pipe->input_filter = ngx_http_proxy_copy_filter;
    r->upstream->input_filter = ngx_http_proxy_non_buffered_copy_filter;
    r->state = 0;

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_body_output_filter(void *data, ngx_chain_t *in)
{
    ngx_http_request_t  *r = data;

    off_t                  size;
    u_char                *chunk;
    ngx_int_t              rc;
    ngx_buf_t             *b;
    ngx_chain_t           *out, *cl, *tl, **ll, **fl;
    ngx_http_proxy_ctx_t  *ctx;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "proxy output filter");

    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (in == NULL) {
        out = in;
        goto out;
    }

    out = NULL;
    ll = &out;

    if (!ctx->header_sent) {
        /* first buffer contains headers, pass it unmodified */

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "proxy output header");

        ctx->header_sent = 1;

        tl = ngx_alloc_chain_link(r->pool);
        if (tl == NULL) {
            return NGX_ERROR;
        }

        tl->buf = in->buf;
        *ll = tl;
        ll = &tl->next;

        in = in->next;

        if (in == NULL) {
            tl->next = NULL;
            goto out;
        }
    }

    size = 0;
    cl = in;
    fl = ll;

    for ( ;; ) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "proxy output chunk: %d", ngx_buf_size(cl->buf));

        size += ngx_buf_size(cl->buf);

        if (cl->buf->flush
            || cl->buf->sync
            || ngx_buf_in_memory(cl->buf)
            || cl->buf->in_file)
        {
            tl = ngx_alloc_chain_link(r->pool);
            if (tl == NULL) {
                return NGX_ERROR;
            }

            tl->buf = cl->buf;
            *ll = tl;
            ll = &tl->next;
        }

        if (cl->next == NULL) {
            break;
        }

        cl = cl->next;
    }

    if (size) {
        tl = ngx_chain_get_free_buf(r->pool, &ctx->free);
        if (tl == NULL) {
            return NGX_ERROR;
        }

        b = tl->buf;
        chunk = b->start;

        if (chunk == NULL) {
            /* the "0000000000000000" is 64-bit hexadecimal string */

            chunk = ngx_palloc(r->pool, sizeof("0000000000000000" CRLF) - 1);
            if (chunk == NULL) {
                return NGX_ERROR;
            }

            b->start = chunk;
            b->end = chunk + sizeof("0000000000000000" CRLF) - 1;
        }

        b->tag = (ngx_buf_tag_t) &ngx_http_proxy_body_output_filter;
        b->memory = 0;
        b->temporary = 1;
        b->pos = chunk;
        b->last = ngx_sprintf(chunk, "%xO" CRLF, size);

        tl->next = *fl;
        *fl = tl;
    }

    if (cl->buf->last_buf) {
        tl = ngx_chain_get_free_buf(r->pool, &ctx->free);
        if (tl == NULL) {
            return NGX_ERROR;
        }

        b = tl->buf;

        b->tag = (ngx_buf_tag_t) &ngx_http_proxy_body_output_filter;
        b->temporary = 0;
        b->memory = 1;
        b->last_buf = 1;
        b->pos = (u_char *) CRLF "0" CRLF CRLF;
        b->last = b->pos + 7;

        cl->buf->last_buf = 0;

        *ll = tl;

        if (size == 0) {
            b->pos += 2;
        }

    } else if (size > 0) {
        tl = ngx_chain_get_free_buf(r->pool, &ctx->free);
        if (tl == NULL) {
            return NGX_ERROR;
        }

        b = tl->buf;

        b->tag = (ngx_buf_tag_t) &ngx_http_proxy_body_output_filter;
        b->temporary = 0;
        b->memory = 1;
        b->pos = (u_char *) CRLF;
        b->last = b->pos + 2;

        *ll = tl;

    } else {
        *ll = NULL;
    }

out:

    rc = ngx_chain_writer(&r->upstream->writer, out);

    ngx_chain_update_chains(r->pool, &ctx->free, &ctx->busy, &out,
                            (ngx_buf_tag_t) &ngx_http_proxy_body_output_filter);

    return rc;
}


static ngx_int_t //ngx_http_upstream_process_header中执行
ngx_http_proxy_process_status_line(ngx_http_request_t *r)
{
    size_t                 len;
    ngx_int_t              rc;
    ngx_http_upstream_t   *u;
    ngx_http_proxy_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL) {
        return NGX_ERROR;
    }

    u = r->upstream;

    rc = ngx_http_parse_status_line(r, &u->buffer, &ctx->status);//解析状态行(响应行)，也就是第一行

    if (rc == NGX_AGAIN) {//如果数据还没有那么多
        return rc;
    }

    if (rc == NGX_ERROR) {

#if (NGX_HTTP_CACHE)

        if (r->cache) {
            r->http_version = NGX_HTTP_VERSION_9;
            return NGX_OK;
        }

#endif

        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent no valid HTTP/1.0 header");

#if 0
        if (u->accel) {
            return NGX_HTTP_UPSTREAM_INVALID_HEADER;
        }
#endif

        r->http_version = NGX_HTTP_VERSION_9;
        u->state->status = NGX_HTTP_OK;
        u->headers_in.connection_close = 1;

        return NGX_OK;
    }

    if (u->state && u->state->status == 0) {//有状态字段，就保存状态码
        u->state->status = ctx->status.code;
    }

    u->headers_in.status_n = ctx->status.code;

    len = ctx->status.end - ctx->status.start;
    u->headers_in.status_line.len = len;

    u->headers_in.status_line.data = ngx_pnalloc(r->pool, len);
    if (u->headers_in.status_line.data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(u->headers_in.status_line.data, ctx->status.start, len);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http proxy status %ui \"%V\"",
                   u->headers_in.status_n, &u->headers_in.status_line);

    if (ctx->status.http_version < NGX_HTTP_VERSION_11) {
        u->headers_in.connection_close = 1;
    }

    u->process_header = ngx_http_proxy_process_header;

    return ngx_http_proxy_process_header(r);
}


/*
//上游后端服务器模块的可读事件回调会调用ngx_http_upstream_process_header，然后调用process_header进行头部数据的解析了。
//注意这里只读取头部字段(读取头部字段有可能会读取到一部分包体)，没有读取body部分，那body部分再申明地方读取的呢,如:
//不管buffering是否打开，后端发送的头都不会被buffer，首先会发送header，然后才是body的发送，而body的发送就需要区分buffering选项了。
*/
static ngx_int_t //ngx_http_upstream_process_header中执行该函数
ngx_http_proxy_process_header(ngx_http_request_t *r)
{
    ngx_int_t                       rc;
    ngx_table_elt_t                *h;
    ngx_http_upstream_t            *u;
    ngx_http_proxy_ctx_t           *ctx;
    ngx_http_upstream_header_t     *hh;
    ngx_http_upstream_main_conf_t  *umcf;

    umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);

    for ( ;; ) {//启用cache的情况下，注意这时候buf的内容实际上已经在ngx_http_upstream_process_header中出去了为缓存如文件中预留的头部内存

        rc = ngx_http_parse_header_line(r, &r->upstream->buffer, 1);

        if (rc == NGX_OK) {//下面将这个新的头部字段保存起来，保存到headers_in.headers

            /* a header line has been parsed successfully */

            h = ngx_list_push(&r->upstream->headers_in.headers);
            if (h == NULL) {
                return NGX_ERROR;
            }

            h->hash = r->header_hash;

            h->key.len = r->header_name_end - r->header_name_start;
            h->value.len = r->header_end - r->header_start;

            h->key.data = ngx_pnalloc(r->pool,
                               h->key.len + 1 + h->value.len + 1 + h->key.len);
            if (h->key.data == NULL) {
                return NGX_ERROR;
            }

            h->value.data = h->key.data + h->key.len + 1;
            h->lowcase_key = h->key.data + h->key.len + 1 + h->value.len + 1; //key-value后面是key的小写字符串

            ngx_memcpy(h->key.data, r->header_name_start, h->key.len);
            h->key.data[h->key.len] = '\0';
            ngx_memcpy(h->value.data, r->header_start, h->value.len);
            h->value.data[h->value.len] = '\0';

            if (h->key.len == r->lowcase_index) {
                ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);

            } else {
                ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
            }

            hh = ngx_hash_find(&umcf->headers_in_hash, h->hash,
                               h->lowcase_key, h->key.len); //在ngx_http_upstream_headers_in查找是否与其中的成员匹配

            //ngx_http_upstream_headers_in中有匹配向，则执行对应的handler
            if (hh && hh->handler(r, h, hh->offset) != NGX_OK) {
                return NGX_ERROR;
            }

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http proxy header: \"%V: %V\"",
                           &h->key, &h->value);

            continue;
        }

        if (rc == NGX_HTTP_PARSE_HEADER_DONE) {//全部都已经解析完毕了，肯定是碰到空行\R\N了

            /* a whole header has been parsed successfully */

            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http proxy header done");

            /*
             * if no "Server" and "Date" in header line,
             * then add the special empty headers
             */
            /* 说明向客户端发送的头部行中必须包括server:  date: ，如果后端没有发送这两个字段，则nginx会添加空value给这两个头部行 */
            if (r->upstream->headers_in.server == NULL) { //如果后端没有发送server:xxx头部行，则直接添加server: value长度为0
                h = ngx_list_push(&r->upstream->headers_in.headers);
                if (h == NULL) {
                    return NGX_ERROR;
                }

                h->hash = ngx_hash(ngx_hash(ngx_hash(ngx_hash(
                                    ngx_hash('s', 'e'), 'r'), 'v'), 'e'), 'r');

                ngx_str_set(&h->key, "Server");
                ngx_str_null(&h->value);
                h->lowcase_key = (u_char *) "server";
            }

            if (r->upstream->headers_in.date == NULL) {// Date 发送HTTP消息的日期。例如：Date: Mon,10PR 18:42:51 GMT 
                h = ngx_list_push(&r->upstream->headers_in.headers);
                if (h == NULL) {
                    return NGX_ERROR;
                }

                h->hash = ngx_hash(ngx_hash(ngx_hash('d', 'a'), 't'), 'e');

                ngx_str_set(&h->key, "Date");
                ngx_str_null(&h->value);
                h->lowcase_key = (u_char *) "date";
            }

            /* clear content length if response is chunked */

            u = r->upstream;

            if (u->headers_in.chunked) { //chunked编码方式就不需要包含content-length: 头部行，包体长度有chunked报文格式指定包体内容长度
                u->headers_in.content_length_n = -1;
            }

            /*
             * set u->keepalive if response has no body; this allows to keep
             * connections alive in case of r->header_only or X-Accel-Redirect
             */

            ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

            if (u->headers_in.status_n == NGX_HTTP_NO_CONTENT
                || u->headers_in.status_n == NGX_HTTP_NOT_MODIFIED
                || ctx->head
                || (!u->headers_in.chunked
                    && u->headers_in.content_length_n == 0))
            {
                u->keepalive = !u->headers_in.connection_close;
            }

            if (u->headers_in.status_n == NGX_HTTP_SWITCHING_PROTOCOLS) {
                u->keepalive = 0;

                if (r->headers_in.upgrade) {//后端返回//HTTP/1.1 101的时候置1  
                    u->upgrade = 1;
                }
            }

            int keepalive_t = u->keepalive;
            ngx_log_debugall(r->connection->log, 0,
                      "upstream header recv ok, u->keepalive:%d", keepalive_t);

            //ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,  "yang test .... body:%*s", (size_t) (r->upstream->buffer.last - r->upstream->buffer.pos), r->upstream->buffer.pos);
            return NGX_OK;
        }

        if (rc == NGX_AGAIN) {
            return NGX_AGAIN;
        }

        /* there was error while a header line parsing */
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "upstream sent invalid header");

        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }
}


static ngx_int_t
ngx_http_proxy_input_filter_init(void *data)
{
    ngx_http_request_t    *r = data;
    ngx_http_upstream_t   *u;
    ngx_http_proxy_ctx_t  *ctx;

    u = r->upstream;
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_log_debug4(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http proxy filter init upstream status:%d is HEAD:%d chunked:%d content_length_n:%O",
                   u->headers_in.status_n, ctx->head, u->headers_in.chunked,
                   u->headers_in.content_length_n);

    /* as per RFC2616, 4.4 Message Length */

    if (u->headers_in.status_n == NGX_HTTP_NO_CONTENT
        || u->headers_in.status_n == NGX_HTTP_NOT_MODIFIED
        || ctx->head)
    {
        /* 1xx, 204, and 304 and replies to HEAD requests */
        /* no 1xx since we don't send Expect and Upgrade */

        u->pipe->length = 0;
        u->length = 0;
        u->keepalive = !u->headers_in.connection_close;

    } else if (u->headers_in.chunked) {
        /* chunked */

        u->pipe->input_filter = ngx_http_proxy_chunked_filter;
        u->pipe->length = 3; /* "0" LF LF */

        u->input_filter = ngx_http_proxy_non_buffered_chunked_filter;
        u->length = 1;

    } else if (u->headers_in.content_length_n == 0) {
        /* empty body: special case as filter won't be called */

        u->pipe->length = 0;
        u->length = 0;
        u->keepalive = !u->headers_in.connection_close;

    } else {
        /* content length or connection close */

        u->pipe->length = u->headers_in.content_length_n;
        u->length = u->headers_in.content_length_n;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_copy_filter(ngx_event_pipe_t *p, ngx_buf_t *buf)
{
    ngx_buf_t           *b;
    ngx_chain_t         *cl;
    ngx_http_request_t  *r;

    if (buf->pos == buf->last) {
        return NGX_OK;
    }

    cl = ngx_chain_get_free_buf(p->pool, &p->free);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    b = cl->buf;

    ngx_memcpy(b, buf, sizeof(ngx_buf_t));
    b->shadow = buf;
    b->tag = p->tag;
    b->last_shadow = 1;
    b->recycled = 1;
    buf->shadow = b;

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0, "input buf #%d", b->num);

    if (p->in) {
        *p->last_in = cl;
    } else {
        p->in = cl;
    }
    p->last_in = &cl->next;

    if (p->length == -1) {
        return NGX_OK;
    }

    p->length -= b->last - b->pos;

    if (p->length == 0) {
        r = p->input_ctx;
        p->upstream_done = 1;
        r->upstream->keepalive = !r->upstream->headers_in.connection_close;

    } else if (p->length < 0) {
        r = p->input_ctx;
        p->upstream_done = 1;

        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                      "upstream sent more data than specified in "
                      "\"Content-Length\" header");
    }

    int upstream_done = p->upstream_done;
    if(upstream_done)
        ngx_log_debugall(p->log, 0, "proxy copy filter upstream_done:%d", upstream_done);
    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_chunked_filter(ngx_event_pipe_t *p, ngx_buf_t *buf)
{
    ngx_int_t              rc;
    ngx_buf_t             *b, **prev;
    ngx_chain_t           *cl;
    ngx_http_request_t    *r;
    ngx_http_proxy_ctx_t  *ctx;

    if (buf->pos == buf->last) {
        return NGX_OK;
    }

    r = p->input_ctx;
    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL) {
        return NGX_ERROR;
    }

    b = NULL;
    prev = &buf->shadow;

    for ( ;; ) {

        rc = ngx_http_parse_chunked(r, buf, &ctx->chunked);

        if (rc == NGX_OK) {

            /* a chunk has been parsed successfully */

            cl = ngx_chain_get_free_buf(p->pool, &p->free);
            if (cl == NULL) {
                return NGX_ERROR;
            }

            b = cl->buf;

            ngx_memzero(b, sizeof(ngx_buf_t));

            b->pos = buf->pos;
            b->start = buf->start;
            b->end = buf->end;
            b->tag = p->tag;
            b->temporary = 1;
            b->recycled = 1;

            *prev = b;
            prev = &b->shadow;

            if (p->in) {
                *p->last_in = cl;
            } else {
                p->in = cl;
            }
            p->last_in = &cl->next;

            /* STUB */ b->num = buf->num;

            ngx_log_debug2(NGX_LOG_DEBUG_EVENT, p->log, 0,
                           "input buf #%d %p", b->num, b->pos);

            if (buf->last - buf->pos >= ctx->chunked.size) {

                buf->pos += (size_t) ctx->chunked.size;
                b->last = buf->pos;
                ctx->chunked.size = 0;

                continue;
            }

            ctx->chunked.size -= buf->last - buf->pos;
            buf->pos = buf->last;
            b->last = buf->last;

            continue;
        }

        if (rc == NGX_DONE) {

            /* a whole response has been parsed successfully */

            p->upstream_done = 1;
            r->upstream->keepalive = !r->upstream->headers_in.connection_close;

            break;
        }

        if (rc == NGX_AGAIN) {

            /* set p->length, minimal amount of data we want to see */

            p->length = ctx->chunked.length;

            break;
        }

        /* invalid response */

        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent invalid chunked response");

        return NGX_ERROR;
    }

    int upstream_done = p->upstream_done;
    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http proxy chunked state %d, length %d, p->upstream_done:%d",
                   ctx->chunked.state, p->length, upstream_done);

    if (b) {
        b->shadow = buf;
        b->last_shadow = 1;

        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "input buf %p %z", b->pos, b->last - b->pos);

        return NGX_OK;
    }

    /* there is no data record in the buf, add it to free chain */

    if (ngx_event_pipe_add_free_buf(p, buf) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_non_buffered_copy_filter(void *data, ssize_t bytes)
{
    ngx_http_request_t   *r = data;

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

    if (u->length == 0) {
        u->keepalive = !u->headers_in.connection_close;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_non_buffered_chunked_filter(void *data, ssize_t bytes)
{
    ngx_http_request_t   *r = data;

    ngx_int_t              rc;
    ngx_buf_t             *b, *buf;
    ngx_chain_t           *cl, **ll;
    ngx_http_upstream_t   *u;
    ngx_http_proxy_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL) {
        return NGX_ERROR;
    }

    u = r->upstream;
    buf = &u->buffer;

    buf->pos = buf->last;
    buf->last += bytes;

    for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
        ll = &cl->next;
    }

    for ( ;; ) {

        rc = ngx_http_parse_chunked(r, buf, &ctx->chunked);

        if (rc == NGX_OK) {

            /* a chunk has been parsed successfully */

            cl = ngx_chain_get_free_buf(r->pool, &u->free_bufs);
            if (cl == NULL) {
                return NGX_ERROR;
            }

            *ll = cl;
            ll = &cl->next;

            b = cl->buf;

            b->flush = 1;
            b->memory = 1;

            b->pos = buf->pos;
            b->tag = u->output.tag;

            if (buf->last - buf->pos >= ctx->chunked.size) {
                buf->pos += (size_t) ctx->chunked.size;
                b->last = buf->pos;
                ctx->chunked.size = 0;

            } else {
                ctx->chunked.size -= buf->last - buf->pos;
                buf->pos = buf->last;
                b->last = buf->last;
            }

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http proxy out buf %p %z",
                           b->pos, b->last - b->pos);

            continue;
        }

        if (rc == NGX_DONE) {

            /* a whole response has been parsed successfully */

            u->keepalive = !u->headers_in.connection_close;
            u->length = 0;

            break;
        }

        if (rc == NGX_AGAIN) {
            break;
        }

        /* invalid response */

        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "upstream sent invalid chunked response");

        return NGX_ERROR;
    }

    /* provide continuous buffer for subrequests in memory */

    if (r->subrequest_in_memory) {

        cl = u->out_bufs;

        if (cl) {
            buf->pos = cl->buf->pos;
        }

        buf->last = buf->pos;

        for (cl = u->out_bufs; cl; cl = cl->next) {
            ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http proxy in memory %p-%p %uz",
                           cl->buf->pos, cl->buf->last, ngx_buf_size(cl->buf));

            if (buf->last == cl->buf->pos) {
                buf->last = cl->buf->last;
                continue;
            }

            buf->last = ngx_movemem(buf->last, cl->buf->pos,
                                    cl->buf->last - cl->buf->pos);

            cl->buf->pos = buf->last - (cl->buf->last - cl->buf->pos);
            cl->buf->last = buf->last;
        }
    }

    return NGX_OK;
}


static void
ngx_http_proxy_abort_request(ngx_http_request_t *r)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "abort http proxy request");

    return;
}


static void
ngx_http_proxy_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "finalize http proxy request");

    return;
}


static ngx_int_t
ngx_http_proxy_host_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_proxy_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->len = ctx->vars.host_header.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = ctx->vars.host_header.data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_port_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_proxy_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->len = ctx->vars.port.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = ctx->vars.port.data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_add_x_forwarded_for_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    size_t             len;
    u_char            *p;
    ngx_uint_t         i, n;
    ngx_table_elt_t  **h;

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    n = r->headers_in.x_forwarded_for.nelts;
    h = r->headers_in.x_forwarded_for.elts;

    len = 0;

    for (i = 0; i < n; i++) {
        len += h[i]->value.len + sizeof(", ") - 1;
    }

    if (len == 0) {
        v->len = r->connection->addr_text.len;
        v->data = r->connection->addr_text.data;
        return NGX_OK;
    }

    len += r->connection->addr_text.len;

    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->len = len;
    v->data = p;

    for (i = 0; i < n; i++) {
        p = ngx_copy(p, h[i]->value.data, h[i]->value.len);
        *p++ = ','; *p++ = ' ';
    }

    ngx_memcpy(p, r->connection->addr_text.data, r->connection->addr_text.len);

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_internal_body_length_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_proxy_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL || ctx->internal_body_length < 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    v->data = ngx_pnalloc(r->pool, NGX_OFF_T_LEN);

    if (v->data == NULL) {
        return NGX_ERROR;
    }

    v->len = ngx_sprintf(v->data, "%O", ctx->internal_body_length) - v->data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_internal_chunked_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_proxy_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_proxy_module);

    if (ctx == NULL || !ctx->internal_chunked) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    v->data = (u_char *) "chunked";
    v->len = sizeof("chunked") - 1;

    return NGX_OK;
}

/*
location /proxy1/ {			
    proxy_pass  http://10.10.0.103:8080/; 		
}

如果url为http://10.2.13.167/proxy1/，则ngx_http_upstream_rewrite_location处理后，
后端返回Location: http://10.10.0.103:8080/secure/MyJiraHome.jspa
则实际发送给浏览器客户端的headers_out.headers.location为http://10.2.13.167/proxy1/secure/MyJiraHome.jspa
*/
//ngx_http_upstream_rewrite_location->ngx_http_proxy_rewrite_redirect中执行
//获取新的重定向的新的uri解析出来，存入h->value(也就是ngx_http_upstream_headers_in_t->location中，见)
static ngx_int_t
ngx_http_proxy_rewrite_redirect(ngx_http_request_t *r, ngx_table_elt_t *h,
    size_t prefix)
{
    size_t                      len;
    ngx_int_t                   rc;
    ngx_uint_t                  i;
    ngx_http_proxy_rewrite_t   *pr;
    ngx_http_proxy_loc_conf_t  *plcf;

    plcf = ngx_http_get_module_loc_conf(r, ngx_http_proxy_module);

    pr = plcf->redirects->elts;

    if (pr == NULL) {
        return NGX_DECLINED;
    }

    len = h->value.len - prefix;

    for (i = 0; i < plcf->redirects->nelts; i++) {
        rc = pr[i].handler(r, h, prefix, len, &pr[i]); //ngx_http_proxy_rewrite_complex_handler

        if (rc != NGX_DECLINED) {
            return rc;
        }
    }

    return NGX_DECLINED;
}


static ngx_int_t
ngx_http_proxy_rewrite_cookie(ngx_http_request_t *r, ngx_table_elt_t *h)
{
    size_t                      prefix;
    u_char                     *p;
    ngx_int_t                   rc, rv;
    ngx_http_proxy_loc_conf_t  *plcf;

    p = (u_char *) ngx_strchr(h->value.data, ';');
    if (p == NULL) {
        return NGX_DECLINED;
    }

    prefix = p + 1 - h->value.data;

    rv = NGX_DECLINED;

    plcf = ngx_http_get_module_loc_conf(r, ngx_http_proxy_module);

    if (plcf->cookie_domains) {
        p = ngx_strcasestrn(h->value.data + prefix, "domain=", 7 - 1);

        if (p) {
            rc = ngx_http_proxy_rewrite_cookie_value(r, h, p + 7,
                                                     plcf->cookie_domains);
            if (rc == NGX_ERROR) {
                return NGX_ERROR;
            }

            if (rc != NGX_DECLINED) {
                rv = rc;
            }
        }
    }

    if (plcf->cookie_paths) {
        p = ngx_strcasestrn(h->value.data + prefix, "path=", 5 - 1);

        if (p) {
            rc = ngx_http_proxy_rewrite_cookie_value(r, h, p + 5,
                                                     plcf->cookie_paths);
            if (rc == NGX_ERROR) {
                return NGX_ERROR;
            }

            if (rc != NGX_DECLINED) {
                rv = rc;
            }
        }
    }

    return rv;
}


static ngx_int_t
ngx_http_proxy_rewrite_cookie_value(ngx_http_request_t *r, ngx_table_elt_t *h,
    u_char *value, ngx_array_t *rewrites)
{
    size_t                     len, prefix;
    u_char                    *p;
    ngx_int_t                  rc;
    ngx_uint_t                 i;
    ngx_http_proxy_rewrite_t  *pr;

    prefix = value - h->value.data;

    p = (u_char *) ngx_strchr(value, ';');

    len = p ? (size_t) (p - value) : (h->value.len - prefix);

    pr = rewrites->elts;

    for (i = 0; i < rewrites->nelts; i++) {
        rc = pr[i].handler(r, h, prefix, len, &pr[i]);

        if (rc != NGX_DECLINED) {
            return rc;
        }
    }

    return NGX_DECLINED;
}

/*
location /proxy1/ {			
    proxy_pass  http://10.10.0.103:8080/; 		
}

如果url为http://10.2.13.167/proxy1/，则ngx_http_upstream_rewrite_location处理后，
后端返回Location: http://10.10.0.103:8080/secure/MyJiraHome.jspa
则实际发送给浏览器客户端的headers_out.headers.location为http://10.2.13.167/proxy1/secure/MyJiraHome.jspa
*/
//ngx_http_upstream_rewrite_location->ngx_http_proxy_rewrite_redirect中执行
//获取新的重定向的新的uri解析出来，存入h->value
static ngx_int_t
ngx_http_proxy_rewrite_complex_handler(ngx_http_request_t *r,
    ngx_table_elt_t *h, size_t prefix, size_t len, ngx_http_proxy_rewrite_t *pr)
{
    ngx_str_t  pattern, replacement;

    //解析proxy_redirect /xxx1/$adfa /xxx2/$dgg中的/xxx1/$adfa为完整字符串到pattern
    if (ngx_http_complex_value(r, &pr->pattern.complex, &pattern) != NGX_OK) { 
        return NGX_ERROR;
    }

    if (pattern.len > len
        || ngx_rstrncmp(h->value.data + prefix, pattern.data,
                        pattern.len) != 0)
    {
        return NGX_DECLINED;
    }

    //解析proxy_redirect /xxx1/$adfa /xxx2/$dgg中的/xxx2/$dgg为完整字符串到replacement
    if (ngx_http_complex_value(r, &pr->replacement, &replacement) != NGX_OK) {
        return NGX_ERROR;
    }

    return ngx_http_proxy_rewrite(r, h, prefix, pattern.len, &replacement);
}


#if (NGX_PCRE)

static ngx_int_t
ngx_http_proxy_rewrite_regex_handler(ngx_http_request_t *r, ngx_table_elt_t *h,
    size_t prefix, size_t len, ngx_http_proxy_rewrite_t *pr)
{
    ngx_str_t  pattern, replacement;

    pattern.len = len;
    pattern.data = h->value.data + prefix;

    if (ngx_http_regex_exec(r, pr->pattern.regex, &pattern) != NGX_OK) {
        return NGX_DECLINED;
    }

    if (ngx_http_complex_value(r, &pr->replacement, &replacement) != NGX_OK) {
        return NGX_ERROR;
    }

    if (prefix == 0 && h->value.len == len) {
        h->value = replacement;
        return NGX_OK;
    }

    return ngx_http_proxy_rewrite(r, h, prefix, len, &replacement);
}

#endif


static ngx_int_t
ngx_http_proxy_rewrite_domain_handler(ngx_http_request_t *r,
    ngx_table_elt_t *h, size_t prefix, size_t len, ngx_http_proxy_rewrite_t *pr)
{
    u_char     *p;
    ngx_str_t   pattern, replacement;

    if (ngx_http_complex_value(r, &pr->pattern.complex, &pattern) != NGX_OK) {
        return NGX_ERROR;
    }

    p = h->value.data + prefix;

    if (p[0] == '.') {
        p++;
        prefix++;
        len--;
    }

    if (pattern.len != len || ngx_rstrncasecmp(pattern.data, p, len) != 0) {
        return NGX_DECLINED;
    }

    if (ngx_http_complex_value(r, &pr->replacement, &replacement) != NGX_OK) {
        return NGX_ERROR;
    }

    return ngx_http_proxy_rewrite(r, h, prefix, len, &replacement);
}

//拷贝replacement
static ngx_int_t
ngx_http_proxy_rewrite(ngx_http_request_t *r, ngx_table_elt_t *h, size_t prefix,
    size_t len, ngx_str_t *replacement)
{
    u_char  *p, *data;
    size_t   new_len;

    new_len = replacement->len + h->value.len - len;

    if (replacement->len > len) {

        data = ngx_pnalloc(r->pool, new_len + 1);
        if (data == NULL) {
            return NGX_ERROR;
        }

        p = ngx_copy(data, h->value.data, prefix);
        p = ngx_copy(p, replacement->data, replacement->len);

        ngx_memcpy(p, h->value.data + prefix + len,
                   h->value.len - len - prefix + 1);

        h->value.data = data;

    } else {
        p = ngx_copy(h->value.data + prefix, replacement->data,
                     replacement->len);

        ngx_memmove(p, h->value.data + prefix + len,
                    h->value.len - len - prefix + 1);
    }

    h->value.len = new_len;

    return NGX_OK;
}


static ngx_int_t
ngx_http_proxy_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var, *v;

    for (v = ngx_http_proxy_vars; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}


static void *
ngx_http_proxy_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_proxy_main_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_proxy_main_conf_t));
    if (conf == NULL) {
        return NULL;
    }

#if (NGX_HTTP_CACHE)
    if (ngx_array_init(&conf->caches, cf->pool, 4,
                       sizeof(ngx_http_file_cache_t *))
        != NGX_OK)
    {
        return NULL;
    }
#endif

    return conf;
}


static void *
ngx_http_proxy_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_proxy_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_proxy_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->upstream.bufs.num = 0;
     *     conf->upstream.ignore_headers = 0;
     *     conf->upstream.next_upstream = 0;
     *     conf->upstream.cache_zone = NULL;
     *     conf->upstream.cache_use_stale = 0;
     *     conf->upstream.cache_methods = 0;
     *     conf->upstream.temp_path = NULL;
     *     conf->upstream.hide_headers_hash = { NULL, 0 };
     *     conf->upstream.uri = { 0, NULL };
     *     conf->upstream.location = NULL;
     *     conf->upstream.store_lengths = NULL;
     *     conf->upstream.store_values = NULL;
     *     conf->upstream.ssl_name = NULL;
     *
     *     conf->method = { 0, NULL };
     *     conf->headers_source = NULL;
     *     conf->headers.lengths = NULL;
     *     conf->headers.values = NULL;
     *     conf->headers.hash = { NULL, 0 };
     *     conf->headers_cache.lengths = NULL;
     *     conf->headers_cache.values = NULL;
     *     conf->headers_cache.hash = { NULL, 0 };
     *     conf->body_lengths = NULL;
     *     conf->body_values = NULL;
     *     conf->body_source = { 0, NULL };
     *     conf->redirects = NULL;
     *     conf->ssl = 0;
     *     conf->ssl_protocols = 0;
     *     conf->ssl_ciphers = { 0, NULL };
     *     conf->ssl_trusted_certificate = { 0, NULL };
     *     conf->ssl_crl = { 0, NULL };
     *     conf->ssl_certificate = { 0, NULL };
     *     conf->ssl_certificate_key = { 0, NULL };
     */

    conf->upstream.store = NGX_CONF_UNSET;
    conf->upstream.store_access = NGX_CONF_UNSET_UINT;
    conf->upstream.next_upstream_tries = NGX_CONF_UNSET_UINT;
    conf->upstream.buffering = NGX_CONF_UNSET;
    conf->upstream.request_buffering = NGX_CONF_UNSET;
    conf->upstream.ignore_client_abort = NGX_CONF_UNSET;
    conf->upstream.force_ranges = NGX_CONF_UNSET;

    conf->upstream.local = NGX_CONF_UNSET_PTR;

    conf->upstream.connect_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.send_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.read_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.next_upstream_timeout = NGX_CONF_UNSET_MSEC;

    conf->upstream.send_lowat = NGX_CONF_UNSET_SIZE;
    conf->upstream.buffer_size = NGX_CONF_UNSET_SIZE;
    conf->upstream.limit_rate = NGX_CONF_UNSET_SIZE;

    conf->upstream.busy_buffers_size_conf = NGX_CONF_UNSET_SIZE;
    conf->upstream.max_temp_file_size_conf = NGX_CONF_UNSET_SIZE;
    conf->upstream.temp_file_write_size_conf = NGX_CONF_UNSET_SIZE;

    conf->upstream.pass_request_headers = NGX_CONF_UNSET;
    conf->upstream.pass_request_body = NGX_CONF_UNSET;

#if (NGX_HTTP_CACHE)
    conf->upstream.cache = NGX_CONF_UNSET;
    conf->upstream.cache_min_uses = NGX_CONF_UNSET_UINT;
    conf->upstream.cache_bypass = NGX_CONF_UNSET_PTR;
    conf->upstream.no_cache = NGX_CONF_UNSET_PTR;
    conf->upstream.cache_valid = NGX_CONF_UNSET_PTR;
    conf->upstream.cache_lock = NGX_CONF_UNSET;
    conf->upstream.cache_lock_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.cache_lock_age = NGX_CONF_UNSET_MSEC;
    conf->upstream.cache_revalidate = NGX_CONF_UNSET;
#endif

    conf->upstream.hide_headers = NGX_CONF_UNSET_PTR;
    conf->upstream.pass_headers = NGX_CONF_UNSET_PTR;

    conf->upstream.intercept_errors = NGX_CONF_UNSET;

#if (NGX_HTTP_SSL)
    conf->upstream.ssl_session_reuse = NGX_CONF_UNSET;
    conf->upstream.ssl_server_name = NGX_CONF_UNSET;
    conf->upstream.ssl_verify = NGX_CONF_UNSET;
    conf->ssl_verify_depth = NGX_CONF_UNSET_UINT;
    conf->ssl_passwords = NGX_CONF_UNSET_PTR;
#endif

    /* "proxy_cyclic_temp_file" is disabled */
    conf->upstream.cyclic_temp_file = 0;

    conf->redirect = NGX_CONF_UNSET;
    conf->upstream.change_buffering = 1;

    conf->cookie_domains = NGX_CONF_UNSET_PTR;
    conf->cookie_paths = NGX_CONF_UNSET_PTR;

    conf->http_version = NGX_CONF_UNSET_UINT;

    conf->headers_hash_max_size = NGX_CONF_UNSET_UINT;
    conf->headers_hash_bucket_size = NGX_CONF_UNSET_UINT;

    ngx_str_set(&conf->upstream.module, "proxy");

    return conf;
}


static char *
ngx_http_proxy_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_proxy_loc_conf_t *prev = parent;
    ngx_http_proxy_loc_conf_t *conf = child;

    u_char                     *p;
    size_t                      size;
    ngx_int_t                   rc;
    ngx_hash_init_t             hash;
    ngx_http_core_loc_conf_t   *clcf;
    ngx_http_proxy_rewrite_t   *pr;
    ngx_http_script_compile_t   sc;

#if (NGX_HTTP_CACHE)

    if (conf->upstream.store > 0) {
        conf->upstream.cache = 0;
    }

    if (conf->upstream.cache > 0) {
        conf->upstream.store = 0;
    }

#endif

    if (conf->upstream.store == NGX_CONF_UNSET) {
        ngx_conf_merge_value(conf->upstream.store,
                              prev->upstream.store, 0);

        conf->upstream.store_lengths = prev->upstream.store_lengths;
        conf->upstream.store_values = prev->upstream.store_values;
    }

    ngx_conf_merge_uint_value(conf->upstream.store_access,
                              prev->upstream.store_access, 0600);

    ngx_conf_merge_uint_value(conf->upstream.next_upstream_tries,
                              prev->upstream.next_upstream_tries, 0);

    ngx_conf_merge_value(conf->upstream.buffering,
                              prev->upstream.buffering, 1);

    ngx_conf_merge_value(conf->upstream.request_buffering,
                              prev->upstream.request_buffering, 1);

    ngx_conf_merge_value(conf->upstream.ignore_client_abort,
                              prev->upstream.ignore_client_abort, 0);

    ngx_conf_merge_value(conf->upstream.force_ranges,
                              prev->upstream.force_ranges, 0);

    ngx_conf_merge_ptr_value(conf->upstream.local,
                              prev->upstream.local, NULL);

    ngx_conf_merge_msec_value(conf->upstream.connect_timeout,
                              prev->upstream.connect_timeout, 60000);

    ngx_conf_merge_msec_value(conf->upstream.send_timeout,
                              prev->upstream.send_timeout, 60000);

    ngx_conf_merge_msec_value(conf->upstream.read_timeout,
                              prev->upstream.read_timeout, 60000);

    ngx_conf_merge_msec_value(conf->upstream.next_upstream_timeout,
                              prev->upstream.next_upstream_timeout, 0);

    ngx_conf_merge_size_value(conf->upstream.send_lowat,
                              prev->upstream.send_lowat, 0);

    ngx_conf_merge_size_value(conf->upstream.buffer_size,
                              prev->upstream.buffer_size,
                              (size_t) ngx_pagesize);

    ngx_conf_merge_size_value(conf->upstream.limit_rate,
                              prev->upstream.limit_rate, 0);

    ngx_conf_merge_bufs_value(conf->upstream.bufs, prev->upstream.bufs,
                              8, ngx_pagesize);

    if (conf->upstream.bufs.num < 2) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "there must be at least 2 \"proxy_buffers\"");
        return NGX_CONF_ERROR;
    }


    size = conf->upstream.buffer_size;
    if (size < conf->upstream.bufs.size) {
        size = conf->upstream.bufs.size;
    }


    ngx_conf_merge_size_value(conf->upstream.busy_buffers_size_conf,
                              prev->upstream.busy_buffers_size_conf,
                              NGX_CONF_UNSET_SIZE);

    if (conf->upstream.busy_buffers_size_conf == NGX_CONF_UNSET_SIZE) {
        conf->upstream.busy_buffers_size = 2 * size;
    } else {
        conf->upstream.busy_buffers_size =
                                         conf->upstream.busy_buffers_size_conf;
    }

    if (conf->upstream.busy_buffers_size < size) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
             "\"proxy_busy_buffers_size\" must be equal to or greater than "
             "the maximum of the value of \"proxy_buffer_size\" and "
             "one of the \"proxy_buffers\"");

        return NGX_CONF_ERROR;
    }

    if (conf->upstream.busy_buffers_size
        > (conf->upstream.bufs.num - 1) * conf->upstream.bufs.size)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
             "\"proxy_busy_buffers_size\" must be less than "
             "the size of all \"proxy_buffers\" minus one buffer");

        return NGX_CONF_ERROR;
    }


    ngx_conf_merge_size_value(conf->upstream.temp_file_write_size_conf,
                              prev->upstream.temp_file_write_size_conf,
                              NGX_CONF_UNSET_SIZE);

    if (conf->upstream.temp_file_write_size_conf == NGX_CONF_UNSET_SIZE) {
        conf->upstream.temp_file_write_size = 2 * size;
    } else {
        conf->upstream.temp_file_write_size =
                                      conf->upstream.temp_file_write_size_conf;
    }

    if (conf->upstream.temp_file_write_size < size) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
             "\"proxy_temp_file_write_size\" must be equal to or greater "
             "than the maximum of the value of \"proxy_buffer_size\" and "
             "one of the \"proxy_buffers\"");

        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_size_value(conf->upstream.max_temp_file_size_conf,
                              prev->upstream.max_temp_file_size_conf,
                              NGX_CONF_UNSET_SIZE);

    if (conf->upstream.max_temp_file_size_conf == NGX_CONF_UNSET_SIZE) {
        conf->upstream.max_temp_file_size = 1024 * 1024 * 1024;
    } else {
        conf->upstream.max_temp_file_size =
                                        conf->upstream.max_temp_file_size_conf;
    }

    if (conf->upstream.max_temp_file_size != 0
        && conf->upstream.max_temp_file_size < size)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
             "\"proxy_max_temp_file_size\" must be equal to zero to disable "
             "temporary files usage or must be equal to or greater than "
             "the maximum of the value of \"proxy_buffer_size\" and "
             "one of the \"proxy_buffers\"");

        return NGX_CONF_ERROR;
    }


    ngx_conf_merge_bitmask_value(conf->upstream.ignore_headers,
                              prev->upstream.ignore_headers,
                              NGX_CONF_BITMASK_SET);


    ngx_conf_merge_bitmask_value(conf->upstream.next_upstream,
                              prev->upstream.next_upstream,
                              (NGX_CONF_BITMASK_SET
                               |NGX_HTTP_UPSTREAM_FT_ERROR
                               |NGX_HTTP_UPSTREAM_FT_TIMEOUT));

    if (conf->upstream.next_upstream & NGX_HTTP_UPSTREAM_FT_OFF) {
        conf->upstream.next_upstream = NGX_CONF_BITMASK_SET
                                       |NGX_HTTP_UPSTREAM_FT_OFF;
    }

    if (ngx_conf_merge_path_value(cf, &conf->upstream.temp_path,
                              prev->upstream.temp_path,
                              &ngx_http_proxy_temp_path)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }


#if (NGX_HTTP_CACHE)

    if (conf->upstream.cache == NGX_CONF_UNSET) {
        ngx_conf_merge_value(conf->upstream.cache,
                              prev->upstream.cache, 0);

        conf->upstream.cache_zone = prev->upstream.cache_zone;
        conf->upstream.cache_value = prev->upstream.cache_value;
    }

    //proxy_cache abc必须和proxy_cache_path xxx keys_zone=abc:10m;一起，否则在ngx_http_proxy_merge_loc_conf会失败，因为没有为该abc创建ngx_http_file_cache_t
    if (conf->upstream.cache_zone && conf->upstream.cache_zone->data == NULL) {
        ngx_shm_zone_t  *shm_zone;

        shm_zone = conf->upstream.cache_zone;

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"proxy_cache\" zone \"%V\" is unknown",
                           &shm_zone->shm.name);

        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_uint_value(conf->upstream.cache_min_uses,
                              prev->upstream.cache_min_uses, 1);

    ngx_conf_merge_bitmask_value(conf->upstream.cache_use_stale,
                              prev->upstream.cache_use_stale,
                              (NGX_CONF_BITMASK_SET
                               |NGX_HTTP_UPSTREAM_FT_OFF));

    if (conf->upstream.cache_use_stale & NGX_HTTP_UPSTREAM_FT_OFF) {
        conf->upstream.cache_use_stale = NGX_CONF_BITMASK_SET
                                         |NGX_HTTP_UPSTREAM_FT_OFF;
    }

    if (conf->upstream.cache_use_stale & NGX_HTTP_UPSTREAM_FT_ERROR) {
        conf->upstream.cache_use_stale |= NGX_HTTP_UPSTREAM_FT_NOLIVE;
    }

    if (conf->upstream.cache_methods == 0) {
        conf->upstream.cache_methods = prev->upstream.cache_methods;
    }

    conf->upstream.cache_methods |= NGX_HTTP_GET|NGX_HTTP_HEAD;

    ngx_conf_merge_ptr_value(conf->upstream.cache_bypass,
                             prev->upstream.cache_bypass, NULL);

    ngx_conf_merge_ptr_value(conf->upstream.no_cache,
                             prev->upstream.no_cache, NULL);

    ngx_conf_merge_ptr_value(conf->upstream.cache_valid,
                             prev->upstream.cache_valid, NULL);

    if (conf->cache_key.value.data == NULL) {
        conf->cache_key = prev->cache_key;
    }

    ngx_conf_merge_value(conf->upstream.cache_lock,
                              prev->upstream.cache_lock, 0);

    ngx_conf_merge_msec_value(conf->upstream.cache_lock_timeout,
                              prev->upstream.cache_lock_timeout, 5000);

    ngx_conf_merge_msec_value(conf->upstream.cache_lock_age,
                              prev->upstream.cache_lock_age, 5000);

    ngx_conf_merge_value(conf->upstream.cache_revalidate,
                              prev->upstream.cache_revalidate, 0);

#endif

    ngx_conf_merge_str_value(conf->method, prev->method, "");

    if (conf->method.len
        && conf->method.data[conf->method.len - 1] != ' ')
    {
        conf->method.data[conf->method.len] = ' ';
        conf->method.len++;
    }

    ngx_conf_merge_value(conf->upstream.pass_request_headers,
                              prev->upstream.pass_request_headers, 1);
    ngx_conf_merge_value(conf->upstream.pass_request_body,
                              prev->upstream.pass_request_body, 1);

    ngx_conf_merge_value(conf->upstream.intercept_errors,
                              prev->upstream.intercept_errors, 0);

#if (NGX_HTTP_SSL)

    ngx_conf_merge_value(conf->upstream.ssl_session_reuse,
                              prev->upstream.ssl_session_reuse, 1);

    ngx_conf_merge_bitmask_value(conf->ssl_protocols, prev->ssl_protocols,
                                 (NGX_CONF_BITMASK_SET|NGX_SSL_TLSv1
                                  |NGX_SSL_TLSv1_1|NGX_SSL_TLSv1_2));

    ngx_conf_merge_str_value(conf->ssl_ciphers, prev->ssl_ciphers,
                             "DEFAULT");

    if (conf->upstream.ssl_name == NULL) {
        conf->upstream.ssl_name = prev->upstream.ssl_name;
    }

    ngx_conf_merge_value(conf->upstream.ssl_server_name,
                              prev->upstream.ssl_server_name, 0);
    ngx_conf_merge_value(conf->upstream.ssl_verify,
                              prev->upstream.ssl_verify, 0);
    ngx_conf_merge_uint_value(conf->ssl_verify_depth,
                              prev->ssl_verify_depth, 1);
    ngx_conf_merge_str_value(conf->ssl_trusted_certificate,
                              prev->ssl_trusted_certificate, "");
    ngx_conf_merge_str_value(conf->ssl_crl, prev->ssl_crl, "");

    ngx_conf_merge_str_value(conf->ssl_certificate,
                              prev->ssl_certificate, "");
    ngx_conf_merge_str_value(conf->ssl_certificate_key,
                              prev->ssl_certificate_key, "");
    ngx_conf_merge_ptr_value(conf->ssl_passwords, prev->ssl_passwords, NULL);

    if (conf->ssl && ngx_http_proxy_set_ssl(cf, conf) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

#endif

    ngx_conf_merge_value(conf->redirect, prev->redirect, 1);

    if (conf->redirect) {

        if (conf->redirects == NULL) {
            conf->redirects = prev->redirects;
        }

        //ngx_http_proxy_redirect和ngx_http_proxy_rewrite_redirect配合阅读，他们一起确定重定向后的地址
        if (conf->redirects == NULL && conf->url.data) {
        //ngx_http_proxy_merge_loc_conf该if里面内容和ngx_http_proxy_redirect里面的proxy_redirect default一样，也就是说proxy_redirect默认为default
            conf->redirects = ngx_array_create(cf->pool, 1,
                                             sizeof(ngx_http_proxy_rewrite_t));
            if (conf->redirects == NULL) {
                return NGX_CONF_ERROR;
            }

            pr = ngx_array_push(conf->redirects);
            if (pr == NULL) {
                return NGX_CONF_ERROR;
            }

            ngx_memzero(&pr->pattern.complex,
                        sizeof(ngx_http_complex_value_t));

            ngx_memzero(&pr->replacement, sizeof(ngx_http_complex_value_t));

            pr->handler = ngx_http_proxy_rewrite_complex_handler;

            if (conf->vars.uri.len) {
                pr->pattern.complex.value = conf->url; 
                pr->replacement.value = conf->location;

            } else {
                pr->pattern.complex.value.len = conf->url.len
                                                + sizeof("/") - 1;

                p = ngx_pnalloc(cf->pool, pr->pattern.complex.value.len);
                if (p == NULL) {
                    return NGX_CONF_ERROR;
                }

                pr->pattern.complex.value.data = p;

                p = ngx_cpymem(p, conf->url.data, conf->url.len);
                *p = '/';

                ngx_str_set(&pr->replacement.value, "/");
            }
        }
    }

    ngx_conf_merge_ptr_value(conf->cookie_domains, prev->cookie_domains, NULL);

    ngx_conf_merge_ptr_value(conf->cookie_paths, prev->cookie_paths, NULL);

    ngx_conf_merge_uint_value(conf->http_version, prev->http_version,
                              NGX_HTTP_VERSION_10);

    ngx_conf_merge_uint_value(conf->headers_hash_max_size,
                              prev->headers_hash_max_size, 512);

    ngx_conf_merge_uint_value(conf->headers_hash_bucket_size,
                              prev->headers_hash_bucket_size, 64);

    conf->headers_hash_bucket_size = ngx_align(conf->headers_hash_bucket_size,
                                               ngx_cacheline_size);

    hash.max_size = conf->headers_hash_max_size;
    hash.bucket_size = conf->headers_hash_bucket_size;
    hash.name = "proxy_headers_hash";

    if (ngx_http_upstream_hide_headers_hash(cf, &conf->upstream,
            &prev->upstream, ngx_http_proxy_hide_headers, &hash)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    if (clcf->noname
        && conf->upstream.upstream == NULL && conf->proxy_lengths == NULL)
    {
        conf->upstream.upstream = prev->upstream.upstream;
        conf->location = prev->location;
        conf->vars = prev->vars;

        conf->proxy_lengths = prev->proxy_lengths;
        conf->proxy_values = prev->proxy_values;

#if (NGX_HTTP_SSL)
        conf->upstream.ssl = prev->upstream.ssl;
#endif
    }

    if (clcf->lmt_excpt && clcf->handler == NULL
        && (conf->upstream.upstream || conf->proxy_lengths))
    {
        clcf->handler = ngx_http_proxy_handler;
    }

    if (conf->body_source.data == NULL) {
        conf->body_flushes = prev->body_flushes;
        conf->body_source = prev->body_source;
        conf->body_lengths = prev->body_lengths;
        conf->body_values = prev->body_values;
    }

    if (conf->body_source.data && conf->body_lengths == NULL) {

        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = &conf->body_source;
        sc.flushes = &conf->body_flushes;
        sc.lengths = &conf->body_lengths;
        sc.values = &conf->body_values;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    if (conf->headers_source == NULL) {
        conf->headers = prev->headers;
#if (NGX_HTTP_CACHE)
        conf->headers_cache = prev->headers_cache;
#endif
        conf->headers_source = prev->headers_source;
    }

    rc = ngx_http_proxy_init_headers(cf, conf, &conf->headers,
                                     ngx_http_proxy_headers);
    if (rc != NGX_OK) {
        return NGX_CONF_ERROR;
    }

#if (NGX_HTTP_CACHE)

    if (conf->upstream.cache) {
        rc = ngx_http_proxy_init_headers(cf, conf, &conf->headers_cache,
                                         ngx_http_proxy_cache_headers);
        if (rc != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

#endif

    return NGX_CONF_OK;
}

/* 
  把proxy_set_header与ngx_http_proxy_headers合在一起，然后把其中的key:value字符串和长度信息添加到headers->lengths和headers->values中
  把ngx_http_proxy_cache_headers合在一起，然后把其中的key:value字符串和长度信息添加到headers->lengths和headers->values中
*/
//1. 里面存储的是ngx_http_proxy_headers和proxy_set_header设置的头部信息头添加到该lengths和values 存入ngx_http_proxy_loc_conf_t->headers
//2. 里面存储的是ngx_http_proxy_cache_headers设置的头部信息头添加到该lengths和values  存入ngx_http_proxy_loc_conf_t->headers_cache
static ngx_int_t
ngx_http_proxy_init_headers(ngx_conf_t *cf, ngx_http_proxy_loc_conf_t *conf,
    ngx_http_proxy_headers_t *headers, ngx_keyval_t *default_headers)
{
    u_char                       *p;
    size_t                        size;
    uintptr_t                    *code;
    ngx_uint_t                    i;
    ngx_array_t                   headers_names, headers_merged;
    ngx_keyval_t                 *src, *s, *h;
    ngx_hash_key_t               *hk;
    ngx_hash_init_t               hash;
    ngx_http_script_compile_t     sc;
    ngx_http_script_copy_code_t  *copy;

    if (headers->hash.buckets) { //如果不为空，返回，如果是第一次进入该函数，则走下面的流程创建空间初始化
        return NGX_OK;
    }

    if (ngx_array_init(&headers_names, cf->temp_pool, 4, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_array_init(&headers_merged, cf->temp_pool, 4, sizeof(ngx_keyval_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (conf->headers_source == NULL) {
        conf->headers_source = ngx_array_create(cf->pool, 4,
                                                sizeof(ngx_keyval_t));
        if (conf->headers_source == NULL) {
            return NGX_ERROR;
        }
    }

    headers->lengths = ngx_array_create(cf->pool, 64, 1); //64个1字节的数组
    if (headers->lengths == NULL) {
        return NGX_ERROR;
    }

    headers->values = ngx_array_create(cf->pool, 512, 1); //512个1字节的数组
    if (headers->values == NULL) {
        return NGX_ERROR;
    }

    src = conf->headers_source->elts;
    for (i = 0; i < conf->headers_source->nelts; i++) { //把proxy_set_header设置的头部信息添加到headers_merged数组中

        s = ngx_array_push(&headers_merged);
        if (s == NULL) {
            return NGX_ERROR;
        }

        *s = src[i];
    }

    h = default_headers; //ngx_http_proxy_headers或者ngx_http_proxy_cache_headers

    while (h->key.len) { //把default_headers(ngx_http_proxy_headers或者ngx_http_proxy_cache_headers)中不重复的元素添加到headers_merged中

        src = headers_merged.elts;
        for (i = 0; i < headers_merged.nelts; i++) {
            if (ngx_strcasecmp(h->key.data, src[i].key.data) == 0) {
                goto next;
            }
        }

        s = ngx_array_push(&headers_merged);
        if (s == NULL) {
            return NGX_ERROR;
        }

        *s = *h;

    next:

        h++;
    }


    /* 
    把proxy_set_header与ngx_http_proxy_headers合在一起，然后把其中的key:value字符串和长度信息添加到headers->lengths和headers->values中
    把ngx_http_proxy_cache_headers合在一起，然后把其中的key:value字符串和长度信息添加到headers->lengths和headers->values中
     */
    src = headers_merged.elts;
    for (i = 0; i < headers_merged.nelts; i++) {

        /* 拷贝headers_merged中的成员信息到headers_names数组中 */
        hk = ngx_array_push(&headers_names);
        if (hk == NULL) {
            return NGX_ERROR;
        }

        hk->key = src[i].key;
        hk->key_hash = ngx_hash_key_lc(src[i].key.data, src[i].key.len);
        hk->value = (void *) 1;

        if (src[i].value.len == 0) { //value为空的，不用添加到headers->lengths中
            continue;
        }

        if (ngx_http_script_variables_count(&src[i].value) == 0) {
            copy = ngx_array_push_n(headers->lengths,
                                    sizeof(ngx_http_script_copy_code_t));
            if (copy == NULL) {
                return NGX_ERROR;
            }

            //key:value字符串长度code
            copy->code = (ngx_http_script_code_pt)
                                                 ngx_http_script_copy_len_code;
            copy->len = src[i].key.len + sizeof(": ") - 1
                        + src[i].value.len + sizeof(CRLF) - 1; //key:value字符串长度   
                        //这个长度最后在解析key:value值的时候从ngx_http_script_copy_len_code获取


            //key:value字符串code
            size = (sizeof(ngx_http_script_copy_code_t)
                       + src[i].key.len + sizeof(": ") - 1
                       + src[i].value.len + sizeof(CRLF) - 1
                       + sizeof(uintptr_t) - 1)
                    & ~(sizeof(uintptr_t) - 1); //4字节对齐


            copy = ngx_array_push_n(headers->values, size);
            if (copy == NULL) {
                return NGX_ERROR;
            }

            copy->code = ngx_http_script_copy_code;
            copy->len = src[i].key.len + sizeof(": ") - 1
                        + src[i].value.len + sizeof(CRLF) - 1;

            p = (u_char *) copy + sizeof(ngx_http_script_copy_code_t); //指向开辟空间实际的存储key:value的地方

            p = ngx_cpymem(p, src[i].key.data, src[i].key.len);
            *p++ = ':'; *p++ = ' ';
            p = ngx_cpymem(p, src[i].value.data, src[i].value.len);
            *p++ = CR; *p = LF;

        } else {
            /*
                ngx_http_proxy_cache_headers和ngx_http_proxy_cache_headers设置的头部里面存在变量
               */
            copy = ngx_array_push_n(headers->lengths,
                                    sizeof(ngx_http_script_copy_code_t));
            if (copy == NULL) {
                return NGX_ERROR;
            }

            copy->code = (ngx_http_script_code_pt)
                                                 ngx_http_script_copy_len_code;
            copy->len = src[i].key.len + sizeof(": ") - 1;


            size = (sizeof(ngx_http_script_copy_code_t)
                    + src[i].key.len + sizeof(": ") - 1 + sizeof(uintptr_t) - 1)
                    & ~(sizeof(uintptr_t) - 1);

            copy = ngx_array_push_n(headers->values, size);
            if (copy == NULL) {
                return NGX_ERROR;
            }

            copy->code = ngx_http_script_copy_code;
            copy->len = src[i].key.len + sizeof(": ") - 1;

            p = (u_char *) copy + sizeof(ngx_http_script_copy_code_t);
            p = ngx_cpymem(p, src[i].key.data, src[i].key.len);
            *p++ = ':'; *p = ' ';


            ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

            sc.cf = cf;
            sc.source = &src[i].value;
            sc.flushes = &headers->flushes;
            sc.lengths = &headers->lengths;
            sc.values = &headers->values;

            if (ngx_http_script_compile(&sc) != NGX_OK) {
                return NGX_ERROR;
            }


            copy = ngx_array_push_n(headers->lengths,
                                    sizeof(ngx_http_script_copy_code_t));
            if (copy == NULL) {
                return NGX_ERROR;
            }

            copy->code = (ngx_http_script_code_pt)
                                                 ngx_http_script_copy_len_code;
            copy->len = sizeof(CRLF) - 1;


            size = (sizeof(ngx_http_script_copy_code_t)
                    + sizeof(CRLF) - 1 + sizeof(uintptr_t) - 1)
                    & ~(sizeof(uintptr_t) - 1);

            copy = ngx_array_push_n(headers->values, size);
            if (copy == NULL) {
                return NGX_ERROR;
            }

            copy->code = ngx_http_script_copy_code;
            copy->len = sizeof(CRLF) - 1;

            p = (u_char *) copy + sizeof(ngx_http_script_copy_code_t);
            *p++ = CR; *p = LF;
        }

        code = ngx_array_push_n(headers->lengths, sizeof(uintptr_t)); //headers->lengths末尾处添加空指针表示结束标记
        if (code == NULL) {
            return NGX_ERROR;
        }

        *code = (uintptr_t) NULL;

        code = ngx_array_push_n(headers->values, sizeof(uintptr_t));//headers->values末尾处添加空指针表示结束标记
        if (code == NULL) {
            return NGX_ERROR;
        }

        *code = (uintptr_t) NULL;
    }

    code = ngx_array_push_n(headers->lengths, sizeof(uintptr_t));
    if (code == NULL) {
        return NGX_ERROR;
    }

    *code = (uintptr_t) NULL;


    //
    hash.hash = &headers->hash;
    hash.key = ngx_hash_key_lc;
    hash.max_size = conf->headers_hash_max_size;
    hash.bucket_size = conf->headers_hash_bucket_size;
    hash.name = "proxy_headers_hash";
    hash.pool = cf->pool;
    hash.temp_pool = NULL;

    return ngx_hash_init(&hash, headers_names.elts, headers_names.nelts);
}

/*
语法：proxy_pass URL 
默认值：no 
使用字段：location, location中的if字段 
这个指令设置被代理服务器的地址和被映射的URI，地址可以使用主机名或IP加端口号的形式，例如：
proxy_pass http://localhost:8000/uri/;或者一个unix socket：
proxy_pass http://unix:/path/to/backend.socket:/uri/;路径在unix关键字的后面指定，位于两个冒号之间。
注意：HTTP Host头没有转发，它将设置为基于proxy_pass声明，例如，如果你移动虚拟主机example.com到另外一台机器，然后重新配置正常
（监听example.com到一个新的IP），同时在旧机器上手动将新的example.comIP写入/etc/hosts，同时使用proxy_pass重定向到http://example.com, 
然后修改DNS到新的IP。
当传递请求时，Nginx将location对应的URI部分替换成proxy_pass指令中所指定的部分，但是有两个例外会使其无法确定如何去替换：

■location通过正则表达式指定；
■在使用代理的location中利用rewrite指令改变URI，使用这个配置可以更加精确的处理请求（break）：

location  /name/ {
  rewrite      /name/([^/] +)  /users?name=$1  break;
  proxy_pass   http://127.0.0.1;
}这些情况下URI并没有被映射传递。
此外，需要标明一些标记以便URI将以和客户端相同的发送形式转发，而不是处理过的形式，在其处理期间：


■两个以上的斜杠将被替换为一个： ”//” – ”/”; 

■删除引用的当前目录：”/./” – ”/”; 

■删除引用的先前目录：”/dir /../” – ”/“。
如果在服务器上必须以未经任何处理的形式发送URI，那么在proxy_pass指令中必须使用未指定URI的部分：

location  /some/path/ {
  proxy_pass   http://127.0.0.1;
}在指令中使用变量是一种比较特殊的情况：被请求的URL不会使用并且你必须完全手工标记URL。
这意味着下列的配置并不能让你方便的进入某个你想要的虚拟主机目录，代理总是将它转发到相同的URL（在一个server字段的配置）：


location / {
  proxy_pass   http://127.0.0.1:8080/VirtualHostBase/https/$server_name:443/some/path/VirtualHostRoot;
}解决方法是使用rewrite和proxy_pass的组合：

location / {
  rewrite ^(.*)$ /VirtualHostBase/https/$server_name:443/some/path/VirtualHostRoot$1 break;
  proxy_pass   http://127.0.0.1:8080;
}这种情况下请求的URL将被重写， proxy_pass中的拖尾斜杠并没有实际意义。
如果需要通过ssl信任连接到一个上游服务器组，proxy_pass前缀为 https://，并且同时指定ssl的端口，如：


upstream backend-secure {
  server 10.0.0.20:443;
}
 
server {
  listen 10.0.0.1:443;
  location / {
    proxy_pass https://backend-secure;
  }
}
*/
//proxy_pass解析 //这个函数会在nginx碰到proxy_pass指令的时候，就调用，然后设置相关的回调函数到对应的模块中去
static char *
ngx_http_proxy_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{//在NGX_HTTP_CONTENT_PHASE里面调用这个handler，即ngx_http_proxy_handler。  
    ngx_http_proxy_loc_conf_t *plcf = conf;

    size_t                      add;
    u_short                     port;
    ngx_str_t                  *value, *url;
    ngx_url_t                   u;
    ngx_uint_t                  n;
    ngx_http_core_loc_conf_t   *clcf;
    ngx_http_script_compile_t   sc;

    if (plcf->upstream.upstream || plcf->proxy_lengths) {
        return "is duplicate";
    }

    //获取当前的location，即在哪个location配置的"proxy_pass"指令  
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    //设置loc的handler，这个clcf->handler会在ngx_http_update_location_config()里面赋予r->content_handler，从
    // 而在NGX_HTTP_CONTENT_PHASE里面调用这个handler，即ngx_http_proxy_handler。  
    clcf->handler = ngx_http_proxy_handler;

    if (clcf->name.data[clcf->name.len - 1] == '/') {
        clcf->auto_redirect = 1;
    }

    value = cf->args->elts;

    url = &value[1];
    //
    n = ngx_http_script_variables_count(url);

    if (n) {//proxy_pass xxxx中是否带有变量，如果有变量，就解析对应的变量值

        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = url;
        sc.lengths = &plcf->proxy_lengths;
        sc.values = &plcf->proxy_values;
        sc.variables = n;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

#if (NGX_HTTP_SSL)
        plcf->ssl = 1;
#endif

        return NGX_CONF_OK;
    }

    //proxypass后面的参数必须以http://或者https://开头
    if (ngx_strncasecmp(url->data, (u_char *) "http://", 7) == 0) {
        add = 7;
        port = 80;

    } else if (ngx_strncasecmp(url->data, (u_char *) "https://", 8) == 0) {

#if (NGX_HTTP_SSL)
        plcf->ssl = 1;

        add = 8;
        port = 443;
#else
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "https protocol requires SSL support");
        return NGX_CONF_ERROR;
#endif

    } else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid URL prefix");
        return NGX_CONF_ERROR;
    }

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url.len = url->len - add; //设置url的长度，除去http://(https://)  
    u.url.data = url->data + add; //设置url，除去http://(https://)，比如原来是"http://backend1"，现在就是"backend1"  
    u.default_port = port; //默认port   80http  443htps 前面赋值
    u.uri_part = 1;
    u.no_resolve = 1; //不要resolve这个url的域名  

    //当做单个server的upstream加入到upstream里面,和 upstream {}类似，就相当于一个upstream{}配置块
    plcf->upstream.upstream = ngx_http_upstream_add(cf, &u, 0); //upstream中保存的是取了http://头部的信息
    if (plcf->upstream.upstream == NULL) { 
        return NGX_CONF_ERROR;
    }

    plcf->vars.schema.len = add;
    plcf->vars.schema.data = url->data;
    plcf->vars.key_start = plcf->vars.schema;

    ngx_http_proxy_set_vars(&u, &plcf->vars);

    plcf->location = clcf->name;

    if (clcf->named
#if (NGX_PCRE)
        || clcf->regex
#endif
        || clcf->noname)
    {
        if (plcf->vars.uri.len) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"proxy_pass\" cannot have URI part in "
                               "location given by regular expression, "
                               "or inside named location, "
                               "or inside \"if\" statement, "
                               "or inside \"limit_except\" block");
            return NGX_CONF_ERROR;
        }

        plcf->location.len = 0;
    }

    plcf->url = *url; 
    
    return NGX_CONF_OK;
}

/*
语法：proxy_redirect [ default|off|redirect replacement ];
默认值：proxy_redirect default;
使用字段：http, server, location
这个指令为被代理服务器应答中必须改变的应答头：”Location”和”Refresh”设置值。
我们假设被代理的服务器返回的应答头字段为：Location: http://localhost:8000/two/some/uri/。
指令：


proxy_redirect http://localhost:8000/two/ http://frontend/one/;会将其重写为：Location: http://frontend/one/some/uri/。
在重写字段里面可以不使用服务器名：

proxy_redirect http://localhost:8000/two/ /;这样，默认的服务器名和端口将被设置，端口默认80。
默认的重写可以使用参数default，将使用location和proxy_pass的值。
下面两个配置是等价的：


location /one/ {
  proxy_pass       http://upstream:port/two/;
  proxy_redirect   default;
}
 
location /one/ {
  proxy_pass       http://upstream:port/two/;
  proxy_redirect   http://upstream:port/two/   /one/;
}同样，在重写字段中可以使用变量：


proxy_redirect   http://localhost:8000/    http://$host:$server_port/;这个指令可以重复使用：


proxy_redirect   default;
proxy_redirect   http://localhost:8000/    /;
proxy_redirect   http://www.example.com/   /;参数off在本级中禁用所有的proxy_redirect指令：


proxy_redirect   off;
proxy_redirect   default;
proxy_redirect   http://localhost:8000/    /;
proxy_redirect   http://www.example.com/   /;这个指令可以很容易的将被代理服务器的服务器名重写为代理服务器的服务器名：

proxy_redirect   /   /;
*/ 

//ngx_http_proxy_redirect和ngx_http_proxy_rewrite_redirect配合阅读，他们一起确定重定向后的地址
static char *
ngx_http_proxy_redirect(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    u_char                            *p;
    ngx_str_t                         *value;
    ngx_http_proxy_rewrite_t          *pr;
    ngx_http_compile_complex_value_t   ccv;

    if (plcf->redirect == 0) {
        return NGX_CONF_OK;
    }

    plcf->redirect = 1;

    value = cf->args->elts;

    if (cf->args->nelts == 2) {
        if (ngx_strcmp(value[1].data, "off") == 0) {
            plcf->redirect = 0;
            plcf->redirects = NULL;
            return NGX_CONF_OK;
        }

        if (ngx_strcmp(value[1].data, "false") == 0) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0,
                           "invalid parameter \"false\", use \"off\" instead");
            plcf->redirect = 0;
            plcf->redirects = NULL;
            return NGX_CONF_OK;
        }

        if (ngx_strcmp(value[1].data, "default") != 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid parameter \"%V\"", &value[1]);
            return NGX_CONF_ERROR;
        }
    }

    if (plcf->redirects == NULL) {
        plcf->redirects = ngx_array_create(cf->pool, 1,
                                           sizeof(ngx_http_proxy_rewrite_t));
        if (plcf->redirects == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    pr = ngx_array_push(plcf->redirects);
    if (pr == NULL) {
        return NGX_CONF_ERROR;
    }

    if (ngx_strcmp(value[1].data, "default") == 0) {
        if (plcf->proxy_lengths) { //default，则proxy_pass不能携带参数
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"proxy_redirect default\" cannot be used "
                               "with \"proxy_pass\" directive with variables");
            return NGX_CONF_ERROR;
        }

        if (plcf->url.data == NULL) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"proxy_redirect default\" should be placed "
                               "after the \"proxy_pass\" directive");
            return NGX_CONF_ERROR;
        }

        pr->handler = ngx_http_proxy_rewrite_complex_handler;

        ngx_memzero(&pr->pattern.complex, sizeof(ngx_http_complex_value_t));

        ngx_memzero(&pr->replacement, sizeof(ngx_http_complex_value_t));

        if (plcf->vars.uri.len) {
            pr->pattern.complex.value = plcf->url;
            pr->replacement.value = plcf->location;

        } else {
            pr->pattern.complex.value.len = plcf->url.len + sizeof("/") - 1;

            p = ngx_pnalloc(cf->pool, pr->pattern.complex.value.len);
            if (p == NULL) {
                return NGX_CONF_ERROR;
            }

            pr->pattern.complex.value.data = p;

            p = ngx_cpymem(p, plcf->url.data, plcf->url.len);
            *p = '/';

            ngx_str_set(&pr->replacement.value, "/");
        }

        return NGX_CONF_OK;
    }


    if (value[1].data[0] == '~') {
        value[1].len--;
        value[1].data++;

        if (value[1].data[0] == '*') {
            value[1].len--;
            value[1].data++;

            if (ngx_http_proxy_rewrite_regex(cf, pr, &value[1], 1) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

        } else {
            if (ngx_http_proxy_rewrite_regex(cf, pr, &value[1], 0) != NGX_OK) {
                return NGX_CONF_ERROR;
            }
        }

    } else {

        ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

        ccv.cf = cf;
        ccv.value = &value[1];
        ccv.complex_value = &pr->pattern.complex;

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        pr->handler = ngx_http_proxy_rewrite_complex_handler;
    }


    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[2];
    ccv.complex_value = &pr->replacement;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_proxy_cookie_domain(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    ngx_str_t                         *value;
    ngx_http_proxy_rewrite_t          *pr;
    ngx_http_compile_complex_value_t   ccv;

    if (plcf->cookie_domains == NULL) {
        return NGX_CONF_OK;
    }

    value = cf->args->elts;

    if (cf->args->nelts == 2) {

        if (ngx_strcmp(value[1].data, "off") == 0) {
            plcf->cookie_domains = NULL;
            return NGX_CONF_OK;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[1]);
        return NGX_CONF_ERROR;
    }

    if (plcf->cookie_domains == NGX_CONF_UNSET_PTR) {
        plcf->cookie_domains = ngx_array_create(cf->pool, 1,
                                     sizeof(ngx_http_proxy_rewrite_t));
        if (plcf->cookie_domains == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    pr = ngx_array_push(plcf->cookie_domains);
    if (pr == NULL) {
        return NGX_CONF_ERROR;
    }

    if (value[1].data[0] == '~') {
        value[1].len--;
        value[1].data++;

        if (ngx_http_proxy_rewrite_regex(cf, pr, &value[1], 1) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

    } else {

        if (value[1].data[0] == '.') {
            value[1].len--;
            value[1].data++;
        }

        ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

        ccv.cf = cf;
        ccv.value = &value[1];
        ccv.complex_value = &pr->pattern.complex;

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        pr->handler = ngx_http_proxy_rewrite_domain_handler;

        if (value[2].data[0] == '.') {
            value[2].len--;
            value[2].data++;
        }
    }

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[2];
    ccv.complex_value = &pr->replacement;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_proxy_cookie_path(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    ngx_str_t                         *value;
    ngx_http_proxy_rewrite_t          *pr;
    ngx_http_compile_complex_value_t   ccv;

    if (plcf->cookie_paths == NULL) {
        return NGX_CONF_OK;
    }

    value = cf->args->elts;

    if (cf->args->nelts == 2) {

        if (ngx_strcmp(value[1].data, "off") == 0) {
            plcf->cookie_paths = NULL;
            return NGX_CONF_OK;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[1]);
        return NGX_CONF_ERROR;
    }

    if (plcf->cookie_paths == NGX_CONF_UNSET_PTR) {
        plcf->cookie_paths = ngx_array_create(cf->pool, 1,
                                     sizeof(ngx_http_proxy_rewrite_t));
        if (plcf->cookie_paths == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    pr = ngx_array_push(plcf->cookie_paths);
    if (pr == NULL) {
        return NGX_CONF_ERROR;
    }

    if (value[1].data[0] == '~') {
        value[1].len--;
        value[1].data++;

        if (value[1].data[0] == '*') {
            value[1].len--;
            value[1].data++;

            if (ngx_http_proxy_rewrite_regex(cf, pr, &value[1], 1) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

        } else {
            if (ngx_http_proxy_rewrite_regex(cf, pr, &value[1], 0) != NGX_OK) {
                return NGX_CONF_ERROR;
            }
        }

    } else {

        ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

        ccv.cf = cf;
        ccv.value = &value[1];
        ccv.complex_value = &pr->pattern.complex;

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        pr->handler = ngx_http_proxy_rewrite_complex_handler;
    }

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[2];
    ccv.complex_value = &pr->replacement;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_proxy_rewrite_regex(ngx_conf_t *cf, ngx_http_proxy_rewrite_t *pr,
    ngx_str_t *regex, ngx_uint_t caseless)
{
#if (NGX_PCRE)
    u_char               errstr[NGX_MAX_CONF_ERRSTR];
    ngx_regex_compile_t  rc;

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.pattern = *regex;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;

    if (caseless) {
        rc.options = NGX_REGEX_CASELESS;
    }

    pr->pattern.regex = ngx_http_regex_compile(cf, &rc);
    if (pr->pattern.regex == NULL) {
        return NGX_ERROR;
    }

    pr->handler = ngx_http_proxy_rewrite_regex_handler;

    return NGX_OK;

#else

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "using regex \"%V\" requires PCRE library", regex);
    return NGX_ERROR;

#endif
}


static char *
ngx_http_proxy_store(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    ngx_str_t                  *value;
    ngx_http_script_compile_t   sc;

    if (plcf->upstream.store != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "off") == 0) {
        plcf->upstream.store = 0;
        return NGX_CONF_OK;
    }

#if (NGX_HTTP_CACHE)
    if (plcf->upstream.cache > 0) {
        return "is incompatible with \"proxy_cache\"";
    }
#endif

    plcf->upstream.store = 1;

    if (ngx_strcmp(value[1].data, "on") == 0) {
        return NGX_CONF_OK;
    }

    /* include the terminating '\0' into script */
    value[1].len++;

    ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

    sc.cf = cf;
    sc.source = &value[1];
    sc.lengths = &plcf->upstream.store_lengths;
    sc.values = &plcf->upstream.store_values;
    sc.variables = ngx_http_script_variables_count(&value[1]);
    sc.complete_lengths = 1;
    sc.complete_values = 1;

    if (ngx_http_script_compile(&sc) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


#if (NGX_HTTP_CACHE)
//proxy_cache abc必须和proxy_cache_path xxx keys_zone=abc:10m;一起，否则在ngx_http_proxy_merge_loc_conf会失败，因为没有为该abc创建ngx_http_file_cache_t
static char *
ngx_http_proxy_cache(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    ngx_str_t                         *value;
    ngx_http_complex_value_t           cv;
    ngx_http_compile_complex_value_t   ccv;

    value = cf->args->elts;

    if (plcf->upstream.cache != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    if (ngx_strcmp(value[1].data, "off") == 0) {
        plcf->upstream.cache = 0;
        return NGX_CONF_OK;
    }

    if (plcf->upstream.store > 0) {
        return "is incompatible with \"proxy_store\"";
    }

    plcf->upstream.cache = 1;

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &cv;

    //调用正则表达式库解析proxy_cache xxx$ss 中的字符串，解析出的变量length和value存在与cv中
    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    if (cv.lengths != NULL) { //说明proxy_cache配置中存在变量

        plcf->upstream.cache_value = ngx_palloc(cf->pool,
                                             sizeof(ngx_http_complex_value_t));
        if (plcf->upstream.cache_value == NULL) {
            return NGX_CONF_ERROR;
        }

        *plcf->upstream.cache_value = cv;

        return NGX_CONF_OK;
    }

    plcf->upstream.cache_zone = ngx_shared_memory_add(cf, &value[1], 0,
                                                      &ngx_http_proxy_module);
    if (plcf->upstream.cache_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_proxy_cache_key(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    ngx_str_t                         *value;
    ngx_http_compile_complex_value_t   ccv;

    value = cf->args->elts;

    if (plcf->cache_key.value.data) {
        return "is duplicate";
    }

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &plcf->cache_key;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

#endif


#if (NGX_HTTP_SSL)

static char *
ngx_http_proxy_ssl_password_file(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proxy_loc_conf_t *plcf = conf;

    ngx_str_t  *value;

    if (plcf->ssl_passwords != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    value = cf->args->elts;

    plcf->ssl_passwords = ngx_ssl_read_password_file(cf, &value[1]);

    if (plcf->ssl_passwords == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

#endif


static char *
ngx_http_proxy_lowat_check(ngx_conf_t *cf, void *post, void *data)
{
#if (NGX_FREEBSD)
    ssize_t *np = data;

    if ((u_long) *np >= ngx_freebsd_net_inet_tcp_sendspace) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"proxy_send_lowat\" must be less than %d "
                           "(sysctl net.inet.tcp.sendspace)",
                           ngx_freebsd_net_inet_tcp_sendspace);

        return NGX_CONF_ERROR;
    }

#elif !(NGX_HAVE_SO_SNDLOWAT)
    ssize_t *np = data;

    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "\"proxy_send_lowat\" is not supported, ignored");

    *np = 0;

#endif

    return NGX_CONF_OK;
}


#if (NGX_HTTP_SSL)

static ngx_int_t
ngx_http_proxy_set_ssl(ngx_conf_t *cf, ngx_http_proxy_loc_conf_t *plcf)
{
    ngx_pool_cleanup_t  *cln;

    plcf->upstream.ssl = ngx_pcalloc(cf->pool, sizeof(ngx_ssl_t));
    if (plcf->upstream.ssl == NULL) {
        return NGX_ERROR;
    }

    plcf->upstream.ssl->log = cf->log;

    if (ngx_ssl_create(plcf->upstream.ssl, plcf->ssl_protocols, NULL)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    cln = ngx_pool_cleanup_add(cf->pool, 0);
    if (cln == NULL) {
        return NGX_ERROR;
    }

    cln->handler = ngx_ssl_cleanup_ctx;
    cln->data = plcf->upstream.ssl;

    if (plcf->ssl_certificate.len) {

        if (plcf->ssl_certificate_key.len == 0) {
            ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                          "no \"proxy_ssl_certificate_key\" is defined "
                          "for certificate \"%V\"", &plcf->ssl_certificate);
            return NGX_ERROR;
        }

        if (ngx_ssl_certificate(cf, plcf->upstream.ssl, &plcf->ssl_certificate,
                                &plcf->ssl_certificate_key, plcf->ssl_passwords)
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    if (SSL_CTX_set_cipher_list(plcf->upstream.ssl->ctx,
                                (const char *) plcf->ssl_ciphers.data)
        == 0)
    {
        ngx_ssl_error(NGX_LOG_EMERG, cf->log, 0,
                      "SSL_CTX_set_cipher_list(\"%V\") failed",
                      &plcf->ssl_ciphers);
        return NGX_ERROR;
    }

    if (plcf->upstream.ssl_verify) {
        if (plcf->ssl_trusted_certificate.len == 0) {
            ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                      "no proxy_ssl_trusted_certificate for proxy_ssl_verify");
            return NGX_ERROR;
        }

        if (ngx_ssl_trusted_certificate(cf, plcf->upstream.ssl,
                                        &plcf->ssl_trusted_certificate,
                                        plcf->ssl_verify_depth)
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        if (ngx_ssl_crl(cf, plcf->upstream.ssl, &plcf->ssl_crl) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}

#endif


static void
ngx_http_proxy_set_vars(ngx_url_t *u, ngx_http_proxy_vars_t *v)
{   //AF_UNIX代表域套接字
    if (u->family != AF_UNIX) {//ngx_http_upstream_add->ngx_parse_inet_url里面设置为AF_INET 

        if (u->no_port || u->port == u->default_port) {

            v->host_header = u->host;

            if (u->default_port == 80) {
                ngx_str_set(&v->port, "80");

            } else {
                ngx_str_set(&v->port, "443"); //https端口443
            }

        } else {
            v->host_header.len = u->host.len + 1 + u->port_text.len;
            v->host_header.data = u->host.data;
            v->port = u->port_text;
        }

        v->key_start.len += v->host_header.len;  

        /*
            proxy_pass  http://10.10.0.103:8080/tttxx; 
            printf("yang test ....... no_port:%u, port:%u, default_port:%u, host:%s, key:%s\n", 
                u->no_port, u->port, u->default_port, u->host.data, v->key_start.data);
            yang test ....... no_port:0, port:8080, default_port:80, host:10.10.0.103:8080/tttxx, key:http://10.10.0.103:8080/tttxx
         */
    } else {
        ngx_str_set(&v->host_header, "localhost");
        ngx_str_null(&v->port);
        v->key_start.len += sizeof("unix:") - 1 + u->host.len + 1;
    }

    v->uri = u->uri;
}

