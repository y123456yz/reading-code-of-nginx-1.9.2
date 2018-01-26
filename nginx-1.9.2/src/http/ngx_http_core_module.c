
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    u_char    *name;
    uint32_t   method;
} ngx_http_method_name_t;

//client_body_in_file_only  on | off | clean
#define NGX_HTTP_REQUEST_BODY_FILE_OFF    0
#define NGX_HTTP_REQUEST_BODY_FILE_ON     1
#define NGX_HTTP_REQUEST_BODY_FILE_CLEAN  2


static ngx_int_t ngx_http_core_find_location(ngx_http_request_t *r);
static ngx_int_t ngx_http_core_find_static_location(ngx_http_request_t *r,
    ngx_http_location_tree_node_t *node);

static ngx_int_t ngx_http_core_preconfiguration(ngx_conf_t *cf);
static ngx_int_t ngx_http_core_postconfiguration(ngx_conf_t *cf);
static void *ngx_http_core_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_core_init_main_conf(ngx_conf_t *cf, void *conf);
static void *ngx_http_core_create_srv_conf(ngx_conf_t *cf);
static char *ngx_http_core_merge_srv_conf(ngx_conf_t *cf,
    void *parent, void *child);
static void *ngx_http_core_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_core_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);

static char *ngx_http_core_server(ngx_conf_t *cf, ngx_command_t *cmd,
    void *dummy);
static char *ngx_http_core_location(ngx_conf_t *cf, ngx_command_t *cmd,
    void *dummy);
static ngx_int_t ngx_http_core_regex_location(ngx_conf_t *cf,
    ngx_http_core_loc_conf_t *clcf, ngx_str_t *regex, ngx_uint_t caseless);

static char *ngx_http_core_types(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_type(ngx_conf_t *cf, ngx_command_t *dummy,
    void *conf);

static char *ngx_http_core_listen(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_server_name(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_root(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_core_limit_except(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_set_aio(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_directio(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_error_page(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_try_files(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_open_file_cache(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_error_log(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_keepalive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_internal(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_resolver(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#if (NGX_HTTP_GZIP)
static ngx_int_t ngx_http_gzip_accept_encoding(ngx_str_t *ae);
static ngx_uint_t ngx_http_gzip_quantity(u_char *p, u_char *last);
static char *ngx_http_gzip_disable(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#endif
static ngx_int_t ngx_http_get_forwarded_addr_internal(ngx_http_request_t *r,
    ngx_addr_t *addr, u_char *xff, size_t xfflen, ngx_array_t *proxies,
    int recursive);
#if (NGX_HAVE_OPENAT)
static char *ngx_http_disable_symlinks(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#endif

static char *ngx_http_core_lowat_check(ngx_conf_t *cf, void *post, void *data);
static char *ngx_http_core_pool_size(ngx_conf_t *cf, void *post, void *data);

static ngx_conf_post_t  ngx_http_core_lowat_post =
    { ngx_http_core_lowat_check };

static ngx_conf_post_handler_pt  ngx_http_core_pool_size_p =
    ngx_http_core_pool_size;


static ngx_conf_enum_t  ngx_http_core_request_body_in_file[] = {
    { ngx_string("off"), NGX_HTTP_REQUEST_BODY_FILE_OFF },
    { ngx_string("on"), NGX_HTTP_REQUEST_BODY_FILE_ON },
    { ngx_string("clean"), NGX_HTTP_REQUEST_BODY_FILE_CLEAN },
    { ngx_null_string, 0 }
};


static ngx_conf_enum_t  ngx_http_core_satisfy[] = {
    { ngx_string("all"), NGX_HTTP_SATISFY_ALL },
    { ngx_string("any"), NGX_HTTP_SATISFY_ANY },
    { ngx_null_string, 0 }
};


static ngx_conf_enum_t  ngx_http_core_lingering_close[] = {
    { ngx_string("off"), NGX_HTTP_LINGERING_OFF },
    { ngx_string("on"), NGX_HTTP_LINGERING_ON },
    { ngx_string("always"), NGX_HTTP_LINGERING_ALWAYS },
    { ngx_null_string, 0 }
};


static ngx_conf_enum_t  ngx_http_core_if_modified_since[] = {
    { ngx_string("off"), NGX_HTTP_IMS_OFF },
    { ngx_string("exact"), NGX_HTTP_IMS_EXACT },
    { ngx_string("before"), NGX_HTTP_IMS_BEFORE },
    { ngx_null_string, 0 }
};


static ngx_conf_bitmask_t  ngx_http_core_keepalive_disable[] = {
    { ngx_string("none"), NGX_HTTP_KEEPALIVE_DISABLE_NONE },
    { ngx_string("msie6"), NGX_HTTP_KEEPALIVE_DISABLE_MSIE6 },
    { ngx_string("safari"), NGX_HTTP_KEEPALIVE_DISABLE_SAFARI },
    { ngx_null_string, 0 }
};


static ngx_path_init_t  ngx_http_client_temp_path = {
    ngx_string(NGX_HTTP_CLIENT_TEMP_PATH), { 0, 0, 0 }
};


#if (NGX_HTTP_GZIP)

static ngx_conf_enum_t  ngx_http_gzip_http_version[] = {
    { ngx_string("1.0"), NGX_HTTP_VERSION_10 },
    { ngx_string("1.1"), NGX_HTTP_VERSION_11 },
    { ngx_null_string, 0 }
};


static ngx_conf_bitmask_t  ngx_http_gzip_proxied_mask[] = {
    { ngx_string("off"), NGX_HTTP_GZIP_PROXIED_OFF },
    { ngx_string("expired"), NGX_HTTP_GZIP_PROXIED_EXPIRED },
    { ngx_string("no-cache"), NGX_HTTP_GZIP_PROXIED_NO_CACHE },
    { ngx_string("no-store"), NGX_HTTP_GZIP_PROXIED_NO_STORE },
    { ngx_string("private"), NGX_HTTP_GZIP_PROXIED_PRIVATE },
    { ngx_string("no_last_modified"), NGX_HTTP_GZIP_PROXIED_NO_LM },
    { ngx_string("no_etag"), NGX_HTTP_GZIP_PROXIED_NO_ETAG },
    { ngx_string("auth"), NGX_HTTP_GZIP_PROXIED_AUTH },
    { ngx_string("any"), NGX_HTTP_GZIP_PROXIED_ANY },
    { ngx_null_string, 0 }
};


static ngx_str_t  ngx_http_gzip_no_cache = ngx_string("no-cache");
static ngx_str_t  ngx_http_gzip_no_store = ngx_string("no-store");
static ngx_str_t  ngx_http_gzip_private = ngx_string("private");

#endif

//Ïà¹ØÅäÖÃ¼ûngx_event_core_commands ngx_http_core_commands ngx_stream_commands ngx_http_core_commands ngx_core_commands  ngx_mail_commands
static ngx_command_t  ngx_http_core_commands[] = {
    /* È·¶¨Í°µÄ¸öÊý£¬Ô½´ó³åÍ»¸ÅÂÊÔ½Ð¡¡£variables_hash_max_sizeÊÇÃ¿¸öÍ°ÖÐ¶ÔÓ¦µÄÉ¢ÁÐÐÅÏ¢¸öÊý 
        ÅäÖÃ¿é:http server location
     */
    { ngx_string("variables_hash_max_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_core_main_conf_t, variables_hash_max_size),
      NULL },

    /* server_names_hash_max_size 32 | 64 |128 £¬ÎªÁËÌá¸öÑ°ÕÒserver_nameµÄÄÜÁ¦£¬nginxÊ¹ÓÃÉ¢ÁÐ±íÀ´´æ´¢server name¡£
        Õâ¸öÊÇÉèÖÃÃ¿¸öÉ¢ÁÐÍ°Õ¦ÅªµÄÄÚ´æ´óÐ¡£¬×¢ÒâºÍvariables_hash_max_sizeÇø±ð
        ÅäÖÃ¿é:http server location
    */
    { ngx_string("variables_hash_bucket_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_core_main_conf_t, variables_hash_bucket_size),
      NULL },
    /* server_names_hash_max_size 32 | 64 |128 £¬ÎªÁËÌá¸öÑ°ÕÒserver_nameµÄÄÜÁ¦£¬nginxÊ¹ÓÃÉ¢ÁÐ±íÀ´´æ´¢server name¡£
    */
    { ngx_string("server_names_hash_max_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_core_main_conf_t, server_names_hash_max_size),
      NULL },

    { ngx_string("server_names_hash_bucket_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_core_main_conf_t, server_names_hash_bucket_size),
      NULL },

    { ngx_string("server"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_http_core_server,
      0,
      0,
      NULL },

    /*
    connection_pool_size
    Óï·¨£ºconnection_pool_size size;
    Ä¬ÈÏ£ºconnection_pool_size 256;
    ÅäÖÃ¿é£ºhttp¡¢server
    Nginx¶ÔÓÚÃ¿¸ö½¨Á¢³É¹¦µÄTCPÁ¬½Ó»áÔ¤ÏÈ·ÖÅäÒ»¸öÄÚ´æ³Ø£¬ÉÏÃæµÄsizeÅäÖÃÏî½«Ö¸¶¨Õâ¸öÄÚ´æ³ØµÄ³õÊ¼´óÐ¡£¨¼´ngx_connection_t½á¹¹ÌåÖÐµÄpoolÄÚ´æ³Ø³õÊ¼´óÐ¡£¬
    9.8.1½Ú½«½éÉÜÕâ¸öÄÚ´æ³ØÊÇºÎÊ±·ÖÅäµÄ£©£¬ÓÃÓÚ¼õÉÙÄÚºË¶ÔÓÚÐ¡¿éÄÚ´æµÄ·ÖÅä´ÎÊý¡£ÐèÉ÷ÖØÉèÖÃ£¬ÒòÎª¸ü´óµÄsize»áÊ¹·þÎñÆ÷ÏûºÄµÄÄÚ´æÔö¶à£¬¶ø¸üÐ¡µÄsizeÔò»áÒý·¢¸ü¶àµÄÄÚ´æ·ÖÅä´ÎÊý¡£
    */
    { ngx_string("connection_pool_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, connection_pool_size),
      &ngx_http_core_pool_size_p },

/*
Óï·¨£ºrequest_pool_size size;
Ä¬ÈÏ£ºrequest_pool_size 4k;
ÅäÖÃ¿é£ºhttp¡¢server
Nginx¿ªÊ¼´¦ÀíHTTPÇëÇóÊ±£¬½«»áÎªÃ¿¸öÇëÇó¶¼·ÖÅäÒ»¸öÄÚ´æ³Ø£¬sizeÅäÖÃÏî½«Ö¸¶¨Õâ¸öÄÚ´æ³ØµÄ³õÊ¼´óÐ¡£¨¼´ngx_http_request_t½á¹¹ÌåÖÐµÄpoolÄÚ´æ³Ø³õÊ¼´óÐ¡£¬
11.3½Ú½«½éÉÜÕâ¸öÄÚ´æ³ØÊÇºÎÊ±·ÖÅäµÄ£©£¬ÓÃÓÚ¼õÉÙÄÚºË¶ÔÓÚÐ¡¿éÄÚ´æµÄ·ÖÅä´ÎÊý¡£TCPÁ¬½Ó¹Ø±ÕÊ±»áÏú»Ùconnection_pool_sizeÖ¸¶¨µÄÁ¬½ÓÄÚ´æ³Ø£¬HTTPÇëÇó½áÊø
Ê±»áÏú»Ùrequest_pool_sizeÖ¸¶¨µÄHTTPÇëÇóÄÚ´æ³Ø£¬µ«ËüÃÇµÄ´´½¨¡¢Ïú»ÙÊ±¼ä²¢²»Ò»ÖÂ£¬ÒòÎªÒ»¸öTCPÁ¬½Ó¿ÉÄÜ±»¸´ÓÃÓÚ¶à¸öHTTPÇëÇó¡£
*/
    { ngx_string("request_pool_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, request_pool_size),
      &ngx_http_core_pool_size_p },
    /*
    ¶ÁÈ¡HTTPÍ·²¿µÄ³¬Ê±Ê±¼ä
    Óï·¨£ºclient_header_timeout time£¨Ä¬ÈÏµ¥Î»£ºÃë£©;
    Ä¬ÈÏ£ºclient_header_timeout 60;
    ÅäÖÃ¿é£ºhttp¡¢server¡¢location
    ¿Í»§¶ËÓë·þÎñÆ÷½¨Á¢Á¬½Óºó½«¿ªÊ¼½ÓÊÕHTTPÍ·²¿£¬ÔÚÕâ¸ö¹ý³ÌÖÐ£¬Èç¹ûÔÚÒ»¸öÊ±¼ä¼ä¸ô£¨³¬Ê±Ê±¼ä£©ÄÚÃ»ÓÐ¶ÁÈ¡µ½¿Í»§¶Ë·¢À´µÄ×Ö½Ú£¬ÔòÈÏÎª³¬Ê±£¬
    ²¢Ïò¿Í»§¶Ë·µ»Ø408 ("Request timed out")ÏìÓ¦¡£
    */
    { ngx_string("client_header_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, client_header_timeout),
      NULL },

/*
Óï·¨:  client_header_buffer_size size;
Ä¬ÈÏÖµ:  client_header_buffer_size 1k;
ÉÏÏÂÎÄ:  http, server

ÉèÖÃ¶ÁÈ¡¿Í»§¶ËÇëÇóÍ·²¿µÄ»º³åÈÝÁ¿¡£ ¶ÔÓÚ´ó¶àÊýÇëÇó£¬1KµÄ»º³å×ãÒÓ¡£ µ«Èç¹ûÇëÇóÖÐº¬ÓÐµÄcookieºÜ³¤£¬»òÕßÇëÇóÀ´×ÔWAPµÄ¿Í»§¶Ë£¬¿ÉÄÜ
ÇëÇóÍ·²»ÄÜ·ÅÔÚ1KµÄ»º³åÖÐ¡£ Èç¹û´ÓÇëÇóÐÐ£¬»òÕßÄ³¸öÇëÇóÍ·¿ªÊ¼²»ÄÜÍêÕûµÄ·ÅÔÚÕâ¿é¿Õ¼äÖÐ£¬ÄÇÃ´nginx½«°´ÕÕ large_client_header_buffers
Ö¸ÁîµÄÅäÖÃ·ÖÅä¸ü¶à¸ü´óµÄ»º³åÀ´´æ·Å¡£
*/
    { ngx_string("client_header_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, client_header_buffer_size),
      NULL },

/*
´æ´¢³¬´óHTTPÍ·²¿µÄÄÚ´æbuffer´óÐ¡
Óï·¨£ºlarge_client_header_buffers number size;
Ä¬ÈÏ£ºlarge_client_header_buffers 4 8k;
ÅäÖÃ¿é£ºhttp¡¢server
large_client_header_buffers¶¨ÒåÁËNginx½ÓÊÕÒ»¸ö³¬´óHTTPÍ·²¿ÇëÇóµÄbuffer¸öÊýºÍÃ¿¸öbufferµÄ´óÐ¡¡£Èç¹ûHTTPÇëÇóÐÐ£¨ÈçGET /index HTTP/1.1£©
µÄ´óÐ¡³¬¹ýÉÏÃæµÄµ¥¸öbuffer£¬Ôò·µ»Ø"Request URI too large" (414)¡£ÇëÇóÖÐÒ»°ã»áÓÐÐí¶àheader£¬Ã¿Ò»¸öheaderµÄ´óÐ¡Ò²²»ÄÜ³¬¹ýµ¥¸öbufferµÄ´óÐ¡£¬
·ñÔò»á·µ»Ø"Bad request" (400)¡£µ±È»£¬ÇëÇóÐÐºÍÇëÇóÍ·²¿µÄ×ÜºÍÒ²²»¿ÉÒÔ³¬¹ýbuffer¸öÊý*buffer´óÐ¡¡£

ÉèÖÃ¶ÁÈ¡¿Í»§¶ËÇëÇó³¬´óÇëÇóµÄ»º³å×î´ónumber(ÊýÁ¿)ºÍÃ¿¿é»º³åµÄsize(ÈÝÁ¿)¡£ HTTPÇëÇóÐÐµÄ³¤¶È²»ÄÜ³¬¹ýÒ»¿é»º³åµÄÈÝÁ¿£¬·ñÔònginx·µ»Ø´íÎó414
(Request-URI Too Large)µ½¿Í»§¶Ë¡£ Ã¿¸öÇëÇóÍ·µÄ³¤¶ÈÒ²²»ÄÜ³¬¹ýÒ»¿é»º³åµÄÈÝÁ¿£¬·ñÔònginx·µ»Ø´íÎó400 (Bad Request)µ½¿Í»§¶Ë¡£ »º³å½öÔÚ±ØÐè
ÊÇ²Å·ÖÅä£¬Ä¬ÈÏÃ¿¿éµÄÈÝÁ¿ÊÇ8K×Ö½Ú¡£ ¼´Ê¹nginx´¦ÀíÍêÇëÇóºóÓë¿Í»§¶Ë±£³ÖÈë³¤Á¬½Ó£¬nginxÒ²»áÊÍ·ÅÕâÐ©»º³å¡£ 
*/ //µ±client_header_buffer_size²»¹»´æ´¢Í·²¿ÐÐµÄÊ±ºò£¬ÓÃlarge_client_header_buffersÔÙ´Î·ÖÅä¿Õ¼ä´æ´¢
    { ngx_string("large_client_header_buffers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_bufs_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, large_client_header_buffers),
      NULL },

/*
ºöÂÔ²»ºÏ·¨µÄHTTPÍ·²¿
Óï·¨£ºignore_invalid_headers on | off;
Ä¬ÈÏ£ºignore_invalid_headers on;
ÅäÖÃ¿é£ºhttp¡¢server
Èç¹û½«ÆäÉèÖÃÎªoff£¬ÄÇÃ´µ±³öÏÖ²»ºÏ·¨µÄHTTPÍ·²¿Ê±£¬Nginx»á¾Ü¾ø·þÎñ£¬²¢Ö±½ÓÏòÓÃ»§·¢ËÍ400£¨Bad Request£©´íÎó¡£Èç¹û½«ÆäÉèÖÃÎªon£¬Ôò»áºöÂÔ´ËHTTPÍ·²¿¡£
*/
    { ngx_string("ignore_invalid_headers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, ignore_invalid_headers),
      NULL },

/*
merge_slashes
Óï·¨£ºmerge_slashes on | off;
Ä¬ÈÏ£ºmerge_slashes on;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
´ËÅäÖÃÏî±íÊ¾ÊÇ·ñºÏ²¢ÏàÁÚµÄ¡°/¡±£¬ÀýÈç£¬//test///a.txt£¬ÔÚÅäÖÃÎªonÊ±£¬»á½«ÆäÆ¥ÅäÎªlocation /test/a.txt£»Èç¹ûÅäÖÃÎªoff£¬Ôò²»»áÆ¥Åä£¬URI½«ÈÔÈ»ÊÇ//test///a.txt¡£
*/
    { ngx_string("merge_slashes"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, merge_slashes),
      NULL },

/*
HTTPÍ·²¿ÊÇ·ñÔÊÐíÏÂ»­Ïß
Óï·¨£ºunderscores_in_headers on | off;
Ä¬ÈÏ£ºunderscores_in_headers off;
ÅäÖÃ¿é£ºhttp¡¢server
Ä¬ÈÏÎªoff£¬±íÊ¾HTTPÍ·²¿µÄÃû³ÆÖÐ²»ÔÊÐí´ø¡°_¡±£¨ÏÂ»­Ïß£©¡£
*/
    { ngx_string("underscores_in_headers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, underscores_in_headers),
      NULL },
    /*
        location [= ~ ~* ^~ @ ] /uri/ {....} location»á³¢ÊÔ¸ù¾ÝÓÃ»§ÇëÇóÖÐurlÀ´Æ¥ÅäÉÏÃæµÄ/url±í´ïÊ½£¬Èç¹û¿ÉÒÔÆ¥Åä
        ¾ÍÑ¡Ôñlocation{}¿éÖÐµÄÅäÖÃÀ´´¦ÀíÓÃ»§ÇëÇó¡£µ±È»£¬Æ¥Åä·½Ê½ÊÇ¶àÑùµÄ£¬ÈçÏÂ:
        1) = ±íÊ¾°Ñurlµ±×ö×Ö·û´®£¬ÒÔ±ãÓÚ²ÎÊýÖÐµÄurl×öÍêÈ«Æ¥Åä¡£ÀýÈç
            localtion = / {
                #Ö»ÓÐµ±ÓÃ»§ÇëÇóÊÇ/Ê±£¬²Å»áÊ¹ÓÃ¸ÃlocationÏÂµÄÅäÖÃ¡£
            }
        2) ~±íÊ¾Æ¥ÅäurlÊ±ÊÇ×ÖÄ¸´óÐ¡Ð´Ãô¸ÐµÄ¡£
        3) ~*±íÊ¾Æ¥ÅäurlÊ±ºöÂÔ×ÖÄ¸´óÐ¡Ð´ÎÊÌâ
        4) ^~±íÊ¾Æ¥ÅäurlÊ±Ö¸ÐèÒªÆäÇ°°ë²¿·ÖÓëurl²ÎÊýÆ¥Åä¼´¿É£¬ÀýÈç:
            location ^~ /images/ {
                #ÒÔ/images/¿ªÍ¨µÄÇëÇó¶¼»á±»Æ¥ÅäÉÏ
            }
        5) @±íÊ¾½öÓÃÓÚnginx·þÎñÆ÷ÄÚ²¿ÇëÇóÖ®¼äµÄÖØ¶¨Ïò£¬´øÓÐ@µÄlocation²»Ö±½Ó´¦ÀíÓÃ»§ÇëÇó¡£µ±È»£¬ÔÚurl²ÎÊýÀïÊÇ¿ÉÒÔÓÃ
            ÕýÔò±í´ïÊ½µÄ£¬ÀýÈç:
            location ~* \.(gif|jpg|jpeg)$ {
                #Æ¥ÅäÒÔ.gif .jpg .jpeg½áÎ²µÄÇëÇó¡£
            }

        ÉÏÃæÕâÐ©·½Ê½±í´ïÎª"Èç¹ûÆ¥Åä£¬Ôò..."£¬Èç¹ûÒªÊµÏÖ"Èç¹û²»Æ¥Åä£¬Ôò...."£¬¿ÉÒÔÔÚ×îºóÒ»¸ölocationÖÐÊ¹ÓÃ/×÷Îª²ÎÊý£¬Ëü»áÆ¥ÅäËùÓÐ
        µÄHTTPÇëÇó£¬ÕâÑù¾Í¿ÉÒÔ±íÊ¾Èç¹û²»ÄÜÆ¥ÅäÇ°ÃæµÄËùÓÐlocation£¬ÔòÓÉ"/"Õâ¸ölocation´¦Àí¡£ÀýÈç:
            location / {
                # /¿ÉÒÔÆ¥ÅäËùÓÐÇëÇó¡£
            }

         ÍêÈ«Æ¥Åä > Ç°×ºÆ¥Åä > ÕýÔò±í´ïÊ½ > /
    */ //location {}ÅäÖÃ²éÕÒ¿ÉÒÔ²Î¿¼ngx_http_core_find_config_phase->ngx_http_core_find_location
    { ngx_string("location"), 
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_BLOCK|NGX_CONF_TAKE12,
      ngx_http_core_location,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },
    //lisen [ip:port | ip(²»ÖªµÀ¶Ë¿ÚÄ¬ÈÏÎª80) | ¶Ë¿Ú | *:¶Ë¿Ú | localhost:¶Ë¿Ú] [ngx_http_core_listenº¯ÊýÖÐµÄ²ÎÊý]
    { ngx_string("listen"),  //
      NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_core_listen,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },
      
    /* server_name www.a.com www.b.com£¬¿ÉÒÔ¸ú¶à¸ö·þÎñÆ÷ÓòÃû
    ÔÚ¿ªÊ¼´¦ÀíÒ»¸öHTTPÇëÇóÊÇ£¬nginx»áÈ¡³öheaderÍ·ÖÐµÄHost,ÓëÃ¿¸öserverµÄserver_name½øÐÐÆ¥Åä£¬Ò»´Î¾ö¶¨µ½µ×ÓÉÄÇÒ»¸öserver
    ¿éÀ´´¦ÀíÕâ¸öÇëÇó¡£ÓÐ¿ÉÄÜÒ»¸öhostÓë¶à¸öserver¿éÖÐµÄ¶à¸öserver_nameÆ¥Åä£¬ÕâÊÇ¾Í»á¸ù¾ÝÆ¥ÅäÓÅÏÈ¼¶À´Ñ¡ÔñÊµ¼Ê´¦ÀíµÄserver¿é¡£
    server_nameÓëHOstµÄÆ¥ÅäÓÅÏÈ¼¶ÈçÏÂ:
    1 Ê×ÏÈÆ¥Åä×Ö·û´®ÍêÈ«Æ¥ÅäµÄservername,Èçwww.aaa.com
    2 Æä´ÎÑ¡ÔñÍ¨Åä·ûÔÚÇ°ÃæµÄservername,Èç*.aaa.com
    3 ÔÙ´ÎÑ¡ÔñÍ¨Åä·ûÔÚºóÃæµÄservername,Èçwww.aaa.*
    4 ×îÐÂÑ¡ÔñÊ¹ÓÃÕýÔò±í´ïÊ½²ÅÆ¥ÅäµÄservername.

    Èç¹û¶¼²»Æ¥Åä£¬°´ÕÕÏÂÃæË³ÐòÑ¡Ôñ´¦Àíserver¿é:
    1 ÓÅÏÈÑ¡ÔñÔÚlistenÅäÖÃÏîºó¼ÓÈë[default|default_server]µÄserver¿é¡£
    2 ÕÒµ½Æ¥Åälisten¶Ë¿ÚµÄµÚÒ»¸öserver¿é

    Èç¹ûserver_nameºóÃæ¸ú×Å¿Õ×Ö·û´®£¬Èçserver_name ""±íÊ¾Æ¥ÅäÃ»ÓÐhostÕâ¸öHTTPÍ·²¿µÄÇëÇó
    ¸Ã²ÎÊýÄ¬ÈÏÎªserver_name ""
    server_name_in_redirect on | off ¸ÃÅäÖÃÐèÒªÅäºÏserver_nameÊ¹ÓÃ¡£ÔÚÊ¹ÓÃon´ò¿ªºó,±íÊ¾ÔÚÖØ¶¨ÏòÇëÇóÊ±»áÊ¹ÓÃ
    server_nameÀïµÄµÚÒ»¸öÖ÷»úÃû´úÌæÔ­ÏÈÇëÇóÖÐµÄHostÍ·²¿£¬¶øÊ¹ÓÃoff¹Ø±ÕÊ±£¬±íÊ¾ÔÚÖØ¶¨ÏòÇëÇóÊ±Ê¹ÓÃÇëÇó±¾ÉíµÄHOSTÍ·²¿
    */ //¹Ù·½ÏêÏ¸ÎÄµµ²Î¿¼http://nginx.org/en/docs/http/server_names.html
    { ngx_string("server_name"),
      NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_core_server_name,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

/*
types_hash_max_size
Óï·¨£ºtypes_hash_max_size size;
Ä¬ÈÏ£ºtypes_hash_max_size 1024;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
types_hash_max_sizeÓ°ÏìÉ¢ÁÐ±íµÄ³åÍ»ÂÊ¡£types_hash_max_sizeÔ½´ó£¬¾Í»áÏûºÄ¸ü¶àµÄÄÚ´æ£¬µ«É¢ÁÐkeyµÄ³åÍ»ÂÊ»á½µµÍ£¬¼ìË÷ËÙ¶È¾Í¸ü¿ì¡£types_hash_max_sizeÔ½Ð¡£¬ÏûºÄµÄÄÚ´æ¾ÍÔ½Ð¡£¬µ«É¢ÁÐkeyµÄ³åÍ»ÂÊ¿ÉÄÜÉÏÉý¡£
*/
    { ngx_string("types_hash_max_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, types_hash_max_size),
      NULL },

/*
types_hash_bucket_size
Óï·¨£ºtypes_hash_bucket_size size;
Ä¬ÈÏ£ºtypes_hash_bucket_size 32|64|128;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
ÎªÁË¿ìËÙÑ°ÕÒµ½ÏàÓ¦MIME type£¬NginxÊ¹ÓÃÉ¢ÁÐ±íÀ´´æ´¢MIME typeÓëÎÄ¼þÀ©Õ¹Ãû¡£types_hash_bucket_size ÉèÖÃÁËÃ¿¸öÉ¢ÁÐÍ°Õ¼ÓÃµÄÄÚ´æ´óÐ¡¡£
*/
    { ngx_string("types_hash_bucket_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, types_hash_bucket_size),
      NULL },

    /*
    ÏÂÃæÊÇMIMEÀàÐÍµÄÉèÖÃÅäÖÃÏî¡£
    MIME typeÓëÎÄ¼þÀ©Õ¹µÄÓ³Éä
    Óï·¨£ºtype {...};
    ÅäÖÃ¿é£ºhttp¡¢server¡¢location
    ¶¨ÒåMIME typeµ½ÎÄ¼þÀ©Õ¹ÃûµÄÓ³Éä¡£¶à¸öÀ©Õ¹Ãû¿ÉÒÔÓ³Éäµ½Í¬Ò»¸öMIME type¡£ÀýÈç£º
    types {
     text/html    html;
     text/html    conf;
     image/gif    gif;
     image/jpeg   jpg;
    }
    */ //typesºÍdefault_type¶ÔÓ¦
//types {}ÅäÖÃngx_http_core_typeÊ×ÏÈ´æÔÚÓë¸ÃÊý×éÖÐ£¬È»ºóÔÚngx_http_core_merge_loc_conf´æÈëtypes_hashÖÐ£¬ÕæÕýÉúÐ§¼ûngx_http_set_content_type
    { ngx_string("types"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF
                                          |NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_http_core_types,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
/*
Ä¬ÈÏMIME type
Óï·¨£ºdefault_type MIME-type;
Ä¬ÈÏ£ºdefault_type text/plain;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
µ±ÕÒ²»µ½ÏàÓ¦µÄMIME typeÓëÎÄ¼þÀ©Õ¹ÃûÖ®¼äµÄÓ³ÉäÊ±£¬Ê¹ÓÃÄ¬ÈÏµÄMIME type×÷ÎªHTTP headerÖÐµÄContent-Type¡£
*/ //typesºÍdefault_type¶ÔÓ¦
    { ngx_string("default_type"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, default_type),
      NULL },

    /* 
       nginxÖ¸¶¨ÎÄ¼þÂ·¾¶ÓÐÁ½ÖÖ·½Ê½rootºÍalias£¬ÕâÁ½ÕßµÄÓÃ·¨Çø±ð£¬Ê¹ÓÃ·½·¨×Ü½áÁËÏÂ£¬·½±ã´ó¼ÒÔÚÓ¦ÓÃ¹ý³ÌÖÐ£¬¿ìËÙÏìÓ¦¡£rootÓëaliasÖ÷ÒªÇø±ðÔÚÓÚnginxÈçºÎ½âÊÍlocationºóÃæµÄuri£¬Õâ»áÊ¹Á½Õß·Ö±ðÒÔ²»Í¬µÄ·½Ê½½«ÇëÇóÓ³Éäµ½·þÎñÆ÷ÎÄ¼þÉÏ¡£
       [root]
       Óï·¨£ºroot path
       Ä¬ÈÏÖµ£ºroot html
       ÅäÖÃ¶Î£ºhttp¡¢server¡¢location¡¢if
       [alias]
       Óï·¨£ºalias path
       ÅäÖÃ¶Î£ºlocation
       ÊµÀý£º
       
       location ~ ^/weblogs/ {
        root /data/weblogs/www.ttlsa.com;
        autoindex on;
        auth_basic            "Restricted";
        auth_basic_user_file  passwd/weblogs;
       }
       Èç¹ûÒ»¸öÇëÇóµÄURIÊÇ/weblogs/httplogs/www.ttlsa.com-access.logÊ±£¬web·þÎñÆ÷½«»á·µ»Ø·þÎñÆ÷ÉÏµÄ/data/weblogs/www.ttlsa.com/weblogs/httplogs/www.ttlsa.com-access.logµÄÎÄ¼þ¡£
       [info]root»á¸ù¾ÝÍêÕûµÄURIÇëÇóÀ´Ó³Éä£¬Ò²¾ÍÊÇ/path/uri¡£[/info]
       Òò´Ë£¬Ç°ÃæµÄÇëÇóÓ³ÉäÎªpath/weblogs/httplogs/www.ttlsa.com-access.log¡£
       
       
       location ^~ /binapp/ {  
        limit_conn limit 4;
        limit_rate 200k;
        internal;  
        alias /data/statics/bin/apps/;
       }
       alias»á°ÑlocationºóÃæÅäÖÃµÄÂ·¾¶¶ªÆúµô£¬°Ñµ±Ç°Æ¥Åäµ½µÄÄ¿Â¼Ö¸Ïòµ½Ö¸¶¨µÄÄ¿Â¼¡£Èç¹ûÒ»¸öÇëÇóµÄURIÊÇ/binapp/a.ttlsa.com/faviconÊ±£¬web·þÎñÆ÷½«»á·µ»Ø·þÎñÆ÷ÉÏµÄ/data/statics/bin/apps/a.ttlsa.com/favicon.jgpµÄÎÄ¼þ¡£
       [warning]1. Ê¹ÓÃaliasÊ±£¬Ä¿Â¼ÃûºóÃæÒ»¶¨Òª¼Ó"/"¡£
       2. alias¿ÉÒÔÖ¸¶¨ÈÎºÎÃû³Æ¡£
       3. aliasÔÚÊ¹ÓÃÕýÔòÆ¥ÅäÊ±£¬±ØÐë²¶×½ÒªÆ¥ÅäµÄÄÚÈÝ²¢ÔÚÖ¸¶¨µÄÄÚÈÝ´¦Ê¹ÓÃ¡£
       4. aliasÖ»ÄÜÎ»ÓÚlocation¿éÖÐ¡£[/warning]
       ÈçÐè×ªÔØÇë×¢Ã÷³ö´¦£º  http://www.ttlsa.com/html/2907.html


       ¶¨Òå×ÊÔ´ÎÄ¼þÂ·¾¶¡£Ä¬ÈÏroot html.ÅäÖÃ¿é:http  server location  if£¬ Èç:
        location /download/ {
            root /opt/web/html/;
        } 
        Èç¹ûÓÐÒ»¸öÇëÇóµÄurlÊÇ/download/index/aa.html,ÄÇÃ´WEB½«»á·µ»Ø·þÎñÆ÷ÉÏ/opt/web/html/download/index/aa.htmlÎÄ¼þµÄÄÚÈÝ
    */
    { ngx_string("root"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_core_root,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /*
    ÒÔalias·½Ê½ÉèÖÃ×ÊÔ´Â·¾¶
    Óï·¨£ºalias path;
    ÅäÖÃ¿é£ºlocation
    
    aliasÒ²ÊÇÓÃÀ´ÉèÖÃÎÄ¼þ×ÊÔ´Â·¾¶µÄ£¬ËüÓërootµÄ²»Í¬µãÖ÷ÒªÔÚÓÚÈçºÎ½â¶Á½ô¸úlocationºóÃæµÄuri²ÎÊý£¬Õâ½«»áÖÂÊ¹aliasÓërootÒÔ²»Í¬µÄ·½Ê½½«ÓÃ»§ÇëÇóÓ³Éäµ½ÕæÕýµÄ´ÅÅÌÎÄ¼þÉÏ¡£ÀýÈç£¬Èç¹ûÓÐÒ»¸öÇëÇóµÄURIÊÇ/conf/nginx.conf£¬¶øÓÃ»§Êµ¼ÊÏë·ÃÎÊµÄÎÄ¼þÔÚ/usr/local/nginx/conf/nginx.conf£¬ÄÇÃ´ÏëÒªÊ¹ÓÃaliasÀ´½øÐÐÉèÖÃµÄ»°£¬¿ÉÒÔ²ÉÓÃÈçÏÂ·½Ê½£º
    location /conf {
       alias /usr/local/nginx/conf/;   
    }
    
    Èç¹ûÓÃrootÉèÖÃ£¬ÄÇÃ´Óï¾äÈçÏÂËùÊ¾£º
    location /conf {
       root /usr/local/nginx/;       
    }
    
    Ê¹ÓÃaliasÊ±£¬ÔÚURIÏòÊµ¼ÊÎÄ¼þÂ·¾¶µÄÓ³Éä¹ý³ÌÖÐ£¬ÒÑ¾­°ÑlocationºóÅäÖÃµÄ/confÕâ²¿·Ö×Ö·û´®¶ªÆúµô£¬
    Òò´Ë£¬/conf/nginx.confÇëÇó½«¸ù¾Ýalias pathÓ³ÉäÎªpath/nginx.conf¡£rootÔò²»È»£¬Ëü»á¸ù¾ÝÍêÕûµÄURI
    ÇëÇóÀ´Ó³Éä£¬Òò´Ë£¬/conf/nginx.confÇëÇó»á¸ù¾Ýroot pathÓ³ÉäÎªpath/conf/nginx.conf¡£ÕâÒ²ÊÇroot
    ¿ÉÒÔ·ÅÖÃµ½http¡¢server¡¢location»òif¿éÖÐ£¬¶øaliasÖ»ÄÜ·ÅÖÃµ½location¿éÖÐµÄÔ­Òò¡£
    
    aliasºóÃæ»¹¿ÉÒÔÌí¼ÓÕýÔò±í´ïÊ½£¬ÀýÈç£º
    location ~ ^/test/(\w+)\.(\w+)$ {
        alias /usr/local/nginx/$2/$1.$2;
    }
    
    ÕâÑù£¬ÇëÇóÔÚ·ÃÎÊ/test/nginx.confÊ±£¬Nginx»á·µ»Ø/usr/local/nginx/conf/nginx.confÎÄ¼þÖÐµÄÄÚÈÝ¡£

    nginxÖ¸¶¨ÎÄ¼þÂ·¾¶ÓÐÁ½ÖÖ·½Ê½rootºÍalias£¬ÕâÁ½ÕßµÄÓÃ·¨Çø±ð£¬Ê¹ÓÃ·½·¨×Ü½áÁËÏÂ£¬·½±ã´ó¼ÒÔÚÓ¦ÓÃ¹ý³ÌÖÐ£¬¿ìËÙÏìÓ¦¡£rootÓëaliasÖ÷ÒªÇø±ðÔÚÓÚnginxÈçºÎ½âÊÍlocationºóÃæµÄuri£¬Õâ»áÊ¹Á½Õß·Ö±ðÒÔ²»Í¬µÄ·½Ê½½«ÇëÇóÓ³Éäµ½·þÎñÆ÷ÎÄ¼þÉÏ¡£
[root]
Óï·¨£ºroot path
Ä¬ÈÏÖµ£ºroot html
ÅäÖÃ¶Î£ºhttp¡¢server¡¢location¡¢if
[alias]
Óï·¨£ºalias path
ÅäÖÃ¶Î£ºlocation
ÊµÀý£º

location ~ ^/weblogs/ {
 root /data/weblogs/www.ttlsa.com;
 autoindex on;
 auth_basic            "Restricted";
 auth_basic_user_file  passwd/weblogs;
}
Èç¹ûÒ»¸öÇëÇóµÄURIÊÇ/weblogs/httplogs/www.ttlsa.com-access.logÊ±£¬web·þÎñÆ÷½«»á·µ»Ø·þÎñÆ÷ÉÏµÄ/data/weblogs/www.ttlsa.com/weblogs/httplogs/www.ttlsa.com-access.logµÄÎÄ¼þ¡£
[info]root»á¸ù¾ÝÍêÕûµÄURIÇëÇóÀ´Ó³Éä£¬Ò²¾ÍÊÇ/path/uri¡£[/info]
Òò´Ë£¬Ç°ÃæµÄÇëÇóÓ³ÉäÎªpath/weblogs/httplogs/www.ttlsa.com-access.log¡£


location ^~ /binapp/ {  
 limit_conn limit 4;
 limit_rate 200k;
 internal;  
 alias /data/statics/bin/apps/;
}
alias»á°ÑlocationºóÃæÅäÖÃµÄÂ·¾¶¶ªÆúµô£¬°Ñµ±Ç°Æ¥Åäµ½µÄÄ¿Â¼Ö¸Ïòµ½Ö¸¶¨µÄÄ¿Â¼¡£Èç¹ûÒ»¸öÇëÇóµÄURIÊÇ/binapp/a.ttlsa.com/faviconÊ±£¬web·þÎñÆ÷½«»á·µ»Ø·þÎñÆ÷ÉÏµÄ/data/statics/bin/apps/a.ttlsa.com/favicon.jgpµÄÎÄ¼þ¡£
[warning]1. Ê¹ÓÃaliasÊ±£¬Ä¿Â¼ÃûºóÃæÒ»¶¨Òª¼Ó"/"¡£
2. alias¿ÉÒÔÖ¸¶¨ÈÎºÎÃû³Æ¡£
3. aliasÔÚÊ¹ÓÃÕýÔòÆ¥ÅäÊ±£¬±ØÐë²¶×½ÒªÆ¥ÅäµÄÄÚÈÝ²¢ÔÚÖ¸¶¨µÄÄÚÈÝ´¦Ê¹ÓÃ¡£
4. aliasÖ»ÄÜÎ»ÓÚlocation¿éÖÐ¡£[/warning]
ÈçÐè×ªÔØÇë×¢Ã÷³ö´¦£º  http://www.ttlsa.com/html/2907.html
    */
    { ngx_string("alias"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_core_root,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /*
°´HTTP·½·¨ÃûÏÞÖÆÓÃ»§ÇëÇó
Óï·¨:  limit_except method ... { ... }
Ä¬ÈÏÖµ:  ¡ª  
ÉÏÏÂÎÄ:  location
 
ÔÊÐí°´ÇëÇóµÄHTTP·½·¨ÏÞÖÆ¶ÔÄ³Â·¾¶µÄÇëÇó¡£methodÓÃÓÚÖ¸¶¨²»ÓÉÕâÐ©ÏÞÖÆÌõ¼þ½øÐÐ¹ýÂËµÄHTTP·½·¨£¬¿ÉÑ¡ÖµÓÐ GET¡¢ HEAD¡¢ POST¡¢ PUT¡¢ 
DELETE¡¢ MKCOL¡¢ COPY¡¢ MOVE¡¢ OPTIONS¡¢ PROPFIND¡¢ PROPPATCH¡¢ LOCK¡¢ UNLOCK »òÕß PATCH¡£ Ö¸¶¨methodÎªGET·½·¨µÄÍ¬Ê±£¬
nginx»á×Ô¶¯Ìí¼ÓHEAD·½·¨¡£ ÄÇÃ´ÆäËûHTTP·½·¨µÄÇëÇó¾Í»áÓÉÖ¸ÁîÒýµ¼µÄÅäÖÃ¿éÖÐµÄngx_http_access_module Ä£¿éºÍngx_http_auth_basic_module
Ä£¿éµÄÖ¸ÁîÀ´ÏÞÖÆ·ÃÎÊ¡£Èç£º 

limit_except GET {
    allow 192.168.1.0/32;
    deny  all;
}

ÇëÁôÒâÉÏÃæµÄÀý×Ó½«¶Ô³ýGETºÍHEAD·½·¨ÒÔÍâµÄËùÓÐHTTP·½·¨µÄÇëÇó½øÐÐ·ÃÎÊÏÞÖÆ¡£ 
    */
    { ngx_string("limit_except"),
      NGX_HTTP_LOC_CONF|NGX_CONF_BLOCK|NGX_CONF_1MORE,
      ngx_http_core_limit_except,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
/*
HTTPÇëÇó°üÌåµÄ×î´óÖµ
Óï·¨£ºclient_max_body_size size;
Ä¬ÈÏ£ºclient_max_body_size 1m;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
ä¯ÀÀÆ÷ÔÚ·¢ËÍº¬ÓÐ½Ï´óHTTP°üÌåµÄÇëÇóÊ±£¬ÆäÍ·²¿»áÓÐÒ»¸öContent-Length×Ö¶Î£¬client_max_body_sizeÊÇÓÃÀ´ÏÞÖÆContent-LengthËùÊ¾ÖµµÄ´óÐ¡µÄ¡£Òò´Ë£¬
Õâ¸öÏÞÖÆ°üÌåµÄÅäÖÃ·Ç³£ÓÐÓÃ´¦£¬ÒòÎª²»ÓÃµÈNginx½ÓÊÕÍêËùÓÐµÄHTTP°üÌå¡ªÕâÓÐ¿ÉÄÜÏûºÄºÜ³¤Ê±¼ä¡ª¾Í¿ÉÒÔ¸æËßÓÃ»§ÇëÇó¹ý´ó²»±»½ÓÊÜ¡£ÀýÈç£¬ÓÃ»§ÊÔÍ¼
ÉÏ´«Ò»¸ö10GBµÄÎÄ¼þ£¬NginxÔÚÊÕÍê°üÍ·ºó£¬·¢ÏÖContent-Length³¬¹ýclient_max_body_size¶¨ÒåµÄÖµ£¬¾ÍÖ±½Ó·¢ËÍ413 ("Request Entity Too Large")ÏìÓ¦¸ø¿Í»§¶Ë¡£
*/
    { ngx_string("client_max_body_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_off_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_max_body_size),
      NULL },

/*
´æ´¢HTTPÍ·²¿µÄÄÚ´æbuffer´óÐ¡
Óï·¨£ºclient_header_buffer_size size;
Ä¬ÈÏ£ºclient_header_buffer_size 1k;
ÅäÖÃ¿é£ºhttp¡¢server
ÉÏÃæÅäÖÃÏî¶¨ÒåÁËÕý³£Çé¿öÏÂNginx½ÓÊÕÓÃ»§ÇëÇóÖÐHTTP header²¿·Ö£¨°üÀ¨HTTPÐÐºÍHTTPÍ·²¿£©Ê±·ÖÅäµÄÄÚ´æbuffer´óÐ¡¡£ÓÐÊ±£¬
ÇëÇóÖÐµÄHTTP header²¿·Ö¿ÉÄÜ»á³¬¹ýÕâ¸ö´óÐ¡£¬ÕâÊ±large_client_header_buffers¶¨ÒåµÄbuffer½«»áÉúÐ§¡£
*/
    { ngx_string("client_body_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_buffer_size),
      NULL },

    /*
    ¶ÁÈ¡HTTP°üÌåµÄ³¬Ê±Ê±¼ä
    Óï·¨£ºclient_body_timeout time£¨Ä¬ÈÏµ¥Î»£ºÃë£©£»
    Ä¬ÈÏ£ºclient_body_timeout 60;
    ÅäÖÃ¿é£ºhttp¡¢server¡¢location
    ´ËÅäÖÃÏîÓëclient_header_timeoutÏàËÆ£¬Ö»ÊÇÕâ¸ö³¬Ê±Ê±¼äÖ»ÔÚ¶ÁÈ¡HTTP°üÌåÊ±²ÅÓÐÐ§¡£
    */
    { ngx_string("client_body_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_timeout),
      NULL },

/*
HTTP°üÌåµÄÁÙÊ±´æ·ÅÄ¿Â¼
Óï·¨£ºclient_body_temp_path dir-path [ level1 [ level2 [ level3 ]]]
Ä¬ÈÏ£ºclient_body_temp_path client_body_temp;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
ÉÏÃæÅäÖÃÏî¶¨ÒåHTTP°üÌå´æ·ÅµÄÁÙÊ±Ä¿Â¼¡£ÔÚ½ÓÊÕHTTP°üÌåÊ±£¬Èç¹û°üÌåµÄ´óÐ¡´óÓÚclient_body_buffer_size£¬Ôò»áÒÔÒ»¸öµÝÔöµÄÕûÊýÃüÃû²¢´æ·Åµ½
client_body_temp_pathÖ¸¶¨µÄÄ¿Â¼ÖÐ¡£ºóÃæ¸ú×ÅµÄlevel1¡¢level2¡¢level3£¬ÊÇÎªÁË·ÀÖ¹Ò»¸öÄ¿Â¼ÏÂµÄÎÄ¼þÊýÁ¿Ì«¶à£¬´Ó¶øµ¼ÖÂÐÔÄÜÏÂ½µ£¬
Òò´ËÊ¹ÓÃÁËlevel²ÎÊý£¬ÕâÑù¿ÉÒÔ°´ÕÕÁÙÊ±ÎÄ¼þÃû×î¶àÔÙ¼ÓÈý²ãÄ¿Â¼¡£ÀýÈç£º
client_body_temp_path  /opt/nginx/client_temp 1 2;
Èç¹ûÐÂÉÏ´«µÄHTTP °üÌåÊ¹ÓÃ00000123456×÷ÎªÁÙÊ±ÎÄ¼þÃû£¬¾Í»á±»´æ·ÅÔÚÕâ¸öÄ¿Â¼ÖÐ¡£
/opt/nginx/client_temp/6/45/00000123456
*/
    { ngx_string("client_body_temp_path"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1234,
      ngx_conf_set_path_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_temp_path),
      NULL },

    /*
    HTTP°üÌåÖ»´æ´¢µ½´ÅÅÌÎÄ¼þÖÐ
    Óï·¨£ºclient_body_in_file_only on | clean | off;
    Ä¬ÈÏ£ºclient_body_in_file_only off;
    ÅäÖÃ¿é£ºhttp¡¢server¡¢location
    µ±ÖµÎª·ÇoffÊ±£¬ÓÃ»§ÇëÇóÖÐµÄHTTP°üÌåÒ»ÂÉ´æ´¢µ½´ÅÅÌÎÄ¼þÖÐ£¬¼´Ê¹Ö»ÓÐ0×Ö½ÚÒ²»á´æ´¢ÎªÎÄ¼þ¡£µ±ÇëÇó½áÊøÊ±£¬Èç¹ûÅäÖÃÎªon£¬ÔòÕâ¸öÎÄ¼þ²»»á
    ±»É¾³ý£¨¸ÃÅäÖÃÒ»°ãÓÃÓÚµ÷ÊÔ¡¢¶¨Î»ÎÊÌâ£©£¬µ«Èç¹ûÅäÖÃÎªclean£¬Ôò»áÉ¾³ý¸ÃÎÄ¼þ¡£
   */
    { ngx_string("client_body_in_file_only"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_in_file_only),
      &ngx_http_core_request_body_in_file },

/*
HTTP°üÌå¾¡Á¿Ð´Èëµ½Ò»¸öÄÚ´æbufferÖÐ
Óï·¨£ºclient_body_in_single_buffer on | off;
Ä¬ÈÏ£ºclient_body_in_single_buffer off;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
ÓÃ»§ÇëÇóÖÐµÄHTTP°üÌåÒ»ÂÉ´æ´¢µ½ÄÚ´æÎ¨Ò»Í¬Ò»¸öbufferÖÐ¡£µ±È»£¬Èç¹ûHTTP°üÌåµÄ´óÐ¡³¬¹ýÁËÏÂÃæclient_body_buffer_sizeÉèÖÃµÄÖµ£¬°üÌå»¹ÊÇ»áÐ´Èëµ½´ÅÅÌÎÄ¼þÖÐ¡£
*/
    { ngx_string("client_body_in_single_buffer"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_in_single_buffer),
      NULL },

/*
sendfileÏµÍ³µ÷ÓÃ
Óï·¨£ºsendfile on | off;
Ä¬ÈÏ£ºsendfile off;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
¿ÉÒÔÆôÓÃLinuxÉÏµÄsendfileÏµÍ³µ÷ÓÃÀ´·¢ËÍÎÄ¼þ£¬Ëü¼õÉÙÁËÄÚºËÌ¬ÓëÓÃ»§Ì¬Ö®¼äµÄÁ½´ÎÄÚ´æ¸´ÖÆ£¬ÕâÑù¾Í»á´Ó´ÅÅÌÖÐ¶ÁÈ¡ÎÄ¼þºóÖ±½ÓÔÚÄÚºËÌ¬·¢ËÍµ½Íø¿¨Éè±¸£¬
Ìá¸ßÁË·¢ËÍÎÄ¼þµÄÐ§ÂÊ¡£
*/ 
    /*
    When both AIO and sendfile are enabled on Linux, AIO is used for files that are larger than or equal to the size specified in the 
    directio directive, while sendfile is used for files of smaller sizes or when directio is disabled. 
    Èç¹ûaio on; sendfile¶¼ÅäÖÃÁË£¬²¢ÇÒÖ´ÐÐÁËb->file->directio = of.is_directio(²¢ÇÒof.is_directioÒªÎª1)Õâ¼¸¸öÄ£¿é£¬
    Ôòµ±ÎÄ¼þ´óÐ¡´óÓÚµÈÓÚdirectioÖ¸¶¨size(Ä¬ÈÏ512)µÄÊ±ºòÊ¹ÓÃaio,µ±Ð¡ÓÚsize»òÕßdirectio offµÄÊ±ºòÊ¹ÓÃsendfile
    ÉúÐ§¼ûngx_open_and_stat_file  if (of->directio <= ngx_file_size(&fi)) { ngx_directio_on } ÒÔ¼°ngx_output_chain_copy_buf

    ²»¹ý²»Âú×ãÉÏÃæµÄÌõ¼þ£¬Èç¹ûaio on; sendfile¶¼ÅäÖÃÁË£¬Ôò»¹ÊÇÒÔsendfileÎª×¼
    */ //ngx_output_chain_as_is  ngx_output_chain_copy_bufÊÇaioºÍsendfileºÍÆÕÍ¨ÎÄ¼þ¶ÁÐ´µÄ·ÖÖ§µã  ngx_linux_sendfile_chainÊÇsendfile·¢ËÍºÍÆÕÍ¨write·¢ËÍµÄ·Ö½çµã
    { ngx_string("sendfile"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_FLAG,
//Ò»°ã´ó»º´æÎÄ¼þÓÃaio·¢ËÍ£¬Ð¡ÎÄ¼þÓÃsendfile£¬ÒòÎªaioÊÇÒì²½µÄ£¬²»Ó°ÏìÆäËûÁ÷³Ì£¬µ«ÊÇsendfileÊÇÍ¬²½µÄ£¬Ì«´óµÄ»°¿ÉÄÜÐèÒª¶à´Îsendfile²ÅÄÜ·¢ËÍÍê£¬ÓÐÖÖ×èÈû¸Ð¾õ
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, sendfile),
      NULL },

    { ngx_string("sendfile_max_chunk"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, sendfile_max_chunk),
      NULL },

/*
AIOÏµÍ³µ÷ÓÃ
Óï·¨£ºaio on | off;
Ä¬ÈÏ£ºaio off;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
´ËÅäÖÃÏî±íÊ¾ÊÇ·ñÔÚFreeBSD»òLinuxÏµÍ³ÉÏÆôÓÃÄÚºË¼¶±ðµÄÒì²½ÎÄ¼þI/O¹¦ÄÜ¡£×¢Òâ£¬ËüÓësendfile¹¦ÄÜÊÇ»¥³âµÄ¡£

Syntax:  aio on | off | threads[=pool];
 
Default:  aio off; 
Context:  http, server, location
 
Enables or disables the use of asynchronous file I/O (AIO) on FreeBSD and Linux: 

location /video/ {
    aio            on;
    output_buffers 1 64k;
}

On FreeBSD, AIO can be used starting from FreeBSD 4.3. AIO can either be linked statically into a kernel: 

options VFS_AIO
or loaded dynamically as a kernel loadable module: 

kldload aio

On Linux, AIO can be used starting from kernel version 2.6.22. Also, it is necessary to enable directio, or otherwise reading will be blocking: 

location /video/ {
    aio            on;
    directio       512;
    output_buffers 1 128k;
}

On Linux, directio can only be used for reading blocks that are aligned on 512-byte boundaries (or 4K for XFS). File¡¯s unaligned end is 
read in blocking mode. The same holds true for byte range requests and for FLV requests not from the beginning of a file: reading of 
unaligned data at the beginning and end of a file will be blocking. 

When both AIO and sendfile are enabled on Linux, AIO is used for files that are larger than or equal to the size specified in the directio 
directive, while sendfile is used for files of smaller sizes or when directio is disabled. 

location /video/ {
    sendfile       on;
    aio            on;
    directio       8m;
}

Finally, files can be read and sent using multi-threading (1.7.11), without blocking a worker process: 

location /video/ {
    sendfile       on;
    aio            threads;
}
Read and send file operations are offloaded to threads of the specified pool. If the pool name is omitted, the pool with the name ¡°default¡± 
is used. The pool name can also be set with variables: 

aio threads=pool$disk;
By default, multi-threading is disabled, it should be enabled with the --with-threads configuration parameter. Currently, multi-threading is 
compatible only with the epoll, kqueue, and eventport methods. Multi-threaded sending of files is only supported on Linux. 
*/
/*
When both AIO and sendfile are enabled on Linux, AIO is used for files that are larger than or equal to the size specified in the 
directio directive, while sendfile is used for files of smaller sizes or when directio is disabled. 
Èç¹ûaio on; sendfile¶¼ÅäÖÃÁË£¬²¢ÇÒÖ´ÐÐÁËb->file->directio = of.is_directio(²¢ÇÒof.is_directioÒªÎª1)Õâ¼¸¸öÄ£¿é£¬
Ôòµ±ÎÄ¼þ´óÐ¡´óÓÚµÈÓÚdirectioÖ¸¶¨size(Ä¬ÈÏ512)µÄÊ±ºòÊ¹ÓÃaio,µ±Ð¡ÓÚsize»òÕßdirectio offµÄÊ±ºòÊ¹ÓÃsendfile
ÉúÐ§¼ûngx_open_and_stat_file  if (of->directio <= ngx_file_size(&fi)) { ngx_directio_on } ÒÔ¼°ngx_output_chain_copy_buf

²»¹ý²»Âú×ãÉÏÃæµÄÌõ¼þ£¬Èç¹ûaio on; sendfile¶¼ÅäÖÃÁË£¬Ôò»¹ÊÇÒÔsendfileÎª×¼
*/ //ngx_output_chain_as_is  ngx_output_chain_align_file_buf  ngx_output_chain_copy_bufÊÇaioºÍsendfileºÍÆÕÍ¨ÎÄ¼þ¶ÁÐ´µÄ·ÖÖ§µã  ngx_linux_sendfile_chainÊÇsendfile·¢ËÍºÍÆÕÍ¨write·¢ËÍµÄ·Ö½çµã
    { ngx_string("aio"),  
//Ò»°ã´ó»º´æÎÄ¼þÓÃaio·¢ËÍ£¬Ð¡ÎÄ¼þÓÃsendfile£¬ÒòÎªaioÊÇÒì²½µÄ£¬²»Ó°ÏìÆäËûÁ÷³Ì£¬µ«ÊÇsendfileÊÇÍ¬²½µÄ£¬Ì«´óµÄ»°¿ÉÄÜÐèÒª¶à´Îsendfile²ÅÄÜ·¢ËÍÍê£¬ÓÐÖÖ×èÈû¸Ð¾õ
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_core_set_aio,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("read_ahead"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, read_ahead),
      NULL },

/*
Óï·¨£ºdirectio size | off;
Ä¬ÈÏ£ºdirectio off;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
µ±ÎÄ¼þ´óÐ¡´óÓÚ¸ÃÖµµÄÊ±ºò£¬¿ÉÒÔ´ËÅäÖÃÏîÔÚFreeBSDºÍLinuxÏµÍ³ÉÏÊ¹ÓÃO_DIRECTÑ¡ÏîÈ¥¶ÁÈ¡ÎÄ¼þ£¬Í¨³£¶Ô´óÎÄ¼þµÄ¶ÁÈ¡ËÙ¶ÈÓÐÓÅ»¯×÷ÓÃ¡£×¢Òâ£¬ËüÓësendfile¹¦ÄÜÊÇ»¥³âµÄ¡£
*/
/*
When both AIO and sendfile are enabled on Linux, AIO is used for files that are larger than or equal to the size specified in the 
directio directive, while sendfile is used for files of smaller sizes or when directio is disabled. 
Èç¹ûaio on; sendfile¶¼ÅäÖÃÁË£¬²¢ÇÒÖ´ÐÐÁËb->file->directio = of.is_directio(²¢ÇÒof.is_directioÒªÎª1)Õâ¼¸¸öÄ£¿é£¬
Ôòµ±ÎÄ¼þ´óÐ¡´óÓÚµÈÓÚdirectioÖ¸¶¨size(Ä¬ÈÏ512)µÄÊ±ºòÊ¹ÓÃaio,µ±Ð¡ÓÚsize»òÕßdirectio offµÄÊ±ºòÊ¹ÓÃsendfile
ÉúÐ§¼ûngx_open_and_stat_file  if (of->directio <= ngx_file_size(&fi)) { ngx_directio_on } ÒÔ¼°ngx_output_chain_copy_buf

²»¹ý²»Âú×ãÉÏÃæµÄÌõ¼þ£¬Èç¹ûaio on; sendfile¶¼ÅäÖÃÁË£¬Ôò»¹ÊÇÒÔsendfileÎª×¼


µ±¶ÁÈë³¤¶È´óÓÚµÈÓÚÖ¸¶¨sizeµÄÎÄ¼þÊ±£¬¿ªÆôDirectIO¹¦ÄÜ¡£¾ßÌåµÄ×ö·¨ÊÇ£¬ÔÚFreeBSD»òLinuxÏµÍ³¿ªÆôÊ¹ÓÃO_DIRECT±êÖ¾£¬ÔÚMacOS XÏµÍ³¿ªÆô
Ê¹ÓÃF_NOCACHE±êÖ¾£¬ÔÚSolarisÏµÍ³¿ªÆôÊ¹ÓÃdirectio()¹¦ÄÜ¡£ÕâÌõÖ¸Áî×Ô¶¯¹Ø±Õsendfile(0.7.15°æ)¡£ËüÔÚ´¦Àí´óÎÄ¼þÊ± 
*/ //ngx_output_chain_as_is  ngx_output_chain_align_file_buf  ngx_output_chain_copy_bufÊÇaioºÍsendfileºÍÆÕÍ¨ÎÄ¼þ¶ÁÐ´µÄ·ÖÖ§µã  ngx_linux_sendfile_chainÊÇsendfile·¢ËÍºÍÆÕÍ¨write·¢ËÍµÄ·Ö½çµã
  //ÉúÐ§¼ûngx_open_and_stat_file  if (of->directio <= ngx_file_size(&fi)) { ngx_directio_on }

    /* Êý¾ÝÔÚÎÄ¼þÀïÃæ£¬²¢ÇÒ³ÌÐòÓÐ×ßµ½ÁË b->file->directio = of.is_directio(²¢ÇÒof.is_directioÒªÎª1)Õâ¼¸¸öÄ£¿é£¬
        ²¢ÇÒÎÄ¼þ´óÐ¡´óÓÚdirectio xxxÖÐµÄ´óÐ¡²Å²Å»áÉúÐ§£¬¼ûngx_output_chain_align_file_buf  ngx_output_chain_as_is */
    { ngx_string("directio"), //ÔÚ»ñÈ¡»º´æÎÄ¼þÄÚÈÝµÄÊ±ºò£¬Ö»ÓÐÎÄ¼þ´óÐ¡´óÓëµÈÓÚdirectioµÄÊ±ºò²Å»áÉúÐ§ngx_directio_on
//Ò»°ã´ó»º´æÎÄ¼þÓÃaio·¢ËÍ£¬Ð¡ÎÄ¼þÓÃsendfile£¬ÒòÎªaioÊÇÒì²½µÄ£¬²»Ó°ÏìÆäËûÁ÷³Ì£¬Ì«´óµÄ»°¿ÉÄÜÐèÒª¶à´Îsendfile²ÅÄÜ·¢ËÍÍê£¬ÓÐÖÖ×èÈû¸Ð¾õ
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_core_directio,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
directio_alignment
Óï·¨£ºdirectio_alignment size;
Ä¬ÈÏ£ºdirectio_alignment 512;  ËüÓëdirectioÅäºÏÊ¹ÓÃ£¬Ö¸¶¨ÒÔdirectio·½Ê½¶ÁÈ¡ÎÄ¼þÊ±µÄ¶ÔÆë·½Ê½
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
ËüÓëdirectioÅäºÏÊ¹ÓÃ£¬Ö¸¶¨ÒÔdirectio·½Ê½¶ÁÈ¡ÎÄ¼þÊ±µÄ¶ÔÆë·½Ê½¡£Ò»°ãÇé¿öÏÂ£¬512BÒÑ¾­×ã¹»ÁË£¬µ«Õë¶ÔÒ»Ð©¸ßÐÔÄÜÎÄ¼þÏµÍ³£¬ÈçLinuxÏÂµÄXFSÎÄ¼þÏµÍ³£¬
¿ÉÄÜÐèÒªÉèÖÃµ½4KB×÷Îª¶ÔÆë·½Ê½¡£
*/ // Ä¬ÈÏ512   ÔÚngx_output_chain_get_bufÉúÐ§£¬±íÊ¾·ÖÅäÄÚ´æ¿Õ¼äµÄÊ±ºò£¬¿Õ¼äÆðÊ¼µØÖ·ÐèÒª°´ÕÕÕâ¸öÖµ¶ÔÆë
    { ngx_string("directio_alignment"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_off_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, directio_alignment),
      NULL },
/*
tcp_nopush
¹Ù·½:

tcp_nopush
Syntax: tcp_nopush on | off
Default: off
Context: http
server
location
Reference: tcp_nopush
This directive permits or forbids the use of thesocket options TCP_NOPUSH on FreeBSD or TCP_CORK on Linux. This option is 
onlyavailable when using sendfile.
Setting this option causes nginx to attempt to sendit¡¯s HTTP response headers in one packet on Linux and FreeBSD 4.x
You can read more about the TCP_NOPUSH and TCP_CORKsocket options here.

 
linux ÏÂÊÇtcp_cork,ÉÏÃæµÄÒâË¼¾ÍÊÇËµ£¬µ±Ê¹ÓÃsendfileº¯ÊýÊ±£¬tcp_nopush²ÅÆð×÷ÓÃ£¬ËüºÍÖ¸Áîtcp_nodelayÊÇ»¥³âµÄ¡£tcp_corkÊÇlinuxÏÂ
tcp/ip´«ÊäµÄÒ»¸ö±ê×¼ÁË£¬Õâ¸ö±ê×¼µÄ´ó¸ÅµÄÒâË¼ÊÇ£¬Ò»°ãÇé¿öÏÂ£¬ÔÚtcp½»»¥µÄ¹ý³ÌÖÐ£¬µ±Ó¦ÓÃ³ÌÐò½ÓÊÕµ½Êý¾Ý°üºóÂíÉÏ´«ËÍ³öÈ¥£¬²»µÈ´ý£¬
¶øtcp_corkÑ¡ÏîÊÇÊý¾Ý°ü²»»áÂíÉÏ´«ËÍ³öÈ¥£¬µÈµ½Êý¾Ý°ü×î´óÊ±£¬Ò»´ÎÐÔµÄ´«Êä³öÈ¥£¬ÕâÑùÓÐÖúÓÚ½â¾öÍøÂç¶ÂÈû£¬ÒÑ¾­ÊÇÄ¬ÈÏÁË¡£

Ò²¾ÍÊÇËµtcp_nopush = on »áÉèÖÃµ÷ÓÃtcp_cork·½·¨£¬Õâ¸öÒ²ÊÇÄ¬ÈÏµÄ£¬½á¹û¾ÍÊÇÊý¾Ý°ü²»»áÂíÉÏ´«ËÍ³öÈ¥£¬µÈµ½Êý¾Ý°ü×î´óÊ±£¬Ò»´ÎÐÔµÄ´«Êä³öÈ¥£¬
ÕâÑùÓÐÖúÓÚ½â¾öÍøÂç¶ÂÈû¡£

ÒÔ¿ìµÝÍ¶µÝ¾ÙÀýËµÃ÷Ò»ÏÂ£¨ÒÔÏÂÊÇÎÒµÄÀí½â£¬Ò²ÐíÊÇ²»ÕýÈ·µÄ£©£¬µ±¿ìµÝ¶«Î÷Ê±£¬¿ìµÝÔ±ÊÕµ½Ò»¸ö°ü¹ü£¬ÂíÉÏÍ¶µÝ£¬ÕâÑù±£Ö¤ÁË¼´Ê±ÐÔ£¬µ«ÊÇ»á
ºÄ·Ñ´óÁ¿µÄÈËÁ¦ÎïÁ¦£¬ÔÚÍøÂçÉÏ±íÏÖ¾ÍÊÇ»áÒýÆðÍøÂç¶ÂÈû£¬¶øµ±¿ìµÝÊÕµ½Ò»¸ö°ü¹ü£¬°Ñ°ü¹ü·Åµ½¼¯É¢µØ£¬µÈÒ»¶¨ÊýÁ¿ºóÍ³Ò»Í¶µÝ£¬ÕâÑù¾ÍÊÇtcp_corkµÄ
Ñ¡Ïî¸ÉµÄÊÂÇé£¬ÕâÑùµÄ»°£¬»á×î´ó»¯µÄÀûÓÃÍøÂç×ÊÔ´£¬ËäÈ»ÓÐÒ»µãµãÑÓ³Ù¡£

¶ÔÓÚnginxÅäÖÃÎÄ¼þÖÐµÄtcp_nopush£¬Ä¬ÈÏ¾ÍÊÇtcp_nopush,²»ÐèÒªÌØ±ðÖ¸¶¨£¬Õâ¸öÑ¡Ïî¶ÔÓÚwww£¬ftpµÈ´óÎÄ¼þºÜÓÐ°ïÖú

tcp_nodelay
        TCP_NODELAYºÍTCP_CORK»ù±¾ÉÏ¿ØÖÆÁË°üµÄ¡°Nagle»¯¡±£¬Nagle»¯ÔÚÕâÀïµÄº¬ÒåÊÇ²ÉÓÃNagleËã·¨°Ñ½ÏÐ¡µÄ°ü×é×°Îª¸ü´óµÄÖ¡¡£ John NagleÊÇNagleËã·¨µÄ·¢Ã÷ÈË£¬ºóÕß¾ÍÊÇÓÃËûµÄÃû×ÖÀ´ÃüÃûµÄ£¬ËûÔÚ1984ÄêÊ×´ÎÓÃÕâÖÖ·½·¨À´³¢ÊÔ½â¾ö¸£ÌØÆû³µ¹«Ë¾µÄÍøÂçÓµÈûÎÊÌâ£¨ÓûÁË½âÏêÇéÇë²Î¿´IETF RFC 896£©¡£Ëû½â¾öµÄÎÊÌâ¾ÍÊÇËùÎ½µÄsilly window syndrome£¬ÖÐÎÄ³Æ¡°ÓÞ´À´°¿ÚÖ¢ºòÈº¡±£¬¾ßÌåº¬ÒåÊÇ£¬ÒòÎªÆÕ±éÖÕ¶ËÓ¦ÓÃ³ÌÐòÃ¿²úÉúÒ»´Î»÷¼ü²Ù×÷¾Í»á·¢ËÍÒ»¸ö°ü£¬¶øµäÐÍÇé¿öÏÂÒ»¸ö°ü»áÓµÓÐÒ»¸ö×Ö½ÚµÄÊý¾ÝÔØºÉÒÔ¼°40¸ö×Ö½Ú³¤µÄ°üÍ·£¬ÓÚÊÇ²úÉú4000%µÄ¹ýÔØ£¬ºÜÇáÒ×µØ¾ÍÄÜÁîÍøÂç·¢ÉúÓµÈû,¡£ Nagle»¯ºóÀ´³ÉÁËÒ»ÖÖ±ê×¼²¢ÇÒÁ¢¼´ÔÚÒòÌØÍøÉÏµÃÒÔÊµÏÖ¡£ËüÏÖÔÚÒÑ¾­³ÉÎªÈ±Ê¡ÅäÖÃÁË£¬µ«ÔÚÎÒÃÇ¿´À´£¬ÓÐÐ©³¡ºÏÏÂ°ÑÕâÒ»Ñ¡Ïî¹ØµôÒ²ÊÇºÏºõÐèÒªµÄ¡£

       ÏÖÔÚÈÃÎÒÃÇ¼ÙÉèÄ³¸öÓ¦ÓÃ³ÌÐò·¢³öÁËÒ»¸öÇëÇó£¬Ï£Íû·¢ËÍÐ¡¿éÊý¾Ý¡£ÎÒÃÇ¿ÉÒÔÑ¡ÔñÁ¢¼´·¢ËÍÊý¾Ý»òÕßµÈ´ý²úÉú¸ü¶àµÄÊý¾ÝÈ»ºóÔÙÒ»´Î·¢ËÍÁ½ÖÖ²ßÂÔ¡£Èç¹ûÎÒÃÇÂíÉÏ·¢ËÍÊý¾Ý£¬ÄÇÃ´½»»¥ÐÔµÄÒÔ¼°¿Í»§/·þÎñÆ÷ÐÍµÄÓ¦ÓÃ³ÌÐò½«¼«´óµØÊÜÒæ¡£Èç¹ûÇëÇóÁ¢¼´·¢³öÄÇÃ´ÏìÓ¦Ê±¼äÒ²»á¿ìÒ»Ð©¡£ÒÔÉÏ²Ù×÷¿ÉÒÔÍ¨¹ýÉèÖÃÌ×½Ó×ÖµÄTCP_NODELAY = on Ñ¡ÏîÀ´Íê³É£¬ÕâÑù¾Í½ûÓÃÁËNagle Ëã·¨¡£

       ÁíÍâÒ»ÖÖÇé¿öÔòÐèÒªÎÒÃÇµÈµ½Êý¾ÝÁ¿´ïµ½×î´óÊ±²ÅÍ¨¹ýÍøÂçÒ»´Î·¢ËÍÈ«²¿Êý¾Ý£¬ÕâÖÖÊý¾Ý´«Êä·½Ê½ÓÐÒæÓÚ´óÁ¿Êý¾ÝµÄÍ¨ÐÅÐÔÄÜ£¬µäÐÍµÄÓ¦ÓÃ¾ÍÊÇÎÄ¼þ·þÎñÆ÷¡£Ó¦ÓÃ NagleËã·¨ÔÚÕâÖÖÇé¿öÏÂ¾Í»á²úÉúÎÊÌâ¡£µ«ÊÇ£¬Èç¹ûÄãÕýÔÚ·¢ËÍ´óÁ¿Êý¾Ý£¬Äã¿ÉÒÔÉèÖÃTCP_CORKÑ¡Ïî½ûÓÃNagle»¯£¬Æä·½Ê½ÕýºÃÍ¬ TCP_NODELAYÏà·´£¨TCP_CORKºÍ TCP_NODELAYÊÇ»¥ÏàÅÅ³âµÄ£©¡£



tcp_nopush
Óï·¨£ºtcp_nopush on | off;
Ä¬ÈÏ£ºtcp_nopush off;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
ÔÚ´ò¿ªsendfileÑ¡ÏîÊ±£¬È·¶¨ÊÇ·ñ¿ªÆôFreeBSDÏµÍ³ÉÏµÄTCP_NOPUSH»òLinuxÏµÍ³ÉÏµÄTCP_CORK¹¦ÄÜ¡£´ò¿ªtcp_nopushºó£¬½«»áÔÚ·¢ËÍÏìÓ¦Ê±°ÑÕû¸öÏìÓ¦°üÍ··Åµ½Ò»¸öTCP°üÖÐ·¢ËÍ¡£
*/ // tcp_nopush on | off;Ö»ÓÐ¿ªÆôsendfile£¬nopush²ÅÉúÐ§£¬Í¨¹ýÉèÖÃTCP_CORKÊµÏÖ
    { ngx_string("tcp_nopush"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, tcp_nopush),
      NULL },

      /*
      ÔÚÔªÊý¾Ý²Ù×÷µÈÐ¡°ü´«ËÍÊ±£¬·¢ÏÖÐÔÄÜ²»ºÃ£¬Í¨¹ýµ÷ÊÔ·¢ÏÖ¸úsocketµÄTCP_NODELAYÓÐºÜ´ó¹ØÏµ¡£
      TCP_NODELAY ºÍ TCP_CORK£¬ 
      ÕâÁ½¸öÑ¡Ïî¶¼¶ÔÍøÂçÁ¬½ÓµÄÐÐÎª¾ßÓÐÖØÒªµÄ×÷ÓÃ¡£Ðí¶àUNIXÏµÍ³¶¼ÊµÏÖÁËTCP_NODELAYÑ¡Ïî£¬µ«ÊÇ£¬TCP_CORKÔòÊÇLinuxÏµÍ³Ëù¶ÀÓÐµÄ¶øÇÒÏà¶Ô½ÏÐÂ£»ËüÊ×ÏÈÔÚÄÚºË°æ±¾2.4ÉÏµÃÒÔÊµÏÖ¡£
      ´ËÍâ£¬ÆäËûUNIXÏµÍ³°æ±¾Ò²ÓÐ¹¦ÄÜÀàËÆµÄÑ¡Ïî£¬ÖµµÃ×¢ÒâµÄÊÇ£¬ÔÚÄ³ÖÖÓÉBSDÅÉÉúµÄÏµÍ³ÉÏµÄ TCP_NOPUSHÑ¡ÏîÆäÊµ¾ÍÊÇTCP_CORKµÄÒ»²¿·Ö¾ßÌåÊµÏÖ¡£ 
      TCP_NODELAYºÍTCP_CORK»ù±¾ÉÏ¿ØÖÆÁË°üµÄ¡°Nagle»¯¡±£¬Nagle»¯ÔÚÕâÀïµÄº¬ÒåÊÇ²ÉÓÃNagleËã·¨°Ñ½ÏÐ¡µÄ°ü×é×°Îª¸ü´óµÄÖ¡¡£ John NagleÊÇNagleËã·¨µÄ·¢Ã÷ÈË£¬
      ºóÕß¾ÍÊÇÓÃËûµÄÃû×ÖÀ´ÃüÃûµÄ£¬ËûÔÚ1984ÄêÊ×´ÎÓÃÕâÖÖ·½·¨À´³¢ÊÔ½â¾ö¸£ÌØÆû³µ¹«Ë¾µÄÍøÂçÓµÈûÎÊÌâ£¨ÓûÁË½âÏêÇéÇë²Î¿´IETF RFC 896£©¡£Ëû½â¾öµÄÎÊÌâ¾ÍÊÇËùÎ½µÄsilly 
      window syndrome £¬ÖÐÎÄ³Æ¡°ÓÞ´À´°¿ÚÖ¢ºòÈº¡±£¬¾ßÌåº¬ÒåÊÇ£¬ÒòÎªÆÕ±éÖÕ¶ËÓ¦ÓÃ³ÌÐòÃ¿²úÉúÒ»´Î»÷¼ü²Ù×÷¾Í»á·¢ËÍÒ»¸ö°ü£¬¶øµäÐÍÇé¿öÏÂÒ»¸ö°ü»áÓµÓÐ
      Ò»¸ö×Ö½ÚµÄÊý¾ÝÔØºÉÒÔ¼°40 ¸ö×Ö½Ú³¤µÄ°üÍ·£¬ÓÚÊÇ²úÉú4000%µÄ¹ýÔØ£¬ºÜÇáÒ×µØ¾ÍÄÜÁîÍøÂç·¢ÉúÓµÈû,¡£ Nagle»¯ºóÀ´³ÉÁËÒ»ÖÖ±ê×¼²¢ÇÒÁ¢¼´ÔÚÒòÌØÍøÉÏµÃÒÔÊµÏÖ¡£
      ËüÏÖÔÚÒÑ¾­³ÉÎªÈ±Ê¡ÅäÖÃÁË£¬µ«ÔÚÎÒÃÇ¿´À´£¬ÓÐÐ©³¡ºÏÏÂ°ÑÕâÒ»Ñ¡Ïî¹ØµôÒ²ÊÇºÏºõÐèÒªµÄ¡£ 
      ÏÖÔÚÈÃÎÒÃÇ¼ÙÉèÄ³¸öÓ¦ÓÃ³ÌÐò·¢³öÁËÒ»¸öÇëÇó£¬Ï£Íû·¢ËÍÐ¡¿éÊý¾Ý¡£ÎÒÃÇ¿ÉÒÔÑ¡ÔñÁ¢¼´·¢ËÍÊý¾Ý»òÕßµÈ´ý²úÉú¸ü¶àµÄÊý¾ÝÈ»ºóÔÙÒ»´Î·¢ËÍÁ½ÖÖ²ßÂÔ¡£Èç¹ûÎÒÃÇÂíÉÏ·¢ËÍÊý¾Ý£¬
      ÄÇÃ´½»»¥ÐÔµÄÒÔ¼°¿Í»§/·þÎñÆ÷ÐÍµÄÓ¦ÓÃ³ÌÐò½«¼«´óµØÊÜÒæ¡£ÀýÈç£¬µ±ÎÒÃÇÕýÔÚ·¢ËÍÒ»¸ö½Ï¶ÌµÄÇëÇó²¢ÇÒµÈºò½Ï´óµÄÏìÓ¦Ê±£¬Ïà¹Ø¹ýÔØÓë´«ÊäµÄÊý¾Ý×ÜÁ¿Ïà±È¾Í»á±È½ÏµÍ£¬
      ¶øÇÒ£¬Èç¹ûÇëÇóÁ¢¼´·¢³öÄÇÃ´ÏìÓ¦Ê±¼äÒ²»á¿ìÒ»Ð©¡£ÒÔÉÏ²Ù×÷¿ÉÒÔÍ¨¹ýÉèÖÃÌ×½Ó×ÖµÄTCP_NODELAYÑ¡ÏîÀ´Íê³É£¬ÕâÑù¾Í½ûÓÃÁËNagle Ëã·¨¡£ 
      ÁíÍâÒ»ÖÖÇé¿öÔòÐèÒªÎÒÃÇµÈµ½Êý¾ÝÁ¿´ïµ½×î´óÊ±²ÅÍ¨¹ýÍøÂçÒ»´Î·¢ËÍÈ«²¿Êý¾Ý£¬ÕâÖÖÊý¾Ý´«Êä·½Ê½ÓÐÒæÓÚ´óÁ¿Êý¾ÝµÄÍ¨ÐÅÐÔÄÜ£¬µäÐÍµÄÓ¦ÓÃ¾ÍÊÇÎÄ¼þ·þÎñÆ÷¡£
      Ó¦ÓÃ NagleËã·¨ÔÚÕâÖÖÇé¿öÏÂ¾Í»á²úÉúÎÊÌâ¡£µ«ÊÇ£¬Èç¹ûÄãÕýÔÚ·¢ËÍ´óÁ¿Êý¾Ý£¬Äã¿ÉÒÔÉèÖÃTCP_CORKÑ¡Ïî½ûÓÃNagle»¯£¬Æä·½Ê½ÕýºÃÍ¬ TCP_NODELAYÏà·´
      £¨TCP_CORK ºÍ TCP_NODELAY ÊÇ»¥ÏàÅÅ³âµÄ£©¡£ÏÂÃæ¾ÍÈÃÎÒÃÇ×ÐÏ¸·ÖÎöÏÂÆä¹¤×÷Ô­Àí¡£ 
      ¼ÙÉèÓ¦ÓÃ³ÌÐòÊ¹ÓÃsendfile()º¯ÊýÀ´×ªÒÆ´óÁ¿Êý¾Ý¡£Ó¦ÓÃÐ­ÒéÍ¨³£ÒªÇó·¢ËÍÄ³Ð©ÐÅÏ¢À´Ô¤ÏÈ½âÊÍÊý¾Ý£¬ÕâÐ©ÐÅÏ¢ÆäÊµ¾ÍÊÇ±¨Í·ÄÚÈÝ¡£µäÐÍÇé¿öÏÂ±¨Í·ºÜÐ¡£¬
      ¶øÇÒÌ×½Ó×ÖÉÏÉèÖÃÁËTCP_NODELAY¡£ÓÐ±¨Í·µÄ°ü½«±»Á¢¼´´«Êä£¬ÔÚÄ³Ð©Çé¿öÏÂ£¨È¡¾öÓÚÄÚ²¿µÄ°ü¼ÆÊýÆ÷£©£¬ÒòÎªÕâ¸ö°ü³É¹¦µØ±»¶Ô·½ÊÕµ½ºóÐèÒªÇëÇó¶Ô·½È·ÈÏ¡£
      ÕâÑù£¬´óÁ¿Êý¾ÝµÄ´«Êä¾Í»á±»ÍÆ³Ù¶øÇÒ²úÉúÁË²»±ØÒªµÄÍøÂçÁ÷Á¿½»»»¡£ 
      µ«ÊÇ£¬Èç¹ûÎÒÃÇÔÚÌ×½Ó×ÖÉÏÉèÖÃÁËTCP_CORK£¨¿ÉÒÔ±ÈÓ÷ÎªÔÚ¹ÜµÀÉÏ²åÈë¡°Èû×Ó¡±£©Ñ¡Ïî£¬¾ßÓÐ±¨Í·µÄ°ü¾Í»áÌî²¹´óÁ¿µÄÊý¾Ý£¬ËùÓÐµÄÊý¾Ý¶¼¸ù¾Ý´óÐ¡×Ô¶¯µØÍ¨¹ý°ü´«Êä³öÈ¥¡£
      µ±Êý¾Ý´«ÊäÍê³ÉÊ±£¬×îºÃÈ¡ÏûTCP_CORK Ñ¡ÏîÉèÖÃ¸øÁ¬½Ó¡°°ÎÈ¥Èû×Ó¡±ÒÔ±ãÈÎÒ»²¿·ÖµÄÖ¡¶¼ÄÜ·¢ËÍ³öÈ¥¡£ÕâÍ¬¡°Èû×¡¡±ÍøÂçÁ¬½ÓÍ¬µÈÖØÒª¡£ 
      ×Ü¶øÑÔÖ®£¬Èç¹ûÄã¿Ï¶¨ÄÜÒ»Æð·¢ËÍ¶à¸öÊý¾Ý¼¯ºÏ£¨ÀýÈçHTTPÏìÓ¦µÄÍ·ºÍÕýÎÄ£©£¬ÄÇÃ´ÎÒÃÇ½¨ÒéÄãÉèÖÃTCP_CORKÑ¡Ïî£¬ÕâÑùÔÚÕâÐ©Êý¾ÝÖ®¼ä²»´æÔÚÑÓ³Ù¡£
      ÄÜ¼«´óµØÓÐÒæÓÚWWW¡¢FTPÒÔ¼°ÎÄ¼þ·þÎñÆ÷µÄÐÔÄÜ£¬Í¬Ê±Ò²¼ò»¯ÁËÄãµÄ¹¤×÷¡£Ê¾Àý´úÂëÈçÏÂ£º 
      
      intfd, on = 1; 
      ¡­ 
      ´Ë´¦ÊÇ´´½¨Ì×½Ó×ÖµÈ²Ù×÷£¬³öÓÚÆª·ùµÄ¿¼ÂÇÊ¡ÂÔ
      ¡­ 
      setsockopt (fd, SOL_TCP, TCP_CORK, &on, sizeof (on));  cork 
      write (fd, ¡­); 
      fprintf (fd, ¡­); 
      sendfile (fd, ¡­); 
      write (fd, ¡­); 
      sendfile (fd, ¡­); 
      ¡­ 
      on = 0; 
      setsockopt (fd, SOL_TCP, TCP_CORK, &on, sizeof (on));  °ÎÈ¥Èû×Ó 
      »òÕß
      setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(char*)&yes,sizeof(int));
      */
    /*
    Óï·¨£ºtcp_nodelay on | off;
    Ä¬ÈÏ£ºtcp_nodelay on;
    ÅäÖÃ¿é£ºhttp¡¢server¡¢location
    È·¶¨¶ÔkeepaliveÁ¬½ÓÊÇ·ñÊ¹ÓÃTCP_NODELAYÑ¡Ïî¡£ TCP_NODEALYÆäÊµ¾ÍÊÇ½ûÓÃnaggleËã·¨£¬¼´Ê¹ÊÇÐ¡°üÒ²Á¢¼´·¢ËÍ£¬TCP_CORKÔòºÍËûÏà·´£¬Ö»ÓÐÌî³äÂúºó²Å·¢ËÍ
    */
    { ngx_string("tcp_nodelay"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, tcp_nodelay),
      NULL },

/*    reset_timeout_connection (¸Ð¾õºÜ¶ànginxÔ´ÂëÃ»Õâ¸ö²ÎÊý)
     
    Óï·¨£ºreset_timeout_connection on | off;
    
    Ä¬ÈÏ£ºreset_timeout_connection off;
    
    ÅäÖÃ¿é£ºhttp¡¢server¡¢location
    
    Á¬½Ó³¬Ê±ºó½«Í¨¹ýÏò¿Í»§¶Ë·¢ËÍRST°üÀ´Ö±½ÓÖØÖÃÁ¬½Ó¡£Õâ¸öÑ¡Ïî´ò¿ªºó£¬Nginx»áÔÚÄ³¸öÁ¬½Ó³¬Ê±ºó£¬²»ÊÇÊ¹ÓÃÕý³£ÇéÐÎÏÂµÄËÄ´ÎÎÕÊÖ¹Ø±ÕTCPÁ¬½Ó£¬
    ¶øÊÇÖ±½ÓÏòÓÃ»§·¢ËÍRSTÖØÖÃ°ü£¬²»ÔÙµÈ´ýÓÃ»§µÄÓ¦´ð£¬Ö±½ÓÊÍ·ÅNginx·þÎñÆ÷ÉÏ¹ØÓÚÕâ¸öÌ×½Ó×ÖÊ¹ÓÃµÄËùÓÐ»º´æ£¨ÈçTCP»¬¶¯´°¿Ú£©¡£Ïà±ÈÕý³£µÄ¹Ø±Õ·½Ê½£¬
    ËüÊ¹µÃ·þÎñÆ÷±ÜÃâ²úÉúÐí¶à´¦ÓÚFIN_WAIT_1¡¢FIN_WAIT_2¡¢TIME_WAIT×´Ì¬µÄTCPÁ¬½Ó¡£
    
    ×¢Òâ£¬Ê¹ÓÃRSTÖØÖÃ°ü¹Ø±ÕÁ¬½Ó»á´øÀ´Ò»Ð©ÎÊÌâ£¬Ä¬ÈÏÇé¿öÏÂ²»»á¿ªÆô¡£
*/         
    /*
    ·¢ËÍÏìÓ¦µÄ³¬Ê±Ê±¼ä
    Óï·¨£ºsend_timeout time;
    Ä¬ÈÏ£ºsend_timeout 60;
    ÅäÖÃ¿é£ºhttp¡¢server¡¢location
    Õâ¸ö³¬Ê±Ê±¼äÊÇ·¢ËÍÏìÓ¦µÄ³¬Ê±Ê±¼ä£¬¼´Nginx·þÎñÆ÷Ïò¿Í»§¶Ë·¢ËÍÁËÊý¾Ý°ü£¬µ«¿Í»§¶ËÒ»Ö±Ã»ÓÐÈ¥½ÓÊÕÕâ¸öÊý¾Ý°ü¡£Èç¹ûÄ³¸öÁ¬½Ó³¬¹ý
    send_timeout¶¨ÒåµÄ³¬Ê±Ê±¼ä£¬ÄÇÃ´Nginx½«»á¹Ø±ÕÕâ¸öÁ¬½Ó
    */
    { ngx_string("send_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, send_timeout),
      NULL },

    { ngx_string("send_lowat"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, send_lowat),
      &ngx_http_core_lowat_post },
/* 
clcf->postpone_output£ºÓÉÓÚ´¦Àípostpone_outputÖ¸Áî£¬ÓÃÓÚÉèÖÃÑÓÊ±Êä³öµÄãÐÖµ¡£±ÈÈçÖ¸Áî¡°postpone s¡±£¬µ±Êä³öÄÚÈÝµÄsizeÐ¡ÓÚs£¬ Ä¬ÈÏ1460
²¢ÇÒ²»ÊÇ×îºóÒ»¸öbuffer£¬Ò²²»ÐèÒªflush£¬ÄÇÃ´¾ÍÑÓÊ±Êä³ö¡£¼ûngx_http_write_filter  
*/
    { ngx_string("postpone_output"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, postpone_output),
      NULL },

/* 
Óï·¨:  limit_rate rate;
Ä¬ÈÏÖµ:  limit_rate 0;
ÉÏÏÂÎÄ:  http, server, location, if in location

ÏÞÖÆÏò¿Í»§¶Ë´«ËÍÏìÓ¦µÄËÙÂÊÏÞÖÆ¡£²ÎÊýrateµÄµ¥Î»ÊÇ×Ö½Ú/Ãë£¬ÉèÖÃÎª0½«¹Ø±ÕÏÞËÙ¡£ nginx°´Á¬½ÓÏÞËÙ£¬ËùÒÔÈç¹ûÄ³¸ö¿Í»§¶ËÍ¬Ê±¿ªÆôÁËÁ½¸öÁ¬½Ó£¬
ÄÇÃ´¿Í»§¶ËµÄÕûÌåËÙÂÊÊÇÕâÌõÖ¸ÁîÉèÖÃÖµµÄ2±¶¡£ 

Ò²¿ÉÒÔÀûÓÃ$limit_rate±äÁ¿ÉèÖÃÁ÷Á¿ÏÞÖÆ¡£Èç¹ûÏëÔÚÌØ¶¨Ìõ¼þÏÂÏÞÖÆÏìÓ¦´«ÊäËÙÂÊ£¬¿ÉÒÔÊ¹ÓÃÕâ¸ö¹¦ÄÜ£º 
server {

    if ($slow) {
        set $limit_rate 4k;
    }
    ...
}

´ËÍâ£¬Ò²¿ÉÒÔÍ¨¹ý¡°X-Accel-Limit-Rate¡±ÏìÓ¦Í·À´Íê³ÉËÙÂÊÏÞÖÆ¡£ ÕâÖÖ»úÖÆ¿ÉÒÔÓÃproxy_ignore_headersÖ¸ÁîºÍ fastcgi_ignore_headersÖ¸Áî¹Ø±Õ¡£ 
*/
    { ngx_string("limit_rate"), //limit_rateÏÞÖÆ°üÌåµÄ·¢ËÍËÙ¶È£¬limit_reqÏÞÖÆÁ¬½ÓÇëÇóÁ¬ÀíËÙ¶È
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, limit_rate),
      NULL },

/*

Óï·¨:  limit_rate_after size;
Ä¬ÈÏÖµ:  limit_rate_after 0;
ÉÏÏÂÎÄ:  http, server, location, if in location
 

ÉèÖÃ²»ÏÞËÙ´«ÊäµÄÏìÓ¦´óÐ¡¡£µ±´«ÊäÁ¿´óÓÚ´ËÖµÊ±£¬³¬³ö²¿·Ö½«ÏÞËÙ´«ËÍ¡£ 
±ÈÈç: 
location /flv/ {
    flv;
    limit_rate_after 500k;
    limit_rate       50k;
}
*/
    { ngx_string("limit_rate_after"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, limit_rate_after),
      NULL },
        /*
        keepalive³¬Ê±Ê±¼ä
        Óï·¨£ºkeepalive_timeout time£¨Ä¬ÈÏµ¥Î»£ºÃë£©;
        Ä¬ÈÏ£ºkeepalive_timeout 75;
        ÅäÖÃ¿é£ºhttp¡¢server¡¢location
        Ò»¸ökeepalive Á¬½ÓÔÚÏÐÖÃ³¬¹ýÒ»¶¨Ê±¼äºó£¨Ä¬ÈÏµÄÊÇ75Ãë£©£¬·þÎñÆ÷ºÍä¯ÀÀÆ÷¶¼»áÈ¥¹Ø±ÕÕâ¸öÁ¬½Ó¡£µ±È»£¬keepalive_timeoutÅäÖÃÏîÊÇÓÃ
        À´Ô¼ÊøNginx·þÎñÆ÷µÄ£¬NginxÒ²»á°´ÕÕ¹æ·¶°ÑÕâ¸öÊ±¼ä´«¸øä¯ÀÀÆ÷£¬µ«Ã¿¸öä¯ÀÀÆ÷¶Ô´ýkeepaliveµÄ²ßÂÔÓÐ¿ÉÄÜÊÇ²»Í¬µÄ¡£
        */ //×¢ÒâºÍngx_http_upstream_keepalive_commandsÖÐkeepaliveµÄÇø±ð
    { ngx_string("keepalive_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_core_keepalive,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
/*
Ò»¸ökeepalive³¤Á¬½ÓÉÏÔÊÐí³ÐÔØµÄÇëÇó×î´óÊý
Óï·¨£ºkeepalive_requests n;
Ä¬ÈÏ£ºkeepalive_requests 100;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
Ò»¸ökeepaliveÁ¬½ÓÉÏÄ¬ÈÏ×î¶àÖ»ÄÜ·¢ËÍ100¸öÇëÇó¡£ ÉèÖÃÍ¨¹ýÒ»¸ö³¤Á¬½Ó¿ÉÒÔ´¦ÀíµÄ×î´óÇëÇóÊý¡£ ÇëÇóÊý³¬¹ý´ËÖµ£¬³¤Á¬½Ó½«¹Ø±Õ¡£ 
*/
    { ngx_string("keepalive_requests"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, keepalive_requests),
      NULL },
/*
¶ÔÄ³Ð©ä¯ÀÀÆ÷½ûÓÃkeepalive¹¦ÄÜ
Óï·¨£ºkeepalive_disable [ msie6 | safari | none ]...
Ä¬ÈÏ£ºkeepalive_disable  msie6 safari
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
HTTPÇëÇóÖÐµÄkeepalive¹¦ÄÜÊÇÎªÁËÈÃ¶à¸öÇëÇó¸´ÓÃÒ»¸öHTTP³¤Á¬½Ó£¬Õâ¸ö¹¦ÄÜ¶Ô·þÎñÆ÷µÄÐÔÄÜÌá¸ßÊÇºÜÓÐ°ïÖúµÄ¡£µ«ÓÐÐ©ä¯ÀÀÆ÷£¬ÈçIE 6ºÍSafari£¬
ËüÃÇ¶ÔÓÚÊ¹ÓÃkeepalive¹¦ÄÜµÄPOSTÇëÇó´¦ÀíÓÐ¹¦ÄÜÐÔÎÊÌâ¡£Òò´Ë£¬Õë¶ÔIE 6¼°ÆäÔçÆÚ°æ±¾¡¢Safariä¯ÀÀÆ÷Ä¬ÈÏÊÇ½ûÓÃkeepalive¹¦ÄÜµÄ¡£
*/
    { ngx_string("keepalive_disable"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, keepalive_disable),
      &ngx_http_core_keepalive_disable },

       /*
        Ïà¶ÔÓÚNGX HTTP ACCESS PHASE½×¶Î´¦Àí·½·¨£¬satisfyÅäÖÃÏî²ÎÊýµÄÒâÒå
        ©³©¥©¥©¥©¥©¥©¥©¥©×©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©·
        ©§satisfyµÄ²ÎÊý ©§    ÒâÒå                                                                              ©§
        ©Ç©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
        ©§              ©§    NGX HTTP ACCESS PHASE½×¶Î¿ÉÄÜÓÐºÜ¶àHTTPÄ£¿é¶¼¶Ô¿ØÖÆÇëÇóµÄ·ÃÎÊÈ¨ÏÞ¸ÐÐËÈ¤£¬         ©§
        ©§              ©§ÄÇÃ´ÒÔÄÄÒ»¸öÎª×¼ÄØ£¿µ±satisfyµÄ²ÎÊýÎªallÊ±£¬ÕâÐ©HTTPÄ£¿é±ØÐëÍ¬Ê±·¢Éú×÷ÓÃ£¬¼´ÒÔ¸Ã½×    ©§
        ©§all           ©§                                                                                      ©§
        ©§              ©§¶ÎÖÐÈ«²¿µÄhandler·½·¨¹²Í¬¾ö¶¨ÇëÇóµÄ·ÃÎÊÈ¨ÏÞ£¬»»¾ä»°Ëµ£¬ÕâÒ»½×¶ÎµÄËùÓÐhandler·½·¨±Ø    ©§
        ©§              ©§ÐëÈ«²¿·µ»ØNGX OK²ÅÄÜÈÏÎªÇëÇó¾ßÓÐ·ÃÎÊÈ¨ÏÞ                                              ©§
        ©Ç©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
        ©§              ©§  ÓëallÏà·´£¬²ÎÊýÎªanyÊ±ÒâÎ¶×ÅÔÚNGX¡ªHTTP__ ACCESS¡ªPHASE½×¶ÎÖ»ÒªÓÐÈÎÒâÒ»¸ö           ©§
        ©§              ©§HTTPÄ£¿éÈÏÎªÇëÇóºÏ·¨£¬¾Í²»ÓÃÔÙµ÷ÓÃÆäËûHTTPÄ£¿é¼ÌÐø¼ì²éÁË£¬¿ÉÒÔÈÏÎªÇëÇóÊÇ¾ßÓÐ·ÃÎÊ      ©§
        ©§              ©§È¨ÏÞµÄ¡£Êµ¼ÊÉÏ£¬ÕâÊ±µÄÇé¿öÓÐÐ©¸´ÔÓ£ºÈç¹ûÆäÖÐÈÎºÎÒ»¸öhandler·½·¨·µ»ØNGX¶þOK£¬ÔòÈÏÎª    ©§
        ©§              ©§ÇëÇó¾ßÓÐ·ÃÎÊÈ¨ÏÞ£»Èç¹ûÄ³Ò»¸öhandler·½·¨·µ»Ø403ÈÖÕß401£¬ÔòÈÏÎªÇëÇóÃ»ÓÐ·ÃÎÊÈ¨ÏÞ£¬»¹     ©§
        ©§any           ©§                                                                                      ©§
        ©§              ©§ÐèÒª¼ì²éNGX¡ªHTTP¡ªACCESS¡ªPHASE½×¶ÎµÄÆäËûhandler·½·¨¡£Ò²¾ÍÊÇËµ£¬anyÅäÖÃÏîÏÂÈÎ        ©§
        ©§              ©§ºÎÒ»¸öhandler·½·¨Ò»µ©ÈÏÎªÇëÇó¾ßÓÐ·ÃÎÊÈ¨ÏÞ£¬¾ÍÈÏÎªÕâÒ»½×¶ÎÖ´ÐÐ³É¹¦£¬¼ÌÐøÏòÏÂÖ´ÐÐ£»Èç   ©§
        ©§              ©§¹ûÆäÖÐÒ»¸öhandler·½·¨ÈÏÎªÃ»ÓÐ·ÃÎÊÈ¨ÏÞ£¬ÔòÎ´±ØÒÔ´ËÎª×¼£¬»¹ÐèÒª¼ì²âÆäËûµÄhanlder·½·¨¡£  ©§
        ©§              ©§allºÍanyÓÐµãÏñ¡°&&¡±ºÍ¡°¡§¡±µÄ¹ØÏµ                                                    ©§
        ©»©¥©¥©¥©¥©¥©¥©¥©ß©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¿
        */
    { ngx_string("satisfy"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, satisfy),
      &ngx_http_core_satisfy },

    /*
     internal 
     Óï·¨£ºinternal 
     Ä¬ÈÏÖµ£ºno 
     Ê¹ÓÃ×Ö¶Î£º location 
     internalÖ¸ÁîÖ¸¶¨Ä³¸ölocationÖ»ÄÜ±»¡°ÄÚ²¿µÄ¡±ÇëÇóµ÷ÓÃ£¬Íâ²¿µÄµ÷ÓÃÇëÇó»á·µ»Ø"Not found" (404)
     ¡°ÄÚ²¿µÄ¡±ÊÇÖ¸ÏÂÁÐÀàÐÍ£º
     
     ¡¤Ö¸Áîerror_pageÖØ¶¨ÏòµÄÇëÇó¡£
     ¡¤ngx_http_ssi_moduleÄ£¿éÖÐÊ¹ÓÃinclude virtualÖ¸Áî´´½¨µÄÄ³Ð©×ÓÇëÇó¡£
     ¡¤ngx_http_rewrite_moduleÄ£¿éÖÐÊ¹ÓÃrewriteÖ¸ÁîÐÞ¸ÄµÄÇëÇó¡£
     
     Ò»¸ö·ÀÖ¹´íÎóÒ³Ãæ±»ÓÃ»§Ö±½Ó·ÃÎÊµÄÀý×Ó£º
     
     error_page 404 /404.html;
     location  /404.html {  //±íÊ¾Æ¥Åä/404.htmlµÄlocation±ØÐëuriÊÇÖØ¶¨ÏòºóµÄuri
       internal;
     }
     */ 
     /* ¸Ãlocation{}±ØÐëÊÇÄÚ²¿ÖØ¶¨Ïò(indexÖØ¶¨Ïò ¡¢error_pagesµÈÖØ¶¨Ïòµ÷ÓÃngx_http_internal_redirect)ºóÆ¥ÅäµÄlocation{}£¬·ñÔò²»ÈÃ·ÃÎÊ¸Ãlocation */
     //ÔÚlocation{}ÖÐÅäÖÃÁËinternal£¬±íÊ¾Æ¥Åä¸ÃuriµÄlocation{}±ØÐëÊÇ½øÐÐÖØ¶¨ÏòºóÆ¥ÅäµÄ¸Ãlocation,Èç¹û²»Âú×ãÌõ¼þÖ±½Ó·µ»ØNGX_HTTP_NOT_FOUND£¬
     //ÉúÐ§µØ·½¼ûngx_http_core_find_config_phase   
    { ngx_string("internal"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_core_internal,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
/*
lingering_close
Óï·¨£ºlingering_close off | on | always;
Ä¬ÈÏ£ºlingering_close on;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
¸ÃÅäÖÃ¿ØÖÆNginx¹Ø±ÕÓÃ»§Á¬½ÓµÄ·½Ê½¡£always±íÊ¾¹Ø±ÕÓÃ»§Á¬½ÓÇ°±ØÐëÎÞÌõ¼þµØ´¦ÀíÁ¬½ÓÉÏËùÓÐÓÃ»§·¢ËÍµÄÊý¾Ý¡£off±íÊ¾¹Ø±ÕÁ¬½ÓÊ±ÍêÈ«²»¹ÜÁ¬½Ó
ÉÏÊÇ·ñÒÑ¾­ÓÐ×¼±¸¾ÍÐ÷µÄÀ´×ÔÓÃ»§µÄÊý¾Ý¡£onÊÇÖÐ¼äÖµ£¬Ò»°ãÇé¿öÏÂÔÚ¹Ø±ÕÁ¬½ÓÇ°¶¼»á´¦ÀíÁ¬½ÓÉÏµÄÓÃ»§·¢ËÍµÄÊý¾Ý£¬³ýÁËÓÐÐ©Çé¿öÏÂÔÚÒµÎñÉÏÈÏ¶¨ÕâÖ®ºóµÄÊý¾ÝÊÇ²»±ØÒªµÄ¡£
*/
    { ngx_string("lingering_close"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, lingering_close),
      &ngx_http_core_lingering_close },

    /*
    lingering_time
    Óï·¨£ºlingering_time time;
    Ä¬ÈÏ£ºlingering_time 30s;
    ÅäÖÃ¿é£ºhttp¡¢server¡¢location
    lingering_closeÆôÓÃºó£¬Õâ¸öÅäÖÃÏî¶ÔÓÚÉÏ´«´óÎÄ¼þºÜÓÐÓÃ¡£ÉÏÎÄ½²¹ý£¬µ±ÓÃ»§ÇëÇóµÄContent-Length´óÓÚmax_client_body_sizeÅäÖÃÊ±£¬
    Nginx·þÎñ»áÁ¢¿ÌÏòÓÃ»§·¢ËÍ413£¨Request entity too large£©ÏìÓ¦¡£µ«ÊÇ£¬ºÜ¶à¿Í»§¶Ë¿ÉÄÜ²»¹Ü413·µ»ØÖµ£¬ÈÔÈ»³ÖÐø²»¶ÏµØÉÏ´«HTTP body£¬
    ÕâÊ±£¬¾­¹ýÁËlingering_timeÉèÖÃµÄÊ±¼äºó£¬Nginx½«²»¹ÜÓÃ»§ÊÇ·ñÈÔÔÚÉÏ´«£¬¶¼»á°ÑÁ¬½Ó¹Ø±Õµô¡£
    */
    { ngx_string("lingering_time"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, lingering_time),
      NULL },
/*
lingering_timeout
Óï·¨£ºlingering_timeout time;
Ä¬ÈÏ£ºlingering_timeout 5s;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
lingering_closeÉúÐ§ºó£¬ÔÚ¹Ø±ÕÁ¬½ÓÇ°£¬»á¼ì²âÊÇ·ñÓÐÓÃ»§·¢ËÍµÄÊý¾Ýµ½´ï·þÎñÆ÷£¬Èç¹û³¬¹ýlingering_timeoutÊ±¼äºó»¹Ã»ÓÐÊý¾Ý¿É¶Á£¬
¾ÍÖ±½Ó¹Ø±ÕÁ¬½Ó£»·ñÔò£¬±ØÐëÔÚ¶ÁÈ¡ÍêÁ¬½Ó»º³åÇøÉÏµÄÊý¾Ý²¢¶ªÆúµôºó²Å»á¹Ø±ÕÁ¬½Ó¡£
*/
    { ngx_string("lingering_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, lingering_timeout),
      NULL },

    { ngx_string("reset_timedout_connection"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, reset_timedout_connection),
      NULL },
    /*  server_name_in_redirect on | off ¸ÃÅäÖÃÐèÒªÅäºÏserver_nameÊ¹ÓÃ¡£ÔÚÊ¹ÓÃon´ò¿ªºó,±íÊ¾ÔÚÖØ¶¨ÏòÇëÇóÊ±»áÊ¹ÓÃ
    server_nameÀïµÄµÚÒ»¸öÖ÷»úÃû´úÌæÔ­ÏÈÇëÇóÖÐµÄHostÍ·²¿£¬¶øÊ¹ÓÃoff¹Ø±ÕÊ±£¬±íÊ¾ÔÚÖØ¶¨ÏòÇëÇóÊ±Ê¹ÓÃÇëÇó±¾ÉíµÄHOSTÍ·²¿ */
    { ngx_string("server_name_in_redirect"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, server_name_in_redirect),
      NULL },

    { ngx_string("port_in_redirect"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, port_in_redirect),
      NULL },

    { ngx_string("msie_padding"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, msie_padding),
      NULL },

    { ngx_string("msie_refresh"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, msie_refresh),
      NULL },

/*
ÎÄ¼þÎ´ÕÒµ½Ê±ÊÇ·ñ¼ÇÂ¼µ½errorÈÕÖ¾
Óï·¨£ºlog_not_found on | off;
Ä¬ÈÏ£ºlog_not_found on;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
´ËÅäÖÃÏî±íÊ¾µ±´¦ÀíÓÃ»§ÇëÇóÇÒÐèÒª·ÃÎÊÎÄ¼þÊ±£¬Èç¹ûÃ»ÓÐÕÒµ½ÎÄ¼þ£¬ÊÇ·ñ½«´íÎóÈÕÖ¾¼ÇÂ¼µ½error.logÎÄ¼þÖÐ¡£Õâ½öÓÃÓÚ¶¨Î»ÎÊÌâ¡£
*/
    { ngx_string("log_not_found"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, log_not_found),
      NULL },

    { ngx_string("log_subrequest"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, log_subrequest),
      NULL },

    /* 
    ÊÇ·ñÔÊÐíµÝ¹éÊ¹ÓÃerror_page
    Óï·¨£ºrecursive_error_pages [on | off];
    Ä¬ÈÏ£ºrecursive_error_pages off;
    ÅäÖÃ¿é£ºhttp¡¢server¡¢location
    È·¶¨ÊÇ·ñÔÊÐíµÝ¹éµØ¶¨Òåerror_page¡£
    */
    { ngx_string("recursive_error_pages"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, recursive_error_pages),
      NULL },

/*
·µ»Ø´íÎóÒ³ÃæÊ±ÊÇ·ñÔÚServerÖÐ×¢Ã÷Nginx°æ±¾
Óï·¨£ºserver_tokens on | off;
Ä¬ÈÏ£ºserver_tokens on;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
±íÊ¾´¦ÀíÇëÇó³ö´íÊ±ÊÇ·ñÔÚÏìÓ¦µÄServerÍ·²¿ÖÐ±êÃ÷Nginx°æ±¾£¬ÕâÊÇÎªÁË·½±ã¶¨Î»ÎÊÌâ¡£
*/
    { ngx_string("server_tokens"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, server_tokens),
      NULL },

/*
¶ÔIf-Modified-SinceÍ·²¿µÄ´¦Àí²ßÂÔ
Óï·¨£ºif_modified_since [off|exact|before];
Ä¬ÈÏ£ºif_modified_since exact;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
³öÓÚÐÔÄÜ¿¼ÂÇ£¬Webä¯ÀÀÆ÷Ò»°ã»áÔÚ¿Í»§¶Ë±¾µØ»º´æÒ»Ð©ÎÄ¼þ£¬²¢´æ´¢µ±Ê±»ñÈ¡µÄÊ±¼ä¡£ÕâÑù£¬ÏÂ´ÎÏòWeb·þÎñÆ÷»ñÈ¡»º´æ¹ýµÄ×ÊÔ´Ê±£¬
¾Í¿ÉÒÔÓÃIf-Modified-SinceÍ·²¿°ÑÉÏ´Î»ñÈ¡µÄÊ±¼äÉÓ´øÉÏ£¬¶øif_modified_since½«¸ù¾ÝºóÃæµÄ²ÎÊý¾ö¶¨ÈçºÎ´¦ÀíIf-Modified-SinceÍ·²¿¡£

Ïà¹Ø²ÎÊýËµÃ÷ÈçÏÂ¡£
off£º±íÊ¾ºöÂÔÓÃ»§ÇëÇóÖÐµÄIf-Modified-SinceÍ·²¿¡£ÕâÊ±£¬Èç¹û»ñÈ¡Ò»¸öÎÄ¼þ£¬ÄÇÃ´»áÕý³£µØ·µ»ØÎÄ¼þÄÚÈÝ¡£HTTPÏìÓ¦ÂëÍ¨³£ÊÇ200¡£
exact£º½«If-Modified-SinceÍ·²¿°üº¬µÄÊ±¼äÓë½«Òª·µ»ØµÄÎÄ¼þÉÏ´ÎÐÞ¸ÄµÄÊ±¼ä×ö¾«È·±È½Ï£¬Èç¹ûÃ»ÓÐÆ¥ÅäÉÏ£¬Ôò·µ»Ø200ºÍÎÄ¼þµÄÊµ¼ÊÄÚÈÝ£¬Èç¹ûÆ¥ÅäÉÏ£¬
Ôò±íÊ¾ä¯ÀÀÆ÷»º´æµÄÎÄ¼þÄÚÈÝÒÑ¾­ÊÇ×îÐÂµÄÁË£¬Ã»ÓÐ±ØÒªÔÙ·µ»ØÎÄ¼þ´Ó¶øÀË·ÑÊ±¼äÓë´ø¿íÁË£¬ÕâÊ±»á·µ»Ø304 Not Modified£¬ä¯ÀÀÆ÷ÊÕµ½ºó»áÖ±½Ó¶ÁÈ¡×Ô¼ºµÄ±¾µØ»º´æ¡£
before£ºÊÇ±Èexact¸ü¿íËÉµÄ±È½Ï¡£Ö»ÒªÎÄ¼þµÄÉÏ´ÎÐÞ¸ÄÊ±¼äµÈÓÚ»òÕßÔçÓÚÓÃ»§ÇëÇóÖÐµÄIf-Modified-SinceÍ·²¿µÄÊ±¼ä£¬¾Í»áÏò¿Í»§¶Ë·µ»Ø304 Not Modified¡£
*/ //ÉúÐ§¼ûngx_http_test_if_modified
    { ngx_string("if_modified_since"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, if_modified_since),
      &ngx_http_core_if_modified_since },

    { ngx_string("max_ranges"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, max_ranges),
      NULL },

    { ngx_string("chunked_transfer_encoding"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, chunked_transfer_encoding),
      NULL },

    //ÉúÐ§¼ûngx_http_set_etag
    { ngx_string("etag"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, etag),
      NULL },

    /*
    ¸ù¾ÝHTTP·µ»ØÂëÖØ¶¨ÏòÒ³Ãæ
    Óï·¨£ºerror_page code [ code... ] [ = | =answer-code ] uri | @named_location
    ÅäÖÃ¿é£ºhttp¡¢server¡¢location¡¢if 
    
    µ±¶ÔÓÚÄ³¸öÇëÇó·µ»Ø´íÎóÂëÊ±£¬Èç¹ûÆ¥ÅäÉÏÁËerror_pageÖÐÉèÖÃµÄcode£¬ÔòÖØ¶¨Ïòµ½ÐÂµÄURIÖÐ¡£ÀýÈç£º
    error_page   404          /404.html;
    error_page   502 503 504  /50x.html;
    error_page   403          http://example.com/forbidden.html;
    error_page   404          = @fetch;
    
    ×¢Òâ£¬ËäÈ»ÖØ¶¨ÏòÁËURI£¬µ«·µ»ØµÄHTTP´íÎóÂë»¹ÊÇÓëÔ­À´µÄÏàÍ¬¡£ÓÃ»§¿ÉÒÔÍ¨¹ý¡°=¡±À´¸ü¸Ä·µ»ØµÄ´íÎóÂë£¬ÀýÈç£º
    error_page 404 =200 /empty.gif;
    error_page 404 =403 /forbidden.gif;
    
    Ò²¿ÉÒÔ²»Ö¸¶¨È·ÇÐµÄ·µ»Ø´íÎóÂë£¬¶øÊÇÓÉÖØ¶¨ÏòºóÊµ¼Ê´¦ÀíµÄÕæÊµ½á¹ûÀ´¾ö¶¨£¬ÕâÊ±£¬Ö»Òª°Ñ¡°=¡±ºóÃæµÄ´íÎóÂëÈ¥µô¼´¿É£¬ÀýÈç£º
    error_page 404 = /empty.gif;
    
    Èç¹û²»ÏëÐÞ¸ÄURI£¬Ö»ÊÇÏëÈÃÕâÑùµÄÇëÇóÖØ¶¨Ïòµ½ÁíÒ»¸ölocationÖÐ½øÐÐ´¦Àí£¬ÄÇÃ´¿ÉÒÔÕâÑùÉèÖÃ£º
    location / (
        error_page 404 @fallback;
    )
     
    location @fallback (
        proxy_pass http://backend;
    )
    
    ÕâÑù£¬·µ»Ø404µÄÇëÇó»á±»·´Ïò´úÀíµ½http://backendÉÏÓÎ·þÎñÆ÷ÖÐ´¦Àí
    */ //try_filesºÍerror_page¶¼ÓÐÖØ¶¨Ïò¹¦ÄÜ  //error_page´íÎóÂë±ØÐëmust be between 300 and 599£¬²¢ÇÒ²»ÄÜÎª499£¬¼ûngx_http_core_error_page
    { ngx_string("error_page"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_2MORE,
      ngx_http_core_error_page,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /*
    
    Óï·¨£ºtry_files path1 [path2] uri;
    ÅäÖÃ¿é£ºserver¡¢location   

    try_filesºóÒª¸úÈô¸ÉÂ·¾¶£¬Èçpath1 path2...£¬¶øÇÒ×îºó±ØÐëÒªÓÐuri²ÎÊý£¬ÒâÒåÈçÏÂ£º³¢ÊÔ°´ÕÕË³Ðò·ÃÎÊÃ¿Ò»¸öpath£¬Èç¹û¿ÉÒÔÓÐÐ§µØ¶ÁÈ¡£¬
    ¾ÍÖ±½ÓÏòÓÃ»§·µ»ØÕâ¸öpath¶ÔÓ¦µÄÎÄ¼þ½áÊøÇëÇó£¬·ñÔò¼ÌÐøÏòÏÂ·ÃÎÊ¡£Èç¹ûËùÓÐµÄpath¶¼ÕÒ²»µ½ÓÐÐ§µÄÎÄ¼þ£¬¾ÍÖØ¶¨Ïòµ½×îºóµÄ²ÎÊýuriÉÏ¡£Òò´Ë£¬
    ×îºóÕâ¸ö²ÎÊýuri±ØÐë´æÔÚ£¬¶øÇÒËüÓ¦¸ÃÊÇ¿ÉÒÔÓÐÐ§ÖØ¶¨ÏòµÄ¡£ÀýÈç£º
    try_files /system/maintenance.html $uri $uri/index.html $uri.html @other;
    location @other {
      proxy_pass http://backend;
    }
    
    ÉÏÃæÕâ¶Î´úÂë±íÊ¾Èç¹ûÇ°ÃæµÄÂ·¾¶£¬Èç/system/maintenance.htmlµÈ£¬¶¼ÕÒ²»µ½£¬¾Í»á·´Ïò´úÀíµ½http://backend·þÎñÉÏ¡£»¹¿ÉÒÔÓÃÖ¸¶¨´íÎóÂëµÄ·½Ê½Óëerror_pageÅäºÏÊ¹ÓÃ£¬ÀýÈç£º
    location / {
      try_files $uri $uri/ /error.php?c=404 =404;
    }
    */ //try_filesºÍerror_page¶¼ÓÐÖØ¶¨Ïò¹¦ÄÜ
    { ngx_string("try_files"),  //×¢Òâtry_filesÖÁÉÙ°üº¬Á½¸ö²ÎÊý£¬·ñÔò½âÎöÅäÖÃÎÄ¼þ»á³ö´í
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_2MORE,
      ngx_http_core_try_files,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("post_action"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, post_action),
      NULL },
    
//    error_log file [ debug | info | notice | warn | error | crit ] 
    { ngx_string("error_log"), //ngx_errlog_moduleÖÐµÄerror_logÅäÖÃÖ»ÄÜÈ«¾ÖÅäÖÃ£¬ngx_http_core_moduleÔÚhttp{} server{} local{}ÖÐÅäÖÃ
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_core_error_log,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
´ò¿ªÎÄ¼þ»º´æ

Óï·¨£ºopen_file_cache max = N [inactive = time] | off;

Ä¬ÈÏ£ºopen_file_cache off;

ÅäÖÃ¿é£ºhttp¡¢server¡¢location

ÎÄ¼þ»º´æ»áÔÚÄÚ´æÖÐ´æ´¢ÒÔÏÂ3ÖÖÐÅÏ¢£º

ÎÄ¼þ¾ä±ú¡¢ÎÄ¼þ´óÐ¡ºÍÉÏ´ÎÐÞ¸ÄÊ±¼ä¡£

ÒÑ¾­´ò¿ª¹ýµÄÄ¿Â¼½á¹¹¡£

Ã»ÓÐÕÒµ½µÄ»òÕßÃ»ÓÐÈ¨ÏÞ²Ù×÷µÄÎÄ¼þÐÅÏ¢¡£

ÕâÑù£¬Í¨¹ý¶ÁÈ¡»º´æ¾Í¼õÉÙÁË¶Ô´ÅÅÌµÄ²Ù×÷¡£

¸ÃÅäÖÃÏîºóÃæ¸ú3ÖÖ²ÎÊý¡£
max£º±íÊ¾ÔÚÄÚ´æÖÐ´æ´¢ÔªËØµÄ×î´ó¸öÊý¡£µ±´ïµ½×î´óÏÞÖÆÊýÁ¿ºó£¬½«²ÉÓÃLRU£¨Least Recently Used£©Ëã·¨´Ó»º´æÖÐÌÔÌ­×î½ü×îÉÙÊ¹ÓÃµÄÔªËØ¡£
inactive£º±íÊ¾ÔÚinactiveÖ¸¶¨µÄÊ±¼ä¶ÎÄÚÃ»ÓÐ±»·ÃÎÊ¹ýµÄÔªËØ½«»á±»ÌÔÌ­¡£Ä¬ÈÏÊ±¼äÎª60Ãë¡£
off£º¹Ø±Õ»º´æ¹¦ÄÜ¡£
ÀýÈç£º
open_file_cache max=1000 inactive=20s; //Èç¹û20sÄÚÓÐÇëÇóµ½¸Ã»º´æ£¬Ôò¸Ã»º´æ¼ÌÐøÉúÐ§£¬Èç¹û20sÄÚ¶¼Ã»ÓÐÇëÇó¸Ã»º´æ£¬Ôò20sÍâÇëÇó£¬»áÖØÐÂ»ñÈ¡Ô­ÎÄ¼þ²¢Éú³É»º´æ
*/

/*
   ×¢Òâopen_file_cache inactive=20sºÍfastcgi_cache_valid 20sµÄÇø±ð£¬Ç°ÕßÖ¸µÄÊÇÈç¹û¿Í»§¶ËÔÚ20sÄÚÃ»ÓÐÇëÇóµ½À´£¬Ôò»á°Ñ¸Ã»º´æÎÄ¼þ¶ÔÓ¦µÄfd statÊôÐÔÐÅÏ¢
   ´Óngx_open_file_cache_t->rbtree(expire_queue)ÖÐÉ¾³ý(¿Í»§¶ËµÚÒ»´ÎÇëÇó¸Ãuri¶ÔÓ¦µÄ»º´æÎÄ¼þµÄÊ±ºò»á°Ñ¸ÃÎÄ¼þ¶ÔÓ¦µÄstatÐÅÏ¢½Úµãngx_cached_open_file_sÌí¼Óµ½
   ngx_open_file_cache_t->rbtree(expire_queue)ÖÐ)£¬´Ó¶øÌá¸ß»ñÈ¡»º´æÎÄ¼þµÄÐ§ÂÊ
   fastcgi_cache_validÖ¸µÄÊÇºÎÊ±»º´æÎÄ¼þ¹ýÆÚ£¬¹ýÆÚÔòÉ¾³ý£¬¶¨Ê±Ö´ÐÐngx_cache_manager_process_handler->ngx_http_file_cache_manager
*/

/* 
   Èç¹ûÃ»ÓÐÅäÖÃopen_file_cache max=1000 inactive=20s;£¬Ò²¾ÍÊÇËµÃ»ÓÐ»º´æcache»º´æÎÄ¼þ¶ÔÓ¦µÄÎÄ¼þstatÐÅÏ¢£¬ÔòÃ¿´Î¶¼Òª´ÓÐÂ´ò¿ªÎÄ¼þ»ñÈ¡ÎÄ¼þstatÐÅÏ¢£¬
   Èç¹ûÓÐÅäÖÃopen_file_cache£¬Ôò»á°Ñ´ò¿ªµÄcache»º´æÎÄ¼þstatÐÅÏ¢°´ÕÕngx_crc32_long×öhashºóÌí¼Óµ½ngx_cached_open_file_t->rbtreeÖÐ£¬ÕâÑùÏÂ´ÎÔÚÇëÇó¸Ã
   uri£¬Ôò¾Í²»ÓÃÔÙ´ÎopenÎÄ¼þºóÔÚstat»ñÈ¡ÎÄ¼þÊôÐÔÁË£¬ÕâÑù¿ÉÒÔÌá¸ßÐ§ÂÊ,²Î¿¼ngx_open_cached_file 

   ´´½¨»º´æÎÄ¼þstat½Úµãnodeºó£¬Ã¿´ÎÐÂÀ´ÇëÇóµÄÊ±ºò¶¼»á¸üÐÂaccessedÊ±¼ä£¬Òò´ËÖ»ÒªinactiveÊ±¼äÄÚÓÐÇëÇó£¬¾Í²»»áÉ¾³ý»º´æstat½Úµã£¬¼ûngx_expire_old_cached_files
   inactiveÊ±¼äÄÚÃ»ÓÐÐÂµÄÇëÇóÔò»á´ÓºìºÚÊ÷ÖÐÉ¾³ý¸Ã½Úµã£¬Í¬Ê±¹Ø±Õ¸ÃÎÄ¼þ¼ûngx_open_file_cleanup  ngx_close_cached_file  ngx_expire_old_cached_files
   */ //¿ÉÒÔ²Î¿¼ngx_open_file_cache_t  ²Î¿¼ngx_open_cached_file 
   
    { ngx_string("open_file_cache"), 
//open_file_cache inactive 30Ö÷ÒªÓÃÓÚÊÇ·ñÔÚ30sÄÚÓÐÐÂÇëÇó£¬Ã»ÓÐÔòÉ¾³ý»º´æ£¬¶øopen_file_cache_min_uses±íÊ¾Ö»Òª»º´æÔÚºìºÚÊ÷ÖÐ£¬²¢ÇÒ±éÀú¸ÃÎÄ¼þ´ÎÊý´ïµ½Ö¸¶¨´ÎÊý£¬Ôò²»»ácloseÎÄ¼þ£¬Ò²¾Í²»»á´ÓÐÂ»ñÈ¡statÐÅÏ¢
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_core_open_file_cache,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache),
      NULL },

/*
¼ìÑé»º´æÖÐÔªËØÓÐÐ§ÐÔµÄÆµÂÊ
Óï·¨£ºopen_file_cache_valid time;
Ä¬ÈÏ£ºopen_file_cache_valid 60s;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
ÉèÖÃ¼ì²éopen_file_cache»º´æstatÐÅÏ¢µÄÔªËØµÄÊ±¼ä¼ä¸ô¡£ 
*/ 
//±íÊ¾60sºóÀ´µÄµÚÒ»¸öÇëÇóÒª¶ÔÎÄ¼þstatÐÅÏ¢×öÒ»´Î¼ì²é£¬¼ì²éÊÇ·ñ·¢ËÍ±ä»¯£¬Èç¹û·¢ËÍ±ä»¯Ôò´ÓÐÂ»ñÈ¡ÎÄ¼þstatÐÅÏ¢»òÕß´ÓÐÂ´´½¨¸Ã½×¶Î£¬
    //ÉúÐ§ÔÚngx_open_cached_fileÖÐµÄ(&& now - file->created < of->valid ) 
    { ngx_string("open_file_cache_valid"), 
    //open_file_cache_min_usesºóÕßÊÇÅÐ¶ÏÊÇ·ñÐèÒªcloseÃèÊö·û£¬È»ºóÖØÐÂ´ò¿ª»ñÈ¡fdºÍstatÐÅÏ¢£¬open_file_cache_validÖ»ÊÇ¶¨ÆÚ¶Ôstat(³¬¹ý¸ÃÅäÖÃÊ±¼äºó£¬ÔÚÀ´Ò»¸öµÄÊ±ºò»á½øÐÐÅÐ¶Ï)½øÐÐ¸üÐÂ
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_sec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache_valid),
      NULL },

/*
²»±»ÌÔÌ­µÄ×îÐ¡·ÃÎÊ´ÎÊý
Óï·¨£ºopen_file_cache_min_uses number;
Ä¬ÈÏ£ºopen_file_cache_min_uses 1;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
 
ÉèÖÃÔÚÓÉopen_file_cacheÖ¸ÁîµÄinactive²ÎÊýÅäÖÃµÄ³¬Ê±Ê±¼äÄÚ£¬ÎÄ¼þÓ¦¸Ã±»·ÃÎÊµÄ×îÐ¡number(´ÎÊý)¡£Èç¹û·ÃÎÊ´ÎÊý´óÓÚµÈÓÚ´ËÖµ£¬ÎÄ¼þÃè
Êö·û»á±£ÁôÔÚ»º´æÖÐ£¬·ñÔò´Ó»º´æÖÐÉ¾³ý¡£ 
*/  //ÀýÈçopen_file_cache max=102400 inactive=20s; Ö»Òª¸Ã»º´æÎÄ¼þ±»±éÀú´ÎÊý³¬¹ýopen_file_cache_min_uses´ÎÇëÇó£¬Ôò»º´æÖÐµÄÎÄ¼þ¸ü¸ÄÐÅÏ¢²»±ä,²»»ácloseÎÄ¼þ
    //ÕâÊ±ºòµÄÇé¿öÊÇ:ÇëÇó´øÓÐIf-Modified-Since£¬µÃµ½µÄÊÇ304ÇÒLast-ModifiedÊ±¼äÃ»±ä
/*
file->uses >= min_uses±íÊ¾Ö»ÒªÔÚinactiveÊ±¼äÄÚ¸Ãngx_cached_open_file_s file½Úµã±»±éÀúµ½µÄ´ÎÊý´ïµ½min_uses´Î£¬ÔòÓÀÔ¶²»»á¹Ø±ÕÎÄ¼þ(Ò²¾ÍÊÇ²»ÓÃÖØÐÂ»ñÈ¡ÎÄ¼þstatÐÅÏ¢)£¬
³ý·Ç¸Ãcache nodeÊ§Ð§£¬»º´æ³¬Ê±inactiveºó»á´ÓºìºÚÊ÷ÖÐÉ¾³ý¸Ãfile node½Úµã£¬Í¬Ê±¹Ø±ÕÎÄ¼þµÈ¼ûngx_open_file_cleanup  ngx_close_cached_file  ngx_expire_old_cached_files
*/    { ngx_string("open_file_cache_min_uses"), 
//Ö»Òª»º´æÆ¥Åä´ÎÊý´ïµ½ÕâÃ´¶à´Î£¬¾Í²»»áÖØÐÂ¹Ø±Õclose¸ÃÎÄ¼þ»º´æ£¬ÏÂ´ÎÒ²¾Í²»»á´ÓÐÂ´ò¿ªÎÄ¼þ»ñÈ¡ÎÄ¼þÃèÊö·û£¬³ý·Ç»º´æÊ±¼äinactiveÄÚ¶¼Ã»ÓÐÐÂÇëÇó£¬Ôò»áÉ¾³ý½Úµã²¢¹Ø±ÕÎÄ¼þ
//open_file_cache inactive 30Ö÷ÒªÓÃÓÚÊÇ·ñÔÚ30sÄÚÓÐÐÂÇëÇó£¬Ã»ÓÐÔòÉ¾³ý»º´æ£¬¶øopen_file_cache_min_uses±íÊ¾Ö»Òª»º´æÔÚºìºÚÊ÷ÖÐ£¬²¢ÇÒ±éÀú¸ÃÎÄ¼þ´ÎÊý´ïµ½Ö¸¶¨´ÎÊý£¬Ôò²»»ácloseÎÄ¼þ£¬Ò²¾Í²»»á´ÓÐÂ»ñÈ¡statÐÅÏ¢

//open_file_cache_min_usesºóÕßÊÇÅÐ¶ÏÊÇ·ñÐèÒªcloseÃèÊö·û£¬È»ºóÖØÐÂ´ò¿ª»ñÈ¡fdºÍstatÐÅÏ¢£¬open_file_cache_validÖ»ÊÇ¶¨ÆÚ¶Ôstat(³¬¹ý¸ÃÅäÖÃÊ±¼äºó£¬ÔÚÀ´Ò»¸öµÄÊ±ºò»á½øÐÐÅÐ¶Ï)½øÐÐ¸üÐÂ

      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache_min_uses),
      NULL },

/*
ÊÇ·ñ»º´æ´ò¿ªÎÄ¼þ´íÎóµÄÐÅÏ¢
Óï·¨£ºopen_file_cache_errors on | off;
Ä¬ÈÏ£ºopen_file_cache_errors off;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
´ËÅäÖÃÏî±íÊ¾ÊÇ·ñÔÚÎÄ¼þ»º´æÖÐ»º´æ´ò¿ªÎÄ¼þÊ±³öÏÖµÄÕÒ²»µ½Â·¾¶¡¢Ã»ÓÐÈ¨ÏÞµÈ´íÎóÐÅÏ¢¡£
*/
    { ngx_string("open_file_cache_errors"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache_errors),
      NULL },

    { ngx_string("open_file_cache_events"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache_events),
      NULL },

/*
DNS½âÎöµØÖ·
Óï·¨£ºresolver address ...;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
ÉèÖÃDNSÃû×Ö½âÎö·þÎñÆ÷µÄµØÖ·£¬ÀýÈç£º
resolver 127.0.0.1 192.0.2.1;
*/
    { ngx_string("resolver"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_core_resolver,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
DNS½âÎöµÄ³¬Ê±Ê±¼ä
Óï·¨£ºresolver_timeout time;
Ä¬ÈÏ£ºresolver_timeout 30s;
ÅäÖÃ¿é£ºhttp¡¢server¡¢location
´ËÅäÖÃÏî±íÊ¾DNS½âÎöµÄ³¬Ê±Ê±¼ä¡£
*/ //²ú¿¼:http://theantway.com/2013/09/understanding_the_dns_resolving_in_nginx/         NginxµÄDNS½âÎö¹ý³Ì·ÖÎö 
    { ngx_string("resolver_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, resolver_timeout),
      NULL },

#if (NGX_HTTP_GZIP)

    { ngx_string("gzip_vary"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, gzip_vary),
      NULL },

    { ngx_string("gzip_http_version"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, gzip_http_version),
      &ngx_http_gzip_http_version },

    { ngx_string("gzip_proxied"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, gzip_proxied),
      &ngx_http_gzip_proxied_mask },

    { ngx_string("gzip_disable"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_gzip_disable,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

#endif

#if (NGX_HAVE_OPENAT)

/*
Óï·¨:  disable_symlinks off;
disable_symlinks on | if_not_owner [from=part];
 
Ä¬ÈÏÖµ:  disable_symlinks off; 
ÉÏÏÂÎÄ:  http, server, location
 
¾ö¶¨nginx´ò¿ªÎÄ¼þÊ±ÈçºÎ´¦Àí·ûºÅÁ´½Ó£º 

off
Ä¬ÈÏÐÐÎª£¬ÔÊÐíÂ·¾¶ÖÐ³öÏÖ·ûºÅÁ´½Ó£¬²»×ö¼ì²é¡£ 
on
Èç¹ûÎÄ¼þÂ·¾¶ÖÐÈÎºÎ×é³É²¿·ÖÖÐº¬ÓÐ·ûºÅÁ´½Ó£¬¾Ü¾ø·ÃÎÊ¸ÃÎÄ¼þ¡£ 
if_not_owner
Èç¹ûÎÄ¼þÂ·¾¶ÖÐÈÎºÎ×é³É²¿·ÖÖÐº¬ÓÐ·ûºÅÁ´½Ó£¬ÇÒ·ûºÅÁ´½ÓºÍÁ´½ÓÄ¿±êµÄËùÓÐÕß²»Í¬£¬¾Ü¾ø·ÃÎÊ¸ÃÎÄ¼þ¡£ 
from=part
µ±nginx½øÐÐ·ûºÅÁ´½Ó¼ì²éÊ±(²ÎÊýonºÍ²ÎÊýif_not_owner)£¬Â·¾¶ÖÐËùÓÐ²¿·ÖÄ¬ÈÏ¶¼»á±»¼ì²é¡£¶øÊ¹ÓÃfrom=part²ÎÊý¿ÉÒÔ±ÜÃâ¶ÔÂ·¾¶¿ªÊ¼²¿·Ö½øÐÐ·ûºÅÁ´½Ó¼ì²é£¬
¶øÖ»¼ì²éºóÃæµÄ²¿·ÖÂ·¾¶¡£Èç¹ûÄ³Â·¾¶²»ÊÇÒÔÖ¸¶¨Öµ¿ªÊ¼£¬Õû¸öÂ·¾¶½«±»¼ì²é£¬¾ÍÈçÍ¬Ã»ÓÐÖ¸¶¨Õâ¸ö²ÎÊýÒ»Ñù¡£Èç¹ûÄ³Â·¾¶ÓëÖ¸¶¨ÖµÍêÈ«Æ¥Åä£¬½«²»×ö¼ì²é¡£Õâ
¸ö²ÎÊýµÄÖµ¿ÉÒÔ°üº¬±äÁ¿¡£ 

±ÈÈç£º 
disable_symlinks on from=$document_root;
ÕâÌõÖ¸ÁîÖ»ÔÚÓÐopenat()ºÍfstatat()½Ó¿ÚµÄÏµÍ³ÉÏ¿ÉÓÃ¡£µ±È»£¬ÏÖÔÚµÄFreeBSD¡¢LinuxºÍSolaris¶¼Ö§³ÖÕâÐ©½Ó¿Ú¡£ 
²ÎÊýonºÍif_not_owner»á´øÀ´´¦Àí¿ªÏú¡£ 
Ö»ÔÚÄÇÐ©²»Ö§³Ö´ò¿ªÄ¿Â¼²éÕÒÎÄ¼þµÄÏµÍ³ÖÐ£¬Ê¹ÓÃÕâÐ©²ÎÊýÐèÒª¹¤×÷½ø³ÌÓÐÕâÐ©±»¼ì²éÄ¿Â¼µÄ¶ÁÈ¨ÏÞ¡£ 
*/
    { ngx_string("disable_symlinks"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_disable_symlinks,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

#endif

      ngx_null_command
};

/*
ÈÎºÎÒ»¸öHTTPÄ£¿éµÄserverÏà¹ØµÄÅäÖÃÏî¶¼ÊÇ¿ÉÄÜ³öÏÖÔÚmain¼¶±ðÖÐ£¬¶ølocationÏà¹ØµÄÅäÖÃÏî¿ÉÄÜ³öÏÖÔÚmain¡¢srv¼¶±ðÖÐ


main server locationÅäÖÃ¶¨ÒåÈçÏÂ:
1.mainÅäÖÃÏî:Ö»ÄÜÔÚhttp{}ÄÚ serverÍâµÄÅäÖÃ¡£ÀýÈç http{aaa; server{ location{} }}   aaaÎªmainÅäÖÃÏî
2.serverÅäÖÃÏî:¿ÉÒÔÔÚhttpÄÚ£¬serverÍâÅäÖÃ£¬Ò²¿ÉÒÔÔÚserverÄÚÅäÖÃ¡£ ÀýÈç http{bbb; server{bbb; location{} }}   bbbÎªserverÅäÖÃÏî
2.serverÅäÖÃÏî:¿ÉÒÔÔÚhttpÄÚ£¬serverÍâÅäÖÃ£¬Ò²¿ÉÒÔÔÚserverÄÚÅäÖÃ,Ò²¿ÉÒÔ³öÏÖÔÚlocationÖÐ¡£ ÀýÈç http{ccc; server{ccc; location{ccc} }}   cccÎªlocationÅäÖÃÏî 

ÕâÑùÇø·Ömain srv local_createµÄÔ­ÒòÊÇ:
ÀýÈç
http {
    sss;
    xxx; 
    server {
        sss;
        xxx; 
        location {
            xxx;
        } 
    }
},
ÆäÖÐµÄxxx¿ÉÒÔÍ¬Ê±³öÏÖÔÚhttp server ºÍlocationÖÐ£¬ËùÒÔÔÚ½âÎöµ½http{}ÐÐµÄÊ±ºòÐèÒª´´½¨mainÀ´´æ´¢NGX_HTTP_MAIN_CONFÅäÖÃ¡£
ÄÇÃ´ÎªÊ²Ã´»¹ÐèÒªµ÷ÓÃsevºÍloc¶ÔÓ¦µÄcreateÄØ?
ÒòÎªserverÀàÅäÖÃ¿ÉÄÜÍ¬Ê±³öÏÖÔÚmainÖÐ£¬ËùÒÔÐèÒª´æ´¢ÕâÐ´ÅäÖÃ£¬ËùÒÔÒª´´½¨srvÀ´´æ´¢ËûÃÇ£¬¾ÍÊÇÉÏÃæµÄsssÅäÖÃ¡£
Í¬ÀílocationÀàÅäÖÃ¿ÉÄÜÍ¬Ê±³öÏÖÔÚmainÖÐ£¬ËùÒÔÐèÒª´æ´¢ÕâÐ´ÅäÖÃ£¬ËùÒÔÒª´´½¨locÀ´´æ´¢ËûÃÇ£¬¾ÍÊÇÉÏÃæµÄsssÅäÖÃ¡£

ÔÚ½âÎöµ½server{}µÄÊ±ºò£¬ÓÉÓÚlocationÅäÖÃÒ²¿ÉÄÜ³öÏÖÔÚserver{}ÄÚ£¬¾ÍÊÇÉÏÃæserver{}ÖÐµÄxxx;ËùÒÔ½âÎöµ½server{}µÄÊ±ºò
ÐèÒªµ÷¶¯srvºÍloc create;

ËùÒÔ×îÖÕÒª¾ö¶¨Ê¹ÓÃÄÇ¸össsÅäÖÃºÍxxxÅäÖÃ£¬Õâ¾ÍÐèÒª°ÑhttpºÍserverµÄsssºÏ²¢£¬ http¡¢serverºÍlocationÖÐµÄxxxºÏ²¢
*/
static ngx_http_module_t  ngx_http_core_module_ctx = {
    ngx_http_core_preconfiguration,        /* preconfiguration */ //ÔÚ½âÎöhttp{}ÄÚµÄÅäÖÃÏîÇ°»Øµ÷
    ngx_http_core_postconfiguration,       /* postconfiguration */ //½âÎöÍêhttp{}ÄÚµÄËùÓÐÅäÖÃÏîºó»Øµ÷

    ////½âÎöµ½http{}ÐÐÊ±£¬ÔÚngx_http_blockÖ´ÐÐ¡£¸Ãº¯Êý´´½¨µÄ½á¹¹Ìå³ÉÔ±Ö»ÄÜ³öÏÖÔÚhttpÖÐ£¬²»»á³öÏÖÔÚserverºÍlocationÖÐ
    ngx_http_core_create_main_conf,        /* create main configuration */
    //http{}µÄËùÓÐÏî½âÎöÍêºóÖ´ÐÐ
    ngx_http_core_init_main_conf,          /* init main configuration */ //½âÎöÍêmainÅäÖÃÏîºó»Øµ÷

    //½âÎöserver{}   local{}ÏîÊ±£¬»áÖ´ÐÐ
    //´´½¨ÓÃÓÚ´æ´¢¿ÉÍ¬Ê±³öÏÖÔÚmain¡¢srv¼¶±ðÅäÖÃÏîµÄ½á¹¹Ìå£¬¸Ã½á¹¹ÌåÖÐµÄ³ÉÔ±ÓëserverÅäÖÃÊÇÏà¹ØÁªµÄ
    ngx_http_core_create_srv_conf,         /* create server configuration */
    /* merge_srv_conf·½·¨¿ÉÒÔ°Ñ³öÏÖÔÚmain¼¶±ðÖÐµÄÅäÖÃÏîÖµºÏ²¢µ½srv¼¶±ðÅäÖÃÏîÖÐ */
    ngx_http_core_merge_srv_conf,          /* merge server configuration */

    //½âÎöµ½http{}  server{}  local{}ÐÐÊ±£¬»áÖ´ÐÐ
    //´´½¨ÓÃÓÚ´æ´¢¿ÉÍ¬Ê±³öÏÖÔÚmain¡¢srv¡¢loc¼¶±ðÅäÖÃÏîµÄ½á¹¹Ìå£¬¸Ã½á¹¹ÌåÖÐµÄ³ÉÔ±ÓëlocationÅäÖÃÊÇÏà¹ØÁªµÄ
    ngx_http_core_create_loc_conf,         /* create location configuration */
    //°Ñ³öÏÖÔÚmain¡¢srv¼¶±ðµÄÅäÖÃÏîÖµºÏ²¢µ½loc¼¶±ðµÄÅäÖÃÏîÖÐ
    ngx_http_core_merge_loc_conf           /* merge location configuration */
};

//ÔÚ½âÎöµ½http{}ÐÐµÄÊ±ºò£¬»á¸ù¾Ýngx_http_blockÀ´Ö´ÐÐngx_http_core_module_ctxÖÐµÄÏà¹ØcreateÀ´´´½¨´æ´¢ÅäÖÃÏîÄ¿µÄ¿Õ¼ä
ngx_module_t  ngx_http_core_module = { //http{}ÄÚ²¿ ºÍserver location¶¼ÊôÓÚÕâ¸öÄ£¿é£¬ËûÃÇµÄmain_create  srv_create loc_ctreate¶¼ÊÇÒ»ÑùµÄ
//http{}Ïà¹ØÅäÖÃ½á¹¹´´½¨Ê×ÏÈÐèÒªÖ´ÐÐngx_http_core_module£¬¶øºó²ÅÄÜÖ´ÐÐ¶ÔÓ¦µÄhttp×ÓÄ£¿é£¬ÕâÀïÓÐ¸öË³Ðò¹ØÏµÔÚÀïÃæ¡£ÒòÎª
//ngx_http_core_loc_conf_t ngx_http_core_srv_conf_t ngx_http_core_main_conf_tµÄÏà¹Ø
    NGX_MODULE_V1,
    &ngx_http_core_module_ctx,               /* module context */
    ngx_http_core_commands,                  /* module directives */
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


ngx_str_t  ngx_http_core_get_method = { 3, (u_char *) "GET " };

//ngx_http_process_request->ngx_http_handler->ngx_http_core_run_phases
//ngx_http_run_posted_requests->ngx_http_handler
void
ngx_http_handler(ngx_http_request_t *r) /* Ö´ÐÐ11¸ö½×¶ÎµÄÖ¸¶¨½×¶Î */
{
    ngx_http_core_main_conf_t  *cmcf;

    r->connection->log->action = NULL;

    r->connection->unexpected_eof = 0;

/*
    ¼ì²éngx_http_request_t½á¹¹ÌåµÄinternal±êÖ¾Î»£¬Èç¹ûinternalÎª0£¬Ôò´ÓÍ·²¿phase_handlerÖ´ÐÐ£»Èç¹ûinternal±êÖ¾Î»Îª1£¬Ôò±íÊ¾ÇëÇóµ±Ç°ÐèÒª×öÄÚ²¿Ìø×ª£¬
½«Òª°Ñ½á¹¹ÌåÖÐµÄphase_handlerÐòºÅÖÃÎªserver_rewrite_index¡£×¢Òângx_http_phase_engine_t½á¹¹ÌåÖÐµÄhandlers¶¯Ì¬Êý×éÖÐ±£´æÁËÇëÇóÐèÒª¾­ÀúµÄËùÓÐ
»Øµ÷·½·¨£¬¶øserver_rewrite_indexÔòÊÇhandlersÊý×éÖÐNGX_HTTP_SERVER_REWRITE_PHASE´¦Àí½×¶ÎµÄµÚÒ»¸öngx_http_phase_handler_t»Øµ÷·½·¨Ëù´¦µÄÎ»ÖÃ¡£
    ¾¿¾¹handlersÊý×éÊÇÔõÃ´Ê¹ÓÃµÄÄØ£¿ÊÂÊµÉÏ£¬ËüÒªÅäºÏ×Ångx_http_request_t½á¹¹ÌåµÄphase_handlerÐòºÅÊ¹ÓÃ£¬ÓÉphase_handlerÖ¸¶¨×ÅÇëÇó½«ÒªÖ´ÐÐ
µÄhandlersÊý×éÖÐµÄ·½·¨Î»ÖÃ¡£×¢Òâ£¬handlersÊý×éÖÐµÄ·½·¨¶¼ÊÇÓÉ¸÷¸öHTTPÄ£¿éÊµÏÖµÄ£¬Õâ¾ÍÊÇËùÓÐHTTPÄ£¿éÄÜ¹»¹²Í¬´¦ÀíÇëÇóµÄÔ­Òò¡£ 
 */
    if (!r->internal) {
        
        switch (r->headers_in.connection_type) {
        case 0:
            r->keepalive = (r->http_version > NGX_HTTP_VERSION_10); //Ö¸Ã÷ÔÚ1.0ÒÔÉÏ°æ±¾Ä¬ÈÏÊÇ³¤Á¬½Ó
            break;

        case NGX_HTTP_CONNECTION_CLOSE:
            r->keepalive = 0;
            break;

        case NGX_HTTP_CONNECTION_KEEP_ALIVE:
            r->keepalive = 1;
            break;
        }
    
        r->lingering_close = (r->headers_in.content_length_n > 0
                              || r->headers_in.chunked); 
        /*
       µ±internal±êÖ¾Î»Îª0Ê±£¬±íÊ¾²»ÐèÒªÖØ¶¨Ïò£¨Èç¸Õ¿ªÊ¼´¦ÀíÇëÇóÊ±£©£¬½«phase_handlerÐòºÅÖÃÎª0£¬ÒâÎ¶×Å´Óngx_http_phase_engine_tÖ¸¶¨Êý×é
       µÄµÚÒ»¸ö»Øµ÷·½·¨¿ªÊ¼Ö´ÐÐ£¨ÁË½ângx_http_phase_engine_tÊÇÈçºÎ½«¸÷HTTPÄ£¿éµÄ»Øµ÷·½·¨¹¹Ôì³ÉhandlersÊý×éµÄ£©¡£
          */
        r->phase_handler = 0;

    } else {
/* 
ÔÚÕâÒ»²½ÖèÖÐ£¬°Ñphase_handlerÐòºÅÉèÎªserver_rewrite_index£¬ÕâÒâÎ¶×ÅÎÞÂÛÖ®Ç°Ö´ÐÐµ½ÄÄÒ»¸ö½×¶Î£¬ÂíÉÏ¶¼ÒªÖØÐÂ´ÓNGX_HTTP_SERVER_REWRITE_PHASE
½×¶Î¿ªÊ¼ÔÙ´ÎÖ´ÐÐ£¬ÕâÊÇNginxµÄÇëÇó¿ÉÒÔ·´¸´rewriteÖØ¶¨ÏòµÄ»ù´¡¡£
*/
        cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);
        r->phase_handler = cmcf->phase_engine.server_rewrite_index;
    }

    r->valid_location = 1;
#if (NGX_HTTP_GZIP)
    r->gzip_tested = 0;
    r->gzip_ok = 0;
    r->gzip_vary = 0;
#endif

    r->write_event_handler = ngx_http_core_run_phases;
    ngx_http_core_run_phases(r);  
}

/*  
    Ã¿¸öngx_http_phases½×¶Î¶ÔÓ¦µÄcheckerº¯Êý£¬´¦ÓÚÍ¬Ò»¸ö½×¶ÎµÄcheckerº¯ÊýÏàÍ¬£¬¼ûngx_http_init_phase_handlers
    NGX_HTTP_SERVER_REWRITE_PHASE  -------  ngx_http_core_rewrite_phase
    NGX_HTTP_FIND_CONFIG_PHASE     -------  ngx_http_core_find_config_phase
    NGX_HTTP_REWRITE_PHASE         -------  ngx_http_core_rewrite_phase
    NGX_HTTP_POST_REWRITE_PHASE    -------  ngx_http_core_post_rewrite_phase
    NGX_HTTP_ACCESS_PHASE          -------  ngx_http_core_access_phase
    NGX_HTTP_POST_ACCESS_PHASE     -------  ngx_http_core_post_access_phase
    NGX_HTTP_TRY_FILES_PHASE       -------  NGX_HTTP_TRY_FILES_PHASE
    NGX_HTTP_CONTENT_PHASE         -------  ngx_http_core_content_phase
    ÆäËû¼¸¸ö½×¶Î                   -------  ngx_http_core_generic_phase

    HTTP¿ò¼ÜÎª11¸ö½×¶ÎÊµÏÖµÄchecker·½·¨  ¸³Öµ¼ûngx_http_init_phase_handlers
©³©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©×©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©·
©§    ½×¶ÎÃû³Æ                  ©§    checker·½·¨                   ©§
©³©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©×©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©·
©§   NGX_HTTP_POST_READ_PHASE   ©§    ngx_http_core_generic_phase   ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX HTTP SERVER REWRITE PHASE ©§ngx_http_core_rewrite_phase       ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX HTTP FIND CONFIG PHASE    ©§ngx_http_core find config_phase   ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX HTTP REWRITE PHASE        ©§ngx_http_core_rewrite_phase       ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX HTTP POST REWRITE PHASE   ©§ngx_http_core_post_rewrite_phase  ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX HTTP PREACCESS PHASE      ©§ngx_http_core_generic_phase       ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX HTTP ACCESS PHASE         ©§ngx_http_core_access_phase        ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX HTTP POST ACCESS PHASE    ©§ngx_http_core_post_access_phase   ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX HTTP TRY FILES PHASE      ©§ngx_http_core_try_files_phase     ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX HTTP CONTENT PHASE        ©§ngx_http_core_content_phase       ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX HTTP LOG PHASE            ©§ngx_http_core_generic_phase       ©§
©»©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ß©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¿
*/
/*
Í¨³£À´Ëµ£¬ÔÚ½ÓÊÕÍêHTTPÍ·²¿ºó£¬ÊÇÎÞ·¨ÔÚÒ»´ÎNginx¿ò¼ÜµÄµ÷¶ÈÖÐ´¦ÀíÍêÒ»¸öÇëÇóµÄ¡£ÔÚµÚÒ»´Î½ÓÊÕÍêHTTPÍ·²¿ºó£¬HTTP¿ò¼Ü½«µ÷¶È
ngx_http_process_request·½·¨¿ªÊ¼´¦ÀíÇëÇó£¬Èç¹ûÄ³¸öchecker·½·¨·µ»ØÁËNGX_OK£¬Ôò½«»á°Ñ¿ØÖÆÈ¨½»»¹¸øNginx¿ò¼Ü¡£µ±Õâ¸öÇëÇó
ÉÏ¶ÔÓ¦µÄÊÂ¼þÔÙ´Î´¥·¢Ê±£¬HTTP¿ò¼Ü½«²»»áÔÙµ÷¶Èngx_http_process_request·½·¨´¦ÀíÇëÇó£¬¶øÊÇÓÉngx_http_request_handler·½·¨
¿ªÊ¼´¦ÀíÇëÇó¡£ÀýÈçrecvËäÈ»°ÑÍ·²¿ÐÐÄÚÈÝ¶ÁÈ¡Íê±Ï£¬²¢ÄÜ½âÎöÍê³É£¬µ«ÊÇ¿ÉÄÜÓÐÐ¯´øÇëÇóÄÚÈÝ£¬ÄÚÈÝ¿ÉÄÜÃ»ÓÐ¶ÁÍê
*/
//Í¨¹ýÖ´ÐÐµ±Ç°r->phase_handlerËùÖ¸ÏòµÄ½×¶ÎµÄcheckerº¯Êý
//ngx_http_process_request->ngx_http_handler->ngx_http_core_run_phases
void
ngx_http_core_run_phases(ngx_http_request_t *r) //Ö´ÐÐ¸ÃÇëÇó¶ÔÓÚµÄ½×¶ÎµÄchecker(),²¢»ñÈ¡·µ»ØÖµ
{
    ngx_int_t                   rc;
    ngx_http_phase_handler_t   *ph;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    ph = cmcf->phase_engine.handlers;

    while (ph[r->phase_handler].checker) { //´¦ÓÚÍ¬Ò»ngx_http_phases½×¶ÎµÄËùÓÐngx_http_phase_handler_tµÄcheckerÖ¸ÏòÏàÍ¬µÄº¯Êý£¬¼ûngx_http_init_phase_handlers
/*
handler·½·¨ÆäÊµ½öÄÜÔÚchecker·½·¨ÖÐ±»µ÷ÓÃ£¬¶øÇÒchecker·½·¨ÓÉHTTP¿ò¼ÜÊµÏÖ£¬ËùÒÔ¿ÉÒÔ¿ØÖÆ¸÷HTTPÄ£¿éÊµÏÖµÄ´¦Àí·½·¨ÔÚ²»Í¬µÄ½×¶ÎÖÐ²ÉÓÃ²»Í¬µÄµ÷ÓÃÐÐÎª

ngx_http_request_t½á¹¹ÌåÖÐµÄphase_handler³ÉÔ±½«¾ö¶¨Ö´ÐÐµ½ÄÄÒ»½×¶Î£¬ÒÔ¼°ÏÂÒ»½×¶ÎÓ¦µ±Ö´ÐÐÄÄ¸öHTTPÄ£¿éÊµÏÖµÄÄÚÈÝ¡£¿ÉÒÔ¿´µ½ÇëÇóµÄphase_handler³ÉÔ±
»á±»ÖØÖÃ£¬¶øHTTP¿ò¼ÜÊµÏÖµÄcheckerÇî·¨Ò²»áÐÞ¸Äphase_handler³ÉÔ±µÄÖµ

µ±checker·½·¨µÄ·µ»ØÖµ·ÇNGX_OKÊ±£¬ÒâÎ¶×ÅÏòÏÂÖ´ÐÐphase_engineÖÐµÄ¸÷´¦Àí·½·¨£»·´Ö®£¬µ±ÈÎºÎÒ»¸öchecker·½·¨·µ»ØNGX_OKÊ±£¬ÒâÎ¶×Å°Ñ¿ØÖÆÈ¨½»»¹
¸øNginxµÄÊÂ¼þÄ£¿é£¬ÓÉËü¸ù¾ÝÊÂ¼þ£¨ÍøÂçÊÂ¼þ¡¢¶¨Ê±Æ÷ÊÂ¼þ¡¢Òì²½I/OÊÂ¼þµÈ£©ÔÙ´Îµ÷¶ÈÇëÇó¡£È»¶ø£¬Ò»¸öÇëÇó¶à°ëÐèÒªNginxÊÂ¼þÄ£¿é¶à´ÎµØµ÷¶ÈHTTPÄ£
¿é´¦Àí£¬Ò²¾ÍÊÇÔÚ¸Ãº¯ÊýÍâÉèÖÃµÄ¶Á/Ð´ÊÂ¼þµÄ»Øµ÷·½·¨ngx_http_request_handler
*/
        
        rc = ph[r->phase_handler].checker(r, &ph[r->phase_handler]);

 /* Ö±½Ó·µ»ØNGX OK»áÊ¹´ýHTTP¿ò¼ÜÁ¢¿Ì°Ñ¿ØÖÆÈ¨½»»¹¸øepollÊÂ¼þ¿ò¼Ü£¬²»ÔÙ´¦Àíµ±Ç°ÇëÇó£¬Î¨ÓÐÕâ¸öÇëÇóÉÏµÄÊÂ¼þÔÙ´Î±»´¥·¢²Å»á¼ÌÐøÖ´ÐÐ¡£*/
        if (rc == NGX_OK) { //Ö´ÐÐphase_handler½×¶ÎµÄhecker  handler·½·¨ºó£¬·µ»ØÖµÎªNGX_OK£¬ÔòÖ±½ÓÍË³ö£¬·ñÔò¼ÌÐøÑ­»·Ö´ÐÐchecker handler
            return;
        }
    }
}

const char* ngx_http_phase_2str(ngx_uint_t phase)  
{
    static char buf[56];
    
    switch(phase)
    {
        case NGX_HTTP_POST_READ_PHASE:
            return "NGX_HTTP_POST_READ_PHASE";

        case NGX_HTTP_SERVER_REWRITE_PHASE:
            return "NGX_HTTP_SERVER_REWRITE_PHASE"; 

        case NGX_HTTP_FIND_CONFIG_PHASE:
            return "NGX_HTTP_FIND_CONFIG_PHASE";

        case NGX_HTTP_REWRITE_PHASE:
            return "NGX_HTTP_REWRITE_PHASE";

        case NGX_HTTP_POST_REWRITE_PHASE:
            return "NGX_HTTP_POST_REWRITE_PHASE";

        case NGX_HTTP_PREACCESS_PHASE:
            return "NGX_HTTP_PREACCESS_PHASE"; 

        case NGX_HTTP_ACCESS_PHASE:
            return "NGX_HTTP_ACCESS_PHASE";

        case NGX_HTTP_POST_ACCESS_PHASE:
            return "NGX_HTTP_POST_ACCESS_PHASE";

        case NGX_HTTP_TRY_FILES_PHASE:
            return "NGX_HTTP_TRY_FILES_PHASE";

        case NGX_HTTP_CONTENT_PHASE:
            return "NGX_HTTP_CONTENT_PHASE"; 

        case NGX_HTTP_LOG_PHASE:
            return "NGX_HTTP_LOG_PHASE";
    }

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf), "error phase:%u", (unsigned int)phase);
    return buf;
}


/*
//NGX_HTTP_POST_READ_PHASE   NGX_HTTP_PREACCESS_PHASE  NGX_HTTP_LOG_PHASEÄ¬ÈÏ¶¼ÊÇ¸Ãº¯ÊýÅÎ¶ÎÏÂHTTPÄ£¿éµÄngx_http_handler_pt·½·¨·µ»ØÖµÒâÒå
©³©¥©¥©¥©¥©¥©¥©¥©×©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©·
©§    ·µ»ØÖµ    ©§    ÒâÒå                                                                          ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§              ©§  Ö´ÐÐÏÂÒ»¸öngx_http_phases½×¶ÎÖÐµÄµÚÒ»¸öngx_http_handler_pt´¦Àí·½·¨¡£ÕâÒâÎ¶×ÅÁ½  ©§
©§              ©§µã£º¢Ù¼´Ê¹µ±Ç°½×¶ÎÖÐºóÐø»¹ÓÐÒ»ÇúHTTPÄ£¿éÉèÖÃÁËngx_http_handler_pt´¦Àí·½·¨£¬·µ»Ø   ©§
©§NGX_OK        ©§                                                                                  ©§
©§              ©§NGX_OKÖ®ºóËüÃÇÒ²ÊÇµÃ²»µ½Ö´ÐÐ»ú»áµÄ£»¢ÚÈç¹ûÏÂÒ»¸öngx_http_phases½×¶ÎÖÐÃ»ÓÐÈÎºÎ     ©§
©§              ©§HTTPÄ£¿éÉèÖÃÁËngx_http_handler_pt´¦Àí·½·¨£¬½«ÔÙ´ÎÑ°ÕÒÖ®ºóµÄ½×¶Î£¬Èç´ËÑ­»·ÏÂÈ¥     ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX_DECLINED  ©§  °´ÕÕË³ÐòÖ´ÐÐÏÂÒ»¸öngx_http_handler_pt´¦Àí·½·¨¡£Õâ¸öË³Ðò¾ÍÊÇngx_http_phase_      ©§
©§              ©§engine_tÖÐËùÓÐngx_http_phase_handler_t½á¹¹Ìå×é³ÉµÄÊý×éµÄË³Ðò                      ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX_AGAIN     ©§  µ±Ç°µÄngx_http_handler_pt´¦Àí·½·¨ÉÐÎ´½áÊø£¬ÕâÒâÎ¶×Å¸Ã´¦Àí·½·¨ÔÚµ±Ç°½×¶ÎÓÐ»ú»á   ©§
©§              ©§ÔÙ´Î±»µ÷ÓÃ¡£ÕâÊ±Ò»°ã»á°Ñ¿ØÖÆÈ¨½»»¹¸øÊÂ¼þÄ£¿é£¬µ±ÏÂ´Î¿ÉÐ´ÊÂ¼þ·¢ÉúÊ±»áÔÙ´ÎÖ´ÐÐµ½¸Ã  ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©Ï                                                                                  ©§
©§NGX_DONE      ©§ngx_http_handler_pt´¦Àí·½·¨                                                       ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§ NGX_ERROR    ©§                                                                                  ©§
©§              ©§  ÐèÒªµ÷ÓÃngx_http_finalize_request½áÊøÇëÇó                                       ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©Ï                                                                                  ©§
©§ÆäËû          ©§                                                                                  ©§
©»©¥©¥©¥©¥©¥©¥©¥©ß©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¿
*/ 
/* 
ÓÐ3¸öHTTP½×¶Î¶¼Ê¹ÓÃÁËngx_http_core_generic_phase×÷ÎªËüÃÇµÄchecker·½·¨£¬ÕâÒâÎ¶×ÅÈÎºÎÊÔÍ¼ÔÚNGX_HTTP_POST_READ_PHASE   NGX_HTTP_PREACCESS_PHASE  
NGX_HTTP_LOG_PHASEÕâ3¸ö½×¶Î´¦ÀíÇëÇóµÄHTTPÄ£¿é¶¼ÐèÒªÁË½ângx_http_core_generic_phase·½·¨ 
*/ //ËùÓÐ½×¶ÎµÄcheckerÔÚngx_http_core_run_phasesÖÐµ÷ÓÃ
//NGX_HTTP_POST_READ_PHASE   NGX_HTTP_PREACCESS_PHASE  NGX_HTTP_LOG_PHASEÄ¬ÈÏ¶¼ÊÇ¸Ãº¯Êý
//µ±HTTP¿ò¼ÜÔÚ½¨Á¢µÄTCPÁ¬½ÓÉÏ½ÓÊÕµ½¿Í»§·¢ËÍµÄÍêÕûHTTPÇëÇóÍ·²¿Ê±£¬¿ªÊ¼Ö´ÐÐNGX_HTTP_POST_READ_PHASE½×¶ÎµÄchecker·½·¨
ngx_int_t
ngx_http_core_generic_phase(ngx_http_request_t *r, ngx_http_phase_handler_t *ph)
{
    ngx_int_t  rc;

    /*
     * generic phase checker,
     * used by the post read and pre-access phases
     */

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "generic phase: %ui (%s)", r->phase_handler, ngx_http_phase_2str(ph->phase));

    rc = ph->handler(r); //µ÷ÓÃÕâÒ»½×¶ÎÖÐ¸÷HTTPÄ£¿éÌí¼ÓµÄhandler´¦Àí·½·¨

    if (rc == NGX_OK) {//Èç¹ûhandler·½·¨·µ»ØNGX OK£¬Ö®ºó½«½øÈëÏÂÒ»¸ö½×¶Î´¦Àí£¬¶ø²»ÙÝÀí»áµ±Ç°½×¶ÎÖÐÊÇ·ñ»¹ÓÐÆäËûµÄ´¦Àí·½·¨
        r->phase_handler = ph->next; //Ö±½ÓÖ¸ÏòÏÂÒ»¸ö´¦Àí½×¶ÎµÄµÚÒ»¸ö·½·¨
        return NGX_AGAIN;
    }

//Èç¹ûhandler·½·¨·µ»ØNGX_DECLINED£¬ÄÇÃ´½«½øÈëÏÂÒ»¸ö´¦Àí·½·¨£¬Õâ¸ö´¦Àí·½·¨¼È¿ÉÄÜÊôÓÚµ±Ç°½×¶Î£¬Ò²¿ÉÄÜÊôÓÚÏÂÒ»¸ö½×¶Î¡£×¢Òâ·µ»Ø
//NGX_OKÓëNGX_DECLINEDÖ®¼äµÄÇø±ð
    if (rc == NGX_DECLINED) {
        r->phase_handler++;//½ô½Ó×ÅµÄÏÂÒ»¸ö´¦Àí·½·¨
        return NGX_AGAIN;
    }
/*
Èç¹ûhandler·½·¨·µ»ØNGX_AGAIN»òÕßNGX_DONE£¬ÔòÒâÎ¶×Å¸Õ²ÅµÄhandler·½·¨ÎÞ·¨ÔÚÕâÒ»´Îµ÷¶ÈÖÐ´¦ÀíÍêÕâÒ»¸ö½×¶Î£¬ËüÐèÒª¶à´Îµ÷¶È²ÅÄÜÍê³É£¬
Ò²¾ÍÊÇËµ£¬¸Õ¸ÕÖ´ÐÐ¹ýµÄhandler·½·¨Ï£Íû£ºÈç¹ûÇëÇó¶ÔÓ¦µÄÊÂ¼þÔÙ´Î±»´¥·¢Ê±£¬½«ÓÉngx_http_request_handlerÍ¨¹ýngx_http_core_ run_phasesÔÙ´Î
µ÷ÓÃÕâ¸öhandler·½·¨¡£Ö±½Ó·µ»ØNGX_OK»áÊ¹´ýHTTP¿ò¼ÜÁ¢¿Ì°Ñ¿ØÖÆÈ¨½»»¹¸øepollÊÂ¼þ¿ò¼Ü£¬²»ÔÙ´¦Àíµ±Ç°ÇëÇó£¬Î¨ÓÐÕâ¸öÇëÇóÉÏµÄÊÂ¼þÔÙ´Î±»´¥·¢²Å»á¼ÌÐøÖ´ÐÐ¡£
*/
//Èç¹ûhandler·½·¨·µ»ØNGX_AGAIN»òÕßNGX_DONE£¬ÄÇÃ´µ±Ç°ÇëÇó½«ÈÔÈ»Í£ÁôÔÚÕâÒ»¸ö´¦Àí½×¶ÎÖÐ
    if (rc == NGX_AGAIN || rc == NGX_DONE) { //phase_handlerÃ»ÓÐ·¢Éú±ä»¯£¬µ±Õâ¸öÇëÇóÉÏµÄÊÂ¼þÔÙ´Î´¥·¢µÄÊ±ºò¼ÌÐøÔÚ¸Ã½×¶ÎÖ´ÐÐ
        return NGX_OK;
    }

    /* rc == NGX_ERROR || rc == NGX_HTTP_...  */
    //Èç¹ûhandler·½·¨·µ»ØNGX_ERROR»òÕßÀàËÆNGX_HTTP¿ªÍ·µÄ·µ»ØÂë£¬Ôòµ÷ÓÃngx_http_finalize_request½áÊøÇëÇó
    ngx_http_finalize_request(r, rc);

    return NGX_OK;
}

/*
NGX_HTTP_SERVER_REWRITE_PHASE  NGX_HTTP_REWRITE_PHASE½×¶ÎµÄchecker·½·¨ÊÇngx_http_core_rewrite_phase¡£±í10-2×Ü½áÁË¸Ã½×¶Î
ÏÂngx_http_handler_pt´¦Àí·½·¨µÄ·µ»ØÖµÊÇÈçºÎÓ°ÏìHTTP¿ò¼ÜÖ´ÐÐµÄ£¬×¢Òâ£¬Õâ¸ö½×¶ÎÖÐ²»´æÔÚ·µ»ØÖµ¿ÉÒÔÊ¹ÇëÇóÖ±½ÓÌøµ½ÏÂÒ»¸ö½×¶ÎÖ´ÐÐ¡£
NGX_HTTP_REWRITE_PHASE  NGX_HTTP_POST_REWRITE_PHASE½×¶ÎHTTPÄ£¿éµÄngx_http_handler_pt·½·¨·µ»ØÖµÒâÒå
©³©¥©¥©¥©¥©¥©¥©¥©×©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©·
©§    ·µ»ØÖµ    ©§    ÒâÒå                                                                            ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§              ©§  µ±Ç°µÄngx_http_handler_pt´¦Àí·½·¨ÉÐÎ´½áÊø£¬ÕâÒâÎ¶×Å¸Ã´¦Àí·½·¨ÔÚµ±Ç°½×¶ÎÖÐÓÐ»ú»á   ©§
©§NGX DONE      ©§                                                                                    ©§
©§              ©§ÔÙ´Î±»µ÷ÓÃ                                                                          ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§              ©§  µ±Ç°ngx_http_handler_pt´¦Àí·½·¨Ö´ÐÐÍê±Ï£¬°´ÕÕË³ÐòÖ´ÐÐÏÂÒ»¸öngx_http_handler_pt´¦  ©§
©§NGX DECLINED  ©§                                                                                    ©§
©§              ©§Àí·½·¨                                                                              ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX AGAIN     ©§                                                                                    ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©Ï                                                                                    ©§
©§NGX DONE      ©§                                                                                    ©§
©§              ©§  ÐèÒªµ÷ÓÃngx_http_finalize_request½áÊøÇëÇó                                         ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©Ï                                                                                    ©§
©§ NGX ERROR    ©§                                                                                    ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©Ï                                                                                    ©§
©§ÆäËû          ©§                                                                                    ©§
©»©¥©¥©¥©¥©¥©¥©¥©ß©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¿

*/ //ËùÓÐ½×¶ÎµÄcheckerÔÚngx_http_core_run_phasesÖÐµ÷ÓÃ
ngx_int_t
ngx_http_core_rewrite_phase(ngx_http_request_t *r, ngx_http_phase_handler_t *ph) 
{
    ngx_int_t  rc;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "rewrite phase: %ui (%s)", r->phase_handler, ngx_http_phase_2str(ph->phase));

    rc = ph->handler(r);//ngx_http_rewrite_handler

/* ½«phase_handler¼Ó1±íÊ¾½«ÒªÖ´ÐÐÏÂÒ»¸ö»Øµ÷·½·¨¡£×¢Òâ£¬´ËÊ±·µ»ØµÄÊÇNGX AGAIN£¬HTTP¿ò¼Ü²»»á°Ñ½ø³Ì¿ØÖÆÈ¨½»»¹¸øepollÊÂ¼þ¿ò¼Ü£¬¶ø
ÊÇ¼ÌÐøÁ¢¿ÌÖ´ÐÐÇëÇóµÄÏÂÒ»¸ö»Øµ÷·½·¨¡£ */
    if (rc == NGX_DECLINED) {
        r->phase_handler++;
        return NGX_AGAIN;
    }

    /*
     Èç¹ûhandler·½·¨·µ»ØNGX_DONE£¬ÔòÒâÎ¶×Å¸Õ²ÅµÄhandler·½·¨ÎÞ·¨ÔÚÕâÒ»´Îµ÷¶ÈÖÐ´¦ÀíÍêÕâÒ»¸ö½×¶Î£¬ËüÐèÒª¶à´ÎµÄµ÷¶È²ÅÄÜÍê³É¡£×¢Òâ£¬´Ë
     Ê±·µ»ØNGX_OK£¬Ëü»áÊ¹µÃHTTP¿ò¼ÜÁ¢¿Ì°Ñ¿ØÖÆÈ¨½»»¹¸øepollµÈÊÂ¼þÄ£¿é£¬²»ÔÙ´¦Àíµ±Ç°ÇëÇó£¬Î¨ÓÐÕâ¸öÇëÇóÉÏµÄÊÂ¼þÔÙ´Î±»´¥·¢Ê±²Å»á¼ÌÐøÖ´ÐÐ¡£
     */
    if (rc == NGX_DONE) { //phase_handlerÃ»ÓÐ·¢Éú±ä»¯£¬Òò´ËÈç¹û¸ÃÇëÇóµÄÊÂ¼þÔÙ´Î´¥·¢£¬»¹»á½Ó×ÅÉÏ´ÎµÄhandlerÖ´ÐÐ
        return NGX_OK;
    }

    /*
    ÎªÊ²Ã´¸ÃcheckerÖ´ÐÐhandlerÃ»ÓÐNGX_DECLINED(r- >phase_handler  =  ph- >next) ?????
    ´ð:ngx_http_core_rewrite_phase·½·¨Óëngx_http_core_generic_phase·½·¨ÓÐÒ»¸öÏÔÖøµÄ²»Í¬µã£ºÇ°ÕßÓÀÔ¶²»»áµ¼ÖÂ¿ç¹ýÍ¬
Ò»¸öHTTP½×¶ÎµÄÆäËû´¦Àí·½·¨£¬¾ÍÖ±½ÓÌøµ½ÏÂÒ»¸ö½×¶ÎÀ´´¦ÀíÇëÇó¡£Ô­ÒòÆäÊµºÜ¼òµ¥£¬¿ÉÄÜÓÐÐí¶àHTTPÄ£¿éÔÚNGX_HTTP_REWRITE_PHASEºÍ
NGX_HTTP_POST_REWRITE_PHASE½×¶ÎÍ¬Ê±´¦ÀíÖØÐ´URLÕâÑùµÄÒµÎñ£¬HTTP¿ò¼ÜÈÏÎªÕâÁ½¸öÅÎ¶ÎµÄHTTPÄ£¿éÊÇÍêÈ«Æ½µÈµÄ£¬ÐòºÅ¿¿Ç°µÄHTTPÄ£¿éÓÅÏÈ
¼¶²¢²»»á¸ü¸ß£¬Ëü²»ÄÜ¾ö¶¨ÐòºÅ¿¿ºóµÄHTTPÄ£¿éÊÇ·ñ¿ÉÒÔÔÙ´ÎÖØÐ´URL¡£Òò´Ë£¬ngx_http_core_rewrite_phase·½·¨¾ø¶Ô²»»á°Ñphase_handlerÖ±½Ó
ÉèÖÃµ½ÏÂÒ»¸ö½×¶Î´¦Àí·½·¨µÄÁ÷³ÌÖÐ£¬¼´²»¿ÉÄÜ´æÔÚÀàËÆÏÂÃæµÄ´úÂë: r- >phase_handler  =  ph- >next ;
     */
    

    /* NGX_OK, NGX_AGAIN, NGX_ERROR, NGX_HTTP_...  */

    ngx_http_finalize_request(r, rc);

    return NGX_OK;
}


/*
NGXHTTP¡ªFIND¡ªCONFIG¡ªPHASE½×¶ÎÉÏ²»ÄÜ¹ÒÔØÈÎºÎ»Øµ÷º¯Êý£¬ÒòÎªËüÃÇÓÀÔ¶Ò²²»»á±»Ö´ÐÐ£¬¸Ã½×¶ÎÍê³ÉµÄÊÇNginxµÄÌØ¶¨ÈÎÎñ£¬¼´½øÐÐLocation¶¨Î»
*/
//ËùÓÐ½×¶ÎµÄcheckerÔÚngx_http_core_run_phasesÖÐµ÷ÓÃ
ngx_int_t
ngx_http_core_find_config_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph)
{
    u_char                    *p;
    size_t                     len;
    ngx_int_t                  rc;
    ngx_http_core_loc_conf_t  *clcf;
    
    
    r->content_handler = NULL; //Ê×ÏÈ³õÊ¼»¯content_handler£¬Õâ¸ö»áÔÚngx_http_core_content_phaseÀïÃæÊ¹ÓÃ
    r->uri_changed = 0;

    char buf[NGX_STR2BUF_LEN];
    ngx_str_t_2buf(buf, &r->uri);    
    
    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "find config phase: %ui (%s), uri:%s", r->phase_handler, 
        (char*)ngx_http_phase_2str(ph->phase), buf);

    rc = ngx_http_core_find_location(r);//½âÎöÍêHTTP{}¿éºó£¬ngx_http_init_static_location_treesº¯Êý»á´´½¨Ò»¿ÅÈý²æÊ÷£¬ÒÔ¼ÓËÙÅäÖÃ²éÕÒ¡£
	//ÕÒµ½ËùÊôµÄlocation£¬²¢ÇÒloc_confÒ²ÒÑ¾­¸üÐÂÁËr->loc_confÁË¡£

    if (rc == NGX_ERROR) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_OK;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);//ÓÃ¸ÕÕÒµ½µÄloc_conf£¬µÃµ½Æähttp_coreÄ£¿éµÄÎ»ÖÃÅäÖÃ¡£

    /* ¸Ãlocation{}±ØÐëÊÇÄÚ²¿ÖØ¶¨Ïò(indexÖØ¶¨Ïò ¡¢error_pagesµÈÖØ¶¨Ïòµ÷ÓÃngx_http_internal_redirect)ºóÆ¥ÅäµÄlocation{}£¬·ñÔò²»ÈÃ·ÃÎÊ¸Ãlocation */
    if (!r->internal && clcf->internal) { //ÊÇ·ñÊÇiÔÚÄÚ²¿ÖØ¶¨Ïò£¬Èç¹ûÊÇ£¬ÖÐ¶ÏÂð å
        ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
        return NGX_OK;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "using configuration \"%s%V\"",
                   (clcf->noname ? "*" : (clcf->exact_match ? "=" : "")),
                   &clcf->name);

    //Õâ¸öºÜÖØÒª£¬¸üÐÂlocationÅäÖÃ£¬Ö÷ÒªÊÇ r->content_handler = clcf->handler;ÉèÖÃ»Øµ÷´Ó¶øÔÚcontent_phrase½×¶ÎÓÃÕâ¸öhandler¡£
    ngx_http_update_location_config(r);

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http cl:%O max:%O, rc:%d",
                   r->headers_in.content_length_n, clcf->client_max_body_size, rc);

    if (r->headers_in.content_length_n != -1
        && !r->discard_body
        && clcf->client_max_body_size
        && clcf->client_max_body_size < r->headers_in.content_length_n) //³¤¶È³¬³¤ÁË¡£¾Ü¾ø
    {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "client intended to send too large body: %O bytes",
                      r->headers_in.content_length_n);

        r->expect_tested = 1;
        (void) ngx_http_discard_request_body(r);
        ngx_http_finalize_request(r, NGX_HTTP_REQUEST_ENTITY_TOO_LARGE);
        return NGX_OK;
    }

    if (rc == NGX_DONE) {//auto redirect,ÐèÒª½øÐÐÖØ¶¨Ïò¡£ÏÂÃæ¾Í¸ø¿Í»§¶Ë·µ»Ø301£¬´øÉÏÕýÈ·µÄlocationÍ·²¿
        ngx_http_clear_location(r);

        r->headers_out.location = ngx_list_push(&r->headers_out.headers);
        if (r->headers_out.location == NULL) {//Ôö¼ÓÒ»¸ölocationÍ·
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_OK;
        }

        /*
         * we do not need to set the r->headers_out.location->hash and
         * r->headers_out.location->key fields
         */

        if (r->args.len == 0) {//Èç¹û¿Í»§¶ËÇëÇóÃ»ÓÃ´ø²ÎÊý£¬±ÈÈçÇëÇóÊÇ: GET /AAA/BBB/
            r->headers_out.location->value = clcf->name;

        } else {//Èç¹û¿Í»§¶ËÇëÇóÓÐ´ø²ÎÊý£¬±ÈÈçÇëÇóÊÇ: GET /AAA/BBB/?query=word£¬ÔòÐèÒª½«²ÎÊýÒ²´øÔÚlocationºóÃæ
            len = clcf->name.len + 1 + r->args.len;
            p = ngx_pnalloc(r->pool, len);

            if (p == NULL) {
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_OK;
            }

            r->headers_out.location->value.len = len;
            r->headers_out.location->value.data = p;

            p = ngx_cpymem(p, clcf->name.data, clcf->name.len);
            *p++ = '?';
            ngx_memcpy(p, r->args.data, r->args.len); //GET /AAA/BBB/?query=wordÓÉ/AAA/BBB/ºÍquery=word×é³É£¬?Ã»ÓÐ±£´æÔÚ
        }

        ngx_http_finalize_request(r, NGX_HTTP_MOVED_PERMANENTLY);
        return NGX_OK;
    }

    r->phase_handler++;
    return NGX_AGAIN;
}

//ÄÚ²¿ÖØ¶¨ÏòÊÇ´ÓNGX_HTTP_SERVER_REWRITE_PHASE´¦¼ÌÐøÖ´ÐÐ(ngx_http_internal_redirect)£¬¶øÖØÐÂrewriteÊÇ´ÓNGX_HTTP_FIND_CONFIG_PHASE´¦Ö´ÐÐ(ngx_http_core_post_rewrite_phase)
//ËùÓÐ½×¶ÎµÄcheckerÔÚngx_http_core_run_phasesÖÐµ÷ÓÃ
ngx_int_t
ngx_http_core_post_rewrite_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph)
{//ÅÐ¶ÏÒ»ÏÂÊÇ·ñÄÚ²¿ÖØ¶¨Ïò³¬¹ý11´Î¡£Ã»×öÆäËûÊÂÇé¡£
    ngx_http_core_srv_conf_t  *cscf;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post rewrite phase: %ui (%s)", r->phase_handler, ngx_http_phase_2str(ph->phase));

    if (!r->uri_changed) { //²»ÐèÒª´ÓÐÂ rewrite£¬ÔòÖ±½ÓÖ´ÐÐÏÂÒ»¸öpt  
        r->phase_handler++;
        return NGX_AGAIN;
    }

    // ÀýÈçrewrite   ^.*$ www.galaxywind.com last;¾Í»á¶à´ÎÖ´ÐÐrewrite

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "uri changes(cycle rewrite): %d", r->uri_changes);

    /*
     * gcc before 3.3 compiles the broken code for
     *     if (r->uri_changes-- == 0)
     * if the r->uri_changes is defined as
     *     unsigned  uri_changes:4
     */

    r->uri_changes--;//ÖØ¶¨Ïò³¬¹ý10´ÎÁË£¬ÖÐ¶ÏÇëÇó¡£

    if (r->uri_changes == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "rewrite or internal redirection cycle "
                      "while processing \"%V\"", &r->uri);

        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_OK;
    }
    
    //ÄÚ²¿ÖØ¶¨ÏòÊÇ´ÓNGX_HTTP_SERVER_REWRITE_PHASE´¦¼ÌÐøÖ´ÐÐ(ngx_http_internal_redirect)£¬¶øÖØÐÂrewriteÊÇ´ÓNGX_HTTP_FIND_CONFIG_PHASE´¦Ö´ÐÐ(ngx_http_core_post_rewrite_phase)
    r->phase_handler = ph->next; //×¢Òâ:NGX_HTTP_POST_REWRITE_PHASEµÄÏÂÒ»½×¶ÎÊÇNGX_HTTP_FIND_CONFIG_PHASE

    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
    r->loc_conf = cscf->ctx->loc_conf;

    return NGX_AGAIN;
}

/*
  NGX_HTTP_ACCESS_PHASE½×¶ÎÏÂHTTP
    Ä£¿éµÄngx_http_handler_pt·½·¨·µ»ØÖµÒâÒå
©³©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©×©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©·
©§    ·µ»ØÖµ            ©§    ÒâÒå                                                                    ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§                      ©§  Èç¹ûÔÚnginx£®confÖÐÅäÖÃÁËsatisfy all£¬ÄÇÃ´½«°´ÕÕË³ÐòÖ´ÐÐÏÂÒ»¸öngx_        ©§
©§NGX OK                ©§http_handler_pt´¦Àí·½·¨£ºÈç¹ûÔÚnginx.confÖÐÅäÖÃÁËsatisfy any£¬ÄÇÃ´½«Ö´ÐÐ    ©§
©§                      ©§ÏÂÒ»¸öngx_http_phases½×¶ÎÖÐµÄµÚÒ»¸öngx_http_handler_pt´¦Àí·½·¨              ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX DECLINED          ©§  °´ÕÕË³ÐòÖ´ÐÐÏÂÒ»¸öngx_http_handler_pt´¦Àí·½·¨                             ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX AGAIN             ©§  µ±Ç°µÄngx_http_handler_pt´¦Àí·½·¨ÉÐÎ´Ð÷Êø£¬ÕâÒâÎ¶×Å¸Ã´¦Àí·½·¨ÔÚµ±Ç°       ©§
©§                      ©§½×¶ÎÖÐÓÐ»ú»áÔÙ´Î±»µ÷ÓÃ¡£ÕâÊ±»á°Ñ¿ØÖÆÈ¨½»»¹¸øÊÂ¼þÄ£¿é£¬ÏÂ´Î¿ÉÐ´ÊÂ¼þ·¢        ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï                                                                            ©§
©§NGX DONE              ©§ÉúÊ±»áÔÙ´ÎÖ´ÐÐµ½¸Ãngx_http_handler_pt´¦Àí·½·¨                               ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§NGX HTTP FORBIDDEN    ©§  Èç¹ûÔÚnginx.confÖÐÅäÖÃÁËsatisfy any£¬ÄÇÃ´½«ngx_http_request_tÖÐµÄ         ©§
©§                      ©§access code³ÉÔ±ÉèÎª·µ»ØÖµ£¬°´ÕÕË³ÐòÖ´ÐÐÏÂÒ»¸öngx_http_handler_pt´¦Àí·½      ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï                                                                            ©§
©§                      ©§·¨£»Èç¹ûÔÚnginx.confÖÐÅäÖÃÁËsatisfy all£¬ÄÇÃ´µ÷ÓÃngx_http_finalize_request  ©§
©§NGX HTTP UNAUTHORIZED ©§                                                                            ©§
©§                      ©§½áÊøÇëÇó                                                                    ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§ NGX ERROR            ©§                                                                            ©§
©§                      ©§  ÐèÒªµ÷ÓÃngx_http_finalize_request½áÊøÇëÇó                                 ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï                                                                            ©§
©§ÆäËû                  ©§                                                                            ©§
©»©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©ß©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¿
    ´Ó±í10-3ÖÐ¿ÉÒÔ¿´³ö£¬NGX_HTTP_ACCESS_PHASE½×¶ÎÊµ¼ÊÉÏÓënginx.confÅäÖÃÎÄ¼þÖÐµÄsatisfyÅäÖÃÏîÓÐ½ôÃÜµÄÁªÏµ£¬ËùÒÔ£¬ÈÎºÎ½é
ÈëNGX_HTTP_ACCESS_PHASE½×¶ÎµÄHTTPÄ£¿é£¬ÔÚÊµÏÖngx_http_handler_pt·½·¨Ê±¶¼ÐèÒª×¢ÒâsatisfyµÄ²ÎÊý£¬¸Ã²ÎÊý¿ÉÒÔÓÉ
ngx_http_core_loc_conf_tÐ÷¹¹ÌåÖÐµÃµ½¡£
*/ 
/*
ngx_http_core¡ªaccess_phase·½·¨ÊÇ½öÓÃÓÚNGX¡ªHTTP__ ACCESS PHASE½×¶ÎµÄ´¦Àí·½·¨£¬ÕâÒ»½×¶ÎÓÃÓÚ¿ØÖÆÓÃ»§·¢ÆðµÄÇëÇóÊÇ·ñºÏ·¨£¬Èç¼ì²â¿Í
»§¶ËµÄIPµØÖ·ÊÇ·ñÔÊÐí·ÃÎÊ¡£ËüÉæ¼°nginx.confÅäÖÃÎÄ¼þÖÐsatisfyÅäÖÃÏîµÄ²ÎÊýÖµ£¬¼û±í11-2¡£
    ¶ÔÓÚ±í11-2µÄanyÅäÖÃÏî£¬ÊÇÍ¨¹ýngx_http_request_t½á¹¹ÌåÖÐµÄaccess¡ªcode³ÉÔ±À´
´«µÝhandler·½·¨µÄ·µÔ²ÖµµÄ

*/
//ËùÓÐ½×¶ÎµÄcheckerÔÚngx_http_core_run_phasesÖÐµ÷ÓÃ
ngx_int_t
ngx_http_core_access_phase(ngx_http_request_t *r, ngx_http_phase_handler_t *ph)
{
    ngx_int_t                  rc;
    ngx_http_core_loc_conf_t  *clcf;

    /*
    ¼ÈÈ»NGX_HTTP_ACCESS_PHASE½×¶ÎÓÃÓÚ¿ØÖÆ¿Í»§¶ËÊÇ·ñÓÐÈ¨ÏÞ·ÃÎÊ·þÎñ£¬ÄÇÃ´Ëü¾Í²»ÐèÒª¶Ô×ÓÇëÇóÆð×÷ÓÃ¡£ÈçºÎÅÐ¶ÏÇëÇó¾¿¾¹ÊÇÀ´×Ô¿Í
»§¶ËµÄÔ­Ê¼ÇëÇó»¹ÊÇ±»ÅÉÉú³öµÄ×ÓÇëÇóÄØ£¿ºÜ¼òµ¥£¬¼ì²éngx_http_request_t½á¹¹ÌåÖÐµÄmainÖ¸Õë¼´¿É¡£ngx_ http_init_request
·½·¨»á°ÑmainÖ¸ÕëÖ¸ÏòÆä×ÔÉí£¬¶øÓÉÕâ¸öÇëÇóÅÉÉú³öµÄÆäËû×ÓÇëÇóÖÐµÄmainÖ¸Õë£¬ÈÔÈ»»áÖ¸Ïòngx_http_init_request·½·¨³õÊ¼»¯µÄÔ­Ê¼ÇëÇó¡£
Òò´Ë£¬¼ì²émain³ÉÔ±Óëngx_http_request_t×ÔÉíµÄÖ¸ÕëÊÇ·ñÏàµÈ¼´¿É
     */
    if (r != r->main) { //ÊÇ·ñÊÇ×ÓÇëÇó£¬Èç¹ûÊÇ×ÓÇëÇó£¬ËµÃ÷¸¸ÇëÇóÒÑ¾­ÓÐÈ¨ÏÞÁË£¬Òò´Ë×ÓÇëÇóÒ²ÓÐÈ¨ÏÞ£¬Ö±½ÓÌø¹ý¸ÃNGX_HTTP_ACCESS_PHASE½×¶Î
        r->phase_handler = ph->next;
        return NGX_AGAIN;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "access phase: %ui (%s)", r->phase_handler, ngx_http_phase_2str(ph->phase));

    rc = ph->handler(r);

    /*
    ·µ»ØNGX¡ªDECLINEDÒâÎ¶×Åhandler·½·¨Ö´ÐÐÍê±ÏÇÒ¡°ÒâÓÌÎ´¾¡¡±£¬Ï£ÍûÁ¢¿ÌÖ´ÐÐÏÂÒ»¸öhandler·½·¨£¬ÎÞÂÛÆäÊÇ·ñÊôÓÚNGX HTTP_ACCESS_PHASE½×¶Î£¬
    ÔÚÕâÒ»²½ÖÐÖ»ÐèÒª°Ñphase_handler¼Ó1£¬Í¬Ê±ngx_http_core_access_phase·½·¨·µ»ØNGX AGAIN¼´¿É¡£*/
    if (rc == NGX_DECLINED) {
        r->phase_handler++;
        return NGX_AGAIN;
    }

    /*
     ·µ»ØNGX¡£AGAIN»òÕßNGX¡ªDONEÒâÎ¶×Åµ±Ç°µÄNGX_HTTP_ACCESS_PHASE½×¶ÎÃ»ÓÐÒ»´ÎÐÔÖ´ÐÐÍê±Ï£¬ËùÒÔÔÚÕâÒ»²½ÖÐ»áÔÝÊ±½áÊøµ±Ç°ÇëÇóµÄ
     ´¦Àí£¬½«¿ØÖÆÈ¨½»»¹¸øÊÂ¼þÄ£¿é£¬ngx_http_core_access_phase·½·¨½áÊø¡£µ±ÇëÇóÖÐ¶ÔÓ¦µÄÊÂ¼þÔÙ´Î´¥·¢Ê±²Å»á¼ÌÐø´¦Àí¸ÃÇëÇó¡£
     */
    if (rc == NGX_AGAIN || rc == NGX_DONE) {
        return NGX_OK;
    }

    /*
    ÓÉÓÚNGX HTTP ACCESS PHASE½×¶ÎÊÇÔÚNGX HTTP¡ªFIND¡ªCONFIG¡ªPHASE½×¶ÎÖ®ºóµÄ£¬Òò´ËÕâÊ±ÇëÇóÒÑ¾­ÕÒµ½ÁËÆ¥ÅäµÄlocationÅäÖÃ¿é£¬
ÏÈ°Ñlocation¿é¶ÔÓ¦µÄngx_http_core_loc_conf tÅäÖÃ½á¹¹ÌåÈ¡³öÀ´£¬ÒòÎªÕâÀïÓÐÒ»¸öÅäÖÃÏîsatisfyÊÇÏÂÒ»²½ÐèÒªÓÃµ½µÄ¡£
     */
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

/*
Ïà¶ÔÓÚNGX HTTP ACCESS PHASE½×¶Î´¦Àí·½·¨£¬satisfyÅäÖÃÏî²ÎÊýµÄÒâÒå
©³©¥©¥©¥©¥©¥©¥©¥©×©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©·
©§satisfyµÄ²ÎÊý ©§    ÒâÒå                                                                              ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§              ©§    NGX HTTP ACCESS PHASE½×¶Î¿ÉÄÜÓÐºÜ¶àHTTPÄ£¿é¶¼¶Ô¿ØÖÆÇëÇóµÄ·ÃÎÊÈ¨ÏÞ¸ÐÐËÈ¤£¬         ©§
©§              ©§ÄÇÃ´ÒÔÄÄÒ»¸öÎª×¼ÄØ£¿µ±satisfyµÄ²ÎÊýÎªallÊ±£¬ÕâÐ©HTTPÄ£¿é±ØÐëÍ¬Ê±·¢Éú×÷ÓÃ£¬¼´ÒÔ¸Ã½×    ©§
©§all           ©§                                                                                      ©§
©§              ©§¶ÎÖÐÈ«²¿µÄhandler·½·¨¹²Í¬¾ö¶¨ÇëÇóµÄ·ÃÎÊÈ¨ÏÞ£¬»»¾ä»°Ëµ£¬ÕâÒ»½×¶ÎµÄËùÓÐhandler·½·¨±Ø    ©§
©§              ©§ÐëÈ«²¿·µ»ØNGX OK²ÅÄÜÈÏÎªÇëÇó¾ßÓÐ·ÃÎÊÈ¨ÏÞ                                              ©§
©Ç©¥©¥©¥©¥©¥©¥©¥©ï©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©Ï
©§              ©§  ÓëallÏà·´£¬²ÎÊýÎªanyÊ±ÒâÎ¶×ÅÔÚNGX¡ªHTTP__ ACCESS¡ªPHASE½×¶ÎÖ»ÒªÓÐÈÎÒâÒ»¸ö           ©§
©§              ©§HTTPÄ£¿éÈÏÎªÇëÇóºÏ·¨£¬¾Í²»ÓÃÔÙµ÷ÓÃÆäËûHTTPÄ£¿é¼ÌÐø¼ì²éÁË£¬¿ÉÒÔÈÏÎªÇëÇóÊÇ¾ßÓÐ·ÃÎÊ      ©§
©§              ©§È¨ÏÞµÄ¡£Êµ¼ÊÉÏ£¬ÕâÊ±µÄÇé¿öÓÐÐ©¸´ÔÓ£ºÈç¹ûÆäÖÐÈÎºÎÒ»¸öhandler·½·¨·µ»ØNGX¶þOK£¬ÔòÈÏÎª    ©§
©§              ©§ÇëÇó¾ßÓÐ·ÃÎÊÈ¨ÏÞ£»Èç¹ûÄ³Ò»¸öhandler·½·¨·µ»Ø403ÈÖÕß401£¬ÔòÈÏÎªÇëÇóÃ»ÓÐ·ÃÎÊÈ¨ÏÞ£¬»¹     ©§
©§any           ©§                                                                                      ©§
©§              ©§ÐèÒª¼ì²éNGX¡ªHTTP¡ªACCESS¡ªPHASE½×¶ÎµÄÆäËûhandler·½·¨¡£Ò²¾ÍÊÇËµ£¬anyÅäÖÃÏîÏÂÈÎ        ©§
©§              ©§ºÎÒ»¸öhandler·½·¨Ò»µ©ÈÏÎªÇëÇó¾ßÓÐ·ÃÎÊÈ¨ÏÞ£¬¾ÍÈÏÎªÕâÒ»½×¶ÎÖ´ÐÐ³É¹¦£¬¼ÌÐøÏòÏÂÖ´ÐÐ£»Èç   ©§
©§              ©§¹ûÆäÖÐÒ»¸öhandler·½·¨ÈÏÎªÃ»ÓÐ·ÃÎÊÈ¨ÏÞ£¬ÔòÎ´±ØÒÔ´ËÎª×¼£¬»¹ÐèÒª¼ì²âÆäËûµÄhanlder·½·¨¡£  ©§
©§              ©§allºÍanyÓÐµãÏñ¡°&&¡±ºÍ¡°¡§¡±µÄ¹ØÏµ                                                    ©§
©»©¥©¥©¥©¥©¥©¥©¥©ß©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¿
*/
    if (clcf->satisfy == NGX_HTTP_SATISFY_ALL) { //±ØÐëNGX¡ªHTTP¡ªACCESS¡ªPHASE½×¶ÎµÄËùÓÐhandler¶¼·µ»ØNGX_OK²ÅËã¾ßÓÐÈ¨ÏÞ·ÃÎÊ

        if (rc == NGX_OK) {
            r->phase_handler++;
            return NGX_AGAIN;
        }

    } else {
        if (rc == NGX_OK) { //Ö»ÒªÓÐÒ»¸öÄ£¿éµÄhandlerÔÊÐí·ÃÎÊ£¬¸Ã¿Í»§¶Ë¾ÍÓÐÈ¨ÏÞ
            r->access_code = 0;

            if (r->headers_out.www_authenticate) {
                r->headers_out.www_authenticate->hash = 0;
            }

            r->phase_handler = ph->next; //Ö±½ÓÌø¹ý¸Ã½×¶Îµ½ÏÂÒ»½×¶Î
            return NGX_AGAIN;
        }

    /*
      Èç¹û·µ»ØÖµÊÇNGX_HTTP_FORBIDDEN »òÕßNGX_HTTP_UNAUTHORIZED£¬Ôò±íÊ¾Õâ¸öHTTPÄ£¿éµÄhandler·½·¨ÈÏÎªÇëÇóÃ»ÓÐÈ¨ÏÞ·ÃÎÊ·þÎñ£¬µ«
      Ö»ÒªNGX_HTTP_ACCESS_PHASE½×¶ÎµÄÈÎºÎÒ»¸öhandler·½·¨·µ»ØNGX_OK¾ÍÈÏÎªÇëÇóºÏ·¨£¬ËùÒÔºóÐøµÄhandler·½·¨¿ÉÄÜ»á¸ü¸ÄÕâÒ»½á¹û¡£
      ÕâÊ±½«ÇëÇóµÄaccess_code³ÉÔ±ÉèÖÃÎªhandlerÇî·¨µÄ·µ»ØÖµ£¬ÓÃÓÚ´«µÝµ±Ç°HTTPÄ£¿éµÄ´¦Àí½á¹û
      */
        if (rc == NGX_HTTP_FORBIDDEN || rc == NGX_HTTP_UNAUTHORIZED) { //ËäÈ»µ±Ç°Ä£¿éµÄhandlerÈÎÎñÃ»È¨ÏÞ£¬µ¥ºóÃæÆäËûÄ£¿éµÄhandler¿ÉÄÜÔÊÐí¸Ã¿Í»§¶Ë·ÃÎÊ
            if (r->access_code != NGX_HTTP_UNAUTHORIZED) {
                r->access_code = rc;
            }

            r->phase_handler++;
            return NGX_AGAIN;
        }
    }

    /* rc == NGX_ERROR || rc == NGX_HTTP_...  */

    ngx_http_finalize_request(r, rc);
    return NGX_OK;
}

/*
NGX_HTTP_POST_ACCESS_PHASE½×¶ÎÓÖÊÇÒ»¸öÖ»ÄÜÓÉHTTP¿ò¼ÜÊµÏÖµÄ½×¶Î£¬²»ÔÊÐíHTTPÄ£¿éÏò¸Ã½×¶ÎÌí¼Óngx_http_handler_pt´¦Àí·½·¨¡£Õâ¸ö½×¶ÎÍêÈ«ÊÇÎªÖ®Ç°
µÄNGX_HTTP_ACCESS_PHASE½×¶Î·þÎñµÄ£¬»»¾ä»°Ëµ£¬Èç¹ûÃ»ÓÐÈÎºÎHTTPÄ£¿é½éÈëNGX_HTTP_ACCESS_PHASE½×¶Î´¦ÀíÇëÇó£¬NGX_HTTP_POST_ACCESS_PHASE½×¶Î¾Í
²»»á´æÔÚ¡£
    NGX_HTTP_POST_ACCESS_PHASE½×¶ÎµÄchecker·½·¨ÊÇngx_http_core_post_access_phase£¬ËüµÄ¹¤×÷·Ç³£¼òµ¥£¬¾ÍÊÇ¼ì²éngx_http_request_tÇëÇó
ÖÐµÄaccess_code³ÉÔ±£¬µ±Æä²»ÎªOÊ±¾Í½áÊøÇëÇó£¨±íÊ¾Ã»ÓÐ·ÃÎÊÈ¨ÏÞ£©£¬·ñÔò¼ÌÐøÖ´ÐÐÏÂÒ»¸öngx_http_handler_pt´¦Àí·½·¨¡£
*/ //ËùÓÐ½×¶ÎµÄcheckerÔÚngx_http_core_run_phasesÖÐµ÷ÓÃ
ngx_int_t
ngx_http_core_post_access_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph)
{
    ngx_int_t  access_code;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post access phase: %ui (%s)", r->phase_handler, ngx_http_phase_2str(ph->phase));

    access_code = r->access_code;

    if (access_code) {
        if (access_code == NGX_HTTP_FORBIDDEN) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "access forbidden by rule");
        }

        r->access_code = 0;
        ngx_http_finalize_request(r, access_code);
        return NGX_OK;
    }

    r->phase_handler++;
    return NGX_AGAIN;
}

/*
NGX_HTTP_TRY_FILES_PHASE½×¶ÎÒ²ÊÇÒ»¸öÖ»ÄÜÓÉHTTP¿ò¼ÜÊµÏÖµÄ½×¶Î£¬²»ÔÊÐíHTTPÄ£¿éÏò¸Ã½×¶ÎÌí¼Óngx_http_handler_pt´¦Àí·½·¨¡£
    NGX_HTTP_TRY_FILES_HASE½×¶ÎµÄchecker·½·¨ÊÇngx_http_core_try_files_phase£¬ËüÊÇÓënginx.confÖÐµÄtry_filesÅäÖÃÏîÃÜÇÐÏà¹ØµÄ£¬
Èç¹ûtry_filesºóÖ¸¶¨µÄ¾²Ì¬ÎÄ¼þ×ÊÔ´ÖÐÓÐÒ»¸ö¿ÉÒÔ·ÃÎÊ£¬ÕâÊ±¾Í»áÖ±½Ó¶ÁÈ¡ÎÄ¼þ²¢ÓÑËÍÏìÓ¦¸øÓÃ»§£¬²»»áÔÙÏòÏÂÖ´ÐÐºóÐøµÄ½×¶Î£»
Èç¹ûËùÓÐµÄ¾²Ì¬ÎÄ¼þ×ÊÔ´¶¼ÎÞ·¨Ö´ÐÐ£¬½«»á¼ÌÐøÖ´ÐÐngx_http_phase_engine_tÖÐµÄÏÂÒ»¸öngx_http_handler_pt´¦Àí·½·¨¡£
*/ //ËùÓÐ½×¶ÎµÄcheckerÔÚngx_http_core_run_phasesÖÐµ÷ÓÃ

/*
Èç¹ûÅäÖÃÊÇ:try_files index_large.html gmime-gmime-cipher-context.html;£¬ Ôò´òÓ¡ÈçÏÂ:
rying to use file: "index_large.html" "/usr/local/nginx/htmlindex_large.html"   ¿ÉÒÔ¿´³öÂ·¾¶ÓÐÎÊÌâ£¬Ôò»á´Óngx_open_cached_file·µ»Ø¼ÌÐø²éÕÒºóÃæµÄÎÄ¼þ
trying to use file: "gmime-gmime-cipher-context.html" "/usr/local/nginx/htmlgmime-gmime-cipher-context.html"  Õâ¸öÎÄ¼þ»¹ÊÇ²»´æÔÚ
internal redirect: "gmime-gmime-cipher-context.html?" ÄÚ²¿ÖØ¶¨Ïò£¬uriÐÞ¸ÄÎª×îºóÃæµÄÄÇ¸ötry_filesÎÄ¼þ
rewrite phase: 0 (NGX_HTTP_SERVER_REWRITE_PHASE)
find config phase: 1 (NGX_HTTP_FIND_CONFIG_PHASE), uri:gmime-gmime-cipher-context.html


Èç¹ûÅäÖÃÊÇ:try_files /index_large.html gmime-gmime-cipher-context.html;  
rying to use file: "/index_large.html" "/usr/local/nginx/html/index_large.html"  ÓÐÕâ¸öÎÄ¼þ£¬ÔÚngx_open_cached_file»á·µ»Ø³É¹¦
try file uri: "/index_large.html"
content phase: 10 (NGX_HTTP_CONTENT_PHASE) //½øÈëÏÂÒ»phaseÖ´ÐÐ


ËùÒÔtry_files ºóÃæµÄÎÄ¼þÒª¼Ó¡®/¡¯
*/
ngx_int_t //Ö»ÓÐÅäÖÃÁËtry_files aaa bbbºó²Å»áÔÚ cmcf->phase_engine.handlersÌí¼Ó½Úµãpt£¬¼ûngx_http_init_phase_handlers£¬Èç¹ûÃ»ÓÐÅäÖÃ£¬ÔòÖ±½ÓÌø¹ýtry_files½×¶Î
ngx_http_core_try_files_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph) //Èç¹ûtry_filesÖÐµÄÎÄ¼þ´æÔÚ£¬ÔòÐÞ¸ÄuriÎª¸ÃÎÄ¼þÃû
{
    size_t                        len, root, alias, reserve, allocated;
    u_char                       *p, *name;
    ngx_str_t                     path, args;
    ngx_uint_t                    test_dir;
    ngx_http_try_file_t          *tf;
    ngx_open_file_info_t          of;
    ngx_http_script_code_pt       code;
    ngx_http_script_engine_t      e;
    ngx_http_core_loc_conf_t     *clcf;
    ngx_http_script_len_code_pt   lcode;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "try files phase: %ui (%s)", r->phase_handler, ngx_http_phase_2str(ph->phase));

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (clcf->try_files == NULL) {
        r->phase_handler++;
        return NGX_AGAIN;
    }

    allocated = 0;
    root = 0;
    name = NULL;
    /* suppress MSVC warning */
    path.data = NULL;

    tf = clcf->try_files;

    alias = clcf->alias;

    for ( ;; ) {

        if (tf->lengths) { //Èç¹ûtry_filesºóÃæ¸úµÄ²ÎÊý´øÓÐ±äÁ¿µÈ£¬ÔòÐèÒª°Ñ±äÁ¿½âÎöÎª¶ÔÓ¦µÄ×Ö·û´®£¬³õÊ¼¸³Öµ¼ûngx_http_core_try_files
            ngx_memzero(&e, sizeof(ngx_http_script_engine_t));

            e.ip = tf->lengths->elts;
            e.request = r;

            /* 1 is for terminating '\0' as in static names */
            len = 1;

            while (*(uintptr_t *) e.ip) {
                lcode = *(ngx_http_script_len_code_pt *) e.ip;
                len += lcode(&e);
            }

        } else {
            len = tf->name.len;
        }

        if (!alias) {
            reserve = len > r->uri.len ? len - r->uri.len : 0;

        } else if (alias == NGX_MAX_SIZE_T_VALUE) {
            reserve = len;

        } else {
            reserve = len > r->uri.len - alias ? len - (r->uri.len - alias) : 0;
        }

        if (reserve > allocated || !allocated) {

            /* 16 bytes are preallocation */
            allocated = reserve + 16;

            if (ngx_http_map_uri_to_path(r, &path, &root, allocated) == NULL) {
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_OK;
            }

            name = path.data + root;
         }

        if (tf->values == NULL) {

            /* tf->name.len includes the terminating '\0' */

            ngx_memcpy(name, tf->name.data, tf->name.len);

            path.len = (name + tf->name.len - 1) - path.data;

        } else {
            e.ip = tf->values->elts;
            e.pos = name;
            e.flushed = 1;

            while (*(uintptr_t *) e.ip) {
                code = *(ngx_http_script_code_pt *) e.ip;
                code((ngx_http_script_engine_t *) &e);
            }

            path.len = e.pos - path.data;

            *e.pos = '\0';

            if (alias && ngx_strncmp(name, clcf->name.data, alias) == 0) {
                ngx_memmove(name, name + alias, len - alias);
                path.len -= alias;
            }
        }

        test_dir = tf->test_dir;

        tf++;

/*
Èç¹ûÅäÖÃÊÇ:try_files index_large.html gmime-gmime-cipher-context.html;£¬ Ôò´òÓ¡ÈçÏÂ:
rying to use file: "index_large.html" "/usr/local/nginx/htmlindex_large.html"   ¿ÉÒÔ¿´³öÂ·¾¶ÓÐÎÊÌâ£¬Ôò»á´Óngx_open_cached_file·µ»Ø¼ÌÐø²éÕÒºóÃæµÄÎÄ¼þ
trying to use file: "gmime-gmime-cipher-context.html" "/usr/local/nginx/htmlgmime-gmime-cipher-context.html"  Õâ¸öÎÄ¼þ»¹ÊÇ²»´æÔÚ
internal redirect: "gmime-gmime-cipher-context.html?" ÄÚ²¿ÖØ¶¨Ïò£¬uriÐÞ¸ÄÎª×îºóÃæµÄÄÇ¸ötry_filesÎÄ¼þ
rewrite phase: 0 (NGX_HTTP_SERVER_REWRITE_PHASE)
find config phase: 1 (NGX_HTTP_FIND_CONFIG_PHASE), uri:gmime-gmime-cipher-context.html


Èç¹ûÅäÖÃÊÇ:try_files /index_large.html gmime-gmime-cipher-context.html;  
rying to use file: "/index_large.html" "/usr/local/nginx/html/index_large.html"  ÓÐÕâ¸öÎÄ¼þ£¬ÔÚngx_open_cached_file»á·µ»Ø³É¹¦
try file uri: "/index_large.html"
content phase: 10 (NGX_HTTP_CONTENT_PHASE) //½øÈëÏÂÒ»phaseÖ´ÐÐ


ËùÒÔtry_files ºóÃæµÄÎÄ¼þÒª¼Ó/
*/
        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "trying to use %s: \"%s\" \"%s\"",
                       test_dir ? "dir" : "file", name, path.data);

        if (tf->lengths == NULL && tf->name.len == 0) {

            if (tf->code) {
                ngx_http_finalize_request(r, tf->code);
                return NGX_OK;
            }

            path.len -= root;
            path.data += root;

            if (path.data[0] == '@') {
                (void) ngx_http_named_location(r, &path);

            } else {
                ngx_http_split_args(r, &path, &args);

                (void) ngx_http_internal_redirect(r, &path, &args); //Èç¹ûÎÄ¼þ²»´æÔÚ£¬Ôò»áÒÔ×îºóÒ»¸öÎÄ¼þ½øÐÐÄÚ²¿ÖØ¶¨Ïò
            }

            ngx_http_finalize_request(r, NGX_DONE);
            return NGX_OK;
        }

        ngx_memzero(&of, sizeof(ngx_open_file_info_t));

        of.read_ahead = clcf->read_ahead;
        of.directio = clcf->directio;
        of.valid = clcf->open_file_cache_valid;
        of.min_uses = clcf->open_file_cache_min_uses;
        of.test_only = 1;
        of.errors = clcf->open_file_cache_errors;
        of.events = clcf->open_file_cache_events;

        if (ngx_http_set_disable_symlinks(r, clcf, &path, &of) != NGX_OK) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_OK;
        }

        if (ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool)
            != NGX_OK)
        {
            if (of.err != NGX_ENOENT
                && of.err != NGX_ENOTDIR
                && of.err != NGX_ENAMETOOLONG)
            {
                ngx_log_error(NGX_LOG_CRIT, r->connection->log, of.err,
                              "%s \"%s\" failed", of.failed, path.data);
            }

            continue;
        }

        if (of.is_dir != test_dir) {
            continue;
        }

        path.len -= root;
        path.data += root;

        if (!alias) {
            r->uri = path;

        } else if (alias == NGX_MAX_SIZE_T_VALUE) {
            if (!test_dir) {
                r->uri = path;
                r->add_uri_to_alias = 1;
            }

        } else {
            r->uri.len = alias + path.len;
            r->uri.data = ngx_pnalloc(r->pool, r->uri.len);
            if (r->uri.data == NULL) {
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_OK;
            }

            p = ngx_copy(r->uri.data, clcf->name.data, alias);
            ngx_memcpy(p, name, path.len);
        }

        ngx_http_set_exten(r);

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "try file uri: \"%V\"", &r->uri);

        r->phase_handler++;
        return NGX_AGAIN;
    }

    /* not reached */
}

/*
    ÕâÊÇÒ»¸öºËÐÄHTTP½×¶Î£¬¿ÉÒÔËµ´ó²¿·ÖHTTPÄ£¿é¶¼»áÔÚ´Ë½×¶ÎÖØÐÂ¶¨ÒåNginx·þÎñÆ÷µÄÐÐÎª£¬ÈçµÚ3ÕÂÖÐÌáµ½µÄmytestÄ£¿é¡£NGX_HTTP_CONTENT_PHASE
½×¶ÎÖ®ËùÒÔ±»ÖÚ¶àHTTPÄ£¿é¡°ÖÓ°®¡±£¬Ö÷Òª»ùÓÚÒÔÏÂÁ½¸öÔ­Òò£º
    ÆäÒ»£¬ÒÔÉÏ9¸ö½×¶ÎÖ÷Òª×¨×¢ÓÚ4¼þ»ù´¡ÐÔ¹¤×÷£ºrewriteÖØÐ´URL¡¢ÕÒµ½locationÅäÖÃ¿é¡¢ÅÐ¶ÏÇëÇóÊÇ·ñ¾ß±¸·ÃÎÊÈ¨ÏÞ¡¢try_files¹¦ÄÜÓÅÏÈ¶ÁÈ¡¾²Ì¬×ÊÔ´ÎÄ¼þ£¬
Õâ4¸ö¹¤×÷Í¨³£ÊÊÓÃÓÚ¾ø´ó²¿·ÖÇëÇó£¬Òò´Ë£¬Ðí¶àHTTPÄ£¿éÏ£Íû¿ÉÒÔ¹²ÏíÕâ9¸ö½×¶ÎÖÐÒÑ¾­Íê³ÉµÄ¹¦ÄÜ¡£

    Æä¶þ£¬NGX_HTTP_CONTENT_PHASE½×¶ÎÓëÆäËû½×¶Î¶¼²»ÏàÍ¬µÄÊÇ£¬ËüÏòHTTPÄ£¿éÌá¹©ÁËÁ½ÖÖ½éÈë¸Ã½×¶ÎµÄ·½Ê½£ºµÚÒ»ÖÖÓëÆäËû10¸ö½×¶ÎÒ»Ñù£¬
Í¨¹ýÏòÈ«¾ÖµÄngx_http_core_main_conf_t½á¹¹ÌåµÄphasesÊý×éÖÐÌí¼Óngx_http_handler_pt¸°Àí·½·¨À´ÊµÏÖ£¬¶øµÚ¶þÖÖÊÇ±¾½×¶Î¶ÀÓÐµÄ£¬°ÑÏ£Íû´¦ÀíÇëÇóµÄ
ngx_http_handler_pt·½·¨ÉèÖÃµ½locationÏà¹ØµÄngx_http_core_loc_conf_t½á¹¹ÌåµÄhandlerÖ¸ÕëÖÐ£¬ÕâÕýÊÇµÚ3ÕÂÖÐmytestÀý×ÓµÄÓÃ·¨¡£

    ÉÏÃæËùËµµÄµÚÒ»ÖÖ·½Ê½£¬Ò²ÊÇHTTPÄ£¿é½éÈëÆäËû10¸ö½×¶ÎµÄÎ¨Ò»·½Ê½£¬ÊÇÍ¨¹ýÔÚ±Ø¶¨»á±»µ÷ÓÃµÄpostconfiguration·½·¨ÏòÈ«¾ÖµÄ
ngx_http_core_main_conf_t½á¹¹ÌåµÄphases[NGX_HTTP_CONTENT_PHASE]¶¯Ì¬Êý×éÌí¼Óngx_http_handler_pt´¦Àí·½·¨À´´ï³ÉµÄ£¬Õâ¸ö´¦Àí·½·¨½«»áÓ¦ÓÃÓÚÈ«²¿µÄHTTPÇëÇó¡£

    ¶øµÚ¶þÖÖ·½Ê½ÊÇÍ¨¹ýÉèÖÃngx_http_core_loc_conf_t½á¹¹ÌåµÄhandlerÖ¸ÕëÀ´ÊµÏÖµÄ£¬Ã¿Ò»¸ölocation¶¼¶ÔÓ¦×ÅÒ»¸ö¶ÀÁ¢µÄngx_http_core_loc_conf½á
¹¹Ìå¡£ÕâÑù£¬ÎÒÃÇ¾Í²»±ØÔÚ±Ø¶¨»á±»µ÷ÓÃµÄpostconfiguration·½·¨ÖÐÌí¼Óngx_http_handler_pt´¦Àí·½·¨ÁË£¬¶ø¿ÉÒÔÑ¡»ÓÔÚngx_command_tµÄÄ³¸öÅäÖÃÏî
£¨ÈçµÚ3ÕÂÖÐµÄmytestÅäÖÃÏî£©µÄ»Øµ÷·½·¨ÖÐÌí¼Ó´¦Àí·½·¨£¬½«µ±Ç°location¿éËùÊôµÄngx_http_core- loc¡ªconf_t½á¹¹ÌåÖÐµÄhandlerÉèÖÃÎª
ngx_http_handler_pt´¦Àí·½·¨¡£ÕâÑù×öµÄºÃ´¦ÊÇ£¬ngx_http_handler_pt´¦Àí·½·¨²»ÔÙÓ¦ÓÃÓÚËùÓÐµÄHTTPÇëÇó£¬½ö½öµ±ÓÃ»§ÇëÇóµÄURIÆ¥ÅäÁËlocationÊ±
(Ò²¾ÍÊÇmytestÅäÖÃÏîËùÔÚµÄlocation)²Å»á±»µ÷ÓÃ¡£

    ÕâÒ²¾ÍÒâÎ¶×ÅËüÊÇÒ»ÖÖÍêÈ«²»Í¬ÓÚÆäËû½×¶ÎµÄÊ¹ÓÃ·½Ê½¡£ Òò´Ë£¬µ±HTTPÄ£¿éÊµÏÖÁËÄ³¸öngx_http_handler_pt´¦Àí·½·¨²¢Ï£Íû½éÈëNGX_HTTP_CONTENT_PHASE½×
¶ÎÀ´´¦ÀíÓÃ»§ÇëÇóÊ±£¬Èç¹ûÏ£ÍûÕâ¸öngx_http_handler_pt·½·¨Ó¦ÓÃÓÚËùÓÐµÄÓÃ»§ÇëÇó£¬ÔòÓ¦¸ÃÔÚngx_http_module_t½Ó¿ÚµÄpostconfiguration·½·¨ÖÐ£¬
Ïòngx_http_core_main_conf_t½á¹¹ÌåµÄphases[NGX_HTTP_CONTENT_PHASE]¶¯Ì¬Êý×éÖÐÌí¼Óngx_http_handler_pt´¦Àí·½·¨£»·´Ö®£¬Èç¹ûÏ£ÍûÕâ¸ö·½Ê½
½öÓ¦ÓÃÓÚURIÆ¥Åä¶¡Ä³Ð©locationµÄÓÃ»§ÇëÇó£¬ÔòÓ¦¸ÃÔÚÒ»¸ölocationÏÂÅäÖÃÏîµÄ»Øµ÷·½·¨ÖÐ£¬°Ñngx_http_handler_pt·½·¨ÉèÖÃµ½ngx_http_core_loc_conf_t
½á¹¹ÌåµÄhandlerÖÐ¡£
    ×¢Òângx_http_core_loc_conf_t½á¹¹ÌåÖÐ½öÓÐÒ»¸öhandlerÖ¸Õë£¬Ëü²»ÊÇÊý×é£¬ÕâÒ²¾ÍÒâÎ¶×ÅÈç¹û²ÉÓÃÉÏÊöµÄµÚ¶þÖÖ·½·¨Ìí¼Óngx_http_handler_pt´¦Àí·½·¨£¬
ÄÇÃ´Ã¿¸öÇëÇóÔÚNGX_HTTP_CONTENT PHASE½×¶ÎÖ»ÄÜÓÐÒ»¸öngx_http_handler_pt´¦Àí·½·¨¡£¶øÊ¹ÓÃµÚÒ»ÖÖ·½·¨Ê±ÊÇÃ»ÓÐÕâ¸öÏÞÖÆµÄ£¬NGX_HTTP_CONTENT_PHASE½×
¶Î¿ÉÒÔ¾­ÓÉÈÎÒâ¸öHTTPÄ£¿é´¦Àí¡£

    µ±Í¬Ê±Ê¹ÓÃÕâÁ½ÖÖ·½Ê½ÉèÖÃngx_http_handler_pt´¦Àí·½·¨Ê±£¬Ö»ÓÐµÚ¶þÖÖ·½Ê½ÉèÖÃµÄngx_http_handler_pt´¦Àí·½·¨²Å»áÉúÐ§£¬Ò²¾ÍÊÇÉèÖÃ
handlerÖ¸ÕëµÄ·½Ê½ÓÅÏÈ¼¶¸ü¸ß£¬¶øµÚÒ»ÖÖ·½Ê½ÉèÖÃµÄngx_http_handler_pt´¦Àí·½·¨½«²»»áÉúÐ§¡£Èç¹ûÒ»¸ölocationÅäÖÃ¿éÄÚÓÐ¶à¸öHTTPÄ£¿éµÄ
ÅäÖÃÏîÔÚ½âÎö¹ý³Ì¶¼ÊÔÍ¼°´ÕÕµÚ¶þÖÖ·½Ê½ÉèÖÃngx_http_handler_pt¸°Àí·½·¨£¬ÄÇÃ´ºóÃæµÄÅäÖÃÏî½«ÓÐ¿ÉÄÜ¸²¸ÇÇ°ÃæµÄÅäÖÃÏî½âÎöÊ±¶ÔhandlerÖ¸ÕëµÄÉèÖÃ¡£

NGX_HTTP_CONTENT_PHASE½×¶ÎµÄchecker·½·¨ÊÇngx_http_core_content_phase¡£ngx_http_handler_pt´¦Àí·½·¨µÄ·µ»ØÖµÔÚÒÔÉÏÁ½ÖÖ·½Ê½ÏÂ¾ß±¸ÁË²»Í¬ÒâÒå¡£
    ÔÚµÚÒ»ÖÖ·½Ê½ÏÂ£¬ngx_http_handler_pt´¦Àí·½·¨ÎÞÂÛ·µ»ØÈÎºÎÖµ£¬¶¼»áÖ±½Óµ÷ÓÃngx_http_finalize_request·½·¨½áÊøÇëÇó¡£µ±È»£¬
ngx_http_finalize_request·½·¨¸ù¾Ý·µ»ØÖµµÄ²»Í¬Î´±Ø»áÖ±½Ó½áÊøÇëÇó£¬ÕâÔÚµÚ11ÕÂÖÐ»áÏêÏ¸½éÉÜ¡£

    ÔÚµÚ¶þÖÖ·½Ê½ÏÂ£¬Èç¹ûngx_http_handler_pt´¦Àí·½·¨·µ»ØNGX_DECLINED£¬½«°´Ë³ÐòÏòºóÖ´ÐÐÏÂÒ»¸öngx_http_handler_pt´¦Àí·½·¨£»Èç¹û·µ»ØÆäËûÖµ£¬
Ôòµ÷ÓÃngx_http_finalize_request·½·¨½áÊøÇëÇó¡£
*/ //ËùÓÐ½×¶ÎµÄcheckerÔÚngx_http_core_run_phasesÖÐµ÷ÓÃ
ngx_int_t
ngx_http_core_content_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph)
{
    size_t     root;
    ngx_int_t  rc;
    ngx_str_t  path;

    /*
    ¼ì²ângx_http_request_t½á¹¹ÌåµÄcontent_handler³ÉÔ±ÊÇ·ñÎª¿Õ£¬ÆäÊµ¾ÍÊÇ¿´ÔÚNGX_HTTP_FIND_CONFIG_PHASE½×¶ÎÆ¥ÅäÁËURIÇëÇó
    µÄlocationÄÚ£¬ÊÇ·ñÓÐHTTPÄ£¿é°Ñ´¦Àí·½·¨ÉèÖÃµ½ÁËngx_http_core_loc_conf_t½á¹¹ÌåµÄhandler³ÉÔ±ÖÐ
     */
    if (r->content_handler) { //Èç¹ûÔÚclcf->handlerÖÐÉèÖÃÁË·½·¨£¬ÔòÖ±½Ó´ÓÕâÀï½øÈ¥Ö´ÐÐ¸Ã·½·¨£¬È»ºó·µ»Ø£¬¾Í²»»áÖ´ÐÐcontent½×¶ÎµÄÆäËûÈÎºÎ·½·¨ÁË£¬²Î¿¼Àý×Óngx_http_mytest_handler
        //Èç¹ûÓÐcontent_handler,¾ÍÖ±½Óµ÷ÓÃ¾ÍÐÐÁË.±ÈÈçÈç¹ûÊÇFCGI£¬ÔÚÓöµ½ÅäÖÃfastcgi_pass   127.0.0.1:8777;µÄÊ±ºò,»áµ÷ÓÃngx_http_fastcgi_passº¯Êý
        //£¬×¢²á±¾locationµÄ´¦ÀíhanderÎªngx_http_fastcgi_handler¡£ ´Ó¶øÔÚngx_http_update_location_configÀïÃæ»á¸üÐÂcontent_handlerÖ¸ÕëÎªµ±Ç°locËù¶ÔÓ¦µÄÖ¸Õë¡£  
        
        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "content phase(content_handler): %ui (%s)", r->phase_handler, ngx_http_phase_2str(ph->phase));
        r->write_event_handler = ngx_http_request_empty_handler;
        //ÉÏÃæµÄr->content_handler»áÖ¸Ïòngx_http_mytest_handler´¦Àí·½·¨¡£Ò²¾ÍÊÇËµ£¬ÊÂÊµÉÏngx_http_finalize_request¾ö¶¨ÁËngx_http_mytest_handlerÈçºÎÆð×÷ÓÃ¡£
        ngx_http_finalize_request(r, r->content_handler(r));
        return NGX_OK;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "content phase: %ui (%s)", r->phase_handler, ngx_http_phase_2str(ph->phase));

    rc = ph->handler(r);

    /*
     ×¢Òâ:´Óngx_http_core_content_phase·½·¨ÖÐ¿ÉÒÔ¿´µ½£¬ÇëÇóÔÚµÚ10¸ö½×¶ÎNGX_HTTP_CONTENT_PHASEºó£¬²¢Ã»ÓÐÈ¥µ÷ÓÃµÚ11¸ö½×¶ÎNGX_HTTP_LOG_PHASEµÄ´¦Àí
     ·½·¨£¬ÊÂÊµÉÏ£¬¼ÇÂ¼·ÃÎÊÈÕÖ¾ÊÇ±ØÐëÔÚÇëÇó½«Òª½áÊøÊ±²ÅÄÜ½øÐÐµÄ£¬Òò´Ë£¬NGX_HTTP_LOG_PHASE½×¶ÎµÄ»Øµ÷·½·¨ÔÚngx_http_free_request·½·¨ÖÐ²Å»áµ÷ÓÃµ½¡£
     */

    if (rc != NGX_DECLINED) {//¸Ã½×¶ÎµÄÏÂÒ»½×¶Îlog½×¶ÎÔÚÇëÇó½«Òª½áÊøngx_http_free_requestÖÐµ÷ÓÃ£¬Òò´Ë×îºóÒ»¸öcontent·½·¨´¦ÀíÍêºó½áÊøÇëÇó
        ngx_http_finalize_request(r, rc);
        return NGX_OK;
    }

    /* rc == NGX_DECLINED */

    ph++;

    /* ¼ÈÈ»handler·½·¨·µ»ØNGX__ DECLINEDÏ£ÍûÖ´ÐÐÏÂÒ»¸öhandler·½·¨£¬ÄÇÃ´ÕâÒ»²½°ÑÇëÇóµÄphase_handlerÐòºÅ¼Ó1£¬ngx_http_core_content_phase·½·¨
    ·µ»ØNGX_ AGAIN£¬±íÊ¾Ï£ÍûHTTP¿ò¼ÜÁ¢¿ÌÖ´ÐÐÏÂÒ»¸öhandler·½·¨¡£ */
    if (ph->checker) {
        r->phase_handler++;
        return NGX_AGAIN;
    }

    /* no content handler was found */

    if (r->uri.data[r->uri.len - 1] == '/') {

        if (ngx_http_map_uri_to_path(r, &path, &root, 0) != NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "directory index of \"%s\" is forbidden", path.data);
        }

        /* ÒÔNGX_ HTTP FORBIDDEN×÷Îª²ÎÊýµ÷ÓÃngx_http_finalize_request·½·¨£¬±íÊ¾½áÊøÇëÇó²¢·µ»Ø403´íÎóÂë¡£Í¬Ê±£¬
        ngx_http_core_content_phase·½·¨·µ»ØNGX¡ªOK£¬±íÊ¾½»»¹¿ØÖÆÈ¨¸øÊÂ¼þÄ£¿é¡£ */
        ngx_http_finalize_request(r, NGX_HTTP_FORBIDDEN);
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "no handler found");

    /*
    ÒÔNGX HTTP NOT¡ªFOUND×÷Îª²ÎÊýµ÷ÓÃngx_http_finalize_request·½·¨£¬±íÊ¾½áÊøÇëÇó²¢·µ»Ø404´íÎóÂë¡£Í¬Ê±£¬ngx_http_core_content_phase·½
    ·¨·µ»ØNGX_OK£¬±íÊ¾½»»¹¿ØÖÆÈ¨¸øÊÂ¼þÄ£¿é¡£
     */
    ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
    return NGX_OK;
}


//Ö÷ÒªÊÇ°ÑÅäÖÃÖÐµÄÒ»Ð©²ÎÊý¿½±´µ½rÖÐ£¬Í¬Ê±°Ñr->content_handler = clcf->handler;
void
ngx_http_update_location_config(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (r->method & clcf->limit_except) {
        r->loc_conf = clcf->limit_except_loc_conf;
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    }

    if (r == r->main) {
        ngx_set_connection_log(r->connection, clcf->error_log);
    }

    //Èç¹û±àÒëµÄÊ±ºòÊ¹ÄÜÁËsendfile£¬Ôòngx_io = ngx_linux_io 
    if ((ngx_io.flags & NGX_IO_SENDFILE) && clcf->sendfile) {
        r->connection->sendfile = 1;

    } else {
        r->connection->sendfile = 0;
    }

    if (clcf->client_body_in_file_only) { //ÅäÖÃclient_body_in_file_only on | clean
        r->request_body_in_file_only = 1;
        r->request_body_in_persistent_file = 1;
        r->request_body_in_clean_file =
            clcf->client_body_in_file_only == NGX_HTTP_REQUEST_BODY_FILE_CLEAN;
        r->request_body_file_log_level = NGX_LOG_NOTICE;

    } else {//ÅäÖÃclient_body_in_file_only offÄ¬ÈÏÊÇ¸ÃÅäÖÃ
        r->request_body_file_log_level = NGX_LOG_WARN;
    }

    r->request_body_in_single_buf = clcf->client_body_in_single_buffer;

    if (r->keepalive) {
        if (clcf->keepalive_timeout == 0) {
            r->keepalive = 0;

        } else if (r->connection->requests >= clcf->keepalive_requests) {
            r->keepalive = 0;

        } else if (r->headers_in.msie6
                   && r->method == NGX_HTTP_POST
                   && (clcf->keepalive_disable
                       & NGX_HTTP_KEEPALIVE_DISABLE_MSIE6))
        {
            /*
             * MSIE may wait for some time if an response for
             * a POST request was sent over a keepalive connection
             */
            r->keepalive = 0;

        } else if (r->headers_in.safari
                   && (clcf->keepalive_disable
                       & NGX_HTTP_KEEPALIVE_DISABLE_SAFARI))
        {
            /*
             * Safari may send a POST request to a closed keepalive
             * connection and may stall for some time, see
             *     https://bugs.webkit.org/show_bug.cgi?id=5760
             */
            r->keepalive = 0;
        }
    }

    if (!clcf->tcp_nopush) {
        /* disable TCP_NOPUSH/TCP_CORK use */
        r->connection->tcp_nopush = NGX_TCP_NOPUSH_DISABLED;
    }

    if (r->limit_rate == 0) {
        r->limit_rate = clcf->limit_rate;
    }

    if (clcf->handler) {
        r->content_handler = clcf->handler;
    }
}


/*
 * NGX_OK       - exact or regex match
 * NGX_DONE     - auto redirect
 * NGX_AGAIN    - inclusive match
 * NGX_ERROR    - regex error
 * NGX_DECLINED - no match
 */

static ngx_int_t
ngx_http_core_find_location(ngx_http_request_t *r)//Í¼½â²Î¿¼http://blog.chinaunix.net/uid-27767798-id-3759557.html
{
    ngx_int_t                  rc;
    ngx_http_core_loc_conf_t  *pclcf;
#if (NGX_PCRE)
    ngx_int_t                  n;
    ngx_uint_t                 noregex;
    ngx_http_core_loc_conf_t  *clcf, **clcfp;

    noregex = 0;
#endif

    pclcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    //¸ü¼Ór->uriÕÒµ½¶ÔÓ¦µÄlocation{}
    rc = ngx_http_core_find_static_location(r, pclcf->static_locations);//ÕÒµ½¶ÔÓ¦µÄÅäÖÃlocationºó£¬½«loc_confÊý×éÊ×µØÖ·ÉèÖÃµ½r->loc_confÖ¸ÕëÉÏÃæ£¬ÕâÑù¾ÍÇÐ»»ÁËlocationÀ²¡£
    if (rc == NGX_AGAIN) {//ÕâÀï´ú±íµÄÊÇ·Çexact¾«È·Æ¥Åä³É¹¦µÄ¡£¿Ï¶¨µ½Õâ»¹²»ÊÇÕýÔò³É¹¦µÄ¡£

#if (NGX_PCRE)
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        noregex = clcf->noregex;//^~ ¿ªÍ·±íÊ¾uriÒÔÄ³¸ö³£¹æ×Ö·û´®¿ªÍ·£¬Àí½âÎªÆ¥Åä urlÂ·¾¶¼´¿É¡£Îª1±íÊ¾³£¹æ×Ö·û´®¿ªÍ·¡£²»ÊÇÕýÔòÆ¥Åä¡£
#endif

        /* look up nested locations */

        rc = ngx_http_core_find_location(r);//Èç¹ûÊÇ·Ç¾«È·Æ¥Åä³É¹¦µÄ£¬ÏÂÃæ¿´¿´ÓÐÃ»ÓÐÇ¶Ì×µÄ
    }

    //Èç¹ûÊÇÍêÈ«Æ¥Åä£¬»òÕßÊÇÖØ¶¨ÏòÆ¥Åä£¬ÔòÖ±½Ó·µ»Ø£¬²»ÔÚÆ¥ÅäÕýÔò±í´ïÊ½
    if (rc == NGX_OK || rc == NGX_DONE) { //·µ»ØÕâÁ½¸öÖµ±íÊ¾ÕÒµ½¶ÔÓ¦µÄlocation{},²»ÐèÒªÔÙ½øÐÐ²éÕÒÕýÔò±í´ïÊ½
        return rc;//³É¹¦ÕÒµ½ÁËr->loc_confÒÑ¾­ÉèÖÃÎªËù¶ÔÓ¦µÄÄÇ¸ölocationsµÄloc_conf½á¹¹ÁË¡£
    }

    /* rc == NGX_DECLINED or rc == NGX_AGAIN in nested location */

    //Ç°×ºÆ¥ÅäÓÐÆ¥Åäµ½location»òÕßÃ»ÓÐÆ¥Åäµ½location¶¼Òª½øÐÐÕýÔò±í´ïÊ½Æ¥Åä

    /*
        ÀýÈçÓÐÈçÏÂÅäÖÃ:
        location /mytest {		 #1	 Ç°×ºÆ¥Åä
            mytest;		
         } 		

         location ~* /mytest {		 #2	 ÕýÔò±í´ïÊ½Æ¥Åä
            mytest;		
         }  

         Èç¹ûÇëÇóÊÇhttp://10.135.10.167/mytestÔòÆ¥Åä#1,
         Èç¹û°Ñ#1¸ÄÎªlocation /mytes£¬ÔòÆ¥Åä#2
         Èç¹û°Ñ#1¸ÄÎªlocation /£¬ÔòÆ¥Åä#2
   */
#if (NGX_PCRE)

    if (noregex == 0 && pclcf->regex_locations) {//ÓÃÁËÕýÔò±í´ïÊ½£¬¶øÇÒÄØ£¬regex_locationsÕýÔò±í´ïÊ½ÁÐ±íÀïÃæÓÐ»õ£¬ÄÇ¾ÍÆ¥ÅäÖ®¡£

        for (clcfp = pclcf->regex_locations; *clcfp; clcfp++) {//¶ÔÃ¿Ò»¸öÕýÔò±í´ïÊ½£¬Æ¥ÅäÖ®¡£

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "ngx pcre test location: ~ \"%V\"", &(*clcfp)->name);

            n = ngx_http_regex_exec(r, (*clcfp)->regex, &r->uri);//Æ¥Åä³É¹¦£¬¼ÇÂ¼¡£

            if (n == NGX_OK) {
                r->loc_conf = (*clcfp)->loc_conf;

                /* look up nested locations */

                rc = ngx_http_core_find_location(r);
                
                //¿´¿´ÊÇ·ñ»¹ÓÐÇ¶Ì×µÄ¡£×¢Òâ£¬Õâ¸öÔÙ´Î½øÈ¥µÄÊ±ºò£¬ÓÉÓÚr->loc_confÒÑ¾­±»ÉèÖÃÎªÐÂµÄlocationÁË£¬ËùÒÔÆäÊµÕâ¸öËãÊÇµÝ¹é½øÈëÁË¡£
                return (rc == NGX_ERROR) ? rc : NGX_OK;
            }

            if (n == NGX_DECLINED) {
                continue;
            }

            return NGX_ERROR;
        }
    }
#endif

    return rc;
}


/*
 * NGX_OK       - exact match
 * NGX_DONE     - auto redirect
 * NGX_AGAIN    - inclusive match
 * NGX_DECLINED - no match
 */
//ÔÚnodeÊ÷ÖÐ²éÕÒr->uri½Úµã
static ngx_int_t
ngx_http_core_find_static_location(ngx_http_request_t *r,
    ngx_http_location_tree_node_t *node)//Í¼½â²Î¿¼http://blog.chinaunix.net/uid-27767798-id-3759557.html
{
    u_char     *uri;
    size_t      len, n;
    ngx_int_t   rc, rv;

    
    //requestµÄÇëÇóÂ·¾¶³¤¶ÈºÍµØÖ·
    len = r->uri.len;
    uri = r->uri.data;

    rv = NGX_DECLINED; //Ä¬ÈÏ¾«×¼Æ¥ÅäºÍÇ°×ºÆ¥Åä Æ¥Åä²»µ½£¬ÐèÒªÆ¥ÅäºóÃæµÄÕýÔò

    for ( ;; ) {

        if (node == NULL) {
            return rv;
        }

        char buf[NGX_STR2BUF_LEN];
        ngx_str_t_2buf(buf, &r->uri);  
        
        //ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "static_locations test location: \"%*s\"", node->len, node->name);
        //nÊÇuriµÄ³¤¶ÈºÍnode name³¤¶ÈµÄ×îÐ¡Öµ£¬ºÃ±È½ÏËûÃÇµÄ½»¼¯

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "test location: \"%*s\"",
                       (size_t) node->len, node->name);
        n = (len <= (size_t) node->len) ? len : node->len;
        
        rc = ngx_filename_cmp(uri, node->name, n);

        //²»µÃ0±íÊ¾uriºÍnodeµÄname²»ÏàµÈ£¬ÕâÊ±ºòÈý²æÊ÷¾ÍÄÜ¼ÓËÙ²éÕÒµÄÐ§ÂÊ£¬Ñ¡ÔñnodeµÄ×ó½Úµã»òÕßÓÒ½Úµã
        if (rc != 0) {
            node = (rc < 0) ? node->left : node->right;

            continue;
        }
        
        //×ßµ½ÕâÀï±íÊ¾uriºÍnode->nameÖÐËûÃÇÇ°ÃæµÄ½»¼¯×Ö·ûÍêÈ«ÏàÍ¬£¬Ôò±È½ÏÈý²æÊ÷ÖÐµÄtreeÊ÷
        
        //Èç¹û½»¼¯ÏàµÈ£¬Èç¹ûuriµÄ³¤¶È±ÈnodeµÄ³¤¶È»¹Òª³¤
        if (len > (size_t) node->len) {
        
            //Èç¹ûÕâ¸ö½ÚµãÊÇÇ°×ºÆ¥ÅäµÄÄÇÖÖÐèÒªµÝ¹étree½Úµã£¬ÒòÎªtree½ÚµãºóÃæµÄ×Ó½ÚµãÓµÓÐÏàÍ¬µÄÇ°×º¡£
            if (node->inclusive) {
             /*
                ÒòÎªÇ°×ºÒÑ¾­Æ¥Åäµ½ÁË£¬ËùÒÔÕâÀïÏÈÔÝÇÒ°Ñloc_conf×÷Îªtarget£¬µ«ÊÇ²»±£Ö¤ºóÃæµÄtree½ÚµãµÄ×Ó½ÚµãÊÇ·ñÓÐºÍuriÍêÈ«Æ¥Åä
                »òÕß¸ü¶àÇ°×ºÆ¥ÅäµÄ¡£ÀýÈçÈç¹ûuriÊÇ/abc,µ±Ç°node½ÚµãÊÇ/a,ËäÈ»Æ¥Åäµ½ÁËlocation /a,ÏÈ°Ñ/aµÄlocation
                ÅäÖÃ×÷Îªtarget£¬µ«ÊÇÓÐ¿ÉÄÜÔÚ/aµÄtree½ÚµãÓÐ/abcµÄlocation£¬ËùÒÔÐèÒªµÝ¹étree½Úµã¿´Ò»ÏÂ¡£ 
                */
                r->loc_conf = node->inclusive->loc_conf;
                
            /*
            ÖÃ³Éagain±íÊ¾ÐèÒªµÝ¹éÇ¶Ì×location£¬ÎªÊ²Ã´ÒªÇ¶Ì×µÝ¹éÄØ£¬ÒòÎªlocationµÄÇ¶Ì×ÅäÖÃËäÈ»¹Ù·½²»ÍÆ¼ö£¬µ«ÊÇÅäÖÃµÄ»°£¬¸¸×Ó
            locationÐèÒªÓÐÏàÍ¬µÄÇ°×º¡£ËùÒÔÐèÒªµÝ¹éÇ¶Ì×location 
               */
                rv = NGX_AGAIN;

                node = node->tree;//nodeÖØÐÂ±äÎªtree½Úµã
                
                uri += n;
                len -= n;

                continue;
            }

          /*
                ¶ÔÓÚ¾«È·Æ¥ÅäµÄlocation²»»á·ÅÔÚ¹«¹²Ç°×º½ÚµãµÄtree½ÚµãÖÐ£¬»áµ¥À­³öÀ´Ò»¸önodeºÍÇ°×º½ÚµãÆ½ÐÐ¡£Ò²¾ÍÊÇËµ¶ÔÓÚ¾«È·Æ¥
                Åä £½/abcd ºÍÇ°×ºÆ¥ÅäµÄ/abcÁ½¸ölocationÅäÖÃ£¬=/abcd²»»áÊÇ/abc½ÚµãµÄtree½Úµã¡£=/abcd Ö»ÄÜÊÇ£¯abcµÄright½Úµã 
            */
            
            /* exact only */
            node = node->right;

            continue;
        }

        if (len == (size_t) node->len) { //Èç¹ûÊÇuriºÍnodeµÄnameÊÇÍêÈ«ÏàµÈµÄ

            if (node->exact) { //Èç¹ûÊÇ¾«È·Æ¥Åä£¬ÄÇÃ´¾ÍÊÇÖ±½Ó·µ»ØokÁË
                
                r->loc_conf = node->exact->loc_conf;
                return NGX_OK;

            } else { //Èç¹û»¹ÊÇÇ°×ºÄ£Ê½µÄlocation£¬ÄÇÃ´ÐèÒªµÝ¹éÇ¶Ì×locationÁË£¬ÐèÒªÌáÇ°ÉèÖÃloc_conf£¬Èç¹ûÇ¶Ì×ÓÐÆ¥ÅäµÄÔÙ¸²¸Ç
                r->loc_conf = node->inclusive->loc_conf;
                return NGX_AGAIN;
            }
        }

        /* len < node->len */

        if (len + 1 == (size_t) node->len && node->auto_redirect) {

            r->loc_conf = (node->exact) ? node->exact->loc_conf:
                                          node->inclusive->loc_conf;
            rv = NGX_DONE;
        }
        
        /*
        Èç¹ûÇ°×ºÏàµÈ£¬uriµÄ³¤¶È±ÈnodeµÄ³¤¶È»¹ÒªÐ¡£¬±ÈÈçnodeµÄnameÊÇ/abc £¬uriÊÇ/ab,ÕâÖÖÇé¿öÊÇ/abc Ò»¶¨ÊÇ¾«È·Æ¥Åä£¬ÒòÎªÈç¹ûÊÇ
        Ç°×ºÆ¥ÅäÄÇÃ´£¯abc ¿Ï¶¨»áÔÙ£¯abµÄtree Ö¸ÕëÀïÃæ¡£ 
          */
        node = node->left;
    }
}


void *
ngx_http_test_content_type(ngx_http_request_t *r, ngx_hash_t *types_hash)
{
    u_char      c, *lowcase;
    size_t      len;
    ngx_uint_t  i, hash;

    if (types_hash->size == 0) {
        return (void *) 4;
    }

    if (r->headers_out.content_type.len == 0) {
        return NULL;
    }

    len = r->headers_out.content_type_len;

    if (r->headers_out.content_type_lowcase == NULL) {

        lowcase = ngx_pnalloc(r->pool, len);
        if (lowcase == NULL) {
            return NULL;
        }

        r->headers_out.content_type_lowcase = lowcase;

        hash = 0;

        for (i = 0; i < len; i++) {
            c = ngx_tolower(r->headers_out.content_type.data[i]);
            hash = ngx_hash(hash, c);
            lowcase[i] = c;
        }

        r->headers_out.content_type_hash = hash;
    }

    return ngx_hash_find(types_hash, r->headers_out.content_type_hash,
                         r->headers_out.content_type_lowcase, len);
}

/*¿ÉÒÔµ÷ÓÃngx_http_set_content_type(r)·½·¨°ïÖúÎÒÃÇÉèÖÃContent-TypeÍ·²¿£¬Õâ¸ö·½·¨»á¸ù¾ÝURIÖÐµÄÎÄ¼þÀ©Õ¹Ãû²¢¶ÔÓ¦×Åmime.typeÀ´ÉèÖÃContent-TypeÖµ,È¡ÖµÈç:image/jpeg*/
ngx_int_t
ngx_http_set_content_type(ngx_http_request_t *r)
{
    u_char                     c, *exten;
    ngx_str_t                 *type;
    ngx_uint_t                 i, hash;
    ngx_http_core_loc_conf_t  *clcf;

    if (r->headers_out.content_type.len) {
        return NGX_OK;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (r->exten.len) {

        hash = 0;

        for (i = 0; i < r->exten.len; i++) {
            c = r->exten.data[i];

            if (c >= 'A' && c <= 'Z') {

                exten = ngx_pnalloc(r->pool, r->exten.len);
                if (exten == NULL) {
                    return NGX_ERROR;
                }

                hash = ngx_hash_strlow(exten, r->exten.data, r->exten.len);

                r->exten.data = exten;

                break;
            }

            hash = ngx_hash(hash, c);
        }

        type = ngx_hash_find(&clcf->types_hash, hash,
                             r->exten.data, r->exten.len);

        if (type) { //±éÀútypes_hash±í£¬È»ºó°Ñ¶ÔÓ¦µÄvalue´æÔÚÓÚtypes_hashcontent_type
            r->headers_out.content_type_len = type->len;
            r->headers_out.content_type = *type;

            return NGX_OK;
        }
    }

    r->headers_out.content_type_len = clcf->default_type.len;
    r->headers_out.content_type = clcf->default_type;

    return NGX_OK;
}


void
ngx_http_set_exten(ngx_http_request_t *r)
{
    ngx_int_t  i;

    ngx_str_null(&r->exten);

    for (i = r->uri.len - 1; i > 1; i--) {
        if (r->uri.data[i] == '.' && r->uri.data[i - 1] != '/') {

            r->exten.len = r->uri.len - i - 1;
            r->exten.data = &r->uri.data[i + 1];

            return;

        } else if (r->uri.data[i] == '/') {
            return;
        }
    }

    return;
}

/*
 ETagÊÇÒ»¸ö¿ÉÒÔÓëWeb×ÊÔ´¹ØÁªµÄ¼ÇºÅ£¨token£©¡£µäÐÍµÄWeb×ÊÔ´¿ÉÒÔÒ»¸öWebÒ³£¬µ«Ò²¿ÉÄÜÊÇJSON»òXMLÎÄµµ¡£·þÎñÆ÷µ¥¶À¸ºÔðÅÐ¶Ï¼ÇºÅÊÇÊ²Ã´
 ¼°Æäº¬Òå£¬²¢ÔÚHTTPÏìÓ¦Í·ÖÐ½«Æä´«ËÍµ½¿Í»§¶Ë£¬ÒÔÏÂÊÇ·þÎñÆ÷¶Ë·µ»ØµÄ¸ñÊ½£ºETag:"50b1c1d4f775c61:df3"¿Í»§¶ËµÄ²éÑ¯¸üÐÂ¸ñÊ½ÊÇÕâÑù
 µÄ£ºIf-None-Match : W / "50b1c1d4f775c61:df3"Èç¹ûETagÃ»¸Ä±ä£¬Ôò·µ»Ø×´Ì¬304È»ºó²»·µ»Ø£¬ÕâÒ²ºÍLast-ModifiedÒ»Ñù¡£²âÊÔEtagÖ÷Òª
 ÔÚ¶ÏµãÏÂÔØÊ±±È½ÏÓÐÓÃ¡£ "etag:XXX" ETagÖµµÄ±ä¸üËµÃ÷×ÊÔ´×´Ì¬ÒÑ¾­±»ÐÞ¸Ä
 */ //ÉèÖÃetagÍ·²¿ÐÐ £¬Èç¹û¿Í»§¶ËÔÚµÚÒ»´ÎÇëÇóÎÄ¼þºÍµÚ¶þ´ÎÇëÇóÎÄ¼þÕâ¶ÎÊ±¼ä£¬ÎÄ¼þÐÞ¸ÄÁË£¬Ôòetag¾Í±äÁË
ngx_int_t
ngx_http_set_etag(ngx_http_request_t *r) //ngx_http_test_if_matchÑéÖ¤¿Í»§¶Ë¹ýÀ´µÄetag, ngx_http_set_etagÉèÖÃ×îÐÂetag
{ //¼´Ê¹ÊÇÏÂÔØÒ»¸ö³¬´óÎÄ¼þ£¬ÕûÌåÒ²Ö»»áµ÷ÓÃ¸Ã½Ó¿ÚÒ»´Î£¬Õë¶ÔÒ»¸öÎÄ¼þµ÷ÓÃÒ»´Î£¬²»»áËµÒòÎª´óÎÄ¼þÒª¶à´Î·¢ËÍ»¹»áµ÷ÓÃÕâÀï£¬Òò´ËÒ»¸öÎÄ¼þÖ»ÒªÃ»ÐÞ¹ý£¬ÆðetagÊ¼ÖÕÊÇ²»±äµÄ
    ngx_table_elt_t           *etag;
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (!clcf->etag) { //ÆäËûÊÇetag onÅäÖÃ£¬Ä¬ÈÏÊÇ1
        return NGX_OK;
    }

    etag = ngx_list_push(&r->headers_out.headers);
    if (etag == NULL) {
        return NGX_ERROR;
    }

    etag->hash = 1;
    ngx_str_set(&etag->key, "ETag");

    etag->value.data = ngx_pnalloc(r->pool, NGX_OFF_T_LEN + NGX_TIME_T_LEN + 3);
    if (etag->value.data == NULL) {
        etag->hash = 0;
        return NGX_ERROR;
    }

    etag->value.len = ngx_sprintf(etag->value.data, "\"%xT-%xO\"",
                                  r->headers_out.last_modified_time,
                                  r->headers_out.content_length_n)
                      - etag->value.data;

    r->headers_out.etag = etag;

    return NGX_OK;
}


void
ngx_http_weak_etag(ngx_http_request_t *r)
{
    size_t            len;
    u_char           *p;
    ngx_table_elt_t  *etag;

    etag = r->headers_out.etag;

    if (etag == NULL) {
        return;
    }

    if (etag->value.len > 2
        && etag->value.data[0] == 'W'
        && etag->value.data[1] == '/')
    {
        return;
    }

    if (etag->value.len < 1 || etag->value.data[0] != '"') {
        r->headers_out.etag->hash = 0;
        r->headers_out.etag = NULL;
        return;
    }

    p = ngx_pnalloc(r->pool, etag->value.len + 2);
    if (p == NULL) {
        r->headers_out.etag->hash = 0;
        r->headers_out.etag = NULL;
        return;
    }

    len = ngx_sprintf(p, "W/%V", &etag->value) - p;

    etag->value.data = p;
    etag->value.len = len;
}


ngx_int_t
ngx_http_send_response(ngx_http_request_t *r, ngx_uint_t status,   //statusÕæÕý·¢ËÍ¸ø¿Í»§¶ËµÄÍ·²¿ÐÐ×é°üÉúÐ§ÔÚngx_http_status_lines
    ngx_str_t *ct, ngx_http_complex_value_t *cv)
{
    ngx_int_t     rc;
    ngx_str_t     val;
    ngx_buf_t    *b;
    ngx_chain_t   out;

    if (ngx_http_discard_request_body(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    r->headers_out.status = status;

    if (ngx_http_complex_value(r, cv, &val) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (status == NGX_HTTP_MOVED_PERMANENTLY
        || status == NGX_HTTP_MOVED_TEMPORARILY
        || status == NGX_HTTP_SEE_OTHER
        || status == NGX_HTTP_TEMPORARY_REDIRECT)
    {
        ngx_http_clear_location(r);

        r->headers_out.location = ngx_list_push(&r->headers_out.headers);
        if (r->headers_out.location == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        r->headers_out.location->hash = 1;
        ngx_str_set(&r->headers_out.location->key, "Location");
        r->headers_out.location->value = val;

        return status;
    }

    r->headers_out.content_length_n = val.len;

    if (ct) {
        r->headers_out.content_type_len = ct->len;
        r->headers_out.content_type = *ct;

    } else {
        if (ngx_http_set_content_type(r) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    if (r->method == NGX_HTTP_HEAD || (r != r->main && val.len == 0)) {
        return ngx_http_send_header(r);
    }

    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b->pos = val.data;
    b->last = val.data + val.len;
    b->memory = val.len ? 1 : 0;
    b->last_buf = (r == r->main) ? 1 : 0;
    b->last_in_chain = 1;

    out.buf = b;
    out.next = NULL;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}

/*
·¢ËÍHTTPÍ·²¿

ÏÂÃæ¿´Ò»ÏÂHTTP¿ò¼ÜÌá¹©µÄ·¢ËÍHTTPÍ·²¿µÄ·½·¨£¬ÈçÏÂËùÊ¾¡£

ÔÚÏòheadersÁ´±íÖÐÌí¼Ó×Ô¶¨ÒåµÄHTTPÍ·²¿Ê±£¬¿ÉÒÔ²Î¿¼ngx_list_pushµÄÊ¹ÓÃ·½·¨¡£ÕâÀïÓÐÒ»¸ö¼òµ¥µÄÀý×Ó£¬ÈçÏÂËùÊ¾¡£
ngx_table_elt_t* h = ngx_list_push(&r->headers_out.headers);
if (h == NULL) {
 return NGX_ERROR;
}

h->hash = 1;
h->key.len = sizeof("TestHead") - 1;
h->key.data = (u_char *) "TestHead";
h->value.len = sizeof("TestValue") - 1;
h->value.data = (u_char *) "TestValue";

ÕâÑù½«»áÔÚÏìÓ¦ÖÐÐÂÔöÒ»ÐÐHTTPÍ·²¿£º
TestHead: TestValud\r\n

Èç¹û·¢ËÍµÄÊÇÒ»¸ö²»º¬ÓÐHTTP°üÌåµÄÏìÓ¦£¬ÕâÊ±¾Í¿ÉÒÔÖ±½Ó½áÊøÇëÇóÁË£¨ÀýÈç£¬ÔÚngx_http_mytest_handler·½·¨ÖÐ£¬Ö±½ÓÔÚngx_http_send_header·½·¨Ö´ÐÐºó½«Æä·µ»ØÖµreturn¼´¿É£©¡£

×¢Òâ¡¡ngx_http_send_header·½·¨»áÊ×ÏÈµ÷ÓÃËùÓÐµÄHTTP¹ýÂËÄ£¿é¹²Í¬´¦Àíheaders_outÖÐ¶¨ÒåµÄHTTPÏìÓ¦Í·²¿£¬È«²¿´¦ÀíÍê±Ïºó²Å»áÐòÁÐ»¯ÎªTCP×Ö·ûÁ÷·¢ËÍµ½¿Í»§¶Ë£¬Ïà¹ØÁ÷³Ì¿É²Î¼û11.9.1½Ú
*/ 

/*
·¢ËÍ»º´æÎÄ¼þÖÐÄÚÈÝµ½¿Í»§¶Ë¹ý³Ì:
 ngx_http_file_cache_open->ngx_http_file_cache_read->ngx_http_file_cache_aio_readÕâ¸öÁ÷³Ì»ñÈ¡ÎÄ¼þÖÐÇ°ÃæµÄÍ·²¿ÐÅÏ¢Ïà¹ØÄÚÈÝ£¬²¢»ñÈ¡Õû¸ö
 ÎÄ¼þstatÐÅÏ¢£¬ÀýÈçÎÄ¼þ´óÐ¡µÈ¡£
 Í·²¿²¿·ÖÔÚngx_http_cache_send->ngx_http_send_header·¢ËÍ£¬
 »º´æÎÄ¼þºóÃæµÄ°üÌå²¿·ÖÔÚngx_http_cache_sendºó°ë²¿´úÂëÖÐ´¥·¢ÔÚfilterÄ£¿éÖÐ·¢ËÍ

 ½ÓÊÕºó¶ËÊý¾Ý²¢×ª·¢µ½¿Í»§¶Ë´¥·¢Êý¾Ý·¢ËÍ¹ý³Ì:
 ngx_event_pipe_write_to_downstreamÖÐµÄ
 if (p->upstream_eof || p->upstream_error || p->upstream_done) {
    ±éÀúp->in »òÕß±éÀúp->out£¬È»ºóÖ´ÐÐÊä³ö
    p->output_filter(p->output_ctx, p->out);
 }
 */

//µ÷ÓÃngx_http_output_filter·½·¨¼´¿ÉÏò¿Í»§¶Ë·¢ËÍHTTPÏìÓ¦°üÌå£¬ngx_http_send_header·¢ËÍÏìÓ¦ÐÐºÍÏìÓ¦Í·²¿
ngx_int_t
ngx_http_send_header(ngx_http_request_t *r)
{
    if (r->post_action) {
        return NGX_OK;
    }

    ngx_log_debugall(r->connection->log, 0, "ngx http send header");
    if (r->header_sent) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "header already sent");
        return NGX_ERROR;
    }

    if (r->err_status) {
        r->headers_out.status = r->err_status;
        r->headers_out.status_line.len = 0;
    }

    return ngx_http_top_header_filter(r);
}
/*
×¢Òâ¡¡ÔÚÏòÓÃ»§·¢ËÍÏìÓ¦°üÌåÊ±£¬±ØÐëÀÎ¼ÇNginxÊÇÈ«Òì²½µÄ·þÎñÆ÷£¬Ò²¾ÍÊÇËµ£¬²»¿ÉÒÔÔÚ½ø³ÌµÄÕ»Àï·ÖÅäÄÚ´æ²¢½«Æä×÷Îª°üÌå·¢ËÍ¡£µ±ngx_http_output_filter·½·¨·µ»ØÊ±£¬
¿ÉÄÜÓÉÓÚTCPÁ¬½ÓÉÏµÄ»º³åÇø»¹²»¿ÉÐ´£¬ËùÒÔµ¼ÖÂngx_buf_t»º³åÇøÖ¸ÏòµÄÄÚ´æ»¹Ã»ÓÐ·¢ËÍ£¬¿ÉÕâÊ±·½·¨·µ»ØÒÑ°Ñ¿ØÖÆÈ¨½»¸øNginxÁË£¬ÓÖ»áµ¼ÖÂÕ»ÀïµÄÄÚ´æ±»ÊÍ·Å£¬×îºó¾Í»á
Ôì³ÉÄÚ´æÔ½½ç´íÎó¡£Òò´Ë£¬ÔÚ·¢ËÍÏìÓ¦°üÌåÊ±£¬¾¡Á¿½«ngx_buf_tÖÐµÄposÖ¸ÕëÖ¸Ïò´ÓÄÚ´æ³ØÀï·ÖÅäµÄÄÚ´æ¡£
*/
//rÊÇrequestÇëÇó£¬inÊÇÊäÈëµÄchain
//µ÷ÓÃngx_http_output_filter·½·¨¼´¿ÉÏò¿Í»§¶Ë·¢ËÍHTTPÏìÓ¦°üÌå£¬ngx_http_send_header·¢ËÍÏìÓ¦ÐÐºÍÏìÓ¦Í·²¿

/*
Êµ¼ÊÉÏ£¬Nginx»¹·â×°ÁËÒ»¸öÉú³Éngx_buf_tµÄ¼ò±ã·½·¨£¬ËüÍêÈ«µÈ¼ÛÓÚÉÏÃæµÄ6ÐÐÓï¾ä£¬ÈçÏÂËùÊ¾¡£
ngx_buf_t *b = ngx_create_temp_buf(r->pool, 128);
·ÖÅäÍêÄÚ´æºó£¬¿ÉÒÔÏòÕâ¶ÎÄÚ´æÐ´ÈëÊý¾Ý¡£µ±Ð´ÍêÊý¾Ýºó£¬ÒªÈÃb->lastÖ¸ÕëÖ¸ÏòÊý¾ÝµÄÄ©Î²£¬Èç¹ûb->lastÓëb->posÏàµÈ£¬ÄÇÃ´HTTP¿ò¼ÜÊÇ²»»á·¢ËÍÒ»¸ö×Ö½ÚµÄ°üÌåµÄ¡£

×îºó£¬°ÑÉÏÃæµÄngx_buf_t *bÓÃngx_chain_t´«¸øngx_http_output_filter·½·¨¾Í¿ÉÒÔ·¢ËÍHTTPÏìÓ¦µÄ°üÌåÄÚÈÝÁË¡£ÀýÈç£º
ngx_chain_t out;
out.buf = b;
out.next = NULL;
return ngx_http_output_filter(r, &out);
*/ 

/*
NginxÊÇÒ»¸öÈ«Òì²½µÄÊÂ¼þÇý¶¯¼Ü¹¹£¬ÄÇÃ´½ö½öµ÷ÓÃngx_http_send_header·½·¨ºÍngx_http_output_filter·½·¨£¬¾Í¿ÉÒÔ°ÑÏìÓ¦È«²¿·¢ËÍ¸ø¿Í»§¶ËÂð£¿µ±
È»²»ÊÇ£¬µ±ÏìÓ¦¹ý´óÎÞ·¨Ò»´Î·¢ËÍÍêÊ±£¨TCPµÄ»¬¶¯´°¿ÚÒ²ÊÇÓÐÏÞµÄ£¬Ò»´Î·Ç×èÈûµÄ·¢ËÍ¶à°ëÊÇÎÞ·¨·¢ËÍÍêÕûµÄHTTPÏìÓ¦µÄ£©£¬¾ÍÐèÒªÏòepollÒÔ¼°¶¨Ê±
Æ÷ÖÐÌí¼ÓÐ´ÊÂ¼þÁË£¬µ±Á¬½ÓÔÙ´Î¿ÉÐ´Ê±£¬¾Íµ÷ÓÃngx_http_writer·½·¨¼ÌÐø·¢ËÍÏìÓ¦£¬Ö±µ½È«²¿µÄÏìÓ¦¶¼·¢ËÍµ½¿Í»§¶ËÎªÖ¹¡£
*/

/* ×¢Òâ:µ½ÕâÀïµÄinÊµ¼ÊÉÏÊÇÒÑ¾­Ö¸ÏòÊý¾ÝÄÚÈÝ²¿·Ö£¬»òÕßÈç¹û·¢ËÍµÄÊý¾ÝÐèÒª´ÓÎÄ¼þÖÐ¶ÁÈ¡£¬inÖÐÒ²»áÖ¸¶¨ÎÄ¼þfile_posºÍfile_lastÒÑ¾­ÎÄ¼þfdµÈ,
   ¿ÉÒÔ²Î¿¼ngx_http_cache_send ngx_http_send_header ngx_http_output_filter */

//µ÷ÓÃngx_http_output_filter·½·¨¼´¿ÉÏò¿Í»§¶Ë·¢ËÍHTTPÏìÓ¦°üÌå£¬ngx_http_send_header·¢ËÍÏìÓ¦ÐÐºÍÏìÓ¦Í·²¿
ngx_int_t
ngx_http_output_filter(ngx_http_request_t *r, ngx_chain_t *in)
{//Èç¹ûÄÚÈÝ±»±£´æµ½ÁËÁÙÊ±ÎÄ¼þÖÐ£¬Ôò»áÔÚngx_http_copy_filter->ngx_output_chain->ngx_output_chain_copy_buf->ngx_read_fileÖÐ¶ÁÈ¡ÎÄ¼þÄÚÈÝ£¬È»ºó·¢ËÍ
    ngx_int_t          rc;
    ngx_connection_t  *c;

    c = r->connection;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http output filter \"%V?%V\"", &r->uri, &r->args);

    
    rc = ngx_http_top_body_filter(r, in); //filterÉÏÃæµÄ×îºóÒ»¸ö¹³×ÓÊÇngx_http_write_filter

    if (rc == NGX_ERROR) {
        /* NGX_ERROR may be returned by any filter */
        c->error = 1;
    }

    return rc;
}

//°Ñuri¿½±´µ½¸ùÄ¿Â¼rootÅäÖÃ»òÕßaliasºóÃæ
u_char *
ngx_http_map_uri_to_path(ngx_http_request_t *r, ngx_str_t *path,
    size_t *root_length, size_t reserved)
{
    u_char                    *last;
    size_t                     alias;
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    alias = clcf->alias;

    if (alias && !r->valid_location) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "\"alias\" cannot be used in location \"%V\" "
                      "where URI was rewritten", &clcf->name);
        return NULL;
    }

    if (clcf->root_lengths == NULL) {

        *root_length = clcf->root.len;//ÉèÖÃºÃroot Ä¿Â¼µÄ³¤¶È

        path->len = clcf->root.len + reserved + r->uri.len - alias + 1;

        path->data = ngx_pnalloc(r->pool, path->len);
        if (path->data == NULL) {
            return NULL;
        }

        last = ngx_copy(path->data, clcf->root.data, clcf->root.len); //¿½±´Ç°ÃæµÄÂ·¾¶¸ù²¿·Ö

    } else {

        if (alias == NGX_MAX_SIZE_T_VALUE) {
            reserved += r->add_uri_to_alias ? r->uri.len + 1 : 1;

        } else {
            reserved += r->uri.len - alias + 1;
        }
        
        //±àÒëÒ»ÏÂÕâÐ©±äÁ¿£¬¼ÆËãÆäÖµ¡£
        if (ngx_http_script_run(r, path, clcf->root_lengths->elts, reserved,
                                clcf->root_values->elts)
            == NULL)
        {
            return NULL;
        }

        if (ngx_get_full_name(r->pool, (ngx_str_t *) &ngx_cycle->prefix, path) //½«name²ÎÊý×ªÎª¾ø¶ÔÂ·¾¶¡£
            != NGX_OK)
        {
            return NULL;
        }

        *root_length = path->len - reserved;
        last = path->data + *root_length;

        if (alias == NGX_MAX_SIZE_T_VALUE) {
            if (!r->add_uri_to_alias) {
                *last = '\0';
                return last;
            }

            alias = 0;
        }
    }

    last = ngx_cpystrn(last, r->uri.data + alias, r->uri.len - alias + 1);

    return last;
}


ngx_int_t
ngx_http_auth_basic_user(ngx_http_request_t *r)
{
    ngx_str_t   auth, encoded;
    ngx_uint_t  len;

    if (r->headers_in.user.len == 0 && r->headers_in.user.data != NULL) {
        return NGX_DECLINED;
    }

    if (r->headers_in.authorization == NULL) {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    encoded = r->headers_in.authorization->value;

    if (encoded.len < sizeof("Basic ") - 1
        || ngx_strncasecmp(encoded.data, (u_char *) "Basic ",
                           sizeof("Basic ") - 1)
           != 0)
    {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    encoded.len -= sizeof("Basic ") - 1;
    encoded.data += sizeof("Basic ") - 1;

    while (encoded.len && encoded.data[0] == ' ') {
        encoded.len--;
        encoded.data++;
    }

    if (encoded.len == 0) {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    auth.len = ngx_base64_decoded_length(encoded.len);
    auth.data = ngx_pnalloc(r->pool, auth.len + 1);
    if (auth.data == NULL) {
        return NGX_ERROR;
    }

    if (ngx_decode_base64(&auth, &encoded) != NGX_OK) {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    auth.data[auth.len] = '\0';

    for (len = 0; len < auth.len; len++) {
        if (auth.data[len] == ':') {
            break;
        }
    }

    if (len == 0 || len == auth.len) {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    r->headers_in.user.len = len;
    r->headers_in.user.data = auth.data;
    r->headers_in.passwd.len = auth.len - len - 1;
    r->headers_in.passwd.data = &auth.data[len + 1];

    return NGX_OK;
}


#if (NGX_HTTP_GZIP)

ngx_int_t
ngx_http_gzip_ok(ngx_http_request_t *r)
{
    time_t                     date, expires;
    ngx_uint_t                 p;
    ngx_array_t               *cc;
    ngx_table_elt_t           *e, *d, *ae;
    ngx_http_core_loc_conf_t  *clcf;

    r->gzip_tested = 1;

    if (r != r->main) {
        return NGX_DECLINED;
    }

    ae = r->headers_in.accept_encoding;
    if (ae == NULL) {
        return NGX_DECLINED;
    }

    if (ae->value.len < sizeof("gzip") - 1) {
        return NGX_DECLINED;
    }

    /*
     * test first for the most common case "gzip,...":
     *   MSIE:    "gzip, deflate"
     *   Firefox: "gzip,deflate"
     *   Chrome:  "gzip,deflate,sdch"
     *   Safari:  "gzip, deflate"
     *   Opera:   "gzip, deflate"
     */

    if (ngx_memcmp(ae->value.data, "gzip,", 5) != 0
        && ngx_http_gzip_accept_encoding(&ae->value) != NGX_OK)
    {
        return NGX_DECLINED;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (r->headers_in.msie6 && clcf->gzip_disable_msie6) {
        return NGX_DECLINED;
    }

    if (r->http_version < clcf->gzip_http_version) {
        return NGX_DECLINED;
    }

    if (r->headers_in.via == NULL) {
        goto ok;
    }

    p = clcf->gzip_proxied;

    if (p & NGX_HTTP_GZIP_PROXIED_OFF) {
        return NGX_DECLINED;
    }

    if (p & NGX_HTTP_GZIP_PROXIED_ANY) {
        goto ok;
    }

    if (r->headers_in.authorization && (p & NGX_HTTP_GZIP_PROXIED_AUTH)) {
        goto ok;
    }

    e = r->headers_out.expires;

    if (e) {

        if (!(p & NGX_HTTP_GZIP_PROXIED_EXPIRED)) {
            return NGX_DECLINED;
        }

        expires = ngx_parse_http_time(e->value.data, e->value.len);
        if (expires == NGX_ERROR) {
            return NGX_DECLINED;
        }

        d = r->headers_out.date;

        if (d) {
            date = ngx_parse_http_time(d->value.data, d->value.len);
            if (date == NGX_ERROR) {
                return NGX_DECLINED;
            }

        } else {
            date = ngx_time();
        }

        if (expires < date) {
            goto ok;
        }

        return NGX_DECLINED;
    }

    cc = &r->headers_out.cache_control;

    if (cc->elts) {

        if ((p & NGX_HTTP_GZIP_PROXIED_NO_CACHE)
            && ngx_http_parse_multi_header_lines(cc, &ngx_http_gzip_no_cache,
                                                 NULL)
               >= 0)
        {
            goto ok;
        }

        if ((p & NGX_HTTP_GZIP_PROXIED_NO_STORE)
            && ngx_http_parse_multi_header_lines(cc, &ngx_http_gzip_no_store,
                                                 NULL)
               >= 0)
        {
            goto ok;
        }

        if ((p & NGX_HTTP_GZIP_PROXIED_PRIVATE)
            && ngx_http_parse_multi_header_lines(cc, &ngx_http_gzip_private,
                                                 NULL)
               >= 0)
        {
            goto ok;
        }

        return NGX_DECLINED;
    }

    if ((p & NGX_HTTP_GZIP_PROXIED_NO_LM) && r->headers_out.last_modified) {
        return NGX_DECLINED;
    }

    if ((p & NGX_HTTP_GZIP_PROXIED_NO_ETAG) && r->headers_out.etag) {
        return NGX_DECLINED;
    }

ok:

#if (NGX_PCRE)

    if (clcf->gzip_disable && r->headers_in.user_agent) {

        if (ngx_regex_exec_array(clcf->gzip_disable,
                                 &r->headers_in.user_agent->value,
                                 r->connection->log)
            != NGX_DECLINED)
        {
            return NGX_DECLINED;
        }
    }

#endif

    r->gzip_ok = 1;

    return NGX_OK;
}


/*
 * gzip is enabled for the following quantities:
 *     "gzip; q=0.001" ... "gzip; q=1.000"
 * gzip is disabled for the following quantities:
 *     "gzip; q=0" ... "gzip; q=0.000", and for any invalid cases
 */

static ngx_int_t
ngx_http_gzip_accept_encoding(ngx_str_t *ae)
{
    u_char  *p, *start, *last;

    start = ae->data;
    last = start + ae->len;

    for ( ;; ) {
        p = ngx_strcasestrn(start, "gzip", 4 - 1);
        if (p == NULL) {
            return NGX_DECLINED;
        }

        if (p == start || (*(p - 1) == ',' || *(p - 1) == ' ')) {
            break;
        }

        start = p + 4;
    }

    p += 4;

    while (p < last) {
        switch (*p++) {
        case ',':
            return NGX_OK;
        case ';':
            goto quantity;
        case ' ':
            continue;
        default:
            return NGX_DECLINED;
        }
    }

    return NGX_OK;

quantity:

    while (p < last) {
        switch (*p++) {
        case 'q':
        case 'Q':
            goto equal;
        case ' ':
            continue;
        default:
            return NGX_DECLINED;
        }
    }

    return NGX_OK;

equal:

    if (p + 2 > last || *p++ != '=') {
        return NGX_DECLINED;
    }

    if (ngx_http_gzip_quantity(p, last) == 0) {
        return NGX_DECLINED;
    }

    return NGX_OK;
}


static ngx_uint_t
ngx_http_gzip_quantity(u_char *p, u_char *last)
{
    u_char      c;
    ngx_uint_t  n, q;

    c = *p++;

    if (c != '0' && c != '1') {
        return 0;
    }

    q = (c - '0') * 100;

    if (p == last) {
        return q;
    }

    c = *p++;

    if (c == ',' || c == ' ') {
        return q;
    }

    if (c != '.') {
        return 0;
    }

    n = 0;

    while (p < last) {
        c = *p++;

        if (c == ',' || c == ' ') {
            break;
        }

        if (c >= '0' && c <= '9') {
            q += c - '0';
            n++;
            continue;
        }

        return 0;
    }

    if (q > 100 || n > 3) {
        return 0;
    }

    return q;
}

#endif


/*
rÊÇÎÒÃÇµÄmodule handlerÖÐ£¬nginxµ÷ÓÃÊ±´«¸øÎÒÃÇµÄÇëÇó£¬ÕâÊ±ÎÒÃÇÖ±½Ó´«¸øsubrequest¼´¿É¡£uriºÍargsÊÇÎÒÃÇÐèÒª·ÃÎÊbackend serverµÄURL£¬
¶øpsrÊÇsubrequestº¯ÊýÖ´ÐÐÍêºó·µ»Ø¸øÎÒÃÇµÄÐÂÇëÇó£¬¼´½«Òª·ÃÎÊbackend serverµÄÇëÇóÖ¸Õë¡£psÖ¸Ã÷ÁË»Øµ÷º¯Êý£¬¾ÍÊÇËµ£¬Èç¹ûÕâ¸öÇëÇóÖ´ÐÐÍê±Ï£¬
½ÓÊÕµ½ÁËbackend serverµÄÏìÓ¦ºó£¬¾Í»á»Øµ÷Õâ¸öº¯Êý¡£flags»áÖ¸¶¨Õâ¸ö×ÓÇëÇóµÄÒ»Ð©ÌØÕ÷¡£
*/ 

/*
sub1_rºÍsub2_r¶¼ÊÇÍ¬Ò»¸ö¸¸ÇëÇó£¬¾ÍÊÇroot_rÇëÇó£¬sub1_rºÍsub2_r¾ÍÊÇngx_http_postponed_request_s->request³ÉÔ±
ËüÃÇÓÉngx_http_postponed_request_s->nextÁ¬½ÓÔÚÒ»Æð£¬²Î¿¼ngx_http_subrequest

                          -----root_r(Ö÷ÇëÇó)     
                          |postponed
                          |                next
            -------------sub1_r(data1)--------------sub2_r(data1)
            |                                       |postponed                    
            |postponed                              |
            |                                     sub21_r-----sub22
            |
            |               next
          sub11_r(data11)-----------sub12_r(data12)

    Í¼ÖÐµÄroot½Úµã¼´ÎªÖ÷ÇëÇó£¬ËüµÄpostponedÁ´±í´Ó×óÖÁÓÒ¹ÒÔØÁË3¸ö½Úµã£¬SUB1ÊÇËüµÄµÚÒ»¸ö×ÓÇëÇó£¬DATA1ÊÇËü²úÉúµÄÒ»¶ÎÊý¾Ý£¬SUB2ÊÇËüµÄµÚ2¸ö×ÓÇëÇó£¬
¶øÇÒÕâ2¸ö×ÓÇëÇó·Ö±ðÓÐËüÃÇ×Ô¼ºµÄ×ÓÇëÇó¼°Êý¾Ý¡£ngx_connection_tÖÐµÄdata×Ö¶Î±£´æµÄÊÇµ±Ç°¿ÉÒÔÍùout chain·¢ËÍÊý¾ÝµÄÇëÇó£¬ÎÄÕÂ¿ªÍ·Ëµµ½·¢µ½¿Í»§¶Ë
µÄÊý¾Ý±ØÐë°´ÕÕ×ÓÇëÇó´´½¨µÄË³Ðò·¢ËÍ£¬ÕâÀï¼´ÊÇ°´ºóÐø±éÀúµÄ·½·¨£¨SUB11->DATA11->SUB12->DATA12->(SUB1)->DATA1->SUB21->SUB22->(SUB2)->(ROOT)£©£¬
ÉÏÍ¼ÖÐµ±Ç°ÄÜ¹»Íù¿Í»§¶Ë£¨out chain£©·¢ËÍÊý¾ÝµÄÇëÇóÏÔÈ»¾ÍÊÇSUB11£¬Èç¹ûSUB12ÌáÇ°Ö´ÐÐÍê³É£¬²¢²úÉúÊý¾ÝDATA121£¬Ö»ÒªÇ°ÃæËü»¹ÓÐ½ÚµãÎ´·¢ËÍÍê±Ï£¬
DATA121Ö»ÄÜÏÈ¹ÒÔØÔÚSUB12µÄpostponedÁ´±íÏÂ¡£ÕâÀï»¹Òª×¢ÒâÒ»ÏÂµÄÊÇc->dataµÄÉèÖÃ£¬µ±SUB11Ö´ÐÐÍê²¢ÇÒ·¢ËÍÍêÊý¾ÝÖ®ºó£¬ÏÂÒ»¸ö½«Òª·¢ËÍµÄ½ÚµãÓ¦¸ÃÊÇ
DATA11£¬µ«ÊÇ¸Ã½ÚµãÊµ¼ÊÉÏ±£´æµÄÊÇÊý¾Ý£¬¶ø²»ÊÇ×ÓÇëÇó£¬ËùÒÔc->dataÕâÊ±Ó¦¸ÃÖ¸ÏòµÄÊÇÓµÓÐ¸ÄÊý¾Ý½ÚµãµÄSUB1ÇëÇó¡£

·¢ËÍÊý¾Ýµ½¿Í»§¶ËÓÅÏÈ¼¶:
1.×ÓÇëÇóÓÅÏÈ¼¶±È¸¸ÇëÇó¸ß
2.Í¬¼¶(Ò»¸ör²úÉú¶à¸ö×ÓÇëÇó)ÇëÇó£¬´Ó×óµ½ÓÒÓÅÏÈ¼¶ÓÉ¸ßµ½µÍ(ÒòÎªÏÈ´´½¨µÄ×ÓÇëÇóÏÈ·¢ËÍÊý¾Ýµ½¿Í»§¶Ë)
·¢ËÍÊý¾Ýµ½¿Í»§¶ËË³Ðò¿ØÖÆ¼ûngx_http_postpone_filter    nginxÍ¨¹ý×ÓÇëÇó·¢ËÍÊý¾Ýµ½ºó¶Ë¼ûngx_http_run_posted_requests
*/

//subrequest×¢Òângx_http_run_posted_requestsÓëngx_http_subrequest ngx_http_postpone_filter ngx_http_finalize_requestÅäºÏÔÄ¶Á
//subrequest²Î¿¼http://blog.csdn.net/fengmo_q/article/details/6685840  nginx subrequestµÄÊµÏÖ½âÎö

/*
    (1)  ngx_http_request_t *r
    ngx_http_request_t *rÊÇµ±Ç°µÄÇëÇó£¬Ò²¾ÍÊÇ¸¸ÇëÇó¡£
    (2) uri
    ngx_str_t *uriÊÇ×ÓÇëÇóµÄURI£¬Ëü¶Ô¾¿¾ºÑ¡ÓÃnginx.confÅäÖÃÎÄ¼þÖÐµÄÄÄ¸öÄ£¿éÀ´´¦Àí×ÓÇëÇóÆð¾ö¶¨ÐÔ×÷ÓÃ¡£
    (3) ngx_str_t *args
    ngx_str_t *argsÊÇ×ÓÇëÇóµÄURI²ÎÊý£¬Èç¹ûÃ»ÓÐ²ÎÊý£¬¿ÉÒÔ´«ËÍNULL¿ÕÖ¸Õë¡£
    (4) ngx_http_request_t **psr
        psrÊÇÊä³ö²ÎÊý¶ø²»ÊÇÊäÈë²ÎÊý£¬Ëü½«°Ñngx_http_subrequestÉú³ÉµÄ×ÓÇëÇó´«³öÀ´¡£Ò»°ã£¬ÎÒÃÇÏÈ½¨Á¢Ò»¸ö×Ó
    ÇëÇóµÄ¿ÕÖ¸Õëngx_http_request_t *psr£¬ÔÙ°ÑËüµÄµØÖ·&psr´«ÈËµ½ngx_http_subrequest·½·¨ÖÐ£¬Èç¹ûngx_http_subrequest
    ·µ»Ø³É¹¦£¬psr¾ÍÖ¸Ïò½¨Á¢ºÃµÄ×ÓÇëÇó¡£
    (5)  ngx_http_post_subrequest_t *ps
    ngx_http_post_subrequest_t½á¹¹ÌåµØÖ·£¬ËüÖ¸³ö×ÓÇëÇó½áÊøÊ±±ØÐë»Øµ÷µÄ´¦Àí·½·¨¡£
    (6) ngx_uint_t flags
        flagµÄÈ¡Öµ·¶Î§°üÀ¨£º¢Ù0ÔÚÃ»ÓÐÌØÊâÐèÇóµÄÇé¿öÏÂ¶¼Ó¦¸ÃÌîÐ´Ëü£»¢ÚNGX_HTTP_SUBREQUEST_IN_MEMORY¡£
    Õâ¸öºê»á½«×ÓÇëÇóµÄsubrequest_in_memory±êÖ¾Î»ÖÃÎª1£¬ÕâÒâÎ¶×ÅÈç¹û×ÓÇëÇóÊ¹ÓÃupstream·ÃÎÊÉÏÓÎ·þÎñÆ÷£¬
    ÄÇÃ´ÉÏÓÎ·þÎñÆ÷µÄÏìÓ¦¶¼½«»áÔÚÄÚ´æÖÐ´¦Àí£»¢ÛNGX_HTTP_SUBREQUEST_WAITED¡£Õâ¸öºê»á½«×ÓÇëÇóµÄwaited±êÖ¾Î»ÖÃÎª1£¬
    µ±×ÓÇëÇóÌáÇ°½áÊøÊ±£¬ÓÐ¸ödone±êÖ¾Î»»áÖÃÎª1£¬µ«Ä¿Ç°HTTP¿ò¼Ü²¢Ã»ÓÐÕë¶ÔÕâÁ½¸ö±êÖ¾Î»×öÈÎºÎÊµÖÊÐÔ´¦Àí¡£×¢Òâ£¬
    flagÊÇ°´±ÈÌØÎ»²Ù×÷µÄ£¬ÕâÑù¿ÉÒÔÍ¬Ê±º¬ÓÐÉÏÊö3¸öÖµ¡£
    (7)·µ»ØÖµ
    ·µ»ØNGX OK±íÊ¾³É¹¦½¨Á¢×ÓÇëÇó£»·µ»ØNGX_ERROR±íÊ¾½¨Á¢×ÓÇëÇóÊ§°Ü¡£

    ¸Ãº¯ÊýÖ÷ÒªÊÇ´´½¨Ò»¸ö×ÓÇëÇó½á¹¹ngx_http_request_t£¬È»ºó°Ñ¸¸ÇëÇórµÄÏà¹ØÖµ¸³¸ø×ÓÇëÇór
*/
ngx_int_t
ngx_http_subrequest(ngx_http_request_t *r,
    ngx_str_t *uri, ngx_str_t *args, ngx_http_request_t **psr,
    ngx_http_post_subrequest_t *ps, ngx_uint_t flags) /* **psrµÄÄ¿µÄÊÇÎªÁË°Ñ´´½¨µÄ×ÓÇëÇórÍ¨¹ýpsr´«µÝ³öÀ´ */
{
    ngx_time_t                    *tp;
    ngx_connection_t              *c;
    ngx_http_request_t            *sr;
    ngx_http_core_srv_conf_t      *cscf;
    ngx_http_postponed_request_t  *pr, *p;

    r->main->subrequests--;

    if (r->main->subrequests == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "subrequests cycle while processing \"%V\"", uri);
        r->main->subrequests = 1;
        return NGX_ERROR;
    }

    sr = ngx_pcalloc(r->pool, sizeof(ngx_http_request_t));
    if (sr == NULL) {
        return NGX_ERROR;
    }

    sr->signature = NGX_HTTP_MODULE;

    c = r->connection;
    sr->connection = c; /* Í¬Ò»¸ö¸¸ÇëÇó¶ÔÓ¦µÄËùÓÐ×ÓÇëÇóËüÃÇµÄconnection¶¼¶ÔÓ¦µÄÍ¬Ò»¸ö¿Í»§¶Ë£¬ËùÒÔ¶¼ÊÇ¹²ÓÃµÄÍ¬Ò»¸öconn */

    sr->ctx = ngx_pcalloc(r->pool, sizeof(void *) * ngx_http_max_module);
    if (sr->ctx == NULL) {
        return NGX_ERROR;
    }

    if (ngx_list_init(&sr->headers_out.headers, r->pool, 20,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
    sr->main_conf = cscf->ctx->main_conf;
    sr->srv_conf = cscf->ctx->srv_conf;
    sr->loc_conf = cscf->ctx->loc_conf;

    sr->pool = r->pool;

    sr->headers_in = r->headers_in;

    ngx_http_clear_content_length(sr);
    ngx_http_clear_accept_ranges(sr);
    ngx_http_clear_last_modified(sr);

    sr->request_body = r->request_body;

#if (NGX_HTTP_V2)
    sr->stream = r->stream;
#endif

    sr->method = NGX_HTTP_GET;
    sr->http_version = r->http_version;

    sr->request_line = r->request_line;
    sr->uri = *uri;

    if (args) {
        sr->args = *args;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http subrequest \"%V?%V\"", uri, &sr->args);

     /* ½âÎöflags£¬ subrequest_in_memoryÔÚupstreamÄ£¿é½âÎöÍêÍ·²¿£¬ 
       ·¢ËÍbody¸ødownsstreamÊ±ÓÃµ½ */  
    sr->subrequest_in_memory = (flags & NGX_HTTP_SUBREQUEST_IN_MEMORY) != 0;
    sr->waited = (flags & NGX_HTTP_SUBREQUEST_WAITED) != 0;

    sr->unparsed_uri = r->unparsed_uri;
    sr->method_name = ngx_http_core_get_method;
    sr->http_protocol = r->http_protocol;

    ngx_http_set_exten(sr);

    /* Ö÷ÇëÇó±£´æÔÚmain×Ö¶ÎÖÐ£¬ÕâÀïÆäÊµ¾ÍÊÇ×îÉÏ²ã¸úÇëÇó£¬ÀýÈçµ±Ç°ÊÇËÄ²ã×ÓÇëÇó£¬ÔòmainÊ¼ÖÕÖ¸ÏòµÚÒ»²ã¸¸ÇëÇó£¬
        ¶ø²»ÊÇµÚÈý´Î¸¸ÇëÇó£¬parentÖ¸ÏòµÚÈý²ã¸¸ÇëÇó */  
    sr->main = r->main;
    sr->parent = r; 
    
    sr->post_subrequest = ps;/* ±£´æ»Øµ÷handler¼°Êý¾Ý£¬ÔÚ×ÓÇëÇóÖ´ÐÐÍê£¬½«»áµ÷ÓÃ */  

     /* ¶ÁÊÂ¼þhandler¸³ÖµÎª²»×öÈÎºÎÊÂµÄº¯Êý£¬ÒòÎª×ÓÇëÇó²»ÓÃÔÙ¶ÁÊý¾Ý»òÕß¼ì²éÁ¬½Ó×´Ì¬£» 
       Ð´ÊÂ¼þhandlerÎªngx_http_handler£¬Ëü»áÖØ×ßphase */  
    sr->read_event_handler = ngx_http_request_empty_handler;
    sr->write_event_handler = ngx_http_handler;

    /*  sub1_rºÍsub2_r¶¼ÊÇÍ¬Ò»¸ö¸¸ÇëÇó£¬¾ÍÊÇroot_rÇëÇó£¬sub1_rºÍsub2_r¾ÍÊÇngx_http_postponed_request_s->request³ÉÔ±
    ËüÃÇÓÉngx_http_postponed_request_s->nextÁ¬½ÓÔÚÒ»Æð£¬²Î¿¼ngx_http_subrequest

                          -----root_r(Ö÷ÇëÇó)     
                          |postponed
                          |                next
            -------------sub1_r(data1)--------------sub2_r(data1)
            |                                       |postponed                    
            |postponed                              |
            |                                     sub21_r-----sub22
            |
            |               next
          sub11_r(data11)-----------sub12_r(data12)

          ÏÂÃæµÄÕâ¸öifÖÐ×îÖÕc->dataÖ¸ÏòµÄÊÇsub11_r£¬Ò²¾ÍÊÇ×î×óÏÂ²ãµÄr
     */
    //×¢Òâ:ÔÚ´´½¨×ÓÇëÇóµÄ¹ý³ÌÖÐ²¢Ã»ÓÐ´´½¨ÐÂµÄngx_connection_t£¬Ò²¾ÍÊÇÊ¼ÖÕÓÃµÄrootÇëÇóµÄngx_connection_t
    if (c->data == r && r->postponed == NULL) { //ËµÃ÷ÊÇr»¹Ã»ÓÐ×ÓÇëÇó£¬ÔÚ´´½¨rµÄµÚÒ»¸ö×ÓÇëÇó£¬ÀýÈçµÚ¶þ²ãrµÄµÚÒ»¸ö×ÓÇëÇó¾ÍÊÇµÚÈý²ãr
        c->data = sr;  /* ×îÖÕ¿Í»§¶ËÇëÇór->connection->dataÖ¸Ïò×îÏÂ²ã×ó±ßµÄ×ÓÇëÇó */
    } 

    
    /*
     ¶ÔÓÚ×ÓÇëÇó£¬ËäÈ»ÓÐ¶ÀÁ¢µÄngx_http_request_t¶ÔÏór£¬µ«ÊÇÈ´Ã»ÓÐ¶îµÄÍâ´´½¨r->variables£¬ºÍ¸¸ÇëÇó£¨»òÕßËµÖ÷ÇëÇó£©ÊÇ¹²ÏíµÄ

     Õë¶Ô×ÓÇëÇó£¬ËäÈ»ÖØÐÂ´´½¨ÁËngx_http_request_t±äÁ¿sr£¬µ«×ÓÇëÇóµÄNginx±äÁ¿ÖµÊý×ésr->variablesÈ´ÊÇÖ±½ÓÖ¸Ïò¸¸ÇëÇóµÄr->variables¡£
 ÆäÊµÕâ²¢²»ÄÑÀí½â£¬ÒòÎª¸¸×ÓÇëÇóµÄ´ó²¿·Ö±äÁ¿Öµ¶¼ÊÇÒ»ÑùµÄ£¬µ±È»Ã»±ØÒªÉêÇëÁíÍâµÄÕ­¼ä£¬¶ø¶ÔÓÚÄÇÐ©¸¸×ÓÇëÇóÖ®¼ä¿ÉÄÜ»áÓÐ²»Í¬±äÁ¿ÖµµÄ
±äÁ¿£¬ÓÖÓÐNGXHTTP_VARNOCACHEABLE±ê¼ÇµÄ´æÔÚ£¬ËùÒÔÒ²²»»áÓÐÊ²Ã´ÎÊÌâ¡£±ÈÈç±äÁ¿$args£¬ÔÚ¸¸ÇëÇóÀïÈ¥·ÃÎÊ¸Ã±äÁ¿ÖµÊ±£¬·¢ÏÖ¸Ã±äÁ¿ÊÇ²»¿É»º
´æµÄ£¬ÓÚÊÇ¾Íµ÷ÓÃget_handler0º¯Êý´Ómain_req¶ÔÏóµÄargs×Ö¶Î£¨¼´r->args£©ÀïÈ¥È¡£¬´ËÊ±µÃµ½µÄÖµ¿ÉÄÜÊÇpage=9999¡£¶øÔÚ×ÓÇëÇóÀïÈ¥·ÃÎÊ¸Ã±ä
Á¿ÖµÊ±£¬·¢ÏÖ¸Ã±äÁ¿ÊÇ²»¿É»º´æµÄ£¬ÓÚÊÇÒ²µ÷ÓÃget_handler0º¯Êý´Ósub__req¶ÔÏóµÄargs×Ö¶Î£¨¼´sr->args£®×¢Òâ¶ÔÏósrÓërÖ®¼äÊÇ·Ö¸ô¿ªµÄ£©Àï
È¥È¡£¬´ËÊ±µÃµ½µÄÖµ¾Í¿ÉÄÜÊÇid=12¡£Òò¶ø£¬ÔÚ»ñÈ¡¸¸×ÓÇëÇóÖ®¼ä¿É±ä±äÁ¿µÄÖµÊ±£¬²¢²»»áÏà»¥¸ÉÈÅ
     */
    sr->variables = r->variables;/* Ä¬ÈÏ¹²Ïí¸¸ÇëÇóµÄ±äÁ¿£¬µ±È»ÄãÒ²¿ÉÒÔ¸ù¾ÝÐèÇóÔÚ´´½¨Íê×ÓÇëÇóºó£¬ÔÙ´´½¨×ÓÇëÇó¶ÀÁ¢µÄ±äÁ¿¼¯ */  

    sr->log_handler = r->log_handler;

    pr = ngx_palloc(r->pool, sizeof(ngx_http_postponed_request_t));
    if (pr == NULL) {
        return NGX_ERROR;
    }

    pr->request = sr;
    pr->out = NULL;
    pr->next = NULL;

    //Á¬½ÓÍ¼ÐÎ»¯¿ÉÒÔ²Î¿¼http://blog.csdn.net/fengmo_q/article/details/6685840
    if (r->postponed) {/* °Ñ¸Ã×ÓÇëÇó¹ÒÔØÔÚÆä¸¸ÇëÇóµÄpostponedÁ´±íµÄ¶ÓÎ² */  
        //Í¬Ò»¸örÖÐ´´½¨µÄ×ÓÇëÇóÍ¨¹ýr->postponed->nextÁ¬½ÓÔÚÒ»Æð£¬ÕâÐ©×ÓÇëÇóÖÐ·Ö±ðÔÚ´´½¨×ÓÇëÇóÔòÍ¨¹ýpostponedÖ¸Ïò¸÷×ÔµÄ×ÓÇëÇó
        for (p = r->postponed; p->next; p = p->next) { /* void */ }
        p->next = pr;

    } else {
        r->postponed = pr;
    }

    /* ÕâÀï¸³ÖµÎª1£¬ÐèÒª×ö´Ó¶¨Ïò */
    sr->internal = 1;

     /* ¼Ì³Ð¸¸ÇëÇóµÄÒ»Ð©×´Ì¬ */  
    sr->discard_body = r->discard_body;
    sr->expect_tested = 1;
    sr->main_filter_need_in_memory = r->main_filter_need_in_memory;

    sr->uri_changes = NGX_HTTP_MAX_URI_CHANGES + 1;

    tp = ngx_timeofday();
    sr->start_sec = tp->sec;
    sr->start_msec = tp->msec;
     /* Ôö¼ÓÖ÷ÇëÇóµÄÒýÓÃÊý£¬Õâ¸ö×Ö¶ÎÖ÷ÒªÊÇÔÚngx_http_finalize_requestµ÷ÓÃµÄÒ»Ð©½áÊøÇëÇóºÍ 
       Á¬½ÓµÄº¯ÊýÖÐÊ¹ÓÃ */  
    r->main->count++;

    *psr = sr;

    return ngx_http_post_request(sr, NULL);/* ngx_http_post_request½«¸Ã×ÓÇëÇó¹ÒÔØÔÚÖ÷ÇëÇóµÄposted_requestsÁ´±í¶ÓÎ² */  
}

//ÄÚ²¿ÖØ¶¨ÏòÊÇ´ÓNGX_HTTP_SERVER_REWRITE_PHASE´¦¼ÌÐøÖ´ÐÐ(ngx_http_internal_redirect)£¬¶øÖØÐÂrewriteÊÇ´ÓNGX_HTTP_FIND_CONFIG_PHASE´¦Ö´ÐÐ(ngx_http_core_post_rewrite_phase)
ngx_int_t
ngx_http_internal_redirect(ngx_http_request_t *r,
    ngx_str_t *uri, ngx_str_t *args)
{
    ngx_http_core_srv_conf_t  *cscf;

    r->uri_changes--; //ÖØ¶¨Ïò´ÎÊý¼õ1£¬Èç¹ûµ½0ÁË£¬ËµÃ÷ÕâÃ´¶à´ÎÖØ¶¨ÏòÒÑ¾­½áÊø£¬Ö±½Ó·µ»Ø

    if (r->uri_changes == 0) {//³õÊ¼ÉèÖÃÎªNGX_HTTP_MAX_URI_CHANGES£¬¼´×î¶àÖØ¶¨Ïò10´Ë£¬ÕâÊÇÎªÁË±ÜÃâÑ­»·ÖØ¶¨ÏòµÄÎÊÌâ¡£
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "rewrite or internal redirection cycle "
                      "while internally redirecting to \"%V\"", uri);

        r->main->count++;
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_DONE;
    }

    r->uri = *uri;

    if (args) {
        r->args = *args;

    } else {
        ngx_str_null(&r->args);
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "internal redirect: \"%V?%V\"", uri, &r->args);

    ngx_http_set_exten(r);

    /* clear the modules contexts */
    ngx_memzero(r->ctx, sizeof(void *) * ngx_http_max_module);

    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
    r->loc_conf = cscf->ctx->loc_conf;

    ngx_http_update_location_config(r);

#if (NGX_HTTP_CACHE)
    r->cache = NULL;
#endif

    r->internal = 1; //ÉèÖÃÇëÇóÎªÄÚ²¿ÖØ¶¨Ïò×´Ì¬¡£Í¨Öªngx_http_handler,½øÐÐ¼ä¶ÏÑ¡ÔñµÄÊ±ºò´Óserver_rewrite_index¿ªÊ¼½øÐÐÑ­»·´¦Àí£¬²»È»ÓÖ»ØÈ¥ÁË
    r->valid_unparsed_uri = 0;
    r->add_uri_to_alias = 0;
    r->main->count++;

    ngx_http_handler(r); //½øÐÐÄÚ²¿ÖØ¶¨Ïò¹ý³Ì

    return NGX_DONE;
}

//@nameÃüÃûÖØ¶¨Ïò
ngx_int_t
ngx_http_named_location(ngx_http_request_t *r, ngx_str_t *name)
{
    ngx_http_core_srv_conf_t    *cscf;
    ngx_http_core_loc_conf_t   **clcfp;
    ngx_http_core_main_conf_t   *cmcf;

    r->main->count++;
    r->uri_changes--;

    if (r->uri_changes == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "rewrite or internal redirection cycle "
                      "while redirect to named location \"%V\"", name);

        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_DONE;
    }

    if (r->uri.len == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "empty URI in redirect to named location \"%V\"", name);

        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_DONE;
    }

    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

    if (cscf->named_locations) {

        for (clcfp = cscf->named_locations; *clcfp; clcfp++) {

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "name location, test location: \"%V\", uri:%s", &(*clcfp)->name, r->uri);

            if (name->len != (*clcfp)->name.len
                || ngx_strncmp(name->data, (*clcfp)->name.data, name->len) != 0)
            {
                continue;
            }

            ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "using location: %V \"%V?%V\"",
                           name, &r->uri, &r->args);

            r->internal = 1;
            r->content_handler = NULL;
            r->uri_changed = 0;
            r->loc_conf = (*clcfp)->loc_conf;

            /* clear the modules contexts */
            ngx_memzero(r->ctx, sizeof(void *) * ngx_http_max_module);

            ngx_http_update_location_config(r);

            cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

            r->phase_handler = cmcf->phase_engine.location_rewrite_index;

            r->write_event_handler = ngx_http_core_run_phases;
            ngx_http_core_run_phases(r);

            return NGX_DONE;
        }
    }

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "could not find named location \"%V\"", name);

    ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);

    return NGX_DONE;
}

//pollµÄÇåÀíÓÃngx_pool_cleanup_add, ngx_http_request_tµÄÇåÀíÓÃngx_http_cleanup_add
ngx_http_cleanup_t *
ngx_http_cleanup_add(ngx_http_request_t *r, size_t size) //ÉêÇëÒ»¸öngx_http_cleanup_tÌí¼Óµ½r->cleanupÍ·²¿ 
{
    ngx_http_cleanup_t  *cln;

    r = r->main;

    cln = ngx_palloc(r->pool, sizeof(ngx_http_cleanup_t));
    if (cln == NULL) {
        return NULL;
    }

    if (size) {
        cln->data = ngx_palloc(r->pool, size);
        if (cln->data == NULL) {
            return NULL;
        }

    } else {
        cln->data = NULL;
    }

    cln->handler = NULL;
    cln->next = r->cleanup;

    r->cleanup = cln;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http cleanup add: %p", cln);

    return cln;
}

//·ûºÅÁ¬½ÓÏà¹Ø
ngx_int_t
ngx_http_set_disable_symlinks(ngx_http_request_t *r,
    ngx_http_core_loc_conf_t *clcf, ngx_str_t *path, ngx_open_file_info_t *of)
{
#if (NGX_HAVE_OPENAT)
    u_char     *p;
    ngx_str_t   from;

    of->disable_symlinks = clcf->disable_symlinks;

    if (clcf->disable_symlinks_from == NULL) {
        return NGX_OK;
    }

    if (ngx_http_complex_value(r, clcf->disable_symlinks_from, &from)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (from.len == 0
        || from.len > path->len
        || ngx_memcmp(path->data, from.data, from.len) != 0)
    {
        return NGX_OK;
    }

    if (from.len == path->len) {
        of->disable_symlinks = NGX_DISABLE_SYMLINKS_OFF;
        return NGX_OK;
    }

    p = path->data + from.len;

    if (*p == '/') {
        of->disable_symlinks_from = from.len;
        return NGX_OK;
    }

    p--;

    if (*p == '/') {
        of->disable_symlinks_from = from.len - 1;
    }
#endif

    return NGX_OK;
}


ngx_int_t
ngx_http_get_forwarded_addr(ngx_http_request_t *r, ngx_addr_t *addr,
    ngx_array_t *headers, ngx_str_t *value, ngx_array_t *proxies,
    int recursive)
{
    ngx_int_t          rc;
    ngx_uint_t         i, found;
    ngx_table_elt_t  **h;

    if (headers == NULL) {
        return ngx_http_get_forwarded_addr_internal(r, addr, value->data,
                                                    value->len, proxies,
                                                    recursive);
    }

    i = headers->nelts;
    h = headers->elts;

    rc = NGX_DECLINED;

    found = 0;

    while (i-- > 0) {
        rc = ngx_http_get_forwarded_addr_internal(r, addr, h[i]->value.data,
                                                  h[i]->value.len, proxies,
                                                  recursive);

        if (!recursive) {
            break;
        }

        if (rc == NGX_DECLINED && found) {
            rc = NGX_DONE;
            break;
        }

        if (rc != NGX_OK) {
            break;
        }

        found = 1;
    }

    return rc;
}


static ngx_int_t
ngx_http_get_forwarded_addr_internal(ngx_http_request_t *r, ngx_addr_t *addr,
    u_char *xff, size_t xfflen, ngx_array_t *proxies, int recursive)
{
    u_char           *p;
    in_addr_t         inaddr;
    ngx_int_t         rc;
    ngx_addr_t        paddr;
    ngx_cidr_t       *cidr;
    ngx_uint_t        family, i;
#if (NGX_HAVE_INET6)
    ngx_uint_t        n;
    struct in6_addr  *inaddr6;
#endif

#if (NGX_SUPPRESS_WARN)
    inaddr = 0;
#if (NGX_HAVE_INET6)
    inaddr6 = NULL;
#endif
#endif

    family = addr->sockaddr->sa_family;

    if (family == AF_INET) {
        inaddr = ((struct sockaddr_in *) addr->sockaddr)->sin_addr.s_addr;
    }

#if (NGX_HAVE_INET6)
    else if (family == AF_INET6) {
        inaddr6 = &((struct sockaddr_in6 *) addr->sockaddr)->sin6_addr;

        if (IN6_IS_ADDR_V4MAPPED(inaddr6)) {
            family = AF_INET;

            p = inaddr6->s6_addr;

            inaddr = p[12] << 24;
            inaddr += p[13] << 16;
            inaddr += p[14] << 8;
            inaddr += p[15];

            inaddr = htonl(inaddr);
        }
    }
#endif

    for (cidr = proxies->elts, i = 0; i < proxies->nelts; i++) {
        if (cidr[i].family != family) {
            goto next;
        }

        switch (family) {

#if (NGX_HAVE_INET6)
        case AF_INET6:
            for (n = 0; n < 16; n++) {
                if ((inaddr6->s6_addr[n] & cidr[i].u.in6.mask.s6_addr[n])
                    != cidr[i].u.in6.addr.s6_addr[n])
                {
                    goto next;
                }
            }
            break;
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
        case AF_UNIX:
            break;
#endif

        default: /* AF_INET */
            if ((inaddr & cidr[i].u.in.mask) != cidr[i].u.in.addr) {
                goto next;
            }
            break;
        }

        for (p = xff + xfflen - 1; p > xff; p--, xfflen--) {
            if (*p != ' ' && *p != ',') {
                break;
            }
        }

        for ( /* void */ ; p > xff; p--) {
            if (*p == ' ' || *p == ',') {
                p++;
                break;
            }
        }

        if (ngx_parse_addr(r->pool, &paddr, p, xfflen - (p - xff)) != NGX_OK) {
            return NGX_DECLINED;
        }

        *addr = paddr;

        if (recursive && p > xff) {
            rc = ngx_http_get_forwarded_addr_internal(r, addr, xff, p - 1 - xff,
                                                      proxies, 1);

            if (rc == NGX_DECLINED) {
                return NGX_DONE;
            }

            /* rc == NGX_OK || rc == NGX_DONE  */
            return rc;
        }

        return NGX_OK;

    next:
        continue;
    }

    return NGX_DECLINED;
}

/*
cf¿Õ¼äÊ¼ÖÕÔÚÒ»¸öµØ·½£¬¾ÍÊÇngx_init_cycleÖÐµÄconf£¬Ê¹ÓÃÖÐÖ»ÊÇ¼òµ¥µÄÐÞ¸ÄconfÖÐµÄctxÖ¸ÏòÒÑ¾­cmd_typeÀàÐÍ£¬È»ºóÔÚ½âÎöµ±Ç°{}ºó£¬ÖØÐÂ»Ö¸´½âÎöµ±Ç°{}Ç°µÄÅäÖÃ
²Î¿¼"http" "server" "location"ngx_http_block  ngx_http_core_server  ngx_http_core_location  ngx_http_core_location
*/
//¼ûngx_http_core_location location{}ÅäÖÃµÄctx->loc_conf[ngx_http_core_module.ctx_index]´æ´¢ÔÚ¸¸¼¶server{}µÄctx->loc_conf[ngx_http_core_module.ctx_index]->locationsÖÐ
//¼ûngx_http_core_server server{}ÅäÖÃµÄctx->srv_conf´æ´¢ÔÚ¸¸¼¶http{}ctx¶ÔÓ¦µÄctx->main_conf[ngx_http_core_module.ctx_index]->serversÖÐ£¬Í¨¹ýÕâ¸ösrv_conf[]->ctx¾ÍÄÜ»ñÈ¡µ½server{}µÄÉÏÏÂÎÄctx
static char *
ngx_http_core_server(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{ //Í¼ÐÎ»¯²Î¿¼:ÉîÈëÀí½âNGINXÖÐµÄÍ¼9-2  Í¼10-1  Í¼4-2£¬½áºÏÍ¼¿´,²¢¿ÉÒÔÅäºÏhttp://tech.uc.cn/?p=300¿´
    char                        *rv;
    void                        *mconf;
    ngx_uint_t                   i;
    ngx_conf_t                   pcf;
    ngx_http_module_t           *module;
    struct sockaddr_in          *sin;
    ngx_http_conf_ctx_t         *ctx, *http_ctx;
    ngx_http_listen_opt_t        lsopt;
    ngx_http_core_srv_conf_t    *cscf, **cscfp;
    ngx_http_core_main_conf_t   *cmcf;

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    //server{}µÄ¸¸¼¶http{}µÄÉÏÏÂÎÄctx
    http_ctx = cf->ctx; //±£´æÉÏÒ»¼¶http{}ÖÐ(server{}Íâ)µÄÅäÖÃµ½http_ctxÖÐ  ÔÚngx_init_cycleÖÐcf->ctx = cycle->conf_ctx;

    /*
    server¿éÏÂngx_http_conf ctx_t½á¹¹ÖÐµÄmain confÊý×é½«Í¨¹ýÖ±½ÓÖ¸ÏòÀ´¸´ÓÃËùÊôµÄhttp¿éÏÂµÄ
    main_confÊý×é£¨ÆäÊµÊÇËµserver¿éÏÂÃ»ÓÐmain¼¶±ðÅäÖÃ£¬ÕâÊÇÏÔÈ»µÄ£©
    */ //Í¼ÐÎ»¯²Î¿¼:ÉîÈëÀí½âNGINXÖÐµÄÍ¼9-2  Í¼10-1  Í¼4-2£¬½áºÏÍ¼¿´,²¢¿ÉÒÔÅäºÏhttp://tech.uc.cn/?p=300¿´
    ctx->main_conf = http_ctx->main_conf;

    /* the server{}'s srv_conf */

    ctx->srv_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->srv_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    /* the server{}'s loc_conf */

    ctx->loc_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->loc_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    for (i = 0; ngx_modules[i]; i++) {
        if (ngx_modules[i]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = ngx_modules[i]->ctx;

        if (module->create_srv_conf) {
            mconf = module->create_srv_conf(cf); //ºÍÖ´ÐÐµ½http{}ÐÐÒ»Ñù£¬erver±¾ÐÐÒ²µ÷ÓÃngx_http_core_module_ctx
            if (mconf == NULL) {
                return NGX_CONF_ERROR;
            }

            ctx->srv_conf[ngx_modules[i]->ctx_index] = mconf;
        }

        if (module->create_loc_conf) {
            mconf = module->create_loc_conf(cf);
            if (mconf == NULL) {
                return NGX_CONF_ERROR;
            }

            ctx->loc_conf[ngx_modules[i]->ctx_index] = mconf;
        }
    }

    /* the server configuration context */

    cscf = ctx->srv_conf[ngx_http_core_module.ctx_index];
    cscf->ctx = ctx; //serv_conf[]Ö¸ÏòÔÚ¸Ãserver{}ÀïÃæ´´½¨µÄngx_http_conf_ctx_t¿Õ¼ä,Òò´ËÖ»Òª»ñÈ¡µ½server{} cscf£¬¾ÍÄÜÍ¬Ê±»ñÈ¡µ½ctx->loc_conf

    //Ò²¾ÍÊÇ¸¸¼¶ÉÏÏÂÎÄctxµÄmain_conf[]£¬¼ûÇ°Ãæctx->main_conf = http_ctx->main_conf;
    cmcf = ctx->main_conf[ngx_http_core_module.ctx_index]; //serverÐÐµÄÊ±ºò´´½¨µÄmain_conf[]Ö¸ÕëÊµ¼ÊÉÏÖ¸ÏòµÄÊÇÉÏ¼¶httpÐÐµÄÊ±ºò´´½¨µÄmain_confÖ¸Õë¿Õ¼ä£¬http_ctx->main_conf;

    cscfp = ngx_array_push(&cmcf->servers); //´Ó¸¸¼¶µÄmain_confÖÐµÄservers arrayÊý×éÖÐ»ñÈ¡Ò»¸öelets³ÉÔ±£¬ÓÃÀ´±£´æ±¾serverÖÐµÄserv_conf
    if (cscfp == NULL) {
        return NGX_CONF_ERROR;
    }
    //server{}ÅäÖÃµÄctx->srv_conf´æ´¢ÔÚ¸¸¼¶http{}ctx¶ÔÓ¦µÄctx->main_conf[ngx_http_core_module.ctx_index]->serversÖÐ£¬Í¨¹ýÕâ¸ösrv_conf[]->ctx¾ÍÄÜ»ñÈ¡µ½server{}µÄÉÏÏÂÎÄctx
    *cscfp = cscf;//Õâ¸öºÍÉÏÃæµÄngx_array_push£¬°ÑÔÚserver{}ÐÐ´´½¨µÄngx_http_conf_ctx_t¿Õ¼ä±£´æµ½ÁË¸¸¼¶httpÀïÃæµÄserverÖÐ

    /* parse inside server{} */

    pcf = *cf; //±£´æserver{}ÖÐµÄcf
    cf->ctx = ctx; //Ö¸ÏòÎªÐÂµÄserver{}¿ª±ÙµÄngx_http_conf_ctx_t,ÓÃÓÚÔÚ½âÎölocation{}ÀïÃæµÄÅäÖÃµÄÊ±ºòÓÃ,
    cf->cmd_type = NGX_HTTP_SRV_CONF; 

    //ÕâÀïµÄcfÊÇÎªserver{}¿ª±ÙµÄ¿Õ¼äÐÅÏ¢£¬ÓÃÀ´´æ´¢server{}ÖÐµÄÏà¹Ø½âÎöÏî£¬ºÍº¯Êý²ÎÊýcf->ctxÖÐµÄ¿Õ¼ä²»Ò»Ñù£¬²ÎÊýcf->ctx¿Õ¼äÊÇhttp{}´æ´¢¿Õ¼ä
    rv = ngx_conf_parse(cf, NULL); 

    *cf = pcf; //»Ö¸´µ½Ö®Ç°server{}ÖÐµÄ

    if (rv == NGX_CONF_OK && !cscf->listen) { //Èç¹ûÃ»ÓÐÍ¨¹ý"listen portnum"ÉèÖÃ¸Ãserver¼àÌýµÄ¶Ë¿Ú£¬ÔòÊ¹ÓÃÏÂÃæÄ¬ÈÏµÄ¶Ë¿Ú
        ngx_memzero(&lsopt, sizeof(ngx_http_listen_opt_t));

        sin = &lsopt.u.sockaddr_in;

        sin->sin_family = AF_INET;
#if (NGX_WIN32)
        sin->sin_port = htons(80);
#else
//Èç¹ûÔÚserver{£©¿éÄÚÃ»ÓÐ½âÎöµ½listenÅäÖÃÏî£¬ÔòÒâÎ¶×Åµ±Ç°µÄserverÐéÄâÖ÷»ú²¢Ã»ÓÐ¼àÌýTCP¶Ë¿Ú£¬Õâ²»·ûºÏHTTP¿ò¼ÜµÄÉè¼ÆÔ­Ôò¡£ÓÚÊÇ½«¿ªÊ¼¼àÌýÄ¬ÈÏ¶Ë¿Ú80£¬Êµ¼ÊÉÏ£¬
//Èç¹ûµ±Ç°½ø³ÌÃ»ÓÐÈ¨ÏÞ¼àÌý1024ÒÔÏÂµÄ¶Ë¿Ú£¬Ôò»á¸ÄÎª¼àÌý8000¶Ë¿Ú
        sin->sin_port = htons((getuid() == 0) ? 80 : 8000);
#endif
        sin->sin_addr.s_addr = INADDR_ANY;

        lsopt.socklen = sizeof(struct sockaddr_in);

        lsopt.backlog = NGX_LISTEN_BACKLOG;
        lsopt.rcvbuf = -1;
        lsopt.sndbuf = -1;
#if (NGX_HAVE_SETFIB)
        lsopt.setfib = -1;
#endif
#if (NGX_HAVE_TCP_FASTOPEN)
        lsopt.fastopen = -1;
#endif
        lsopt.wildcard = 1;

        (void) ngx_sock_ntop(&lsopt.u.sockaddr, lsopt.socklen, lsopt.addr,
                             NGX_SOCKADDR_STRLEN, 1);

        if (ngx_http_add_listen(cf, cscf, &lsopt) != NGX_OK) { //Èç¹ûÃ»ÓÐÅäÖÃlistenµÄÊ±ºòÊ¹ÓÃÉÏÃæµÄÄ¬ÈÏÅäÖÃ
            return NGX_CONF_ERROR;
        }
    }

    return rv;
}
/*
Nginx LocationÅäÖÃ×Ü½á(2012-03-09 21:49:25)×ªÔØ¨‹±êÇ©£º nginxlocationÅäÖÃrewriteÔÓÌ¸ ·ÖÀà£º ³ÌÐòÉè¼Æ»ýÀÛ  
Óï·¨¹æÔò£º location [=|~|~*|^~] /uri/ { ¡­ }
= ¿ªÍ·±íÊ¾¾«È·Æ¥Åä
^~ ¿ªÍ·±íÊ¾uriÒÔÄ³¸ö³£¹æ×Ö·û´®¿ªÍ·£¬Àí½âÎªÆ¥Åä urlÂ·¾¶¼´¿É¡£nginx²»¶Ôurl×ö±àÂë£¬Òò´ËÇëÇóÎª/static/20%/aa£¬¿ÉÒÔ±»¹æÔò^~ /static/ /aaÆ¥Åäµ½£¨×¢ÒâÊÇ¿Õ¸ñ£©¡£
~ ¿ªÍ·±íÊ¾Çø·Ö´óÐ¡Ð´µÄÕýÔòÆ¥Åä
~*  ¿ªÍ·±íÊ¾²»Çø·Ö´óÐ¡Ð´µÄÕýÔòÆ¥Åä
!~ºÍ!~*·Ö±ðÎªÇø·Ö´óÐ¡Ð´²»Æ¥Åä¼°²»Çø·Ö´óÐ¡Ð´²»Æ¥Åä µÄÕýÔò
/ Í¨ÓÃÆ¥Åä£¬ÈÎºÎÇëÇó¶¼»áÆ¥Åäµ½¡£
¶à¸ölocationÅäÖÃµÄÇé¿öÏÂÆ¥ÅäË³ÐòÎª£¨²Î¿¼×ÊÁÏ¶øÀ´£¬»¹Î´Êµ¼ÊÑéÖ¤£¬ÊÔÊÔ¾ÍÖªµÀÁË£¬²»±Ø¾ÐÄà£¬½ö¹©²Î¿¼£©:
Ê×ÏÈÆ¥Åä =£¬Æä´ÎÆ¥Åä^~, Æä´ÎÊÇ°´ÎÄ¼þÖÐË³ÐòµÄÕýÔòÆ¥Åä£¬×îºóÊÇ½»¸ø / Í¨ÓÃÆ¥Åä¡£µ±ÓÐÆ¥Åä³É¹¦Ê±ºò£¬Í£Ö¹Æ¥Åä£¬°´µ±Ç°Æ¥Åä¹æÔò´¦ÀíÇëÇó¡£
Àý×Ó£¬ÓÐÈçÏÂÆ¥Åä¹æÔò£º
location = / {
   #¹æÔòA
}
location = /login {
   #¹æÔòB
}
location ^~ /static/ {
   #¹æÔòC
}
location ~ \.(gif|jpg|png|js|css)$ {
   #¹æÔòD
}
location ~* \.png$ {
   #¹æÔòE
}
location !~ \.xhtml$ {
   #¹æÔòF
}
location !~* \.xhtml$ {
   #¹æÔòG
}
location / {
   #¹æÔòH
}
ÄÇÃ´²úÉúµÄÐ§¹ûÈçÏÂ£º
·ÃÎÊ¸ùÄ¿Â¼/£¬ ±ÈÈçhttp://localhost/ ½«Æ¥Åä¹æÔòA
·ÃÎÊ http://localhost/login ½«Æ¥Åä¹æÔòB£¬http://localhost/register ÔòÆ¥Åä¹æÔòH
·ÃÎÊ http://localhost/static/a.html ½«Æ¥Åä¹æÔòC
·ÃÎÊ http://localhost/a.gif, http://localhost/b.jpg ½«Æ¥Åä¹æÔòDºÍ¹æÔòE£¬µ«ÊÇ¹æÔòDË³ÐòÓÅÏÈ£¬¹æÔòE²»Æð×÷ÓÃ£¬¶ø http://localhost/static/c.png ÔòÓÅÏÈÆ¥Åäµ½¹æÔòC
·ÃÎÊ http://localhost/a.PNG ÔòÆ¥Åä¹æÔòE£¬¶ø²»»áÆ¥Åä¹æÔòD£¬ÒòÎª¹æÔòE²»Çø·Ö´óÐ¡Ð´¡£
·ÃÎÊ http://localhost/a.xhtml ²»»áÆ¥Åä¹æÔòFºÍ¹æÔòG£¬http://localhost/a.XHTML²»»áÆ¥Åä¹æÔòG£¬ÒòÎª²»Çø·Ö´óÐ¡Ð´¡£¹æÔòF£¬¹æÔòGÊôÓÚÅÅ³ý·¨£¬·ûºÏÆ¥Åä¹æÔòµ«ÊÇ²»»áÆ¥Åäµ½£¬ËùÒÔÏëÏë¿´Êµ¼ÊÓ¦ÓÃÖÐÄÄÀï»áÓÃµ½¡£
·ÃÎÊ http://localhost/category/id/1111 Ôò×îÖÕÆ¥Åäµ½¹æÔòH£¬ÒòÎªÒÔÉÏ¹æÔò¶¼²»Æ¥Åä£¬Õâ¸öÊ±ºòÓ¦¸ÃÊÇnginx×ª·¢ÇëÇó¸øºó¶ËÓ¦ÓÃ·þÎñÆ÷£¬±ÈÈçFastCGI£¨php£©£¬tomcat£¨jsp£©£¬nginx×÷Îª·½Ïò´úÀí·þÎñÆ÷´æÔÚ¡£


ËùÒÔÊµ¼ÊÊ¹ÓÃÖÐ£¬¸öÈË¾õµÃÖÁÉÙÓÐÈý¸öÆ¥Åä¹æÔò¶¨Òå£¬ÈçÏÂ£º
#Ö±½ÓÆ¥ÅäÍøÕ¾¸ù£¬Í¨¹ýÓòÃû·ÃÎÊÍøÕ¾Ê×Ò³±È½ÏÆµ·±£¬Ê¹ÓÃÕâ¸ö»á¼ÓËÙ´¦Àí£¬¹ÙÍøÈçÊÇËµ¡£
#ÕâÀïÊÇÖ±½Ó×ª·¢¸øºó¶ËÓ¦ÓÃ·þÎñÆ÷ÁË£¬Ò²¿ÉÒÔÊÇÒ»¸ö¾²Ì¬Ê×Ò³
# µÚÒ»¸ö±ØÑ¡¹æÔò
location = / {
    proxy_pass http://tomcat:8080/index
}
# µÚ¶þ¸ö±ØÑ¡¹æÔòÊÇ´¦Àí¾²Ì¬ÎÄ¼þÇëÇó£¬ÕâÊÇnginx×÷Îªhttp·þÎñÆ÷µÄÇ¿Ïî
# ÓÐÁ½ÖÖÅäÖÃÄ£Ê½£¬Ä¿Â¼Æ¥Åä»òºó×ºÆ¥Åä,ÈÎÑ¡ÆäÒ»»ò´îÅäÊ¹ÓÃ
location ^~ /static/ {
    root /webroot/static/;
}
location ~* \.(gif|jpg|jpeg|png|css|js|ico)$ {
    root /webroot/res/;
}
#µÚÈý¸ö¹æÔò¾ÍÊÇÍ¨ÓÃ¹æÔò£¬ÓÃÀ´×ª·¢¶¯Ì¬ÇëÇóµ½ºó¶ËÓ¦ÓÃ·þÎñÆ÷
#·Ç¾²Ì¬ÎÄ¼þÇëÇó¾ÍÄ¬ÈÏÊÇ¶¯Ì¬ÇëÇó£¬×Ô¼º¸ù¾ÝÊµ¼Ê°ÑÎÕ
#±Ï¾¹Ä¿Ç°µÄÒ»Ð©¿ò¼ÜµÄÁ÷ÐÐ£¬´ø.php,.jspºó×ºµÄÇé¿öºÜÉÙÁË
location / {
    proxy_pass http://tomcat:8080/
}

Èý¡¢ReWriteÓï·¨
last ¨C »ù±¾ÉÏ¶¼ÓÃÕâ¸öFlag¡£
break ¨C ÖÐÖ¹Rewirte£¬²»ÔÚ¼ÌÐøÆ¥Åä
redirect ¨C ·µ»ØÁÙÊ±ÖØ¶¨ÏòµÄHTTP×´Ì¬302
permanent ¨C ·µ»ØÓÀ¾ÃÖØ¶¨ÏòµÄHTTP×´Ì¬301
1¡¢ÏÂÃæÊÇ¿ÉÒÔÓÃÀ´ÅÐ¶ÏµÄ±í´ïÊ½£º
-fºÍ!-fÓÃÀ´ÅÐ¶ÏÊÇ·ñ´æÔÚÎÄ¼þ
-dºÍ!-dÓÃÀ´ÅÐ¶ÏÊÇ·ñ´æÔÚÄ¿Â¼
-eºÍ!-eÓÃÀ´ÅÐ¶ÏÊÇ·ñ´æÔÚÎÄ¼þ»òÄ¿Â¼
-xºÍ!-xÓÃÀ´ÅÐ¶ÏÎÄ¼þÊÇ·ñ¿ÉÖ´ÐÐ
2¡¢ÏÂÃæÊÇ¿ÉÒÔÓÃ×÷ÅÐ¶ÏµÄÈ«¾Ö±äÁ¿
Àý£ºhttp://localhost:88/test1/test2/test.php
$host£ºlocalhost
$server_port£º88
$request_uri£ºhttp://localhost:88/test1/test2/test.php
$document_uri£º/test1/test2/test.php
$document_root£ºD:\nginx/html
$request_filename£ºD:\nginx/html/test1/test2/test.php
ËÄ¡¢RedirectÓï·¨
server {
listen 80;
server_name start.igrow.cn;
index index.html index.php;
root html;
if ($http_host !~ ¡°^star\.igrow\.cn$&quot {
rewrite ^(.*) http://star.igrow.cn$1 redirect;
}
}
Îå¡¢·ÀµÁÁ´
location ~* \.(gif|jpg|swf)$ {
valid_referers none blocked start.igrow.cn sta.igrow.cn;
if ($invalid_referer) {
rewrite ^/ http://$host/logo.png;
}
}
Áù¡¢¸ù¾ÝÎÄ¼þÀàÐÍÉèÖÃ¹ýÆÚÊ±¼ä
location ~* \.(js|css|jpg|jpeg|gif|png|swf)$ {
if (-f $request_filename) {
expires 1h;
break;
}
}
Æß¡¢½ûÖ¹·ÃÎÊÄ³¸öÄ¿Â¼
location ~* \.(txt|doc)${
root /data/www/wwwroot/linuxtone/test;
deny all;
}
Ò»Ð©¿ÉÓÃµÄÈ«¾Ö±äÁ¿:
$args
$content_length
$content_type
$document_root
$document_uri
$host
$http_user_agent
$http_cookie
$limit_rate
$request_body_file
$request_method
$remote_addr
$remote_port
$remote_user
$request_filename
$request_uri
$query_string
$scheme
$server_protocol
$server_addr
$server_name
$server_port
$uri

*/

/*
1¡¢nginxÅäÖÃ»ù´¡

1¡¢ÕýÔò±í´ïÊ½Æ¥Åä

~ Çø·Ö´óÐ¡Ð´Æ¥Åä

~* ²»Çø·Ö´óÐ¡Ð´Æ¥Åä

!~ºÍ!~*·Ö±ðÎªÇø·Ö´óÐ¡Ð´²»Æ¥Åä¼°²»Çø·Ö´óÐ¡Ð´²»Æ¥Åä

^ ÒÔÊ²Ã´¿ªÍ·µÄÆ¥Åä

$ ÒÔÊ²Ã´½áÎ²µÄÆ¥Åä

×ªÒå×Ö·û¡£¿ÉÒÔ×ª. * ?µÈ

* ´ú±íÈÎÒâ×Ö·û

2¡¢ÎÄ¼þ¼°Ä¿Â¼Æ¥Åä

-fºÍ!-fÓÃÀ´ÅÐ¶ÏÊÇ·ñ´æÔÚÎÄ¼þ

-dºÍ!-dÓÃÀ´ÅÐ¶ÏÊÇ·ñ´æÔÚÄ¿Â¼

-eºÍ!-eÓÃÀ´ÅÐ¶ÏÊÇ·ñ´æÔÚÎÄ¼þ»òÄ¿Â¼

-xºÍ!-xÓÃÀ´ÅÐ¶ÏÎÄ¼þÊÇ·ñ¿ÉÖ´ÐÐ

Àý:

location = /

#Æ¥ÅäÈÎºÎ²éÑ¯£¬ÒòÎªËùÓÐÇëÇó¶¼ÒÑ / ¿ªÍ·¡£µ«ÊÇÕýÔò±í´ïÊ½¹æÔòºÍ³¤µÄ¿é¹æÔò½«±»ÓÅÏÈºÍ²éÑ¯Æ¥Åä

location ^~ /images/ {

# Æ¥ÅäÈÎºÎÒÑ/images/¿ªÍ·µÄÈÎºÎ²éÑ¯²¢ÇÒÍ£Ö¹ËÑË÷¡£ÈÎºÎÕýÔò±í´ïÊ½½«²»»á±»²âÊÔ¡£

location ~* .(gif|jpg|jpeg)$ {

# Æ¥ÅäÈÎºÎÒÑ.gif¡¢.jpg »ò .jpeg ½áÎ²µÄÇëÇó

ÈëÃÅ

1¡¢ifÖ¸Áî
ËùÓÐµÄNginxÄÚÖÃ±äÁ¿¶¼¿ÉÒÔÍ¨¹ýifÖ¸ÁîºÍÕýÔò±í´ïÊ½À´½øÐÐÆ¥Åä£¬²¢ÇÒ¸ù¾ÝÆ¥Åä½á¹û½øÐÐÒ»Ð©²Ù×÷£¬ÈçÏÂ£º

 ´úÂëÈçÏÂ ¸´ÖÆ´úÂë 
if ($http_user_agent ~ MSIE) {
  rewrite  ^(.*)$  /msie/$1  break;
}
 
if ($http_cookie ~* "id=([^;] +)(?:;|$)" ) {
  set  $id  $1;
} 

Ê¹ÓÃ·ûºÅ~*ºÍ~Ä£Ê½Æ¥ÅäµÄÕýÔò±í´ïÊ½£º

1.~ÎªÇø·Ö´óÐ¡Ð´µÄÆ¥Åä¡£
2.~*²»Çø·Ö´óÐ¡Ð´µÄÆ¥Åä£¨Æ¥ÅäfirefoxµÄÕýÔòÍ¬Ê±Æ¥ÅäFireFox£©¡£
3.!~ºÍ!~*ÒâÎª¡°²»Æ¥ÅäµÄ¡±¡£
NginxÔÚºÜ¶àÄ£¿éÖÐ¶¼ÓÐÄÚÖÃµÄ±äÁ¿£¬³£ÓÃµÄÄÚÖÃ±äÁ¿ÔÚHTTPºËÐÄÄ£¿éÖÐ£¬ÕâÐ©±äÁ¿¶¼¿ÉÒÔÊ¹ÓÃÕýÔò±í´ïÊ½½øÐÐÆ¥Åä¡£

2¡¢¿ÉÒÔÍ¨¹ýÕýÔò±í´ïÊ½Æ¥ÅäµÄÖ¸Áî
location
²é¿´Î¬»ù£ºlocation
¿ÉÄÜÕâ¸öÖ¸ÁîÊÇÎÒÃÇÆ½Ê±Ê¹ÓÃÕýÔòÆ¥ÅäÓÃµÄ×î¶àµÄÖ¸Áî£º

 ´úÂëÈçÏÂ ¸´ÖÆ´úÂë 
location ~ .*.php?$ {
        fastcgi_pass   127.0.0.1:9000;
        fastcgi_index  index.php;
        fastcgi_param  SCRIPT_FILENAME  /data/wwwsite/test.com/$fastcgi_script_name;
        include        fcgi.conf;
    }
 

¼¸ºõÃ¿¸ö»ùÓÚLEMPµÄÖ÷»ú¶¼»áÓÐÈçÉÏÒ»¶Î´úÂë¡£ËûµÄÆ¥Åä¹æÔòÀàËÆÓÚifÖ¸Áî£¬²»¹ýËû¶àÁËÈý¸ö±êÊ¶·û£¬^~¡¢=¡¢@¡£²¢

ÇÒËüÃ»ÓÐÈ¡·´ÔËËã·û!£¬ÕâÈý¸ö±êÊ¶·ûµÄ×÷ÓÃ·Ö±ðÊÇ£º

1.^~ ±êÊ¶·ûºóÃæ¸úÒ»¸ö×Ö·û´®¡£Nginx½«ÔÚÕâ¸ö×Ö·û´®Æ¥ÅäºóÍ£Ö¹½øÐÐÕýÔò±í´ïÊ½µÄÆ¥Åä£¨locationÖ¸ÁîÖÐÕýÔò±í´ï

Ê½µÄÆ¥ÅäµÄ½á¹ûÓÅÏÈÊ¹ÓÃ£©£¬Èç£ºlocation ^~ /images/£¬ÄãÏ£Íû¶Ô/images/Õâ¸öÄ¿Â¼½øÐÐÒ»Ð©ÌØ±ðµÄ²Ù×÷£¬ÈçÔö¼Ó

expiresÍ·£¬·ÀµÁÁ´µÈ£¬µ«ÊÇÄãÓÖÏë°Ñ³ýÁËÕâ¸öÄ¿Â¼µÄÍ¼Æ¬ÍâµÄËùÓÐÍ¼Æ¬Ö»½øÐÐÔö¼ÓexpiresÍ·µÄ²Ù×÷£¬Õâ¸ö²Ù×÷¿ÉÄÜ

»áÓÃµ½ÁíÍâÒ»¸ölocation£¬ÀýÈç£ºlocation ~* .(gif|jpg|jpeg)$£¬ÕâÑù£¬Èç¹ûÓÐÇëÇó/images/1.jpg£¬nginxÈçºÎ¾ö

¶¨È¥½øÐÐÄÄ¸ölocationÖÐµÄ²Ù×÷ÄØ£¿½á¹ûÈ¡¾öÓÚ±êÊ¶·û^~£¬Èç¹ûÄãÕâÑùÐ´£ºlocation /images/£¬ÕâÑùnginx»á½«1.jpg

Æ¥Åäµ½location ~* .(gif|jpg|jpeg)$Õâ¸ölocationÖÐ£¬Õâ²¢²»ÊÇÄãÐèÒªµÄ½á¹û£¬¶øÔö¼ÓÁË^~Õâ¸ö±êÊ¶·ûºó£¬ËüÔÚÆ¥

ÅäÁË/images/Õâ¸ö×Ö·û´®ºó¾ÍÍ£Ö¹ËÑË÷ÆäËü´øÕýÔòµÄlocation¡£
2.= ±íÊ¾¾«È·µÄ²éÕÒµØÖ·£¬Èçlocation = /ËüÖ»»áÆ¥ÅäuriÎª/µÄÇëÇó£¬Èç¹ûÇëÇóÎª/index.html£¬½«²éÕÒÁíÍâµÄ

location£¬¶ø²»»áÆ¥ÅäÕâ¸ö£¬µ±È»¿ÉÒÔÐ´Á½¸ölocation£¬location = /ºÍlocation /£¬ÕâÑù/index.html½«Æ¥Åäµ½ºóÕß

£¬Èç¹ûÄãµÄÕ¾µã¶Ô/µÄÇëÇóÁ¿½Ï´ó£¬¿ÉÒÔÊ¹ÓÃÕâ¸ö·½·¨À´¼Ó¿ìÇëÇóµÄÏìÓ¦ËÙ¶È¡£
3.@ ±íÊ¾ÎªÒ»¸ölocation½øÐÐÃüÃû£¬¼´×Ô¶¨ÒåÒ»¸ölocation£¬Õâ¸ölocation²»ÄÜ±»Íâ½çËù·ÃÎÊ£¬Ö»ÄÜÓÃÓÚNginx²úÉúµÄ

×ÓÇëÇó£¬Ö÷ÒªÎªerror_pageºÍtry_files¡£
×¢Òâ£¬Õâ3¸ö±êÊ¶·ûºóÃæ²»ÄÜ¸úÕýÔò±í´ïÊ½£¬ËäÈ»ÅäÖÃÎÄ¼þ¼ì²é»áÍ¨¹ý£¬¶øÇÒÃ»ÓÐÈÎºÎ¾¯¸æ£¬µ«ÊÇËûÃÇ²¢²»»á½øÐÐÆ¥Åä

¡£
×ÛÉÏËùÊö£¬locationÖ¸Áî¶ÔÓÚºóÃæÖµµÄÆ¥ÅäË³ÐòÎª£º

1.±êÊ¶·û¡°=¡±µÄlocation»á×îÏÈ½øÐÐÆ¥Åä£¬Èç¹ûÇëÇóuriÆ¥ÅäÕâ¸ölocation£¬½«¶ÔÇëÇóÊ¹ÓÃÕâ¸ölocationµÄÅäÖÃ¡£
2.½øÐÐ×Ö·û´®Æ¥Åä£¬Èç¹ûÆ¥Åäµ½µÄlocationÓÐ^~Õâ¸ö±êÊ¶·û£¬Æ¥ÅäÍ£Ö¹·µ»ØÕâ¸ölocationµÄÅäÖÃ¡£
3.°´ÕÕÅäÖÃÎÄ¼þÖÐ¶¨ÒåµÄË³Ðò½øÐÐÕýÔò±í´ïÊ½Æ¥Åä¡£×îÔçÆ¥ÅäµÄlocation½«·µ»ØÀïÃæµÄÅäÖÃ¡£
4.Èç¹ûÕýÔò±í´ïÊ½ÄÜ¹»Æ¥Åäµ½ÇëÇóµÄuri£¬½«Ê¹ÓÃÕâ¸öÕýÔò¶ÔÓ¦µÄlocation£¬Èç¹ûÃ»ÓÐ£¬ÔòÊ¹ÓÃµÚ¶þÌõÆ¥ÅäµÄ½á¹û¡£
server_name
²é¿´Î¬»ù£ºserver_name
server_nameÓÃÓÚÅäÖÃ»ùÓÚÓòÃû»òIPµÄÐéÄâÖ÷»ú£¬Õâ¸öÖ¸ÁîÒ²ÊÇ¿ÉÒÔÊ¹ÓÃÕýÔò±í´ïÊ½µÄ£¬µ«ÊÇ×¢Òâ£¬Õâ¸öÖ¸ÁîÖÐµÄÕýÔò

±í´ïÊ½²»ÓÃ´øÈÎºÎµÄ±êÊ¶·û£¬µ«ÊÇ±ØÐëÒÔ~¿ªÍ·£º

 ´úÂëÈçÏÂ ¸´ÖÆ´úÂë 
server {
  server_name   www.example.com   ~^wwwd+.example.com$;
} 

server_nameÖ¸ÁîÖÐµÄÕýÔò±í´ïÊ½¿ÉÒÔÊ¹ÓÃÒýÓÃ£¬¸ß¼¶µÄÓ¦ÓÃ¿ÉÒÔ²é¿´ÕâÆªÎÄÕÂ£ºÔÚserver_nameÖÐÊ¹ÓÃÕýÔò±í´ïÊ½

fastcgi_split_path_info
²é¿´Î¬»ù£ºfastcgi_split_path_info
Õâ¸öÖ¸Áî°´ÕÕCGI±ê×¼À´ÉèÖÃSCRIPT_FILENAME (SCRIPT_NAME)ºÍPATH_INFO±äÁ¿£¬ËüÊÇÒ»¸ö±»·Ö¸î³ÉÁ½²¿·Ö£¨Á½¸öÒýÓÃ

£©µÄÕýÔò±í´ïÊ½¡£ÈçÏÂ£º

 

 ´úÂëÈçÏÂ ¸´ÖÆ´úÂë 
location ~ ^.+.p(www.111cn.net)hp {
  (...)
  fastcgi_split_path_info ^(.+.php)(.*)$;
  fastcgi_param SCRIPT_FILENAME /path/to/php$fastcgi_script_name;
  fastcgi_param PATH_INFO $fastcgi_path_info;
  fastcgi_param PATH_TRANSLATED $document_root$fastcgi_path_info;
  (...)
} 

µÚÒ»¸öÒýÓÃ£¨.+.php£©¼ÓÉÏ/path/to/php½«×÷ÎªSCRIPT_FILENAME£¬µÚ¶þ¸öÒýÓÃ(.*)ÎªPATH_INFO£¬ÀýÈçÇëÇóµÄÍêÕû

URIÎªshow.php/article/0001£¬ÔòÉÏÀýÖÐSCRIPT_FILENAMEµÄÖµÎª/path/to/php/show.php£¬PATH_INFOÔò

Îª/article/0001¡£
Õâ¸öÖ¸ÁîÍ¨³£ÓÃÓÚÒ»Ð©Í¨¹ýPATH_INFOÃÀ»¯URIµÄ¿ò¼Ü£¨ÀýÈçCodeIgniter£©¡£

gzip_disable
²é¿´Î¬»ù£ºgzip_disable
Í¨¹ýÕýÔò±í´ïÊ½À´Ö¸¶¨ÔÚÄÄÐ©ä¯ÀÀÆ÷ÖÐ½ûÓÃgzipÑ¹Ëõ¡£


gzip_disable     "msie6";rewrite
²é¿´Î¬»ù£ºrewrite
Õâ¸öÖ¸ÁîÓ¦¸ÃÒ²ÊÇÓÃµÄ±È½Ï¶àµÄ£¬ËüÐèÒªÊ¹ÓÃÍêÕûµÄ°üº¬ÒýÓÃµÄÕýÔò±í´ïÊ½£º

 ´úÂëÈçÏÂ ¸´ÖÆ´úÂë 
rewrite  "/photos/([0-9] {2})([0-9] {2})([0-9] {2})" /path/to/photos/$1/$1$2/$1$2$3.png;Í¨³£»·¾³ÏÂÎÒÃÇ
 

»á°ÑËüºÍif½áºÏÀ´Ê¹ÓÃ£º

 ´úÂëÈçÏÂ ¸´ÖÆ´úÂë 
if ($host ~* www.(.*)) {
  set $host_without_www $1;
  rewrite ^(.*)$ http://$host_without_www$1 permanent; # $1Îª'/foo'£¬¶ø²»ÊÇ'www.mydomain.com/foo'
}
 

NginxÖÐµÄÕýÔòÈçºÎÆ¥ÅäÖÐÎÄ
Ê×ÏÈÈ·¶¨ÔÚ±àÒëpcreÊ±¼ÓÁËenable-utf8²ÎÊý£¬Èç¹ûÃ»ÓÐ£¬ÇëÖØÐÂ±àÒëpcre£¬È»ºó¾Í¿ÉÒÔÔÚNginxµÄÅäÖÃÎÄ¼þÖÐÊ¹ÓÃÕâ

ÑùµÄÕýÔò£º¡±(*UTF8)^/[x{4e00}-x{9fbf}]+)$¡±×¢ÒâÒýºÅºÍÇ°ÃæµÄ(*UTF8)£¬(*UTF8)½«¸æËßÕâ¸öÕýÔòÇÐ»»ÎªUTF8Ä£

Ê½¡£

 ´úÂëÈçÏÂ ¸´ÖÆ´úÂë 
[root@backup conf]# pcretest
PCRE version 8.10 2010-06-25

  re> /^[x{4e00}-x{9fbf}]+/8
data> ²âÊÔ
 0: x{6d4b}x{8bd5}
data> NginxÄ£¿é²Î¿¼ÊÖ²áÖÐÎÄ°æ
No match
data> ²Î¿¼ÊÖ²áÖÐÎÄ°æ
 0: x{53c2}x{8003}x{624b}x{518c}x{4e2d}x{6587}x{7248}
 

locationË³Ðò´íÎóµ¼ÖÂÏÂÔØ.phpÔ´Âë¶ø²»Ö´ÐÐphp³ÌÐòµÄÎÊÌâ

¿´ÏÂÃæµÄÀý×ÓÆ¬¶Ï(server¶Î¡¢wordpress°²×°µ½¶à¸öÄ¿Â¼)£º 
=====================================

 ´úÂëÈçÏÂ ¸´ÖÆ´úÂë 
location / { 
        try_files $uri $uri/ /index.html; 
}

location /user1/ { 
      try_files $uri $uri/ /user1/index.php?q=$uri&$args; 
}

location ~* ^/(user2|user3)/ { 
        try_files $uri $uri/ /$1/index.php?q=$uri&$args; 
}

location ~ .php$ { 
        fastcgi_pass 127.0.0.1:9000; 
        fastcgi_index index.php; 
        include fastcgi_params; 
}
 

=====================================

nginx.confµÄÅäÖÃ´úÂë¿´ÉÏÈ¥Ã»ÓÐÈÎºÎÎÊÌâ£¬¶øÊÂÊµÉÏ£º 
·ÃÎÊ /user1/»áÕý³£Ö´ÐÐphp³ÌÐò¡£ 
·ÃÎÊ /user2/ »ò /user3/ ¶¼²»»áÖ´ÐÐ³ÌÐò£¬¶øÊÇÖ±½ÓÏÂÔØ³ÌÐòµÄÔ´´úÂë¡£

Ô­ÒòÔÚÄÄÀï£¿¿´µ½ËûÃÇµØÇø±ðÁËÂð£¿ 
/user1/ÊÇÆÕÍ¨locationÐ´·¨ 
¶ø/user2/ »ò /user3/ ÊÇÕýÔò±í´ïÊ½Æ¥ÅäµÄlocation

ÎÊÌâ¾Í³öÔÚÁË/user2/ »ò /user3/Æ¥ÅälocationÖ¸ÁîÊ¹ÓÃÁËÕýÔò±í´ïÊ½£¬ËùÒÔ±ØÐë×¢Òâ´úÂë¶ÎµÄÏÈºóË³Ðò£¬±ØÐë°Ñ

location ~ .php$ {...}¶ÎÉÏÒÆ¡¢·Åµ½ËüµÄÇ°ÃæÈ¥¡£

ÕýÈ·µÄ´úÂë¾ÙÀý£º 
=====================================

 ´úÂëÈçÏÂ ¸´ÖÆ´úÂë 
location / { 
        try_files $uri $uri/ /index.html; 
}

location /user1/ { 
      try_files $uri $uri/ /user1/index.php?q=$uri&$args; 
}

location ~ .php$ { 
        fastcgi_pass 127.0.0.1:9000; 
        fastcgi_index index.php; 
        include fastcgi_params; 
}

location ~* ^/(user2|user3)/ { 
        try_files $uri $uri/ /$1/index.php?q=$uri&$args; 
}
 

=====================================

¡¾×¢Òâ¡¿¶ÔÓÚÆÕÍ¨locationÖ¸ÁîÐÐ£¬ÊÇÃ»ÓÐÈÎºÎË³ÐòµÄÒªÇóµÄ¡£Èç¹ûÄãÒ²Óöµ½ÁËÀàËÆµÄÎÊÌâ£¬¿ÉÒÔ³¢ÊÔµ÷ÕûÊ¹ÓÃÕýÔò

±í´ïÊ½µÄlocationÖ¸ÁîÆ¬¶ÏµÄË³ÐòÀ´µ÷ÊÔ

from:http://www.111cn.net/sys/nginx/45335.htm

*/
/*
cf¿Õ¼äÊ¼ÖÕÔÚÒ»¸öµØ·½£¬¾ÍÊÇngx_init_cycleÖÐµÄconf£¬Ê¹ÓÃÖÐÖ»ÊÇ¼òµ¥µÄÐÞ¸ÄconfÖÐµÄctxÖ¸ÏòÒÑ¾­cmd_typeÀàÐÍ£¬È»ºóÔÚ½âÎöµ±Ç°{}ºó£¬ÖØÐÂ»Ö¸´½âÎöµ±Ç°{}Ç°µÄÅäÖÃ
²Î¿¼"http" "server" "location"ngx_http_block  ngx_http_core_server  ngx_http_core_location  ngx_http_core_location
*/
//¼ûngx_http_core_location location{}ÅäÖÃµÄctx->loc_conf[ngx_http_core_module.ctx_index]´æ´¢ÔÚ¸¸¼¶server{}µÄctx->loc_conf[ngx_http_core_module.ctx_index]->locationsÖÐ
//¼ûngx_http_core_server server{}ÅäÖÃµÄctx->srv_conf´æ´¢ÔÚ¸¸¼¶http{}ctx¶ÔÓ¦µÄctx->main_conf[ngx_http_core_module.ctx_index]->serversÖÐ£¬Í¨¹ýÕâ¸ösrv_conf[]->ctx¾ÍÄÜ»ñÈ¡µ½server{}µÄÉÏÏÂÎÄctx
static char *
ngx_http_core_location(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{//Í¼ÐÎ»¯²Î¿¼:ÉîÈëÀí½âNGINXÖÐµÄÍ¼9-2  Í¼10-1  Í¼4-2£¬½áºÏÍ¼¿´,²¢¿ÉÒÔÅäºÏhttp://tech.uc.cn/?p=300¿´
    char                      *rv;
    u_char                    *mod;
    size_t                     len;
    ngx_str_t                 *value, *name;
    ngx_uint_t                 i;
    ngx_conf_t                 save;
    ngx_http_module_t         *module;
    ngx_http_conf_ctx_t       *ctx, 
        *pctx; //¸¸¼¶ctx
    ngx_http_core_loc_conf_t  *clcf, *pclcf;//clcfÎª±¾¼¶¶ÔÓ¦µÄloc_conf£¬¸¸¼¶pctx¶ÔÓ¦µÄloc_conf   pclcf = pctx->loc_conf[ngx_http_core_module.ctx_index];

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    pctx = cf->ctx;//ÁãÊ±´æ´¢¸¸¼¶µÄctx,Ò²¾ÍÊÇserver{}ÉÏÏÂÎÄ
    ctx->main_conf = pctx->main_conf; //Ö¸Ïò¸¸µÄmain
    ctx->srv_conf = pctx->srv_conf; //Ö¸Ïò¸¸µÄsrv //Í¼ÐÎ»¯²Î¿¼:ÉîÈëÀí½âNGINXÖÐµÄÍ¼9-2  Í¼10-1  Í¼4-2£¬½áºÏÍ¼¿´,²¢¿ÉÒÔÅäºÏhttp://tech.uc.cn/?p=300¿´

    ctx->loc_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->loc_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    for (i = 0; ngx_modules[i]; i++) {
        if (ngx_modules[i]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = ngx_modules[i]->ctx;

        if (module->create_loc_conf) {
            ctx->loc_conf[ngx_modules[i]->ctx_index] =
                                                   module->create_loc_conf(cf);
            if (ctx->loc_conf[ngx_modules[i]->ctx_index] == NULL) {
                 return NGX_CONF_ERROR;
            }
        }
    }

    clcf = ctx->loc_conf[ngx_http_core_module.ctx_index];
    clcf->loc_conf = ctx->loc_conf;//

    value = cf->args->elts;

    /*
    // »ñÈ¡ location ÐÐ½âÎö½á¹û£¬Êý×éÀàÐÍ£¬Èç£º["location", "^~", "/images/"] value = cf->args->elts;  
         ¸ù¾Ý²ÎÊý¸öÊý²»Í¬£¬À´ÅÐ¶Ï location ÀàÐÍ£¬¶Ô¶ÔÏàÓ¦×Ö¶Î¸³Öµ
         Èç¹ûÊÇÕýÔò±í´ïÊ½£¬Ôò»áµ÷ÓÃ ngx_http_core_regex_location ¶Ô re ½øÐÐ±àÒë
    */

    /*
    = ¿ªÍ·±íÊ¾¾«È·Æ¥Åä
    ^~ ¿ªÍ·±íÊ¾uriÒÔÄ³¸ö³£¹æ×Ö·û´®¿ªÍ·£¬Àí½âÎªÆ¥Åä urlÂ·¾¶¼´¿É¡£nginx²»¶Ôurl×ö±àÂë£¬Òò´ËÇëÇóÎª/static/20%/aa£¬¿ÉÒÔ±»¹æÔò^~ /static/ /aaÆ¥Åäµ½£¨×¢ÒâÊÇ¿Õ¸ñ£©¡£
    ~ ¿ªÍ·±íÊ¾Çø·Ö´óÐ¡Ð´µÄÕýÔòÆ¥Åä
    ~*  ¿ªÍ·±íÊ¾²»Çø·Ö´óÐ¡Ð´µÄÕýÔòÆ¥Åä
    !~ºÍ!~*·Ö±ðÎªÇø·Ö´óÐ¡Ð´²»Æ¥Åä¼°²»Çø·Ö´óÐ¡Ð´²»Æ¥Åä µÄÕýÔò
    / Í¨ÓÃÆ¥Åä£¬ÈÎºÎÇëÇó¶¼»áÆ¥Åäµ½¡£

    
    locationÆ¥ÅäÃüÁî
    
    ~      #²¨ÀËÏß±íÊ¾Ö´ÐÐÒ»¸öÕýÔòÆ¥Åä£¬Çø·Ö´óÐ¡Ð´
    ~*    #±íÊ¾Ö´ÐÐÒ»¸öÕýÔòÆ¥Åä£¬²»Çø·Ö´óÐ¡Ð´
    ^~    #^~±íÊ¾ÆÕÍ¨×Ö·ûÆ¥Åä£¬Èç¹û¸ÃÑ¡ÏîÆ¥Åä£¬Ö»Æ¥Åä¸ÃÑ¡Ïî£¬²»Æ¥Åä±ðµÄÑ¡Ïî£¬Ò»°ãÓÃÀ´Æ¥ÅäÄ¿Â¼
    =      #½øÐÐÆÕÍ¨×Ö·û¾«È·Æ¥Åä
    @     #"@" ¶¨ÒåÒ»¸öÃüÃûµÄ location£¬Ê¹ÓÃÔÚÄÚ²¿¶¨ÏòÊ±£¬ÀýÈç error_page, try_files
    
     
    
    location Æ¥ÅäµÄÓÅÏÈ¼¶(ÓëlocationÔÚÅäÖÃÎÄ¼þÖÐµÄË³ÐòÎÞ¹Ø)
    = ¾«È·Æ¥Åä»áµÚÒ»¸ö±»´¦Àí¡£Èç¹û·¢ÏÖ¾«È·Æ¥Åä£¬nginxÍ£Ö¹ËÑË÷ÆäËûÆ¥Åä¡£
    ÆÕÍ¨×Ö·ûÆ¥Åä£¬ÕýÔò±í´ïÊ½¹æÔòºÍ³¤µÄ¿é¹æÔò½«±»ÓÅÏÈºÍ²éÑ¯Æ¥Åä£¬Ò²¾ÍÊÇËµÈç¹û¸ÃÏîÆ¥Åä»¹ÐèÈ¥¿´ÓÐÃ»ÓÐÕýÔò±í´ïÊ½Æ¥ÅäºÍ¸ü³¤µÄÆ¥Åä¡£
    ^~ ÔòÖ»Æ¥Åä¸Ã¹æÔò£¬nginxÍ£Ö¹ËÑË÷ÆäËûÆ¥Åä£¬·ñÔònginx»á¼ÌÐø´¦ÀíÆäËûlocationÖ¸Áî¡£
    ×îºóÆ¥ÅäÀí´øÓÐ"~"ºÍ"~*"µÄÖ¸Áî£¬Èç¹ûÕÒµ½ÏàÓ¦µÄÆ¥Åä£¬ÔònginxÍ£Ö¹ËÑË÷ÆäËûÆ¥Åä£»µ±Ã»ÓÐÕýÔò±í´ïÊ½»òÕßÃ»ÓÐÕýÔò±í´ïÊ½±»Æ¥ÅäµÄÇé¿öÏÂ£¬ÄÇÃ´Æ¥Åä³Ì¶È×î¸ßµÄÖð×ÖÆ¥ÅäÖ¸Áî»á±»Ê¹ÓÃ¡£
    
    location ÓÅÏÈ¼¶¹Ù·½ÎÄµµ
    
    1.Directives with the = prefix that match the query exactly. If found, searching stops.
    2.All remaining directives with conventional strings, longest match first. If this match used the ^~ prefix, searching stops.
    3.Regular expressions, in order of definition in the configuration file.
    4.If #3 yielded a match, that result is used. Else the match from #2 is used.
    1.=Ç°×ºµÄÖ¸ÁîÑÏ¸ñÆ¥ÅäÕâ¸ö²éÑ¯¡£Èç¹ûÕÒµ½£¬Í£Ö¹ËÑË÷¡£
    2.ËùÓÐÊ£ÏÂµÄ³£¹æ×Ö·û´®£¬×î³¤µÄÆ¥Åä¡£Èç¹ûÕâ¸öÆ¥ÅäÊ¹ÓÃ^?Ç°×º£¬ËÑË÷Í£Ö¹¡£
    3.ÕýÔò±í´ïÊ½£¬ÔÚÅäÖÃÎÄ¼þÖÐ¶¨ÒåµÄË³Ðò¡£
    4.Èç¹ûµÚ3Ìõ¹æÔò²úÉúÆ¥ÅäµÄ»°£¬½á¹û±»Ê¹ÓÃ¡£·ñÔò£¬ÈçÍ¬´ÓµÚ2Ìõ¹æÔò±»Ê¹ÓÃ¡£
     
    
    ÀýÈç
    
    location  = / {
  # Ö»Æ¥Åä"/".
      [ configuration A ] 
    }
    location  / {
  # Æ¥ÅäÈÎºÎÇëÇó£¬ÒòÎªËùÓÐÇëÇó¶¼ÊÇÒÔ"/"¿ªÊ¼
  # µ«ÊÇ¸ü³¤×Ö·ûÆ¥Åä»òÕßÕýÔò±í´ïÊ½Æ¥Åä»áÓÅÏÈÆ¥Åä
      [ configuration B ] 
    }
    location ^~ /images/ {
  # Æ¥ÅäÈÎºÎÒÔ /images/ ¿ªÊ¼µÄÇëÇó£¬²¢Í£Ö¹Æ¥Åä ÆäËülocation
      [ configuration C ] 
    }
    location ~* \.(gif|jpg|jpeg)$ {
  # Æ¥ÅäÒÔ gif, jpg, or jpeg½áÎ²µÄÇëÇó. 
  # µ«ÊÇËùÓÐ /images/ Ä¿Â¼µÄÇëÇó½«ÓÉ [Configuration C]´¦Àí.   
      [ configuration D ] 
    }ÇëÇóURIÀý×Ó:
    
    ?/ -> ·ûºÏconfiguration A
    ?/documents/document.html -> ·ûºÏconfiguration B
    ?/images/1.gif -> ·ûºÏconfiguration C
    ?/documents/1.jpg ->·ûºÏ configuration D
    @location Àý×Ó
    error_page 404 = @fetch;
    
    location @fetch(
    proxy_pass http://fetch;
    )
    
    */
    if (cf->args->nelts == 3) {

        len = value[1].len;
        mod = value[1].data;
        name = &value[2];

        //location = /mytest {}
        if (len == 1 && mod[0] == '=') { //ÀàËÆ location = / {}£¬ËùÎ½×¼È·Æ¥Åä¡£
            clcf->name = *name;
            clcf->exact_match = 1;

        //location ^~ /mytest {}
        // ^~ ¿ªÍ·±íÊ¾uriÒÔÄ³¸ö³£¹æ×Ö·û´®¿ªÍ·£¬Àí½âÎªÆ¥Åä urlÂ·¾¶¼´¿É¡£nginx²»¶Ôurl×ö±àÂë£¬Òò´ËÇëÇóÎª/static/20%/aa£¬¿ÉÒÔ±»¹æÔò^~ /static/ /aaÆ¥Åäµ½£¨×¢ÒâÊÇ¿Õ¸ñ£©¡£
        } else if (len == 2 && mod[0] == '^' && mod[1] == '~') {//Ã»ÓÐÕýÔò£¬Ö¸ÀàËÆlocation ^~ /a { ... } µÄlocation¡£
            //Ç°×ºÆ¥Åä

            clcf->name = *name;
            clcf->noregex = 1;

        //location ~ /mytest {}
        } else if (len == 1 && mod[0] == '~') {//~ ¿ªÍ·±íÊ¾Çø·Ö´óÐ¡Ð´µÄÕýÔòÆ¥Åä

            if (ngx_http_core_regex_location(cf, clcf, name, 0) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

        //location ~* /mytest {}
        } else if (len == 2 && mod[0] == '~' && mod[1] == '*') {// ~*  ¿ªÍ·±íÊ¾²»Çø·Ö´óÐ¡Ð´µÄÕýÔòÆ¥Åä

            if (ngx_http_core_regex_location(cf, clcf, name, 1) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

        } else {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid location modifier \"%V\"", &value[1]);
            return NGX_CONF_ERROR;
        }

    } else {

        name = &value[1];
        //location =/mytest {} ºÍÇ°ÃæµÄÇø±ðÊÇ=ºóÃæÃ»ÓÐ¿Õ¸ñ
        if (name->data[0] == '=') {

            clcf->name.len = name->len - 1;
            clcf->name.data = name->data + 1;
            clcf->exact_match = 1;

        //location ^~/mytest {}
        } else if (name->data[0] == '^' && name->data[1] == '~') { //Ç°×ºÆ¥Åä

            clcf->name.len = name->len - 2;
            clcf->name.data = name->data + 2;
            clcf->noregex = 1;

        //location ~/mytest {}
        } else if (name->data[0] == '~') { 
 
            name->len--;
            name->data++;

            if (name->data[0] == '*') {//²»Çø·Ö´óÐ¡Ð´ÕýÔòÆ¥Åä

                name->len--;
                name->data++;

                if (ngx_http_core_regex_location(cf, clcf, name, 1) != NGX_OK) {
                    return NGX_CONF_ERROR;
                }

            } else {//Çø·Ö´óÐ¡Ð´ÕýÔòÆ¥Åä
                if (ngx_http_core_regex_location(cf, clcf, name, 0) != NGX_OK) {
                    return NGX_CONF_ERROR;
                }
            }

        } else { 
        //ngx_http_add_locationÖÐ°Ñ¾«È·Æ¥Åä ÕýÔò±í´ïÊ½ name  nonameÅäÖÃÒÔÍâµÄÆäËûÅäÖÃ¶¼Ëã×öÇ°×ºÆ¥Åä  ÀýÈç//location ^~  xxx{}      location /XXX {}
        //location /xx {}È«²¿¶¼Æ¥Åä£¬   //location @mytest {}  //location !~ mytest {}  //location !~* mytest {}
//ÒÔ¡¯@¡¯¿ªÍ·µÄ£¬Èçlocation @test {}
// @  ±íÊ¾ÎªÒ»¸ölocation½øÐÐÃüÃû£¬¼´×Ô¶¨ÒåÒ»¸ölocation£¬Õâ¸ölocation²»ÄÜ±»Íâ½çËù·ÃÎÊ£¬Ö»ÄÜÓÃÓÚNginx²úÉúµÄ×ÓÇëÇó£¬Ö÷ÒªÎªerror_pageºÍtry_files¡£      
            clcf->name = *name;

            if (name->data[0] == '@') {
                clcf->named = 1;
            }
        }
    }

    pclcf = pctx->loc_conf[ngx_http_core_module.ctx_index];

    if (pclcf->name.len) {

        /* nested location */

#if 0
        clcf->prev_location = pclcf;
#endif

        if (pclcf->exact_match) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "location \"%V\" cannot be inside "
                               "the exact location \"%V\"",
                               &clcf->name, &pclcf->name);
            return NGX_CONF_ERROR;
        }

        if (pclcf->named) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "location \"%V\" cannot be inside "
                               "the named location \"%V\"",
                               &clcf->name, &pclcf->name);
            return NGX_CONF_ERROR;
        }

        if (clcf->named) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "named location \"%V\" can be "
                               "on the server level only",
                               &clcf->name);
            return NGX_CONF_ERROR;
        }

        len = pclcf->name.len;

#if (NGX_PCRE)
        if (clcf->regex == NULL
            && ngx_filename_cmp(clcf->name.data, pclcf->name.data, len) != 0)
#else
        if (ngx_filename_cmp(clcf->name.data, pclcf->name.data, len) != 0)
#endif
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "location \"%V\" is outside location \"%V\"",
                               &clcf->name, &pclcf->name);
            return NGX_CONF_ERROR;
        }
    }

    //ctx->loc_conf[ngx_http_core_module.ctx_index]´æ´¢ÔÚ¸¸¼¶server{}µÄctx->loc_conf[ngx_http_core_module.ctx_index]->locationsÖÐ
    if (ngx_http_add_location(cf, &pclcf->locations, clcf) != NGX_OK) {  
        return NGX_CONF_ERROR;
    }

    save = *cf; //±£´æ½âÎölocation{}Ç°µÄcfÅäÖÃ
    cf->ctx = ctx; //Ö¸ÏòÎªlocation{}´´½¨µÄ¿Õ¼ä
    cf->cmd_type = NGX_HTTP_LOC_CONF;
    
    rv = ngx_conf_parse(cf, NULL);

    *cf = save; //»Ö¸´locationÖ®Ç°µÄÅäÖÃ

    return rv;
}


static ngx_int_t
ngx_http_core_regex_location(ngx_conf_t *cf, ngx_http_core_loc_conf_t *clcf,
    ngx_str_t *regex, ngx_uint_t caseless)
{
#if (NGX_PCRE)
    ngx_regex_compile_t  rc;
    u_char               errstr[NGX_MAX_CONF_ERRSTR];

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.pattern = *regex;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;

#if (NGX_HAVE_CASELESS_FILESYSTEM)
    rc.options = NGX_REGEX_CASELESS;
#else
    rc.options = caseless ? NGX_REGEX_CASELESS : 0;
#endif

    clcf->regex = ngx_http_regex_compile(cf, &rc);
    if (clcf->regex == NULL) {
        return NGX_ERROR;
    }

    clcf->name = *regex;

    return NGX_OK;

#else

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "using regex \"%V\" requires PCRE library",
                       regex);
    return NGX_ERROR;

#endif
}

//types {}ÅäÖÃngx_http_core_typeÊ×ÏÈ´æÔÚÓë¸ÃÊý×éÖÐ£¬È»ºóÔÚngx_http_core_merge_loc_conf´æÈëtypes_hashÖÐ£¬ÕæÕýÉúÐ§¼ûngx_http_set_content_type
static char *
ngx_http_core_types(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    char        *rv;
    ngx_conf_t   save;

    if (clcf->types == NULL) {
        clcf->types = ngx_array_create(cf->pool, 64, sizeof(ngx_hash_key_t));
        if (clcf->types == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    save = *cf;
    cf->handler = ngx_http_core_type;
    cf->handler_conf = conf;

    rv = ngx_conf_parse(cf, NULL);

    *cf = save;

    return rv;
}

//types {}ÅäÖÃngx_http_core_typeÊ×ÏÈ´æÔÚÓë¸ÃÊý×éÖÐ£¬È»ºóÔÚngx_http_core_merge_loc_conf´æÈëtypes_hashÖÐ£¬ÕæÕýÉúÐ§¼ûngx_http_set_content_type
static char *
ngx_http_core_type(ngx_conf_t *cf, ngx_command_t *dummy, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t       *value, *content_type, *old;
    ngx_uint_t       i, n, hash;
    ngx_hash_key_t  *type;

    value = cf->args->elts;

    if (ngx_strcmp(value[0].data, "include") == 0) { //Ç¶ÈëÆäËûÅäÖÃÎÄ¼þ
        if (cf->args->nelts != 2) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid number of arguments"
                               " in \"include\" directive");
            return NGX_CONF_ERROR;
        }

        return ngx_conf_include(cf, dummy, conf);
    }

    content_type = ngx_palloc(cf->pool, sizeof(ngx_str_t));
    if (content_type == NULL) {
        return NGX_CONF_ERROR;
    }

    *content_type = value[0];

    for (i = 1; i < cf->args->nelts; i++) {

        hash = ngx_hash_strlow(value[i].data, value[i].data, value[i].len);

        type = clcf->types->elts;
        for (n = 0; n < clcf->types->nelts; n++) {
            if (ngx_strcmp(value[i].data, type[n].key.data) == 0) {
                old = type[n].value;
                type[n].value = content_type;

                ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                                   "duplicate extension \"%V\", "
                                   "content type: \"%V\", "
                                   "previous content type: \"%V\"",
                                   &value[i], content_type, old);
                goto next;
            }
        }


        type = ngx_array_push(clcf->types);
        if (type == NULL) {
            return NGX_CONF_ERROR;
        }

        type->key = value[i];
        type->key_hash = hash;
        type->value = content_type;

    next:
        continue;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_core_preconfiguration(ngx_conf_t *cf)
{
    return ngx_http_variables_add_core_vars(cf);
}


static ngx_int_t
ngx_http_core_postconfiguration(ngx_conf_t *cf)
{
    ngx_http_top_request_body_filter = ngx_http_request_body_save_filter;

    return NGX_OK;
}

static void *
ngx_http_core_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_core_main_conf_t));
    if (cmcf == NULL) {
        return NULL;
    }

    if (ngx_array_init(&cmcf->servers, cf->pool, 4,
                       sizeof(ngx_http_core_srv_conf_t *))
        != NGX_OK)
    {
        return NULL;
    }

    cmcf->server_names_hash_max_size = NGX_CONF_UNSET_UINT;
    cmcf->server_names_hash_bucket_size = NGX_CONF_UNSET_UINT;

    cmcf->variables_hash_max_size = NGX_CONF_UNSET_UINT;
    cmcf->variables_hash_bucket_size = NGX_CONF_UNSET_UINT;

    return cmcf;
}


static char *
ngx_http_core_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_core_main_conf_t *cmcf = conf;

    ngx_conf_init_uint_value(cmcf->server_names_hash_max_size, 512);
    ngx_conf_init_uint_value(cmcf->server_names_hash_bucket_size,
                             ngx_cacheline_size);

    cmcf->server_names_hash_bucket_size =
            ngx_align(cmcf->server_names_hash_bucket_size, ngx_cacheline_size);


    ngx_conf_init_uint_value(cmcf->variables_hash_max_size, 1024);
    ngx_conf_init_uint_value(cmcf->variables_hash_bucket_size, 64);

    cmcf->variables_hash_bucket_size =
               ngx_align(cmcf->variables_hash_bucket_size, ngx_cacheline_size);

    if (cmcf->ncaptures) {
        cmcf->ncaptures = (cmcf->ncaptures + 1) * 3; //pcre_exec½øÐÐÕýÔò±í´ïÊ½Æ¥ÅäµÄÊ±ºò£¬ÐèÒªlenÐèÒªÂú×ã¸ÃÌõ¼þ£¬¼ûhttp://www.rosoo.net/a/201004/9082.html
    }

    return NGX_CONF_OK;
}


static void *
ngx_http_core_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_core_srv_conf_t  *cscf;

    cscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_core_srv_conf_t));
    if (cscf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->client_large_buffers.num = 0;
     */

    if (ngx_array_init(&cscf->server_names, cf->temp_pool, 4,
                       sizeof(ngx_http_server_name_t))
        != NGX_OK)
    {
        return NULL;
    }

    cscf->connection_pool_size = NGX_CONF_UNSET_SIZE;
    cscf->request_pool_size = NGX_CONF_UNSET_SIZE;
    cscf->client_header_timeout = NGX_CONF_UNSET_MSEC;
    cscf->client_header_buffer_size = NGX_CONF_UNSET_SIZE;
    cscf->ignore_invalid_headers = NGX_CONF_UNSET;
    cscf->merge_slashes = NGX_CONF_UNSET;
    cscf->underscores_in_headers = NGX_CONF_UNSET;

    return cscf;
}


static char *
ngx_http_core_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_core_srv_conf_t *prev = parent;
    ngx_http_core_srv_conf_t *conf = child;

    ngx_str_t                name;
    ngx_http_server_name_t  *sn;

    /* TODO: it does not merge, it inits only */

    ngx_conf_merge_size_value(conf->connection_pool_size,
                              prev->connection_pool_size, 256);
    ngx_conf_merge_size_value(conf->request_pool_size,
                              prev->request_pool_size, 4096);
    ngx_conf_merge_msec_value(conf->client_header_timeout,
                              prev->client_header_timeout, 60000);
    ngx_conf_merge_size_value(conf->client_header_buffer_size,
                              prev->client_header_buffer_size, 1024);
    ngx_conf_merge_bufs_value(conf->large_client_header_buffers,
                              prev->large_client_header_buffers,
                              4, 8192);

    if (conf->large_client_header_buffers.size < conf->connection_pool_size) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the \"large_client_header_buffers\" size must be "
                           "equal to or greater than \"connection_pool_size\"");
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_value(conf->ignore_invalid_headers,
                              prev->ignore_invalid_headers, 1);

    ngx_conf_merge_value(conf->merge_slashes, prev->merge_slashes, 1);

    ngx_conf_merge_value(conf->underscores_in_headers,
                              prev->underscores_in_headers, 0);

    if (conf->server_names.nelts == 0) {
        /* the array has 4 empty preallocated elements, so push cannot fail */
        sn = ngx_array_push(&conf->server_names);
#if (NGX_PCRE)
        sn->regex = NULL;
#endif
        sn->server = conf;
        ngx_str_set(&sn->name, "");
    }

    sn = conf->server_names.elts;
    name = sn[0].name;

#if (NGX_PCRE)
    if (sn->regex) {
        name.len++;
        name.data--;
    } else
#endif

    if (name.data[0] == '.') {
        name.len--;
        name.data++;
    }

    conf->server_name.len = name.len;
    conf->server_name.data = ngx_pstrdup(cf->pool, &name);
    if (conf->server_name.data == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static void *
ngx_http_core_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_core_loc_conf_t));
    if (clcf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     clcf->root = { 0, NULL };
     *     clcf->limit_except = 0;
     *     clcf->post_action = { 0, NULL };
     *     clcf->types = NULL;
     *     clcf->default_type = { 0, NULL };
     *     clcf->error_log = NULL;
     *     clcf->error_pages = NULL;
     *     clcf->try_files = NULL;
     *     clcf->client_body_path = NULL;
     *     clcf->regex = NULL;
     *     clcf->exact_match = 0;
     *     clcf->auto_redirect = 0;
     *     clcf->alias = 0;
     *     clcf->gzip_proxied = 0;
     *     clcf->keepalive_disable = 0;
     */

    clcf->client_max_body_size = NGX_CONF_UNSET;
    clcf->client_body_buffer_size = NGX_CONF_UNSET_SIZE;
    clcf->client_body_timeout = NGX_CONF_UNSET_MSEC;
    clcf->satisfy = NGX_CONF_UNSET_UINT;
    clcf->if_modified_since = NGX_CONF_UNSET_UINT;
    clcf->max_ranges = NGX_CONF_UNSET_UINT;
    clcf->client_body_in_file_only = NGX_CONF_UNSET_UINT;
    clcf->client_body_in_single_buffer = NGX_CONF_UNSET;
    clcf->internal = NGX_CONF_UNSET;
    clcf->sendfile = NGX_CONF_UNSET;
    clcf->sendfile_max_chunk = NGX_CONF_UNSET_SIZE;
    clcf->aio = NGX_CONF_UNSET;
#if (NGX_THREADS)
    clcf->thread_pool = NGX_CONF_UNSET_PTR;
    clcf->thread_pool_value = NGX_CONF_UNSET_PTR;
#endif
    clcf->read_ahead = NGX_CONF_UNSET_SIZE;
    clcf->directio = NGX_CONF_UNSET;
    clcf->directio_alignment = NGX_CONF_UNSET;
    clcf->tcp_nopush = NGX_CONF_UNSET;
    clcf->tcp_nodelay = NGX_CONF_UNSET;
    clcf->send_timeout = NGX_CONF_UNSET_MSEC;
    clcf->send_lowat = NGX_CONF_UNSET_SIZE;
    clcf->postpone_output = NGX_CONF_UNSET_SIZE;
    clcf->limit_rate = NGX_CONF_UNSET_SIZE;
    clcf->limit_rate_after = NGX_CONF_UNSET_SIZE;
    clcf->keepalive_timeout = NGX_CONF_UNSET_MSEC;
    clcf->keepalive_header = NGX_CONF_UNSET;
    clcf->keepalive_requests = NGX_CONF_UNSET_UINT;
    clcf->lingering_close = NGX_CONF_UNSET_UINT;
    clcf->lingering_time = NGX_CONF_UNSET_MSEC;
    clcf->lingering_timeout = NGX_CONF_UNSET_MSEC;
    clcf->resolver_timeout = NGX_CONF_UNSET_MSEC;
    clcf->reset_timedout_connection = NGX_CONF_UNSET;
    clcf->server_name_in_redirect = NGX_CONF_UNSET;
    clcf->port_in_redirect = NGX_CONF_UNSET;
    clcf->msie_padding = NGX_CONF_UNSET;
    clcf->msie_refresh = NGX_CONF_UNSET;
    clcf->log_not_found = NGX_CONF_UNSET;
    clcf->log_subrequest = NGX_CONF_UNSET;
    clcf->recursive_error_pages = NGX_CONF_UNSET;
    clcf->server_tokens = NGX_CONF_UNSET;
    clcf->chunked_transfer_encoding = NGX_CONF_UNSET;
    clcf->etag = NGX_CONF_UNSET;
    clcf->types_hash_max_size = NGX_CONF_UNSET_UINT;
    clcf->types_hash_bucket_size = NGX_CONF_UNSET_UINT;

    clcf->open_file_cache = NGX_CONF_UNSET_PTR;
    clcf->open_file_cache_valid = NGX_CONF_UNSET;
    clcf->open_file_cache_min_uses = NGX_CONF_UNSET_UINT;
    clcf->open_file_cache_errors = NGX_CONF_UNSET;
    clcf->open_file_cache_events = NGX_CONF_UNSET;

#if (NGX_HTTP_GZIP)
    clcf->gzip_vary = NGX_CONF_UNSET;
    clcf->gzip_http_version = NGX_CONF_UNSET_UINT;
#if (NGX_PCRE)
    clcf->gzip_disable = NGX_CONF_UNSET_PTR;
#endif
    clcf->gzip_disable_msie6 = 3;
#if (NGX_HTTP_DEGRADATION)
    clcf->gzip_disable_degradation = 3;
#endif
#endif

#if (NGX_HAVE_OPENAT)
    clcf->disable_symlinks = NGX_CONF_UNSET_UINT;
    clcf->disable_symlinks_from = NGX_CONF_UNSET_PTR;
#endif

    return clcf;
}


static ngx_str_t  ngx_http_core_text_html_type = ngx_string("text/html");
static ngx_str_t  ngx_http_core_image_gif_type = ngx_string("image/gif");
static ngx_str_t  ngx_http_core_image_jpeg_type = ngx_string("image/jpeg");

static ngx_hash_key_t  ngx_http_core_default_types[] = {
    { ngx_string("html"), 0, &ngx_http_core_text_html_type },
    { ngx_string("gif"), 0, &ngx_http_core_image_gif_type },
    { ngx_string("jpg"), 0, &ngx_http_core_image_jpeg_type },
    { ngx_null_string, 0, NULL }
};


static char *
ngx_http_core_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_core_loc_conf_t *prev = parent;
    ngx_http_core_loc_conf_t *conf = child;

    ngx_uint_t        i;
    ngx_hash_key_t   *type;
    ngx_hash_init_t   types_hash;

    if (conf->root.data == NULL) {

        conf->alias = prev->alias;
        conf->root = prev->root;
        conf->root_lengths = prev->root_lengths;
        conf->root_values = prev->root_values;

        if (prev->root.data == NULL) {
            ngx_str_set(&conf->root, "html");

            if (ngx_conf_full_name(cf->cycle, &conf->root, 0) != NGX_OK) {
                return NGX_CONF_ERROR;
            }
        }
    }

    if (conf->post_action.data == NULL) {
        conf->post_action = prev->post_action;
    }

    ngx_conf_merge_uint_value(conf->types_hash_max_size,
                              prev->types_hash_max_size, 1024);

    ngx_conf_merge_uint_value(conf->types_hash_bucket_size,
                              prev->types_hash_bucket_size, 64);

    conf->types_hash_bucket_size = ngx_align(conf->types_hash_bucket_size,
                                             ngx_cacheline_size);

    /*
     * the special handling of the "types" directive in the "http" section
     * to inherit the http's conf->types_hash to all servers
     */

    if (prev->types && prev->types_hash.buckets == NULL) {

        types_hash.hash = &prev->types_hash;
        types_hash.key = ngx_hash_key_lc;
        types_hash.max_size = conf->types_hash_max_size;
        types_hash.bucket_size = conf->types_hash_bucket_size;
        types_hash.name = "types_hash";
        types_hash.pool = cf->pool;
        types_hash.temp_pool = NULL;

        if (ngx_hash_init(&types_hash, prev->types->elts, prev->types->nelts)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    if (conf->types == NULL) {
        conf->types = prev->types;
        conf->types_hash = prev->types_hash;
    }

    if (conf->types == NULL) {
        conf->types = ngx_array_create(cf->pool, 3, sizeof(ngx_hash_key_t));
        if (conf->types == NULL) {
            return NGX_CONF_ERROR;
        }

        for (i = 0; ngx_http_core_default_types[i].key.len; i++) {
            type = ngx_array_push(conf->types);
            if (type == NULL) {
                return NGX_CONF_ERROR;
            }

            type->key = ngx_http_core_default_types[i].key;
            type->key_hash =
                       ngx_hash_key_lc(ngx_http_core_default_types[i].key.data,
                                       ngx_http_core_default_types[i].key.len);
            type->value = ngx_http_core_default_types[i].value;
        }
    }

    if (conf->types_hash.buckets == NULL) {

        types_hash.hash = &conf->types_hash;
        types_hash.key = ngx_hash_key_lc;
        types_hash.max_size = conf->types_hash_max_size;
        types_hash.bucket_size = conf->types_hash_bucket_size;
        types_hash.name = "types_hash";
        types_hash.pool = cf->pool;
        types_hash.temp_pool = NULL;

        if (ngx_hash_init(&types_hash, conf->types->elts, conf->types->nelts)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    if (conf->error_log == NULL) {
        if (prev->error_log) {
            conf->error_log = prev->error_log;
        } else {
            conf->error_log = &cf->cycle->new_log;
        }
    }

    if (conf->error_pages == NULL && prev->error_pages) {
        conf->error_pages = prev->error_pages;
    }

    ngx_conf_merge_str_value(conf->default_type,
                              prev->default_type, "text/plain");

    ngx_conf_merge_off_value(conf->client_max_body_size,
                              prev->client_max_body_size, 1 * 1024 * 1024);
    ngx_conf_merge_size_value(conf->client_body_buffer_size,
                              prev->client_body_buffer_size,
                              (size_t) 2 * ngx_pagesize);
    ngx_conf_merge_msec_value(conf->client_body_timeout,
                              prev->client_body_timeout, 60000);

    ngx_conf_merge_bitmask_value(conf->keepalive_disable,
                              prev->keepalive_disable,
                              (NGX_CONF_BITMASK_SET
                               |NGX_HTTP_KEEPALIVE_DISABLE_MSIE6));
    ngx_conf_merge_uint_value(conf->satisfy, prev->satisfy,
                              NGX_HTTP_SATISFY_ALL);
    ngx_conf_merge_uint_value(conf->if_modified_since, prev->if_modified_since,
                              NGX_HTTP_IMS_EXACT);
    ngx_conf_merge_uint_value(conf->max_ranges, prev->max_ranges,
                              NGX_MAX_INT32_VALUE);
    ngx_conf_merge_uint_value(conf->client_body_in_file_only,
                              prev->client_body_in_file_only,
                              NGX_HTTP_REQUEST_BODY_FILE_OFF);
    ngx_conf_merge_value(conf->client_body_in_single_buffer,
                              prev->client_body_in_single_buffer, 0);
    ngx_conf_merge_value(conf->internal, prev->internal, 0);
    ngx_conf_merge_value(conf->sendfile, prev->sendfile, 0);
    ngx_conf_merge_size_value(conf->sendfile_max_chunk,
                              prev->sendfile_max_chunk, 0);
#if (NGX_HAVE_FILE_AIO || NGX_THREADS)
    ngx_conf_merge_value(conf->aio, prev->aio, NGX_HTTP_AIO_OFF);
#endif
#if (NGX_THREADS)
    ngx_conf_merge_ptr_value(conf->thread_pool, prev->thread_pool, NULL);
    ngx_conf_merge_ptr_value(conf->thread_pool_value, prev->thread_pool_value,
                             NULL);
#endif
    ngx_conf_merge_size_value(conf->read_ahead, prev->read_ahead, 0);
    ngx_conf_merge_off_value(conf->directio, prev->directio,
                              NGX_OPEN_FILE_DIRECTIO_OFF);
    ngx_conf_merge_off_value(conf->directio_alignment, prev->directio_alignment,
                              512);
    ngx_conf_merge_value(conf->tcp_nopush, prev->tcp_nopush, 0);
    ngx_conf_merge_value(conf->tcp_nodelay, prev->tcp_nodelay, 1);

    ngx_conf_merge_msec_value(conf->send_timeout, prev->send_timeout, 60000);
    ngx_conf_merge_size_value(conf->send_lowat, prev->send_lowat, 0);
    ngx_conf_merge_size_value(conf->postpone_output, prev->postpone_output,
                              1460);
    ngx_conf_merge_size_value(conf->limit_rate, prev->limit_rate, 0);
    ngx_conf_merge_size_value(conf->limit_rate_after, prev->limit_rate_after,
                              0);
    ngx_conf_merge_msec_value(conf->keepalive_timeout,
                              prev->keepalive_timeout, 75000);
    ngx_conf_merge_sec_value(conf->keepalive_header,
                              prev->keepalive_header, 0);
    ngx_conf_merge_uint_value(conf->keepalive_requests,
                              prev->keepalive_requests, 100);
    ngx_conf_merge_uint_value(conf->lingering_close,
                              prev->lingering_close, NGX_HTTP_LINGERING_ON);
    ngx_conf_merge_msec_value(conf->lingering_time,
                              prev->lingering_time, 30000);
    ngx_conf_merge_msec_value(conf->lingering_timeout,
                              prev->lingering_timeout, 5000);
    ngx_conf_merge_msec_value(conf->resolver_timeout,
                              prev->resolver_timeout, 30000);

    if (conf->resolver == NULL) {

        if (prev->resolver == NULL) {

            /*
             * create dummy resolver in http {} context
             * to inherit it in all servers
             */

            prev->resolver = ngx_resolver_create(cf, NULL, 0);
            if (prev->resolver == NULL) {
                return NGX_CONF_ERROR;
            }
        }

        conf->resolver = prev->resolver;
    }

    if (ngx_conf_merge_path_value(cf, &conf->client_body_temp_path,
                              prev->client_body_temp_path,
                              &ngx_http_client_temp_path)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_value(conf->reset_timedout_connection,
                              prev->reset_timedout_connection, 0);
    ngx_conf_merge_value(conf->server_name_in_redirect,
                              prev->server_name_in_redirect, 0);
    ngx_conf_merge_value(conf->port_in_redirect, prev->port_in_redirect, 1);
    ngx_conf_merge_value(conf->msie_padding, prev->msie_padding, 1);
    ngx_conf_merge_value(conf->msie_refresh, prev->msie_refresh, 0);
    ngx_conf_merge_value(conf->log_not_found, prev->log_not_found, 1);
    ngx_conf_merge_value(conf->log_subrequest, prev->log_subrequest, 0);
    ngx_conf_merge_value(conf->recursive_error_pages,
                              prev->recursive_error_pages, 0);
    ngx_conf_merge_value(conf->server_tokens, prev->server_tokens, 1);
    ngx_conf_merge_value(conf->chunked_transfer_encoding,
                              prev->chunked_transfer_encoding, 1);
    ngx_conf_merge_value(conf->etag, prev->etag, 1);

    ngx_conf_merge_ptr_value(conf->open_file_cache,
                              prev->open_file_cache, NULL);

    ngx_conf_merge_sec_value(conf->open_file_cache_valid,
                              prev->open_file_cache_valid, 60);

    ngx_conf_merge_uint_value(conf->open_file_cache_min_uses,
                              prev->open_file_cache_min_uses, 1);

    ngx_conf_merge_sec_value(conf->open_file_cache_errors,
                              prev->open_file_cache_errors, 0);

    ngx_conf_merge_sec_value(conf->open_file_cache_events,
                              prev->open_file_cache_events, 0);
#if (NGX_HTTP_GZIP)

    ngx_conf_merge_value(conf->gzip_vary, prev->gzip_vary, 0);
    ngx_conf_merge_uint_value(conf->gzip_http_version, prev->gzip_http_version,
                              NGX_HTTP_VERSION_11);
    ngx_conf_merge_bitmask_value(conf->gzip_proxied, prev->gzip_proxied,
                              (NGX_CONF_BITMASK_SET|NGX_HTTP_GZIP_PROXIED_OFF));

#if (NGX_PCRE)
    ngx_conf_merge_ptr_value(conf->gzip_disable, prev->gzip_disable, NULL);
#endif

    if (conf->gzip_disable_msie6 == 3) {
        conf->gzip_disable_msie6 =
            (prev->gzip_disable_msie6 == 3) ? 0 : prev->gzip_disable_msie6;
    }

#if (NGX_HTTP_DEGRADATION)

    if (conf->gzip_disable_degradation == 3) {
        conf->gzip_disable_degradation =
            (prev->gzip_disable_degradation == 3) ?
                 0 : prev->gzip_disable_degradation;
    }

#endif
#endif

#if (NGX_HAVE_OPENAT)
    ngx_conf_merge_uint_value(conf->disable_symlinks, prev->disable_symlinks,
                              NGX_DISABLE_SYMLINKS_OFF);
    ngx_conf_merge_ptr_value(conf->disable_symlinks_from,
                             prev->disable_symlinks_from, NULL);
#endif

    return NGX_CONF_OK;
}
/*
Óï·¨(0.7.x)£ºlisten address:port [ default [ backlog=num | rcvbuf=size | sndbuf=size | accept_filter=filter | deferred | bind | ssl ] ] 
Óï·¨(0.8.x)£ºlisten address:port [ default_server [ backlog=num | rcvbuf=size | sndbuf=size | accept_filter=filter | reuseport | deferred | bind | ssl ] ] 
Ä¬ÈÏÖµ£ºlisten 80 
Ê¹ÓÃ×Ö¶Î£ºserver 
listenÖ¸ÁîÖ¸¶¨ÁËserver{...}×Ö¶ÎÖÐ¿ÉÒÔ±»·ÃÎÊµ½µÄipµØÖ·¼°¶Ë¿ÚºÅ£¬¿ÉÒÔÖ»Ö¸¶¨Ò»¸öip£¬Ò»¸ö¶Ë¿Ú£¬
»òÕßÒ»¸ö¿É½âÎöµÄ·þÎñÆ÷Ãû¡£ 
listen 127.0.0.1:8000;
listen 127.0.0.1;
http://nginx.179401.cn/StandardHTTPModules/HTTPCore.html£¨µÚ 7£¯21 Ò³£©[2010-6-19 11:44:26]
HTTPºËÐÄÄ£¿é£¨HTTP Core£©
listen 8000;
listen *:8000;
listen localhost:8000;
ipv6µØÖ·¸ñÊ½£¨0.7.36£©ÔÚÒ»¸ö·½À¨ºÅÖÐÖ¸¶¨£º 
listen [::]:8000;
listen [fe80::1];
µ±linux£¨Ïà¶ÔÓÚFreeBSD£©°ó¶¨IPv6[::]£¬ÄÇÃ´ËüÍ¬Ñù½«°ó¶¨ÏàÓ¦µÄIPv4µØÖ·£¬Èç¹ûÔÚÒ»Ð©·Çipv6·þÎñ
Æ÷ÉÏÈÔÈ»ÕâÑùÉèÖÃ£¬½«»á°ó¶¨Ê§°Ü£¬µ±È»¿ÉÒÔÊ¹ÓÃÍêÕûµÄµØÖ·À´´úÌæ[::]ÒÔÃâ·¢ÉúÎÊÌâ£¬Ò²¿ÉÒÔÊ¹
ÓÃ"default ipv6only=on" Ñ¡ÏîÀ´ÖÆ¶¨Õâ¸ölisten×Ö¶Î½ö°ó¶¨ipv6µØÖ·£¬×¢ÒâÕâ¸öÑ¡Ïî½ö¶ÔÕâÐÐlistenÉú
Ð§£¬²»»áÓ°Ïìserver¿éÖÐÆäËûlisten×Ö¶ÎÖ¸¶¨µÄipv4µØÖ·¡£ 
listen [2a02:750:5::123]:80;
listen [::]:80 default ipv6only=on;
Èç¹ûÖ»ÓÐipµØÖ·Ö¸¶¨£¬ÔòÄ¬ÈÏ¶Ë¿ÚÎª80¡£ 

Èç¹ûÖ¸ÁîÓÐdefault²ÎÊý£¬ÄÇÃ´Õâ¸öserver¿é½«ÊÇÍ¨¹ý¡°µØÖ·:¶Ë¿Ú¡±À´½øÐÐ·ÃÎÊµÄÄ¬ÈÏ·þÎñÆ÷£¬Õâ¶ÔÓÚÄãÏëÎªÄÇÐ©²»Æ¥Åäserver_nameÖ¸ÁîÖÐµÄ
Ö÷»úÃûÖ¸¶¨Ä¬ÈÏserver¿éµÄÐéÄâÖ÷»ú£¨»ùÓÚÓòÃû£©·Ç³£ÓÐÓÃ£¬Èç¹ûÃ»ÓÐÖ¸Áî´øÓÐdefault²ÎÊý£¬ÄÇÃ´Ä¬ÈÏ·þÎñÆ÷½«Ê¹ÓÃµÚÒ»¸öserver¿é¡£ 

listenÔÊÐíÒ»Ð©²»Í¬µÄ²ÎÊý£¬¼´ÏµÍ³µ÷ÓÃlisten(2)ºÍbind(2)ÖÐÖ¸¶¨µÄ²ÎÊý£¬ÕâÐ©²ÎÊý±ØÐëÓÃÔÚdefault²ÎÊýÖ®ºó£º 
backlog=num -- Ö¸¶¨µ÷ÓÃlisten(2)Ê±backlogµÄÖµ£¬Ä¬ÈÏÎª-1¡£ 
rcvbuf=size -- ÎªÕýÔÚ¼àÌýµÄ¶Ë¿ÚÖ¸¶¨SO_RCVBUF¡£ 
sndbuf=size -- ÎªÕýÔÚ¼àÌýµÄ¶Ë¿ÚÖ¸¶¨SO_SNDBUF¡£ 
accept_filter=filter -- Ö¸¶¨accept-filter¡£ 
¡¤½öÓÃÓÚFreeBSD£¬¿ÉÒÔÓÐÁ½¸ö¹ýÂËÆ÷£¬datareadyÓëhttpready£¬½öÔÚ×îÖÕ°æ±¾µÄFreeBSD£¨FreeBSD: 6.0, 5.4-STABLEÓë4.11-STABLE£©ÉÏ£¬ÎªËûÃÇ·¢ËÍ-HUPÐÅºÅ¿ÉÄÜ»á¸Ä±äaccept-filter¡£
deferred -- ÔÚlinuxÏµÍ³ÉÏÑÓ³Ùaccept(2)µ÷ÓÃ²¢Ê¹ÓÃÒ»¸ö¸¨ÖúµÄ²ÎÊý£º TCP_DEFER_ACCEPT¡£
bind -- ½«bind(2)·Ö¿ªµ÷ÓÃ¡£
¡¤Ö÷ÒªÖ¸ÕâÀïµÄ¡°µØÖ·:¶Ë¿Ú¡±£¬Êµ¼ÊÉÏÈç¹û¶¨ÒåÁË²»Í¬µÄÖ¸Áî¼àÌýÍ¬Ò»¸ö¶Ë¿Ú£¬µ«ÊÇÃ¿¸ö²»Í¬µÄµØÖ·ºÍÄ³
ÌõÖ¸Áî¾ù¼àÌýÎªÕâ¸ö¶Ë¿ÚµÄËùÓÐµØÖ·£¨*:port£©£¬ÄÇÃ´nginxÖ»½«bind(2)µ÷ÓÃÓÚ*:port¡£ÕâÖÖÇé¿öÏÂÍ¨¹ýÏµÍ³
µ÷ÓÃgetsockname()È·¶¨ÄÄ¸öµØÖ·ÉÏÓÐÁ¬½Óµ½´ï£¬µ«ÊÇÈç¹ûÊ¹ÓÃÁËparameters backlog, rcvbuf, sndbuf, 
accept_filter»òdeferredÕâÐ©²ÎÊý£¬ÄÇÃ´½«×ÜÊÇ½«Õâ¸ö¡°µØÖ·:¶Ë¿Ú¡±·Ö¿ªµ÷ÓÃ¡£
ssl -- ²ÎÊý£¨0.7.14£©²»½«listen(2)ºÍbind(2)ÏµÍ³µ÷ÓÃ¹ØÁª¡£
¡¤±»Ö¸¶¨Õâ¸ö²ÎÊýµÄlisten½«±»ÔÊÐí¹¤×÷ÔÚSSLÄ£Ê½£¬Õâ½«ÔÊÐí·þÎñÆ÷Í¬Ê±¹¤×÷ÔÚHTTPºÍHTTPSÁ½ÖÖÐ­Òé
ÏÂ£¬ÀýÈç£º
listen 80;
listen 443 default ssl;
Ò»¸öÊ¹ÓÃÕâÐ©²ÎÊýµÄÍêÕûÀý×Ó£º 
listen 127.0.0.1 default accept_filter=dataready backlog=1024;
0.8.21°æ±¾ÒÔºónginx¿ÉÒÔ¼àÌýunixÌ×½Ó¿Ú£º 
listen unix:/tmp/nginx1.sock;
*/
//"listen"ÅäÖÃÏî,×îÖÕ´æ·ÅÔÚngx_http_core_main_conf_t->ports
static char *
ngx_http_core_listen(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_srv_conf_t *cscf = conf;

    ngx_str_t              *value, size;
    ngx_url_t               u;
    ngx_uint_t              n;
    ngx_http_listen_opt_t   lsopt;

    cscf->listen = 1;

    value = cf->args->elts;

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url = value[1];
    u.listen = 1;
    u.default_port = 80;

    if (ngx_parse_url(cf->pool, &u) != NGX_OK) {
        if (u.err) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "%s in \"%V\" of the \"listen\" directive",
                               u.err, &u.url);
        }

        return NGX_CONF_ERROR;
    }

    ngx_memzero(&lsopt, sizeof(ngx_http_listen_opt_t));

    ngx_memcpy(&lsopt.u.sockaddr, u.sockaddr, u.socklen);

    lsopt.socklen = u.socklen;
    lsopt.backlog = NGX_LISTEN_BACKLOG;
    lsopt.rcvbuf = -1;
    lsopt.sndbuf = -1;
#if (NGX_HAVE_SETFIB)
    lsopt.setfib = -1;
#endif
#if (NGX_HAVE_TCP_FASTOPEN)
    lsopt.fastopen = -1;
#endif
    lsopt.wildcard = u.wildcard; 
#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
    lsopt.ipv6only = 1;
#endif

    (void) ngx_sock_ntop(&lsopt.u.sockaddr, lsopt.socklen, lsopt.addr,
                         NGX_SOCKADDR_STRLEN, 1);

    for (n = 2; n < cf->args->nelts; n++) {
    /* ½²ËùÔÚµÄserver¿é×÷ÎªÕû¸öWEB·þÎñµÄÄ¬ÈÏserver¿é¡£Èç¹ûÃ»ÓÐÉèÖÃÕâ¸ö²úÉú£¬ÄÇÃ´½«»áÒÔÔÚnginx.confÖÐÕÒµ½µÄµÚÒ»¸öserver¿é×÷Îª
    Ä¬ÈÏserver¿é£¬ÎªÊ²Ã´ÐèÒªÄ¬ÈÏÐéÄâÖ÷»úÄØ?µ±Ò»¸öÇëÇóÎÞ·¨Æ¥ÅäÅäÖÃÎÄ¼þÖÐµÄËùÓÐÖ÷»úÓòÃûÊ±£¬¾Í»áÑ¡ÓÃÄ¬ÈÏÐéÄâÖ÷»ú*/
        if (ngx_strcmp(value[n].data, "default_server") == 0
            || ngx_strcmp(value[n].data, "default") == 0)
        {
            lsopt.default_server = 1;
            continue;
        }
/*
    instructs to make a separate bind() call for a given address:port pair. This is useful because if there are several listen 
directives with the same port but different addresses, and one of the listen directives listens on all addresses for the 
given port (*:port), nginx will bind() only to *:port. It should be noted that the getsockname() system call will be made 
in this case to determine the address that accepted the connection. If the setfib, backlog, rcvbuf, sndbuf, accept_filter, 
deferred, ipv6only, or so_keepalive parameters are used then for a given address:port pair a separate bind() call will always be made. 
*/
        //bind IP:port
        if (ngx_strcmp(value[n].data, "bind") == 0) {
            lsopt.set = 1;
            lsopt.bind = 1;
            continue;
        }

#if (NGX_HAVE_SETFIB)
        if (ngx_strncmp(value[n].data, "setfib=", 7) == 0) {
            lsopt.setfib = ngx_atoi(value[n].data + 7, value[n].len - 7);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.setfib == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid setfib \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }
#endif

#if (NGX_HAVE_TCP_FASTOPEN)
        if (ngx_strncmp(value[n].data, "fastopen=", 9) == 0) {
            lsopt.fastopen = ngx_atoi(value[n].data + 9, value[n].len - 9);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.fastopen == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid fastopen \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }
#endif
        //TCPÈý´ÎÎÕÊÖµÄÊ±ºò£¬Èç¹û·þÎñÆ÷¶Ë»¹Ã»ÓÐ¿ªÊ¼´¦Àí¼àÌý¾ä±ú£¬ÄÇÃ´ÄÚºË×î¶àÖ§³Öbacklog¸ö»º´æ£¬Èç¹û³¬¹ýÁËÕâ¸öÖµ£¬Ôò¿Í»§¶Ë»á½¨Á¢Á¬½ÓÊ§°Ü¡£
        if (ngx_strncmp(value[n].data, "backlog=", 8) == 0) {
            lsopt.backlog = ngx_atoi(value[n].data + 8, value[n].len - 8);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.backlog == NGX_ERROR || lsopt.backlog == 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid backlog \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[n].data, "rcvbuf=", 7) == 0) {//SO_RCVBUF
            size.len = value[n].len - 7;
            size.data = value[n].data + 7;

            lsopt.rcvbuf = ngx_parse_size(&size);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.rcvbuf == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid rcvbuf \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[n].data, "sndbuf=", 7) == 0) {//SO_SENDBUF
            size.len = value[n].len - 7;
            size.data = value[n].data + 7;

            lsopt.sndbuf = ngx_parse_size(&size);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.sndbuf == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid sndbuf \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }
        //ÉèÖÃaccept¹ýÂËÆ÷£¬Ö§¶ÓFreeBSD²Ù×÷ÏµÍ³ÓÐÓÃ
        if (ngx_strncmp(value[n].data, "accept_filter=", 14) == 0) {
#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
            lsopt.accept_filter = (char *) &value[n].data[14];
            lsopt.set = 1;
            lsopt.bind = 1;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "accept filters \"%V\" are not supported "
                               "on this platform, ignored",
                               &value[n]);
#endif
            continue;
        }
        
        /* 
        TCP_DEFER_ACCEPT ÓÅ»¯ Ê¹ÓÃTCP_DEFER_ACCEPT¿ÉÒÔ¼õÉÙÓÃ»§³ÌÐòholdµÄÁ¬½ÓÊý£¬Ò²¿ÉÒÔ¼õÉÙÓÃ»§µ÷ÓÃepoll_ctlºÍepoll_waitµÄ´ÎÊý£¬´Ó¶øÌá¸ßÁË³ÌÐòµÄÐÔÄÜ¡£
        ÉèÖÃlistenÌ×½Ó×ÖµÄTCP_DEFER_ACCEPTÑ¡Ïîºó£¬ Ö»µ±Ò»¸öÁ´½ÓÓÐÊý¾ÝÊ±ÊÇ²Å»á´ÓaccpetÖÐ·µ»Ø£¨¶ø²»ÊÇÈý´ÎÎÕÊÖÍê³É)¡£ËùÒÔ½ÚÊ¡ÁËÒ»´Î¶ÁµÚÒ»¸öhttpÇëÇó°üµÄ¹ý³Ì£¬¼õÉÙÁËÏµÍ³µ÷ÓÃ
          
        ²éÑ¯×ÊÁÏ£¬TCP_DEFER_ACCEPTÊÇÒ»¸öºÜÓÐÈ¤µÄÑ¡Ïî£¬
        Linux Ìá¹©µÄÒ»¸öÌØÊâ setsockopt ,¡¡ÔÚ accept µÄ socket ÉÏÃæ£¬Ö»ÓÐµ±Êµ¼ÊÊÕµ½ÁËÊý¾Ý£¬²Å»½ÐÑÕýÔÚ accept µÄ½ø³Ì£¬¿ÉÒÔ¼õÉÙÒ»Ð©ÎÞÁÄµÄÉÏÏÂÎÄÇÐ»»¡£´úÂëÈçÏÂ¡£
        val = 5;
        setsockopt(srv_socket->fd, SOL_TCP, TCP_DEFER_ACCEPT, &val, sizeof(val)) ;
        ÀïÃæ val µÄµ¥Î»ÊÇÃë£¬×¢ÒâÈç¹û´ò¿ªÕâ¸ö¹¦ÄÜ£¬kernel ÔÚ val ÃëÖ®ÄÚ»¹Ã»ÓÐÊÕµ½Êý¾Ý£¬²»»á¼ÌÐø»½ÐÑ½ø³Ì£¬¶øÊÇÖ±½Ó¶ªÆúÁ¬½Ó¡£
        ¾­¹ý²âÊÔ·¢ÏÖ£¬ÉèÖÃTCP_DEFER_ACCEPTÑ¡Ïîºó£¬·þÎñÆ÷ÊÜµ½Ò»¸öCONNECTÇëÇóºó£¬²Ù×÷ÏµÍ³²»»áAccept£¬Ò²²»»á´´½¨IO¾ä±ú¡£²Ù×÷ÏµÍ³Ó¦¸ÃÔÚÈô¸ÉÃë£¬(µ«¿Ï¶¨Ô¶Ô¶´óÓÚÉÏÃæÉèÖÃµÄ1s) ºó£¬
        »áÊÍ·ÅÏà¹ØµÄÁ´½Ó¡£µ«Ã»ÓÐÍ¬Ê±¹Ø±ÕÏàÓ¦µÄ¶Ë¿Ú£¬ËùÒÔ¿Í»§¶Ë»áÒ»Ö±ÒÔÎª´¦ÓÚÁ´½Ó×´Ì¬¡£Èç¹ûConnectºóÃæÂíÉÏÓÐºóÐøµÄ·¢ËÍÊý¾Ý£¬ÄÇÃ´·þÎñÆ÷»áµ÷ÓÃAccept½ÓÊÕÕâ¸öÁ´½Ó¶Ë¿Ú¡£
        ¸Ð¾õÁËÒ»ÏÂ£¬Õâ¸ö¶Ë¿ÚÉèÖÃ¶ÔÓÚCONNECTÁ´½ÓÉÏÀ´¶øÓÖÊ²Ã´¶¼²»¸ÉµÄ¹¥»÷·½Ê½´¦ÀíºÜÓÐÐ§¡£ÎÒÃÇÔ­À´µÄ´úÂë¶¼ÊÇÏÈÔÊÐíÁ´½Ó£¬È»ºóÔÙ½øÐÐ³¬Ê±´¦Àí£¬±ÈËûÕâ¸öÓÐµãOutÁË¡£²»¹ýÕâ¸öÑ¡Ïî¿ÉÄÜ»áµ¼ÖÂ¶¨Î»Ä³Ð©ÎÊÌâÂé·³¡£
        timeout = 0±íÊ¾È¡Ïû TCP_DEFER_ACCEPTÑ¡Ïî

        ÐÔÄÜËÄÉ±ÊÖ£ºÄÚ´æ¿½±´£¬ÄÚ´æ·ÖÅä£¬½ø³ÌÇÐ»»£¬ÏµÍ³µ÷ÓÃ¡£TCP_DEFER_ACCEPT ¶ÔÐÔÄÜµÄ¹±Ï×£¬¾ÍÔÚÓÚ ¼õÉÙÏµÍ³µ÷ÓÃÁË¡£
        */
        if (ngx_strcmp(value[n].data, "deferred") == 0) { //ËÑË÷TCP_DEFER_ACCEPT
#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
            lsopt.deferred_accept = 1;
            lsopt.set = 1;
            lsopt.bind = 1;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "the deferred accept is not supported "
                               "on this platform, ignored");
#endif
            continue;
        }

        if (ngx_strncmp(value[n].data, "ipv6only=o", 10) == 0) {
#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
            struct sockaddr  *sa;

            sa = &lsopt.u.sockaddr;

            if (sa->sa_family == AF_INET6) {

                if (ngx_strcmp(&value[n].data[10], "n") == 0) {
                    lsopt.ipv6only = 1;

                } else if (ngx_strcmp(&value[n].data[10], "ff") == 0) {
                    lsopt.ipv6only = 0;

                } else {
                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                       "invalid ipv6only flags \"%s\"",
                                       &value[n].data[9]);
                    return NGX_CONF_ERROR;
                }

                lsopt.set = 1;
                lsopt.bind = 1;

            } else {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "ipv6only is not supported "
                                   "on addr \"%s\", ignored", lsopt.addr);
            }

            continue;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "ipv6only is not supported "
                               "on this platform");
            return NGX_CONF_ERROR;
#endif
        }

        if (ngx_strcmp(value[n].data, "reuseport") == 0) {
#if (NGX_HAVE_REUSEPORT)
            lsopt.reuseport = 1;
            lsopt.set = 1;
            lsopt.bind = 1;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "reuseport is not supported "
                               "on this platform, ignored");
#endif
            continue;
        }

    //ÔÚµ±Ç°¶Ë¿ÚÉÏ½¨Á¢µÄÁ¬½Ó±ØÐë»ùÓÚSSLÐ­Òé
    /*
    ±»Ö¸¶¨Õâ¸ö²ÎÊýµÄlisten½«±»ÔÊÐí¹¤×÷ÔÚSSLÄ£Ê½£¬Õâ½«ÔÊÐí·þÎñÆ÷Í¬Ê±¹¤×÷ÔÚHTTPºÍHTTPSÁ½ÖÖÐ­ÒéÏÂ£¬ÀýÈç£º
        listen 80;
        listen 443 default ssl;
    */
        if (ngx_strcmp(value[n].data, "ssl") == 0) {
#if (NGX_HTTP_SSL)
            lsopt.ssl = 1;
            continue;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "the \"ssl\" parameter requires "
                               "ngx_http_ssl_module");
            return NGX_CONF_ERROR;
#endif
        }

        if (ngx_strcmp(value[n].data, "http2") == 0) {
#if (NGX_HTTP_V2)
            lsopt.http2 = 1;
            continue;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "the \"http2\" parameter requires "
                               "ngx_http_v2_module");
            return NGX_CONF_ERROR;
#endif
        }

        if (ngx_strcmp(value[n].data, "spdy") == 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid parameter \"spdy\": "
                               "the SPDY module was deprecated, "
                               "use the HTTP/2 module instead");
            return NGX_CONF_ERROR;
        }

        if (ngx_strncmp(value[n].data, "so_keepalive=", 13) == 0) {

            if (ngx_strcmp(&value[n].data[13], "on") == 0) {
                lsopt.so_keepalive = 1;

            } else if (ngx_strcmp(&value[n].data[13], "off") == 0) {
                lsopt.so_keepalive = 2;

            } else {

#if (NGX_HAVE_KEEPALIVE_TUNABLE)
                u_char     *p, *end;
                ngx_str_t   s;

                end = value[n].data + value[n].len;
                s.data = value[n].data + 13;

                p = ngx_strlchr(s.data, end, ':');
                if (p == NULL) {
                    p = end;
                }

                if (p > s.data) {
                    s.len = p - s.data;

                    lsopt.tcp_keepidle = ngx_parse_time(&s, 1);
                    if (lsopt.tcp_keepidle == (time_t) NGX_ERROR) {
                        goto invalid_so_keepalive;
                    }
                }

                s.data = (p < end) ? (p + 1) : end;

                p = ngx_strlchr(s.data, end, ':');
                if (p == NULL) {
                    p = end;
                }

                if (p > s.data) {
                    s.len = p - s.data;

                    lsopt.tcp_keepintvl = ngx_parse_time(&s, 1);
                    if (lsopt.tcp_keepintvl == (time_t) NGX_ERROR) {
                        goto invalid_so_keepalive;
                    }
                }

                s.data = (p < end) ? (p + 1) : end;

                if (s.data < end) {
                    s.len = end - s.data;

                    lsopt.tcp_keepcnt = ngx_atoi(s.data, s.len);
                    if (lsopt.tcp_keepcnt == NGX_ERROR) {
                        goto invalid_so_keepalive;
                    }
                }

                if (lsopt.tcp_keepidle == 0 && lsopt.tcp_keepintvl == 0
                    && lsopt.tcp_keepcnt == 0)
                {
                    goto invalid_so_keepalive;
                }

                lsopt.so_keepalive = 1;

#else

                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "the \"so_keepalive\" parameter accepts "
                                   "only \"on\" or \"off\" on this platform");
                return NGX_CONF_ERROR;

#endif
            }

            lsopt.set = 1;
            lsopt.bind = 1;

            continue;

#if (NGX_HAVE_KEEPALIVE_TUNABLE)
        invalid_so_keepalive:

            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid so_keepalive value: \"%s\"",
                               &value[n].data[13]);
            return NGX_CONF_ERROR;
#endif
        }

        if (ngx_strcmp(value[n].data, "proxy_protocol") == 0) {
            lsopt.proxy_protocol = 1;
            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[n]);
        return NGX_CONF_ERROR;
    }

    if (ngx_http_add_listen(cf, cscf, &lsopt) == NGX_OK) {
        return NGX_CONF_OK;
    }

    return NGX_CONF_ERROR;
}

//Õâ¸öº¯ÊýÍê³ÉµÄ¹¦ÄÜºÜ¼òµ¥¾ÍÊÇ½«server_nameÖ¸ÁîÖ¸¶¨µÄÐéÄâÖ÷»úÃûÌí¼Óµ½ngx_http_core_srv_conf_tµÄserver_namesÊý×éÖÐ£¬ÒÔ±ãÔÚºóÃæ¶ÔÕû¸öweb serverÖ§³ÖµÄÐéÄâÖ÷»ú½øÐÐ³õÊ¼»¯¡£
static char *
ngx_http_core_server_name(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_srv_conf_t *cscf = conf;

    u_char                   ch;
    ngx_str_t               *value;
    ngx_uint_t               i;
    ngx_http_server_name_t  *sn;

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {
        ch = value[i].data[0];

        if ((ch == '*' && (value[i].len < 3 || value[i].data[1] != '.'))
            || (ch == '.' && value[i].len < 2))
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "server name \"%V\" is invalid", &value[i]);
            return NGX_CONF_ERROR;
        }

        if (ngx_strchr(value[i].data, '/')) {
            ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                               "server name \"%V\" has suspicious symbols",
                               &value[i]);
        }

        sn = ngx_array_push(&cscf->server_names);//°Ñ½âÎö³öµÄserver_name²ÎÊý±£´æµ½server_names arrayÖÐ
        if (sn == NULL) {
            return NGX_CONF_ERROR;
        }

#if (NGX_PCRE)
        sn->regex = NULL;
#endif
        sn->server = cscf;

        if (ngx_strcasecmp(value[i].data, (u_char *) "$hostname") == 0) { //Èç¹ûÊÇ$hostname£¬ÔòÖ±½Ó´æ´¢gethostnameÏµÍ³µ÷ÓÃµÃµ½µÄÖ÷»úÃû
            sn->name = cf->cycle->hostname;

        } else {
            sn->name = value[i];
        }

        if (value[i].data[0] != '~') { //²»ÊÇÕýÔò±í´ïÊ½
            ngx_strlow(sn->name.data, sn->name.data, sn->name.len); //Èç¹ûÊ××Ö·û²»Îª"~"Ôò£¬Ôòserver_nameºóÃæµÄ²ÎÊý×ª»»ÎªÐ¡Ð´×ÖÄ¸
            continue;
        }

#if (NGX_PCRE)
        {
            u_char               *p;
            ngx_regex_compile_t   rc;
            u_char                errstr[NGX_MAX_CONF_ERRSTR];

            if (value[i].len == 1) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "empty regex in server name \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            value[i].len--;
            value[i].data++;

            ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

            rc.pattern = value[i];
            rc.err.len = NGX_MAX_CONF_ERRSTR;
            rc.err.data = errstr;

            for (p = value[i].data; p < value[i].data + value[i].len; p++) {
                if (*p >= 'A' && *p <= 'Z') {
                    rc.options = NGX_REGEX_CASELESS;
                    break;
                }
            }

            sn->regex = ngx_http_regex_compile(cf, &rc);
            if (sn->regex == NULL) {
                return NGX_CONF_ERROR;
            }

            sn->name = value[i];
            cscf->captures = (rc.captures > 0);
        }
#else
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "using regex \"%V\" "
                           "requires PCRE library", &value[i]);

        return NGX_CONF_ERROR;
#endif
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_core_root(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t                  *value;
    ngx_int_t                   alias;
    ngx_uint_t                  n;
    ngx_http_script_compile_t   sc;

    alias = (cmd->name.len == sizeof("alias") - 1) ? 1 : 0;

    if (clcf->root.data) {

        if ((clcf->alias != 0) == alias) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"%V\" directive is duplicate",
                               &cmd->name);
        } else {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"%V\" directive is duplicate, "
                               "\"%s\" directive was specified earlier",
                               &cmd->name, clcf->alias ? "alias" : "root");
        }

        return NGX_CONF_ERROR;
    }

    if (clcf->named && alias) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the \"alias\" directive cannot be used "
                           "inside the named location");

        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    if (ngx_strstr(value[1].data, "$document_root")
        || ngx_strstr(value[1].data, "${document_root}"))
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the $document_root variable cannot be used "
                           "in the \"%V\" directive",
                           &cmd->name);

        return NGX_CONF_ERROR;
    }

    if (ngx_strstr(value[1].data, "$realpath_root")
        || ngx_strstr(value[1].data, "${realpath_root}"))
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the $realpath_root variable cannot be used "
                           "in the \"%V\" directive",
                           &cmd->name);

        return NGX_CONF_ERROR;
    }

    clcf->alias = alias ? clcf->name.len : 0;
    clcf->root = value[1];

    if (!alias && clcf->root.data[clcf->root.len - 1] == '/') {
        clcf->root.len--;
    }

    if (clcf->root.data[0] != '$') {
        if (ngx_conf_full_name(cf->cycle, &clcf->root, 0) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    n = ngx_http_script_variables_count(&clcf->root);

    ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));
    sc.variables = n;

#if (NGX_PCRE)
    if (alias && clcf->regex) {
        clcf->alias = NGX_MAX_SIZE_T_VALUE;
        n = 1;
    }
#endif

    if (n) {
        sc.cf = cf;
        sc.source = &clcf->root;
        sc.lengths = &clcf->root_lengths;
        sc.values = &clcf->root_values;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


static ngx_http_method_name_t  ngx_methods_names[] = {
   { (u_char *) "GET",       (uint32_t) ~NGX_HTTP_GET },
   { (u_char *) "HEAD",      (uint32_t) ~NGX_HTTP_HEAD },
   { (u_char *) "POST",      (uint32_t) ~NGX_HTTP_POST },
   { (u_char *) "PUT",       (uint32_t) ~NGX_HTTP_PUT },
   { (u_char *) "DELETE",    (uint32_t) ~NGX_HTTP_DELETE },
   { (u_char *) "MKCOL",     (uint32_t) ~NGX_HTTP_MKCOL },
   { (u_char *) "COPY",      (uint32_t) ~NGX_HTTP_COPY },
   { (u_char *) "MOVE",      (uint32_t) ~NGX_HTTP_MOVE },
   { (u_char *) "OPTIONS",   (uint32_t) ~NGX_HTTP_OPTIONS },
   { (u_char *) "PROPFIND",  (uint32_t) ~NGX_HTTP_PROPFIND },
   { (u_char *) "PROPPATCH", (uint32_t) ~NGX_HTTP_PROPPATCH },
   { (u_char *) "LOCK",      (uint32_t) ~NGX_HTTP_LOCK },
   { (u_char *) "UNLOCK",    (uint32_t) ~NGX_HTTP_UNLOCK },
   { (u_char *) "PATCH",     (uint32_t) ~NGX_HTTP_PATCH },
   { NULL, 0 }
};


static char *
ngx_http_core_limit_except(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *pclcf = conf;

    char                      *rv;
    void                      *mconf;
    ngx_str_t                 *value;
    ngx_uint_t                 i;
    ngx_conf_t                 save;
    ngx_http_module_t         *module;
    ngx_http_conf_ctx_t       *ctx, *pctx;
    ngx_http_method_name_t    *name;
    ngx_http_core_loc_conf_t  *clcf;

    if (pclcf->limit_except) {
        return "duplicate";
    }

    pclcf->limit_except = 0xffffffff;

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {
        for (name = ngx_methods_names; name->name; name++) {

            if (ngx_strcasecmp(value[i].data, name->name) == 0) {
                pclcf->limit_except &= name->method;
                goto next;
            }
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid method \"%V\"", &value[i]);
        return NGX_CONF_ERROR;

    next:
        continue;
    }

    if (!(pclcf->limit_except & NGX_HTTP_GET)) {
        pclcf->limit_except &= (uint32_t) ~NGX_HTTP_HEAD;
    }

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    pctx = cf->ctx;
    ctx->main_conf = pctx->main_conf;
    ctx->srv_conf = pctx->srv_conf;

    ctx->loc_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->loc_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    for (i = 0; ngx_modules[i]; i++) {
        if (ngx_modules[i]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = ngx_modules[i]->ctx;

        if (module->create_loc_conf) {

            mconf = module->create_loc_conf(cf);
            if (mconf == NULL) {
                 return NGX_CONF_ERROR;
            }

            ctx->loc_conf[ngx_modules[i]->ctx_index] = mconf;
        }
    }


    clcf = ctx->loc_conf[ngx_http_core_module.ctx_index];
    pclcf->limit_except_loc_conf = ctx->loc_conf;
    clcf->loc_conf = ctx->loc_conf;
    clcf->name = pclcf->name;
    clcf->noname = 1; //limit_exceptÅäÖÃ±»×÷ÎªlocationµÄnonameÐÎÊ½
    clcf->lmt_excpt = 1;

    if (ngx_http_add_location(cf, &pclcf->locations, clcf) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    save = *cf;
    cf->ctx = ctx;
    cf->cmd_type = NGX_HTTP_LMT_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = save;

    return rv;
}

//aio on | off | threads[=pool];
static char *
ngx_http_core_set_aio(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t  *value;

    if (clcf->aio != NGX_CONF_UNSET) {
        return "is duplicate";
    }

#if (NGX_THREADS)
    clcf->thread_pool = NULL;
    clcf->thread_pool_value = NULL;
#endif

    value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "off") == 0) {
        clcf->aio = NGX_HTTP_AIO_OFF;
        return NGX_CONF_OK;
    }

    if (ngx_strcmp(value[1].data, "on") == 0) {
#if (NGX_HAVE_FILE_AIO)
        clcf->aio = NGX_HTTP_AIO_ON;
        return NGX_CONF_OK;
#else
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"aio on\" "
                           "is unsupported on this platform");
        return NGX_CONF_ERROR;
#endif
    }

#if (NGX_HAVE_AIO_SENDFILE)

    if (ngx_strcmp(value[1].data, "sendfile") == 0) {
        clcf->aio = NGX_HTTP_AIO_ON;

        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "the \"sendfile\" parameter of "
                           "the \"aio\" directive is deprecated");
        return NGX_CONF_OK;
    }

#endif

    if (ngx_strncmp(value[1].data, "threads", 7) == 0
        && (value[1].len == 7 || value[1].data[7] == '='))
    {
#if (NGX_THREADS)
        ngx_str_t                          name;
        ngx_thread_pool_t                 *tp;
        ngx_http_complex_value_t           cv;
        ngx_http_compile_complex_value_t   ccv;

        clcf->aio = NGX_HTTP_AIO_THREADS;

        if (value[1].len >= 8) {
            name.len = value[1].len - 8;
            name.data = value[1].data + 8;

            ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

            ccv.cf = cf;
            ccv.value = &name;
            ccv.complex_value = &cv;

            if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            if (cv.lengths != NULL) {
                clcf->thread_pool_value = ngx_palloc(cf->pool,
                                    sizeof(ngx_http_complex_value_t));
                if (clcf->thread_pool_value == NULL) {
                    return NGX_CONF_ERROR;
                }

                *clcf->thread_pool_value = cv;

                return NGX_CONF_OK;
            }

            tp = ngx_thread_pool_add(cf, &name);

        } else {
            tp = ngx_thread_pool_add(cf, NULL);
        }

        if (tp == NULL) {
            return NGX_CONF_ERROR;
        }

        clcf->thread_pool = tp; //aio thread ÅäÖÃµÄÊ±ºò£¬location{}¿é¶ÔÓ¦µÄthread_pollÐÅÏ¢

        return NGX_CONF_OK;
#else
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"aio threads\" "
                           "is unsupported on this platform");
        return NGX_CONF_ERROR;
#endif
    }

    return "invalid value";
}


static char *
ngx_http_core_directio(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t  *value;

    if (clcf->directio != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "off") == 0) {
        clcf->directio = NGX_OPEN_FILE_DIRECTIO_OFF;
        return NGX_CONF_OK;
    }

    //×îÖÕÉúÐ§//ÉúÐ§¼ûngx_open_and_stat_file  if (of->directio <= ngx_file_size(&fi)) { ngx_directio_on }
    clcf->directio = ngx_parse_offset(&value[1]);
    if (clcf->directio == (off_t) NGX_ERROR) {
        return "invalid value";
    }

    return NGX_CONF_OK;
}

/*
¸ù¾ÝHTTP·µ»ØÂëÖØ¶¨ÏòÒ³Ãæ
Óï·¨£ºerror_page code [ code... ] [ = | =answer-code ] uri | @named_location
ÅäÖÃ¿é£ºhttp¡¢server¡¢location¡¢if 

µ±¶ÔÓÚÄ³¸öÇëÇó·µ»Ø´íÎóÂëÊ±£¬Èç¹ûÆ¥ÅäÉÏÁËerror_pageÖÐÉèÖÃµÄcode£¬ÔòÖØ¶¨Ïòµ½ÐÂµÄURIÖÐ¡£ÀýÈç£º
error_page   404          /404.html;
error_page   502 503 504  /50x.html;
error_page   403          http://example.com/forbidden.html;
error_page   404          = @fetch;

×¢Òâ£¬ËäÈ»ÖØ¶¨ÏòÁËURI£¬µ«·µ»ØµÄHTTP´íÎóÂë»¹ÊÇÓëÔ­À´µÄÏàÍ¬¡£ÓÃ»§¿ÉÒÔÍ¨¹ý¡°=¡±À´¸ü¸Ä·µ»ØµÄ´íÎóÂë£¬ÀýÈç£º
error_page 404 =200 /empty.gif;
error_page 404 =403 /forbidden.gif;

Ò²¿ÉÒÔ²»Ö¸¶¨È·ÇÐµÄ·µ»Ø´íÎóÂë£¬¶øÊÇÓÉÖØ¶¨ÏòºóÊµ¼Ê´¦ÀíµÄÕæÊµ½á¹ûÀ´¾ö¶¨£¬ÕâÊ±£¬Ö»Òª°Ñ¡°=¡±ºóÃæµÄ´íÎóÂëÈ¥µô¼´¿É£¬ÀýÈç£º
error_page 404 = /empty.gif;

Èç¹û²»ÏëÐÞ¸ÄURI£¬Ö»ÊÇÏëÈÃÕâÑùµÄÇëÇóÖØ¶¨Ïòµ½ÁíÒ»¸ölocationÖÐ½øÐÐ´¦Àí£¬ÄÇÃ´¿ÉÒÔÕâÑùÉèÖÃ£º
location / (
    error_page 404 @fallback;
)
 
location @fallback (
    proxy_pass http://backend;
)

ÕâÑù£¬·µ»Ø404µÄÇëÇó»á±»·´Ïò´úÀíµ½http://backendÉÏÓÎ·þÎñÆ÷ÖÐ´¦Àí
*/  //(error_pagesÄÚÈÝÊÇ´Óngx_http_error_pagesÖÐÈ¡µÄ)
static char * //clcf->error_pages¸³Öµ²Î¿¼ngx_http_core_error_page    ÉúÐ§¼ûngx_http_send_error_page
ngx_http_core_error_page(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{ //(error_pagesÄÚÈÝÊÇ´Óngx_http_error_pagesÖÐÈ¡µÄ)
    ngx_http_core_loc_conf_t *clcf = conf;

    u_char                            *p;
    ngx_int_t                          overwrite;
    ngx_str_t                         *value, uri, args;
    ngx_uint_t                         i, n;
    ngx_http_err_page_t               *err;
    ngx_http_complex_value_t           cv;
    ngx_http_compile_complex_value_t   ccv;

    if (clcf->error_pages == NULL) {
        clcf->error_pages = ngx_array_create(cf->pool, 4,
                                             sizeof(ngx_http_err_page_t));
        if (clcf->error_pages == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    i = cf->args->nelts - 2;

    if (value[i].data[0] == '=') {
        if (i == 1) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid value \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }

        //=ºóÃæÈç¹û¸úÐÂµÄ·µ»ØÂë£¬Ôò±ØÐë½ô¸ú=ºóÃæ
        if (value[i].len > 1) {//error_page 404 =200 /empty.gif;£¬±íÊ¾ÒÔ200×÷ÎªÐÂµÄ·µ»ØÂë£¬ÓÃ»§¿ÉÒÔÍ¨¹ý¡°=¡±À´¸ü¸Ä·µ»ØµÄ´íÎóÂë
            //error_page 404 =200 /empty.gif;
            overwrite = ngx_atoi(&value[i].data[1], value[i].len - 1); //»ñÈ¡ÐÂµÄ·µ»ØÂë

            if (overwrite == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

        } else {//error_page 404 = /empty.gif; ±íÊ¾·µ»ØÂëÓÉÖØ¶¨ÏòºóÊµ¼Ê´¦ÀíµÄÕæÊµ½á¹ûÀ´¾ö¶¨£¬ÕâÊ±£¬Ö»Òª°Ñ¡°=¡±ºóÃæÃ»ÓÐ·µ»ØÂëºÅ
            overwrite = 0;
        }

        n = 2;

    } else {
        overwrite = -1; //Ã»ÓÐ=£¬¾ÍÊÇÒÔerror_page   404          /404.html;ÖÐµÄ404×÷Îª·µ»ØÂë
        n = 1;
    }

    uri = value[cf->args->nelts - 1];

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &uri;
    ccv.complex_value = &cv;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    ngx_str_null(&args);

    if (cv.lengths == NULL && uri.len && uri.data[0] == '/') {
        p = (u_char *) ngx_strchr(uri.data, '?');

        if (p) {
            cv.value.len = p - uri.data;
            cv.value.data = uri.data;
            p++;
            args.len = (uri.data + uri.len) - p;
            args.data = p;
        }
    }

    //½âÎöerror_page 401 404 =200 /empty.gif;ÖÐµÄ401 402
    for (i = 1; i < cf->args->nelts - n; i++) { //error_page´íÎóÂë±ØÐëmust be between 300 and 599£¬²¢ÇÒ²»ÄÜÎª499
        err = ngx_array_push(clcf->error_pages);
        if (err == NULL) {
            return NGX_CONF_ERROR;
        }

        err->status = ngx_atoi(value[i].data, value[i].len);

        if (err->status == NGX_ERROR || err->status == 499) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid value \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }

        if (err->status < 300 || err->status > 599) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "value \"%V\" must be between 300 and 599",
                               &value[i]);
            return NGX_CONF_ERROR;
        }

        err->overwrite = overwrite;

        if (overwrite == -1) {
            switch (err->status) {
                case NGX_HTTP_TO_HTTPS:
                case NGX_HTTPS_CERT_ERROR:
                case NGX_HTTPS_NO_CERT:
                    err->overwrite = NGX_HTTP_BAD_REQUEST;
                default:
                    break;
            }
        }

        err->value = cv;
        err->args = args;
    }

    return NGX_CONF_OK;
}

//°Ñtry_files aaa bbb cccÅäÖÃÖÐµÄaaa bbb ccc´æ´¢µ½clcf->try_files[]ÖÐ
static char *
ngx_http_core_try_files(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t                  *value;
    ngx_int_t                   code;
    ngx_uint_t                  i, n;
    ngx_http_try_file_t        *tf;
    ngx_http_script_compile_t   sc;
    ngx_http_core_main_conf_t  *cmcf;

    if (clcf->try_files) {
        return "is duplicate";
    }

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    cmcf->try_files = 1;

    tf = ngx_pcalloc(cf->pool, cf->args->nelts * sizeof(ngx_http_try_file_t));
    if (tf == NULL) {
        return NGX_CONF_ERROR;
    }

    clcf->try_files = tf;

    value = cf->args->elts;

    for (i = 0; i < cf->args->nelts - 1; i++) {
        // try_files aaa bbb cccÖÐµÄ aaa bbb ccc ÕâÈý¸ö²ÎÊý´æ·ÅÔÚtf[]Êý×éµÄÈý¸ö³ÉÔ±ÖÐ
        tf[i].name = value[i + 1]; //

        if (tf[i].name.len > 0
            && tf[i].name.data[tf[i].name.len - 1] == '/'
            && i + 2 < cf->args->nelts)
        {
            tf[i].test_dir = 1;
            tf[i].name.len--;
            tf[i].name.data[tf[i].name.len] = '\0';
        }

        n = ngx_http_script_variables_count(&tf[i].name);

        if (n) {
            ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

            sc.cf = cf;
            sc.source = &tf[i].name;
            sc.lengths = &tf[i].lengths;
            sc.values = &tf[i].values;
            sc.variables = n;
            sc.complete_lengths = 1;
            sc.complete_values = 1;

            if (ngx_http_script_compile(&sc) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

        } else {
            /* add trailing '\0' to length */
            tf[i].name.len++;
        }
    }

    if (tf[i - 1].name.data[0] == '=') {

        code = ngx_atoi(tf[i - 1].name.data + 1, tf[i - 1].name.len - 2);

        if (code == NGX_ERROR || code > 999) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid code \"%*s\"",
                               tf[i - 1].name.len - 1, tf[i - 1].name.data);
            return NGX_CONF_ERROR;
        }

        tf[i].code = code;
    }

    return NGX_CONF_OK;
}


/*
nginxÓÐÁ½¸öÖ¸ÁîÊÇ¹ÜÀí»º´æÎÄ¼þÃèÊö·ûµÄ:Ò»¸ö¾ÍÊÇ±¾ÎÄÖÐËµµ½µÄngx_http_log_moduleÄ£¿éµÄopen_file_log_cacheÅäÖÃ;´æ´¢ÔÚngx_http_log_loc_conf_t->open_file_cache 
ÁíÒ»¸öÊÇngx_http_core_moduleÄ£¿éµÄ open_file_cacheÅäÖÃ£¬´æ´¢ÔÚngx_http_core_loc_conf_t->open_file_cache;Ç°ÕßÊÇÖ»ÓÃÀ´¹ÜÀíaccess±äÁ¿ÈÕÖ¾ÎÄ¼þ¡£
ºóÕßÓÃÀ´¹ÜÀíµÄ¾Í¶àÁË£¬°üÀ¨£ºstatic£¬index£¬tryfiles£¬gzip£¬mp4£¬flv£¬¶¼ÊÇ¾²Ì¬ÎÄ¼þÅ¶!
ÕâÁ½¸öÖ¸ÁîµÄhandler¶¼µ÷ÓÃÁËº¯Êý ngx_open_file_cache_init £¬Õâ¾ÍÊÇÓÃÀ´¹ÜÀí»º´æÎÄ¼þÃèÊö·ûµÄµÚÒ»²½£º³õÊ¼»¯
*/

//open_file_cache max=1000 inactive=20s; Ö´ÐÐ¸Ãº¯Êý   max=numÖÐµÄnum±íÊ¾×î¶à»º´æÕâÃ´¶à¸öÎÄ¼þ
static char *
ngx_http_core_open_file_cache(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    time_t       inactive;
    ngx_str_t   *value, s;
    ngx_int_t    max;
    ngx_uint_t   i;

    if (clcf->open_file_cache != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    value = cf->args->elts;

    max = 0;
    inactive = 60; //Ä¬ÈÏ60

    for (i = 1; i < cf->args->nelts; i++) { //¸³Öµ¸øngx_open_file_cache_tÖÐµÄ³ÉÔ±

        if (ngx_strncmp(value[i].data, "max=", 4) == 0) {

            max = ngx_atoi(value[i].data + 4, value[i].len - 4);
            if (max <= 0) {
                goto failed;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "inactive=", 9) == 0) {

            s.len = value[i].len - 9;
            s.data = value[i].data + 9;

            inactive = ngx_parse_time(&s, 1);
            if (inactive == (time_t) NGX_ERROR) {
                goto failed;
            }

            continue;
        }

        if (ngx_strcmp(value[i].data, "off") == 0) { //offÔòÖ±½ÓÖÃÎªNULL

            clcf->open_file_cache = NULL;

            continue;
        }

    failed:

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid \"open_file_cache\" parameter \"%V\"",
                           &value[i]);
        return NGX_CONF_ERROR;
    }

    if (clcf->open_file_cache == NULL) {
        return NGX_CONF_OK;
    }

    if (max == 0) { //±ØÐëÐ¯´ømax²ÎÊý  
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                        "\"open_file_cache\" must have the \"max\" parameter");
        return NGX_CONF_ERROR;
    }

    clcf->open_file_cache = ngx_open_file_cache_init(cf->pool, max, inactive);
    if (clcf->open_file_cache) {  
        return NGX_CONF_OK;
    }

    return NGX_CONF_ERROR;
}

/* È«¾ÖÖÐÅäÖÃµÄerror_log xxx´æ´¢ÔÚngx_cycle_s->new_log£¬http{}¡¢server{}¡¢local{}ÅäÖÃµÄerror_log±£´æÔÚngx_http_core_loc_conf_t->error_log,
   ¼ûngx_log_set_log,Èç¹ûÖ»ÅäÖÃÈ«¾Öerror_log£¬²»ÅäÖÃhttp{}¡¢server{}¡¢local{}ÔòÔÚngx_http_core_merge_loc_conf conf->error_log = &cf->cycle->new_log;  */
  
static char *
ngx_http_core_error_log(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    return ngx_log_set_log(cf, &clcf->error_log);
}


static char *
ngx_http_core_keepalive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t  *value;

    if (clcf->keepalive_timeout != NGX_CONF_UNSET_MSEC) {
        return "is duplicate";
    }

    value = cf->args->elts;

    clcf->keepalive_timeout = ngx_parse_time(&value[1], 0);

    if (clcf->keepalive_timeout == (ngx_msec_t) NGX_ERROR) {
        return "invalid value";
    }

    if (cf->args->nelts == 2) {
        return NGX_CONF_OK;
    }

    clcf->keepalive_header = ngx_parse_time(&value[2], 1);

    if (clcf->keepalive_header == (time_t) NGX_ERROR) {
        return "invalid value";
    }

    return NGX_CONF_OK;
}


//ÔÚlocation{}ÖÐÅäÖÃÁËinternal£¬±íÊ¾Æ¥Åä¸ÃuriµÄlocation{}±ØÐëÊÇ½øÐÐÖØ¶¨ÏòºóÆ¥ÅäµÄ¸Ãlocation,Èç¹û²»Âú×ãÌõ¼þÖ±½Ó·µ»ØNGX_HTTP_NOT_FOUND£¬
//ÉúÐ§µØ·½¼ûngx_http_core_find_config_phase   
static char *
ngx_http_core_internal(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    if (clcf->internal != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    clcf->internal = 1;

    return NGX_CONF_OK;
}


static char *
ngx_http_core_resolver(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf = conf;

    ngx_str_t  *value;

    if (clcf->resolver) {
        return "is duplicate";
    }

    value = cf->args->elts;
    
    // ³õÊ¼»¯£¬µÚ¶þ¸ö²ÎÊýÊÇÎÒÃÇÉèÖÃµÄÓòÃû½âÎö·þÎñÆ÷µÄIPµØÖ·
    clcf->resolver = ngx_resolver_create(cf, &value[1], cf->args->nelts - 1);
    if (clcf->resolver == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


#if (NGX_HTTP_GZIP)

static char *
ngx_http_gzip_disable(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf = conf;

#if (NGX_PCRE)

    ngx_str_t            *value;
    ngx_uint_t            i;
    ngx_regex_elt_t      *re;
    ngx_regex_compile_t   rc;
    u_char                errstr[NGX_MAX_CONF_ERRSTR];

    if (clcf->gzip_disable == NGX_CONF_UNSET_PTR) {
        clcf->gzip_disable = ngx_array_create(cf->pool, 2,
                                              sizeof(ngx_regex_elt_t));
        if (clcf->gzip_disable == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.pool = cf->pool;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strcmp(value[i].data, "msie6") == 0) {
            clcf->gzip_disable_msie6 = 1;
            continue;
        }

#if (NGX_HTTP_DEGRADATION)

        if (ngx_strcmp(value[i].data, "degradation") == 0) {
            clcf->gzip_disable_degradation = 1;
            continue;
        }

#endif

        re = ngx_array_push(clcf->gzip_disable);
        if (re == NULL) {
            return NGX_CONF_ERROR;
        }

        rc.pattern = value[i];
        rc.options = NGX_REGEX_CASELESS;

        if (ngx_regex_compile(&rc) != NGX_OK) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "%V", &rc.err);
            return NGX_CONF_ERROR;
        }

        re->regex = rc.regex;
        re->name = value[i].data;
    }

    return NGX_CONF_OK;

#else
    ngx_str_t   *value;
    ngx_uint_t   i;

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {
        if (ngx_strcmp(value[i].data, "msie6") == 0) {
            clcf->gzip_disable_msie6 = 1;
            continue;
        }

#if (NGX_HTTP_DEGRADATION)

        if (ngx_strcmp(value[i].data, "degradation") == 0) {
            clcf->gzip_disable_degradation = 1;
            continue;
        }

#endif

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "without PCRE library \"gzip_disable\" supports "
                           "builtin \"msie6\" and \"degradation\" mask only");

        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;

#endif
}

#endif


#if (NGX_HAVE_OPENAT)

static char *
ngx_http_disable_symlinks(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t                         *value;
    ngx_uint_t                         i;
    ngx_http_compile_complex_value_t   ccv;

    if (clcf->disable_symlinks != NGX_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strcmp(value[i].data, "off") == 0) {
            clcf->disable_symlinks = NGX_DISABLE_SYMLINKS_OFF;
            continue;
        }

        if (ngx_strcmp(value[i].data, "if_not_owner") == 0) {
            clcf->disable_symlinks = NGX_DISABLE_SYMLINKS_NOTOWNER;
            continue;
        }

        if (ngx_strcmp(value[i].data, "on") == 0) {
            clcf->disable_symlinks = NGX_DISABLE_SYMLINKS_ON;
            continue;
        }

        if (ngx_strncmp(value[i].data, "from=", 5) == 0) {
            value[i].len -= 5;
            value[i].data += 5;

            ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

            ccv.cf = cf;
            ccv.value = &value[i];
            ccv.complex_value = ngx_palloc(cf->pool,
                                           sizeof(ngx_http_complex_value_t));
            if (ccv.complex_value == NULL) {
                return NGX_CONF_ERROR;
            }

            if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            //disable_symlinks on | if_not_owner [from=part];ÖÐfromÐ¯´øµÄ²ÎÊýpart
            clcf->disable_symlinks_from = ccv.complex_value;

            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    if (clcf->disable_symlinks == NGX_CONF_UNSET_UINT) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"off\", \"on\" "
                           "or \"if_not_owner\" parameter",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    if (cf->args->nelts == 2) {
        clcf->disable_symlinks_from = NULL;
        return NGX_CONF_OK;
    }

    if (clcf->disable_symlinks_from == NGX_CONF_UNSET_PTR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "duplicate parameters \"%V %V\"",
                           &value[1], &value[2]);
        return NGX_CONF_ERROR;
    }

    if (clcf->disable_symlinks == NGX_DISABLE_SYMLINKS_OFF) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"from=\" cannot be used with \"off\" parameter");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

#endif


static char *
ngx_http_core_lowat_check(ngx_conf_t *cf, void *post, void *data)
{
#if (NGX_FREEBSD)
    ssize_t *np = data;

    if ((u_long) *np >= ngx_freebsd_net_inet_tcp_sendspace) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"send_lowat\" must be less than %d "
                           "(sysctl net.inet.tcp.sendspace)",
                           ngx_freebsd_net_inet_tcp_sendspace);

        return NGX_CONF_ERROR;
    }

#elif !(NGX_HAVE_SO_SNDLOWAT)
    ssize_t *np = data;

    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "\"send_lowat\" is not supported, ignored");

    *np = 0;

#endif

    return NGX_CONF_OK;
}


static char *
ngx_http_core_pool_size(ngx_conf_t *cf, void *post, void *data)
{
    size_t *sp = data;

    if (*sp < NGX_MIN_POOL_SIZE) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the pool size must be no less than %uz",
                           NGX_MIN_POOL_SIZE);
        return NGX_CONF_ERROR;
    }

    if (*sp % NGX_POOL_ALIGNMENT) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the pool size must be a multiple of %uz",
                           NGX_POOL_ALIGNMENT);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}
