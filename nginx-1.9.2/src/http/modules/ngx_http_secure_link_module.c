
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_md5.h>

/*
验证权限有两种方式，一种是secure_link_secret，另一种是secure_link+secure_link_md5,他们不能共存，见ngx_http_secure_link_merge_conf

如果是secure_link+secure_link_md5方式，则一般客户端请求uri中携带secure_link $md5_str, time中的md5_str字符串，nginx收到后，进行64base decode
计算出字符串后，把该字符串与secure_link_md5字符串进行MD5运算，同时判断secure_link $md5_str, time中的time和当前时间是否过期，如果字符串
比较相同并且没有过期，则置变量$secure_link为'1'，否则子变量为‘0'，见ngx_http_secure_link_variable，同时把secure_link $md5_str, time
中的time存入变量$secure_link_expires

如果是secure_link_secret方式，则是把客户端请求uri中的/p/5e814704a28d9bc1914ff19fa0c4a00a/link/xx中的5e814704a28d9bc1914ff19fa0c4a00a字符串与
secure_link_secret xxx计算出的MD5值进行比较，相同则有权限，置变量$secure_link为'1'，否则子变量为‘0'。该方式不涉及time过期
*/

//参考ngx_http_secure_link_commands
typedef struct {
    ngx_http_complex_value_t  *variable; //secure_link md5_str, 120配置， 见ngx_http_set_complex_value_slot
//命令行敲echo -n 'secure_link_md5配置的字符串' |     openssl md5 -binary | openssl base64 | tr +/ -_ | tr -d = 获取字符串对应的MD5值
    ngx_http_complex_value_t  *md5;//secure_link_md5 $the_uri_you_want_to_hashed_by_md5配置
//命令行敲echo -n 'secure_link_secret配置的字符串' | openssl md5 -hex  获取secure_link_secret配置配置的MD5信息
    ngx_str_t                  secret;//secure_link_secret配置
} ngx_http_secure_link_conf_t;


/*
验证权限有两种方式，一种是secure_link_secret，另一种是secure_link+secure_link_md5,他们不能共存，见ngx_http_secure_link_merge_conf

如果是secure_link+secure_link_md5方式，则一般客户端请求uri中携带secure_link $md5_str, time中的md5_str字符串，nginx收到后，进行64base decode
计算出字符串后，把该字符串与secure_link_md5字符串进行MD5运算，同时判断secure_link $md5_str, time中的time和当前时间是否过期，如果字符串
比较相同并且没有过期，则置变量$secure_link为'1'，否则子变量为‘0'，见ngx_http_secure_link_variable，同时把secure_link $md5_str, time
中的time存入变量$secure_link_expires

如果是secure_link_secret方式，则是把客户端请求uri中的/p/5e814704a28d9bc1914ff19fa0c4a00a/link/xx中的5e814704a28d9bc1914ff19fa0c4a00a字符串与
secure_link_secret xxx计算出的MD5值进行比较，相同则有权限，置变量$secure_link为'1'，否则子变量为‘0'。该方式不涉及time过期

因此开启secure_link一般需要客户端浏览器配合使用，一般需要配套软件支持
*/
typedef struct {
    ngx_str_t                  expires;//secure_link md5_str, time配置中的time字符串，见ngx_http_secure_link_variable
} ngx_http_secure_link_ctx_t;


static ngx_int_t ngx_http_secure_link_old_variable(ngx_http_request_t *r,
    ngx_http_secure_link_conf_t *conf, ngx_http_variable_value_t *v,
    uintptr_t data);
static ngx_int_t ngx_http_secure_link_expires_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static void *ngx_http_secure_link_create_conf(ngx_conf_t *cf);
static char *ngx_http_secure_link_merge_conf(ngx_conf_t *cf, void *parent,
    void *child);
static ngx_int_t ngx_http_secure_link_add_variables(ngx_conf_t *cf);

/*
配置nginx
server {

    listen       80;
    server_name  s1.down.ttlsa.com;
    access_log  /data/logs/nginx/s1.down.ttlsa.com.access.log  main;

    index index.html index.<a href="http://www.ttlsa.com/php/" title="php"target="_blank">php</a> index.html;
    root /data/site/s1.down.ttlsa.com;

    location / {
        secure_link $arg_st,$arg_e;
        secure_link_md5 ttlsa.com$uri$arg_e;

        if ($secure_link = "") {
            return 403;
        }

        if ($secure_link = "0") {
            return 403;
        }
    }
}

php下载页面

<?php
 # 作用：生成nginx secure link链接
 # 站点：www.ttlsa.com
 # 作者：凉白开
 # 时间：2013-09-11
$secret = 'ttlsa.com'; # 密钥
 $path = '/web/nginx-1.4.2.tar.gz'; # 下载文件
 # 下载到期时间,time是当前时间,300表示300秒,也就是说从现在到300秒之内文件不过期
 $expire = time()+300;
# 用文件路径、密钥、过期时间生成加密串
 $md5 = base64_encode(md5($secret . $path . $expire, true));
 $md5 = strtr($md5, '+/', '-_');
 $md5 = str_replace('=', '', $md5);
# 加密后的下载地址
 echo '<a href=http://s1.down.ttlsa.com/web/nginx-1.4.2.tar.gz?st='.$md5.'&e='.$expire.'>nginx-1.4.2</a>';
 echo '<br>http://s1.down.ttlsa.com/web/nginx-1.4.2.tar.gz?st='.$md5.'&e='.$expire;
 ?>


测试nginx防盗链
打开http://test.ttlsa.com/down.php点击上面的连接下载
 下载地址如下：
http://s1.down.ttlsa.com/web/nginx-1.4.2.tar.gz?st=LSVzmZllg68AJaBmeK3E8Q&e=1378881984
页面不要刷新，等到5分钟后在下载一次，你会发现点击下载会跳转到403页面。

secure link 防盗链原理
?用户访问down.php
?down.php根据secret密钥、过期时间、文件uri生成加密串
?将加密串与过期时间作为参数跟到文件下载地址的后面
?nginx下载服务器接收到了过期时间，也使用过期时间、配置里密钥、文件uri生成加密串
?将用户传进来的加密串与自己生成的加密串进行对比，一致允许下载，不一致403

整个过程实际上很简单，类似于用户密码验证. 尤为注意的一点是大家一定不要泄露了自己的密钥，否则别人就可以盗链了，除了泄露之外最好能经常更新密钥.

*/  

//ngx_http_secure_link_module现在可以代替ngx_http_accesskey_module，他们功能类似   ngx_http_secure_link_module Nginx的安全模块,免得别人拿webserver权限。
//ngx_http_referer_module具有普通防盗链功能


/*
The ngx_http_secure_link_module module (0.7.18) is used to check authenticity of requested links, protect resources 
from unauthorized access, and limit link lifetime. 
该模块是控制客户端的访问权限的，如果某次访问有权限，可以访问，也可以控制这次在多长时间段内可以访问该文件或者资源
*/

/*
验证权限有两种方式，一种是secure_link_secret，另一种是secure_link+secure_link_md5,他们不能共存，见ngx_http_secure_link_merge_conf

如果是secure_link+secure_link_md5方式，则一般客户端请求uri中携带secure_link $md5_str, time中的md5_str字符串，nginx收到后，进行64base decode
计算出字符串后，把该字符串与secure_link_md5字符串进行MD5运算，同时判断secure_link $md5_str, time中的time和当前时间是否过期，如果字符串
比较相同并且没有过期，则置变量$secure_link为'1'，否则子变量为‘0'，见ngx_http_secure_link_variable，同时把secure_link $md5_str, time
中的time存入变量$secure_link_expires

如果是secure_link_secret方式，则是把客户端请求uri中的/p/5e814704a28d9bc1914ff19fa0c4a00a/link/xx中的5e814704a28d9bc1914ff19fa0c4a00a字符串与
secure_link_secret xxx计算出的MD5值进行比较，相同则有权限，置变量$secure_link为'1'，否则子变量为‘0'。该方式不涉及time过期
*/
static ngx_command_t  ngx_http_secure_link_commands[] = {
    /*
    语法：secure_link $md5_hash[,$expire_time] 
    默认值：none 
    使用字段：location
    这个指令指定这个链接URL的MD5哈希值和过期时间，$md5_hash为基于URL的64位编码，$expired_time为自1970-01-01 00:00:00 UTC时间以来的秒数，
    如果你不增加$expire_time，那么这个URL永远不会过期。
    */ //客户端有权限访问该资源，多长时间内都可以访问，过了这个时间段就不能访问了，只有重新获取权限

    /*
       secure_link $md5_str, time中的md5_str一般是从客户端请求中的uri获取，获取到后进行64位decode，然后与secure_link_md5设置的字符串进行MD5
       值运算后的结果比较，同时把time与想在事件比较，看是否过期，比较结果相同并且没有过期则认为权限通过，设置$secure_link为'1'，否则不通过
       设置为‘0’参考ngx_http_secure_link_variable

       超时后会重新把$secure_link设置为‘0’，表示过期。如果匹配MD5_str失败，表示无效，$secure_link设置为空字符串。

       注意md5_str字符串长度不能超过24字节，参考ngx_http_secure_link_variable,MD5值比较也参考ngx_http_secure_link_variable
     */ 
    //如果配置了secure_link_secret则不能配置 secure_link secure_link_md5
     //secure_link生效比较见ngx_http_secure_link_variable，见ngx_http_secure_link_merge_conf

    /*
验证权限有两种方式，一种是secure_link_secret，另一种是secure_link+secure_link_md5,他们不能共存，见ngx_http_secure_link_merge_conf

如果是secure_link+secure_link_md5方式，则一般客户端请求uri中携带secure_link $md5_str, time中的md5_str字符串，nginx收到后，进行64base decode
计算出字符串后，把该字符串与secure_link_md5字符串进行MD5运算，同时判断secure_link $md5_str, time中的time和当前时间是否过期，如果字符串
比较相同并且没有过期，则置变量$secure_link为'1'，否则子变量为‘0'，见ngx_http_secure_link_variable，同时把secure_link $md5_str, time
中的time存入变量$secure_link_expires

如果是secure_link_secret方式，则是把客户端请求uri中的/p/5e814704a28d9bc1914ff19fa0c4a00a/link/xx中的5e814704a28d9bc1914ff19fa0c4a00a字符串与
secure_link_secret xxx计算出的MD5值进行比较，相同则有权限，置变量$secure_link为'1'，否则子变量为‘0'。该方式不涉及time过期
*/ //secure_link $MD5_STR可以从uri中获取，可以参考ngx_http_arg
     
    { ngx_string("secure_link"),   //注意还存在变量$secure_link_expires和变量$secure_link
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_set_complex_value_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_secure_link_conf_t, variable),
      NULL },

/*
语法：secure_link_md5 $the_uri_you_want_to_hashed_by_md5
默认值：none 
使用字段：location
这个指令指定你需要通过MD5哈希的字符串，字符串可以包含变量，哈希值将和”secure_link”设置的$md5_hash变量进行比较，如果结果相同，
$secure_link变量值为1，否则为空字符串。
*/ //secure_link_md5生效比较见ngx_http_secure_link_variable 
//如果配置了secure_link_secret则不能配置 secure_link secure_link_md5，见ngx_http_secure_link_merge_conf
    { ngx_string("secure_link_md5"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_set_complex_value_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_secure_link_conf_t, md5),
      NULL },

    /*
    语法：secure_link_secret secret_word 
    默认值：none 
    使用字段：location
    指令为审核请求指定一个秘密字段，一个被保护连接的完整网址如下：
    /prefix/MD5 hash/reference
    在secret指令中指定的secret_word将被计算为一个MD5值而串联到受保护的链接中，例如：受保护的文件top_secret_file.pdf位于目录p。nginx配置为：
    location /p/ {
        secure_link_secret segredo;
     
        if ($secure_link = "") {
            return 403;
        }
     
        rewrite ^ /p/$secure_link break;
    }
    
    你可以通过openssl命令来计算MD5值：
    echo -n 'top_secret_file.pdfsegredo' | openssl dgst -md5
    
    得到的值为0849e9c72988f118896724a0502b92a8。可以通过下面首保护的连接访问:
    http://example.com/p/0849e9c72988f118896724a0502b92a8/top_secret_file.pdf
    
    注意，不能使用跟路径，即/，例如：
    location / {
   #不能使用/
       secure_link_secret segredo;
       [...]
    }
    */ //如果配置了secure_link_secret则不能配置 secure_link secure_link_md5，见ngx_http_secure_link_merge_conf

        /*
    验证权限有两种方式，一种是secure_link_secret，另一种是secure_link+secure_link_md5,他们不能共存，见ngx_http_secure_link_merge_conf
    
    如果是secure_link+secure_link_md5方式，则一般客户端请求uri中携带secure_link $md5_str, time中的md5_str字符串，nginx收到后，进行64base decode
    计算出字符串后，把该字符串与secure_link_md5字符串进行MD5运算，同时判断secure_link $md5_str, time中的time和当前时间是否过期，如果字符串
    比较相同并且没有过期，则置变量$secure_link为'1'，否则子变量为‘0'，见ngx_http_secure_link_variable，同时把secure_link $md5_str, time
    中的time存入变量$secure_link_expires
    
    如果是secure_link_secret方式，则是把客户端请求uri中的/p/5e814704a28d9bc1914ff19fa0c4a00a/link/xx中的5e814704a28d9bc1914ff19fa0c4a00a字符串与
    secure_link_secret xxx计算出的MD5值进行比较，相同则有权限，置变量$secure_link为'1'，否则子变量为‘0'。该方式不涉及time过期
    */

    { ngx_string("secure_link_secret"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_secure_link_conf_t, secret),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_secure_link_module_ctx = {
    ngx_http_secure_link_add_variables,    /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_secure_link_create_conf,      /* create location configuration */
    ngx_http_secure_link_merge_conf        /* merge location configuration */
};

//ngx_http_secure_link_module现在可以代替ngx_http_accesskey_module，他们功能类似   ngx_http_secure_link_module Nginx的安全模块,免得别人拿webserver权限。
//ngx_http_referer_module具有普通防盗链功能

//该模块一般需要专门的客户端支持
ngx_module_t  ngx_http_secure_link_module = { 
//访问权限控制相关模块:nginx进行访问限制的有ngx_http_access_module模块和 ngx_http_auth_basic_module模块   ngx_http_secure_link_module
    NGX_MODULE_V1,
    &ngx_http_secure_link_module_ctx,      /* module context */
    ngx_http_secure_link_commands,         /* module directives */
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


/*
变量

$secure_link
根据你是否使用”secure_link_secret”，这个值有两个不同的意义：
如果使用”secure_link_secret”，并且验证的URL通过验证，这个值为true，否则为空字符串。
如果使用”secure_link”和”secure_link_md5”。并且验证的URL通过验证$secure_link为'1'。如果本地时间超过$expire_time, $secure_link值为'0'。否则，将为空字符串。

$secure_link_expires
等于变量$expire_time的值。

The status of these checks is made available in the $secure_link variable. 
//是否有权限访问的判断结果存储在secure_link中，为1可以访问，为0不能访问

*/ 

//$secure_link变量赋值见ngx_http_secure_link_variable
static ngx_str_t  ngx_http_secure_link_name = ngx_string("secure_link");
//$secure_link_expires变量赋值见ngx_http_secure_link_expires_variable,表示secure_link $md5_str, time中的time，每次客户端请求过来
//都要判断该时间，过期置$secure_link为0，没过期置1，见ngx_http_secure_link_variable
static ngx_str_t  ngx_http_secure_link_expires_name =
    ngx_string("secure_link_expires");

//获取secure_link配置的值，同时与secure_link_md5运算的值进行比较，相同则置$secure_link变量为1，不同则置0
static ngx_int_t
ngx_http_secure_link_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                       *p, *last;
    ngx_str_t                     val, hash;
    time_t                        expires;
    ngx_md5_t                     md5;
    ngx_http_secure_link_ctx_t   *ctx;
    ngx_http_secure_link_conf_t  *conf;
    u_char                        hash_buf[16], md5_buf[16];

    conf = ngx_http_get_module_loc_conf(r, ngx_http_secure_link_module);

    if (conf->secret.data) {
        return ngx_http_secure_link_old_variable(r, conf, v, data);
    }

    if (conf->variable == NULL || conf->md5 == NULL) {
        goto not_found;
    }

    if (ngx_http_complex_value(r, conf->variable, &val) != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "secure link: \"%V\"", &val);

    last = val.data + val.len;

    p = ngx_strlchr(val.data, last, ','); ////secure_link md5_str, 120配置，
    expires = 0;

    if (p) {
        val.len = p++ - val.data; //这时候，val.len=md5_str的字符串长度了，不包括后面的,120字符串

        expires = ngx_atotm(p, last - p); //获取//secure_link md5_str, 120配置中的120
        if (expires <= 0) {
            goto not_found;
        }

        ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_secure_link_ctx_t));
        if (ctx == NULL) {
            return NGX_ERROR;
        }

        ngx_http_set_ctx(r, ctx, ngx_http_secure_link_module);

        ctx->expires.len = last - p;
        ctx->expires.data = p;
    }

    //这时候，val.len=md5_str的字符串长度了，不包括后面的,120字符串
    if (val.len > 24) {//secure_link配置解析出的字符串不能超过24个字符 ?????? 为什么呢
        goto not_found;
    }

    hash.len = 16;
    hash.data = hash_buf;

    //把secure_link md5_str, 120配置中的md5_str字符串进行base64解密decode运算后的值存到这里面    
    if (ngx_decode_base64url(&hash, &val) != NGX_OK) {
        goto not_found;
    }

    if (hash.len != 16) {
        goto not_found;
    }

    //解析secure_link_md5 $the_uri_you_want_to_hashed_by_md5配置的字符串
    if (ngx_http_complex_value(r, conf->md5, &val) != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "secure link md5: \"%V\"", &val);

    //echo -n 'ttlsa.com/download/nginx-1.9.2.rar1452130593' |     openssl md5 -binary | openssl base64 | tr +/ -_ | tr -d =
    ngx_md5_init(&md5);
    ngx_md5_update(&md5, val.data, val.len);
    ngx_md5_final(md5_buf, &md5);

    if (ngx_memcmp(hash_buf, md5_buf, 16) != 0) { //比较判断是否一致
        goto not_found;
    }

    //设置v变量对应的值为0或者1
    v->data = (u_char *) ((expires && expires < ngx_time()) ? "0" : "1");
    v->len = 1;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;

not_found:

    v->not_found = 1;

    return NGX_OK;
}


static ngx_int_t
ngx_http_secure_link_old_variable(ngx_http_request_t *r,
    ngx_http_secure_link_conf_t *conf, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    u_char      *p, *start, *end, *last;
    size_t       len;
    ngx_int_t    n;
    ngx_uint_t   i;
    ngx_md5_t    md5;
    u_char       hash[16];

    p = &r->unparsed_uri.data[1];
    last = r->unparsed_uri.data + r->unparsed_uri.len;

    // 获取/p/5e814704a28d9bc1914ff19fa0c4a00a/link/xx请求中5e814704a28d9bc1914ff19fa0c4a00a字符串
    while (p < last) {
        if (*p++ == '/') {
            start = p;
            goto md5_start;
        }
    }

    goto not_found;

md5_start:
    // 获取/p/5e814704a28d9bc1914ff19fa0c4a00a/link/xx请求中/link/xx字符串做为新的uri
    while (p < last) {
        if (*p++ == '/') {
            end = p - 1;
            goto url_start;
        }
    }

    goto not_found;

url_start:

    len = last - p;

    //5e814704a28d9bc1914ff19fa0c4a00a必须满足32字节，因为MD5运算结果是32字节
    if (end - start != 32 || len == 0) {
        goto not_found;
    }

    //计算secure_link_secret配置的字符串的MD5校验值
    ngx_md5_init(&md5);
    ngx_md5_update(&md5, p, len);
    ngx_md5_update(&md5, conf->secret.data, conf->secret.len);
    ngx_md5_final(hash, &md5);

    for (i = 0; i < 16; i++) {
        n = ngx_hextoi(&start[2 * i], 2);
        if (n == NGX_ERROR || n != hash[i]) {
            goto not_found;
        }
    }

    v->len = len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;

not_found:

    v->not_found = 1;

    return NGX_OK;
}


static ngx_int_t
ngx_http_secure_link_expires_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_secure_link_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_secure_link_module);

    if (ctx) {
        v->len = ctx->expires.len;
        v->valid = 1;
        v->no_cacheable = 0;
        v->not_found = 0;
        v->data = ctx->expires.data;

    } else {
        v->not_found = 1;
    }

    return NGX_OK;
}


static void *
ngx_http_secure_link_create_conf(ngx_conf_t *cf)
{
    ngx_http_secure_link_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_secure_link_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->variable = NULL;
     *     conf->md5 = NULL;
     *     conf->secret = { 0, NULL };
     */

    return conf;
}


static char *
ngx_http_secure_link_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_secure_link_conf_t *prev = parent;
    ngx_http_secure_link_conf_t *conf = child;

    if (conf->secret.data) { //如果配置了secure_link_secret则不能配置 secure_link secure_link_md5
        if (conf->variable || conf->md5) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"secure_link_secret\" cannot be mixed with "
                               "\"secure_link\" and \"secure_link_md5\"");
            return NGX_CONF_ERROR;
        }

        return NGX_CONF_OK;
    }

    if (conf->variable == NULL) {
        conf->variable = prev->variable;
    }

    if (conf->md5 == NULL) {
        conf->md5 = prev->md5;
    }

    if (conf->variable == NULL && conf->md5 == NULL) {
        conf->secret = prev->secret;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_secure_link_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var;

    var = ngx_http_add_variable(cf, &ngx_http_secure_link_name, 0);
    if (var == NULL) {
        return NGX_ERROR;
    }

    var->get_handler = ngx_http_secure_link_variable;

    var = ngx_http_add_variable(cf, &ngx_http_secure_link_expires_name, 0);
    if (var == NULL) {
        return NGX_ERROR;
    }

    var->get_handler = ngx_http_secure_link_expires_variable;

    return NGX_OK;
}
