
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_array_t                    caches;  /* ngx_http_file_cache_t * */
} ngx_http_fastcgi_main_conf_t;


typedef struct { //´´½¨¿Õ¼äºÍ¸³Öµ¼ûngx_http_fastcgi_init_params
    ngx_array_t                   *flushes;
    ngx_array_t                   *lengths;//fastcgi_param  HTTP_xx  XXXÉèÖÃµÄ·ÇHTTP_±äÁ¿µÄÏà¹Ø±äÁ¿µÄ³¤¶ÈcodeÌí¼Óµ½ÕâÀïÃæ
    ngx_array_t                   *values;//fastcgi_param  HTTP_xx  XXXÉèÖÃµÄ·ÇHTTP_±äÁ¿µÄÏà¹Ø±äÁ¿µÄvalueÖµcodeÌí¼Óµ½ÕâÀïÃæ
    ngx_uint_t                     number; //fastcgi_param  HTTP_  XXX;»·¾³ÖÐÍ¨¹ýfastcgi_paramÉèÖÃµÄHTTP_xx±äÁ¿¸öÊý
    ngx_hash_t                     hash;//fastcgi_param  HTTP_  XXX;»·¾³ÖÐÍ¨¹ýfastcgi_paramÉèÖÃµÄHTTP_xxÍ¨¹ýhashÔËËã´æµ½¸Ãhash±íÖÐ,Êµ¼Ê´æµ½hashÖÐÊ±»á°ÑHTTP_Õâ5¸ö×Ö·ûÈ¥µô
} ngx_http_fastcgi_params_t; //ngx_http_fastcgi_loc_conf_t->params(params_source)ÖÐ´æ´¢


typedef struct {
    ngx_http_upstream_conf_t       upstream; //ÀýÈçfastcgiÔÚngx_http_fastcgi_passÖÐ´´½¨upstream¿Õ¼ä£¬ngx_http_xxx_pass

    ngx_str_t                      index;
    //ÔÚngx_http_fastcgi_init_paramsÖÐÍ¨¹ý½Å±¾½âÎöÒýÇæ°Ñ±äÁ¿codeÌí¼Óµ½paramsÖÐ
    ngx_http_fastcgi_params_t      params; //ParamsÊý¾Ý°ü£¬ÓÃÓÚ´«µÝÖ´ÐÐÒ³ÃæËùÐèÒªµÄ²ÎÊýºÍ»·¾³±äÁ¿¡£
#if (NGX_HTTP_CACHE)
    ngx_http_fastcgi_params_t      params_cache;
#endif

    //fastcgi_paramÉèÖÃµÄ´«ËÍµ½FastCGI·þÎñÆ÷µÄÏà¹Ø²ÎÊý¶¼Ìí¼Óµ½¸ÃÊý×éÖÐ£¬¼ûngx_http_upstream_param_set_slot
    ngx_array_t                   *params_source;  //×îÖÕ»áÔÚngx_http_fastcgi_init_paramsÖÐÍ¨¹ý½Å±¾½âÎöÒýÇæ°Ñ±äÁ¿codeÌí¼Óµ½paramsÖÐ

    /*
    Sets a string to search for in the error stream of a response received from a FastCGI server. If the string is found then 
    it is considered that the FastCGI server has returned an invalid response. This allows handling application errors in nginx, for example: 
    
    location /php {
        fastcgi_pass backend:9000;
        ...
        fastcgi_catch_stderr "PHP Fatal error";
        fastcgi_next_upstream error timeout invalid_header;
    }
    */ //Èç¹ûºó¶Ë·µ»ØµÄfastcgi ERRSTDÐÅÏ¢ÖÐµÄdata²¿·Ö´øÓÐfastcgi_catch_stderrÅäÖÃµÄ×Ö·û´®£¬Ôò»áÇëÇóÏÂÒ»¸öºó¶Ë·þÎñÆ÷ ²Î¿¼ngx_http_fastcgi_process_header
    ngx_array_t                   *catch_stderr; //fastcgi_catch_stderr xxx_catch_stderr

    //ÔÚngx_http_fastcgi_evalÖÐÖ´ÐÐ¶ÔÓ¦µÄcode£¬´Ó¶ø°ÑÏà¹Ø±äÁ¿×ª»»ÎªÆÕÍ¨×Ö·û´®   
    //¸³Öµ¼ûngx_http_fastcgi_pass
    ngx_array_t                   *fastcgi_lengths; //fastcgiÏà¹Ø²ÎÊýµÄ³¤¶Ècode  Èç¹ûfastcgi_pass xxxÖÐÓÐ±äÁ¿£¬Ôò¸ÃÊý×éÎª¿Õ
    ngx_array_t                   *fastcgi_values; //fastcgiÏà¹Ø²ÎÊýµÄÖµcode

    ngx_flag_t                     keep_conn; //fastcgi_keep_conn  on | off  Ä¬ÈÏoff

#if (NGX_HTTP_CACHE)
    ngx_http_complex_value_t       cache_key;
    //fastcgi_cache_key proxy_cache_keyÖ¸ÁîµÄÊ±ºò¼ÆËã³öÀ´µÄ¸´ÔÓ±í´ïÊ½½á¹¹£¬´æ·ÅÔÚflcf->cache_keyÖÐ ngx_http_fastcgi_cache_key ngx_http_proxy_cache_key
#endif

#if (NGX_PCRE)
    ngx_regex_t                   *split_regex;
    ngx_str_t                      split_name;
#endif
} ngx_http_fastcgi_loc_conf_t;

//http://my.oschina.net/goal/blog/196599
typedef enum { //¶ÔÓ¦ngx_http_fastcgi_header_tµÄ¸÷¸ö×Ö¶Î   ²Î¿¼ngx_http_fastcgi_process_record½âÎö¹ý³Ì ×é°ü¹ý³Ìngx_http_fastcgi_create_request
    //fastcgiÍ·²¿
    ngx_http_fastcgi_st_version = 0,
    ngx_http_fastcgi_st_type,
    ngx_http_fastcgi_st_request_id_hi,
    ngx_http_fastcgi_st_request_id_lo,
    ngx_http_fastcgi_st_content_length_hi,
    ngx_http_fastcgi_st_content_length_lo,
    ngx_http_fastcgi_st_padding_length,
    ngx_http_fastcgi_st_reserved,
    
    ngx_http_fastcgi_st_data, //fastcgiÄÚÈÝ
    ngx_http_fastcgi_st_padding //8×Ö½Ú¶ÔÆëÌî³ä×Ö¶Î
} ngx_http_fastcgi_state_e; //fastcgi±¨ÎÄ¸ñÊ½£¬Í·²¿(8×Ö½Ú)+ÄÚÈÝ(Ò»°ãÊÇ8ÄÚÈÝÍ·²¿+Êý¾Ý)+Ìî³ä×Ö¶Î(8×Ö½Ú¶ÔÆëÒýÆðµÄÌî³ä×Ö½ÚÊý) 
 

typedef struct {
    u_char                        *start;
    u_char                        *end;
} ngx_http_fastcgi_split_part_t; //´´½¨ºÍ¸³Öµ¼ûngx_http_fastcgi_process_header  Èç¹ûÒ»´Î½âÎöfastcgiÍ·²¿ÐÐÐÅÏ¢Ã»Íê³É£¬ÐèÒªÔÙ´Î¶ÁÈ¡ºó¶ËÊý¾Ý½âÎö

//ÔÚ½âÎö´Óºó¶Ë·¢ËÍ¹ýÀ´µÄfastcgiÍ·²¿ÐÅÏ¢µÄÊ±ºòÓÃµ½£¬¼ûngx_http_fastcgi_process_header
typedef struct { //ngx_http_fastcgi_handler·ÖÅä¿Õ¼ä
//ÓÃËûÀ´¼ÇÂ¼Ã¿´Î¶ÁÈ¡½âÎö¹ý³ÌÖÐµÄ¸÷¸ö×´Ì¬(Èç¹ûÐèÒª¶à´Îepoll´¥·¢¶ÁÈ¡£¬¾ÍÐèÒª¼ÇÂ¼Ç°Ãæ¶ÁÈ¡½âÎöÊ±ºòµÄ×´Ì¬)f = ngx_http_get_module_ctx(r, ngx_http_fastcgi_module);
    ngx_http_fastcgi_state_e       state; //±êÊ¶½âÎöµ½ÁËfastcgi 8×Ö½ÚÍ·²¿ÖÐµÄÄÇ¸öµØ·½
    u_char                        *pos; //Ö¸ÏòÒª½âÎöÄÚÈÝµÄÍ·
    u_char                        *last;//Ö¸ÏòÒª½âÎöÄÚÈÝµÄÎ²²¿
    ngx_uint_t                     type; //½»»¥±êÊ¶£¬ÀýÈçNGX_HTTP_FASTCGI_STDOUTµÈ
    size_t                         length; //¸ÃÌõfastcgiÐÅÏ¢µÄ°üÌåÄÚÈÝ³¤¶È ²»°üÀ¨paddingÌî³ä
    size_t                         padding; //Ìî³äÁË¶àÉÙ¸ö×Ö½Ú£¬´Ó¶ø8×Ö½Ú¶ÔÆë

    ngx_chain_t                   *free;
    ngx_chain_t                   *busy;

    unsigned                       fastcgi_stdout:1; //±êÊ¶ÓÐÊÕµ½fastcgi stdout±êÊ¶ÐÅÏ¢
    unsigned                       large_stderr:1; //±êÊ¶ÓÐÊÕµ½fastcgi stderr±êÊ¶ÐÅÏ¢
    unsigned                       header_sent:1;
    //´´½¨ºÍ¸³Öµ¼ûngx_http_fastcgi_process_header  Èç¹ûÒ»´Î½âÎöfastcgiÍ·²¿ÐÐÐÅÏ¢Ã»Íê³É£¬ÐèÒªÔÙ´Î¶ÁÈ¡ºó¶ËÊý¾Ý½âÎö
    ngx_array_t                   *split_parts;

    ngx_str_t                      script_name;
    ngx_str_t                      path_info;
} ngx_http_fastcgi_ctx_t; 
//ÓÃËûÀ´¼ÇÂ¼Ã¿´Î¶ÁÈ¡½âÎö¹ý³ÌÖÐµÄ¸÷¸ö×´Ì¬(Èç¹ûÐèÒª¶à´Îepoll´¥·¢¶ÁÈ¡£¬¾ÍÐèÒª¼ÇÂ¼Ç°Ãæ¶ÁÈ¡½âÎöÊ±ºòµÄ×´Ì¬)f = ngx_http_get_module_ctx(r, ngx_http_fastcgi_module);

#define NGX_HTTP_FASTCGI_KEEP_CONN      1  //NGX_HTTP_FASTCGI_RESPONDER±êÊ¶µÄfastcgi headerÖÐµÄflagÎª¸ÃÖµ±íÊ¾ºÍºó¶ËÊ¹ÓÃ³¤Á¬½Ó

//FASTCGI½»»¥Á÷³Ì±êÊ¶£¬¿ÉÒÔ²Î¿¼http://my.oschina.net/goal/blog/196599
#define NGX_HTTP_FASTCGI_RESPONDER      1 //µ½ºó¶Ë·þÎñÆ÷µÄ±êÊ¶ÐÅÏ¢ ²Î¿¼ngx_http_fastcgi_create_request  Õâ¸ö±êÊ¶Ð¯´ø³¤Á¬½Ó»¹ÊÇ¶ÌÁ¬½Óngx_http_fastcgi_request_start

#define NGX_HTTP_FASTCGI_BEGIN_REQUEST  1 //µ½ºó¶Ë·þÎñÆ÷µÄ±êÊ¶ÐÅÏ¢ ²Î¿¼ngx_http_fastcgi_create_request  ÇëÇó¿ªÊ¼ ngx_http_fastcgi_request_start
#define NGX_HTTP_FASTCGI_ABORT_REQUEST  2 
#define NGX_HTTP_FASTCGI_END_REQUEST    3 //ºó¶Ëµ½nginx ²Î¿¼ngx_http_fastcgi_process_record
#define NGX_HTTP_FASTCGI_PARAMS         4 //µ½ºó¶Ë·þÎñÆ÷µÄ±êÊ¶ÐÅÏ¢ ²Î¿¼ngx_http_fastcgi_create_request ¿Í»§¶ËÇëÇóÐÐÖÐµÄHTTP_xxÐÅÏ¢ºÍfastcgi_params²ÎÊýÍ¨¹ýËû·¢ËÍ
#define NGX_HTTP_FASTCGI_STDIN          5 //µ½ºó¶Ë·þÎñÆ÷µÄ±êÊ¶ÐÅÏ¢ ²Î¿¼ngx_http_fastcgi_create_request  ¿Í»§¶Ë·¢ËÍµ½·þÎñ¶ËµÄ°üÌåÓÃÕâ¸ö±êÊ¶

#define NGX_HTTP_FASTCGI_STDOUT         6 //ºó¶Ëµ½nginx ²Î¿¼ngx_http_fastcgi_process_record  ¸Ã±êÊ¶Ò»°ã»áÐ¯´øÊý¾Ý£¬Í¨¹ý½âÎöµ½µÄngx_http_fastcgi_ctx_t->length±íÊ¾Êý¾Ý³¤¶È
#define NGX_HTTP_FASTCGI_STDERR         7 //ºó¶Ëµ½nginx ²Î¿¼ngx_http_fastcgi_process_record
#define NGX_HTTP_FASTCGI_DATA           8  


/*
typedef struct {     
unsigned char version;     
unsigned char type;     
unsigned char requestIdB1;     
unsigned char requestIdB0;     
unsigned char contentLengthB1;     
unsigned char contentLengthB0;     
unsigned char paddingLength;     //Ìî³ä×Ö½ÚÊý
unsigned char reserved;    

unsigned char contentData[contentLength]; //ÄÚÈÝ²»·û
unsigned char paddingData[paddingLength];  //Ìî³ä×Ö·û
} FCGI_Record; 

*/
//fastcgi±¨ÎÄ¸ñÊ½£¬Í·²¿(8×Ö½Ú)+ÄÚÈÝ(Ò»°ãÊÇ8ÄÚÈÝÍ·²¿+Êý¾Ý)+Ìî³ä×Ö¶Î(8×Ö½Ú¶ÔÆëÒýÆðµÄÌî³ä×Ö½ÚÊý)  ¿ÉÒÔ²Î¿¼http://my.oschina.net/goal/blog/196599
typedef struct { //½âÎöµÄÊ±ºò¶ÔÓ¦Ç°ÃæµÄngx_http_fastcgi_state_e
    u_char  version;
    u_char  type; //NGX_HTTP_FASTCGI_BEGIN_REQUEST  µÈ
    u_char  request_id_hi;//ÐòÁÐºÅ£¬ÇëÇóÓ¦´ðÒ»°ãÒ»ÖÂ
    u_char  request_id_lo;
    u_char  content_length_hi; //ÄÚÈÝ×Ö½ÚÊý
    u_char  content_length_lo;
    u_char  padding_length; //Ìî³ä×Ö½ÚÊý
    u_char  reserved;//±£Áô×Ö¶Î
} ngx_http_fastcgi_header_t; //   ²Î¿¼ngx_http_fastcgi_process_record½âÎö¹ý³Ì ×é°ü¹ý³Ìngx_http_fastcgi_create_request


typedef struct {
    u_char  role_hi;
    u_char  role_lo; //NGX_HTTP_FASTCGI_RESPONDER»òÕß0
    u_char  flags;//NGX_HTTP_FASTCGI_KEEP_CONN»òÕß0  Èç¹ûÉèÖÃÁËºÍºó¶Ë³¤Á¬½Óflcf->keep_connÔòÎªNGX_HTTP_FASTCGI_KEEP_CONN·ñÔòÎª0£¬¼ûngx_http_fastcgi_create_request
    u_char  reserved[5];
} ngx_http_fastcgi_begin_request_t;//°üº¬ÔÚngx_http_fastcgi_request_start_t


typedef struct {
    u_char  version;
    u_char  type;
    u_char  request_id_hi;
    u_char  request_id_lo;
} ngx_http_fastcgi_header_small_t; //°üº¬ÔÚngx_http_fastcgi_request_start_t


typedef struct {
    ngx_http_fastcgi_header_t         h0;//ÇëÇó¿ªÊ¼Í·°üÀ¨Õý³£Í·£¬¼ÓÉÏ¿ªÊ¼ÇëÇóµÄÍ·²¿£¬
    ngx_http_fastcgi_begin_request_t  br;
    
    //ÕâÊÇÊ²Ã´å?Äª·ÇÊÇÏÂÒ»¸öÇëÇóµÄ²ÎÊý²¿·ÖµÄÍ·²¿£¬Ô¤ÏÈ×·¼ÓÔÚ´Ë?¶Ô£¬µ±ÎªNGX_HTTP_FASTCGI_PARAMSÄ£Ê½Ê±£¬ºóÃæÖ±½Ó×·¼ÓKV
    ngx_http_fastcgi_header_small_t   h1;
} ngx_http_fastcgi_request_start_t; //¼ûngx_http_fastcgi_request_start


static ngx_int_t ngx_http_fastcgi_eval(ngx_http_request_t *r,
    ngx_http_fastcgi_loc_conf_t *flcf);
#if (NGX_HTTP_CACHE)
static ngx_int_t ngx_http_fastcgi_create_key(ngx_http_request_t *r);
#endif
static ngx_int_t ngx_http_fastcgi_create_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_fastcgi_reinit_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_fastcgi_body_output_filter(void *data,
    ngx_chain_t *in);
static ngx_int_t ngx_http_fastcgi_process_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_fastcgi_input_filter_init(void *data);
static ngx_int_t ngx_http_fastcgi_input_filter(ngx_event_pipe_t *p,
    ngx_buf_t *buf);
static ngx_int_t ngx_http_fastcgi_non_buffered_filter(void *data,
    ssize_t bytes);
static ngx_int_t ngx_http_fastcgi_process_record(ngx_http_request_t *r,
    ngx_http_fastcgi_ctx_t *f);
static void ngx_http_fastcgi_abort_request(ngx_http_request_t *r);
static void ngx_http_fastcgi_finalize_request(ngx_http_request_t *r,
    ngx_int_t rc);

static ngx_int_t ngx_http_fastcgi_add_variables(ngx_conf_t *cf);
static void *ngx_http_fastcgi_create_main_conf(ngx_conf_t *cf);
static void *ngx_http_fastcgi_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_fastcgi_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);
static ngx_int_t ngx_http_fastcgi_init_params(ngx_conf_t *cf,
    ngx_http_fastcgi_loc_conf_t *conf, ngx_http_fastcgi_params_t *params,
    ngx_keyval_t *default_params);

static ngx_int_t ngx_http_fastcgi_script_name_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_fastcgi_path_info_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_http_fastcgi_ctx_t *ngx_http_fastcgi_split(ngx_http_request_t *r,
    ngx_http_fastcgi_loc_conf_t *flcf);

static char *ngx_http_fastcgi_pass(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_fastcgi_split_path_info(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
static char *ngx_http_fastcgi_store(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#if (NGX_HTTP_CACHE)
static char *ngx_http_fastcgi_cache(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_fastcgi_cache_key(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#endif

static char *ngx_http_fastcgi_lowat_check(ngx_conf_t *cf, void *post,
    void *data);


static ngx_conf_post_t  ngx_http_fastcgi_lowat_post =
    { ngx_http_fastcgi_lowat_check };


static ngx_conf_bitmask_t  ngx_http_fastcgi_next_upstream_masks[] = {
    { ngx_string("error"), NGX_HTTP_UPSTREAM_FT_ERROR },
    { ngx_string("timeout"), NGX_HTTP_UPSTREAM_FT_TIMEOUT },
    { ngx_string("invalid_header"), NGX_HTTP_UPSTREAM_FT_INVALID_HEADER },
    { ngx_string("http_500"), NGX_HTTP_UPSTREAM_FT_HTTP_500 },
    { ngx_string("http_503"), NGX_HTTP_UPSTREAM_FT_HTTP_503 },
    { ngx_string("http_403"), NGX_HTTP_UPSTREAM_FT_HTTP_403 },
    { ngx_string("http_404"), NGX_HTTP_UPSTREAM_FT_HTTP_404 },
    { ngx_string("updating"), NGX_HTTP_UPSTREAM_FT_UPDATING },
    { ngx_string("off"), NGX_HTTP_UPSTREAM_FT_OFF },
    { ngx_null_string, 0 }
};


ngx_module_t  ngx_http_fastcgi_module;


static ngx_command_t  ngx_http_fastcgi_commands[] = {
/*
Óï·¨£ºfastcgi_pass fastcgi-server 
Ä¬ÈÏÖµ£ºnone 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Ö¸¶¨FastCGI·þÎñÆ÷¼àÌý¶Ë¿ÚÓëµØÖ·£¬¿ÉÒÔÊÇ±¾»ú»òÕßÆäËü£º
fastcgi_pass   localhost:9000;

Ê¹ÓÃUnix socket:
fastcgi_pass   unix:/tmp/fastcgi.socket;

Í¬Ñù¿ÉÒÔÊ¹ÓÃÒ»¸öupstream×Ö¶ÎÃû³Æ£º
upstream backend  {
  server   localhost:1234;
}
 
fastcgi_pass   backend;
*/
    { ngx_string("fastcgi_pass"),
      NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
      ngx_http_fastcgi_pass,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
fastcgi_index 
Óï·¨£ºfastcgi_index file 
Ä¬ÈÏÖµ£ºnone 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Èç¹ûURIÒÔÐ±Ïß½áÎ²£¬ÎÄ¼þÃû½«×·¼Óµ½URIºóÃæ£¬Õâ¸öÖµ½«´æ´¢ÔÚ±äÁ¿$fastcgi_script_nameÖÐ¡£ÀýÈç£º

fastcgi_index  index.php;
fastcgi_param  SCRIPT_FILENAME  /home/www/scripts/php$fastcgi_script_name;ÇëÇó"/page.php"µÄ²ÎÊýSCRIPT_FILENAME½«±»ÉèÖÃÎª
"/home/www/scripts/php/page.php"£¬µ«ÊÇ"/"Îª"/home/www/scripts/php/index.php"¡£
*/
    { ngx_string("fastcgi_index"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, index),
      NULL },
/*
Óï·¨£ºfastcgi_split_path_info regex 
Ê¹ÓÃ×Ö¶Î£ºlocation 
¿ÉÓÃ°æ±¾£º0.7.31ÒÔÉÏ£¬Ê¾Àý£º

location ~ ^(.+\.php)(.*)$ {
...
fastcgi_split_path_info ^(.+\.php)(.*)$;
fastcgi_param SCRIPT_FILENAME /path/to/php$fastcgi_script_name;
fastcgi_param PATH_INFO $fastcgi_path_info;
fastcgi_param PATH_TRANSLATED $document_root$fastcgi_path_info;
...
}ÇëÇó"/show.php/article/0001"µÄ²ÎÊýSCRIPT_FILENAME½«ÉèÖÃÎª"/path/to/php/show.php"£¬²ÎÊýPATH_INFOÎª"/article/0001"¡£
*/
    { ngx_string("fastcgi_split_path_info"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_fastcgi_split_path_info,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
Óï·¨£ºfastcgi_store [on | off | path] 
Ä¬ÈÏÖµ£ºfastcgi_store off 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
ÖÆ¶¨ÁË´æ´¢Ç°¶ËÎÄ¼þµÄÂ·¾¶£¬²ÎÊýonÖ¸¶¨ÁË½«Ê¹ÓÃrootºÍaliasÖ¸ÁîÏàÍ¬µÄÂ·¾¶£¬off½ûÖ¹´æ´¢£¬´ËÍâ£¬²ÎÊýÖÐ¿ÉÒÔÊ¹ÓÃ±äÁ¿Ê¹Â·¾¶Ãû¸üÃ÷È·£º

fastcgi_store   /data/www$original_uri;Ó¦´ðÖÐµÄ"Last-Modified"Í·½«ÉèÖÃÎÄ¼þµÄ×îºóÐÞ¸ÄÊ±¼ä£¬ÎªÁËÊ¹ÕâÐ©ÎÄ¼þ¸ü¼Ó°²È«£¬¿ÉÒÔ½«ÆäÔÚÒ»¸öÄ¿Â¼ÖÐ´æÎªÁÙÊ±ÎÄ¼þ£¬Ê¹ÓÃfastcgi_temp_pathÖ¸Áî¡£
Õâ¸öÖ¸Áî¿ÉÒÔÓÃÔÚÎªÄÇÐ©²»ÊÇ¾­³£¸Ä±äµÄºó¶Ë¶¯Ì¬Êä³ö´´½¨±¾µØ¿½±´µÄ¹ý³ÌÖÐ¡£Èç£º

location /images/ {
  root                 /data/www;
  error_page           404 = /fetch$uri;
}
 
location /fetch {
  internal;
 
  fastcgi_pass           fastcgi://backend;
  fastcgi_store          on;
  fastcgi_store_access   user:rw  group:rw  all:r;
  fastcgi_temp_path      /data/temp;
 
  alias                  /data/www;
}fastcgi_store²¢²»ÊÇ»º´æ£¬Ä³Ð©ÐèÇóÏÂËü¸üÏñÊÇÒ»¸ö¾µÏñ¡£
*/   //fastcgi_storeºÍfastcgi_cacheÖ»ÄÜÅäÖÃÒ»¸ö
    { ngx_string("fastcgi_store"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_fastcgi_store,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
Óï·¨£ºfastcgi_store_access users:permissions [users:permission ...] 
Ä¬ÈÏÖµ£ºfastcgi_store_access user:rw 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Õâ¸ö²ÎÊýÖ¸¶¨´´½¨ÎÄ¼þ»òÄ¿Â¼µÄÈ¨ÏÞ£¬ÀýÈç£º

fastcgi_store_access  user:rw  group:rw  all:r;Èç¹ûÒªÖ¸¶¨Ò»¸ö×éµÄÈËµÄÏà¹ØÈ¨ÏÞ£¬¿ÉÒÔ²»Ð´ÓÃ»§£¬Èç£º
fastcgi_store_access  group:rw  all:r;
*/
    { ngx_string("fastcgi_store_access"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE123,
      ngx_conf_set_access_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.store_access),
      NULL },

/*
Óï·¨£ºfastcgi_buffers the_number is_size; 
Ä¬ÈÏÖµ£ºfastcgi_buffers 8 4k/8k; 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Õâ¸ö²ÎÊýÖ¸¶¨ÁË´ÓFastCGI·þÎñÆ÷µ½À´µÄÓ¦´ð£¬±¾µØ½«ÓÃ¶àÉÙºÍ¶à´óµÄ»º³åÇø¶ÁÈ¡£¬Ä¬ÈÏÕâ¸ö²ÎÊýµÈÓÚ·ÖÒ³´óÐ¡£¬¸ù¾Ý»·¾³µÄ²»Í¬¿ÉÄÜÊÇ4K, 8K»ò16K¡£
*/
    { ngx_string("fastcgi_buffering"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.buffering),
      NULL },

    //ÉèÖÃÊÇ·ñ»º´æÇëÇóbody µ½´ÅÅÌÊ±  ×¢Òâfastcgi_request_bufferingºÍfastcgi_bufferingµÄÇø±ð£¬Ò»¸öÊÇ¿Í»§¶Ë°üÌå£¬Ò»¸öÊÇºó¶Ë·þÎñÆ÷Ó¦´ðµÄ°üÌå
    { ngx_string("fastcgi_request_buffering"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.request_buffering),
      NULL },

/*
Óï·¨£ºfastcgi_ignore_client_abort on|off 
Ä¬ÈÏÖµ£ºfastcgi_ignore_client_abort off 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
"Èç¹ûµ±Ç°Á¬½ÓÇëÇóFastCGI·þÎñÆ÷Ê§°Ü£¬Îª·ÀÖ¹ÆäÓënginx·þÎñÆ÷¶Ï¿ªÁ¬½Ó£¬¿ÉÒÔÓÃÕâ¸öÖ¸Áî¡£
*/
    { ngx_string("fastcgi_ignore_client_abort"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.ignore_client_abort),
      NULL },

    { ngx_string("fastcgi_bind"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_upstream_bind_set_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.local),
      NULL },

/*
Óï·¨£ºfastcgi_connect_timeout time 
Ä¬ÈÏÖµ£ºfastcgi_connect_timeout 60 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Ö¸¶¨Í¬FastCGI·þÎñÆ÷µÄÁ¬½Ó³¬Ê±Ê±¼ä£¬Õâ¸öÖµ²»ÄÜ³¬¹ý75Ãë¡£
*/ //acceptºó¶Ë·þÎñÆ÷µÄÊ±ºò£¬ÓÐ¿ÉÄÜconnect·µ»ØNGX_AGAIN£¬±íÊ¾É¢²½ÎÕÊÖµÄack»¹Ã»ÓÐ»ØÀ´¡£Òò´ËÉèÖÃÕâ¸ö³¬Ê±ÊÂ¼þ±íÊ¾Èç¹û60s»¹Ã»ÓÐack¹ýÀ´£¬Ôò×ö³¬Ê±´¦Àí
    { ngx_string("fastcgi_connect_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.connect_timeout),
      NULL },
    //ÏòFastCGI´«ËÍÇëÇóµÄ³¬Ê±Ê±¼ä£¬Õâ¸öÖµÊÇÖ¸ÒÑ¾­Íê³ÉÁ½´ÎÎÕÊÖºóÏòFastCGI´«ËÍÇëÇóµÄ³¬Ê±Ê±¼ä¡£
    { ngx_string("fastcgi_send_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.send_timeout),
      NULL },

    { ngx_string("fastcgi_send_lowat"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.send_lowat),
      &ngx_http_fastcgi_lowat_post },

/*
Óï·¨£ºfastcgi_buffer_size the_size ;
Ä¬ÈÏÖµ£ºfastcgi_buffer_size 4k/8k ;
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Õâ¸ö²ÎÊýÖ¸¶¨½«ÓÃ¶à´óµÄ»º³åÇøÀ´¶ÁÈ¡´ÓFastCGI·þÎñÆ÷µ½À´Ó¦´ðµÄµÚÒ»²¿·Ö¡£
Í¨³£À´ËµÔÚÕâ¸ö²¿·ÖÖÐ°üº¬Ò»¸öÐ¡µÄÓ¦´ðÍ·¡£
Ä¬ÈÏµÄ»º³åÇø´óÐ¡Îªfastcgi_buffersÖ¸ÁîÖÐµÄÃ¿¿é´óÐ¡£¬¿ÉÒÔ½«Õâ¸öÖµÉèÖÃ¸üÐ¡¡£
*/ //Í·²¿ÐÐ²¿·Ö(Ò²¾ÍÊÇµÚÒ»¸öfastcgi data±êÊ¶ÐÅÏ¢£¬ÀïÃæÒ²»áÐ¯´øÒ»²¿·ÖÍøÒ³Êý¾Ý)µÄfastcgi±êÊ¶ÐÅÏ¢¿ª±ÙµÄ¿Õ¼äÓÃbuffer_sizeÅäÖÃÖ¸¶¨
//ngx_http_upstream_process_headerÖÐ·ÖÅäfastcgi_buffer_sizeÖ¸¶¨µÄ¿Õ¼ä
    { ngx_string("fastcgi_buffer_size"),  //×¢ÒâºÍºóÃæµÄfastcgi_buffersµÄÇø±ð  //Ö¸¶¨µÄ¿Õ¼ä¿ª±ÙÔÚngx_http_upstream_process_header
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.buffer_size),
      NULL },

    { ngx_string("fastcgi_pass_request_headers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.pass_request_headers),
      NULL },

    //ÊÇ·ñ×ª·¢¿Í»§¶Ëä¯ÀÀÆ÷¹ýÀ´µÄ°üÌåµ½ºó¶ËÈ¥
    { ngx_string("fastcgi_pass_request_body"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.pass_request_body),
      NULL },

/*
Óï·¨£ºfastcgi_intercept_errors on|off 
Ä¬ÈÏÖµ£ºfastcgi_intercept_errors off 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Õâ¸öÖ¸ÁîÖ¸¶¨ÊÇ·ñ´«µÝ4xxºÍ5xx´íÎóÐÅÏ¢µ½¿Í»§¶Ë£¬»òÕßÔÊÐínginxÊ¹ÓÃerror_page´¦Àí´íÎóÐÅÏ¢¡£
Äã±ØÐëÃ÷È·µÄÔÚerror_pageÖÐÖ¸¶¨´¦Àí·½·¨Ê¹Õâ¸ö²ÎÊýÓÐÐ§£¬ÕýÈçIgorËùËµ¡°Èç¹ûÃ»ÓÐÊÊµ±µÄ´¦Àí·½·¨£¬nginx²»»áÀ¹½ØÒ»¸ö´íÎó£¬Õâ¸ö´íÎó
²»»áÏÔÊ¾×Ô¼ºµÄÄ¬ÈÏÒ³Ãæ£¬ÕâÀïÔÊÐíÍ¨¹ýÄ³Ð©·½·¨À¹½Ø´íÎó¡£
*/
    { ngx_string("fastcgi_intercept_errors"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.intercept_errors),
      NULL },
/*
Óï·¨£ºfastcgi_read_timeout time 
Ä¬ÈÏÖµ£ºfastcgi_read_timeout 60 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Ç°¶ËFastCGI·þÎñÆ÷µÄÏìÓ¦³¬Ê±Ê±¼ä£¬Èç¹ûÓÐÒ»Ð©Ö±µ½ËüÃÇÔËÐÐÍê²ÅÓÐÊä³öµÄ³¤Ê±¼äÔËÐÐµÄFastCGI½ø³Ì£¬»òÕßÔÚ´íÎóÈÕÖ¾ÖÐ³öÏÖÇ°¶Ë·þÎñÆ÷ÏìÓ¦³¬
Ê±´íÎó£¬¿ÉÄÜÐèÒªµ÷ÕûÕâ¸öÖµ¡£
*/ //½ÓÊÕFastCGIÓ¦´ðµÄ³¬Ê±Ê±¼ä£¬Õâ¸öÖµÊÇÖ¸ÒÑ¾­Íê³ÉÁ½´ÎÎÕÊÖºó½ÓÊÕFastCGIÓ¦´ðµÄ³¬Ê±Ê±¼ä¡£
    { ngx_string("fastcgi_read_timeout"), //¶ÁÈ¡ºó¶ËÊý¾ÝµÄ³¬Ê±Ê±¼ä
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.read_timeout),
      NULL },

    /* ¶ÁÈ¡Íêºó¶ËµÄµÚÒ»¸öfastcgi data±êÊ¶Êý¾Ýºó(Ò»°ãÊÇÍ·²¿ÐÐºÍ²¿·ÖÊý¾Ý£¬¸Ã²¿·Ö¿Õ¼ä´óÐ¡ÓÃfastcgi_buffer_size)Ö¸¶¨£¬Èç¹ûÍøÒ³°üÌå±È½Ï´ó
        ÔòÐèÒª¿ª±Ù¶à¸öÐÂµÄbuffÀ´´æ´¢°üÌå²¿·Ö£¬·ÖÅäfastcgi_buffers 5 3K£¬±íÊ¾Èç¹ûÕâ²¿·Ö¿Õ¼äÓÃÍê(ÀýÈçºó¶ËËÙ¶È¿ì£¬µ½¿Í»§¶ËËÙ¶ÈÂý)£¬Ôò²»ÔÚ½ÓÊÕ
        ºó¶ËÊý¾Ý£¬µÈ´ý×Å5¸ö3KÖÐµÄ²¿·Ö·¢ËÍ³öÈ¥ºó£¬ÔÚÖØÐÂÓÃ¿ÕÓà³öÀ´µÄ¿Õ¼ä½ÓÊÕºó¶ËÊý¾Ý
     */ //×¢ÒâÕâ¸öÖ»¶Ôbuffing·½Ê½ÓÐÐ§  //¸ÃÅäÖÃÅäÖÃµÄ¿Õ¼äÕæÕý·ÖÅä¿Õ¼äÔÚ//ÔÚngx_event_pipe_read_upstreamÖÐ´´½¨¿Õ¼ä
    { ngx_string("fastcgi_buffers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_bufs_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.bufs),
      NULL },

    //xxx_buffersÖ¸¶¨Îª½ÓÊÕºó¶Ë·þÎñÆ÷Êý¾Ý×î¶à¿ª±ÙÕâÃ´¶à¿Õ¼ä£¬xxx_busy_buffers_sizeÖ¸¶¨Ò»´Î·¢ËÍºóÓÐ¿ÉÄÜÊý¾ÝÃ»ÓÐÈ«²¿·¢ËÍ³öÈ¥£¬Òò´Ë·ÅÈëbusyÁ´ÖÐ
    //µ±Ã»ÓÐ·¢ËÍ³öÈ¥µÄbusyÁ´ÖÐµÄbufÖ¸ÏòµÄÊý¾Ý(²»ÊÇbufÖ¸ÏòµÄ¿Õ¼äÎ´·¢ËÍµÄÊý¾Ý)´ïµ½xxx_busy_buffers_size¾Í²»ÄÜ´Óºó¶Ë¶ÁÈ¡Êý¾Ý£¬Ö»ÓÐbusyÁ´ÖÐµÄÊý¾Ý·¢ËÍÒ»²¿·Ö³öÈ¥ºóÐ¡Óëxxx_busy_buffers_size²ÅÄÜ¼ÌÐø¶ÁÈ¡
    //buffring·½Ê½ÏÂÉúÐ§£¬ÉúÐ§µØ·½¿ÉÒÔ²Î¿¼ngx_event_pipe_write_to_downstream
    { ngx_string("fastcgi_busy_buffers_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.busy_buffers_size_conf),
      NULL },

    { ngx_string("fastcgi_force_ranges"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.force_ranges),
      NULL },

    { ngx_string("fastcgi_limit_rate"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.limit_rate),
      NULL },

#if (NGX_HTTP_CACHE)
/*
       nginxµÄ´æ´¢ÏµÍ³·ÖÁ½Àà£¬Ò»ÀàÊÇÍ¨¹ýproxy_store¿ªÆôµÄ£¬´æ´¢·½Ê½ÊÇ°´ÕÕurlÖÐµÄÎÄ¼þÂ·¾¶£¬´æ´¢ÔÚ±¾µØ¡£±ÈÈç/file/2013/0001/en/test.html£¬
     ÄÇÃ´nginx¾Í»áÔÚÖ¸¶¨µÄ´æ´¢Ä¿Â¼ÏÂÒÀ´Î½¨Á¢¸÷¸öÄ¿Â¼ºÍÎÄ¼þ¡£ÁíÒ»ÀàÊÇÍ¨¹ýproxy_cache¿ªÆô£¬ÕâÖÖ·½Ê½´æ´¢µÄÎÄ¼þ²»ÊÇ°´ÕÕurlÂ·¾¶À´×éÖ¯µÄ£¬
     ¶øÊÇÊ¹ÓÃÒ»Ð©ÌØÊâ·½Ê½À´¹ÜÀíµÄ(ÕâÀï³ÆÎª×Ô¶¨Òå·½Ê½)£¬×Ô¶¨Òå·½Ê½¾ÍÊÇÎÒÃÇÒªÖØµã·ÖÎöµÄ¡£ÄÇÃ´ÕâÁ½ÖÖ·½Ê½¸÷ÓÐÊ²Ã´ÓÅÊÆÄØ£¿


    °´urlÂ·¾¶´æ´¢ÎÄ¼þµÄ·½Ê½£¬³ÌÐò´¦ÀíÆðÀ´±È½Ï¼òµ¥£¬µ«ÊÇÐÔÄÜ²»ÐÐ¡£Ê×ÏÈÓÐµÄurl¾Þ³¤£¬ÎÒÃÇÒªÔÚ±¾µØÎÄ¼þÏµÍ³ÉÏ½¨Á¢Èç´ËÉîµÄÄ¿Â¼£¬ÄÇÃ´ÎÄ¼þµÄ´ò¿ª
    ºÍ²éÕÒ¶¼ºÜ»áºÜÂý(»ØÏëkernelÖÐÍ¨¹ýÂ·¾¶Ãû²éÕÒinodeµÄ¹ý³Ì°É)¡£Èç¹ûÊ¹ÓÃ×Ô¶¨Òå·½Ê½À´´¦ÀíÄ£Ê½£¬¾¡¹ÜÒ²Àë²»¿ªÎÄ¼þºÍÂ·¾¶£¬µ«ÊÇËü²»»áÒòurl³¤¶È
    ¶ø²úÉú¸´ÔÓÐÔÔö¼ÓºÍÐÔÄÜµÄ½µµÍ¡£´ÓÄ³ÖÖÒâÒåÉÏËµÕâÊÇÒ»ÖÖÓÃ»§Ì¬ÎÄ¼þÏµÍ³£¬×îµäÐÍµÄÓ¦¸ÃËãÊÇsquidÖÐµÄCFS¡£nginxÊ¹ÓÃµÄ·½Ê½Ïà¶Ô¼òµ¥£¬Ö÷ÒªÒÀ¿¿
    urlµÄmd5ÖµÀ´¹ÜÀí
     */

/*
Óï·¨£ºfastcgi_cache zone|off; 
Ä¬ÈÏÖµ£ºoff 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Îª»º´æÊµ¼ÊÊ¹ÓÃµÄ¹²ÏíÄÚ´æÖ¸¶¨Ò»¸öÇøÓò£¬ÏàÍ¬µÄÇøÓò¿ÉÒÔÓÃÔÚ²»Í¬µÄµØ·½¡£
*/   //fastcgi_storeºÍfastcgi_cacheÖ»ÄÜÅäÖÃÒ»¸ö
//xxx_cache(proxy_cache fastcgi_cache) abc±ØÐëxxx_cache_path(proxy_cache_path fastcgi_cache_path) xxx keys_zone=abc:10m;Ò»Æð£¬·ñÔòÔÚngx_http_proxy_merge_loc_conf»áÊ§°Ü£¬ÒòÎªÃ»ÓÐÎª¸Ãabc´´½¨ngx_http_file_cache_t
//fastcgi_cache Ö¸ÁîÖ¸¶¨ÁËÔÚµ±Ç°×÷ÓÃÓòÖÐÊ¹ÓÃÄÄ¸ö»º´æÎ¬»¤»º´æÌõÄ¿£¬²ÎÊý¶ÔÓ¦µÄ»º´æ±ØÐëÊÂÏÈÓÉ fastcgi_cache_path Ö¸Áî¶¨Òå¡£ 
//»ñÈ¡¸Ã½á¹¹ngx_http_upstream_cache_get£¬Êµ¼ÊÉÏÊÇÍ¨¹ýproxy_cache xxx»òÕßfastcgi_cache xxxÀ´»ñÈ¡¹²ÏíÄÚ´æ¿éÃûµÄ£¬Òò´Ë±ØÐëÉèÖÃproxy_cache»òÕßfastcgi_cache

    { ngx_string("fastcgi_cache"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_fastcgi_cache,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
/*
Óï·¨£ºfastcgi_cache_key line
Ä¬ÈÏÖµ£ºnone 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
ÉèÖÃ»º´æµÄ¹Ø¼ü×Ö£¬Èç£º

fastcgi_cache_key localhost:9000$request_uri;
*/ //proxyºÍfastcgiÇø±ð:Default:  proxy_cache_key $scheme$proxy_host$request_uri; fastcgi_cache_keyÃ»ÓÐÄ¬ÈÏÖµ
    { ngx_string("fastcgi_cache_key"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_fastcgi_cache_key,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
Óï·¨£ºfastcgi_cache_path path [levels=m:n] keys_zone=name:size [inactive=time] [max_size=size] 
Ä¬ÈÏÖµ£ºnone 
Ê¹ÓÃ×Ö¶Î£ºhttp 
clean_time²ÎÊýÔÚ0.7.45°æ±¾ÖÐÒÑ¾­ÒÆ³ý¡£
Õâ¸öÖ¸ÁîÖ¸¶¨FastCGI»º´æµÄÂ·¾¶ÒÔ¼°ÆäËûµÄÒ»Ð©²ÎÊý£¬ËùÓÐµÄÊý¾ÝÒÔÎÄ¼þµÄÐÎÊ½´æ´¢£¬»º´æµÄ¹Ø¼ü×Ö(key)ºÍÎÄ¼þÃûÎª´úÀíµÄurl¼ÆËã³öµÄMD5Öµ¡£
Level²ÎÊýÉèÖÃ»º´æÄ¿Â¼µÄÄ¿Â¼·Ö¼¶ÒÔ¼°×ÓÄ¿Â¼µÄÊýÁ¿£¬ÀýÈçÖ¸ÁîÈç¹ûÉèÖÃÎª£º

fastcgi_cache_path  /data/nginx/cache  levels=1:2   keys_zone=one:10m;ÄÇÃ´Êý¾ÝÎÄ¼þ½«´æ´¢Îª£º
±ÈÈçlevels 2:2»áÉú³É256*256¸ö×ÖÄ¿Â¼£¬keys_zoneÊÇÕâ¸ö»º´æ¿Õ¼äµÄÃû×Ö
/data/nginx/cache/c/29/b7f54b2df7773722d382f4809d65029c»º´æÖÐµÄÎÄ¼þÊ×ÏÈ±»Ð´ÈëÒ»¸öÁÙÊ±ÎÄ¼þ²¢ÇÒËæºó±»ÒÆ¶¯µ½»º´æÄ¿Â¼µÄ×îºóÎ»ÖÃ£¬0.8.9°æ±¾
Ö®ºó¿ÉÒÔ½«ÁÙÊ±ÎÄ¼þºÍ»º´æÎÄ¼þ´æ´¢ÔÚ²»Í¬µÄÎÄ¼þÏµÍ³£¬µ«ÊÇÐèÒªÃ÷°×ÕâÖÖÒÆ¶¯²¢²»ÊÇ¼òµ¥µÄÔ­×ÓÖØÃüÃûÏµÍ³µ÷ÓÃ£¬¶øÊÇÕû¸öÎÄ¼þµÄ¿½±´£¬ËùÒÔ×îºÃ
ÔÚfastcgi_temp_pathºÍfastcgi_cache_pathµÄÖµÖÐÊ¹ÓÃÏàÍ¬µÄÎÄ¼þÏµÍ³¡£
ÁíÍâ£¬ËùÓÐ»î¶¯µÄ¹Ø¼ü×Ö¼°Êý¾ÝÏà¹ØÐÅÏ¢¶¼´æ´¢ÓÚ¹²ÏíÄÚ´æ³Ø£¬Õâ¸öÖµµÄÃû³ÆºÍ´óÐ¡Í¨¹ýkey_zone²ÎÊýÖ¸¶¨£¬inactive²ÎÊýÖ¸¶¨ÁËÄÚ´æÖÐµÄÊý¾Ý´æ´¢Ê±¼ä£¬Ä¬ÈÏÎª10·ÖÖÓ¡£
max_size²ÎÊýÉèÖÃ»º´æµÄ×î´óÖµ£¬Ò»¸öÖ¸¶¨µÄcache manager½ø³Ì½«ÖÜÆÚÐÔµÄÉ¾³ý¾ÉµÄ»º´æÊý¾Ý¡£
*/ //XXX_cache»º´æÊÇÏÈÐ´ÔÚxxx_temp_pathÔÙÒÆµ½xxx_cache_path£¬ËùÒÔÕâÁ½¸öÄ¿Â¼×îºÃÔÚÍ¬Ò»¸ö·ÖÇø
//xxx_cache(proxy_cache fastcgi_cache) abc±ØÐëxxx_cache_path(proxy_cache_path fastcgi_cache_path) xxx keys_zone=abc:10m;Ò»Æð£¬·ñÔòÔÚngx_http_proxy_merge_loc_conf»áÊ§°Ü£¬ÒòÎªÃ»ÓÐÎª¸Ãabc´´½¨ngx_http_file_cache_t
//fastcgi_cache Ö¸ÁîÖ¸¶¨ÁËÔÚµ±Ç°×÷ÓÃÓòÖÐÊ¹ÓÃÄÄ¸ö»º´æÎ¬»¤»º´æÌõÄ¿£¬²ÎÊý¶ÔÓ¦µÄ»º´æ±ØÐëÊÂÏÈÓÉ fastcgi_cache_path Ö¸Áî¶¨Òå¡£ 
//»ñÈ¡¸Ã½á¹¹ngx_http_upstream_cache_get£¬Êµ¼ÊÉÏÊÇÍ¨¹ýproxy_cache xxx»òÕßfastcgi_cache xxxÀ´»ñÈ¡¹²ÏíÄÚ´æ¿éÃûµÄ£¬Òò´Ë±ØÐëÉèÖÃproxy_cache»òÕßfastcgi_cache

/*
·Ç»º´æ·½Ê½(p->cacheable=0)p->temp_file->path = u->conf->temp_path; ÓÉngx_http_fastcgi_temp_pathÖ¸¶¨Â·¾¶
»º´æ·½Ê½(p->cacheable=1) p->temp_file->path = r->cache->file_cache->temp_path;¼ûproxy_cache_path»òÕßfastcgi_cache_path use_temp_path=Ö¸¶¨Â·¾¶ 
¼ûngx_http_upstream_send_response 

µ±Ç°fastcgi_buffers ºÍfastcgi_buffer_sizeÅäÖÃµÄ¿Õ¼ä¶¼ÒÑ¾­ÓÃÍêÁË£¬ÔòÐèÒª°ÑÊý¾ÝÐ´µÀÁÙÊ±ÎÄ¼þÖÐÈ¥£¬²Î¿¼ngx_event_pipe_read_upstream
*/  //´Óngx_http_file_cache_update¿ÉÒÔ¿´³ö£¬ºó¶ËÊý¾ÝÏÈÐ´µ½ÁÙÊ±ÎÄ¼þºó£¬ÔÚÐ´Èëxxx_cache_pathÖÐ£¬¼ûngx_http_file_cache_update
    { ngx_string("fastcgi_cache_path"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_2MORE,
      ngx_http_file_cache_set_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_main_conf_t, caches),
      &ngx_http_fastcgi_module },

    //XXX_cache_bypass  xx1 xx2ÉèÖÃµÄxx2²»Îª¿Õ»òÕß²»Îª0£¬Ôò²»»á´Ó»º´æÖÐÈ¡£¬¶øÊÇÖ±½Ó³åºó¶Ë¶ÁÈ¡  µ«ÊÇÕâÐ©ÇëÇóµÄºó¶ËÏìÓ¦Êý¾ÝÒÀÈ»¿ÉÒÔ±» upstream Ä£¿é»º´æ¡£ 
    //XXX_no_cache  xx1 xx2ÉèÖÃµÄxx2²»Îª¿Õ»òÕß²»Îª0£¬Ôòºó¶Ë»ØÀ´µÄÊý¾Ý²»»á±»»º´æ
    { ngx_string("fastcgi_cache_bypass"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_set_predicate_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.cache_bypass),
      NULL },

/*
Óï·¨£ºfastcgi_no_cache variable [...]
Ä¬ÈÏÖµ£ºNone 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
È·¶¨ÔÚºÎÖÖÇé¿öÏÂ»º´æµÄÓ¦´ð½«²»»áÊ¹ÓÃ£¬Ê¾Àý£º

fastcgi_no_cache $cookie_nocache  $arg_nocache$arg_comment;
fastcgi_no_cache $http_pragma     $http_authorization;Èç¹ûÎª¿Õ×Ö·û´®»òÕßµÈÓÚ0£¬±í´ïÊ½µÄÖµµÈÓÚfalse£¬ÀýÈç£¬ÔÚÉÏÊöÀý×ÓÖÐ£¬Èç¹û
ÔÚÇëÇóÖÐÉèÖÃÁËcookie "nocache"£¬»º´æ½«±»ÈÆ¹ý¡£
*/
    //XXX_cache_bypass  xx1 xx2ÉèÖÃµÄxx2²»Îª¿Õ»òÕß²»Îª0£¬Ôò²»»á´Ó»º´æÖÐÈ¡£¬¶øÊÇÖ±½Ó³åºó¶Ë¶ÁÈ¡
    //XXX_no_cache  xx1 xx2ÉèÖÃµÄxx2²»Îª¿Õ»òÕß²»Îª0£¬Ôòºó¶Ë»ØÀ´µÄÊý¾Ý²»»á±»»º´æ
    { ngx_string("fastcgi_no_cache"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_set_predicate_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.no_cache),
      NULL },

/*
Óï·¨£ºfastcgi_cache_valid [http_error_code|time] 
Ä¬ÈÏÖµ£ºnone 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
ÎªÖ¸¶¨µÄhttp·µ»Ø´úÂëÖ¸¶¨»º´æÊ±¼ä£¬ÀýÈç£º

fastcgi_cache_valid  200 302  10m;
fastcgi_cache_valid  404      1m;½«ÏìÓ¦×´Ì¬ÂëÎª200ºÍ302»º´æ10·ÖÖÓ£¬404»º´æ1·ÖÖÓ¡£
Ä¬ÈÏÇé¿öÏÂ»º´æÖ»´¦Àí200£¬301£¬302µÄ×´Ì¬¡£
Í¬ÑùÒ²¿ÉÒÔÔÚÖ¸ÁîÖÐÊ¹ÓÃany±íÊ¾ÈÎºÎÒ»¸ö¡£

fastcgi_cache_valid  200 302 10m;
fastcgi_cache_valid  301 1h;
fastcgi_cache_valid  any 1m;
*/
    { ngx_string("fastcgi_cache_valid"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_file_cache_valid_set_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.cache_valid),
      NULL },
/*
Óï·¨£ºfastcgi_cache_min_uses n 
Ä¬ÈÏÖµ£ºfastcgi_cache_min_uses 1 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Ö¸ÁîÖ¸¶¨ÁË¾­¹ý¶àÉÙ´ÎÇëÇóµÄÏàÍ¬URL½«±»»º´æ¡£
*/
    { ngx_string("fastcgi_cache_min_uses"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.cache_min_uses),
      NULL },

    { ngx_string("fastcgi_cache_use_stale"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.cache_use_stale),
      &ngx_http_fastcgi_next_upstream_masks },

/*
Óï·¨£ºfastcgi_cache_methods [GET HEAD POST]; 
Ä¬ÈÏÖµ£ºfastcgi_cache_methods GET HEAD; 
Ê¹ÓÃ×Ö¶Î£ºmain,http,location 
ÎÞ·¨½ûÓÃGET/HEAD £¬¼´Ê¹ÄãÖ»ÊÇÕâÑùÉèÖÃ£º
fastcgi_cache_methods  POST;
*/
    { ngx_string("fastcgi_cache_methods"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.cache_methods),
      &ngx_http_upstream_cache_method_mask },

/*
Õâ¸öÖ÷Òª½â¾öÒ»¸öÎÊÌâ:
¼ÙÉèÏÖÔÚÓÖÁ½¸ö¿Í»§¶Ë£¬Ò»¸ö¿Í»§¶ËÕýÔÚ»ñÈ¡ºó¶ËÊý¾Ý£¬²¢ÇÒºó¶Ë·µ»ØÁËÒ»²¿·Ö£¬Ôònginx»á»º´æÕâÒ»²¿·Ö£¬²¢ÇÒµÈ´ýËùÓÐºó¶ËÊý¾Ý·µ»Ø¼ÌÐø»º´æ¡£
µ«ÊÇÔÚ»º´æµÄ¹ý³ÌÖÐÈç¹û¿Í»§¶Ë2Ò³À´Ïëºó¶ËÈ¥Í¬ÑùµÄÊý¾ÝuriµÈ¶¼Ò»Ñù£¬Ôò»áÈ¥µ½¿Í»§¶Ë»º´æÒ»°ëµÄÊý¾Ý£¬ÕâÊ±ºò¾Í¿ÉÒÔÍ¨¹ý¸ÃÅäÖÃÀ´½â¾öÕâ¸öÎÊÌâ£¬
Ò²¾ÍÊÇ¿Í»§¶Ë1»¹Ã»»º´æÍêÈ«²¿Êý¾ÝµÄ¹ý³ÌÖÐ¿Í»§¶Ë2Ö»ÓÐµÈ¿Í»§¶Ë1»ñÈ¡ÍêÈ«²¿ºó¶ËÊý¾Ý£¬»òÕß»ñÈ¡µ½proxy_cache_lock_timeout³¬Ê±£¬Ôò¿Í»§¶Ë2Ö»ÓÐ´Óºó¶Ë»ñÈ¡Êý¾Ý
*/
    { ngx_string("fastcgi_cache_lock"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.cache_lock),
      NULL },

    { ngx_string("fastcgi_cache_lock_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.cache_lock_timeout),
      NULL },

    { ngx_string("fastcgi_cache_lock_age"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.cache_lock_age),
      NULL },

    { ngx_string("fastcgi_cache_revalidate"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.cache_revalidate),
      NULL },

#endif
        /*
    Ä¬ÈÏÇé¿öÏÂp->temp_file->path = u->conf->temp_path; Ò²¾ÍÊÇÓÉngx_http_fastcgi_temp_pathÖ¸¶¨Â·¾¶£¬µ«ÊÇÈç¹ûÊÇ»º´æ·½Ê½(p->cacheable=1)²¢ÇÒÅäÖÃ
    proxy_cache_path(fastcgi_cache_path) /a/bµÄÊ±ºò´øÓÐuse_temp_path=off(±íÊ¾²»Ê¹ÓÃngx_http_fastcgi_temp_pathÅäÖÃµÄpath)£¬
    Ôòp->temp_file->path = r->cache->file_cache->temp_path; Ò²¾ÍÊÇÁÙÊ±ÎÄ¼þ/a/b/temp¡£use_temp_path=off±íÊ¾²»Ê¹ÓÃngx_http_fastcgi_temp_path
    ÅäÖÃµÄÂ·¾¶£¬¶øÊ¹ÓÃÖ¸¶¨µÄÁÙÊ±Â·¾¶/a/b/temp   ¼ûngx_http_upstream_send_response 
    */
    ////XXX_cache»º´æÊÇÏÈÐ´ÔÚxxx_temp_pathÔÙÒÆµ½xxx_cache_path£¬ËùÒÔÕâÁ½¸öÄ¿Â¼×îºÃÔÚÍ¬Ò»¸ö·ÖÇø
    { ngx_string("fastcgi_temp_path"), //´Óngx_http_file_cache_update¿ÉÒÔ¿´³ö£¬ºó¶ËÊý¾ÝÏÈÐ´µ½ÁÙÊ±ÎÄ¼þºó£¬ÔÚÐ´Èëxxx_cache_pathÖÐ£¬¼ûngx_http_file_cache_update
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1234,
      ngx_conf_set_path_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.temp_path),
      NULL },

/*
ÔÚbuffering±êÖ¾Î»Îª1Ê±£¬Èç¹ûÉÏÓÎËÙ¶È¿ìÓÚÏÂÓÎËÙ¶È£¬½«ÓÐ¿ÉÄÜ°ÑÀ´×ÔÉÏÓÎµÄÏìÓ¦´æ´¢µ½ÁÙÊ±ÎÄ¼þÖÐ£¬¶ømax_temp_file_sizeÖ¸¶¨ÁËÁÙÊ±ÎÄ¼þµÄ
×î´ó³¤¶È¡£Êµ¼ÊÉÏ£¬Ëü½«ÏÞÖÆngx_event_pipe_t½á¹¹ÌåÖÐµÄtemp_file
*/
    { ngx_string("fastcgi_max_temp_file_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.max_temp_file_size_conf),
      NULL },
//±íÊ¾½«»º³åÇøÖÐµÄÏìÓ¦Ð´ÈëÁÙÊ±ÎÄ¼þÊ±Ò»´ÎÐ´Èë×Ö·ûÁ÷µÄ×î´ó³¤¶È
    { ngx_string("fastcgi_temp_file_write_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.temp_file_write_size_conf),
      NULL },
/*
Óï·¨£ºfastcgi_next_upstream error|timeout|invalid_header|http_500|http_503|http_404|off 
Ä¬ÈÏÖµ£ºfastcgi_next_upstream error timeout 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Ö¸ÁîÖ¸¶¨ÄÄÖÖÇé¿öÇëÇó½«±»×ª·¢µ½ÏÂÒ»¸öFastCGI·þÎñÆ÷£º
¡¤error ¡ª ´«ËÍÖÐµÄÇëÇó»òÕßÕýÔÚ¶ÁÈ¡Ó¦´ðÍ·µÄÇëÇóÔÚÁ¬½Ó·þÎñÆ÷µÄÊ±ºò·¢Éú´íÎó¡£
¡¤timeout ¡ª ´«ËÍÖÐµÄÇëÇó»òÕßÕýÔÚ¶ÁÈ¡Ó¦´ðÍ·µÄÇëÇóÔÚÁ¬½Ó·þÎñÆ÷µÄÊ±ºò³¬Ê±¡£
¡¤invalid_header ¡ª ·þÎñÆ÷·µ»Ø¿ÕµÄ»òÕßÎÞÐ§µÄÓ¦´ð¡£
¡¤http_500 ¡ª ·þÎñÆ÷·µ»Ø500Ó¦´ð´úÂë¡£
¡¤http_503 ¡ª ·þÎñÆ÷·µ»Ø503Ó¦´ð´úÂë¡£
¡¤http_404 ¡ª ·þÎñÆ÷·µ»Ø404Ó¦´ð´úÂë¡£
¡¤off ¡ª ½ûÖ¹ÇëÇó´«ËÍµ½ÏÂÒ»¸öFastCGI·þÎñÆ÷¡£
×¢Òâ´«ËÍÇëÇóÔÚ´«ËÍµ½ÏÂÒ»¸ö·þÎñÆ÷Ö®Ç°¿ÉÄÜÒÑ¾­½«¿ÕµÄÊý¾Ý´«ËÍµ½ÁË¿Í»§¶Ë£¬ËùÒÔ£¬Èç¹ûÔÚÊý¾Ý´«ËÍÖÐÓÐ´íÎó»òÕß³¬Ê±·¢Éú£¬Õâ¸öÖ¸Áî¿ÉÄÜ
ÎÞ·¨ÐÞ¸´Ò»Ð©´«ËÍ´íÎó¡£
*/
    { ngx_string("fastcgi_next_upstream"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.next_upstream),
      &ngx_http_fastcgi_next_upstream_masks },

    { ngx_string("fastcgi_next_upstream_tries"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.next_upstream_tries),
      NULL },

    { ngx_string("fastcgi_next_upstream_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.next_upstream_timeout),
      NULL },
/*
Óï·¨£ºfastcgi_param parameter value 
Ä¬ÈÏÖµ£ºnone 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Ö¸¶¨Ò»Ð©´«µÝµ½FastCGI·þÎñÆ÷µÄ²ÎÊý¡£
¿ÉÒÔÊ¹ÓÃ×Ö·û´®£¬±äÁ¿£¬»òÕßÆä×éºÏ£¬ÕâÀïµÄÉèÖÃ²»»á¼Ì³Ðµ½ÆäËûµÄ×Ö¶Î£¬ÉèÖÃÔÚµ±Ç°×Ö¶Î»áÇå³ýµôÈÎºÎÖ®Ç°µÄ¶¨Òå¡£
ÏÂÃæÊÇÒ»¸öPHPÐèÒªÊ¹ÓÃµÄ×îÉÙ²ÎÊý£º

  fastcgi_param  SCRIPT_FILENAME  /home/www/scripts/php$fastcgi_script_name;
  fastcgi_param  QUERY_STRING     $query_string;
  PHPÊ¹ÓÃSCRIPT_FILENAME²ÎÊý¾ö¶¨ÐèÒªÖ´ÐÐÄÄ¸ö½Å±¾£¬QUERY_STRING°üº¬ÇëÇóÖÐµÄÄ³Ð©²ÎÊý¡£
Èç¹ûÒª´¦ÀíPOSTÇëÇó£¬ÔòÐèÒªÁíÍâÔö¼ÓÈý¸ö²ÎÊý£º

  fastcgi_param  REQUEST_METHOD   $request_method;
  fastcgi_param  CONTENT_TYPE     $content_type;
  fastcgi_param  CONTENT_LENGTH   $content_length;Èç¹ûPHPÔÚ±àÒëÊ±´øÓÐ--enable-force-cgi-redirect£¬Ôò±ØÐë´«µÝÖµÎª200µÄREDIRECT_STATUS²ÎÊý£º

fastcgi_param  REDIRECT_STATUS  200;
*/
    { ngx_string("fastcgi_param"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE23,
      ngx_http_upstream_param_set_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, params_source),
      NULL },

    { ngx_string("fastcgi_pass_header"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.pass_headers),
      NULL },
/*
Óï·¨£ºfastcgi_hide_header name 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Ä¬ÈÏÇé¿öÏÂnginx²»»á½«À´×ÔFastCGI·þÎñÆ÷µÄ"Status"ºÍ"X-Accel-..."Í·´«ËÍµ½¿Í»§¶Ë£¬Õâ¸ö²ÎÊýÒ²¿ÉÒÔÒþ²ØÄ³Ð©ÆäËüµÄÍ·¡£
Èç¹û±ØÐë´«µÝ"Status"ºÍ"X-Accel-..."Í·£¬Ôò±ØÐëÊ¹ÓÃfastcgi_pass_headerÇ¿ÖÆÆä´«ËÍµ½¿Í»§¶Ë¡£
*/
    { ngx_string("fastcgi_hide_header"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.hide_headers),
      NULL },
/*
Óï·¨£ºfastcgi_ignore_headers name [name...] 
Ê¹ÓÃ×Ö¶Î£ºhttp, server, location 
Õâ¸öÖ¸Áî½ûÖ¹´¦ÀíÒ»Ð©FastCGI·þÎñÆ÷Ó¦´ðµÄÍ·²¿×Ö¶Î£¬±ÈÈç¿ÉÒÔÖ¸¶¨Ïñ"X-Accel-Redirect", "X-Accel-Expires", "Expires"»ò"Cache-Control"µÈ¡£
If not disabled, processing of these header fields has the following effect:

¡°X-Accel-Expires¡±, ¡°Expires¡±, ¡°Cache-Control¡±, ¡°Set-Cookie¡±, and ¡°Vary¡± set the parameters of response caching;
¡°X-Accel-Redirect¡± performs an internal redirect to the specified URI;
¡°X-Accel-Limit-Rate¡± sets the rate limit for transmission of a response to a client;
¡°X-Accel-Buffering¡± enables or disables buffering of a response;
¡°X-Accel-Charset¡± sets the desired charset of a response.
*/
    { ngx_string("fastcgi_ignore_headers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, upstream.ignore_headers),
      &ngx_http_upstream_ignore_headers_masks },

/*
Sets a string to search for in the error stream of a response received from a FastCGI server. If the string is found then it is 
considered that the FastCGI server has returned an invalid response. This allows handling application errors in nginx, for example: 

location /php {
    fastcgi_pass backend:9000;
    ...
    fastcgi_catch_stderr "PHP Fatal error";
    fastcgi_next_upstream error timeout invalid_header;
}
*/
    { ngx_string("fastcgi_catch_stderr"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, catch_stderr),
      NULL },

    { ngx_string("fastcgi_keep_conn"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fastcgi_loc_conf_t, keep_conn),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_fastcgi_module_ctx = {
    ngx_http_fastcgi_add_variables,        /* preconfiguration */
    NULL,                                  /* postconfiguration */

    ngx_http_fastcgi_create_main_conf,     /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_fastcgi_create_loc_conf,      /* create location configuration */
    ngx_http_fastcgi_merge_loc_conf        /* merge location configuration */
};

/*
step1. web ·þÎñÆ÷ÊÕµ½¿Í»§¶Ë£¨ä¯ÀÀÆ÷£©µÄÇëÇóHttp Request£¬Æô¶¯CGI³ÌÐò£¬²¢Í¨¹ý»·¾³±äÁ¿¡¢±ê×¼ÊäÈë´«µÝÊý¾Ý
step2. cgi½ø³ÌÆô¶¯½âÎöÆ÷¡¢¼ÓÔØÅäÖÃ£¨ÈçÒµÎñÏà¹ØÅäÖÃ£©¡¢Á¬½ÓÆäËü·þÎñÆ÷£¨ÈçÊý¾Ý¿â·þÎñÆ÷£©¡¢Âß¼­´¦ÀíµÈ
step3. cgi³Ì½«´¦Àí½á¹ûÍ¨¹ý±ê×¼Êä³ö¡¢±ê×¼´íÎó£¬´«µÝ¸øweb ·þÎñÆ÷
step4. web ·þÎñÆ÷ÊÕµ½cgi·µ»ØµÄ½á¹û£¬¹¹½¨Http Response·µ»Ø¸ø¿Í»§¶Ë£¬²¢É±ËÀcgi½ø³Ì
*/ //http://chenzhenianqing.cn/articles/category/%e5%90%84%e7%a7%8dserver/nginx
ngx_module_t  ngx_http_fastcgi_module = {
    NGX_MODULE_V1,
    &ngx_http_fastcgi_module_ctx,          /* module context */
    ngx_http_fastcgi_commands,             /* module directives */
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


static ngx_http_fastcgi_request_start_t  ngx_http_fastcgi_request_start = { //²Î¿¼ngx_http_fastcgi_create_request
    //¸ÃÍ·²¿±íÊ¾nginx¿ªÊ¼ÇëÇó, ÇëÇóÓÉFCGI_BEGIN_REQUEST¿ªÊ¼
    { 1,                                               /* version */
      NGX_HTTP_FASTCGI_BEGIN_REQUEST,                  /* type */
      0,                                               /* request_id_hi */
      1,                                               /* request_id_lo */
      0,                                               /* content_length_hi */
      sizeof(ngx_http_fastcgi_begin_request_t),        /* content_length_lo */
      0,                                               /* padding_length */
      0 },                                             /* reserved */

    //¸ÃÍ·²¿ËµÃ÷ÊÇ·ñºÍºó¶Ë²ÉÓÃ³¤Á¬½Ó
    { 0,                                               /* role_hi */
      NGX_HTTP_FASTCGI_RESPONDER,                      /* role_lo */
      0, /* NGX_HTTP_FASTCGI_KEEP_CONN */              /* flags */
      { 0, 0, 0, 0, 0 } },                             /* reserved[5] */

    //params²ÎÊýÍ·²¿µÄÇ°4×Ö½Ú£¬Ê£ÓàµÄÈ«²¿ÔÚ²ÎÊýÖÐÒ»ÆðÌî³ä£¬¿ÉÒÔ²Î¿¼ngx_http_fastcgi_create_request
    { 1,                                               /* version */
      NGX_HTTP_FASTCGI_PARAMS,                         /* type */
      0,                                               /* request_id_hi */
      1 },                                             /* request_id_lo */

};


static ngx_http_variable_t  ngx_http_fastcgi_vars[] = {

    { ngx_string("fastcgi_script_name"), NULL,
      ngx_http_fastcgi_script_name_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    { ngx_string("fastcgi_path_info"), NULL,
      ngx_http_fastcgi_path_info_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_NOHASH, 0 },

    { ngx_null_string, NULL, NULL, 0, 0, 0 }
};

//×îÖÕÌí¼Óµ½ÁËngx_http_upstream_conf_t->hide_headers_hash±íÖÐ  ²»ÐèÒª·¢ËÍ¸ø¿Í»§¶Ë
static ngx_str_t  ngx_http_fastcgi_hide_headers[] = {
    ngx_string("Status"),
    ngx_string("X-Accel-Expires"),
    ngx_string("X-Accel-Redirect"),
    ngx_string("X-Accel-Limit-Rate"),
    ngx_string("X-Accel-Buffering"),
    ngx_string("X-Accel-Charset"),
    ngx_null_string
};


#if (NGX_HTTP_CACHE)

static ngx_keyval_t  ngx_http_fastcgi_cache_headers[] = {
    { ngx_string("HTTP_IF_MODIFIED_SINCE"),
      ngx_string("$upstream_cache_last_modified") },
    { ngx_string("HTTP_IF_UNMODIFIED_SINCE"), ngx_string("") },
    { ngx_string("HTTP_IF_NONE_MATCH"), ngx_string("$upstream_cache_etag") },
    { ngx_string("HTTP_IF_MATCH"), ngx_string("") },
    { ngx_string("HTTP_RANGE"), ngx_string("") },
    { ngx_string("HTTP_IF_RANGE"), ngx_string("") },
    { ngx_null_string, ngx_null_string }
};

#endif


static ngx_path_init_t  ngx_http_fastcgi_temp_path = {
    ngx_string(NGX_HTTP_FASTCGI_TEMP_PATH), { 1, 2, 0 }
};

/*
ngx_http_fastcgi_handlerº¯Êý×÷Îªnginx¶ÁÈ¡ÇëÇóµÄheaderÍ·²¿ºó,¾Í»áµ÷ÓÃngx_http_core_content_phase½øÒ»²½µ÷ÓÃµ½ÕâÀï£¬¿ÉÒÔ¿´µ½upstream»¹Ã»ÓÐµ½£¬
ÆäÊµupstreamÊÇÓÉÕâÐ©fastcgiÄ£¿é»òÕßproxyÄ£¿éÊ¹ÓÃµÄ¡£»òÕßËµ»¥ÏàÊ¹ÓÃ£ºfastcgiÆô¶¯upstream£¬ÉèÖÃÏà¹ØµÄ»Øµ÷£¬È»ºóupstream»áµ÷ÓÃÕâÐ©»Øµ÷Íê³É¹¦ÄÜ
*/
static ngx_int_t
ngx_http_fastcgi_handler(ngx_http_request_t *r)
{//FCGI´¦ÀíÈë¿Ú,ngx_http_core_run_phasesÀïÃæµ±×öÒ»¸öÄÚÈÝ´¦ÀíÄ£¿éµ÷ÓÃµÄ¡£(NGX_HTTP_CONTENT_PHASE½×¶ÎÖ´ÐÐ)£¬Êµ¼Ê¸³ÖµÔÚ:
//ngx_http_core_find_config_phaseÀïÃæµÄngx_http_update_location_configÉèÖÃ¡£ÕæÕýµ÷ÓÃ¸Ãº¯ÊýµÄµØ·½ÊÇngx_http_core_content_phase->ngx_http_finalize_request(r, r->content_handler(r)); 
    ngx_int_t                      rc;
    ngx_http_upstream_t           *u;
    ngx_http_fastcgi_ctx_t        *f;
    ngx_http_fastcgi_loc_conf_t   *flcf;
#if (NGX_HTTP_CACHE)
    ngx_http_fastcgi_main_conf_t  *fmcf;
#endif

    //´´½¨Ò»¸öngx_http_upstream_t½á¹¹£¬·Åµ½r->upstreamÀïÃæÈ¥¡£
    if (ngx_http_upstream_create(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    f = ngx_pcalloc(r->pool, sizeof(ngx_http_fastcgi_ctx_t)); //´´½¨fastcgiËùÊôµÄupstreamÉÏÏÂÎÄngx_http_fastcgi_ctx_t
    if (f == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_http_set_ctx(r, f, ngx_http_fastcgi_module); //³ýÁËÅäÖÃÖÐµÄ²ÎÊý¿ÉÒÔÉèÖÃctxÍâ£¬ÆäËûÐèÒªµÄ²ÎÊýÒ²¿ÉÒÔÍ¨¹ýr->ctx[]Êý×éÀ´ÉèÖÃ£¬´Ó¶ø¿ÉÒÔµÃµ½±£´æ£¬Ö»ÒªÖªµÀr£¬¾Í¿ÉÒÔÍ¨¹ýr->ctx[]»ñÈ¡µ½

    flcf = ngx_http_get_module_loc_conf(r, ngx_http_fastcgi_module);//µÃµ½fcgiµÄÅäÖÃ¡£(r)->loc_conf[module.ctx_index]

    if (flcf->fastcgi_lengths) {//Èç¹ûÕâ¸öfcgiÓÐ±äÁ¿£¬ÄÇÃ´¾ÃÐèÒª½âÎöÒ»ÏÂ±äÁ¿¡£
        if (ngx_http_fastcgi_eval(r, flcf) != NGX_OK) { //¼ÆËãfastcgi_pass   127.0.0.1:9000;ºóÃæµÄURLµÄÄÚÈÝ¡£Ò²¾ÍÊÇÓòÃû½âÎö
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    u = r->upstream; //¾ÍÊÇÉÏÃængx_http_upstream_createÖÐ´´½¨µÄ

    ngx_str_set(&u->schema, "fastcgi://");
    u->output.tag = (ngx_buf_tag_t) &ngx_http_fastcgi_module;

    u->conf = &flcf->upstream;

#if (NGX_HTTP_CACHE)
    fmcf = ngx_http_get_module_main_conf(r, ngx_http_fastcgi_module);

    u->caches = &fmcf->caches;
    u->create_key = ngx_http_fastcgi_create_key;
#endif

    u->create_request = ngx_http_fastcgi_create_request; //ÔÚngx_http_upstream_init_requestÖÐÖ´ÐÐ
    u->reinit_request = ngx_http_fastcgi_reinit_request; //ÔÚngx_http_upstream_reinitÖÐÖ´ÐÐ
    u->process_header = ngx_http_fastcgi_process_header; //ÔÚngx_http_upstream_process_headerÖÐÖ´ÐÐ
    u->abort_request = ngx_http_fastcgi_abort_request;  
    u->finalize_request = ngx_http_fastcgi_finalize_request; //ÔÚngx_http_upstream_finalize_requestÖÐÖ´ÐÐ
    r->state = 0;

    //ÏÂÃæµÄÊý¾Ý½á¹¹ÊÇ¸øevent_pipeÓÃµÄ£¬ÓÃÀ´¶ÔFCGIµÄÊý¾Ý½øÐÐbuffering´¦ÀíµÄ¡£
    u->buffering = flcf->upstream.buffering; //Ä¬ÈÏÎª1
    
    u->pipe = ngx_pcalloc(r->pool, sizeof(ngx_event_pipe_t));
    if (u->pipe == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    //ÉèÖÃ¶ÁÈ¡fcgiÐ­Òé¸ñÊ½Êý¾ÝµÄ»Øµ÷£¬µ±½âÎöÍê´øÓÐ\r\n\r\nµÄÍ·²¿µÄFCGI°üºó£¬ºóÃæµÄ°ü½âÎö¶¼ÓÉÕâ¸öº¯Êý½øÐÐ´¦Àí¡£
    u->pipe->input_filter = ngx_http_fastcgi_input_filter;
    u->pipe->input_ctx = r;

    u->input_filter_init = ngx_http_fastcgi_input_filter_init;
    u->input_filter = ngx_http_fastcgi_non_buffered_filter;
    u->input_filter_ctx = r;

    if (!flcf->upstream.request_buffering
        && flcf->upstream.pass_request_body)
    { //Èç¹ûÐèÒªÍ¸´«²¢ÇÒ²»ÐèÒª»»³É°üÌå
        r->request_body_no_buffering = 1;
    }

    //¶ÁÈ¡ÇëÇó°üÌå
    rc = ngx_http_read_client_request_body(r, ngx_http_upstream_init); //¶ÁÈ¡Íê¿Í»§¶Ë·¢ËÍÀ´µÄ°üÌåºóÖ´ÐÐngx_http_upstream_init

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    return NGX_DONE;
}

//¼ÆËãfastcgi_pass   127.0.0.1:9000;ºóÃæµÄURLÄÚÈÝ£¬ÉèÖÃµ½u->resolvedÉÏÃæÈ¥
static ngx_int_t
ngx_http_fastcgi_eval(ngx_http_request_t *r, ngx_http_fastcgi_loc_conf_t *flcf)
{
    ngx_url_t             url;
    ngx_http_upstream_t  *u;

    ngx_memzero(&url, sizeof(ngx_url_t));
    //¸ù¾ÝlcodesºÍcodes¼ÆËãÄ¿±ê×Ö·û´®µÄÄÚÈÝ¡¢Ä¿±ê×Ö·û´®½á¹û´æ·ÅÔÚvalue->data;ÀïÃæ£¬Ò²¾ÍÊÇurl.url
    if (ngx_http_script_run(r, &url.url, flcf->fastcgi_lengths->elts, 0,
                            flcf->fastcgi_values->elts)
        == NULL)
    {
        return NGX_ERROR;
    }

    url.no_resolve = 1;

    if (ngx_parse_url(r->pool, &url) != NGX_OK) {//¶Ôu²ÎÊýÀïÃæµÄurl,unix,inet6µÈµØÖ·½øÐÐ¼òÎö£»
         if (url.err) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "%s in upstream \"%V\"", url.err, &url.url);
        }

        return NGX_ERROR;
    }

    u = r->upstream;

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
        u->resolved->port = url.port;
        u->resolved->no_port = url.no_port;
    }

    return NGX_OK;
}


#if (NGX_HTTP_CACHE)

//½âÎöfastcgi_cache_key xxx ²ÎÊýÖµµ½r->cache->keys
static ngx_int_t //ngx_http_upstream_cacheÖÐÖ´ÐÐ
ngx_http_fastcgi_create_key(ngx_http_request_t *r)
{//¸ù¾ÝÖ®Ç°ÔÚ½âÎöscgi_cache_keyÖ¸ÁîµÄÊ±ºò¼ÆËã³öÀ´µÄ¸´ÔÓ±í´ïÊ½½á¹¹£¬´æ·ÅÔÚflcf->cache_keyÖÐµÄ£¬¼ÆËã³öcache_key¡£
    ngx_str_t                    *key;
    ngx_http_fastcgi_loc_conf_t  *flcf;

    key = ngx_array_push(&r->cache->keys);
    if (key == NULL) {
        return NGX_ERROR;
    }

    flcf = ngx_http_get_module_loc_conf(r, ngx_http_fastcgi_module);

    //´Óflcf->cache_key(fastcgi_cache_keyÅäÖÃÏî)ÖÐ½âÎö³ö¶ÔÓ¦codeº¯ÊýµÄÏà¹Ø±äÁ¿×Ö·û´®£¬´æµ½ r->cache->keys
    if (ngx_http_complex_value(r, &flcf->cache_key, key) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

#endif

/*
2025/03/22 03:55:55[    ngx_http_core_post_access_phase,  2163]  [debug] 2357#2357: *3 post access phase: 8 (NGX_HTTP_POST_ACCESS_PHASE)
2025/03/22 03:55:55[        ngx_http_core_content_phase,  2485]  [debug] 2357#2357: *3 content phase(content_handler): 9 (NGX_HTTP_CONTENT_PHASE)
2025/03/22 03:55:55[             ngx_http_upstream_init,   617]  [debug] 2357#2357: *3 http init upstream, client timer: 0
2025/03/22 03:55:55[                ngx_epoll_add_event,  1398]  [debug] 2357#2357: *3 epoll add event: fd:3 op:3 ev:80002005
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "SCRIPT_FILENAME"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "/var/yyz/www"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "/"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "/test.php"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "SCRIPT_FILENAME: /var/yyz/www//test.php"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "QUERY_STRING" //¿Õ±äÁ¿£¬Ã»value£¬Ò²·¢ËÍÁË£¬Èç¹û¼ÓÉÏif_no_emputy²ÎÊý¾Í²»»á·¢ËÍ
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "QUERY_STRING: "
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "REQUEST_METHOD"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "GET"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "REQUEST_METHOD: GET"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "CONTENT_TYPE"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "CONTENT_TYPE: "
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "CONTENT_LENGTH"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "CONTENT_LENGTH: "
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "SCRIPT_NAME"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "/test.php"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "SCRIPT_NAME: /test.php"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "REQUEST_URI"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "/test.php"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "REQUEST_URI: /test.php"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "DOCUMENT_URI"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "/test.php"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "DOCUMENT_URI: /test.php"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "DOCUMENT_ROOT"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "/var/yyz/www"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "DOCUMENT_ROOT: /var/yyz/www"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "SERVER_PROTOCOL"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "HTTP/1.1"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "SERVER_PROTOCOL: HTTP/1.1"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "REQUEST_SCHEME"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "http"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "REQUEST_SCHEME: http"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: ""
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "GATEWAY_INTERFACE"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "CGI/1.1"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "GATEWAY_INTERFACE: CGI/1.1"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "SERVER_SOFTWARE"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "nginx/"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "1.9.2"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "SERVER_SOFTWARE: nginx/1.9.2"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "REMOTE_ADDR"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "10.2.13.1"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "REMOTE_ADDR: 10.2.13.1"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "REMOTE_PORT"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "52365"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "REMOTE_PORT: 52365"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "SERVER_ADDR"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "10.2.13.167"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "SERVER_ADDR: 10.2.13.167"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "SERVER_PORT"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "80"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "SERVER_PORT: 80"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "SERVER_NAME"
2025/03/22 03:55:55[      ngx_http_script_copy_var_code,   988]  [debug] 2357#2357: *3 http script var: "localhost"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "SERVER_NAME: localhost"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "REDIRECT_STATUS"
2025/03/22 03:55:55[          ngx_http_script_copy_code,   864]  [debug] 2357#2357: *3 http script copy: "200"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1353]  [debug] 2357#2357: *3 fastcgi param: "REDIRECT_STATUS: 200"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1426]  [debug] 2357#2357: *3 fastcgi param: "HTTP_ACCEPT: application/x-ms-application, image/jpeg, application/xaml+xml, image/gif, image/pjpeg, application/x-ms-xbap, application/vnd.ms-excel, application/vnd.ms-powerpoint, application/msword, * / *"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1426]  [debug] 2357#2357: *3 fastcgi param: "HTTP_ACCEPT_LANGUAGE: zh-CN"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1426]  [debug] 2357#2357: *3 fastcgi param: "HTTP_USER_AGENT: Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; WOW64; Trident/4.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; Media Center PC 6.0; InfoPath.3)"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1426]  [debug] 2357#2357: *3 fastcgi param: "HTTP_ACCEPT_ENCODING: gzip, deflate"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1426]  [debug] 2357#2357: *3 fastcgi param: "HTTP_HOST: 10.2.13.167"
2025/03/22 03:55:55[    ngx_http_fastcgi_create_request,  1426]  [debug] 2357#2357: *3 fastcgi param: "HTTP_CONNECTION: Keep-Alive"
2025/03/22 03:55:55[               ngx_http_cleanup_add,  3986]  [debug] 2357#2357: *3 http cleanup add: 080EE06C
2025/03/22 03:55:55[ngx_http_upstream_get_round_robin_peer,   429]  [debug] 2357#2357: *3 get rr peer, try: 1
2025/03/22 03:55:55[             ngx_event_connect_peer,    32]  [debug] 2357#2357: *3 socket 11
2025/03/22 03:55:55[           ngx_epoll_add_connection,  1483]  [debug] 2357#2357: *3 epoll add connection: fd:11 ev:80002005
2025/03/22 03:55:55[             ngx_event_connect_peer,   125]  [debug] 2357#2357: *3 connect to 127.0.0.1:3666, fd:11 #4
2025/03/22 03:55:55[          ngx_http_upstream_connect,  1520]  [debug] 2357#2357: *3 http upstream connect: -2
2025/03/22 03:55:55[                ngx_event_add_timer,    88]  [debug] 2357#2357: *3 <ngx_http_upstream_connect,  1624>  event timer add: 11: 60000:3125260832
ÉÏÃæ·¢ËÍÁËºÜ¶àvalueÎª¿ÕµÄ±äÁ¿£¬¼ÓÉÏif_no_emputy¿ÉÒÔ±ÜÃâ·¢ËÍ¿Õ±äÁ¿
*/ 
//ÉèÖÃFCGIµÄ¸÷ÖÖÇëÇó¿ªÊ¼£¬ÇëÇóÍ·²¿£¬HTTP BODYÊý¾Ý²¿·ÖµÄ¿½±´£¬²ÎÊý¿½±´µÈ¡£ºóÃæ»ù±¾¾Í¿ÉÒÔ·¢ËÍÊý¾ÝÁË
//´æ·ÅÔÚu->request_bufsÁ´½Ó±íÀïÃæ¡£
static ngx_int_t //ngx_http_fastcgi_create_requestºÍngx_http_fastcgi_init_paramsÅä¶ÔÔÄ¶Á
ngx_http_fastcgi_create_request(ngx_http_request_t *r) //ngx_http_upstream_init_requestÖÐÖ´ÐÐ¸Ãº¯Êý
{
    off_t                         file_pos;
    u_char                        ch, *pos, *lowcase_key;
    size_t                        size, len, key_len, val_len, padding,
                                  allocated;
    ngx_uint_t                    i, n, next, hash, skip_empty, header_params;
    ngx_buf_t                    *b;
    ngx_chain_t                  *cl, *body;
    ngx_list_part_t              *part;
    ngx_table_elt_t              *header, **ignored;
    ngx_http_upstream_t          *u;
    ngx_http_script_code_pt       code;
    ngx_http_script_engine_t      e, le;
    ngx_http_fastcgi_header_t    *h;
    ngx_http_fastcgi_params_t    *params; //
    ngx_http_fastcgi_loc_conf_t  *flcf;
    ngx_http_script_len_code_pt   lcode;

    len = 0;
    header_params = 0;
    ignored = NULL;

    u = r->upstream;

    flcf = ngx_http_get_module_loc_conf(r, ngx_http_fastcgi_module);

#if (NGX_HTTP_CACHE)
    params = u->cacheable ? &flcf->params_cache : &flcf->params;
#else
    params = &flcf->params; //fastcgi_paramsÉèÖÃµÄ±äÁ¿
#endif

    //ºÍngx_http_fastcgi_init_paramsÅäºÏÔÄ¶Á //ngx_http_fastcgi_create_requestºÍngx_http_fastcgi_init_paramsÅä¶ÔÔÄ¶Á
    if (params->lengths) { //»ñÈ¡fastcgi_paramsÅäÖÃµÄËùÓÐ±äÁ¿³¤¶È£¬Ò²¾ÍÊÇËùÓÐµÄfastcgi_params key value£»ÖÐµÄkey×Ö·û´®³¤¶È£¬Èç¹ûÓÐ¶à¸öÅäÖÃ£¬ÔòÊÇ¶à¸ökeyÖ®ºÍ
        ngx_memzero(&le, sizeof(ngx_http_script_engine_t));

        ngx_http_script_flush_no_cacheable_variables(r, params->flushes);
        le.flushed = 1;

        le.ip = params->lengths->elts;
        le.request = r;

        while (*(uintptr_t *) le.ip) { //¼ÆËãËùÓÐµÄfastcgi_paramÉèÖÃµÄ±äÁ¿µÄkeyºÍvalue×Ö·û´®Ö®ºÍ

            ////fastcgi_paramsÉèÖÃµÄ±äÁ¿
            lcode = *(ngx_http_script_len_code_pt *) le.ip;
            key_len = lcode(&le);

            lcode = *(ngx_http_script_len_code_pt *) le.ip;
            skip_empty = lcode(&le);


            //Ò²¾ÍÊÇÈ¡³öfastcgi_param  SCRIPT_FILENAME  xxx;ÖÐ×Ö·û´®xxxµÄ×Ö·û´®³¤¶È
            for (val_len = 0; *(uintptr_t *) le.ip; val_len += lcode(&le)) {
                lcode = *(ngx_http_script_len_code_pt *) le.ip;
                //ÎªÊ²Ã´ÕâÀï½âÎöµ½Ò»¸ö²ÎÊýºÍÖµºó»áÍË³öforÄØ?ÒòÎªÔÚngx_http_fastcgi_init_paramsÖÐÔÚvalue¶ÔÓ¦µÄcodeºóÃæÌí¼ÓÁËÒ»¸öNULL¿ÕÖ¸Õë£¬Ò²¾ÍÊÇÏÂÃæµÄle.ip += sizeof(uintptr_t);
            }
            le.ip += sizeof(uintptr_t);

            //ºÍngx_http_fastcgi_init_params  ngx_http_upstream_param_set_slotÅäºÏÔÄ¶Á
            if (skip_empty && val_len == 0) { //Èç¹ûfastcgi_param  SCRIPT_FILENAME  xxx  if_not_empty; Èç¹ûxxx½âÎöºóÊÇ¿ÕµÄ£¬ÔòÖ±½ÓÌø¹ý¸Ã±äÁ¿
                continue;
            }

            //fastcgi_paramÉèÖÃµÄ±äÁ¿µÄkeyºÍvalue×Ö·û´®Ö®ºÍ
            len += 1 + key_len + ((val_len > 127) ? 4 : 1) + val_len; //((val_len > 127) ? 4 : 1)±íÊ¾´æ´¢val_len×Ö½Úvalue×Ö·ûÐèÒª¶àÉÙ¸ö×Ö½ÚÀ´±íÊ¾¸Ã×Ö·û³¤¶È
        }
    }

    if (flcf->upstream.pass_request_headers) { //¼ÆËã request header µÄ³¤¶È

        allocated = 0;
        lowcase_key = NULL;

        if (params->number) { 
            n = 0;
            part = &r->headers_in.headers.part;

            while (part) { //¿Í»§¶ËÇëÇóÍ·²¿¸öÊýºÍ+fastcgi_param HTTP_XX±äÁ¿¸öÊý
                n += part->nelts;
                part = part->next;
            }

            ignored = ngx_palloc(r->pool, n * sizeof(void *)); //´´½¨Ò»¸ö ignored Êý×é
            if (ignored == NULL) {
                return NGX_ERROR;
            }
        }

        part = &r->headers_in.headers.part; //È¡µÃ headers µÄµÚÒ»¸ö partÊý×éÐÅÏ¢
        header = part->elts; //È¡µÃ headers µÄµÚÒ»¸ö partÊý×éÊ×ÔªËØ

        for (i = 0; /* void */; i++) {

            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next; //ÏÂÒ»¸öÊý×é Èç¹ûµ±Ç° part ´¦ÀíÍê±Ï£¬¼ÌÐø next partÊý×é
                header = part->elts;//ÏÂÒ»¸öÊý×éµÄÍ·²¿ÔªËØÎ»ÖÃ
                i = 0;
            }

            if (params->number) { //Èç¹ûÓÐÅäÖÃfastcgi_param  HTTP_  XXX
                if (allocated < header[i].key.len) {
                    allocated = header[i].key.len + 16; //×¢ÒâÕâÀï±È"host"³¤¶È¶à16
                    lowcase_key = ngx_pnalloc(r->pool, allocated); 
                    //Îªä¯ÀÀÆ÷·¢ËÍ¹ýÀ´µÄÃ¿Ò»¸öÇëÇóÍ·µÄkey·ÖÅä¿Õ¼ä£¬ÀýÈçÎªhost:www.sina.comÖÐµÄ"host"×Ö·û´®·ÖÅä¿Õ¼ä
                    if (lowcase_key == NULL) {
                        return NGX_ERROR;
                    }
                }

                hash = 0;

                
                /* °Ñ key ×ª³ÉÐ¡Ð´£¬²¢¼ÆËã³ö×îºóÒ»¸ö ch µÄ hash Öµ */
                for (n = 0; n < header[i].key.len; n++) {
                    ch = header[i].key.data[n];

                    if (ch >= 'A' && ch <= 'Z') {
                        ch |= 0x20;

                    } else if (ch == '-') {
                        ch = '_';
                    }

                    hash = ngx_hash(hash, ch);
                    lowcase_key[n] = ch; //ÇëÇóÍ·²¿ÐÐÖÐµÄkey×ª»»ÎªÐ¡Ð´´æÈëlowcase_keyÊý×é
                }

                /*
                    ²éÕÒÕâ¸ö header ÊÇ·ñÔÚ ignore Ãûµ¥Ö®ÄÚ
                    // yes £¬°ÑÕâ¸ö headerÊý×éÖ¸Õë·ÅÔÚ ignore Êý×éÄÚ£¬ºóÃæÓÐÓÃ£¬È»ºó¼ÌÐø´¦ÀíÏÂÒ»¸ö
                    */ //¿Í»§¶ËÇëÇóÍ·²¿ÐÐ¹Ø¼ü×ÖkeyÊÇ·ñÔÚfastcgi_param  HTTP_xx  XXX£¬´æ´¢HTTP_xxµÄhash±íparams->hashÖÐÊÇ·ñÄÜ²éÕÒµ½ºÍÍ·²¿ÐÐkeyÒ»ÑùµÄ
                if (ngx_hash_find(&params->hash, hash, lowcase_key, n)) { 
                    ignored[header_params++] = &header[i]; 
                    //ÇëÇóÍ·ÖÐµÄkeyÔÚfastcgi_param HTTP_XX ÒÑ¾­ÓÐÁËHTTP_XX,Ôò°Ñ¸ÃÇëÇóÐÐÐÅÏ¢Ìí¼Óµ½ignoredÊý×éÖÐ
                    continue;
                }
                
               // n µÄÖµµÄ¼ÆËãºÍÏÂÃæµÄÆäÊµÒ»Ñù
               // ÖÁÓÚ sizeof ºóÔÙ¼õÒ»£¬ÊÇÒòÎªÖ»ÐèÒª¸½¼Ó¸ö "HTTP" µ½ Header ÉÏÈ¥£¬²»ÐèÒª "_"
                n += sizeof("HTTP_") - 1;

            } else {
                n = sizeof("HTTP_") - 1 + header[i].key.len; //Èç¹ûÇëÇóÍ·key²»ÊÇHTTP_£¬»á¶àÒ»¸ö"HTTP_Í·³öÀ´"
            }

            //¼ÆËã FASTCGI ±¨ÎÄ³¤¶È+ÇëÇóÍ·²¿key+value³¤¶ÈºÍÁË
            len += ((n > 127) ? 4 : 1) + ((header[i].value.len > 127) ? 4 : 1)
                + n + header[i].value.len;
        }
    }

    //µ½ÕâÀïÒÑ¾­¼ÆËãÁËfastcgi_paramÉèÖÃµÄ±äÁ¿key+valueºÍÇëÇóÍ·key+value(HTTP_xx)ËùÓÐÕâÐ©×Ö·û´®µÄ³¤¶ÈºÍÁË(×Ü³¤¶Èlen)

    if (len > 65535) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "fastcgi request record is too big: %uz", len);
        return NGX_ERROR;
    }

    //FASTCGI Ð­Òé¹æ¶¨£¬Êý¾Ý±ØÐë 8 bit ¶ÔÆë
    padding = 8 - len % 8;
    padding = (padding == 8) ? 0 : padding;

    //¼ÆËã×ÜµÄËùÐè¿Õ¼ä´óÐ¡
    size = sizeof(ngx_http_fastcgi_header_t) //#1
           + sizeof(ngx_http_fastcgi_begin_request_t) //#2

           + sizeof(ngx_http_fastcgi_header_t) //#3 /* NGX_HTTP_FASTCGI_PARAMS */ //Ç°ÃæÕâÈý¸öÊµ¼ÊÄÚÈÝÔÚngx_http_fastcgi_request_start
           + len + padding  //#4
           + sizeof(ngx_http_fastcgi_header_t) //#5 /* NGX_HTTP_FASTCGI_PARAMS */

           + sizeof(ngx_http_fastcgi_header_t);  //#6 /* NGX_HTTP_FASTCGI_STDIN */


    b = ngx_create_temp_buf(r->pool, size);
    if (b == NULL) {
        return NGX_ERROR;
    }

    cl = ngx_alloc_chain_link(r->pool);// ´´½¨ buffer chain£¬°Ñ¸Õ´´½¨µÄ buffer Á´½øÈ¥
    if (cl == NULL) {
        return NGX_ERROR;
    }

    cl->buf = b;

    ngx_http_fastcgi_request_start.br.flags =
        flcf->keep_conn ? NGX_HTTP_FASTCGI_KEEP_CONN : 0;
    //    Ç°Á½¸ö header µÄÄÚÈÝÊÇÒÑ¾­¶¨ÒåºÃµÄ£¬ÕâÀï¼òµ¥¸´ÖÆ¹ýÀ´
    ngx_memcpy(b->pos, &ngx_http_fastcgi_request_start,
               sizeof(ngx_http_fastcgi_request_start_t));//Ö±½Ó¿½±´Ä¬ÈÏµÄFCGIÍ·²¿×Ö½Ú£¬ÒÔ¼°²ÎÊý²¿·ÖµÄÍ·²¿
    
    //h Ìø¹ý±ê×¼µÄngx_http_fastcgi_request_startÇëÇóÍ·²¿£¬Ìøµ½²ÎÊý¿ªÊ¼Í·²¿¡£Ò²¾ÍÊÇNGX_HTTP_FASTCGI_PARAMS²¿·Ö
    h = (ngx_http_fastcgi_header_t *)
             (b->pos + sizeof(ngx_http_fastcgi_header_t)
                     + sizeof(ngx_http_fastcgi_begin_request_t)); //Ìøµ½ÉÏÃæµÄ#3Î»ÖÃÍ·

    
    //¸ù¾Ý²ÎÊýÄÚÈÝ£¬ Ìî³äÊ£Óàparams²ÎÊýÍ·²¿ÖÐÊ£Óà4×Ö½Ú  ºÍngx_http_fastcgi_request_startÅäºÏÔÄ¶Á
    h->content_length_hi = (u_char) ((len >> 8) & 0xff);
    h->content_length_lo = (u_char) (len & 0xff);
    h->padding_length = (u_char) padding;
    h->reserved = 0;

    //ºÍngx_http_fastcgi_request_startÅäºÏÔÄ¶Á  //ÏÖÔÚb->lastÖ¸Ïò²ÎÊý²¿·ÖµÄ¿ªÍ·£¬Ìø¹ýµÚÒ»¸ö²ÎÊýÍ·²¿¡£ÒòÎªÆäÊý¾ÝÒÑ¾­ÉèÖÃ£¬ÈçÉÏ¡£
    b->last = b->pos + sizeof(ngx_http_fastcgi_header_t)
                     + sizeof(ngx_http_fastcgi_begin_request_t)
                     + sizeof(ngx_http_fastcgi_header_t); //Ìøµ½#4Î»ÖÃ

    /* ÏÂÃæ¾Í¿ªÊ¼Ìî³äparams²ÎÊý + ¿Í»§¶Ë"HTTP_xx" ÇëÇóÐÐ×Ö·û´®ÁË */
    
    if (params->lengths) {//´¦ÀíFCGIµÄ²ÎÊý£¬½øÐÐÏà¹ØµÄ¿½±´²Ù×÷¡£  
        ngx_memzero(&e, sizeof(ngx_http_script_engine_t));

        e.ip = params->values->elts; //ÕâÏÂÃæ¾ÍÊÇ½âÎökey-value¶ÔÓ¦µÄkey×Ö·û´®ÖµºÍvalue×Ö·û´®Öµ
        e.pos = b->last;//FCGIµÄ²ÎÊýÏÈ½ô¸úbºóÃæ×·¼Ó
        e.request = r;
        e.flushed = 1;

        le.ip = params->lengths->elts;
        //ºÍngx_http_fastcgi_init_paramsÅäºÏÔÄ¶Á //ngx_http_fastcgi_create_requestºÍngx_http_fastcgi_init_paramsÅä¶ÔÔÄ¶Á
        while (*(uintptr_t *) le.ip) {//»ñÈ¡¶ÔÓ¦µÄ±äÁ¿²ÎÊý×Ö·û´®
            //Îªngx_http_script_copy_len_code£¬µÃµ½½Å±¾³¤¶È¡£ Ò²¾ÍÊÇfastcgi_param  SCRIPT_FILENAME  xxx;ÖÐ×Ö·û´®SCRIPT_FILENAME×Ö·û´®
            lcode = *(ngx_http_script_len_code_pt *) le.ip;
            key_len = (u_char) lcode(&le);

            lcode = *(ngx_http_script_len_code_pt *) le.ip;
            skip_empty = lcode(&le);//fastcgi_param  SCRIPT_FILENAME  xxx  if_not_empty;ÊÇ·ñÓÐif_not_empty²ÎÊý£¬Èç¹ûÓÐ¸ÃÖµÎª1

            for (val_len = 0; *(uintptr_t *) le.ip; val_len += lcode(&le)) { //Ò²¾ÍÊÇÈ¡³öfastcgi_param  SCRIPT_FILENAME  xxx;ÖÐ×Ö·û´®xxxµÄ×Ö·û´®
                lcode = *(ngx_http_script_len_code_pt *) le.ip; 
                //ÎªÊ²Ã´ÕâÀï½âÎöµ½Ò»¸ö²ÎÊýºÍÖµºó»áÍË³öforÄØ?ÒòÎªÔÚngx_http_fastcgi_init_paramsÖÐÔÚvalue¶ÔÓ¦µÄcodeºóÃæÌí¼ÓÁËÒ»¸öNULL¿ÕÖ¸Õë£¬Ò²¾ÍÊÇÏÂÃæµÄle.ip += sizeof(uintptr_t);
            }
            le.ip += sizeof(uintptr_t);

            if (skip_empty && val_len == 0) { //Èç¹ûÊÇÉèÖÃÁËif_not_emputy£¬Ôò¸ÃÌõÅäÖÃµÄkey value¾Í²»»á·¢ËÍ¸øºó¶Ë
                e.skip = 1; //ngx_http_script_copy_code£¬Ã»ÓÐÊý¾Ý£¬ÔÚ¸Ãº¯ÊýÖÐÎÞÐè¿½±´Êý¾Ý

                while (*(uintptr_t *) e.ip) {
                    code = *(ngx_http_script_code_pt *) e.ip;
                    code((ngx_http_script_engine_t *) &e);
                }
                e.ip += sizeof(uintptr_t);

                e.skip = 0;

                continue;
            }

            *e.pos++ = (u_char) key_len; //KEY³¤¶Èµ½bÖÐ

            //VALUE×Ö·û´®³¤¶Èµ½bÖÐ£¬Èç¹ûÊÇ4×Ö½Ú±íÊ¾µÄ³¤¶È£¬µÚÒ»Î»Îª1£¬·ñÔòÎª0£¬¸ù¾Ý¸ÃÎ»Çø·ÖÊÇ4×Ö½Ú»¹ÊÇ1×Ö½Ú±¨ÎÄÄÚÈÝ³¤¶È
            if (val_len > 127) {
                *e.pos++ = (u_char) (((val_len >> 24) & 0x7f) | 0x80);
                *e.pos++ = (u_char) ((val_len >> 16) & 0xff);
                *e.pos++ = (u_char) ((val_len >> 8) & 0xff);
                *e.pos++ = (u_char) (val_len & 0xff);

            } else {
                *e.pos++ = (u_char) val_len;
            }

            //ÕâÀïÃæµÄngx_http_script_copy_code»á¿½±´½Å±¾ÒýÇæ½âÎö³öµÄ¶ÔÓ¦µÄvalue±äÁ¿ÖÐµÄÖµµ½bÖÐ
            while (*(uintptr_t *) e.ip) { //Ã¿ÌõÅäÖÃfastcgi_param  SCRIPT_FILENAME  xxxµÄvalue codeºóÃæ¶¼ÓÐÒ»¸öNULLÖ¸Õë£¬ËùÒÔÕâÀïÃ¿Ò»Ìõvalue¶ÔÓ¦µÄcode»áÀïÃæÍË³ö
                code = *(ngx_http_script_code_pt *) e.ip;
                code((ngx_http_script_engine_t *) &e);
            }
            e.ip += sizeof(uintptr_t); 

            ngx_log_debug4(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "fastcgi param: \"%*s: %*s\"",
                           key_len, e.pos - (key_len + val_len),
                           val_len, e.pos - val_len);
        }

        b->last = e.pos;
    }
    //´¦ÀíÍêËùÓÐµÄfastcgi_param  xxx   xxx;²ÎÊý

    //Ìí¼Ó¿Í»§¶ËÇëÇóÐÐÖÐµÄHTTP_XX×Ö·û´®ÐÅÏ¢µ½ÉÏÃæµÄ#4ÖÐ
    if (flcf->upstream.pass_request_headers) {

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

            for (n = 0; n < header_params; n++) {
                if (&header[i] == ignored[n]) { //ä¯ÀÀÆ÷ÇëÇóÐÐÖÐµÄkeyºÍfastcgi_paramÉèÖÃµÄ²úÉúkeyÍêÈ«Ò»Ñù£¬ÒòÎªÉÏÃæÒÑ¾­¿½±´µ½bÖÐÁË£¬ËùÒÔÕâÀï²»ÐèÒªÔÙ¿½±´
                    goto next;
                }
            }

            key_len = sizeof("HTTP_") - 1 + header[i].key.len; //ÒòÎªÒª¸½¼ÓÒ»¸öHTTPµ½ÇëÇóÍ·keyÖÐ£¬ÀýÈçhost:xxx;Ôò»á±ä»¯HTTPhost·¢ËÍµ½ºó¶Ë·þÎñÆ÷
            if (key_len > 127) {
                *b->last++ = (u_char) (((key_len >> 24) & 0x7f) | 0x80);
                *b->last++ = (u_char) ((key_len >> 16) & 0xff);
                *b->last++ = (u_char) ((key_len >> 8) & 0xff);
                *b->last++ = (u_char) (key_len & 0xff);

            } else {
                *b->last++ = (u_char) key_len;
            }

            val_len = header[i].value.len;
            if (val_len > 127) {
                *b->last++ = (u_char) (((val_len >> 24) & 0x7f) | 0x80);
                *b->last++ = (u_char) ((val_len >> 16) & 0xff);
                *b->last++ = (u_char) ((val_len >> 8) & 0xff);
                *b->last++ = (u_char) (val_len & 0xff);

            } else {
                *b->last++ = (u_char) val_len;
            }

            b->last = ngx_cpymem(b->last, "HTTP_", sizeof("HTTP_") - 1); //ÔÚÇëÇóÍ·Ç°Ãæ¼Ó¸öHTTP×Ö·û´®

            for (n = 0; n < header[i].key.len; n++) {//°ÑÍ·²¿ÐÐkey×ª³É ´óÐ´£¬È»ºó¸´ÖÆµ½b buffer ÖÐ£¬Á÷Èëhost:www.sina.comÔòkey±äÎªHTTPHOST
                ch = header[i].key.data[n];

                if (ch >= 'a' && ch <= 'z') {
                    ch &= ~0x20;

                } else if (ch == '-') {
                    ch = '_';
                }

                *b->last++ = ch;
            }

            b->last = ngx_copy(b->last, header[i].value.data, val_len); //¿½±´host:www.sina.comÖÐµÄ×Ö·û´®www.sina.comµ½bÖÐ

            ngx_log_debug4(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "fastcgi param: \"%*s: %*s\"",
                           key_len, b->last - (key_len + val_len),
                           val_len, b->last - val_len);
        next:

            continue;
        }
    }


    /* ÕâÉÏÃæµÄfastcgi_param²ÎÊýºÍ¿Í»§¶ËÇëÇóÍ·key¹«ÓÃÒ»¸öcl£¬¿Í»§¶Ë°üÌåÁíÍâÕ¼ÓÃÒ»¸ö»òÕß¶à¸öcl£¬ËûÃÇÍ¨¹ýnextÁ¬½ÓÔÚÒ»Æð£¬×îÖÕÇ°²¿Á¬½Óµ½u->request_bufs
        ËùÓÐÐèÒª·¢Íùºó¶ËµÄÊý¾Ý¾ÍÔÚu->request_bufsÖÐÁË£¬·¢ËÍµÄÊ±ºò´ÓÀïÃæÈ¡³öÀ´¼´¿É*/

    if (padding) { //Ìî³äÊ¹Ëû8×Ö½Ú¶ÔÆë
        ngx_memzero(b->last, padding);
        b->last += padding;
    }

    //×îºóÃæ´øÒ»¸öNGX_HTTP_FASTCGI_PARAMS£¬ÇÒÆðÄÚÈÝ³¤¶ÈÎª0µÄÍ·²¿ÐÐ£¬±íÊ¾ÄÚÈÝ½áÊø
    h = (ngx_http_fastcgi_header_t *) b->last;
    b->last += sizeof(ngx_http_fastcgi_header_t);

    h->version = 1;
    h->type = NGX_HTTP_FASTCGI_PARAMS;
    h->request_id_hi = 0;
    h->request_id_lo = 1;
    h->content_length_hi = 0;//±ê¼ÇÎª²ÎÊý²¿·ÖµÄÍ·£¬ÇÒÏÂÃæµÄÄÚÈÝÎª¿Õ£¬±íÊ¾ÊÇ½áÎ²¡£
    h->content_length_lo = 0;
    h->padding_length = 0;
    h->reserved = 0;

    /* µ½ÕâÀï¿Í»§¶ËÍ·²¿ÐÐÒÑ¾­´¦ÀíÍê±Ï£¬¿¼ÊÔ´¦Àí°üÌåÁË */

    if (r->request_body_no_buffering) { //Ã»ÓÐ»º´æ°üÌå£¬ÔòÖ±½Ó°ÑÍ·²¿ÐÐ°´ÕÕfastcgiÐ­Òé¸ñÊ½·¢ËÍµ½ºó¶Ë

        u->request_bufs = cl;

        u->output.output_filter = ngx_http_fastcgi_body_output_filter;
        u->output.filter_ctx = r;

    } else if (flcf->upstream.pass_request_body) {
        //¿Í»§¶ËÇëÇó°üÌåÁãÊ³ÓÃbodyÖ¸Ïò ngx_http_upstream_init_requestÖÐÈ¡³öµÄ¿Í»§¶Ë°üÌå½á¹¹
        body = u->request_bufs; //Õâ¸öÓÐÊý¾ÝÁËÂð£¬ÓÐµÄ£¬ÔÚngx_http_upstream_init_request¿ªÍ·ÉèÖÃµÄ¡£ÉèÖÃÎª¿Í»§¶Ë·¢ËÍµÄHTTP BODY
        u->request_bufs = cl; //request_bufs´ÓÐÂÖ¸ÏòÉÏÃæ¸³ÖµºÃµÄÍ·²¿ÐÐºÍfastcgi_param±äÁ¿ÄÚÈÝµÄ¿Õ¼ä

#if (NGX_SUPPRESS_WARN)
        file_pos = 0;
        pos = NULL;
#endif
        /* ÕâÉÏÃæµÄfastcgi_param²ÎÊýºÍ¿Í»§¶ËÇëÇóÍ·key¹«ÓÃÒ»¸öcl£¬¿Í»§¶Ë°üÌåÁíÍâÕ¼ÓÃÒ»¸ö»òÕß¶à¸öcl£¬ËûÃÇÍ¨¹ýnextÁ¬½ÓÔÚÒ»Æð£¬×îÖÕÇ°²¿Á¬½Óµ½u->request_bufs
                ËùÓÐÐèÒª·¢Íùºó¶ËµÄÊý¾Ý¾ÍÔÚu->request_bufsÖÐÁË£¬·¢ËÍµÄÊ±ºò´ÓÀïÃæÈ¡³öÀ´¼´¿É*/

        while (body) {

            if (body->buf->in_file) {//Èç¹ûÔÚÎÄ¼þÀïÃæ
                file_pos = body->buf->file_pos;

            } else {
                pos = body->buf->pos;
            }

            next = 0;

            do {
                b = ngx_alloc_buf(r->pool);//ÉêÇëÒ»¿éngx_buf_sÔªÊý¾Ý½á¹¹
                if (b == NULL) {
                    return NGX_ERROR;
                }

                ngx_memcpy(b, body->buf, sizeof(ngx_buf_t));//¿½±´ÔªÊý¾Ý

                if (body->buf->in_file) {
                    b->file_pos = file_pos;
                    file_pos += 32 * 1024;//Ò»´Î32KµÄ´óÐ¡¡£

                    if (file_pos >= body->buf->file_last) { //file_pos²»ÄÜ³¬¹ýÎÄ¼þÖÐÄÚÈÝµÄ×Ü³¤¶È
                        file_pos = body->buf->file_last;
                        next = 1; //ËµÃ÷Êý¾ÝÒ»´Î¾Í¿ÉÒÔ¿½±´Íê£¬Èç¹ûÎª0£¬±íÊ¾ÎÄ¼þÖÐ»º´æµÄ±È32K»¹¶à£¬ÔòÐèÒª¶à´ÎÑ­»·Á¬½Óµ½cl->nextÖÐ
                    }

                    b->file_last = file_pos;
                    len = (ngx_uint_t) (file_pos - b->file_pos);

                } else {
                    b->pos = pos;
                    b->start = pos;
                    pos += 32 * 1024;

                    if (pos >= body->buf->last) {
                        pos = body->buf->last;
                        next = 1; //
                    }

                    b->last = pos;
                    len = (ngx_uint_t) (pos - b->pos);
                }

                padding = 8 - len % 8;
                padding = (padding == 8) ? 0 : padding;

                h = (ngx_http_fastcgi_header_t *) cl->buf->last;
                cl->buf->last += sizeof(ngx_http_fastcgi_header_t);

                h->version = 1;
                h->type = NGX_HTTP_FASTCGI_STDIN; //·¢ËÍBODY²¿·Ö
                h->request_id_hi = 0;
                h->request_id_lo = 1; //NGINX ÓÀÔ¶Ö»ÓÃÁË1¸ö¡£
                h->content_length_hi = (u_char) ((len >> 8) & 0xff);//ËµÃ÷NGINX¶ÔÓÚBODYÊÇÒ»¿é¿é·¢ËÍµÄ£¬²»Ò»¶¨ÊÇÒ»´Î·¢ËÍ¡£
                h->content_length_lo = (u_char) (len & 0xff);
                h->padding_length = (u_char) padding;
                h->reserved = 0;

                cl->next = ngx_alloc_chain_link(r->pool);//ÉêÇëÒ»¸öÐÂµÄÁ´½Ó½á¹¹£¬´æ·ÅÕâ¿éBODY£¬²ÎÊýÉ¶µÄ´æ·ÅÔÚµÚÒ»¿éBODY²¿·ÖÀ²
                if (cl->next == NULL) {
                    return NGX_ERROR;
                }

                cl = cl->next;
                cl->buf = b;//ÉèÖÃÕâ¿éÐÂµÄÁ¬½Ó½á¹¹µÄÊý¾ÝÎª¸Õ¸ÕµÄ²¿·ÖBODYÄÚÈÝ¡£  Ç°ÃæµÄparam±äÁ¿+¿Í»§¶ËÇëÇó±äÁ¿ÄÚÈÝ µÄÏÂÒ»¸öbuf¾ÍÊÇ¸Ã¿Í»§¶Ë°üÌå

                /* ÓÖÖØÐÂ·ÖÅäÁËÒ»¸öb¿Õ¼ä£¬Ö»´æ´¢Ò»¸öÍ·²¿ºÍpadding×Ö¶Î */
                b = ngx_create_temp_buf(r->pool,
                                        sizeof(ngx_http_fastcgi_header_t)
                                        + padding);//´´½¨Ò»¸öÐÂµÄÍ·²¿»º³å£¬´æ·ÅÍ·²¿µÄÊý¾Ý£¬ÒÔ¼°Ìî³ä×Ö½Ú
                if (b == NULL) {
                    return NGX_ERROR;
                }

                if (padding) {
                    ngx_memzero(b->last, padding);
                    b->last += padding;
                }

                cl->next = ngx_alloc_chain_link(r->pool);
                if (cl->next == NULL) {
                    return NGX_ERROR;
                }

                cl = cl->next;//½«Õâ¸öÏÂÒ»¸öÍ·²¿µÄ»º³åÇø·ÅÈëÁ´½Ó±í¡£ºÃ°É£¬Õâ¸öÁ´½Ó±íËã³¤µÄÁË¡£
                cl->buf = b;

            } while (!next); //Îª0£¬±íÊ¾°üÌå´óÓÚ32K£¬ÐèÒª¶à´ÎÑ­»·ÅÐ¶Ï
            //ÏÂÒ»¿éBODYÊý¾Ý
            body = body->next;
        }

    } else {//Èç¹û²»ÓÃ·¢ËÍÇëÇóµÄBODY²¿·Ö¡£Ö±½ÓÊ¹ÓÃ¸Õ²ÅµÄÁ´½Ó±í¾ÍÐÐ¡£²»ÓÃ¿½±´BODYÁË
        u->request_bufs = cl;
    }

    if (!r->request_body_no_buffering) {
        h = (ngx_http_fastcgi_header_t *) cl->buf->last;
        cl->buf->last += sizeof(ngx_http_fastcgi_header_t);

        h->version = 1;
        h->type = NGX_HTTP_FASTCGI_STDIN;//ÀÏ¹æ¾Ø£¬Ò»ÖÖÀàÐÍ½áÎ²À´Ò»¸öÈ«0µÄÍ·²¿¡£
        h->request_id_hi = 0;
        h->request_id_lo = 1;
        h->content_length_hi = 0;
        h->content_length_lo = 0;
        h->padding_length = 0;
        h->reserved = 0;
    }

    cl->next = NULL;//½áÎ²ÁË¡¢

    return NGX_OK;
}


static ngx_int_t
ngx_http_fastcgi_reinit_request(ngx_http_request_t *r)
{
    ngx_http_fastcgi_ctx_t  *f;

    f = ngx_http_get_module_ctx(r, ngx_http_fastcgi_module);

    if (f == NULL) {
        return NGX_OK;
    }

    f->state = ngx_http_fastcgi_st_version;
    f->fastcgi_stdout = 0;
    f->large_stderr = 0;

    if (f->split_parts) {
        f->split_parts->nelts = 0;
    }

    r->state = 0;

    return NGX_OK;
}


static ngx_int_t
ngx_http_fastcgi_body_output_filter(void *data, ngx_chain_t *in)
{
    ngx_http_request_t  *r = data;

    off_t                       file_pos;
    u_char                     *pos, *start;
    size_t                      len, padding;
    ngx_buf_t                  *b;
    ngx_int_t                   rc;
    ngx_uint_t                  next, last;
    ngx_chain_t                *cl, *tl, *out, **ll;
    ngx_http_fastcgi_ctx_t     *f;
    ngx_http_fastcgi_header_t  *h;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "fastcgi output filter");

    f = ngx_http_get_module_ctx(r, ngx_http_fastcgi_module);

    if (in == NULL) {
        out = in;
        goto out;
    }

    out = NULL;
    ll = &out;

    if (!f->header_sent) {
        /* first buffer contains headers, pass it unmodified */

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "fastcgi output header");

        f->header_sent = 1;

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

    cl = ngx_chain_get_free_buf(r->pool, &f->free);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    b = cl->buf;

    b->tag = (ngx_buf_tag_t) &ngx_http_fastcgi_body_output_filter;
    b->temporary = 1;

    if (b->start == NULL) {
        /* reserve space for maximum possible padding, 7 bytes */

        b->start = ngx_palloc(r->pool,
                              sizeof(ngx_http_fastcgi_header_t) + 7);
        if (b->start == NULL) {
            return NGX_ERROR;
        }

        b->pos = b->start;
        b->last = b->start;

        b->end = b->start + sizeof(ngx_http_fastcgi_header_t) + 7;
    }

    *ll = cl;

    last = 0;
    padding = 0;

#if (NGX_SUPPRESS_WARN)
    file_pos = 0;
    pos = NULL;
#endif

    while (in) {

        ngx_log_debug7(NGX_LOG_DEBUG_EVENT, r->connection->log, 0,
                       "fastcgi output in  l:%d f:%d %p, pos %p, size: %z "
                       "file: %O, size: %O",
                       in->buf->last_buf,
                       in->buf->in_file,
                       in->buf->start, in->buf->pos,
                       in->buf->last - in->buf->pos,
                       in->buf->file_pos,
                       in->buf->file_last - in->buf->file_pos);

        if (in->buf->last_buf) {
            last = 1;
        }

        if (ngx_buf_special(in->buf)) {
            in = in->next;
            continue;
        }

        if (in->buf->in_file) {
            file_pos = in->buf->file_pos;

        } else {
            pos = in->buf->pos;
        }

        next = 0;

        do {
            tl = ngx_chain_get_free_buf(r->pool, &f->free);
            if (tl == NULL) {
                return NGX_ERROR;
            }

            b = tl->buf;
            start = b->start;

            ngx_memcpy(b, in->buf, sizeof(ngx_buf_t));

            /*
             * restore b->start to preserve memory allocated in the buffer,
             * to reuse it later for headers and padding
             */

            b->start = start;

            if (in->buf->in_file) {
                b->file_pos = file_pos;
                file_pos += 32 * 1024;

                if (file_pos >= in->buf->file_last) {
                    file_pos = in->buf->file_last;
                    next = 1;
                }

                b->file_last = file_pos;
                len = (ngx_uint_t) (file_pos - b->file_pos);

            } else {
                b->pos = pos;
                pos += 32 * 1024;

                if (pos >= in->buf->last) {
                    pos = in->buf->last;
                    next = 1;
                }

                b->last = pos;
                len = (ngx_uint_t) (pos - b->pos);
            }

            b->tag = (ngx_buf_tag_t) &ngx_http_fastcgi_body_output_filter;
            b->shadow = in->buf;
            b->last_shadow = next;

            b->last_buf = 0;
            b->last_in_chain = 0;

            padding = 8 - len % 8;
            padding = (padding == 8) ? 0 : padding;

            h = (ngx_http_fastcgi_header_t *) cl->buf->last;
            cl->buf->last += sizeof(ngx_http_fastcgi_header_t);

            h->version = 1;
            h->type = NGX_HTTP_FASTCGI_STDIN;
            h->request_id_hi = 0;
            h->request_id_lo = 1;
            h->content_length_hi = (u_char) ((len >> 8) & 0xff);
            h->content_length_lo = (u_char) (len & 0xff);
            h->padding_length = (u_char) padding;
            h->reserved = 0;

            cl->next = tl;
            cl = tl;

            tl = ngx_chain_get_free_buf(r->pool, &f->free);
            if (tl == NULL) {
                return NGX_ERROR;
            }

            b = tl->buf;

            b->tag = (ngx_buf_tag_t) &ngx_http_fastcgi_body_output_filter;
            b->temporary = 1;

            if (b->start == NULL) {
                /* reserve space for maximum possible padding, 7 bytes */

                b->start = ngx_palloc(r->pool,
                                      sizeof(ngx_http_fastcgi_header_t) + 7);
                if (b->start == NULL) {
                    return NGX_ERROR;
                }

                b->pos = b->start;
                b->last = b->start;

                b->end = b->start + sizeof(ngx_http_fastcgi_header_t) + 7;
            }

            if (padding) {
                ngx_memzero(b->last, padding);
                b->last += padding;
            }

            cl->next = tl;
            cl = tl;

        } while (!next);

        in = in->next;
    }

    if (last) {
        h = (ngx_http_fastcgi_header_t *) cl->buf->last;
        cl->buf->last += sizeof(ngx_http_fastcgi_header_t);

        h->version = 1;
        h->type = NGX_HTTP_FASTCGI_STDIN;
        h->request_id_hi = 0;
        h->request_id_lo = 1;
        h->content_length_hi = 0;
        h->content_length_lo = 0;
        h->padding_length = 0;
        h->reserved = 0;

        cl->buf->last_buf = 1;

    } else if (padding == 0) {
        /* TODO: do not allocate buffers instead */
        cl->buf->temporary = 0;
        cl->buf->sync = 1;
    }

    cl->next = NULL;

out:

#if (NGX_DEBUG)

    for (cl = out; cl; cl = cl->next) {
        ngx_log_debug7(NGX_LOG_DEBUG_EVENT, r->connection->log, 0,
                       "fastcgi output out l:%d f:%d %p, pos %p, size: %z "
                       "file: %O, size: %O",
                       cl->buf->last_buf,
                       cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
    }

#endif

    rc = ngx_chain_writer(&r->upstream->writer, out);

    ngx_chain_update_chains(r->pool, &f->free, &f->busy, &out,
                         (ngx_buf_tag_t) &ngx_http_fastcgi_body_output_filter);

    for (cl = f->free; cl; cl = cl->next) {

        /* mark original buffers as sent */

        if (cl->buf->shadow) {
            if (cl->buf->last_shadow) {
                b = cl->buf->shadow;
                b->pos = b->last;
            }

            cl->buf->shadow = NULL;
        }
    }

    return rc;
}

/*
ºó¶Ë·¢ËÍ¹ýÀ´µÄ°üÌå¸ñÊ½
1. Í·²¿ÐÐ°üÌå+ÄÚÈÝ°üÌåÀàÐÍfastcgi¸ñÊ½:8×Ö½ÚfastcgiÍ·²¿ÐÐ+ Êý¾Ý(Í·²¿ÐÐÐÅÏ¢+ ¿ÕÐÐ + Êµ¼ÊÐèÒª·¢ËÍµÄ°üÌåÄÚÈÝ) + Ìî³ä×Ö¶Î  
..... ÖÐ¼ä¿ÉÄÜºó¶Ë°üÌå±È½Ï´ó£¬ÕâÀï»á°üÀ¨¶à¸öNGX_HTTP_FASTCGI_STDOUTÀàÐÍfastcgi±êÊ¶
2. NGX_HTTP_FASTCGI_END_REQUESTÀàÐÍfastcgi¸ñÊ½:¾ÍÖ»ÓÐ8×Ö½ÚÍ·²¿

×¢Òâ:ÕâÁ½²¿·ÖÄÚÈÝÓÐ¿ÉÄÜÔÚÒ»´Îrecv¾ÍÈ«²¿¶ÁÍê£¬Ò²ÓÐ¿ÉÄÜÐèÒª¶ÁÈ¡¶à´Î
²Î¿¼<ÉîÈëÆÊÎönginx> P270
*/
//½âÎö´Óºó¶Ë·þÎñÆ÷¶ÁÈ¡µ½µÄfastcgiÍ·²¿ÐÅÏ¢   //ÔÚngx_http_upstream_process_headerÖÐÖ´ÐÐ¸Ãº¯Êý
static ngx_int_t //¶ÁÈ¡fastcgiÇëÇóÐÐÍ·²¿ÓÃngx_http_fastcgi_process_header ¶ÁÈ¡fastcgi°üÌåÓÃngx_http_fastcgi_input_filter
ngx_http_fastcgi_process_header(ngx_http_request_t *r)
{//½âÎöFCGIµÄÇëÇó·µ»Ø¼ÇÂ¼£¬Èç¹ûÊÇ·µ»Ø±ê×¼Êä³ö£¬Ôò½âÎöÆäÇëÇóµÄHTTPÍ·²¿²¢»Øµ÷ÆäÍ·²¿Êý¾ÝµÄ»Øµ÷¡£Êý¾Ý²¿·Ö»¹Ã»ÓÐ½âÎö¡£
//ngx_http_upstream_process_header»áÃ¿´Î¶ÁÈ¡Êý¾Ýºó£¬µ÷ÓÃÕâÀï¡£
//Çë×¢ÒâÕâ¸öº¯ÊýÖ´ÐÐÍê£¬²»Ò»¶¨ÊÇËùÓÐBODYÊý¾ÝÒ²¶ÁÈ¡Íê±ÏÁË£¬¿ÉÄÜÊÇ°üº¬HTTP HEADERµÄÄ³¸öFCGI°ü¶ÁÈ¡Íê±ÏÁË£¬È»ºó½øÐÐ½âÎöµÄÊ±ºò
//ngx_http_parse_header_lineº¯ÊýÅöµ½ÁË\r\n\r\nÓÚÊÇ·µ»ØNGX_HTTP_PARSE_HEADER_DONE£¬È»ºó±¾º¯Êý¾ÍÖ´ÐÐÍê³É¡£

    u_char                         *p, *msg, *start, *last,
                                   *part_start, *part_end;
    size_t                          size;
    ngx_str_t                      *status_line, *pattern;
    ngx_int_t                       rc, status;
    ngx_buf_t                       buf;
    ngx_uint_t                      i;
    ngx_table_elt_t                *h;
    ngx_http_upstream_t            *u;
    ngx_http_fastcgi_ctx_t         *f;
    ngx_http_upstream_header_t     *hh;
    ngx_http_fastcgi_loc_conf_t    *flcf;
    ngx_http_fastcgi_split_part_t  *part;
    ngx_http_upstream_main_conf_t  *umcf;

    f = ngx_http_get_module_ctx(r, ngx_http_fastcgi_module);

    umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);

    u = r->upstream;

    for ( ;; ) { //ÆôÓÃcacheµÄÇé¿öÏÂ£¬×¢ÒâÕâÊ±ºòbufµÄÄÚÈÝÊµ¼ÊÉÏÒÑ¾­ÔÚngx_http_upstream_process_headerÖÐ³öÈ¥ÁËÎª»º´æÈçÎÄ¼þÖÐÔ¤ÁôµÄÍ·²¿ÄÚ´æ

        if (f->state < ngx_http_fastcgi_st_data) {//ÉÏ´ÎµÄ×´Ì¬¶¼Ã»ÓÐ¶ÁÍêÒ»¸öÍ·²¿,ÏÈ½âÎöÕâÐ©Í·²¿¿´¿´ÊÇ²»ÊÇÓÐÎÊÌâ¡£

            f->pos = u->buffer.pos;
            f->last = u->buffer.last;

            rc = ngx_http_fastcgi_process_record(r, f);

            u->buffer.pos = f->pos;
            u->buffer.last = f->last;

            if (rc == NGX_AGAIN) { //ËµÃ÷Í·²¿8×Ö½Ú»¹Ã»¶ÁÍê£¬ÐèÒª¼ÌÐørecvºó¼ÌÐøµ÷ÓÃngx_http_fastcgi_process_record½âÎö
                return NGX_AGAIN;
            }

            if (rc == NGX_ERROR) {
                return NGX_HTTP_UPSTREAM_INVALID_HEADER;
            }

            if (f->type != NGX_HTTP_FASTCGI_STDOUT
                && f->type != NGX_HTTP_FASTCGI_STDERR)  //ËµÃ÷À´µÄÊÇNGX_HTTP_FASTCGI_END_REQUEST
                //´Óngx_http_fastcgi_process_record½âÎötype¿ÉÒÔ¿´³öÖ»ÄÜÎª NGX_HTTP_FASTCGI_STDOUT NGX_HTTP_FASTCGI_STDERR NGX_HTTP_FASTCGI_END_REQUEST
            {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "upstream sent unexpected FastCGI record: %d",
                              f->type);

                return NGX_HTTP_UPSTREAM_INVALID_HEADER;
            }

            if (f->type == NGX_HTTP_FASTCGI_STDOUT && f->length == 0) { //½ÓÊÕµ½µÄÊÇÐ¯´øÊý¾ÝµÄfastcgi±êÊ¶£¬µ¥lengthÈ¸Îª0£¬
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "upstream prematurely closed FastCGI stdout");

                return NGX_HTTP_UPSTREAM_INVALID_HEADER;
            }
        }

        if (f->state == ngx_http_fastcgi_st_padding) { //Õý³£Çé¿öÏÂ×ßµ½ÕâÀï£¬±íÊ¾Í·²¿ÐÐfastcgi±êÊ¶ÖÐÃ»ÓÐ½âÎöµ½ÕæÕýµÄ°üÌå²¿·Ö£¬Òò´ËÐèÒªÔÙ´Î¶ÁÈ¡½âÎö°üÌå
        //¼û×îºóÃæµÄif (f->length == 0) { //Í·²¿ÐÐ½âÎöÍê±Ïºó£¬ÓÉÓÚÃ»ÓÐ°üÌåÊý¾Ý£¬µ¥¿ÉÄÜÓÐÌî³ä×Ö¶Î
        //Êµ¼ÊÉÏÈç¹ûÓÐpadding£¬Ö»ÓÐÔÚºóÃæµÄSTDOUTÀàÐÍfastcgi Í·²¿ÐÐ½âÎöÍê±Ïºó(Ò²¾ÍÊÇÓöµ½Ò»¸ö¿ÕÐÐ)£¬²¢ÇÒÃ»ÓÐ°üÌå£¬Ò²¾ÍÊÇf=>length=0£¬²Å»áÖ´ÐÐµ½ÕâÀï£¬Èç¹ûÓÐpadding£¬ÕâÀïÊÇ×îºóÖ´ÐÐµÄµØ·½

            if (u->buffer.pos + f->padding < u->buffer.last) {  //ËµÃ÷bufferÖÐµÄÄÚÈÝÒ²°üÀ¨padding£¬ÔòÖ±½ÓÌø¹ýpadding×Ö¶Î
                f->state = ngx_http_fastcgi_st_version;
                u->buffer.pos += f->padding; //

                continue;  //ËµÃ÷Õâ¸öfastcgi±êÊ¶ÐÅÏ¢ºóÃæ»¹ÓÐÆäËûfastcgi±êÊ¶ÐÅÏ¢
            }

            if (u->buffer.pos + f->padding == u->buffer.last) {
                f->state = ngx_http_fastcgi_st_version;
                u->buffer.pos = u->buffer.last;

                return NGX_AGAIN;
            }

            //ËµÃ÷bufferÖÐpaddingÌî³äµÄ×Ö¶Î»¹Ã»ÓÐ¶ÁÍê£¬ÐèÒªÔÙ´Îrecv²ÅÄÜ¶ÁÈ¡µ½padding×Ö¶Î
            f->padding -= u->buffer.last - u->buffer.pos;
            u->buffer.pos = u->buffer.last;

            return NGX_AGAIN;
        }

        //ÕâÏÂÃæÖ»ÄÜÊÇfastcgiÐÅÏ¢µÄNGX_HTTP_FASTCGI_STDOUT»òÕßNGX_HTTP_FASTCGI_STDERR±êÊ¶ÐÅÏ¢

        //µ½ÕâÀï£¬±íÊ¾ÊÇÒ»ÌõfastcgiÐÅÏ¢µÄdataÊý¾Ý²¿·ÖÁË
         /* f->state == ngx_http_fastcgi_st_data */
      
        if (f->type == NGX_HTTP_FASTCGI_STDERR) {

            if (f->length) {
                msg = u->buffer.pos; //msgÖ¸ÏòÊý¾Ý²¿·Öpos´¦

                if (u->buffer.pos + f->length <= u->buffer.last) { //°üÌåÖÐ°üº¬ÍêÕûµÄdata²¿·Ö
                    u->buffer.pos += f->length; //Ö±½ÓÌø¹ýÒ»ÌõpaddingÈ¥´¦Àí£¬°Ñ°üÌåºóÃæµÄÌî³ä×Ö¶ÎÈ¥µô
                    f->length = 0;
                    f->state = ngx_http_fastcgi_st_padding;

                } else {
                    f->length -= u->buffer.last - u->buffer.pos; //¼ÆËãngx_http_fastcgi_st_data½×¶ÎµÄÊý¾Ý²¿·Ö»¹²î¶àÉÙ×Ö½Ú£¬Ò²¾ÍÊÇÐèÒªÔÚ¶ÁÈ¡recv¶àÉÙ×Ö½Ú²ÅÄÜ°Ñlength¶ÁÍê
                    u->buffer.pos = u->buffer.last;
                }

                for (p = u->buffer.pos - 1; msg < p; p--) {//´Ó´íÎóÐÅÏ¢µÄºóÃæÍùÇ°ÃæÉ¨£¬Ö±µ½ÕÒµ½Ò»¸ö²¿Î»\r,\n . ¿Õ¸ñ µÄ×Ö·ûÎªÖ¹£¬Ò²¾ÍÊÇ¹ýÂËºóÃæµÄÕâÐ©×Ö·û°É¡£
                //ÔÚ´íÎóSTDERRµÄÊý¾Ý²¿·Ö´ÓÎ²²¿ÏòÇ°²éÕÒ \r \n . ¿Õ¸ñ×Ö·ûµÄÎ»ÖÃ£¬ÀýÈçabc.dd\rkkk£¬ÔòpÖ¸Ïò\r×Ö·û´®Î»ÖÃ
                    if (*p != LF && *p != CR && *p != '.' && *p != ' ') {
                        break;
                    }
                }

                p++; //ÀýÈçabc.dd\rkkk£¬ÔòpÖ¸Ïò\r×Ö·û´®Î»ÖÃµÄÏÂÒ»¸öÎ»ÖÃk

                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "FastCGI sent in stderr: \"%*s\"", p - msg, msg);//ÀýÈçabc.dd\rkkk£¬´òÓ¡½á¹ûÎªabc.dd

                flcf = ngx_http_get_module_loc_conf(r, ngx_http_fastcgi_module);

                if (flcf->catch_stderr) {
                    pattern = flcf->catch_stderr->elts;

                    for (i = 0; i < flcf->catch_stderr->nelts; i++) {
                        if (ngx_strnstr(msg, (char *) pattern[i].data,
                                        p - msg) //fastcgi_catch_stderr "XXX";ÖÐµÄxxxºÍp-msgÐÅÏ¢ÖÐÆ¥Åä£¬Ôò·µ»Øinvalid£¬¸Ãº¯Êý·µ»ØºóÈ»ºóÇëÇóÏÂÒ»¸öºó¶Ë·þÎñÆ÷
                            != NULL)
                        {
                            return NGX_HTTP_UPSTREAM_INVALID_HEADER;
                        }
                    }
                }

                if (u->buffer.pos == u->buffer.last) { //ËµÃ÷Ã»ÓÐpaddingÌî³ä×Ö¶Î£¬¸ÕºÃÊý¾Ý²¿·Ö½âÎöÒÆ¶¯ºó£¬pos=last

                    if (!f->fastcgi_stdout) {//ÔÚstderr±êÊ¶ÐÅÏ¢Ö®Ç°Ã»ÓÐÊÕµ½¹ýstdout±êÊ¶ÐÅÏ¢

                        /*
                         * the special handling the large number
                         * of the PHP warnings to not allocate memory
                         */

#if (NGX_HTTP_CACHE)
                        if (r->cache) {
                            u->buffer.pos = u->buffer.start
                                                     + r->cache->header_start;
                        } else {
                            u->buffer.pos = u->buffer.start;
                        }
#else
                        u->buffer.pos = u->buffer.start; 
                        //½âÎöÍê¸ÃÌõfastcgi errÐÅÏ¢ºó£¬¸ÕºÃ°ÑrecvµÄÊý¾Ý½âÎöÍê£¬Ò²¾ÍÊÇlast=pos,Ôò¸Ãbuffer¿ÉÒÔ´ÓÐÂrecvÁË£¬È»ºóÑ­»·ÔÚ½âÎö
#endif
                        u->buffer.last = u->buffer.pos;
                        f->large_stderr = 1; 
                    }

                    return NGX_AGAIN; //Ó¦¸Ã»¹Ã»ÓÐ½âÎöµ½fastcgiµÄ½áÊø±ê¼ÇÐÅÏ¢
                }

            } else { //ËµÃ÷ºóÃæ»¹ÓÐpaddingÐÅÏ¢
                f->state = ngx_http_fastcgi_st_padding;
            }

            continue;
        }


        /* f->type == NGX_HTTP_FASTCGI_STDOUT */ //Í·²¿ÐÐ°üÌå

#if (NGX_HTTP_CACHE)

        if (f->large_stderr && r->cache) {
            u_char                     *start;
            ssize_t                     len;
            ngx_http_fastcgi_header_t  *fh;

            start = u->buffer.start + r->cache->header_start;

            len = u->buffer.pos - start - 2 * sizeof(ngx_http_fastcgi_header_t);

            /*
             * A tail of large stderr output before HTTP header is placed
             * in a cache file without a FastCGI record header.
             * To workaround it we put a dummy FastCGI record header at the
             * start of the stderr output or update r->cache_header_start,
             * if there is no enough place for the record header.
             */

            if (len >= 0) {
                fh = (ngx_http_fastcgi_header_t *) start;
                fh->version = 1;
                fh->type = NGX_HTTP_FASTCGI_STDERR;
                fh->request_id_hi = 0;
                fh->request_id_lo = 1;
                fh->content_length_hi = (u_char) ((len >> 8) & 0xff);
                fh->content_length_lo = (u_char) (len & 0xff);
                fh->padding_length = 0;
                fh->reserved = 0;

            } else {
                r->cache->header_start += u->buffer.pos - start
                                           - sizeof(ngx_http_fastcgi_header_t);
            }

            f->large_stderr = 0;
        }

#endif

        f->fastcgi_stdout = 1; //ËµÃ÷½ÓÊÕµ½ÁËfastcgi stdout±êÊ¶ÐÅÏ¢

        start = u->buffer.pos;

        if (u->buffer.pos + f->length < u->buffer.last) {

            /*
             * set u->buffer.last to the end of the FastCGI record data
             * for ngx_http_parse_header_line()
             */

            last = u->buffer.last;
            u->buffer.last = u->buffer.pos + f->length; //lastÖ¸ÏòÊý¾Ý²¿·ÖµÄÄ©Î²´¦£¬ÒòÎªÊý¾ÝÖÐ¿ÉÄÜÓÐ´øpaddingµÈ£¬ËùÓÐ¹ýÂËµôpadding

        } else {
            last = NULL;
        }

        for ( ;; ) { //STDOUT NGX_HTTP_FASTCGI_STDOUT
            //NGX_HTTP_FASTCGI_STDOUTÕæÊµÊý¾ÝµÄÍ·²¿ºÍÎ²²¿£¬²»°üÀ¨padding
            part_start = u->buffer.pos;
            part_end = u->buffer.last;

            rc = ngx_http_parse_header_line(r, &u->buffer, 1); //½âÎöfastcgiºó¶Ë·þÎñÆ÷»ØËÍÀ´µÄÇëÇóÐÐ

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http fastcgi parser: %d", rc);

            if (rc == NGX_AGAIN) { //Ò»ÐÐÇëÇóÐÐÃ»ÓÐ½âÎöÍê±Ï
                break;
            }

            if (rc == NGX_OK) {//½âÎöµ½ÁËÒ»ÐÐÇëÇóÐÐÊý¾ÝÁË¡£ NGX_HTTP_PARSE_HEADER_DONE±íÊ¾ËùÓÐÇëÇóÐÐ½âÎöÍê±Ï£¬Í¨¹ýÁ½¸ö\r\nÈ·¶¨ËùÓÐÍ·²¿ÐÐÍê±Ï£¬Ò²¾ÍÊÇ³öÏÖÒ»¸ö¿ÕÐÐ

                /* a header line has been parsed successfully */

                h = ngx_list_push(&u->headers_in.headers);
                if (h == NULL) {
                    return NGX_ERROR;
                }

                //Èç¹ûÖ®Ç°ÊÇÒ»¶Î¶ÎÍ·²¿Êý¾Ý·ÖÎöµÄ£¬ÔòÏÖÔÚÐèÒª×éºÏÔÚÒ»Æð£¬È»ºóÔÙ´Î½âÎö¡£
                if (f->split_parts && f->split_parts->nelts) {

                    part = f->split_parts->elts;
                    size = u->buffer.pos - part_start;

                    for (i = 0; i < f->split_parts->nelts; i++) {
                        size += part[i].end - part[i].start;
                    }

                    p = ngx_pnalloc(r->pool, size);
                    if (p == NULL) {
                        return NGX_ERROR;
                    }

                    buf.pos = p;

                    for (i = 0; i < f->split_parts->nelts; i++) {
                        p = ngx_cpymem(p, part[i].start,
                                       part[i].end - part[i].start);
                    }

                    p = ngx_cpymem(p, part_start, u->buffer.pos - part_start);

                    buf.last = p;

                    f->split_parts->nelts = 0;

                    rc = ngx_http_parse_header_line(r, &buf, 1);

                    if (rc != NGX_OK) {
                        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                                      "invalid header after joining "
                                      "FastCGI records");
                        return NGX_ERROR;
                    }

                    h->key.len = r->header_name_end - r->header_name_start;
                    h->key.data = r->header_name_start;
                    h->key.data[h->key.len] = '\0';

                    h->value.len = r->header_end - r->header_start;
                    h->value.data = r->header_start;
                    h->value.data[h->value.len] = '\0';

                    h->lowcase_key = ngx_pnalloc(r->pool, h->key.len);
                    if (h->lowcase_key == NULL) {
                        return NGX_ERROR;
                    }

                } else {
                    //°ÑÇëÇóÐÐÖÐµÄkey:value±£´æµ½u->headers_in.headersÖÐµÄÁ´±í³ÉÔ±ÖÐ
                    h->key.len = r->header_name_end - r->header_name_start;
                    h->value.len = r->header_end - r->header_start;

                    h->key.data = ngx_pnalloc(r->pool,
                                              h->key.len + 1 + h->value.len + 1
                                              + h->key.len);
                    if (h->key.data == NULL) {
                        return NGX_ERROR;
                    }

                    //ÉÏÃæ¿ª±ÙµÄ¿Õ¼ä´æ´¢µÄÊÇ:key.data + '\0' + value.data + '\0' + lowcase_key.data
                    h->value.data = h->key.data + h->key.len + 1; //value.dataÎªkey.dataµÄÄ©Î²¼ÓÒ»¸ö'\0'×Ö·ûµÄºóÃæÒ»¸ö×Ö·û
                    h->lowcase_key = h->key.data + h->key.len + 1
                                     + h->value.len + 1;

                    ngx_memcpy(h->key.data, r->header_name_start, h->key.len); //¿½±´key×Ö·û´®µ½key.data
                    h->key.data[h->key.len] = '\0';
                    ngx_memcpy(h->value.data, r->header_start, h->value.len); //¿½±´value×Ö·û´®µ½value.data
                    h->value.data[h->value.len] = '\0';
                }

                h->hash = r->header_hash;

                if (h->key.len == r->lowcase_index) { 
                    ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);

                } else {
                    ngx_strlow(h->lowcase_key, h->key.data, h->key.len); //°Ñkey.data×ª»»ÎªÐ¡Ð´×Ö·û´æµ½lowcase_key
                }

                hh = ngx_hash_find(&umcf->headers_in_hash, h->hash,
                                   h->lowcase_key, h->key.len); //Í¨¹ýlowcase_key¹Ø¼ü×Ö²éÕÒngx_http_upstream_headers_inÖÐ¶ÔÓ¦µÄ³ÉÔ±

                //ÇëÇóÐÐÖÐ¶ÔÓ¦µÄkeyµÄ×Ö·û´®Îª"Status"¶ÔÓ¦µÄvalueÎª"ttt"£¬Ôòr->upstream->headers_in.statas.data = "ttt";
                //Í¨¹ýÕâÀïµÄforÑ­»·ºÍ¸Ãhandlerº¯Êý£¬¿ÉÒÔ»ñÈ¡µ½ËùÓÐ°üÌåµÄÄÚÈÝ£¬²¢ÓÉr->upstream->headers_inÖÐµÄÏà¹Ø³ÉÔ±Ö¸Ïò
                if (hh && hh->handler(r, h, hh->offset) != NGX_OK) { //Ö´ÐÐngx_http_upstream_headers_inÖÐµÄ¸÷¸ö³ÉÔ±µÄhandlerº¯Êý
                    return NGX_ERROR;
                }

                ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                               "http fastcgi header: \"%V: %V\"",
                               &h->key, &h->value);

                if (u->buffer.pos < u->buffer.last) {
                    continue;
                }

                /* the end of the FastCGI record */

                break;
            }

            if (rc == NGX_HTTP_PARSE_HEADER_DONE) { //ËùÓÐµÄÇëÇóÐÐ½âÎöÍê±Ï£¬ÏÂÃæÖ»ÓÐÇëÇóÌåµÄbodyÊý¾ÝÁË¡£

                /* a whole header has been parsed successfully */

                ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                               "http fastcgi header done");

                if (u->headers_in.status) {
                    status_line = &u->headers_in.status->value;

                    status = ngx_atoi(status_line->data, 3);

                    if (status == NGX_ERROR) {
                        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                                      "upstream sent invalid status \"%V\"",
                                      status_line);
                        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
                    }

                    u->headers_in.status_n = status;
                    u->headers_in.status_line = *status_line;

                } else if (u->headers_in.location) { //ËµÃ÷ÉÏÓÎÓÐ·µ»Ø"location"ÐèÒªÖØ¶¨Ïò
                    u->headers_in.status_n = 302;
                    ngx_str_set(&u->headers_in.status_line,
                                "302 Moved Temporarily");

                } else {
                    u->headers_in.status_n = 200; //Ö±½Ó·µ»Ø³É¹¦
                    ngx_str_set(&u->headers_in.status_line, "200 OK");
                }

                if (u->state && u->state->status == 0) {
                    u->state->status = u->headers_in.status_n;
                }

                break;
            }

            /* there was error while a header line parsing */

            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "upstream sent invalid header");

            return NGX_HTTP_UPSTREAM_INVALID_HEADER;
        }

        if (last) {
            u->buffer.last = last;
        }

        f->length -= u->buffer.pos - start; //°ÑÉÏÃæµÄÍ·²¿ÐÐ°üÌå³¤¶ÈÈ¥µô£¬Ê£ÏÂµÄÓ¦¸Ã¾ÍÊÇ °üÌåÊý¾Ý + padding Ìî³äÁË

        if (f->length == 0) { //Í·²¿ÐÐ½âÎöÍê±Ïºó£¬ÓÉÓÚÃ»ÓÐ°üÌåÊý¾Ý£¬µ¥¿ÉÄÜÓÐÌî³ä×Ö¶Î
            f->state = ngx_http_fastcgi_st_padding;
        }

        if (rc == NGX_HTTP_PARSE_HEADER_DONE) { //Í·²¿ÐÐ½âÎöÍê±Ï
            return NGX_OK;//½áÊøÁË£¬½âÎöÍ·²¿È«²¿Íê³É¡£¸Ãfastcgi STDOUTÀàÐÍÍ·²¿ÐÐ°üÌåÈ«²¿½âÎöÍê±Ï
        }

        if (rc == NGX_OK) {
            continue;
        }

        /* rc == NGX_AGAIN */

        //ËµÃ÷Ò»¸öfastcgiµÄÇëÇóÐÐ¸ñÊ½°üÌå»¹Ã»ÓÐ½âÎöÍê±Ï£¬ÄÚºË»º³åÇøÖÐÒÑ¾­Ã»ÓÐÊý¾ÝÁË£¬ÐèÒª°ÑÊ£ÓàµÄ×Ö½ÚÔÙ´Î¶ÁÈ¡£¬´ÓÐÂ½øÐÐ½âÎö Òò´ËÐèÒª¼Ç×¡ÉÏ´Î½âÎöµÄÎ»ÖÃµÈ
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "upstream split a header line in FastCGI records");

        if (f->split_parts == NULL) {
            f->split_parts = ngx_array_create(r->pool, 1,
                                        sizeof(ngx_http_fastcgi_split_part_t));
            if (f->split_parts == NULL) {
                return NGX_ERROR;
            }
        }

        part = ngx_array_push(f->split_parts);
        if (part == NULL) {
            return NGX_ERROR;
        }

        part->start = part_start;//¼ÇÂ¼¿ªÊ¼½âÎöÇ°£¬Í·²¿ÐÐ°üÌåµÄposÎ»ÖÃ
        part->end = part_end; //¼ÇÂ¼¿ªÊ¼½âÎöÇ°£¬Í·²¿ÐÐ°üÌåµÄlastÎ»ÖÃ

        if (u->buffer.pos < u->buffer.last) {
            continue;
        }

        return NGX_AGAIN;
    }
}


static ngx_int_t
ngx_http_fastcgi_input_filter_init(void *data)
{
    ngx_http_request_t           *r = data;
    ngx_http_fastcgi_loc_conf_t  *flcf;

    flcf = ngx_http_get_module_loc_conf(r, ngx_http_fastcgi_module);

    r->upstream->pipe->length = flcf->keep_conn ?
                                (off_t) sizeof(ngx_http_fastcgi_header_t) : -1;

    return NGX_OK;
}

/*
    buffering·½Ê½£¬¶ÁÊý¾ÝÇ°Ê×ÏÈ¿ª±ÙÒ»¿é´ó¿Õ¼ä£¬ÔÚngx_event_pipe_read_upstream->ngx_readv_chainÖÐ¿ª±ÙÒ»¸öngx_buf_t(buf1)½á¹¹Ö¸Ïò¶Áµ½µÄÊý¾Ý£¬
È»ºóÔÚ¶ÁÈ¡Êý¾Ýµ½inÁ´±íµÄÊ±ºò£¬ÔÚngx_http_fastcgi_input_filter»áÖØÐÂ´´½¨Ò»¸öngx_buf_t(buf1)£¬ÕâÀïÃæÉèÖÃbuf1->shadow=buf2->shadow
buf2->shadow=buf1->shadow¡£Í¬Ê±°Ñbuf2Ìí¼Óµ½p->inÖÐ¡£µ±Í¨¹ýngx_http_write_filter·¢ËÍÊý¾ÝµÄÊ±ºò»á°Ñp->inÖÐµÄÊý¾ÝÌí¼Óµ½p->out£¬È»ºó·¢ËÍ£¬
Èç¹ûÒ»´ÎÃ»ÓÐ·¢ËÍÍê³É£¬ÔòÊôÓÚµÄÊý¾Ý»áÁôÔÚp->outÖÐ¡£µ±Êý¾ÝÍ¨¹ýp->output_filter(p->output_ctx, out)·¢ËÍºó£¬buf2»á±»Ìí¼Óµ½p->freeÖÐ£¬
buf1»á±»Ìí¼Óµ½free_raw_bufsÖÐ£¬¼ûngx_event_pipe_write_to_downstream
*/

//buffering·½Ê½£¬Îªngx_http_fastcgi_input_filter  ·Çbuffering·½Ê½Îªngx_http_fastcgi_non_buffered_filter    
//¶ÁÈ¡fastcgiÇëÇóÐÐÍ·²¿ÓÃngx_http_fastcgi_process_header ¶ÁÈ¡fastcgi°üÌåÓÃngx_http_fastcgi_input_filter
static ngx_int_t //´Óngx_event_pipe_read_upstreamµôÓÃ¸Ãº¯Êý
ngx_http_fastcgi_input_filter(ngx_event_pipe_t *p, ngx_buf_t *buf) 
//Ö÷Òª¹¦ÄÜ¾ÍÊÇ½âÎöfastcgi¸ñÊ½°üÌå£¬½âÎö³ö°üÌåºó£¬°Ñ¶ÔÓ¦µÄbuf¼ÓÈëµ½p->in
{//µ±½âÎöÍê´øÓÐ\r\n\r\nµÄÍ·²¿µÄFCGI°üºó£¬ºóÃæµÄ°ü½âÎö¶¼ÓÉÕâ¸öº¯Êý½øÐÐ´¦Àí¡£
    u_char                       *m, *msg;
    ngx_int_t                     rc;
    ngx_buf_t                    *b, **prev;
    ngx_chain_t                  *cl;
    ngx_http_request_t           *r;
    ngx_http_fastcgi_ctx_t       *f;
    ngx_http_fastcgi_loc_conf_t  *flcf;

    if (buf->pos == buf->last) {
        return NGX_OK;
    }

    //ngx_http_get_module_ctx´æ´¢ÔËÐÐ¹ý³ÌÖÐµÄ¸÷ÖÖ×´Ì¬(ÀýÈç¶ÁÈ¡ºó¶ËÊý¾Ý£¬¿ÉÄÜÐèÒª¶à´Î¶ÁÈ¡)  ngx_http_get_module_loc_conf»ñÈ¡¸ÃÄ£¿éÔÚlocal{}ÖÐµÄÅäÖÃÐÅÏ¢
    r = p->input_ctx;
    f = ngx_http_get_module_ctx(r, ngx_http_fastcgi_module); //»ñÈ¡fastcgiÄ£¿é½âÎöºó¶ËÊý¾Ý¹ý³ÌÖÐµÄ¸÷ÖÖ×´Ì¬ÐÅÏ¢£¬ÒòÎª¿ÉÄÜepoll´¥·¢ºÃ¼¸´Î¶Áºó¶ËÊý¾Ý
    flcf = ngx_http_get_module_loc_conf(r, ngx_http_fastcgi_module);

    b = NULL;
    prev = &buf->shadow;

    f->pos = buf->pos;
    f->last = buf->last;

    for ( ;; ) {
        if (f->state < ngx_http_fastcgi_st_data) {//Ð¡ÓÚngx_http_fastcgi_st_data×´Ì¬µÄ±È½ÏºÃ´¦Àí£¬¶Á£¬½âÎö°É¡£ºóÃæ¾ÍÖ»ÓÐdata,padding 2¸ö×´Ì¬ÁË¡£

            rc = ngx_http_fastcgi_process_record(r, f);//ÏÂÃæ¼òµ¥´¦ÀíÒ»ÏÂFCGIµÄÍ·²¿£¬½«ÐÅÏ¢¸³Öµµ½fµÄtype,length,padding³ÉÔ±ÉÏ¡£

            if (rc == NGX_AGAIN) {
                break;//Ã»Êý¾ÝÁË£¬µÈ´ý¶ÁÈ¡
            }

            if (rc == NGX_ERROR) {
                return NGX_ERROR;
            }

            if (f->type == NGX_HTTP_FASTCGI_STDOUT && f->length == 0) {//Èç¹ûÐ­ÒéÍ·±íÊ¾ÊÇ±ê×¼Êä³ö£¬²¢ÇÒ³¤¶ÈÎª0£¬ÄÇ¾ÍÊÇËµÃ÷Ã»ÓÐÄÚÈÝ
                f->state = ngx_http_fastcgi_st_padding; //ÓÖ´ÓÏÂÒ»¸ö°üÍ·¿ªÊ¼£¬Ò²¾ÍÊÇ°æ±¾ºÅ¡£

                if (!flcf->keep_conn) {
                    p->upstream_done = 1;
                }

                ngx_log_debug0(NGX_LOG_DEBUG_HTTP, p->log, 0,
                               "http fastcgi closed stdout");

                continue;
            }

            if (f->type == NGX_HTTP_FASTCGI_END_REQUEST) {//FCGI·¢ËÍÁË¹Ø±ÕÁ¬½ÓµÄÇëÇó¡£

                if (!flcf->keep_conn) {
                    p->upstream_done = 1;
                    break;
                }

                ngx_log_debug2(NGX_LOG_DEBUG_HTTP, p->log, 0,
                               "http fastcgi sent end request, flcf->keep_conn:%d, p->upstream_done:%d", 
                                flcf->keep_conn, p->upstream_done);
                
                continue;
            }
        }


        if (f->state == ngx_http_fastcgi_st_padding) { //ÏÂÃæÊÇ¶ÁÈ¡paddingµÄ½×¶Î£¬

            if (f->type == NGX_HTTP_FASTCGI_END_REQUEST) {

                if (f->pos + f->padding < f->last) {//¶øÕýºÃµ±Ç°»º³åÇøºóÃæÓÐ×ã¹»µÄpadding³¤¶È£¬ÄÇ¾ÍÖ±½ÓÓÃËü£¬È»ºó±ê¼Çµ½ÏÂÒ»¸ö×´Ì¬£¬¼ÌÐø´¦Àí°É
                    p->upstream_done = 1;
                    break;
                }

                if (f->pos + f->padding == f->last) {//¸ÕºÃ½áÊø£¬ÄÇ¾ÍÍË³öÑ­»·£¬Íê³ÉÒ»¿éÊý¾ÝµÄ½âÎö¡£
                    p->upstream_done = 1;
                    r->upstream->keepalive = 1;
                    break;
                }

                f->padding -= f->last - f->pos;

                break;
            }

            if (f->pos + f->padding < f->last) {
                f->state = ngx_http_fastcgi_st_version;
                f->pos += f->padding;

                continue;
            }

            if (f->pos + f->padding == f->last) {
                f->state = ngx_http_fastcgi_st_version;

                break;
            }

            f->padding -= f->last - f->pos;

            break;
        }

        //µ½ÕâÀï£¬¾ÍÖ»ÓÐ¶ÁÈ¡Êý¾Ý²¿·ÖÁË¡£

        /* f->state == ngx_http_fastcgi_st_data */

        if (f->type == NGX_HTTP_FASTCGI_STDERR) {//ÕâÊÇ±ê×¼´íÎóÊä³ö£¬nginx»áÔõÃ´´¦ÀíÄØ£¬´òÓ¡Ò»ÌõÈÕÖ¾¾ÍÐÐÁË¡£

            if (f->length) {//´ú±íÊý¾Ý³¤¶È

                if (f->pos == f->last) {//ºóÃæÃ»¶«Î÷ÁË£¬»¹ÐèÒªÏÂ´ÎÔÙ¶ÁÈ¡Ò»µãÊý¾Ý²ÅÄÜ¼ÌÐøÁË
                    break;
                }

                msg = f->pos;

                if (f->pos + f->length <= f->last) {//´íÎóÐÅÏ¢ÒÑ¾­È«²¿¶ÁÈ¡µ½ÁË£¬
                    f->pos += f->length;
                    f->length = 0;
                    f->state = ngx_http_fastcgi_st_padding;//ÏÂÒ»²½È¥´¦Àípadding

                } else {
                    f->length -= f->last - f->pos;
                    f->pos = f->last;
                }

                for (m = f->pos - 1; msg < m; m--) {//´Ó´íÎóÐÅÏ¢µÄºóÃæÍùÇ°ÃæÉ¨£¬Ö±µ½ÕÒµ½Ò»¸ö²¿Î»\r,\n . ¿Õ¸ñ µÄ×Ö·ûÎªÖ¹£¬Ò²¾ÍÊÇ¹ýÂËºóÃæµÄÕâÐ©×Ö·û°É¡£
                    if (*m != LF && *m != CR && *m != '.' && *m != ' ') {
                        break;
                    }
                }

                ngx_log_error(NGX_LOG_ERR, p->log, 0,
                              "FastCGI sent in stderr: \"%*s\"",
                              m + 1 - msg, msg);

            } else {
                f->state = ngx_http_fastcgi_st_padding;
            }

            continue;
        }

        if (f->type == NGX_HTTP_FASTCGI_END_REQUEST) {

            if (f->pos + f->length <= f->last) {
                f->state = ngx_http_fastcgi_st_padding;
                f->pos += f->length;

                continue;
            }

            f->length -= f->last - f->pos;

            break;
        }


        /* f->type == NGX_HTTP_FASTCGI_STDOUT */

        if (f->pos == f->last) {
            break;
        }

        cl = ngx_chain_get_free_buf(p->pool, &p->free);
        if (cl == NULL) {
            return NGX_ERROR;
        }

        b = cl->buf;
        //ÓÃÕâ¸öÐÂµÄ»º´æÃèÊö½á¹¹£¬Ö¸ÏòbufÕâ¿éÄÚ´æÀïÃæµÄ±ê×¼Êä³öÊý¾Ý²¿·Ö£¬×¢ÒâÕâÀï²¢Ã»ÓÐ¿½±´Êý¾Ý£¬¶øÊÇÓÃbÖ¸ÏòÁËf->posÒ²¾ÍÊÇbufµÄÄ³¸öÊý¾ÝµØ·½¡£
        ngx_memzero(b, sizeof(ngx_buf_t));

        b->pos = f->pos; //´Óposµ½end  bÖÐµÄÖ¸ÕëºÍbufÖÐµÄÖ¸ÕëÖ¸ÏòÏàÍ¬µÄÄÚ´æ¿Õ¼ä
        
        b->start = buf->start; //b ¸úbuf¹²ÏíÒ»¿é¿Í»§¶Ë·¢ËÍ¹ýÀ´µÄÊý¾Ý¡£Õâ¾ÍÊÇshadowµÄµØ·½£¬ ÀàËÆÓ°×Ó?
        b->end = buf->end; //b ¸úbuf¹²ÏíÒ»¿é¿Í»§¶Ë·¢ËÍ¹ýÀ´µÄÊý¾Ý¡£Õâ¾ÍÊÇshadowµÄµØ·½£¬ ÀàËÆÓ°×Ó?
        b->tag = p->tag;
        b->temporary = 1;
        /* 
        ÉèÖÃÎªÐèÒª»ØÊÕµÄ±êÖ¾£¬ÕâÑùÔÚ·¢ËÍÊý¾ÝÊ±£¬»á¿¼ÂÇ»ØÊÕÕâ¿éÄÚ´æµÄ¡£ÎªÊ²Ã´ÒªÉèÖÃÎª1ÄØ£¬ÄÇbufferÔÚÄÄÄØÔÚº¯Êý¿ªÊ¼´¦£¬
        prev = &buf->shadow;ÏÂÃæ¾ÍÓÃbuf->shadowÖ¸ÏòÁËÕâ¿éÐÂ·ÖÅäµÄbÃèÊö½á¹¹£¬ÆäÊµÊý¾ÝÊÇ·Ö¿ªµÄ£¬Ö»ÊÇ2¸öÃèÊö½á¹¹Ö¸ÏòÍ¬Ò»¸öbuffer 
        */
        b->recycled = 1;

        //×¢Òâ:ÔÚºóÃæÒ²»áÈÃb->shadow = buf; Ò²¾ÍÊÇbÊÇbufµÄÓ°×Ó
        *prev = b; //×¢ÒâÔÚ×îÇ°ÃæÉèÖÃÁË:prev = &buf->shadow; Ò²¾ÍÊÇbuf->shadow=b
        /* 
          ÕâÀïÓÃ×î¿ªÊ¼µÄbuf£¬Ò²¾ÍÊÇ¿Í»§¶Ë½ÓÊÕµ½Êý¾ÝµÄÄÇ¿éÊý¾ÝbufµÄshadow³ÉÔ±£¬ÐÎ³ÉÒ»¸öÁ´±í£¬ÀïÃæÃ¿¸öÔªËØ¶¼ÊÇFCGIµÄÒ»¸ö°üµÄdata²¿·ÖÊý¾Ý¡£
          */
        prev = &b->shadow; //Õâ¸ö¸Ð¾õÃ»ÓÃ????Ã»ÈÎºÎÒâÒå      

        //ÏÂÃæ½«µ±Ç°·ÖÎöµÃµ½µÄFCGIÊý¾Ýdata²¿·Ö·ÅÈëp->inµÄÁ´±íÀïÃæ(¼ÓÈëµ½Á´±íÄ©Î²´¦)¡£
        if (p->in) {
            *p->last_in = cl;
        } else {
            p->in = cl;
        }
        p->last_in = &cl->next;//¼Ç×¡×îºóÒ»¿é

        //Í¬Ñù£¬¿½±´Ò»ÏÂÊý¾Ý¿éÐòºÅ¡£²»¹ýÕâÀï×¢Òâ£¬buf¿ÉÄÜ°üº¬ºÃ¼¸¸öFCGIÐ­ÒéÊý¾Ý¿é£¬
		//ÄÇ¾Í¿ÉÄÜ´æÔÚ¶à¸öinÀïÃæµÄb->numµÈÓÚÒ»¸öÏàÍ¬µÄbuf->num.
        /* STUB */ b->num = buf->num;

        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "input buf #%d %p", b->num, b->pos);

        if (f->pos + f->length <= f->last) {//Èç¹ûÊý¾Ý×ã¹»³¤£¬ÄÇÐÞ¸ÄÒ»ÏÂf->pos£¬ºÍf->state´Ó¶ø½øÈëÏÂÒ»¸öÊý¾Ý°üµÄ´¦Àí¡£Êý¾ÝÒÑ¾­·ÅÈëÁËp->inÁËµÄ¡£
            f->state = ngx_http_fastcgi_st_padding;
            f->pos += f->length;
            b->last = f->pos; //ÒÆ¶¯last

            continue;//½ÓÊÕÕâ¿éÊý¾Ý£¬¼ÌÐøÏÂÒ»¿é
        }

        //µ½ÕâÀï£¬±íÊ¾µ±Ç°¶ÁÈ¡µ½µÄÊý¾Ý»¹ÉÙÁË£¬²»¹»Ò»¸öÍêÕû°üµÄ£¬ÄÇ¾ÍÓÃÍêÕâÒ»µã£¬È»ºó·µ»Ø£¬
		//µÈ´ýÏÂ´Îevent_pipeµÄÊ±ºòÔÙ´Îread_upstreamÀ´¶ÁÈ¡Ò»Ð©Êý¾ÝÔÙ´¦ÀíÁË¡£
        f->length -= f->last - f->pos;

        b->last = f->last;//ÒÆ¶¯b->last

        break;

    }

    if (flcf->keep_conn) {

        /* set p->length, minimal amount of data we want to see */

        if (f->state < ngx_http_fastcgi_st_data) {
            p->length = 1;

        } else if (f->state == ngx_http_fastcgi_st_padding) {
            p->length = f->padding;

        } else {
            /* ngx_http_fastcgi_st_data */

            p->length = f->length;
        }
    }

    int upstream_done = p->upstream_done;
    if(upstream_done)
        ngx_log_debugall(p->log, 0, "fastcgi input filter upstream_done:%d", upstream_done);

    if (b) { //¸Õ²ÅÒÑ¾­½âÎöµ½ÁËÊý¾Ý²¿·Ö¡£
        b->shadow = buf; //bufÊÇbµÄÓ°×Ó£¬Ç°ÃæÓÐÉèÖÃbuf->shadow=b
        b->last_shadow = 1;

        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "input buf %p %z", b->pos, b->last - b->pos); //ÕâÊ±ºòµÄb->last - b->posÒÑ¾­È¥µôÁË8×Ö½ÚÍ·²¿

        return NGX_OK;
    }

    //×ßµ½ÕâÀïÒ»°ãÊÇÍâ²ãÓÐ·ÖÅäbuf¿Õ¼ä£¬µ«ÊÇÈ´·¢ÏÖbufÖÐÃ»ÓÐ¶ÁÈ¡µ½Êµ¼ÊµÄÍøÒ³°üÌåÄÚÈÝ£¬Òò´ËÐèÒª°Ñ¸ÃbufÖ¸ÏòÄÚ´æ·ÅÈëfree_raw_bufsÁ´±íÖÐ£¬ÒÔ±¸ÔÚÏÂ´Î
    //¶ÁÈ¡ºó¶Ë°üÌåµÄÊ±ºòÖ±½Ó´ÓÉÏÃæÈ¡

    /* there is no data record in the buf, add it to free chain */
    //½«buf¹ÒÈëfree_raw_bufsÍ·²¿»òÕßµÚ¶þ¸öÎ»ÖÃ£¬Èç¹ûµÚÒ»¸öÎ»ÖÃÓÐÊý¾ÝµÄ»°¡£ 
    //
    if (ngx_event_pipe_add_free_buf(p, buf) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

/*
ºó¶Ë·¢ËÍ¹ýÀ´µÄ°üÌå¸ñÊ½
1. Í·²¿ÐÐ°üÌå+ÄÚÈÝ°üÌåÀàÐÍfastcgi¸ñÊ½:8×Ö½ÚfastcgiÍ·²¿ÐÐ+ Êý¾Ý(Í·²¿ÐÐÐÅÏ¢+ ¿ÕÐÐ + Êµ¼ÊÐèÒª·¢ËÍµÄ°üÌåÄÚÈÝ) + Ìî³ä×Ö¶Î  
..... ÖÐ¼ä¿ÉÄÜºó¶Ë°üÌå±È½Ï´ó£¬ÕâÀï»á°üÀ¨¶à¸öNGX_HTTP_FASTCGI_STDOUTÀàÐÍfastcgi±êÊ¶
2. NGX_HTTP_FASTCGI_END_REQUESTÀàÐÍfastcgi¸ñÊ½:¾ÍÖ»ÓÐ8×Ö½ÚÍ·²¿

×¢Òâ:ÕâÁ½²¿·ÖÄÚÈÝÓÐ¿ÉÄÜÔÚÒ»´Îrecv¾ÍÈ«²¿¶ÁÍê£¬Ò²ÓÐ¿ÉÄÜÐèÒª¶ÁÈ¡¶à´Î
²Î¿¼<ÉîÈëÆÊÎönginx> P270
*/
//dataÊµ¼ÊÉÏÊÇ¿Í»§¶ËµÄÇëÇóngx_http_request_t *r  
//buffering·½Ê½£¬Îªngx_http_fastcgi_input_filter  ·Çbuffering·½Ê½Îªngx_http_fastcgi_non_buffered_filter    
static ngx_int_t //ngx_http_upstream_send_responseÖÐÖ´ÐÐ
ngx_http_fastcgi_non_buffered_filter(void *data, ssize_t bytes) //°Ñºó¶Ë·µ»ØµÄ°üÌåÐÅÏ¢Ìí¼Óµ½u->out_bufsÄ©Î²
{
    u_char                  *m, *msg;
    ngx_int_t                rc;
    ngx_buf_t               *b, *buf;
    ngx_chain_t             *cl, **ll;
    ngx_http_request_t      *r;
    ngx_http_upstream_t     *u;
    ngx_http_fastcgi_ctx_t  *f;

    r = data;
    f = ngx_http_get_module_ctx(r, ngx_http_fastcgi_module);

    u = r->upstream;
    buf = &u->buffer;

    //Ö´ÐÐÕæÊµµÄ°üÌå²¿·Ö
    buf->pos = buf->last;
    buf->last += bytes;

    for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
        ll = &cl->next; //u->out_bufsÖ¸ÏòÄ©Î²´¦
    }

    f->pos = buf->pos;
    f->last = buf->last;

    for ( ;; ) {
        //ÔÚ½ÓÊÕÇëÇóÐÐ+°üÌåÊý¾ÝµÄÊ±ºò£¬ÓÐ¿ÉÄÜNGX_HTTP_FASTCGI_END_REQUESTÀàÐÍfastcgi¸ñÊ½Ò²½ÓÊÕµ½£¬Òò´ËÐèÒª½âÎö
        if (f->state < ngx_http_fastcgi_st_data) {

            rc = ngx_http_fastcgi_process_record(r, f);

            if (rc == NGX_AGAIN) {
                break;
            }

            if (rc == NGX_ERROR) {
                return NGX_ERROR;
            }
        
            if (f->type == NGX_HTTP_FASTCGI_STDOUT && f->length == 0) {
                f->state = ngx_http_fastcgi_st_padding;

                ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                               "http fastcgi closed stdout");

                continue;
            }
        }

        if (f->state == ngx_http_fastcgi_st_padding) { //ÓÐ¿ÉÄÜ´ÓºóÃæµÄif (f->pos + f->length <= f->last) ×ßµ½ÕâÀï

            if (f->type == NGX_HTTP_FASTCGI_END_REQUEST) {

                if (f->pos + f->padding < f->last) {
                    u->length = 0;
                    break;
                }

                if (f->pos + f->padding == f->last) {
                    u->length = 0;
                    u->keepalive = 1;
                    break;
                }

                f->padding -= f->last - f->pos;

                break;
            }

            if (f->pos + f->padding < f->last) { //ËµÃ÷paddingºóÃæ»¹ÓÐÆäËûÐÂµÄfastcgi±êÊ¶ÀàÐÍÐèÒª½âÎö
                f->state = ngx_http_fastcgi_st_version;
                f->pos += f->padding;

                continue;
            }

            if (f->pos + f->padding == f->last) {
                f->state = ngx_http_fastcgi_st_version;

                break;
            }

            f->padding -= f->last - f->pos;

            break;
        }


        /* f->state == ngx_http_fastcgi_st_data */

        if (f->type == NGX_HTTP_FASTCGI_STDERR) {

            if (f->length) {

                if (f->pos == f->last) {
                    break;
                }

                msg = f->pos;

                if (f->pos + f->length <= f->last) {
                    f->pos += f->length;
                    f->length = 0;
                    f->state = ngx_http_fastcgi_st_padding;

                } else {
                    f->length -= f->last - f->pos;
                    f->pos = f->last;
                }

                for (m = f->pos - 1; msg < m; m--) { //´Ó´íÎóÐÅÏ¢µÄºóÃæÍùÇ°ÃæÉ¨£¬Ö±µ½ÕÒµ½Ò»¸ö²¿Î»\r,\n . ¿Õ¸ñ µÄ×Ö·ûÎªÖ¹£¬Ò²¾ÍÊÇ¹ýÂËºóÃæµÄÕâÐ©×Ö·û°É¡£
                    if (*m != LF && *m != CR && *m != '.' && *m != ' ') {
                        break;
                    }
                }
                //¾ÍÓÃÀ´´òÓ¡¸öÈÕÖ¾¡£Ã»ÆäËûµÄ¡£
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "FastCGI sent in stderr: \"%*s\"",
                              m + 1 - msg, msg);

            } else {
                f->state = ngx_http_fastcgi_st_padding;
            }

            continue;
        }

        if (f->type == NGX_HTTP_FASTCGI_END_REQUEST) {

            if (f->pos + f->length <= f->last) { //ËµÃ÷data + paddingÊý¾ÝºóÃæ»¹ÓÐÐÂµÄfastcgi¸ñÊ½°üÌå
                f->state = ngx_http_fastcgi_st_padding;
                f->pos += f->length;

                continue;
            }

            f->length -= f->last - f->pos;

            break;
        }


        /* f->type == NGX_HTTP_FASTCGI_STDOUT */
        //µ½ÕâÀï¾ÍÊÇ±ê×¼µÄÊä³öÀ²£¬Ò²¾ÍÊÇÍøÒ³ÄÚÈÝ¡£

        
        if (f->pos == f->last) {
            break;//ÕýºÃÃ»ÓÐÊý¾Ý£¬·µ»Ø
        }

        cl = ngx_chain_get_free_buf(r->pool, &u->free_bufs); //´Ófree¿ÕÏÐngx_buf_t½á¹¹ÖÐÈ¡Ò»¸ö
        if (cl == NULL) {
            return NGX_ERROR;
        }

        //°Ñcl½ÚµãÌí¼Óµ½u->out_bufsµÄÎ²²¿
        *ll = cl;
        ll = &cl->next;

        b = cl->buf; //Í¨¹ýºóÃæ¸³Öµ´Ó¶øÖ¸ÏòÊµ¼Êu->bufferÖÐµÄ°üÌå²¿·Ö

        b->flush = 1;
        b->memory = 1;

        b->pos = f->pos;
        b->tag = u->output.tag;

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http fastcgi output buf %p", b->pos);

        if (f->pos + f->length <= f->last) { //°Ñ°üÌå²¿·ÖÈ¡³öÀ´£¬ÓÃbÖ¸Ïò
            f->state = ngx_http_fastcgi_st_padding;
            f->pos += f->length; //fÈÆ¹ý°üÌå
            b->last = f->pos; //°üÌåµÄÄ©Î²

            continue;
        }

        f->length -= f->last - f->pos;
        b->last = f->last;

        break;
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
                           "http fastcgi in memory %p-%p %uz",
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


static ngx_int_t
ngx_http_fastcgi_process_record(ngx_http_request_t *r,
    ngx_http_fastcgi_ctx_t *f)
{
    u_char                     ch, *p;
    ngx_http_fastcgi_state_e   state;

    state = f->state;

    for (p = f->pos; p < f->last; p++) {

        ch = *p;

        //ÕâÀïÊÇ°Ñ8×Ö½ÚfastcgiÐ­ÒéÍ·²¿´òÓ¡³öÀ´
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http fastcgi record byte: %02Xd", ch);

        switch (state) {

        case ngx_http_fastcgi_st_version:
            if (ch != 1) { //µÚÒ»¸ö×Ö½Ú±ØÐëÊÇ1
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "upstream sent unsupported FastCGI "
                              "protocol version: %d", ch);
                return NGX_ERROR;
            }
            state = ngx_http_fastcgi_st_type;
            break;

        case ngx_http_fastcgi_st_type:
            switch (ch) {
            case NGX_HTTP_FASTCGI_STDOUT:
            case NGX_HTTP_FASTCGI_STDERR:
            case NGX_HTTP_FASTCGI_END_REQUEST:
                 f->type = (ngx_uint_t) ch;
                 break;
            default:
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "upstream sent invalid FastCGI "
                              "record type: %d", ch);
                return NGX_ERROR;

            }
            state = ngx_http_fastcgi_st_request_id_hi;
            break;

        /* we support the single request per connection */

        case ngx_http_fastcgi_st_request_id_hi:
            if (ch != 0) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "upstream sent unexpected FastCGI "
                              "request id high byte: %d", ch);
                return NGX_ERROR;
            }
            state = ngx_http_fastcgi_st_request_id_lo;
            break;

        case ngx_http_fastcgi_st_request_id_lo:
            if (ch != 1) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                              "upstream sent unexpected FastCGI "
                              "request id low byte: %d", ch);
                return NGX_ERROR;
            }
            state = ngx_http_fastcgi_st_content_length_hi;
            break;

        case ngx_http_fastcgi_st_content_length_hi:
            f->length = ch << 8;
            state = ngx_http_fastcgi_st_content_length_lo;
            break;

        case ngx_http_fastcgi_st_content_length_lo:
            f->length |= (size_t) ch;
            state = ngx_http_fastcgi_st_padding_length;
            break;

        case ngx_http_fastcgi_st_padding_length:
            f->padding = (size_t) ch;
            state = ngx_http_fastcgi_st_reserved;
            break;

        case ngx_http_fastcgi_st_reserved: //8×Ö½ÚÍ·²¿Íê±Ï
            state = ngx_http_fastcgi_st_data;

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http fastcgi record length: %z", f->length); //fastcgi¸ñÊ½°üÌåÄÚÈÝ³¤¶È()

            f->pos = p + 1;
            f->state = state;

            return NGX_OK;

        /* suppress warning */
        case ngx_http_fastcgi_st_data:
        case ngx_http_fastcgi_st_padding:
            break;
        }
    }

    f->state = state;

    return NGX_AGAIN;
}


static void
ngx_http_fastcgi_abort_request(ngx_http_request_t *r)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "abort http fastcgi request");

    return;
}


static void
ngx_http_fastcgi_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "finalize http fastcgi request");

    return;
}


static ngx_int_t
ngx_http_fastcgi_add_variables(ngx_conf_t *cf)
{
   ngx_http_variable_t  *var, *v;

    for (v = ngx_http_fastcgi_vars; v->name.len; v++) {
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
ngx_http_fastcgi_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_fastcgi_main_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_fastcgi_main_conf_t));
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
ngx_http_fastcgi_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_fastcgi_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_fastcgi_loc_conf_t));
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
     *
     *     conf->index.len = { 0, NULL };
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

    /* "fastcgi_cyclic_temp_file" is disabled */
    conf->upstream.cyclic_temp_file = 0;

    conf->upstream.change_buffering = 1;

    conf->catch_stderr = NGX_CONF_UNSET_PTR;

    conf->keep_conn = NGX_CONF_UNSET;

    ngx_str_set(&conf->upstream.module, "fastcgi");

    return conf;
}


static char *
ngx_http_fastcgi_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_fastcgi_loc_conf_t *prev = parent;
    ngx_http_fastcgi_loc_conf_t *conf = child;

    size_t                        size;
    ngx_int_t                     rc;
    ngx_hash_init_t               hash;
    ngx_http_core_loc_conf_t     *clcf;

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
                           "there must be at least 2 \"fastcgi_buffers\"");
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
             "\"fastcgi_busy_buffers_size\" must be equal to or greater than "
             "the maximum of the value of \"fastcgi_buffer_size\" and "
             "one of the \"fastcgi_buffers\"");

        return NGX_CONF_ERROR;
    }

    if (conf->upstream.busy_buffers_size
        > (conf->upstream.bufs.num - 1) * conf->upstream.bufs.size)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
             "\"fastcgi_busy_buffers_size\" must be less than "
             "the size of all \"fastcgi_buffers\" minus one buffer");

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
             "\"fastcgi_temp_file_write_size\" must be equal to or greater "
             "than the maximum of the value of \"fastcgi_buffer_size\" and "
             "one of the \"fastcgi_buffers\"");

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
             "\"fastcgi_max_temp_file_size\" must be equal to zero to disable "
             "temporary files usage or must be equal to or greater than "
             "the maximum of the value of \"fastcgi_buffer_size\" and "
             "one of the \"fastcgi_buffers\"");

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
                              &ngx_http_fastcgi_temp_path)
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

    if (conf->upstream.cache_zone && conf->upstream.cache_zone->data == NULL) {
        ngx_shm_zone_t  *shm_zone;

        shm_zone = conf->upstream.cache_zone;

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"fastcgi_cache\" zone \"%V\" is unknown",
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

    if (conf->upstream.cache && conf->cache_key.value.data == NULL) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "no \"fastcgi_cache_key\" for \"fastcgi_cache\"");
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

    ngx_conf_merge_value(conf->upstream.pass_request_headers,
                              prev->upstream.pass_request_headers, 1);
    ngx_conf_merge_value(conf->upstream.pass_request_body,
                              prev->upstream.pass_request_body, 1);

    ngx_conf_merge_value(conf->upstream.intercept_errors,
                              prev->upstream.intercept_errors, 0);

    ngx_conf_merge_ptr_value(conf->catch_stderr, prev->catch_stderr, NULL);

    ngx_conf_merge_value(conf->keep_conn, prev->keep_conn, 0);


    ngx_conf_merge_str_value(conf->index, prev->index, "");

    hash.max_size = 512;
    hash.bucket_size = ngx_align(64, ngx_cacheline_size);
    hash.name = "fastcgi_hide_headers_hash";

    if (ngx_http_upstream_hide_headers_hash(cf, &conf->upstream,
             &prev->upstream, ngx_http_fastcgi_hide_headers, &hash)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    if (clcf->noname
        && conf->upstream.upstream == NULL && conf->fastcgi_lengths == NULL)
    {
        conf->upstream.upstream = prev->upstream.upstream;
        conf->fastcgi_lengths = prev->fastcgi_lengths;
        conf->fastcgi_values = prev->fastcgi_values;
    }

    if (clcf->lmt_excpt && clcf->handler == NULL
        && (conf->upstream.upstream || conf->fastcgi_lengths))
    {
        clcf->handler = ngx_http_fastcgi_handler;
    }

#if (NGX_PCRE)
    if (conf->split_regex == NULL) {
        conf->split_regex = prev->split_regex;
        conf->split_name = prev->split_name;
    }
#endif

    if (conf->params_source == NULL) {
        conf->params = prev->params;
#if (NGX_HTTP_CACHE)
        conf->params_cache = prev->params_cache;
#endif
        conf->params_source = prev->params_source;
    }

    rc = ngx_http_fastcgi_init_params(cf, conf, &conf->params, NULL);
    if (rc != NGX_OK) {
        return NGX_CONF_ERROR;
    }

#if (NGX_HTTP_CACHE)

    if (conf->upstream.cache) {
        rc = ngx_http_fastcgi_init_params(cf, conf, &conf->params_cache,
                                          ngx_http_fastcgi_cache_headers);
        if (rc != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

#endif

    return NGX_CONF_OK;
}

//ngx_http_fastcgi_create_requestºÍngx_http_fastcgi_init_paramsÅä¶ÔÔÄ¶Á
static ngx_int_t
ngx_http_fastcgi_init_params(ngx_conf_t *cf, ngx_http_fastcgi_loc_conf_t *conf,
    ngx_http_fastcgi_params_t *params, ngx_keyval_t *default_params)
{
    u_char                       *p;
    size_t                        size;
    uintptr_t                    *code;
    ngx_uint_t                    i, nsrc;
    ngx_array_t                   headers_names, params_merged;
    ngx_keyval_t                 *h;
    ngx_hash_key_t               *hk;
    ngx_hash_init_t               hash;
    ngx_http_upstream_param_t    *src, *s;
    ngx_http_script_compile_t     sc;
    ngx_http_script_copy_code_t  *copy;

    if (params->hash.buckets) {
        return NGX_OK;
    }

    if (conf->params_source == NULL && default_params == NULL) {
        params->hash.buckets = (void *) 1;
        return NGX_OK;
    }

    params->lengths = ngx_array_create(cf->pool, 64, 1);
    if (params->lengths == NULL) {
        return NGX_ERROR;
    }

    params->values = ngx_array_create(cf->pool, 512, 1);
    if (params->values == NULL) {
        return NGX_ERROR;
    }

    if (ngx_array_init(&headers_names, cf->temp_pool, 4, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (conf->params_source) {
        src = conf->params_source->elts;
        nsrc = conf->params_source->nelts;

    } else {
        src = NULL;
        nsrc = 0;
    }

    if (default_params) {
        if (ngx_array_init(&params_merged, cf->temp_pool, 4,
                           sizeof(ngx_http_upstream_param_t))
            != NGX_OK)
        {
            return NGX_ERROR;
        }

        for (i = 0; i < nsrc; i++) {

            s = ngx_array_push(&params_merged);
            if (s == NULL) {
                return NGX_ERROR;
            }

            *s = src[i];
        }

        h = default_params;

        while (h->key.len) {

            src = params_merged.elts;
            nsrc = params_merged.nelts;

            for (i = 0; i < nsrc; i++) {
                if (ngx_strcasecmp(h->key.data, src[i].key.data) == 0) {
                    goto next;
                }
            }

            s = ngx_array_push(&params_merged);
            if (s == NULL) {
                return NGX_ERROR;
            }

            s->key = h->key;
            s->value = h->value;
            s->skip_empty = 1;

        next:

            h++;
        }

        src = params_merged.elts;
        nsrc = params_merged.nelts;
    }

    for (i = 0; i < nsrc; i++) {

        if (src[i].key.len > sizeof("HTTP_") - 1
            && ngx_strncmp(src[i].key.data, "HTTP_", sizeof("HTTP_") - 1) == 0)
        {
            hk = ngx_array_push(&headers_names);
            if (hk == NULL) {
                return NGX_ERROR;
            }

            hk->key.len = src[i].key.len - 5;
            hk->key.data = src[i].key.data + 5; //°ÑÍ·²¿µÄHTTP_Õâ5¸ö×Ö·ûÈ¥µô£¬È»ºó¿½±´µ½key->data
            hk->key_hash = ngx_hash_key_lc(hk->key.data, hk->key.len);
            hk->value = (void *) 1;

            if (src[i].value.len == 0) {
                continue;
            }
        }

        ////fastcgi_param  SCRIPT_FILENAME  aaaÖÐ±äÁ¿µÄSCRIPT_FILENAMEµÄ×Ö·û´®³¤¶È³¤¶Ècode
        copy = ngx_array_push_n(params->lengths,
                                sizeof(ngx_http_script_copy_code_t));
        if (copy == NULL) {
            return NGX_ERROR;
        }

        copy->code = (ngx_http_script_code_pt) ngx_http_script_copy_len_code;
        copy->len = src[i].key.len;


        ////fastcgi_param  SCRIPT_FILENAME  aaa  if_not_empty£¬±êÊ¶¸Ãfastcgi_paramÅäÖÃµÄ±äÁ¿SCRIPT_FILENAMEÊÇ·ñÓÐ´øif_not_empty²ÎÊý£¬´´½¨¶ÔÓ¦µÄ³¤¶Ècode£¬
        copy = ngx_array_push_n(params->lengths,
                                sizeof(ngx_http_script_copy_code_t));
        if (copy == NULL) {
            return NGX_ERROR;
        }

        copy->code = (ngx_http_script_code_pt) ngx_http_script_copy_len_code;
        copy->len = src[i].skip_empty; //Õâ1×Ö½Ú±íÊ¾ÊÇ·ñÓÐÅäÖÃÊ±´øÉÏ"if_not_empty"


        //fastcgi_param  SCRIPT_FILENAME  aaa×Ö·û´®SCRIPT_FILENAME(key)¶ÔÓ¦µÄSCRIPT_FILENAME×Ö·û´®code
        size = (sizeof(ngx_http_script_copy_code_t)
                + src[i].key.len + sizeof(uintptr_t) - 1)
               & ~(sizeof(uintptr_t) - 1);

        copy = ngx_array_push_n(params->values, size);
        if (copy == NULL) {
            return NGX_ERROR;
        }

        copy->code = ngx_http_script_copy_code;
        copy->len = src[i].key.len;

        p = (u_char *) copy + sizeof(ngx_http_script_copy_code_t);
        ngx_memcpy(p, src[i].key.data, src[i].key.len);


        //fastcgi_param  SCRIPT_FILENAME  aaaÅäÖÃÖÐ±äÁ¿¶ÔÓ¦µÄaaaÖµ£¬Èç¹ûÖµÊÇ±äÁ¿×é³É£¬ÀýÈç/home/www/scripts/php$fastcgi_script_name
        //ÔòÐèÒªÊ¹ÓÃ½Å±¾½âÎö¸÷ÖÖ±äÁ¿£¬ÕâÐ©Ã´¾ÍÊÇfastcgi_param  SCRIPT_FILENAME  aaaÖÐ×Ö·û´®aaa¶ÔÓ¦µÄ×Ö·û´®¿½±´
        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = &src[i].value;
        sc.flushes = &params->flushes;
        sc.lengths = &params->lengths;
        sc.values = &params->values;

        //°ÑÉÏÃæµÄconf->params_source[]ÖÐµÄ¸÷¸ö³ÉÔ±src¶ÔÓ¦µÄcodeÐÅÏ¢Ìí¼Óµ½params->lengths[]  params->values[]ÖÐ
        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_ERROR;
        }

        code = ngx_array_push_n(params->lengths, sizeof(uintptr_t));
        if (code == NULL) {
            return NGX_ERROR;
        }

        *code = (uintptr_t) NULL;


        code = ngx_array_push_n(params->values, sizeof(uintptr_t));
        if (code == NULL) {
            return NGX_ERROR;
        }

        *code = (uintptr_t) NULL;
    }

    code = ngx_array_push_n(params->lengths, sizeof(uintptr_t));
    if (code == NULL) {
        return NGX_ERROR;
    }

    *code = (uintptr_t) NULL;

    params->number = headers_names.nelts;

    hash.hash = &params->hash;//fastcgi_param  HTTP_  XXX;»·¾³ÖÐÍ¨¹ýfastcgi_paramÉèÖÃµÄHTTP_xx±äÁ¿Í¨¹ýhashÔËËã´æµ½¸Ãhash±íÖÐ
    hash.key = ngx_hash_key_lc;
    hash.max_size = 512;
    hash.bucket_size = 64;
    hash.name = "fastcgi_params_hash";
    hash.pool = cf->pool;
    hash.temp_pool = NULL;

    return ngx_hash_init(&hash, headers_names.elts, headers_names.nelts);
}


static ngx_int_t
ngx_http_fastcgi_script_name_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                       *p;
    ngx_http_fastcgi_ctx_t       *f;
    ngx_http_fastcgi_loc_conf_t  *flcf;

    flcf = ngx_http_get_module_loc_conf(r, ngx_http_fastcgi_module);

    f = ngx_http_fastcgi_split(r, flcf);

    if (f == NULL) {
        return NGX_ERROR;
    }

    if (f->script_name.len == 0
        || f->script_name.data[f->script_name.len - 1] != '/')
    {
        v->len = f->script_name.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = f->script_name.data;

        return NGX_OK;
    }

    v->len = f->script_name.len + flcf->index.len;

    v->data = ngx_pnalloc(r->pool, v->len);
    if (v->data == NULL) {
        return NGX_ERROR;
    }

    p = ngx_copy(v->data, f->script_name.data, f->script_name.len);
    ngx_memcpy(p, flcf->index.data, flcf->index.len);

    return NGX_OK;
}


static ngx_int_t
ngx_http_fastcgi_path_info_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_fastcgi_ctx_t       *f;
    ngx_http_fastcgi_loc_conf_t  *flcf;

    flcf = ngx_http_get_module_loc_conf(r, ngx_http_fastcgi_module);

    f = ngx_http_fastcgi_split(r, flcf);

    if (f == NULL) {
        return NGX_ERROR;
    }

    v->len = f->path_info.len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = f->path_info.data;

    return NGX_OK;
}


static ngx_http_fastcgi_ctx_t *
ngx_http_fastcgi_split(ngx_http_request_t *r, ngx_http_fastcgi_loc_conf_t *flcf)
{
    ngx_http_fastcgi_ctx_t       *f;
#if (NGX_PCRE)
    ngx_int_t                     n;
    int                           captures[(1 + 2) * 3];

    f = ngx_http_get_module_ctx(r, ngx_http_fastcgi_module);

    if (f == NULL) {
        f = ngx_pcalloc(r->pool, sizeof(ngx_http_fastcgi_ctx_t));
        if (f == NULL) {
            return NULL;
        }

        ngx_http_set_ctx(r, f, ngx_http_fastcgi_module);
    }

    if (f->script_name.len) {
        return f;
    }

    if (flcf->split_regex == NULL) {
        f->script_name = r->uri;
        return f;
    }

    n = ngx_regex_exec(flcf->split_regex, &r->uri, captures, (1 + 2) * 3);

    if (n >= 0) { /* match */
        f->script_name.len = captures[3] - captures[2];
        f->script_name.data = r->uri.data + captures[2];

        f->path_info.len = captures[5] - captures[4];
        f->path_info.data = r->uri.data + captures[4];

        return f;
    }

    if (n == NGX_REGEX_NO_MATCHED) {
        f->script_name = r->uri;
        return f;
    }

    ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                  ngx_regex_exec_n " failed: %i on \"%V\" using \"%V\"",
                  n, &r->uri, &flcf->split_name);
    return NULL;

#else

    f = ngx_http_get_module_ctx(r, ngx_http_fastcgi_module);

    if (f == NULL) {
        f = ngx_pcalloc(r->pool, sizeof(ngx_http_fastcgi_ctx_t));
        if (f == NULL) {
            return NULL;
        }

        ngx_http_set_ctx(r, f, ngx_http_fastcgi_module);
    }

    f->script_name = r->uri;

    return f;

#endif
}

/*
Õâ¸öngx_http_fastcgi_handlerÊÇÔÚnginx ½âÎöÅäÖÃµÄÊ±ºò£¬½âÎöµ½ÁËngx_string(¡°fastcgi_pass¡±),Ö¸ÁîµÄÊ±ºò»áµ÷ÓÃngx_http_fastcgi_pass£¨£©½øÐÐÖ¸Áî½âÎö
*/
static char *
ngx_http_fastcgi_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_fastcgi_loc_conf_t *flcf = conf;

    ngx_url_t                   u;
    ngx_str_t                  *value, *url;
    ngx_uint_t                  n;
    ngx_http_core_loc_conf_t   *clcf;
    ngx_http_script_compile_t   sc;

    if (flcf->upstream.upstream || flcf->fastcgi_lengths) {
        return "is duplicate";
    }
    
    //»ñÈ¡µ±Ç°µÄlocation£¬¼´ÔÚÄÄ¸ölocationÅäÖÃµÄ"fastcgi_pass"Ö¸Áî  
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    /* ÉèÖÃ¾ä±ú£¬»áÔÚngx_http_update_location_configÀïÃæÉèÖÃÎªcontent_handleµÄ£¬´Ó¶øÔÚcontent phaseÖÐ±»µ÷ÓÃ  
        
     //ÉèÖÃlocµÄhandler£¬Õâ¸öclcf->handler»áÔÚngx_http_update_location_config()ÀïÃæ¸³Óèr->content_handler£¬´Ó
     ¶øÔÚNGX_HTTP_CONTENT_PHASEÀïÃæµ÷ÓÃÕâ¸öhandler£¬¼´ngx_http_fastcgi_handler¡£  
     */
    clcf->handler = ngx_http_fastcgi_handler;

    if (clcf->name.data[clcf->name.len - 1] == '/') {
        clcf->auto_redirect = 1;
    }

    value = cf->args->elts;

    url = &value[1];

    n = ngx_http_script_variables_count(url);//ÒÔ'$'¿ªÍ·µÄ±äÁ¿ÓÐ¶àÉÙ

    if (n) {

        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = url;
        sc.lengths = &flcf->fastcgi_lengths;
        sc.values = &flcf->fastcgi_values;
        sc.variables = n;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        //Éæ¼°µ½±äÁ¿µÄ²ÎÊýÍ¨¹ý¸Ãº¯Êý°Ñ³¤¶ÈcodeºÍvalue codeÌí¼Óµ½flcf->fastcgi_lengthsºÍflcf->fastcgi_valuesÖÐ
        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        return NGX_CONF_OK;
    }

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url = value[1];
    u.no_resolve = 1;

    //µ±×öµ¥¸öserverµÄupstream¼ÓÈëµ½upstreamÀïÃæ,ºÍ upstream {}ÀàËÆ
    flcf->upstream.upstream = ngx_http_upstream_add(cf, &u, 0); 
    if (flcf->upstream.upstream == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_fastcgi_split_path_info(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
#if (NGX_PCRE)
    ngx_http_fastcgi_loc_conf_t *flcf = conf;

    ngx_str_t            *value;
    ngx_regex_compile_t   rc;
    u_char                errstr[NGX_MAX_CONF_ERRSTR];

    value = cf->args->elts;

    flcf->split_name = value[1];

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.pattern = value[1];
    rc.pool = cf->pool;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;

    if (ngx_regex_compile(&rc) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "%V", &rc.err);
        return NGX_CONF_ERROR;
    }

    if (rc.captures != 2) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "pattern \"%V\" must have 2 captures", &value[1]);
        return NGX_CONF_ERROR;
    }

    flcf->split_regex = rc.regex;

    return NGX_CONF_OK;

#else

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "\"%V\" requires PCRE library", &cmd->name);
    return NGX_CONF_ERROR;

#endif
}


static char *
ngx_http_fastcgi_store(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_fastcgi_loc_conf_t *flcf = conf;

    ngx_str_t                  *value;
    ngx_http_script_compile_t   sc;

    if (flcf->upstream.store != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "off") == 0) {
        flcf->upstream.store = 0;
        return NGX_CONF_OK;
    }

#if (NGX_HTTP_CACHE)
    if (flcf->upstream.cache > 0) {
        return "is incompatible with \"fastcgi_cache\"";
    }
#endif

    flcf->upstream.store = 1;

    if (ngx_strcmp(value[1].data, "on") == 0) {
        return NGX_CONF_OK;
    }

    /* include the terminating '\0' into script */
    value[1].len++;

    ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

    sc.cf = cf;
    sc.source = &value[1];
    sc.lengths = &flcf->upstream.store_lengths;
    sc.values = &flcf->upstream.store_values;
    sc.variables = ngx_http_script_variables_count(&value[1]);
    sc.complete_lengths = 1;
    sc.complete_values = 1;

    if (ngx_http_script_compile(&sc) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


#if (NGX_HTTP_CACHE)

static char *
ngx_http_fastcgi_cache(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_fastcgi_loc_conf_t *flcf = conf;

    ngx_str_t                         *value;
    ngx_http_complex_value_t           cv;
    ngx_http_compile_complex_value_t   ccv;

    value = cf->args->elts;

    if (flcf->upstream.cache != NGX_CONF_UNSET) { //ËµÃ÷ÒÑ¾­ÉèÖÃ¹ýfastcgi_cache xxÁË,ÕâÀï¼ì²âµ½ÓÐÅäÖÃfastcgi_cache£¬±¨´íÖØ¸´
        return "is duplicate";
    }

    if (ngx_strcmp(value[1].data, "off") == 0) {
        flcf->upstream.cache = 0;
        return NGX_CONF_OK;
    }

    if (flcf->upstream.store > 0) {
        return "is incompatible with \"fastcgi_store\"";
    }

    flcf->upstream.cache = 1;

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &cv;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    if (cv.lengths != NULL) {

        flcf->upstream.cache_value = ngx_palloc(cf->pool,
                                             sizeof(ngx_http_complex_value_t));
        if (flcf->upstream.cache_value == NULL) {
            return NGX_CONF_ERROR;
        }

        *flcf->upstream.cache_value = cv;

        return NGX_CONF_OK;
    }

    flcf->upstream.cache_zone = ngx_shared_memory_add(cf, &value[1], 0,
                                                      &ngx_http_fastcgi_module);
    if (flcf->upstream.cache_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

//fastcgi_cache_key
static char *
ngx_http_fastcgi_cache_key(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_fastcgi_loc_conf_t *flcf = conf;

    ngx_str_t                         *value;
    ngx_http_compile_complex_value_t   ccv;

    value = cf->args->elts;

    if (flcf->cache_key.value.data) {
        return "is duplicate";
    }

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &flcf->cache_key;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

#endif


static char *
ngx_http_fastcgi_lowat_check(ngx_conf_t *cf, void *post, void *data)
{
#if (NGX_FREEBSD)
    ssize_t *np = data;

    if ((u_long) *np >= ngx_freebsd_net_inet_tcp_sendspace) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"fastcgi_send_lowat\" must be less than %d "
                           "(sysctl net.inet.tcp.sendspace)",
                           ngx_freebsd_net_inet_tcp_sendspace);

        return NGX_CONF_ERROR;
    }

#elif !(NGX_HAVE_SO_SNDLOWAT)
    ssize_t *np = data;

    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "\"fastcgi_send_lowat\" is not supported, ignored");

    *np = 0;

#endif

    return NGX_CONF_OK;
}
