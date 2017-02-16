
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_FILE_H_INCLUDED_
#define _NGX_FILE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

/*实际上，ngx_open_file与open方法的区别不大，ngx_open_file返回的是Linux系统的文件句柄。对于打开文件的标志位，Nginx也定义了以下几个宏来加以封装。
#define NGX_FILE_RDONLY O_RDONLY
#define NGX_FILE_WRONLY O_WRONLY
#define NGX_FILE_RDWR O_RDWR
#define NGX_FILE_CREATE_OR_OPEN O_CREAT
#define NGX_FILE_OPEN 0
#define NGX_FILE_TRUNCATE O_CREAT|O_TRUNC
#define NGX_FILE_APPEND O_WRONLY|O_APPEND
#define NGX_FILE_NONBLOCK O_NONBLOCK

#define NGX_FILE_DEFAULT_ACCESS 0644
#define NGX_FILE_OWNER_ACCESS 0600

在打开文件时只需要把文件路径传递给name参数，并把打开方式传递给mode、create、access参数即可。例如：
ngx_buf_t *b;
b = ngx_palloc(r->pool, sizeof(ngx_buf_t));

u_char* filename = (u_char*)"/tmp/test.txt";
b->in_file = 1;
b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
b->file->fd = ngx_open_file(filename, NGX_FILE_RDONLY|NGX_FILE_NONBLOCK, NGX_FILE_OPEN, 0);
b->file->log = r->connection->log;
b->file->name.data = filename;
b->file->name.len = sizeof(filename)-1;
if (b->file->fd <= 0)
{
 return NGX_HTTP_NOT_FOUND;
}

到这里其实还没有结束，还需要告知Nginx文件的大小，包括设置响应中的Content-Length头部，以及设置ngx_buf_t缓冲区的file_pos和file_last。
实际上，通过ngx_file_t结构里ngx_file_info_t类型的info变量就可以获取文件信息：
typedef struct stat ngx_file_info_t;

Nginx不只对stat数据结构做了封装，对于由操作系统中获取文件信息的stat方法，Nginx也使用一个宏进行了简单的封装，如下所示：
#define ngx_file_info(file, sb)  stat((const char *) file, sb)

因此，获取文件信息时可以先这样写：
if (ngx_file_info(filename, &b->file->info) == NGX_FILE_ERROR) {
 return NGX_HTTP_INTERNAL_SERVER_ERROR;
}

之后必须要设置Content-Length头部：
r->headers_out.content_length_n = b->file->info.st_size;

还需要设置ngx_buf_t缓冲区的file_pos和file_last：
b->file_pos = 0;
b->file_last = b->file->info.st_size;

这里是告诉Nginx从文件的file_pos偏移量开始发送文件，一直到达file_last偏移量处截止。
注意　当磁盘中有大量的小文件时，会占用Linux文件系统中过多的inode结构，这时，成熟的解决方案会把许多小文件合并成一个大文件。在这种情况下，
当有需要时，只要把上面的file_pos和file_last设置为合适的偏移量，就可以只发送合并大文件中的某一块内容（原来的小文件），
这样就可以大幅降低小文件数量。

Nginx会异步地将整个文件高效地发送给用户，但是我们必须要求HTTP框架在响应发送完毕后关闭已经打开的文件句柄，否则将会出现句柄泄露问题。
设置清理文件句柄也很简单，只需要定义一个ngx_pool_cleanup_t结构体（这是最简单的方法，HTTP框架还提供了其他方式，在请求结束时回调各个HTTP模块的cleanup方法，将在第11章介绍），
将我们刚得到的文件句柄等信息赋给它，并将Nginx提供的ngx_pool_cleanup_file函数设置到它的handler回调方法中即可。

*/
struct ngx_file_s { //一般做为ngx_conf_file_t的成员使用，
    /*
    fd是打开文件的句柄描述符，打开文件这一步需要用户自己来做。Nginx简单封装了一个宏用来代替open系统的调用，如下所示。
    #define ngx_open_file(name, mode, create, access) open((const char *) name, mode|create, access)
    */
    ngx_fd_t                   fd;////文件句柄描述符
    ngx_str_t                  name;//文件名称
    ngx_file_info_t            info;//文件大小等资源信息，实际就是Linux系统定义的stat结构

    /* 该偏移量告诉Nginx现在处理到文件何处了，一般不用设置它，Nginx框架会根据当前发送状态设置它 */
    off_t                      offset; //见ngx_read_file   ngx_write_chain_to_file   注意和ngx_temp_file_t->offset的区别

    //当前文件系统偏移量，一般不用设置它，同样由Nginx框架设置
    off_t                      sys_offset; //见ngx_read_file   ngx_write_chain_to_file

    ngx_log_t                 *log; //日志对象，相关的日志会输出到log指定的日志文件中

#if (NGX_THREADS)
/*
Ngx_http_copy_filter_module.c (src\http):            ctx->thread_handler = ngx_http_copy_thread_handler;
Ngx_http_file_cache.c (src\http):        c->file.thread_handler = ngx_http_cache_thread_handler;
Ngx_output_chain.c (src\core):        buf->file->thread_handler = ctx->thread_handler;
*/
//客户端请求在ngx_http_file_cache_aio_read读取缓存的时候赋值为c->file.thread_handler = ngx_http_cache_thread_handler;然后在ngx_thread_read中执行
    ngx_int_t                (*thread_handler)(ngx_thread_task_t *task,
                                               ngx_file_t *file);
    void                      *thread_ctx; //客户端请求r
#endif

#if (NGX_HAVE_FILE_AIO)
    ngx_event_aio_t           *aio; //初始赋值见ngx_file_aio_init
#endif

    unsigned                   valid_info:1;//目前未使用

    /*
     //Ngx_http_echo_subrequest.c (src\echo-nginx-module-master\src):        b->file->directio = of.is_directio;
    // Ngx_http_flv_module.c (src\http\modules):    b->file->directio = of.is_directio;
    // Ngx_http_gzip_static_module.c (src\http\modules):    b->file->directio = of.is_directio;
     //Ngx_http_mp4_module.c (src\http\modules):    b->file->directio = of.is_directio;
    // Ngx_http_static_module.c (src\http\modules):    b->file->directio = of.is_directio; 
     of.is_directio只有在文件大小大于directio 512配置的大小时才会置1，见ngx_open_and_stat_file中会置1
     只有配置文件中有配置这几个模块相关配置，并且获取的文件大小(例如缓存文件)大于directio 512，也就是文件大小大于512时，则置1
     */
    unsigned                   directio:1;//一般都为0  注意并不是配置了directio  xxx;就会置1，这个和具体模块有关
};


#define NGX_MAX_PATH_LEVEL  3


typedef time_t (*ngx_path_manager_pt) (void *data);
typedef void (*ngx_path_loader_pt) (void *data);

//参考ngx_conf_set_path_slot和ngx_http_file_cache_set_slot
typedef struct {
    //fastcgi_cache_path /tmp/nginx/fcgi/cache levels=1:2 keys_zone=fcgi:10m inactive=30m max_size=128m;中的levels=1:2中的1:2中的/tmp/nginx/fcgi/cache
    ngx_str_t                  name; //路径名
    //可以参考下ngx_create_hashed_filename
    size_t                     len; //levels=x:y最终的结果是path->len = (x+1) + (y+1)  参考ngx_http_file_cache_set_slot
    
/*
 levels=1:2，意思是说使用两级目录，第一级目录名是一个字符，第二级用两个字符。但是nginx最大支持3级目录，即levels=xxx:xxx:xxx。
 那么构成目录名字的字符哪来的呢？假设我们的存储目录为/cache，levels=1:2，那么对于上面的文件 就是这样存储的：
 /cache/0/8d/8ef9229f02c5672c747dc7a324d658d0  注意后面的8d0和cache后面的/0/8d一致  参考ngx_create_hashed_filename
*/ //fastcgi_cache_path /tmp/nginx/fcgi/cache levels=1:2 keys_zone=fcgi:10m inactive=30m max_size=128m;中的levels=1:2中的1:2
//目录创建见ngx_create_path  
    //一个对应的缓存文件的目录f/27/46492fbf0d9d35d3753c66851e81627f中的46492fbf0d9d35d3753c66851e81627f，注意f/27就是最尾部的字节,这个由levle=1:2，就是最后面的1个字节+2个字节
    size_t                     level[3];  //把levels=x:y;中的x和y分别存储在level[0]和level[1] level[3]始终为0
    //ngx_http_file_cache_set_slot中设置为ngx_http_file_cache_manager
    //一般只有涉及到共享内存分配管理的才有该pt，例如fastcgi_cache_path xxx keys_zone=fcgi:10m xxx 只要有这些配置则会启用cache进程，见ngx_start_cache_manager_processes
    ngx_path_manager_pt        manager; //ngx_cache_manager_process_handler中执行  
    //manger和loader。是cache管理回调函数
    
    //ngx_http_file_cache_set_slot中设置为ngx_http_file_cache_loader   ngx_cache_loader_process_handler中执行
    ngx_path_loader_pt         loader; //决定是否启用cache loader进程  参考ngx_start_cache_manager_processes
    void                      *data; //ngx_http_file_cache_set_slot中设置为ngx_http_file_cache_t

    u_char                    *conf_file; //所在的配置文件 见ngx_http_file_cache_set_slot
    ngx_uint_t                 line; //在配置文件中的行号，见ngx_http_file_cache_set_slot
} ngx_path_t;


typedef struct {
    ngx_str_t                  name;
    size_t                     level[3];
} ngx_path_init_t;

//ngx_http_upstream_send_response中会创建ngx_temp_file_t
typedef struct { //ngx_http_write_request_body中会创建该结构并赋值   临时文件资源回收函数为ngx_pool_run_cleanup_file
    ngx_file_t                 file; //里面包括文件信息，fd 文件名等
    //注意和file->offset的区别(file->offset指的是temp临时文件中的某个具体文件的内容末尾处)，(包括头部行数据+网页包体数据)
    //ngx_temp_file_t->offset也就是temp目录下面所有文件的内容之和，因为一般max_temp_file_size要限制temp中临时文件内容大小，不能无限制的往里面写
    off_t                      offset; //指向写入到文件中的内容的最尾处

/* 非缓存方式(p->cacheable=0)p->temp_file->path = u->conf->temp_path; 由ngx_http_fastcgi_temp_path指定路径
缓存方式(p->cacheable=1) p->temp_file->path = r->cache->file_cache->temp_path;见proxy_cache_path或者fastcgi_cache_path use_temp_path=指定路径  见ngx_http_upstream_send_response 
*/  
    ngx_path_t                *path; //文件路径 p->temp_file->path = u->conf->temp_path;  默认值ngx_http_fastcgi_temp_path
    ngx_pool_t                *pool;
    char                      *warn; //提示信息

    ngx_uint_t                 access; //文件权限 6660等  默认0600，见ngx_create_temp_file->ngx_open_tempfile

    unsigned                   log_level:8;  //日志等级request_body_file_log_level
    //p->cacheable == 1的情况下，ngx_http_upstream_send_response中默认置1
    unsigned                   persistent:1; //文件内容是否永久存储 request_body_in_persistent_file
    //默认会清除，见ngx_create_temp_file  后端缓存临时文件时会删除的，但是缓存请求包体有"clean"开关控制
    unsigned                   clean:1; //文件时临时的，关闭连接会删除文件，ngx_pool_delete_file  request_body_in_clean_file 
} ngx_temp_file_t; //这里面的参数使用见ngx_write_chain_to_temp_file创建临时文件


typedef struct {//参考ngx_http_file_cache_update
    ngx_uint_t                 access;
    ngx_uint_t                 path_access;
    time_t                     time;
    ngx_fd_t                   fd;

    unsigned                   create_path:1;
    unsigned                   delete_file:1;

    ngx_log_t                 *log;
} ngx_ext_rename_file_t;


typedef struct {
    off_t                      size;
    size_t                     buf_size;

    ngx_uint_t                 access;
    time_t                     time;

    ngx_log_t                 *log;
} ngx_copy_file_t;


typedef struct ngx_tree_ctx_s  ngx_tree_ctx_t;

typedef ngx_int_t (*ngx_tree_init_handler_pt) (void *ctx, void *prev);
typedef ngx_int_t (*ngx_tree_handler_pt) (ngx_tree_ctx_t *ctx, ngx_str_t *name);

struct ngx_tree_ctx_s {
    off_t                      size;
    off_t                      fs_size;
    ngx_uint_t                 access;
    time_t                     mtime;

    ngx_tree_init_handler_pt   init_handler;
    ngx_tree_handler_pt        file_handler; //文件节点为普通文件时调用 
    ngx_tree_handler_pt        pre_tree_handler; // 在递归进入目录节点时调用 
    ngx_tree_handler_pt        post_tree_handler; // 在递归遍历完目录节点后调用 
    ngx_tree_handler_pt        spec_handler; //文件节点为特殊文件时调用 

    void                      *data;//指向ngx_http_file_cache_t，见ngx_http_file_cache_loader
    size_t                     alloc;

    ngx_log_t                 *log;
};


ngx_int_t ngx_get_full_name(ngx_pool_t *pool, ngx_str_t *prefix,
    ngx_str_t *name);

ssize_t ngx_write_chain_to_temp_file(ngx_temp_file_t *tf, ngx_chain_t *chain);
ngx_int_t ngx_create_temp_file(ngx_file_t *file, ngx_path_t *path,
    ngx_pool_t *pool, ngx_uint_t persistent, ngx_uint_t clean,
    ngx_uint_t access);
void ngx_create_hashed_filename(ngx_path_t *path, u_char *file, size_t len);
ngx_int_t ngx_create_path(ngx_file_t *file, ngx_path_t *path);
ngx_err_t ngx_create_full_path(u_char *dir, ngx_uint_t access);
ngx_int_t ngx_add_path(ngx_conf_t *cf, ngx_path_t **slot);
ngx_int_t ngx_create_paths(ngx_cycle_t *cycle, ngx_uid_t user);
ngx_int_t ngx_ext_rename_file(ngx_str_t *src, ngx_str_t *to,
    ngx_ext_rename_file_t *ext);
ngx_int_t ngx_copy_file(u_char *from, u_char *to, ngx_copy_file_t *cf);
ngx_int_t ngx_walk_tree(ngx_tree_ctx_t *ctx, ngx_str_t *tree);

ngx_atomic_uint_t ngx_next_temp_number(ngx_uint_t collision);

char *ngx_conf_set_path_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char *ngx_conf_merge_path_value(ngx_conf_t *cf, ngx_path_t **path,
    ngx_path_t *prev, ngx_path_init_t *init);
char *ngx_conf_set_access_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


extern ngx_atomic_t      *ngx_temp_number;
extern ngx_atomic_int_t   ngx_random_number;


#endif /* _NGX_FILE_H_INCLUDED_ */
