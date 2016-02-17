
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
缓冲区ngx_buf_t是Nginx处理大数据的关键数据结构，它既应用于内存数据也应用于磁盘数据
*/
typedef struct ngx_buf_s  ngx_buf_t; //从内存池中分配ngx_buf_t空间，通过ngx_chain_s和pool关联，并初始化其中的各个变量初值函数为ngx_create_temp_buf
/*
ngx_buf_t是一种基本数据结构，本质上它提供的仅仅是一些指针成员和标志位。对于HTTP模块来说，需要注意HTTP框架、事件框架是如何设置
和使用pos、last等指针以及如何处理这些标志位的，上述说明只是最常见的用法。（如果我们自定义一个ngx_buf_t结构体，不应当受限于上
述用法，而应该根据业务需求自行定义。例如用一个ngx_buf_t缓冲区转发上下游TCP流时，pos会指向将要发送到下游的TCP流起
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
    off_t            file_pos;  //可以结合ngx_output_chain_copy_buf阅读更好理解    输出数据的打印可以在ngx_http_write_filter中查看调试信息
    //也就是实际存到临时文件中的字节数,见ngx_event_pipe_write_chain_to_temp_file
    off_t            file_last; //写入文件内容的最尾处的长度赋值见ngx_http_read_client_request_body     输出数据的打印可以在ngx_http_write_filter中查看调试信息

    //如果ngx_buf_t缓冲区用于内存，那么start指向这段内存的起始地址
    u_char          *start;         /* start of buffer */ //创建空间见ngx_http_upstream_process_header
    u_char          *end;           /* end of buffer */ //与start成员对应，指向缓冲区内存的末尾
    //ngx_http_request_body_length_filter中赋值为ngx_http_read_client_request_body
    //实际上是一个void*类型的指针，使用者可以关联任意的对象上去，只要对使用者有意义。
    ngx_buf_tag_t    tag;/*表示当前缓冲区的类型，例如由哪个模块使用就指向这个模块ngx_module_t变量的地址*/
    ngx_file_t      *file;//引用的文件  用于存储接收到所有包体后，把包体内容写入到file文件中，赋值见ngx_http_read_client_request_body
    
 /*当前缓冲区的影子缓冲区，该成员很少用到，仅仅在使用缓冲区转发上游服务器的响应时才使用了shadow成员，这是因为Nginx太节
 约内存了，分配一块内存并使用ngx_buf_t表示接收到的上游服务器响应后，在向下游客户端转发时可能会把这块内存存储到文件中，也
 可能直接向下游发送，此时Nginx绝不会重新复制一份内存用于新的目的，而是再次建立一个ngx_buf_t结构体指向原内存，这样多个
 ngx_buf_t结构体指向了同一块内存，它们之间的关系就通过shadow成员来引用。这种设计过于复杂，通常不建议使用

 当这个buf完整copy了另外一个buf的所有字段的时候，那么这两个buf指向的实际上是同一块内存，或者是同一个文件的同一部分，此
 时这两个buf的shadow字段都是指向对方的。那么对于这样的两个buf，在释放的时候，就需要使用者特别小心，具体是由哪里释放，要
 提前考虑好，如果造成资源的多次释放，可能会造成程序崩溃！
 */
    ngx_buf_t       *shadow; //参考ngx_http_fastcgi_input_filter


    /* the buf's content could be changed */
    //为1时表示该buf所包含的内容是在一个用户创建的内存块中，并且可以被在filter处理的过程中进行变更，而不会造成问题
    unsigned         temporary:1; //临时内存标志位，为1时表示数据在内存中且这段内存可以修改

    /*
     * the buf's content is in a memory cache or in a read only memory
     * and must not be changed
     */ //为1时表示该buf所包含的内容是在内存中，但是这些内容确不能被进行处理的filter进行变更。
    unsigned         memory:1;//标志位，为1时表示数据在内存中且这段内存不可以被修改

    /* the buf's content is mmap()ed and must not be changed */
    //为1时表示该buf所包含的内容是在内存中, 是通过mmap使用内存映射从文件中映射到内存中的，这些内容确不能被进行处理的filter进行变更。
    unsigned         mmap:1;//标志位，为1时表示这段内存是用mmap系统调用映射过来的，不可以被修改

    //可以回收的。也就是这个buf是可以被释放的。这个字段通常是配合shadow字段一起使用的，对于使用ngx_create_temp_buf 函数创建的buf，
    //并且是另外一个buf的shadow，那么可以使用这个字段来标示这个buf是可以被释放的。
    //置1了表示该buf需要马上发送出去，参考ngx_http_write_filter -> if (!last && !flush && in && size < (off_t) clcf->postpone_output) {
    unsigned         recycled:1; //标志位，为1时表示可回收利用，当该buf被新的buf指针指向的时候，就置1，见ngx_http_upstream_send_response

    /*
    ngx_buf_t有一个标志位in_file，将in_file置为1就表示这次ngx_buf_t缓冲区发送的是文件而不是内存。
调用ngx_http_output_filter后，若Nginx检测到in_file为1，将会从ngx_buf_t缓冲区中的file成员处获取实际的文件。file的类型是ngx_file_t
    */ //为1时表示该buf所包含的内容是在文件中。   输出数据的打印可以在ngx_http_write_filter中查看调试信息
    unsigned         in_file:1;//标志位，为1时表示这段缓冲区处理的是文件而不是内存，说明包体全部存入文件中，需要配置"client_body_in_file_only" on | clean 

    //遇到有flush字段被设置为1的的buf的chain，则该chain的数据即便不是最后结束的数据（last_buf被设置，标志所有要输出的内容都完了），
    //也会进行输出，不会受postpone_output配置的限制，但是会受到发送速率等其他条件的限制。
    ////置1了表示该buf需要马上发送出去，参考ngx_http_write_filter -> if (!last && !flush && in && size < (off_t) clcf->postpone_output) {
    unsigned         flush:1;//标志位，为1时表示需要执行flush操作  标示需要立即发送缓冲的所有数据；
    /*标志位，对于操作这块缓冲区时是否使用同步方式，需谨慎考虑，这可能会阻塞Nginx进程，Nginx中所有操作几乎都是异步的，这是
    它支持高并发的关键。有些框架代码在sync为1时可能会有阻塞的方式进行I/O操作，它的意义视使用它的Nginx模块而定*/
    unsigned         sync:1;
    /*标志位，表示是否是最后一块缓冲区，因为ngx_buf_t可以由ngx_chain_t链表串联起来，因此，当last_buf为1时，表示当前是最后一块待处理的缓冲区*/
    /*
    如果接受包体接收完成，则存储最后一个包体内容的buf的last_buf置1，见ngx_http_request_body_length_filter  
    如果发送包体的时候只有头部，这里会置1，见ngx_http_header_filter
    如果各个模块在发送包体内容的时候，如果送入ngx_http_write_filter函数的in参数chain表中的某个buf为该包体的最后一段内容，则该buf中的last_buf会置1
     */ // ngx_http_send_special中会置1  见ngx_http_write_filter -> if (!last && !flush && in && size < (off_t) clcf->postpone_output) {
    //置1了表示该buf需要马上发送出去，参考ngx_http_write_filter -> if (!last && !flush && in && size < (off_t) clcf->postpone_output) {
    unsigned         last_buf:1;  //数据被以多个chain传递给了过滤器，此字段为1表明这是最后一个chain。
    //在当前的chain里面，此buf是最后一个。特别要注意的是last_in_chain的buf不一定是last_buf，但是last_buf的buf一定是last_in_chain的。这是因为数据会被以多个chain传递给某个filter模块。
    unsigned         last_in_chain:1;//标志位，表示是否是ngx_chain_t中的最后一块缓冲区

    //在创建一个buf的shadow的时候，通常将新创建的一个buf的last_shadow置为1。  
    //参考ngx_http_fastcgi_input_filter
    //当数据被发送出去后，该标志位1的ngx_buf_t最终会添加到free_raw_bufs中
    //该值一般都为1，
    unsigned         last_shadow:1; /*标志位，表示是否是最后一个影子缓冲区，与shadow域配合使用。通常不建议使用它*/
    unsigned         temp_file:1;//标志位，表示当前缓冲区是否属于临时文件

    /* STUB */ int   num;  //是为读取后端服务器包体分配的第几个buf ，见ngx_event_pipe_read_upstream  表示属于链表chain中的第几个buf
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
typedef struct {//通过output_buffers命令配置
    ngx_int_t    num; //通过output_buffers命令配置outpu链表的个数
    size_t       size;
} ngx_bufs_t; //proxy_buffers  fastcgi_buffers 4 4K赋值见ngx_event_pipe_read_upstream


typedef struct ngx_output_chain_ctx_s  ngx_output_chain_ctx_t;

typedef ngx_int_t (*ngx_output_chain_filter_pt)(void *ctx, ngx_chain_t *in);

#if (NGX_HAVE_FILE_AIO)
typedef void (*ngx_output_chain_aio_pt)(ngx_output_chain_ctx_t *ctx,
    ngx_file_t *file);
#endif

struct ngx_output_chain_ctx_s {//ngx_http_copy_filter中创建空间和赋值
    /*ngx_output_chain_copy_bufc中tx->in中的内存数据或者缓存文件数据会拷贝到dst中，也就是ctx->buf,然后在ngx_output_chain_copy_buf函数
    外层会重新把ctx->buf赋值给新的chain，然后write出去*/

    /* 保存临时的buf */ //实际buf指向的内存空间在ngx_output_chain_align_file_buf或者ngx_output_chain_get_buf 开辟的
    ngx_buf_t                   *buf;

/*ngx_output_chain_copy_bufc中tx->in中的内存数据或者缓存文件数据会拷贝到dst中，也就是ctx->buf,然后在ngx_output_chain_copy_buf函数
外层会重新把ctx->buf赋值给新的chain，然后write出去*/ //一次最多从in中拷贝65536字节，见ngx_output_chain_copy_buf
    
    /* 保存了将要发送的chain */  //实际in是在ngx_output_chain->ngx_output_chain_add_copy(ctx->pool, &ctx->in, in)让ctx->in是输入参数in的直接拷贝赋值
    //如果是aio thread方式，则在ngx_output_chain_copy_buf->ngx_thread_read->ngx_thread_read_handler中读取到in->buf中
    ngx_chain_t                 *in;//in是待发送的数据，busy是已经调用ngx_chain_writer但还没有发送完毕。
    ngx_chain_t                 *free;/* 保存了已经发送完毕的chain，以便于重复利用 */  
    ngx_chain_t                 *busy; /* 保存了还未发送的chain */  

    /*
       和后端的ngx_connection_t在ngx_event_connect_peer这里置为1，但在ngx_http_upstream_connect中c->sendfile &= r->connection->sendfile;，
       和客户端浏览器的ngx_connextion_t的sendfile需要在ngx_http_update_location_config中判断，因此最终是由是否在configure的时候是否有加
       sendfile选项来决定是置1还是置0
    */
    unsigned                     sendfile:1;/* sendfile标记 */  
//direct实际上有几方面的优势，不使用系统缓存一方面，另一方面是使用dma直接由dma控制从内存输入到用户空间的buffer中不经过cpu做mov操作，不消耗cpu。
//表示在获取空间的时候是否需要ctx->alignment字节对齐，见ngx_output_chain_get_buf
//在ngx_output_chain_copy_buf中会对sendfile有影响  //调值置1的前提是ngx_output_chain_as_is返回0
    /*
         Ngx_http_echo_subrequest.c (src\echo-nginx-module-master\src):        b->file->directio = of.is_directio;
         Ngx_http_flv_module.c (src\http\modules):    b->file->directio = of.is_directio;
         Ngx_http_gzip_static_module.c (src\http\modules):    b->file->directio = of.is_directio;
         Ngx_http_mp4_module.c (src\http\modules):    b->file->directio = of.is_directio;
         Ngx_http_static_module.c (src\http\modules):    b->file->directio = of.is_directio;
        只会在上面这几个模块中才有可能置1，因为of.is_directio只有在文件大小大于directio 512配置的大小时才会置1，见ngx_open_and_stat_file中会置1
        
     */ 
     /* 数据在文件里面，并且程序有走到了 b->file->directio = of.is_directio(并且of.is_directio要为1)这几个模块，
        并且文件大小大于directio xxx中的大小才可能置1，见ngx_output_chain_align_file_buf  ngx_output_chain_as_is */
    unsigned                     directio:1;/* directio标记 */ //如果没有启用direction的情况下，ngx_output_chain_align_file_buf 中置1
#if (NGX_HAVE_ALIGNED_DIRECTIO)
    /* 数据在文件里面，并且程序有走到了 b->file->directio = of.is_directio;这几个模块，
        并且文件大小大于directio xxx中的大小 */
    unsigned                     unaligned:1;//如果没有启用direction的情况下，则在ngx_output_chain_align_file_buf中置1
#endif
/* 是否需要在内存中保存一份(使用sendfile的话，内存中没有文件的拷贝的，而我们有时需要处理文件，此时就需要设置这个标记) */ 
    unsigned                     need_in_memory:1;
    
    /* 是否需要在内存中重新复制一份，不管buf是在内存还是文件, 这样的话，后续模块可以直接修改这块内存 */  
    unsigned                     need_in_temp:1;
#if (NGX_HAVE_FILE_AIO || NGX_THREADS)
/*
ngx_http_copy_aio_handler handler ngx_http_copy_aio_event_handler执行后，会置回到0   
ngx_http_copy_thread_handler ngx_http_copy_thread_event_handler置0
ngx_http_cache_thread_handler置1， ngx_http_cache_thread_event_handler置0
ngx_http_file_cache_aio_read中置1，
*/
    //ngx_http_copy_aio_handler中置1   ngx_http_copy_filter中ctx->aio = ngx_http_request_t->aio
    //生效地方见ngx_output_chain
    unsigned                     aio:1; //配置aio on或者aio thread poll的时候置1
#endif

#if (NGX_HAVE_FILE_AIO)
//ngx_http_copy_filter赋值为ctx->aio_handler = ngx_http_copy_aio_handler;执行在ngx_output_chain_copy_buf
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
    //赋值见ngx_http_upstream_init_request  默认512   在ngx_output_chain_get_buf生效，表示分配空间的时候，空间起始地址需要按照这个值对齐
    off_t                        alignment;//directio_alignment 512;  它与directio配合使用，指定以directio方式读取文件时的对齐方式

    ngx_pool_t                  *pool;
    ngx_int_t                    allocated;//已经分陪的buf个数  ngx_output_chain_get_buf创建一个ngx_buf_t的时候，会自增
    //通过output_buffers命令配置，赋值见ngx_http_copy_filter   默认值output_buffers 1 32768  
    //ngx_http_copy_filter中赋值为ngx_http_copy_filter_conf_t->bufs
    //真正判断生效在ngx_output_chain
    ngx_bufs_t                   bufs;//赋值见ngx_http_upstream_init_request  对应loc conf中设置的bufs
    ngx_buf_tag_t                tag; //标识自己所属的模块，例如参考ngx_http_fastcgi_handler  模块标记，主要用于buf回收

/*
如果是fastcgi_pass，并且不需要缓存客户端包体，则output_filter=ngx_http_fastcgi_body_output_filter
如果是proxy_pass，并且不需要缓存客户端包体,并且internal_chunked ==1，则output_filter=ngx_http_proxy_body_output_filter
其他情况默认在ngx_http_upstream_init_request设置为ngx_chain_writer
*/ //copy_filter模块的outpu_filter=ngx_http_next_body_filter
    ngx_output_chain_filter_pt   output_filter; //ngx_output_chain 赋值见ngx_http_upstream_init_request 一般是ngx_http_next_filter,也就是继续调用filter链
    //一般执行客户端请求r
    void                        *filter_ctx;// 赋值见ngx_http_upstream_init_request /* 当前filter的上下文，这里是由于upstream也会调用output_chain */  
};


typedef struct {
    //ngx_http_upstream_connect中初始化赋值u->writer.last = &u->writer.out; last指针指向out，调用ngx_chain_writer后last指向存储在out中cl的最后一个节点的NEXT处
    ngx_chain_t                 *out;//还没有发送出去的待发送数据的头部
    //ngx_http_upstream_connect中初始化赋值u->writer.last = &u->writer.out; last指针指向out，调用ngx_chain_writer后last指向存储在out中cl的最后一个节点的NEXT处
    ngx_chain_t                **last;//永远指向最优一个ngx_chain_t的next字段的地址。这样可以通过这个地址不断的在后面增加元素。
    ngx_connection_t            *connection; //我这个输出链表对应的连接
    ngx_pool_t                  *pool; //等于request对应的pool，见ngx_http_upstream_init_request
    off_t                        limit;
} ngx_chain_writer_ctx_t;


#define NGX_CHAIN_ERROR     (ngx_chain_t *) NGX_ERROR

//返回这个buf里面的内容是否在内存里。
#define ngx_buf_in_memory(b)        (b->temporary || b->memory || b->mmap)

//返回这个buf里面的内容是否仅仅在内存里，并且没有在文件里。
#define ngx_buf_in_memory_only(b)   (ngx_buf_in_memory(b) && !b->in_file)

//如果是special的buf，应该是从ngx_http_send_special过来的
//返回该buf是否是一个特殊的buf，只含有特殊的标志和没有包含真正的数据。
#define ngx_buf_special(b)                                                   \
    ((b->flush || b->last_buf || b->sync)                                    \
     && !ngx_buf_in_memory(b) && !b->in_file)

//返回该buf是否是一个只包含sync标志而不包含真正数据的特殊buf。
#define ngx_buf_sync_only(b)                                                 \
    (b->sync                                                                 \
     && !ngx_buf_in_memory(b) && !b->in_file && !b->flush && !b->last_buf)

//返回该buf所含数据的大小，不管这个数据是在文件里还是在内存里。
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
