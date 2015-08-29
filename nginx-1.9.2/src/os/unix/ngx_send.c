
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


ssize_t
ngx_unix_send(ngx_connection_t *c, u_char *buf, size_t size)
{
    ssize_t       n;
    ngx_err_t     err;
    ngx_event_t  *wev;
    int ready = 0;

    wev = c->write;

#if (NGX_HAVE_KQUEUE)

    if ((ngx_event_flags & NGX_USE_KQUEUE_EVENT) && wev->pending_eof) {
        (void) ngx_connection_error(c, wev->kq_errno,
                               "kevent() reported about an closed connection");
        wev->error = 1;
        return NGX_ERROR;
    }

#endif

    for ( ;; ) {
        n = send(c->fd, buf, size, 0);

        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "send: fd:%d %d of %d", c->fd, n, size);

        if (n > 0) {
            //期待发送1000字节，实际上返回500字节，说明内核缓冲区接收到这500字节后已经满了，不能在写, read为0，只有等epoll写事件触发 read
            //但是，接收如果期待接收1000字节，返回500字节则说明我内核缓冲区中只有500字节，因此可以继续recv，ready还是为1
            if (n < (ssize_t) size) { //说明发送了n字节到缓冲区后，缓冲区满了
                wev->ready = 0;
            }

            c->sent += n;

            ready = wev->ready;
            ngx_log_debug1(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "send, ready:%d", ready);
            return n;
        }

        err = ngx_socket_errno;

        if (n == 0) { //recv返回0，表示连接断开，send返回0当作正常情况处理
            ngx_log_error(NGX_LOG_ALERT, c->log, err, "send() returned zero, ready:0");
            wev->ready = 0;
            return n;
        }

        if (err == NGX_EAGAIN || err == NGX_EINTR) {
            wev->ready = 0;

            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           "send() not ready, ready:0");

            if (err == NGX_EAGAIN) { //内核缓冲区已满
                return NGX_AGAIN;
            }

        } else {
            wev->error = 1;
            (void) ngx_connection_error(c, err, "send() failed");
            return NGX_ERROR;
        }
    }
}

