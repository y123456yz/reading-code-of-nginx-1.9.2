
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


#ifndef _NGX_OPEN_FILE_CACHE_H_INCLUDED_
#define _NGX_OPEN_FILE_CACHE_H_INCLUDED_


#define NGX_OPEN_FILE_DIRECTIO_OFF  NGX_MAX_OFF_T_VALUE

//可以通过ngx_open_and_stat_file获取文件的相关属性信息
typedef struct {
    ngx_fd_t                 fd;
    ngx_file_uniq_t          uniq;//文件inode节点号 同一个设备中的每个文件，这个值都是不同的
    time_t                   mtime; //文件最后被修改的时间
    off_t                    size;
    off_t                    fs_size;
    //取值是从ngx_http_core_loc_conf_s->directio  //在获取缓存文件内容的时候，只有文件大小大与等于directio的时候才会生效ngx_directio_on
    ////默认NGX_OPEN_FILE_DIRECTIO_OFF是个超级大的值，相当于不使能
    off_t                    directio; //生效见ngx_open_and_stat_file  if (of->directio <= ngx_file_size(&fi)) { ngx_directio_on }
    size_t                   read_ahead;  /* read_ahead配置，默认0 */

    /*
    在ngx_file_info_wrapper中获取文件stat属性信息的时候，如果文件不存在或者open失败，或者stat失败，都会把错误放入这两个字段
    of->err = ngx_errno;
    of->failed = ngx_fd_info_n;
     */
    ngx_err_t                err;
    char                    *failed;
    //open_file_cache_valid 60S  
    //表示60s后来的第一个请求要对文件stat信息做一次检查，检查是否发送变化，如果发送变化则从新获取文件stat信息或者从新创建该阶段，
    //生效在ngx_open_cached_file中的(&& now - file->created < of->valid )
    time_t                   valid;//of.valid = ngx_http_core_loc_conf_t->open_file_cache_valid;  

    ngx_uint_t               min_uses;//ngx_http_core_loc_conf_t->open_file_cache_min_uses;

#if (NGX_HAVE_OPENAT)
    //disable_symlinks on | if_not_owner [from=part];中from携带的参数part对应字符串的字节数
    size_t                   disable_symlinks_from;
    //是否检查文件路径是否有符号连接，见ngx_http_set_disable_symlinks  disable_symlinks命令配置，默认off;
    unsigned                 disable_symlinks:2;
#endif

    unsigned                 test_dir:1;
/*
Ngx_http_core_module.c (src\http):        of.test_only = 1;
Ngx_http_index_module.c (src\http\modules):        of.test_only = 1;
Ngx_http_index_module.c (src\http\modules):    of.test_only = 1;
Ngx_http_log_module.c (src\http\modules):        of.test_only = 1;
Ngx_http_script.c (src\http):    of.test_only = 1;
*/
    unsigned                 test_only:1;
    unsigned                 log:1;
    unsigned                 errors:1;
    unsigned                 events:1;//ngx_http_core_loc_conf_t->open_file_cache_min_uses;

    unsigned                 is_dir:1;
    unsigned                 is_file:1;
    unsigned                 is_link:1;
    unsigned                 is_exec:1;
    //注意这里如果文件大小大于direction设置，则置1，后面会使能direct I/O方式,生效见ngx_directio_on
    unsigned                 is_directio:1; //当文件大小大于directio xxx；中的配置时ngx_open_and_stat_file中会置1
} ngx_open_file_info_t;


typedef struct ngx_cached_open_file_s  ngx_cached_open_file_t;

//ngx_cached_open_file_s是ngx_open_file_cache_t->rbtree(expire_queue)的成员   
/*
缓存文件stat状态信息ngx_cached_open_file_s(ngx_open_file_cache_t->rbtree(expire_queue)的成员   )在ngx_expire_old_cached_files进行失效判断, 
缓存文件内容信息(实实在在的文件信息)ngx_http_file_cache_node_t(ngx_http_file_cache_s->sh中的成员)在ngx_http_file_cache_expire进行失效判断。
*/ //为什么需要内存中保存文件stat信息节点?因为这里面可以保存文件的fd已经文件大小等信息，就不用每次重复打开文件并且获取文件大小信息，可以直接读fd，这样可以提高效率
struct ngx_cached_open_file_s {//ngx_open_cached_file中创建节点   主要存储的是文件的fstat信息，见ngx_open_and_stat_file
    //node.key是文件名做的 hash = ngx_crc32_long(name->data, name->len);//文件名做hash添加到ngx_open_file_cache_t->rbtree红黑树中
    ngx_rbtree_node_t        node;
    ngx_queue_t              queue;

    u_char                  *name;
    time_t                   created; //赋值见ngx_open_cached_file  该缓存文件对应的创建时间
    time_t                   accessed; //该阶段最近一次访问时间 ngx_open_cached_file中跟新

    ngx_fd_t                 fd;
    ngx_file_uniq_t          uniq; //赋值见ngx_open_cached_file，最终来源是ngx_open_and_stat_file
    time_t                   mtime;//赋值见ngx_open_cached_file，最终来源是ngx_open_and_stat_file
    off_t                    size;//赋值见ngx_open_cached_file，最终来源是ngx_open_and_stat_file
    ngx_err_t                err;
    //ngx_open_cached_file->ngx_open_file_lookup每次查找到有该文件，则增加1
    uint32_t                 uses;//ngx_open_cached_file中创建节点结构的时候默认置1

#if (NGX_HAVE_OPENAT)
    size_t                   disable_symlinks_from;
    //是否检查文件路径是否有符号连接，见ngx_http_set_disable_symlinks  disable_symlinks命令配置，默认off;
    unsigned                 disable_symlinks:2;
#endif
    //如果是文件，则在ngx_open_cached_file中加1，ngx_open_file_cleanup中减1，也就是表示有多少个客户端请求在使用该node节点ngx_cached_open_file_s
    //只要有一个客户端r在使用该节点node，则不能释放该node节点，见ngx_close_cached_file
    unsigned                 count:24;//ngx_open_cached_file中创建节点结构的时候默认置0  表示在引用该node节点的客户端个数
    unsigned                 close:1;//在ngx_expire_old_cached_files中从红黑树中移除节点后，会关闭文件，同时把close置1
    unsigned                 use_event:1;//ngx_open_cached_file中创建节点结构的时候默认置0

    //下面的标记赋值见ngx_open_cached_file，最终来源是ngx_open_and_stat_file
    unsigned                 is_dir:1;
    unsigned                 is_file:1;
    unsigned                 is_link:1;
    unsigned                 is_exec:1;
    unsigned                 is_directio:1;

    //只对kqueue有用
    ngx_event_t             *event;//ngx_open_cached_file中创建节点结构的时候默认置NULL
};

/*
缓存文件stat状态信息ngx_cached_open_file_s(ngx_open_file_cache_t->rbtree(expire_queue)的成员   )在ngx_expire_old_cached_files进行失效判断, 
缓存文件内容信息(实实在在的文件信息)ngx_http_file_cache_node_t(ngx_http_file_cache_s->sh中的成员)在ngx_http_file_cache_expire进行失效判断。
*/

/*
nginx有两个指令是管理缓存文件描述符的:一个就是本文中说到的ngx_http_log_module模块的open_file_log_cache配置;存储在ngx_http_log_loc_conf_t->open_file_cache 
另一个是ngx_http_core_module模块的 open_file_cache配置，存储在ngx_http_core_loc_conf_t->open_file_cache;前者是只用来管理access变量日志文件。
后者用来管理的就多了，包括：static，index，tryfiles，gzip，mp4，flv，都是静态文件哦!
这两个指令的handler都调用了函数 ngx_open_file_cache_init ，这就是用来管理缓存文件描述符的第一步：初始化
*/


//为什么需要内存中保存文件stat信息节点?因为这里面可以保存文件的fd已经文件大小等信息，就不用每次重复打开文件并且获取文件大小信息，可以直接读fd，这样可以提高效率
typedef struct { //ngx_open_file_cache_init中创建空间和赋值，在ngx_open_file_cache_cleanup中释放资源
    //该红黑树和expire_queue队列的节点成员是ngx_cached_open_file_s
    ngx_rbtree_t             rbtree;//rbtree红黑树和expire_queue队列中包含的是同样的元素
    ngx_rbtree_node_t        sentinel;
    //这个是用于过期快速判断用的，一般最尾部的最新过期，前面的红黑树rbtree一般是用于遍历查找的
    ngx_queue_t              expire_queue; //队列用于获取第一个插入队列的元素和最后一个擦人队列的元素，前面的rbtree红黑树用于遍历查找

    //初始化为0，在ngx_open_cached_file中创建新节点后+1，在ngx_expire_old_cached_files或者ngx_open_file_cache_remove中减1
    ngx_uint_t               current; //红黑树和expire_queue队列中成员node个数，
    /*
    在ngx_open_cached_file中创建新节点后，做如下判断
    if (cache->current >= cache->max) { //红黑树中节点个数超限了，删除最老的node节点
        ngx_expire_old_cached_files(cache, 0, pool->log);
    }
     */
    ngx_uint_t               max; //open_file_cache max=1000 inactive=20s;中的max
    time_t                   inactive; //open_file_cache max=1000 inactive=20s;中的20s  ngx_expire_old_cached_files中生效
} ngx_open_file_cache_t; //注意ngx_http_file_cache_sh_t和ngx_open_file_cache_t的区别


typedef struct {
    ngx_open_file_cache_t   *cache;
    ngx_cached_open_file_t  *file;
 //file->uses >= min_uses表示只要该ngx_cached_open_file_s file节点被遍历到的次数达到min_uses次，则永远不会关闭文件，除非该cache node失效，见ngx_open_file_cleanup  ngx_close_cached_file
    ngx_uint_t               min_uses; //这个值就是open_file_cache_min_uses 30S配置的时间
    ngx_log_t               *log;
} ngx_open_file_cache_cleanup_t;


typedef struct {

    /* ngx_connection_t stub to allow use c->fd as event ident */
    void                    *data;
    ngx_event_t             *read;
    ngx_event_t             *write;
    ngx_fd_t                 fd;

    ngx_cached_open_file_t  *file;
    ngx_open_file_cache_t   *cache;
} ngx_open_file_cache_event_t;


ngx_open_file_cache_t *ngx_open_file_cache_init(ngx_pool_t *pool,
    ngx_uint_t max, time_t inactive);
ngx_int_t ngx_open_cached_file(ngx_open_file_cache_t *cache, ngx_str_t *name,
    ngx_open_file_info_t *of, ngx_pool_t *pool);


#endif /* _NGX_OPEN_FILE_CACHE_H_INCLUDED_ */
