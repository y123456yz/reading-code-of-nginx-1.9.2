
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>

//ngx_linux_sendfile_chain和ngx_writev_chain
ngx_chain_t *
ngx_writev_chain(ngx_connection_t *c, ngx_chain_t *in, off_t limit)
{//调用writev一次发送多个缓冲区，如果没有发送完毕，则返回剩下的链接结构头部。
//ngx_chain_writer调用这里，调用方式为 ctx->out = c->send_chain(c, ctx->out, ctx->limit);
//第二个参数为要发送的数据
    ssize_t        n, sent;
    off_t          send, prev_send;
    ngx_chain_t   *cl;
    ngx_event_t   *wev;
    ngx_iovec_t    vec;
    struct iovec   iovs[NGX_IOVS_PREALLOCATE];

    wev = c->write;//拿到这个连接的写事件结构

    if (!wev->ready) {//连接还没准备好，返回当前的节点。
        return in;
    }

#if (NGX_HAVE_KQUEUE)

    if ((ngx_event_flags & NGX_USE_KQUEUE_EVENT) && wev->pending_eof) {
        (void) ngx_connection_error(c, wev->kq_errno,
                               "kevent() reported about an closed connection");
        wev->error = 1;
        return NGX_CHAIN_ERROR;
    }

#endif

    /* the maximum limit size is the maximum size_t value - the page size */

    if (limit == 0 || limit > (off_t) (NGX_MAX_SIZE_T_VALUE - ngx_pagesize)) {
        limit = NGX_MAX_SIZE_T_VALUE - ngx_pagesize;//够大了，最大的整数
    }

    send = 0;

    vec.iovs = iovs;
    vec.nalloc = NGX_IOVS_PREALLOCATE;

    for ( ;; ) {
        prev_send = send; //prev_send为上一次调用ngx_writev发送出去的字节数

        /* create the iovec and coalesce the neighbouring bufs */

        cl = ngx_output_chain_to_iovec(&vec, in, limit - send, c->log);

        if (cl == NGX_CHAIN_ERROR) {
            return NGX_CHAIN_ERROR;
        }

        if (cl && cl->buf->in_file) {
            ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                          "file buf in writev "
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

            return NGX_CHAIN_ERROR;
        }

        send += vec.size; //为ngx_output_chain_to_iovec中组包的in链中所有数据长度和

        n = ngx_writev(c, &vec); 
        //我期望发送vec->size字节数据，但是实际上内核发送出去的很可能比vec->size小，n为实际发送出去的字节数，因此需要继续发送

        if (n == NGX_ERROR) {
            return NGX_CHAIN_ERROR;
        }

        sent = (n == NGX_AGAIN) ? 0 : n;//记录发送的数据大小。

        c->sent += sent;//递增统计数据，这个链接上发送的数据大小

        in = ngx_chain_update_sent(in, sent); //send是此次调用ngx_wrtev发送成功的字节数
        //ngx_chain_update_sent返回后的in链已经不包括之前发送成功的in节点了，这上面只包含剩余的数据
        
        if (send - prev_send != sent) { //这里说明最多调用ngx_writev两次成功发送后，这里就会返回
            wev->ready = 0; //标记暂时不能发送数据了，必须重新epoll_add写事件
            return in;
        }

        if (send >= limit || in == NULL) { //数据发送完毕，或者本次发送成功的字节数比limit还多，则返回出去
            return in; //
        }
    }
}

//把in链中的buf拷贝到vec->iovs[n++]中
ngx_chain_t *
ngx_output_chain_to_iovec(ngx_iovec_t *vec, ngx_chain_t *in, size_t limit,
    ngx_log_t *log)
{
    size_t         total, size;
    u_char        *prev;
    ngx_uint_t     n;
    struct iovec  *iov;

    iov = NULL;
    prev = NULL;
    total = 0;
    n = 0;
    //循环发送数据，一次一块IOV_MAX数目的缓冲区。
    for ( /* void */ ; in && total < limit; in = in->next) {

        if (ngx_buf_special(in->buf)) {
            continue;
        }

        if (in->buf->in_file) { //如果为1,表示是sendfile发送，见ngx_output_chain_copy_buf
            break;
        }

        if (!ngx_buf_in_memory(in->buf)) {
            ngx_log_error(NGX_LOG_ALERT, log, 0,
                          "bad buf in output chain "
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

            return NGX_CHAIN_ERROR;
        }

        size = in->buf->last - in->buf->pos;//计算这个节点的大小

        if (size > limit - total) {//超过最大发送大小。截断，这次只发送这么多
            size = limit - total;
        }

        if (prev == in->buf->pos) {//如果还是等于刚才的位置，那就复用
            iov->iov_len += size;

        } else {//否则要新增一个节点。返回之
            if (n == vec->nalloc) {
                break;
            }

            iov = &vec->iovs[n++];

            iov->iov_base = (void *) in->buf->pos;//从这里开始
            iov->iov_len = size;//有这么多我要发送
        }

        prev = in->buf->pos + size;//记录刚才发到了这个位置，为指针。
        total += size;//增加已经记录的数据长度。
    }

    vec->count = n;
    vec->size = total;

    return in;
}


ssize_t
ngx_writev(ngx_connection_t *c, ngx_iovec_t *vec)
{
    ssize_t    n;
    ngx_err_t  err;

eintr:
    //调用writev发送这些数据，返回发送的数据大小
    //readv 和writev可以一下读写多个缓冲区的内容，read和write只能一下读写一个缓冲区的内容； 
    /* On success, the readv() function returns the number of bytes read; the writev() function returns the number of bytes written.  
        On error, -1 is returned, and errno is  set appropriately. readv返回被读的字节总数。如果没有更多数据和碰到文件末尾时返回0的计数。 */
    n = writev(c->fd, vec->iovs, vec->count);

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0,
                   "writev: %z of %uz", n, vec->size);

    if (n == -1) {
        err = ngx_errno;

        switch (err) {
        case NGX_EAGAIN:
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           "writev() not ready");
            return NGX_AGAIN;

        case NGX_EINTR:
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           "writev() was interrupted");
            goto eintr;

        default:
            c->write->error = 1;
            ngx_connection_error(c, err, "writev() failed");
            return NGX_ERROR;
        }
    }

    return n;
}
