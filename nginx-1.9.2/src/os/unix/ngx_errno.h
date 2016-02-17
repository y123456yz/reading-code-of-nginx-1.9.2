
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_ERRNO_H_INCLUDED_
#define _NGX_ERRNO_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef int               ngx_err_t;

#define NGX_EPERM         EPERM
#define NGX_ENOENT        ENOENT //参数file_name指定的文件不存在
#define NGX_ENOPATH       ENOENT
#define NGX_ESRCH         ESRCH
#define NGX_EINTR         EINTR
#define NGX_ECHILD        ECHILD
#define NGX_ENOMEM        ENOMEM
#define NGX_EACCES        EACCES
#define NGX_EBUSY         EBUSY
#define NGX_EEXIST        EEXIST
#define NGX_EXDEV         EXDEV
#define NGX_ENOTDIR       ENOTDIR
#define NGX_EISDIR        EISDIR
#define NGX_EINVAL        EINVAL
#define NGX_ENFILE        ENFILE
#define NGX_EMFILE        EMFILE
#define NGX_ENOSPC        ENOSPC
#define NGX_EPIPE         EPIPE
#define NGX_EINPROGRESS   EINPROGRESS
#define NGX_ENOPROTOOPT   ENOPROTOOPT
#define NGX_EOPNOTSUPP    EOPNOTSUPP
#define NGX_EADDRINUSE    EADDRINUSE
#define NGX_ECONNABORTED  ECONNABORTED
#define NGX_ECONNRESET    ECONNRESET
#define NGX_ENOTCONN      ENOTCONN
#define NGX_ETIMEDOUT     ETIMEDOUT
#define NGX_ECONNREFUSED  ECONNREFUSED
#define NGX_ENAMETOOLONG  ENAMETOOLONG
#define NGX_ENETDOWN      ENETDOWN
#define NGX_ENETUNREACH   ENETUNREACH
#define NGX_EHOSTDOWN     EHOSTDOWN
#define NGX_EHOSTUNREACH  EHOSTUNREACH
#define NGX_ENOSYS        ENOSYS
#define NGX_ECANCELED     ECANCELED
#define NGX_EILSEQ        EILSEQ
#define NGX_ENOMOREFILES  0
#define NGX_ELOOP         ELOOP
#define NGX_EBADF         EBADF

#if (NGX_HAVE_OPENAT)
#define NGX_EMLINK        EMLINK
#endif

/*
EWOULDBLOCK用于非阻塞模式，不需要重新读或者写
另外，如果出现EINTR即errno为4，错误描述Interrupted system call，操作也应该继续。
EINTR指操作被中断唤醒，需要重新读/写

在Linux环境下开发经常会碰到很多错误(设置errno)，其中EAGAIN是其中比较常见的一个错误(比如用在非阻塞操作中)。
从字面上来看，是提示再试一次。这个错误经常出现在当应用程序进行一些非阻塞(non-blocking)操作(对文件或socket)的时候。
例如，以 O_NONBLOCK的标志打开文件/socket/FIFO，如果你连续做read操作而没有数据可读。此时程序不会阻塞起来等待数据准备就绪返回，
read函数会返回一个错误EAGAIN，提示你的应用程序现在没有数据可读请稍后再试。

在Linux中使用非阻塞的socket的情形下。 
（一）发送时

　　当客户通过Socket提供的send函数发送大的数据包时，就可能返回一个EAGAIN的错误。该错误产生的原因是由于send 函数中的size变量大小超过了
tcp_sendspace的值。tcp_sendspace定义了应用在调用send之前能够在kernel中缓存的数据量。当应用程序在socket中设置了O_NDELAY或者O_NONBLOCK属性后，
如果发送缓存被占满，send就会返回EAGAIN的错误。 

　　为了消除该错误，有三种方法可以选择： 
　　1.调大tcp_sendspace，使之大于send中的size参数 
　　---no -p -o tcp_sendspace=65536 

　　2.在调用send前，在setsockopt函数中为SNDBUF设置更大的值 

　　3.使用write替代send，因为write没有设置O_NDELAY或者O_NONBLOCK

（二）接收时

       接收数据时常遇到Resource temporarily unavailable的提示，errno代码为11(EAGAIN)。这表明你在非阻塞模式下调用了阻塞操作，
       在该操作没有完成就返回这个错误，这个错误不会破坏socket的同步，不用管它，下次循环接着recv就可以。对非阻塞socket而言，
       EAGAIN不是一种错误。在VxWorks和Windows上，EAGAIN的名字叫做EWOULDBLOCK。其实这算不上错误，只是一种异常而已。

　　另外，如果出现EINTR即errno为4，错误描述Interrupted system call，操作也应该继续。
　　最后，如果recv的返回值为0，那表明对方已将连接断开，我们的接收操作也应该结束。

（三）以下是另一种解释

假如发送端流量大于接收端的流量(意思是epoll所在的程序读比转发的socket要快),由于是非阻塞的socket,那么send()函数虽然返回,但实际缓冲
区的数据并未真正发给接收端,这样不断的读和发，当缓冲区满后会产生EAGAIN错误(参考man send),同时,不理会这次请求发送的数据.
*/

#if (__hpux__)
#define NGX_EAGAIN        EWOULDBLOCK
#else
#define NGX_EAGAIN        EAGAIN //11
#endif


#define ngx_errno                  errno
#define ngx_socket_errno           errno
#define ngx_set_errno(err)         errno = err
#define ngx_set_socket_errno(err)  errno = err


u_char *ngx_strerror(ngx_err_t err, u_char *errstr, size_t size);
ngx_int_t ngx_strerror_init(void);


#endif /* _NGX_ERRNO_H_INCLUDED_ */
