
/*
 * Copyright (C) Nginx, Inc.
 * Copyright (C) Valentin V. Bartenev
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>
#include <ngx_http_v2_module.h>

//这里的ngx_http_v2_integer_octets ngx_http_v2_integer_octets index索引编码过程和ngx_http_v2_state_header_block中的解码过程对应
//ngx_http_v2_integer_octets ngx_http_v2_indexed进行整数编码，ngx_http_v2_literal_size进行字符串编码
#define ngx_http_v2_integer_octets(v)  (((v) + 127) / 128)
#define ngx_http_v2_literal_size(h)                                           \
    (ngx_http_v2_integer_octets(sizeof(h) - 1) + sizeof(h) - 1)

/* 128也就是位操作1000 0000,也就是该index在索引表中，如果i为1表示索引表的0，i=2对应索引表的1，i=3对应索引表的2，i=4对应索引表的3 */
#define ngx_http_v2_indexed(i)      (128 + (i))
#define ngx_http_v2_inc_indexed(i)  (64 + (i))

/* 和ngx_http_v2_static_table数组下表对应，相差1 */
#define NGX_HTTP_V2_STATUS_INDEX          8
#define NGX_HTTP_V2_STATUS_200_INDEX      8
#define NGX_HTTP_V2_STATUS_204_INDEX      9
#define NGX_HTTP_V2_STATUS_206_INDEX      10
#define NGX_HTTP_V2_STATUS_304_INDEX      11
#define NGX_HTTP_V2_STATUS_400_INDEX      12
#define NGX_HTTP_V2_STATUS_404_INDEX      13
#define NGX_HTTP_V2_STATUS_500_INDEX      14

#define NGX_HTTP_V2_CONTENT_LENGTH_INDEX  28
#define NGX_HTTP_V2_CONTENT_TYPE_INDEX    31
#define NGX_HTTP_V2_DATE_INDEX            33
#define NGX_HTTP_V2_LAST_MODIFIED_INDEX   44
#define NGX_HTTP_V2_LOCATION_INDEX        46
#define NGX_HTTP_V2_SERVER_INDEX          54
#define NGX_HTTP_V2_VARY_INDEX            59


static u_char *ngx_http_v2_write_int(u_char *pos, ngx_uint_t prefix,
    ngx_uint_t value);
static void ngx_http_v2_write_headers_head(u_char *pos, size_t length,
    ngx_uint_t sid, ngx_uint_t end_headers, ngx_uint_t end_stream);
static void ngx_http_v2_write_continuation_head(u_char *pos, size_t length,
    ngx_uint_t sid, ngx_uint_t end_headers);

static ngx_chain_t *ngx_http_v2_send_chain(ngx_connection_t *fc,
    ngx_chain_t *in, off_t limit);

static ngx_chain_t *ngx_http_v2_filter_get_shadow(
    ngx_http_v2_stream_t *stream, ngx_buf_t *buf, off_t offset, off_t size);
static ngx_http_v2_out_frame_t *ngx_http_v2_filter_get_data_frame(
    ngx_http_v2_stream_t *stream, size_t len, ngx_chain_t *first,
    ngx_chain_t *last);

static ngx_inline ngx_int_t ngx_http_v2_flow_control(
    ngx_http_v2_connection_t *h2c, ngx_http_v2_stream_t *stream);
static void ngx_http_v2_waiting_queue(ngx_http_v2_connection_t *h2c,
    ngx_http_v2_stream_t *stream);

static ngx_inline ngx_int_t ngx_http_v2_filter_send(
    ngx_connection_t *fc, ngx_http_v2_stream_t *stream);

static ngx_int_t ngx_http_v2_headers_frame_handler(
    ngx_http_v2_connection_t *h2c, ngx_http_v2_out_frame_t *frame);
static ngx_int_t ngx_http_v2_data_frame_handler(
    ngx_http_v2_connection_t *h2c, ngx_http_v2_out_frame_t *frame);
static ngx_inline void ngx_http_v2_handle_frame(
    ngx_http_v2_stream_t *stream, ngx_http_v2_out_frame_t *frame);
static ngx_inline void ngx_http_v2_handle_stream(
    ngx_http_v2_connection_t *h2c, ngx_http_v2_stream_t *stream);

static void ngx_http_v2_filter_cleanup(void *data);

static ngx_int_t ngx_http_v2_filter_init(ngx_conf_t *cf);


static ngx_http_module_t  ngx_http_v2_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_v2_filter_init,               /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


ngx_module_t  ngx_http_v2_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_v2_filter_module_ctx,        /* module context */
    NULL,                                  /* module directives */
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

/*
2017/03/18 17:01:45[      ngx_http_proxy_process_status_line,  2466]  [debug] 30470#30470: *3 http proxy status 404 "404 Not Found"
2017/03/18 17:01:45[           ngx_http_proxy_process_header,  2544]  [debug] 30470#30470: *3 http proxy header: "Date: Sat, 18 Mar 2017 09:03:34 GMT"
2017/03/18 17:01:45[           ngx_http_proxy_process_header,  2544]  [debug] 30470#30470: *3 http proxy header: "Content-Type: text/plain; charset=utf-8"
2017/03/18 17:01:45[           ngx_http_proxy_process_header,  2544]  [debug] 30470#30470: *3 http proxy header: "Content-Length: 9"
2017/03/18 17:01:45[           ngx_http_proxy_process_header,  2544]  [debug] 30470#30470: *3 http proxy header: "X-Backend-Header-Rtt: 0.002134"
2017/03/18 17:01:45[           ngx_http_proxy_process_header,  2544]  [debug] 30470#30470: *3 http proxy header: "Connection: close"
2017/03/18 17:01:45[           ngx_http_proxy_process_header,  2544]  [debug] 30470#30470: *3 http proxy header: "Server: nghttpx"
2017/03/18 17:01:45[           ngx_http_proxy_process_header,  2544]  [debug] 30470#30470: *3 http proxy header: "Via: 2 nghttpx"
2017/03/18 17:01:45[           ngx_http_proxy_process_header,  2544]  [debug] 30470#30470: *3 http proxy header: "x-frame-options: SAMEORIGIN"
2017/03/18 17:01:45[                            ngx_memalign,    72]  [debug] 30470#30470: *3 posix_memalign: 0000000000C40B90:4096 @16
2017/03/18 17:01:45[           ngx_http_proxy_process_header,  2544]  [debug] 30470#30470: *3 http proxy header: "x-xss-protection: 1; mode=block"
2017/03/18 17:01:45[           ngx_http_proxy_process_header,  2544]  [debug] 30470#30470: *3 http proxy header: "x-content-type-options: nosniff"
2017/03/18 17:01:45[           ngx_http_proxy_process_header,  2554]  [debug] 30470#30470: *3 http proxy header done
2017/03/18 17:01:45[      ngx_http_proxy_process_header,  2622][yangya  [debug] 30470#30470: *3 upstream header recv ok, u->keepalive:0
2017/03/18 17:01:45[           ngx_http_proxy_process_header,  2625]  [debug] 30470#30470: *3 yang test .... body:not found
2017/03/18 17:01:45[               ngx_http_send_header,  3358][yangya  [debug] 30470#30470: *3 ngx http send header
2017/03/18 17:01:45[               ngx_http_v2_header_filter,   143]  [debug] 30470#30470: *3 http2 header filter
*/

/* NGINX在ngx_http_v2_state_header_block对接收到的头部帧进行解码解包，在ngx_http_v2_header_filter中对头部帧进行编码组包 */
/* NGINX接收客户端的header帧在函数ngx_http_v2_header_filter，发送响应的header帧在函数ngx_http_v2_header_filter */
/* 接收完后端响应的头部信息后，解析成功后发送这些头部信息到客户端，需要走该header filter模块 */
static ngx_int_t
ngx_http_v2_header_filter(ngx_http_request_t *r)
{
    u_char                     status, *p, *head;
    size_t                     len, rest;
    ngx_buf_t                 *b;
    ngx_str_t                  host, location;
    ngx_uint_t                 i, port, continuation;
    ngx_chain_t               *cl;
    ngx_list_part_t           *part;
    ngx_table_elt_t           *header;
    ngx_connection_t          *fc;
    ngx_http_cleanup_t        *cln;
    ngx_http_v2_stream_t      *stream;
    ngx_http_v2_out_frame_t   *frame;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_core_srv_conf_t  *cscf;
    struct sockaddr_in        *sin;
#if (NGX_HAVE_INET6)
    struct sockaddr_in6       *sin6;
#endif
    u_char                     addr[NGX_SOCKADDR_STRLEN];


    if (!r->stream) { /* 如果没有创建对应的stream，则直接跳到下一个filter */
        return ngx_http_next_header_filter(r);
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http2 header filter");

    if (r->header_sent) {
        return NGX_OK;
    }

    r->header_sent = 1;

    if (r != r->main) {
        return NGX_OK;
    }

    if (r->method == NGX_HTTP_HEAD) {
        r->header_only = 1;
    }

    switch (r->headers_out.status) {

    case NGX_HTTP_OK:
        status = ngx_http_v2_indexed(NGX_HTTP_V2_STATUS_200_INDEX);
        break;

    case NGX_HTTP_NO_CONTENT:
        r->header_only = 1;

        ngx_str_null(&r->headers_out.content_type);

        r->headers_out.content_length = NULL;
        r->headers_out.content_length_n = -1;

        r->headers_out.last_modified_time = -1;
        r->headers_out.last_modified = NULL;

        status = ngx_http_v2_indexed(NGX_HTTP_V2_STATUS_204_INDEX);
        break;

    case NGX_HTTP_PARTIAL_CONTENT:
        status = ngx_http_v2_indexed(NGX_HTTP_V2_STATUS_206_INDEX);
        break;

    case NGX_HTTP_NOT_MODIFIED:
        r->header_only = 1;
        status = ngx_http_v2_indexed(NGX_HTTP_V2_STATUS_304_INDEX);
        break;

    default:
        r->headers_out.last_modified_time = -1;
        r->headers_out.last_modified = NULL;

        switch (r->headers_out.status) {

        case NGX_HTTP_BAD_REQUEST:
            status = ngx_http_v2_indexed(NGX_HTTP_V2_STATUS_400_INDEX);
            break;

        case NGX_HTTP_NOT_FOUND:
            status = ngx_http_v2_indexed(NGX_HTTP_V2_STATUS_404_INDEX);
            break;

        case NGX_HTTP_INTERNAL_SERVER_ERROR:
            status = ngx_http_v2_indexed(NGX_HTTP_V2_STATUS_500_INDEX);
            break;

        default:
            status = 0;
        }
    }

    /* NGINX在ngx_http_v2_state_header_block对接收到的头部帧进行解码解包，在ngx_http_v2_header_filter中对头部帧进行编码组包
       静态映射表在ngx_http_v2_static_table
    */

    /* 头部9字节 + status响应长度(1字节为什么可以表示status响应码，因为一个字节就可以表示静态表的那个成员,见ngx_http_v2_static_table) */
    len = NGX_HTTP_V2_FRAME_HEADER_SIZE
          + (status ? 1 : 1 + ngx_http_v2_literal_size("418"));

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    //对server:进行编码
    if (r->headers_out.server == NULL) {
        len += 1 + clcf->server_tokens ? ngx_http_v2_literal_size(NGINX_VER)
                                       : ngx_http_v2_literal_size("nginx");
    }

    if (r->headers_out.date == NULL) {
        len += 1 + ngx_http_v2_literal_size("Wed, 31 Dec 1986 18:00:00 GMT");
    }

    if (r->headers_out.content_type.len) {
        len += NGX_HTTP_V2_INT_OCTETS + r->headers_out.content_type.len;

        if (r->headers_out.content_type_len == r->headers_out.content_type.len
            && r->headers_out.charset.len)
        {
            len += sizeof("; charset=") - 1 + r->headers_out.charset.len;
        }
    }

    if (r->headers_out.content_length == NULL
        && r->headers_out.content_length_n >= 0)
    {
        len += 1 + ngx_http_v2_integer_octets(NGX_OFF_T_LEN) + NGX_OFF_T_LEN;
    }

    if (r->headers_out.last_modified == NULL
        && r->headers_out.last_modified_time != -1)
    {
        len += 1 + ngx_http_v2_literal_size("Wed, 31 Dec 1986 18:00:00 GMT");
    }

    fc = r->connection;

    if (r->headers_out.location && r->headers_out.location->value.len) {

        if (r->headers_out.location->value.data[0] == '/') {
            if (clcf->server_name_in_redirect) {
                cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
                host = cscf->server_name;

            } else if (r->headers_in.server.len) {
                host = r->headers_in.server;

            } else {
                host.len = NGX_SOCKADDR_STRLEN;
                host.data = addr;

                if (ngx_connection_local_sockaddr(fc, &host, 0) != NGX_OK) {
                    return NGX_ERROR;
                }
            }

            switch (fc->local_sockaddr->sa_family) {

#if (NGX_HAVE_INET6)
            case AF_INET6:
                sin6 = (struct sockaddr_in6 *) fc->local_sockaddr;
                port = ntohs(sin6->sin6_port);
                break;
#endif
#if (NGX_HAVE_UNIX_DOMAIN)
            case AF_UNIX:
                port = 0;
                break;
#endif
            default: /* AF_INET */
                sin = (struct sockaddr_in *) fc->local_sockaddr;
                port = ntohs(sin->sin_port);
                break;
            }

            location.len = sizeof("https://") - 1 + host.len
                           + r->headers_out.location->value.len;

            if (clcf->port_in_redirect) {

#if (NGX_HTTP_SSL)
                if (fc->ssl)
                    port = (port == 443) ? 0 : port;
                else
#endif
                    port = (port == 80) ? 0 : port;

            } else {
                port = 0;
            }

            if (port) {
                location.len += sizeof(":65535") - 1;
            }

            location.data = ngx_pnalloc(r->pool, location.len);
            if (location.data == NULL) {
                return NGX_ERROR;
            }

            p = ngx_cpymem(location.data, "http", sizeof("http") - 1);

#if (NGX_HTTP_SSL)
            if (fc->ssl) {
                *p++ = 's';
            }
#endif

            *p++ = ':'; *p++ = '/'; *p++ = '/';
            p = ngx_cpymem(p, host.data, host.len);

            if (port) {
                p = ngx_sprintf(p, ":%ui", port);
            }

            p = ngx_cpymem(p, r->headers_out.location->value.data,
                              r->headers_out.location->value.len);

            /* update r->headers_out.location->value for possible logging */

            r->headers_out.location->value.len = p - location.data;
            r->headers_out.location->value.data = location.data;
            ngx_str_set(&r->headers_out.location->key, "Location");
        }

        r->headers_out.location->hash = 0;

        len += 1 + NGX_HTTP_V2_INT_OCTETS + r->headers_out.location->value.len;
    }

#if (NGX_HTTP_GZIP)
    if (r->gzip_vary) {
        if (clcf->gzip_vary) {
            len += 1 + ngx_http_v2_literal_size("Accept-Encoding");

        } else {
            r->gzip_vary = 0;
        }
    }
#endif

    part = &r->headers_out.headers.part;
    header = part->elts;

    /* header_out数组列表中的所有NAME:VALUE长度加进来 */
    for (i = 0; /* void */; i++) {

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

        if (header[i].key.len > NGX_HTTP_V2_MAX_FIELD) {
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                          "too long response header name: \"%V\"",
                          &header[i].key);
            return NGX_ERROR;
        }

        if (header[i].value.len > NGX_HTTP_V2_MAX_FIELD) {
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                          "too long response header value: \"%V: %V\"",
                          &header[i].key, &header[i].value);
            return NGX_ERROR;
        }

        len += NGX_HTTP_V2_INT_OCTETS + header[i].key.len
               + NGX_HTTP_V2_INT_OCTETS + header[i].value.len;
    }

    stream = r->stream;

    /* 如果整个头部帧内容超过了最大frame_size大小，则可能需要拆分到多个帧 */
    len += NGX_HTTP_V2_FRAME_HEADER_SIZE
           * (len / stream->connection->frame_size);

    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NGX_ERROR;
    }

    b->last_buf = r->header_only;

    b->last += NGX_HTTP_V2_FRAME_HEADER_SIZE;

    if (status) {
        *b->last++ = status;
    } else {
        *b->last++ = ngx_http_v2_inc_indexed(NGX_HTTP_V2_STATUS_INDEX);
        *b->last++ = 3;
        b->last = ngx_sprintf(b->last, "%03ui", r->headers_out.status);
    }

    if (r->headers_out.server == NULL) {
        *b->last++ = ngx_http_v2_inc_indexed(NGX_HTTP_V2_SERVER_INDEX);

        if (clcf->server_tokens) {
            *b->last++ = sizeof(NGINX_VER) - 1;
            b->last = ngx_cpymem(b->last, NGINX_VER, sizeof(NGINX_VER) - 1);

        } else {
            *b->last++ = sizeof("nginx") - 1;
            b->last = ngx_cpymem(b->last, "nginx", sizeof("nginx") - 1);
        }
    }

    if (r->headers_out.date == NULL) {
        *b->last++ = ngx_http_v2_inc_indexed(NGX_HTTP_V2_DATE_INDEX);
        *b->last++ = (u_char) ngx_cached_http_time.len;

        b->last = ngx_cpymem(b->last, ngx_cached_http_time.data,
                             ngx_cached_http_time.len);
    }

    if (r->headers_out.content_type.len) {
        *b->last++ = ngx_http_v2_inc_indexed(NGX_HTTP_V2_CONTENT_TYPE_INDEX);

        if (r->headers_out.content_type_len == r->headers_out.content_type.len
            && r->headers_out.charset.len)
        {
            *b->last = 0;
            b->last = ngx_http_v2_write_int(b->last, ngx_http_v2_prefix(7),
                                            r->headers_out.content_type.len
                                            + sizeof("; charset=") - 1
                                            + r->headers_out.charset.len);

            p = b->last;

            b->last = ngx_cpymem(p, r->headers_out.content_type.data,
                                 r->headers_out.content_type.len);

            b->last = ngx_cpymem(b->last, "; charset=",
                                 sizeof("; charset=") - 1);

            b->last = ngx_cpymem(b->last, r->headers_out.charset.data,
                                 r->headers_out.charset.len);

            /* update r->headers_out.content_type for possible logging */

            r->headers_out.content_type.len = b->last - p;
            r->headers_out.content_type.data = p;

        } else {
            *b->last = 0;
            b->last = ngx_http_v2_write_int(b->last, ngx_http_v2_prefix(7),
                                            r->headers_out.content_type.len);
            b->last = ngx_cpymem(b->last, r->headers_out.content_type.data,
                                 r->headers_out.content_type.len);
        }
    }

    if (r->headers_out.content_length == NULL
        && r->headers_out.content_length_n >= 0)
    {
        *b->last++ = ngx_http_v2_inc_indexed(NGX_HTTP_V2_CONTENT_LENGTH_INDEX);

        p = b->last;
        b->last = ngx_sprintf(b->last + 1, "%O",
                              r->headers_out.content_length_n);
        *p = (u_char) (b->last - p - 1);
    }

    if (r->headers_out.last_modified == NULL
        && r->headers_out.last_modified_time != -1)
    {
        *b->last++ = ngx_http_v2_inc_indexed(NGX_HTTP_V2_LAST_MODIFIED_INDEX);

        p = b->last;
        b->last = ngx_http_time(b->last + 1, r->headers_out.last_modified_time);
        *p = (u_char) (b->last - p - 1);
    }

    if (r->headers_out.location && r->headers_out.location->value.len) {
        *b->last++ = ngx_http_v2_inc_indexed(NGX_HTTP_V2_LOCATION_INDEX);

        *b->last = 0;
        b->last = ngx_http_v2_write_int(b->last, ngx_http_v2_prefix(7),
                                        r->headers_out.location->value.len);
        b->last = ngx_cpymem(b->last, r->headers_out.location->value.data,
                                      r->headers_out.location->value.len);
    }

#if (NGX_HTTP_GZIP)
    if (r->gzip_vary) {
        *b->last++ = ngx_http_v2_inc_indexed(NGX_HTTP_V2_VARY_INDEX);
        *b->last++ = sizeof("Accept-Encoding") - 1;
        b->last = ngx_cpymem(b->last, "Accept-Encoding",
                             sizeof("Accept-Encoding") - 1);
    }
#endif

    continuation = 0;
    head = b->pos;

    len = b->last - head - NGX_HTTP_V2_FRAME_HEADER_SIZE;
    rest = stream->connection->frame_size - len;

    part = &r->headers_out.headers.part;
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

        if (header[i].hash == 0) {
            continue;
        }

        len = 1 + NGX_HTTP_V2_INT_OCTETS * 2
              + header[i].key.len
              + header[i].value.len;

        if (len > rest) {
            len = b->last - head - NGX_HTTP_V2_FRAME_HEADER_SIZE;

            if (continuation) {
                ngx_http_v2_write_continuation_head(head, len,
                                                    stream->node->id, 0);
            } else {
                continuation = 1;
                ngx_http_v2_write_headers_head(head, len, stream->node->id, 0,
                                               r->header_only);
            }

            rest = stream->connection->frame_size;
            head = b->last;

            b->last += NGX_HTTP_V2_FRAME_HEADER_SIZE;
        }

        p = b->last;

        *p++ = 0;

        *p = 0;
        p = ngx_http_v2_write_int(p, ngx_http_v2_prefix(7), header[i].key.len);
        ngx_strlow(p, header[i].key.data, header[i].key.len);
        p += header[i].key.len;

        *p = 0;
        p = ngx_http_v2_write_int(p, ngx_http_v2_prefix(7),
                                  header[i].value.len);
        p = ngx_cpymem(p, header[i].value.data, header[i].value.len);

        rest -= p - b->last;
        b->last = p;
    }

    len = b->last - head - NGX_HTTP_V2_FRAME_HEADER_SIZE;

    if (continuation) {
        ngx_http_v2_write_continuation_head(head, len, stream->node->id, 1);

    } else {
        ngx_http_v2_write_headers_head(head, len, stream->node->id, 1,
                                       r->header_only);
    }

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    cl->buf = b;
    cl->next = NULL;

    /* 针对前面的header帧封包，组一个frame结构，挂到h2c->last_out队列，通过ngx_http_v2_filter_send触发发送出去 */
    frame = ngx_palloc(r->pool, sizeof(ngx_http_v2_out_frame_t));
    if (frame == NULL) {
        return NGX_ERROR;
    }

    frame->first = cl;
    frame->last = cl;
    //该frame上对应的数据发送完毕后，会调用ngx_http_v2_headers_frame_handler
    frame->handler = ngx_http_v2_headers_frame_handler;
    frame->stream = stream;
    frame->length = b->last - b->pos - NGX_HTTP_V2_FRAME_HEADER_SIZE;
    frame->blocked = 1;
    frame->fin = r->header_only;

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, stream->request->connection->log, 0,
                   "http2:%ui create HEADERS frame %p: len:%uz",
                   stream->node->id, frame, frame->length);

    ngx_http_v2_queue_blocked_frame(stream->connection, frame);

    cln = ngx_http_cleanup_add(r, 0);
    if (cln == NULL) {
        return NGX_ERROR;
    }

    
    cln->handler = ngx_http_v2_filter_cleanup;
    cln->data = stream;

    stream->queued = 1;

    //把ngx_http_v2_send_chain.send_chain=ngx_http_v2_header_filter,后面的数据帧就通过该函数发送，
    //在读取到后端数据后开始走out filter流程，然后调用ngx_http_output_filter，最终执行该ngx_http_v2_send_chain
    fc->send_chain = ngx_http_v2_send_chain;
    fc->need_last_buf = 1;

    return ngx_http_v2_filter_send(fc, stream);
}

static u_char *
ngx_http_v2_write_int(u_char *pos, ngx_uint_t prefix, ngx_uint_t value)
{
    if (value < prefix) {
        *pos++ |= value;
        return pos;
    }

    *pos++ |= prefix;
    value -= prefix;

    while (value >= 128) {
        *pos++ = value % 128 + 128;
        value /= 128;
    }

    *pos++ = (u_char) value;

    return pos;
}


static void
ngx_http_v2_write_headers_head(u_char *pos, size_t length, ngx_uint_t sid,
    ngx_uint_t end_headers, ngx_uint_t end_stream)
{
    u_char  flags;

    pos = ngx_http_v2_write_len_and_type(pos, length,
                                         NGX_HTTP_V2_HEADERS_FRAME);

    flags = NGX_HTTP_V2_NO_FLAG;

    if (end_headers) {
        flags |= NGX_HTTP_V2_END_HEADERS_FLAG;
    }

    if (end_stream) {
        flags |= NGX_HTTP_V2_END_STREAM_FLAG;
    }

    *pos++ = flags;

    (void) ngx_http_v2_write_sid(pos, sid);
}


static void
ngx_http_v2_write_continuation_head(u_char *pos, size_t length, ngx_uint_t sid,
    ngx_uint_t end_headers)
{
    pos = ngx_http_v2_write_len_and_type(pos, length,
                                         NGX_HTTP_V2_CONTINUATION_FRAME);

    *pos++ = end_headers ? NGX_HTTP_V2_END_HEADERS_FLAG : NGX_HTTP_V2_NO_FLAG;

    (void) ngx_http_v2_write_sid(pos, sid);
}

/*
客户端一次uri请求发送过来header帧后，nginx应答给客户端的header帧和数据帧的stream id就是客户端请求header帧的id信息

HEADER帧发送流程:ngx_http_v2_filter_send->ngx_http_v2_send_output_queue
DATA帧发送流程:ngx_http_v2_send_chain->ngx_http_v2_send_output_queue
一次发送不完(例如协议栈写满返回AGAIN)则下次通过ngx_http_v2_write_handler->ngx_http_v2_send_output_queue再次发送

例如通过同一个connect来下载两个文件，则2个文件的相关信息会被组成一个一个交替的帧挂载到该链表上，通过该函数进行交替发送
发送队列last_out中的数据
*/

/*
当http2头部帧发送的时候，会在ngx_http_v2_header_filter把ngx_http_v2_send_chain.send_chain=ngx_http_v2_send_chain

该函数发送数据帧
在读取到后端数据后开始走out filter流程，然后调用ngx_http_output_filter，最终执行该ngx_http_v2_send_chain
*/
static ngx_chain_t *
ngx_http_v2_send_chain(ngx_connection_t *fc, ngx_chain_t *in, off_t limit)
{
    off_t                      size, offset;
    size_t                     rest, frame_size;
    ngx_chain_t               *cl, *out, **ln;
    ngx_http_request_t        *r;
    ngx_http_v2_stream_t      *stream;
    ngx_http_v2_loc_conf_t    *h2lcf;
    ngx_http_v2_out_frame_t   *frame;
    ngx_http_v2_connection_t  *h2c;

    r = fc->data;
    stream = r->stream;
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "ngx_http_v2_send_chain");

#if (NGX_SUPPRESS_WARN)
    size = 0;
#endif

    while (in) {
        size = ngx_buf_size(in->buf);

        if (size || in->buf->last_buf) {
            break;
        }

        in = in->next;
    }

    if (in == NULL) { /* chain链上没有要发送的数据 */

        if (stream->queued) {
            fc->write->delayed = 1;
        } else {
            fc->buffered &= ~NGX_HTTP_V2_BUFFERED;
        }

        return NULL;
    }

    h2c = stream->connection;

    /* 窗口 */
    if (size && ngx_http_v2_flow_control(h2c, stream) == NGX_DECLINED) {
        fc->write->delayed = 1;
        return in;
    }

    if (in->buf->tag == (ngx_buf_tag_t) &ngx_http_v2_filter_get_shadow) {
        cl = ngx_alloc_chain_link(r->pool);
        if (cl == NULL) {
            return NGX_CHAIN_ERROR;
        }

        cl->buf = in->buf;
        in->buf = cl->buf->shadow;

        offset = ngx_buf_in_memory(in->buf)
                 ? (cl->buf->pos - in->buf->pos)
                 : (cl->buf->file_pos - in->buf->file_pos);

        cl->next = stream->free_bufs;
        stream->free_bufs = cl;

    } else {
        offset = 0;
    }

    /* 发送limit不能超过h2c->send_window和stream->send_window的最小值 */
    if (limit == 0 || limit > (off_t) h2c->send_window) {
        limit = h2c->send_window;
    }
    if (limit > stream->send_window) {
        limit = (stream->send_window > 0) ? stream->send_window : 0;
    }

    h2lcf = ngx_http_get_module_loc_conf(r, ngx_http_v2_module);

    /* frame_size为本地配置chunk_size和对端通知的frame_size的最小值 */
    frame_size = (h2lcf->chunk_size < h2c->frame_size)
                 ? h2lcf->chunk_size : h2c->frame_size;

#if (NGX_SUPPRESS_WARN)
    cl = NULL;
#endif

    for ( ;; ) {  
        if ((off_t) frame_size > limit) {
            frame_size = (size_t) limit;
        }

        ln = &out;
        rest = frame_size;

        /* 把chain链中的buf组包到新的cl chain(即out链)链中,但是数据总大小不能超过rest限制 */
        while ((off_t) rest >= size) {

            if (offset) {
                cl = ngx_http_v2_filter_get_shadow(stream, in->buf,
                                                   offset, size);
                if (cl == NULL) {
                    return NGX_CHAIN_ERROR;
                }

                offset = 0;

            } else {
                cl = ngx_alloc_chain_link(r->pool);
                if (cl == NULL) {
                    return NGX_CHAIN_ERROR;
                }

                cl->buf = in->buf;
            }

            *ln = cl;
            ln = &cl->next;

            rest -= (size_t) size;
            in = in->next;

            if (in == NULL) {
                frame_size -= rest; //in链中的数据已经全部移到out链，这时候的frame_size就是out中的数据大小
                rest = 0; //清0，说明所有的in链中的数据都可以全部发送出去
                break;
            }

            size = ngx_buf_size(in->buf);
        }

        if (rest) {
            cl = ngx_http_v2_filter_get_shadow(stream, in->buf, offset, rest);
            if (cl == NULL) {
                return NGX_CHAIN_ERROR;
            }

            cl->buf->flush = 0;
            cl->buf->last_buf = 0;

            *ln = cl;

            offset += rest;
            size -= rest;
        }

        frame = ngx_http_v2_filter_get_data_frame(stream, frame_size, out, cl);
        if (frame == NULL) {
            return NGX_CHAIN_ERROR;
        }

        ngx_http_v2_queue_frame(h2c, frame);

        /* 发送了这么多数据，则窗口减少 */
        h2c->send_window -= frame_size;

        stream->send_window -= frame_size;
        stream->queued++;

        if (in == NULL) {
            break;
        }

        limit -= frame_size;

        if (limit == 0) {
            break;
        }
    }

    if (offset) {
        cl = ngx_http_v2_filter_get_shadow(stream, in->buf, offset, size);
        if (cl == NULL) {
            return NGX_CHAIN_ERROR;
        }

        in->buf = cl->buf;
        ngx_free_chain(r->pool, cl);
    }

    if (ngx_http_v2_filter_send(fc, stream) == NGX_ERROR) {
        return NGX_CHAIN_ERROR;
    }

    if (in && ngx_http_v2_flow_control(h2c, stream) == NGX_DECLINED) {
        fc->write->delayed = 1;
    }

    return in;
}


static ngx_chain_t *
ngx_http_v2_filter_get_shadow(ngx_http_v2_stream_t *stream, ngx_buf_t *buf,
    off_t offset, off_t size)
{
    ngx_buf_t    *chunk;
    ngx_chain_t  *cl;

    cl = ngx_chain_get_free_buf(stream->request->pool, &stream->free_bufs);
    if (cl == NULL) {
        return NULL;
    }

    chunk = cl->buf;

    ngx_memcpy(chunk, buf, sizeof(ngx_buf_t));

    chunk->tag = (ngx_buf_tag_t) &ngx_http_v2_filter_get_shadow;
    chunk->shadow = buf;

    if (ngx_buf_in_memory(chunk)) {
        chunk->pos += offset;
        chunk->last = chunk->pos + size;
    }

    if (chunk->in_file) {
        chunk->file_pos += offset;
        chunk->file_last = chunk->file_pos + size;
    }

    return cl;
}

//获取data帧frame结构
static ngx_http_v2_out_frame_t *
ngx_http_v2_filter_get_data_frame(ngx_http_v2_stream_t *stream,
    size_t len, ngx_chain_t *first, ngx_chain_t *last)
{
    u_char                      flags;
    ngx_buf_t                  *buf;
    ngx_chain_t                *cl;
    ngx_http_v2_out_frame_t  *frame;

    //帧结构stream->free_frames会进行重复利用
    frame = stream->free_frames;

    if (frame) {
        stream->free_frames = frame->next;

    } else {
        frame = ngx_palloc(stream->request->pool,
                           sizeof(ngx_http_v2_out_frame_t));
        if (frame == NULL) {
            return NULL;
        }
    }

    flags = last->buf->last_buf ? NGX_HTTP_V2_END_STREAM_FLAG : 0;

    ngx_log_debug4(NGX_LOG_DEBUG_HTTP, stream->request->connection->log, 0,
                   "http2:%ui create DATA frame %p: len:%uz flags:%ui",
                   stream->node->id, frame, len, (ngx_uint_t) flags);

    cl = ngx_chain_get_free_buf(stream->request->pool,
                                &stream->free_data_headers);
    if (cl == NULL) {
        return NULL;
    }

    buf = cl->buf;

    if (!buf->start) {
        buf->start = ngx_palloc(stream->request->pool, NGX_HTTP_V2_FRAME_HEADER_SIZE);
        if (buf->start == NULL) {
            return NULL;
        }

        buf->end = buf->start + NGX_HTTP_V2_FRAME_HEADER_SIZE;
        buf->last = buf->end;

        buf->tag = (ngx_buf_tag_t) &ngx_http_v2_filter_get_data_frame;
        buf->memory = 1;
    }

    buf->pos = buf->start;
    buf->last = buf->pos;

    buf->last = ngx_http_v2_write_len_and_type(buf->last, len,
                                               NGX_HTTP_V2_DATA_FRAME);
    *buf->last++ = flags;

    buf->last = ngx_http_v2_write_sid(buf->last, stream->node->id);

    cl->next = first;
    first = cl;

    last->buf->flush = 1;

    frame->first = first;
    frame->last = last;
    frame->handler = ngx_http_v2_data_frame_handler;
    frame->stream = stream;
    frame->length = len;
    frame->blocked = 0;
    frame->fin = last->buf->last_buf;

    return frame;
}

/*
客户端一次uri请求发送过来header帧后，nginx应答给客户端的header帧和数据帧的stream id就是客户端请求header帧的id信息
HEADER帧发送流程:ngx_http_v2_filter_send->ngx_http_v2_send_output_queue
DATA帧发送流程:ngx_http_v2_send_chain->ngx_http_v2_send_output_queue
一次发送不完(例如协议栈写满返回AGAIN)则下次通过ngx_http_v2_write_handler->ngx_http_v2_send_output_queue再次发送

例如通过同一个connect来下载两个文件，则2个文件的相关信息会被组成一个一个交替的帧挂载到该链表上，通过该函数进行交替发送
发送队列last_out中的数据
*/
static ngx_inline ngx_int_t
ngx_http_v2_filter_send(ngx_connection_t *fc, ngx_http_v2_stream_t *stream)
{
    stream->blocked = 1;

    if (ngx_http_v2_send_output_queue(stream->connection) == NGX_ERROR) {
        fc->error = 1;
        return NGX_ERROR;
    }

    stream->blocked = 0;

    if (stream->queued) {
        fc->buffered |= NGX_HTTP_V2_BUFFERED;
        fc->write->delayed = 1;
        return NGX_AGAIN;
    }

    fc->buffered &= ~NGX_HTTP_V2_BUFFERED;

    return NGX_OK;
}

/* 查看发送窗口是不是大于0，大于0则发送 */
static ngx_inline ngx_int_t
ngx_http_v2_flow_control(ngx_http_v2_connection_t *h2c,
    ngx_http_v2_stream_t *stream)
{
    if (stream->send_window <= 0) {
        stream->exhausted = 1;
        return NGX_DECLINED;
    }

    if (h2c->send_window == 0) {
        ngx_http_v2_waiting_queue(h2c, stream);
        return NGX_DECLINED;
    }

    return NGX_OK;
}


static void
ngx_http_v2_waiting_queue(ngx_http_v2_connection_t *h2c,
    ngx_http_v2_stream_t *stream)
{
    ngx_queue_t           *q;
    ngx_http_v2_stream_t  *s;

    if (stream->handled) {
        return;
    }

    stream->handled = 1;

    for (q = ngx_queue_last(&h2c->waiting);
         q != ngx_queue_sentinel(&h2c->waiting);
         q = ngx_queue_prev(q))
    {
        s = ngx_queue_data(q, ngx_http_v2_stream_t, queue);

        if (s->node->rank < stream->node->rank
            || (s->node->rank == stream->node->rank
                && s->node->rel_weight >= stream->node->rel_weight))
        {
            break;
        }
    }

    ngx_queue_insert_after(q, &stream->queue);
}


//每一个h2c->last_out链表中的frame发送完成都会调用对应的handler,这里是header帧发送完成的handler
static ngx_int_t
ngx_http_v2_headers_frame_handler(ngx_http_v2_connection_t *h2c,
    ngx_http_v2_out_frame_t *frame)
{
    ngx_buf_t             *buf;
    ngx_http_v2_stream_t  *stream;

    buf = frame->first->buf;

    if (buf->pos != buf->last) { /* 说明还没有发送完成 */
        return NGX_AGAIN;
    }

    stream = frame->stream;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, h2c->connection->log, 0,
                   "http2:%ui HEADERS frame %p was sent",
                   stream->node->id, frame);

    ngx_free_chain(stream->request->pool, frame->first);

    ngx_http_v2_handle_frame(stream, frame);

    ngx_http_v2_handle_stream(h2c, stream);

    return NGX_OK;
}

//每一个h2c->last_out链表中的frame发送完成都会调用对应的handler,这里是data帧发送完成的handler
static ngx_int_t
ngx_http_v2_data_frame_handler(ngx_http_v2_connection_t *h2c,
    ngx_http_v2_out_frame_t *frame)
{
    ngx_buf_t             *buf;
    ngx_chain_t           *cl, *ln;
    ngx_http_v2_stream_t  *stream;

    stream = frame->stream;

    cl = frame->first;

    if (cl->buf->tag == (ngx_buf_tag_t) &ngx_http_v2_filter_get_data_frame) {

        if (cl->buf->pos != cl->buf->last) { /* 说明该frame只有部分数据发送了 */
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, h2c->connection->log, 0,
                           "http2:%ui DATA frame %p was sent partially",
                           stream->node->id, frame);

            return NGX_AGAIN;
        }

        ln = cl->next;

        cl->next = stream->free_data_headers;
        stream->free_data_headers = cl;

        if (cl == frame->last) {
            goto done;
        }

        cl = ln;
    }

    for ( ;; ) {
        if (cl->buf->tag == (ngx_buf_tag_t) &ngx_http_v2_filter_get_shadow) {
            buf = cl->buf->shadow;

            if (ngx_buf_in_memory(buf)) {
                buf->pos = cl->buf->pos;
            }

            if (buf->in_file) {
                buf->file_pos = cl->buf->file_pos;
            }
        }

        if (ngx_buf_size(cl->buf) != 0) {

            if (cl != frame->first) {
                frame->first = cl;
                ngx_http_v2_handle_stream(h2c, stream);
            }

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, h2c->connection->log, 0,
                           "http2:%ui DATA frame %p was sent partially",
                           stream->node->id, frame);

            return NGX_AGAIN;
        }

        ln = cl->next;

        if (cl->buf->tag == (ngx_buf_tag_t) &ngx_http_v2_filter_get_shadow) {
            cl->next = stream->free_bufs;
            stream->free_bufs = cl;

        } else {
            ngx_free_chain(stream->request->pool, cl);
        }

        if (cl == frame->last) {
            goto done;
        }

        cl = ln;
    }

done:

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, h2c->connection->log, 0,
                   "http2:%ui DATA frame %p was sent",
                   stream->node->id, frame);

    stream->request->header_size += NGX_HTTP_V2_FRAME_HEADER_SIZE;

    ngx_http_v2_handle_frame(stream, frame);

    ngx_http_v2_handle_stream(h2c, stream);

    return NGX_OK;
}


static ngx_inline void
ngx_http_v2_handle_frame(ngx_http_v2_stream_t *stream,
    ngx_http_v2_out_frame_t *frame)
{
    ngx_http_request_t  *r;

    r = stream->request;

    r->connection->sent += NGX_HTTP_V2_FRAME_HEADER_SIZE + frame->length;

    if (frame->fin) {
        stream->out_closed = 1;
    }

    frame->next = stream->free_frames;
    stream->free_frames = frame;

    stream->queued--;
}


static ngx_inline void
ngx_http_v2_handle_stream(ngx_http_v2_connection_t *h2c,
    ngx_http_v2_stream_t *stream)
{
    ngx_event_t  *wev;

    if (stream->handled || stream->blocked || stream->exhausted) {
        return;
    }

    wev = stream->request->connection->write;

    /*
     * This timer can only be set if the stream was delayed because of rate
     * limit.  In that case the event should be triggered by the timer.
     */

    if (!wev->timer_set) {
        wev->delayed = 0;

        stream->handled = 1;
        ngx_queue_insert_tail(&h2c->posted, &stream->queue);
    }
}


static void
ngx_http_v2_filter_cleanup(void *data)
{
    ngx_http_v2_stream_t *stream = data;

    size_t                     window;
    ngx_http_v2_out_frame_t   *frame, **fn;
    ngx_http_v2_connection_t  *h2c;

    if (stream->handled) {
        stream->handled = 0;
        ngx_queue_remove(&stream->queue);
    }

    if (stream->queued == 0) {
        return;
    }

    window = 0;
    h2c = stream->connection;
    fn = &h2c->last_out;

    for ( ;; ) {
        frame = *fn;

        if (frame == NULL) {
            break;
        }

        if (frame->stream == stream && !frame->blocked) {
            *fn = frame->next;

            window += frame->length;

            if (--stream->queued == 0) {
                break;
            }

            continue;
        }

        fn = &frame->next;
    }

    if (h2c->send_window == 0 && window && !ngx_queue_empty(&h2c->waiting)) {
        ngx_queue_add(&h2c->posted, &h2c->waiting);
        ngx_queue_init(&h2c->waiting);
    }

    h2c->send_window += window;
}


static ngx_int_t
ngx_http_v2_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_v2_header_filter;

    return NGX_OK;
}
