
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * ioctl(FIONBIO) sets a non-blocking mode with the single syscall
 * while fcntl(F_SETFL, O_NONBLOCK) needs to learn the current state
 * using fcntl(F_GETFL).
 *
 * ioctl() and fcntl() are syscalls at least in FreeBSD 2.x, Linux 2.2
 * and Solaris 7.
 *
 * ioctl() in Linux 2.4 and 2.6 uses BKL, however, fcntl(F_SETFL) uses it too.
 */


#if (NGX_HAVE_FIONBIO)

int
ngx_nonblocking(ngx_socket_t s)
{
    int  nb;

    nb = 1;

    return ioctl(s, FIONBIO, &nb);
}


int
ngx_blocking(ngx_socket_t s)
{
    int  nb;

    nb = 0;

    return ioctl(s, FIONBIO, &nb);
}

#endif


#if (NGX_FREEBSD)

int
ngx_tcp_nopush(ngx_socket_t s)
{
    int  tcp_nopush;

    tcp_nopush = 1;

    return setsockopt(s, IPPROTO_TCP, TCP_NOPUSH,
                      (const void *) &tcp_nopush, sizeof(int));
}

/*
Nagle算法的基本定义是任意时刻，最多只能有一个未被确认的小段。 所谓“小段”，指的是小于MSS尺寸的数据块，所谓“未被确认”，是指一
个数据块发送出去后，没有收到对方发送的ACK确认该数据已收到。
    Nagle算法的规则（可参考tcp_output.c文件里tcp_nagle_check函数注释）：
      （1）如果包长度达到MSS，则允许发送；
      （2）如果该包含有FIN，则允许发送；
      （3）设置了TCP_NODELAY选项，则允许发送；
      （4）未设置TCP_CORK选项时，若所有发出去的小数据包（包长度小于MSS）均被确认，则允许发送； 
      （5）上述条件都未满足，但发生了超时（一般为200ms），则立即发送。
Nagle算法只允许一个未被ACK的包存在于网络，它并不管包的大小，因此它事实上就是一个扩展的停-等协议，只不过它是基于包停-等的，而不
是基于字节停-等的。Nagle算法完全由TCP协议的ACK机制决定，这会带来一些问题，比如如果对端ACK回复很快的话，Nagle事实上不会拼接太多
的数据包，虽然避免了网络拥塞，网络总体的利用率依然很低。


TCP链接的过程中，默认开启Nagle算法，进行小包发送的优化。
2. TCP_NODELAY 选项
    默认情况下，发送数据采用Negale 算法。这样虽然提高了网络吞吐量，但是实时性却降低了，在一些交互性很强的应用程序来说是不
    允许的，使用TCP_NODELAY选项可以禁止Negale 算法。
    此时，应用程序向内核递交的每个数据包都会立即发送出去。需要注意的是，虽然禁止了Negale 算法，但网络的传输仍然受到TCP确认延迟机制的影响。
3. TCP_CORK 选项 (tcp_nopush = on 会设置调用tcp_cork方法，配合sendfile 选项仅在使用sendfile的时候才开启)
    所谓的CORK就是塞子的意思，形象地理解就是用CORK将连接塞住，使得数据先不发出去，等到拔去塞子后再发出去。设置该选项后，内核
    会尽力把小数据包拼接成一个大的数据包（一个MTU）再发送出去，当然若一定时间后（一般为200ms，该值尚待确认），内核仍然没有组
    合成一个MTU时也必须发送现有的数据（不可能让数据一直等待吧）。
    然而，TCP_CORK的实现可能并不像你想象的那么完美，CORK并不会将连接完全塞住。内核其实并不知道应用层到底什么时候会发送第二批
    数据用于和第一批数据拼接以达到MTU的大小，因此内核会给出一个时间限制，在该时间内没有拼接成一个大包（努力接近MTU）的话，内
    核就会无条件发送。也就是说若应用层程序发送小包数据的间隔不够短时，TCP_CORK就没有一点作用，反而失去了数据的实时性（每个小
    包数据都会延时一定时间再发送）。

4. Nagle算法与CORK算法区别
  Nagle算法和CORK算法非常类似，但是它们的着眼点不一样，Nagle算法主要避免网络因为太多的小包（协议头的比例非常之大）而拥塞，而CORK
  算法则是为了提高网络的利用率，使得总体上协议头占用的比例尽可能的小。如此看来这二者在避免发送小包上是一致的，在用户控制的层面上，
  Nagle算法完全不受用户socket的控制，你只能简单的设置TCP_NODELAY而禁用它，CORK算法同样也是通过设置或者清除TCP_CORK使能或者禁用之，
  然而Nagle算法关心的是网络拥塞问题，只要所有的ACK回来则发包，而CORK算法却可以关心内容，在前后数据包发送间隔很短的前提下（很重要，
  否则内核会帮你将分散的包发出），即使你是分散发送多个小数据包，你也可以通过使能CORK算法将这些内容拼接在一个包内，如果此时用Nagle
  算法的话，则可能做不到这一点。

  naggle(tcp_nodelay设置)算法，只要发送出去一个包，并且受到协议栈ACK应答，内核就会继续把缓冲区的数据发送出去。
  core(tcp_core设置)算法，受到对方应答后，内核首先检查当前缓冲区中的包是否有1500，如果有则直接发送，如果受到应答的时候还没有1500，则
  等待200ms，如果200ms内还没有1500字节，则发送
*/ //参考http://m.blog.csdn.net/blog/c_cyoxi/8673645
int
ngx_tcp_push(ngx_socket_t s)
{
    int  tcp_nopush;

    tcp_nopush = 0;

    return setsockopt(s, IPPROTO_TCP, TCP_NOPUSH,
                      (const void *) &tcp_nopush, sizeof(int));
}

#elif (NGX_LINUX)  //linux情况nginx使用NGX_CORK  选项仅在使用sendfile的时候才开启  见配置tcp_nopush on | off;


/*

Nagle算法的基本定义是任意时刻，最多只能有一个未被确认的小段。 所谓“小段”，指的是小于MSS尺寸的数据块，所谓“未被确认”，是指一
个数据块发送出去后，没有收到对方发送的ACK确认该数据已收到。
    Nagle算法的规则（可参考tcp_output.c文件里tcp_nagle_check函数注释）：
      （1）如果包长度达到MSS，则允许发送；
      （2）如果该包含有FIN，则允许发送；
      （3）设置了TCP_NODELAY选项，则允许发送；
      （4）未设置TCP_CORK选项时，若所有发出去的小数据包（包长度小于MSS）均被确认，则允许发送； 
      （5）上述条件都未满足，但发生了超时（一般为200ms），则立即发送。
Nagle算法只允许一个未被ACK的包存在于网络，它并不管包的大小，因此它事实上就是一个扩展的停-等协议，只不过它是基于包停-等的，而不
是基于字节停-等的。Nagle算法完全由TCP协议的ACK机制决定，这会带来一些问题，比如如果对端ACK回复很快的话，Nagle事实上不会拼接太多
的数据包，虽然避免了网络拥塞，网络总体的利用率依然很低。


TCP链接的过程中，默认开启Nagle算法，进行小包发送的优化。
2. TCP_NODELAY 选项
    默认情况下，发送数据采用Negale 算法。这样虽然提高了网络吞吐量，但是实时性却降低了，在一些交互性很强的应用程序来说是不
    允许的，使用TCP_NODELAY选项可以禁止Negale 算法。
    此时，应用程序向内核递交的每个数据包都会立即发送出去。需要注意的是，虽然禁止了Negale 算法，但网络的传输仍然受到TCP确认延迟机制的影响。
3. TCP_CORK 选项  选项仅在使用sendfile的时候才开启
    所谓的CORK就是塞子的意思，形象地理解就是用CORK将连接塞住，使得数据先不发出去，等到拔去塞子后再发出去。设置该选项后，内核
    会尽力把小数据包拼接成一个大的数据包（一个MTU）再发送出去，当然若一定时间后（一般为200ms，该值尚待确认），内核仍然没有组
    合成一个MTU时也必须发送现有的数据（不可能让数据一直等待吧）。
    然而，TCP_CORK的实现可能并不像你想象的那么完美，CORK并不会将连接完全塞住。内核其实并不知道应用层到底什么时候会发送第二批
    数据用于和第一批数据拼接以达到MTU的大小，因此内核会给出一个时间限制，在该时间内没有拼接成一个大包（努力接近MTU）的话，内
    核就会无条件发送。也就是说若应用层程序发送小包数据的间隔不够短时，TCP_CORK就没有一点作用，反而失去了数据的实时性（每个小
    包数据都会延时一定时间再发送）。

4. Nagle算法与CORK算法区别
  Nagle算法和CORK算法非常类似，但是它们的着眼点不一样，Nagle算法主要避免网络因为太多的小包（协议头的比例非常之大）而拥塞，而CORK
  算法则是为了提高网络的利用率，使得总体上协议头占用的比例尽可能的小。如此看来这二者在避免发送小包上是一致的，在用户控制的层面上，
  Nagle算法完全不受用户socket的控制，你只能简单的设置TCP_NODELAY而禁用它，CORK算法同样也是通过设置或者清除TCP_CORK使能或者禁用之，
  然而Nagle算法关心的是网络拥塞问题，只要所有的ACK回来则发包，而CORK算法却可以关心内容，在前后数据包发送间隔很短的前提下（很重要，
  否则内核会帮你将分散的包发出），即使你是分散发送多个小数据包，你也可以通过使能CORK算法将这些内容拼接在一个包内，如果此时用Nagle
  算法的话，则可能做不到这一点。

  naggle(tcp_nodelay设置)算法，只要发送出去一个包，并且受到应答，内核就会继续把缓冲区的数据发送出去。
  core(tcp_core设置)算法，受到对方应答后，内核首先检查当前缓冲区中的包是否有1500，如果有则直接发送，如果受到应答的时候还没有1500，则
  等待200ms，如果200ms内还没有1500字节，则发送
*/ //参考http://m.blog.csdn.net/blog/c_cyoxi/8673645  http://blog.csdn.net/zmj_88888888/article/details/9169227

int
ngx_tcp_nopush(ngx_socket_t s)
{
    int  cork;

    cork = 1;

    return setsockopt(s, IPPROTO_TCP, TCP_CORK,
                      (const void *) &cork, sizeof(int)); //选项仅在使用sendfile的时候才开启
}


int
ngx_tcp_push(ngx_socket_t s)//选项仅在使用sendfile的时候才开启
{
    int  cork;

    cork = 0;

    return setsockopt(s, IPPROTO_TCP, TCP_CORK,
                      (const void *) &cork, sizeof(int));
}

#else

int
ngx_tcp_nopush(ngx_socket_t s)
{
    return 0;
}


int
ngx_tcp_push(ngx_socket_t s)
{
    return 0;
}

#endif
