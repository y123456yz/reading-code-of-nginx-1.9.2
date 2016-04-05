
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
//见ngx_http_file_cache_open->ngx_http_file_cache_read，然后在ngx_http_upstream_cache会把u->cache_status = NGX_HTTP_CACHE_EXPIRED;
#define NGX_HTTP_CACHE_STALE         4 //表示自己是第一个发现该缓存过期的客户端请求，因此自己需要从后端从新获取
#define NGX_HTTP_CACHE_UPDATING      5 //缓存内容过期，同时己有同样使用该缓存节点的其它请求正在向请求新的响应数据。等其他请求获取完后端数据后，自己在读缓存文件向客户端发送
#define NGX_HTTP_CACHE_REVALIDATED   6
#define NGX_HTTP_CACHE_HIT           7 //缓存正常命中
/*
因缓存节点被查询次数还未达 `min_uses`，对此请求禁用缓存机制继续请求处理，但是不再缓存其响应数据 (`u->cacheable = 0`)。
case NGX_HTTP_CACHE_SCARCE: (函数ngx_http_upstream_cache)
        u->cacheable = 0;

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

/*
为后端应答回来的数据创建缓存文件用该函数获取缓存文件名，客户端请求过来后，也是采用该函数获取缓存文件名，只要
proxy_cache_key $scheme$proxy_host$request_uri配置中的变量对应的值一样，则获取到的文件名肯定是一样的，即使是不同的客户端r，参考ngx_http_file_cache_name
因为不同客户端的proxy_cache_key配置的对应变量value一样，则他们计算出来的ngx_http_cache_s->key[]也会一样，他们的在红黑树和queue队列中的
node节点也会是同一个，参考ngx_http_file_cache_lookup  
*/

/*
   同一个客户端请求r只拥有一个r->ngx_http_cache_t和r->ngx_http_cache_t->ngx_http_file_cache_t结构，同一个客户端可能会请求后端的多个uri，
   则在向后端发起请求前，在ngx_http_file_cache_open->ngx_http_file_cache_exists中会按照proxy_cache_key $scheme$proxy_host$request_uri计算出来的
   MD5来创建对应的红黑树节点，然后添加到ngx_http_file_cache_t->sh->rbtree红黑树中。
*/

/*
缓存文件stat状态信息ngx_cached_open_file_s(ngx_open_file_cache_t->rbtree(expire_queue)的成员   )在ngx_expire_old_cached_files进行失效判断, 
缓存文件内容信息(实实在在的文件信息)ngx_http_file_cache_node_t(ngx_http_file_cache_s->sh中的成员)在ngx_http_file_cache_expire进行失效判断。
*/

//该结构为什么能代表一个缓存文件? 因为ngx_http_file_cache_node_t中的node+key[]就是一个对应的缓存文件的目录f/27/46492fbf0d9d35d3753c66851e81627f中的46492fbf0d9d35d3753c66851e81627f，注意f/27就是最尾部的字节
//该结构式红黑树节点，被添加到ngx_http_file_cache_t->sh->rbtree红黑树中以及ngx_http_file_cache_t->sh->queue队列中
typedef struct { //ngx_http_file_cache_add中创建 //ngx_http_file_cache_exists中创建空间和赋值    
    ngx_rbtree_node_t                node; /* 缓存查询树的节点 */ //node就是本ngx_http_file_cache_node_t结构的前面ngx_rbtree_node_t个字节
    ngx_queue_t                      queue; /* LRU页面置换算法 队列中的节点 */
    
    //参考ngx_http_file_cache_exists，存储的是ngx_http_cache_t->key中的后面一些字节
    u_char                           key[NGX_HTTP_CACHE_KEY_LEN
                                         - sizeof(ngx_rbtree_key_t)]; 

    //ngx_http_file_cache_exists中第一次创建的时候默认为1  ngx_http_file_cache_update会剪1，
    //ngx_http_upstream_finalize_request->ngx_http_file_cache_free也会减1  ngx_http_file_cache_exists中加1，表示有多少个客户端连接在获取该缓存
    unsigned                         count:20;    /* 引用计数 */ //参考
    //只会做自增操作，见ngx_http_file_cache_exists中加1，表示总共有多少个客户端请求该缓存，即使和该客户端连接断开也不会做减1操作
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
*/ //只有客户端请求该uri次数达到Proxy_cache_min_uses 3中的3次才会置1，见ngx_http_file_cache_exists，因为如果没有达到3次，则u->cachable = 0
    //表示该缓存文件是否存在，Proxy_cache_min_uses 3，则第3次后开始获取后端数据，获取完毕后在ngx_http_file_cache_update中置1
    unsigned                         exists:1;//是否存在对应的cache文件，
//updating – 缓存内容过期，某个请求正在获取有效的后端响应并更新此缓存节点。参见 ngx_http_cache_t::updating 。 
    unsigned                         updating:1; //客户端请求到nginx后，发现缓存过期，则会重新从后端获取数据，updating置1，见ngx_http_file_cache_read
    //参考ngx_http_file_cache_delete
    unsigned                         deleting:1;     /* 正在被清理中 */     //是否正在删除
                                     /* 11 unused bits */
    //文件inode节点号，
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

/*
   同一个客户端请求r只拥有一个r->ngx_http_cache_t和r->ngx_http_cache_t->ngx_http_file_cache_t结构，同一个客户端可能会请求后端的多个uri，
   则在向后端发起请求前，在ngx_http_file_cache_open->ngx_http_file_cache_exists中会按照proxy_cache_key $scheme$proxy_host$request_uri计算出来的
   MD5来创建对应的红黑树节点，然后添加到ngx_http_file_cache_t->sh->rbtree红黑树中。所以不同的客户端uri会有不同的node节点存在于红黑树中
*/

/*ngx_http_upstream_init_request->ngx_http_upstream_cache 客户端获取缓存 后端应答回来数据后在ngx_http_upstream_send_response->ngx_http_file_cache_create
中创建临时文件，然后在ngx_event_pipe_write_chain_to_temp_file把读取的后端数据写入临时文件，最后在
ngx_http_upstream_send_response->ngx_http_upstream_process_request->ngx_http_file_cache_update中把临时文件内容rename(相当于mv)到proxy_cache_path指定
的cache目录下面
*/
struct ngx_http_cache_s {//保存于ngx_http_request_s->cache
    //cache file: "/var/yyz/cache_xxx/c/c1/13cc494353644acaed96a080cac13c1c"ngx_http_file_cache_name中把path+level+key
    ngx_file_t                       file;    /* 缓存文件描述结构体 */ //cache file的创建见ngx_http_file_cache_name
    //初始化在ngx_http_file_cache_new  proxy_cache_key $scheme$proxy_host$request_uri
    //ngx_http_xxx_create_key把xxx_cache_key (proxy_cache_key  fastcgi_cache_key)中的配置的value解析出来保存到kyes数组中，一般只会生效一个xxx_cache_key，
    ngx_array_t                      keys; //这里面存储的值是从ngx_http_xxx_create_key来的(例如ngx_http_fastcgi_create_key)
    uint32_t                         crc32; //ngx_http_file_cache_create_key,就是上面的keys数组中的所有xxx_cache_key配置中的字符串进行crc32校验值
    //proxy_cache_key  fastcgi_cache_key配置的参数字符串列表进行MD5计算的结果，见ngx_http_file_cache_create_key
    //例如proxy_cache_key $scheme$proxy_host$request_uri则为这三个参数对应的value进行MD5运算的结果
    u_char                           key[NGX_HTTP_CACHE_KEY_LEN];//xxx_cache_key配置字符串进行MD5运算的值 ngx_http_file_cache_create_key
    //ngx_memcpy(c->main, c->key, NGX_HTTP_CACHE_KEY_LEN);
    u_char                           main[NGX_HTTP_CACHE_KEY_LEN];//获取xxx_cache_key配置字符串进行MD5运算的值

    //文件inode节点号
    ngx_file_uniq_t                  uniq; //赋值见ngx_http_file_cache_exists，真正的来源在//文件的uniq  赋值见ngx_http_file_cache_update
    //参考ngx_http_upstream_process_cache_control  ngx_http_upstream_process_accel_expires 可以由后端应答头部行决定
    //如果后端没有携带这两个函数中头部行携带的时间，则通过ngx_http_file_cache_valid(fastcgi_cache_valid proxy_cache_valid xxx 4m;)获取时间
    //在ngx_http_file_cache_read中读取缓存的时候，会通过if (c->valid_sec < now) { }判断缓存是否过期，
    time_t                           valid_sec; //赋值见ngx_http_upstream_send_response(也就是fastcgi_cache_valid设置的有效时间)  //每次nginx退出的时候，例如kill nginx都会坚持缓存文件，如果过期，则会删除
    //后端携带的头部行"Last-Modified:XXX"赋值，见ngx_http_upstream_process_last_modified
    time_t                           last_modified;
    
    time_t                           date;

    /*
     Etag确定浏览器缓存： Etag的原理是将文件资源编号一个etag值，Response给访问者，访问者再次请求时，带着这个Etag值，与服务端所请求
     的文件的Etag对比，如果不同了就重新发送加载，如果相同，则返回304. HTTP/1.1304 Not Modified
     */ //etag设置见ngx_http_set_etag
    ngx_str_t                        etag; //后端返回头部行 "etab:xxxx"
    ngx_str_t                        vary;//后端返回的头部行带有vary:xxx  见ngx_http_upstream_process_vary
    u_char                           variant[NGX_HTTP_CACHE_KEY_LEN];

    /*
     ////[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header][body]
    c->header_start = sizeof(ngx_http_file_cache_header_t)
                      + sizeof(ngx_http_file_cache_key) + len + 1; //+1是因为key后面有有个'\N'
     */ //实际在接收后端第一个头部行相关信息的时候，会预留u->buffer.pos += r->cache->header_start;字节，见ngx_http_upstream_process_header
     //表示缓存中的header部分内容长度[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header]长度
     //写缓冲区封装过程参考:ngx_http_file_cache_set_header        注意和下面的body_start的区别
    size_t                           header_start; //见ngx_http_file_cache_create_key
    //r->cache->body_start = (u_short) (u->buffer.pos - u->buffer.start); 见ngx_http_upstream_send_response//本次epoll从后端读取数据的长度,并准备缓存
    //c->body_start = u->conf->buffer_size; //xxx_buffer_size(fastcgi_buffer_size proxy_buffer_size memcached_buffer_size)
    //缓存中[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header][body]的[body]前面部分的字节长度
    //后端返回的网页包体部分在buffer中的存储位置

    //ngx_http_upstream_send_response中的(r->cache->body_start = (u_short) (u->buffer.pos - u->buffer.start); 
    //后端返回的网页包体部分在buffer中的存储位置，也就是出去后端头部行部分后的开始处,说白了body_start就是缓存文件中头部行相关部分的长度
    /*
    root@root:/var/yyz# cat cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f   封包过程见ngx_http_file_cache_set_header
     3hwhdBw
     KEY: /test2.php
     
     X-Powered-By: PHP/5.2.13
     Content-type: text/html
    //body_start就是上面这一段内存内容长度
    
     <Html> 
     <title>file update</title>
     <body> 
     <form method="post" action="" enctype="multipart/form-data">
     <input type="file" name="file" /> 
     <input type="submit" value="submit" /> 
     </form> 
     </body> 
     </html>

     注意第三行哪里其实有8字节的fastcgi表示头部结构ngx_http_fastcgi_header_t，通过vi cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f可以看出

 offset    0  1  2  3   4  5  6  7   8  9  a  b   c  d  e  f  0123456789abcdef
00000000 <03>00 00 00  ab 53 83 56  ff ff ff ff  2b 02 82 56  ....玈.V+..V
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


header_start: [ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"] 也就是上面的第一行和第二行
body_start: [ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header]也就是上面的第一到第五行内容
因此:body_start = header_start + [header]部分(例如fastcgi返回的头部行标识部分)
     */ 
     //body_start就是上面的头部相关部分的长度，缓存中[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header][body]的[body]前面部分的字节长度
    //客户端获取缓存文件中前面[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header]部分在函数ngx_http_file_cache_open中

    size_t                           body_start;//写缓冲区头部内容封装过程参考:ngx_http_file_cache_set_header
    
    off_t                            length;//缓存文件的大小，见ngx_http_file_cache_open
    off_t                            fs_size;//文件ngx_http_file_cache_t->bsize字节对齐，见ngx_http_file_cache_open
//c->min_uses = u->conf->cache_min_uses; //Proxy_cache_min_uses number 默认为1，当客户端发送相同请求达到规定次数后，nginx才对响应数据进行缓存；
    ngx_uint_t                       min_uses;
    ngx_uint_t                       error;
    ngx_uint_t                       valid_msec;

    ngx_buf_t                       *buf;/* 存储缓存文件头 */ //ngx_http_file_cache_open中创建空间
    

    ngx_http_file_cache_t           *file_cache;//通过ngx_http_upstream_cache->ngx_http_upstream_cache_get获取
    //ngx_http_file_cache_node_t  最近获取到的(新创建或者遍历查询得到的)ngx_http_file_cache_node_t，见ngx_http_file_cache_exists
    //在获取后端数据前，首先会会查找缓存是否有缓存该请求数据，如果没有，则会在ngx_http_file_cache_open中创建node,然后继续去后端获取数据
    ngx_http_file_cache_node_t      *node; //ngx_http_file_cache_exists中创建空间和赋值

#if (NGX_THREADS)
//ngx_http_file_cache_aio_read->ngx_thread_read中创建空间和赋值
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
    /* 
     缓存内容己过期，当前请求正等待其它请求更新此缓存节点。 注意这是同一个客户端r请求，同一个客户端在获取后端数据的过程中(后端数据还没返回)，又发送一次get请求
     */
     /* ngx_http_file_cache_open如果返回NGX_AGAIN，则会在函数外执行下面的代码，也就是等待前面的请求后端返回后，再次触发后面的请求执行ngx_http_upstream_init_request过程
        这时候前面从后端获取的数据肯定已经得到缓存
        r->write_event_handler = ngx_http_upstream_init_request;  //这么触发该write handler呢?因为前面的请求获取到后端数据后，在触发epoll_in的同时
        也会触发epoll_out，从而会执行该函数
        return;  
     */
    unsigned                         waiting:1; //ngx_http_file_cache_lock中置1

    unsigned                         updated:1;
/* updating – 缓存内容己过期，并且当前请求正在获取有效的后端响应并更新此缓存节点。参见 ngx_http_file_cache_node:updating 。 
如果一个客户端在获取后端数据，有可能需要和后端多次epoll read才能获取完，则在获取过程中当一个对象未缓存完整时，
一个名为updating的成员会置1，同时exists成员置0
*/  //客户端请求到nginx后，发现缓存过期，则会重新从后端获取数据，updating置1，见ngx_http_file_cache_read
    unsigned                         updating:1;
//该请求的缓存已经存在，并且对该缓存的请求次数达到了最低要求次数min_uses,则exists会在ngx_http_file_cache_exists中置1
//只有客户端请求该uri次数达到Proxy_cache_min_uses 3中的3次才会置1，见ngx_http_file_cache_exists，因为如果没有达到3次，则u->cachable = 0

//表示该缓存文件是否存在，Proxy_cache_min_uses 3，则第3次后开始获取后端数据，获取完毕后在ngx_http_file_cache_update中置1，但是只有在地4次请求的时候才会在ngx_http_file_cache_exists赋值为1
    unsigned                         exists:1; 
   
    unsigned                         temp_file:1;
    //只有打开file aio才会在ngx_http_file_cache_aio_read中置1，表示已经通知内核进行读操作，只是现在还没有读取完毕，内核完成读取后会通过epoll事件通知
    // 注意这是同一个客户端r请求，同一个客户端在获取后端数据的过程中(后端数据还没返回)，又发送一次get请求，注意只有aio才有该情况
    unsigned                         reading:1; 
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
typedef struct { //写入文件前赋值等见ngx_http_file_cache_set_header，读取文件中的该头部结构在ngx_http_file_cache_read
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
     /*
    root@root:/var/yyz# cat cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f   封包过程见ngx_http_file_cache_set_header
     3hwhdBw
     KEY: /test2.php
     
     X-Powered-By: PHP/5.2.13
     Content-type: text/html
    //body_start就是上面这一段内存内容长度
    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    
     <Html> 
     <title>file update</title>
     <body> 
     <form method="post" action="" enctype="multipart/form-data">
     <input type="file" name="file" /> 
     <input type="submit" value="submit" /> 
     </form> 
     </body> 
     </html>
     */ 
    //创建存放缓存文件中前面[ngx_http_file_cache_header_t]["\nKEY: "][fastcgi_cache_key中的KEY]["\n"][header]部分的内容长度空间,也就是
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@前面的内容
    u_short                          body_start;
    u_char                           etag_len;
    /*
     Etag确定浏览器缓存： Etag的原理是将文件资源编号一个etag值，Response给访问者，访问者再次请求时，带着这个Etag值，与服务端所请求
     的文件的Etag对比，如果不同了就重新发送加载，如果相同，则返回304. HTTP/1.1304 Not Modified
     */ //etag设置见ngx_http_set_etag
    u_char                           etag[NGX_HTTP_CACHE_ETAG_LEN];
    u_char                           vary_len;
    u_char                           vary[NGX_HTTP_CACHE_VARY_LEN];
    u_char                           variant[NGX_HTTP_CACHE_KEY_LEN];
} ngx_http_file_cache_header_t;

//ngx_http_file_cache_init创建空间和初始化    
//ngx_http_file_cache_s->sh成员就是该结构

/*
缓存文件stat状态信息ngx_cached_open_file_s在ngx_expire_old_cached_files进行失效判断, 缓存文件内容信息(实实在在的文件信息)
ngx_http_file_cache_node_t在ngx_http_file_cache_expire进行失效判断。
*/

/*所有的ngx_http_file_cache_node_t除了添加到上面的rbtree红黑树外，还会添加到队列queue中，红黑树用于按照key来查找对应的node节点，参考
    ngx_http_file_cache_lookup。queue用于快速获取最先添加到queue对了和最后添加queue对了的node节点用于删除跟新等，参考ngx_http_file_cache_expire*/
typedef struct { //用于保存缓存节点 和 缓存的当前状态 (是否正在从磁盘加载、当前缓存大小等)；
    //以ngx_http_cache_t->key字符串中的最前面4字节为key来在红黑树中变量，见ngx_http_file_cache_lookup
    ngx_rbtree_t                     rbtree; //红黑树初始化在ngx_http_file_cache_init
    //红黑树初始化在ngx_http_file_cache_init
    ngx_rbtree_node_t                sentinel; //sentinel哨兵代表外部节点，所有的叶子以及根部的父节点，都指向这个唯一的哨兵nil，哨兵的颜色为黑色
    /*所有的ngx_http_file_cache_node_t除了添加到上面的rbtree红黑树外，还会添加到队列queue中，红黑树用于按照key来查找对应的node节点，参考
    ngx_http_file_cache_lookup。queue用于快速获取最先添加到queue对了和最后添加queue对了的node节点用于删除跟新等，参考ngx_http_file_cache_expire*/
    ngx_queue_t                      queue;//队列初始化在ngx_http_file_cache_init，
    //cold表示这个cache是否已经被loader进程load过了(ngx_cache_loader_process_handler->ngx_http_file_cache_loader)  
    //ngx_http_file_cache_loader置0，表示缓存文件已经加载完毕，ngx_http_file_cache_init默认初始化是置1的
    //test=0,表示进程起来后缓存文件已经加载完毕，为1表示进程刚起来还没有加载缓存文件，默认值1
    ngx_atomic_t                     cold;  /* 缓存是否可用 (加载完毕) */ //进程起来后一般60s开始加载缓存目录，见
    ngx_atomic_t                     loading;  /* 是否正在被 loader 进程加载 */ //正在load这个cache  loader进程pid，见ngx_http_file_cache_loader
    //缓存文件总大小，在文件老化删除后，size会减去删掉这部分大小，见ngx_http_file_cache_delete
    off_t                            size;    /* 初始化为 0 */ //占用了缓存空间的总大小，赋值见ngx_http_file_cache_update  
} ngx_http_file_cache_sh_t; //注意ngx_http_file_cache_sh_t和ngx_open_file_cache_t的区别
//缓存好文章参考:缓存服务器涉及与实现(一  到  五) http://blog.csdn.net/brainkick/article/details/8535242

/*ngx_http_upstream_init_request->ngx_http_upstream_cache 客户端获取缓存 后端应答回来数据后在ngx_http_upstream_send_response->ngx_http_file_cache_create
中创建临时文件，然后在ngx_event_pipe_write_chain_to_temp_file把读取的后端数据写入临时文件，最后在
ngx_http_upstream_send_response->ngx_http_upstream_process_request->ngx_http_file_cache_update中把临时文件内容rename(相当于mv)到proxy_cache_path指定
的cache目录下面
*/

/*
缓存文件stat状态信息ngx_cached_open_file_s(ngx_open_file_cache_t->rbtree(expire_queue)的成员   )在ngx_expire_old_cached_files进行失效判断, 
缓存文件内容信息(实实在在的文件信息)ngx_http_file_cache_node_t(ngx_http_file_cache_s->sh中的成员)在ngx_http_file_cache_expire进行失效判断。
*/

/*
   同一个客户端请求r只拥有一个r->ngx_http_cache_t和r->ngx_http_cache_t->ngx_http_file_cache_t结构，同一个客户端可能会请求后端的多个uri，
   则在向后端发起请求前，在ngx_http_file_cache_open->ngx_http_file_cache_exists中会按照proxy_cache_key $scheme$proxy_host$request_uri计算出来的
   MD5来创建对应的红黑树节点，然后添加到ngx_http_file_cache_t->sh->rbtree红黑树中。所以不同的客户端uri会有不同的node节点存在于红黑树中
*/

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
