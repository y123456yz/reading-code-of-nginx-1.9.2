
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

//Í¨¹ıngx_http_upstream_init_main_conf°ÑËùÓĞngx_http_upstream_headers_in³ÉÔ±×öhashÔËËã£¬·ÅÈëngx_http_upstream_main_conf_tµÄheaders_in_hashÖĞ
//ÕâĞ©³ÉÔ±×îÖÕ»á¸³Öµ¸øngx_http_request_t->upstream->headers_in
ngx_http_upstream_header_t  ngx_http_upstream_headers_in[] = { //ºó¶ËÓ¦´ğµÄÍ·²¿ĞĞÆ¥ÅäÕâÀïÃæµÄ×Ö¶Îºó£¬×îÖÕÓÉngx_http_upstream_headers_in_tÀïÃæµÄ³ÉÔ±Ö¸Ïò
//¸ÃÊı×éÉúĞ§µØ·½¼ûngx_http_proxy_process_header£¬Í¨¹ıhandler(Èçngx_http_upstream_copy_header_line)°Ñºó¶ËÍ·²¿ĞĞµÄÏà¹ØĞÅÏ¢¸³Öµ¸øngx_http_request_t->upstream->headers_inÏà¹Ø³ÉÔ±
    { ngx_string("Status"),
                 ngx_http_upstream_process_header_line, //Í¨¹ı¸Ãhandlerº¯Êı°Ñ´Óºó¶Ë·şÎñÆ÷½âÎöµ½µÄÍ·²¿ĞĞ×Ö¶Î¸³Öµ¸øngx_http_upstream_headers_in_t->status
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

//Ö»ÓĞÔÚÅäÖÃÁËlocation /uri {mytest;}ºó£¬HTTP¿ò¼Ü²Å»áÔÚÄ³¸öÇëÇóÆ¥ÅäÁË/uriºóµ÷ÓÃËü´¦ÀíÇëÇó
    { ngx_string("Location"),  //ºó¶ËÓ¦´ğÕâ¸ö£¬±íÊ¾ĞèÒªÖØ¶¨Ïò
                 ngx_http_upstream_process_header_line,
                 offsetof(ngx_http_upstream_headers_in_t, location),
                 ngx_http_upstream_rewrite_location, 0, 0 }, //ngx_http_upstream_process_headersÖĞÖ´ĞĞ

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

    { ngx_string("Vary"), //nginxÔÚ»º´æ¹ı³ÌÖĞ²»»á´¦Àí"Vary"Í·£¬ÎªÁËÈ·±£Ò»Ğ©Ë½ÓĞÊı¾İ²»±»ËùÓĞµÄÓÃ»§¿´µ½£¬
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
¹ØÓÚnginx upstreamµÄ¼¸ÖÖÅäÖÃ·½Ê½
·¢±íÓÚ2011 Äê 06 ÔÂ 16 ÈÕÓÉedwin 
Æ½Ê±Ò»Ö±ÒÀÀµÓ²¼şÀ´×÷load blance£¬×î½üÑĞ¾¿NginxÀ´×ö¸ºÔØÉè±¸£¬¼ÇÂ¼ÏÂupstreamµÄ¼¸ÖÖÅäÖÃ·½Ê½¡£

µÚÒ»ÖÖ£ºÂÖÑ¯

upstream test{
    server 192.168.0.1:3000;
    server 192.168.0.1:3001;
}µÚ¶şÖÖ£ºÈ¨ÖØ

upstream test{
    server 192.168.0.1 weight=2;
    server 192.168.0.2 weight=3;
}ÕâÖÖÄ£Ê½¿É½â¾ö·şÎñÆ÷ĞÔÄÜ²»µÈµÄÇé¿öÏÂÂÖÑ¯±ÈÂÊµÄµ÷Åä

µÚÈıÖÖ£ºip_hash

upstream test{
    ip_hash;
    server 192.168.0.1;
    server 192.168.0.2;
}ÕâÖÖÄ£Ê½»á¸ù¾İÀ´Ô´IPºÍºó¶ËÅäÖÃÀ´×öhash·ÖÅä£¬È·±£¹Ì¶¨IPÖ»·ÃÎÊÒ»¸öºó¶Ë

µÚËÄÖÖ£ºfair

ĞèÒª°²×°Upstream Fair Balancer Module

upstream test{
    server 192.168.0.1;
    server 192.168.0.2;
    fair;
}ÕâÖÖÄ£Ê½»á¸ù¾İºó¶Ë·şÎñµÄÏìÓ¦Ê±¼äÀ´·ÖÅä£¬ÏìÓ¦Ê±¼ä¶ÌµÄºó¶ËÓÅÏÈ·ÖÅä

µÚÎåÖÖ£º×Ô¶¨Òåhash

ĞèÒª°²×°Upstream Hash Module

upstream test{
    server 192.168.0.1;
    server 192.168.0.2;
    hash $request_uri;
}ÕâÖÖÄ£Ê½¿ÉÒÔ¸ù¾İ¸ø¶¨µÄ×Ö·û´®½øĞĞHash·ÖÅä

¾ßÌåÓ¦ÓÃ£º

server{
    listen 80;
    server_name .test.com;
    charset utf-8;
    
    location / {
        proxy_pass http://test/;
    } 
}´ËÍâupstreamÃ¿¸öºó¶ËµÄ¿ÉÉèÖÃ²ÎÊıÎª£º

1.down: ±íÊ¾´ËÌ¨serverÔİÊ±²»²ÎÓë¸ºÔØ¡£
2.weight: Ä¬ÈÏÎª1£¬weightÔ½´ó£¬¸ºÔØµÄÈ¨ÖØ¾ÍÔ½´ó¡£
3.max_fails: ÔÊĞíÇëÇóÊ§°ÜµÄ´ÎÊıÄ¬ÈÏÎª1.µ±³¬¹ı×î´ó´ÎÊıÊ±£¬·µ»Øproxy_next_upstreamÄ£¿é¶¨ÒåµÄ´íÎó¡£
4.fail_timeout: max_fails´ÎÊ§°Üºó£¬ÔİÍ£µÄÊ±¼ä¡£
5.backup: ÆäËüËùÓĞµÄ·Çbackup»úÆ÷down»òÕßÃ¦µÄÊ±ºò£¬ÇëÇóbackup»úÆ÷£¬Ó¦¼±´ëÊ©¡£
*/
static ngx_command_t  ngx_http_upstream_commands[] = {
/*
Óï·¨£ºupstream name { ... } 
Ä¬ÈÏÖµ£ºnone 
Ê¹ÓÃ×Ö¶Î£ºhttp 
Õâ¸ö×Ö¶ÎÉèÖÃÒ»Èº·şÎñÆ÷£¬¿ÉÒÔ½«Õâ¸ö×Ö¶Î·ÅÔÚproxy_passºÍfastcgi_passÖ¸ÁîÖĞ×÷ÎªÒ»¸öµ¥¶ÀµÄÊµÌå£¬ËüÃÇ¿ÉÒÔ¿ÉÒÔÊÇ¼àÌı²»Í¬¶Ë¿ÚµÄ·şÎñÆ÷£¬
²¢ÇÒÒ²¿ÉÒÔÊÇÍ¬Ê±¼àÌıTCPºÍUnix socketµÄ·şÎñÆ÷¡£
·şÎñÆ÷¿ÉÒÔÖ¸¶¨²»Í¬µÄÈ¨ÖØ£¬Ä¬ÈÏÎª1¡£
Ê¾ÀıÅäÖÃ

upstream backend {
  server backend1.example.com weight=5;
  server 127.0.0.1:8080       max_fails=3  fail_timeout=30s;
  server unix:/tmp/backend3;

  server backup1.example.com:8080 backup; 
}ÇëÇó½«°´ÕÕÂÖÑ¯µÄ·½Ê½·Ö·¢µ½ºó¶Ë·şÎñÆ÷£¬µ«Í¬Ê±Ò²»á¿¼ÂÇÈ¨ÖØ¡£
ÔÚÉÏÃæµÄÀı×ÓÖĞÈç¹ûÃ¿´Î·¢Éú7¸öÇëÇó£¬5¸öÇëÇó½«±»·¢ËÍµ½backend1.example.com£¬ÆäËûÁ½Ì¨½«·Ö±ğµÃµ½Ò»¸öÇëÇó£¬Èç¹ûÓĞÒ»Ì¨·şÎñÆ÷²»¿ÉÓÃ£¬ÄÇÃ´
ÇëÇó½«±»×ª·¢µ½ÏÂÒ»Ì¨·şÎñÆ÷£¬Ö±µ½ËùÓĞµÄ·şÎñÆ÷¼ì²é¶¼Í¨¹ı¡£Èç¹ûËùÓĞµÄ·şÎñÆ÷¶¼ÎŞ·¨Í¨¹ı¼ì²é£¬ÄÇÃ´½«·µ»Ø¸ø¿Í»§¶Ë×îºóÒ»Ì¨¹¤×÷µÄ·şÎñÆ÷²úÉúµÄ½á¹û¡£

max_fails=number

  ÉèÖÃÔÚfail_timeout²ÎÊıÉèÖÃµÄÊ±¼äÄÚ×î´óÊ§°Ü´ÎÊı£¬Èç¹ûÔÚÕâ¸öÊ±¼äÄÚ£¬ËùÓĞÕë¶Ô¸Ã·şÎñÆ÷µÄÇëÇó
  ¶¼Ê§°ÜÁË£¬ÄÇÃ´ÈÏÎª¸Ã·şÎñÆ÷»á±»ÈÏÎªÊÇÍ£»úÁË£¬Í£»úÊ±¼äÊÇfail_timeoutÉèÖÃµÄÊ±¼ä¡£Ä¬ÈÏÇé¿öÏÂ£¬
  ²»³É¹¦Á¬½ÓÊı±»ÉèÖÃÎª1¡£±»ÉèÖÃÎªÁãÔò±íÊ¾²»½øĞĞÁ´½ÓÊıÍ³¼Æ¡£ÄÇĞ©Á¬½Ó±»ÈÏÎªÊÇ²»³É¹¦µÄ¿ÉÒÔÍ¨¹ı
  proxy_next_upstream, fastcgi_next_upstream£¬ºÍmemcached_next_upstreamÖ¸ÁîÅäÖÃ¡£http_404
  ×´Ì¬²»»á±»ÈÏÎªÊÇ²»³É¹¦µÄ³¢ÊÔ¡£

fail_time=time
  ÉèÖÃ ¶à³¤Ê±¼äÄÚÊ§°Ü´ÎÊı´ïµ½×î´óÊ§°Ü´ÎÊı»á±»ÈÏÎª·şÎñÆ÷Í£»úÁË·şÎñÆ÷»á±»ÈÏÎªÍ£»úµÄÊ±¼ä³¤¶È Ä¬ÈÏÇé¿öÏÂ£¬³¬Ê±Ê±¼ä±»ÉèÖÃÎª10S
*/
    { ngx_string("upstream"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_TAKE1,
      ngx_http_upstream,
      0,
      0,
      NULL },

    /*
    Óï·¨£ºserver name [parameters];
    ÅäÖÃ¿é£ºupstream
    serverÅäÖÃÏîÖ¸¶¨ÁËÒ»Ì¨ÉÏÓÎ·şÎñÆ÷µÄÃû×Ö£¬Õâ¸öÃû×Ö¿ÉÒÔÊÇÓòÃû¡¢IPµØÖ·¶Ë¿Ú¡¢UNIX¾ä±úµÈ£¬ÔÚÆäºó»¹¿ÉÒÔ¸úÏÂÁĞ²ÎÊı¡£
    weight=number£ºÉèÖÃÏòÕâÌ¨ÉÏÓÎ·şÎñÆ÷×ª·¢µÄÈ¨ÖØ£¬Ä¬ÈÏÎª1¡£ weigth²ÎÊı±íÊ¾È¨Öµ£¬È¨ÖµÔ½¸ß±»·ÖÅäµ½µÄ¼¸ÂÊÔ½´ó 
    max_fails=number£º¸ÃÑ¡ÏîÓëfail_timeoutÅäºÏÊ¹ÓÃ£¬Ö¸ÔÚfail_timeoutÊ±¼ä¶ÎÄÚ£¬Èç¹ûÏòµ±Ç°µÄÉÏÓÎ·şÎñÆ÷×ª·¢Ê§°Ü´ÎÊı³¬¹ınumber£¬ÔòÈÏÎªÔÚµ±Ç°µÄfail_timeoutÊ±¼ä¶ÎÄÚÕâÌ¨ÉÏÓÎ·şÎñÆ÷²»¿ÉÓÃ¡£max_failsÄ¬ÈÏÎª1£¬Èç¹ûÉèÖÃÎª0£¬Ôò±íÊ¾²»¼ì²éÊ§°Ü´ÎÊı¡£
    fail_timeout=time£ºfail_timeout±íÊ¾¸ÃÊ±¼ä¶ÎÄÚ×ª·¢Ê§°Ü¶àÉÙ´Îºó¾ÍÈÏÎªÉÏÓÎ·şÎñÆ÷ÔİÊ±²»¿ÉÓÃ£¬ÓÃÓÚÓÅ»¯·´Ïò´úÀí¹¦ÄÜ¡£ËüÓëÏòÉÏÓÎ·şÎñÆ÷½¨Á¢Á¬½ÓµÄ³¬Ê±Ê±¼ä¡¢¶ÁÈ¡ÉÏÓÎ·şÎñÆ÷µÄÏìÓ¦³¬Ê±Ê±¼äµÈÍêÈ«ÎŞ¹Ø¡£fail_timeoutÄ¬ÈÏÎª10Ãë¡£
    down£º±íÊ¾ËùÔÚµÄÉÏÓÎ·şÎñÆ÷ÓÀ¾ÃÏÂÏß£¬Ö»ÔÚÊ¹ÓÃip_hashÅäÖÃÏîÊ±²ÅÓĞÓÃ¡£
    backup£ºÔÚÊ¹ÓÃip_hashÅäÖÃÏîÊ±ËüÊÇÎŞĞ§µÄ¡£Ëü±íÊ¾ËùÔÚµÄÉÏÓÎ·şÎñÆ÷Ö»ÊÇ±¸·İ·şÎñÆ÷£¬Ö»ÓĞÔÚËùÓĞµÄ·Ç±¸·İÉÏÓÎ·şÎñÆ÷¶¼Ê§Ğ§ºó£¬²Å»áÏòËùÔÚµÄÉÏÓÎ·şÎñÆ÷×ª·¢ÇëÇó¡£
    ÀıÈç£º
    upstream  backend  {
      server   backend1.example.com    weight=5;
      server   127.0.0.1:8080          max_fails=3  fail_timeout=30s;
      server   unix:/tmp/backend3;
    }

    
    Óï·¨£ºserver name [parameters] 
    Ä¬ÈÏÖµ£ºnone 
    Ê¹ÓÃ×Ö¶Î£ºupstream 
    Ö¸¶¨ºó¶Ë·şÎñÆ÷µÄÃû³ÆºÍÒ»Ğ©²ÎÊı£¬¿ÉÒÔÊ¹ÓÃÓòÃû£¬IP£¬¶Ë¿Ú£¬»òÕßunix socket¡£Èç¹ûÖ¸¶¨ÎªÓòÃû£¬ÔòÊ×ÏÈ½«Æä½âÎöÎªIP¡£
    ¡¤weight = NUMBER - ÉèÖÃ·şÎñÆ÷È¨ÖØ£¬Ä¬ÈÏÎª1¡£
    ¡¤max_fails = NUMBER - ÔÚÒ»¶¨Ê±¼äÄÚ£¨Õâ¸öÊ±¼äÔÚfail_timeout²ÎÊıÖĞÉèÖÃ£©¼ì²éÕâ¸ö·şÎñÆ÷ÊÇ·ñ¿ÉÓÃÊ±²úÉúµÄ×î¶àÊ§°ÜÇëÇóÊı£¬Ä¬ÈÏÎª1£¬½«ÆäÉèÖÃÎª0¿ÉÒÔ¹Ø±Õ¼ì²é£¬ÕâĞ©´íÎóÔÚproxy_next_upstream»òfastcgi_next_upstream£¨404´íÎó²»»áÊ¹max_failsÔö¼Ó£©ÖĞ¶¨Òå¡£
    ¡¤fail_timeout = TIME - ÔÚÕâ¸öÊ±¼äÄÚ²úÉúÁËmax_failsËùÉèÖÃ´óĞ¡µÄÊ§°Ü³¢ÊÔÁ¬½ÓÇëÇóºóÕâ¸ö·şÎñÆ÷¿ÉÄÜ²»¿ÉÓÃ£¬Í¬ÑùËüÖ¸¶¨ÁË·şÎñÆ÷²»¿ÉÓÃµÄÊ±¼ä£¨ÔÚÏÂÒ»´Î³¢ÊÔÁ¬½ÓÇëÇó·¢ÆğÖ®Ç°£©£¬Ä¬ÈÏÎª10Ãë£¬fail_timeoutÓëÇ°¶ËÏìÓ¦Ê±¼äÃ»ÓĞÖ±½Ó¹ØÏµ£¬²»¹ı¿ÉÒÔÊ¹ÓÃproxy_connect_timeoutºÍproxy_read_timeoutÀ´¿ØÖÆ¡£
    ¡¤down - ±ê¼Ç·şÎñÆ÷´¦ÓÚÀëÏß×´Ì¬£¬Í¨³£ºÍip_hashÒ»ÆğÊ¹ÓÃ¡£
    ¡¤backup - (0.6.7»ò¸ü¸ß)Èç¹ûËùÓĞµÄ·Ç±¸·İ·şÎñÆ÷¶¼å´»ú»ò·±Ã¦£¬ÔòÊ¹ÓÃ±¾·şÎñÆ÷£¨ÎŞ·¨ºÍip_hashÖ¸Áî´îÅäÊ¹ÓÃ£©¡£
    Ê¾ÀıÅäÖÃ
    
    upstream  backend  {
      server   backend1.example.com    weight=5;
      server   127.0.0.1:8080          max_fails=3  fail_timeout=30s;
      server   unix:/tmp/backend3;
    }×¢Òâ£ºÈç¹ûÄãÖ»Ê¹ÓÃÒ»Ì¨ÉÏÓÎ·şÎñÆ÷£¬nginx½«ÉèÖÃÒ»¸öÄÚÖÃ±äÁ¿Îª1£¬¼´max_failsºÍfail_timeout²ÎÊı²»»á±»´¦Àí¡£
    ½á¹û£ºÈç¹ûnginx²»ÄÜÁ¬½Óµ½ÉÏÓÎ£¬ÇëÇó½«¶ªÊ§¡£
    ½â¾ö£ºÊ¹ÓÃ¶àÌ¨ÉÏÓÎ·şÎñÆ÷¡£
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
¸ºÔØ¾ùºâÏà¹ØÅäÖÃ:
upstream
server
ip_hash:¸ù¾İ¿Í»§¶ËµÄIPÀ´×öhash,²»¹ıÈç¹ûsquid -- nginx -- server(s)Ôò£¬ipÓÀÔ¶ÊÇsquid·şÎñÆ÷ip,Òò´Ë²»¹ÜÓÃ,ĞèÒªngx_http_realip_module»òÕßµÚÈı·½Ä£¿é
keepalive:ÅäÖÃµ½ºó¶ËµÄ×î´óÁ¬½ÓÊı£¬±£³Ö³¤Á¬½Ó£¬²»±ØÎªÃ¿Ò»¸ö¿Í»§¶Ë¶¼ÖØĞÂ½¨Á¢nginxµ½ºó¶ËPHPµÈ·şÎñÆ÷µÄÁ¬½Ó£¬ĞèÒª±£³ÖºÍºó¶Ë
    ³¤Á¬½Ó£¬ÀıÈçfastcgi:fastcgi_keep_conn on;       proxy:  proxy_http_version 1.1;  proxy_set_header Connection "";
least_conn:¸ù¾İÆäÈ¨ÖØÖµ£¬½«ÇëÇó·¢ËÍµ½»îÔ¾Á¬½ÓÊı×îÉÙµÄÄÇÌ¨·şÎñÆ÷
hash:¿ÉÒÔ°´ÕÕuri  ip µÈ²ÎÊı½øĞĞ×öhash

²Î¿¼http://tengine.taobao.org/nginx_docs/cn/docs/http/ngx_http_upstream_module.html#ip_hash
*/


/*
Nginx²»½ö½ö¿ÉÒÔÓÃ×öWeb·şÎñÆ÷¡£upstream»úÖÆÆäÊµÊÇÓÉngx_http_upstream_moduleÄ£¿éÊµÏÖµÄ£¬ËüÊÇÒ»¸öHTTPÄ£¿é£¬Ê¹ÓÃupstream»úÖÆÊ±¿Í
»§¶ËµÄÇëÇó±ØĞë»ùÓÚHTTP¡£

¼ÈÈ»upstreamÊÇÓÃÓÚ·ÃÎÊ¡°ÉÏÓÎ¡±·şÎñÆ÷µÄ£¬ÄÇÃ´£¬NginxĞèÒª·ÃÎÊÊ²Ã´ÀàĞÍµÄ¡°ÉÏÓÎ¡±·şÎñÆ÷ÄØ£¿ÊÇApache¡¢TomcatÕâÑùµÄWeb·şÎñÆ÷£¬»¹
ÊÇmemcached¡¢cassandraÕâÑùµÄKey-Value´æ´¢ÏµÍ³£¬ÓÖ»òÊÇmongoDB¡¢MySQLÕâÑùµÄÊı¾İ¿â£¿Õâ¾ÍÉæ¼°upstream»úÖÆµÄ·¶Î§ÁË¡£»ùÓÚÊÂ¼şÇı¶¯
¼Ü¹¹µÄupstream»úÖÆËùÒª·ÃÎÊµÄ¾ÍÊÇËùÓĞÖ§³ÖTCPµÄÉÏÓÎ·şÎñÆ÷¡£Òò´Ë£¬¼ÈÓĞngx_http_proxy_moduleÄ£¿é»ùÓÚupstream»úÖÆÊµÏÖÁËHTTPµÄ·´Ïò
´úÀí¹¦ÄÜ£¬Ò²ÓĞÀàËÆngx_http_memcached_moduleµÄÄ£¿é»ùÓÚupstream»úÖÆÊ¹µÃÇëÇó¿ÉÒÔ·ÃÎÊmemcached·şÎñÆ÷¡£

µ±nginx½ÓÊÕµ½Ò»¸öÁ¬½Óºó£¬¶ÁÈ¡Íê¿Í»§¶Ë·¢ËÍ³öÀ´µÄHeader£¬È»ºó¾Í»á½øĞĞ¸÷¸ö´¦Àí¹ı³ÌµÄµ÷ÓÃ¡£Ö®ºó¾ÍÊÇupstream·¢»Ó×÷ÓÃµÄÊ±ºòÁË£¬
upstreamÔÚ¿Í»§¶Ë¸úºó¶Ë±ÈÈçFCGI/PHPÖ®¼ä£¬½ÓÊÕ¿Í»§¶ËµÄHTTP body£¬·¢ËÍ¸øFCGI£¬È»ºó½ÓÊÕFCGIµÄ½á¹û£¬·¢ËÍ¸ø¿Í»§¶Ë¡£×÷ÎªÒ»¸öÇÅÁºµÄ×÷ÓÃ¡£
Í¬Ê±£¬upstreamÎªÁË³ä·ÖÏÔÊ¾ÆäÁé»îĞÔ£¬ÖÁÓÚºó¶Ë¾ßÌåÊÇÊ²Ã´Ğ­Òé£¬Ê²Ã´ÏµÍ³Ëû¶¼²»care£¬ÎÒÖ»ÊµÏÖÖ÷ÌåµÄ¿ò¼Ü£¬¾ßÌåµ½FCGIĞ­ÒéµÄ·¢ËÍ£¬½ÓÊÕ£¬
½âÎö£¬ÕâĞ©¶¼½»¸øºóÃæµÄ²å¼şÀ´´¦Àí£¬±ÈÈçÓĞfastcgi,memcached,proxyµÈ²å¼ş

http://chenzhenianqing.cn/articles/category/%e5%90%84%e7%a7%8dserver/nginx
upstreamºÍFastCGI memcached  uwsgi  scgi proxyµÄ¹ØÏµ²Î¿¼:http://chenzhenianqing.cn/articles/category/%e5%90%84%e7%a7%8dserver/nginx
*/
ngx_module_t  ngx_http_upstream_module = { //¸ÃÄ£¿éÊÇ·ÃÎÊÉÏÓÎ·şÎñÆ÷Ïà¹ØÄ£¿éµÄ»ù´¡(ÀıÈç FastCGI memcached  uwsgi  scgi proxy¶¼»áÓÃµ½upstreamÄ£¿é  ngx_http_proxy_module  ngx_http_memcached_module)
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
      NGX_HTTP_VAR_NOCACHEABLE, 0 }, //Ç°¶Ë·şÎñÆ÷´¦ÀíÇëÇóµÄ·şÎñÆ÷µØÖ·

    { ngx_string("upstream_status"), NULL,
      ngx_http_upstream_status_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 }, //Ç°¶Ë·şÎñÆ÷µÄÏìÓ¦×´Ì¬¡£

    { ngx_string("upstream_connect_time"), NULL,
      ngx_http_upstream_response_time_variable, 2,
      NGX_HTTP_VAR_NOCACHEABLE, 0 }, 

    { ngx_string("upstream_header_time"), NULL,
      ngx_http_upstream_response_time_variable, 1,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_string("upstream_response_time"), NULL,
      ngx_http_upstream_response_time_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },//Ç°¶Ë·şÎñÆ÷µÄÓ¦´ğÊ±¼ä£¬¾«È·µ½ºÁÃë£¬²»Í¬µÄÓ¦´ğÒÔ¶ººÅºÍÃ°ºÅ·Ö¿ª¡£

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


//ngx_http_upstream_create´´½¨ngx_http_upstream_t£¬×ÊÔ´»ØÊÕÓÃngx_http_upstream_finalize_request
//upstream×ÊÔ´»ØÊÕÔÚngx_http_upstream_finalize_request   ngx_http_XXX_handler(ngx_http_proxy_handler)ÖĞÖ´ĞĞ
ngx_int_t
ngx_http_upstream_create(ngx_http_request_t *r)//´´½¨Ò»¸öngx_http_upstream_t½á¹¹£¬·Åµ½r->upstreamÀïÃæÈ¥¡£
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
    1)µ÷ÓÃngx_http_up stream_init·½·¨Æô¶¯upstream¡£
    2) upstreamÄ£¿é»áÈ¥¼ì²éÎÄ¼ş»º´æ£¬Èç¹û»º´æÖĞÒÑ¾­ÓĞºÏÊÊµÄÏìÓ¦°ü£¬Ôò»áÖ±½Ó·µ»Ø»º´æ£¨µ±È»±ØĞëÊÇÔÚÊ¹ÓÃ·´Ïò´úÀíÎÄ¼ş»º´æµÄÇ°ÌáÏÂ£©¡£
    ÎªÁËÈÃ¶ÁÕß·½±ãµØÀí½âupstream»úÖÆ£¬±¾ÕÂ½«²»ÔÙÌá¼°ÎÄ¼ş»º´æ¡£
    3)»Øµ÷mytestÄ£¿éÒÑ¾­ÊµÏÖµÄcreate_request»Øµ÷·½·¨¡£
    4) mytestÄ£¿éÍ¨¹ıÉèÖÃr->upstream->request_bufsÒÑ¾­¾ö¶¨ºÃ·¢ËÍÊ²Ã´ÑùµÄÇëÇóµ½ÉÏÓÎ·şÎñÆ÷¡£
    5) upstreamÄ£¿é½«»á¼ì²éresolved³ÉÔ±£¬Èç¹ûÓĞresolved³ÉÔ±µÄ»°£¬¾Í¸ù¾İËüÉèÖÃºÃÉÏÓÎ·şÎñÆ÷µÄµØÖ·r->upstream->peer³ÉÔ±¡£
    6)ÓÃÎŞ×èÈûµÄTCPÌ×½Ó×Ö½¨Á¢Á¬½Ó¡£
    7)ÎŞÂÛÁ¬½ÓÊÇ·ñ½¨Á¢³É¹¦£¬¸ºÔğ½¨Á¢Á¬½ÓµÄconnect·½·¨¶¼»áÁ¢¿Ì·µ»Ø¡£
*/
//ngx_http_upstream_init·½·¨½«»á¸ù¾İngx_http_upstream_conf_tÖĞµÄ³ÉÔ±³õÊ¼»¯upstream£¬Í¬Ê±»á¿ªÊ¼Á¬½ÓÉÏÓÎ·şÎñÆ÷£¬ÒÔ´ËÕ¹¿ªÕû¸öupstream´¦ÀíÁ÷³Ì
void ngx_http_upstream_init(ngx_http_request_t *r) //ÔÚ¶ÁÈ¡Íêä¯ÀÀÆ÷·¢ËÍÀ´µÄÇëÇóÍ·²¿×Ö¶Îºó£¬»áÍ¨¹ıproxy fastcgiµÈÄ£¿é¶ÁÈ¡°üÌå£¬¶ÁÈ¡ÍêºóÖ´ĞĞ¸Ãº¯Êı
{
    ngx_connection_t     *c;

    c = r->connection;//µÃµ½¿Í»§¶ËÁ¬½Ó½á¹¹¡£

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http init upstream, client timer: %d", c->read->timer_set);

#if (NGX_HTTP_SPDY)
    if (r->spdy_stream) {
        ngx_http_upstream_init_request(r);
        return;
    }
#endif

    /*
        Ê×ÏÈ¼ì²éÇëÇó¶ÔÓ¦ÓÚ¿Í»§¶ËµÄÁ¬½Ó£¬Õâ¸öÁ¬½ÓÉÏµÄ¶ÁÊÂ¼şÈç¹ûÔÚ¶¨Ê±Æ÷ÖĞ£¬Ò²¾ÍÊÇËµ£¬¶ÁÊÂ¼şµÄtimer_ set±êÖ¾Î»Îª1£¬ÄÇÃ´µ÷ÓÃngx_del_timer
    ·½·¨°ÑÕâ¸ö¶ÁÊÂ¼ş´Ó¶¨Ê±Æ÷ÖĞÒÆ³ı¡£ÎªÊ²Ã´Òª×öÕâ¼şÊÂÄØ£¿ÒòÎªÒ»µ©Æô¶¯upstream»úÖÆ£¬¾Í²»Ó¦¸Ã¶Ô¿Í»§¶ËµÄ¶Á²Ù×÷´øÓĞ³¬Ê±Ê±¼äµÄ´¦Àí(³¬Ê±»á¹Ø±Õ¿Í»§¶ËÁ¬½Ó)£¬
    ÇëÇóµÄÖ÷Òª´¥·¢ÊÂ¼ş½«ÒÔÓëÉÏÓÎ·şÎñÆ÷µÄÁ¬½ÓÎªÖ÷¡£
     */
    if (c->read->timer_set) {
        ngx_del_timer(c->read, NGX_FUNC_LINE);
    }

    if (ngx_event_flags & NGX_USE_CLEAR_EVENT) { //Èç¹ûepollÊ¹ÓÃ±ßÔµ´¥·¢

/*
2025/04/24 05:31:47[             ngx_http_upstream_init,   654]  [debug] 15507#15507: *1 <   ngx_http_upstream_init,   653> epoll NGX_WRITE_EVENT(et) read add
2025/04/24 05:31:47[                ngx_epoll_add_event,  1400]  [debug] 15507#15507: *1 epoll modify read and write event: fd:11 op:3 ev:80002005
025/04/24 05:31:47[           ngx_epoll_process_events,  1624]  [debug] 15507#15507: begin to epoll_wait, epoll timer: 60000 
2025/04/24 05:31:47[           ngx_epoll_process_events,  1709]  [debug] 15507#15507: epoll: fd:11 epoll-out(ev:0004) d:B26A00E8
Êµ¼ÊÉÏÊÇÍ¨¹ıngx_http_upstream_initÖĞµÄmod epoll_ctlÌí¼Ó¶ÁĞ´ÊÂ¼ş´¥·¢µÄ£¬µ±±¾´ÎÑ­»·ÍË»Øµ½ngx_worker_process_cycle ..->ngx_epoll_process_events
µÄÊ±ºò£¬¾Í»á´¥·¢epoll_out,´Ó¶øÖ´ĞĞngx_http_upstream_wr_check_broken_connection
*/
        //ÕâÀïÊµ¼ÊÉÏÊÇ´¥·¢Ö´ĞĞngx_http_upstream_check_broken_connection
        if (!c->write->active) {//ÒªÔö¼Ó¿ÉĞ´ÊÂ¼şÍ¨Öª£¬ÎªÉ¶?ÒòÎª´ı»á¿ÉÄÜ¾ÍÄÜĞ´ÁË,¿ÉÄÜ»á×ª·¢ÉÏÓÎ·şÎñÆ÷µÄÄÚÈİ¸øä¯ÀÀÆ÷µÈ¿Í»§¶Ë
            //Êµ¼ÊÉÏÊÇ¼ì²éºÍ¿Í»§¶ËµÄÁ¬½ÓÊÇ·ñÒì³£ÁË
            char tmpbuf[256];
            
            snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> epoll NGX_WRITE_EVENT(et) write add", NGX_FUNC_LINE);
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

//ngx_http_upstream_initµ÷ÓÃÕâÀï£¬´ËÊ±¿Í»§¶Ë·¢ËÍµÄÊı¾İ¶¼ÒÑ¾­½ÓÊÕÍê±ÏÁË¡£
/*
1. µ÷ÓÃcreate_request´´½¨fcgi»òÕßproxyµÄÊı¾İ½á¹¹¡£
2. µ÷ÓÃngx_http_upstream_connectÁ¬½ÓÏÂÓÎ·şÎñÆ÷¡£
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

    u = r->upstream;//ngx_http_upstream_createÀïÃæÉèÖÃµÄ  ngx_http_XXX_handler(ngx_http_proxy_handler)ÖĞÖ´ĞĞ

#if (NGX_HTTP_CACHE)

    if (u->conf->cache) {
        ngx_int_t  rc;

        int cache = u->conf->cache;
        rc = ngx_http_upstream_cache(r, u);
        ngx_log_debugall(r->connection->log, 0, "ngx http cache, conf->cache:%d, rc:%d", cache, rc);
        
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
    ÉèÖÃNginxÓëÏÂÓÎ¿Í»§¶ËÖ®¼äTCPÁ¬½ÓµÄ¼ì²é·½·¨
    Êµ¼ÊÉÏ£¬ÕâÁ½¸ö·½·¨¶¼»áÍ¨¹ıngx_http_upstream_check_broken_connection·½·¨¼ì²éNginxÓëÏÂÓÎµÄÁ¬½ÓÊÇ·ñÕı³££¬Èç¹û³öÏÖ´íÎó£¬¾Í»áÁ¢¼´ÖÕÖ¹Á¬½Ó¡£
     */
/*
2025/04/24 05:31:47[             ngx_http_upstream_init,   654]  [debug] 15507#15507: *1 <   ngx_http_upstream_init,   653> epoll NGX_WRITE_EVENT(et) read add
2025/04/24 05:31:47[                ngx_epoll_add_event,  1400]  [debug] 15507#15507: *1 epoll modify read and write event: fd:11 op:3 ev:80002005
2025/04/24 05:31:47[           ngx_epoll_process_events,  1624]  [debug] 15507#15507: begin to epoll_wait, epoll timer: 60000 
2025/04/24 05:31:47[           ngx_epoll_process_events,  1709]  [debug] 15507#15507: epoll: fd:11 epoll-out(ev:0004) d:B26A00E8
Êµ¼ÊÉÏÊÇÍ¨¹ıngx_http_upstream_initÖĞµÄmod epoll_ctlÌí¼Ó¶ÁĞ´ÊÂ¼ş´¥·¢µÄ£¬µ±±¾´ÎÑ­»·ÍË»Øµ½ngx_worker_process_cycle ..->ngx_epoll_process_events
µÄÊ±ºò£¬¾Í»á´¥·¢epoll_out,´Ó¶øÖ´ĞĞngx_http_upstream_wr_check_broken_connection
*/
    if (!u->store && !r->post_action && !u->conf->ignore_client_abort) {
        //×¢ÒâÕâÊ±ºòµÄr»¹ÊÇ¿Í»§¶ËµÄÁ¬½Ó£¬ÓëÉÏÓÎ·şÎñÆ÷µÄÁ¬½Ór»¹Ã»ÓĞ½¨Á¢
        r->read_event_handler = ngx_http_upstream_rd_check_broken_connection; //ÉèÖÃ»Øµ÷ĞèÒª¼ì²âÁ¬½ÓÊÇ·ñÓĞÎÊÌâ¡£
        r->write_event_handler = ngx_http_upstream_wr_check_broken_connection;
    }
    

    //ÓĞ½ÓÊÕµ½¿Í»§¶Ë°üÌå£¬Ôò°Ñ°üÌå½á¹¹¸³Öµ¸øu->request_bufs£¬ÔÚºóÃæµÄif (u->create_request(r) != NGX_OK) {»áÓÃµ½
    if (r->request_body) {//¿Í»§¶Ë·¢ËÍ¹ıÀ´µÄPOSTÊı¾İ´æ·ÅÔÚ´Ë,ngx_http_read_client_request_body·ÅµÄ
        u->request_bufs = r->request_body->bufs; //¼ÇÂ¼¿Í»§¶Ë·¢ËÍµÄÊı¾İ£¬ÏÂÃæÔÚcreate_requestµÄÊ±ºò¿½±´µ½·¢ËÍ»º³åÁ´½Ó±íÀïÃæµÄ¡£
    }

    /*
     µ÷ÓÃÇëÇóÖĞngx_http_upstream_t½á¹¹ÌåÀïÓÉÄ³¸öHTTPÄ£¿éÊµÏÖµÄcreate_request·½·¨£¬¹¹Ôì·¢ÍùÉÏÓÎ·şÎñÆ÷µÄÇëÇó
     £¨ÇëÇóÖĞµÄÄÚÈİÊÇÉèÖÃµ½request_bufs»º³åÇøÁ´±íÖĞµÄ£©¡£Èç¹ûcreate_request·½·¨Ã»ÓĞ·µ»ØNGX_OK£¬Ôòupstream½áÊø

     Èç¹ûÊÇFCGI¡£ÏÂÃæ×é½¨ºÃFCGIµÄ¸÷ÖÖÍ·²¿£¬°üÀ¨ÇëÇó¿ªÊ¼Í·£¬ÇëÇó²ÎÊıÍ·£¬ÇëÇóSTDINÍ·¡£´æ·ÅÔÚu->request_bufsÁ´½Ó±íÀïÃæ¡£
	Èç¹ûÊÇProxyÄ£¿é£¬ngx_http_proxy_create_request×é¼ş·´Ïò´úÀíµÄÍ·²¿É¶µÄ,·Åµ½u->request_bufsÀïÃæ
	FastCGI memcached  uwsgi  scgi proxy¶¼»áÓÃµ½upstreamÄ£¿é
     */
    if (u->create_request(r) != NGX_OK) { //ngx_http_XXX_create_request   ngx_http_proxy_create_requestµÈ
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    
    /* »ñÈ¡ngx_http_upstream_t½á¹¹ÖĞÖ÷¶¯Á¬½Ó½á¹¹peerµÄlocal±¾µØµØÖ·ĞÅÏ¢ */
    u->peer.local = ngx_http_upstream_get_local(r, u->conf->local);

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    
    /* ³õÊ¼»¯ngx_http_upstream_t½á¹¹ÖĞ³ÉÔ±outputÏòÏÂÓÎ·¢ËÍÏìÓ¦µÄ·½Ê½ */
    u->output.alignment = clcf->directio_alignment; //
    u->output.pool = r->pool;
    u->output.bufs.num = 1;
    u->output.bufs.size = clcf->client_body_buffer_size;

    if (u->output.output_filter == NULL) {
        //ÉèÖÃ¹ıÂËÄ£¿éµÄ¿ªÊ¼¹ıÂËº¯ÊıÎªwriter¡£Ò²¾ÍÊÇoutput_filter¡£ÔÚngx_output_chain±»µ÷ÓÃÒÑ½øĞĞÊı¾İµÄ¹ıÂË
        u->output.output_filter = ngx_chain_writer; 
        u->output.filter_ctx = &u->writer; //²Î¿¼ngx_chain_writer£¬ÀïÃæ»á½«Êä³öbufÒ»¸ö¸öÁ¬½Óµ½ÕâÀï¡£
    }

    u->writer.pool = r->pool;
    
    /* Ìí¼ÓÓÃÓÚ±íÊ¾ÉÏÓÎÏìÓ¦µÄ×´Ì¬£¬ÀıÈç£º´íÎó±àÂë¡¢°üÌå³¤¶ÈµÈ */
    if (r->upstream_states == NULL) {//Êı×éupstream_states£¬±£ÁôupstreamµÄ×´Ì¬ĞÅÏ¢¡£

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

    cln = ngx_http_cleanup_add(r, 0);//»·ĞÎÁ´±í£¬ÉêÇëÒ»¸öĞÂµÄÔªËØ¡£
    if (cln == NULL) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    cln->handler = ngx_http_upstream_cleanup; //µ±ÇëÇó½áÊøÊ±£¬Ò»¶¨»áµ÷ÓÃngx_http_upstream_cleanup·½·¨
    cln->data = r;//Ö¸ÏòËùÖ¸µÄÇëÇó½á¹¹Ìå¡£
    u->cleanup = &cln->handler;

    /*
    http://www.pagefault.info/?p=251
    È»ºó¾ÍÊÇÕâ¸öº¯Êı×îºËĞÄµÄ´¦Àí²¿·Ö£¬ÄÇ¾ÍÊÇ¸ù¾İupstreamµÄÀàĞÍÀ´½øĞĞ²»Í¬µÄ²Ù×÷£¬ÕâÀïµÄupstream¾ÍÊÇÎÒÃÇÍ¨¹ıXXX_pass´«µİ½øÀ´µÄÖµ£¬
    ÕâÀïµÄupstreamÓĞ¿ÉÄÜÏÂÃæ¼¸ÖÖÇé¿ö¡£ 
    Ngx_http_fastcgi_module.c (src\http\modules):    { ngx_string("fastcgi_pass"),
    Ngx_http_memcached_module.c (src\http\modules):    { ngx_string("memcached_pass"),
    Ngx_http_proxy_module.c (src\http\modules):    { ngx_string("proxy_pass"),
    Ngx_http_scgi_module.c (src\http\modules):    { ngx_string("scgi_pass"),
    Ngx_http_uwsgi_module.c (src\http\modules):    { ngx_string("uwsgi_pass"),
    Ngx_stream_proxy_module.c (src\stream):    { ngx_string("proxy_pass"),
    1 XXX_passÖĞ²»°üº¬±äÁ¿¡£
    2 XXX_pass´«µİµÄÖµ°üº¬ÁËÒ»¸ö±äÁ¿($¿ªÊ¼).ÕâÖÖÇé¿öÒ²¾ÍÊÇËµupstreamµÄurlÊÇ¶¯Ì¬±ä»¯µÄ£¬Òò´ËĞèÒªÃ¿´Î¶¼½âÎöÒ»±é.
    ¶øµÚ¶şÖÖÇé¿öÓÖ·ÖÎª2ÖÖ£¬Ò»ÖÖÊÇÔÚ½øÈëupstreamÖ®Ç°£¬Ò²¾ÍÊÇ upstreamÄ£¿éµÄhandlerÖ®ÖĞÒÑ¾­±»resolveµÄµØÖ·(Çë¿´ngx_http_XXX_evalº¯Êı)£¬
    Ò»ÖÖÊÇÃ»ÓĞ±»resolve£¬´ËÊ±¾ÍĞèÒªupstreamÄ£¿éÀ´½øĞĞresolve¡£½ÓÏÂÀ´µÄ´úÂë¾ÍÊÇ´¦ÀíÕâ²¿·ÖµÄ¶«Î÷¡£
    */
    if (u->resolved == NULL) {//ÉÏÓÎµÄIPµØÖ·ÊÇ·ñ±»½âÎö¹ı£¬ngx_http_fastcgi_handlerµ÷ÓÃngx_http_fastcgi_eval»á½âÎö¡£ ÎªNULLËµÃ÷Ã»ÓĞ½âÎö¹ı£¬Ò²¾ÍÊÇfastcgi_pas xxxÖĞµÄxxx²ÎÊıÃ»ÓĞ±äÁ¿
        uscf = u->conf->upstream; //upstream¸³ÖµÔÚngx_http_fastcgi_pass
        printf("yang test ............. resoleved NULL\n");
    } else { //fastcgi_pass xxxµÄxxxÖĞÓĞ±äÁ¿£¬ËµÃ÷ºó¶Ë·şÎñÆ÷ÊÇ»á¸ù¾İÇëÇó¶¯Ì¬±ä»¯µÄ£¬²Î¿¼ngx_http_fastcgi_handler

#if (NGX_HTTP_SSL)
        u->ssl_name = u->resolved->host;
#endif
    //ngx_http_fastcgi_handler »áµ÷ÓÃ ngx_http_fastcgi_evalº¯Êı£¬½øĞĞfastcgi_pass ºóÃæµÄURLµÄ¼òÎö£¬½âÎö³öunixÓò£¬»òÕßsocket.
         printf("yang test ............. resoleved NO NULL, host:%s\n", u->resolved->host.data);
        // Èç¹ûÒÑ¾­ÊÇipµØÖ·¸ñÊ½ÁË£¬¾Í²»ĞèÒªÔÙ½øĞĞ½âÎö
        if (u->resolved->sockaddr) {//Èç¹ûµØÖ·ÒÑ¾­±»resolve¹ıÁË£¬ÎÒIPµØÖ·£¬´ËÊ±´´½¨round robin peer¾ÍĞĞ

            if (ngx_http_upstream_create_round_robin_peer(r, u->resolved)
                != NGX_OK)
            {
                ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            ngx_http_upstream_connect(r, u);//Á¬½Ó 

            return;
        }

        //ÏÂÃæ¿ªÊ¼²éÕÒÓòÃû£¬ÒòÎªfcgi_passºóÃæ²»ÊÇip:port£¬¶øÊÇurl£»
        host = &u->resolved->host;//»ñÈ¡hostĞÅÏ¢¡£ 
        // ½ÓÏÂÀ´¾ÍÒª¿ªÊ¼²éÕÒÓòÃû

        umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);

        uscfp = umcf->upstreams.elts;

        for (i = 0; i < umcf->upstreams.nelts; i++) {//±éÀúËùÓĞµÄÉÏÓÎÄ£¿é£¬¸ù¾İÆähost½øĞĞ²éÕÒ£¬ÕÒµ½host,portÏàÍ¬µÄ¡£

            uscf = uscfp[i];//ÕÒÒ»¸öIPÒ»ÑùµÄÉÏÁ÷Ä£¿é

            if (uscf->host.len == host->len
                && ((uscf->port == 0 && u->resolved->no_port)
                     || uscf->port == u->resolved->port)
                && ngx_strncasecmp(uscf->host.data, host->data, host->len) == 0)
            {
                goto found;//Õâ¸öhostÕıºÃÏàµÈ
            }
        }

        if (u->resolved->port == 0) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "no port in upstream \"%V\"", host);
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        //Ã»°ì·¨ÁË£¬url²»ÔÚupstreamsÊı×éÀïÃæ£¬Ò²¾ÍÊÇ²»ÊÇÎÒÃÇÅäÖÃµÄ£¬ÄÇÃ´³õÊ¼»¯ÓòÃû½âÎöÆ÷
        temp.name = *host;
        
        // ³õÊ¼»¯ÓòÃû½âÎöÆ÷
        ctx = ngx_resolve_start(clcf->resolver, &temp);//½øĞĞÓòÃû½âÎö£¬´ø»º´æµÄ¡£ÉêÇëÏà¹ØµÄ½á¹¹£¬·µ»ØÉÏÏÂÎÄµØÖ·¡£
        if (ctx == NULL) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        if (ctx == NGX_NO_RESOLVER) {//ÎŞ·¨½øĞĞÓòÃû½âÎö¡£ 
            // ·µ»ØNGX_NO_RESOLVER±íÊ¾ÎŞ·¨½øĞĞÓòÃû½âÎö
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "no resolver defined to resolve %V", host);

            ngx_http_upstream_finalize_request(r, u, NGX_HTTP_BAD_GATEWAY);
            return;
        }
        
        // ÉèÖÃĞèÒª½âÎöµÄÓòÃûµÄÀàĞÍÓëĞÅÏ¢
        ctx->name = *host;
        ctx->handler = ngx_http_upstream_resolve_handler;//ÉèÖÃÓòÃû½âÎöÍê³ÉºóµÄ»Øµ÷º¯Êı¡£
        ctx->data = r;
        ctx->timeout = clcf->resolver_timeout;

        u->resolved->ctx = ctx;

        //¿ªÊ¼ÓòÃû½âÎö£¬Ã»ÓĞÍê³ÉÒ²»á·µ»ØµÄ¡£
        if (ngx_resolve_name(ctx) != NGX_OK) {
            u->resolved->ctx = NULL;
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        return;
        // ÓòÃû»¹Ã»ÓĞ½âÎöÍê³É£¬ÔòÖ±½Ó·µ»Ø
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

    ngx_http_upstream_connect(r, u);//µ÷ÓÃngx_http_upstream_connect·½·¨ÏòÉÏÓÎ·şÎñÆ÷·¢ÆğÁ¬½Ó
}


#if (NGX_HTTP_CACHE)
/*ngx_http_upstream_init_request->ngx_http_upstream_cache ¿Í»§¶Ë»ñÈ¡»º´æ ºó¶ËÓ¦´ğ»ØÀ´Êı¾İºóÔÚngx_http_upstream_send_response->ngx_http_file_cache_create
ÖĞ´´½¨ÁÙÊ±ÎÄ¼ş£¬È»ºóÔÚngx_event_pipe_write_chain_to_temp_file°Ñ¶ÁÈ¡µÄºó¶ËÊı¾İĞ´ÈëÁÙÊ±ÎÄ¼ş£¬×îºóÔÚ
ngx_http_upstream_send_response->ngx_http_upstream_process_request->ngx_http_file_cache_updateÖĞ°ÑÁÙÊ±ÎÄ¼şÄÚÈİrename(Ïàµ±ÓÚmv)µ½proxy_cache_pathÖ¸¶¨
µÄcacheÄ¿Â¼ÏÂÃæ
*/
static ngx_int_t
ngx_http_upstream_cache(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    ngx_int_t               rc;
    ngx_http_cache_t       *c; 
    ngx_http_file_cache_t  *cache;

    c = r->cache;

    if (c == NULL) { /* Èç¹û»¹Î´¸øµ±Ç°ÇëÇó·ÖÅä»º´æÏà¹Ø½á¹¹Ìå( ngx_http_cache_t ) Ê±£¬´´½¨´ËÀàĞÍ×Ö¶Î( r->cache ) ²¢³õÊ¼»¯£º */
        //ÀıÈçproxy |fastcgi _cache_methods  POSTÉèÖÃÖµ»º´æPOSTÇëÇó£¬µ«ÊÇ¿Í»§¶ËÇëÇó·½·¨ÊÇGET£¬ÔòÖ±½Ó·µ»Ø
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

        if (u->create_key(r) != NGX_OK) {////½âÎöxx_cache_key adfaxx ²ÎÊıÖµµ½r->cache->keys
            return NGX_ERROR;
        }

        /* TODO: add keys */

        ngx_http_file_cache_create_key(r); /* Éú³É md5sum(key) ºÍ crc32(key)²¢¼ÆËã `c->header_start` Öµ */

        if (r->cache->header_start + 256 >= u->conf->buffer_size) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "%V_buffer_size %uz is not enough for cache key, "
                          "it should be increased to at least %uz",
                          &u->conf->module, u->conf->buffer_size,
                          ngx_align(r->cache->header_start + 256, 1024));

            r->cache = NULL;
            return NGX_DECLINED;
        }

        u->cacheable = 1;/* Ä¬ÈÏËùÓĞÇëÇóµÄÏìÓ¦½á¹û¶¼ÊÇ¿É±»»º´æµÄ */

        c = r->cache;

        
        /* ºóĞø»á½øĞĞµ÷Õû */
        c->body_start = u->conf->buffer_size; //xxx_buffer_size(fastcgi_buffer_size proxy_buffer_size memcached_buffer_size)
        c->min_uses = u->conf->cache_min_uses; //Proxy_cache_min_uses number Ä¬ÈÏÎª1£¬µ±¿Í»§¶Ë·¢ËÍÏàÍ¬ÇëÇó´ïµ½¹æ¶¨´ÎÊıºó£¬nginx²Å¶ÔÏìÓ¦Êı¾İ½øĞĞ»º´æ£»
        c->file_cache = cache;

        /*
          ¸ù¾İÅäÖÃÎÄ¼şÖĞ ( fastcgi_cache_bypass ) »º´æÈÆ¹ıÌõ¼şºÍÇëÇóĞÅÏ¢£¬ÅĞ¶ÏÊÇ·ñÓ¦¸Ã 
          ¼ÌĞø³¢ÊÔÊ¹ÓÃ»º´æÊı¾İÏìÓ¦¸ÃÇëÇó£º 
          */
        switch (ngx_http_test_predicates(r, u->conf->cache_bypass)) {//ÅĞ¶ÏÊÇ·ñÓ¦¸Ã³å»º´æÖĞÈ¡£¬»¹ÊÇ´Óºó¶Ë·şÎñÆ÷È¡

        case NGX_ERROR:
            return NGX_ERROR;

        case NGX_DECLINED:
            u->cache_status = NGX_HTTP_CACHE_BYPASS;
            return NGX_DECLINED;

        default: /* NGX_OK */ //Ó¦¸Ã´Óºó¶Ë·şÎñÆ÷ÖØĞÂ»ñÈ¡
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
            //Èç¹ûÉèÖÃÁËfastcgi_cache_use_stale updating£¬±íÊ¾ËµËäÈ»¸Ã»º´æÎÄ¼şÊ§Ğ§ÁË£¬ÒÑ¾­ÓĞÆäËû¿Í»§¶ËÇëÇóÔÚ»ñÈ¡ºó¶ËÊı¾İ£¬µ«ÊÇÏÖÔÚ»¹Ã»ÓĞ»ñÈ¡ÍêÕû£¬
            //ÕâÊ±ºò¾Í¿ÉÒÔ°ÑÒÔÇ°¹ıÆÚµÄ»º´æ·¢ËÍ¸øµ±Ç°ÇëÇóµÄ¿Í»§¶Ë
            u->cache_status = rc;
            rc = NGX_OK;

        } else {
            rc = NGX_HTTP_CACHE_STALE;
        }

        break;

    case NGX_OK: //»º´æÕı³£ÃüÖĞ
        u->cache_status = NGX_HTTP_CACHE_HIT;
    }

    switch (rc) {

    case NGX_OK:

        rc = ngx_http_upstream_cache_send(r, u);

        if (rc != NGX_HTTP_UPSTREAM_INVALID_HEADER) {
            return rc;
        }

        break;

    case NGX_HTTP_CACHE_STALE: //±íÊ¾»º´æ¹ıÆÚ£¬¼ûÉÏÃæµÄngx_http_file_cache_open->ngx_http_file_cache_read

        c->valid_sec = 0;
        u->buffer.start = NULL;
        u->cache_status = NGX_HTTP_CACHE_EXPIRED;

        break;

    //Èç¹û·µ»ØÕâ¸ö£¬»á°ÑcachedÖÃ0£¬·µ»Ø³öÈ¥ºóÖ»ÓĞ´Óºó¶Ë´ÓĞÂ»ñÈ¡Êı¾İ
    case NGX_DECLINED: //±íÊ¾»º´æÎÄ¼ş´æÔÚ£¬»ñÈ¡»º´æÎÄ¼şÖĞÇ°ÃæµÄÍ·²¿²¿·Ö¼ì²éÓĞÎÊÌâ£¬Ã»ÓĞÍ¨¹ı¼ì²é¡£»òÕß»º´æÎÄ¼ş²»´æÔÚ(µÚÒ»´ÎÇëÇó¸Ãuri»òÕßÃ»ÓĞ´ïµ½¿ªÊ¼»º´æµÄÇëÇó´ÎÊı)

        if ((size_t) (u->buffer.end - u->buffer.start) < u->conf->buffer_size) {
            u->buffer.start = NULL;

        } else {
            u->buffer.pos = u->buffer.start + c->header_start;
            u->buffer.last = u->buffer.pos;
        }

        break;

    case NGX_HTTP_CACHE_SCARCE: //Ã»ÓĞ´ïµ½ÇëÇó´ÎÊı£¬Ö»ÓĞ´ïµ½ÇëÇó´ÎÊı²Å»á»º´æ

        u->cacheable = 0;//ÕâÀïÖÃ0£¬¾ÍÊÇËµÈç¹ûÅäÖÃ5´Î¿ªÊ¼»º´æ£¬ÔòÇ°Ãæ4´Î¶¼²»»á»º´æ£¬°ÑcacheableÖÃ0¾Í²»»á»º´æÁË

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
    //»ñÈ¡proxy_cacheÉèÖÃµÄ¹²ÏíÄÚ´æ¿éÃû£¬Ö±½Ó·µ»Øu->conf->cache_zone->data(Õâ¸öÊÇÔÚproxy_cache_path fastcgi_cache_pathÉèÖÃµÄ)£¬Òò´Ë±ØĞëÍ¬Ê±ÉèÖÃ
    //proxy_cacheºÍproxy_cache_path
        *cache = u->conf->cache_zone->data;
        ngx_log_debugall(r->connection->log, 0, "ngx http upstream cache get use keys_zone:%V", &u->conf->cache_zone->shm.name);
        return NGX_OK;
    }

    //ËµÃ÷proxy_cache xxx$ssÖĞ´øÓĞ²ÎÊı
    if (ngx_http_complex_value(r, u->conf->cache_value, &val) != NGX_OK) {
        return NGX_ERROR;
    }

    if (val.len == 0
        || (val.len == 3 && ngx_strncmp(val.data, "off", 3) == 0))
    {
        return NGX_DECLINED;
    }

    caches = u->caches->elts; //ÔÚproxy_cache_pathÉèÖÃµÄzone_keyÖĞ²éÕÒÓĞÃ»ÓĞ¶ÔÓ¦µÄ¹²ÏíÄÚ´æÃû//keys_zone=fcgi:10mÖĞµÄfcgi

    for (i = 0; i < u->caches->nelts; i++) {//ÔÚu->cachesÖĞ²éÕÒproxy_cache»òÕßfastcgi_cache xxx$ss½âÎö³öµÄxxx$ss×Ö·û´®£¬ÊÇ·ñÓĞÏàÍ¬µÄ
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

    /*
    root@root:/var/yyz# cat cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f   ·â°ü¹ı³Ì¼ûngx_http_file_cache_set_header
     3hwhdBw
     KEY: /test2.php
     
     X-Powered-By: PHP/5.2.13
     Content-type: text/html
    //body_start¾ÍÊÇÉÏÃæÕâÒ»¶ÎÄÚ´æÄÚÈİ³¤¶È
    
     <Html> 
     <title>file update</title>
     <body> 
     <form method="post" action="" enctype="multipart/form-data">
     <input type="file" name="file" /> 
     <input type="submit" value="submit" /> 
     </form> 
     </body> 
     </html>
    
     ×¢ÒâµÚÈıĞĞÄÄÀïÆäÊµÓĞ8×Ö½ÚµÄfastcgi±íÊ¾Í·²¿½á¹¹ngx_http_fastcgi_header_t£¬Í¨¹ıvi cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f¿ÉÒÔ¿´³ö
    
     offset    0  1  2  3   4  5  6  7   8  9  a  b   c  d  e  f  0123456789abcdef
    00000000 <03>00 00 00  ab 53 83 56  ff ff ff ff  2b 02 82 56  ....«S.V+..V
    00000010  64 42 77 17  00 00 91 00  ce 00 00 00  00 00 00 00  dBw...........
    00000020  00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  ................
    00000030  00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  ................
    00000040  00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  ................
    00000050  00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  ................
    00000060  00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  ................
    00000070  00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  ................
    00000080  0a 4b 45 59  3a 20 2f 74  65 73 74 32  2e 70 68 70  .KEY: /test2.php
    00000090  0a 01 06 00  01 01 0c 04  00 58 2d 50  6f 77 65 72  .........X-Power
    000000a0  65 64 2d 42  79 3a 20 50  48 50 2f 35  2e 32 2e 31  ed-By: PHP/5.2.1
    000000b0  33 0d 0a 43  6f 6e 74 65  6e 74 2d 74  79 70 65 3a  3..Content-type:
    000000c0  20 74 65 78  74 2f 68 74  6d 6c 0d 0a  0d 0a 3c 48   text/html....<H
    000000d0  74 6d 6c 3e  20 0d 0a 3c  74 69 74 6c  65 3e 66 69  tml> ..<title>fi
    000000e0  6c 65 20 75  70 64 61 74  65 3c 2f 74  69 74 6c 65  le update</title
    000000f0  3e 0d 0a 3c  62 6f 64 79  3e 20 0d 0a  3c 66 6f 72  >..<body> ..<for
    
     offset    0  1  2  3   4  5  6  7   8  9  a  b   c  d  e  f  0123456789abcdef
    00000100  6d 20 6d 65  74 68 6f 64  3d 22 70 6f  73 74 22 20  m method="post"
    00000110  61 63 74 69  6f 6e 3d 22  22 20 65 6e  63 74 79 70  action="" enctyp
    00000120  65 3d 22 6d  75 6c 74 69  70 61 72 74  2f 66 6f 72  e="multipart/for
    00000130  6d 2d 64 61  74 61 22 3e  0d 0a 3c 69  6e 70 75 74  m-data">..<input
    00000140  20 74 79 70  65 3d 22 66  69 6c 65 22  20 6e 61 6d   type="file" nam
    00000150  65 3d 22 66  69 6c 65 22  20 2f 3e 20  0d 0a 3c 69  e="file" /> ..<i
    00000160  6e 70 75 74  20 74 31 31  31 31 31 31  31 31 31 31  nput t1111111111
    00000170  31 31 31 31  31 31 31 31  31 31 31 31  31 31 31 31  1111111111111111
    00000180  31 31 31 31  31 31 31 31  31 31 31 31  31 31 31 31  1111111111111111
    00000190  31 31 31 31  31 31 31 31  31 31 31 31  31 31 31 31  1111111111111111
    000001a0  31 31 31 31  31 31 31 31  31 31 31 31  31 79 70 65  1111111111111ype
    000001b0  3d 22 73 75  62 6d 69 74  22 20 76 61  6c 75 65 3d  ="submit" value=
    000001c0  22 73 75 62  6d 69 74 22  20 2f 3e 20  0d 0a 3c 2f  "submit" /> ..</
    000001d0  66 6f 72 6d  3e 20 0d 0a  3c 2f 62 6f  64 79 3e 20  form> ..</body>
    000001e0  0d 0a 3c 2f  68 74 6d 6c  3e 20 0d 0a               ..</html> ..
    
    
    header_start: [ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_keyÖĞµÄKEY]["\n"] Ò²¾ÍÊÇÉÏÃæµÄµÚÒ»ĞĞºÍµÚ¶şĞĞ
    body_start: [ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_keyÖĞµÄKEY]["\n"][header]Ò²¾ÍÊÇÉÏÃæµÄµÚÒ»µ½µÚÎåĞĞÄÚÈİ
    Òò´Ë:body_start = header_start + [header]²¿·Ö(ÀıÈçfastcgi·µ»ØµÄÍ·²¿ĞĞ±êÊ¶²¿·Ö)
         */ 

    if (c->header_start == c->body_start) {
        r->http_version = NGX_HTTP_VERSION_9;
        return ngx_http_cache_send(r);
    }

    /* TODO: cache stack */
    //ngx_http_file_cache_open->ngx_http_file_cache_readÖĞc->buf->lastÖ¸ÏòÁË¶ÁÈ¡µ½µÄÊı¾İµÄÄ©Î²
    u->buffer = *c->buf;
//Ö¸Ïò[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_keyÖĞµÄKEY]["\n"][header]ÖĞµÄ[header]¿ªÊ¼´¦£¬Ò²¾ÍÊÇÇ°ÃæµÄ"X-Powered-By: PHP/5.2.13"
    u->buffer.pos += c->header_start; //Ö¸Ïòºó¶Ë·µ»Ø¹ıÀ´µÄÊı¾İ¿ªÊ¼´¦(ºó¶Ë·µ»ØµÄÔ­Ê¼Í·²¿ĞĞ+ÍøÒ³°üÌåÊı¾İ)

    ngx_memzero(&u->headers_in, sizeof(ngx_http_upstream_headers_in_t));
    u->headers_in.content_length_n = -1;
    u->headers_in.last_modified_time = -1;

    if (ngx_list_init(&u->headers_in.headers, r->pool, 8,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    rc = u->process_header(r); //°Ñºó¶Ë·µ»Ø¹ıÀ´µÄÍ·²¿ĞĞĞÅÏ¢³öÈ¥fastcgiÍ·²¿8×Ö½ÚÒÔÍâµÄÊı¾İ²¿·Öµ½

    if (rc == NGX_OK) {
        //°Ñºó¶ËÍ·²¿ĞĞÖĞµÄÏà¹ØÊı¾İ½âÎöµ½u->headers_inÖĞ
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

//¿Í»§¶ËÊÂ¼ş´¦ÀíhandlerÒ»°ã(write(read)->handler)Ò»°ãÎªngx_http_request_handler£¬ ºÍºó¶ËµÄhandlerÒ»°ã(write(read)->handler)Ò»°ãÎªngx_http_upstream_handler£¬ ºÍºó¶ËµÄ
//ºÍºó¶Ë·şÎñÆ÷µÄ¶ÁĞ´ÊÂ¼ş´¥·¢ºó×ßµ½ÕâÀï
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

    //µ±evÎªngx_connection_t->write Ä¬ÈÏwriteÎª1£»µ±evÎªngx_connection_t->read Ä¬ÈÏwriteÎª0
    if (ev->write) { //ËµÃ÷ÊÇc->writeÊÂ¼ş
        u->write_event_handler(r, u);//ngx_http_upstream_send_request_handler

    } else { //ËµÃ÷ÊÇc->readÊÂ¼ş
        u->read_event_handler(r, u); //ngx_http_upstream_process_header ngx_http_upstream_process_non_buffered_upstream
        
    }

    ngx_http_run_posted_requests(c);
}

/*
2025/04/24 05:31:47[             ngx_http_upstream_init,   654]  [debug] 15507#15507: *1 <   ngx_http_upstream_init,   653> epoll NGX_WRITE_EVENT(et) read add
2025/04/24 05:31:47[                ngx_epoll_add_event,  1400]  [debug] 15507#15507: *1 epoll modify read and write event: fd:11 op:3 ev:80002005
025/04/24 05:31:47[           ngx_epoll_process_events,  1624]  [debug] 15507#15507: begin to epoll_wait, epoll timer: 60000 
2025/04/24 05:31:47[           ngx_epoll_process_events,  1709]  [debug] 15507#15507: epoll: fd:11 epoll-out(ev:0004) d:B26A00E8
Êµ¼ÊÉÏÊÇÍ¨¹ıngx_http_upstream_initÖĞµÄmod epoll_ctlÌí¼Ó¶ÁĞ´ÊÂ¼ş´¥·¢µÄ
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
Êµ¼ÊÉÏÊÇÍ¨¹ıngx_http_upstream_initÖĞµÄmod epoll_ctlÌí¼Ó¶ÁĞ´ÊÂ¼ş´¥·¢µÄ
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
Êµ¼ÊÉÏÊÇÍ¨¹ıngx_http_upstream_initÖĞµÄmod epoll_ctlÌí¼Ó¶ÁĞ´ÊÂ¼ş´¥·¢µÄ£¬µ±±¾´ÎÑ­»·ÍË»Øµ½ngx_worker_process_cycle ..->ngx_epoll_process_events
µÄÊ±ºò£¬¾Í»á´¥·¢epoll_out,´Ó¶øÖ´ĞĞngx_http_upstream_wr_check_broken_connection
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
upstream»úÖÆÓëÉÏÓÎ·şÎñÆ÷ÊÇÍ¨¹ıTCP½¨Á¢Á¬½ÓµÄ£¬ÖÚËùÖÜÖª£¬½¨Á¢TCPÁ¬½ÓĞèÒªÈı´ÎÎÕÊÖ£¬¶øÈı´ÎÎÕÊÖÏûºÄµÄÊ±¼äÊÇ²»¿É¿ØµÄ¡£ÎªÁË±£Ö¤½¨Á¢TCP
Á¬½ÓÕâ¸ö²Ù×÷²»»á×èÈû½ø³Ì£¬NginxÊ¹ÓÃÎŞ×èÈûµÄÌ×½Ó×ÖÀ´Á¬½ÓÉÏÓÎ·şÎñÆ÷¡£µ÷ÓÃµÄngx_http_upstream_connect·½·¨¾ÍÊÇÓÃÀ´Á¬½ÓÉÏÓÎ·şÎñÆ÷µÄ£¬
ÓÉÓÚÊ¹ÓÃÁË·Ç×èÈûµÄÌ×½Ó×Ö£¬µ±·½·¨·µ»ØÊ±ÓëÉÏÓÎÖ®¼äµÄTCPÁ¬½ÓÎ´±Ø»á³É¹¦½¨Á¢£¬¿ÉÄÜ»¹ĞèÒªµÈ´ıÉÏÓÎ·şÎñÆ÷·µ»ØTCPµÄSYN/ACK°ü¡£Òò´Ë£¬
ngx_http_upstream_connect·½·¨Ö÷Òª¸ºÔğ·¢Æğ½¨Á¢Á¬½ÓÕâ¸ö¶¯×÷£¬Èç¹ûÕâ¸ö·½·¨Ã»ÓĞÁ¢¿Ì·µ»Ø³É¹¦£¬ÄÇÃ´ĞèÒªÔÚepollÖĞ¼à¿ØÕâ¸öÌ×½Ó×Ö£¬µ±
Ëü³öÏÖ¿ÉĞ´ÊÂ¼şÊ±£¬¾ÍËµÃ÷Á¬½ÓÒÑ¾­½¨Á¢³É¹¦ÁË¡£

//µ÷ÓÃsocket,connectÁ¬½ÓÒ»¸öºó¶ËµÄpeer,È»ºóÉèÖÃ¶ÁĞ´ÊÂ¼ş»Øµ÷º¯Êı£¬½øÈë·¢ËÍÊı¾İµÄngx_http_upstream_send_requestÀïÃæ
//ÕâÀï¸ºÔğÁ¬½Óºó¶Ë·şÎñ£¬È»ºóÉèÖÃ¸÷¸ö¶ÁĞ´ÊÂ¼ş»Øµ÷¡£×îºóÈç¹ûÁ¬½Ó½¨Á¢³É¹¦£¬»áµ÷ÓÃngx_http_upstream_send_request½øĞĞÊı¾İ·¢ËÍ¡£
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
    //³õÊ¼¸³Öµ¼ûngx_http_upstream_connect->ngx_event_connect_peer(&u->peer);
    //¿ÉÒÔ¿´³öÓĞ¶àÉÙ¸ö¿Í»§¶ËÁ¬½Ó£¬nginx¾ÍÒªÓëphp·şÎñÆ÷½¨Á¢¶àÉÙ¸öÁ¬½Ó£¬ÎªÊ²Ã´nginxºÍphp·şÎñÆ÷²»Ö»½¨Á¢Ò»¸öÁ¬½ÓÄØ????????????????
    rc = ngx_event_connect_peer(&u->peer); //½¨Á¢Ò»¸öTCPÌ×½Ó×Ö£¬Í¬Ê±£¬Õâ¸öÌ×½Ó×ÖĞèÒªÉèÖÃÎª·Ç×èÈûÄ£Ê½¡£

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream connect: %i", rc);

    if (rc == NGX_ERROR) {//
    //Èô rc = NGX_ERROR£¬±íÊ¾·¢ÆğÁ¬½ÓÊ§°Ü£¬Ôòµ÷ÓÃngx_http_upstream_finalize_request ·½·¨¹Ø±ÕÁ¬½ÓÇëÇó£¬²¢ return ´Óµ±Ç°º¯Êı·µ»Ø£»
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    u->state->peer = u->peer.name;

    if (rc == NGX_BUSY) {
    //Èô rc = NGX_BUSY£¬±íÊ¾µ±Ç°ÉÏÓÎ·şÎñÆ÷´¦ÓÚ²»»îÔ¾×´Ì¬£¬Ôòµ÷ÓÃ ngx_http_upstream_next ·½·¨¸ù¾İ´«ÈëµÄ²ÎÊı³¢ÊÔÖØĞÂ·¢ÆğÁ¬½ÓÇëÇó£¬²¢ return ´Óµ±Ç°º¯Êı·µ»Ø£»
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "no live upstreams");
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_NOLIVE);
        return;
    }

    if (rc == NGX_DECLINED) {
    //Èô rc = NGX_DECLINED£¬±íÊ¾µ±Ç°ÉÏÓÎ·şÎñÆ÷¸ºÔØ¹ıÖØ£¬Ôòµ÷ÓÃ ngx_http_upstream_next ·½·¨³¢ÊÔÓëÆäËûÉÏÓÎ·şÎñÆ÷½¨Á¢Á¬½Ó£¬²¢ return ´Óµ±Ç°º¯Êı·µ»Ø£»
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
        return;
    }

    /* rc == NGX_OK || rc == NGX_AGAIN || rc == NGX_DONE */

    c = u->peer.connection;

    c->data = r;
/*
ÉèÖÃÉÏÓÎÁ¬½Ó ngx_connection_t ½á¹¹ÌåµÄ¶ÁÊÂ¼ş¡¢Ğ´ÊÂ¼şµÄ»Øµ÷·½·¨ handler ¶¼Îª ngx_http_upstream_handler£¬ÉèÖÃ ngx_http_upstream_t 
½á¹¹ÌåµÄĞ´ÊÂ¼ş write_event_handler µÄ»Øµ÷Îª ngx_http_upstream_send_request_handler£¬¶ÁÊÂ¼ş read_event_handler µÄ»Øµ÷·½·¨Îª 
ngx_http_upstream_process_header£»
*/
    c->write->handler = ngx_http_upstream_handler; 
    c->read->handler = ngx_http_upstream_handler;

    //ÕâÒ»²½ÖèÊµ¼ÊÉÏ¾ö¶¨ÁËÏòÉÏÓÎ·şÎñÆ÷·¢ËÍÇëÇóµÄ·½·¨ÊÇngx_http_upstream_send_request_handler.
//ÓÉĞ´ÊÂ¼ş(Ğ´Êı¾İ»òÕß¿Í»§¶ËÁ¬½Ó·µ»Ø³É¹¦)´¥·¢c->write->handler = ngx_http_upstream_handler;È»ºóÔÚngx_http_upstream_handlerÖĞÖ´ĞĞngx_http_upstream_send_request_handler
    u->write_event_handler = ngx_http_upstream_send_request_handler; //Èç¹ûngx_event_connect_peer·µ»ØNGX_AGAINÒ²Í¨¹ı¸Ãº¯Êı´¥·¢Á¬½Ó³É¹¦
//ÉèÖÃupstream»úÖÆµÄread_event_handler·½·¨Îªngx_http_upstream_process_header£¬Ò²¾ÍÊÇÓÉngx_http_upstream_process_header·½·¨½ÓÊÕÉÏÓÎ·şÎñÆ÷µÄÏìÓ¦¡£
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
    //¿Í»§¶Ë°üÌå´æÈëÁËÁÙÊ±ÎÄ¼şºó£¬ÔòÊ¹ÓÃr->request_body->bufsÁ´±íÖĞµÄngx_buf_t½á¹¹µÄfile_posºÍfile_lastÖ¸Ïò£¬ËùÒÔr->request_body->buf¿ÉÒÔ¼ÌĞø¶ÁÈ¡°üÌå
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
    
    if (rc == NGX_AGAIN) { //ÕâÀïµÄ¶¨Ê±Æ÷ÔÚngx_http_upstream_send_request»áÉ¾³ı
            /*
            2025/04/24 02:54:29[             ngx_event_connect_peer,    32]  [debug] 14867#14867: *1 socket 12
2025/04/24 02:54:29[           ngx_epoll_add_connection,  1486]  [debug] 14867#14867: *1 epoll add connection: fd:12 ev:80002005
2025/04/24 02:54:29[             ngx_event_connect_peer,   125]  [debug] 14867#14867: *1 connect to 127.0.0.1:3666, fd:12 #2
2025/04/24 02:54:29[          ngx_http_upstream_connect,  1549]  [debug] 14867#14867: *1 http upstream connect: -2 //·µ»ØNGX_AGAIN
2025/04/24 02:54:29[                ngx_event_add_timer,    88]  [debug] 14867#14867: *1 <ngx_http_upstream_connect,  1665>  event timer add: 12: 60000:1677807811 //ÕâÀïÌí¼Ó
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
2025/04/24 02:54:29[                ngx_event_del_timer,    39]  [debug] 14867#14867: *1 <ngx_http_upstream_send_request,  2052>  event timer del: 12: 1677807811//ÕâÀïÉ¾³ı
2025/04/24 02:54:29[                ngx_event_add_timer,    88]  [debug] 14867#14867: *1 <ngx_http_upstream_send_request,  2075>  event timer add: 12: 60000:1677807813
           */
        /*
          Èô rc = NGX_AGAIN£¬±íÊ¾µ±Ç°ÒÑ¾­·¢ÆğÁ¬½Ó£¬µ«ÊÇÃ»ÓĞÊÕµ½ÉÏÓÎ·şÎñÆ÷µÄÈ·ÈÏÓ¦´ğ±¨ÎÄ£¬¼´ÉÏÓÎÁ¬½ÓµÄĞ´ÊÂ¼ş²»¿ÉĞ´£¬ÔòĞèµ÷ÓÃ ngx_add_timer 
          ·½·¨½«ÉÏÓÎÁ¬½ÓµÄĞ´ÊÂ¼şÌí¼Óµ½¶¨Ê±Æ÷ÖĞ£¬¹ÜÀí³¬Ê±È·ÈÏÓ¦´ğ£»
            
          ÕâÒ»²½´¦Àí·Ç×èÈûµÄÁ¬½ÓÉĞÎ´³É¹¦½¨Á¢Ê±µÄ¶¯×÷¡£Êµ¼ÊÉÏ£¬ÔÚngx_event_connect_peerÖĞ£¬Ì×½Ó×ÖÒÑ¾­¼ÓÈëµ½epollÖĞ¼à¿ØÁË£¬Òò´Ë£¬
          ÕâÒ»²½½«µ÷ÓÃngx_add_timer·½·¨°ÑĞ´ÊÂ¼şÌí¼Óµ½¶¨Ê±Æ÷ÖĞ£¬³¬Ê±Ê±¼äÎªngx_http_upstream_conf_t½á¹¹ÌåÖĞµÄconnect_timeout
          ³ÉÔ±£¬ÕâÊÇÔÚÉèÖÃ½¨Á¢TCPÁ¬½ÓµÄ³¬Ê±Ê±¼ä¡£
          */ //ÕâÀïµÄ¶¨Ê±Æ÷ÔÚngx_http_upstream_send_request»áÉ¾³ı
        ngx_add_timer(c->write, u->conf->connect_timeout, NGX_FUNC_LINE);
        return; //´ó²¿·ÖÇé¿öÍ¨¹ıÕâÀï·µ»Ø£¬È»ºóÍ¨¹ıngx_http_upstream_send_request_handlerÀ´Ö´ĞĞepoll writeÊÂ¼ş
    }

    
//Èô rc = NGX_OK£¬±íÊ¾³É¹¦½¨Á¢Á¬½Ó£¬Ôòµ÷ÓÃ ngx_http_upsream_send_request ·½·¨ÏòÉÏÓÎ·şÎñÆ÷·¢ËÍÇëÇó£»
#if (NGX_HTTP_SSL)

    if (u->ssl && c->ssl == NULL) {
        ngx_http_upstream_ssl_init_connection(r, u, c);
        return;
    }

#endif

    //Èç´ôÒÑ¾­³É¹¦½¨Á¢Á¬½Ó£¬Ôòµ÷ÓÃngx_http_upstream_send_request·½·¨ÏòÉÏÓÎ·şÎñÆ÷·¢ËÍÇëÇó
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
    ngx_uint_t do_write) //ÏòÉÏÓÎ·şÎñÆ÷·¢ËÍÇëÇó   µ±Ò»´Î·¢ËÍ²»Íê£¬Í¨¹ıngx_http_upstream_send_request_handlerÔÙ´Î´¥·¢·¢ËÍ
{
    ngx_int_t          rc;
    ngx_connection_t  *c;

    c = u->peer.connection; //ÏòÉÏÓÎ·şÎñÆ÷µÄÁ¬½ÓĞÅÏ¢

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream send request");

    if (u->state->connect_time == (ngx_msec_t) -1) {
        u->state->connect_time = ngx_current_msec - u->state->response_time;
    }

    //Í¨¹ıgetsockopt²âÊÔÓëÉÏÓÎ·şÎñÆ÷µÄtcpÁ¬½ÓÊÇ·ñÒì³£
    if (!u->request_sent && ngx_http_upstream_test_connect(c) != NGX_OK) { //²âÊÔÁ¬½ÓÊ§°Ü
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);//Èç¹û²âÊÔÊ§°Ü£¬µ÷ÓÃngx_http_upstream_nextº¯Êı£¬Õâ¸öº¯Êı¿ÉÄÜÔÙ´Îµ÷ÓÃpeer.getµ÷ÓÃ±ğµÄÁ¬½Ó¡£
        return;
    }

    c->log->action = "sending request to upstream";

    rc = ngx_http_upstream_send_request_body(r, u, do_write);

    if (rc == NGX_ERROR) {
        /*  Èô·µ»ØÖµrc=NGX_ERROR£¬±íÊ¾µ±Ç°Á¬½ÓÉÏ³ö´í£¬ ½«´íÎóĞÅÏ¢´«µİ¸øngx_http_upstream_next·½·¨£¬ ¸Ã·½·¨¸ù¾İ´íÎóĞÅÏ¢¾ö¶¨
        ÊÇ·ñÖØĞÂÏòÉÏÓÎÆäËû·şÎñÆ÷·¢ÆğÁ¬½Ó£» ²¢return´Óµ±Ç°º¯Êı·µ»Ø£» */
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
        return;
    }

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        ngx_http_upstream_finalize_request(r, u, rc);
        return;
    }

    /* 
         Èô·µ»ØÖµrc = NGX_AGAIN£¬±íÊ¾ÇëÇóÊı¾İ²¢Î´ÍêÈ«·¢ËÍ£¬ ¼´ÓĞÊ£ÓàµÄÇëÇóÊı¾İ±£´æÔÚoutputÖĞ£¬µ«´ËÊ±£¬Ğ´ÊÂ¼şÒÑ¾­²»¿ÉĞ´£¬ 
         Ôòµ÷ÓÃngx_add_timer·½·¨°Ñµ±Ç°Á¬½ÓÉÏµÄĞ´ÊÂ¼şÌí¼Óµ½¶¨Ê±Æ÷»úÖÆ£¬ ²¢µ÷ÓÃngx_handle_write_event·½·¨½«Ğ´ÊÂ¼ş×¢²áµ½epollÊÂ¼ş»úÖÆÖĞ£» 
     */ //Í¨¹ıngx_http_upstream_read_request_handler½øĞĞÔÙ´Îepoll write
    if (rc == NGX_AGAIN) {//Ğ­ÒéÕ»»º³åÇøÒÑÂú£¬ĞèÒªµÈ´ı·¢ËÍÊı¾İ³öÈ¥ºó³ö·¢epoll¿ÉĞ´£¬´Ó¶ø¼ÌĞøwrite
        if (!c->write->ready) {  
        //ÕâÀï¼Ó¶¨Ê±Æ÷µÄÔ­ÒòÊÇ£¬ÀıÈçÎÒ°ÑÊı¾İÈÓµ½Ğ­ÒéÕ»ÁË£¬²¢ÇÒĞ­ÒéÕ»ÒÑ¾­ÂúÁË£¬µ«ÊÇ¶Ô·½¾ÍÊÇ²»½ÓÊÜÊı¾İ£¬Ôì³ÉÊı¾İÒ»Ö±ÔÚĞ­ÒéÕ»»º´æÖĞ
        //Òò´ËÖ»ÒªÊı¾İ·¢ËÍ³öÈ¥£¬¾Í»á´¥·¢epoll¼ÌĞøĞ´£¬´Ó¶øÔÚÏÂÃæÁ½ĞĞÉ¾³ıĞ´³¬Ê±¶¨Ê±Æ÷
            ngx_add_timer(c->write, u->conf->send_timeout, NGX_FUNC_LINE); 
            //Èç¹û³¬Ê±»áÖ´ĞĞngx_http_upstream_send_request_handler£¬ÕâÀïÃæ¶ÔĞ´³¬Ê±½øĞĞ´¦Àí

        } else if (c->write->timer_set) { //ÀıÈçngx_http_upstream_send_request_body·¢ËÍÁËÈı´Î·µ»ØNGX_AGAIN,ÄÇÃ´µÚ¶ş´Î¾ÍĞèÒª°ÑµÚÒ»´ÎÉÏÃæµÄ³¬Ê±¶¨Ê±Æ÷¹ØÁË£¬±íÊ¾·¢ËÍÕı³£
            ngx_del_timer(c->write, NGX_FUNC_LINE);
        }

        //ÔÚÁ¬½Óºó¶Ë·şÎñÆ÷conncetÇ°£¬ÓĞÉèÖÃngx_add_conn£¬ÀïÃæÒÑ¾­½«fdÌí¼Óµ½ÁË¶ÁĞ´ÊÂ¼şÖĞ£¬Òò´ËÕâÀïÊµ¼ÊÉÏÖ»ÊÇ¼òµ¥Ö´ĞĞÏÂngx_send_lowat
        if (ngx_handle_write_event(c->write, u->conf->send_lowat, NGX_FUNC_LINE) != NGX_OK) {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        return;
    }

    /* rc == NGX_OK */ 
    //Ïòºó¶ËµÄÊı¾İ·¢ËÍÍê±Ï

    //µ±·¢Íùºó¶Ë·şÎñÆ÷µÄÊı¾İ°ü¹ı´ó£¬ĞèÒª·Ö¶à´Î·¢ËÍµÄÊ±ºò£¬ÔÚÉÏÃæµÄif (rc == NGX_AGAIN)ÖĞ»áÌí¼Ó¶¨Ê±Æ÷À´´¥·¢·¢ËÍ£¬Èç¹ûĞ­ÒéÕ»Ò»Ö±²»·¢ËÍÊı¾İ³öÈ¥
    //¾Í»á³¬Ê±£¬Èç¹ûÊı¾İ×îÖÕÈ«²¿·¢ËÍ³öÈ¥ÔòĞèÒªÎª×îºóÒ»´Îtime_writeÌí¼ÓÉ¾³ı²Ù×÷¡£

    //Èç¹û·¢Íùºó¶ËµÄÊı¾İ³¤¶ÈºóĞ¡£¬ÔòÒ»°ã²»»áÔÙÉÏÃÅÌí¼Ó¶¨Ê±Æ÷£¬ÕâÀïµÄtimer_set¿Ï¶¨Îª0£¬ËùÒÔÈç¹û°Îµôºó¶ËÍøÏß£¬Í¨¹ıngx_http_upstream_test_connect
    //ÊÇÅĞ¶Ï²»³öºó¶Ë·şÎñÆ÷µôÏßµÄ£¬ÉÏÃæµÄngx_http_upstream_send_request_body»¹ÊÇ»á·µ»Ø³É¹¦µÄ£¬ËùÒÔÕâÀïÓĞ¸öbug
    if (c->write->timer_set) { //ÕâÀïµÄ¶¨Ê±Æ÷ÊÇngx_http_upstream_connectÖĞconnect·µ»ØNGX_AGAINµÄÊ±ºòÌí¼ÓµÄ¶¨Ê±Æ÷
        /*
2025/04/24 02:54:29[             ngx_event_connect_peer,    32]  [debug] 14867#14867: *1 socket 12
2025/04/24 02:54:29[           ngx_epoll_add_connection,  1486]  [debug] 14867#14867: *1 epoll add connection: fd:12 ev:80002005
2025/04/24 02:54:29[             ngx_event_connect_peer,   125]  [debug] 14867#14867: *1 connect to 127.0.0.1:3666, fd:12 #2
2025/04/24 02:54:29[          ngx_http_upstream_connect,  1549]  [debug] 14867#14867: *1 http upstream connect: -2 //·µ»ØNGX_AGAIN
2025/04/24 02:54:29[                ngx_event_add_timer,    88]  [debug] 14867#14867: *1 <ngx_http_upstream_connect,  1665>  event timer add: 12: 60000:1677807811 //ÕâÀïÌí¼Ó
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
2025/04/24 02:54:29[                ngx_event_del_timer,    39]  [debug] 14867#14867: *1 <ngx_http_upstream_send_request,  2052>  event timer del: 12: 1677807811//ÕâÀïÉ¾³ı
2025/04/24 02:54:29[                ngx_event_add_timer,    88]  [debug] 14867#14867: *1 <ngx_http_upstream_send_request,  2075>  event timer add: 12: 60000:1677807813
           */
        ngx_del_timer(c->write, NGX_FUNC_LINE);
    }

    /* Èô·µ»ØÖµ rc = NGX_OK£¬±íÊ¾ÒÑ¾­·¢ËÍÍêÈ«²¿ÇëÇóÊı¾İ£¬ ×¼±¸½ÓÊÕÀ´×ÔÉÏÓÎ·şÎñÆ÷µÄÏìÓ¦±¨ÎÄ£¬ÔòÖ´ĞĞÒÔÏÂ³ÌĞò£»  */ 
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

    u->write_event_handler = ngx_http_upstream_dummy_handler; //Êı¾İÒÑ¾­ÔÚÇ°ÃæÈ«²¿·¢Íùºó¶Ë·şÎñÆ÷ÁË£¬ËùÒÔ²»ĞèÒªÔÙ×öĞ´´¦Àí

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "send out chain data to uppeer server OK");
    //ÔÚÁ¬½Óºó¶Ë·şÎñÆ÷conncetÇ°£¬ÓĞÉèÖÃngx_add_conn£¬ÀïÃæÒÑ¾­½«fdÌí¼Óµ½ÁË¶ÁĞ´ÊÂ¼şÖĞ£¬Òò´ËÕâÀïÊµ¼ÊÉÏÖ»ÊÇ¼òµ¥Ö´ĞĞÏÂngx_send_lowat
    if (ngx_handle_write_event(c->write, 0, NGX_FUNC_LINE) != NGX_OK) {
        ngx_http_upstream_finalize_request(r, u,
                                           NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    //Õâ»ØÊı¾İÒÑ¾­·¢ËÍÁË£¬¿ÉÒÔ×¼±¸½ÓÊÕÁË£¬ÉèÖÃ½ÓÊÕºó¶ËÓ¦´ğµÄ³¬Ê±¶¨Ê±Æ÷¡£ 
    /* 
        ¸Ã¶¨Ê±Æ÷ÔÚÊÕµ½ºó¶ËÓ¦´ğÊı¾İºóÉ¾³ı£¬¼ûngx_event_pipe 
        if (rev->timer_set) {
            ngx_del_timer(rev, NGX_FUNC_LINE);
        }
     */
    ngx_add_timer(c->read, u->conf->read_timeout, NGX_FUNC_LINE); //Èç¹û³¬Ê±ÔÚ¸Ãº¯Êı¼ì²ângx_http_upstream_process_header

    if (c->read->ready) {
        ngx_http_upstream_process_header(r, u);
        return;
    }
}

//Ïòºó¶Ë·¢ËÍÇëÇóµÄµ÷ÓÃ¹ı³Ìngx_http_upstream_send_request_body->ngx_output_chain->ngx_chain_writer
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
           out = u->request_bufs; //Èç¹ûÊÇfastcgiÕâÀïÃæÎªÊµ¼Ê·¢Íùºó¶ËµÄÊı¾İ(°üÀ¨fastcgi¸ñÊ½Í·²¿+¿Í»§¶Ë°üÌåµÈ)

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

//ngx_http_upstream_send_request_handlerÓÃ»§Ïòºó¶Ë·¢ËÍ°üÌåÊ±£¬Ò»´Î·¢ËÍÃ»ÍêÍê³É£¬ÔÙ´Î³ö·¢epoll writeµÄÊ±ºòµ÷ÓÃ
static void
ngx_http_upstream_send_request_handler(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_connection_t  *c;

    c = u->peer.connection;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http upstream send request handler");

    if (c->write->timedout) { //¸Ã¶¨Ê±Æ÷ÔÚngx_http_upstream_send_requestÌí¼ÓµÄ
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_TIMEOUT);
        return;
    }

#if (NGX_HTTP_SSL)

    if (u->ssl && c->ssl == NULL) {
        ngx_http_upstream_ssl_init_connection(r, u, c);
        return;
    }

#endif

    if (u->header_sent) { //¶¼ÒÑ¾­ÊÕµ½ºó¶ËµÄÊı¾İ²¢ÇÒ·¢ËÍ¸ø¿Í»§¶Ëä¯ÀÀÆ÷ÁË£¬ËµÃ÷²»»áÔÙÏëºó¶ËĞ´Êı¾İ£¬
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

//ngx_http_upstream_handlerÖĞÖ´ĞĞ
/*
ºó¶Ë·¢ËÍ¹ıÀ´µÄÍ·²¿ĞĞ°üÌå¸ñÊ½: 8×Ö½ÚfastcgiÍ·²¿ĞĞ+ Êı¾İ(Í·²¿ĞĞĞÅÏ¢+ ¿ÕĞĞ + Êµ¼ÊĞèÒª·¢ËÍµÄ°üÌåÄÚÈİ) + Ìî³ä×Ö¶Î
*/
static void
ngx_http_upstream_process_header(ngx_http_request_t *r, ngx_http_upstream_t *u)
{//¶ÁÈ¡FCGIÍ·²¿Êı¾İ£¬»òÕßproxyÍ·²¿Êı¾İ¡£ngx_http_upstream_send_request·¢ËÍÍêÊı¾İºó£¬
//»áµ÷ÓÃÕâÀï£¬»òÕßÓĞ¿ÉĞ´ÊÂ¼şµÄÊ±ºò»áµ÷ÓÃÕâÀï¡£
//ngx_http_upstream_connectº¯ÊıÁ¬½Ófastcgiºó£¬»áÉèÖÃÕâ¸ö»Øµ÷º¯ÊıÎªfcgiÁ¬½ÓµÄ¿É¶ÁÊÂ¼ş»Øµ÷¡£
    ssize_t            n;
    ngx_int_t          rc;
    ngx_connection_t  *c;

    c = u->peer.connection;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process header, fd:%d, buffer_size:%uz", c->fd, u->conf->buffer_size);

    c->log->action = "reading response header from upstream";

    if (c->read->timedout) {//¶Á³¬Ê±ÁË£¬ÂÖÑ¯ÏÂÒ»¸ö¡£ ngx_event_expire_timers³¬Ê±ºó×ßµ½ÕâÀï
        //¸Ã¶¨Ê±Æ÷Ìí¼ÓµØ·½ÔÚngx_http_upstream_send_request
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_TIMEOUT);
        return;
    }

    if (!u->request_sent && ngx_http_upstream_test_connect(c) != NGX_OK) {
        ngx_http_upstream_next(r, u, NGX_HTTP_UPSTREAM_FT_ERROR);
        return;
    }

    if (u->buffer.start == NULL) { //·ÖÅäÒ»¿é»º´æ£¬ÓÃÀ´´æ·Å½ÓÊÜ»ØÀ´µÄÊı¾İ¡£
        u->buffer.start = ngx_palloc(r->pool, u->conf->buffer_size); 
        //Í·²¿ĞĞ²¿·Ö(Ò²¾ÍÊÇµÚÒ»¸öfastcgi data±êÊ¶ĞÅÏ¢£¬ÀïÃæÒ²»áĞ¯´øÒ»²¿·ÖÍøÒ³Êı¾İ)µÄfastcgi±êÊ¶ĞÅÏ¢¿ª±ÙµÄ¿Õ¼äÓÃbuffer_sizeÅäÖÃÖ¸¶¨
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

        //³õÊ¼»¯headers_in´æ·ÅÍ·²¿ĞÅÏ¢£¬ºó¶ËFCGI,proxy½âÎöºóµÄHTTPÍ·²¿½«·ÅÈëÕâÀï
        if (ngx_list_init(&u->headers_in.headers, r->pool, 8,
                          sizeof(ngx_table_elt_t))
            != NGX_OK)
        {
            ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

#if (NGX_HTTP_CACHE)
        /*
        pVpVZ"
        KEY: /test.php

        //ÏÂÃæÊÇºó¶ËÊµ¼Ê·µ»ØµÄÄÚÈİ£¬ÉÏÃæµÄÊÇÔ¤ÁôµÄÍ·²¿
        IX-Powered-By: PHP/5.2.13
        Content-type: text/html

        <Html> 
        <Head> 
        <title>Your page Subject and domain name</title>
          */
        if (r->cache) { //×¢ÒâÕâÀïÌø¹ıÁËÔ¤ÁôµÄÍ·²¿ÄÚ´æ£¬ÓÃÓÚ´æ´¢cacheĞ´ÈëÎÄ¼şÊ±ºòµÄÍ·²¿²¿·Ö£¬¼û
            u->buffer.pos += r->cache->header_start;
            u->buffer.last = u->buffer.pos;
        }
#endif
    }

    for ( ;; ) {
        //recv Îª ngx_unix_recv£¬¶ÁÈ¡Êı¾İ·ÅÔÚu->buffer.lastµÄÎ»ÖÃ£¬·µ»Ø¶Áµ½µÄ´óĞ¡¡£
        n = c->recv(c, u->buffer.last, u->buffer.end - u->buffer.last);

        if (n == NGX_AGAIN) { //ÄÚºË»º³åÇøÒÑ¾­Ã»Êı¾İÁË
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
        //ngx_http_xxx_process_header ngx_http_proxy_process_header
        rc = u->process_header(r);//ngx_http_fastcgi_process_headerµÈ£¬½øĞĞÊı¾İ´¦Àí£¬±ÈÈçºó¶Ë·µ»ØµÄÊı¾İÍ·²¿½âÎö£¬body¶ÁÈ¡µÈ¡£

        if (rc == NGX_AGAIN) {
            ngx_log_debugall(c->log, 0,  " ngx_http_upstream_process_header u->process_header() return NGX_AGAIN");

            if (u->buffer.last == u->buffer.end) { //·ÖÅäµÄÓÃÀ´´æ´¢fastcgi STDOUTÍ·²¿ĞĞ°üÌåµÄbufÒÑ¾­ÓÃÍêÁËÍ·²¿ĞĞ¶¼»¹Ã»ÓĞ½âÎöÍê³É£¬
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

    //µ½ÕâÀï£¬FCGIµÈ¸ñÊ½µÄÊı¾İÒÑ¾­½âÎöÎª±ê×¼HTTPµÄ±íÊ¾ĞÎÊ½ÁË(³ıÁËBODY)£¬ËùÒÔ¿ÉÒÔ½øĞĞupstreamµÄprocess_headers¡£
	//ÉÏÃæµÄ u->process_header(r)ÒÑ¾­½øĞĞFCGIµÈ¸ñÊ½µÄ½âÎöÁË¡£ÏÂÃæ½«Í·²¿Êı¾İ¿½±´µ½headers_out.headersÊı×éÖĞ¡£
    if (ngx_http_upstream_process_headers(r, u) != NGX_OK) {
        return;
    }
    
    if (!r->subrequest_in_memory) {//Èç¹ûÃ»ÓĞ×ÓÇëÇóÁË£¬ÄÇ¾ÍÖ±½Ó·¢ËÍÏìÓ¦¸ø¿Í»§¶Ë°É¡£
        //buffering·½Ê½ºÍ·Çbuffering·½Ê½ÔÚº¯Êıngx_http_upstream_send_response·Ö²æ
        ngx_http_upstream_send_response(r, u);//¸ø¿Í»§¶Ë·¢ËÍÏìÓ¦£¬ÀïÃæ»á´¦Àíheader,body·Ö¿ª·¢ËÍµÄÇé¿öµÄ
        return;
    }

    /* subrequest content in memory */
    //×ÓÇëÇó£¬²¢ÇÒºó¶ËÊı¾İĞèÒª±£´æµ½ÄÚ´æÔÚ

    
    //×¢ÒâÏÂÃæÖ»ÊÇ°Ñºó¶ËÊı¾İ´æµ½bufÖĞ£¬µ«ÊÇÃ»ÓĞ·¢ËÍµ½¿Í»§¶Ë£¬Êµ¼Ê·¢ËÍÒ»°ãÊÇÓÉngx_http_finalize_request->ngx_http_set_write_handlerÊµÏÖ
    
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

    u->read_event_handler = ngx_http_upstream_process_body_in_memory;//ÉèÖÃbody²¿·ÖµÄ¶ÁÊÂ¼ş»Øµ÷¡£

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

//¼ì²éºÍc->fdµÄtcpÁ¬½ÓÊÇ·ñÓĞÒì³£
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
½âÎöÇëÇóµÄÍ·²¿×Ö¶Î¡£Ã¿ĞĞHEADER»Øµ÷Æäcopy_handler£¬È»ºó¿½±´Ò»ÏÂ×´Ì¬ÂëµÈ¡£¿½±´Í·²¿×Ö¶Îµ½headers_out
*/ //ngx_http_upstream_process_headerºÍngx_http_upstream_process_headersºÜÏñÅ¶£¬º¯ÊıÃû£¬×¢Òâ
static ngx_int_t //°Ñ´Óºó¶Ë·µ»Ø¹ıÀ´µÄÍ·²¿ĞĞĞÅÏ¢¿½±´µ½r->headers_outÖĞ£¬ÒÔ±¸Íù¿Í»§¶Ë·¢ËÍÓÃ
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
    {//Èç¹ûÍ·²¿ÖĞÊ¹ÓÃÁËX-Accel-RedirectÌØĞÔ£¬Ò²¾ÍÊÇÏÂÔØÎÄ¼şµÄÌØĞÔ£¬ÔòÔÚÕâÀï½øĞĞÎÄ¼şÏÂÔØ¡££¬ÖØ¶¨Ïò¡£
    /*nginx X-Accel-RedirectÊµÏÖÎÄ¼şÏÂÔØÈ¨ÏŞ¿ØÖÆ 
    ¶ÔÎÄ¼şÏÂÔØµÄÈ¨ÏŞ½øĞĞ¾«È·¿ØÖÆÔÚºÜ¶àµØ·½¶¼ĞèÒª£¬ÀıÈçÓĞ³¥µÄÏÂÔØ·şÎñ¡¢ÍøÂçÓ²ÅÌ¡¢¸öÈËÏà²á¡¢·ÀÖ¹±¾Õ¾ÄÚÈİ±»ÍâÕ¾µÁÁ´µÈ
    ²½Öè0£¬clientÇëÇóhttp://downloaddomain.com/download/my.iso£¬´ËÇëÇó±»CGI³ÌĞò½âÎö£¨¶ÔÓÚ nginxÓ¦¸ÃÊÇfastcgi£©¡£
    ²½Öè1£¬CGI³ÌĞò¸ù¾İ·ÃÎÊÕßµÄÉí·İºÍËùÇëÇóµÄ×ÊÔ´ÆäÊÇ·ñÓĞÏÂÔØÈ¨ÏŞÀ´ÅĞ¶¨ÊÇ·ñÓĞ´ò¿ªµÄÈ¨ÏŞ¡£Èç¹ûÓĞ£¬ÄÇÃ´¸ù¾İ´ËÇëÇóµÃµ½¶ÔÓ¦ÎÄ¼şµÄ´ÅÅÌ´æ·ÅÂ·¾¶£¬ÀıÈçÊÇ /var/data/my.iso¡£
        ÄÇÃ´³ÌĞò·µ»ØÊ±ÔÚHTTP header¼ÓÈëX-Accel-Redirect: /protectfile/data/my.iso£¬²¢¼ÓÉÏhead Content-Type:application/octet-stream¡£
    ²½Öè2£¬nginxµÃµ½cgi³ÌĞòµÄ»ØÓ¦ºó·¢ÏÖ´øÓĞX-Accel-RedirectµÄheader£¬ÄÇÃ´¸ù¾İÕâ¸öÍ·¼ÇÂ¼µÄÂ·¾¶ĞÅÏ¢´ò¿ª´ÅÅÌÎÄ¼ş¡£
    ²½Öè3£¬nginx°Ñ´ò¿ªÎÄ¼şµÄÄÚÈİ·µ»Ø¸øclient¶Ë¡£
    ÕâÑùËùÓĞµÄÈ¨ÏŞ¼ì²é¶¼¿ÉÒÔÔÚ²½Öè1ÄÚÍê³É£¬¶øÇÒcgi·µ»Ø´øX-Accel-RedirectµÄÍ·ºó£¬ÆäÖ´ĞĞÒÑ¾­ÖÕÖ¹£¬Ê£ÏÂµÄ´«ÊäÎÄ¼şµÄ¹¤×÷ÓÉnginx À´½Ó¹Ü£¬
        Í¬Ê±X-Accel-RedirectÍ·µÄĞÅÏ¢±»nginxÉ¾³ı£¬Ò²Òş²ØÁËÎÄ¼şÊµ¼Ê´æ´¢Ä¿Â¼£¬²¢ÇÒÓÉÓÚnginxÔÚ´ò¿ª¾²Ì¬ÎÄ¼şÉÏÊ¹ÓÃÁË sendfile(2)£¬ÆäIOĞ§ÂÊ·Ç³£¸ß¡£
    */
        ngx_http_upstream_finalize_request(r, u, NGX_DECLINED);

        part = &u->headers_in.headers.part; //ºó¶Ë·şÎñÆ÷Ó¦´ğµÄÍ·²¿ĞĞĞÅÏ¢È«²¿ÔÚ¸ÃheadersÁ´±íÊı×éÖĞ
        h = part->elts;

        for (i = 0; /* void */; i++) {

            if (i >= part->nelts) { //headersÉÏÃæµÄÏÂÒ»¸öÁ´±í
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
            //Èç¹ûºó¶Ë·şÎñÆ÷ÓĞ·µ»Øngx_http_upstream_headers_inÖĞµÄÍ·²¿ĞĞ×Ö¶Î£¬Èç¹û¸ÃÊı×éÖĞµÄ³ÉÔ±redirectÎª1£¬ÔòÖ´ĞĞ³ÉÔ±µÄ¶ÔÓ¦µÄcopy_handler
                if (hh->copy_handler(r, &h[i], hh->conf) != NGX_OK) {
                    ngx_http_finalize_request(r,
                                              NGX_HTTP_INTERNAL_SERVER_ERROR);
                    return NGX_DONE;
                }
            }
        }

        uri = u->headers_in.x_accel_redirect->value; //ĞèÒªÄÚ²¿ÖØ¶¨ÏòµÄĞÂµÄuri£¬Í¨¹ıºóÃæµÄngx_http_internal_redirect´ÓĞÂ×ß13 phase½×¶ÎÁ÷³Ì

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

            ngx_http_internal_redirect(r, &uri, &args);//Ê¹ÓÃÄÚ²¿ÖØ¶¨Ïò£¬ÇÉÃîµÄÏÂÔØ¡£ÀïÃæÓÖ»á×ßµ½¸÷ÖÖÇëÇó´¦Àí½×¶Î¡£
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
                          h[i].lowcase_key, h[i].key.len)) //ÕâĞ©Í·²¿²»ĞèÒª·¢ËÍ¸ø¿Í»§¶Ë£¬Òş²Ø
        {
            continue;
        }

        hh = ngx_hash_find(&umcf->headers_in_hash, h[i].hash,
                           h[i].lowcase_key, h[i].key.len);

        if (hh) {//Ò»¸ö¸ö¿½±´µ½ÇëÇóµÄheaders_outÀïÃæ
            if (hh->copy_handler(r, &h[i], hh->conf) != NGX_OK) { //´Óu->headers_in.headers¸´ÖÆµ½r->headers_out.headersÓÃÓÚ·¢ËÍ
                ngx_http_upstream_finalize_request(r, u,
                                               NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_DONE;
            }

            continue; 
        }

        //Èç¹ûÃ»ÓĞ×¢²á¾ä±ú(ÔÚngx_http_upstream_headers_inÕÒ²»µ½¸Ã³ÉÔ±)£¬¿½±´ºó¶Ë·şÎñÆ÷·µ»ØµÄÒ»ĞĞÒ»ĞĞµÄÍ·²¿ĞÅÏ¢(u->headers_in.headersÖĞµÄÍ·²¿ĞĞ¸³Öµ¸ør->headers_out.headers)
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

    //¿½±´×´Ì¬ĞĞ£¬ÒòÎªÕâ¸ö²»ÊÇ´æÔÚheaders_inÀïÃæµÄ¡£
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

    if (rev->timedout) { //ÔÚ·¢ËÍÇëÇóµ½ºó¶ËµÄÊ±ºò£¬ÎÒÃÇĞèÒªµÈ´ı¶Ô·½Ó¦´ğ£¬Òò´ËÉèÖÃÁË¶Á³¬Ê±¶¨Ê±Æ÷£¬¼ûngx_http_upstream_send_request
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

//·¢ËÍºó¶Ë·µ»Ø»ØÀ´µÄÊı¾İ¸ø¿Í»§¶Ë¡£ÀïÃæ»á´¦Àíheader,body·Ö¿ª·¢ËÍµÄÇé¿öµÄ 
static void 
ngx_http_upstream_send_response(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
    int                        tcp_nodelay;
    ssize_t                    n;
    ngx_int_t                  rc;
    ngx_event_pipe_t          *p;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;
    int flag;
    time_t  now, valid;

    rc = ngx_http_send_header(r);//ÏÈ·¢header£¬ÔÙ·¢body //µ÷ÓÃÃ¿Ò»¸öfilter¹ıÂË£¬´¦ÀíÍ·²¿Êı¾İ¡£×îºó½«Êı¾İ·¢ËÍ¸ø¿Í»§¶Ë¡£µ÷ÓÃngx_http_top_header_filter

    if (rc == NGX_ERROR || rc > NGX_OK || r->post_action) {
        ngx_http_upstream_finalize_request(r, u, rc);
        return;
    }

    u->header_sent = 1;//±ê¼ÇÒÑ¾­·¢ËÍÁËÍ·²¿×Ö¶Î£¬ÖÁÉÙÊÇÒÑ¾­¹ÒÔØ³öÈ¥£¬¾­¹ıÁËfilterÁË¡£

    if (u->upgrade) {
        ngx_http_upstream_upgrade(r, u);
        return;
    }

    c = r->connection;

    if (r->header_only) {//Èç¹ûÖ»ĞèÒª·¢ËÍÍ·²¿Êı¾İ£¬±ÈÈç¿Í»§¶ËÓÃcurl -I ·ÃÎÊµÄ¡£·µ»Ø204×´Ì¬Âë¼´¿É¡£

        if (!u->buffering) { //ÅäÖÃ²»ĞèÒª»º´æ°üÌå£¬»òÕßºó¶ËÒªÇó²»ÅäÖÃ»º´æ°üÌå£¬Ö±½Ó½áÊø
            ngx_http_upstream_finalize_request(r, u, rc);
            return;
        }

        if (!u->cacheable && !u->store) { //Èç¹û¶¨ÒåÁË#if (NGX_HTTP_CACHE)Ôò¿ÉÄÜÖÃ1
            ngx_http_upstream_finalize_request(r, u, rc);
            return;
        }

        u->pipe->downstream_error = 1; //ÃüÃû¿Í»§¶ËÖ»ÇëÇóÍ·²¿ĞĞ£¬µ«ÊÇÉÏÓÎÈ¸ÅäÖÃ»òÕßÒªÇó»º´æ»òÕß´æ´¢°üÌå
    }

    if (r->request_body && r->request_body->temp_file) { //¿Í»§¶Ë·¢ËÍ¹ıÀ´µÄ°üÌå´æ´¢ÔÚÁÙÊ±ÎÄ¼şÖĞ£¬ÔòĞèÒª°Ñ´æ´¢ÁÙÊ±ÎÄ¼şÉ¾³ı
        ngx_pool_run_cleanup_file(r->pool, r->request_body->temp_file->file.fd); 
        //Ö®Ç°ÁÙÊ±ÎÄ¼şÄÚÈİÒÑ¾­²»ĞèÒªÁË£¬ÒòÎªÔÚngx_http_fastcgi_create_request(ngx_http_xxx_create_request)ÖĞÒÑ¾­°ÑÁÙÊ±ÎÄ¼şÖĞµÄÄÚÈİ
        //¸³Öµ¸øu->request_bufs²¢Í¨¹ı·¢ËÍµ½ÁËºó¶Ë·şÎñÆ÷£¬ÏÖÔÚĞèÒª·¢Íù¿Í»§¶ËµÄÄÚÈİÎªÉÏÓÎÓ¦´ğ»ØÀ´µÄ°üÌå£¬Òò´Ë´ËÁÙÊ±ÎÄ¼şÄÚÈİÒÑ¾­Ã»ÓÃÁË
        r->request_body->temp_file->file.fd = NGX_INVALID_FILE;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    /*
     Èç¹û¿ªÆô»º³å£¬ÄÇÃ´Nginx½«¾¡¿ÉÄÜ¶àµØ¶ÁÈ¡ºó¶Ë·şÎñÆ÷µÄÏìÓ¦Êı¾İ£¬µÈ´ïµ½Ò»¶¨Á¿£¨±ÈÈçbufferÂú£©ÔÙ´«ËÍ¸ø×îÖÕ¿Í»§¶Ë¡£Èç¹û¹Ø±Õ£¬
     ÄÇÃ´Nginx¶ÔÊı¾İµÄÖĞ×ª¾ÍÊÇÒ»¸öÍ¬²½µÄ¹ı³Ì£¬¼´´Óºó¶Ë·şÎñÆ÷½ÓÊÕµ½ÏìÓ¦Êı¾İ¾ÍÁ¢¼´½«Æä·¢ËÍ¸ø¿Í»§¶Ë¡£
     */
    flag = u->buffering;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0, "ngx_http_upstream_send_response, buffering flag:%d", flag);
    if (!u->buffering) { 
    //bufferingÎª1£¬±íÊ¾ÉÏÓÎÀ´µÄ°üÌåÏÈ»º´æÉÏÓÎ·¢ËÍÀ´µÄ°üÌå£¬È»ºóÔÚ·¢ËÍµ½ÏÂÓÎ£¬Èç¹û¸ÃÖµÎª0£¬Ôò½ÓÊÕ¶àÉÙÉÏÓÎ°üÌå¾ÍÏòÏÂÓÎ×ª·¢¶àÉÙ°üÌå

        if (u->input_filter == NULL) { //Èç¹ûinput_filterÎª¿Õ£¬ÔòÉèÖÃÄ¬ÈÏµÄfilter£¬È»ºó×¼±¸·¢ËÍÊı¾İµ½¿Í»§¶Ë¡£È»ºóÊÔ×Å¶Á¶ÁFCGI
            u->input_filter_init = ngx_http_upstream_non_buffered_filter_init;
            //ngx_http_upstream_non_buffered_filter½«u->buffer.last - u->buffer.posÖ®¼äµÄÊı¾İ·Åµ½u->out_bufs·¢ËÍ»º³åÈ¥Á´±íÀïÃæ¡£
            //¸ù¾İ¾ßÌåµÄµ½ÉÏÓÎ×ª·¢µÄ·½Ê½£¬Ñ¡ÔñÊ¹ÓÃfastcgi memcachedµÈ£¬ngx_http_xxx_filter
            u->input_filter = ngx_http_upstream_non_buffered_filter; //Ò»°ã¾ÍÉèÖÃÎªÕâ¸öÄ¬ÈÏµÄ£¬memcacheÎªngx_http_memcached_filter
            u->input_filter_ctx = r;
        }

        //ÉèÖÃupstreamµÄ¶ÁÊÂ¼ş»Øµ÷£¬ÉèÖÃ¿Í»§¶ËÁ¬½ÓµÄĞ´ÊÂ¼ş»Øµ÷¡£
        u->read_event_handler = ngx_http_upstream_process_non_buffered_upstream;
        r->write_event_handler =
                             ngx_http_upstream_process_non_buffered_downstream;//µ÷ÓÃ¹ıÂËÄ£¿éÒ»¸ö¸ö¹ıÂËbody£¬×îÖÕ·¢ËÍ³öÈ¥¡£

        r->limit_rate = 0;
        //ngx_http_XXX_input_filter_init(Èçngx_http_fastcgi_input_filter_init ngx_http_proxy_input_filter_init ngx_http_proxy_input_filter_init)  
        //Ö»ÓĞmemcached»áÖ´ĞĞngx_http_memcached_filter_init£¬ÆäËû·½Ê½Ê²Ã´Ò²Ã»×ö 
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

        /* 
          ²»ÊÇ»¹Ã»½ÓÊÕ°üÌåÂï£¬ÎªÊ²Ã´¾Í¿ªÊ¼·¢ËÍÁËÄØ¿
              ÕâÊÇÒòÎªÔÚÇ°ÃæµÄngx_http_upstream_process_header½ÓÊÕfastcgiÍ·²¿ĞĞ±êÊ¶°üÌå´¦ÀíµÄÊ±ºò£¬ÓĞ¿ÉÄÜ»á°ÑÒ»²¿·Öfastcgi°üÌå±êÊ¶Ò²ÊÕ¹ıÁË£¬
          Òò´ËĞèÒª´¦Àí
          */
        
        if (n) {//µÃµ½½«Òª·¢ËÍµÄÊı¾İµÄ´óĞ¡£¬Ã¿´ÎÓĞ¶àÉÙ¾Í·¢ËÍ¶àÉÙ¡£²»µÈ´ıupstreamÁË  ÒòÎªÕâÊÇ²»»º´æ·½Ê½·¢ËÍ°üÌåµ½¿Í»§¶Ë
            u->buffer.last = u->buffer.pos;

            u->state->response_length += n;//Í³¼ÆÇëÇóµÄ·µ»Ø°üÌåÊı¾İ(²»°üÀ¨ÇëÇóĞĞ)³¤¶È¡£

            //ÏÂÃæinput_filterÖ»ÊÇ¼òµ¥µÄ¿½±´bufferÉÏÃæµÄÊı¾İ×Ü¹²n³¤¶ÈµÄ£¬µ½u->out_bufsÀïÃæÈ¥£¬ÒÔ´ı·¢ËÍ¡£
            //ngx_http_xxx_non_buffered_filter(Èçngx_http_fastcgi_non_buffered_filter)
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

        return; //ÕâÀï»á·µ»Ø»ØÈ¥
    }

    /* TODO: preallocate event_pipe bufs, look "Content-Length" */

#if (NGX_HTTP_CACHE)

    /* ×¢ÒâÕâÊ±ºò»¹ÊÇÔÚ¶ÁÈ¡µÚÒ»¸öÍ·²¿ĞĞµÄ¹ı³ÌÖĞ(¿ÉÄÜ»áĞ¯´ø²¿·Ö»òÕßÈ«²¿°üÌåÊı¾İÔÚÀïÃæ)  */

    if (r->cache && r->cache->file.fd != NGX_INVALID_FILE) {
        ngx_pool_run_cleanup_file(r->pool, r->cache->file.fd);
        r->cache->file.fd = NGX_INVALID_FILE;
    }

    /*   
     fastcgi_no_cache ÅäÖÃÖ¸Áî¿ÉÒÔÊ¹ upstream Ä£¿é²»ÔÙ»º´æÂú×ã¼È¶¨Ìõ¼şµÄÇëÇóµÃ 
     µ½µÄÏìÓ¦¡£ÓÉÉÏÃæ ngx_http_test_predicates º¯Êı¼°Ïà¹Ø´úÂëÍê³É¡£ 
     */
    switch (ngx_http_test_predicates(r, u->conf->no_cache)) {

    case NGX_ERROR:
        ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
        return;

    case NGX_DECLINED:
        u->cacheable = 0;
        break;

    default: /* NGX_OK */
        //ÔÚ¿Í»§¶ËÇëÇóºó¶ËµÄÊ±ºò£¬Èç¹ûÃ»ÓĞÃüÖĞ£¬Ôò»á°Ñcache_statusÖÃÎªNGX_HTTP_CACHE_BYPASS
        if (u->cache_status == NGX_HTTP_CACHE_BYPASS) {//ËµÃ÷ÊÇÒòÎªÅäÖÃÁËxxx_cache_bypass¹¦ÄÜ£¬´Ó¶øÖ±½Ó´Óºó¶ËÈ¡Êı¾İ

            /* create cache if previously bypassed */
            /*
               fastcgi_cache_bypass ÅäÖÃÖ¸Áî¿ÉÒÔÊ¹Âú×ã¼È¶¨Ìõ¼şµÄÇëÇóÈÆ¹ı»º´æÊı¾İ£¬µ«ÊÇÕâĞ©ÇëÇóµÄÏìÓ¦Êı¾İÒÀÈ»¿ÉÒÔ±» upstream Ä£¿é»º´æ¡£ 
               */
            if (ngx_http_file_cache_create(r) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }
        }

        break;
    }

    /*
     u->cacheable ÓÃÓÚ¿ØÖÆÊÇ·ñ¶ÔÏìÓ¦½øĞĞ»º´æ²Ù×÷¡£ÆäÄ¬ÈÏÖµÎª 1£¬ÔÚ»º´æ¶ÁÈ¡¹ı³ÌÖĞ ¿ÉÒòÄ³Ğ©Ìõ¼ş½«ÆäÉèÖÃÎª 0£¬¼´²»ÔÚ»º´æ¸ÃÇëÇóµÄÏìÓ¦Êı¾İ¡£ 
     */
    if (u->cacheable) {
        now = ngx_time();

        /*
           »º´æÄÚÈİµÄÓĞĞ§Ê±¼äÓÉ fastcgi_cache_valid  proxy_cache_validÅäÖÃÖ¸ÁîÉèÖÃ£¬²¢ÇÒÎ´¾­¸ÃÖ¸ÁîÉèÖÃµÄÏìÓ¦Êı¾İÊÇ²»»á±» upstream Ä£¿é»º´æµÄ¡£
          */
        valid = r->cache->valid_sec;

        if (valid == 0) { //¸³Öµproxy_cache_valid xxx 4m;ÖĞµÄ4m
            valid = ngx_http_file_cache_valid(u->conf->cache_valid,
                                              u->headers_in.status_n);
            if (valid) {
                r->cache->valid_sec = now + valid;
            }
        }

        if (valid) {
            r->cache->date = now;
            //ÔÚ¸Ãº¯ÊıÇ°ngx_http_upstream_process_header->p->process_header()º¯ÊıÖĞÒÑ¾­½âÎö³ö°üÌåÍ·²¿ĞĞ 
            r->cache->body_start = (u_short) (u->buffer.pos - u->buffer.start); //ºó¶Ë·µ»ØµÄÍøÒ³°üÌå²¿·ÖÔÚbufferÖĞµÄ´æ´¢Î»ÖÃ

            if (u->headers_in.status_n == NGX_HTTP_OK
                || u->headers_in.status_n == NGX_HTTP_PARTIAL_CONTENT)
            {
                //ºó¶ËĞ¯´øµÄÍ·²¿ĞĞ"Last-Modified:XXX"¸³Öµ£¬¼ûngx_http_upstream_process_last_modified
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

            /* 
               ×¢ÒâÕâÊ±ºò»¹ÊÇÔÚ¶ÁÈ¡µÚÒ»¸öÍ·²¿ĞĞµÄ¹ı³ÌÖĞ(¿ÉÄÜ»áĞ¯´ø²¿·Ö»òÕßÈ«²¿°üÌåÊı¾İÔÚÀïÃæ)  
                 
               upstream Ä£¿éÔÚÉêÇë u->buffer ¿Õ¼äÊ±£¬ÒÑ¾­Ô¤ÏÈÎª»º´æÎÄ¼ş°üÍ··ÖÅäÁË¿Õ¼ä£¬ËùÒÔ¿ÉÒÔÖ±½Óµ÷ÓÃ ngx_http_file_cache_set_header 
               ÔÚ´Ë¿Õ¼äÖĞ³õÊ¼»¯»º´æÎÄ¼ş°üÍ·£º 
               */
            if (ngx_http_file_cache_set_header(r, u->buffer.start) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                return;
            }

        } else {
            u->cacheable = 0;
        }
    }

    now = ngx_time();
    if(r->cache) {
        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http cacheable: %d, r->cache->valid_sec:%T, now:%T", u->cacheable, r->cache->valid_sec, now);
    } else {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http cacheable: %d", u->cacheable);
    }               
    if (u->cacheable == 0 && r->cache) {
        ngx_http_file_cache_free(r->cache, u->pipe->temp_file);
    }

#endif
    //buffering·½Ê½»á×ßµ½ÕâÀï£¬Í¨¹ıpipe·¢ËÍ£¬Èç¹ûÎª0£¬ÔòÉÏÃæµÄ³ÌĞò»áreturn
    
    p = u->pipe;

    //ÉèÖÃfilter£¬¿ÉÒÔ¿´µ½¾ÍÊÇhttpµÄÊä³öfilter
    p->output_filter = (ngx_event_pipe_output_filter_pt) ngx_http_output_filter;
    p->output_ctx = r;
    p->tag = u->output.tag;
    p->bufs = u->conf->bufs;//ÉèÖÃbufs£¬Ëü¾ÍÊÇupstreamÖĞÉèÖÃµÄbufs.u == &flcf->upstream;
    p->busy_size = u->conf->busy_buffers_size; //Ä¬ÈÏ
    p->upstream = u->peer.connection;//¸³Öµ¸úºó¶ËupstreamµÄÁ¬½Ó¡£
    p->downstream = c;//¸³Öµ¸ú¿Í»§¶ËµÄÁ¬½Ó¡£
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

    ngx_int_t cacheable = p->cacheable;

   if (r->cache && r->cache->file_cache->temp_path && r->cache->file_cache->path) {
        ngx_log_debugall(p->log, 0, "ngx_http_upstream_send_response, "
            "p->cacheable:%i, tempfile:%V, pathfile:%V", cacheable, &r->cache->file_cache->temp_path->name, &r->cache->file_cache->path->name);
    }
    
    if (p->cacheable) {
        p->temp_file->persistent = 1;
        /*
Ä¬ÈÏÇé¿öÏÂp->temp_file->path = u->conf->temp_path; Ò²¾ÍÊÇÓÉngx_http_fastcgi_temp_pathÖ¸¶¨Â·¾¶£¬µ«ÊÇÈç¹ûÊÇ»º´æ·½Ê½(p->cacheable=1)²¢ÇÒÅäÖÃ
proxy_cache_path(fastcgi_cache_path) /a/bµÄÊ±ºò´øÓĞuse_temp_path=off(±íÊ¾²»Ê¹ÓÃngx_http_fastcgi_temp_pathÅäÖÃµÄpath)£¬
Ôòp->temp_file->path = r->cache->file_cache->temp_path; Ò²¾ÍÊÇÁÙÊ±ÎÄ¼ş/a/b/temp¡£use_temp_path=off±íÊ¾²»Ê¹ÓÃngx_http_fastcgi_temp_path
ÅäÖÃµÄÂ·¾¶£¬¶øÊ¹ÓÃÖ¸¶¨µÄÁÙÊ±Â·¾¶/a/b/temp   ¼ûngx_http_upstream_send_response 
*/
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

    p->preread_bufs->buf = &u->buffer; //°Ñ°üÌå²¿·ÖµÄposºÍlast´æ´¢µ½p->preread_bufs->buf
    p->preread_bufs->next = NULL;
    u->buffer.recycled = 1;

    //Ö®Ç°¶ÁÈ¡ºó¶ËÍ·²¿ĞĞĞÅÏ¢µÄÊ±ºòµÄbuf»¹ÓĞÊ£ÓàÊı¾İ£¬Õâ²¿·ÖÊı¾İ¾ÍÊÇ°üÌåÊı¾İ£¬Ò²¾ÍÊÇ¶ÁÈ¡Í·²¿ĞĞfastcgi±êÊ¶ĞÅÏ¢µÄÊ±ºò°Ñ²¿·Ö°üÌåÊı¾İ¶ÁÈ¡ÁË
    p->preread_size = u->buffer.last - u->buffer.pos; 
    
    if (u->cacheable) { //×¢Òâ×ßµ½ÕâÀïµÄÊ±ºò£¬Ç°ÃæÒÑ¾­°Ñºó¶ËÍ·²¿ĞĞĞÅÏ¢½âÎö³öÀ´ÁË£¬u->buffer.posÖ¸ÏòµÄÊÇÊµ¼ÊÊı¾İ²¿·Ö

        p->buf_to_file = ngx_calloc_buf(r->pool);
        if (p->buf_to_file == NULL) {
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }

        //Ö¸ÏòµÄÊÇÎª»ñÈ¡ºó¶ËÍ·²¿ĞĞµÄÊ±ºò·ÖÅäµÄµÚÒ»¸ö»º³åÇø£¬buf´óĞ¡ÓÉxxx_buffer_size(fastcgi_buffer_size proxy_buffer_size memcached_buffer_size)Ö¸¶¨
        /*
            ÕâÀïÃæÖ»´æ´¢ÁËÍ·²¿ĞĞbufferÖĞÍ·²¿ĞĞµÄÄÚÈİ²¿·Ö£¬ÒòÎªºóÃæĞ´ÁÙÊ±ÎÄ¼şµÄÊ±ºò£¬ĞèÒª°Ñºó¶ËÍ·²¿ĞĞÒ²Ğ´½øÀ´£¬ÓÉÓÚÇ°Ãæ¶ÁÈ¡Í·²¿ĞĞºóÖ¸ÕëÒÑ¾­Ö¸ÏòÁËÊı¾İ²¿·Ö
            Òò´ËĞèÒªÁÙÊ±ÓÃbuf_to_file->startÖ¸ÏòÍ·²¿ĞĞ²¿·Ö¿ªÊ¼£¬posÖ¸ÏòÊı¾İ²¿·Ö¿ªÊ¼£¬Ò²¾ÍÊÇÍ·²¿ĞĞ²¿·Ö½áÎ²
          */
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
    u->buffer.last = u->buffer.pos; //°üÌåÊı¾İÖ¸ÏòÔÚÇ°ÃæÒÑ¾­´æ´¢µ½ÁËp->preread_bufs->buf

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

    //buffering·½Ê½£¬ºó¶ËÍ·²¿ĞÅÏ¢ÒÑ¾­¶ÁÈ¡Íê±ÏÁË£¬Èç¹ûºó¶Ë»¹ÓĞ°üÌåĞèÒª·¢ËÍ£¬Ôò±¾¶ËÍ¨¹ı¸Ã·½Ê½¶ÁÈ¡
    u->read_event_handler = ngx_http_upstream_process_upstream;
    r->write_event_handler = ngx_http_upstream_process_downstream; //µ±¿ÉĞ´ÊÂ¼ş´Ù·¢µÄÊ±ºò£¬Í¨¹ı¸Ãº¯Êı¼ÌĞøĞ´Êı¾İ

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

    if (upstream->read->timedout || upstream->write->timedout) { //ÔÚ·¢ËÍÇëÇóµ½ºó¶ËµÄÊ±ºò£¬ÎÒÃÇĞèÒªµÈ´ı¶Ô·½Ó¦´ğ£¬Òò´ËÉèÖÃÁË¶Á³¬Ê±¶¨Ê±Æ÷£¬¼ûngx_http_upstream_send_request
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

/*
ngx_http_upstream_send_response·¢ËÍÍêHERDERºó£¬Èç¹ûÊÇ·Ç»º³åÄ£Ê½£¬»áµ÷ÓÃÕâÀï½«Êı¾İ·¢ËÍ³öÈ¥µÄ¡£
Õâ¸öº¯ÊıÊµ¼ÊÉÏÅĞ¶ÏÒ»ÏÂ³¬Ê±ºó£¬¾Íµ÷ÓÃngx_http_upstream_process_non_buffered_requestÁË¡£nginxÀÏ·½·¨¡£
*/
static void 
//buffringÄ£Ê½Í¨¹ıngx_http_upstream_process_upstream¸Ãº¯Êı´¦Àí£¬·ÇbuffringÄ£Ê½Í¨¹ıngx_http_upstream_process_non_buffered_downstream´¦Àí
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

    //ÏÂÃæ¿ªÊ¼½«out_bufsÀïÃæµÄÊı¾İ·¢ËÍ³öÈ¥£¬È»ºó¶ÁÈ¡Êı¾İ£¬È»ºó·¢ËÍ£¬Èç´ËÑ­»·¡£
    ngx_http_upstream_process_non_buffered_request(r, 1);
}

//ngx_http_upstream_send_responseÉèÖÃºÍµ÷ÓÃÕâÀï£¬µ±ÉÏÓÎµÄPROXYÓĞÊı¾İµ½À´£¬¿ÉÒÔ¶ÁÈ¡µÄÊ±ºòµ÷ÓÃÕâÀï¡£
//buffering·½Ê½£¬Îªngx_http_fastcgi_input_filter  ·Çbuffering·½Ê½Îªngx_http_upstream_non_buffered_filter
static void
ngx_http_upstream_process_non_buffered_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{
    ngx_connection_t  *c;

    c = u->peer.connection;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process non buffered upstream");

    c->log->action = "reading upstream";

    if (c->read->timedout) { //ÔÚ·¢ËÍÇëÇóµ½ºó¶ËµÄÊ±ºò£¬ÎÒÃÇĞèÒªµÈ´ı¶Ô·½Ó¦´ğ£¬Òò´ËÉèÖÃÁË¶Á³¬Ê±¶¨Ê±Æ÷£¬¼ûngx_http_upstream_send_request
        ngx_connection_error(c, NGX_ETIMEDOUT, "upstream timed out");
        ngx_http_upstream_finalize_request(r, u, NGX_HTTP_GATEWAY_TIME_OUT);
        return;
    }
    
    //ÕâÀï¸úngx_http_upstream_process_non_buffered_downstreamÆäÊµ¾ÍÒ»¸öÇø±ğ: ²ÎÊıÎª0£¬±íÊ¾²»ÓÃÁ¢¼´·¢ËÍÊı¾İ£¬ÒòÎªÃ»ÓĞÊı¾İ¿ÉÒÔ·¢ËÍ£¬µÃÏÈ¶ÁÈ¡²ÅĞĞ¡£
    ngx_http_upstream_process_non_buffered_request(r, 0);
}

/*
µ÷ÓÃ¹ıÂËÄ£¿é£¬½«Êı¾İ·¢ËÍ³öÈ¥£¬do_writeÎªÊÇ·ñÒª¸ø¿Í»§¶Ë·¢ËÍÊı¾İ¡£
1.Èç¹ûÒª·¢ËÍ£¬¾Íµ÷ÓÃngx_http_output_filter½«Êı¾İ·¢ËÍ³öÈ¥¡£
2.È»ºóngx_unix_recv¶ÁÈ¡Êı¾İ£¬·ÅÈëout_bufsÀïÃæÈ¥¡£Èç´ËÑ­»·
*/
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
    downstream = r->connection;//ÕÒµ½Õâ¸öÇëÇóµÄ¿Í»§¶ËÁ¬½Ó
    upstream = u->peer.connection;//ÕÒµ½ÉÏÓÎµÄÁ¬½Ó

    b = &u->buffer; //ÕÒµ½ÕâÛçÒª·¢ËÍµÄÊı¾İ£¬²»¹ı´ó²¿·Ö¶¼±»input filter·Åµ½out_bufsÀïÃæÈ¥ÁË¡£

    do_write = do_write || u->length == 0; //do_writeÎª1Ê±±íÊ¾ÒªÁ¢¼´·¢ËÍ¸ø¿Í»§¶Ë¡£

    for ( ;; ) {

        if (do_write) { //ÒªÁ¢¼´·¢ËÍ¡£
            //out_bufsÖĞµÄÊı¾İÊÇ´Óngx_http_fastcgi_non_buffered_filter»ñÈ¡
            if (u->out_bufs || u->busy_bufs) {
                //Èç¹ûu->out_bufs²»ÎªNULLÔòËµÃ÷ÓĞĞèÒª·¢ËÍµÄÊı¾İ£¬ÕâÊÇu->input_filter_init(u->input_filter_ctx)(ngx_http_upstream_non_buffered_filter)¿½±´µ½ÕâÀïµÄ¡£
				//u->busy_bufs´ú±íÊÇÔÚ¶ÁÈ¡fastcgiÇëÇóÍ·µÄÊ±ºò£¬¿ÉÄÜÀïÃæ»á´øÓĞ°üÌåÊı¾İ£¬¾ÍÊÇÍ¨¹ıÕâÀï·¢ËÍ
                rc = ngx_http_output_filter(r, u->out_bufs);

                if (rc == NGX_ERROR) {
                    ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                    return;
                }

                //¾ÍÊÇ°Ñngx_http_output_filterµ÷ÓÃºóÎ´·¢ËÍÍê±ÏµÄÊı¾İbufÌí¼Óµ½busy_bufsÖĞ£¬Èç¹ûÏÂ´ÎÔÙ´Îµ÷ÓÃngx_http_output_filterºó°Ñbusy_bufsÖĞÉÏÒ»´ÎÃ»ÓĞ·¢ËÍÍêµÄ·¢ËÍ³öÈ¥ÁË£¬Ôò°Ñ¶ÔÓ¦µÄbufÒÆ³ıÌí¼Óµ½freeÖĞ
                //ÏÂÃæ½«out_bufsµÄÔªËØÒÆ¶¯µ½busy_bufsµÄºóÃæ£»½«ÒÑ¾­·¢ËÍÍê±ÏµÄbusy_bufsÁ´±íÔªËØÒÆ¶¯µ½free_bufsÀïÃæ
                ngx_chain_update_chains(r->pool, &u->free_bufs, &u->busy_bufs,
                                        &u->out_bufs, u->output.tag);
            }

            if (u->busy_bufs == NULL) {//busy_bufsÃ»ÓĞÁË£¬¶¼·¢ÍêÁË¡£ÏëÒª·¢ËÍµÄÊı¾İ¶¼ÒÑ¾­·¢ËÍÍê±Ï

                if (u->length == 0
                    || (upstream->read->eof && u->length == -1)) //°üÌåÊı¾İÒÑ¾­¶ÁÍêÁË
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

                b->pos = b->start;//ÖØÖÃu->buffer,ÒÔ±ãÓëÏÂ´ÎÊ¹ÓÃ£¬´Ó¿ªÊ¼Æğ¡£bÖ¸ÏòµÄ¿Õ¼ä¿ÉÒÔ¼ÌĞø¶ÁÊı¾İÁË
                b->last = b->start;
            }
        }

        size = b->end - b->last;//µÃµ½µ±Ç°bufµÄÊ£Óà¿Õ¼ä

        if (size && upstream->read->ready) { 
        //ÎªÊ²Ã´¿ÉÄÜ×ßµ½ÕâÀï?ÒòÎªÔÚngx_http_upstream_process_headerÖĞ¶ÁÈ¡ºó¶ËÊı¾İµÄÊ±ºò£¬buf´óĞ¡Ä¬ÈÏÎªÒ³Ãæ´óĞ¡ngx_pagesize
        //µ¥ÓĞ¿ÉÄÜºó¶Ë·¢ËÍ¹ıÀ´µÄÊı¾İ±Èngx_pagesize´ó£¬Òò´Ë¾ÍÃ»ÓĞ¶ÁÍê£¬Ò²¾ÍÊÇrecvÖĞ²»»á°ÉreadyÖÃ0£¬ËùÒÔÕâÀï¿ÉÒÔ¼ÌĞø¶Á

            n = upstream->recv(upstream, b->last, size);

            if (n == NGX_AGAIN) { //ËµÃ÷ÒÑ¾­ÄÚºË»º³åÇøÊı¾İÒÑ¾­¶ÁÍê£¬ÍË³öÑ­»·£¬È»ºó¸ù¾İepollÊÂ¼şÀ´¼ÌĞø´¥·¢¶ÁÈ¡ºó¶ËÊı¾İ
                break;
            }

            if (n > 0) {
                u->state->response_length += n;

                if (u->input_filter(u->input_filter_ctx, n) == NGX_ERROR) {
                    ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
                    return;
                }
            }

            do_write = 1;//ÒòÎª¸Õ¸ÕÎŞÂÛÈçºÎn´óÓÚ0£¬ËùÒÔ¶ÁÈ¡ÁËÊı¾İ£¬ÄÇÃ´ÏÂÒ»¸öÑ­»·»á½«out_bufsµÄÊı¾İ·¢ËÍ³öÈ¥µÄ¡£

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
    //ÀıÈçÎÒ°ÑÊı¾İ°ÑÊı¾İĞ´µ½ÄÚºËĞ­ÒéÕ»µ½Ğ´ÂúĞ­ÒéÕ»»º´æ£¬µ«ÊÇ¶Ô¶ËÒ»Ö±²»¶ÁÈ¡µÄÊ±ºò£¬Êı¾İÒ»Ö±·¢²»³öÈ¥ÁË£¬Ò²²»»á´¥·¢epoll_waitĞ´ÊÂ¼ş£¬
    //ÕâÀï¼Ó¸ö¶¨Ê±Æ÷¾ÍÊÇÎªÁË±ÜÃâÕâÖÖÇé¿ö·¢Éú
        ngx_add_timer(downstream->write, clcf->send_timeout, NGX_FUNC_LINE);

    } else if (downstream->write->timer_set) {
        ngx_del_timer(downstream->write, NGX_FUNC_LINE);
    }

    if (ngx_handle_read_event(upstream->read, 0, NGX_FUNC_LINE) != NGX_OK) { //epollÔÚacceptµÄÊ±ºò¶ÁĞ´ÒÑ¾­¼ÓÈëepollÖĞ£¬Òò´Ë¶ÔepollÀ´ËµÃ»ÓÃ
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

/*
½«u->buffer.last - u->buffer.posÖ®¼äµÄÊı¾İ·Åµ½u->out_bufs·¢ËÍ»º³åÈ¥Á´±íÀïÃæ¡£ÕâÑù¿ÉĞ´µÄÊ±ºò¾Í»á·¢ËÍ¸ø¿Í»§¶Ë¡£
ngx_http_upstream_process_non_buffered_requestº¯Êı»á¶ÁÈ¡out_bufsÀïÃæµÄÊı¾İ£¬È»ºóµ÷ÓÃÊä³ö¹ıÂËÁ´½Ó½øĞĞ·¢ËÍµÄ¡£
*/ //buffering·½Ê½£¬Îªngx_http_fastcgi_input_filter  ·Çbuffering·½Ê½Îªngx_http_upstream_non_buffered_filter
static ngx_int_t
ngx_http_upstream_non_buffered_filter(void *data, ssize_t bytes)
{
    ngx_http_request_t  *r = data;

    ngx_buf_t            *b;
    ngx_chain_t          *cl, **ll;
    ngx_http_upstream_t  *u;

    u = r->upstream;

    for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) { //±éÀúu->out_bufs
        ll = &cl->next;
    }

    cl = ngx_chain_get_free_buf(r->pool, &u->free_bufs);//·ÖÅäÒ»¸ö¿ÕÏĞµÄchain buff
    if (cl == NULL) {
        return NGX_ERROR;
    }

    *ll = cl; //½«ĞÂÉêÇëµÄ»º´æÁ´½Ó½øÀ´¡£

    cl->buf->flush = 1;
    cl->buf->memory = 1;

    b = &u->buffer; //È¥³ı½«Òª·¢ËÍµÄÕâ¸öÊı¾İ£¬Ó¦¸ÃÊÇ¿Í»§¶ËµÄ·µ»ØÊı¾İÌå¡£½«Æä·ÅÈë

    cl->buf->pos = b->last;
    b->last += bytes;
    cl->buf->last = b->last;
    cl->buf->tag = u->output.tag;

    if (u->length == -1) {//u->length±íÊ¾½«Òª·¢ËÍµÄÊı¾İ´óĞ¡Èç¹ûÎª-1,ÔòËµÃ÷ºó¶ËĞ­Òé²¢Ã»ÓĞÖ¸¶¨ĞèÒª·¢ËÍµÄ´óĞ¡(ÀıÈçchunk·½Ê½)£¬´ËÊ±ÎÒÃÇÖ»ĞèÒª·¢ËÍÎÒÃÇ½ÓÊÕµ½µÄ.
        return NGX_OK;
    }

    u->length -= bytes;//¸üĞÂ½«Òª·¢ËÍµÄÊı¾İ´óĞ¡
 
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

/*
ÕâÊÇÔÚÓĞbufferingµÄÇé¿öÏÂÊ¹ÓÃµÄº¯Êı¡£
ngx_http_upstream_send_responseµ÷ÓÃÕâÀï·¢¶¯Ò»ÏÂÊı¾İ¶ÁÈ¡¡£ÒÔºóÓĞÊı¾İ¿É¶ÁµÄÊ±ºòÒ²»áµ÷ÓÃÕâÀïµÄ¶ÁÈ¡ºó¶ËÊı¾İ¡£ÉèÖÃµ½ÁËu->read_event_handlerÁË¡£
*/
static void
ngx_http_upstream_process_upstream(ngx_http_request_t *r,
    ngx_http_upstream_t *u) 
//buffringÄ£Ê½Í¨¹ıngx_http_upstream_process_upstream¸Ãº¯Êı´¦Àí£¬·ÇbuffringÄ£Ê½Í¨¹ıngx_http_upstream_process_non_buffered_downstream´¦Àí
{ //×¢Òâ×ßµ½ÕâÀïµÄÊ±ºò£¬ºó¶Ë·¢ËÍµÄÍ·²¿ĞĞĞÅÏ¢ÒÑ¾­ÔÚÇ°ÃæµÄngx_http_upstream_send_response->ngx_http_send_headerÒÑ¾­°ÑÍ·²¿ĞĞ²¿·Ö·¢ËÍ¸ø¿Í»§¶ËÁË
    ngx_event_t       *rev;
    ngx_event_pipe_t  *p;
    ngx_connection_t  *c;

    c = u->peer.connection;
    p = u->pipe;
    rev = c->read;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http upstream process upstream");

    c->log->action = "reading upstream";

    if (rev->timedout) { //ÔÚ·¢ËÍÇëÇóµ½ºó¶ËµÄÊ±ºò£¬ÎÒÃÇĞèÒªµÈ´ı¶Ô·½Ó¦´ğ£¬Òò´ËÉèÖÃÁË¶Á³¬Ê±¶¨Ê±Æ÷£¬¼ûngx_http_upstream_send_request

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

    } else {//ÇëÇóÃ»ÓĞ³¬Ê±£¬ÄÇÃ´¶Ôºó¶Ë£¬´¦ÀíÒ»ÏÂ¶ÁÊÂ¼ş¡£ngx_event_pipe¿ªÊ¼´¦Àí

        if (rev->delayed) {

            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http upstream delayed");

            if (ngx_handle_read_event(rev, 0, NGX_FUNC_LINE) != NGX_OK) {
                ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            }

            return;
        }

        if (ngx_event_pipe(p, 0) == NGX_ABORT) { //×¢ÒâÕâÀïµÄdo_writeÎª0
            ngx_http_upstream_finalize_request(r, u, NGX_ERROR);
            return;
        }
    }
    //×¢Òâ×ßµ½ÕâÀïµÄÊ±ºò£¬ºó¶Ë·¢ËÍµÄÍ·²¿ĞĞĞÅÏ¢ÒÑ¾­ÔÚÇ°ÃæµÄngx_http_upstream_send_response->ngx_http_send_headerÒÑ¾­°ÑÍ·²¿ĞĞ²¿·Ö·¢ËÍ¸ø¿Í»§¶ËÁË
    //¸Ãº¯Êı´¦ÀíµÄÖ»ÊÇºó¶Ë·Å»Ø¹ıÀ´µÄÍøÒ³°üÌå²¿·Ö

    ngx_http_upstream_process_request(r, u);
}

//ngx_http_upstream_init_request->ngx_http_upstream_cache ¿Í»§¶Ë»ñÈ¡»º´æ ºó¶ËÓ¦´ğ»ØÀ´Êı¾İºóÔÚngx_http_file_cache_createÖĞ´´½¨ÁÙÊ±ÎÄ¼ş
//ºó¶Ë»º´æÎÄ¼ş´´½¨ÔÚngx_http_upstream_send_response£¬ºó¶ËÓ¦´ğÊı¾İÔÚngx_http_upstream_send_response->ngx_http_upstream_process_request->ngx_http_file_cache_updateÖĞ½øĞĞ»º´æ
static void
ngx_http_upstream_process_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u)
{//×¢Òâ×ßµ½ÕâÀïµÄÊ±ºò£¬ºó¶Ë·¢ËÍµÄÍ·²¿ĞĞĞÅÏ¢ÒÑ¾­ÔÚÇ°ÃæµÄngx_http_upstream_send_response->ngx_http_send_headerÒÑ¾­°ÑÍ·²¿ĞĞ²¿·Ö·¢ËÍ¸ø¿Í»§¶ËÁË
//¸Ãº¯Êı´¦ÀíµÄÖ»ÊÇºó¶Ë·Å»Ø¹ıÀ´µÄÍøÒ³°üÌå²¿·Ö
    ngx_temp_file_t   *tf;
    ngx_event_pipe_t  *p;

    p = u->pipe;

    if (u->peer.connection) {

        if (u->store) {

            if (p->upstream_eof || p->upstream_done) { //±¾´ÎÄÚºË»º³åÇøÊı¾İ¶ÁÈ¡Íê±Ï£¬»òÕßºó¶ËËùÓĞÊı¾İ¶ÁÈ¡Íê±Ï

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
        tf = p->temp_file;

        if(r->cache) {
        ngx_log_debugall(r->connection->log, 0, "ngx http cache, p->length:%O, u->headers_in.content_length_n:%O, "
            "tf->offset:%O, r->cache->body_start:%ui", p->length, u->headers_in.content_length_n,
                tf->offset, r->cache->body_start);
        } else {
            ngx_log_debugall(r->connection->log, 0, "ngx http cache, p->length:%O, u->headers_in.content_length_n:%O, "
            "tf->offset:%O", p->length, u->headers_in.content_length_n,
                tf->offset);
        }
        
        /*
          ÔÚNginxÊÕµ½ºó¶Ë·şÎñÆ÷µÄÏìÓ¦Ö®ºó£¬»á°ÑÕâ¸öÏìÓ¦·¢»Ø¸øÓÃ»§¡£¶øÈç¹û»º´æ¹¦ÄÜÆôÓÃµÄ»°£¬Nginx¾Í»á°ÑÏìÓ¦´æÈë´ÅÅÌÀï¡£
          */ //ºó¶ËÓ¦´ğÊı¾İÔÚngx_http_upstream_process_request->ngx_http_file_cache_updateÖĞ½øĞĞ»º´æ
        if (u->cacheable) { //ÊÇ·ñÒª»º´æ£¬¼´proxy_no_cacheÖ¸Áî  

            if (p->upstream_done) { //ºó¶ËÊı¾İÒÑ¾­¶ÁÈ¡Íê±Ï,Ğ´Èë»º´æ
                ngx_http_file_cache_update(r, p->temp_file);

            } else if (p->upstream_eof) { //p->upstream->recv_chain(p->upstream, chain, limit);·µ»Ø0µÄÊ±ºòÖÃ1
                        
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
        size_t upstream_done = p->upstream_done;
        size_t upstream_eof = p->upstream_eof;
        size_t upstream_error = p->upstream_error;
        size_t downstream_error = p->downstream_error;
          
        ngx_log_debugall(r->connection->log, 0, "ngx http cache, upstream_done:%z, upstream_eof:%z, "
            "upstream_error:%z, downstream_error:%z", upstream_done, upstream_eof, 
            upstream_error, downstream_error);
            
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

//Èç¹û²âÊÔÊ§°Ü£¬µ÷ÓÃngx_http_upstream_nextº¯Êı£¬Õâ¸öº¯Êı¿ÉÄÜÔÙ´Îµ÷ÓÃpeer.getµ÷ÓÃ±ğµÄºó¶Ë·şÎñÆ÷½øĞĞÁ¬½Ó¡£
static void // ngx_http_upstream_next ·½·¨³¢ÊÔÓëÆäËûÉÏÓÎ·şÎñÆ÷½¨Á¢Á¬½Ó  Ê×ÏÈĞèÒª¸ù¾İºó¶Ë·µ»ØµÄstatusºÍ³¬Ê±µÈĞÅÏ¢À´ÅĞ¶ÏÊÇ·ñĞèÒªÖØĞÂÁ¬½ÓÏÂÒ»¸öºó¶Ë·şÎñÆ÷
ngx_http_upstream_next(ngx_http_request_t *r, ngx_http_upstream_t *u,
    ngx_uint_t ft_type) //ºÍºó¶ËÄ³¸ö·şÎñÆ÷½»»¥³ö´í(ÀıÈçconnect)£¬ÔòÑ¡ÔñÏÂÒ»¸öºó¶Ë·şÎñÆ÷£¬Í¬Ê±±ê¼Ç¸Ã·şÎñÆ÷³ö´í
{
    ngx_msec_t  timeout;
    ngx_uint_t  status, state;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http next upstream, %xi", ft_type);

    if (u->peer.sockaddr) {

        if (ft_type == NGX_HTTP_UPSTREAM_FT_HTTP_403
            || ft_type == NGX_HTTP_UPSTREAM_FT_HTTP_404) //ºó¶Ë·şÎñÆ÷¾Ü¾ø·şÎñ£¬±íÊ¾»¹ÊÇ¿ÉÓÃµÄ£¬Ö»ÊÇ¾Ü¾øÁËµ±Ç°ÇëÇó
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
            || (timeout && ngx_current_msec - u->peer.start_time >= timeout))  //ÅĞ¶ÏÊÇ·ñĞèÒªÖØĞÂÁ¬½ÓÏÂÒ»¸öºó¶Ë·şÎñÆ÷£¬²»ĞèÒªÔòÖ±½Ó·µ»Ø´íÎó¸ø¿Í»§¶Ë
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

    ngx_http_upstream_connect(r, u);//ÔÙ´Î·¢ÆğÁ¬½Ó
}


static void
ngx_http_upstream_cleanup(void *data)
{
    ngx_http_request_t *r = data;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "cleanup http upstream request: \"%V\"", &r->uri);

    ngx_http_upstream_finalize_request(r, r->upstream, NGX_DONE);
}

//ngx_http_upstream_create´´½¨ngx_http_upstream_t£¬×ÊÔ´»ØÊÕÓÃngx_http_upstream_finalize_request
static void
ngx_http_upstream_finalize_request(ngx_http_request_t *r,
    ngx_http_upstream_t *u, ngx_int_t rc)
{
    ngx_uint_t  flush;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "finalize http upstream request rc: %i", rc);

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

    if (u->peer.connection) { //Èç¹ûÊÇÉèÖÃÁËkeepalive numÅäÖÃ£¬ÔòÔÚngx_http_upstream_free_keepalive_peerÖĞ»á°Ñu->peer.connectionÖÃÎªNULL,±ÜÃâ¹Ø±ÕÁ¬½Ó£¬»º´æÆğÀ´±ÜÃâÖØ¸´½¨Á¢ºÍ¹Ø±ÕÁ¬½Ó

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

    if (r->header_only) { //Ö»·¢ËÍÍ·²¿ĞĞ£¬ÇëÇóºó¶ËµÄÍ·²¿ĞĞÔÚngx_http_upstream_send_response->ngx_http_send_headerÒÑ¾­·¢ËÍ
        ngx_http_finalize_request(r, rc);
        return;
    }

    if (rc == 0) { //ËµÃ÷ÊÇNGX_OK
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

    if (*ph == NULL) { //Õâ¸ö¾ÍÊÇ¼á³Ör->upstream->headers_inÖĞµÄoffset³ÉÔ±´¦ÊÇ·ñÓĞ´æ·Å¸Ãngx_table_elt_tµÄ¿Õ¼ä
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

//°Ñ×Ö·û´®Ê±¼ä"2014-12-22 12:03:44"×ª»»Îªtime_tÊ±¼ä´ê
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

    //Èç¹ûCache-Control²ÎÊıÖµÎªno-cache¡¢no-store¡¢privateÖĞÈÎÒâÒ»¸öÊ±£¬Ôò²»»º´æ...²»»º´æ...
    if (ngx_strlcasestrn(start, last, (u_char *) "no-cache", 8 - 1) != NULL
        || ngx_strlcasestrn(start, last, (u_char *) "no-store", 8 - 1) != NULL
        || ngx_strlcasestrn(start, last, (u_char *) "private", 7 - 1) != NULL)
    {
        u->cacheable = 0;
        return NGX_OK;
    }

    
    //Èç¹ûCache-Control²ÎÊıÖµÎªmax-ageÊ±£¬»á±»»º´æ£¬ÇÒnginxÉèÖÃµÄcacheµÄ¹ıÆÚÊ±¼ä£¬¾ÍÊÇÏµÍ³µ±Ç°Ê±¼ä + mag-ageµÄÖµ
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

//²Î¿¼http://blog.csdn.net/clh604/article/details/9064641
static ngx_int_t
ngx_http_upstream_process_accel_expires(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset)
{
    ngx_http_upstream_t  *u;

    u = r->upstream;
    u->headers_in.x_accel_expires = h; //ºó¶ËĞ¯´øÓĞ"x_accel_expires"Í·²¿ĞĞ

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

//ºó¶Ë·µ»ØµÄÍ·²¿ĞĞ´øÓĞvary:xxx  ngx_http_upstream_process_vary
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

//Content-Type:text/html;charset=ISO-8859-1½âÎö±àÂë·½Ê½´æµ½r->headers_out.charset
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

//Èç¹û½ÓÊÜµ½µÄºó¶ËÍ·²¿ĞĞÖĞÖ¸¶¨ÓĞlocation:xxxÍ·²¿ĞĞ£¬ÔòĞèÒª½øĞĞÖØ¶¨Ïò£¬²Î¿¼proxy_redirect
/*
location /proxy1/ {			
    proxy_pass  http://10.10.0.103:8080/; 		
}

Èç¹ûurlÎªhttp://10.2.13.167/proxy1/£¬Ôòngx_http_upstream_rewrite_location´¦Àíºó£¬
ºó¶Ë·µ»ØLocation: http://10.10.0.103:8080/secure/MyJiraHome.jspa
ÔòÊµ¼Ê·¢ËÍ¸øä¯ÀÀÆ÷¿Í»§¶ËµÄheaders_out.headers.locationÎªhttp://10.2.13.167/proxy1/secure/MyJiraHome.jspa
*/
static ngx_int_t
ngx_http_upstream_rewrite_location(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset) //ngx_http_upstream_headers_inÖĞµÄ³ÉÔ±copy_handler
{
    ngx_int_t         rc;
    ngx_table_elt_t  *ho;

    ho = ngx_list_push(&r->headers_out.headers);
    if (ho == NULL) {
        return NGX_ERROR;
    }

    *ho = *h;

    if (r->upstream->rewrite_redirect) {
        rc = r->upstream->rewrite_redirect(r, ho, 0); //ngx_http_proxy_rewrite_redirect

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

  ÉèÖÃÔÚfail_timeout²ÎÊıÉèÖÃµÄÊ±¼äÄÚ×î´óÊ§°Ü´ÎÊı£¬Èç¹ûÔÚÕâ¸öÊ±¼äÄÚ£¬ËùÓĞÕë¶Ô¸Ã·şÎñÆ÷µÄÇëÇó
  ¶¼Ê§°ÜÁË£¬ÄÇÃ´ÈÏÎª¸Ã·şÎñÆ÷»á±»ÈÏÎªÊÇÍ£»úÁË£¬Í£»úÊ±¼äÊÇfail_timeoutÉèÖÃµÄÊ±¼ä¡£Ä¬ÈÏÇé¿öÏÂ£¬
  ²»³É¹¦Á¬½ÓÊı±»ÉèÖÃÎª1¡£±»ÉèÖÃÎªÁãÔò±íÊ¾²»½øĞĞÁ´½ÓÊıÍ³¼Æ¡£ÄÇĞ©Á¬½Ó±»ÈÏÎªÊÇ²»³É¹¦µÄ¿ÉÒÔÍ¨¹ı
  proxy_next_upstream, fastcgi_next_upstream£¬ºÍmemcached_next_upstreamÖ¸ÁîÅäÖÃ¡£http_404
  ×´Ì¬²»»á±»ÈÏÎªÊÇ²»³É¹¦µÄ³¢ÊÔ¡£

fail_time=time
  ÉèÖÃ ¶à³¤Ê±¼äÄÚÊ§°Ü´ÎÊı´ïµ½×î´óÊ§°Ü´ÎÊı»á±»ÈÏÎª·şÎñÆ÷Í£»úÁË·şÎñÆ÷»á±»ÈÏÎªÍ£»úµÄÊ±¼ä³¤¶È Ä¬ÈÏÇé¿öÏÂ£¬³¬Ê±Ê±¼ä±»ÉèÖÃÎª10S

*/
static char *
ngx_http_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{//µ±Åöµ½upstream{}Ö¸ÁîµÄÊ±ºòµ÷ÓÃÕâÀï¡£
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
    u.host = value[1]; //upstream backend { }ÖĞµÄbackend
    u.no_resolve = 1;
    u.no_port = 1;

    //ÏÂÃæ½«u´ú±íµÄÊı¾İÉèÖÃµ½umcf->upstreamsÀïÃæÈ¥¡£È»ºó·µ»Ø¶ÔÓ¦µÄupstream{}½á¹¹Êı¾İÖ¸Õë¡£
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
    ctx->main_conf = http_ctx->main_conf; //»ñÈ¡¸Ãupstream xxx{}Ëù´¦µÄhttp{}

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

    //¸Ãupstream{}ÖĞ¿ÉÒÔÅäÖÃËùÓĞµÄloc¼¶±ğÄ£¿éµÄÅäÖÃĞÅÏ¢£¬Òò´ËÎªÃ¿¸öÄ£¿é´´½¨¶ÔÓ¦µÄ´æ´¢¿Õ¼ä
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

    pcf = *cf;   //±£´æupstream{}Ëù´¦µÄctx
    cf->ctx = ctx;//ÁÙÊ±ÇĞ»»ctx£¬½øÈëupstream{}¿éÖĞ½øĞĞ½âÎö¡£
    cf->cmd_type = NGX_HTTP_UPS_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pcf; //upstream{}ÄÚ²¿ÅäÖÃ½âÎöÍê±Ïºó£¬»Ö¸´µ½Ö®Ç°µÄcf

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

        if (ngx_parse_url(cf->pool, u) != NGX_OK) { //½âÎöuri£¬Èç¹ûuriÊÇIP:PORTĞÎÊ½Ôò»ñÈ¡ËûÃÇ£¬Èç¹ûÊÇÓòÃûwww.xxx.comĞÎÊ½£¬Ôò½âÎöÓòÃû
            if (u->err) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "%s in upstream \"%V\"", u->err, &u->url);
            }

            return NULL;
        }
    }

    umcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_upstream_module);

    uscfp = umcf->upstreams.elts;

    //±éÀúµ±Ç°µÄupstream£¬Èç¹ûÓĞÖØ¸´µÄ£¬Ôò±È½ÏÆäÏà¹ØµÄ×Ö¶Î£¬²¢´òÓ¡ÈÕÖ¾¡£Èç¹ûÕÒµ½ÏàÍ¬µÄ£¬Ôò·µ»Ø¶ÔÓ¦Ö¸Õë¡£Ã»ÕÒµ½ÔòÔÚºóÃæ´´½¨
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

        return uscfp[i];//ÕÒµ½ÏàÍ¬µÄÅäÖÃÊı¾İÁË£¬Ö±½Ó·µ»ØËüµÄÖ¸Õë¡£
    }

    uscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_srv_conf_t));
    if (uscf == NULL) {
        return NULL;
    }

    uscf->flags = flags;
    uscf->host = u->host;
    uscf->file_name = cf->conf_file->file.name.data; //ÅäÖÃÎÄ¼şÃû³Æ
    uscf->line = cf->conf_file->line;
    uscf->port = u->port;
    uscf->default_port = u->default_port;
    uscf->no_port = u->no_port;

    //±ÈÈç: server xx.xx.xx.xx:xx weight=2 max_fails=3;  ¸Õ¿ªÊ¼£¬ngx_http_upstream»áµ÷ÓÃ±¾º¯Êı¡£µ«ÊÇÆänaddres=0.
    if (u->naddrs == 1 && (u->port || u->family == AF_UNIX)) {
        uscf->servers = ngx_array_create(cf->pool, 1,
                                         sizeof(ngx_http_upstream_server_t));
        if (uscf->servers == NULL) {
            return NULL;
        }

        us = ngx_array_push(uscf->servers);//¼ÇÂ¼±¾upstream{}¿éµÄËùÓĞserverÖ¸Áî¡£
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

//proxy_bind  fastcgi_bind
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

//fastcgi_param  ParamsÊı¾İ°ü£¬ÓÃÓÚ´«µİÖ´ĞĞÒ³ÃæËùĞèÒªµÄ²ÎÊıºÍ»·¾³±äÁ¿¡£
char *
ngx_http_upstream_param_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf) //uboot.din
{
    char  *p = conf;

    ngx_str_t                   *value;
    ngx_array_t                **a;
    ngx_http_upstream_param_t   *param;

    //fastcgi_paramÉèÖÃµÄ´«ËÍµ½FastCGI·şÎñÆ÷µÄÏà¹Ø²ÎÊı¶¼Ìí¼Óµ½¸ÃÊı×éÖĞ£¬¼ûngx_http_upstream_param_set_slot
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

        param->skip_empty = 1; //ºÍngx_http_fastcgi_init_paramsÅäºÏÔÄ¶Á£¬Èç¹ûÉèÖÃÁË¸ÃÖµ£¬²¢ÇÒvalue²¿·ÖÎª0£¬ÔòÖ±½Ó²»Ê¹ÓÃ´Ë±äÁ¿
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
            && ((conf->cache == 0) == (prev->cache == 0)) //ÒÑ¾­×ö¹ıhashÔËËãÁË
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

    for (h = default_hide_headers; h->len; h++) { //°Ñdefault_hide_headersÖĞµÄÔªËØ¸³Öµ¸øhide_headersÊı×éÖĞ
        hk = ngx_array_push(&hide_headers);
        if (hk == NULL) {
            return NGX_ERROR;
        }

        hk->key = *h;
        hk->key_hash = ngx_hash_key_lc(h->data, h->len);
        hk->value = (void *) 1;
    }

    if (conf->hide_headers != NGX_CONF_UNSET_PTR) { //proxy_hide_header  fastcgi_hide_headerÅäÖÃµÄÏà¹ØĞÅÏ¢Ò²ÒªÌí¼Óµ½hide_headersÊı×é

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

    //Èç¹ûhide_headersÓĞÏà¹ØĞÅÏ¢£¬±íÊ¾ĞèÒªÓ°²Ø£¬µ¥xxx_pass_headerÖĞÓĞÉèÖÃÁË²»Òş²Ø£¬ÔòÄ¬ÈÏ¸ÃĞÅÏ¢»¹ÊÇÓ°²Ø£¬°Ñpass_headerÖĞµÄ¸ÃÏîÈ¥µô
    if (conf->pass_headers != NGX_CONF_UNSET_PTR) { //proxy_pass_headers  fastcgi_pass_headersÅäÖÃµÄÏà¹ØĞÅÏ¢´Óhide_headersÊı×é

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

    //°Ñdefault_hide_headers(ngx_http_proxy_hide_headers  ngx_http_fastcgi_hide_headers)ÖĞµÄ³ÉÔ±×öhash±£´æµ½conf->hide_headers_hash
    hash->hash = &conf->hide_headers_hash; //°ÑÄ¬ÈÏµÄdefault_hide_headers  xxx_pass_headersÅäÖÃµÄ 
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

        if (init(cf, uscfp[i]) != NGX_OK) { //Ö´ĞĞinit_upstream
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

