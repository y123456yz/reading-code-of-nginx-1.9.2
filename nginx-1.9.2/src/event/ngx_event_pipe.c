
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_event_pipe.h>


static ngx_int_t ngx_event_pipe_read_upstream(ngx_event_pipe_t *p);
static ngx_int_t ngx_event_pipe_write_to_downstream(ngx_event_pipe_t *p);

static ngx_int_t ngx_event_pipe_write_chain_to_temp_file(ngx_event_pipe_t *p);
static ngx_inline void ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf);
static ngx_int_t ngx_event_pipe_drain_chains(ngx_event_pipe_t *p);

/*
在有buffering的时候，使用event_pipe进行数据的转发，调用ngx_event_pipe_write_to_downstream函数读取数据，或者发送数据给客户端。
ngx_event_pipe将upstream响应发送回客户端。do_write代表是否要往客户端发送，写数据。
如果设置了，那么会先发给客户端，再读upstream数据，当然，如果读取了数据，也会调用这里的。
*/ //ngx_event_pipe->ngx_event_pipe_write_to_downstream
ngx_int_t
ngx_event_pipe(ngx_event_pipe_t *p, ngx_int_t do_write)
{//注意走到这里的时候，后端发送的头部行信息已经在前面的ngx_http_upstream_send_response->ngx_http_send_header已经把头部行部分发送给客户端了
//该函数处理的只是后端放回过来的网页包体部分
    ngx_int_t     rc;
    ngx_uint_t    flags;
    ngx_event_t  *rev, *wev;
    
    //这个for循环是不断的用ngx_event_pipe_read_upstream读取客户端数据，然后调用ngx_event_pipe_write_to_downstream
    for ( ;; ) {
        if (do_write) { //注意这里的do_write，为1是先写后读，以此循环。为0是先读后写，以此循环
            p->log->action = "sending to client";

            rc = ngx_event_pipe_write_to_downstream(p);
            

            if (rc == NGX_ABORT) {
                return NGX_ABORT;
            }

            if (rc == NGX_BUSY) {
                return NGX_OK;
            }
        }

        p->read = 0;
        p->upstream_blocked = 0;

        p->log->action = "reading upstream";
        
        //从upstream读取数据到chain的链表里面，然后整块整块的调用input_filter进行协议的解析，并将HTTP结果存放在p->in，p->last_in的链表里面。
        if (ngx_event_pipe_read_upstream(p) == NGX_ABORT) {
            return NGX_ABORT;
        }

        /* 非cachable方式下，指定内存用完了数据还没有读完的情况下，或者是后端包体读取完毕，则会从这里返回，其他情况下都会在这里面一直循环 */

        //p->read的值可以参考ngx_event_pipe_read_upstream->p->upstream->recv_chain()->ngx_readv_chain里面是否赋值为0
        //upstream_blocked是在ngx_event_pipe_read_upstream里面设置的变量,代表是否有数据已经从upstream读取了。
        if (!p->read && !p->upstream_blocked) { //内核缓冲区数据已经读完，或者本地指定内存已经用完，则推出
            break; //读取后端返回NGX_AGAIN则read置0
        }

        do_write = 1;//还要写。因为我这次读到了一些数据
    }

    if (p->upstream->fd != (ngx_socket_t) -1) {
        rev = p->upstream->read;

        flags = (rev->eof || rev->error) ? NGX_CLOSE_EVENT : 0;

        //得到这个连接的读写事件结构，如果其发生了错误，那么将其读写事件注册删除掉，否则保存原样。
        if (ngx_handle_read_event(rev, flags, NGX_FUNC_LINE) != NGX_OK) {
            return NGX_ABORT;
        }

        if (!rev->delayed) {
            if (rev->active && !rev->ready) {//没有读写数据了，那就设置一个读超时定时器
                ngx_add_timer(rev, p->read_timeout, NGX_FUNC_LINE); //本轮读取后端数据完毕，添加超时定时器，继续读，如果时间到还没数据，表示超时

            } else if (rev->timer_set) {
             /*
                这里删除的定时器是发送数据到后端后，需要等待后端应答，在
                ngx_http_upstream_send_request->ngx_add_timer(c->read, u->conf->read_timeout, NGX_FUNC_LINE); 中添加的定时器 
                */
                ngx_del_timer(rev, NGX_FUNC_LINE);
            }
        }
    }

    if (p->downstream->fd != (ngx_socket_t) -1
        && p->downstream->data == p->output_ctx)
    {
        wev = p->downstream->write;
        if (ngx_handle_write_event(wev, p->send_lowat, NGX_FUNC_LINE) != NGX_OK) {
            return NGX_ABORT;
        }

        if (!wev->delayed) {
            if (wev->active && !wev->ready) { //想客户端的写超时设置
                ngx_add_timer(wev, p->send_timeout, NGX_FUNC_LINE);

            } else if (wev->timer_set) {
                ngx_del_timer(wev, NGX_FUNC_LINE);
            }
        }
    }

    return NGX_OK;
}

/*
1.从preread_bufs，free_raw_bufs或者ngx_create_temp_buf寻找一块空闲的或部分空闲的内存；
2.调用p->upstream->recv_chain==ngx_readv_chain，用writev的方式读取FCGI的数据,填充chain。
3.对于整块buf都满了的chain节点调用input_filter(ngx_http_fastcgi_input_filter)进行upstream协议解析，比如FCGI协议，解析后的结果放入p->in里面；
4.对于没有填充满的buffer节点，放入free_raw_bufs以待下次进入时从后面进行追加。
5.当然了，如果对端发送完数据FIN了，那就直接调用input_filter处理free_raw_bufs这块数据。
*/
/*
    buffering方式，读数据前首先开辟一块大空间，在ngx_event_pipe_read_upstream->ngx_readv_chain中开辟一个ngx_buf_t(buf1)结构指向读到的数据，
然后在读取数据到in链表的时候，在ngx_http_fastcgi_input_filter会重新创建一个ngx_buf_t(buf1)，这里面设置buf1->shadow=buf2->shadow
buf2->shadow=buf1->shadow。同时把buf2添加到p->in中。当通过ngx_http_write_filter发送数据的时候会把p->in中的数据添加到ngx_http_request_t->out，然后发送，
如果一次没有发送完成，则属于的数据会留在ngx_http_request_t->out中，由写事件触发再次发送。当数据通过p->output_filter(p->output_ctx, out)发送后，buf2
会被添加到p->free中，buf1会被添加到free_raw_bufs中，见ngx_event_pipe_write_to_downstream
*/
static ngx_int_t
ngx_event_pipe_read_upstream(ngx_event_pipe_t *p) 
//ngx_event_pipe_write_to_downstream写数据到客户端，ngx_event_pipe_read_upstream从后端读取数据
{//ngx_event_pipe调用这里读取后端的数据。
    off_t         limit;
    ssize_t       n, size;
    ngx_int_t     rc;
    ngx_buf_t    *b;
    ngx_msec_t    delay;
    ngx_chain_t  *chain, *cl, *ln;
    int upstream_eof = 0;
    int upstream_error = 0;
    int single_buf = 0;
    int leftsize = 0;
    int upstream_done = 0;
    ngx_chain_t  *free_raw_bufs = NULL;

    if (p->upstream_eof || p->upstream_error || p->upstream_done) {
        return NGX_OK;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                   "pipe read upstream, read ready: %d", p->upstream->read->ready);

    for ( ;; ) {
        //数据读取完毕，或者出错，直接退出循环
        if (p->upstream_eof || p->upstream_error || p->upstream_done) {
            break;
        }

        //如果没有预读数据，并且跟upstream的连接还没有read，那就可以退出了，因为没数据可读。
        if (p->preread_bufs == NULL && !p->upstream->read->ready) { //如果后端协议栈数据读取完毕，返回NGX_AGAIN，则ready会置0
            break;
        }

        /*
          下面这个大的if-else就干一件事情: 寻找一块空闲的内存缓冲区，用来待会存放读取进来的upstream的数据。
		如果preread_bufs不为空，就先用之，否则看看free_raw_bufs有没有，或者申请一块
          */
        if (p->preread_bufs) {

            /* use the pre-read bufs if they exist */

            chain = p->preread_bufs;
            p->preread_bufs = NULL;
            n = p->preread_size;

            ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                           "pipe preread: %z", n); //这是读取头部行等信息的时候顺带读取到的包体长度

            if (n) {
                p->read = 1; //表示有读到客户端包体
            }
        } else {
#if (NGX_HAVE_KQUEUE)

            /*
             * kqueue notifies about the end of file or a pending error.
             * This test allows not to allocate a buf on these conditions
             * and not to call c->recv_chain().
             */

            if (p->upstream->read->available == 0
                && p->upstream->read->pending_eof)
            {
                p->upstream->read->ready = 0;
                p->upstream->read->eof = 1;
                p->upstream_eof = 1;
                p->read = 1;

                if (p->upstream->read->kq_errno) {
                    p->upstream->read->error = 1;
                    p->upstream_error = 1;
                    p->upstream_eof = 0;

                    ngx_log_error(NGX_LOG_ERR, p->log,
                                  p->upstream->read->kq_errno,
                                  "kevent() reported that upstream "
                                  "closed connection");
                }

                break;
            }
#endif

            if (p->limit_rate) {
                if (p->upstream->read->delayed) {
                    break;
                }

                limit = (off_t) p->limit_rate * (ngx_time() - p->start_sec + 1)
                        - p->read_length;

                if (limit <= 0) {
                    p->upstream->read->delayed = 1;
                    delay = (ngx_msec_t) (- limit * 1000 / p->limit_rate + 1);
                    ngx_add_timer(p->upstream->read, delay, NGX_FUNC_LINE);
                    break;
                }

            } else {
                limit = 0;
            }

            if (p->free_raw_bufs) { //上次分配了chain->buf后，调用ngx_readv_chain读取数据的时候返回NGX_AGAIN,则这次新的epoll读事件触发后，直接使用上次没有用的chain来重新读取数据
                //当后面的n = p->upstream->recv_chain返回NGX_AGAIN,下次epoll再次触发读的时候，直接用free_raw_bufs

                /* use the free bufs if they exist */

                chain = p->free_raw_bufs;
                if (p->single_buf) { //如果设置了NGX_USE_AIO_EVENT标志， the posted aio operation may currupt a shadow buffer
                    p->free_raw_bufs = p->free_raw_bufs->next;
                    chain->next = NULL;
                } else { //如果不是AIO，那么可以用多块内存一次用readv读取的。
                    p->free_raw_bufs = NULL;
                }

            } else if (p->allocated < p->bufs.num) {

                /* allocate a new buf if it's still allowed */
                /*
                    如果没有超过fastcgi_buffers等指令的限制，那么申请一块内存吧。因为现在没有空闲内存了。
                    allocate a new buf if it's still allowed申请一个ngx_buf_t以及size大小的数据。用来存储从FCGI读取的数据。
                    */
                b = ngx_create_temp_buf(p->pool, p->bufs.size);
                if (b == NULL) {
                    return NGX_ABORT;
                }

                p->allocated++;

                chain = ngx_alloc_chain_link(p->pool);
                if (chain == NULL) {
                    return NGX_ABORT;
                }

                chain->buf = b;
                chain->next = NULL;

            } else if (!p->cacheable
                       && p->downstream->data == p->output_ctx
                       && p->downstream->write->ready
                       && !p->downstream->write->delayed)
            {
            //没有开启换成，并且前面已经开辟了5个3Kbuf已经都开辟了，不能在分配空间了
            //到这里，那说明没法申请内存了，但是配置里面没要求必须先保留在cache里，那我们可以吧当前的数据发送给客户端了。跳出循环进行write操作，然后就会空余处空间来继续读。
                /*
                 * if the bufs are not needed to be saved in a cache and
                 * a downstream is ready then write the bufs to a downstream
                 */

                p->upstream_blocked = 1; 

                ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "pipe downstream ready");

                break;

            } else if (p->cacheable
                       || p->temp_file->offset < p->max_temp_file_size)  //如果后端内容超过了max_temp_file_size，则不缓存
            
            /* 当前fastcgi_buffers 和fastcgi_buffer_size配置的空间都已经用完了，则需要把读取到(就是fastcgi_buffers 和
                fastcgi_buffer_size指定的空间中保存的读取数据)的数据写道临时文件中去 */ 
            
            {//必须缓存，而且当前的缓存文件的位移，其大小小于可允许的大小，那good，可以写入文件了。
             //这里可以看出，在开启cache的时候，只有前面的fastcgi_buffers  5 3K都已经用完了，才会写入临时文件中去//下面将r->in的数据写到临时文件
                /*
                 * if it is allowed, then save some bufs from p->in
                 * to a temporary file, and add them to a p->out chain
                 */

                rc = ngx_event_pipe_write_chain_to_temp_file(p);//下面将r->in的数据写到临时文件

                ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "pipe temp offset: %O", p->temp_file->offset);

                if (rc == NGX_BUSY) {
                    break;
                }

                if (rc == NGX_AGAIN) {
                    if (ngx_event_flags & NGX_USE_LEVEL_EVENT
                        && p->upstream->read->active
                        && p->upstream->read->ready)
                    {
                        if (ngx_del_event(p->upstream->read, NGX_READ_EVENT, 0)
                            == NGX_ERROR)
                        {
                            return NGX_ABORT;
                        }
                    }
                }

                if (rc != NGX_OK) {
                    return rc;
                }

                chain = p->free_raw_bufs;
                if (p->single_buf) {
                    p->free_raw_bufs = p->free_raw_bufs->next;
                    chain->next = NULL;
                } else {
                    p->free_raw_bufs = NULL;
                }

            } else {

                /* there are no bufs to read in */

                ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "no pipe bufs to read in");

                break;
            }

        //到这里，肯定是找到空闲的buf了，chain指向之了。ngx_readv_chain .调用readv不断的读取连接的数据。放入chain的链表里面这里的
        //chain是不是只有一块? 其next成员为空呢，不一定，如果free_raw_bufs不为空，上面的获取空闲buf只要没有使用AIO的话，就可能有多个buffer链表的。
        //注意:这里面只是把读到的数据放入了chain->buf中，但是没有移动尾部last指针，实际上该函数返回后pos和last都还是指向读取数据的头部的
            n = p->upstream->recv_chain(p->upstream, chain, limit); //chain->buf空间用来存储recv_chain从后端接收到的数据

            leftsize = chain->buf->end - chain->buf->last;
            ngx_log_debug2(NGX_LOG_DEBUG_EVENT, p->log, 0,
                           "pipe recv chain: %z, left-size:%d", n, leftsize);

            if (p->free_raw_bufs) { //free_raw_bufs不为空，那就将chain指向的这块放到free_raw_bufs头部。
                chain->next = p->free_raw_bufs;
            }
            p->free_raw_bufs = chain; //把读取到的存有后端数据的chain赋值给free_raw_bufs

            if (n == NGX_ERROR) {
                p->upstream_error = 1;
                return NGX_ERROR;
            }

            if (n == NGX_AGAIN) { //循环回去通过epoll读事件触发继续读,一般都是把内核缓冲区数据读完后从这里返回
                if (p->single_buf) {
                    ngx_event_pipe_remove_shadow_links(chain->buf);
                }

                single_buf = p->single_buf;
                /*
                    2025/04/27 00:40:55[                    ngx_readv_chain,   179]  [debug] 22653#22653: *3 readv() not ready (11: Resource temporarily unavailable)
                    2025/04/27 00:40:55[       ngx_event_pipe_read_upstream,   337]  [debug] 22653#22653: *3 pipe recv chain: -2
                    */
                ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "ngx_event_pipe_read_upstream recv return ngx_again, single_buf:%d ", single_buf);
                break; //当下次epoll再次触发读的时候，就直接使用p->free_raw_bufs
            }

            p->read = 1; //表示有读到数据，并且可以继续读

            if (n == 0) { 
                p->upstream_eof = 1;//upstream_eof表示内核协议栈已经读取完毕，内核协议栈已经没有数据了，需要再次epoll触发读操作
                break; //跳出循环
            }
        } //从上面for循环刚开始的if (p->preread_bufs) {到这里，都在寻找一个空闲的缓冲区，然后读取数据填充chain。够长的。
//读取了数据，下面要进行FCGI协议解析，保存了。

        delay = p->limit_rate ? (ngx_msec_t) n * 1000 / p->limit_rate : 0;

        p->read_length += n; //表示读取到的后端包体部分数据长度增加n字节
        cl = chain; //cl保存了前面ngx_readv_chain的时候读取的数据
        p->free_raw_bufs = NULL;

        while (cl && n > 0) {

		    //下面的函数将c->buf中用shadow指针连接起来的链表中所有节点的recycled,temporary,shadow成员置空。
            ngx_event_pipe_remove_shadow_links(cl->buf);

            /* 前面的n = p->upstream->recv_chain()读取数据后，没有移动last指针，实际上该函数返回后pos和last都还是指向读取数据的头部的 */
            size = cl->buf->end - cl->buf->last; //buf中剩余的空间

            if (n >= size) { //读取的数据比第一块cl->buf(也就是chain->buf)多，说明读到的数据可以把第一个buf塞满
                cl->buf->last = cl->buf->end; //把这坨全部用了,readv填充了数据。

                /* STUB */ cl->buf->num = p->num++; //第几块，cl链中(cl->next)中的第几块

                //主要功能就是解析fastcgi格式包体，解析出包体后，把对应的buf加入到p->in
                //FCGI为ngx_http_fastcgi_input_filter，其他为ngx_event_pipe_copy_input_filter 。用来解析特定格式数据
                if (p->input_filter(p, cl->buf) == NGX_ERROR) { //整块buffer的调用协议解析句柄
                    //这里面，如果cl->buf这块数据解析出来了DATA数据，那么cl->buf->shadow成员指向一个链表，
                //通过shadow成员链接起来的链表，每个成员就是零散的fcgi data数据部分。
                    
                    return NGX_ABORT;
                }

                n -= size;

                //继续处理下一块，并释放这个节点。
                ln = cl;
                cl = cl->next; 
                ngx_free_chain(p->pool, ln);

            } else {  //说明本次读到的n字节数据不能装满一个buf，则移动last指针，同时返回出去继续读

            //如果这个节点的空闲内存数目大于剩下要处理的，就将剩下的存放在这里。 通过后面的if (p->free_raw_bufs && p->length != -1){}执行p->input_filter(p, cl->buf)
                /*
                    啥意思，不用调用input_filter了吗，不是。是这样的，如果剩下的这块数据还不够塞满当前这个cl的缓存大小，
                    那就先存起来，怎么存呢: 别释放cl了，只是移动其大小，然后n=0使循环退出。然后在下面几行的if (cl) {里面可以检测到这种情况
                    于是在下面的if里面会将这个ln处的数据放入free_raw_bufs的头部。不过这里会有多个连接吗? 可能有的。
                    */
                cl->buf->last += n;
                n = 0;
            }
        }

        if (cl) {
            //将上面没有填满一块内存块的数据链接放到free_raw_bufs的前面。注意上面修改了cl->buf->last，后续的读入数据不会
            //覆盖这些数据的。看ngx_readv_chain然后继续读
            for (ln = cl; ln->next; ln = ln->next) { /* void */ }  

            ln->next = p->free_raw_bufs;
            p->free_raw_bufs = cl;
        }

        if (delay > 0) {
            p->upstream->read->delayed = 1;
            ngx_add_timer(p->upstream->read, delay, NGX_FUNC_LINE);
            break;
        }
    }//注意这里是for循环，只有满足p->upstream_eof || p->upstream_error || p->upstream_done才推出

#if (NGX_DEBUG)

    for (cl = p->busy; cl; cl = cl->next) {
        ngx_log_debug8(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe buf busy s:%d t:%d f:%d "
                       "%p, pos %p, size: %z "
                       "file: %O, size: %O",
                       (cl->buf->shadow ? 1 : 0),
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
    }

    for (cl = p->out; cl; cl = cl->next) {
        ngx_log_debug8(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe buf out  s:%d t:%d f:%d "
                       "%p, pos %p, size: %z "
                       "file: %O, size: %O",
                       (cl->buf->shadow ? 1 : 0),
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
    }

    for (cl = p->in; cl; cl = cl->next) {
        ngx_log_debug8(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe buf in   s:%d t:%d f:%d "
                       "%p, pos %p, size: %z "
                       "file: %O, size: %O",
                       (cl->buf->shadow ? 1 : 0),
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
    }

    for (cl = p->free_raw_bufs; cl; cl = cl->next) {
        ngx_log_debug8(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe buf free s:%d t:%d f:%d "
                       "%p, pos %p, size: %z "
                       "file: %O, size: %O",
                       (cl->buf->shadow ? 1 : 0),
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);
    }

    upstream_eof = p->upstream_eof;
    upstream_error = p->upstream_error;
    free_raw_bufs = p->free_raw_bufs;
    upstream_done = p->upstream_done;
    ngx_log_debug5(NGX_LOG_DEBUG_EVENT, p->log, 0,
                   "pipe length: %O, p->upstream_eof:%d, p->upstream_error:%d, p->free_raw_bufs:%p, upstream_done:%d", 
                   p->length, upstream_eof, upstream_error, free_raw_bufs, upstream_done);

#endif

    if (p->free_raw_bufs && p->length != -1) { //注意前面已经把读取到的chain数据加入到了free_raw_bufs
        cl = p->free_raw_bufs;

        if (cl->buf->last - cl->buf->pos >= p->length) { //包体读取完毕

            p->free_raw_bufs = cl->next;

            /* STUB */ cl->buf->num = p->num++;

            //主要功能就是解析fastcgi格式包体，解析出包体后，把对应的buf加入到p->in
            //FCGI为ngx_http_fastcgi_input_filter，其他为ngx_event_pipe_copy_input_filter 。用来解析特定格式数据
            if (p->input_filter(p, cl->buf) == NGX_ERROR) {
                 return NGX_ABORT;
            }

            ngx_free_chain(p->pool, cl);
        }
    }

    if (p->length == 0) { //后端页面包体数据读取完毕或者本来就没有包体，把upstream_done置1
        p->upstream_done = 1;
        p->read = 1;
    }

    //upstream_eof表示内核协议栈已经读取完毕，内核协议栈已经没有数据了，需要再次epoll触发读操作  //注意前面已经把读取到的chain数据加入到了free_raw_bufs
    if ((p->upstream_eof || p->upstream_error) && p->free_raw_bufs) {//没办法了，都快到头了，或者出现错误了，所以处理一下这块不完整的buffer

        /* STUB */ p->free_raw_bufs->buf->num = p->num++;
        //如果数据读取完毕了，或者后端出现问题了，并且，free_raw_bufs不为空，后面还有一部分数据，
		//当然只可能有一块。那就调用input_filter处理它。FCGI为ngx_http_fastcgi_input_filter 在ngx_http_fastcgi_handler里面设置的

		//这里考虑一种情况: 这是最后一块数据了，没满，里面没有data数据，所以ngx_http_fastcgi_input_filter会调用ngx_event_pipe_add_free_buf函数，
		//将这块内存放入free_raw_bufs的前面，可是君不知，这最后一块不存在数据部分的内存正好等于free_raw_bufs，因为free_raw_bufs还没来得及改变。
		//所以，就把自己给替换掉了。这种情况会发生吗?
        if (p->input_filter(p, p->free_raw_bufs->buf) == NGX_ERROR) {
            return NGX_ABORT;
        }

        p->free_raw_bufs = p->free_raw_bufs->next;

        if (p->free_bufs && p->buf_to_file == NULL) {
            for (cl = p->free_raw_bufs; cl; cl = cl->next) {
                if (cl->buf->shadow == NULL) {
                //这个shadow成员指向由我这块buf产生的小FCGI数据块buf的指针列表。如果为NULL，就说明这块buf没有data，可以释放了。
                    ngx_pfree(p->pool, cl->buf->start);
                }
            }
        }
    }

    if (p->cacheable && (p->in || p->buf_to_file)) {  

        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0, "pipe write chain");

        if (ngx_event_pipe_write_chain_to_temp_file(p) == NGX_ABORT) {
            return NGX_ABORT;
        }
    }

    upstream_done = p->upstream_done;
    if(upstream_done)
        ngx_log_debugall(p->log, 0, "pipe read upstream upstream_done:%d", upstream_done);

    return NGX_OK;
}

/*
    buffering方式，读数据前首先开辟一块大空间，在ngx_event_pipe_read_upstream->ngx_readv_chain中开辟一个ngx_buf_t(buf1)结构指向读到的数据，
然后在读取数据到in链表的时候，在ngx_http_fastcgi_input_filter会重新创建一个ngx_buf_t(buf1)，这里面设置buf1->shadow=buf2->shadow
buf2->shadow=buf1->shadow。同时把buf2添加到p->in中。当通过ngx_http_write_filter发送数据的时候会把p->in中的数据添加到ngx_http_request_t->out，然后发送，
如果一次没有发送完成，则属于的数据会留在ngx_http_request_t->out中，由写事件触发再次发送。当数据通过p->output_filter(p->output_ctx, out)发送后，buf2
会被添加到p->free中，buf1会被添加到free_raw_bufs中，见ngx_event_pipe_write_to_downstream
*/
//ngx_event_pipe_write_to_downstream写数据到客户端，ngx_event_pipe_read_upstream从后端读取数据
static ngx_int_t
ngx_event_pipe_write_to_downstream(ngx_event_pipe_t *p) 
{//ngx_event_pipe调用这里进行数据发送给客户端，数据已经准备在p->out,p->in里面了。
    u_char            *prev;
    size_t             bsize;
    ngx_int_t          rc;
    ngx_uint_t         flush, flushed, prev_last_shadow;
    ngx_chain_t       *out, **ll, *cl;
    ngx_connection_t  *downstream;

    downstream = p->downstream;   //与客户端的连接信息

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                   "pipe write downstream, write ready: %d", downstream->write->ready);

    flushed = 0;

    for ( ;; ) {
        if (p->downstream_error) { //如果客户端连接出错了。drain=排水；流干,
            return ngx_event_pipe_drain_chains(p);//清空upstream发过来的，解析过格式后的HTML数据。将其放入free_raw_bufs里面。
        }

        /*
         ngx_event_pipe_write_to_downstream
         if (p->upstream_eof || p->upstream_error || p->upstream_done) {
            p->output_filter(p->output_ctx, p->out);
         }
          */

        //upstream_eof表示内核缓冲区数据已经读完 如果upstream的连接已经关闭了，或出问题了，或者发送完毕了，那就可以发送了。
        if (p->upstream_eof || p->upstream_error || p->upstream_done) {
            //实际上在接受完后端数据后，在想客户端发送包体部分的时候，会两次调用该函数，一次是ngx_event_pipe_write_to_downstream-> p->output_filter(),
            //另一次是ngx_http_upstream_finalize_request->ngx_http_send_special,
            
            /* pass the p->out and p->in chains to the output filter */

            for (cl = p->busy; cl; cl = cl->next) {
                cl->buf->recycled = 0;//不需要回收重复利用了，因为upstream_done了，不会再给我发送数据了。
            }


/*
发送缓存文件中内容到客户端过程:
 ngx_http_file_cache_open->ngx_http_file_cache_read->ngx_http_file_cache_aio_read这个流程获取文件中前面的头部信息相关内容，并获取整个
 文件stat信息，例如文件大小等。
 头部部分在ngx_http_cache_send->ngx_http_send_header发送，
 缓存文件后面的包体部分在ngx_http_cache_send后半部代码中触发在filter模块中发送

 接收后端数据并转发到客户端触发数据发送过程:
 ngx_event_pipe_write_to_downstream中的
 if (p->upstream_eof || p->upstream_error || p->upstream_done) {
    遍历p->in 或者遍历p->out，然后执行输出
    p->output_filter(p->output_ctx, p->out);
 }
 */
            //如果没有开启缓存，数据不会写入临时文件中，p->out = NULL
            if (p->out) {  //和临时文件相关,如果换成存在与临时文件中，走这里
                ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "pipe write downstream flush out");

                for (cl = p->out; cl; cl = cl->next) {
                    cl->buf->recycled = 0;
                }

                //下面，因为p->out的链表里面一块块都是解析后的后端服务器页面数据，所以直接调用ngx_http_output_filter进行数据发送就行了。
                //注意: 没有发送完毕的数据会保存到ngx_http_request_t->out中，HTTP框架会触发再次把r->out写出去，而不是存在p->out中的
                rc = p->output_filter(p->output_ctx, p->out);

                if (rc == NGX_ERROR) {
                    p->downstream_error = 1;
                    return ngx_event_pipe_drain_chains(p);
                }

                p->out = NULL;
            }

            //ngx_event_pipe_read_upstream读取数据后通过ngx_http_fastcgi_input_filter把读取到的数据加入到p->in链表
            //如果开启缓存，则数据写入临时文件中，p->in=NULL
            if (p->in) { //跟out同理。简单调用ngx_http_output_filter进入各个filter发送过程中。
                ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "pipe write downstream flush in");

                for (cl = p->in; cl; cl = cl->next) {
                    cl->buf->recycled = 0; //已经是最后的了，不需要回收了
                }

                //注意下面的发送不是真的writev了，得看具体情况比如是否需要recycled,是否是最后一块等。ngx_http_write_filter会判断这个的。
                rc = p->output_filter(p->output_ctx, p->in);//调用ngx_http_output_filter发送，最后一个是ngx_http_write_filter

                if (rc == NGX_ERROR) {
                    p->downstream_error = 1;
                    return ngx_event_pipe_drain_chains(p);
                }

                p->in = NULL; //在执行上面的output_filter()后，p->in中的数据会添加到r->out中
            }

            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, p->log, 0,  "pipe write downstream done");

            /* TODO: free unused bufs */

            p->downstream_done = 1;
            break; //这里会退出循环
        }

        //否则upstream数据还没有发送完毕。
        if (downstream->data != p->output_ctx
            || !downstream->write->ready
            || downstream->write->delayed)
        {
            break;
        }

        /* bsize is the size of the busy recycled bufs */

        prev = NULL;
        bsize = 0;

        //这里遍历需要busy这个正在发送，已经调用过output_filter的buf链表，计算一下那些可以回收重复利用的buf
        //计算这些buf的总容量，注意这里不是计算busy中还有多少数据没有真正writev出去，而是他们总共的最大容量
        for (cl = p->busy; cl; cl = cl->next) {

            if (cl->buf->recycled) {
                if (prev == cl->buf->start) {
                    continue;
                }

                bsize += cl->buf->end - cl->buf->start; //计算还没有发送出去的ngx_buf_t所指向所有空间的大小
                prev = cl->buf->start;
            }
        }

        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe write busy: %uz", bsize);

        out = NULL;

        //busy_size为fastcgi_busy_buffers_size 指令设置的大小，指最大待发送的busy状态的内存总大小。
		//如果大于这个大小，nginx会尝试去发送新的数据并回收这些busy状态的buf。
        if (bsize >= (size_t) p->busy_size) {
            ngx_log_debug2(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "break while(), bsize:%uz >= (size_t) p->busy_size: %uz", bsize, (size_t) p->busy_size);
            flush = 1;
            goto flush;
        }

        flush = 0;
        ll = NULL;
        prev_last_shadow = 1; //标记上一个节点是不是正好是一块FCGI buffer的最后一个数据节点。

        //遍历p->out,p->in里面的未发送数据，将他们放到out链表后面，注意这里发送的数据不超过busy_size因为配置限制了。
        for ( ;; ) {
        //循环，这个循环的终止后，我们就能获得几块HTML数据节点，并且他们跨越了1个以上的FCGI数据块的并以最后一块带有last_shadow结束。
            if (p->out) { //buf到tempfile的数据会放到out里面。一次read后端服务端数据返回NGX_AGIAN后开始发送缓存中的内容
                //说明数据缓存到了临时文件中
                cl = p->out; 

                if (cl->buf->recycled) {
                    ngx_log_error(NGX_LOG_ALERT, p->log, 0,
                                  "recycled buffer in pipe out chain");
                }

                p->out = p->out->next;

            } else if (!p->cacheable && p->in) { //说明数据时缓存到内存中的
                cl = p->in;

                ngx_log_debug3(NGX_LOG_DEBUG_EVENT, p->log, 0,
                               "pipe write buf ls:%d %p %z",
                               cl->buf->last_shadow, //
                               cl->buf->pos,
                               cl->buf->last - cl->buf->pos);


                //1.对于在in里面的数据，如果其需要回收;
				//2.并且又是某一块大FCGI buf的最后一个有效html数据节点；
				//3.而且当前的没法送的大小大于busy_size, 那就需要回收一下了，因为我们有buffer机制
                if (cl->buf->recycled && prev_last_shadow) {
                    if (bsize + cl->buf->end - cl->buf->start > p->busy_size) {
                        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "break while(), bsize + cl->buf->end - cl->buf->start:%uz > p->busy_size: %uz", 
                            bsize, (size_t) p->busy_size);
                        flush = 1;//超过了大小，标记一下待会是需要真正发送的。不过这个好像没发挥多少作用，因为后面不怎么判断、
                        break;//停止处理后面的内存块，因为这里已经大于busy_size了。
                    }

                    bsize += cl->buf->end - cl->buf->start;
                }

                prev_last_shadow = cl->buf->last_shadow;

                p->in = p->in->next;

            } else {
                break; //一般
            }

            cl->next = NULL;

            if (out) {
                *ll = cl;
            } else {
                out = cl;//指向第一块数据
            }
            ll = &cl->next;
        }

    //到这里后，out指针指向一个链表，其里面的数据是从p->out,p->in来的要发送的数据。见ngx_http_output_filter
    flush:

        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe write: out:%p, flush:%d", out, flush);

        //下面将out指针指向的内存调用output_filter，进入filter过程。
        //如果后端数据有写入临时文件，则out=NULL，只有在获取到全部后端数据并写入临时文件后，才会通过前面的if (p->upstream_eof || p->upstream_error || p->upstream_done) {p->output_filter()}发送出去
        if (out == NULL) { //在下面的ngx_chain_update_chains中有可能置为NULL，表示out链上的数据发送完毕

            if (!flush) {
                break;
            }

            /* a workaround for AIO */
            if (flushed++ > 10) { //最多循环10次，从而可以达到异步的效果，避免在里面反复循环
                return NGX_BUSY;
            }
        }

        rc = p->output_filter(p->output_ctx, out);//简单调用ngx_http_output_filter进入各个filter发送过程中。

/*
    读数据前首先开辟一块大空间，在ngx_event_pipe_read_upstream->ngx_readv_chain中开辟一个ngx_buf_t(buf1)结构指向读到的数据，
然后在读取数据到in链表的时候，在ngx_http_fastcgi_input_filter会重新创建一个ngx_buf_t(buf1)，这里面设置buf1->shadow=buf2->shadow
buf2->shadow=buf1->shadow。同时把buf2添加到p->in中。当通过ngx_http_write_filter发送数据的时候会把p->in中的数据添加到ngx_http_request_t->out，然后发送，
如果一次没有发送完成，则剩余的数据会留在p->out中。当数据通过p->output_filter(p->output_ctx, out)发送后，buf2会被添加到p->free中，
buf1会被添加到free_raw_bufs中，见ngx_event_pipe_write_to_downstream
*/
        //将没有全部发送的buf(last != end)加入到busy，已经全部处理了的buf(end = last)放入free中
        //实际上p->busy最终指向的是ngx_http_write_filter中未发送完的r->out中保存的数据，见ngx_http_write_filter
        /*实际上p->busy最终指向的是ngx_http_write_filter中未发送完的r->out中保存的数据，这部分数据始终在r->out的最前面，后面在读到数据后在
    ngx_http_write_filter中会把新来的数据加到r->out后面，也就是未发送的数据在r->out前面新数据在链后面，所以实际write是之前未发送的先发送出去*/
        ngx_chain_update_chains(p->pool, &p->free, &p->busy, &out, p->tag);

        if (rc == NGX_ERROR) {
            p->downstream_error = 1;
            return ngx_event_pipe_drain_chains(p);
        }

        for (cl = p->free; cl; cl = cl->next) { 

            if (cl->buf->temp_file) {
                if (p->cacheable || !p->cyclic_temp_file) {
                    continue;
                }

                /* reset p->temp_offset if all bufs had been sent */

                if (cl->buf->file_last == p->temp_file->offset) {
                    p->temp_file->offset = 0;
                }
            }

            /* TODO: free buf if p->free_bufs && upstream done */

            /* add the free shadow raw buf to p->free_raw_bufs */

            if (cl->buf->last_shadow) {
                if (ngx_event_pipe_add_free_buf(p, cl->buf->shadow) != NGX_OK) { //配合参考ngx_http_fastcgi_input_filter阅读
                //也就是在读取后端数据的时候创建的ngx_buf_t(读取数据时创建的第一个ngx_buf_t)放入free_raw_bufs
                    return NGX_ABORT;
                }

                cl->buf->last_shadow = 0;
            }

            cl->buf->shadow = NULL;
        }
    }

    return NGX_OK;
}

/*
2015/12/16 04:25:19[           ngx_event_process_posted,    67]  [debug] 19348#19348: *3 delete posted event B0895098
2015/12/16 04:25:19[          ngx_http_upstream_handler,  1332]  [debug] 19348#19348: *3 http upstream request(ev->write:0): "/test3.php?"
2015/12/16 04:25:19[   ngx_http_upstream_process_header,  2417]  [debug] 19348#19348: *3 http upstream process header, fd:14, buffer_size:512
2015/12/16 04:25:19[                      ngx_unix_recv,   204]  [debug] 19348#19348: *3 recv: fd:14 read-size:367 of 367, ready:1
2015/12/16 04:25:19[    ngx_http_fastcgi_process_record,  3080]  [debug] 19348#19348: *3 http fastcgi record byte: 00
2015/12/16 04:25:19[    ngx_http_fastcgi_process_record,  3152]  [debug] 19348#19348: *3 http fastcgi record length: 8184
2015/12/16 04:25:19[    ngx_http_fastcgi_process_header,  2325]  [debug] 19348#19348: *3 http fastcgi parser: 0
2015/12/16 04:25:19[    ngx_http_fastcgi_process_header,  2433]  [debug] 19348#19348: *3 http fastcgi header: "X-Powered-By: PHP/5.2.13"
2015/12/16 04:25:19[    ngx_http_fastcgi_process_header,  2325]  [debug] 19348#19348: *3 http fastcgi parser: 0
2015/12/16 04:25:19[    ngx_http_fastcgi_process_header,  2433]  [debug] 19348#19348: *3 http fastcgi header: "Content-type: text/html"
2015/12/16 04:25:19[    ngx_http_fastcgi_process_header,  2325]  [debug] 19348#19348: *3 http fastcgi parser: 1
2015/12/16 04:25:19[    ngx_http_fastcgi_process_header,  2449]  [debug] 19348#19348: *3 http fastcgi header done
2015/12/16 04:25:19[               ngx_http_send_header,  3150]  [debug] 19348#19348: *3 ngx http send header

2015/12/16 04:25:19[             ngx_http_header_filter,   677]  [debug] 19348#19348: *3 HTTP/1.1 200 OK
Server: nginx/1.9.2
Date: Tue, 15 Dec 2015 20:25:19 GMT
Content-Type: text/html
Transfer-Encoding: chunked
Connection: keep-alive
X-Powered-By: PHP/5.2.13

2015/12/16 04:25:19[              ngx_http_write_filter,   204]  [debug] 19348#19348: *3 write new buf t:1 f:0 080F2CE8, pos 080F2CE8, size: 180 file: 0, size: 0
2015/12/16 04:25:19[              ngx_http_write_filter,   244]  [debug] 19348#19348: *3 http write filter: l:0 f:0 s:180
2015/12/16 04:25:19[    ngx_http_upstream_send_response,  3120]  [debug] 19348#19348: *3 ngx_http_upstream_send_response, buffering flag:1
2015/12/16 04:25:19[     ngx_http_file_cache_set_header,  1256]  [debug] 19348#19348: *3 http file cache set header
2015/12/16 04:25:19[    ngx_http_upstream_send_response,  3303]  [debug] 19348#19348: *3 http cacheable: 1
2015/12/16 04:25:19[    ngx_http_upstream_send_response,  3343]  [debug] 19348#19348: *3 ngx_http_upstream_send_response, p->cacheable:1, tempfile:/var/yyz/cache_xxx/temp, pathfile:/var/yyz/cache_xxx
2015/12/16 04:25:19[ ngx_http_upstream_process_upstream,  4052]  [debug] 19348#19348: *3 http upstream process upstream
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   145]  [debug] 19348#19348: *3 pipe read upstream: 1
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   171]  [debug] 19348#19348: *3 pipe preread: 306
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2773]  [debug] 19348#19348: *3 input buf #0 080F2BB6
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2815]  [debug] 19348#19348: *3 input buf 080F2BB6 306
2015/12/16 04:25:19[                    ngx_readv_chain,   106]  [debug] 19348#19348: *3 readv: 1, last(iov_len):512
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   344]  [debug] 19348#19348: *3 pipe recv chain: 512, left-size:512
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2773]  [debug] 19348#19348: *3 input buf #1 080F2E7C
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2815]  [debug] 19348#19348: *3 input buf 080F2E7C 512
2015/12/16 04:25:19[                    ngx_readv_chain,   106]  [debug] 19348#19348: *3 readv: 1, last(iov_len):512
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   344]  [debug] 19348#19348: *3 pipe recv chain: 512, left-size:512
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2773]  [debug] 19348#19348: *3 input buf #2 080F30E4
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2815]  [debug] 19348#19348: *3 input buf 080F30E4 512
2015/12/16 04:25:19[ngx_event_pipe_write_chain_to_temp_file,   840]  [debug] 19348#19348: *3 ngx_event_pipe_write_chain_to_temp_file, p->buf_to_file:080F27E4, p->cacheable:1, tempfile:/var/yyz/cache_xxx/temp
2015/12/16 04:25:19[               ngx_create_temp_file,   169]  [debug] 19348#19348: *3 hashed path: /var/yyz/cache_xxx/temp/2/00/0000000002
2015/12/16 04:25:19[               ngx_create_temp_file,   174]  [debug] 19348#19348: *3 temp fd:-1
2015/12/16 04:25:19[                    ngx_create_path,   260]  [debug] 19348#19348: *3 temp file: "/var/yyz/cache_xxx/temp/2"
2015/12/16 04:25:19[                    ngx_create_path,   260]  [debug] 19348#19348: *3 temp file: "/var/yyz/cache_xxx/temp/2/00"
2015/12/16 04:25:19[               ngx_create_temp_file,   169]  [debug] 19348#19348: *3 hashed path: /var/yyz/cache_xxx/temp/2/00/0000000002
2015/12/16 04:25:19[               ngx_create_temp_file,   174]  [debug] 19348#19348: *3 temp fd:15
2015/12/16 04:25:19[            ngx_write_chain_to_file,   355]  [debug] 19348#19348: *3 writev: 15, 1536
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   296]  [debug] 19348#19348: *3 pipe temp offset: 1536
2015/12/16 04:25:19[                    ngx_readv_chain,   106]  [debug] 19348#19348: *3 readv: 3, last(iov_len):512
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   344]  [debug] 19348#19348: *3 pipe recv chain: 1536, left-size:512
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2773]  [debug] 19348#19348: *3 input buf #3 080F2AE8
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2815]  [debug] 19348#19348: *3 input buf 080F2AE8 512
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2773]  [debug] 19348#19348: *3 input buf #4 080F2E7C
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2815]  [debug] 19348#19348: *3 input buf 080F2E7C 512
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2773]  [debug] 19348#19348: *3 input buf #5 080F30E4
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2815]  [debug] 19348#19348: *3 input buf 080F30E4 512
2015/12/16 04:25:19[ngx_event_pipe_write_chain_to_temp_file,   840]  [debug] 19348#19348: *3 ngx_event_pipe_write_chain_to_temp_file, p->buf_to_file:00000000, p->cacheable:1, tempfile:/var/yyz/cache_xxx/temp
2015/12/16 04:25:19[            ngx_write_chain_to_file,   355]  [debug] 19348#19348: *3 writev: 15, 1536
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   296]  [debug] 19348#19348: *3 pipe temp offset: 3072
2015/12/16 04:25:19[                    ngx_readv_chain,   106]  [debug] 19348#19348: *3 readv: 3, last(iov_len):512
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   344]  [debug] 19348#19348: *3 pipe recv chain: 1536, left-size:512
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2773]  [debug] 19348#19348: *3 input buf #6 080F2AE8
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2815]  [debug] 19348#19348: *3 input buf 080F2AE8 512
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2773]  [debug] 19348#19348: *3 input buf #7 080F2E7C
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2815]  [debug] 19348#19348: *3 input buf 080F2E7C 512
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2773]  [debug] 19348#19348: *3 input buf #8 080F30E4
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2815]  [debug] 19348#19348: *3 input buf 080F30E4 512
2015/12/16 04:25:19[ngx_event_pipe_write_chain_to_temp_file,   840]  [debug] 19348#19348: *3 ngx_event_pipe_write_chain_to_temp_file, p->buf_to_file:00000000, p->cacheable:1, tempfile:/var/yyz/cache_xxx/temp
2015/12/16 04:25:19[            ngx_write_chain_to_file,   355]  [debug] 19348#19348: *3 writev: 15, 1536
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   296]  [debug] 19348#19348: *3 pipe temp offset: 7680
2015/12/16 04:25:19[                    ngx_readv_chain,   106]  [debug] 19348#19348: *3 readv: 3, last(iov_len):512
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   344]  [debug] 19348#19348: *3 pipe recv chain: 657, left-size:512
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2773]  [debug] 19348#19348: *3 input buf #15 080F2AE8
2015/12/16 04:25:19[      ngx_http_fastcgi_input_filter,  2815]  [debug] 19348#19348: *3 input buf 080F2AE8 512
2015/12/16 04:25:19[                    ngx_readv_chain,   106]  [debug] 19348#19348: *3 readv: 2, last(iov_len):512
2015/12/16 04:25:19[                    ngx_readv_chain,   179]  [debug] 19348#19348: *3 readv() not ready (11: Resource temporarily unavailable)
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   344]  [debug] 19348#19348: *3 pipe recv chain: -2, left-size:367
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   367]  [debug] 19348#19348: *3 ngx_event_pipe_read_upstream recv return ngx_again, single_buf:0 
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   468]  [debug] 19348#19348: *3 pipe buf out  s:0 t:0 f:1 00000000, pos 00000000, size: 0 file: 206, size: 7474
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   481]  [debug] 19348#19348: *3 pipe buf in   s:1 t:1 f:0 080F2AE8, pos 080F2AE8, size: 512 file: 0, size: 0
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   494]  [debug] 19348#19348: *3 pipe buf free s:0 t:1 f:0 080F2E7C, pos 080F2E7C, size: 145 file: 0, size: 0
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   494]  [debug] 19348#19348: *3 pipe buf free s:0 t:1 f:0 080F30E4, pos 080F30E4, size: 0 file: 0, size: 0
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   502]  [debug] 19348#19348: *3 pipe length: -1, p->upstream_eof:0, p->upstream_error:0, p->free_raw_bufs:080F339C
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   559]  [debug] 19348#19348: *3 pipe write chain
2015/12/16 04:25:19[ngx_event_pipe_write_chain_to_temp_file,   840]  [debug] 19348#19348: *3 ngx_event_pipe_write_chain_to_temp_file, p->buf_to_file:00000000, p->cacheable:1, tempfile:/var/yyz/cache_xxx/temp
2015/12/16 04:25:19[                     ngx_write_file,   182]  [debug] 19348#19348: *3 write: 15, 080F2AE8, 512, 7680
2015/12/16 04:25:19[ ngx_event_pipe_write_to_downstream,   590]  [debug] 19348#19348: *3 pipe write downstream: 1
2015/12/16 04:25:19[ ngx_event_pipe_write_to_downstream,   685]  [debug] 19348#19348: *3 pipe write busy: 0
2015/12/16 04:25:19[ ngx_event_pipe_write_to_downstream,   762]  [debug] 19348#19348: *3 pipe write: out:080F27DC, f:0
2015/12/16 04:25:19[             ngx_http_output_filter,  3200]  [debug] 19348#19348: *3 http output filter "/test3.php?"
2015/12/16 04:25:19[               ngx_http_copy_filter,   157]  [debug] 19348#19348: *3 http copy filter: "/test3.php?"
2015/12/16 04:25:19[                      ngx_read_file,    31]  [debug] 19348#19348: *3 read: 15, 081109E0, 7986, 206
2015/12/16 04:25:19[           ngx_http_postpone_filter,   176]  [debug] 19348#19348: *3 http postpone filter "/test3.php?" 080F3434
2015/12/16 04:25:19[       ngx_http_chunked_body_filter,   212]  [debug] 19348#19348: *3 http chunk: 7986
2015/12/16 04:25:19[       ngx_http_chunked_body_filter,   273]  [debug] 19348#19348: *3 yang test ..........xxxxxxxx ################## lstbuf:0
2015/12/16 04:25:19[              ngx_http_write_filter,   148]  [debug] 19348#19348: *3 write old buf t:1 f:0 080F2CE8, pos 080F2CE8, size: 180 file: 0, size: 0
2015/12/16 04:25:19[              ngx_http_write_filter,   204]  [debug] 19348#19348: *3 write new buf t:1 f:0 080F3480, pos 080F3480, size: 6 file: 0, size: 0
2015/12/16 04:25:19[              ngx_http_write_filter,   204]  [debug] 19348#19348: *3 write new buf t:1 f:0 081109E0, pos 081109E0, size: 7986 file: 0, size: 0
2015/12/16 04:25:19[              ngx_http_write_filter,   204]  [debug] 19348#19348: *3 write new buf t:0 f:0 00000000, pos 080CD85D, size: 2 file: 0, size: 0
2015/12/16 04:25:19[              ngx_http_write_filter,   244]  [debug] 19348#19348: *3 http write filter: l:0 f:1 s:8174
2015/12/16 04:25:19[              ngx_http_write_filter,   372]  [debug] 19348#19348: *3 http write filter limit 0
2015/12/16 04:25:19[                         ngx_writev,   199]  [debug] 19348#19348: *3 writev: 8174 of 8174
2015/12/16 04:25:19[              ngx_http_write_filter,   378]  [debug] 19348#19348: *3 http write filter 00000000
2015/12/16 04:25:19[               ngx_http_copy_filter,   221]  [debug] 19348#19348: *3 http copy filter: 0 "/test3.php?"
2015/12/16 04:25:19[ ngx_event_pipe_write_to_downstream,   685]  [debug] 19348#19348: *3 pipe write busy: 0
2015/12/16 04:25:19[ ngx_event_pipe_write_to_downstream,   762]  [debug] 19348#19348: *3 pipe write: out:00000000, f:0
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   145]  [debug] 19348#19348: *3 pipe read upstream: 0
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   494]  [debug] 19348#19348: *3 pipe buf free s:0 t:1 f:0 080F2E7C, pos 080F2E7C, size: 145 file: 0, size: 0
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   494]  [debug] 19348#19348: *3 pipe buf free s:0 t:1 f:0 080F30E4, pos 080F30E4, size: 0 file: 0, size: 0
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   494]  [debug] 19348#19348: *3 pipe buf free s:0 t:1 f:0 080F2AE8, pos 080F2AE8, size: 0 file: 0, size: 0
2015/12/16 04:25:19[       ngx_event_pipe_read_upstream,   502]  [debug] 19348#19348: *3 pipe length: -1, p->upstream_eof:0, p->upstream_error:0, p->free_raw_bufs:080F339C
2015/12/16 04:25:19[                ngx_event_add_timer,    77]  [debug] 19348#19348: *3 <           ngx_event_pipe,    80>  event timer: 14, old: 2807200323, new: 2807200547, 
2015/12/16 04:25:19[           ngx_event_process_posted,    65]  [debug] 19348#19348: begin to run befor posted event AEA95098
2015/12/16 04:25:19[           ngx_event_process_posted,    67]  [debug] 19348#19348: *3 delete posted event AEA95098
2015/12/16 04:25:19[          ngx_http_upstream_handler,  1332]  [debug] 19348#19348: *3 http upstream request(ev->write:1): "/test3.php?"
2015/12/16 04:25:19[    ngx_http_upstream_dummy_handler,  4286]  [debug] 19348#19348: *3 http upstream dummy handler
2015/12/16 04:25:19[           ngx_worker_process_cycle,  1141]  [debug] 19348#19348: worker(19348) cycle again
2015/12/16 04:25:19[           ngx_trylock_accept_mutex,   405]  [debug] 19348#19348: accept mutex locked
2015/12/16 04:25:19[           ngx_epoll_process_events,  1622]  [debug] 19348#19348: begin to epoll_wait, epoll timer: 59776 
2015/12/16 04:25:19[           ngx_epoll_process_events,  1710]  [debug] 19348#19348: epoll: fd:14 EPOLLIN EPOLLOUT  (ev:0005) d:B2695159

*/

/*
如果配置xxx_buffers  XXX_buffer_size指定的空间都用完了，则会把缓存中的数据写入临时文件，然后继续读，读到ngx_event_pipe_write_chain_to_temp_file
后写入临时文件，直到read返回NGX_AGAIN,然后在ngx_event_pipe_write_to_downstream->ngx_output_chain->ngx_output_chain_copy_buf中读取临时文件内容
发送到后端，当数据继续到来，通过epoll read继续循环该流程
*/

/*ngx_http_upstream_init_request->ngx_http_upstream_cache 客户端获取缓存 后端应答回来数据后在ngx_http_upstream_send_response->ngx_http_file_cache_create
中创建临时文件，然后在ngx_event_pipe_write_chain_to_temp_file把读取的后端数据写入临时文件，最后在
ngx_http_upstream_send_response->ngx_http_upstream_process_request->ngx_http_file_cache_update中把临时文件内容rename(相当于mv)到proxy_cache_path指定
的cache目录下面
*/
/*后端数据读取完毕，并且全部写入临时文件后才会执行rename过程，为什么需要临时文件的原因是:例如之前的缓存过期了，现在有个请求正在从后端
获取数据写入临时文件，如果是直接写入缓存文件，则在获取后端数据过程中，如果在来一个客户端请求，如果允许proxy_cache_use_stale updating，则
后面的请求可以直接获取之前老旧的过期缓存，从而可以避免冲突(前面的请求写文件，后面的请求获取文件内容) 
*/
static ngx_int_t
ngx_event_pipe_write_chain_to_temp_file(ngx_event_pipe_t *p)
{
    ssize_t       size, bsize, n;
    ngx_buf_t    *b;
    ngx_uint_t    prev_last_shadow;
    int cacheable = p->cacheable;
    ngx_chain_t  *cl, *tl, *next, *out, **ll, **last_out, **last_free, fl;

    ngx_log_debugall(p->log, 0, "ngx_event_pipe_write_chain_to_temp_file, p->buf_to_file:%p, "
        "p->cacheable:%d, tempfile:%V", p->buf_to_file, cacheable, &p->temp_file->path->name);
    if (p->buf_to_file) { //fl添加到p->in头部，让后赋值给out，out中连接的fl + p，buf_to_file存储的是后端的头部行部分，不包括数据
        fl.buf = p->buf_to_file;
        fl.next = p->in;
        out = &fl; 

    } else {
        out = p->in; //说明头部行第一次在写临时文件的时候已经写进去了，到这里的都是网页包体部分
    }

    if (!p->cacheable) {

        size = 0;
        cl = out;
        ll = NULL;
        prev_last_shadow = 1;

        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0,
                       "pipe offset: %O", p->temp_file->offset);

        do {
            bsize = cl->buf->last - cl->buf->pos;

            ngx_log_debug4(NGX_LOG_DEBUG_EVENT, p->log, 0,
                           "pipe buf ls:%d %p, pos %p, size: %z",
                           cl->buf->last_shadow, cl->buf->start,
                           cl->buf->pos, bsize);

            if (prev_last_shadow
                && ((size + bsize > p->temp_file_write_size)
                    || (p->temp_file->offset + size + bsize
                        > p->max_temp_file_size)))
            {
                break;
            }

            prev_last_shadow = cl->buf->last_shadow;

            size += bsize;
            ll = &cl->next;
            cl = cl->next;

        } while (cl);

        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0, "size: %z", size);

        if (ll == NULL) {
            return NGX_BUSY;
        }

        if (cl) {
           p->in = cl;
           *ll = NULL;

        } else {
           p->in = NULL;
           p->last_in = &p->in;
        }

    } else {
        p->in = NULL; //注意这里把in置为NULL，因为in中的数据已经放到了out中，在后面写入临时文件
        p->last_in = &p->in;
    }

    //创建临时文件并写入
    n = ngx_write_chain_to_temp_file(p->temp_file, out);

    if (n == NGX_ERROR) {
        return NGX_ABORT;
    }

    if (p->buf_to_file) { //说明是第一次写这个临时文件
        //后端头部行部分的数据长度，见ngx_http_upstream_send_response
        p->temp_file->offset = p->buf_to_file->last - p->buf_to_file->pos;  //此时的offset为后端头部行数据长度
        //n是写道文件中的内容(包括后端头部行和网页包体)，因此n等于网页包体内容长度
        n -= p->buf_to_file->last - p->buf_to_file->pos; //n是本次(第一次写临时文件)写入数据部分的长度，off+n就是第一次写入临时文件头部行和包体部分长度
        p->buf_to_file = NULL;
        out = out->next;
    }

    if (n > 0) {
        /* update previous buffer or add new buffer */

        if (p->out) {//说明前面已经写过临时文件
            for (cl = p->out; cl->next; cl = cl->next) { /* void */ } //变量临时文件buf节点，

            b = cl->buf;

            if (b->file_last == p->temp_file->offset) {
                p->temp_file->offset += n;
                b->file_last = p->temp_file->offset;
                goto free;
            }

            last_out = &cl->next; //新来的数据写入临时文件后会重新再后面创建一个chain并加入到p->out尾部

        } else { //第一次写临时文件
            last_out = &p->out; //注意这里last_out指向了&p->out
        }

        cl = ngx_chain_get_free_buf(p->pool, &p->free);
        if (cl == NULL) {
            return NGX_ABORT;
        }

        b = cl->buf;

        ngx_memzero(b, sizeof(ngx_buf_t));

        b->tag = p->tag;

        /* 新开盘的b的file_pos指向本次写入临时文件中的内容头，file_last指向本次写入临时文件中的内容尾部 */
        
        b->file = &p->temp_file->file; //b->file指向这个临时文件
        b->file_pos = p->temp_file->offset; //file_pos大小等于后端头部行部分的数据长度，也就是指向后端返回的网页包体部分数据头部
        p->temp_file->offset += n; //也就是实际存到临时文件中的字节数(包括头部行数据+网页包体数据)，包括临时temp路径下面的所有文件内容大小
        b->file_last = p->temp_file->offset;

        b->in_file = 1;
        b->temp_file = 1;

        *last_out = cl; //把新创建的指向对应临时文件的cl添加到p->out
    }

free:

    for (last_free = &p->free_raw_bufs;
         *last_free != NULL;
         last_free = &(*last_free)->next)
    {
        /* void */
    }

    for (cl = out; cl; cl = next) { 
    //p->in链中的各个chain指向的内存信息已经写入临时文件，并通过创建新的chain节点指向文件里面的各个偏移信息，那么之前p->in中的
    //各个链chain需要加入free链中，以备可以分配复用
        next = cl->next;

        cl->next = p->free;
        p->free = cl;

        b = cl->buf;

        if (b->last_shadow) {

            tl = ngx_alloc_chain_link(p->pool);
            if (tl == NULL) {
                return NGX_ABORT;
            }

            tl->buf = b->shadow;
            tl->next = NULL;

            *last_free = tl;
            last_free = &tl->next;

            b->shadow->pos = b->shadow->start;
            b->shadow->last = b->shadow->start;

            ngx_event_pipe_remove_shadow_links(b->shadow);
        }
    }

    return NGX_OK;
}


/* the copy input filter */

ngx_int_t
ngx_event_pipe_copy_input_filter(ngx_event_pipe_t *p, ngx_buf_t *buf)
{
    ngx_buf_t    *b;
    ngx_chain_t  *cl;

    if (buf->pos == buf->last) {
        return NGX_OK;
    }

    cl = ngx_chain_get_free_buf(p->pool, &p->free);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    b = cl->buf;

    ngx_memcpy(b, buf, sizeof(ngx_buf_t));
    b->shadow = buf;
    b->tag = p->tag;
    b->last_shadow = 1;
    b->recycled = 1;
    buf->shadow = b;

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, p->log, 0, "input buf #%d", b->num);

    if (p->in) {
        *p->last_in = cl;
    } else {
        p->in = cl;
    }
    p->last_in = &cl->next;

    if (p->length == -1) {
        return NGX_OK;
    }

    p->length -= b->last - b->pos;

    return NGX_OK;
}


/*
//删除数据的shadow，以及recycled设置为0，表示不需要循环利用，这里实现了buffering功能
//因为ngx_http_write_filter函数里面判断如果有recycled标志，就会立即将数据发送出去，
//因此这里将这些标志清空，到ngx_http_write_filter那里就会尽量缓存的。
*/
static ngx_inline void
ngx_event_pipe_remove_shadow_links(ngx_buf_t *buf)
{
    ngx_buf_t  *b, *next;

    b = buf->shadow; //这个shadow指向的是buf这块裸FCGI数据的第一个数据节点

    if (b == NULL) {
        return;
    }

    while (!b->last_shadow) { //如果不是最后一个数据节点，不断往后遍历，
        next = b->shadow;

        b->temporary = 0;
        b->recycled = 0;

        b->shadow = NULL;//把shadow成员置空。
        b = next;
    }

    b->temporary = 0;
    b->recycled = 0;
    b->last_shadow = 0;

    b->shadow = NULL;

    buf->shadow = NULL;
}


ngx_int_t
ngx_event_pipe_add_free_buf(ngx_event_pipe_t *p, ngx_buf_t *b)
{ //将参数的b代表的数据块挂入free_raw_bufs的开头或者第二个位置。b为上层觉得没用了的数据块。
    ngx_chain_t  *cl;

    cl = ngx_alloc_chain_link(p->pool);//这里不会出现b就等于free_raw_bufs->buf的情况吗
    if (cl == NULL) {
        return NGX_ERROR;
    }

    if (p->buf_to_file && b->start == p->buf_to_file->start) { 
        b->pos = p->buf_to_file->last;
        b->last = p->buf_to_file->last;

    } else {
        b->pos = b->start;//置空这坨数据
        b->last = b->start;
    }

    b->shadow = NULL;

    cl->buf = b;

    if (p->free_raw_bufs == NULL) { //如果该表中没有节点，则直接把心来的cl加进来
        p->free_raw_bufs = cl;
        cl->next = NULL;

        return NGX_OK;
    }

    //看下面的注释，意思是，如果最前面的free_raw_bufs中没有数据，那就吧当前这块数据放入头部就行。
	//否则如果当前free_raw_bufs有数据，那就得放到其后面了。为什么会有数据呢?比如，读取一些数据后，还剩下一个尾巴存放在free_raw_bufs，然后开始往客户端写数据
	//写完后，自然要把没用的buffer放入到这里面来。这个是在ngx_event_pipe_write_to_downstream里面做的。或者干脆在ngx_event_pipe_drain_chains里面做。
	//因为这个函数在inpupt_filter里面调用是从数据块开始处理，然后到后面的，
	//并且在调用input_filter之前是会将free_raw_bufs置空的。应该是其他地方也有调用。
    if (p->free_raw_bufs->buf->pos == p->free_raw_bufs->buf->last) { 
    //头部buf没有数据  参考函数ngx_event_pipe_read_upstream，因为读取后端数据未填满一个buf指向的缓冲区，则会加入free表头

        /* add the free buf to the list start */

        cl->next = p->free_raw_bufs; //加入到链表头部
        p->free_raw_bufs = cl;

        return NGX_OK;
    }

    /* the first free buf is partially filled, thus add the free buf after it */

    cl->next = p->free_raw_bufs->next; //加入到尾部
    p->free_raw_bufs->next = cl;

    return NGX_OK;
}

/*
遍历p->in/out/busy，将其链表所属的fastcgi数据块释放，放入到free_raw_bufs中间去。也就是，清空upstream发过来的，解析过格式后的HTML PHP等数据。
*/
static ngx_int_t
ngx_event_pipe_drain_chains(ngx_event_pipe_t *p)
{
    ngx_chain_t  *cl, *tl;

    for ( ;; ) {
        if (p->busy) {
            cl = p->busy;
            p->busy = NULL;

        } else if (p->out) {
            cl = p->out;
            p->out = NULL;

        } else if (p->in) {
            cl = p->in;
            p->in = NULL;

        } else {
            return NGX_OK;
        }

        while (cl) {/*要知道，这里cl里面，比如p->in里面的这些ngx_buf_t结构所指向的数据内存实际上是在
        ngx_event_pipe_read_upstream里面的input_filter进行协议解析的时候设置为跟从客户端读取数据时的buf公用的，也就是所谓的影子。
		然后，虽然p->in指向的链表里面有很多很多个节点，每个节点代表一块HTML PHP等代码，但是他们并不是独占一块内存的，而是可能共享的，
		比如一块大的buffer，里面有3个FCGI的STDOUT数据包，都有data部分，那么将存在3个b的节点链接到p->in的末尾，他们的shadow成员
		分别指向下一个节点，最后一个节点就指向其所属的大内存结构。具体在ngx_http_fastcgi_input_filter实现。
        */
            if (cl->buf->last_shadow) {//碰到了某个大FCGI数据块的最后一个节点，释放只，然后进入下一个大块里面的某个小html 数据块。
                if (ngx_event_pipe_add_free_buf(p, cl->buf->shadow) != NGX_OK) {
                    return NGX_ABORT;
                }

                cl->buf->last_shadow = 0;
            }

            cl->buf->shadow = NULL;
            tl = cl->next;
            cl->next = p->free;//把cl这个小buf节点放入p->free，供ngx_http_fastcgi_input_filter进行重复使用。
            p->free = cl;
            cl = tl;
        }
    }
}
