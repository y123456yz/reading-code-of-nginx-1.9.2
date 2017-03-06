
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>

/*
tcp_nopush
官方:

tcp_nopush

Syntax: tcp_nopush on | off

Default: off

Context: http

server

location

Reference: tcp_nopush

 

This directive permits or forbids the use of thesocket options TCP_NOPUSH on FreeBSD or TCP_CORK on Linux. This option is onlyavailable when using sendfile.

Setting this option causes nginx to attempt to sendit’s HTTP response headers in one packet on Linux and FreeBSD 4.x

You can read more about the TCP_NOPUSH and TCP_CORKsocket options here.

 

linux 下是tcp_cork,上面的意思就是说，当使用sendfile函数时，tcp_nopush才起作用，它和指令tcp_nodelay是互斥的。tcp_cork是linux下tcp/ip传输的一个标准了，这个标准的大概的意思是，一般情况下，在tcp交互的过程中，当应用程序接收到数据包后马上传送出去，不等待，而tcp_cork选项是数据包不会马上传送出去，等到数据包最大时，一次性的传输出去，这样有助于解决网络堵塞，已经是默认了。

也就是说tcp_nopush = on 会设置调用tcp_cork方法，这个也是默认的，结果就是数据包不会马上传送出去，等到数据包最大时，一次性的传输出去，这样有助于解决网络堵塞。


以快递投递举例说明一下（以下是我的理解，也许是不正确的），当快递东西时，快递员收到一个包裹，马上投递，这样保证了即时性，但是会耗费大量的人力物力，在网络上表现就是会引起网络堵塞，而当快递收到一个包裹，把包裹放到集散地，等一定数量后统一投递，这样就是tcp_cork的选项干的事情，这样的话，会最大化的利用网络资源，虽然有一点点延迟。

对于nginx配置文件中的tcp_nopush，默认就是tcp_nopush,不需要特别指定，这个选项对于www，ftp等大文件很有帮助

 

tcp_nodelay
        TCP_NODELAY和TCP_CORK基本上控制了包的“Nagle化”，Nagle化在这里的含义是采用Nagle算法把较小的包组装为更大的帧。 John Nagle是Nagle算法的发明人，后者就是用他的名字来命名的，他在1984年首次用这种方法来尝试解决福特汽车公司的网络拥塞问题（欲了解详情请参看IETF RFC 896）。他解决的问题就是所谓的silly window syndrome，中文称“愚蠢窗口症候群”，具体含义是，因为普遍终端应用程序每产生一次击键操作就会发送一个包，而典型情况下一个包会拥有一个字节的数据载荷以及40个字节长的包头，于是产生4000%的过载，很轻易地就能令网络发生拥塞,。 Nagle化后来成了一种标准并且立即在因特网上得以实现。它现在已经成为缺省配置了，但在我们看来，有些场合下把这一选项关掉也是合乎需要的。

       现在让我们假设某个应用程序发出了一个请求，希望发送小块数据。我们可以选择立即发送数据或者等待产生更多的数据然后再一次发送两种策略。如果我们马上发送数据，那么交互性的以及客户/服务器型的应用程序将极大地受益。如果请求立即发出那么响应时间也会快一些。以上操作可以通过设置套接字的TCP_NODELAY = on 选项来完成，这样就禁用了Nagle 算法。

       另外一种情况则需要我们等到数据量达到最大时才通过网络一次发送全部数据，这种数据传输方式有益于大量数据的通信性能，典型的应用就是文件服务器。应用 Nagle算法在这种情况下就会产生问题。但是，如果你正在发送大量数据，你可以设置TCP_CORK选项禁用Nagle化，其方式正好同 TCP_NODELAY相反（TCP_CORK和 TCP_NODELAY是互相排斥的）。




endfile
=================
nginx 的 sendfile 选项可以在 nginx 发送文件时使用系统调用 sendfile(2)


sendfile(2) 可以在传输文件数据时使用内核空间中的一些数据。这样可以节省大量的资源：
1.sendfile(2) 是系统调用，这就意味着操作是在内核中完成的，因此没有昂贵的上下文切换资源
2.sendfile(2) 取代了系统 read 和 write 操作。
3.sendfile(2) 允许零复制，也就是可以直接从 DMA 写数据


先来看一下不用 sendfile 的传统网络传输过程：


read(file, tmp_buf, len);
write(socket, tmp_buf, len);


硬盘 >> kernel buffer >> user buffer >> kernel socket buffer >> 协议栈


一般来说一个网络应用是通过读硬盘数据，然后写数据到 socket 来完成网络传输的。
具体过程是：


1、系统调用 read() 产生一个上下文切换：从 user mode 切换到 kernel mode，然后 DMA 执行拷贝，把文件数据从硬盘读到一个 kernel buffer 里。
2、数据从 kernel buffer 拷贝到 user buffer，然后系统调用 read() 返回，这时又产生一个上下文切换：从kernel mode 切换到 user mode。
3、系统调用 write() 产生一个上下文切换：从 user mode 切换到 kernel mode，然后把步骤2读到 user buffer 的数据拷贝到 kernel buffer（数据第2次拷贝到 kernel buffer），不过这次是个不同的 kernel buffer，这个 buffer 和 socket 相关联。
4、系统调用 write() 返回，产生一个上下文切换：从 kernel mode 切换到 user mode（第4次切换了），然后 DMA 从 kernel buffer 拷贝数据到协议栈（第4次拷贝了）。


上面4个步骤有4次上下文切换，有4次拷贝，在kernel 2.0+ 版本中，系统调用 sendfile() 就是用来简化上面步骤提升性能的。
sendfile() 不但能减少切换次数而且还能减少拷贝次数。


------------------------------
再来看一下用 sendfile() 来进行网络传输的过程：


sendfile(socket, file, len);


硬盘 >> kernel buffer (快速拷贝到kernel socket buffer) >> 协议栈


1、系统调用 sendfile() 通过 DMA 把硬盘数据拷贝到 kernel buffer，然后数据被 kernel 直接拷贝到另外一个与 socket 相关的 kernel buffer。这里没有 user mode 和 kernel mode 之间的切换，在 kernel 中直接完成了从一个 buffer 到另一个 buffer 的拷贝。
2、DMA 把数据从 kernel buffer 直接拷贝给协议栈，没有切换，也不需要数据从 user mode 拷贝到 kernel mode，因为数据就在 kernel 里。

*/
static ssize_t ngx_linux_sendfile(ngx_connection_t *c, ngx_buf_t *file,
    size_t size);

#if (NGX_THREADS)
#include <ngx_thread_pool.h>

#if !(NGX_HAVE_SENDFILE64)
#error sendfile64() is required!
#endif

static ngx_int_t ngx_linux_sendfile_thread(ngx_connection_t *c, ngx_buf_t *file,
    size_t size, size_t *sent);
static void ngx_linux_sendfile_thread_handler(void *data, ngx_log_t *log);
#endif


/*
 * On Linux up to 2.4.21 sendfile() (syscall #187) works with 32-bit
 * offsets only, and the including <sys/sendfile.h> breaks the compiling,
 * if off_t is 64 bit wide.  So we use own sendfile() definition, where offset
 * parameter is int32_t, and use sendfile() for the file parts below 2G only,
 * see src/os/unix/ngx_linux_config.h
 *
 * Linux 2.4.21 has the new sendfile64() syscall #239.
 *
 * On Linux up to 2.6.16 sendfile() does not allow to pass the count parameter
 * more than 2G-1 bytes even on 64-bit platforms: it returns EINVAL,
 * so we limit it to 2G-1 bytes.
 */

#define NGX_SENDFILE_MAXSIZE  2147483647L


/*
向后端的数据发送，不会经过各个filter模块，向客户端的包体响应会经过各个filter模块
2016/01/05 21:02:43[           ngx_event_process_posted,    67]  [debug] 23495#23495: *1 delete posted event AEA04098
2016/01/05 21:02:43[          ngx_http_upstream_handler,  1400]  [debug] 23495#23495: *1 http upstream request(ev->write:1): "/test2.php?"
2016/01/05 21:02:43[ngx_http_upstream_send_request_handler,  2420]  [debug] 23495#23495: *1 http upstream send request handler
2016/01/05 21:02:43[     ngx_http_upstream_send_request,  2167]  [debug] 23495#23495: *1 http upstream send request
2016/01/05 21:02:43[ngx_http_upstream_send_request_body,  2305]  [debug] 23495#23495: *1 http upstream send request body
2016/01/05 21:02:43[                   ngx_output_chain,    67][yangya  [debug] 23495#23495: *1 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2016/01/05 21:02:43[                   ngx_output_chain,    90][yangya  [debug] 23495#23495: *1 only one chain buf to output_filter
2016/01/05 21:02:43[                   ngx_chain_writer,   747]  [debug] 23495#23495: *1 chain writer buf fl:0 s:600
2016/01/05 21:02:43[                   ngx_chain_writer,   762]  [debug] 23495#23495: *1 chain writer in: 080F268C
2016/01/05 21:02:43[           ngx_linux_sendfile_chain,   161][yangya  [debug] 23495#23495: *1 @@@@@@@@@@@@@@@@@@@@@@@begin ngx_linux_sendfile_chain @@@@@@@@@@@@@@@@@@@
2016/01/05 21:02:43[                         ngx_writev,   201]  [debug] 23495#23495: *1 writev: 600 of 600
2016/01/05 21:02:43[                   ngx_chain_writer,   801]  [debug] 23495#23495: *1 chain writer out: 00000000


向后端的数据发送，不会经过各个filter模块，向客户端的包体响应会经过各个filter模块
2016/01/05 21:02:43[ ngx_event_pipe_write_to_downstream,   623]  [debug] 23495#23495: *1 pipe write downstream flush out
2016/01/05 21:02:43[             ngx_http_output_filter,  3338]  [debug] 23495#23495: *1 http output filter "/test2.php?"
2016/01/05 21:02:43[               ngx_http_copy_filter,   199]  [debug] 23495#23495: *1 http copy filter: "/test2.php?", r->aio:0
2016/01/05 21:02:43[                   ngx_output_chain,    67][yangya  [debug] 23495#23495: *1 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2016/01/05 21:02:43[                      ngx_read_file,    83]  [debug] 23495#23495: *1 read file /var/yyz/cache_xxx/temp/1/00/0000000001: 15, 081109E0, 215, 206
2016/01/05 21:02:43[           ngx_http_postpone_filter,   176]  [debug] 23495#23495: *1 http postpone filter "/test2.php?" 080F2D4C
2016/01/05 21:02:43[       ngx_http_chunked_body_filter,   212]  [debug] 23495#23495: *1 http chunk: 215
2016/01/05 21:02:43[       ngx_http_chunked_body_filter,   273]  [debug] 23495#23495: *1 yang test ..........xxxxxxxx ################## lstbuf:0
2016/01/05 21:02:43[              ngx_http_write_filter,   151]  [debug] 23495#23495: *1 write old buf t:1 f:0 080F2AE8, pos 080F2AE8, size: 180 file: 0, size: 0
2016/01/05 21:02:43[              ngx_http_write_filter,   207]  [debug] 23495#23495: *1 write new buf t:1 f:0 080F2D98, pos 080F2D98, size: 4 file: 0, size: 0
2016/01/05 21:02:43[              ngx_http_write_filter,   207]  [debug] 23495#23495: *1 write new buf t:1 f:0 081109E0, pos 081109E0, size: 215 file: 0, size: 0
2016/01/05 21:02:43[              ngx_http_write_filter,   207]  [debug] 23495#23495: *1 write new buf t:0 f:0 00000000, pos 080CDEDD, size: 2 file: 0, size: 0
2016/01/05 21:02:43[              ngx_http_write_filter,   247]  [debug] 23495#23495: *1 http write filter: last:0 flush:1 size:401
2016/01/05 21:02:43[              ngx_http_write_filter,   379]  [debug] 23495#23495: *1 http write filter limit 0
2016/01/05 21:02:43[           ngx_linux_sendfile_chain,   161][yangya  [debug] 23495#23495: *1 @@@@@@@@@@@@@@@@@@@@@@@begin ngx_linux_sendfile_chain @@@@@@@@@@@@@@@@@@@
2016/01/05 21:02:43[                         ngx_writev,   201]  [debug] 23495#23495: *1 writev: 401 of 401
2016/01/05 21:02:43[              ngx_http_write_filter,   385]  [debug] 23495#23495: *1 http write filter 00000000
2016/01/05 21:02:43[               ngx_http_copy_filter,   276]  [debug] 23495#23495: *1 http copy filter rc: 0, buffered:0 "/test2.php?"
2016/01/05 21:02:43[ ngx_event_pipe_write_to_downstream,   662]  [debug] 23495#23495: *1 pipe write downstream done

*/

//ngx_linux_io
ngx_chain_t * //只要支持sendfile，不管陪没有配置sendfile on都会走到该函数中，除非开启了异步aio
ngx_linux_sendfile_chain(ngx_connection_t *c, ngx_chain_t *in, off_t limit)
{ //向后端的数据发送，不会经过各个filter模块，向客户端的包体响应会经过各个filter模块
    int            tcp_nodelay;
    off_t          send, prev_send;
    size_t         file_size, sent;
    ssize_t        n;
    ngx_err_t      err;
    ngx_buf_t     *file;
    ngx_event_t   *wev;
    ngx_chain_t   *cl;
    ngx_iovec_t    header;
    struct iovec   headers[NGX_IOVS_PREALLOCATE];
#if (NGX_THREADS)
    ngx_int_t      rc;
    ngx_uint_t     thread_handled, thread_complete;
#endif

    ngx_log_debugall(c->log, 0, "@@@@@@@@@@@@@@@@@@@@@@@begin ngx_linux_sendfile_chain @@@@@@@@@@@@@@@@@@@");
    wev = c->write;

    if (!wev->ready) {
        return in;
    }

    /* the maximum limit size is 2G-1 - the page size */

    if (limit == 0 || limit > (off_t) (NGX_SENDFILE_MAXSIZE - ngx_pagesize)) {
        limit = NGX_SENDFILE_MAXSIZE - ngx_pagesize;
    }


    send = 0;

    header.iovs = headers;
    header.nalloc = NGX_IOVS_PREALLOCATE;

    for ( ;; ) {
        prev_send = send;
#if (NGX_THREADS)
        thread_handled = 0;
        thread_complete = 0;
#endif

        /* create the iovec and coalesce the neighbouring bufs */
        //把in链中的buf拷贝到vec->iovs[n++]中，注意只会拷贝内存中的数据到iovec中，不会拷贝文件中的
        cl = ngx_output_chain_to_iovec(&header, in, limit - send, c->log);

        if (cl == NGX_CHAIN_ERROR) {
            return NGX_CHAIN_ERROR;
        }

        send += header.size; //in中所有数据size之和

        /* set TCP_CORK if there is a header before a file */

        if (c->tcp_nopush == NGX_TCP_NOPUSH_UNSET
            && header.count != 0 //等于0，则表明chain链中的所有数据在文件中
            && cl
            && cl->buf->in_file)
        {
            /* the TCP_CORK and TCP_NODELAY are mutually exclusive */

            if (c->tcp_nodelay == NGX_TCP_NODELAY_SET) {

                tcp_nodelay = 0;

                if (setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY,
                               (const void *) &tcp_nodelay, sizeof(int)) == -1)
                {
                    err = ngx_socket_errno;

                    /*
                     * there is a tiny chance to be interrupted, however,
                     * we continue a processing with the TCP_NODELAY
                     * and without the TCP_CORK
                     */

                    if (err != NGX_EINTR) {
                        wev->error = 1;
                        ngx_connection_error(c, err,
                                             "setsockopt(TCP_NODELAY) failed");
                        return NGX_CHAIN_ERROR;
                    }

                } else {
                    c->tcp_nodelay = NGX_TCP_NODELAY_UNSET;

                    ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, 0,
                                   "no tcp_nodelay");
                }
            }

            if (c->tcp_nodelay == NGX_TCP_NODELAY_UNSET) {

                if (ngx_tcp_nopush(c->fd) == NGX_ERROR) {
                    err = ngx_socket_errno;

                    /*
                     * there is a tiny chance to be interrupted, however,
                     * we continue a processing without the TCP_CORK
                     */

                    if (err != NGX_EINTR) {
                        wev->error = 1;
                        ngx_connection_error(c, err,
                                             ngx_tcp_nopush_n " failed");
                        return NGX_CHAIN_ERROR;
                    }

                } else {
                    c->tcp_nopush = NGX_TCP_NOPUSH_SET;

                    ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, 0,
                                   "tcp_nopush");
                }
            }
        }

        /* get the file buf */
        //=0等于0，则表明chain链中的所有数据在文件中，一般sendfile on的时候走这里
        /*
         说明chain中的数据是in_file的，也就是在缓存文件中，一般开启sendfile on的时候走这里,因为ngx_output_chain_as_is返回1，不会重新开辟内存空间读取缓存内容。
         in_file中的内存还是in_file的，而不会拷贝到新分配的内存中， 参考ngx_http_copy_filter->ngx_output_chain   ngx_output_chain_as_is等
            */
        if (header.count == 0 && cl && cl->buf->in_file && send < limit) {
            file = cl->buf;

            /* coalesce the neighbouring file bufs */

            file_size = (size_t) ngx_chain_coalesce_file(&cl, limit - send);

            send += file_size;
#if 1
            if (file_size == 0) {
                ngx_debug_point();
                return NGX_CHAIN_ERROR;
            }
#endif

#if (NGX_THREADS)
            if (file->file->thread_handler) {
                rc = ngx_linux_sendfile_thread(c, file, file_size, &sent);

                switch (rc) {
                case NGX_OK:
                    thread_handled = 1;
                    break;

                case NGX_DONE:
                    thread_complete = 1;
                    break;

                case NGX_AGAIN:
                    break;

                default: /* NGX_ERROR */
                    return NGX_CHAIN_ERROR;
                }

            } else
#endif
            {
                n = ngx_linux_sendfile(c, file, file_size);

                if (n == NGX_ERROR) {
                    return NGX_CHAIN_ERROR;
                }

                sent = (n == NGX_AGAIN) ? 0 : n;
            }

        } else {
         /*
         说明chain中的数据在内存中，一般不开启sendfile on的时候走这里,因为ngx_http_copy_filter->ngx_output_chain中会重新
         分配内存读取缓存文件内容，见ngx_output_chain_as_is。之前buf->in_file的内容就会变好内存型的
            */n = ngx_writev(c, &header);

            if (n == NGX_ERROR) {
                return NGX_CHAIN_ERROR;
            }

            sent = (n == NGX_AGAIN) ? 0 : n;
        }

        c->sent += sent;

        in = ngx_chain_update_sent(in, sent);

        if ((size_t) (send - prev_send) != sent) {
#if (NGX_THREADS)
            if (thread_handled) {
                return in;
            }

            if (thread_complete) {
                send = prev_send + sent;
                continue;
            }
#endif
            wev->ready = 0;
            return in;
        }

        if (send >= limit || in == NULL) {
            return in;
        }
    }
}

/*
rocktmq中对零拷贝的解释
（1）零拷贝原理：Consumer消费消息过程，使用了零拷贝，零拷贝包括一下2中方式，RocketMQ使用第一种方式，因小块数据传输的要求效果比sendfile方式好
    a )使用mmap+write方式   (mmap将一个文件或者其它对象映射进内存)
     优点：即使频繁调用，使用小文件块传输，效率也很高
     缺点：不能很好的利用DMA方式，会比sendfile多消耗CPU资源，内存安全性控制复杂，需要避免JVM Crash问题
    b）使用sendfile方式
     优点：可以利用DMA方式，消耗CPU资源少，大块文件传输效率高，无内存安全新问题
     缺点：小块文件效率低于mmap方式，只能是BIO方式传输，不能使用NIO

    mmap是一种内存映射文件的方法，即将一个文件或者其它对象映射到进程的地址空间，实现文件磁盘地址和进程虚拟地址空间
中一段虚拟地址的一一对映关系。实现这样的映射关系后，进程就可以采用指针的方式读写操作这一段内存，而系统会自动回
写脏页面到对应的文件磁盘上，即完成了对文件的操作而不必再调用read,write等系统调用函数。相反，内核空间对这段区域
的修改也直接反映用户空间，从而可以实现不同进程间的文件共享。


http://www.linuxjournal.com/article/6345?page=0,0
http://blog.csdn.net/kisimple/article/details/42499225
*/
static ssize_t
ngx_linux_sendfile(ngx_connection_t *c, ngx_buf_t *file, size_t size)
{
#if (NGX_HAVE_SENDFILE64)
    off_t      offset;
#else
    int32_t    offset;
#endif
    ssize_t    n;
    ngx_err_t  err;

#if (NGX_HAVE_SENDFILE64)
    offset = file->file_pos;
#else
    offset = (int32_t) file->file_pos;
#endif

eintr:

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0,
                   "sendfile: @%O %uz", file->file_pos, size);
  //一般大缓存文件用aio发送，小文件用sendfile，因为aio是异步的，不影响其他流程，但是sendfile是同步的，太大的话可能需要多次sendfile才能发送完，有种阻塞感觉

    //它减少了内核态与用户态之间的两次内存复制，这样就会从磁盘中读取文件后直接在内核态发送到网卡设备，
    n = sendfile(c->fd, file->file->fd, &offset, size);

    if (n == -1) {
        err = ngx_errno;

        switch (err) {
        case NGX_EAGAIN:
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           "sendfile() is not ready");
            return NGX_AGAIN;

        case NGX_EINTR:
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           "sendfile() was interrupted");
            goto eintr;

        default:
            c->write->error = 1;
            ngx_connection_error(c, err, "sendfile() failed");
            return NGX_ERROR;
        }
    }

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, c->log, 0, "sendfile: %z of %uz @%O",
                   n, size, file->file_pos);

    return n;
}


#if (NGX_THREADS)
//ngx_linux_sendfile_thread中创建空间和赋值
typedef struct {
    ngx_buf_t     *file;
    ngx_socket_t   socket;
    size_t         size;

    size_t         sent;
    ngx_err_t      err;
} ngx_linux_sendfile_ctx_t;


static ngx_int_t
ngx_linux_sendfile_thread(ngx_connection_t *c, ngx_buf_t *file, size_t size,
    size_t *sent)
{
    ngx_uint_t                 flags;
    ngx_event_t               *wev;
    ngx_thread_task_t         *task;
    ngx_linux_sendfile_ctx_t  *ctx;

    ngx_log_debug3(NGX_LOG_DEBUG_CORE, c->log, 0,
                   "linux sendfile thread: %d, %uz, %O",
                   file->file->fd, size, file->file_pos);

    task = c->sendfile_task;

    if (task == NULL) {
        task = ngx_thread_task_alloc(c->pool, sizeof(ngx_linux_sendfile_ctx_t));
        if (task == NULL) {
            return NGX_ERROR;
        }

        task->handler = ngx_linux_sendfile_thread_handler;

        c->sendfile_task = task;
    }

    ctx = task->ctx;
    wev = c->write;

    if (task->event.complete) {
        task->event.complete = 0;

        if (ctx->err && ctx->err != NGX_EAGAIN) {
            wev->error = 1;
            ngx_connection_error(c, ctx->err, "sendfile() failed");
            return NGX_ERROR;
        }

        *sent = ctx->sent;

        return (ctx->sent == ctx->size) ? NGX_DONE : NGX_AGAIN;
    }

    ctx->file = file;
    ctx->socket = c->fd;
    ctx->size = size;

    if (wev->active) {
        flags = (ngx_event_flags & NGX_USE_CLEAR_EVENT) ? NGX_CLEAR_EVENT
                                                        : NGX_LEVEL_EVENT;

        if (ngx_del_event(wev, NGX_WRITE_EVENT, flags) == NGX_ERROR) {
            return NGX_ERROR;
        }
    }

    if (file->file->thread_handler(task, file->file) != NGX_OK) {
        return NGX_ERROR;
    }

    *sent = 0;

    return NGX_OK;
}


static void
ngx_linux_sendfile_thread_handler(void *data, ngx_log_t *log)
{
    ngx_linux_sendfile_ctx_t *ctx = data;

    off_t       offset;
    ssize_t     n;
    ngx_buf_t  *file;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, log, 0, "linux sendfile thread handler");

    file = ctx->file;
    offset = file->file_pos;

again:

    n = sendfile(ctx->socket, file->file->fd, &offset, ctx->size);

    if (n == -1) {
        ctx->err = ngx_errno;

    } else {
        ctx->sent = n;
        ctx->err = 0;
    }

#if 0
    ngx_time_update();
#endif

    ngx_log_debug4(NGX_LOG_DEBUG_EVENT, log, 0,
                   "sendfile: %z (err: %i) of %uz @%O",
                   n, ctx->err, ctx->size, file->file_pos);

    if (ctx->err == NGX_EINTR) {
        goto again;
    }
}

#endif /* NGX_THREADS */
