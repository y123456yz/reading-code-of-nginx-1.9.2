
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_CACHE_H_INCLUDED_
#define _NGX_HTTP_CACHE_H_INCLUDED_

/*
缓存涉及相关:
?http://blog.csdn.net/brainkick/article/details/8535242 
?http://blog.csdn.net/brainkick/article/details/8570698 
?http://blog.csdn.net/brainkick/article/details/8583335 
?http://blog.csdn.net/brainkick/article/details/8592027 
?http://blog.csdn.net/brainkick/article/details/39966271 
*/

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define NGX_HTTP_CACHE_MISS          1
//xxx_cache_bypass 配置指令可以使满足既定条件的请求绕过缓存数据，但是这些请求的响应数据依然可以被 upstream 模块缓存。 
#define NGX_HTTP_CACHE_BYPASS        2 //说明不能从缓存获取，应该从后端获取 通过xxx_cache_bypass设置判断ngx_http_test_predicates(r, u->conf->cache_bypass)
#define NGX_HTTP_CACHE_EXPIRED       3  //缓存内容过期，当前请求需要向后端请求新的响应数据。
#define NGX_HTTP_CACHE_STALE         4
#define NGX_HTTP_CACHE_UPDATING      5 //缓存内容过期，同时己有同样使用该缓存节点的其它请求正在向请求新的响应数据。
#define NGX_HTTP_CACHE_REVALIDATED   6
#define NGX_HTTP_CACHE_HIT           7 //缓存正常命中
/*
因缓存节点被查询次数还未达 `min_uses`，对此请求禁用缓存机制继续请求处理，但是不再缓存其响应数据 (`u->cacheable = 0`)。
*/
#define NGX_HTTP_CACHE_SCARCE        8

#define NGX_HTTP_CACHE_KEY_LEN       16
#define NGX_HTTP_CACHE_ETAG_LEN      42
#define NGX_HTTP_CACHE_VARY_LEN      42

#define NGX_HTTP_CACHE_VERSION       3


typedef struct { //创建空间和赋值见ngx_http_file_cache_valid_set_slot
    ngx_uint_t                       status; //2XX 3XX 4XX 5XX等，如果为0表示proxy_cache_valid any 3m;
    time_t                           valid; //proxy_cache_valid xxx 4m;中的4m
} ngx_http_cache_valid_t;

//结构体 ngx_http_file_cache_node_t 保存磁盘缓存文件在内存中的描述信息 
//一个cache文件对应一个node，这个node中主要保存了cache 的key和uniq， uniq主要是关联文件，而key是用于红黑树。
typedef struct { //ngx_http_file_cache_add中创建 //ngx_http_file_cache_exists中创建空间和赋值    
    ngx_rbtree_node_t                node; /* 缓存查询树的节点 */ //node就是本ngx_http_file_cache_node_t结构的前面ngx_rbtree_node_t个字节
    ngx_queue_t                      queue; /* LRU页面置换算法 队列中的节点 */
    
    //参考ngx_http_file_cache_exists，存储的是ngx_http_cache_t->key中的后面一些字节
    u_char                           key[NGX_HTTP_CACHE_KEY_LEN
                                         - sizeof(ngx_rbtree_key_t)]; 

    //ngx_http_file_cache_exists中第一次创建的时候默认为1  ngx_http_file_cache_update会剪1
    unsigned                         count:20;    /* 引用计数 */ //参考
    unsigned                         uses:10;    /* 被请求查询到的次数 */     //多少请求在使用  ngx_http_file_cache_exists没查找到一次自增一次
/*
valid_sec , valid_msec – 缓存内容的过期时间，缓存内容过期后被查询 时会由 ngx_http_file_cache_read 返回 NGX_HTTP_CACHE_STALE ，
然后由fastcgi_cache_use_stale配置指令决定是否及何种情况下使用过期内容。 
*/
    unsigned                         valid_msec:10;
    /*
    当后端响应码 >= NGX_HTTP_SPECIAL_RESPONSE , 并且打开了fastcgi_intercept_errors 配置，同时 fastcgi_cache_valid 配置指令和 
error_page 配置指令也对该响应码做了设定的情况下，该字段记录响应码， 并列的valid_sec字段记录该响应码的持续时间。这种error节点并不对 
应实际的缓存文件。
     */
    unsigned                         error:10;
/*
该缓存节点是否有对应的缓存文件。新创建的缓存节点或者过期的error节点 (参见error字段，当error不等于0时，Nginx 随后也不 
会再关心该节点的exists字段值) 该字段值为0。当正常节点(error等于0) 的exists为0时，进入cache lock 模式。 
*/
    unsigned                         exists:1;//是否存在对应的cache文件
//updating – 缓存内容过期，某个请求正在获取有效的后端响应并更新此缓存节点。参见 ngx_http_cache_t::updating 。 
    unsigned                         updating:1;
    //参考ngx_http_file_cache_delete
    unsigned                         deleting:1;     /* 正在被清理中 */     //是否正在删除
                                     /* 11 unused bits */

    ngx_file_uniq_t                  uniq;//文件的uniq  赋值见ngx_http_file_cache_update
    //expires – 缓存节点的可回收时间 (附带缓存内容)。 
    time_t                           expire;//cache失效时间  该node失效时间，参考ngx_http_file_cache_exists
    time_t                           valid_sec; //比如cache control中的max-age
    size_t                           body_start; //其实应该是body大小  赋值见ngx_http_file_cache_update
    off_t                            fs_size; //文件大小   赋值见ngx_http_file_cache_update
    ngx_msec_t                       lock_time;
} ngx_http_file_cache_node_t;

//参考: nginx proxy cache分析  http://blog.csdn.net/xiaolang85/article/details/38260041
//参考:nginx proxy cache的实现原理 http://blog.itpub.net/15480802/viewspace-1421409/
/*
请求对应的缓存条目的完整信息 (请求使用的缓存 file_cache 、缓存条目对应的缓存节点信息 node 、缓存文件 file 、key 值及其检验 crc32 等等) 
都临时保存于ngx_http_cache_t(r->cache) 结构体中，这个结构体中的信息量基本上相当于ngx_http_file_cache_header_t 和 ngx_http_file_cache_node_t的总和： 
*/ //ngx_http_upstream_cache->ngx_http_file_cache_new中创建空间

//ngx_http_upstream_init_request->ngx_http_upstream_cache 客户端获取缓存。后端应答回来数据后在ngx_http_file_cache_create中创建临时文件
//后端缓存文件创建在ngx_http_upstream_send_response，后端应答数据在ngx_http_upstream_send_response->ngx_http_upstream_process_request->ngx_http_file_cache_update中进行缓存
//
struct ngx_http_cache_s {//保存于ngx_http_request_s->cache
    ngx_file_t                       file;    /* 缓存文件描述结构体 */ //cache file的创建见ngx_http_file_cache_name
    //初始化在ngx_http_file_cache_new
    //ngx_http_xxx_create_key把xxx_cache_key (proxy_cache_key  fastcgi_cache_key)中的配置的value解析出来保存到kyes数组中，一般只会生效一个xxx_cache_key，
    ngx_array_t                      keys; //这里面存储的值是从ngx_http_xxx_create_key来的(例如ngx_http_fastcgi_create_key)
    uint32_t                         crc32; //ngx_http_file_cache_create_key,就是上面的keys数组中的所有xxx_cache_key配置中的字符串进行crc32校验值   ·
    u_char                           key[NGX_HTTP_CACHE_KEY_LEN];//xxx_cache_key配置字符串进行MD5运算的值
    //ngx_memcpy(c->main, c->key, NGX_HTTP_CACHE_KEY_LEN);
    u_char                           main[NGX_HTTP_CACHE_KEY_LEN];//获取xxx_cache_key配置字符串进行MD5运算的值

    ngx_file_uniq_t                  uniq;
    //参考ngx_http_upstream_process_cache_control  ngx_http_upstream_process_accel_expires 可以由后端应答头部行决定
    //如果后端没有携带这两个函数中头部行携带的时间自动，则通过ngx_http_file_cache_valid(proxy_cache_valid xxx 4m;)获取时间
    time_t                           valid_sec; 
    //后端携带的头部行"Last-Modified:XXX"赋值，见ngx_http_upstream_process_last_modified
    time_t                           last_modified;
    
    time_t                           date;

    ngx_str_t                        etag; //后端返回头部行 "etab:xxxx"
    ngx_str_t                        vary;
    u_char                           variant[NGX_HTTP_CACHE_KEY_LEN];

    /*
     ////[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header][body]
    c->header_start = sizeof(ngx_http_file_cache_header_t)
                      + sizeof(ngx_http_file_cache_key) + len + 1; //+1是因为key后面有有个'\N'
     */ //实际在接收后端第一个头部行相关信息的时候，会预留u->buffer.pos += r->cache->header_start;字节，见ngx_http_upstream_process_header
     //表示缓存中的header部分内容长度[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header]
    size_t                           header_start; //见ngx_http_file_cache_create_key
    //r->cache->body_start = (u_short) (u->buffer.pos - u->buffer.start); 见ngx_http_upstream_send_response//本次epoll从后端读取数据的长度,并准备缓存
    //c->body_start = u->conf->buffer_size; //xxx_buffer_size(fastcgi_buffer_size proxy_buffer_size memcached_buffer_size)
    //缓存中[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header][body]的[body]//后端返回的网页包体部分在buffer中的存储位置

    //ngx_http_upstream_send_response中的(r->cache->body_start = (u_short) (u->buffer.pos - u->buffer.start); //后端返回的网页包体部分在buffer中的存储位置
    size_t                           body_start;
    
    off_t                            length;
    off_t                            fs_size;
//c->min_uses = u->conf->cache_min_uses; //Proxy_cache_min_uses number 默认为1，当客户端发送相同请求达到规定次数后，nginx才对响应数据进行缓存；
    ngx_uint_t                       min_uses;
    ngx_uint_t                       error;
    ngx_uint_t                       valid_msec;

    ngx_buf_t                       *buf;/* 存储缓存文件头 */
    

    ngx_http_file_cache_t           *file_cache;//通过ngx_http_upstream_cache_get获取
    //ngx_http_file_cache_node_t  最近获取到的(新创建或者遍历查询得到的)ngx_http_file_cache_node_t，见ngx_http_file_cache_exists
    //在获取后端数据前，首先会会查找缓存是否有缓存该请求数据，如果没有，则会在ngx_http_file_cache_open中创建node,然后继续去后端获取数据
    ngx_http_file_cache_node_t      *node; //ngx_http_file_cache_exists中创建空间和赋值

#if (NGX_THREADS)
    ngx_thread_task_t               *thread_task;
#endif

    ngx_msec_t                       lock_timeout; //参考下面的lock  //proxy_cache_lock_timeout 设置，默认5S
    ngx_msec_t                       lock_age; //XXX_cache_lock_age  proxy_cache_lock_age
    ngx_msec_t                       lock_time;
    ngx_msec_t                       wait_time;

    ngx_event_t                      wait_event;

/*
这个主要解决一个问题: //proxy_cache_lock 默认off 0  //proxy_cache_lock_timeout 设置，默认5S
假设现在又两个客户端，一个客户端正在获取后端数据，并且后端返回了一部分，则nginx会缓存这一部分，并且等待所有后端数据返回继续缓存。
但是在缓存的过程中如果客户端2页来想后端去同样的数据uri等都一样，则会去到客户端缓存一半的数据，这时候就可以通过该配置来解决这个问题，
也就是客户端1还没缓存完全部数据的过程中客户端2只有等客户端1获取完全部后端数据，或者获取到proxy_cache_lock_timeout超时，则客户端2只有从后端获取数据
*/
    unsigned                         lock:1;//c->lock = u->conf->cache_lock;
    //缓存内容己过期，当前请求正等待其它请求更新此缓存节点。 
    unsigned                         waiting:1; //ngx_http_file_cache_lock中置1

    unsigned                         updated:1;
/* updating – 缓存内容己过期，并且当前请求正在获取有效的后端响应并更新此缓存节点。参见 ngx_http_file_cache_node:updating 。 
如果一个客户端在获取后端数据，有可能需要和后端多次epoll read才能获取完，则在获取过程中当一个对象未缓存完整时，
一个名为updating的成员会置1，同时exists成员置0
*/
    unsigned                         updating:1;
//该请求的缓存已经存在，并且对该缓存的请求次数达到了最低要求次数min_uses,则exists会在ngx_http_file_cache_exists中置1
    unsigned                         exists:1;
    unsigned                         temp_file:1;
    unsigned                         reading:1; //只有打开file aio才会在ngx_http_file_cache_aio_read中置1
    unsigned                         secondary:1;
};

/*
每个文件系统中的缓存文件都有固定的存储格式，其中 ngx_http_file_cache_header_t为包头结构，存储缓存文件的相关信息
(修改时间、缓存 key 的 crc32 值、和用于指明 HTTP 响应包头和包体在缓存文件中偏移位置的字段等)：

root@root:/var/yyz/cache_xxx# cat b/7d/bf6813c2bc0becb369a8d8367b6b77db 
oV尛oVZ"  
KEY: /test.php
IX-Powered-By: PHP/5.2.13
Content-type: text/html

//下面才是真正的文件内容
<Html> 
<Head> 
<title>Your page Subject and domain name</title>

<Meta NAME="" CONTENT="">
"" your others meta tagB
"" your others meta tag
"" your others meta tag
"" your others meta tag
"" your others meta tag
"" your others meta tag
"" your others meta tag
*/ 
//实际在接收后端第一个头部行相关信息的时候，会预留u->buffer.pos += r->cache->header_start;字节，见ngx_http_upstream_process_header
//[ngx_http_file_cache_header_t]["\nKEY: "][orig_key]["\n"][header][body] ，见ngx_http_file_cache_create_key
typedef struct { //赋值等见ngx_http_file_cache_set_header
    ngx_uint_t                       version;
    time_t                           valid_sec;
    time_t                           last_modified;
    time_t                           date;
    uint32_t                         crc32;
    u_short                          valid_msec;
    /*
    ////[ngx_http_file_cache_header_t]["\nKEY: "][orig_key]["\n"][header][body]
    c->header_start = sizeof(ngx_http_file_cache_header_t)
                      + sizeof(ngx_http_file_cache_key) + len + 1; 
     */
    u_short                          header_start; //指向该
    u_short                          body_start;
    u_char                           etag_len;
    u_char                           etag[NGX_HTTP_CACHE_ETAG_LEN];
    u_char                           vary_len;
    u_char                           vary[NGX_HTTP_CACHE_VARY_LEN];
    u_char                           variant[NGX_HTTP_CACHE_KEY_LEN];
} ngx_http_file_cache_header_t;

//ngx_http_file_cache_init创建空间和初始化
typedef struct { //用于保存缓存节点 和 缓存的当前状态 (是否正在从磁盘加载、当前缓存大小等)；
    //以ngx_http_cache_t->key字符串中的最前面4字节为key来在红黑树中变量，见ngx_http_file_cache_lookup
    ngx_rbtree_t                     rbtree; //红黑树初始化在ngx_http_file_cache_init
    //红黑树初始化在ngx_http_file_cache_init
    ngx_rbtree_node_t                sentinel; //sentinel哨兵代表外部节点，所有的叶子以及根部的父节点，都指向这个唯一的哨兵nil，哨兵的颜色为黑色
    ngx_queue_t                      queue;//队列初始化在ngx_http_file_cache_init
    //cold表示这个cache是否已经被loader进程load过了
    ngx_atomic_t                     cold;  /* 缓存是否可用 (加载完毕) */ 
    ngx_atomic_t                     loading;  /* 是否正在被 loader 进程加载 */ //正在load这个cache
    off_t                            size;    /* 初始化为 0 */ //占用了缓存空间的总大小，赋值见ngx_http_file_cache_update  
} ngx_http_file_cache_sh_t;
//缓存好文章参考:缓存服务器涉及与实现(一  到  五) http://blog.csdn.net/brainkick/article/details/8535242

//ngx_http_upstream_init_request->ngx_http_upstream_cache 客户端获取缓存 后端应答回来数据后在ngx_http_file_cache_create中创建临时文件
//后端缓存文件创建在ngx_http_upstream_send_response，后端应答数据在ngx_http_upstream_send_response->ngx_http_upstream_process_request->ngx_http_file_cache_update中进行缓存

//获取该结构ngx_http_upstream_cache_get，实际上是通过proxy_cache xxx或者fastcgi_cache xxx来获取共享内存块名的，因此必须设置proxy_cache或者fastcgi_cache
struct ngx_http_file_cache_s { //ngx_http_file_cache_set_slot中创建空间，保存在ngx_http_proxy_main_conf_t->caches数组中
    //sh 维护 LRU 结构用于保存缓存节点 和 缓存的当前状态 (是否正在从磁盘加载、当前缓存大小等)；
    ngx_http_file_cache_sh_t        *sh; //ngx_http_file_cache_init中分配空间
    //shpool是用于管理共享内存的 slab allocator ，所有缓存节点占用空间都由它进行分配
    ngx_slab_pool_t                 *shpool;

    ngx_path_t                      *path;//ngx_http_file_cache_set_slot中创建ngx_path_t空间  
    /*
默认情况下p->temp_file->path = u->conf->temp_path; 也就是由ngx_http_fastcgi_temp_path指定路径，但是如果是缓存方式(p->cacheable=1)并且配置
proxy_cache_path(fastcgi_cache_path) /a/b的时候带有use_temp_path=off(表示不使用ngx_http_fastcgi_temp_path配置的path)，
则p->temp_file->path = r->cache->file_cache->temp_path; 也就是临时文件/a/b/temp。use_temp_path=off表示不使用ngx_http_fastcgi_temp_path
配置的路径，而使用指定的临时路径/a/b/temp   见ngx_http_upstream_send_response 
*/
    //如果proxy_cache_path /aa/bb use_temp_path=off ，则temp_path默认为/aa/bb/temp
    ngx_path_t                      *temp_path; //proxy_cache_path带有use_temp_path=on   赋值参考ngx_http_file_cache_set_slot

    off_t                            max_size;//缓存占用硬盘大小最大这么多 proxy_cache_path max_size=128m 最多暂用128M
    size_t                           bsize; //文件系统的block size  ngx_http_file_cache_init中赋值

    time_t                           inactive; //设置缓存驻留时间   //proxy_cache_path inactive=30m 表示该proxy_cache30 天失效

    ngx_uint_t                       files;//当前有多少个cache文件(超过loader_files之后会被清0)
    //loader_files这个值也就是一个阈值，当load的文件个数大于这个值之后，load进程会短暂的休眠(时间位loader_sleep)
    ngx_uint_t                       loader_files;//proxy_cache_path带有loader_files= 表示最多有多少个cache文件
    ngx_msec_t                       last;//最后被manage或者loader访问的时间
    //loader_files这个值也就是一个阈值，当load的文件个数大于这个值之后，load进程会短暂的休眠(时间位loader_sleep)
    //loader_sleep和上面的loader_files配合使用，当文件个数大于loader_files，就会休眠
    //loader_threshold配合上面的last，也就是loader遍历的休眠间隔。
    
    //loader_sleep和上面的loader_files配合使用，当文件个数大于loader_files，就会休眠
    ngx_msec_t                       loader_sleep;//proxy_cache_path带有loader_sleep=
    //loader_threshold配合上面的last，也就是loader遍历的休眠间隔。
    ngx_msec_t                       loader_threshold;//proxy_cache_path带有loader_threshold=

    //fastcgi_cache_path keys_zone=fcgi:10m;中的keys_zone=fcgi:10m指定共享内存名字已经共享内存空间大小
    ngx_shm_zone_t                  *shm_zone;
};


ngx_int_t ngx_http_file_cache_new(ngx_http_request_t *r);
ngx_int_t ngx_http_file_cache_create(ngx_http_request_t *r);
void ngx_http_file_cache_create_key(ngx_http_request_t *r);
ngx_int_t ngx_http_file_cache_open(ngx_http_request_t *r);
ngx_int_t ngx_http_file_cache_set_header(ngx_http_request_t *r, u_char *buf);
void ngx_http_file_cache_update(ngx_http_request_t *r, ngx_temp_file_t *tf);
void ngx_http_file_cache_update_header(ngx_http_request_t *r);
ngx_int_t ngx_http_cache_send(ngx_http_request_t *);
void ngx_http_file_cache_free(ngx_http_cache_t *c, ngx_temp_file_t *tf);
time_t ngx_http_file_cache_valid(ngx_array_t *cache_valid, ngx_uint_t status);

char *ngx_http_file_cache_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
char *ngx_http_file_cache_valid_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


extern ngx_str_t  ngx_http_cache_status[];


#endif /* _NGX_HTTP_CACHE_H_INCLUDED_ */
