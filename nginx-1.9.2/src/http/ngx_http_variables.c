
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>


static ngx_int_t ngx_http_variable_request(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
#if 0
static void ngx_http_variable_request_set(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
#endif
static ngx_int_t ngx_http_variable_request_get_size(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static void ngx_http_variable_request_set_size(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_header(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_variable_cookies(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_headers(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_headers_internal(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data, u_char sep);

static ngx_int_t ngx_http_variable_unknown_header_in(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_unknown_header_out(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_request_line(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_cookie(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_argument(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
#if (NGX_HAVE_TCP_INFO)
static ngx_int_t ngx_http_variable_tcpinfo(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
#endif

static ngx_int_t ngx_http_variable_content_length(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_host(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_binary_remote_addr(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_remote_addr(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_remote_port(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_proxy_protocol_addr(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_server_addr(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_server_port(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_scheme(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_https(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static void ngx_http_variable_set_args(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_is_args(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_document_root(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_realpath_root(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_request_filename(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_server_name(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_request_method(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_remote_user(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_bytes_sent(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_body_bytes_sent(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_pipe(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_request_completion(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_request_body(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_request_body_file(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_request_length(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_request_time(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_status(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_variable_sent_content_type(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_sent_content_length(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_sent_location(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_sent_last_modified(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_sent_connection(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_sent_keep_alive(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_sent_transfer_encoding(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_variable_connection(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_connection_requests(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_variable_nginx_version(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_hostname(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_pid(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_msec(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_time_iso8601(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_variable_time_local(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

/*
 * TODO:
 *     Apache CGI: AUTH_TYPE, PATH_INFO (null), PATH_TRANSLATED
 *                 REMOTE_HOST (null), REMOTE_IDENT (null),
 *                 SERVER_SOFTWARE
 *
 *     Apache SSI: DOCUMENT_NAME, LAST_MODIFIED, USER_NAME (file owner)
 */

/*
 * the $http_host, $http_user_agent, $http_referer, and $http_via
 * variables may be handled by generic
 * ngx_http_variable_unknown_header_in(), but for performance reasons
 * they are handled using dedicated entries
 */

 /*
一、日志分类
        Nginx日志主要分为两种：访问日志和错误日志。日志开关在Nginx配置文件（/etc/nginx/nginx.conf）中设置，两种日志都可以选择性关闭，默认都是打开的。
1、访问日志
        访问日志主要记录客户端访问Nginx的每一个请求，格式可以自定义。通过访问日志，你可以得到用户地域来源、跳转来源、使用终端、某个URL访问量等相关信息。Nginx中访问日志相关指令主要有两条：
        （1）log_format
        log_format用来设置日志格式，也就是日志文件中每条日志的格式，具体如下：
        log_format name(格式名称) type(格式样式)
        举例说明如下：
        log_format  main  '$server_name $remote_addr - $remote_user [$time_local] "$request" '
                        '$status $uptream_status $body_bytes_sent "$http_referer" '
                        '"$http_user_agent" "$http_x_forwarded_for" '
                        '$ssl_protocol $ssl_cipher $upstream_addr $request_time $upstream_response_time';
        上面红色部分为Nginx默认指定的格式样式，每个样式的含义如下：
        $server_name：虚拟主机名称。
        $remote_addr：远程客户端的IP地址。
        -：空白，用一个“-”占位符替代，历史原因导致还存在。
        $remote_user：远程客户端用户名称，用于记录浏览者进行身份验证时提供的名字，如登录百度的用户名scq2099yt，如果没有登录就是空白。
        [$time_local]：访问的时间与时区，比如18/Jul/2012:17:00:01 +0800，时间信息最后的"+0800"表示服务器所处时区位于UTC之后的8小时。
        $request：请求的URI和HTTP协议，这是整个PV日志记录中最有用的信息，记录服务器收到一个什么样的请求
        $status：记录请求返回的http状态码，比如成功是200。
        $uptream_status：upstream状态，比如成功是200.
        $body_bytes_sent：发送给客户端的文件主体内容的大小，比如899，可以将日志每条记录中的这个值累加起来以粗略估计服务器吞吐量。
        $http_referer：记录从哪个页面链接访问过来的。 
        $http_user_agent：客户端浏览器信息
        $http_x_forwarded_for：客户端的真实ip，通常web服务器放在反向代理的后面，这样就不能获取到客户的IP地址了，通过$remote_add拿到的IP地址是反向代理服务器的iP地址。反向代理服务器在转发请求的http头信息中，可以增加x_forwarded_for信息，用以记录原有客户端的IP地址和原来客户端的请求的服务器地址。
        $ssl_protocol：SSL协议版本，比如TLSv1。
        $ssl_cipher：交换数据中的算法，比如RC4-SHA。 
        $upstream_addr：upstream的地址，即真正提供服务的主机地址。 
        $request_time：整个请求的总时间。 
        $upstream_response_time：请求过程中，upstream的响应时间。
        访问日志中一个典型的记录如下：
        192.168.1.102 - scq2099yt [18/Mar/2013:23:30:42 +0800] "GET /stats/awstats.pl?config=scq2099yt HTTP/1.1" 200 899 "http://192.168.1.1/pv/" "Mozilla/4.0 (compatible; MSIE 6.0; Windows XXX; Maxthon)"
        需要注意的是：log_format配置必须放在http内，否则会出现如下警告信息：
        nginx: [warn] the "log_format" directive may be used only on "http" level in /etc/nginx/nginx.conf:97
        （2）access_log
        access_log指令用来指定日志文件的存放路径（包含日志文件名）、格式和缓存大小，具体如下：
        access_log path(存放路径) [format(自定义日志格式名称) [buffer=size | off]]
        举例说明如下：
        access_log  logs/access.log  main;
        如果想关闭日志，可以如下：
        access_log off;
        能够使用access_log指令的字段包括：http、server、location。
        需要注意的是：Nginx进程设置的用户和组必须对日志路径有创建文件的权限，否则，会报错。
        小技巧：如果需要在access_log中记录post请求的参数，可以参考这里。
        Nginx支持为每个location指定强大的日志记录。同样的连接可以在同一时间输出到不止一个的日志中，更多信息请查看这里，这里，还有这里。
2、错误日志
        错误日志主要记录客户端访问Nginx出错时的日志，格式不支持自定义。通过错误日志，你可以得到系统某个服务或server的性能瓶颈等。因此，将日志好好利用，你可以得到很多有价值的信息。错误日志由指令error_log来指定，具体格式如下：
        error_log path(存放路径) level(日志等级)
        path含义同access_log，level表示日志等级，具体如下：
        [ debug | info | notice | warn | error | crit ]
        从左至右，日志详细程度逐级递减，即debug最详细，crit最少。
        举例说明如下：
        error_log  logs/error.log  info;
        需要注意的是：error_log off并不能关闭错误日志，而是会将错误日志记录到一个文件名为off的文件中。
        正确的关闭错误日志记录功能的方法如下：
        error_log /dev/null;
        上面表示将存储日志的路径设置为“垃圾桶”。

*/ //日志可以按天进行分割，可以参考nginx日志分析解决方案- Awstats 
/*
那么首先需要了解一下nginx中变量的组织结构，nginx变量可以归为两类：


类型1：nginx内部变量
    nginx核心变量：ngx_http_core_variables 变量集
    nginx各模块有自己的变量：比如ngx_http_browser_module 模块的 ngx_http_browsers 变量集
类型2：nginx的配置文件中，我们会用到很多变量。
    比如：log_format  main  '$remote_addr';  中的$remote_addr'
    比如：set $name hello;中的$name 
    比如：limit_conn_zone $binary_remote_addr zone=ip_zone:10m; 中的$binary_remote_addr
    比如：set $memcached_key test; 中的$memcached_key


在记录access_log访问日志文件时，可以使用ngx_http_core_module模块处理请求时所产生的丰富的变量，当然，这些变量还可以用于其他HTTP模块。
例如，当URI中的某个参数满足设定的条件时，有些HTTP模块的配置项可以使用类似$arg_PARAMETER这样的变量。又如，若想把每个请求中的限速信
息记录到access日志文件中，则可以使用$limit_rate变量。

表2-1列出了ngx_http_core_module模块提供的这些变量。

表2-1　ngx_http_core_module模块提供的变量
参 数 名 意　义
$arg_PARAMETER HTTP请求中某个参数的值，如/index.html?size=100，可以用$arg_size取得100这个值
$args HTTP请求中的完整参数。例如，在请求/index.html?_w=120&_h=120中，$args表示字符串_w=120&_h=120
$binary_remote_addr 二进制格式的客户端地址。例如：\x0A\xE0B\x0E
$body_bytes_sent 表示在向客户端发送的http响应中，包体部分的字节数
$content_length 表示客户端请求头部中的Content-Length字段
$content_type 表示客户端请求头部中的Content-Type字段
$cookie_COOKIE 表示在客户端请求头部中的cookie字段
$document_root 表示当前请求所使用的root配置项的值
$uri 表示当前请求的URI，不带任何参数
$document_uri 与$uri含义相同
$request_uri 表示客户端发来的原始请求URI，带完整的参数。$uri和$document_uri未必是用户的原始请求，在内部重定向后可能是重定向后的URI，而$request_uri永远不会改变，始终是客户端的原始URI
$host 表示客户端请求头部中的Host字段。如果Host字段不存在，则以实际处理的server（虚拟主机）名称代替。如果Host字段中带有端口，如IP:PORT，那么$host是去掉端口的，它的值为IP。$host是全小写的。这些特性与http_HEADER中的http_host不同，http_host只是“忠实”地取出Host头部对应的值
$hostname 表示Nginx所在机器的名称，与gethostbyname调用返回的值相同
$http_HEADER 表示当前HTTP请求中相应头部的值。HEADER名称全小写。例如，用$http_host表示请求中Host头部对应的值
$sent_http_HEADER 表示返回客户端的HTTP响应中相应头部的值。HEADER名称全小写。例如，用$sent_http_content_type表示响应中Content-Type头部对应的值
$is_args 表示请求中的URI是否带参数，如果带参数，$is_args值为?，如果不带参数，则是空字符串
$limit_rate 表示当前连接的限速是多少，0表示无限速
$nginx_version 表示当前Nginx的版本号，如1.0.14
$query_string 请求URI中的参数，与$args相同，然而$query_string是只读的不会改变
$remote_addr 表示客户端的地址
$remote_port 表示客户端连接使用的端口
$remote_user 表示使用Auth Basic Module时定义的用户名
$request_filename 表示用户请求中的URI经过root或alias转换后的文件路径
$request_body 表示HTTP请求中的包体，该参数只在proxy_pass或fastcgi_pass中有意义
$request_body_file 表示HTTP请求中的包体存储的临时文件名
$request_completion 当请求已经全部完成时，其值为“ok”。若没有完成，就要返回客户端，则其值为空字符串；或者在断点续传等情况下使用HTTP range访问的并不是文件的最后一块，那么其值也是空字符串
$request_method 表示HTTP请求的方法名，如GET、PUT、POST等
$scheme 表示HTTP scheme，如在请求https://nginx.com/中表示https
$server_addr 表示服务器地址
$server_name 表示服务器名称
$server_port 表示服务器端口
$server_protocol 表示服务器向客户端发送响应的协议，如HTTP/1.1或HTTP/1.0
*/ //相关代码分析可以参考:http://blog.csdn.net/fangru/article/details/9163553
//Nginx在很多模块中都有内置的变量，常用的内置变量在HTT(www.111cn.net)P核心模块中，这些变量都可以使用正则表达式进行匹配。
/*
核心模块ngx_http_core_module提供的变量使用ngx_http_core_variables 描述，由preconfiguration回调函数 ngx_http_variables_add_core_vars进行定义：
*/ //http://ialloc.org/posts/2013/10/20/ngx-notes-http-variables/
static ngx_http_variable_t  ngx_http_core_variables[] = { //参考<输入剖析nginx-变量>  下面这些变量的值的来源是ngx_http_headers_in中的handler函数

    { ngx_string("http_host"), NULL, ngx_http_variable_header, //ngx_http_get_variable中执行ngx_http_variable_header，从而从headers_in.host获取值
      offsetof(ngx_http_request_t, headers_in.host), 0, 0 }, 

    { ngx_string("http_user_agent"), NULL, ngx_http_variable_header,
      offsetof(ngx_http_request_t, headers_in.user_agent), 0, 0 },

    { ngx_string("http_referer"), NULL, ngx_http_variable_header,
      offsetof(ngx_http_request_t, headers_in.referer), 0, 0 },

#if (NGX_HTTP_GZIP)
    { ngx_string("http_via"), NULL, ngx_http_variable_header,
      offsetof(ngx_http_request_t, headers_in.via), 0, 0 },
#endif

#if (NGX_HTTP_X_FORWARDED_FOR)
    { ngx_string("http_x_forwarded_for"), NULL, ngx_http_variable_headers,
      offsetof(ngx_http_request_t, headers_in.x_forwarded_for), 0, 0 },
#endif

    { ngx_string("http_cookie"), NULL, ngx_http_variable_cookies,
      offsetof(ngx_http_request_t, headers_in.cookies), 0, 0 },

    { ngx_string("content_length"), NULL, ngx_http_variable_content_length,
      0, 0, 0 },

    { ngx_string("content_type"), NULL, ngx_http_variable_header,
      offsetof(ngx_http_request_t, headers_in.content_type), 0, 0 },

    { ngx_string("host"), NULL, ngx_http_variable_host, 0, 0, 0 },

    { ngx_string("binary_remote_addr"), NULL,
      ngx_http_variable_binary_remote_addr, 0, 0, 0 },

    { ngx_string("remote_addr"), NULL, ngx_http_variable_remote_addr, 0, 0, 0 },

    { ngx_string("remote_port"), NULL, ngx_http_variable_remote_port, 0, 0, 0 },

    { ngx_string("proxy_protocol_addr"), NULL,
      ngx_http_variable_proxy_protocol_addr, 0, 0, 0 },

    { ngx_string("server_addr"), NULL, ngx_http_variable_server_addr, 0, 0, 0 },

    { ngx_string("server_port"), NULL, ngx_http_variable_server_port, 0, 0, 0 },

    { ngx_string("server_protocol"), NULL, ngx_http_variable_request,
      offsetof(ngx_http_request_t, http_protocol), 0, 0 },

    { ngx_string("scheme"), NULL, ngx_http_variable_scheme, 0, 0, 0 },

    { ngx_string("https"), NULL, ngx_http_variable_https, 0, 0, 0 },

    { ngx_string("request_uri"), NULL, ngx_http_variable_request,
      offsetof(ngx_http_request_t, unparsed_uri), 0, 0 },

    //$uri 请求中的当前URI(不带请求参数，参数位于$args)，可以不同于浏览器传递的$request_uri的值，
    //它可以通过内部重定向，或者使用index指令进行修改。
    { ngx_string("uri"), NULL, ngx_http_variable_request,
      offsetof(ngx_http_request_t, uri),
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("document_uri"), NULL, ngx_http_variable_request,
      offsetof(ngx_http_request_t, uri),
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("request"), NULL, ngx_http_variable_request_line, 0, 0, 0 },

    { ngx_string("document_root"), NULL,
      ngx_http_variable_document_root, 0, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("realpath_root"), NULL,
      ngx_http_variable_realpath_root, 0, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("query_string"), NULL, ngx_http_variable_request,
      offsetof(ngx_http_request_t, args),
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("args"),
      ngx_http_variable_set_args,
      ngx_http_variable_request,
      offsetof(ngx_http_request_t, args),
      NGX_HTTP_VAR_CHANGEABLE|NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("is_args"), NULL, ngx_http_variable_is_args,
      0, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("request_filename"), NULL,
      ngx_http_variable_request_filename, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("server_name"), NULL, ngx_http_variable_server_name, 0, 0, 0 },

    { ngx_string("request_method"), NULL,
      ngx_http_variable_request_method, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("remote_user"), NULL, ngx_http_variable_remote_user, 0, 0, 0 },

    { ngx_string("bytes_sent"), NULL, ngx_http_variable_bytes_sent,
      0, 0, 0 },

    { ngx_string("body_bytes_sent"), NULL, ngx_http_variable_body_bytes_sent,
      0, 0, 0 },

    { ngx_string("pipe"), NULL, ngx_http_variable_pipe,
      0, 0, 0 },

    { ngx_string("request_completion"), NULL,
      ngx_http_variable_request_completion,
      0, 0, 0 },

    { ngx_string("request_body"), NULL,
      ngx_http_variable_request_body,
      0, 0, 0 },

    { ngx_string("request_body_file"), NULL,
      ngx_http_variable_request_body_file,
      0, 0, 0 },

    { ngx_string("request_length"), NULL, ngx_http_variable_request_length,
      0, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("request_time"), NULL, ngx_http_variable_request_time,
      0, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("status"), NULL,
      ngx_http_variable_status, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("sent_http_content_type"), NULL,
      ngx_http_variable_sent_content_type, 0, 0, 0 },

    { ngx_string("sent_http_content_length"), NULL,
      ngx_http_variable_sent_content_length, 0, 0, 0 },

    { ngx_string("sent_http_location"), NULL,
      ngx_http_variable_sent_location, 0, 0, 0 },

    { ngx_string("sent_http_last_modified"), NULL,
      ngx_http_variable_sent_last_modified, 0, 0, 0 },

    { ngx_string("sent_http_connection"), NULL,
      ngx_http_variable_sent_connection, 0, 0, 0 },

    { ngx_string("sent_http_keep_alive"), NULL,
      ngx_http_variable_sent_keep_alive, 0, 0, 0 },

    { ngx_string("sent_http_transfer_encoding"), NULL,
      ngx_http_variable_sent_transfer_encoding, 0, 0, 0 },

    { ngx_string("sent_http_cache_control"), NULL, ngx_http_variable_headers,
      offsetof(ngx_http_request_t, headers_out.cache_control), 0, 0 },

    { ngx_string("limit_rate"), ngx_http_variable_request_set_size,
      ngx_http_variable_request_get_size,
      offsetof(ngx_http_request_t, limit_rate),
      NGX_HTTP_VAR_CHANGEABLE|NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("connection"), NULL,
      ngx_http_variable_connection, 0, 0, 0 },

    { ngx_string("connection_requests"), NULL,
      ngx_http_variable_connection_requests, 0, 0, 0 },

    { ngx_string("nginx_version"), NULL, ngx_http_variable_nginx_version,
      0, 0, 0 },

    { ngx_string("hostname"), NULL, ngx_http_variable_hostname,
      0, 0, 0 },

    { ngx_string("pid"), NULL, ngx_http_variable_pid,
      0, 0, 0 },

    { ngx_string("msec"), NULL, ngx_http_variable_msec,
      0, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("time_iso8601"), NULL, ngx_http_variable_time_iso8601,
      0, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("time_local"), NULL, ngx_http_variable_time_local,
      0, NGX_HTTP_VAR_NOCACHEABLE, 0 },

#if (NGX_HAVE_TCP_INFO)
    { ngx_string("tcpinfo_rtt"), NULL, ngx_http_variable_tcpinfo,
      0, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("tcpinfo_rttvar"), NULL, ngx_http_variable_tcpinfo,
      1, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("tcpinfo_snd_cwnd"), NULL, ngx_http_variable_tcpinfo,
      2, NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("tcpinfo_rcv_space"), NULL, ngx_http_variable_tcpinfo,
      3, NGX_HTTP_VAR_NOCACHEABLE, 0 },
#endif

    { ngx_null_string, NULL, NULL, 0, 0, 0 }
};


ngx_http_variable_value_t  ngx_http_variable_null_value =
    ngx_http_variable("");
ngx_http_variable_value_t  ngx_http_variable_true_value =
    ngx_http_variable("1");


/*
其中name是变量的名称
flags是变量的标志：
NGX_HTTP_VAR_CHANGEABLE：允许重复定义；
NGX_HTTP_VAR_NOCACHEABLE：变量值不可以被缓存，每次使用必须计算；
NGX_HTTP_VAR_INDEXED：指示变量值存储在数组中，当使用ngx_http_get_variable函数获取变量时不会每次都为变量分配值空间；
NGX_HTTP_VAR_NOHASH：配置解析完以后，变量名不进hash索引，处理请求时不可以用变量名访问变量。

创建了变量以后，需要设置变量的get_handler和set_handler，以及data。比如map是这样设置的：
    var->get_handler = ngx_http_map_variable;
    var->data = (uintptr_t) map;
 
变量的get_handler和set_handler体现了两种使用策略，
get_handler体现的是lazy_handle的策略，只有使用到变量，才会计算变量值；
set_handler体现的是active_handle的策略，每执行一个请求，都会计算变量值。
同时设置get_handler和set_handler回调函数是没有意义的，必须根据变量的使用特点，确定使用其中某一种回调函数。一般来说，get_handler更通用一些。
而这里设置的data将会作为将来调用get_handler或者set_handler的参数。
*/ 

/*
模块内自己的变量在preconfiguration的时候一般会调用ngx_http_add_variable加入到variables_keys， 例如变量ngx_http_fastcgi_vars ngx_http_browsers 
配置文件中set配置的变量也会通过ngx_http_add_variable创建
//注意:ngx_http_core_module模块一定要在其他涉及到变量模块前定义，因为variables_keys空间是在ngx_http_core_module模块的ngx_http_variables_add_core_vars中创建
*/

//遍历variables_keys hash表，如果表中已经存在name，则直接返回hash中对应的ngx_http_variable_t信息，否则创建ngx_http_variable_t hash节点
//注意:ngx_http_core_module模块一定要在其他涉及到变量模块前定义，因为variables_keys空间是在ngx_http_core_module模块的ngx_http_variables_add_core_vars中创建
 ngx_http_variable_t * 
ngx_http_add_variable(ngx_conf_t *cf, ngx_str_t *name, ngx_uint_t flags)
{
    ngx_int_t                   rc;
    ngx_uint_t                  i;
    ngx_hash_key_t             *key;
    ngx_http_variable_t        *v;
    ngx_http_core_main_conf_t  *cmcf;

    if (name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid variable name \"$\"");
        return NULL;
    }

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    key = cmcf->variables_keys->keys.elts;
    for (i = 0; i < cmcf->variables_keys->keys.nelts; i++) {
        if (name->len != key[i].key.len
            || ngx_strncasecmp(name->data, key[i].key.data, name->len) != 0) //没有匹配到name字符串
        {
            continue;
        }

        v = key[i].value; //ngx_http_core_variables中的每一个ngx_http_variable_t成员

        if (!(v->flags & NGX_HTTP_VAR_CHANGEABLE)) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "the duplicate \"%V\" variable", name);
            return NULL;
        }

        return v; //如果已经存在该name字符串变量，直接返回ngx_http_core_variables中对应的ngx_http_variable_t成员
    }

    v = ngx_palloc(cf->pool, sizeof(ngx_http_variable_t));
    if (v == NULL) {
        return NULL;
    }

    v->name.len = name->len;
    v->name.data = ngx_pnalloc(cf->pool, name->len);
    if (v->name.data == NULL) {
        return NULL;
    }

    ngx_strlow(v->name.data, name->data, name->len);

    v->set_handler = NULL;
    v->get_handler = NULL;
    v->data = 0;
    v->flags = flags;
    v->index = 0;

    rc = ngx_hash_add_key(cmcf->variables_keys, &v->name, v, 0);

    if (rc == NGX_ERROR) {
        return NULL;
    }

    if (rc == NGX_BUSY) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "conflicting variable name \"%V\"", name);
        return NULL;
    }

    return v;
}

/*
那可能有读者可能会有疑问了。如果某个变量设置了NGX_HTTP_VAR_NOHASH的标志，不能使用变量名访问，那应该怎么访问呢？


ngx_http_get_variable_index会把变量加入一个array并返回变量在array中的下标，这样模块保存变量的下标，并通过
ngx_http_get_indexed_variable直接得到变量值
*/ 

/*在解析配置文件中的时候，遇到set配置变量的时候，会把对应的变量添加到variables数组，但是在解析配置文件过程中或者解析配置文件前，并没有把模块
中自带的变量加入variables数组，而variables_keys中则存储了模块中自定义的以及set等在配置文件中解析到的变量
*/
ngx_int_t //在cmcf->variables中查找是否有对应的name，并获取在variables数组中的下标
ngx_http_get_variable_index(ngx_conf_t *cf, ngx_str_t *name)
{
    ngx_uint_t                  i;
    ngx_http_variable_t        *v;         
    ngx_http_core_main_conf_t  *cmcf;

    if (name->len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid variable name \"$\"");
        return NGX_ERROR;
    }

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    v = cmcf->variables.elts;

    if (v == NULL) {
        if (ngx_array_init(&cmcf->variables, cf->pool, 4, 
                           sizeof(ngx_http_variable_t))
            != NGX_OK)
        {
            return NGX_ERROR;
        }

    } else {
        for (i = 0; i < cmcf->variables.nelts; i++) {
            if (name->len != v[i].name.len
                || ngx_strncasecmp(name->data, v[i].name.data, name->len) != 0)
            {
                continue;
            }

            return i; //返回数组中的下标
        }
    }

    v = ngx_array_push(&cmcf->variables);
    if (v == NULL) {
        return NGX_ERROR;
    }

    v->name.len = name->len;
    v->name.data = ngx_pnalloc(cf->pool, name->len);
    if (v->name.data == NULL) {
        return NGX_ERROR;
    }

    ngx_strlow(v->name.data, name->data, name->len);

    v->set_handler = NULL;
    v->get_handler = NULL;
    v->data = 0;
    v->flags = 0;
    v->index = cmcf->variables.nelts - 1; //标记在cmcf->variables数组中的位置

    return v->index; //返回数组中的下标  ngx_http_variable_t->index
}

/*
ngx_http_get_variable_index会把变量加入一个array并返回变量在array中的下标，这样模块保存变量的下标，并通过
ngx_http_get_indexed_variable直接得到变量值
*/
ngx_http_variable_value_t *
ngx_http_get_indexed_variable(ngx_http_request_t *r, ngx_uint_t index)
{
    ngx_http_variable_t        *v;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    if (cmcf->variables.nelts <= index) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "unknown variable index: %ui", index);
        return NULL;
    }

    if (r->variables[index].not_found || r->variables[index].valid) {
        return &r->variables[index];
    }

    v = cmcf->variables.elts;

    //ngx_http_rewrite_set中设置为ngx_http_rewrite_var  args的get_handler是ngx_http_variable_request ngx_http_variables_init_vars中设置"http_"等的get_handler
    if (v[index].get_handler(r, &r->variables[index], v[index].data)
        == NGX_OK)
    {
        if (v[index].flags & NGX_HTTP_VAR_NOCACHEABLE) {
            r->variables[index].no_cacheable = 1;
        }

        return &r->variables[index];
    }

    r->variables[index].valid = 0;
    r->variables[index].not_found = 1;

    return NULL;
}


ngx_http_variable_value_t *
ngx_http_get_flushed_variable(ngx_http_request_t *r, ngx_uint_t index)
{
    ngx_http_variable_value_t  *v;

    v = &r->variables[index];

    if (v->valid || v->not_found) {
        if (!v->no_cacheable) {
            return v;
        }

        v->valid = 0;
        v->not_found = 0;
    }

    return ngx_http_get_indexed_variable(r, index);
}

/*
NGX HTTP_VAR_INDEXED、NGXHTTP_VARNOHASH、变量cmcf->variables_hash以及取值函数ngx_http_get_variable等，它们都是为SSI模块实现而设计的
*/
ngx_http_variable_value_t *
ngx_http_get_variable(ngx_http_request_t *r, ngx_str_t *name, ngx_uint_t key)
{
    ngx_http_variable_t        *v;
    ngx_http_variable_value_t  *vv;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    v = ngx_hash_find(&cmcf->variables_hash, key, name->data, name->len);

    if (v) {
        if (v->flags & NGX_HTTP_VAR_INDEXED) {
            return ngx_http_get_flushed_variable(r, v->index);

        } else {

            vv = ngx_palloc(r->pool, sizeof(ngx_http_variable_value_t));

            if (vv && v->get_handler(r, vv, v->data) == NGX_OK) {
                return vv;
            }

            return NULL;
        }
    }

    vv = ngx_palloc(r->pool, sizeof(ngx_http_variable_value_t));
    if (vv == NULL) {
        return NULL;
    }

    if (ngx_strncmp(name->data, "http_", 5) == 0) {

        if (ngx_http_variable_unknown_header_in(r, vv, (uintptr_t) name)
            == NGX_OK)
        {
            return vv;
        }

        return NULL;
    }

    if (ngx_strncmp(name->data, "sent_http_", 10) == 0) {

        if (ngx_http_variable_unknown_header_out(r, vv, (uintptr_t) name)
            == NGX_OK)
        {
            return vv;
        }

        return NULL;
    }

    if (ngx_strncmp(name->data, "upstream_http_", 14) == 0) {

        if (ngx_http_upstream_header_variable(r, vv, (uintptr_t) name)
            == NGX_OK)
        {
            return vv;
        }

        return NULL;
    }

    if (ngx_strncmp(name->data, "cookie_", 7) == 0) {

        if (ngx_http_variable_cookie(r, vv, (uintptr_t) name) == NGX_OK) {
            return vv;
        }

        return NULL;
    }

    if (ngx_strncmp(name->data, "upstream_cookie_", 16) == 0) {

        if (ngx_http_upstream_cookie_variable(r, vv, (uintptr_t) name)
            == NGX_OK)
        {
            return vv;
        }

        return NULL;
    }

    if (ngx_strncmp(name->data, "arg_", 4) == 0) {

        if (ngx_http_variable_argument(r, vv, (uintptr_t) name) == NGX_OK) {
            return vv;
        }

        return NULL;
    }

    vv->not_found = 1;

    return vv;
}


static ngx_int_t
ngx_http_variable_request(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    ngx_str_t  *s;

    s = (ngx_str_t *) ((char *) r + data);

    if (s->data) {
        v->len = s->len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = s->data;

    } else {
        v->not_found = 1;
    }

    return NGX_OK;
}


#if 0

static void
ngx_http_variable_request_set(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_str_t  *s;

    s = (ngx_str_t *) ((char *) r + data);

    s->len = v->len;
    s->data = v->data;
}

#endif


static ngx_int_t
ngx_http_variable_request_get_size(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    size_t  *sp;

    sp = (size_t *) ((char *) r + data);

    v->data = ngx_pnalloc(r->pool, NGX_SIZE_T_LEN);
    if (v->data == NULL) {
        return NGX_ERROR;
    }

    v->len = ngx_sprintf(v->data, "%uz", *sp) - v->data;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}


static void
ngx_http_variable_request_set_size(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ssize_t    s, *sp;
    ngx_str_t  val;

    val.len = v->len;
    val.data = v->data;

    s = ngx_parse_size(&val);

    if (s == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "invalid size \"%V\"", &val);
        return;
    }

    sp = (ssize_t *) ((char *) r + data);

    *sp = s;

    return;
}


static ngx_int_t
ngx_http_variable_header(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    ngx_table_elt_t  *h;

    h = *(ngx_table_elt_t **) ((char *) r + data);

    if (h) {
        v->len = h->value.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = h->value.data;

    } else {
        v->not_found = 1;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_cookies(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    return ngx_http_variable_headers_internal(r, v, data, ';');
}


static ngx_int_t
ngx_http_variable_headers(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    return ngx_http_variable_headers_internal(r, v, data, ',');
}


static ngx_int_t
ngx_http_variable_headers_internal(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data, u_char sep)
{
    size_t             len;
    u_char            *p, *end;
    ngx_uint_t         i, n;
    ngx_array_t       *a;
    ngx_table_elt_t  **h;

    a = (ngx_array_t *) ((char *) r + data);

    n = a->nelts;
    h = a->elts;

    len = 0;

    for (i = 0; i < n; i++) {

        if (h[i]->hash == 0) {
            continue;
        }

        len += h[i]->value.len + 2;
    }

    if (len == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    len -= 2;

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    if (n == 1) {
        v->len = (*h)->value.len;
        v->data = (*h)->value.data;

        return NGX_OK;
    }

    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->len = len;
    v->data = p;

    end = p + len;

    for (i = 0; /* void */ ; i++) {

        if (h[i]->hash == 0) {
            continue;
        }

        p = ngx_copy(p, h[i]->value.data, h[i]->value.len);

        if (p == end) {
            break;
        }

        *p++ = sep; *p++ = ' ';
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_unknown_header_in(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    return ngx_http_variable_unknown_header(v, (ngx_str_t *) data,
                                            &r->headers_in.headers.part,
                                            sizeof("http_") - 1);
}


static ngx_int_t
ngx_http_variable_unknown_header_out(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    return ngx_http_variable_unknown_header(v, (ngx_str_t *) data,
                                            &r->headers_out.headers.part,
                                            sizeof("sent_http_") - 1);
}


ngx_int_t
ngx_http_variable_unknown_header(ngx_http_variable_value_t *v, ngx_str_t *var,
    ngx_list_part_t *part, size_t prefix)
{
    u_char            ch;
    ngx_uint_t        i, n;
    ngx_table_elt_t  *header;

    header = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            header = part->elts;
            i = 0;
        }

        if (header[i].hash == 0) {
            continue;
        }

        for (n = 0; n + prefix < var->len && n < header[i].key.len; n++) {
            ch = header[i].key.data[n];

            if (ch >= 'A' && ch <= 'Z') {
                ch |= 0x20;

            } else if (ch == '-') {
                ch = '_';
            }

            if (var->data[n + prefix] != ch) {
                break;
            }
        }

        if (n + prefix == var->len && n == header[i].key.len) {
            v->len = header[i].value.len;
            v->valid = 1;
            v->no_cacheable = 0;
            v->not_found = 0;
            v->data = header[i].value.data;

            return NGX_OK;
        }
    }

    v->not_found = 1;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_request_line(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char  *p, *s;

    s = r->request_line.data;

    if (s == NULL) {
        s = r->request_start;

        if (s == NULL) {
            v->not_found = 1;
            return NGX_OK;
        }

        for (p = s; p < r->header_in->last; p++) {
            if (*p == CR || *p == LF) {
                break;
            }
        }

        r->request_line.len = p - s;
        r->request_line.data = s;
    }

    v->len = r->request_line.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = s;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_cookie(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    ngx_str_t *name = (ngx_str_t *) data;

    ngx_str_t  cookie, s;

    s.len = name->len - (sizeof("cookie_") - 1);
    s.data = name->data + sizeof("cookie_") - 1;

    if (ngx_http_parse_multi_header_lines(&r->headers_in.cookies, &s, &cookie)
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

/*把请求中GET /download/nginx-1.9.2.rar?st=xhWL03HbtjrojpEAfiD6Mw&e=1452139931 HTTP/1.1的st和e形成变量$arg_st #arg_e，value分别
为xhWL03HbtjrojpEAfiD6Mw 1452139931即$arg_st=xhWL03HbtjrojpEAfiD6Mw，#arg_e=1452139931 */
static ngx_int_t
ngx_http_variable_argument(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    ngx_str_t *name = (ngx_str_t *) data;

    u_char     *arg;
    size_t      len;
    ngx_str_t   value;

    len = name->len - (sizeof("arg_") - 1);
    arg = name->data + sizeof("arg_") - 1;

    if (ngx_http_arg(r, arg, len, &value) != NGX_OK) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->data = value.data;
    v->len = value.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}


#if (NGX_HAVE_TCP_INFO)

static ngx_int_t
ngx_http_variable_tcpinfo(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    struct tcp_info  ti;
    socklen_t        len;
    uint32_t         value;

    len = sizeof(struct tcp_info);
    if (getsockopt(r->connection->fd, IPPROTO_TCP, TCP_INFO, &ti, &len) == -1) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->data = ngx_pnalloc(r->pool, NGX_INT32_LEN);
    if (v->data == NULL) {
        return NGX_ERROR;
    }

    switch (data) {
    case 0:
        value = ti.tcpi_rtt;
        break;

    case 1:
        value = ti.tcpi_rttvar;
        break;

    case 2:
        value = ti.tcpi_snd_cwnd;
        break;

    case 3:
        value = ti.tcpi_rcv_space;
        break;

    /* suppress warning */
    default:
        value = 0;
        break;
    }

    v->len = ngx_sprintf(v->data, "%uD", value) - v->data;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}

#endif


static ngx_int_t
ngx_http_variable_content_length(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    if (r->headers_in.content_length) {
        v->len = r->headers_in.content_length->value.len;
        v->data = r->headers_in.content_length->value.data;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;

    } else if (r->reading_body) {
        v->not_found = 1;
        v->no_cacheable = 1;

    } else if (r->headers_in.content_length_n >= 0) {
        p = ngx_pnalloc(r->pool, NGX_OFF_T_LEN);
        if (p == NULL) {
            return NGX_ERROR;
        }

        v->len = ngx_sprintf(p, "%O", r->headers_in.content_length_n) - p;
        v->data = p;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;

    } else {
        v->not_found = 1;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_host(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    ngx_http_core_srv_conf_t  *cscf;

    if (r->headers_in.server.len) {
        v->len = r->headers_in.server.len;
        v->data = r->headers_in.server.data;

    } else {
        cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

        v->len = cscf->server_name.len;
        v->data = cscf->server_name.data;
    }

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_binary_remote_addr(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    struct sockaddr_in   *sin;
#if (NGX_HAVE_INET6)
    struct sockaddr_in6  *sin6;
#endif

    switch (r->connection->sockaddr->sa_family) {

#if (NGX_HAVE_INET6)
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) r->connection->sockaddr;

        v->len = sizeof(struct in6_addr);
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = sin6->sin6_addr.s6_addr;

        break;
#endif

    default: /* AF_INET */
        sin = (struct sockaddr_in *) r->connection->sockaddr;

        v->len = sizeof(in_addr_t);
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = (u_char *) &sin->sin_addr;

        break;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_remote_addr(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    v->len = r->connection->addr_text.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = r->connection->addr_text.data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_remote_port(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_uint_t            port;
    struct sockaddr_in   *sin;
#if (NGX_HAVE_INET6)
    struct sockaddr_in6  *sin6;
#endif

    v->len = 0;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    v->data = ngx_pnalloc(r->pool, sizeof("65535") - 1);
    if (v->data == NULL) {
        return NGX_ERROR;
    }

    switch (r->connection->sockaddr->sa_family) {

#if (NGX_HAVE_INET6)
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) r->connection->sockaddr;
        port = ntohs(sin6->sin6_port);
        break;
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
    case AF_UNIX:
        port = 0;
        break;
#endif

    default: /* AF_INET */
        sin = (struct sockaddr_in *) r->connection->sockaddr;
        port = ntohs(sin->sin_port);
        break;
    }

    if (port > 0 && port < 65536) {
        v->len = ngx_sprintf(v->data, "%ui", port) - v->data;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_proxy_protocol_addr(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    v->len = r->connection->proxy_protocol_addr.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = r->connection->proxy_protocol_addr.data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_server_addr(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_str_t  s;
    u_char     addr[NGX_SOCKADDR_STRLEN];

    s.len = NGX_SOCKADDR_STRLEN;
    s.data = addr;

    if (ngx_connection_local_sockaddr(r->connection, &s, 0) != NGX_OK) {
        return NGX_ERROR;
    }

    s.data = ngx_pnalloc(r->pool, s.len);
    if (s.data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(s.data, addr, s.len);

    v->len = s.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = s.data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_server_port(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_uint_t            port;
    struct sockaddr_in   *sin;
#if (NGX_HAVE_INET6)
    struct sockaddr_in6  *sin6;
#endif

    v->len = 0;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    if (ngx_connection_local_sockaddr(r->connection, NULL, 0) != NGX_OK) {
        return NGX_ERROR;
    }

    v->data = ngx_pnalloc(r->pool, sizeof("65535") - 1);
    if (v->data == NULL) {
        return NGX_ERROR;
    }

    switch (r->connection->local_sockaddr->sa_family) {

#if (NGX_HAVE_INET6)
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) r->connection->local_sockaddr;
        port = ntohs(sin6->sin6_port);
        break;
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
    case AF_UNIX:
        port = 0;
        break;
#endif

    default: /* AF_INET */
        sin = (struct sockaddr_in *) r->connection->local_sockaddr;
        port = ntohs(sin->sin_port);
        break;
    }

    if (port > 0 && port < 65536) {
        v->len = ngx_sprintf(v->data, "%ui", port) - v->data;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_scheme(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
#if (NGX_HTTP_SSL)

    if (r->connection->ssl) {
        v->len = sizeof("https") - 1;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = (u_char *) "https";

        return NGX_OK;
    }

#endif

    v->len = sizeof("http") - 1;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = (u_char *) "http";

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_https(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
#if (NGX_HTTP_SSL)

    if (r->connection->ssl) {
        v->len = sizeof("on") - 1;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = (u_char *) "on";

        return NGX_OK;
    }

#endif

    *v = ngx_http_variable_null_value;

    return NGX_OK;
}


static void
ngx_http_variable_set_args(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    r->args.len = v->len;
    r->args.data = v->data;
    r->valid_unparsed_uri = 0;
}


static ngx_int_t
ngx_http_variable_is_args(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    if (r->args.len == 0) {
        v->len = 0;
        v->data = NULL;
        return NGX_OK;
    }

    v->len = 1;
    v->data = (u_char *) "?";

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_document_root(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_str_t                  path;
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (clcf->root_lengths == NULL) {
        v->len = clcf->root.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = clcf->root.data;

    } else {
        if (ngx_http_script_run(r, &path, clcf->root_lengths->elts, 0,
                                clcf->root_values->elts)
            == NULL)
        {
            return NGX_ERROR;
        }

        if (ngx_get_full_name(r->pool, (ngx_str_t *) &ngx_cycle->prefix, &path)
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        v->len = path.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = path.data;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_realpath_root(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                    *real;
    size_t                     len;
    ngx_str_t                  path;
    ngx_http_core_loc_conf_t  *clcf;
#if (NGX_HAVE_MAX_PATH)
    u_char                     buffer[NGX_MAX_PATH];
#endif

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (clcf->root_lengths == NULL) {
        path = clcf->root;

    } else {
        if (ngx_http_script_run(r, &path, clcf->root_lengths->elts, 1,
                                clcf->root_values->elts)
            == NULL)
        {
            return NGX_ERROR;
        }

        path.data[path.len - 1] = '\0';

        if (ngx_get_full_name(r->pool, (ngx_str_t *) &ngx_cycle->prefix, &path)
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

#if (NGX_HAVE_MAX_PATH)
    real = buffer;
#else
    real = NULL;
#endif

    real = ngx_realpath(path.data, real);

    if (real == NULL) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                      ngx_realpath_n " \"%s\" failed", path.data);
        return NGX_ERROR;
    }

    len = ngx_strlen(real);

    v->data = ngx_pnalloc(r->pool, len);
    if (v->data == NULL) {
#if !(NGX_HAVE_MAX_PATH)
        ngx_free(real);
#endif
        return NGX_ERROR;
    }

    v->len = len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    ngx_memcpy(v->data, real, len);

#if !(NGX_HAVE_MAX_PATH)
    ngx_free(real);
#endif

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_request_filename(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    size_t     root;
    ngx_str_t  path;

    if (ngx_http_map_uri_to_path(r, &path, &root, 0) == NULL) {
        return NGX_ERROR;
    }

    /* ngx_http_map_uri_to_path() allocates memory for terminating '\0' */

    v->len = path.len - 1;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = path.data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_server_name(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_core_srv_conf_t  *cscf;

    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

    v->len = cscf->server_name.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = cscf->server_name.data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_request_method(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    if (r->main->method_name.data) {
        v->len = r->main->method_name.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = r->main->method_name.data;

    } else {
        v->not_found = 1;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_remote_user(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_int_t  rc;

    rc = ngx_http_auth_basic_user(r);

    if (rc == NGX_DECLINED) {
        v->not_found = 1;
        return NGX_OK;
    }

    if (rc == NGX_ERROR) {
        return NGX_ERROR;
    }

    v->len = r->headers_in.user.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = r->headers_in.user.data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_bytes_sent(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    p = ngx_pnalloc(r->pool, NGX_OFF_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->len = ngx_sprintf(p, "%O", r->connection->sent) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_body_bytes_sent(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    off_t    sent;
    u_char  *p;

    sent = r->connection->sent - r->header_size;

    if (sent < 0) {
        sent = 0;
    }

    p = ngx_pnalloc(r->pool, NGX_OFF_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->len = ngx_sprintf(p, "%O", sent) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_pipe(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    v->data = (u_char *) (r->pipeline ? "p" : ".");
    v->len = 1;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_status(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_uint_t  status;

    v->data = ngx_pnalloc(r->pool, NGX_INT_T_LEN);
    if (v->data == NULL) {
        return NGX_ERROR;
    }

    if (r->err_status) {
        status = r->err_status;

    } else if (r->headers_out.status) {
        status = r->headers_out.status;

    } else if (r->http_version == NGX_HTTP_VERSION_9) {
        status = 9;

    } else {
        status = 0;
    }

    v->len = ngx_sprintf(v->data, "%03ui", status) - v->data;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_sent_content_type(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    if (r->headers_out.content_type.len) {
        v->len = r->headers_out.content_type.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = r->headers_out.content_type.data;

    } else {
        v->not_found = 1;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_sent_content_length(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    if (r->headers_out.content_length) {
        v->len = r->headers_out.content_length->value.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = r->headers_out.content_length->value.data;

        return NGX_OK;
    }

    if (r->headers_out.content_length_n >= 0) {
        p = ngx_pnalloc(r->pool, NGX_OFF_T_LEN);
        if (p == NULL) {
            return NGX_ERROR;
        }

        v->len = ngx_sprintf(p, "%O", r->headers_out.content_length_n) - p;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = p;

        return NGX_OK;
    }

    v->not_found = 1;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_sent_location(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_str_t  name;

    if (r->headers_out.location) {
        v->len = r->headers_out.location->value.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = r->headers_out.location->value.data;

        return NGX_OK;
    }

    ngx_str_set(&name, "sent_http_location");

    return ngx_http_variable_unknown_header(v, &name,
                                            &r->headers_out.headers.part,
                                            sizeof("sent_http_") - 1);
}


static ngx_int_t
ngx_http_variable_sent_last_modified(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    if (r->headers_out.last_modified) {
        v->len = r->headers_out.last_modified->value.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = r->headers_out.last_modified->value.data;

        return NGX_OK;
    }

    if (r->headers_out.last_modified_time >= 0) {
        p = ngx_pnalloc(r->pool, sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1);
        if (p == NULL) {
            return NGX_ERROR;
        }

        v->len = ngx_http_time(p, r->headers_out.last_modified_time) - p;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = p;

        return NGX_OK;
    }

    v->not_found = 1;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_sent_connection(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    size_t   len;
    char    *p;

    if (r->headers_out.status == NGX_HTTP_SWITCHING_PROTOCOLS) {
        len = sizeof("upgrade") - 1;
        p = "upgrade";

    } else if (r->keepalive) {
        len = sizeof("keep-alive") - 1;
        p = "keep-alive";

    } else {
        len = sizeof("close") - 1;
        p = "close";
    }

    v->len = len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = (u_char *) p;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_sent_keep_alive(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                    *p;
    ngx_http_core_loc_conf_t  *clcf;

    if (r->keepalive) {
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        if (clcf->keepalive_header) {

            p = ngx_pnalloc(r->pool, sizeof("timeout=") - 1 + NGX_TIME_T_LEN);
            if (p == NULL) {
                return NGX_ERROR;
            }

            v->len = ngx_sprintf(p, "timeout=%T", clcf->keepalive_header) - p;
            v->valid = 1;
            v->no_cacheable = 0;
            v->not_found = 0;
            v->data = p;

            return NGX_OK;
        }
    }

    v->not_found = 1;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_sent_transfer_encoding(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    if (r->chunked) {
        v->len = sizeof("chunked") - 1;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = (u_char *) "chunked";

    } else {
        v->not_found = 1;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_request_completion(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    if (r->request_complete) {
        v->len = 2;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = (u_char *) "OK";

        return NGX_OK;
    }

    v->len = 0;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = (u_char *) "";

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_request_body(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char       *p;
    size_t        len;
    ngx_buf_t    *buf;
    ngx_chain_t  *cl;

    if (r->request_body == NULL
        || r->request_body->bufs == NULL
        || r->request_body->temp_file)
    {
        v->not_found = 1;

        return NGX_OK;
    }

    cl = r->request_body->bufs;
    buf = cl->buf;

    if (cl->next == NULL) {
        v->len = buf->last - buf->pos;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = buf->pos;

        return NGX_OK;
    }

    len = buf->last - buf->pos;
    cl = cl->next;

    for ( /* void */ ; cl; cl = cl->next) {
        buf = cl->buf;
        len += buf->last - buf->pos;
    }

    p = ngx_pnalloc(r->pool, len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;
    cl = r->request_body->bufs;

    for ( /* void */ ; cl; cl = cl->next) {
        buf = cl->buf;
        p = ngx_cpymem(p, buf->pos, buf->last - buf->pos);
    }

    v->len = len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_request_body_file(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    if (r->request_body == NULL || r->request_body->temp_file == NULL) {
        v->not_found = 1;

        return NGX_OK;
    }

    v->len = r->request_body->temp_file->file.name.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = r->request_body->temp_file->file.name.data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_request_length(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    p = ngx_pnalloc(r->pool, NGX_OFF_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->len = ngx_sprintf(p, "%O", r->request_length) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_request_time(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char          *p;
    ngx_time_t      *tp;
    ngx_msec_int_t   ms;

    p = ngx_pnalloc(r->pool, NGX_TIME_T_LEN + 4);
    if (p == NULL) {
        return NGX_ERROR;
    }

    tp = ngx_timeofday();

    ms = (ngx_msec_int_t)
             ((tp->sec - r->start_sec) * 1000 + (tp->msec - r->start_msec));
    ms = ngx_max(ms, 0);

    v->len = ngx_sprintf(p, "%T.%03M", (time_t) ms / 1000, ms % 1000) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_connection(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    p = ngx_pnalloc(r->pool, NGX_ATOMIC_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->len = ngx_sprintf(p, "%uA", r->connection->number) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_connection_requests(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    p = ngx_pnalloc(r->pool, NGX_INT_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->len = ngx_sprintf(p, "%ui", r->connection->requests) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_nginx_version(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    v->len = sizeof(NGINX_VERSION) - 1;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = (u_char *) NGINX_VERSION;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_hostname(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    v->len = ngx_cycle->hostname.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = ngx_cycle->hostname.data;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_pid(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    p = ngx_pnalloc(r->pool, NGX_INT64_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->len = ngx_sprintf(p, "%P", ngx_pid) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_msec(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char      *p;
    ngx_time_t  *tp;

    p = ngx_pnalloc(r->pool, NGX_TIME_T_LEN + 4);
    if (p == NULL) {
        return NGX_ERROR;
    }

    tp = ngx_timeofday();

    v->len = ngx_sprintf(p, "%T.%03M", tp->sec, tp->msec) - p;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_time_iso8601(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    p = ngx_pnalloc(r->pool, ngx_cached_http_log_iso8601.len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(p, ngx_cached_http_log_iso8601.data,
               ngx_cached_http_log_iso8601.len);

    v->len = ngx_cached_http_log_iso8601.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


static ngx_int_t
ngx_http_variable_time_local(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char  *p;

    p = ngx_pnalloc(r->pool, ngx_cached_http_log_time.len);
    if (p == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(p, ngx_cached_http_log_time.data, ngx_cached_http_log_time.len);

    v->len = ngx_cached_http_log_time.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


void *
ngx_http_map_find(ngx_http_request_t *r, ngx_http_map_t *map, ngx_str_t *match)
{
    void        *value;
    u_char      *low;
    size_t       len;
    ngx_uint_t   key;

    len = match->len;

    if (len) {
        low = ngx_pnalloc(r->pool, len);
        if (low == NULL) {
            return NULL;
        }

    } else {
        low = NULL;
    }

    key = ngx_hash_strlow(low, match->data, len);

    value = ngx_hash_find_combined(&map->hash, key, low, len);
    if (value) {
        return value;
    }

#if (NGX_PCRE)

    if (len && map->nregex) {
        ngx_int_t              n;
        ngx_uint_t             i;
        ngx_http_map_regex_t  *reg;

        reg = map->regex;

        for (i = 0; i < map->nregex; i++) {

            n = ngx_http_regex_exec(r, reg[i].regex, match);

            if (n == NGX_OK) {
                return reg[i].value;
            }

            if (n == NGX_DECLINED) {
                continue;
            }

            /* NGX_ERROR */

            return NULL;
        }
    }

#endif

    return NULL;
}


#if (NGX_PCRE)

static ngx_int_t
ngx_http_variable_not_found(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    v->not_found = 1;
    return NGX_OK;
}

/*
1. 调用ngx_regex_compile编译正则表达式，并且得到相关的数据，比如子模式数目，命名子模式数目，列表等。
2.	将正则表达式信息存储到ngx_http_regex_t里面，包括正则句柄，子模式数目
3. 	将命名子模式 加入到cmcf->variables_keys和cmcf->variables中。以备后续通过名字查找变量值。*/
ngx_http_regex_t *
ngx_http_regex_compile(ngx_conf_t *cf, ngx_regex_compile_t *rc)
{
    u_char                     *p;
    size_t                      size;
    ngx_str_t                   name;
    ngx_uint_t                  i, n;
    ngx_http_variable_t        *v;
    ngx_http_regex_t           *re;
    ngx_http_regex_variable_t  *rv;
    ngx_http_core_main_conf_t  *cmcf;

    rc->pool = cf->pool;

    if (ngx_regex_compile(rc) != NGX_OK) { //调用pcre_compile编译正则表达。返回结果存在rc->regex = re;
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "%V", &rc->err);
        return NULL;
    }

    re = ngx_pcalloc(cf->pool, sizeof(ngx_http_regex_t));
    if (re == NULL) {
        return NULL;
    }

    re->regex = rc->regex;
    re->ncaptures = rc->captures;//得到$2,$3有多少个。pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &rc->captures);
    re->name = rc->pattern;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    cmcf->ncaptures = ngx_max(cmcf->ncaptures, re->ncaptures); 

    n = (ngx_uint_t) rc->named_captures;//命名的子模式: pcre_fullinfo(re, NULL, PCRE_INFO_NAMECOUNT, &rc->named_captures);

    if (n == 0) { //如果命名子模式(?<name>exp)没有
        return re;
    }

    rv = ngx_palloc(rc->pool, n * sizeof(ngx_http_regex_variable_t));
    if (rv == NULL) {//为每一个命名变量申请空间单独存储。
        return NULL;
    }

    re->variables = rv;//记录这种命名的子模式
    re->nvariables = n;

    size = rc->name_size; 
    p = rc->names; //正则的命名子模式的二维表，里面记录了子模式的名称，以及在所有模式中的下标.
    //names是这么得到的: pcre_fullinfo(re, NULL, PCRE_INFO_NAMETABLE, &rc->names);
    //一个命名子模式的二维表存储在p的位置。每一行是一个命名模式，每行的字节数为name_size
    //参考http://blog.csdn.net/kofiory/article/details/5829697

    ///ngx_http_regex_compile中分配空间,保存变量   ngx_http_regex_exec获取对应变量的值
    for (i = 0; i < n; i++) { //遍历每一个named_captures，
        rv[i].capture = 2 * ((p[0] << 8) + p[1]); //第一个自己左移8位+第二个字节等于这个命名子模式在所有模式中的下标。

        name.data = &p[2]; //第三个字节开始就是命名的名字。
        name.len = ngx_strlen(name.data); //名字长度
        
        ///将这个命名子模式加入到cmcf->variables_keys中
        v = ngx_http_add_variable(cf, &name, NGX_HTTP_VAR_CHANGEABLE);
        if (v == NULL) {
            return NULL;
        }

        //然后从cmcf->variables中查找这个变量对应的下标是多少，如果没有，当然就增加到里面去。
        rv[i].index = ngx_http_get_variable_index(cf, &name);
        if (rv[i].index == NGX_ERROR) {
            return NULL;
        }

        //初始化为空函数
        v->get_handler = ngx_http_variable_not_found;

        p += size;
    }

    return re;
}

//用已经编译的regex 跟e->line,也就是s去匹配，看看是否匹配成功。如果匹配成功，整理存放在r->variables里面、
ngx_int_t
ngx_http_regex_exec(ngx_http_request_t *r, ngx_http_regex_t *re, ngx_str_t *s)
{
    ngx_int_t                   rc, index;
    ngx_uint_t                  i, n, len;
    ngx_http_variable_value_t  *vv;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    if (re->ncaptures) { //根据ncaptures大小，申请内存用于存放正则匹配的结果。
        len = cmcf->ncaptures;

        if (r->captures == NULL) {
            r->captures = ngx_palloc(r->pool, len * sizeof(int));
            if (r->captures == NULL) {
                return NGX_ERROR;
            }
        }

    } else {
        len = 0;
    }

    //根据正则表达式到指定的字符串中进行查找和匹配,并输出匹配的结果.
    rc = ngx_regex_exec(re->regex, s, r->captures, len); //len的取值见http://www.rosoo.net/a/201004/9082.html

    if (rc == NGX_REGEX_NO_MATCHED) {
        return NGX_DECLINED;
    }

    if (rc < 0) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      ngx_regex_exec_n " failed: %i on \"%V\" using \"%V\"",
                      rc, s, &re->name);
        return NGX_ERROR;
    }

    ////ngx_http_regex_compile中分配空间,保存变量   ngx_http_regex_exec获取对应变量的值
    for (i = 0; i < re->nvariables; i++) { //把正则表达式中的命名子模式的变量获取到
        /*
           例如:rewrite   ^(?<name1>/download/.*)/media/(?<name2>.*)\..*$    $name1/mp3/$name2.mp3  break;
           如果uri为/download/aaa/media/bbb.com，则name1变量为/download/aaa  name2变量为bbb 
           下面的vv->len和vv->data分别指向"/download/aaa"字符串长度，和字符串"/download/aaa"
          */
        n = re->variables[i].capture;
        index = re->variables[i].index;
        vv = &r->variables[index];

        //从s和re的对应关系中，从s中获取re中的变量的值
        vv->len = r->captures[n + 1] - r->captures[n];
        vv->valid = 1;
        vv->no_cacheable = 0;
        vv->not_found = 0;
        vv->data = &s->data[r->captures[n]];
        
#if (NGX_DEBUG)
        {
        ngx_http_variable_t  *v;

        v = cmcf->variables.elts;

        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http regex set $%V to \"%*s\"",
                       &v[index].name, vv->len, vv->data);
        }
#endif
    }

    /*  
     例如正则表达式语句re.name= ^(/download/.*)/media/(.*)/tt/(.*)$，  s=/download/aa/media/bdb/tt/ad,则他们会匹配，同时匹配的
     变量数有3个，则返回值为3+1=4,如果不匹配则返回-1

     这里*2是因为获取前面例子中的3个变量对应的值需要成对使用r->captures，参考ngx_http_script_copy_capture_code等
     */
    r->ncaptures = rc * 2;  
    r->captures_data = s->data;
    
    return NGX_OK;
}

#endif

//ngx_http_variables_add_core_vars把ngx_http_core_variables中的各种变量信息存放到cmcf->variables_keys中
ngx_int_t
ngx_http_variables_add_core_vars(ngx_conf_t *cf)
{
    ngx_int_t                   rc;
    ngx_http_variable_t        *cv, *v;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    cmcf->variables_keys = ngx_pcalloc(cf->temp_pool,
                                       sizeof(ngx_hash_keys_arrays_t));
    if (cmcf->variables_keys == NULL) {
        return NGX_ERROR;
    }

    cmcf->variables_keys->pool = cf->pool;
    cmcf->variables_keys->temp_pool = cf->pool;

    if (ngx_hash_keys_array_init(cmcf->variables_keys, NGX_HASH_SMALL)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    for (cv = ngx_http_core_variables; cv->name.len; cv++) {
        v = ngx_palloc(cf->pool, sizeof(ngx_http_variable_t));
        if (v == NULL) {
            return NGX_ERROR;
        }

        *v = *cv;

        rc = ngx_hash_add_key(cmcf->variables_keys, &v->name, v,
                              NGX_HASH_READONLY_KEY);

        if (rc == NGX_OK) {
            continue;
        }

        if (rc == NGX_BUSY) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "conflicting variable name \"%V\"", &v->name);
        }

        return NGX_ERROR;
    }

    return NGX_OK;
}

/*
这个合法性检测逻辑很简单，实现在函数ngx_http_variables_init_vars内，其遍历cmcf->variables内收集的所有已使用变量，逐个去已定义变量
cmcf->variableskeys集合罩查找。如果找到则表示用户使用无误，如果没找到，则需要注意，这还只能说明它可能是一个非法变量。
    有一部分变量虽然没有包含在cmcf->variables_key内，但是它们却合法。这部分变量是以”http_”、”sent_http_”、”upstream http”、”cookie_”、
”arg”开头的五类变量。这些变量庞大并且不可预知，不可能提前定义并收集到cmcf->variables_keys内，比如以”arg”开头代表的参数类变量会根
据客户端请求URI时附带的参数不I司而不同，一个类似于“http:// 192.168. 164.2/?pageid=2”这样的请求会自动生成变量$argpageid。因此还需判断
用户在配置文件里使用的变量是否在这五类变量里，具体来说就是检测用户使用的变量名前面几个字符是否与它们一致（这也间接说明，用户自定义变量时
最好不要以这些字符开头）。当然，如果用户在配置文件里使用了变量$argpageid，而客户端请求时却并没有带上pageid参数，此时变量$arg_pageid值为空，
但它总还算是合法的，但如果提示类似如下这样的错误，请需检查配置文件内变量名是否书写正确。(unknown "XXX" variable)
    函数ngx_http_variables_init_vars()在对已使用变量进行合法性检测的同时，对于合法的使用变量会将其对应的三个主要字段设置好，
即get_handler0回调、data数据、flags旗标。从前面给出的结构体ngx_http_variables定义来看，name存储的是变量名字符串，index存储的
是该变量在cmcf->variables内的下标（通过函数ngx_http_get_variable_index获得），这两个都是不变的，而set_handlerr()回调目前只在
使用set配置指令构造脚本引擎时才会用到，而那里直接使用cmcf->variables_keys里对应变量的该字段，并且一旦配置文件解析完毕，
set_handlerr()回调也就用不上了，所以只有剩下的三个字段才需要做赋值操作，即从cmcf->variables_keys里对应变量的对应字段拷贝过来，
或是另外五类变量就根据不同类别进行固定的赋值。
*/
ngx_int_t
ngx_http_variables_init_vars(ngx_conf_t *cf)
{
    ngx_uint_t                  i, n;
    ngx_hash_key_t             *key;
    ngx_hash_init_t             hash;
    ngx_http_variable_t        *v, *av;
    ngx_http_core_main_conf_t  *cmcf;

    /* set the handlers for the indexed http variables */

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    v = cmcf->variables.elts;
    key = cmcf->variables_keys->keys.elts;

    for (i = 0; i < cmcf->variables.nelts; i++) {

        for (n = 0; n < cmcf->variables_keys->keys.nelts; n++) {

            av = key[n].value;

            if (v[i].name.len == key[n].key.len
                && ngx_strncmp(v[i].name.data, key[n].key.data, v[i].name.len)
                   == 0)
            { //对于合法的使用变量会将其对应的三个主要字段设置好，即get_handler0回调、data数据、flags旗标
                v[i].get_handler = av->get_handler;
                v[i].data = av->data;

                av->flags |= NGX_HTTP_VAR_INDEXED;
                v[i].flags = av->flags;

                av->index = i;

                if (av->get_handler == NULL) {
                    break;
                }

                goto next;
            }
        }

        /*
          ngx_http_variables_init_vars会检测variables中的变量是否在variables_keys中，如果不在，说明这个使用到的变量有可能是没有定义过的，
          这样是不能使用的。为什么是有可能？看for循环下面的那几个if条件判断，如果不在variables_keys中的变量在下面的if判断中就说明这个
          变量是合法的，否则不合法

            有一部分变量虽然没有包含在cmcf->variables_key内，但是它们却合法。这部分变量是以”http_”、”sent_http_”、”upstream http”、”cookie_”、
        ”arg”开头的五类变量。这些变量庞大并且不可预知，不可能提前定义并收集到cmcf->variables_keys内，比如以”arg”开头代表的参数类变量会根
        据客户端请求URI时附带的参数不同而不同，一个类似于“http:// 192.168. 164.2/?pageid=2”这样的请求会自动生成变量$argpageid。因此还需判断
        用户在配置文件里使用的变量是否在这五类变量里，具体来说就是检测用户使用的变量名前面几个字符是否与它们一致（这也间接说明，用户自定义变量时
        最好不要以这些字符开头）。当然，如果用户在配置文件里使用了变量$argpageid，而客户端请求时却并没有带上pageid参数，此时变量$arg_pageid值为空，
        但它总还算是合法的，但如果提示类似如下这样的错误，请需检查配置文件内变量名是否书写正确。(unknown "XXX" variable)
          */ //在variables中，但是不在variables_keys中的变量如果不是以下面的字符串开头，则是非法的

        //如果set 设置的变量名不是以以下字符串开头，则在ngx_http_rewrite_set的时候其实已经设置好了get_hendler和data
        
        if (ngx_strncmp(v[i].name.data, "http_", 5) == 0) {
            v[i].get_handler = ngx_http_variable_unknown_header_in;
            v[i].data = (uintptr_t) &v[i].name;

            continue;
        }

        if (ngx_strncmp(v[i].name.data, "sent_http_", 10) == 0) {
            v[i].get_handler = ngx_http_variable_unknown_header_out;
            v[i].data = (uintptr_t) &v[i].name;

            continue;
        }

        if (ngx_strncmp(v[i].name.data, "upstream_http_", 14) == 0) {
            v[i].get_handler = ngx_http_upstream_header_variable;
            v[i].data = (uintptr_t) &v[i].name;
            v[i].flags = NGX_HTTP_VAR_NOCACHEABLE;

            continue;
        }

        if (ngx_strncmp(v[i].name.data, "cookie_", 7) == 0) {
            v[i].get_handler = ngx_http_variable_cookie;
            v[i].data = (uintptr_t) &v[i].name;

            continue;
        }

        if (ngx_strncmp(v[i].name.data, "upstream_cookie_", 16) == 0) {
            v[i].get_handler = ngx_http_upstream_cookie_variable;
            v[i].data = (uintptr_t) &v[i].name;
            v[i].flags = NGX_HTTP_VAR_NOCACHEABLE;

            continue;
        }

        if (ngx_strncmp(v[i].name.data, "arg_", 4) == 0) {
            v[i].get_handler = ngx_http_variable_argument;
            v[i].data = (uintptr_t) &v[i].name;
            v[i].flags = NGX_HTTP_VAR_NOCACHEABLE;

            continue;
        }

        ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                      "unknown \"%V\" variable", &v[i].name);

        return NGX_ERROR;

    next:
        continue;
    }


    for (n = 0; n < cmcf->variables_keys->keys.nelts; n++) {
        av = key[n].value;

        if (av->flags & NGX_HTTP_VAR_NOHASH) {
            key[n].key.data = NULL;
        }
    }

    hash.hash = &cmcf->variables_hash;
    hash.key = ngx_hash_key;
    hash.max_size = cmcf->variables_hash_max_size;
    hash.bucket_size = cmcf->variables_hash_bucket_size;
    hash.name = "variables_hash";
    hash.pool = cf->pool;
    hash.temp_pool = NULL;

    if (ngx_hash_init(&hash, cmcf->variables_keys->keys.elts,
                      cmcf->variables_keys->keys.nelts)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    cmcf->variables_keys = NULL;

    return NGX_OK;
}

