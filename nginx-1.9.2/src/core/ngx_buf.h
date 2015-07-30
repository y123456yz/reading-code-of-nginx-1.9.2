
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_BUF_H_INCLUDED_
#define _NGX_BUF_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef void *            ngx_buf_tag_t;
/*
缓冲区ngx_buf_t是Nginx处理大数据的关键数据结构，它既应用于内存数据也应用于磁盘数据。下面主要介绍ngx_buf_t结构体本身，而描述磁盘
文件的ngx_file_t结构体则在3.8.1节中说明。下面来看一下相关代码：
*/
typedef struct ngx_buf_s  ngx_buf_t; //从内存池中分配ngx_buf_t空间，通过ngx_chain_s和pool关联，并初始化其中的各个变量初值函数为ngx_create_temp_buf
/*
ngx_buf_t是一种基本数据结构，本质上它提供的仅仅是一些指针成员和标志位。对于HTTP模块来说，需要注意HTTP框架、事件框架是如何设置
和使用pos、last等指针以及如何处理这些标志位的，上述说明只是最常见的用法。（如果我们自定义一个ngx_buf_t结构体，不应当受限于上
述用法，而应该根据业务需求自行定义。例如，在13.7节中用一个ngx_buf_t缓冲区转发上下游TCP流时，pos会指向将要发送到下游的TCP流起
始地址，而last会指向预备接收上游TCP流的缓冲区起始地址。）
*/
/*
实际上，Nginx还封装了一个生成ngx_buf_t的简便方法，它完全等价于上面的6行语句，如下所示。
ngx_buf_t *b = ngx_create_temp_buf(r->pool, 128);

分配完内存后，可以向这段内存写入数据。当写完数据后，要让b->last指针指向数据的末尾，如果b->last与b->pos相等，那么HTTP框架是不会发送一个字节的包体的。

最后，把上面的ngx_buf_t *b用ngx_chain_t传给ngx_http_output_filter方法就可以发送HTTP响应的包体内容了。例如：
ngx_chain_t out;
out.buf = b;
out.next = NULL;
return ngx_http_output_filter(r, &out);
*/ //参考http://blog.chinaunix.net/uid-26335251-id-3483044.html
struct ngx_buf_s { //可以参考ngx_create_temp_buf         函数空间在ngx_create_temp_buf创建，让指针指向这些空间
/*pos通常是用来告诉使用者本次应该从pos这个位置开始处理内存中的数据，这样设置是因为同一个ngx_buf_t可能被多次反复处理。
当然，pos的含义是由使用它的模块定义的*/
    //它的pos成员和last成员指向的地址之间的内存就是接收到的还未解析的字符流
    u_char          *pos; //pos指针指向从内存池里分配的内存。 pos为已扫描的内存端中，还未解析的内存的尾部，
    u_char          *last;/*last通常表示有效的内容到此为止，注意，pos与last之间的内存是希望nginx处理的内容*/

    /* 处理文件时，file_pos与file_last的含义与处理内存时的pos与last相同，file_pos表示将要处理的文件位置，file_last表示截止的文件位置 */
    off_t            file_pos;
    off_t            file_last; //写入文件内容的最尾处的长度赋值见ngx_http_read_client_request_body

    //如果ngx_buf_t缓冲区用于内存，那么start指向这段内存的起始地址
    u_char          *start;         /* start of buffer */
    u_char          *end;           /* end of buffer */ //与start成员对应，指向缓冲区内存的末尾
    //ngx_http_request_body_length_filter中赋值为ngx_http_read_client_request_body
    ngx_buf_tag_t    tag;/*表示当前缓冲区的类型，例如由哪个模块使用就指向这个模块ngx_module_t变量的地址*/
    ngx_file_t      *file;//引用的文件  用于存储接收到所有包体后，把包体内容写入到file文件中，赋值见ngx_http_read_client_request_body
    
 /*当前缓冲区的影子缓冲区，该成员很少用到，仅仅在使用缓冲区转发上游服务器的响应时才使用了shadow成员，
 这是因为Nginx太节约内存了，分配一块内存并使用ngx_buf_t表示接收到的上游服务器响应后，在向下游客户端转发时可能会
 把这块内存存储到文件中，也可能直接向下游发送，此时Nginx绝不会重新复制一份内存用于新的目的，而是再次建立一个
 ngx_buf_t结构体指向原内存，这样多个ngx_buf_t结构体指向了同一块内存，它们之间的关系就通过shadow成员来引用。
 这种设计过于复杂，通常不建议使用*/
    ngx_buf_t       *shadow;


    /* the buf's content could be changed */
    unsigned         temporary:1; //临时内存标志位，为1时表示数据在内存中且这段内存可以修改

    /*
     * the buf's content is in a memory cache or in a read only memory
     * and must not be changed
     */
    unsigned         memory:1;//标志位，为1时表示数据在内存中且这段内存不可以被修改

    /* the buf's content is mmap()ed and must not be changed */
    unsigned         mmap:1;//标志位，为1时表示这段内存是用mmap系统调用映射过来的，不可以被修改

    unsigned         recycled:1; //标志位，为1时表示可回收

    /*
    ngx_buf_t有一个标志位in_file，将in_file置为1就表示这次ngx_buf_t缓冲区发送的是文件而不是内存。
调用ngx_http_output_filter后，若Nginx检测到in_file为1，将会从ngx_buf_t缓冲区中的file成员处获取实际的文件。file的类型是ngx_file_t
    */
    unsigned         in_file:1;//标志位，为1时表示这段缓冲区处理的是文件而不是内存，说明包体全部存入文件中，需要配置"client_body_in_file_only" on | clean 
    unsigned         flush:1;//标志位，为1时表示需要执行flush操作  标示需要立即发送缓冲的所有数据；
    /*标志位，对于操作这块缓冲区时是否使用同步方式，需谨慎考虑，这可能会阻塞Nginx进程，Nginx中所有操作几乎都是异步的，这是
    它支持高并发的关键。有些框架代码在sync为1时可能会有阻塞的方式进行I/O操作，它的意义视使用它的Nginx模块而定*/
    unsigned         sync:1;
    /*标志位，表示是否是最后一块缓冲区，因为ngx_buf_t可以由ngx_chain_t链表串联起来，因此，当last_buf为1时，表示当前是最后一块待处理的缓冲区*/
    /*
    如果接受包体接收完成，则存储最后一个包体内容的buf的last_buf置1，见ngx_http_request_body_length_filter  
    如果发送包体的时候只有头部，这里会置1，见ngx_http_header_filter
    如果各个模块在发送包体内容的时候，如果送入ngx_http_write_filter函数的in参数chain表中的某个buf为该包体的最后一段内容，则该buf中的last_buf会置1
     */
    unsigned         last_buf:1; 
    unsigned         last_in_chain:1;//标志位，表示是否是ngx_chain_t中的最后一块缓冲区

    unsigned         last_shadow:1; /*标志位，表示是否是最后一个影子缓冲区，与shadow域配合使用。通常不建议使用它*/
    unsigned         temp_file:1;//标志位，表示当前缓冲区是否属于临时文件

    /* STUB */ int   num;
};

/*
ngx_chain_t是与ngx_buf_t配合使用的链表数据结构，下面看一下它的定义：
typedef struct ngx_chain_s       ngx_chain_t;
struct ngx_chain_s {
    ngx_buf_t    *buf;
    ngx_chain_t  *next;
};

buf指向当前的ngx_buf_t缓冲区，next则用来指向下一个ngx_chain_t。如果这是最后一个ngx_chain_t，则需要把next置为NULL。

在向用户发送HTTP 包体时，就要传入ngx_chain_t链表对象，注意，如果是最后一个ngx_chain_t，那么必须将next置为NULL，
否则永远不会发送成功，而且这个请求将一直不会结束（Nginx框架的要求）。
*/
struct ngx_chain_s {
    ngx_buf_t    *buf;
    ngx_chain_t  *next;
};

//表示num个size空间大小  如 4 8K，表示4个8K空间，可以参考ngx_conf_set_bufs_slot
typedef struct {
    ngx_int_t    num;
    size_t       size;
} ngx_bufs_t;


typedef struct ngx_output_chain_ctx_s  ngx_output_chain_ctx_t;

typedef ngx_int_t (*ngx_output_chain_filter_pt)(void *ctx, ngx_chain_t *in);

#if (NGX_HAVE_FILE_AIO)
typedef void (*ngx_output_chain_aio_pt)(ngx_output_chain_ctx_t *ctx,
    ngx_file_t *file);
#endif

struct ngx_output_chain_ctx_s {
    ngx_buf_t                   *buf;
    ngx_chain_t                 *in;
    ngx_chain_t                 *free;
    ngx_chain_t                 *busy;

    unsigned                     sendfile:1;
    unsigned                     directio:1;
#if (NGX_HAVE_ALIGNED_DIRECTIO)
    unsigned                     unaligned:1;
#endif
    unsigned                     need_in_memory:1;
    unsigned                     need_in_temp:1;
#if (NGX_HAVE_FILE_AIO || NGX_THREADS)
    unsigned                     aio:1;
#endif

#if (NGX_HAVE_FILE_AIO)
    ngx_output_chain_aio_pt      aio_handler;
#if (NGX_HAVE_AIO_SENDFILE)
    ssize_t                    (*aio_preload)(ngx_buf_t *file);
#endif
#endif

#if (NGX_THREADS)
    ngx_int_t                  (*thread_handler)(ngx_thread_task_t *task,
                                                 ngx_file_t *file);
    ngx_thread_task_t           *thread_task;
#endif
    //赋值见ngx_http_upstream_init_request
    off_t                        alignment;//directio_alignment 512;  它与directio配合使用，指定以directio方式读取文件时的对齐方式

    ngx_pool_t                  *pool;
    ngx_int_t                    allocated;
    ngx_bufs_t                   bufs;
    ngx_buf_tag_t                tag; //标识自己所属的模块，例如参考ngx_http_fastcgi_handler

    ngx_output_chain_filter_pt   output_filter; //ngx_output_chain中执行
    void                        *filter_ctx;
};


typedef struct {
    ngx_chain_t                 *out;
    ngx_chain_t                **last;
    ngx_connection_t            *connection;
    ngx_pool_t                  *pool; //等于request对应的pool，见ngx_http_upstream_init_request
    off_t                        limit;
} ngx_chain_writer_ctx_t;


#define NGX_CHAIN_ERROR     (ngx_chain_t *) NGX_ERROR


#define ngx_buf_in_memory(b)        (b->temporary || b->memory || b->mmap)
#define ngx_buf_in_memory_only(b)   (ngx_buf_in_memory(b) && !b->in_file)

#define ngx_buf_special(b)                                                   \
    ((b->flush || b->last_buf || b->sync)                                    \
     && !ngx_buf_in_memory(b) && !b->in_file)

#define ngx_buf_sync_only(b)                                                 \
    (b->sync                                                                 \
     && !ngx_buf_in_memory(b) && !b->in_file && !b->flush && !b->last_buf)

#define ngx_buf_size(b)                                                      \
    (ngx_buf_in_memory(b) ? (off_t) (b->last - b->pos):                      \
                            (b->file_last - b->file_pos))

ngx_buf_t *ngx_create_temp_buf(ngx_pool_t *pool, size_t size);
ngx_chain_t *ngx_create_chain_of_bufs(ngx_pool_t *pool, ngx_bufs_t *bufs);


#define ngx_alloc_buf(pool)  ngx_palloc(pool, sizeof(ngx_buf_t))
#define ngx_calloc_buf(pool) ngx_pcalloc(pool, sizeof(ngx_buf_t))

ngx_chain_t *ngx_alloc_chain_link(ngx_pool_t *pool);

/*
pool 中的 chain 指向一个 ngx_chain_t 数据，其值是由宏 ngx_free_chain 进行赋予的，指向之前用完了的，
可以释放的ngx_chain_t数据。由函数ngx_alloc_chain_link进行使用。
*/

#define ngx_free_chain(pool, cl)                                             \
    cl->next = pool->chain;                                                  \
    pool->chain = cl



ngx_int_t ngx_output_chain(ngx_output_chain_ctx_t *ctx, ngx_chain_t *in);
ngx_int_t ngx_chain_writer(void *ctx, ngx_chain_t *in);

ngx_int_t ngx_chain_add_copy(ngx_pool_t *pool, ngx_chain_t **chain,
    ngx_chain_t *in);
ngx_chain_t *ngx_chain_get_free_buf(ngx_pool_t *p, ngx_chain_t **free);
void ngx_chain_update_chains(ngx_pool_t *p, ngx_chain_t **free,
    ngx_chain_t **busy, ngx_chain_t **out, ngx_buf_tag_t tag);

off_t ngx_chain_coalesce_file(ngx_chain_t **in, off_t limit);

ngx_chain_t *ngx_chain_update_sent(ngx_chain_t *in, off_t sent);

#endif /* _NGX_BUF_H_INCLUDED_ */
