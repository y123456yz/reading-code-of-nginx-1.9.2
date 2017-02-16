
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


static ngx_int_t ngx_enable_accept_events(ngx_cycle_t *cycle);
static ngx_int_t ngx_disable_accept_events(ngx_cycle_t *cycle, ngx_uint_t all);
static void ngx_close_accepted_connection(ngx_connection_t *c);

/*
如何建立新连接
    上文提刭过，处理新连接事件的回调函数是ngx_event_accept，其原型如下。void ngx_event_accept (ngx_event_t★ev)
    下面简单介绍一下它的流程，如图9-6所示。
    下面对流程中的7个步骤进行说明。
    1)首先调用accept方法试图建立新连接，如果没有准备好的新连接事件，ngx_event_accept方法会直接返回。
    2)设置负载均衡阈值ngx_accept_disabled，这个阈值是进程允许的总连接数的1/8减去空闲连接数，
    3)调用ngx_get_connection方法由连接池中获取一个ngx_connection_t连接对象。
    4)为ngx_connection_t中的pool指针建立内存池。在这个连接释放到空闲连接池时，释放pool内存池。
    5)设置套接字的属性，如设为非阻塞套接字。
    6)将这个新连接对应的读事件添加到epoll等事件驱动模块中，这样，在这个连接上如果接收到用户请求epoll_wait，就会收集到这个事件。
    7)调用监听对象ngx_listening_t中的handler回调方法。ngx_listening_t结构俸的handler回调方法就是当新的TCP连接刚刚建立完成时在这里调用的。
    最后，如果监听事件的available标志位为1，再次循环到第1步，否则ngx_event_accept方法结束。事件的available标志位对应着multi_accept配置
    项。当available为l时，告诉Nginx -次性尽量多地建立新连接，它的实现原理也就在这里
*/
//这里的event是在ngx_event_process_init中从连接池中获取的 ngx_connection_t中的->read读事件
//accept是在ngx_event_process_init(但进程或者不配置负载均衡的时候)或者(多进程，配置负载均衡)的时候把accept事件添加到epoll中
void //该形参中的ngx_connection_t(ngx_event_t)是为accept事件连接准备的空间，当accept返回成功后，会重新获取一个ngx_connection_t(ngx_event_t)用来读写该连接
ngx_event_accept(ngx_event_t *ev) //在ngx_process_events_and_timers中执行              
{ //一个accept事件对应一个ev，如当前一次有4个客户端accept，应该对应4个ev事件，一次来多个accept的处理在下面的do {}while中实现
    socklen_t          socklen;
    ngx_err_t          err;
    ngx_log_t         *log;
    ngx_uint_t         level;
    ngx_socket_t       s;

//如果是文件异步i/o中的ngx_event_aio_t，则它来自ngx_event_aio_t->ngx_event_t(只有读),如果是网络事件中的event,则为ngx_connection_s中的event(包括读和写)
    ngx_event_t       *rev, *wev; 
    ngx_listening_t   *ls;
    ngx_connection_t  *c, *lc;
    ngx_event_conf_t  *ecf;
    u_char             sa[NGX_SOCKADDRLEN];
#if (NGX_HAVE_ACCEPT4)
    static ngx_uint_t  use_accept4 = 1;
#endif

    if (ev->timedout) {
        if (ngx_enable_accept_events((ngx_cycle_t *) ngx_cycle) != NGX_OK) {
            return;
        }

        ev->timedout = 0;
    }

    ecf = ngx_event_get_conf(ngx_cycle->conf_ctx, ngx_event_core_module);

    if (!(ngx_event_flags & NGX_USE_KQUEUE_EVENT)) {
        ev->available = ecf->multi_accept;   
    }

    lc = ev->data;
    ls = lc->listening;
    ev->ready = 0;

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "accept on %V, ready: %d", &ls->addr_text, ev->available);

    do { /* 如果是一次读取一个accept事件的话，循环体只执行一次， 如果是一次性可以读取所有的accept事件，则这个循环体执行次数为accept事件数*/
        socklen = NGX_SOCKADDRLEN;

#if (NGX_HAVE_ACCEPT4) //ngx_close_socket可以关闭套接字
        if (use_accept4) {
            s = accept4(lc->fd, (struct sockaddr *) sa, &socklen,
                        SOCK_NONBLOCK);
        } else {
            s = accept(lc->fd, (struct sockaddr *) sa, &socklen);
        }
#else
    /*
            针对非阻塞I/O执行的系统调用则总是立即返回，而不管事件足否已经发生。如果事件没有眭即发生，这些系统调用就
        返回—1．和出错的情况一样。此时我们必须根据errno来区分这两种情况。对accept、send和recv而言，事件未发牛时errno
        通常被设置成EAGAIN（意为“再来一次”）或者EWOULDBLOCK（意为“期待阻塞”）：对conncct而言，errno则被
        设置成EINPROGRESS（意为“在处理中"）。
          */
        s = accept(lc->fd, (struct sockaddr *) sa, &socklen);
#endif

        if (s == (ngx_socket_t) -1) {
            err = ngx_socket_errno;

            /* 如果要去一次性读取所有的accept信息，当读取完毕后，通过这里返回。所有的accept事件都读取完毕 */
            if (err == NGX_EAGAIN) { //如果event{}开启multi_accept，则在accept完该listen ip:port对应的ip和端口连接后，会通过这里返回
                ngx_log_debug0(NGX_LOG_DEBUG_EVENT, ev->log, err,
                               "accept() not ready");
                return;
            }

            level = NGX_LOG_ALERT;

            if (err == NGX_ECONNABORTED) {
                level = NGX_LOG_ERR;

            } else if (err == NGX_EMFILE || err == NGX_ENFILE) {
                level = NGX_LOG_CRIT;
            }

#if (NGX_HAVE_ACCEPT4)
            ngx_log_error(level, ev->log, err,
                          use_accept4 ? "accept4() failed" : "accept() failed");

            if (use_accept4 && err == NGX_ENOSYS) {
                use_accept4 = 0;
                ngx_inherited_nonblocking = 0;
                continue;
            }
#else
            ngx_log_error(level, ev->log, err, "accept() failed");
#endif

            if (err == NGX_ECONNABORTED) {
                if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {
                    ev->available--;
                }

                if (ev->available) {
                    continue;
                }
            }

            if (err == NGX_EMFILE || err == NGX_ENFILE) {
                if (ngx_disable_accept_events((ngx_cycle_t *) ngx_cycle, 1)
                    != NGX_OK)
                {
                    return;
                }

                if (ngx_use_accept_mutex) {
                    if (ngx_accept_mutex_held) {
                        ngx_shmtx_unlock(&ngx_accept_mutex);
                        ngx_accept_mutex_held = 0;
                    }
//当前进程连接accpet失败，则可以暂时设置为1，下次来的时候由其他进程竞争accpet锁，下下次该进程继续竞争该accept，因为在下次的时候ngx_process_events_and_timers
//ngx_accept_disabled = 1; 减去1后为0，可以继续竞争
                    ngx_accept_disabled = 1; 
                } else { ////如果是不需要实现负载均衡，则扫尾延时下继续在ngx_process_events_and_timers中accept
                    ngx_add_timer(ev, ecf->accept_mutex_delay, NGX_FUNC_LINE);
                }
            }

            return;
        }

#if (NGX_STAT_STUB)
        (void) ngx_atomic_fetch_add(ngx_stat_accepted, 1);
#endif
        //设置负载均衡阀值 最开始free_connection_n=connection_n，见ngx_event_process_init
        ngx_accept_disabled = ngx_cycle->connection_n / 8
                              - ngx_cycle->free_connection_n; //判断可用连接的数目和总数目的八分之一大小，如果可用的小于八分之一，为正

        //在服务器端accept客户端连接成功(ngx_event_accept)后，会通过ngx_get_connection从连接池获取一个ngx_connection_t结构，也就是每个客户端连接对于一个ngx_connection_t结构，
        //并且为其分配一个ngx_http_connection_t结构，ngx_connection_t->data = ngx_http_connection_t，见ngx_http_init_connection

        //从连接池中获取一个空闲ngx_connection_t，用于客户端连接建立成功后向该连接读写数据，函数形参中的ngx_event_t对应的是为accept事件对应的
        //ngx_connection_t中对应的event
        c = ngx_get_connection(s, ev->log);  //ngx_get_connection中c->fd = s;
        //注意，这里的ngx_connection_t是从连接池中从新获取的，和ngx_epoll_process_events中的ngx_connection_t是两个不同的。

        if (c == NULL) {
            if (ngx_close_socket(s) == -1) {
                ngx_log_error(NGX_LOG_ALERT, ev->log, ngx_socket_errno,
                              ngx_close_socket_n " failed");
            }

            return;
        }

#if (NGX_STAT_STUB)
        (void) ngx_atomic_fetch_add(ngx_stat_active, 1);
#endif

        c->pool = ngx_create_pool(ls->pool_size, ev->log);
        if (c->pool == NULL) {
            ngx_close_accepted_connection(c);
            return;
        }

        c->sockaddr = ngx_palloc(c->pool, socklen);
        if (c->sockaddr == NULL) {
            ngx_close_accepted_connection(c);
            return;
        }

        ngx_memcpy(c->sockaddr, sa, socklen);

        log = ngx_palloc(c->pool, sizeof(ngx_log_t));
        if (log == NULL) {
            ngx_close_accepted_connection(c);
            return;
        }

        /* set a blocking mode for iocp and non-blocking mode for others */

        if (ngx_inherited_nonblocking) {
            if (ngx_event_flags & NGX_USE_IOCP_EVENT) {
                if (ngx_blocking(s) == -1) {
                    ngx_log_error(NGX_LOG_ALERT, ev->log, ngx_socket_errno,
                                  ngx_blocking_n " failed");
                    ngx_close_accepted_connection(c);
                    return;
                }
            }

        } else {
            if (!(ngx_event_flags & NGX_USE_IOCP_EVENT)) {
                if (ngx_nonblocking(s) == -1) {
                    ngx_log_error(NGX_LOG_ALERT, ev->log, ngx_socket_errno,
                                  ngx_nonblocking_n " failed");
                    ngx_close_accepted_connection(c);
                    return;
                }
            }
        }

        *log = ls->log;

        c->recv = ngx_recv;
        c->send = ngx_send;
        c->recv_chain = ngx_recv_chain;
        c->send_chain = ngx_send_chain;

        c->log = log;
        c->pool->log = log;

        c->socklen = socklen;
        c->listening = ls;
        c->local_sockaddr = ls->sockaddr;
        c->local_socklen = ls->socklen;

        c->unexpected_eof = 1;

#if (NGX_HAVE_UNIX_DOMAIN)
        if (c->sockaddr->sa_family == AF_UNIX) {
            c->tcp_nopush = NGX_TCP_NOPUSH_DISABLED;
            c->tcp_nodelay = NGX_TCP_NODELAY_DISABLED;
#if (NGX_SOLARIS)
            /* Solaris's sendfilev() supports AF_NCA, AF_INET, and AF_INET6 */
            c->sendfile = 0;
#endif
        }
#endif
        //注意，这里的ngx_connection_t是从连接池中从新获取的，和ngx_epoll_process_events中的ngx_connection_t是两个不同的。
        rev = c->read; 
        wev = c->write;

        wev->ready = 1;

        if (ngx_event_flags & NGX_USE_IOCP_EVENT) {
            rev->ready = 1;
        }

        if (ev->deferred_accept) {
            rev->ready = 1;
#if (NGX_HAVE_KQUEUE)
            rev->available = 1;
#endif
        }

        rev->log = log;
        wev->log = log;

        /*
         * TODO: MT: - ngx_atomic_fetch_add()
         *             or protection by critical section or light mutex
         *
         * TODO: MP: - allocated in a shared memory
         *           - ngx_atomic_fetch_add()
         *             or protection by critical section or light mutex
         */

        c->number = ngx_atomic_fetch_add(ngx_connection_counter, 1);

#if (NGX_STAT_STUB)
        (void) ngx_atomic_fetch_add(ngx_stat_handled, 1);
#endif

        if (ls->addr_ntop) {
            c->addr_text.data = ngx_pnalloc(c->pool, ls->addr_text_max_len);
            if (c->addr_text.data == NULL) {
                ngx_close_accepted_connection(c);
                return;
            }

            c->addr_text.len = ngx_sock_ntop(c->sockaddr, c->socklen,
                                             c->addr_text.data,
                                             ls->addr_text_max_len, 0);
            if (c->addr_text.len == 0) {
                ngx_close_accepted_connection(c);
                return;
            }
        }

#if (NGX_DEBUG)
        {

        ngx_str_t             addr;
        struct sockaddr_in   *sin;
        ngx_cidr_t           *cidr;
        ngx_uint_t            i;
        u_char                text[NGX_SOCKADDR_STRLEN];
#if (NGX_HAVE_INET6)
        struct sockaddr_in6  *sin6;
        ngx_uint_t            n;
#endif

        cidr = ecf->debug_connection.elts;
        for (i = 0; i < ecf->debug_connection.nelts; i++) {
            if (cidr[i].family != (ngx_uint_t) c->sockaddr->sa_family) {
                goto next;
            }

            switch (cidr[i].family) {

#if (NGX_HAVE_INET6)
            case AF_INET6:
                sin6 = (struct sockaddr_in6 *) c->sockaddr;
                for (n = 0; n < 16; n++) {
                    if ((sin6->sin6_addr.s6_addr[n]
                        & cidr[i].u.in6.mask.s6_addr[n])
                        != cidr[i].u.in6.addr.s6_addr[n])
                    {
                        goto next;
                    }
                }
                break;
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
            case AF_UNIX:
                break;
#endif

            default: /* AF_INET */
                sin = (struct sockaddr_in *) c->sockaddr;
                if ((sin->sin_addr.s_addr & cidr[i].u.in.mask)
                    != cidr[i].u.in.addr)
                {
                    goto next;
                }
                break;
            }

            log->log_level = NGX_LOG_DEBUG_CONNECTION|NGX_LOG_DEBUG_ALL;
            break;

        next:
            continue;
        }

        if (log->log_level & NGX_LOG_DEBUG_EVENT) {
            addr.data = text;
            addr.len = ngx_sock_ntop(c->sockaddr, c->socklen, text,
                                     NGX_SOCKADDR_STRLEN, 1);

            ngx_log_debug3(NGX_LOG_DEBUG_EVENT, log, 0,
                           "*%uA accept: %V fd:%d", c->number, &addr, s);
        }

        }
#endif

        if (ngx_add_conn && (ngx_event_flags & NGX_USE_EPOLL_EVENT) == 0) { //如果是epoll,不会走到这里面去
            if (ngx_add_conn(c) == NGX_ERROR) {
                ngx_close_accepted_connection(c);
                return;
            }
        }

        log->data = NULL;
        log->handler = NULL;

        ls->handler(c);//ngx_http_init_connection

        if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {
            ev->available--;
        }

    } while (ev->available); //一次性读取所有当前的accept，直到accept返回NGX_EAGAIN，然后退出
}

/*
获得accept锁，多个worker仅有一个可以得到这把锁。获得锁不是阻塞过程，都是立刻返回，获取成功的话ngx_accept_mutex_held被置为1。
拿到锁，意味着监听句柄被放到本进程的epoll中了，如果没有拿到锁，则监听句柄会被从epoll中取出。 
*/ //尝试获取锁，如果获取了锁，那么还要将当前监听端口全部注册到当前worker进程的epoll当中去   
ngx_int_t
ngx_trylock_accept_mutex(ngx_cycle_t *cycle)
{
    if (ngx_shmtx_trylock(&ngx_accept_mutex)) {//

        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                       "accept mutex locked");

        //如果本来已经获得锁，则直接返回Ok   
        if (ngx_accept_mutex_held && ngx_accept_events == 0) {
            return NGX_OK;
        }

   //到达这里，说明重新获得锁成功，因此需要打开被关闭的listening句柄，调用ngx_enable_accept_events函数，将监听端口注册到当前worker进程的epoll当中去   
        if (ngx_enable_accept_events(cycle) == NGX_ERROR) {
            ngx_shmtx_unlock(&ngx_accept_mutex);
            return NGX_ERROR;
        }

        ngx_accept_events = 0;
        ngx_accept_mutex_held = 1; ////表示当前获取了锁   

        return NGX_OK;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                   "accept mutex lock failed: %ui", ngx_accept_mutex_held);

//这里表示的是以前曾经获取过，但是这次却获取失败了，那么需要将监听端口从当前的worker进程的epoll当中移除，调用的是ngx_disable_accept_events函数   
    if (ngx_accept_mutex_held) {
        if (ngx_disable_accept_events(cycle, 0) == NGX_ERROR) {
            return NGX_ERROR;
        }

        ngx_accept_mutex_held = 0;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_enable_accept_events(ngx_cycle_t *cycle)
{
    ngx_uint_t         i;
    ngx_listening_t   *ls;
    ngx_connection_t  *c;

    ls = cycle->listening.elts;

    /*
    注意:这里的循环会把所有的listening加入到了epoll中，那不是每个进程都会把所有的listening加入到epoll中吗，不是有惊群了吗?
    答案:实际上在本进程ngx_enable_accept_events把所有listen加入本进程epoll中后，本进程获取到ngx_accept_mutex锁后，在执行accept事件的
    过程中如果如果其他进程也开始ngx_trylock_accept_mutex，如果之前已经获取到锁，并把所有的listen添加到了epoll中，这时会因为没法获取到
    accept锁，而把之前加入到本进程，但没有accept过的时间全部清除。和ngx_disable_accept_events配合使用
    最终只有一个进程能accept到同一个客户端连接
     */
    for (i = 0; i < cycle->listening.nelts; i++) { 

        c = ls[i].connection;

        //后面的ngx_add_event->ngx_epoll_add_event中把listening中的c->read->active置1， ngx_epoll_del_event中把listening中置read->active置0
        if (c == NULL || c->read->active) { //之前本进程已经添加过，不用再加入epoll事件中，避免重复
            continue;
        }

        char tmpbuf[256];
        
        snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> epoll NGX_READ_EVENT(et) read add", NGX_FUNC_LINE);
        ngx_log_debug0(NGX_LOG_DEBUG_EVENT, cycle->log, 0, tmpbuf);
        if (ngx_add_event(c->read, NGX_READ_EVENT, 0) == NGX_ERROR) { //ngx_epoll_add_event
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_disable_accept_events(ngx_cycle_t *cycle, ngx_uint_t all)
{
    ngx_uint_t         i;
    ngx_listening_t   *ls;
    ngx_connection_t  *c;

    ls = cycle->listening.elts;
    for (i = 0; i < cycle->listening.nelts; i++) {

        c = ls[i].connection;

        if (c == NULL || !c->read->active) { //ngx_epoll_add_event中置1
            continue;
        }

#if (NGX_HAVE_REUSEPORT)

        /*
         * do not disable accept on worker's own sockets
         * when disabling accept events due to accept mutex
         */

        if (ls[i].reuseport && !all) {
            continue;
        }

#endif

        if (ngx_del_event(c->read, NGX_READ_EVENT, NGX_DISABLE_EVENT) //ngx_epoll_del_event
            == NGX_ERROR)
        {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}


static void
ngx_close_accepted_connection(ngx_connection_t *c)
{
    ngx_socket_t  fd;

    ngx_free_connection(c);

    fd = c->fd;
    c->fd = (ngx_socket_t) -1;

    if (ngx_close_socket(fd) == -1) {
        ngx_log_error(NGX_LOG_ALERT, c->log, ngx_socket_errno,
                      ngx_close_socket_n " failed");
    }

    if (c->pool) {
        ngx_destroy_pool(c->pool);
    }

#if (NGX_STAT_STUB)
    (void) ngx_atomic_fetch_add(ngx_stat_active, -1);
#endif
}


u_char *
ngx_accept_log_error(ngx_log_t *log, u_char *buf, size_t len)
{
    return ngx_snprintf(buf, len, " while accepting new connection on %V",
                        log->data);
}
