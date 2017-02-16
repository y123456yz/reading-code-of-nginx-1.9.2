
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_PALLOC_H_INCLUDED_
#define _NGX_PALLOC_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * NGX_MAX_ALLOC_FROM_POOL should be (ngx_pagesize - 1), i.e. 4095 on x86.
 * On Windows NT it decreases a number of locked pages in a kernel.
 */
#define NGX_MAX_ALLOC_FROM_POOL  (ngx_pagesize - 1)

#define NGX_DEFAULT_POOL_SIZE    (16 * 1024)

#define NGX_POOL_ALIGNMENT       16
#define NGX_MIN_POOL_SIZE                                                     \
    ngx_align((sizeof(ngx_pool_t) + 2 * sizeof(ngx_pool_large_t)),            \
              NGX_POOL_ALIGNMENT)


typedef void (*ngx_pool_cleanup_pt)(void *data);

typedef struct ngx_pool_cleanup_s  ngx_pool_cleanup_t;

/*
ngx_pool_cleanup_t与ngx_http_cleanup_pt是不同的，ngx_pool_cleanup_t仅在所用的内存池销毁时才会被调用来清理资源，它何时释放资
源将视所使用的内存池而定，而ngx_http_cleanup_pt是在ngx_http_request_t结构体释放时被调用来释放资源的。


如果我们需要添加自己的回调函数，则需要调用ngx_pool_cleanup_add来得到一个ngx_pool_cleanup_t，然后设置handler为我们的清理函数，
并设置data为我们要清理的数据。这样在ngx_destroy_pool中会循环调用handler清理数据；

比如：我们可以将一个开打的文件描述符作为资源挂载到内存池上，同时提供一个关闭文件描述的函数注册到handler上，那么内存池在释放
的时候，就会调用我们提供的关闭文件函数来处理文件描述符资源了
*/
//内存池pool中清理数据的用的，见ngx_pool_s  ngx_destroy_pool
struct ngx_pool_cleanup_s { //这个是添加到ngx_pool_s中的cleanup上的，见ngx_pool_cleanup_add
    ngx_pool_cleanup_pt   handler;// 当前 cleanup 数据的回调函数  ngx_destroy_pool中执行    例如清理文件句柄ngx_pool_cleanup_file等
    void                 *data;// 内存的真正地址     回调时，将此数据传入回调函数；  ngx_pool_cleanup_add中开辟空间
    
    ngx_pool_cleanup_t   *next;// 指向下一块 cleanup 内存的指针
};


typedef struct ngx_pool_large_s  ngx_pool_large_t;

/*
内存池           ---  ngx_pool_s;
内存块数据       ---  ngx_pool_data_t;
大内存块         --- ngx_pool_large_s; 
*///大块内存结构体,链表结构  
struct ngx_pool_large_s { //ngx_pool_s中的大块内存成员
    ngx_pool_large_t     *next;
    void                 *alloc;//申请的内存块地址   
};

/*
内存池           ---  ngx_pool_s;
内存块数据       ---  ngx_pool_data_t;
大内存块         --- ngx_pool_large_s; 
*/ //内存块包含的数据   
typedef struct {
    u_char               *last;//申请过的内存的尾地址,可申请的首地址    pool->d.last ~ pool->d.end 中的内存区便是可用数据区。
    u_char               *end;//当前内存池节点可以申请的内存的最终位置  
    ngx_pool_t           *next;//下一个内存池节点ngx_pool_t,见ngx_palloc_block
    ngx_uint_t            failed;//当前节点申请内存失败的次数,   如果发现从当前pool中分配内存失败四次，则使用下一个pool,见ngx_palloc_block 
} ngx_pool_data_t;

/*
为了减少内存碎片的数量，并通过统一管理来减少代码中出现内存泄漏的可能性，Nginx设计了ngx_pool_t内存池数据结构。
*/
/*
内存池           ---  ngx_pool_s;
内存块数据       ---  ngx_pool_data_t;
大内存块         --- ngx_pool_large_s; 
*/
//内存池数据结构,链表形式存储   图形化理解参考Nginx 内存池（pool）分析 http://www.linuxidc.com/Linux/2011-08/41860.htm
struct ngx_pool_s {
    ngx_pool_data_t       d;//节点数据    // 包含 pool 的数据区指针的结构体 pool->d.last ~ pool->d.end 中的内存区便是可用数据区。
    size_t                max;//当前内存节点可以申请的最大内存空间 // 一次最多从pool中开辟的最大空间
    //每次从pool中分配内存的时候都是从curren开始遍历pool节点获取内存的
    ngx_pool_t           *current;//内存池中可以申请内存的第一个节点      pool 当前正在使用的pool的指针 current 永远指向此pool的开始地址。current的意思是当前的pool地址

/*
pool 中的 chain 指向一个 ngx_chain_t 数据，其值是由宏 ngx_free_chain 进行赋予的，指向之前用完了的，
可以释放的ngx_chain_t数据。由函数ngx_alloc_chain_link进行使用。
*/
    ngx_chain_t          *chain;// pool 当前可用的 ngx_chain_t 数据，注意：由 ngx_free_chain 赋值   ngx_alloc_chain_link
    ngx_pool_large_t     *large;//节点中大内存块指针   // pool 中指向大数据快的指针（大数据快是指 size > max 的数据块）
    ngx_pool_cleanup_t   *cleanup;// pool 中指向 ngx_pool_cleanup_t 数据块的指针 //cleanup在ngx_pool_cleanup_add赋值
    ngx_log_t            *log; // pool 中指向 ngx_log_t 的指针，用于写日志的  ngx_event_accept会赋值
};

typedef struct {//ngx_open_cached_file中创建空间和赋值
    ngx_fd_t              fd;//文件句柄
    u_char               *name; //文件名称
    ngx_log_t            *log;//日志对象
} ngx_pool_cleanup_file_t;


void *ngx_alloc(size_t size, ngx_log_t *log);
void *ngx_calloc(size_t size, ngx_log_t *log);

ngx_pool_t *ngx_create_pool(size_t size, ngx_log_t *log);
void ngx_destroy_pool(ngx_pool_t *pool);
void ngx_reset_pool(ngx_pool_t *pool);

void *ngx_palloc(ngx_pool_t *pool, size_t size);
void *ngx_pnalloc(ngx_pool_t *pool, size_t size);
void *ngx_pcalloc(ngx_pool_t *pool, size_t size);
void *ngx_pmemalign(ngx_pool_t *pool, size_t size, size_t alignment);
ngx_int_t ngx_pfree(ngx_pool_t *pool, void *p);


ngx_pool_cleanup_t *ngx_pool_cleanup_add(ngx_pool_t *p, size_t size);
void ngx_pool_run_cleanup_file(ngx_pool_t *p, ngx_fd_t fd);
void ngx_pool_cleanup_file(void *data);
void ngx_pool_delete_file(void *data);


#endif /* _NGX_PALLOC_H_INCLUDED_ */
