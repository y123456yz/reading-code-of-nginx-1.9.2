
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
    ngx_array_t                      upstreams;
                                             /* ngx_http_upstream_srv_conf_t */
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


typedef struct {
    ngx_str_t                        name;
    ngx_addr_t                      *addrs;
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


struct ngx_http_upstream_srv_conf_s {
    ngx_http_upstream_peer_t         peer;
    void                           **srv_conf;

    ngx_array_t                     *servers;  /* ngx_http_upstream_server_t */

    ngx_uint_t                       flags;
    ngx_str_t                        host;
    u_char                          *file_name;
    ngx_uint_t                       line;
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
事实上，HTTP反向代理模块在nginx.conf文件中提供的配置项大都是用来设置ngx_http_upstream_conf_t结构体中的成员的。上面列出的3个超
时时间是必须要设置的，因为它们默认为0，如果不设置将永远无法与上游服务器建立起TCP连接（因为connect timeout值为0）。
*/
typedef struct {
    ngx_http_upstream_srv_conf_t    *upstream;

    ngx_msec_t                       connect_timeout;
    ngx_msec_t                       send_timeout;
    ngx_msec_t                       read_timeout;
    ngx_msec_t                       timeout;
    ngx_msec_t                       next_upstream_timeout;

    size_t                           send_lowat;
    size_t                           buffer_size;
    size_t                           limit_rate;

    size_t                           busy_buffers_size;
    size_t                           max_temp_file_size;
    size_t                           temp_file_write_size;

    size_t                           busy_buffers_size_conf;
    size_t                           max_temp_file_size_conf;
    size_t                           temp_file_write_size_conf;

    ngx_bufs_t                       bufs;

    ngx_uint_t                       ignore_headers;
    ngx_uint_t                       next_upstream;
    ngx_uint_t                       store_access;
    ngx_uint_t                       next_upstream_tries;
    ngx_flag_t                       buffering;
    ngx_flag_t                       request_buffering;//释放缓存HTTP包体
    ngx_flag_t                       pass_request_headers;
    ngx_flag_t                       pass_request_body;

    ngx_flag_t                       ignore_client_abort;
    ngx_flag_t                       intercept_errors;
    ngx_flag_t                       cyclic_temp_file;
    ngx_flag_t                       force_ranges;

    ngx_path_t                      *temp_path;

    ngx_hash_t                       hide_headers_hash;

/*
hide_headers的类型是ngx_array_t动态数组（实际上，upstream模块将会通过hide_headers来构造hide_headers_hash散列表）。
由于upstream模块要求hide_headers不可以为NULL，所以必须要初始化hide_headers成员。upstream模块提供了
ngx_http_upstream_hide_headers hash方法来初始化hide_headers，但仅可用在合并配置项方法内。
*/
    ngx_array_t                     *hide_headers;
    ngx_array_t                     *pass_headers;

    ngx_http_upstream_local_t       *local;

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

    ngx_array_t                     *store_lengths;
    ngx_array_t                     *store_values;

#if (NGX_HTTP_CACHE)
    signed                           cache:2;
#endif
    signed                           store:2;
    unsigned                         intercept_404:1;
    unsigned                         change_buffering:1;

#if (NGX_HTTP_SSL)
    ngx_ssl_t                       *ssl;
    ngx_flag_t                       ssl_session_reuse;

    ngx_http_complex_value_t        *ssl_name;
    ngx_flag_t                       ssl_server_name;
    ngx_flag_t                       ssl_verify;
#endif

    ngx_str_t                        module;
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
typedef struct {
    ngx_str_t                        host;
    in_port_t                        port;
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
struct ngx_http_upstream_s {
    ngx_http_upstream_handler_pt     read_event_handler;
    ngx_http_upstream_handler_pt     write_event_handler;

    ngx_peer_connection_t            peer;

    ngx_event_pipe_t                *pipe;

    /* request_bufs决定发送什么样的请求给上游服务器，在实现create_request方法时需要设置它 */
    ngx_chain_t                     *request_bufs; //发往上游服务器的请求头内容放入该buf

    ngx_output_chain_ctx_t           output;
    ngx_chain_writer_ctx_t           writer;

    //upstream访问时的所有限制性参数，在5.1.2节会详细讨论它
    /*
    conf成员，它用于设置upstream模块处理请求时的参数，包括连接、发送、接收的超时时间等。
    事实上，HTTP反向代理模块在nginx.conf文件中提供的配置项大都是用来设置ngx_http_upstream_conf_t结构体中的成员的。
    上面列出的3个超时时间(connect_timeout  send_imeout read_timeout)是必须要设置的，因为它们默认为0，如果不设置将永远无法与上游服务器建立起TCP连接（因为connect timeout值为0）。
    */
    ngx_http_upstream_conf_t        *conf;
#if (NGX_HTTP_CACHE)
    ngx_array_t                     *caches;
#endif

    ngx_http_upstream_headers_in_t   headers_in;

    //通过resolved可以直接指定上游服务器地址．在5.1.3节会详细讨论它
    ngx_http_upstream_resolved_t    *resolved;

    ngx_buf_t                        from_client;

    /*
    buffer成员存储接收自上游服务器发来的响应内容，由于它会被复用，所以具有下列多种意义：a）在
    使用process_header方法解析上游响应的包头时，buffer中将会保存完整的响应包头：b）当下面的buffering
    成员为1，而且此时upstream是向下游转发上游的包体时，buffer没有意义；c）当buffering标志住为0时，
    buffer缓冲区会被用于反复地接收上游的包体，进而向下游转发；d）当upstream并不用于转发上游包体时，
    buffer会被用于反复接收上游的包体，HTTP模块实现的input_filter方法需要关注它
    */
    ngx_buf_t                        buffer; //从上游服务器接收的内容在该buffer，发往上游的请求内容在request_bufs中
    off_t                            length;

    ngx_chain_t                     *out_bufs;
    ngx_chain_t                     *busy_bufs;
    ngx_chain_t                     *free_bufs;

/*
input_filter init与input_filter回调方法
    input_filter_init与input_filter这两个方法都用于处理上游的响应包体，因为处理包体
前HTTP模块可能需要做一些初始化工作。例如，分配一些内存用于存放解析的中间状态
等，这时upstream就提供了input_filter_init方法。而input_filter方法就是实际处理包体的
方法。这两个回调方法都可以选择不予实现，这是因为当这两个方法不实现时，upstream
模块会自动设置它们为预置方法（上文讲过，由于upstream有3种处理包体的方式，所以
upstream模块准备了3对input_filter_init、input_filter方法）。因此，一旦试图重定义mput_
filter init、input_filter方法，就意味着我们对upstream模块的默认实现是不满意的，所以才
要重定义该功能。此时，首先必须要弄清楚默认的input_filter方法到底做了什么，在12.6
节～12.8节介绍的3种处理包体方式中，都会涉及默认的input_filter方法所做的工作。
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
    ngx_int_t                      (*input_filter_init)(void *data);
    ngx_int_t                      (*input_filter)(void *data, ssize_t bytes);
    void                            *input_filter_ctx;

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
    //在ngx_http_upstream_init_request中执行
    ngx_int_t                      (*create_request)(ngx_http_request_t *r);

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
*/
    ngx_int_t                      (*reinit_request)(ngx_http_request_t *r);

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
    ngx_int_t                      (*process_header)(ngx_http_request_t *r); 
    void                           (*abort_request)(ngx_http_request_t *r);
   
/*
当调用ngx_http_upstream_init启动upstream机制后，在各种原因（无论成功还是失败）导致该请求被销毁前都会调用finalize_request方
法（参见图5-1）。在finalize_request方法中可以不做任何事情，但必须实现finalize_request方法，否则Nginx会出现空指针调用的严重错误。

当请求结束时，将会回调finalize_request方法，如果我们希望此时释放资源，如打开
的句柄等，．那么可以把这样的代码添加到finalize_request方法中。本例中定义了mytest_
upstream_finalize_request方法，由于我们没有任何需要释放的资源，所以该方法没有完成任
何实际工作，只是因为upstream模块要求必须实现finalize_request回调方法
*/ //销毁upstream请求时调用
    void                           (*finalize_request)(ngx_http_request_t *r,
                                         ngx_int_t rc);
/*
在重定向URL阶段，如果实现了rewrite_redirect回调方法，那么这时会调用rewrite_redirect。注意，本章不涉及rewrite_redirect方法，
感兴趣的读者可以查看upstream模块的ngx_http_upstream_rewrite_ location方法。如果upstream模块接收到完整的上游响应头部，
而且由HTTP模块的process_header回调方法将解析出的对应于Location的头部设置到了ngx_http_upstream_t中的headers in成员时，
ngx_http_up stre am_proces s_headers方法将会最终调用rewrite—redirect方法（见12.5.3节图12-5的第8步）。
因此，rewrite_ redirect的使用场景比较少，它主要应用于HTTP反向代理模蛱(ngx_http_proxy_module)。
*/
    ngx_int_t                      (*rewrite_redirect)(ngx_http_request_t *r,
                                         ngx_table_elt_t *h, size_t prefix);
    ngx_int_t                      (*rewrite_cookie)(ngx_http_request_t *r,
                                         ngx_table_elt_t *h);

    ngx_msec_t                       timeout;

    ngx_http_upstream_state_t       *state;

    ngx_str_t                        method;
    ngx_str_t                        schema;
    ngx_str_t                        uri;

#if (NGX_HTTP_SSL)
    ngx_str_t                        ssl_name;
#endif

    ngx_http_cleanup_pt             *cleanup;

    unsigned                         store:1;
    unsigned                         cacheable:1;
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
    unsigned                         buffering:1;
    unsigned                         keepalive:1;
    unsigned                         upgrade:1;

    unsigned                         request_sent:1;
    unsigned                         header_sent:1;
};


typedef struct {
    ngx_uint_t                      status;
    ngx_uint_t                      mask;
} ngx_http_upstream_next_t;


typedef struct {
    ngx_str_t   key;
    ngx_str_t   value;
    ngx_uint_t  skip_empty;
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
