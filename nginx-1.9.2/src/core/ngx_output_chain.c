
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


#if 0
#define NGX_SENDFILE_LIMIT  4096
#endif

/*
 * When DIRECTIO is enabled FreeBSD, Solaris, and MacOSX read directly
 * to an application memory from a device if parameters are aligned
 * to device sector boundary (512 bytes).  They fallback to usual read
 * operation if the parameters are not aligned.
 * Linux allows DIRECTIO only if the parameters are aligned to a filesystem
 * sector boundary, otherwise it returns EINVAL.  The sector size is
 * usually 512 bytes, however, on XFS it may be 4096 bytes.
 */

#define NGX_NONE            1


static ngx_inline ngx_int_t
    ngx_output_chain_as_is(ngx_output_chain_ctx_t *ctx, ngx_buf_t *buf);
#if (NGX_HAVE_AIO_SENDFILE)
static ngx_int_t ngx_output_chain_aio_setup(ngx_output_chain_ctx_t *ctx,
    ngx_file_t *file);
#endif
static ngx_int_t ngx_output_chain_add_copy(ngx_pool_t *pool,
    ngx_chain_t **chain, ngx_chain_t *in);
static ngx_int_t ngx_output_chain_align_file_buf(ngx_output_chain_ctx_t *ctx,
    off_t bsize);
static ngx_int_t ngx_output_chain_get_buf(ngx_output_chain_ctx_t *ctx,
    off_t bsize);
static ngx_int_t ngx_output_chain_copy_buf(ngx_output_chain_ctx_t *ctx);

/*
函数目的是发送 in 中的数据，ctx 用来保存发送的上下文，因为发送通常情况下，不能一次完成。nginx 因为使用了 ET 模式，
在网络编程事件管理上简单了，但是编程中处理事件复杂了，需要不停的循环做处理；事件的函数回掉，次数也不确定，因此需
要使用 context 上下文对象来保存发送到什么环节了。
*/
//结合ngx_http_xxx_create_request(ngx_http_fastcgi_create_request)阅读，ctx->in中的数据实际上是从ngx_http_xxx_create_request组成ngx_chain_t来的，数据来源在ngx_http_xxx_create_request
ngx_int_t //向后端发送请求的调用过程ngx_http_upstream_send_request_body->ngx_output_chain->ngx_chain_writer



/*
如果是aio on | thread_pool方式，则会两次执行该函数，并且所有参数几乎一样，只是aio标记取值会变化，日志如下:
2016/01/07 18:47:27[ ngx_event_pipe_write_to_downstream,   604]  [debug] 20923#20923: *1 pipe write downstream, write ready: 1
2016/01/07 18:47:27[ ngx_event_pipe_write_to_downstream,   649]  [debug] 20923#20923: *1 pipe write downstream flush out
2016/01/07 18:47:27[             ngx_http_output_filter,  3377]  [debug] 20923#20923: *1 http output filter "/test2.php?"
2016/01/07 18:47:27[               ngx_http_copy_filter,   206]  [debug] 20923#20923: *1 http copy filter: "/test2.php?", r->aio:0
2016/01/07 18:47:27[                   ngx_output_chain,    67][yangya  [debug] 20923#20923: *1 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2016/01/07 18:47:27[             ngx_output_chain_as_is,   309][yangya  [debug] 20923#20923: ngx_output_chain_as_is--- buf_special:0, in_file:1, directio:0, buf_in_mem:0,need_in_memory:0, need_in_temp:0, memory:0, mmap:0
2016/01/07 18:47:27[             ngx_output_chain_as_is,   309][yangya  [debug] 20923#20923: ngx_output_chain_as_is--- buf_special:0, in_file:1, directio:0, buf_in_mem:0,need_in_memory:0, need_in_temp:0, memory:0, mmap:0

注意第一次走ngx_thread_read，打印信息和第一次完全一样
2016/01/07 18:47:27[                    ngx_thread_read,   147]  [debug] 20923#20923: *1 thread read: fd:14, buf:08115A90, size:1220, offset:206
2016/01/07 18:47:27[              ngx_thread_mutex_lock,   145]  [debug] 20923#20923: pthread_mutex_lock(080F0458) enter
2016/01/07 18:47:27[               ngx_thread_task_post,   280][yangya  [debug] 20923#20923: ngx add task to thread, task id:158
2016/01/07 18:47:27[             ngx_thread_cond_signal,    54]  [debug] 20923#20923: pthread_cond_signal(080F047C)
2016/01/07 18:47:27[               ngx_thread_cond_wait,    96]  [debug] 20923#20928: pthread_cond_wait(080F047C) exit
2016/01/07 18:47:27[            ngx_thread_mutex_unlock,   171]  [debug] 20923#20928: pthread_mutex_unlock(080F0458) exit
2016/01/07 18:47:27[              ngx_thread_pool_cycle,   370]  [debug] 20923#20928: run task #158 in thread pool name:"yang_pool"
2016/01/07 18:47:27[            ngx_thread_read_handler,   201]  [debug] 20923#20928: thread read handler
2016/01/07 18:47:27[            ngx_thread_read_handler,   219]  [debug] 20923#20928: pread: 1220 (err: 0) of 1220 @206
2016/01/07 18:47:27[              ngx_thread_pool_cycle,   376]  [debug] 20923#20928: complete task #158 in thread pool name: "yang_pool"
2016/01/07 18:47:27[              ngx_thread_mutex_lock,   145]  [debug] 20923#20928: pthread_mutex_lock(080F0458) enter
2016/01/07 18:47:27[               ngx_thread_cond_wait,    70]  [debug] 20923#20928: pthread_cond_wait(080F047C) enter
2016/01/07 18:47:27[            ngx_thread_mutex_unlock,   171]  [debug] 20923#20923: pthread_mutex_unlock(080F0458) exit
2016/01/07 18:47:27[               ngx_thread_task_post,   297]  [debug] 20923#20923: task #158 added to thread pool name: "yang_pool" complete
2016/01/07 18:47:27[               ngx_http_copy_filter,   284]  [debug] 20923#20923: *1 http copy filter rc: -2, buffered:4 "/test2.php?"
2016/01/07 18:47:27[ ngx_event_pipe_write_to_downstream,   688]  [debug] 20923#20923: *1 pipe write downstream done
2016/01/07 18:47:27[                ngx_event_del_timer,    39]  [debug] 20923#20923: *1 <           ngx_event_pipe,    91>  event timer del: 12: 464761188
2016/01/07 18:47:27[  ngx_http_upstream_process_request,  4233][yangya  [debug] 20923#20923: *1 ngx http cache, p->length:-1, u->headers_in.content_length_n:-1, tf->offset:1426, r->cache->body_start:206
2016/01/07 18:47:27[         ngx_http_file_cache_update,  1557]  [debug] 20923#20923: *1 http file cache update, c->body_start:206
2016/01/07 18:47:27[         ngx_http_file_cache_update,  1570]  [debug] 20923#20923: *1 http file cache rename: "/var/yyz/cache_xxx/temp/2/00/0000000002" to "/var/yyz/cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f", expire time:1800
2016/01/07 18:47:27[                     ngx_shmtx_lock,   168]  [debug] 20923#20923: shmtx lock
2016/01/07 18:47:27[                   ngx_shmtx_unlock,   249]  [debug] 20923#20923: shmtx unlock
2016/01/07 18:47:27[  ngx_http_upstream_process_request,  4270]  [debug] 20923#20923: *1 http upstream exit: 00000000
2016/01/07 18:47:27[ ngx_http_upstream_finalize_request,  4541]  [debug] 20923#20923: *1 finalize http upstream request rc: 0
2016/01/07 18:47:27[  ngx_http_fastcgi_finalize_request,  3215]  [debug] 20923#20923: *1 finalize http fastcgi request
2016/01/07 18:47:27[ngx_http_upstream_free_round_robin_peer,   887]  [debug] 20923#20923: *1 free rr peer 1 0
2016/01/07 18:47:27[ ngx_http_upstream_finalize_request,  4594]  [debug] 20923#20923: *1 close http upstream connection: 12
2016/01/07 18:47:27[               ngx_close_connection,  1120]  [debug] 20923#20923: *1 delete posted event AEA6B098
2016/01/07 18:47:27[            ngx_reusable_connection,  1177]  [debug] 20923#20923: *1 reusable connection: 0
2016/01/07 18:47:27[               ngx_close_connection,  1139][yangya  [debug] 20923#20923: close socket:12
2016/01/07 18:47:27[ ngx_http_upstream_finalize_request,  4608]  [debug] 20923#20923: *1 http upstream temp fd: 14
2016/01/07 18:47:27[              ngx_http_send_special,  3871][yangya  [debug] 20923#20923: *1 ngx http send special, flags:1, last_buf:1, sync:0, last_in_chain:0, flush:0
2016/01/07 18:47:27[             ngx_http_output_filter,  3377]  [debug] 20923#20923: *1 http output filter "/test2.php?"
2016/01/07 18:47:27[               ngx_http_copy_filter,   206]  [debug] 20923#20923: *1 http copy filter: "/test2.php?", r->aio:1
2016/01/07 18:47:27[                   ngx_output_chain,    67][yangya  [debug] 20923#20923: *1 ctx->sendfile:0, ctx->aio:1, ctx->directio:0
2016/01/07 18:47:27[                   ngx_output_chain,   117][yangya  [debug] 20923#20923: *1 ctx->aio = 1, wait kernel complete read
2016/01/07 18:47:27[               ngx_http_copy_filter,   284]  [debug] 20923#20923: *1 http copy filter rc: -2, buffered:4 "/test2.php?"
2016/01/07 18:47:27[          ngx_http_finalize_request,  2598]  [debug] 20923#20923: *1 http finalize request rc: -2, "/test2.php?" a:1, c:1
2016/01/07 18:47:27[                ngx_event_add_timer,   100]  [debug] 20923#20923: *1 <ngx_http_set_write_handler,  3029>  event timer add fd:13, expire-time:60 s, timer.key:464761210
2016/01/07 18:47:27[           ngx_trylock_accept_mutex,   405]  [debug] 20923#20923: accept mutex locked
2016/01/07 18:47:27[           ngx_epoll_process_events,  1725]  [debug] 20923#20923: epoll: fd:9 EPOLLIN  (ev:0001) d:080E36C0
2016/01/07 18:47:27[           ngx_epoll_process_events,  1771]  [debug] 20923#20923: post event 080E3680
2016/01/07 18:47:27[           ngx_event_process_posted,    65]  [debug] 20923#20923: begin to run befor posted event 080E3680
2016/01/07 18:47:27[           ngx_event_process_posted,    67]  [debug] 20923#20923: delete posted event 080E3680
2016/01/07 18:47:27[            ngx_thread_pool_handler,   401]  [debug] 20923#20923: thread pool handler
2016/01/07 18:47:27[            ngx_thread_pool_handler,   422]  [debug] 20923#20923: run completion handler for task #158
2016/01/07 18:47:27[ ngx_http_copy_thread_event_handler,   429][yangya  [debug] 20923#20923: *1 ngx http aio thread event handler
2016/01/07 18:47:27[           ngx_http_request_handler,  2407]  [debug] 20923#20923: *1 http run request(ev->write:1): "/test2.php?"
2016/01/07 18:47:27[                    ngx_http_writer,  3058]  [debug] 20923#20923: *1 http writer handler: "/test2.php?"
2016/01/07 18:47:27[             ngx_http_output_filter,  3377]  [debug] 20923#20923: *1 http output filter "/test2.php?"
2016/01/07 18:47:27[               ngx_http_copy_filter,   206]  [debug] 20923#20923: *1 http copy filter: "/test2.php?", r->aio:0
2016/01/07 18:47:27[                   ngx_output_chain,    67][yangya  [debug] 20923#20923: *1 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2016/01/07 18:47:27[             ngx_output_chain_as_is,   309][yangya  [debug] 20923#20923: ngx_output_chain_as_is--- buf_special:0, in_file:1, directio:0, buf_in_mem:0,need_in_memory:0, need_in_temp:0, memory:0, mmap:0

注意第二次走ngx_thread_read，打印信息和第一次完全一样
2016/01/07 18:47:27[                    ngx_thread_read,   147]  [debug] 20923#20923: *1 thread read: fd:14, buf:08115A90, size:1220, offset:206

2016/01/07 18:47:27[             ngx_output_chain_as_is,   314][yangya  [debug] 20923#20923: ngx_output_chain_as_is--- buf_special:1, in_file:0, buf_in_mem:0,need_in_memory:0, need_in_temp:0, memory:0, mmap:0
2016/01/07 18:47:27[           ngx_http_postpone_filter,   176]  [debug] 20923#20923: *1 http postpone filter "/test2.php?" 080F3E94
2016/01/07 18:47:27[       ngx_http_chunked_body_filter,   212]  [debug] 20923#20923: *1 http chunk: 1220
2016/01/07 18:47:27[       ngx_http_chunked_body_filter,   212]  [debug] 20923#20923: *1 http chunk: 0
2016/01/07 18:47:27[       ngx_http_chunked_body_filter,   273]  [debug] 20923#20923: *1 yang test ..........xxxxxxxx ################## lstbuf:1
2016/01/07 18:47:27[              ngx_http_write_filter,   151]  [debug] 20923#20923: *1 write old buf t:1 f:0 080F3B60, pos 080F3B60, size: 180 file: 0, size: 0
2016/01/07 18:47:27[              ngx_http_write_filter,   208]  [debug] 20923#20923: *1 write new buf temporary:1 buf-in-file:0, buf->start:080F3EE0, buf->pos:080F3EE0, buf_size: 5 file_pos: 0, in_file_size: 0
2016/01/07 18:47:27[              ngx_http_write_filter,   208]  [debug] 20923#20923: *1 write new buf temporary:1 buf-in-file:0, buf->start:08115A90, buf->pos:08115A90, buf_size: 1220 file_pos: 0, in_file_size: 0
2016/01/07 18:47:27[              ngx_http_write_filter,   208]  [debug] 20923#20923: *1 write new buf temporary:0 buf-in-file:0, buf->start:00000000, buf->pos:080CF058, buf_size: 7 file_pos: 0, in_file_size: 0
2016/01/07 18:47:27[              ngx_http_write_filter,   248]  [debug] 20923#20923: *1 http write filter: last:1 flush:1 size:1412
2016/01/07 18:47:27[              ngx_http_write_filter,   380]  [debug] 20923#20923: *1 http write filter limit 0
2016/01/07 18:47:27[           ngx_linux_sendfile_chain,   201][yangya  [debug] 20923#20923: *1 @@@@@@@@@@@@@@@@@@@@@@@begin ngx_linux_sendfile_chain @@@@@@@@@@@@@@@@@@@
2016/01/07 18:47:27[                         ngx_writev,   238]  [debug] 20923#20923: *1 writev: 1412 of 1412
2016/01/07 18:47:27[              ngx_http_write_filter,   386]  [debug] 20923#20923: *1 http write filter 00000000
2016/01/07 18:47:27[               ngx_http_copy_filter,   284]  [debug] 20923#20923: *1 http copy filter rc: 0, buffered:0 "/test2.php?"
2016/01/07 18:47:27[                    ngx_http_writer,  3124]  [debug] 20923#20923: *1 http writer output filter: 0, "/test2.php?"
2016/01/07 18:47:27[                    ngx_http_writer,  3156]  [debug] 20923#20923: *1 http writer done: "/test2.php?"
2016/01/07 18:47:27[          ngx_http_finalize_request,  2598]  [debug] 20923#20923: *1 http finalize request rc: 0, "/test2.php?" a:1, c:1
*/
//如果是aio on | thread_pool方式，则会两次执行该函数，并且所有参数机会一样，参考上面日志。大文件下载和下文件获取过程机会一样，只是
//在ngx_http_writer后面有判断是否写完成，通过r->buffered是否为0来区分



/* 注意:到这里的in实际上是已经指向数据内容部分，或者如果发送的数据需要从文件中读取，in中也会指定文件file_pos和file_last已经文件fd等,
   可以参考ngx_http_cache_send ngx_http_send_header ngx_http_output_filter */
ngx_output_chain(ngx_output_chain_ctx_t *ctx, ngx_chain_t *in)  //in为需要发送的chain链，上面存储的是实际要发送的数据
{//ctx为&u->output， in为u->request_bufs这里nginx filter的主要逻辑都在这个函数里面,将in参数链表的缓冲块拷贝到
//ctx->in,然后将ctx->in的数据拷贝到out,然后调用output_filter发送出去。

//如果读取后端数据发往客户端，默认流程是//ngx_event_pipe->ngx_event_pipe_write_to_downstream->p->output_filter(p->output_ctx, p->out);走到这里
    off_t         bsize;
    ngx_int_t     rc, last;
    ngx_chain_t  *cl, *out, **last_out;

    ngx_uint_t sendfile = ctx->sendfile;
    ngx_uint_t aio = ctx->aio;
    ngx_uint_t directio = ctx->directio;
    
    ngx_log_debugall(ctx->pool->log, 0, "ctx->sendfile:%ui, ctx->aio:%ui, ctx->directio:%ui", sendfile, aio, directio);
    if (ctx->in == NULL && ctx->busy == NULL
#if (NGX_HAVE_FILE_AIO || NGX_THREADS)
        && !ctx->aio
#endif
       ) //in是待发送的数据，busy是已经调用ngx_chain_writer但还没有发送完毕。
    {
        /*
         * the short path for the case when the ctx->in and ctx->busy chains
         * are empty, the incoming chain is empty too or has the single buf
         * that does not require the copy
         */

        if (in == NULL) { //如果要发送的数据为空，也就是啥也不用发送。那就直接调用output_filter的了。
            ngx_log_debugall(ctx->pool->log, 0, "ngx output chain, in = NULL");
            return ctx->output_filter(ctx->filter_ctx, in);
        }

        if (in->next == NULL //说明发送buf只有一个
#if (NGX_SENDFILE_LIMIT)
            && !(in->buf->in_file && in->buf->file_last > NGX_SENDFILE_LIMIT)
#endif
            && ngx_output_chain_as_is(ctx, in->buf)) //这个函数主要用来判断是否需要复制buf。返回1,表示不需要拷贝，否则为需要拷贝 
        {
            ngx_log_debugall(ctx->pool->log, 0, "only one chain buf to output_filter");
            return ctx->output_filter(ctx->filter_ctx, in);
        }
    }

    /* add the incoming buf to the chain ctx->in */
   
    if (in) {//拷贝一份数据到ctx->in里面，需要老老实实的进行数据拷贝了。将in参数里面的数据拷贝到ctx->in里面。换了个in
        if (ngx_output_chain_add_copy(ctx->pool, &ctx->in, in) == NGX_ERROR) {
            return NGX_ERROR;
        }
    }

    /* out为最终需要传输的chain，也就是交给剩下的filter处理的chain */  
    out = NULL;
    last_out = &out; //下面遍历ctx->in链中的数据并且添加到该last_out中，也就是添加到out链中
    last = NGX_NONE;
	//到现在了，in参数的缓冲链表已经放在了ctx->in里面了。下面准备发送吧。

    for ( ;; ) { //循环读取缓存中或者内存中的数据发送

#if (NGX_HAVE_FILE_AIO || NGX_THREADS)
//实际上在接受完后端数据后，在想客户端发送包体部分的时候，会两次调用该函数，一次是ngx_event_pipe_write_to_downstream-> p->output_filter(),
//另一次是ngx_http_upstream_finalize_request->ngx_http_send_special,

//如果是aio(aio on | aio thread_pool)方式，则第一次该值为0，但是第二次从ngx_http_send_special走到这里的时候已经在ngx_output_chain->ngx_file_aio_read->ngx_http_copy_aio_handler置1

//开始读取数据的时候置1，一定要等到aio on模式情况下的内核异步读完成或者aio thread_pool模式下的线程读数据完成，并且通过notify_epoll通知触发了新epoll_in的时候重新置0，表示数据读取完毕，只有条件满足才能发送数据write
        if (ctx->aio) { //如果是aio，则由内核完成READ，read成功后会epoll触发返回，执行在ngx_file_aio_event_handler
            ngx_log_debugall(ctx->pool->log, 0, "ctx->aio = 1, wait AIO kernel complete read or wait thread pool to read complete");
            return NGX_AGAIN;
        }
#endif
        //结合ngx_http_xxx_create_request(ngx_http_fastcgi_create_request)阅读，ctx->in中的数据实际上是从ngx_http_xxx_create_request组成ngx_chain_t来的，数据来源在ngx_http_xxx_create_request
        while (ctx->in) {//遍历所有待发送的数据。将他们一个个拷贝到out指向的链表中

            /*
             * cycle while there are the ctx->in bufs
             * and there are the free output bufs to copy in
             */

            bsize = ngx_buf_size(ctx->in->buf);
            //这块内存大小为0，然后又不是special 可能有问题。 如果是special的buf，应该是从ngx_http_send_special过来的
            if (bsize == 0 && !ngx_buf_special(ctx->in->buf)) {

                ngx_log_error(NGX_LOG_ALERT, ctx->pool->log, 0,
                              "zero size buf in output "
                              "t:%d r:%d f:%d %p %p-%p %p %O-%O",
                              ctx->in->buf->temporary,
                              ctx->in->buf->recycled,
                              ctx->in->buf->in_file,
                              ctx->in->buf->start,
                              ctx->in->buf->pos,
                              ctx->in->buf->last,
                              ctx->in->buf->file,
                              ctx->in->buf->file_pos,
                              ctx->in->buf->file_last);

                ngx_debug_point();

                ctx->in = ctx->in->next;

                continue;
            }
            /* 判断是否需要复制buf */    
            if (ngx_output_chain_as_is(ctx, ctx->in->buf)) {
                //把ctx->in->buf从ctx->in上面取下来，然后加入到lst_out链表中
                /* move the chain link to the output chain */
                /* 如果不需要复制，则直接链接chain到out，然后继续循环 */ 
                cl = ctx->in;
                ctx->in = cl->next; //已经赋值的会从ctx->in上面摘掉

                *last_out = cl;
                last_out = &cl->next;
                cl->next = NULL;

                continue;
            }


//注意从后端接收的数据到缓存文件中后，在filter模块中，有可能是新的buf数据指针了，因为ngx_http_copy_filter->ngx_output_chain中会重新分配内存读取缓存文件内容

            //如果是需要赋值buf(一般都是sendfile的时候)，用户空间内存里面没有数据，所以需要开辟空间来把文件中的内容赋值一份出来
            
            /* 到达这里，说明我们需要拷贝buf，这里buf最终都会被拷贝进ctx->buf中， 因此这里先判断ctx->buf是否为空 */ 
            if (ctx->buf == NULL) { //每次拷贝数据前，先给ctx->buf分配空间，在下面的ngx_output_chain_get_buf函数中

                /* 如果为空，则取得buf，这里要注意，一般来说如果没有开启directio的话，这个函数都会返回NGX_DECLINED */  
                rc = ngx_output_chain_align_file_buf(ctx, bsize);

                if (rc == NGX_ERROR) {
                    return NGX_ERROR;
                }

                if (rc != NGX_OK) {

                    if (ctx->free) {

                        /* get the free buf */

                        cl = ctx->free;
                        /* 得到free buf */    
                        ctx->buf = cl->buf;
                        ctx->free = cl->next;
                        /* 将要重用的chain链接到ctx->poll中，以便于chain的重用 */  
                        ngx_free_chain(ctx->pool, cl);

                    } else if (out || ctx->allocated == ctx->bufs.num) {//output_buffers 1 32768都用完了
                   /* 
                        如果已经等于buf的个数限制，则跳出循环，发送已经存在的buf。 这里可以看到如果out存在的话，nginx会跳出循环，然后发送out，
                        等发送完会再次处理，这里很好的体现了nginx的流式处理 
                        */ 
                        break;

                    } else if (ngx_output_chain_get_buf(ctx, bsize) != NGX_OK) {/* 上面这个函数也比较关键，它用来取得buf。接下来会详细看这个函数 */
                        //该函数获取到的内存保存到ctx->buf中
                        return NGX_ERROR;
                    }
                }
            }
            
            /* 从原来的buf中拷贝内容或者从原来的文件中读取内容 */  //注意如果是aio on或者aio thread=poll方式返回的是NGX_AGAIN
            rc = ngx_output_chain_copy_buf(ctx); //把ctx->in->buf中的内容赋值给ctx->buf
//ngx_output_chain_copy_bufc中tx->in中的内存数据或者缓存文件数据会拷贝到dst中，也就是ctx->buf,然后在ngx_output_chain_copy_buf函数
//外层会重新把ctx->buf赋值给新的chain，然后write出去 ,见下面的创建新chain

            if (rc == NGX_ERROR) {
                return rc;
            }

            if (rc == NGX_AGAIN) { 
            //AIO是异步方式，由内核自行发送出去，应用层不用管，读取文件中数据完毕后epoll会触发执行ngx_file_aio_event_handler中执行ngx_http_copy_aio_event_handler,表示内核已经读取完毕
                if (out) {
                    break;
                }

                return rc;
            }

            /* delete the completed buf from the ctx->in chain */

            if (ngx_buf_size(ctx->in->buf) == 0) {//这个节点大小为0，移动到下一个节点。
                ctx->in = ctx->in->next;
            }

            cl = ngx_alloc_chain_link(ctx->pool);
            if (cl == NULL) {
                return NGX_ERROR;
            }
            //把ngx_output_chain_copy_buf中从原src拷贝的内容赋值给cl->buf，然后添加到lst_out的头部  也就是添加到out后面
            cl->buf = ctx->buf;
            cl->next = NULL;
            *last_out = cl;
            last_out = &cl->next;
            ctx->buf = NULL;

            //注意这里没有continue;直接往后走
        }

        if (out == NULL && last != NGX_NONE) {

            if (ctx->in) {
                return NGX_AGAIN;
            }

            return last;
        }

        last = ctx->output_filter(ctx->filter_ctx, out); //ngx_chain_writer

        if (last == NGX_ERROR || last == NGX_DONE) {
            return last;
        }

        ngx_chain_update_chains(ctx->pool, &ctx->free, &ctx->busy, &out,
                                ctx->tag);
        last_out = &out;
    }
}

/*
该函数返回1，则表示数据可以直接发送出去；如果返回0，则表示数据还在磁盘文件内，需要利用directio读取或明确要求不能使用sendfile直接发送、
明确要求读到内存缓存等情况；注意：buf->file->directio由of.is_directio与配置项directio最终关联起来

    函数ngx_output_chain_as_is()返回1的情况就不管了，原本该干嘛干嘛，走ngx_http_write_filter() -> ngx_linux_sendfile_chain()流程到最后，
内存数据通过writev()发送，磁盘文件内数据通过sendfile()发送。 
    而返回0的情况表示要读取数据到缓存区，在我们这里的讨论上下文，也就是利用aio进行读取，也就是流程： 
ngx_output_chain_copy_buf() -> ngx_file_aio_read() 
*/
static ngx_inline ngx_int_t
ngx_output_chain_as_is(ngx_output_chain_ctx_t *ctx, ngx_buf_t *buf)//ngx_output_chain_as_is是aio还是sendfile的分支点
{//看看这个节点是否可以拷贝。检测content是否在文件中。判断是否需要复制buf.
//返回1表示上层不需要拷贝buf,否则需要重新alloc一个节点，拷贝实际内存到另外一个节点。

/* 如果返回0，表示需要在ngx_output_chain从新开辟空间，如果之前是in_file的，则会从新读取缓存文件内容到内存中，就变为内存型chain buf了 */

/*
    一般开启sendfile on的时候返回1,因为ngx_output_chain_as_is返回1，不会重新开辟内存空间读取缓存内容。然后通过
ngx_linux_sendfile_chain中的ngx_linux_sendfile直接通过sendfile发送出去，而无需读取缓存文件内容到内存，然后从内存中发送。

    一般不开启缓存功能的时候，该函数也会返回1，这时候后端数据不会缓存到文件，而是接收到内存中然后发送，在ngx_linux_sendfile_chain中
直接调用ngx_writev发送，而不是sendfile发送

    如果返回0，则会在ngx_http_copy_filter->ngx_output_chain->ngx_output_chain_align_file_buf开辟新内存，然后从ngx_output_chain_copy_buf中从新
读取缓存文件内容到新的内存中，然后把新内存数据发送出去，在ngx_linux_sendfile_chain中直接调用ngx_writev发送，而不是sendfile发送
 */

    ngx_uint_t  sendfile;

    unsigned int buf_special = ngx_buf_special(buf);
    unsigned int in_file = buf->in_file;
    unsigned int buf_in_mem = ngx_buf_in_memory(buf);
    unsigned int need_in_memory = ctx->need_in_memory;
    unsigned int need_in_temp = ctx->need_in_temp;
    unsigned int memory = buf->memory;
    unsigned int mmap = buf->mmap;
   if (buf->in_file) {
       unsigned int directio = buf->file->directio;
       ngx_log_debugall(ngx_cycle->log, 0, 
        "ngx_output_chain_as_is--- buf_special:%ui, in_file:%ui, directio:%ui, buf_in_mem:%ui,"
            "need_in_memory:%ui, need_in_temp:%ui, memory:%ui, mmap:%ui", 
            buf_special, in_file, directio, buf_in_mem, need_in_memory, need_in_temp, memory, mmap);
    } else {
        ngx_log_debugall(ngx_cycle->log, 0, 
        "ngx_output_chain_as_is--- buf_special:%ui, in_file:%ui, buf_in_mem:%ui,"
            "need_in_memory:%ui, need_in_temp:%ui, memory:%ui, mmap:%ui", 
            buf_special, in_file, buf_in_mem, need_in_memory, need_in_temp, memory, mmap);
    }
    
    if (ngx_buf_special(buf)) { 
    //说明buf中没有实际数据  例如进程在发送数据到后端后，都会调用一个ngx_http_send_special来触发ngx_http_write_filter立刻把数据发送出去
    //这时候ngx_http_send_special就是填充的一个空buf
        return 1;
    }

#if (NGX_THREADS)
    if (buf->in_file) {
        //ngx_http_copy_filter中赋值为ngx_http_copy_thread_handler
        buf->file->thread_handler = ctx->thread_handler;
        buf->file->thread_ctx = ctx->filter_ctx;
    }
#endif

    /*
     Ngx_http_echo_subrequest.c (src\echo-nginx-module-master\src):        b->file->directio = of.is_directio;
     Ngx_http_flv_module.c (src\http\modules):    b->file->directio = of.is_directio;
     Ngx_http_gzip_static_module.c (src\http\modules):    b->file->directio = of.is_directio;
     Ngx_http_mp4_module.c (src\http\modules):    b->file->directio = of.is_directio;
     Ngx_http_static_module.c (src\http\modules):    b->file->directio = of.is_directio;
    只会在上面这几个模块中置1，从这里返回，也就是说如果有配置这几个模块命令，在同时配置sendfile on; aio on;directio xxx；的前提下，
    会从这里返回出去，然后从新获取空间

    但是如果不是上面的这些模块，在同时配置sendfile on; aio on;directio xxx;的情况下还是会返回1，也就是还是采用sendfile方式
     */
    if (buf->in_file && buf->file->directio) {  
        return 0;//如果buf在文件中，使用了directio，需要拷贝buf
    }

    sendfile = ctx->sendfile;

#if (NGX_SENDFILE_LIMIT)

    if (buf->in_file && buf->file_pos >= NGX_SENDFILE_LIMIT) { //文件中内容超过了sendfile的最大上限,则只有重新读取文件到内存中发送
        sendfile = 0;
    }

#endif

    if (!sendfile) {

        if (!ngx_buf_in_memory(buf)) { //一般不启用sendfile on的时候从这里退出去  如果启用了aio on，没有启用sendfile on的情况下，也会从这里出去
        //不启用sendfile(要么未配置sendfile，要么配置了sendfile，但是文件太大，超过sendfile上限)，并且buf在文件中，返回0，需要重新获取文件内容
            return 0;
        }

        buf->in_file = 0; 
    }

#if (NGX_HAVE_AIO_SENDFILE)
    if (ctx->aio_preload && buf->in_file) {
        (void) ngx_output_chain_aio_setup(ctx, buf->file);
    }
#endif
    /* (使用sendfile的话，内存中没有文件的拷贝的，而我们有时需要处理文件，因此需要拷贝文件内容*/
    if (ctx->need_in_memory && !ngx_buf_in_memory(buf)) {
        return 0;
    }

    if (ctx->need_in_temp && (buf->memory || buf->mmap)) {
        return 0;
    }

    //开启sendfile on一般从这里出去，如果不开启缓存，也就是不缓存数据到文件，一般也会从这里返回
    return 1;
}


#if (NGX_HAVE_AIO_SENDFILE)

static ngx_int_t
ngx_output_chain_aio_setup(ngx_output_chain_ctx_t *ctx, ngx_file_t *file)
{
    ngx_event_aio_t  *aio;

    if (file->aio == NULL && ngx_file_aio_init(file, ctx->pool) != NGX_OK) {
        return NGX_ERROR;
    }

    aio = file->aio;

    aio->data = ctx->filter_ctx;
    aio->preload_handler = ctx->aio_preload;

    return NGX_OK;
}

#endif

//从新获取一个ngx_chain_t结构，该结构的buf指向in->buf，让后把这个新的ngx_chain_t添加到chain链表末尾
static ngx_int_t
ngx_output_chain_add_copy(ngx_pool_t *pool, ngx_chain_t **chain,
    ngx_chain_t *in)
{//ngx_output_chain调用这里，将u->request_bufs也就是参数 in的数据拷贝到chain里面。
//参数为:(ctx->pool, &ctx->in, in)。in代表要发送的，也就是输入的缓冲区链表。
    ngx_chain_t  *cl, **ll;
#if (NGX_SENDFILE_LIMIT)
    ngx_buf_t    *b, *buf;
#endif

    ll = chain;

    for (cl = *chain; cl; cl = cl->next) {
        ll = &cl->next; //ll指向chain的末尾
    }

    while (in) {

        cl = ngx_alloc_chain_link(pool);
        if (cl == NULL) {
            return NGX_ERROR;
        }

#if (NGX_SENDFILE_LIMIT)

        buf = in->buf;

        if (buf->in_file
            && buf->file_pos < NGX_SENDFILE_LIMIT
            && buf->file_last > NGX_SENDFILE_LIMIT)
        {//如果缓冲buffer在文件中，并且文件没有超过限制，那就考吧它，但是，如果这个文件超过了limit，那肿么办，拆分成2快buffer。
            /* split a file buf on two bufs by the sendfile limit */

            b = ngx_calloc_buf(pool);
            if (b == NULL) {
                return NGX_ERROR;
            }

            ngx_memcpy(b, buf, sizeof(ngx_buf_t));

            if (ngx_buf_in_memory(buf)) {
                buf->pos += (ssize_t) (NGX_SENDFILE_LIMIT - buf->file_pos);
                b->last = buf->pos;
            }

            buf->file_pos = NGX_SENDFILE_LIMIT;
            b->file_last = NGX_SENDFILE_LIMIT;

            cl->buf = b;

        } else {
            cl->buf = buf;
            in = in->next;
        }

#else
        cl->buf = in->buf;
        in = in->next;

#endif

        cl->next = NULL;
        *ll = cl;
        ll = &cl->next;
    }

    return NGX_OK;
}

//只有ngx_output_chain_align_file_buf不分配内存直接返回后才会在ngx_output_chain_get_buf分配满足条件ngx_buf_in_memory的内存空间
static ngx_int_t
ngx_output_chain_align_file_buf(ngx_output_chain_ctx_t *ctx, off_t bsize)
{ //调用该函数的前提是需要重新分配空间，ngx_output_chain_as_is返回0
    size_t      size;
    ngx_buf_t  *in;

    in = ctx->in->buf;

    /*
     //Ngx_http_echo_subrequest.c (src\echo-nginx-module-master\src):        b->file->directio = of.is_directio;
     //Ngx_http_flv_module.c (src\http\modules):    b->file->directio = of.is_directio;
     //Ngx_http_gzip_static_module.c (src\http\modules):    b->file->directio = of.is_directio;
     //Ngx_http_mp4_module.c (src\http\modules):    b->file->directio = of.is_directio;
     //Ngx_http_static_module.c (src\http\modules):    b->file->directio = of.is_directio;
     如果数据不在文件中而在内存中，或者没有在配置文件中配置上面这几个模块配置信息，或者获取的文件大小小于directio配置的大小，则直接返回
     */
    if (in->file == NULL || !in->file->directio) {
    //如果没有启用direction,则直接返回，实际空间在该函数外层ngx_output_chain_get_buf中创建
        return NGX_DECLINED;
    }


    /* 数据在文件里面，并且程序有走到了 b->file->directio = of.is_directio;这几个模块，
        并且文件大小大于directio xxx中的大小 */
    ctx->directio = 1;

    size = (size_t) (in->file_pos - (in->file_pos & ~(ctx->alignment - 1)));

    if (size == 0) {

        if (bsize >= (off_t) ctx->bufs.size) {
            return NGX_DECLINED;
        }

        size = (size_t) bsize;

    } else {
        size = (size_t) ctx->alignment - size;

        if ((off_t) size > bsize) {
            size = (size_t) bsize;
        }
    }

    ctx->buf = ngx_create_temp_buf(ctx->pool, size);
    if (ctx->buf == NULL) {
        return NGX_ERROR;
    }
    //注意后面没有指明这段内存属于内存空间，ngx_output_chain_copy_buf->ngx_buf_in_memory不会满足条件
    
    /*
     * we do not set ctx->buf->tag, because we do not want
     * to reuse the buf via ctx->free list
     */

#if (NGX_HAVE_ALIGNED_DIRECTIO)
    ctx->unaligned = 1;
#endif

    return NGX_OK;
}

//只有ngx_output_chain_align_file_buf不分配内存直接返回后才会在ngx_output_chain_get_buf分配满足条件ngx_buf_in_memory的内存空间

//获取bsize字节的空间
static ngx_int_t
ngx_output_chain_get_buf(ngx_output_chain_ctx_t *ctx, off_t bsize)
{ /* 下面开辟的是满足条件ngx_buf_in_memory的内存空间 */ //调用该函数的前提是需要重新分配空间，ngx_output_chain_as_is返回0
    size_t       size;
    ngx_buf_t   *b, *in;
    ngx_uint_t   recycled;

    in = ctx->in->buf;
    size = ctx->bufs.size;
    recycled = 1;

    if (in->last_in_chain) {

        if (bsize < (off_t) size) {

            /*
             * allocate a small temp buf for a small last buf
             * or its small last part
             */

            size = (size_t) bsize;
            recycled = 0;

        } else if (!ctx->directio
                   && ctx->bufs.num == 1
                   && (bsize < (off_t) (size + size / 4)))
        {
            /*
             * allocate a temp buf that equals to a last buf,
             * if there is no directio, the last buf size is lesser
             * than 1.25 of bufs.size and the temp buf is single
             */

            size = (size_t) bsize;
            recycled = 0;
        }
    }

    b = ngx_calloc_buf(ctx->pool);
    if (b == NULL) {
        return NGX_ERROR;
    }

    if (ctx->directio) {//在该函数外层的前面ngx_output_chain_align_file_buf会置directio为1

        /*
         * allocate block aligned to a disk sector size to enable
         * userland buffer direct usage conjunctly with directio
         */

        b->start = ngx_pmemalign(ctx->pool, size, (size_t) ctx->alignment);
        if (b->start == NULL) {
            return NGX_ERROR;
        }

    } else {
        b->start = ngx_palloc(ctx->pool, size);
        if (b->start == NULL) {
            return NGX_ERROR;
        }
    }

    b->pos = b->start;
    b->last = b->start;
    b->end = b->last + size;
    b->temporary = 1;
    b->tag = ctx->tag;
    b->recycled = recycled;

    ctx->buf = b;//该函数获取到的内存保存到ctx->buf中
    ctx->allocated++;

    return NGX_OK;
}

/*
如果是aio on | thread_pool方式，则会两次执行该函数，并且所有参数几乎一样，只是aio标记取值会变化，日志如下:
2016/01/07 18:47:27[ ngx_event_pipe_write_to_downstream,   604]  [debug] 20923#20923: *1 pipe write downstream, write ready: 1
2016/01/07 18:47:27[ ngx_event_pipe_write_to_downstream,   649]  [debug] 20923#20923: *1 pipe write downstream flush out
2016/01/07 18:47:27[             ngx_http_output_filter,  3377]  [debug] 20923#20923: *1 http output filter "/test2.php?"
2016/01/07 18:47:27[               ngx_http_copy_filter,   206]  [debug] 20923#20923: *1 http copy filter: "/test2.php?", r->aio:0
2016/01/07 18:47:27[                   ngx_output_chain,    67][yangya  [debug] 20923#20923: *1 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2016/01/07 18:47:27[             ngx_output_chain_as_is,   309][yangya  [debug] 20923#20923: ngx_output_chain_as_is--- buf_special:0, in_file:1, directio:0, buf_in_mem:0,need_in_memory:0, need_in_temp:0, memory:0, mmap:0
2016/01/07 18:47:27[             ngx_output_chain_as_is,   309][yangya  [debug] 20923#20923: ngx_output_chain_as_is--- buf_special:0, in_file:1, directio:0, buf_in_mem:0,need_in_memory:0, need_in_temp:0, memory:0, mmap:0

注意第一次走ngx_thread_read，打印信息和第一次完全一样
2016/01/07 18:47:27[                    ngx_thread_read,   147]  [debug] 20923#20923: *1 thread read: fd:14, buf:08115A90, size:1220, offset:206
2016/01/07 18:47:27[              ngx_thread_mutex_lock,   145]  [debug] 20923#20923: pthread_mutex_lock(080F0458) enter
2016/01/07 18:47:27[               ngx_thread_task_post,   280][yangya  [debug] 20923#20923: ngx add task to thread, task id:158
2016/01/07 18:47:27[             ngx_thread_cond_signal,    54]  [debug] 20923#20923: pthread_cond_signal(080F047C)
2016/01/07 18:47:27[               ngx_thread_cond_wait,    96]  [debug] 20923#20928: pthread_cond_wait(080F047C) exit
2016/01/07 18:47:27[            ngx_thread_mutex_unlock,   171]  [debug] 20923#20928: pthread_mutex_unlock(080F0458) exit
2016/01/07 18:47:27[              ngx_thread_pool_cycle,   370]  [debug] 20923#20928: run task #158 in thread pool name:"yang_pool"
2016/01/07 18:47:27[            ngx_thread_read_handler,   201]  [debug] 20923#20928: thread read handler
2016/01/07 18:47:27[            ngx_thread_read_handler,   219]  [debug] 20923#20928: pread: 1220 (err: 0) of 1220 @206
2016/01/07 18:47:27[              ngx_thread_pool_cycle,   376]  [debug] 20923#20928: complete task #158 in thread pool name: "yang_pool"
2016/01/07 18:47:27[              ngx_thread_mutex_lock,   145]  [debug] 20923#20928: pthread_mutex_lock(080F0458) enter
2016/01/07 18:47:27[               ngx_thread_cond_wait,    70]  [debug] 20923#20928: pthread_cond_wait(080F047C) enter
2016/01/07 18:47:27[            ngx_thread_mutex_unlock,   171]  [debug] 20923#20923: pthread_mutex_unlock(080F0458) exit
2016/01/07 18:47:27[               ngx_thread_task_post,   297]  [debug] 20923#20923: task #158 added to thread pool name: "yang_pool" complete
2016/01/07 18:47:27[               ngx_http_copy_filter,   284]  [debug] 20923#20923: *1 http copy filter rc: -2, buffered:4 "/test2.php?"
2016/01/07 18:47:27[ ngx_event_pipe_write_to_downstream,   688]  [debug] 20923#20923: *1 pipe write downstream done
2016/01/07 18:47:27[                ngx_event_del_timer,    39]  [debug] 20923#20923: *1 <           ngx_event_pipe,    91>  event timer del: 12: 464761188
2016/01/07 18:47:27[  ngx_http_upstream_process_request,  4233][yangya  [debug] 20923#20923: *1 ngx http cache, p->length:-1, u->headers_in.content_length_n:-1, tf->offset:1426, r->cache->body_start:206
2016/01/07 18:47:27[         ngx_http_file_cache_update,  1557]  [debug] 20923#20923: *1 http file cache update, c->body_start:206
2016/01/07 18:47:27[         ngx_http_file_cache_update,  1570]  [debug] 20923#20923: *1 http file cache rename: "/var/yyz/cache_xxx/temp/2/00/0000000002" to "/var/yyz/cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f", expire time:1800
2016/01/07 18:47:27[                     ngx_shmtx_lock,   168]  [debug] 20923#20923: shmtx lock
2016/01/07 18:47:27[                   ngx_shmtx_unlock,   249]  [debug] 20923#20923: shmtx unlock
2016/01/07 18:47:27[  ngx_http_upstream_process_request,  4270]  [debug] 20923#20923: *1 http upstream exit: 00000000
2016/01/07 18:47:27[ ngx_http_upstream_finalize_request,  4541]  [debug] 20923#20923: *1 finalize http upstream request rc: 0
2016/01/07 18:47:27[  ngx_http_fastcgi_finalize_request,  3215]  [debug] 20923#20923: *1 finalize http fastcgi request
2016/01/07 18:47:27[ngx_http_upstream_free_round_robin_peer,   887]  [debug] 20923#20923: *1 free rr peer 1 0
2016/01/07 18:47:27[ ngx_http_upstream_finalize_request,  4594]  [debug] 20923#20923: *1 close http upstream connection: 12
2016/01/07 18:47:27[               ngx_close_connection,  1120]  [debug] 20923#20923: *1 delete posted event AEA6B098
2016/01/07 18:47:27[            ngx_reusable_connection,  1177]  [debug] 20923#20923: *1 reusable connection: 0
2016/01/07 18:47:27[               ngx_close_connection,  1139][yangya  [debug] 20923#20923: close socket:12
2016/01/07 18:47:27[ ngx_http_upstream_finalize_request,  4608]  [debug] 20923#20923: *1 http upstream temp fd: 14
2016/01/07 18:47:27[              ngx_http_send_special,  3871][yangya  [debug] 20923#20923: *1 ngx http send special, flags:1, last_buf:1, sync:0, last_in_chain:0, flush:0
2016/01/07 18:47:27[             ngx_http_output_filter,  3377]  [debug] 20923#20923: *1 http output filter "/test2.php?"
2016/01/07 18:47:27[               ngx_http_copy_filter,   206]  [debug] 20923#20923: *1 http copy filter: "/test2.php?", r->aio:1
2016/01/07 18:47:27[                   ngx_output_chain,    67][yangya  [debug] 20923#20923: *1 ctx->sendfile:0, ctx->aio:1, ctx->directio:0
2016/01/07 18:47:27[                   ngx_output_chain,   117][yangya  [debug] 20923#20923: *1 ctx->aio = 1, wait kernel complete read
2016/01/07 18:47:27[               ngx_http_copy_filter,   284]  [debug] 20923#20923: *1 http copy filter rc: -2, buffered:4 "/test2.php?"
2016/01/07 18:47:27[          ngx_http_finalize_request,  2598]  [debug] 20923#20923: *1 http finalize request rc: -2, "/test2.php?" a:1, c:1
2016/01/07 18:47:27[                ngx_event_add_timer,   100]  [debug] 20923#20923: *1 <ngx_http_set_write_handler,  3029>  event timer add fd:13, expire-time:60 s, timer.key:464761210
2016/01/07 18:47:27[           ngx_trylock_accept_mutex,   405]  [debug] 20923#20923: accept mutex locked
2016/01/07 18:47:27[           ngx_epoll_process_events,  1725]  [debug] 20923#20923: epoll: fd:9 EPOLLIN  (ev:0001) d:080E36C0
2016/01/07 18:47:27[           ngx_epoll_process_events,  1771]  [debug] 20923#20923: post event 080E3680
2016/01/07 18:47:27[           ngx_event_process_posted,    65]  [debug] 20923#20923: begin to run befor posted event 080E3680
2016/01/07 18:47:27[           ngx_event_process_posted,    67]  [debug] 20923#20923: delete posted event 080E3680
2016/01/07 18:47:27[            ngx_thread_pool_handler,   401]  [debug] 20923#20923: thread pool handler
2016/01/07 18:47:27[            ngx_thread_pool_handler,   422]  [debug] 20923#20923: run completion handler for task #158
2016/01/07 18:47:27[ ngx_http_copy_thread_event_handler,   429][yangya  [debug] 20923#20923: *1 ngx http aio thread event handler
2016/01/07 18:47:27[           ngx_http_request_handler,  2407]  [debug] 20923#20923: *1 http run request(ev->write:1): "/test2.php?"
2016/01/07 18:47:27[                    ngx_http_writer,  3058]  [debug] 20923#20923: *1 http writer handler: "/test2.php?"
2016/01/07 18:47:27[             ngx_http_output_filter,  3377]  [debug] 20923#20923: *1 http output filter "/test2.php?"
2016/01/07 18:47:27[               ngx_http_copy_filter,   206]  [debug] 20923#20923: *1 http copy filter: "/test2.php?", r->aio:0
2016/01/07 18:47:27[                   ngx_output_chain,    67][yangya  [debug] 20923#20923: *1 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2016/01/07 18:47:27[             ngx_output_chain_as_is,   309][yangya  [debug] 20923#20923: ngx_output_chain_as_is--- buf_special:0, in_file:1, directio:0, buf_in_mem:0,need_in_memory:0, need_in_temp:0, memory:0, mmap:0

注意第二次走ngx_thread_read，打印信息和第一次完全一样
2016/01/07 18:47:27[                    ngx_thread_read,   147]  [debug] 20923#20923: *1 thread read: fd:14, buf:08115A90, size:1220, offset:206

2016/01/07 18:47:27[             ngx_output_chain_as_is,   314][yangya  [debug] 20923#20923: ngx_output_chain_as_is--- buf_special:1, in_file:0, buf_in_mem:0,need_in_memory:0, need_in_temp:0, memory:0, mmap:0
2016/01/07 18:47:27[           ngx_http_postpone_filter,   176]  [debug] 20923#20923: *1 http postpone filter "/test2.php?" 080F3E94
2016/01/07 18:47:27[       ngx_http_chunked_body_filter,   212]  [debug] 20923#20923: *1 http chunk: 1220
2016/01/07 18:47:27[       ngx_http_chunked_body_filter,   212]  [debug] 20923#20923: *1 http chunk: 0
2016/01/07 18:47:27[       ngx_http_chunked_body_filter,   273]  [debug] 20923#20923: *1 yang test ..........xxxxxxxx ################## lstbuf:1
2016/01/07 18:47:27[              ngx_http_write_filter,   151]  [debug] 20923#20923: *1 write old buf t:1 f:0 080F3B60, pos 080F3B60, size: 180 file: 0, size: 0
2016/01/07 18:47:27[              ngx_http_write_filter,   208]  [debug] 20923#20923: *1 write new buf temporary:1 buf-in-file:0, buf->start:080F3EE0, buf->pos:080F3EE0, buf_size: 5 file_pos: 0, in_file_size: 0
2016/01/07 18:47:27[              ngx_http_write_filter,   208]  [debug] 20923#20923: *1 write new buf temporary:1 buf-in-file:0, buf->start:08115A90, buf->pos:08115A90, buf_size: 1220 file_pos: 0, in_file_size: 0
2016/01/07 18:47:27[              ngx_http_write_filter,   208]  [debug] 20923#20923: *1 write new buf temporary:0 buf-in-file:0, buf->start:00000000, buf->pos:080CF058, buf_size: 7 file_pos: 0, in_file_size: 0
2016/01/07 18:47:27[              ngx_http_write_filter,   248]  [debug] 20923#20923: *1 http write filter: last:1 flush:1 size:1412
2016/01/07 18:47:27[              ngx_http_write_filter,   380]  [debug] 20923#20923: *1 http write filter limit 0
2016/01/07 18:47:27[           ngx_linux_sendfile_chain,   201][yangya  [debug] 20923#20923: *1 @@@@@@@@@@@@@@@@@@@@@@@begin ngx_linux_sendfile_chain @@@@@@@@@@@@@@@@@@@
2016/01/07 18:47:27[                         ngx_writev,   238]  [debug] 20923#20923: *1 writev: 1412 of 1412
2016/01/07 18:47:27[              ngx_http_write_filter,   386]  [debug] 20923#20923: *1 http write filter 00000000
2016/01/07 18:47:27[               ngx_http_copy_filter,   284]  [debug] 20923#20923: *1 http copy filter rc: 0, buffered:0 "/test2.php?"
2016/01/07 18:47:27[                    ngx_http_writer,  3124]  [debug] 20923#20923: *1 http writer output filter: 0, "/test2.php?"
2016/01/07 18:47:27[                    ngx_http_writer,  3156]  [debug] 20923#20923: *1 http writer done: "/test2.php?"
2016/01/07 18:47:27[          ngx_http_finalize_request,  2598]  [debug] 20923#20923: *1 http finalize request rc: 0, "/test2.php?" a:1, c:1
*/

//如果是aio on | thread_pool方式，则会两次执行该函数，并且所有参数机会一样，参考上面日志
//ngx_output_chain_as_is  ngx_output_chain_copy_buf是aio和sendfile和普通文件读写的分支点
static ngx_int_t //注意如果是aio on或者aio thread=poll方式返回的是NGX_AGAIN
ngx_output_chain_copy_buf(ngx_output_chain_ctx_t *ctx)
{//将ctx->in->buf的缓冲拷贝到ctx->buf上面去。  注意是从新分配了数据空间，用来存储原来的in->buf中的数据，实际上现在就有两份相同的数据了(buf指向相同的内存空间)
    off_t        size;
    ssize_t      n;
    ngx_buf_t   *src, *dst;
    ngx_uint_t   sendfile;

    src = ctx->in->buf;//结合ngx_http_xxx_create_request(ngx_http_fastcgi_create_request)阅读，ctx->in中的数据实际上是从ngx_http_xxx_create_request组成ngx_chain_t来的，数据来源在ngx_http_xxx_create_request
//ctx->in中的内存数据或者缓存文件数据会拷贝到dst中，也就是ngx_output_chain_ctx_t->buf,然后在ngx_output_chain_copy_buf函数外层会重新把ctx->buf赋值给新的chain，然后write出去
    dst = ctx->buf; 
    
    /* 例如这里如果从缓存文件中读取，或者nginx本地服务器文件很大，则dst空间最大32768字节，见ngx_output_chain_get_buf，也即是控制了一次最多从文件中获取32768字节 */
    size = ngx_buf_size(src); //如果buf指向的是文件，则是文件中的内容，否则是内存buf中的内容
    size = ngx_min(size, dst->end - dst->pos); //避免dst空间不够，装不了src中的数据大小

//注意:一般缓存中的文件通过sendfile发送的时候，一般在ngx_output_chain_as_is返回1，表示无需新开辟空间，因此不会走到该函数中来，除非ngx_output_chain_as_is中need_in_memory置1的情况
    sendfile = ctx->sendfile & !ctx->directio;//是否采用sendfile  也就是说如果同时配置了sendfile和aio xxx;directio xxx并且ctx->directio为1,则默认关闭sendfile

#if (NGX_SENDFILE_LIMIT)

    if (src->in_file && src->file_pos >= NGX_SENDFILE_LIMIT) {//说明文件内容超限了，不能使用sendfile
        sendfile = 0;
    }

#endif
    //该函数外层只有ngx_output_chain_align_file_buf不分配内存直接返回后才会在ngx_output_chain_get_buf分配满足条件ngx_buf_in_memory的内存空间
    if (ngx_buf_in_memory(src)) {
    /* (数据在内存中)，或者(数据在文件里面，并且b->file->directio = 0;或者文件大小小于directio xxx中的大小) */
        ngx_memcpy(dst->pos, src->pos, (size_t) size); 
        //这里的size为什么能保证不越界，是因为开辟内存的时候，是在ngx_output_chain_get_buf的时候bsize就等于bsize = ngx_buf_size(ctx->in->buf);
        src->pos += (size_t) size;
        dst->last += (size_t) size; //注意dst->pose并没有移动

        if (src->in_file) { //????????????? 这部分有点没弄明白 sendfile有点晕，后面分析
        //size的数据要么存在于文件中，要么就在内存中。前面的size = ngx_buf_size(src);页可以看出来

            if (sendfile) {
            //在同时开启sendfile on; aio on以及direction xxx；的前提下，如果有模块执行过b->file->directio = 1(of.is_directio);
            //但是文件大小小于direction的配置，则还是使用sendfile
            
                dst->in_file = 1;
                dst->file = src->file;

                //源文件中存储的内容指向
                dst->file_pos = src->file_pos;
                dst->file_last = src->file_pos + size;

            } else {
                dst->in_file = 0;
            }

            src->file_pos += size;

        } else {
            dst->in_file = 0;
        }

        if (src->pos == src->last) {  
            dst->flush = src->flush;
            dst->last_buf = src->last_buf;
            dst->last_in_chain = src->last_in_chain;
        }

    } else {//否则，文件不再内存里面，需要从磁盘读取。

#if (NGX_HAVE_ALIGNED_DIRECTIO)

        if (ctx->unaligned) {
            if (ngx_directio_off(src->file->fd) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_ALERT, ctx->pool->log, ngx_errno,
                              ngx_directio_off_n " \"%s\" failed",
                              src->file->name.data);
            }
        }

#endif
//aio on | off | threads[=pool];
#if (NGX_HAVE_FILE_AIO)
        if (ctx->aio_handler) {// aio on的情况下  ngx_output_chain_copy_buf  ngx_file_aio_read
            n = ngx_file_aio_read(src->file, dst->pos, (size_t) size,
                                  src->file_pos, ctx->pool);
            if (n == NGX_AGAIN) {//第一次到这里表示开始通知内核通过AIO异步读取数据，正常情况下回返回NGX_AGAIN
            //AIO是异步方式，由内核自行发送出去，应用层不用管，发送完毕后会执行ngx_file_aio_event_handler中执行ngx_http_copy_aio_event_handler,表示内核自动发送完毕
                ctx->aio_handler(ctx, src->file); //该函数外层ngx_http_copy_filter赋值为ctx->aio_handler = ngx_http_copy_aio_handler;
                return NGX_AGAIN;
            }
            
            //如果通过notify_epoll通知AIO on方式内核读取数据完成，则从这里返回线程池中线程读取任务的字节数
        } else
#endif //aio on | off | threads[=pool];
#if (NGX_THREADS)
        if (src->file->thread_handler) {//aio thread=poll的情况
            n = ngx_thread_read(&ctx->thread_task, src->file, dst->pos,
                                (size_t) size, src->file_pos, ctx->pool);
            if (n == NGX_AGAIN) {//第一次到这里表示开始通知线程池读取数据，正常情况下回返回NGX_AGAIN
                ctx->aio = 1;
                return NGX_AGAIN;
            }

            //如果通过notify_epoll通知线程池中的线程处理读任务完成，则从这里返回线程池中线程读取任务的字节数
        } else
#endif
        { //为配置aio和sendfile的情况直接从这里读取缓存文件
            n = ngx_read_file(src->file, dst->pos, (size_t) size,
                              src->file_pos); //从src->file文件的src->file_pos处读取size字节到dst->pos指向的内存空间
        }

#if (NGX_HAVE_ALIGNED_DIRECTIO)
        /* 数据在文件里面，并且程序有走到了 b->file->directio = of.is_directio;这几个模块，
        并且文件大小大于directio xxx中的大小 */
        if (ctx->unaligned) {
            ngx_err_t  err;

            err = ngx_errno;
            
            if (ngx_directio_on(src->file->fd) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_ALERT, ctx->pool->log, ngx_errno,
                              ngx_directio_on_n " \"%s\" failed",
                              src->file->name.data);
            }

            ngx_set_errno(err);

            ctx->unaligned = 0;
        }

#endif

        if (n == NGX_ERROR) {
            return (ngx_int_t) n;
        }

        if (n != size) {
            ngx_log_error(NGX_LOG_ALERT, ctx->pool->log, 0,
                          ngx_read_file_n " read only %z of %O from \"%s\"",
                          n, size, src->file->name.data);
            return NGX_ERROR;
        }

        /* 把读取到的数据放入dst同时指出这写数据已经从文件中读取存到内存中 */
        
        dst->last += n; //pos的lst指针后移动n字节，标示内存中多了这么多，注意pos没有移动

        if (sendfile) { //如果是sendfile则通过上面的ngx_read_file会从磁盘文件读取一份到用户空间
            dst->in_file = 1; //标识该buf还是in_file
            dst->file = src->file;
            dst->file_pos = src->file_pos;
            dst->file_last = src->file_pos + n;

        } else {
            dst->in_file = 0; //不是sendfile的，直接把in_file置0
        }

        src->file_pos += n; //file_pos往后移动n字节，标示这n字节已经读取到内存了

        if (src->file_pos == src->file_last) { //磁盘中的内容已经全部读取到应用层内存中  
            //标记这是最后一块读到的数据，因此通过下面的标识通知wirte模块，可以直接write出去了
            dst->flush = src->flush;
            dst->last_buf = src->last_buf;
            dst->last_in_chain = src->last_in_chain;
        }
    }

    return NGX_OK;
}

//向后端发送请求的调用过程ngx_http_upstream_send_request_body->ngx_output_chain->ngx_chain_writer
ngx_int_t
ngx_chain_writer(void *data, ngx_chain_t *in)
{
    ngx_chain_writer_ctx_t *ctx = data;

    off_t              size;
    ngx_chain_t       *cl, *ln, *chain;
    ngx_connection_t  *c;

    c = ctx->connection;
    /*下面的循环，将in里面的每一个链接节点，添加到ctx->filter_ctx所指的链表中。并记录这些in的链表的大小。*/
    for (size = 0; in; in = in->next) {

#if 1
        if (ngx_buf_size(in->buf) == 0 && !ngx_buf_special(in->buf)) {

            ngx_log_error(NGX_LOG_ALERT, ctx->pool->log, 0,
                          "zero size buf in chain writer "
                          "t:%d r:%d f:%d %p %p-%p %p %O-%O",
                          in->buf->temporary,
                          in->buf->recycled,
                          in->buf->in_file,
                          in->buf->start,
                          in->buf->pos,
                          in->buf->last,
                          in->buf->file,
                          in->buf->file_pos,
                          in->buf->file_last);

            ngx_debug_point();

            continue;
        }
#endif

        size += ngx_buf_size(in->buf);

        ngx_log_debug2(NGX_LOG_DEBUG_CORE, c->log, 0,
                       "chain writer buf fl:%d s:%uO",
                       in->buf->flush, ngx_buf_size(in->buf));

        cl = ngx_alloc_chain_link(ctx->pool);
        if (cl == NULL) {
            return NGX_ERROR;
        }

        cl->buf = in->buf; //把in->buf赋值给新的cl->buf，
        cl->next = NULL;
        //下面这两句实际上就是把cl添加到ctx->out链表头中，
        *ctx->last = cl; 
        ctx->last = &cl->next; //向后移动last指针，指向新的最后一个节点的next变量地址。再次循环走到这里的时候，调用ctx->last=cl会把新的cl添加到out的尾部
    }

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, c->log, 0,
                   "chain writer in: %p", ctx->out);
                   
    //遍历刚刚准备的链表，并统计其大小，这是啥意思?ctx->out为链表头，所以这里遍历的是所有的。
    for (cl = ctx->out; cl; cl = cl->next) {

#if 1
        if (ngx_buf_size(cl->buf) == 0 && !ngx_buf_special(cl->buf)) {

            ngx_log_error(NGX_LOG_ALERT, ctx->pool->log, 0,
                          "zero size buf in chain writer "
                          "t:%d r:%d f:%d %p %p-%p %p %O-%O",
                          cl->buf->temporary,
                          cl->buf->recycled,
                          cl->buf->in_file,
                          cl->buf->start,
                          cl->buf->pos,
                          cl->buf->last,
                          cl->buf->file,
                          cl->buf->file_pos,
                          cl->buf->file_last);

            ngx_debug_point();

            continue;
        }
#endif

        size += ngx_buf_size(cl->buf);
    }

    if (size == 0 && !c->buffered) {//啥数据都么有，不用发了都
        return NGX_OK;
    }

    //调用writev将ctx->out的数据全部发送出去。如果没法送完，则返回没发送完毕的部分。记录到out里面
	//在ngx_event_connect_peer连接上游服务器的时候设置的发送链接函数ngx_send_chain=ngx_writev_chain。
    chain = c->send_chain(c, ctx->out, ctx->limit); //ngx_send_chain->ngx_writev_chain  到后端的请求报文是不会走filter过滤模块的，而是直接调用ngx_writev_chain->ngx_writev发送到后端

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, c->log, 0,
                   "chain writer out: %p", chain);

    if (chain == NGX_CHAIN_ERROR) {
        return NGX_ERROR;
    }

    for (cl = ctx->out; cl && cl != chain; /* void */) { //把ctx->out中已经全部发送出去的in节点从out链表摘除放入free中，重复利用
        ln = cl;
        cl = cl->next;
        ngx_free_chain(ctx->pool, ln);
    }

    ctx->out = chain; //ctx->out上面现在只剩下还没有发送出去的in节点了

    if (ctx->out == NULL) { //说明已经ctx->out链中的所有数据已经全部发送完成
        ctx->last = &ctx->out;

        if (!c->buffered) { 
        //发送到后端的请求报文之前buffered一直都没有操作过为0，如果是应答给客户端的响应，则buffered可能在进入ngx_http_write_filter调用
        //c->send_chain()之前已经有赋值给，发送给客户端包体的时候会经过所有的filter模块走到这里
            return NGX_OK;
        }
    }

    return NGX_AGAIN; //如果上面的chain = c->send_chain(c, ctx->out, ctx->limit)后，out中还有数据则返回NGX_AGAIN等待再次事件触发调度
}
