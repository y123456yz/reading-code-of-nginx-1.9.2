
/*
 * Copyright (C) Nginx, Inc.
 * Copyright (C) Valentin V. Bartenev
 */


#ifndef _NGX_HTTP_V2_MODULE_H_INCLUDED_
#define _NGX_HTTP_V2_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    //设置每一个worker的输入缓冲区大小
    size_t                          recv_buffer_size; //http2_recv_buffer_size配置项指定  默认值256 * 1024
    u_char                         *recv_buffer; //根据recv_buffer_size分配内存，赋值见ngx_http_v2_init
} ngx_http_v2_main_conf_t;


typedef struct {
    size_t                          pool_size; //http2_pool_size配置项指定，空间分配在ngx_http_v2_init
    /* 一个连接上同时处理的流最大限度，生效见ngx_http_v2_state_headers */
    ngx_uint_t                      concurrent_streams; //http2_max_concurrent_streams配置项指定 默认128
    //限制经过HPACK压缩后请求头中单个name:value字段的最大尺寸。
    size_t                          max_field_size; //http2_max_field_size配置项指定  默认4096
    /* header内容中的所有name+value之和长度最大值，生效见ngx_http_v2_state_process_header 
    限制经过HPACK压缩后完整请求头的最大尺寸。
    */
    size_t                          max_header_size; //http2_max_header_size配置项指定 默认16384
    ngx_uint_t                      streams_index_mask; //http2_streams_index_size配置项指定 默认32-1
    ngx_msec_t                      recv_timeout; //http2_recv_timeout配置项指定  默认30000
    /* 设置空闲连接关闭的超时时间。 */
    ngx_msec_t                      idle_timeout; //http2_idle_timeout配置项指定  默认180000
} ngx_http_v2_srv_conf_t;

typedef struct {
    /* 设置响应报文内容（response body）分片的最大长度。如果这个值过小，将会带来更高的开销，
        如果值过大，则会导致线头阻塞的问题。默认大小8k。 */
    size_t                          chunk_size; //http2_chunk_size配置项指定
} ngx_http_v2_loc_conf_t;


extern ngx_module_t  ngx_http_v2_module;


#endif /* _NGX_HTTP_V2_MODULE_H_INCLUDED_ */
