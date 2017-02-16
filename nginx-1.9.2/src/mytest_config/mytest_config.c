#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
// --add-module=./src/mytest_config --add-module=./src/my_test_module --add-module=./src/mytest_subrequest --add-module=./src/mytest_upstream --add-module=./src/ngx_http_myfilter_module --with-debug --with-file-aio --add-module=./src/sendfile_test
static void*
ngx_http_mytest_config_create_loc_conf(ngx_conf_t *cf);

static char*
ngx_conf_set_myconfig(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static char *
ngx_http_mytest_config_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

ngx_int_t
ngx_http_mytest_config_post_conf(ngx_conf_t *cf);

/* 每个commands中的参数对应一个变量来存储 */
typedef struct
{
    ngx_str_t   	my_str;
    ngx_int_t   	my_num;
    ngx_flag_t   	my_flag;
    size_t		    my_size;
    ngx_array_t*  	my_str_array;
    ngx_array_t*  	my_keyval;
    off_t   	    my_off;
    ngx_msec_t   	my_msec;
    time_t   	    my_sec;
    ngx_bufs_t   	my_bufs;
    ngx_uint_t   	my_enum_seq;
    ngx_uint_t	    my_bitmask;
    ngx_uint_t   	my_access;
    ngx_path_t*	    my_path;

    ngx_str_t		my_config_str;
    ngx_int_t		my_config_num;
} ngx_http_mytest_config_conf_t;

static ngx_conf_enum_t  test_enums[] =
{
    { ngx_string("apple"), 1 },
    { ngx_string("banana"), 2 },
    { ngx_string("orange"), 3 },
    { ngx_null_string, 0 }
};

static ngx_conf_bitmask_t  test_bitmasks[] =
{
    { ngx_string("good"), 0x0002 },
    { ngx_string("better"), 0x0004 },
    { ngx_string("best"), 0x0008 },
    { ngx_null_string, 0 }
};

/*
一下这些配置项中的st方法的conf参数就是用来保存解析出的配置信息的，在ngx_http_mytest_config_create_loc_conf创建

*/ //
static ngx_command_t  ngx_http_mytest_config_commands[] = //所有配置的源头在ngx_init_cycle
{

    {
        ngx_string("test_flag"),
        NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_flag),
        NULL
    },

    {
        ngx_string("test_str"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_str),
        NULL
    },

    /*
    test_str_array配置项也只能出现在location{．．．}块内。如果有以下配置：
    location ... {
        test_str_array      Content-Length ;
        test_str_array      Content-Encoding ;
    }
    那么，my_str_array->nelts的值将是2，表示出现了两个test_str_array配置项。而且my_str_array->elts指向
ngx_str_t类型组成的数组，这样就可以按以下方式访问这两个值。
ngx_str_t*  pstr  =  mycf->my_str_array->elts ;
于是，pstr[0]和pstr[l】可以取到参数值，分别是{len=14;data=“Content-Length”；}和
{len=16;data=“Content-Encoding”；)。从这里可以看到，当处理HTTP头部这样的配置项
时是很适合使用ngx_conf_set_str_array_slot预设方法的。
    */
    {
        ngx_string("test_str_array"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_array_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_str_array),
        NULL
    },

/*
    ngx_conf_set_keyval_slot与ngx_conf_set str_array_slot非常相似，唯一的不同点是
    ngx_conf_set_str_array_slot要求同名配置项后的参数个数是1，而ngx_conf_set_keyval_
    slot则要求配置项后的参数个数是2，分别袁示key/value。如果用ngx_array_t*类型的my_
    keyval变量存储以test_keyval作为配置名的参数，则必须设置NGX_CONF_TAKE2，表示
    test_keyval后跟两个参数
    如果nginx.conf中出现以下配置项：
    location ... {
         test_keyval   Content-Type    image/png ;
         test_keyval   Content-Type    image/gif;
         test_keyval   Accept-Encoding gzip;
    那么，ngx_array_t*类型的my_keyval将会有3个成员，每个成员的类型如T所示。
    typedef struct {
         ngx_str_t  key;
          ngx_str_t  value ;
    } ngx_keyval_t;
    因此，通过遍历my_keyval就可以获取3个成员，分别是{"Content-Type”，“image/png"}、{"Content-Type”，“image/gif")、{"Accept-Encoding”，“gzip”)。
*/
    {
        ngx_string("test_keyval"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE2,
        ngx_conf_set_keyval_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_keyval),
        NULL
    },

    {
        ngx_string("test_num"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_num),
        NULL
    },

    {
        ngx_string("test_size"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_size_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_size),
        NULL
    },

    {
        ngx_string("test_off"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_off_slot, NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_off),
        NULL
    },

    {
        ngx_string("test_msec"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_msec_slot, NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_msec),
        NULL
    },

    {
        ngx_string("test_sec"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_sec_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_sec),
        NULL
    },

/*
Nginx中许多特有的数据结构都会用到两个概念：单个ngx_buf_t缓存区的空间大小和
允许的缓存区个数。ngx_conf_set_bufs_slot就是用于设置它的，它要求配置项后必须携带两
介参数，第1个参数是数字，通常会用来表示缓存区的个数：第2个参数表示单个缓存区的
空间大小，它像ngx_conf_set_size_slot中的参数单位一样，可以不携带单位，也可以使用k
或者K、m或者M作为单位，如“gzip_buffers 4 8k;”。我们用ngx_http_mytest_conf_t结构
中的ngx_bufs_t my_bufs;来存储参数，ngx_bufs_t（12.1.3节ngx_http_upstream_conf_t结构
体中的bufs成员就是应用ngx_bufs_t配置的一个非常好的例子）的定义很简单
*/
    {
        ngx_string("test_bufs"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE2,
        ngx_conf_set_bufs_slot, NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_bufs),
        NULL
    },

    {
        ngx_string("test_enum"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_enum_slot,  //参数必须是test_enums数组中的值
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_enum_seq),
        test_enums
    },

    {
        ngx_string("test_bitmask"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_bitmask_slot,  //参数必须是test_bitmasks数组中的值
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_bitmask),
        test_bitmasks
    },

    {
        ngx_string("test_access"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE123,
        ngx_conf_set_access_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_access),
        NULL
    },

    {
        ngx_string("test_path"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1234,
        ngx_conf_set_path_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_mytest_config_conf_t, my_path),
        NULL
    },

    {
        ngx_string("test_myconfig"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE12,
        ngx_conf_set_myconfig,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },


    ngx_null_command
};

static ngx_http_module_t  ngx_http_mytest_config_module_ctx =
{
    NULL,                              /* preconfiguration */
    ngx_http_mytest_config_post_conf,  /* postconfiguration */

    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */

    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */

/*
这个会在http{} server{} location{}中分别调用一次，
例如可以分别存储server外的test_str， server内location外的test_str, location内的test_str

//http{}中会调用main_conf srv_conf loc_conf分配空间，见ngx_http_core_server。server{}中会调用srv_conf loc_conf创建空间,
见ngx_http_core_server， location{}中会创建loc_conf空间,见ngx_http_core_location
*/
    ngx_http_mytest_config_create_loc_conf, /* create location configuration */ 
/* 

*/
    ngx_http_mytest_config_merge_loc_conf   /* merge location configuration */
};

ngx_module_t  ngx_http_mytest_config_module =
{
    NGX_MODULE_V1,
    &ngx_http_mytest_config_module_ctx,       /* module context */ //通过在ngx_http_module_t中开辟http local server空间来存储后面ngx_http_mytest_commands中解析出的参数
    ngx_http_mytest_config_commands,          /* module directives */
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

static char *
ngx_http_mytest_config_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_mytest_config_conf_t *prev = (ngx_http_mytest_config_conf_t *)parent;
    ngx_http_mytest_config_conf_t *conf = (ngx_http_mytest_config_conf_t *)child;
    
    ngx_conf_merge_str_value(conf->my_str, prev->my_str, "defaultstr");
    ngx_conf_merge_value(conf->my_flag, prev->my_flag, 0);

    return NGX_CONF_OK;
}

static char* ngx_conf_set_myconfig(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    //注意，参数conf就是http框架传给我们的，在ngx_http_mytest_create_loc_conf
//回调方法中分配的结构体ngx_http_mytest_config_conf_t
    ngx_http_mytest_config_conf_t  *mycf = conf;

    // cf->args是1个ngx_array_t队列，它的成员都是ngx_str_t结构。
//我们用value指向ngx_array_t的elts内容，其中value[1]就是第1
//个参数，同理value[2]是第2个参数
    ngx_str_t* value = cf->args->elts;

    //ngx_array_t的nelts表示参数的个数
    if (cf->args->nelts > 1)
    {
        //直接赋值即可， ngx_str_t结构只是指针的传递
        mycf->my_config_str = value[1];
    }
    if (cf->args->nelts > 2)
    {
        //将字符串形式的第2个参数转为整型
        mycf->my_config_num = ngx_atoi(value[2].data, value[2].len);
        //如果字符串转化整型失败，将报"invalid number"错误，
//nginx启动失败
        if (mycf->my_config_num == NGX_ERROR)
        {
            return "invalid number";
        }
    }

    //返回成功
    return NGX_CONF_OK;
}

static void* ngx_http_mytest_config_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_mytest_config_conf_t  *mycf;
    
    mycf = (ngx_http_mytest_config_conf_t  *)ngx_pcalloc(cf->pool, sizeof(ngx_http_mytest_config_conf_t));
    if (mycf == NULL)
    {
        return NULL;
    }

    mycf->my_flag = NGX_CONF_UNSET;
    mycf->my_num = NGX_CONF_UNSET;
    mycf->my_str_array = NGX_CONF_UNSET_PTR;
    mycf->my_keyval = NULL;
    mycf->my_off = NGX_CONF_UNSET;
    mycf->my_msec = NGX_CONF_UNSET_MSEC;
    mycf->my_sec = NGX_CONF_UNSET;
    mycf->my_size = NGX_CONF_UNSET_SIZE;

    return mycf;
}

//for test
extern ngx_module_t  ngx_http_module;
extern ngx_module_t  ngx_http_core_module;

//仅用于遍历location内的test_str字段
void traversal(ngx_conf_t *cf, ngx_http_location_tree_node_t* node)
{
    if (node != NULL)
    {
        traversal(cf, node->left);
        ngx_http_core_loc_conf_t* loc = NULL;
        if (node->exact != NULL)
        {
            loc = (ngx_http_core_loc_conf_t*)node->exact;
        }
        else if (node->inclusive != NULL)
        {
            loc = (ngx_http_core_loc_conf_t*)node->inclusive;
        }

        if (loc != NULL)
        {
            ngx_http_mytest_config_conf_t  *mycf = (ngx_http_mytest_config_conf_t  *)loc->loc_conf[ngx_http_mytest_config_module.ctx_index];
            ngx_log_error(NGX_LOG_ALERT, cf->log, 0, "in location[name=%V]{} test_str=%V",
                          &loc->name, &mycf->my_str);
        }
        else
        {
            ngx_log_error(NGX_LOG_ALERT, cf->log, 0, "wrong location tree");
        }

        traversal(cf, node->right);
    }
}

//下面以test_str为例在屏幕上输出读取到的所有不同块的值
/*
cf空间始终在一个地方，就是ngx_init_cycle中的conf，使用中只是简单的修改conf中的ctx指向已经cmd_type类型，然后在解析当前{}后，重新恢复解析当前{}前的配置
参考"http" "server" "location"ngx_http_block  ngx_http_core_server  ngx_http_core_location  ngx_http_core_location。说明了在不同的{}对相同的配置项的赋值是用不同变量存的
*/

ngx_int_t
ngx_http_mytest_config_post_conf(ngx_conf_t *cf)
{
    ngx_uint_t i = 0;

    //根据一级模块类型NGX_CORE_MODULE确定conf_ctx数组成员指针位置
    ngx_http_conf_ctx_t* http_root_conf = (ngx_http_conf_ctx_t*)ngx_get_conf(cf->cycle->conf_ctx, ngx_http_module);

    ngx_http_mytest_config_conf_t  *mycf;
    mycf = (ngx_http_mytest_config_conf_t  *)http_root_conf->loc_conf[ngx_http_mytest_config_module.ctx_index];
    ngx_log_error(NGX_LOG_ALERT, cf->log, 0, "in http{} test_str=%V", &mycf->my_str);

    ngx_http_core_main_conf_t* core_main_conf = (ngx_http_core_main_conf_t*)
                                                http_root_conf->main_conf[ngx_http_core_module.ctx_index];

    for (i = 0; i < core_main_conf->servers.nelts; i++)
    {
        ngx_http_core_srv_conf_t* tmpcoresrv = *((ngx_http_core_srv_conf_t**)
                                                 (core_main_conf->servers.elts) + i);
        mycf = (ngx_http_mytest_config_conf_t  *)tmpcoresrv->ctx->loc_conf[ngx_http_mytest_config_module.ctx_index];
        ngx_log_error(NGX_LOG_ALERT, cf->log, 0, "in server[name=%V]{} test_str=%V",
                      &tmpcoresrv->server_name, &mycf->my_str);

        ngx_http_core_loc_conf_t* tmpcoreloc = (ngx_http_core_loc_conf_t*)
                                               tmpcoresrv->ctx->loc_conf[ngx_http_core_module.ctx_index];

        traversal(cf, tmpcoreloc->static_locations);
    }

    return NGX_OK;
}

