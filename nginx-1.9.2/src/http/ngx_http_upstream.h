
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_UPSTREAM_H_INCLUDED_
#define _NGX_HTTP_UPSTREAM_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_event_connect.h>
#include <ngx_event_pipe.h>
#include <ngx_http.h>


#define NGX_HTTP_UPSTREAM_FT_ERROR           0x00000002
#define NGX_HTTP_UPSTREAM_FT_TIMEOUT         0x00000004
#define NGX_HTTP_UPSTREAM_FT_INVALID_HEADER  0x00000008
#define NGX_HTTP_UPSTREAM_FT_HTTP_500        0x00000010
#define NGX_HTTP_UPSTREAM_FT_HTTP_502        0x00000020
#define NGX_HTTP_UPSTREAM_FT_HTTP_503        0x00000040
#define NGX_HTTP_UPSTREAM_FT_HTTP_504        0x00000080
#define NGX_HTTP_UPSTREAM_FT_HTTP_403        0x00000100
#define NGX_HTTP_UPSTREAM_FT_HTTP_404        0x00000200
#define NGX_HTTP_UPSTREAM_FT_UPDATING        0x00000400
#define NGX_HTTP_UPSTREAM_FT_BUSY_LOCK       0x00000800
#define NGX_HTTP_UPSTREAM_FT_MAX_WAITING     0x00001000
#define NGX_HTTP_UPSTREAM_FT_NOLIVE          0x40000000
#define NGX_HTTP_UPSTREAM_FT_OFF             0x80000000

#define NGX_HTTP_UPSTREAM_FT_STATUS          (NGX_HTTP_UPSTREAM_FT_HTTP_500  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_502  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_503  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_504  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_403  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_404)

#define NGX_HTTP_UPSTREAM_INVALID_HEADER     40


#define NGX_HTTP_UPSTREAM_IGN_XA_REDIRECT    0x00000002
#define NGX_HTTP_UPSTREAM_IGN_XA_EXPIRES     0x00000004
#define NGX_HTTP_UPSTREAM_IGN_EXPIRES        0x00000008
#define NGX_HTTP_UPSTREAM_IGN_CACHE_CONTROL  0x00000010
#define NGX_HTTP_UPSTREAM_IGN_SET_COOKIE     0x00000020
#define NGX_HTTP_UPSTREAM_IGN_XA_LIMIT_RATE  0x00000040
#define NGX_HTTP_UPSTREAM_IGN_XA_BUFFERING   0x00000080
#define NGX_HTTP_UPSTREAM_IGN_XA_CHARSET     0x00000100
#define NGX_HTTP_UPSTREAM_IGN_VARY           0x00000200


typedef struct {
    ngx_msec_t                       bl_time;
    ngx_uint_t                       bl_state;

    // HTTP/1.1 200 OK 对应中的200
    ngx_uint_t                       status; //在mytest_process_status_line赋值，源头在ngx_http_parse_status_line // HTTP/1.1 200 OK 对应中的200
    ngx_msec_t                       response_time;
    ngx_msec_t                       connect_time;
    ngx_msec_t                       header_time;
    off_t                            response_length;

    ngx_str_t                       *peer;
} ngx_http_upstream_state_t;


typedef struct {
    ngx_hash_t                       headers_in_hash; //在ngx_http_upstream_init_main_conf中对ngx_http_upstream_headers_in成员进行hash得到
    ngx_array_t                      upstreams; /* ngx_http_upstream_srv_conf_t */
} ngx_http_upstream_main_conf_t;

typedef struct ngx_http_upstream_srv_conf_s  ngx_http_upstream_srv_conf_t;

typedef ngx_int_t (*ngx_http_upstream_init_pt)(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *us);
typedef ngx_int_t (*ngx_http_upstream_init_peer_pt)(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);


typedef struct {
    ngx_http_upstream_init_pt        init_upstream;
    ngx_http_upstream_init_peer_pt   init;
    void                            *data;
} ngx_http_upstream_peer_t;

//server backend1.example.com weight=5;
/*
·weight = NUMBER - 设置服务器权重，默认为1。
·max_fails = NUMBER - 在一定时间内（这个时间在fail_timeout参数中设置）检查这个服务器是否可用时产生的最多失败请求数，默认为1，将其设置为0可以关闭检查，这些错误在proxy_next_upstream或fastcgi_next_upstream（404错误不会使max_fails增加）中定义。
·fail_timeout = TIME - 在这个时间内产生了max_fails所设置大小的失败尝试连接请求后这个服务器可能不可用，同样它指定了服务器不可用的时间（在下一次尝试连接请求发起之前），默认为10秒，fail_timeout与前端响应时间没有直接关系，不过可以使用proxy_connect_timeout和proxy_read_timeout来控制。
·down - 标记服务器处于离线状态，通常和ip_hash一起使用。
·backup - (0.6.7或更高)如果所有的非备份服务器都宕机或繁忙，则使用本服务器（无法和ip_hash指令搭配使用）。
*/
typedef struct { //ngx_http_upstream_srv_conf_s->servers[]中的成员   创建空间和赋值见ngx_http_upstream_server
    ngx_str_t                        name; ////server 127.0.0.1:8080 max_fails=3  fail_timeout=30s;中的uri为/
    ngx_addr_t                      *addrs; //server   127.0.0.1:8080 max_fails=3  fail_timeout=30s;中的127.0.0.1
    ngx_uint_t                       naddrs;
    ngx_uint_t                       weight;
    ngx_uint_t                       max_fails;
    time_t                           fail_timeout;

    unsigned                         down:1;
    unsigned                         backup:1;
} ngx_http_upstream_server_t;


#define NGX_HTTP_UPSTREAM_CREATE        0x0001
#define NGX_HTTP_UPSTREAM_WEIGHT        0x0002
#define NGX_HTTP_UPSTREAM_MAX_FAILS     0x0004
#define NGX_HTTP_UPSTREAM_FAIL_TIMEOUT  0x0008
#define NGX_HTTP_UPSTREAM_DOWN          0x0010
#define NGX_HTTP_UPSTREAM_BACKUP        0x0020

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
*/ //创建空间和部分赋值见ngx_http_upstream_add  server backend1.example.com weight=5;或者xxx_pass(proxy_pass 或者 fastcgi_pass等)都会创建该结构，表示上游服务器地址信息等
struct ngx_http_upstream_srv_conf_s { //upstream {}模块配置信息,该配置相当于server{}一个级别
//一个upstream{}配置结构的数据,这个是umcf(ngx_http_upstream_main_conf_t)->upstreams里面的数组项。umcf是upstream模块的顶层配置了。
    ngx_http_upstream_peer_t         peer;
    void                           **srv_conf; //赋值见ngx_http_upstream，表示upstream{}所处的二级 srv{}级别位置
    //记录本upstream{}块的所有server指令。 server backend1.example.com weight=5;
    ngx_array_t                     *servers;  /* ngx_http_upstream_server_t */ //ngx_http_upstream或者ngx_http_upstream_add中创建空间

    ngx_uint_t                       flags; //NGX_HTTP_UPSTREAM_CREATE | NGX_HTTP_UPSTREAM_MAX_FAILS等
    ngx_str_t                        host; //upstream xxx {}中的xxx
    u_char                          *file_name; //配置文件名称
    ngx_uint_t                       line; //配置文件中的行号
    in_port_t                        port;
    in_port_t                        default_port;
    ngx_uint_t                       no_port;  /* unsigned no_port:1 */

#if (NGX_HTTP_UPSTREAM_ZONE)
    ngx_shm_zone_t                  *shm_zone;
#endif
};


typedef struct {
    ngx_addr_t                      *addr;
    ngx_http_complex_value_t        *value;
} ngx_http_upstream_local_t;

/*
事实上，HTTP反向代理模块在nginx.conf文件中提供的配置项大都是用来设置ngx_http_upstream_conf_t结构体中的成员的。
*/ //在解析到upstream{}配置的时候，创建该结构，和location{}类似
typedef struct { //upstream配置包括proxy fastcgi wcgi等都用该结构
//当在ngx_http_upstream_t结构体中没有实现resolved成员时，upstream这个结构体才会生效，它会定义上游服务器的配置
    ngx_http_upstream_srv_conf_t    *upstream; 

    ngx_msec_t                       connect_timeout;//建立TCP连接的超时时间，实际上就是写事件添加到定时器中时设显的超时时间
    ngx_msec_t                       send_timeout;//发送请求的超时时间。通常就是写事件添加到定时器中设置的超时时间
    ngx_msec_t                       read_timeout;//接收响应的超时时间。通常就是读事件添加到定时器中设置的超时时间
    ngx_msec_t                       timeout;
    ngx_msec_t                       next_upstream_timeout;

    size_t                           send_lowat; //TCP的SO_SNDLOWAT选项，表示发送缓冲区的下限
//定义了接收头部的缓冲区分配的内存大小（ngx_http_upstream_t中的buffer缓冲区），当不转发响应给下游或者在buffering标志位为0
//的情况下转发响应时，它同样表示接收包体的缓冲区大小
    size_t                           buffer_size;
    size_t                           limit_rate;
//仅当buffering标志位为1，并且向下游转发响应时生效。它会设置到ngx_event_pipe_t结构体的busy_size成员中
    size_t                           busy_buffers_size;
/*
在buffering标志位为1时，如果上游速度快于下游速度，将有可能把来自上游的响应存储到临时文件中，而max_temp_file_size指定了临时文件的
最大长度。实际上，它将限制ngx_event_pipe_t结构体中的temp_file
*/
    size_t                           max_temp_file_size;
    size_t                           temp_file_write_size; //表示将缓冲区中的响应写入临时文件时一次写入字符流的最大长度

    size_t                           busy_buffers_size_conf;
    size_t                           max_temp_file_size_conf;
    size_t                           temp_file_write_size_conf;

    ngx_bufs_t                       bufs;//以缓存响应的方式转发上游服务器的包体时所使用的内存大小
/*
针对ngx_http_upstream_t结构体中保存解析完的包头的headers in成员，ignore_headers可以按照二进制位使得upstream在转发包头时跳过对某些头部
的处理。作为32位整型，理论上ignore_headers最多可以表示32个需要跳过不予处理的头部，然而目前upstream机制仅提供8个位用于忽略8个HTTP头部的处
理，包括：
#define NGX_HTTP_UPSTREAM_IGN_XA_REDIRECT    0x00000002
#define NGX_HTTP_UPSTREAM_IGN_XA_EXPIRES     0x00000004
#define NGX_HTTP_UPSTREAM_IGN_EXPIRES        0x00000008
#define NGX_HTTP_UPSTREAM_IGN_CACHE_CONTROL  0x00000010
#define NGX_HTTP_UPSTREAM_IGN_SET_COOKIE     0x00000020
#define NGX_HTTP_UPSTREAM_IGN_XA_LIMIT_RATE  0x00000040
#define NGX_HTTP_UPSTREAM_IGN_XA_BUFFERING   0x00000080
#define NGX_HTTP_UPSTREAM_IGN_XA_CHARSET     0x00000100
#define NGX_HTTP_UPSTREAM_IGN_VARY           0x00000200
*/
    ngx_uint_t                       ignore_headers;
/*
以二进制位来表示一些错误码，如果处理上游响应时发现这些错误码，那么在没有将响应转发给下游客户端时，将会选择下
一个上游服务器来重发请求。参见ngx_ht tp_upstream_next方法
*/
    ngx_uint_t                       next_upstream;
/*
在buffering标志为1的情况下转发响应时，将有可能把响应存放到临时文件中。在ngx_http_upstream_t中的store标志位为1时，
store_access表示所创建的目录、文件的权限
*/
    ngx_uint_t                       store_access;
    ngx_uint_t                       next_upstream_tries;
/*
决定转发响应方式的标志位，buffering为1时表示打开缓存，这时认为上游的网速快于下游的网速，会尽量地在内存或者磁盘中缓存来自上游的
响应；如果buffering为0，仅会开辟一块固定大小的内存块作为缓存来转发响应
*/
    ngx_flag_t                       buffering; //见xxx_buffering如fastcgi_buffering  是否换成后端服务器应答回来的包体
    ngx_flag_t                       request_buffering;//是否换成客户端请求的包体 XXX_request_buffering (例如proxy_request_buffering fastcgi_request_buffering
    ngx_flag_t                       pass_request_headers;////是否转发客户端浏览器过来的请求头部到后端去
    ngx_flag_t                       pass_request_body; ////是否转发客户端浏览器过来的包体到后端去

/*
表示标志位。当它为1时，表示与上游服务器交互时将不检查Nginx与下游客户端间的连接是否断开。
也就是说，即使下游客户端主动关闭了连接，也不会中断与上游服务器间的交互，见ngx_http_upstream_init_request
*/
    ngx_flag_t                       ignore_client_abort;
/*
当解析上游响应的包头时，如果解析后设置到headers_in结构体中的status_n错误码大于400，则会试图把它与error_page中指定的错误码相匹配，
如果匹配上，则发送error_page中指定的响应，否则继续返回上游服务器的错误码。详见ngx_http_upstream_intercept_errors方法
*/
    ngx_flag_t                       intercept_errors;
/*
buffering标志位为1的情况下转发响应时才有意义。这时，如果cyclic_temp_file为l，则会试图复用临时文件中已经使用过的空间。不建议
将cyclic_temp_f ile设为1
*/
    ngx_flag_t                       cyclic_temp_file;
    ngx_flag_t                       force_ranges;

    ngx_path_t                      *temp_path; //在buff ering标志位为1的情况下转发响应时，存放临时文件的路径
/*
不转发的头部。实际上是通过ngx_http_upstream_hide_headers_hash方法，根据hide_headers和pass_headers动态数组构造出的需要隐藏的HTTP头部散列表
*/
    ngx_hash_t                       hide_headers_hash;

/*
hide_headers的类型是ngx_array_t动态数组（实际上，upstream模块将会通过hide_headers来构造hide_headers_hash散列表）。
由于upstream模块要求hide_headers不可以为NULL，所以必须要初始化hide_headers成员。upstream模块提供了
ngx_http_upstream_hide_headers hash方法来初始化hide_headers，但仅可用在合并配置项方法内。
*/
//当转发上游响应头部（ngx_http_upstream_t中headers_in结构体中的头部）给下游客户端时如果不希望某些头部转发给下游，就设置到hide_headers动态数组中
    ngx_array_t                     *hide_headers;
/*
当转发上游响应头部（ngx_http_upstream_t中headers_in结构体中的头部）给下游客户端时，upstream机制默认不会转发如“Date”、“Server”之
类的头部，如果确实希望直接转发它们到下游，就设置到pass_headers动态数组中
*/
    ngx_array_t                     *pass_headers;

    ngx_http_upstream_local_t       *local;//连接上游服务器时便用的本机地址

#if (NGX_HTTP_CACHE)
    ngx_shm_zone_t                  *cache_zone;
    ngx_http_complex_value_t        *cache_value;

    ngx_uint_t                       cache_min_uses;
    ngx_uint_t                       cache_use_stale;
    ngx_uint_t                       cache_methods;

    ngx_flag_t                       cache_lock;
    ngx_msec_t                       cache_lock_timeout;
    ngx_msec_t                       cache_lock_age;

    ngx_flag_t                       cache_revalidate;

    ngx_array_t                     *cache_valid;
    ngx_array_t                     *cache_bypass;
    ngx_array_t                     *no_cache;
#endif

/*
当ngx_http_upstream_t中的store标志位为1时，如果需要将上游的响应存放到文件中，store_lengths将表示存放路径的长度，而store_values表示存放路径
*/
    ngx_array_t                     *store_lengths;
    ngx_array_t                     *store_values;

#if (NGX_HTTP_CACHE)
    signed                           cache:2;
#endif
//xxx_store(例如scgi_store)  on | off |path   只要不是off,store都为1，赋值见ngx_http_fastcgi_store
//制定了存储前端文件的路径，参数on指定了将使用root和alias指令相同的路径，off禁止存储，此外，参数中可以使用变量使路径名更明确：fastcgi_store /data/www$original_uri;
    signed                           store:2;//到目前为止，store标志位的意义与ngx_http_upstream_t中的store相同，仍只有o和1被使用到
/*
上面的intercept_errors标志位定义了400以上的错误码将会与error_page比较后再行处理，实际上这个规则是可以有一个例外情况的，如果将intercept_404
标志位设为1，当上游返回404时会直接转发这个错误码给下游，而不会去与error_page进行比较
*/
    unsigned                         intercept_404:1;
/*
当该标志位为1时，将会根据ngx_http_upstream_t中headers_in结构体里的"X-Accel-Buffering"头部（它的值会是yes和no）来改变buffering
标志位，当其值为yes时，buffering标志位为1。因此，change_buffering为1时将有可能根据上游服务器返回的响应头部，动态地决定是以上
游网速优先还是以下游网速优先
*/
    unsigned                         change_buffering:1;

#if (NGX_HTTP_SSL)
    ngx_ssl_t                       *ssl;
    ngx_flag_t                       ssl_session_reuse;

    ngx_http_complex_value_t        *ssl_name;
    ngx_flag_t                       ssl_server_name;
    ngx_flag_t                       ssl_verify;
#endif

    ngx_str_t                        module; //使用upstream的模块名称，仅用于记录日志
} ngx_http_upstream_conf_t;

//ngx_http_upstream_headers_in中的各个成员
typedef struct {
    ngx_str_t                        name;
    ngx_http_header_handler_pt       handler; //在mytest_upstream_process_header中执行
    ngx_uint_t                       offset;
    ngx_http_header_handler_pt       copy_handler; //ngx_http_upstream_process_headers中执行
    ngx_uint_t                       conf;
    ngx_uint_t                       redirect;  /* unsigned   redirect:1; */
} ngx_http_upstream_header_t;


//参考mytest_upstream_process_header->ngx_http_parse_header_line
//ngx_http_upstream_headers_in
typedef struct {
    ngx_list_t                       headers;

    //在mytest_process_status_line赋值，源头在ngx_http_parse_status_line // HTTP/1.1 200 OK 对应中的200
    ngx_uint_t                       status_n;// HTTP/1.1 200 OK 对应中的200

    //里面内容为HTTP/1.1 200 OK 中的"200 OK "
    ngx_str_t                        status_line; //在mytest_process_status_line赋值，源头在ngx_http_parse_status_line // HTTP/1.1 200 OK 对应中的200

    ngx_table_elt_t                 *status;//在mytest_process_status_line赋值，源头在ngx_http_parse_status_line
    ngx_table_elt_t                 *date;
    ngx_table_elt_t                 *server;
    ngx_table_elt_t                 *connection;

    ngx_table_elt_t                 *expires;
    ngx_table_elt_t                 *etag;
    ngx_table_elt_t                 *x_accel_expires;
    ngx_table_elt_t                 *x_accel_redirect;
    ngx_table_elt_t                 *x_accel_limit_rate;

    ngx_table_elt_t                 *content_type;
    ngx_table_elt_t                 *content_length;

    ngx_table_elt_t                 *last_modified;
    ngx_table_elt_t                 *location;
    ngx_table_elt_t                 *accept_ranges;
    ngx_table_elt_t                 *www_authenticate;
    ngx_table_elt_t                 *transfer_encoding;
    ngx_table_elt_t                 *vary;

#if (NGX_HTTP_GZIP)
    ngx_table_elt_t                 *content_encoding;
#endif

    ngx_array_t                      cache_control;
    ngx_array_t                      cookies;

    off_t                            content_length_n;
    time_t                           last_modified_time;

    unsigned                         connection_close:1;
    unsigned                         chunked:1;
} ngx_http_upstream_headers_in_t;

//resolved结构体，用来保存上游服务器的地址
typedef struct { //创建空间和赋值见ngx_http_fastcgi_eval
    ngx_str_t                        host; //sockaddr对应的地址字符串,如a.b.c.d
    in_port_t                        port; //端口
    ngx_uint_t                       no_port; /* unsigned no_port:1 */

    ngx_uint_t                       naddrs; //地址个数，
    ngx_addr_t                      *addrs; 

    struct sockaddr                 *sockaddr; //上游服务器的地址
    socklen_t                        socklen; //sizeof(struct sockaddr_in);

    ngx_resolver_ctx_t              *ctx;
} ngx_http_upstream_resolved_t;


typedef void (*ngx_http_upstream_handler_pt)(ngx_http_request_t *r,
    ngx_http_upstream_t *u);

/*
upstream有3种处理上游响应包体的方式，但HTTP模块如何告诉
upstream使用哪一种方式处理上游的响应包体呢？当请求的ngx_http_request_t结构体中
subrequest_in_memory标志位为1时，将采用第1种方式，即upstream不转发响应包体
到下游，由HTTP模块实现的input_filter方法处理包体；当subrequest_in_memory为0时，
upstream会转发响应包体。当ngx_http_upstream_conf t配置结构体中的buffering标志位为1
畸，将开启更多的内存和磁盘文件用于缓存上游的响应包体，这意味上游网速更快；当buffering
为0时，将使用固定大小的缓冲区（就是上面介绍的buffer缓冲区）来转发响应包体。
    注意  上述的8个回调方法中，只有create_request、process_header、finalize_request
是必须实现的，其余5个回调方法-input_filter init、input_filter、reinit_request、abort
request、rewrite redirect是可选的。第12章会详细介绍如何使用这5个可选的回调方法。另
外，这8个方法的回调场景见5.2节。
*/

/*
ngx_http_upstream_create方法创建ngx_http_upstream_t结构体，其中的成员还需要各个HTTP模块自行设置。
启动upstream机制使用ngx_http_upstream_init方法
*/ //FastCGI memcached  uwsgi  scgi proxy模块的相关配置都放在该结构中
//ngx_http_request_t->upstream 中存取
struct ngx_http_upstream_s { //该结构中的部分成员是从upstream{}中的相关配置里面(ngx_http_upstream_conf_t)获取的
    //处理读事件的回调方法，每一个阶段都有不同的read event handler
    ngx_http_upstream_handler_pt     read_event_handler;
    //处理写事件的回调方法，每一个阶段都有不同的write event handler
    ngx_http_upstream_handler_pt     write_event_handler;

    //表示主动向上游服务器发起的连接。 
    ngx_peer_connection_t            peer;

    /*
     当向下游客户端转发响应时（ngx_http_request_t结构体中的subrequest_in_memory标志住为0），如果打开了缓存且认为上游网速更快（conf
     配置中的buffering标志位为1），这时会使用pipe成员来转发响应。在使用这种方式转发响应时，必须由HTTP模块在使用upstream机制前构造
     pipe结构体，否则会出现严重的coredump错误
     */
    ngx_event_pipe_t                *pipe;

    /* request_bufs决定发送什么样的请求给上游服务器，在实现create_request方法时需要设置它 
    request_bufs以链表的方式把ngx_buf_t缓冲区链接起来，它表示所有需要发送到上游服务器的请求内容。
    所以，HTTP模块实现的create_request回调方法就在于构造reque st_bufg链表
    */
    ngx_chain_t                     *request_bufs; //发往上游服务器的请求头内容放入该buf

    //定义了向下游发送响应的方式
    ngx_output_chain_ctx_t           output; //输出数据的结构，里面存有要发送的数据，以及发送的output_filter指针
    ngx_chain_writer_ctx_t           writer; //参考ngx_chain_writer，里面会将输出buf一个个连接到这里。
    //调用ngx_output_chain后，要发送的数据都会放在这里，然后发送，然后更新这个链表，指向剩下的还没有调用writev发送的。

    //upstream访问时的所有限制性参数，
    /*
    conf成员，它用于设置upstream模块处理请求时的参数，包括连接、发送、接收的超时时间等。
    事实上，HTTP反向代理模块在nginx.conf文件中提供的配置项大都是用来设置ngx_http_upstream_conf_t结构体中的成员的。
    上面列出的3个超时时间(connect_timeout  send_imeout read_timeout)是必须要设置的，因为它们默认为0，如果不设置将永远无法与上游服务器建立起TCP连接（因为connect timeout值为0）。
    */
    ngx_http_upstream_conf_t        *conf; //使用upstream机制时的各种配置  例如fastcgi赋值在ngx_http_fastcgi_handler
#if (NGX_HTTP_CACHE)
    ngx_array_t                     *caches;
#endif

    /*
     HTTP模块在实现process_header方法时，如果希望upstream直接转发响应，就需要把解析出的响应头部适配为HTTP的响应头部，同时需要
     把包头中的信息设置到headers_in结构体中，这样，会把headers_in中设置的头部添加到要发送到下游客户端的响应头部headers_out中
     */
    ngx_http_upstream_headers_in_t   headers_in;  //存放从上游返回的头部信息，

    //通过resolved可以直接指定上游服务器地址．用于解析主机域名  创建和赋值见ngx_http_xxx_eval(例如ngx_http_fastcgi_eval ngx_http_proxy_eval)
    ngx_http_upstream_resolved_t    *resolved; //解析出来的fastcgi_pass   127.0.0.1:9000;后面的字符串内容，可能有变量嘛。

    ngx_buf_t                        from_client;

    /*
    buffer成员存储接收自上游服务器发来的响应内容，由于它会被复用，所以具有下列多种意义：
    a）在使用process_header方法解析上游响应的包头时，buffer中将会保存完整的响应包头：
    b）当下面的buffering成员为1，而且此时upstream是向下游转发上游的包体时，buffer没有意义；
    c）当buffering标志住为0时，buffer缓冲区会被用于反复地接收上游的包体，进而向下游转发；
    d）当upstream并不用于转发上游包体时，buffer会被用于反复接收上游的包体，HTTP模块实现的input_filter方法需要关注它

    接收上游服务器响应包头的缓冲区，在不需要把响应直接转发给客户端，或者buffering标志位为0的情况下转发包体时，接收包体的缓冲
区仍然使用buffer。注意，如果没有自定义input_filter方法处理包体，将会使用buffer存储全部的包体，这时buf fer必须足够大！它的大小
由ngx_http_upstream_conf_t结构体中的buffer_size成员决定
    */
//读取上游返回的数据的缓冲区，也就是proxy，FCGI返回的数据。这里面有http头部，也可能有body部分。其body部分会跟event_pipe_t的preread_bufs结构对应起来。就是预读的buf，其实是i不小心读到的。
    ngx_buf_t                        buffer; //从上游服务器接收的内容在该buffer，发往上游的请求内容在request_bufs中
    //表示来自上游服务器的响应包体的长度
    off_t                            length; //要发送给客户端的数据大小，还需要读取这么多进来。 

    /*
out_bufs在两种场景下有不同的意义：
①当不需要转发包体，且使用默认的input_filter方法（也就是ngx_http_upstream_non_buffered_filter方法）处理包体时，out bufs将会指向响应包体，
事实上，out bufs链表中会产生多个ngx_buf_t缓冲区，每个缓冲区都指向buffer缓存中的一部分，而这里的一部分就是每次调用recv方法接收到的一段TCP流。
②当需要转发响应包体到下游时（buffering标志位为O，即以下游网速优先），这个链表指向上一次向下游转发响应到现在这段时间内接收自上游的缓存响应
     */
    ngx_chain_t                     *out_bufs;
    /*
    当需要转发响应包体到下游时（buffering标志位为o，即以下游网速优先），它表示上一次向下游转发响应时没有发送完的内容
     */
    ngx_chain_t                     *busy_bufs;//调用了ngx_http_output_filter，并将out_bufs的链表数据移动到这里，待发送完毕后，会移动到free_bufs
    /*
    这个链表将用于回收out_bufs中已经发送给下游的ngx_buf_t结构体，这同样应用在buffering标志位为0即以下游网速优先的场景
     */
    ngx_chain_t                     *free_bufs;//空闲的缓冲区。可以分配

/*
input_filter init与input_filter回调方法
    input_filter_init与input_filter这两个方法都用于处理上游的响应包体，因为处理包体
前HTTP模块可能需要做一些初始化工作。例如，分配一些内存用于存放解析的中间状态
等，这时upstream就提供了input_filter_init方法。而input_filter方法就是实际处理包体的
方法。这两个回调方法都可以选择不予实现，这是因为当这两个方法不实现时，upstream
模块会自动设置它们为预置方法（上文讲过，由于upstream有3种处理包体的方式，所以
upstream模块准备了3对input_filter_init、input_filter方法）。因此，一旦试图重定义mput_
filter init、input_filter方法，就意味着我们对upstream模块的默认实现是不满意的，所以才
要重定义该功能。
    在多数情况下，会在以下场景决定重新实现input_filter方法。
    (1)在转发上游响应到下游的同时，需要做一些特殊处理
    例如，ngx_http_memcached_ module模块会将实际由memcached实现的上游服务器返回
的响应包体，转发到下游的HTTP客户端上。在上述过程中，该模块通过重定义了的input_
filter方法来检测memcached协议下包体的结束，而不是完全、纯粹地透传TCP流。
    (2)当无须在上、下游间转发响应时，并不想等待接收完全部的上游响应后才开始处理
请求
    在不转发响应时，通常会将响应包体存放在内存中解析，如果试图接收到完整的响应后
再来解析，由于响应可能会非常大，这会占用大量内存。而重定义了input_filter方法后，可
以每解析完一部分包体，就释放一些内存。
    重定义input_filter方法必须符合一些规则，如怎样取到刚接收到的包体以及如何稃放缓
冲区使得固定大小的内存缓冲区可以重复使用等。注意，本章的例子并不涉及input_filter方
法，读者可以在第12章中找到input_filter方法的使用方式。
*/
//处理包体前的初始化方法，其中data参数用于传递用户数据结构，它实际上就是下面的input_filter_ctx指针
    ngx_int_t                      (*input_filter_init)(void *data); //ngx_http_XXX_input_filter_init(如ngx_http_fastcgi_input_filter_init)
/* 处理包体的方法，其中data参数用于传递用户数据结构，它实际上就是下面的input_filter_ctx指针，而bytes表示本次接收到的包体长度。
返回NGX ERROR时表示处理包体错误，请求需要结束，否则都将继续upstream流程

用来读取后端的数据，非buffering模式。ngx_http_upstream_non_buffered_filter，ngx_http_memcached_filter等。这个函数的调用时机: 
ngx_http_upstream_process_non_buffered_upstream等调用ngx_unix_recv接收到upstream返回的数据后就调用这里进行协议转换，不过目前转换不多。
*/
    ngx_int_t                      (*input_filter)(void *data, ssize_t bytes); //ngx_http_xxx_non_buffered_filter(如ngx_http_fastcgi_non_buffered_filter)
//用于传递HTTP模块自定义的数据结构，在input_filter_init和input_filter方法被回调时会作为参数传递过去
    void                            *input_filter_ctx;//指向所属的请求等上下文

#if (NGX_HTTP_CACHE)
    ngx_int_t                      (*create_key)(ngx_http_request_t *r);
#endif
    //构造发往上游服务器的请求内容
    /*
    create_request回调方法
    create_request的回调场景最简单，即它只可能被调用1次（如果不启用upstream的
失败重试机制的话。详见第12章），如图5-3所示。下面简单地介绍一下图5-3中的每一
个步骤：
    1)在Nginx主循环（这里的主循环是指8.5节提到的ngx_worker_process_cycle方法）中，会定期地调用事件模块，以检查是否有网络事件发生。
    2)事件模块在接收到HTTP请求后会调用HTIP框架来处理。假设接收、解析完HTTP头部后发现应该由mytest模块处理，这时会调用mytest模
    块的ngx_http_mytest_handler来处理。
    3)这里mytest模块此时会完成5.1.2节～5.1.4节中所列出的步骤。
    4)调用ngx_http_up stream_init方法启动upstream。
    5) upstream模块会去检查文件缓存，如果缓存中已经有合适的响应包，则会直接返回缓存（当然必须是在使用反向代理文件缓存的前提下）。
    为了让读者方便地理解upstream机制，本章将不再提及文件缓存。
    6)回调mytest模块已经实现的create_request回调方法。
    7) mytest模块通过设置r->upstream->request_bufs已经决定好发送什么样的请求到上游服务器。
    8) upstream模块将会检查5.1.3节中介绍过的resolved成员，如果有resolved成员的话，就根据它设置好上游服务器的地址r->upstream->peer成员。
    9)用无阻塞的TCP套接字建立连接。
    10)无论连接是否建立成功，负责建立连接的connect方法都会立刻返回。
    II) ngx_http_upstreamL init返回。
    12) mytest模块的ngx_http_mytest_handler方法返回NGX DONE。
    13)当事件模块处理完这批网络事件后，将控制权交还给Nginx主循环。
    */ //这里定义的mytest_upstream_create_request方法用于创建发送给上游服务器的HTTP请求，upstream模块将会回调它 
    //在ngx_http_upstream_init_request中执行  HTTP模块实现的执行create_request方法用于构造发往上游服务器的请求
    //ngx_http_xxx_create_request(例如ngx_http_fastcgi_create_request)
    ngx_int_t                      (*create_request)(ngx_http_request_t *r);//生成发送到上游服务器的请求缓冲（或者一条缓冲链）

/*
reinit_request可能会被多次回调。它被调用的原因只有一个，就是在第一次试图向上游服务器建立连接时，如果连接由于各种异常原因失败，
那么会根据upstream中conf参数的策略要求再次重连上游服务器，而这时就会调用reinit_request方法了。图5-4描述了典型的reinit_request调用场景。
下面简单地介绍一下图5-4中列出的步骤。
    1) Nginx主循环中会定期地调用事件模块，检查是否有网络事件发生。
    2)事件模块在确定与上游服务器的TCP连接建立成功后，会回调upstream模块的相关方法处理。
    3) upstream棋块这时会把r->upstream->request_sent标志位置为l，表示连接已经建立成功了，现在开始向上游服务器发送请求内容。
    4)发送请求到上游服务器。
    5)发送方法当然是无阻塞的（使用了无阻塞的套接字），会立刻返回。
    6) upstream模块处理第2步中的TCP连接建立成功事件。
    7)事件模块处理完本轮网络事件后，将控制权交还给Nginx主循环。
    8) Nginx主循环重复第1步，调用事件模块检查网络事件。
    9)这时，如果发现与上游服务器建立的TCP连接已经异常断开，那么事件模块会通知upstream模块处理它。
    10)在符合重试次数的前提下，upstream模块会毫不犹豫地再次用无阻塞的套接字试图建立连接。
    11)无论连接是否建立成功都立刻返回。
    12)这时检查r->upstream->request_sent标志位，会发现它已经被置为1了。
    13)如果mytest模块没有实现reinit_request方法，那么是不会调用它的。而如果reinit_request不为NULL空指针，就会回调它。
    14) mytest模块在reinit_request中处理完自己的事情。
    15)处理宪第9步中的TCP连接断开事件，将控制权交还给事件模块。
    16)事件模块处理完本轮网络事件后，交还控制权给Nginx主循环。
*/ //与上游服务器的通信失败后，如果按照重试规则还需要再次向上游服务器发起连接，则会调用reinit_request方法
    //下面的upstream回调指针是各个模块设置的，比如ngx_http_fastcgi_handler里面设置了fcgi的相关回调函数。
    //ngx_http_XXX_reinit_request(ngx_http_fastcgi_reinit_request)
    ngx_int_t                      (*reinit_request)(ngx_http_request_t *r);//在后端服务器被重置的情况下（在create_request被第二次调用之前）被调用

/*
收到上游服务器的响应后就会回调process_header方法。如果process_header返回NGXAGAIN，那么是在告诉upstream还没有收到完整的响应包头，
此时，对子本次upstream请求来说，再次接收到上游服务器发来的TCP流时，还会调用process_header方法处理，直到process_header函数返回
非NGXAGAIN值这一阶段才会停止

process_header回调方法process_header是用于解析上游服务器返回的基于TCP的响应头部的，因此，process_header可能会被多次调用，
它的调用次数与process_header的返回值有关。如图5-5所示，如果process_header返回NGX_AGAIN，这意味着还没有接收到完整的响应头部，
如果再次接收到上游服务器发来的TCP流，还会把它当做头部，仍然调用process_header处理。而在图5-6中，如果process_header返回NGX_OK
（或者其他非NGX_AGAIN的值），那么在这次连接的后续处理中将不会再次调用process_header。
 process header回调场景的序列图
下面简单地介绍一下图5-5中列出的步骤。
    1) Nginx主循环中会定期地调用事件模块，检查是否有网络事件发生。
    2)事件模块接收到上游服务器发来的响应时，会回调upstream模块处理。
    3) upstream模块这时可以从套接字缓冲区中读取到来自上游的TCP流。
    4)读取的响应会存放到r->upstream->buffer指向的内存中。注意：在未解析完响应头部前，若多次接收到字符流，所有接收自上游的
    响应都会完整地存放到r->upstream->buffer缓冲区中。因此，在解析上游响应包头时，如果buffer缓冲区全满却还没有解析到完整的响应
    头部（也就是说，process_header -直在返回NGX_AGAIN），那么请求就会出错。
    5)调用mytest模块实现的process_header方法。
    6) process_header方法实际上就是在解析r->upstream->buffer缓冲区，试图从中取到完整的响应头部（当然，如果上游服务器与Nginx通过HTTP通信，
    就是接收到完整的HTTP头部）。
    7)如果process_header返回NGX AGAIN，那么表示还没有解析到完整的响应头部，下次还会调用process_header处理接收到的上游响应。
    8)调用元阻塞的读取套接字接口。
    9)这时有可能返回套接字缓冲区已经为空。
    10)当第2步中的读取上游响应事件处理完毕后，控制权交还给事件模块。
    11)事件模块处理完本轮网络事件后，交还控制权给Nginx主循环。
*/ //ngx_http_upstream_process_header和ngx_http_upstream_cache_send函数中调用
/*
解析上游服务器返回响应的包头，返回NGX_AGAIN表示包头还没有接收完整，返回NGX_HTTP_UPSTREAM_INVALID_HEADER表示包头不合法，返回
NGX ERROR表示出现错误，返回NGX_OK表示解析到完整的包头
*/ //ngx_http_fastcgi_process_header(ngx_http_XXX_process_header)
    ngx_int_t                      (*process_header)(ngx_http_request_t *r); //处理上游服务器回复的第一个bit，时常是保存一个指向上游回复负载的指针
    void                           (*abort_request)(ngx_http_request_t *r);//在客户端放弃请求的时候被调用 ngx_http_XXX_abort_request
   
/*
当调用ngx_http_upstream_init启动upstream机制后，在各种原因（无论成功还是失败）导致该请求被销毁前都会调用finalize_request方
法（参见图5-1）。在finalize_request方法中可以不做任何事情，但必须实现finalize_request方法，否则Nginx会出现空指针调用的严重错误。

当请求结束时，将会回调finalize_request方法，如果我们希望此时释放资源，如打开
的句柄等，．那么可以把这样的代码添加到finalize_request方法中。本例中定义了mytest_
upstream_finalize_request方法，由于我们没有任何需要释放的资源，所以该方法没有完成任
何实际工作，只是因为upstream模块要求必须实现finalize_request回调方法
*/ //销毁upstream请求时调用  ngx_http_XXX_finalize_request
    void                           (*finalize_request)(ngx_http_request_t *r,
                                         ngx_int_t rc); //请求结束时会调用 //在Nginx完成从上游服务器读入回复以后被调用
/*
在重定向URL阶段，如果实现了rewrite_redirect回调方法，那么这时会调用rewrite_redirect。注意，本章不涉及rewrite_redirect方法，
感兴趣的读者可以查看upstream模块的ngx_http_upstream_rewrite_ location方法。如果upstream模块接收到完整的上游响应头部，
而且由HTTP模块的process_header回调方法将解析出的对应于Location的头部设置到了ngx_http_upstream_t中的headers in成员时，
ngx_http_up stre am_proces s_headers方法将会最终调用rewrite—redirect方法（见12.5.3节图12-5的第8步）。
因此，rewrite_ redirect的使用场景比较少，它主要应用于HTTP反向代理模蛱(ngx_http_proxy_module)。
*/
//在上游返回的响应出现Location或者Refresh头部表示重定向时，会通迂ngx_http_upstream_process_headers方法调用到可由HTTP模块实现的rewrite redirect方法
    ngx_int_t                      (*rewrite_redirect)(ngx_http_request_t *r,
                                         ngx_table_elt_t *h, size_t prefix);
    ngx_int_t                      (*rewrite_cookie)(ngx_http_request_t *r,
                                         ngx_table_elt_t *h);

    ngx_msec_t                       timeout;
    //用于表示上游响应的错误码、包体长度等信息
    ngx_http_upstream_state_t       *state; //从r->upstream_states分配获取，见ngx_http_upstream_init_request

    ngx_str_t                        method; //不使用文件缓存时没有意义 //GET,HEAD,POST
    //schema和uri成员仅在记录日志时会用到，除此以外没有意义
    ngx_str_t                        schema; //就是前面的http,https,mecached://  fastcgt://(ngx_http_fastcgi_handler)等。
    ngx_str_t                        uri;

#if (NGX_HTTP_SSL)
    ngx_str_t                        ssl_name;
#endif
    //目前它仅用于表示是否需要清理资源，相当于一个标志位，实际不会调用到它所指向的方法
    ngx_http_cleanup_pt             *cleanup;
    //是否指定文件缓存路径的标志位 
    //xxx_store(例如scgi_store)  on | off |path   只要不是off,store都为1，赋值见ngx_http_fastcgi_store
//制定了存储前端文件的路径，参数on指定了将使用root和alias指令相同的路径，off禁止存储，此外，参数中可以使用变量使路径名更明确：fastcgi_store /data/www$original_uri;
    unsigned                         store:1; //ngx_http_upstream_init_request赋值
    unsigned                         cacheable:1; //是否启用文件缓存，本章仅讨论cacheable标志住为o的场景
    unsigned                         accel:1;
    unsigned                         ssl:1; //是否基于SSL协议访问上游服务器
#if (NGX_HTTP_CACHE)
    unsigned                         cache_status:3;
#endif

    /*
    upstream有3种处理上游响应包体的方式，但HTTP模块如何告诉
    upstream使用哪一种方式处理上游的响应包体呢？当请求的ngx_http_request_t结构体中
    subrequest_in_memory标志位为1时，将采用第1种方式，即upstream不转发响应包体
    到下游，由HTTP模块实现的input_filter方法处理包体；当subrequest_in_memory为0时，
    upstream会转发响应包体。当ngx_http_upstream_conf t配置结构体中的buffering标志位为1
    畸，将开启更多的内存和磁盘文件用于缓存上游的响应包体，这意味上游网速更快；当buffering
    为0时，将使用固定大小的缓冲区（就是上面介绍的buffer缓冲区）来转发响应包体。
    
    在向客户端转发上游服务器的包体时才有用。当buffering为1时，表示使用多个缓冲区以及磁盘文
    件来转发上游的响应包体。当Nginx与上游间的网速远大于Nginx与下游客户端间的网速时，让Nginx开辟更多的
    内存甚至使用磁盘文件来缓存上游的响应包体，这是有意义的，它可以减轻上游服务器的并发压力。当buffering
    为0时，表示只使用上面的这一个buffer缓冲区来向下游转发响应包体
    */
    unsigned                         buffering:1; //向下游转发上游的响应包体时，是否开启更大的内存及临时磁盘文件用于缓存来不及发送到下游的响应
    unsigned                         keepalive:1;
    unsigned                         upgrade:1;

/*
request_sent表示是否已经向上游服务器发送了请求，当request_sent为1时，表示upstream机制已经向上游服务器发送了全部或者部分的请求。
事实上，这个标志位更多的是为了使用ngx_output_chain方法发送请求，因为该方法发送请求时会自动把未发送完的request_bufs链表记录下来，
为了防止反复发送重复请求，必须有request_sent标志位记录是否调用过ngx_output_chain方法
*/
    unsigned                         request_sent:1;
/*
将上游服务器的响应划分为包头和包尾，如果把响应直接转发给客户端，header_sent标志位表示包头是否发送，header_sent为1时表示已经把
包头转发给客户端了。如果不转发响应到客户端，则header_sent没有意义
*/
    unsigned                         header_sent:1;
};


typedef struct {
    ngx_uint_t                      status;
    ngx_uint_t                      mask;
} ngx_http_upstream_next_t;


typedef struct { //分配空间和赋值见ngx_http_upstream_param_set_slot， 存储在ngx_http_fastcgi_loc_conf_t->params_source中
    ngx_str_t   key; //fastcgi_param  SCRIPT_FILENAME  aaa中的SCRIPT_FILENAME
    ngx_str_t   value; //fastcgi_param  SCRIPT_FILENAME  aaa中的aaa

    //ngx_http_fastcgi_init_params
    ngx_uint_t  skip_empty; //fastcgi_param  SCRIPT_FILENAME  aaa  if_not_empty，则置1   以fastcgi为例该变量的起作用地方在ngx_http_fastcgi_create_request
} ngx_http_upstream_param_t;


ngx_int_t ngx_http_upstream_cookie_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
ngx_int_t ngx_http_upstream_header_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

ngx_int_t ngx_http_upstream_create(ngx_http_request_t *r);
void ngx_http_upstream_init(ngx_http_request_t *r);
ngx_http_upstream_srv_conf_t *ngx_http_upstream_add(ngx_conf_t *cf,
    ngx_url_t *u, ngx_uint_t flags);
char *ngx_http_upstream_bind_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_http_upstream_param_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
ngx_int_t ngx_http_upstream_hide_headers_hash(ngx_conf_t *cf,
    ngx_http_upstream_conf_t *conf, ngx_http_upstream_conf_t *prev,
    ngx_str_t *default_hide_headers, ngx_hash_init_t *hash);


#define ngx_http_conf_upstream_srv_conf(uscf, module)                         \
    uscf->srv_conf[module.ctx_index]


extern ngx_module_t        ngx_http_upstream_module;
extern ngx_conf_bitmask_t  ngx_http_upstream_cache_method_mask[];
extern ngx_conf_bitmask_t  ngx_http_upstream_ignore_headers_masks[];


#endif /* _NGX_HTTP_UPSTREAM_H_INCLUDED_ */
