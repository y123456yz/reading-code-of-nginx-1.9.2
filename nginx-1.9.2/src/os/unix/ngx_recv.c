
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


#if (NGX_HAVE_KQUEUE)

ssize_t
ngx_unix_recv(ngx_connection_t *c, u_char *buf, size_t size)
{
    ssize_t       n;
    ngx_err_t     err;
    ngx_event_t  *rev;

    rev = c->read;

    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {
        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "recv: eof:%d, avail:%d, err:%d",
                       rev->pending_eof, rev->available, rev->kq_errno);

        if (rev->available == 0) {
            if (rev->pending_eof) {
                rev->ready = 0;
                rev->eof = 1;

                if (rev->kq_errno) {
                    rev->error = 1;
                    ngx_set_socket_errno(rev->kq_errno);

                    return ngx_connection_error(c, rev->kq_errno,
                               "kevent() reported about an closed connection");
                }

                return 0;

            } else {
                rev->ready = 0;
                return NGX_AGAIN;
            }
        }
    }

    do {
        n = recv(c->fd, buf, size, 0);

        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "recv: fd:%d %d of %d", c->fd, n, size);

        if (n >= 0) {
            if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {
                rev->available -= n;

                /*
                 * rev->available may be negative here because some additional
                 * bytes may be received between kevent() and recv()
                 */

                if (rev->available <= 0) {
                    if (!rev->pending_eof) {
                        rev->ready = 0;
                    }

                    if (rev->available < 0) {
                        rev->available = 0;
                    }
                }

                if (n == 0) {

                    /*
                     * on FreeBSD recv() may return 0 on closed socket
                     * even if kqueue reported about available data
                     */

                    rev->ready = 0;
                    rev->eof = 1;
                    rev->available = 0;
                }

                return n;
            }

            if ((size_t) n < size
                && !(ngx_event_flags & NGX_USE_GREEDY_EVENT))
            {
                rev->ready = 0;
            }

            if (n == 0) {
                rev->eof = 1;
            }

            return n;
        }

        err = ngx_socket_errno;

        if (err == NGX_EAGAIN || err == NGX_EINTR) {
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           "recv() not ready");
            n = NGX_AGAIN;

        } else {
            n = ngx_connection_error(c, err, "recv() failed");
            break;
        }

    } while (err == NGX_EINTR);

    rev->ready = 0;

    if (n == NGX_ERROR) {
        rev->error = 1;
    }

    return n;
}

#else /* ! NGX_HAVE_KQUEUE */

ssize_t
ngx_unix_recv(ngx_connection_t *c, u_char *buf, size_t size)
{
    ssize_t       n;
    ngx_err_t     err;
    ngx_event_t  *rev;
    int ready = 0;

    rev = c->read;

    do {
        /*
            针对非阻塞I/O执行的系统调用则总是立即返回，而不管事件足否已经发生。如果事件没有眭即发生，这些系统调用就
        返回—1．和出错的情况一样。此时我们必须根据errno来区分这两种情况。对accept、send和recv而言，事件未发牛时errno
        通常被设置成EAGAIN（意为“再来一次”）或者EWOULDBLOCK（意为“期待阻塞”）：对conncct而言，errno则被
        设置成EINPROGRESS（意为“在处理中"）。
          */
        //n = recv(c->fd, buf, size, 0); yang test
        //These calls return the number of bytes received, or -1 if an error occurred.  The return value will be 0 when the peer has performed an orderly shutdown.
        n = recv(c->fd, buf, size, 0);//表示TCP错误，见ngx_http_read_request_header   recv返回0表示对方已经关闭连接

        //读取成功，直接返回   



        //recv返回0，本端不应该去关闭连接，如果是因为对端使用了shutdown来关闭半连接，本端还是可以发送数据的，知识不能读数据，所以这里置ready=0
        //如果不是对端shutdown，那么说明是因为读缓冲区数据读完了，没数据了，读不到数据，所以返回0。返回0不能本端不能关闭套接字
        //recv返回0，表示对端使用shutdown来实现半关闭或者异步读写的情况下，缓冲区没有数据可读，也会返回0。send返回0当作正常情况处理
        if (n == 0) { //表示TCP错误，见ngx_http_read_request_header   recv返回0表示对方已经关闭连接 The return value will be 0 when the peer has performed an orderly shutdown.
            rev->ready = 0;//数据读取完毕ready置0
            rev->eof = 1;
            goto end;

        } else if (n > 0) {
            //期待发送1000字节，实际上返回500字节，说明内核缓冲区接收到这500字节后已经满了，不能在写, read为0，只有等epoll写事件触发 read
            //但是，接收如果期待接收1000字节，返回500字节则说明我内核缓冲区中只有500字节，因此可以继续recv，ready还是为1
            if ((size_t) n < size
                && !(ngx_event_flags & NGX_USE_GREEDY_EVENT)) //数据读取完毕ready置0,需要重新添加add epoll event
            {
                rev->ready = 0; //我期望读取1000字节，单实际上返回500字节，说明内核缓冲区数据已经读取完毕  epoll不会走到这里
            }

            goto end;
        }

        //如果内核数据接收完毕，则走到这里n为-1，err为NGX_EAGAIN
        err = ngx_socket_errno;
        
        /* 
          EINTR错误的产生：当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误。   
          在linux进行非阻塞的socket接收数据时经常出现Resource temporarily unavailable，errno代码为11(EAGAIN)，这表明你在非阻塞模式下调用了阻塞操作，
          在该操作没有完成就返回这个错误，这个错误不会破坏socket的同步，不用管它，下次循环接着recv就可以。对非阻塞socket而言，EAGAIN不是一种错误。
          在VxWorks和Windows上，EAGAIN的名字叫做EWOULDBLOCK。 另外，如果出现EINTR即errno为4，错误描述Interrupted system call，操作也应该继续。

          在Linux环境下开发经常会碰到很多错误(设置errno)，其中EAGAIN是其中比较常见的一个错误(比如用在非阻塞操作中)。
从字面上来看，是提示再试一次。这个错误经常出现在当应用程序进行一些非阻塞(non-blocking)操作(对文件或socket)的时候。
例如，以 O_NONBLOCK的标志打开文件/socket/FIFO，如果你连续做read操作而没有数据可读。此时程序不会阻塞起来等待数据准备就绪返回，
read函数会返回一个错误EAGAIN，提示你的应用程序现在没有数据可读请稍后再试。
          */
        if (err == NGX_EAGAIN || err == NGX_EINTR) {  //这两种情况 ,需要继续读
            
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           "recv() not ready"); //recv() not ready (11: Resource temporarily unavailable)
            n = NGX_AGAIN; //返回这个表示内核数据已有的数据已经读取完，需要重新add epoll event来触发新数据epoll返回

        } else {//TCP连接出错了
            n = ngx_connection_error(c, err, "recv() failed");
            break;
        }

    } while (err == NGX_EINTR); //如果读过程中被中断切换，则继续读

    rev->ready = 0;

    if (n == NGX_ERROR) {
        rev->error = 1;
    }

end:
    ready = rev->ready;
    ngx_log_debug4(NGX_LOG_DEBUG_EVENT, c->log, 0,
                           "recv: fd:%d read-size:%d of %d, ready:%d", c->fd, n, size, ready);
    return n;
}

#endif /* NGX_HAVE_KQUEUE */
