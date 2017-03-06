
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_event_connect.h>

ngx_int_t
ngx_event_connect_peer(ngx_peer_connection_t *pc)
{
    int                rc;
    ngx_int_t          event;
    ngx_err_t          err;
    ngx_uint_t         level;
    ngx_socket_t       s;
    ngx_event_t       *rev, *wev;
    ngx_connection_t  *c;

    /*ngx_http_upstream_get_round_robin_peer ngx_http_upstream_get_least_conn_peer ngx_http_upstream_get_hash_peer  
    ngx_http_upstream_get_ip_hash_peer ngx_http_upstream_get_keepalive_peer等 */
    rc = pc->get(pc, pc->data);//ngx_http_upstream_get_round_robin_peer获取一个peer
    if (rc != NGX_OK) {/* 说明后端服务器全部down掉，或者配置的是keepalive方式，直接选择某个peer上面已有的长连接来发送数据，则直接从这里返回 */
        return rc;
    }

    s = ngx_socket(pc->sockaddr->sa_family, SOCK_STREAM, 0);

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, pc->log, 0, "socket %d", s);

    if (s == (ngx_socket_t) -1) {
        ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                      ngx_socket_n " failed");
        return NGX_ERROR;
    }

    /*
     由于Nginx的事件框架要求每个连接都由一个ngx_connection-t结构体来承载，因此这一步将调用ngx_get_connection方法，由ngx_cycle_t
 核心结构体中free_connections指向的空闲连接池处获取到一个ngx_connection_t结构体，作为承载Nginx与上游服务器间的TCP连接
     */
    c = ngx_get_connection(s, pc->log);

    if (c == NULL) {
        if (ngx_close_socket(s) == -1) {
            ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                          ngx_close_socket_n "failed");
        }

        return NGX_ERROR;
    }

    if (pc->rcvbuf) {
        if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
                       (const void *) &pc->rcvbuf, sizeof(int)) == -1)
        {
            ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                          "setsockopt(SO_RCVBUF) failed");
            goto failed;
        }
    }

    if (ngx_nonblocking(s) == -1) { //建立一个TCP套接字，同时，这个套接字需要设置为非阻塞模式。
        ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                      ngx_nonblocking_n " failed");

        goto failed;
    }

    if (pc->local) {
        if (bind(s, pc->local->sockaddr, pc->local->socklen) == -1) {
            ngx_log_error(NGX_LOG_CRIT, pc->log, ngx_socket_errno,
                          "bind(%V) failed", &pc->local->name);

            goto failed;
        }
    }

    c->recv = ngx_recv;
    c->send = ngx_send;
    c->recv_chain = ngx_recv_chain;
    c->send_chain = ngx_send_chain;

    /*
       和后端的ngx_connection_t在ngx_event_connect_peer这里置为1，但在ngx_http_upstream_connect中c->sendfile &= r->connection->sendfile;，
       和客户端浏览器的ngx_connextion_t的sendfile需要在ngx_http_update_location_config中判断，因此最终是由是否在configure的时候是否有加
       sendfile选项来决定是置1还是置0
    */
    c->sendfile = 1;

    c->log_error = pc->log_error;

    if (pc->sockaddr->sa_family == AF_UNIX) {
        c->tcp_nopush = NGX_TCP_NOPUSH_DISABLED;
        c->tcp_nodelay = NGX_TCP_NODELAY_DISABLED;

#if (NGX_SOLARIS)
        /* Solaris's sendfilev() supports AF_NCA, AF_INET, and AF_INET6 */
        c->sendfile = 0;
#endif
    }

    rev = c->read;
    wev = c->write;

    rev->log = pc->log;
    wev->log = pc->log;

    pc->connection = c;

    c->number = ngx_atomic_fetch_add(ngx_connection_counter, 1);

    /*
     事件模块的ngx_event_actions接口，其中的add_conn方法可以将TCP套接字以期待可读、可写事件的方式添加到事件搜集器中。对于
     epoll事件模块来说，add_conn方法就是把套接字以期待EPOLLIN EPOLLOUT事件的方式加入epoll中，这一步即调用add_conn方法把刚刚
     建立的套接字添加到epoll中，表示如果这个套接字上出现了预期的网络事件，则希望epoll能够回调它的handler方法。
     */
    if (ngx_add_conn) {
        /*
        将这个连接ngx_connection t上的读／写事件的handler回调方法都设置为ngx_http_upstream_handler。，见函数外层的ngx_http_upstream_connect
         */
        if (ngx_add_conn(c) == NGX_ERROR) {
            goto failed;
        }
    }

    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, pc->log, 0,
                   "connect to %V, fd:%d #%uA", pc->name, s, c->number);

    /*
     调用connect方法向上游服务器发起TCP连接，作为非阻塞套接字，connect方法可能立刻返回连接建立成功，也可能告诉用户继续等待上游服务器的响应
     对connect连接是否建立成功的检查会在函数外面的u->read_event_handler = ngx_http_upstream_process_header;见函数外层的ngx_http_upstream_connect
     */
     /*
        针对非阻塞I/O执行的系统调用则总是立即返回，而不管事件足否已经发生。如果事件没有眭即发生，这些系统调用就
    返回—1．和出错的情况一样。此时我们必须根据errno来区分这两种情况。对accept、send和recv而言，事件未发牛时errno
    通常被设置成EAGAIN（意为“再来一次”）或者EWOULDBLOCK（意为“期待阻塞”）：对conncct而言，errno则被
    设置成EINPROGRESS（意为“在处理中"）。
      */ //connect的时候返回成功后使用的sock就是socket创建的sock，这和服务器端accept成功返回一个新的sock不一样
      //上面的ngx_add_conn已经把读写事件一起添加到了epoll中
    rc = connect(s, pc->sockaddr, pc->socklen); //connect返回值可以参考<linux高性能服务器开发> 9.5节

    if (rc == -1) {
        err = ngx_socket_errno;


        if (err != NGX_EINPROGRESS
#if (NGX_WIN32)
            /* Winsock returns WSAEWOULDBLOCK (NGX_EAGAIN) */
            && err != NGX_EAGAIN
#endif
            )
        {
            if (err == NGX_ECONNREFUSED
#if (NGX_LINUX)
                /*
                 * Linux returns EAGAIN instead of ECONNREFUSED
                 * for unix sockets if listen queue is full
                 */
                || err == NGX_EAGAIN
#endif
                || err == NGX_ECONNRESET
                || err == NGX_ENETDOWN
                || err == NGX_ENETUNREACH
                || err == NGX_EHOSTDOWN
                || err == NGX_EHOSTUNREACH)
            {
                level = NGX_LOG_ERR;

            } else {
                level = NGX_LOG_CRIT;
            }

            ngx_log_error(level, c->log, err, "connect() to %V failed",
                          pc->name);

            ngx_close_connection(c);
            pc->connection = NULL;

            return NGX_DECLINED;
        }
    }

    if (ngx_add_conn) {
        if (rc == -1) {
        //这个表示发出了连接三步握手中的SYN，单还没有等待对方完全应答回来表示连接成功通过外层的
        //c->write->handler = ngx_http_upstream_handler;  u->write_event_handler = ngx_http_upstream_send_request_handler促发返回成功
            /* NGX_EINPROGRESS */

            return NGX_AGAIN;
        }

        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, pc->log, 0, "connected");

        wev->ready = 1;

        return NGX_OK;  
    }

    if (ngx_event_flags & NGX_USE_IOCP_EVENT) {

        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, pc->log, ngx_socket_errno,
                       "connect(): %d", rc);

        if (ngx_blocking(s) == -1) {
            ngx_log_error(NGX_LOG_ALERT, pc->log, ngx_socket_errno,
                          ngx_blocking_n " failed");
            goto failed;
        }

        /*
         * FreeBSD's aio allows to post an operation on non-connected socket.
         * NT does not support it.
         *
         * TODO: check in Win32, etc. As workaround we can use NGX_ONESHOT_EVENT
         */

        rev->ready = 1;
        wev->ready = 1;

        return NGX_OK;
    }

    if (ngx_event_flags & NGX_USE_CLEAR_EVENT) {

        /* kqueue */

        event = NGX_CLEAR_EVENT;

    } else {

        /* select, poll, /dev/poll */

        event = NGX_LEVEL_EVENT;
    }

    char tmpbuf[256];
        
        snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> epoll NGX_READ_EVENT(et) read add", NGX_FUNC_LINE);
        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, pc->log, 0, tmpbuf);
    if (ngx_add_event(rev, NGX_READ_EVENT, event) != NGX_OK) {
        goto failed;
    }

    if (rc == -1) {

        /* NGX_EINPROGRESS */
        char tmpbuf[256];
        
        snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> epoll NGX_WRITE_EVENT(et) read add", NGX_FUNC_LINE);
        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, pc->log, 0, tmpbuf);
        if (ngx_add_event(wev, NGX_WRITE_EVENT, event) != NGX_OK) {
            goto failed;
        }

        return NGX_AGAIN; //这个表示发出了连接三步握手中的SYN，单还没有等待对方完全应答回来表示连接成功
        //通过外层的 c->write->handler = ngx_http_upstream_handler;  u->write_event_handler = ngx_http_upstream_send_request_handler促发返回成功
    }

    ngx_log_debug0(NGX_LOG_DEBUG_EVENT, pc->log, 0, "connected");

    wev->ready = 1;

    return NGX_OK;

failed:

    ngx_close_connection(c);
    pc->connection = NULL;

    return NGX_ERROR;
}


ngx_int_t
ngx_event_get_peer(ngx_peer_connection_t *pc, void *data)
{
    return NGX_OK;
}
