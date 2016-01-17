
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define NGX_HTTP_REFERER_NO_URI_PART  ((void *) 4)

/*
The ngx_http_referer_module module is used to block access to a site for requests with invalid values in the “Referer” header 
field. It should be kept in mind that fabricating a request with an appropriate “Referer” field value is quite easy, and so 
the intended purpose of this module is not to block such requests thoroughly but to block the mass flow of requests sent by 
regular browsers. It should also be taken into consideration that regular browsers may not send the “Referer” field even for 
valid requests. 

Example Configuration

valid_referers none blocked server_names
               *.example.com example.* www.example.org/galleries/
               ~\.google\.;

if ($invalid_referer) {
    return 403;
}

referer_hash_bucket_size size;
Default: referer_hash_bucket_size 64;

Sets the bucket size for the valid referers hash tables. The details of setting up hash tables are provided in a separate document. 

Syntax: 
referer_hash_max_size size;
Default: 
referer_hash_max_size 2048;
Sets the maximum size of the valid referers hash tables. The details of setting up hash tables are provided in a separate document. 

Syntax: 
valid_referers none | blocked | server_names | string ...;
Default: —  
Context: 
server, location
 
Specifies the “Referer” request header field values that will cause the embedded $invalid_referer variable to be set to an empty 
string. Otherwise, the variable will be set to “1”. Search for a match is case-insensitive. 

Parameters can be as follows: 
nonethe “Referer” field is missing in the request header; blockedthe “Referer” field is present in the request header, but its 
value has been deleted by a firewall or proxy server; such values are strings that do not start with “http://” or “https://”; 
server_namesthe “Referer” request header field contains one of the server names; arbitrary stringdefines a server name and an 
optional URI prefix. A server name can have an “*” at the beginning or end. During the checking, the server’s port in the “Referer” 
field is ignored; regular expressionthe first symbol should be a “~”. It should be noted that an expression will be matched against 
the text starting after the “http://” or “https://”. 
Example: 
valid_referers none blocked server_names
               *.example.com example.* www.example.org/galleries/
               ~\.google\.;

Embedded Variables
$invalid_refererEmpty string, if the “Referer” request header field value is considered valid, otherwise “1”. 
*/

typedef struct {
    ngx_hash_combined_t      hash;//赋值和初始化见ngx_http_referer_merge_conf

#if (NGX_PCRE)
    //成员类型ngx_regex_elt_t 
    /* 
 用于解析valid_referers server_names ~\.google\.后面跟的正则表达式域名信息，除正则表达式以外的域名信息存储在下面的keys hash数组中

    */
    ngx_array_t             *regex; //创建空间和赋值见ngx_http_add_regex_referer
    ngx_array_t             *server_name_regex;
#endif

    ngx_flag_t               no_referer;//valid_referers none
    ngx_flag_t               blocked_referer; //valid_referers blocked
    ngx_flag_t               server_names; //valid_referers server_names *.example.com example.* www.example.org/galleries/


//创建空间和赋值见ngx_http_valid_referers 里面存储的是valid_referers server_names配置的除正则表达式以外的域名信息，正则表达式域名信息存储在上面的regex
    ngx_hash_keys_arrays_t  *keys;

    //这两个值的生效原理参考ngx_hash_init_t中max_size和bucket_size
    //referer_hash_max_size配置  默认2048  并不是表示实际需要2048个桶，实际需要多少个是算根据有多少个成员添加到桶中来算处理的，参考ngx_hash_init
    ngx_uint_t               referer_hash_max_size; 
    ngx_uint_t               referer_hash_bucket_size;//referer_hash_bucket_size配置 默认64
} ngx_http_referer_conf_t;


static void * ngx_http_referer_create_conf(ngx_conf_t *cf);
static char * ngx_http_referer_merge_conf(ngx_conf_t *cf, void *parent,
    void *child);
static char *ngx_http_valid_referers(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static ngx_int_t ngx_http_add_referer(ngx_conf_t *cf,
    ngx_hash_keys_arrays_t *keys, ngx_str_t *value, ngx_str_t *uri);
static ngx_int_t ngx_http_add_regex_referer(ngx_conf_t *cf,
    ngx_http_referer_conf_t *rlcf, ngx_str_t *name);
#if (NGX_PCRE)
static ngx_int_t ngx_http_add_regex_server_name(ngx_conf_t *cf,
    ngx_http_referer_conf_t *rlcf, ngx_http_regex_t *regex);
#endif
static int ngx_libc_cdecl ngx_http_cmp_referer_wildcards(const void *one,
    const void *two);

/*
我的实现防盗链的做法，也是参考该位前辈的文章。基本原理就是就是一句话：通过判断request请求头的refer是否来源于本站。（当然请求头是来自于客户端的，
是可伪造的，暂不在本文讨论范围内）。

2．  首先我们去了解下什么是HTTP Referer。简言之，HTTP Referer是header的一部分，当浏览器向web服务器发送请求的时候，一般会带上Referer，告诉
服务器我是从哪个页面链接过来的，服务器籍此可以获得一些信息用于处理。比如从我主页上链接到一个朋友那里，他的服务器就能够从HTTP Referer中统计
出每天有多少用户点击我主页上的链接访问他的网站。（注：该文所有用的站点均假设以 http://blog.csdn.net为例）

假如我们要访问资源：http://blog.csdn.net/Beacher_Ma 有两种情况：
1．  我们直接在浏览器上输入该网址。那么该请求的HTTP Referer 就为null
2．  如果我们在其他其他页面中，通过点击，如 http://www.csdn.net 上有一个 http://blog.csdn.net/Beacher_Ma 这样的链接，那么该请求的HTTP Referer 
就为http://www.csdn.net 
*/


/*
一：一般的防盗链如下： 
location ~* \.(gif|jpg|png|swf|flv)$ { 
valid_referers none blocked www.jb51.net jb51.net ; 
if ($invalid_referer) { 
rewrite ^/ http://www.jb51.net/retrun.html; 
#return 403; 
} 
} 


第一行：gif|jpg|png|swf|flv 
表示对gif、jpg、png、swf、flv后缀的文件实行防盗链 
第二行： 表示对www.ingnix.com这2个来路进行判断 
if{}里面内容的意思是，如果来路不是指定来路就跳转到http://www.jb51.net/retrun.html页面，当然直接返回403也是可以的。 

二：针对图片目录防止盗链 

复制代码 代码如下:


location /images/ { 
alias /data/images/; 
valid_referers none blocked server_names *.xok.la xok.la ; 
if ($invalid_referer) {return 403;} 
} 


三：使用第三方模块ngx_http_accesskey_module实现Nginx防盗链 
实现方法如下： 

实现方法如下：
1. 下载NginxHttpAccessKeyModule模块文件：Nginx-accesskey-2.0.3.tar.gz；
2. 解压此文件后，找到nginx-accesskey-2.0.3下的config文件。编辑此文件：替换其中的”$HTTP_ACCESSKEY_MODULE”为”ngx_http_accesskey_module”；
3. 用一下参数重新编译nginx：
./configure --add-module=path/to/nginx-accesskey
4. 修改nginx的conf文件，添加以下几行：
location /download {
  accesskey             on;
  accesskey_hashmethod  md5;
  accesskey_arg         "key";
  accesskey_signature   "mypass$remote_addr";
}
其中：
accesskey为模块开关；
accesskey_hashmethod为加密方式MD5或者SHA-1；
accesskey_arg为url中的关键字参数；
accesskey_signature为加密值，此处为mypass和访问IP构成的字符串。

访问测试脚本download.php：
<?
$ipkey= md5("mypass".$_SERVER['REMOTE_ADDR']);
$output_add_key="<a href=http://www.jb51.net/download/G3200507120520LM.rar?key=".$ipkey.">download_add_key</a><br />";
$output_org_url="<a href=http://www.jb51.net/download/G3200507120520LM.rar>download_org_path</a><br />";
echo $output_add_key;
echo $output_org_url;
?>
访问第一个download_add_key链接可以正常下载，第二个链接download_org_path会返回403 Forbidden错误。

*/
//ngx_http_secure_link_module现在可以代替ngx_http_accesskey_module，他们功能类似   ngx_http_secure_link_module Nginx的安全模块,免得别人拿webserver权限。
//ngx_http_referer_module具有普通防盗链功能

//$invalid_referer变量，Empty string, if the “Referer” request header field value is considered valid, otherwise '1' 见ngx_http_valid_referers
static ngx_command_t  ngx_http_referer_commands[] = {

    { ngx_string("valid_referers"),//该变量值获取生效在ngx_http_referer_variable
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_valid_referers,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
这两个值的生效原理参考ngx_hash_init_t中max_size和bucket_size
referer_hash_max_size配置  默认2048  并不是表示实际需要2048个桶，实际需要多少个是算根据有多少个成员添加到桶中来算处理的，参考ngx_hash_init
 */
    { ngx_string("referer_hash_max_size"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_referer_conf_t, referer_hash_max_size),
      NULL },

    { ngx_string("referer_hash_bucket_size"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_referer_conf_t, referer_hash_bucket_size),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_referer_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_referer_create_conf,          /* create location configuration */
    ngx_http_referer_merge_conf            /* merge location configuration */
};

//ngx_http_secure_link_module现在可以代替ngx_http_accesskey_module，他们功能类似   ngx_http_secure_link_module Nginx的安全模块,免得别人拿webserver权限。
//ngx_http_referer_module具有普通防盗链功能
ngx_module_t  ngx_http_referer_module = {
    NGX_MODULE_V1,
    &ngx_http_referer_module_ctx,          /* module context */
    ngx_http_referer_commands,             /* module directives */
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

//$invalid_referer变量对应的值获取
static ngx_int_t
ngx_http_referer_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v,
     uintptr_t data)
{
    u_char                    *p, *ref, *last;
    size_t                     len;
    ngx_str_t                 *uri;
    ngx_uint_t                 i, key;
    ngx_http_referer_conf_t   *rlcf;
    u_char                     buf[256];
#if (NGX_PCRE)
    ngx_int_t                  rc;
    ngx_str_t                  referer;
#endif

    rlcf = ngx_http_get_module_loc_conf(r, ngx_http_referer_module);

    if (rlcf->hash.hash.buckets == NULL
        && rlcf->hash.wc_head == NULL
        && rlcf->hash.wc_tail == NULL
#if (NGX_PCRE)
        && rlcf->regex == NULL
        && rlcf->server_name_regex == NULL
#endif
       )
    {
        goto valid;
    }

    if (r->headers_in.referer == NULL) {
        if (rlcf->no_referer) {
            goto valid;
        }

        goto invalid;
    }

    len = r->headers_in.referer->value.len;
    ref = r->headers_in.referer->value.data;

    if (len >= sizeof("http://i.ru") - 1) {
        last = ref + len;

        if (ngx_strncasecmp(ref, (u_char *) "http://", 7) == 0) {
            ref += 7;
            len -= 7;
            goto valid_scheme;

        } else if (ngx_strncasecmp(ref, (u_char *) "https://", 8) == 0) {
            ref += 8;
            len -= 8;
            goto valid_scheme;
        }
    }

    if (rlcf->blocked_referer) {
        goto valid;
    }

    goto invalid;

valid_scheme:

    i = 0;
    key = 0;

    for (p = ref; p < last; p++) {
        if (*p == '/' || *p == ':') {
            break;
        }

        if (i == 256) {
            goto invalid;
        }

        buf[i] = ngx_tolower(*p);
        key = ngx_hash(key, buf[i++]);
    }

    uri = ngx_hash_find_combined(&rlcf->hash, key, buf, p - ref);

    if (uri) {
        goto uri;
    }

#if (NGX_PCRE)

    if (rlcf->server_name_regex) {
        referer.len = p - ref;
        referer.data = buf;

        rc = ngx_regex_exec_array(rlcf->server_name_regex, &referer,
                                  r->connection->log);

        if (rc == NGX_OK) {
            goto valid;
        }

        if (rc == NGX_ERROR) {
            return rc;
        }

        /* NGX_DECLINED */
    }

    if (rlcf->regex) {
        referer.len = len;
        referer.data = ref;

        rc = ngx_regex_exec_array(rlcf->regex, &referer, r->connection->log);

        if (rc == NGX_OK) {
            goto valid;
        }

        if (rc == NGX_ERROR) {
            return rc;
        }

        /* NGX_DECLINED */
    }

#endif

invalid:

    *v = ngx_http_variable_true_value;

    return NGX_OK;

uri:

    for ( /* void */ ; p < last; p++) {
        if (*p == '/') {
            break;
        }
    }

    len = last - p;

    if (uri == NGX_HTTP_REFERER_NO_URI_PART) {
        goto valid;
    }

    if (len < uri->len || ngx_strncmp(uri->data, p, uri->len) != 0) {
        goto invalid;
    }

valid:

    *v = ngx_http_variable_null_value;

    return NGX_OK;
}


static void *
ngx_http_referer_create_conf(ngx_conf_t *cf)
{
    ngx_http_referer_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_referer_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->hash = { NULL };
     *     conf->server_names = 0;
     *     conf->keys = NULL;
     */

#if (NGX_PCRE)
    conf->regex = NGX_CONF_UNSET_PTR;
    conf->server_name_regex = NGX_CONF_UNSET_PTR;
#endif

    conf->no_referer = NGX_CONF_UNSET;
    conf->blocked_referer = NGX_CONF_UNSET;
    conf->referer_hash_max_size = NGX_CONF_UNSET_UINT;
    conf->referer_hash_bucket_size = NGX_CONF_UNSET_UINT;

    return conf;
}


static char *
ngx_http_referer_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_referer_conf_t *prev = parent;
    ngx_http_referer_conf_t *conf = child;

    ngx_uint_t                 n;
    ngx_hash_init_t            hash;
    ngx_http_server_name_t    *sn;
    ngx_http_core_srv_conf_t  *cscf;

    if (conf->keys == NULL) {
        conf->hash = prev->hash;

#if (NGX_PCRE)
        ngx_conf_merge_ptr_value(conf->regex, prev->regex, NULL);
        ngx_conf_merge_ptr_value(conf->server_name_regex,
                                 prev->server_name_regex, NULL);
#endif
        ngx_conf_merge_value(conf->no_referer, prev->no_referer, 0);
        ngx_conf_merge_value(conf->blocked_referer, prev->blocked_referer, 0);
        ngx_conf_merge_uint_value(conf->referer_hash_max_size,
                                  prev->referer_hash_max_size, 2048);
        ngx_conf_merge_uint_value(conf->referer_hash_bucket_size,
                                  prev->referer_hash_bucket_size, 64);

        return NGX_CONF_OK;
    }

    if (conf->server_names == 1) {
        cscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_core_module);

        sn = cscf->server_names.elts;
        for (n = 0; n < cscf->server_names.nelts; n++) {

#if (NGX_PCRE)
            if (sn[n].regex) {

                if (ngx_http_add_regex_server_name(cf, conf, sn[n].regex)
                    != NGX_OK)
                {
                    return NGX_CONF_ERROR;
                }

                continue;
            }
#endif

            if (ngx_http_add_referer(cf, conf->keys, &sn[n].name, NULL)
                != NGX_OK)
            {
                return NGX_CONF_ERROR;
            }
        }
    }

    if ((conf->no_referer == 1 || conf->blocked_referer == 1)
        && conf->keys->keys.nelts == 0
        && conf->keys->dns_wc_head.nelts == 0
        && conf->keys->dns_wc_tail.nelts == 0)
    {
        ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                      "the \"none\" or \"blocked\" referers are specified "
                      "in the \"valid_referers\" directive "
                      "without any valid referer");
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_uint_value(conf->referer_hash_max_size,
                              prev->referer_hash_max_size, 2048);
    ngx_conf_merge_uint_value(conf->referer_hash_bucket_size,
                              prev->referer_hash_bucket_size, 64);
    conf->referer_hash_bucket_size = ngx_align(conf->referer_hash_bucket_size,
                                               ngx_cacheline_size);

    hash.key = ngx_hash_key_lc;
    hash.max_size = conf->referer_hash_max_size;
    hash.bucket_size = conf->referer_hash_bucket_size;
    hash.name = "referer_hash";
    hash.pool = cf->pool;

    if (conf->keys->keys.nelts) {
        hash.hash = &conf->hash.hash; //该hash中存放的是ngx_hash_keys_arrays_t->keys[]数组中的成员
        hash.temp_pool = NULL;

        if (ngx_hash_init(&hash, conf->keys->keys.elts, conf->keys->keys.nelts)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    if (conf->keys->dns_wc_head.nelts) {

        ngx_qsort(conf->keys->dns_wc_head.elts,
                  (size_t) conf->keys->dns_wc_head.nelts,
                  sizeof(ngx_hash_key_t),
                  ngx_http_cmp_referer_wildcards);

        hash.hash = NULL;
        hash.temp_pool = cf->temp_pool;

        if (ngx_hash_wildcard_init(&hash, conf->keys->dns_wc_head.elts,
                                   conf->keys->dns_wc_head.nelts)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }

        //该wc_head hash中存储的是ngx_hash_keys_arrays_t->dns_wc_head数组中的成员
        conf->hash.wc_head = (ngx_hash_wildcard_t *) hash.hash;
    }

    if (conf->keys->dns_wc_tail.nelts) {

        /* 把后置通配符数组dns_wc_tail[]对应的字符串进行排序 */
        ngx_qsort(conf->keys->dns_wc_tail.elts,
                  (size_t) conf->keys->dns_wc_tail.nelts,
                  sizeof(ngx_hash_key_t),
                  ngx_http_cmp_referer_wildcards);

        hash.hash = NULL;
        hash.temp_pool = cf->temp_pool;

        if (ngx_hash_wildcard_init(&hash, conf->keys->dns_wc_tail.elts,
                                   conf->keys->dns_wc_tail.nelts)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }

         //该wc_head hash中存储的是ngx_hash_keys_arrays_t->dns_wc_tail数组中的成员
        conf->hash.wc_tail = (ngx_hash_wildcard_t *) hash.hash;
    }

#if (NGX_PCRE)
    ngx_conf_merge_ptr_value(conf->regex, prev->regex, NULL);
    ngx_conf_merge_ptr_value(conf->server_name_regex, prev->server_name_regex,
                             NULL);
#endif

    if (conf->no_referer == NGX_CONF_UNSET) {
        conf->no_referer = 0;
    }

    if (conf->blocked_referer == NGX_CONF_UNSET) {
        conf->blocked_referer = 0;
    }

    conf->keys = NULL;

    return NGX_CONF_OK;
}


static char *
ngx_http_valid_referers(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_referer_conf_t  *rlcf = conf;

    u_char                    *p;
    ngx_str_t                 *value, uri, name;
    ngx_uint_t                 i;
    ngx_http_variable_t       *var;

    //$invalid_referer变量，Empty string, if the “Referer” request header field value is considered valid, otherwise “1”. ，见ngx_http_valid_referers
    ngx_str_set(&name, "invalid_referer");     //该变量值获取生效在ngx_http_referer_variable

    var = ngx_http_add_variable(cf, &name, NGX_HTTP_VAR_CHANGEABLE);
    if (var == NULL) {
        return NGX_CONF_ERROR;
    }

    var->get_handler = ngx_http_referer_variable;

    if (rlcf->keys == NULL) {
        rlcf->keys = ngx_pcalloc(cf->temp_pool, sizeof(ngx_hash_keys_arrays_t));
        if (rlcf->keys == NULL) {
            return NGX_CONF_ERROR;
        }

        rlcf->keys->pool = cf->pool;
        rlcf->keys->temp_pool = cf->pool;

        if (ngx_hash_keys_array_init(rlcf->keys, NGX_HASH_SMALL) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {
        if (value[i].len == 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid referer \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }

        //the “Referer” field is missing in the request header; 
        //也就是不带referer头部行的满足valid_referers 
        if (ngx_strcmp(value[i].data, "none") == 0) {
            rlcf->no_referer = 1;
            continue;
        }

        /*
        the “Referer” field is present in the request header, but its value has been deleted by a firewall or proxy server; 
        such values are strings that do not start with “http://” or “https://”; 
          */
        if (ngx_strcmp(value[i].data, "blocked") == 0) {
            rlcf->blocked_referer = 1;
            continue;
        }

        //the “Referer” request header field contains one of the server names; 
        if (ngx_strcmp(value[i].data, "server_names") == 0) { //该参数后面跟域名信息
            rlcf->server_names = 1;
            continue;
        }


/*
  arbitrary stringdefines a server name and an optional URI prefix. A server name can have an “*” at the beginning or end. 
  During the checking, the server’s port in the “Referer” field is ignored; regular expressionthe first symbol should be a “~”. 
  It should be noted that an expression will be matched against the text starting after the “http://” or “https://”. 
  
  
  Example: 
  valid_referers  none  blocked  server_names
                 *.example.com example.* www.example.org/galleries/
                 ~\.google\.;
  */
        
        if (value[i].data[0] == '~') {
            if (ngx_http_add_regex_referer(cf, rlcf, &value[i]) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            continue;
        }

        ngx_str_null(&uri);

        p = (u_char *) ngx_strchr(value[i].data, '/');

        if (p) {//如果value中带有/字符，则截取其后的字符串，例如www.example.org/galleries/则value会变为/galleries/
            uri.len = (value[i].data + value[i].len) - p;
            uri.data = p;
            value[i].len = p - value[i].data;
        }

        //如果server_name中包含/字符，则uri为/开头的字符串
        if (ngx_http_add_referer(cf, rlcf->keys, &value[i], &uri) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_add_referer(ngx_conf_t *cf, ngx_hash_keys_arrays_t *keys,
    ngx_str_t *value, ngx_str_t *uri)
{
    ngx_int_t   rc;
    ngx_str_t  *u;

    if (uri == NULL || uri->len == 0) {
        u = NGX_HTTP_REFERER_NO_URI_PART;

    } else {
        u = ngx_palloc(cf->pool, sizeof(ngx_str_t));
        if (u == NULL) {
            return NGX_ERROR;
        }

        *u = *uri;
    }

    rc = ngx_hash_add_key(keys, value, u, NGX_HASH_WILDCARD_KEY);

    if (rc == NGX_OK) {
        return NGX_OK;
    }

    if (rc == NGX_DECLINED) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid hostname or wildcard \"%V\"", value);
    }

    if (rc == NGX_BUSY) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "conflicting parameter \"%V\"", value);
    }

    return NGX_ERROR;
}


static ngx_int_t
ngx_http_add_regex_referer(ngx_conf_t *cf, ngx_http_referer_conf_t *rlcf,
    ngx_str_t *name)
{
#if (NGX_PCRE)
    ngx_regex_elt_t      *re;
    ngx_regex_compile_t   rc;
    u_char                errstr[NGX_MAX_CONF_ERRSTR];

    if (name->len == 1) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "empty regex in \"%V\"", name);
        return NGX_ERROR;
    }

    if (rlcf->regex == NGX_CONF_UNSET_PTR) {
        rlcf->regex = ngx_array_create(cf->pool, 2, sizeof(ngx_regex_elt_t));
        if (rlcf->regex == NULL) {
            return NGX_ERROR;
        }
    }

    re = ngx_array_push(rlcf->regex);
    if (re == NULL) {
        return NGX_ERROR;
    }

    name->len--;
    name->data++;

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.pattern = *name;
    rc.pool = cf->pool;
    rc.options = NGX_REGEX_CASELESS;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;

    if (ngx_regex_compile(&rc) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "%V", &rc.err);
        return NGX_ERROR;
    }

    re->regex = rc.regex;
    re->name = name->data;

    return NGX_OK;

#else

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "the using of the regex \"%V\" requires PCRE library",
                       name);

    return NGX_ERROR;

#endif
}


#if (NGX_PCRE)

static ngx_int_t
ngx_http_add_regex_server_name(ngx_conf_t *cf, ngx_http_referer_conf_t *rlcf,
    ngx_http_regex_t *regex)
{
    ngx_regex_elt_t  *re;

    if (rlcf->server_name_regex == NGX_CONF_UNSET_PTR) {
        rlcf->server_name_regex = ngx_array_create(cf->pool, 2,
                                                   sizeof(ngx_regex_elt_t));
        if (rlcf->server_name_regex == NULL) {
            return NGX_ERROR;
        }
    }

    re = ngx_array_push(rlcf->server_name_regex);
    if (re == NULL) {
        return NGX_ERROR;
    }

    re->regex = regex->regex;
    re->name = regex->name.data;

    return NGX_OK;
}

#endif


static int ngx_libc_cdecl
ngx_http_cmp_referer_wildcards(const void *one, const void *two)
{
    ngx_hash_key_t  *first, *second;

    first = (ngx_hash_key_t *) one;
    second = (ngx_hash_key_t *) two;

    return ngx_dns_strcmp(first->key.data, second->key.data);
}
