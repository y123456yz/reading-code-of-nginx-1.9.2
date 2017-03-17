
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_CORE_H_INCLUDED_
#define _NGX_HTTP_CORE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#if (NGX_THREADS)
#include <ngx_thread_pool.h>
#endif


#define NGX_HTTP_GZIP_PROXIED_OFF       0x0002
#define NGX_HTTP_GZIP_PROXIED_EXPIRED   0x0004
#define NGX_HTTP_GZIP_PROXIED_NO_CACHE  0x0008
#define NGX_HTTP_GZIP_PROXIED_NO_STORE  0x0010
#define NGX_HTTP_GZIP_PROXIED_PRIVATE   0x0020
#define NGX_HTTP_GZIP_PROXIED_NO_LM     0x0040
#define NGX_HTTP_GZIP_PROXIED_NO_ETAG   0x0080
#define NGX_HTTP_GZIP_PROXIED_AUTH      0x0100
#define NGX_HTTP_GZIP_PROXIED_ANY       0x0200


#define NGX_HTTP_AIO_OFF                0 //aio off
#define NGX_HTTP_AIO_ON                 1 //aio on
#define NGX_HTTP_AIO_THREADS            2 //aio thread

/*
相对于NGX HTTP ACCESS PHASE阶段处理方法，satisfy配置项参数的意义
┏━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃satisfy的参数 ┃    意义                                                                              ┃
┣━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃              ┃    NGX HTTP ACCESS PHASE阶段可能有很多HTTP模块都对控制请求的访问权限感兴趣，         ┃
┃              ┃那么以哪一个为准呢？当satisfy的参数为all时，这些HTTP模块必须同时发生作用，即以该阶    ┃
┃all           ┃                                                                                      ┃
┃              ┃段中全部的handler方法共同决定请求的访问权限，换句话说，这一阶段的所有handler方法必    ┃
┃              ┃须全部返回NGX OK才能认为请求具有访问权限                                              ┃
┣━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃              ┃  与all相反，参数为any时意味着在NGX—HTTP__ ACCESS—PHASE阶段只要有任意一个             ┃
┃              ┃HTTP模块认为请求合法，就不用再调用其他HTTP模块继续检查了，可以认为请求是具有访问      ┃
┃              ┃权限的。实际上，这时的情况有些复杂：如果其中任何一个handler方法返回NGX二OK，则认为    ┃
┃              ┃请求具有访问权限；如果某一个handler方法返回403戎者401，则认为请求没有访问权限，还     ┃
┃any           ┃                                                                                      ┃
┃              ┃需要检查NGX—HTTP—ACCESS—PHASE阶段的其他handler方法。也就是说，any配置项下任           ┃
┃              ┃何一个handler方法一旦认为请求具有访问权限，就认为这一阶段执行成功，继续向下执行；如   ┃
┃              ┃果其中一个handler方法认为没有访问权限，则未必以此为准，还需要检测其他的hanlder方法。  ┃
┃              ┃all和any有点像“&&”和“¨”的关系                                                         ┃
┗━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
*/
#define NGX_HTTP_SATISFY_ALL            0
#define NGX_HTTP_SATISFY_ANY            1


#define NGX_HTTP_LINGERING_OFF          0
#define NGX_HTTP_LINGERING_ON           1
#define NGX_HTTP_LINGERING_ALWAYS       2


#define NGX_HTTP_IMS_OFF                0
#define NGX_HTTP_IMS_EXACT              1
#define NGX_HTTP_IMS_BEFORE             2


#define NGX_HTTP_KEEPALIVE_DISABLE_NONE    0x0002
#define NGX_HTTP_KEEPALIVE_DISABLE_MSIE6   0x0004
#define NGX_HTTP_KEEPALIVE_DISABLE_SAFARI  0x0008


typedef struct ngx_http_location_tree_node_s  ngx_http_location_tree_node_t;
typedef struct ngx_http_core_loc_conf_s  ngx_http_core_loc_conf_t;

//通过ngx_http_core_listen中的参数配置   通过server{}"listen"参数进行设置下面的各项
typedef struct { 
    union {
        struct sockaddr        sockaddr;
        struct sockaddr_in     sockaddr_in;
#if (NGX_HAVE_INET6)
        struct sockaddr_in6    sockaddr_in6;
#endif
#if (NGX_HAVE_UNIX_DOMAIN)
        struct sockaddr_un     sockaddr_un;
#endif
        u_char                 sockaddr_data[NGX_SOCKADDRLEN];
    } u;

    socklen_t                  socklen;
    //一般在设备listen中设置了bind setfib, backlog, rcvbuf, sndbuf, accept_filter, deferred, ipv6only, or so_keepalive这些参数后会把bind和set一起置1
    unsigned                   set:1;

    /*
     如果指令有default参数，那么这个server块将是通过“地址:端口”来进行访问的默认服务器，这对于你想为那些不匹配server_name指令中的
     主机名指定默认server块的虚拟主机（基于域名）非常有用，如果没有指令带有default参数，那么默认服务器将使用第一个server块。 
     */
    unsigned                   default_server:1; //如果设置了bind 带参数的时候default|default_server，置1，见ngx_http_core_listen
/*
    instructs to make a separate bind() call for a given address:port pair. This is useful because if there are several listen 
directives with the same port but different addresses, and one of the listen directives listens on all addresses for the 
given port (*:port), nginx will bind() only to *:port. It should be noted that the getsockname() system call will be made 
in this case to determine the address that accepted the connection. If the setfib, backlog, rcvbuf, sndbuf, accept_filter, 
deferred, ipv6only, or so_keepalive parameters are used then for a given address:port pair a separate bind() call will always be made. 
*///
/*
当有多个server{}块，例如第一个bind 1.1.1.1:1 第二个server bind 2.2.2.2:2  第三个server bind *:80，则如果不加bind参数，则所有的连接
都会连接到第三个，从而获取到连接后需要使用getsockname来判断，如果每个都加上bind的话，就不用再判断了。如果是连接1.1.1.1:1的会到第一个bind
2.2.2.2:2的到第二个bind，其他的到第三个。
一般在设备listen中设置了bind setfib, backlog, rcvbuf, sndbuf, accept_filter, deferred, ipv6only, or so_keepalive(这些选项只有在bind后才能setsockops)这些参数后会把bind和set一起置1
*/ //见ngx_http_core_listen
    unsigned                   bind:1; //listen *:80；配置项的通配符也可能在ngx_http_init_listening中置1
    unsigned                   wildcard:1;
#if (NGX_HTTP_SSL)
/*
被指定这个参数的listen将被允许工作在SSL模式，这将允许服务器同时工作在HTTP和HTTPS两种协议下，例如：
    listen 80;
    listen 443 default ssl;
*/
    unsigned                   ssl:1;
#endif
#if (NGX_HTTP_V2)
    unsigned                   http2:1; //listen是否启用http2配置，例如listen 443 ssl http2;
#endif
#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
    unsigned                   ipv6only:1;
#endif
#if (NGX_HAVE_REUSEPORT)
    unsigned                   reuseport:1; //端口复用
#endif
    unsigned                   so_keepalive:2; //listen配置项带上so_keepalive参数时置1，见ngx_http_core_listen 打开取值1 off关闭取值2
    unsigned                   proxy_protocol:1; //见ngx_http_core_listen   配置类似listen ip:port  proxy_protocol的时候置1

    int                        backlog;
    int                        rcvbuf;
    int                        sndbuf;
#if (NGX_HAVE_SETFIB)
    int                        setfib;
#endif
#if (NGX_HAVE_TCP_FASTOPEN)
    int                        fastopen;
#endif
#if (NGX_HAVE_KEEPALIVE_TUNABLE)
    int                        tcp_keepidle;
    int                        tcp_keepintvl;
    int                        tcp_keepcnt;
#endif

#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
    char                      *accept_filter;
#endif
#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
    ngx_uint_t                 deferred_accept;
#endif

    u_char                     addr[NGX_SOCKADDR_STRLEN + 1]; //服务器端监听的IP:port字符串格式，见ngx_http_core_server
} ngx_http_listen_opt_t;

/*
    NGX_HTTP_TRY_FILES_PHASE阶段:

    这是一个核心HTTP阶段，可以说大部分HTTP模块都会在此阶段重新定义Nginx服务器的行为，如第3章中提到的mytest模块。NGX_HTTP_CONTENT_PHASE
阶段之所以被众多HTTP模块“钟爱”，主要基于以下两个原因：
    其一，以上9个阶段主要专注于4件基础性工作：rewrite重写URL、找到location配置块、判断请求是否具备访问权限、try_files功能优先读取静态资源文件，
这4个工作通常适用于绝大部分请求，因此，许多HTTP模块希望可以共享这9个阶段中已经完成的功能。

    其二，NGX_HTTP_CONTENT_PHASE阶段与其他阶段都不相同的是，它向HTTP模块提供了两种介入该阶段的方式：第一种与其他10个阶段一样，
通过向全局的ngx_http_core_main_conf_t结构体的phases数组中添加ngx_http_handler_pt赴理方法来实现，而第二种是本阶段独有的，把希望处理请求的
ngx_http_handler_pt方法设置到location相关的ngx_http_core_loc_conf_t结构体的handler指针中，这正是第3章中mytest例子的用法。

    上面所说的第一种方式，也是HTTP模块介入其他10个阶段的唯一方式，是通过在必定会被调用的postconfiguration方法向全局的
ngx_http_core_main_conf_t结构体的phases[NGX_HTTP_CONTENT_PHASE]动态数组添加ngx_http_handler_pt处理方法来达成的，这个处理方法将会应用于全部的HTTP请求。

    而第二种方式是通过设置ngx_http_core_loc_conf_t结构体的handler指针来实现的，每一个location都对应着一个独立的ngx_http_core_loc_conf结
构体。这样，我们就不必在必定会被调用的postconfiguration方法中添加ngx_http_handler_pt处理方法了，而可以选挥在ngx_command_t的某个配置项
（如第3章中的mytest配置项）的回调方法中添加处理方法，将当前location块所属的ngx_http_core- loc—conf_t结构体中的handler设置为
ngx_http_handler_pt处理方法。这样做的好处是，ngx_http_handler_pt处理方法不再应用于所有的HTTP请求，仅仅当用户请求的URI匹配了location时
(也就是mytest配置项所在的location)才会被调用。

    这也就意味着它是一种完全不同于其他阶段的使用方式。 因此，当HTTP模块实现了某个ngx_http_handler_pt处理方法并希望介入NGX_HTTP_CONTENT_PHASE阶
段来处理用户请求时，如果希望这个ngx_http_handler_pt方法应用于所有的用户请求，则应该在ngx_http_module_t接口的postconfiguration方法中，
向ngx_http_core_main_conf_t结构体的phases[NGX_HTTP_CONTENT_PHASE]动态数组中添加ngx_http_handler_pt处理方法；反之，如果希望这个方式
仅应用于URI匹配丁某些location的用户请求，则应该在一个location下配置项的回调方法中，把ngx_http_handler_pt方法设置到ngx_http_core_loc_conf_t
结构体的handler中。
    注意ngx_http_core_loc_conf_t结构体中仅有一个handler指针，它不是数组，这也就意味着如果采用上述的第二种方法添加ngx_http_handler_pt处理方法，
那么每个请求在NGX_HTTP_CONTENT PHASE阶段只能有一个ngx_http_handler_pt处理方法。而使用第一种方法时是没有这个限制的，NGX_HTTP_CONTENT_PHASE阶
段可以经由任意个HTTP模块处理。

    当同时使用这两种方式设置ngx_http_handler_pt处理方法时，只有第二种方式设置的ngx_http_handler_pt处理方法才会生效，也就是设置
handler指针的方式优先级更高，而第一种方式设置的ngx_http_handler_pt处理方法将不会生效。如果一个location配置块内有多个HTTP模块的
配置项在解析过程都试图按照第二种方式设置ngx_http_handler_pt赴理方法，那么后面的配置项将有可能覆盖前面的配置项解析时对handler指针的设置。

NGX_HTTP_CONTENT_PHASE阶段的checker方法是ngx_http_core_content_phase。ngx_http_handler_pt处理方法的返回值在以上两种方式下具备了不同意义。
    在第一种方式下，ngx_http_handler_pt处理方法无论返回任何值，都会直接调用ngx_http_finalize_request方法结束请求。当然，
ngx_http_finalize_request方法根据返回值的不同未必会直接结束请求，这在第11章中会详细介绍。

    在第二种方式下，如果ngx_http_handler_pt处理方法返回NGX_DECLINED，将按顺序向后执行下一个ngx_http_handler_pt处理方法；如果返回其他值，
则调用ngx_http_finalize_request方法结束请求。
*/

/*
┏━━━━┳━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┓
┃序号    ┃    阶段宏名                  ┃    阶段简单描述            ┃
┣━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━┫
┃    O   ┃NGX HTTP POST READ PHASE      ┃请求头读取完成之后的阶段    ┃
┣━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━┫
┃    1   ┃NGX_HTTP_SERVER_REWRITEPHASE  ┃ Server内请求地址重写阶段   ┃
┣━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━┫
┃    2   ┃NGX_HTTP_FIND_CONFIG_PHASE    ┃配置查找阶段                ┃
┣━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━┫
┃    3   ┃NGX_HTTP_REWRITE_PHASE        ┃ Location内请求地址重写阶段 ┃
┣━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━┫
┃    4   ┃NGX HTTP POST REWRITE PHASE   ┃请求地址重写完成之后的阶段  ┃
┣━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━┫
┃    5   ┃NGXHTTPPREACCESS_PHASE        ┃访问权限检查准备阶段        ┃
┣━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━┫
┃    6   ┃NGX HTTP ACCESS PHASE         ┃访问权限检查阶段            ┃
┣━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━┫
┃    7   ┃NGX HTTP POST ACCESS PHASE    ┃访问权隈检查完成之后的阶段  ┃
┣━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━┫
┃    8   ┃NGX_HTTP TRY FILES PHASE      ┃配置项try_fles处理阶段      ┃
┣━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━┫
┃    9   ┃NGX HTTP CONTENT PHASE        ┃内容产生阶段                ┃
┣━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━┫
┃    10  ┃NGX HTTP LOG PHASE            ┃日志模块处理阶段            ┃
┗━━━━┻━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━┛

NGX_HTTP_SERVER_REWRITEPHASE阶段，和第3阶段NGX_HTTP_REWRITE_PHASE都属于地址重写，也都是针对rewrite模块而设定的阶段，前者用于server上下文里的
地址重写，而后者用于location上下文里的地址重写。为什么要设置两个地址重写阶段，原因在于rewrite模块的相关指令（比如rewrite、if、set等）既
可用于server上下文．又可用于location上下文。在客户端请求被Nginx接收后，首先做server查找与定位，在定位到server（如果没
查找到就是默认server，见ngx_http_init_connection  ngx_http_set_virtual_server）后执行NGX_HTTP_SERVER_REWRITEPHASE阶段上的回调函数
*/

/*
    注意ngx_http_phases定义的11个阶段是有顺序的，必须按照其定义的顺序执行。同时也要意识到，并不是说一个用户请求最多只能经过11个
HTTP模块提供的ngx_http_handler_pt方法来处理，NGX_HTTP_POST_READ_PHASE、NGX_HTTP_SERVER_REWRITE_PHASE、NGX_HTTP_REWRITE_PHASE、
NGX_HTTP_PREACCESS_PHASE、NGX_HTTP_ACCESS_PHASE、NGX HTTP_CONTENT_PHASE、NGX_HTTP_LOG_PHASE这7个阶段可以包括任意多个处理方法，
它们是可以同时作用于同一个用户请求的。而NGX_HTTP_FIND_CONFIG_PHASE、NGX_HTTP_POSTREWRITE_PHASE、NGX_HTTP_POST_ACCESS_PHASE、
NGX_HTTP_TRY_FILES_PHASE这4个阶段则不允许HTTP模块加入自己的ngx_http_handler_pt方法处理用户请求，它们仅由HTTP框架实现。

NGX_HTTP_FIND_CONFIG_PHASE、NGX_HTTP_POSTREWRITE_PHASE、NGX_HTTP_POST_ACCESS_PHASE、NGX_HTTP_TRY_FILES_PHASE这4个阶段则不允许HTTP模块
加入自己的ngx_http_handler_pt方法处理用户请求,但是他们的会占用cmcf->phase_engine.handlers[]数组中的一个成员，见ngx_http_init_phase_handlers
*/
typedef enum { //各个阶段的http框架check函数见ngx_http_init_phase_handlers           //所有阶段的checker在ngx_http_core_run_phases中调用
    //在接收到完整的HTTP头部后处理的HTTP阶段   主要是获取客户端真实IP，因为客户端到nginx可能通过了vanish等缓存，
    //ngx_http_realip_module(ngx_http_realip_init->ngx_http_realip_handler)
    NGX_HTTP_POST_READ_PHASE = 0, //该阶段方法有:ngx_http_realip_handler  POST有"在....后"的意思，POST_READ应该就是在解析完请求行和头部行后



    /*在还没有查询到URI匹配的location前，这时rewrite重写URL也作为一个独立的HTTP阶段   Server内请求地址重写阶段 */
    NGX_HTTP_SERVER_REWRITE_PHASE, //该阶段handler方法有:ngx_http_rewrite_module(ngx_http_rewrite_init->ngx_http_rewrite_handler) 

    /*根据URI寻找匹配的location，这个阶段通常由ngx_http_core_module模块实现，不建议其他HTTP模块重新定义这一阶段的行为*/
    NGX_HTTP_FIND_CONFIG_PHASE,//该阶段handler方法有:无，不允许用户添加hander方法在该阶段  该阶段完成的是Nginx的特定任务，即进行Location定位

    //在NGX_HTTP_FIND_CONFIG_PHASE阶段寻找到匹配的location之后再修改请求的URI  Location内请求地址重写阶段
    /*在NGX_HTTP_FIND_CONFIG_PHASE阶段之后重写URL的意义与NGX_HTTP_SERVER_REWRITE_PHASE阶段显然是不同的，因为这两者会导致查找到不同的location块（location是与URI进行匹配的）*/
    NGX_HTTP_REWRITE_PHASE,//该阶段handler方法有:ngx_http_rewrite_handler
    /*这一阶段是用于在rewrite重写URL后重新跳到NGX_HTTP_FIND_CONFIG_PHASE阶段，找到与新的URI匹配的location。所以，这一阶段是无法由第三方HTTP模块处理的，而仅由ngx_http_core_module模块使用*/
    /*
    这一阶段是用于在rewrite重写URL后，防止错误的nginx．conf配置导致死循环（递归地修改URI），因此，这一阶段仅由ngx_http_core_module模块处理。
目前，控制死循环的方式很简单，首先检查rewrite的次数，如果一个请求超过10次重定向，扰认为进入了rewrite死循环，这时在NGX_HTTP_POSTREWRITE_PHASE
阶段就会向用户返回500，表示服务器内部错误
     */
    NGX_HTTP_POST_REWRITE_PHASE,//该阶段handler方法有:无，不允许用户添加hander方法在该阶段

    /*
    NGXHTTPPREACCESSPHASE、NGX_HTTP_ACCESS_PHASE、NGX HTTPPOST_ACCESS_PHASE，很好理解，做访问权限检查的前期、中期、后期工作，
其中后期工作是固定的，判断前面访问权限检查的结果（状态码存故在字段r->access_code内），如果当前请求没有访问权限，那么直接返回状
态403错误，所以这个阶段也无法去挂载额外的回调函数。
     */

    //处理NGX_HTTP_ACCESS_PHASE阶段前，HTTP模块可以介入的处理阶段
    NGX_HTTP_PREACCESS_PHASE,//该阶段handler方法有:ngx_http_degradation_handler  ngx_http_limit_conn_handler  ngx_http_limit_req_handler ngx_http_realip_handler

     /*这个阶段用于让HTTP模块判断是否允许这个请求访问Nginx服务器*/
    NGX_HTTP_ACCESS_PHASE,//该阶段handler方法有:ngx_http_access_handler  ngx_http_auth_basic_handler  ngx_http_auth_request_handler
    /*当NGX_HTTP_ACCESS_PHASE阶段中HTTP模块的handler处理方法返回不允许访问的错误码时（实际是NGX_HTTP_FORBIDDEN或者NGX_HTTP_UNAUTHORIZED），
    这个阶段将负责构造拒绝服务的用户响应。所以，这个阶段实际上用于给NGX_HTTP_ACCESS_PHASE阶段收尾*/
    NGX_HTTP_POST_ACCESS_PHASE,//该阶段handler方法有:无，不允许用户添加hander方法在该阶段



    /*这个阶段完全是为了try_files配置项而设立的。当HTTP请求访问静态文件资源时，try_files配置项可以使这个请求顺序地访问多个静态文件资源，
    如果某一次访问失败，则继续访问try_files中指定的下一个静态资源。另外，这个功能完全是在NGX_HTTP_TRY_FILES_PHASE阶段中实现的*/
    NGX_HTTP_TRY_FILES_PHASE,//该阶段handler方法有:无，不允许用户添加hander方法在该阶段

    /* 其余10个阶段中各HTTP模块的处理方法都是放在全局的ngx_http_core_main_conf_t结构体中的，也就是说，它们对任何一个HTTP请求都是有效的 
    NGX_HTTP_CONTENT_PHASE仅仅针对某种请求唯一生效

    ngx_http_handler_pt处理方法不再应用于所有的HTTP请求，仅仅当用户请求的URI匹配了location时(也就是mytest配置项所在的location)才会被调用。
这也就意味着它是一种完全不同于其他阶段的使用方式。 因此，当HTTP模块实现了某个ngx_http_handler_pt处理方法并希望介入NGX_HTTP_CONTENT_PHASE阶
段来处理用户请求时，如果希望这个ngx_http_handler_pt方法应用于所有的用户请求，则应该在ngx_http_module_t接口的postconfiguration方法中，
向ngx_http_core_main_conf_t结构体的phases[NGX_HTTP_CONTENT_PHASE]动态数组中添加ngx_http_handler_pt处理方法；反之，如果希望这个方式
仅应用于URI匹配丁某些location的用户请求，则应该在一个location下配置项的回调方法中，把ngx_http_handler_pt方法设置到ngx_http_core_loc_conf_t
结构体的handler中。
    注意ngx_http_core_loc_conf_t结构体中仅有一个handler指针，它不是数组，这也就意味着如果采用上述的第二种方法添加ngx_http_handler_pt处理方法，
那么每个请求在NGX_HTTP_CONTENT PHASE阶段只能有一个ngx_http_handler_pt处理方法。而使用第一种方法时是没有这个限制的，NGX_HTTP_CONTENT_PHASE阶
段可以经由任意个HTTP模块处理。
    */
    //用于处理HTTP请求内容的阶段，这是大部分HTTP模块最喜欢介入的阶段  //CONTENT_PHASE阶段的处理回调函数ngx_http_handler_pt比较特殊，见ngx_http_core_content_phase 
    NGX_HTTP_CONTENT_PHASE, //该阶段handler方法有:ngx_http_autoindex_handler  ngx_http_dav_handler ngx_http_gzip_static_handler  ngx_http_index_handler ngx_http_random_index_handler ngx_http_static_handler
    
    /*处理完请求后记录日志的阶段。例如，ngx_http_log_module模块就在这个阶段中加入了一个handler处理方法，使得每个HTTP请求处理完毕后会记录access_log日志*/
    NGX_HTTP_LOG_PHASE //该阶段handler方法有: ngx_http_log_handler
} ngx_http_phases; 

typedef struct ngx_http_phase_handler_s  ngx_http_phase_handler_t;
//一个HTTP处理阶段中的checker检查方法，仅可以由HTTP框架实现，以此控制HTTP请求的处理流程
typedef ngx_int_t (*ngx_http_phase_handler_pt)(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);

/*
注意通常，在任意一个ngx_http_phases阶段，都可以拥有零个或多个ngx_http_phase_handler_s结构体，其含义更接近于某个HTTP模块的处理方法。
一个http{}块解析完毕后将会根据nginx.conf中的配置产生由ngx_http_phase_handler_t组成的数组，在处理HTTP请求时，一般情况下这些阶段是顺序
向后执行的，但ngx_http_phase_handler_t中的next成员使得它们也可以非顺序执行。ngx_http_phase_engine_t结构体就是所有ngx_http_phase_handler_t组成的数组
*/    
//注意:每一个阶段中最后加入到handlers[]中的会首先添加到cmcf->phase_engine.handlers中，见ngx_http_init_phase_handlers
//注意：ngx_http_phase_handler_s结构体仅表示处理阶段中的一个处理方法
struct ngx_http_phase_handler_s { //ngx_http_phase_engine_t结构体就是所有ngx_http_phase_handler_t组成的数组，也就是ngx_http_phase_handler_s结构存储在ngx_http_phase_handler_t
    /*
    在处理到某一个HTTP阶段时，HTTP框架将会在checker方法已实现的前提下首先调用checker方法来处理请求，而不会直接调用任何阶段中的handler方
法，只有在checker方法中才会去调用handler方法。因此，事实上所有的checker方法都是由框架中的ngx_http_core module模块实现的，且普通的HTTP模块无
法重定义checker方法
     */
/*  HTTP框架为11个阶段实现的checker方法  赋值见ngx_http_init_phase_handlers  //所有阶段的checker在ngx_http_core_run_phases中调用
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━┓
┃    阶段名称                  ┃    checker方法                   ┃
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━┓
┃   NGX_HTTP_POST_READ_PHASE   ┃ngx_http_core_generic_phase       ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP SERVER REWRITE PHASE ┃ngx_http_core_rewrite_phase       ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP FIND CONFIG PHASE    ┃ngx_http_core find config_phase   ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP REWRITE PHASE        ┃ngx_http_core_rewrite_phase       ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP POST REWRITE PHASE   ┃ngx_http_core_post_rewrite_phase  ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP PREACCESS PHASE      ┃ngx_http_core_generic_phase       ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP ACCESS PHASE         ┃ngx_http_core_access_phase        ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP POST ACCESS PHASE    ┃ngx_http_core_post_access_phase   ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP TRY FILES PHASE      ┃ngx_http_core_try_files_phase     ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP CONTENT PHASE        ┃ngx_http_core_content_phase       ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP LOG PHASE            ┃ngx_http_core_generic_phase       ┃
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━┛
*/
     //处于同一ngx_http_phases阶段的所有ngx_http_phase_handler_t的checker指向相同的函数，见ngx_http_init_phase_handlers
     //ngx_http_phases阶段中的每一个阶段都有对应的checker函数，通过该checker函数来执行各自对应的。该checker函数在ngx_http_core_run_phases中执行
    ngx_http_phase_handler_pt  checker; //各个阶段的初始化赋值在ngx_http_init_phase_handlers中的checker函数中执行各自的handler方法,cheker是http框架函数，handler是对应的用户具体模块函数
//除ngx_http_core module模块以外的HTTP模块，只能通过定义handler方法才能介入某一个HTTP处理阶段以处理请求
//ngx_http_init_phase_handlers中ngx_http_phase_handler_s->handle指向了ngx_http_phase_t->handlers[i]
    ngx_http_handler_pt        handler; //只有在checker方法中才会去调用handler方法,见ngx_http_core_run_phases
/*
将要执行的下一个HTTP处理阶段的序号
next的设计使得处理阶段不必按顺序依次执行，既可以向后跳跃数个阶段继续执行，也可以跳跃到之前曾经执行过的某个阶段重新执行。通常，
next表示下一个处理阶段中的第1个ngx_http_phase_handler_s处理方法
*/ //NGX_HTTP_POST_REWRITE_PHASE的下一阶段是NGX_HTTP_FIND_CONFIG_PHASE，其他阶段的next都是ngx_http_phases中紧靠着的下一个phase
    ngx_uint_t                 next;//下一阶段的第一个ngx_http_handler_pt处理方法在数组中的位置
    ngx_uint_t  phase;      //yang add标记处于11个phase中的那个phase
};


/*
注意通常，在任意一个ngx_http_phases阶段，都可以拥有零个或多个ngx_http_phase_handler_s结构体，其含义更接近于某个HTTP模块的处理方法。
一个http{}块解析完毕后将会根据nginx.conf中的配置产生由ngx_http_phase_handler_t组成的数组，在处理HTTP请求时，一般情况下这些阶段是顺序
向后执行的，但ngx_http_phase_handler_t中的next成员使得它们也可以非顺序执行。ngx_http_phase_engine_t结构体就是所有ngx_http_phase_handler_t组成的数组

ngx_http_phase_engine_t中保存了在当前nginx.conf配置下，一个用户请求可能经历的所有ngx_http_handler_pt处理方法，这是所有HTTP模块可以合作处理用户请求
的关键！这个ngx_http_phase_engine_t结构体是保存在全局的ngx_http_core_main_conf_t结构体中的

在HTTP框架的初始化过程中，任何HTTP模块都可以在ngx_http_module_t接口的postconfiguration方法中将自定义的方法添加到handler动态数组中，这样，这个方法就会最
终添加到ngx_http_core_main_conf_t->phase_engine中
*/  
//空间创建及赋值参考ngx_http_init_phase_handlers
typedef struct { //ngx_http_phase_engine_t结构体是保存在全局的ngx_http_core_main_conf_t结构体中的
    /* handlers是由ngx_http_phase_handler_t构成的数组首地址，它表示一个请求可能经历的所有ngx_http_handler_pt处理方法，
    配合ngx_http_request_t结构体中的phase_handler成员使用（phase_handler指定了当前请求应当执行哪一个HTTP阶段）*/
    ngx_http_phase_handler_t  *handlers;
    
    /* 表示NGX_HTTP_SERVER_REWRITE_PHASE阶段第1个ngx_http_phase_handler_t处理方法在handlers数组中的序号，用于在执行HTTP请求
    的任何阶段中快速跳转到NGX_HTTP_SERVER_REWRITE_PHASE阶段处理请求 */
    ngx_uint_t                 server_rewrite_index; //赋值参考ngx_http_init_phase_handlers
    /*
    表示NGX_HTTP_REWRITE_PHASE阶段第1个ngx_http_phase_handler_t处理方法在handlers数组中的序号，用于在执行HTTP请求的任何阶段中
    快速跳转到NGX_HTTP_REWRITE_PHASE阶段处理请求
     */
    ngx_uint_t                 location_rewrite_index; //赋值参考ngx_http_init_phase_handlers
} ngx_http_phase_engine_t;


typedef struct { //存储在ngx_http_core_main_conf_t->phases[]
    //handlers动态数组保存着每一个HTTP模块初始化时添加到当前阶段的处理方法
    ////注意:每一个阶段中最后加入到handlers[]中的会首先添加到cmcf->phase_engine.handlers, 见ngx_http_init_phase_handlers
    //各个模块通过postconfiguration()接口加入到各自阶段的该数组中，例如参考ngx_http_realip_init
    ngx_array_t                handlers; //数组中存储的是ngx_http_handler_pt   ngx_http_init_phase_handlers中ngx_http_phase_handler_s->handle指向了ngx_http_phase_t->handlers[i]
} ngx_http_phase_t; 

/*ngx_http_core_main_conf_t(ngx_http_core_create_main_conf中创建) ngx_http_core_srv_conf_t(ngx_http_core_create_srv_conf创建)  
ngx_http_core_loc_conf_s(ngx_http_core_create_loc_conf创建) */
/*
图形化参考:深入理解NGINX中的图9-2(P302)  图10-1(P353) 图10-1(P356) 图10-1(P359)  图4-2(P145)

ngx_http_conf_ctx_t、ngx_http_core_main_conf_t、ngx_http_core_srv_conf_t、ngx_http_core_loc_conf_s和ngx_cycle_s->conf_ctx的关系见:
Nginx的http配置结构体的组织结构:http://tech.uc.cn/?p=300
*/ 

typedef struct {//初始化赋值参考ngx_http_core_module_ctx
/*
servers动态数组中的每一个元素都是一个指针，它指向用于表示server块的ngx_http_core_srv_conf_t结构体的地址（属于ngx_http_core_module模块）。
ngx_http_core_srv_conf_t结构体中有1个ctx指针，它指向解析server块时新生成的ngx_http_conf_ctx_t结构体,因此只要获取到http{}的上下文ctx，就能找到http{}
中server{}块的上下文ctx。图形化理解参考图10-3
 */
    /* 
     http {
        server {
            xxx;
        }   

        server {
            xxx;
        }
     } */ //servers里面存储的是当解析到http{}中server{}行的时候，该server对应的ngx_http_core_srv_conf_t,参考ngx_http_core_server中的ngx_array_push附近代码可以看出
    ngx_array_t                servers;  /* ngx_http_core_srv_conf_t */ //在ngx_http_core_create_main_conf中创建server项

/*
在HTTP框架的初始化过程中，任何HTTP模块都可以在ngx_http_module_t接口的postconfiguration方法中将自定义的方法添加到ngx_http_phase_t->handler动态数组中，
这样，这个方法就会最终添加到ngx_http_core_main_conf_t->phase_engine中
 */
    //由下面各阶段处理方法构成的phases数组构建的阶段引擎才是流水式处理HTTP请求的实际数据结构
    //注意:每一个阶段中最后加入到handlers[]中的会首先添加到cmcf->phase_engine.handlers中，见ngx_http_init_phase_handlers
    ngx_http_phase_engine_t    phase_engine; //在ngx_http_core_main_conf_t中关于HTTP阶段有两个成员：phase_engine和phases
    //ngx_http_headers_in中的全部成员都在该hash表中，见ngx_http_init_headers_in_hash
    ngx_hash_t                 headers_in_hash;//见ngx_http_headers_in  ngx_http_init_headers_in_hash

    //core http变量相关的三个结构体variables_hash   variables   variables_keys
    //最终模块内部以及通过set等在配置文件中设置的变量(variables_keys中的)最终存放到variables_hash中，set配置的变量存放到variables中，variables_keys最终置空
    //从ngx_http_variables_init_vars中可以看出并不是所有的变量都会放入variables_hash，只有variables_keys中的变量才会全部放入variables_hash
    //variables中的变量可能在variables_keys中，也可能不在variables_keys中
/*
NGX HTTP_VAR_INDEXED、NGXHTTP_VARNOHASH、变量cmcf->variables_hash以及取值函数ngx_http_get_variable等，它们都是为SSI模块实现而设计的
*/
    ngx_hash_t                 variables_hash;//赋值见ngx_http_variables_init_vars  //key为ngx_http_variable_t->name， value为ngx_http_variable_t  

    /*
    在解析配置文件中的时候，遇到set等配置变量的时候，会把对应的变量(模块自有变量外的)添加到variables数组，但是在解析配置文件过程
    中或者解析配置文件前，并没有把模块中自带的变量加入variables数组，而variables_keys中则存储了模块中自定义的以及set等在配置文件中解析到的变量
    */ //variables在ngx_http_variables_init_vars中会获取到对应的ngx_http_variable_t成员值

/*
Nginx在配置文件的解析过程中会遇到用户使用变量的情况，所有这些被用户在配置文件里使用的变量都会先通过
ngx_http_get_variable_index0而添加到cmcf->variables内。如果配置文件中出现：set $file ta;，在这里这个$file变量既是定义，又是使用，
先定义它，然后把字符串”ta”赋值给它，这也是一种使用，所以它会被加入到cmcf->variables内，可以简单地认为Nginx在解析配置文件的过程
中遇到的所有变量都会被加入到cmcf->variables内。有些变量虽然没有出现在配置文件内，但是以Nginx默认设置的形式出现在源代码里，比如
ngx_http_log_module模块内的ngx_http_combined_fmt全局静态变量里就出现了一些Nginx变量。
    虽然Nginx默认提供的变量有很多，但只需把我们在配置文件里真正用到了的变量给挑出来，当配置文件解析完后，所有用到的变量也被集中起来了
*/
/*
ngx_http_core_main_conf_t->variabels数组成员的结构式ngx_http_variable_s， ngx_http_request_s->variabels数组成员结构是
ngx_variable_value_t这两个结构的关系很密切，一个所谓变量，一个所谓变量值

r->variables这个变量和cmcf->variables是一一对应的，形成var_ name与var_value对，所以两个数组里的同一个下标位置元素刚好就是
相互对应的变量名和变量值，而我们在使用某个变量时总会先通过函数ngx_http_get_variable_index获得它在变量名数组里的index下标，也就是变
量名里的index字段值，然后利用这个index下标进而去变量值数组里取对应的值

*/ //用户需要使用的变量才加入到variables，variables_keys中的变量时包含使用的和不使用的都在里面。
//变量ngx_http_script_var_code_t->index表示Nginx变量$file在ngx_http_core_main_conf_t->variables数组内的下标，对应每个请求的变量值存储空间就为r->variables[code->index],参考ngx_http_script_set_var_code
    ngx_array_t                variables;       /* ngx_http_variable_t */  //ngx_http_get_variable_index中分配空间 注意和 ngx_http_request_s->variables的区别
    
    ngx_uint_t                 ncaptures;

    ngx_uint_t                 server_names_hash_max_size;
    ngx_uint_t                 server_names_hash_bucket_size;

    ngx_uint_t                 variables_hash_max_size; //默认值见ngx_http_core_init_main_conf
    ngx_uint_t                 variables_hash_bucket_size; //默认值见ngx_http_core_init_main_conf

    //core http变量相关的三个结构体variables_hash   variables   variables_keys
    //解析完配置参数后，variables_keys中存放的是模块中自带的变量或者set在配置文件中设置的变量，见ngx_http_add_variable
    //ngx_http_variables_add_core_vars把ngx_http_core_variables中的各种变量信息存放到cmcf->variables_keys中
    //注意:ngx_http_core_module模块一定要在其他涉及到变量模块前定义，因为variables_keys空间是在ngx_http_core_module模块的ngx_http_variables_add_core_vars中创建
    //variables数组中只包含从配置文件中通过set配置的模块中自定义变量外的变量，variables_keys包括模块自定义的和set等配置项配置的变量，
    //variables中的变量可能在variables_keys中，也可能不在variables_keys中，见ngx_http_variables_init_vars
    //最终该variables_keys表中的成员会加入到variables_hash中，见ngx_http_variables_init_vars，所以variables_keys算是一个临时变量
    /*
    除了http核心模块ngx_http_core_module以外，其他模块都会这么直接或间接地把自身支持的内部变量加到cmcf->variables_keys内，再比如
    ngx_http_proxy_module模块，其相关执行过程如下。ngx_http_proxy_add_variables()  一>ngx_http_add_variable()  一>ngx_hash_add_key()   
        总之，当Nginx解析配置正常结束时，所有的变量都被集中在cmcf->variables_keys内
        所有这些变量需要检查其是否合法，因为Nginx不能让用户在配置文件里使用一个非法的变量，这就需要cmcf->variables_keys的帮忙。见ngx_http_variables_init_vars
     */
    ngx_hash_keys_arrays_t    *variables_keys; //key为ngx_http_variable_t->name， value为ngx_http_variable_t  

    //这里是个数组的原因是:如果配置中有listen 1.1.1.1:50  2.2.2.2:50,则端口都是50，但监听IP不一样，他们存储在该数组中，如果端口一样。如果端口和IP地址一样，则会以第一条为准，算一条
    //则放入到同一个ngx_http_conf_port_t节点中，如果有其他端口地址，则放到下一个ngx_http_conf_port_t节点中，不同端口存储在该结构体的不同节点
    //所有http中的listen，包括在不同server{]种的listen,他们的listen头添加到ngx_http_core_main_conf_t->ports,见ngx_http_add_listen
    //将addrs排序，带通配符的地址排在后面， (listen 1.2.2.2:30 bind) > listen 1.1.1.1:30  > listen *:30,见ngx_http_block
    ngx_array_t               *ports;//没解析到一个listen配置项，就添加一个ngx_http_conf_port_t  赋值见ngx_http_add_listen，存储的是ngx_http_conf_port_t结构

    ngx_uint_t                 try_files;       /* unsigned  try_files:1 */ //是否有配置try_files  赋值见ngx_http_core_try_files

/*
在ngx_http_core_main_conf_t中关于HTTP阶段有两个成员：phase_engine和phases，其中phase_engine控制运行过程中一个HTTP请求所要
经过的HTTP处理阶段，它将配合ngx_http_request_t结构体中的phase_handler成员使用（phase_handler指定了当前请求应当执行哪一个HTTP阶段）；
而phases数组更像一个临时变量，它实际上仅会在Nginx启动过程中用到，它的唯一使命是按照11个阶段的概念初始化phase_engine中的handlers数组
 */
/*
用于在HTTP框架初始化时帮助各个HTTP模块在任意阶段中添加HTTP处理方法，它是一个有11个成员的ngx_http_phase_t数组，其中每一个ngx_http_phase_t
结构体对应一个HTTP阶段。在HTTP框架初始化完毕后，运行过程中的phases数组是无用的

 NGX_HTTP_FIND_CONFIG_PHASE、NGX_HTTP_POSTREWRITE_PHASE、NGX_HTTP_POST_ACCESS_PHASE、 NGX_HTTP_TRY_FILES_PHASE这4个阶段则不允
 许HTTP模块加入自己的ngx_http_handler_pt方法处理用户请求，它们仅由HTTP框架实现。其他7个阶段允许我们加入ngx_http_handler_pt方法
因此最后加入到phases[]各个阶段的ngx_http_handler_pt方法不会出现这四种，因此最终ngx_http_phase_engine_t->handlers数组中也不会出现这四个阶段的处理方法
 */ //phases数组更像一个临时变量，它实际上仅会在Nginx启动过程中用到，它的唯一使命是按照11个阶段的概念初始化phase_engine中的handlers数组
    //在ngx_http_block中在执行每个模块module->postconfiguration接口的时候向phases数组添加ngx_http_handler_pt处理方法，例如可以参考函数ngx_http_rewrite_init等
    //空间创建和初始化见ngx_http_init_phases
    ngx_http_phase_t           phases[NGX_HTTP_LOG_PHASE + 1]; 
} ngx_http_core_main_conf_t;

/*
= 开头表示精确匹配
^~ 开头表示uri以某个常规字符串开头，理解为匹配 url路径即可。nginx不对url做编码，因此请求为/static/20%/aa，可以被规则^~ /static/ /aa匹配到（注意是空格）。
~ 开头表示区分大小写的正则匹配
~*  开头表示不区分大小写的正则匹配
!~和!~*分别为区分大小写不匹配及不区分大小写不匹配 的正则
/ 通用匹配，任何请求都会匹配到。


location匹配命令

~      #波浪线表示执行一个正则匹配，区分大小写
~*    #表示执行一个正则匹配，不区分大小写
^~    #^~表示普通字符匹配，如果该选项匹配，只匹配该选项，不匹配别的选项，一般用来匹配目录
=      #进行普通字符精确匹配
@     #"@" 定义一个命名的 location，使用在内部定向时，例如 error_page, try_files



location 匹配的优先级(与location在配置文件中的顺序无关)
= 精确匹配会第一个被处理。如果发现精确匹配，nginx停止搜索其他匹配。
普通字符匹配，正则表达式规则和长的块规则将被优先和查询匹配，也就是说如果该项匹配还需去看有没有正则表达式匹配和更长的匹配。
^~ 则只匹配该规则，nginx停止搜索其他匹配，否则nginx会继续处理其他location指令。
最后匹配理带有"~"和"~*"的指令，如果找到相应的匹配，则nginx停止搜索其他匹配；当没有正则表达式或者没有正则表达式被匹配的情况下，那么匹配程度最高的逐字匹配指令会被使用。

location 优先级官方文档

1.Directives with the = prefix that match the query exactly. If found, searching stops.
2.All remaining directives with conventional strings, longest match first. If this match used the ^~ prefix, searching stops.
3.Regular expressions, in order of definition in the configuration file.
4.If #3 yielded a match, that result is used. Else the match from #2 is used.
1.=前缀的指令严格匹配这个查询。如果找到，停止搜索。
2.所有剩下的常规字符串，最长的匹配。如果这个匹配使用^?前缀，搜索停止。
3.正则表达式，在配置文件中定义的顺序。
4.如果第3条规则产生匹配的话，结果被使用。否则，如同从第2条规则被使用。


例如

location  = / {
# 只匹配"/".
[ configuration A ] 
}
location  / {
# 匹配任何请求，因为所有请求都是以"/"开始
# 但是更长字符匹配或者正则表达式匹配会优先匹配
[ configuration B ] 
}
location ^~ /images/ {
# 匹配任何以 /images/ 开始的请求，并停止匹配 其它location
[ configuration C ] 
}
location ~* \.(gif|jpg|jpeg)$ {
# 匹配以 gif, jpg, or jpeg结尾的请求. 
# 但是所有 /images/ 目录的请求将由 [Configuration C]处理.   
[ configuration D ] 
}请求URI例子:

?/ -> 符合configuration A
?/documents/document.html -> 符合configuration B
?/images/1.gif -> 符合configuration C
?/documents/1.jpg ->符合 configuration D
@location 例子
error_page 404 = @fetch;

location @fetch(
proxy_pass http://fetch;
)
*/

/*ngx_http_core_main_conf_t(ngx_http_core_create_main_conf中创建) ngx_http_core_srv_conf_t(ngx_http_core_create_srv_conf创建)  
ngx_http_core_loc_conf_s(ngx_http_core_create_loc_conf创建) */

/*
图形化参考:深入理解NGINX中的图9-2(P302)  图10-1(P353) 图10-1(P356) 图10-1(P359)  图4-2(P145)

ngx_http_conf_ctx_t、ngx_http_core_main_conf_t、ngx_http_core_srv_conf_t、ngx_http_core_loc_conf_s和ngx_cycle_s->conf_ctx的关系见:
Nginx的http配置结构体的组织结构:http://tech.uc.cn/?p=300
*/ 
typedef struct {
    /*
用于设置监听socket的指令主要有两个：server_name和listen。server_name指令用于实现虚拟主机的功能，会设置每个server块的虚拟主机名，
在处理请求时会根据请求行中的host来转发请求,listen配置最终存放在ngx_http_core_main_conf_t->ports
当前server块的虚拟主机名，如果存在的话，则会与HTTP请求中的Host头部做匹配，匹配上后再由当前ngx_http_core_srv_conf_t处理请求 */
    /*   
     server {
         listen       80;
         server_name  example.org  www.example.org;
         ...
     }
     */ /* array of the ngx_http_server_name_t, "server_name" directive */
    ngx_array_t                 server_names;//可以参考ngx_http_core_server_name

    /* server ctx */
    /*
    指向当前server{}中开辟ngx_http_conf_ctx_t空间的上下文，参考ngx_http_core_server， ngx_http_core_srv_conf_t结构空间都是由
    ngx_http_conf_ctx_t中的srv_conf创建
    */ //指向当前server块所属的ngx_http_conf_ctx_t结构体
    ngx_http_conf_ctx_t        *ctx; //执行对应的server{}解析的时候开辟的ngx_http_conf_ctx_t

    ngx_str_t                   server_name; 

    size_t                      connection_pool_size; //默认256
    size_t                      request_pool_size; //默认4096，见ngx_http_core_merge_srv_conf
    size_t                      client_header_buffer_size;

    //client_header_buffer_size为读取客户端数据时默认分配的空间，如果该空间不够存储http头部行和请求行，则会调用large_client_header_buffers
    //从新分配空间，并把之前的空间内容拷贝到新空间中，所以，这意味着可变长度的HTTP请求行加上HTTP头部的长度总和不能超过large_client_ header_
    //buffers指定的字节数，否则Nginx将会报错。
    ngx_bufs_t                  large_client_header_buffers; 
    //timer_resolution这个参数加上可以保证定时器每个这么多秒中断一次，从而可以从epoll中返回，并跟新时间，判断哪些事件有超时，执行超时事件，例如客户端继上次
    //发请求过来，隔了client_header_timeout时间后还没有新请求过来，这会关闭连接
    //注意，在解析到完整的头部行和请求行后，会在ngx_http_process_request中会把读事件超时定时器删除
    ngx_msec_t                  client_header_timeout; //默认60秒，如果不设置的话      注意和上面的large_client_header_buffers配合解释
    
    ngx_flag_t                  ignore_invalid_headers; //默认为1
    ngx_flag_t                  merge_slashes;
    ngx_flag_t                  underscores_in_headers; //HTTP头部是否允许下画线, 见ngx_http_parse_header_line

    unsigned                    listen:1;
#if (NGX_PCRE)
    unsigned                    captures:1;
#endif
    /*
            location / {
              try_files index.html index.htm @fallback;
            }
            
            location @fallback {
              root /var/www/error;
              index index.html;
            }
        */

    /*
   ngx_http_init_locations中把name location加入到named_locations，正则表达式location加入到regex_locations  完全匹配和前缀匹配location存入locations
     static_locations把locations中的节点从新组成新的static_locations三叉树
     */ 
    //ngx_http_init_locations中把name location加入到named_locations，正则表达式location加入到regex_locations  完全匹配和前缀匹配location存入locations
    ngx_http_core_loc_conf_t  **named_locations; //指向server{}中所有location中所有的名称name  location @name,见ngx_http_init_locations
} ngx_http_core_srv_conf_t;


/* list of structures to find core_srv_conf quickly at run time */

//server_name配置项的相关信息，见ngx_http_core_server_name
typedef struct {
#if (NGX_PCRE)
    ngx_http_regex_t          *regex;
#endif
    //server_name配置项所处的server{}，见ngx_http_core_server_name
    ngx_http_core_srv_conf_t  *server;   /* virtual name server conf */
    ngx_str_t                  name; //server_name xxx, name就是xxx
} ngx_http_server_name_t;

//结构ngx_http_virtual_names_t中的成员
typedef struct { //创建空间和赋值见ngx_http_add_addrs
     ngx_hash_combined_t       names; //这里面的hash信息是ngx_http_server_names中存储到hash表中的server_name及其所在server{}上下文ctx,server_name为key，上下文ctx为value

     ngx_uint_t                nregex;
     ngx_http_server_name_t   *regex;
} ngx_http_virtual_names_t;

//如果http{}中有未精确配置的listen(bind = 0)切存在通配符listen(listen *:0)则这些未精确配置项的ngx_http_addr_conf_s成员取值就是通配符配置项
//对应的ngx_http_conf_addr_t中的相关成员，见ngx_http_add_addrs
//ngx_http_port_t->ngx_http_in_addr_t->ngx_http_addr_conf_s
//这里面存储有server_name配置信息以及该ip:port对应的上下文信息
struct ngx_http_addr_conf_s {//创建空间赋值在ngx_http_add_addrs。 该结构体是ngx_http_in_addr_t中的conf成员  
//此外，ngx_http_connection_t->addr_conf也指向该结构
    /* the default server configuration for this address:port */
    /*    相同listen ip:port出现在不同的server中，那么opt指向最后解析的listen配置中带有default_server选项所对应的server{}上下文ctx，如果几个listen
       都没有加default参数，则该值为解析到的第一个ip:port所在server{}见ngx_http_add_addresses
           如果ip:port只处于一个server{}中，则默认初始化的时候直接指向listen ip:port所在server{}，见ngx_http_add_address*/
    ngx_http_core_srv_conf_t  *default_server; //通过ngx_http_core_srv_conf_t

    /*
      来源见ngx_http_server_names，他们是进行server_name字符串排序并hash后的存储地址，他们的来源是相同IP:port对应的
      所有server{}块的server_name配置，见ngx_http_server_names
    */
    ngx_http_virtual_names_t  *virtual_names; //创建空间赋值在ngx_http_add_addrs

#if (NGX_HTTP_SSL)
    unsigned                   ssl:1; //listen配置是否启用ssl认证
#endif
#if (NGX_HTTP_V2)
    unsigned                   http2:1;
#endif
    unsigned                   proxy_protocol:1;
};

//ngx_http_port_t->ngx_http_in_addr_t->ngx_http_addr_conf_s
typedef struct {//该结构存放在ngx_http_port_t
    in_addr_t                  addr; //该listen的地址， = sin->sin_addr.s_addr;见ngx_http_add_addrs

    //如果http{}中有未精确配置的listen(bind = 0)切存在通配符listen(listen *:0)则这些未精确配置项的ngx_http_addr_conf_s成员取值就是通配符配置项
//对应的ngx_http_conf_addr_t中的相关成员，见ngx_http_add_addrs
    ngx_http_addr_conf_t       conf;//这里面存储有server_name配置信息以及该ip:port对应的上下文信息
} ngx_http_in_addr_t;//创建空间赋值在ngx_http_add_addrs,是从ngx_http_conf_addr_t获取到的


#if (NGX_HAVE_INET6)

typedef struct {
    struct in6_addr            addr6;
    ngx_http_addr_conf_t       conf;
} ngx_http_in6_addr_t;

#endif

//ngx_http_port_t->ngx_http_in_addr_t->ngx_http_addr_conf_s
//见ngx_http_add_addrs 
//ngx_connection_t->listening->servers指向该结构，见ngx_http_init_connection       
typedef struct {//该结构最终由ngx_conf_t->cycle->listening->servers存储该空间，见ngx_http_init_listening
    /* ngx_http_in_addr_t or ngx_http_in6_addr_t */
    void                      *addrs; //创建空间在ngx_http_add_addrs  这是个数组结构
    ngx_uint_t                 naddrs; //这个记录的是配置中相同port中含有通配符以及存在非bind得配置的个数，赋值见ngx_http_init_listening， 

    //1个ngx_http_port_t对应一个ngx_http_conf_addr_t
} ngx_http_port_t; //这里面的值都是从ngx_http_conf_port_t->ngx_http_conf_addr_t里面获取到的， ngx_http_add_addrs -> ngx_http_add_addrs,是从ngx_http_conf_port_t->ngx_http_conf_addr_t[]获取到的

/*
ngx_http_core_main_conf_t

    |---> prots： 监听的端口号的数组

                |---> ngx_http_conf_port_t：端口号的配置信息 (ngx_http_port_t内容也是从该结构中获取->ngx_http_in_addr_t)

                               |---> addrs：在该端口号上，监听的所有地址的数组 该addrs下的所有端口相同

                                            |---> ngx_http_conf_addr_t：地址配置信息，包含在该addr:port上的多个虚拟主机  (ngx_http_port_t->ngx_http_in_addr_t内容也是从该结构中获取)

                                                           |---> servers：在addr:port上的说个server块的配置信息ngx_http_core_srv_conf_t(//例如不同server{}中有相同的listen ip:port，他们都在同一个ngx_http_conf_addr_t中，但servers指向不同)

                                                           |            |---> ngx_http_core_srv_conf_t

                                                           |---> opt：ngx_http_listen_opt_t类型，监听socket的配置信息

                                                           |---> hash：以server_name为key，ngx_http_core_srv_conf_t为value的hash表，并且server_name不含通配符。

                                                           |---> wc_head：同hash，server_name含前缀通配符。

                                                           |---> wc_tail：同hash，server_name含后缀通配符。



*/
//所有http中的listen，包括在不同server{]种的listen,他们的listen头添加到ngx_http_core_main_conf_t->ports,见ngx_http_add_listen
//该结构存储在ngx_http_core_main_conf_t中的ports，它用来存储listen配置项的相关信息
//所有listen的端口相同，IP不同的listen信息全部存储在该结构中，不同的ip地址放入addrs数组中。监听端口配置信息，addrs是在该端口上所有监听地址的数组。
typedef struct { //见ngx_http_add_listen     (ngx_http_port_t->ngx_http_in_addr_t内容也是从该结构中获取)
    ngx_int_t                  family; //协议族
    in_port_t                  port; //listen命令配置监听的端口
//对于每一个端口信息（ngx_http_conf_port_t）,里面有一个字段为addrs，这个字段是一个数组，这个数组内存放的全是地址信息（ngx_http_conf_addr_t），一个地址信息
//（ngx_http_conf_addr_t）对应着一个ip:port
    //相同port不同IP，那么就用这个区分，他们有不同的ngx_http_conf_addr_t信息。如果两个server{}有相同的listen ip:port,则会创建两个ngx_http_conf_addr_t，见ngx_http_add_addresses
    //两个在不同server{]中的相同IP:port，其在addrs中也是存在的两个ngx_http_conf_addr_t，各自有各自的
    ngx_array_t                addrs;     /* array of ngx_http_conf_addr_t */  //见ngx_http_add_address分配空间     
} ngx_http_conf_port_t; //

/*
ngx_http_core_main_conf_t

    |---> prots： 监听的端口号的数组

                |---> ngx_http_conf_port_t：端口号的配置信息 (ngx_http_port_t->ngx_http_in_addr_t内容也是从该结构中获取->ngx_http_in_addr_t)
                               |---> addrs：在该端口号上，监听的所有地址的数组，该addrs下的所有端口相同  该addrs下的所有端口相同

                                            |---> ngx_http_conf_addr_t：地址配置信息，包含在该addr:port上的多个虚拟主机(ngx_http_port_t->ngx_http_in_addr_t内容也是从该结构中获取->ngx_http_in_addr_t)

                                                           |---> servers：在addr:port上的说个server块的配置信息ngx_http_core_srv_conf_t(//例如不同server{}中有相同的listen ip:port，他们都在同一个ngx_http_conf_addr_t中，但servers指向不同)

                                                           |            |---> ngx_http_core_srv_conf_t

                                                           |---> opt：ngx_http_listen_opt_t类型，监听socket的配置信息

                                                           |---> hash：以server_name为key，ngx_http_core_srv_conf_t为value的hash表，并且server_name不含通配符。

                                                           |---> wc_head：同hash，server_name含前缀通配符。

                                                           |---> wc_tail：同hash，server_name含后缀通配符。
*/
//listen的相关配置项会存储在该结构中，然后把他们全部放到ngx_http_core_main_conf_t中的ports
//该结构存储在ngx_http_conf_port_t中的addrs中
/*
监听地址配置信息，包含了所有在该addr:port监听的所有server块的ngx_http_core_srv_conf_t结构，以及hash、wc_head和wc_tail这些hash结构，
保存了以server name为key，ngx_http_core_srv_conf_t为value的哈希表，用于快速查找对应虚拟主机的配置信息。
*/
typedef struct { //赋值见ngx_http_add_address    (ngx_http_port_t->ngx_http_in_addr_t内容也是从该结构中获取)
    //指向"listen"解析出的配置信息，如果相同listen ip:port出现在不同的server中，那么opt指向最后解析的listen结构ngx_http_listen_opt_t，见ngx_http_add_addresses
    ngx_http_listen_opt_t      opt; 

    /*
      下面这三个赋值见ngx_http_server_names，他们是进行server_name字符串排序并hash后的存储地址，他们的来源是相同IP:port对应的
      所有server{}块的server_name配置，见ngx_http_server_names
    */
    //这三个成员记录的是listen ip:PORT这条配置所在的server{}中的server_name配置信息
    //把一个ngx_http_conf_addr_t成员对应的server{}中的server_name配置信息添加到这三个hash表中
    ngx_hash_t                 hash; //以server_name为key，ngx_http_core_srv_conf_t为value的hash表，并且server_name不含通配符。
    ngx_hash_wildcard_t       *wc_head; //同hash，server_name含前缀通配符。
    ngx_hash_wildcard_t       *wc_tail; //同hash，server_name含后缀通配符。

#if (NGX_PCRE)
    ngx_uint_t                 nregex;
    ngx_http_server_name_t    *regex; //正则表达式的server_name存入这里面 见ngx_http_server_names
#endif

    /* the default server configuration for this address:port */
    /*    相同listen ip:port出现在不同的server中，那么opt指向最后解析的listen配置中带有default_server选项所对应的server{}上下文ctx，如果几个listen
       都没有加default参数，则该值为解析到的第一个ip:port所在server{}见ngx_http_add_addresses
           如果ip:port只处于一个server{}中，则默认初始化的时候直接指向listen ip:port所在server{}，见ngx_http_add_address*/
    ngx_http_core_srv_conf_t  *default_server;  
    //存储的是该listen命令配置所在server{}对应的ngx_http_core_srv_conf_t，赋值见ngx_http_add_server
    //例如不同server{}中有相同的listen ip:port，他们都在同一个ngx_http_conf_addr_t中，但servers指向各自不同的ngx_http_core_srv_conf_t存储在该servers数组中

    /* 如果listen ip:port是唯一的ip:port，则指向自己的server{}上下文，如果是存在通配符listen，并且有未加bind的其listen，则
    在不同的server{}中，每个ip:port对应的server{}上下文存储在该servers数组中，见ngx_http_add_addresses->ngx_http_add_server */
    ngx_array_t                servers;  /* array of ngx_http_core_srv_conf_t */  
} ngx_http_conf_addr_t; //如果有两个listen ip:port bind中的ip:port相同，但是在不同的server{}中，则只会创建一个ngx_http_conf_addr_t,但他们的server{}配置不同，见ngx_http_add_addresses
//(ngx_http_port_t->ngx_http_in_addr_t内容也是从该结构中获取)



typedef struct { //赋值参考ngx_http_core_error_page    生效见ngx_http_send_error_page
    ngx_int_t                  status;
    /*
    error_page 404 =200 /empty.gif;，表示以200作为新的返回码，用户可以通过“=”来更改返回的错误码,此时overwrite为200
    error_page 404 = /empty.gif; 表示返回码由重定向后实际处理的真实结果来决定，这时，只要把“=”后面没有返回码号  此时overwrite=0
    没有=，就是以error_page   404   /404.html;中的404作为返回码   此时overwrite=-1
     */
    ngx_int_t                  overwrite;
    ngx_http_complex_value_t   value;
    ngx_str_t                  args;
} ngx_http_err_page_t;


typedef struct {
    ngx_array_t               *lengths; //这里面的code在ngx_http_core_try_files_phase中执行
    ngx_array_t               *values;
    ngx_str_t                  name; //name

    unsigned                   code:10;
    unsigned                   test_dir:1;
} ngx_http_try_file_t; //ngx_http_core_loc_conf_s中的try_files[]成员

/*
= 开头表示精确匹配
^~ 开头表示uri以某个常规字符串开头，理解为匹配 url路径即可。nginx不对url做编码，因此请求为/static/20%/aa，可以被规则^~ /static/ /aa匹配到（注意是空格）。
~ 开头表示区分大小写的正则匹配
~*  开头表示不区分大小写的正则匹配
!~和!~*分别为区分大小写不匹配及不区分大小写不匹配 的正则
/ 通用匹配，任何请求都会匹配到。


location匹配命令

~      #波浪线表示执行一个正则匹配，区分大小写
~*    #表示执行一个正则匹配，不区分大小写
^~    #^~表示普通字符匹配，如果该选项匹配，只匹配该选项，不匹配别的选项，一般用来匹配目录
=      #进行普通字符精确匹配
@     #"@" 定义一个命名的 location，使用在内部定向时，例如 error_page, try_files



location 匹配的优先级(与location在配置文件中的顺序无关)
= 精确匹配会第一个被处理。如果发现精确匹配，nginx停止搜索其他匹配。
普通字符匹配，正则表达式规则和长的块规则将被优先和查询匹配，也就是说如果该项匹配还需去看有没有正则表达式匹配和更长的匹配。
^~ 则只匹配该规则，nginx停止搜索其他匹配，否则nginx会继续处理其他location指令。
最后匹配理带有"~"和"~*"的指令，如果找到相应的匹配，则nginx停止搜索其他匹配；当没有正则表达式或者没有正则表达式被匹配的情况下，那么匹配程度最高的逐字匹配指令会被使用。

location 优先级官方文档

1.Directives with the = prefix that match the query exactly. If found, searching stops.
2.All remaining directives with conventional strings, longest match first. If this match used the ^~ prefix, searching stops.
3.Regular expressions, in order of definition in the configuration file.
4.If #3 yielded a match, that result is used. Else the match from #2 is used.
1.=前缀的指令严格匹配这个查询。如果找到，停止搜索。
2.所有剩下的常规字符串，最长的匹配。如果这个匹配使用^?前缀，搜索停止。
3.正则表达式，在配置文件中定义的顺序。
4.如果第3条规则产生匹配的话，结果被使用。否则，如同从第2条规则被使用。


例如

location  = / {
# 只匹配"/".
[ configuration A ] 
}
location  / {
# 匹配任何请求，因为所有请求都是以"/"开始
# 但是更长字符匹配或者正则表达式匹配会优先匹配
[ configuration B ] 
}
location ^~ /images/ {
# 匹配任何以 /images/ 开始的请求，并停止匹配 其它location
[ configuration C ] 
}
location ~* \.(gif|jpg|jpeg)$ {
# 匹配以 gif, jpg, or jpeg结尾的请求. 
# 但是所有 /images/ 目录的请求将由 [Configuration C]处理.   
[ configuration D ] 
}请求URI例子:

?/ -> 符合configuration A
?/documents/document.html -> 符合configuration B
?/images/1.gif -> 符合configuration C
?/documents/1.jpg ->符合 configuration D
@location 例子
error_page 404 = @fetch;

location @fetch(
proxy_pass http://fetch;
)
*/
/*ngx_http_core_main_conf_t(ngx_http_core_create_main_conf中创建) ngx_http_core_srv_conf_t(ngx_http_core_create_srv_conf创建)  
ngx_http_core_loc_conf_s(ngx_http_core_create_loc_conf创建) */

/*
图形化参考:深入理解NGINX中的图9-2(P302)  图10-1(P353) 图10-1(P356) 图10-1(P359)  图4-2(P145)

ngx_http_conf_ctx_t、ngx_http_core_main_conf_t、ngx_http_core_srv_conf_t、ngx_http_core_loc_conf_s和ngx_cycle_s->conf_ctx的关系见:
Nginx的http配置结构体的组织结构:http://tech.uc.cn/?p=300
*/ 

//参考ngx_http_core_location
struct ngx_http_core_loc_conf_s {
    //ngx_http_add_location中把精确匹配 正则表达式 name  noname配置以外的其他配置都算做前缀匹配  例如//location ^~  xxx{}      location /XXX {}
    ngx_str_t     name;          /* location name */ //location后面跟的的uri字符串  不包括^~  = 等, 例如location ^~  xxx{}，则name为xxx

//ngx_http_add_location中把精确匹配 正则表达式 name  noname配置以外的其他配置都算做前缀匹配  例如//location ^~  xxx{}      location /XXX {}
#if (NGX_PCRE)
    ngx_http_regex_t  *regex; //非NULL，表示为正则匹配
#endif
    /*
        按HTTP方法名限制用户请求
        语法：limit_except method ... {...}
        配置块：location
        Nginx通过limit_except后面指定的方法名来限制用户请求。方法名可取值包括：GET、HEAD、POST、PUT、DELETE、MKCOL、COPY、MOVE、OPTIONS、PROPFIND、PROPPATCH、LOCK、UNLOCK或者PATCH。例如：
        limit_except GET {
            allow 192.168.1.0/32;
            deny  all;
        }
        注意，允许GET方法就意味着也允许HEAD方法。因此，上面这段代码表示的是禁止GET方法和HEAD方法，但其他HTTP方法是允许的。


        if ($http_user_agent ~ MSIE) {
              rewrite  ^(.*)$  /msie/$1  break;
            }
        
        */

    //ngx_http_init_locations这里拆分出的noname放入tail后，如果locations中包含named location,则noname和name放一起(cscf->named_locations)，如果没有named但是有regex，
    //则和regex放一起(pclcf->regex_locations)，如果name和regex和都没有，则和普通locations放一起(pclcf->locations)
    //"if"配置 或者 limit_except配置置位,  if和limit_except都是当做noname的location解析
    unsigned      noname:1;   /* "if () {}" block or limit_except */ //nginx会把if 指令配置也看做一个location，即noname类型。
    
    unsigned      lmt_excpt:1; //limit_except配置置位

    /*
location / {
  try_files index.html index.htm @fallback;
}

location @fallback {
  root /var/www/error;
  index index.html;
}
     */
 // @  表示为一个location进行命名，即自定义一个location，这个location不能被外界所访问，只能用于Nginx产生的子请求，主要为error_page和try_files。  
    unsigned      named:1; //以’@’开头的命名location，如location @test {}
    

    unsigned      exact_match:1; //类似 location = / {}，所谓准确匹配。
    unsigned      noregex:1; //没有正则，指类似location ^~ /a { ... } 的location。  前缀匹配
    

    unsigned      auto_redirect:1;
#if (NGX_HTTP_GZIP)
    unsigned      gzip_disable_msie6:2;
#if (NGX_HTTP_DEGRADATION)
    unsigned      gzip_disable_degradation:2;
#endif
#endif
    /*
     首先从locations链表中去掉那些正则匹配，还有named和nonamed的location节点。那么location链表中只剩下精准匹配和前缀匹配的那些location节点了，
     从这些节点中产生static location tree。 

     //ngx_http_init_locations中把name location加入到named_locations，正则表达式location加入到regex_locations  完全匹配和前缀匹配location存入locations
     static_locations把locations中的节点从新组成新的static_locations三叉树
     */ 
    ngx_http_location_tree_node_t   *static_locations; //在ngx_http_init_static_location_trees中对server{}块内的location{}(包括exact/inclusive/noregex)进行三叉排序
#if (NGX_PCRE)//ngx_http_init_locations中把name location加入到named_locations，正则表达式location加入到regex_locations  完全匹配和前缀匹配location存入locations
    ngx_http_core_loc_conf_t       **regex_locations; /* 所有的location 正则表达式 {}这种ngx_http_core_loc_conf_t全部指向regex_locations */
#endif

    /* pointer to the modules' loc_conf */
    //执行location{} ctx的ctx->loc_conf
    void        **loc_conf; //赋值见ngx_http_core_location，指向ngx_http_conf_ctx_t->loc_conf

    uint32_t      limit_except; //"limit_except"配置
    void        **limit_except_loc_conf;

    //以mytest为例，模块注册的handler是在ngx_http_core_content_phase执行的，事实上ngx_http_finalize_request决定了ngx_http_mytest_handler如何起作用。
    //该函数在ngx_http_core_content_phase中的ngx_http_finalize_request(r, r->content_handler(r));里面的r->content_handler(r)执行
    //在ngx_http_update_location_config中赋值给r->content_handler = clcf->handler;

    /*
    ngx_http_handler_pt处理方法不再应用于所有的HTTP请求，仅仅当用户请求的URI匹配了location时(也就是mytest配置项所在的location)才会被调用。
这也就意味着它是一种完全不同于其他阶段的使用方式。 因此，当HTTP模块实现了某个ngx_http_handler_pt处理方法并希望介入NGX_HTTP_CONTENT_PHASE阶
段来处理用户请求时，如果希望这个ngx_http_handler_pt方法应用于所有的用户请求，则应该在ngx_http_module_t接口的postconfiguration方法中，
向ngx_http_core_main_conf_t结构体的phases[NGX_HTTP_CONTENT_PHASE]动态数组中添加ngx_http_handler_pt处理方法；反之，如果希望这个方式
仅应用于URI匹配丁某些location的用户请求，则应该在一个location下配置项的回调方法中，把ngx_http_handler_pt方法设置到ngx_http_core_loc_conf_t
结构体的handler中。
    注意ngx_http_core_loc_conf_t结构体中仅有一个handler指针，它不是数组，这也就意味着如果采用上述的第二种方法添加ngx_http_handler_pt处理方法，
那么每个请求在NGX_HTTP_CONTENT PHASE阶段只能有一个ngx_http_handler_pt处理方法。而使用第一种方法时是没有这个限制的，NGX_HTTP_CONTENT_PHASE阶
段可以经由任意个HTTP模块处理。
     */ //该handler执行在，他先赋值给ngx_http_request_t->content_handler，然后在ngx_http_core_content_phase中执行，赋值见ngx_http_update_location_config
    ngx_http_handler_pt  handler;/*HTTP框架在处理用户请求进行到NGX_HTTP_CONTENT_PHASE阶段时，如果请求的主机域名、URI与mytest配置项所在的配置块相匹配，就将调用我们实现的ngx_http_mytest_handler方法处理这个请求*/

    /* location name length for inclusive location with inherited alias */
    size_t        alias;
    ngx_str_t     root;                    /* root, alias */ //默认一般会是/usr/local/nginx/html
    ngx_str_t     post_action;//post_action XXXX配置 默认为空

    ngx_array_t  *root_lengths;
    ngx_array_t  *root_values;

    ngx_array_t  *types; //types {}配置ngx_http_core_type首先存在与该数组中，然后在ngx_http_core_merge_loc_conf存入types_hash中，真正生效见ngx_http_set_content_type
    ngx_hash_t    types_hash;//mime.type文件中的type类型存到该hash中
    ngx_str_t     default_type; //"text/plain"  如果uril最后面的后缀，则默认content-type为该字符串

    off_t         client_max_body_size;    /* client_max_body_size */ //默认1M 1 * 1024 * 1024
    //生效见ngx_open_and_stat_file  if (of->directio <= ngx_file_size(&fi)) { ngx_directio_on }
    off_t         directio;                /* directio */ //默认NGX_OPEN_FILE_DIRECTIO_OFF是个超级大的值，相当于不使能 可以directio 512配置
    //默认512
    off_t         directio_alignment;      /* directio_alignment */ //directio_alignment 512;  它与directio配合使用，指定以directio方式读取文件时的对齐方式
    //实际上是client_body_buffer_size + client_body_buffer_size >> 2
    size_t        client_body_buffer_size; /* client_body_buffer_size */ 
    
    size_t        send_lowat;              /* send_lowat */ //配置该选项后，会启用ngx_send_lowat
   /* 
   clcf->postpone_output：由于处理postpone_output指令，用于设置延时输出的阈值。比如指令“postpone s”，当输出内容的size小于s， 默认1460
   并且不是最后一个buffer，也不需要flush，那么就延时输出。见ngx_http_write_filter -> if (!last && !flush && in && size < (off_t) clcf->postpone_output) {
    */
    size_t        postpone_output;         /* postpone_output */ //默认1460
    size_t        limit_rate;              /* limit_rate */
    //在…后再限制速率为,所以实际的速率应该比limit_rate稍微大一点，可以参考ngx_http_write_filter
    size_t        limit_rate_after;        /* limit_rate_after */ 

    /*
     Syntax:  sendfile_max_chunk size;
     Default:  sendfile_max_chunk 0; 
     Context:  http, server, location
      
     When set to a non-zero value, limits the amount of data that can be transferred in a single sendfile() call. Without the 
     limit, one fast connection may seize the worker process entirely. 如果不设置该参数，可能会阻塞http框架，因为可能发送的包体很大
     */ //如果没有配置该值，则发送的时候默认一次最多发送NGX_MAX_SIZE_T_VALUE - ngx_pagesize;  见ngx_linux_sendfile_chain
    size_t        sendfile_max_chunk;      /* sendfile_max_chunk */ //最大一次发送给客户端的数据大小
    size_t        read_ahead;              /* read_ahead配置，默认0 */
    
    //如果数据包包体很大，对方可能会多次发送才能发送完成，本端需要多次读取，等待读取客户端数据到来的最大超时事件为该变量，见ngx_http_do_read_client_request_body
    ngx_msec_t    client_body_timeout;     /* client_body_timeout */ 
    ngx_msec_t    send_timeout;            /* send_timeout */
    ngx_msec_t    keepalive_timeout;       /* keepalive_timeout */ //当接收到客户端请求，并应答了后，在ngx_http_set_keepalive设置保活定时器，默认75秒

    //min(lingering_time,lingering_timeout)这段时间内可以继续读取数据，如果客户端有发送数据过来，见ngx_http_set_lingering_close
    ngx_msec_t    lingering_time;          /* lingering_time */
    ngx_msec_t    lingering_timeout;       /* lingering_timeout */ 

    
    ngx_msec_t    resolver_timeout;        /* resolver_timeout */

    ngx_resolver_t  *resolver;             /* resolver */

    time_t        keepalive_header;        /* keepalive_timeout */

    ngx_uint_t    keepalive_requests;      /* keepalive_requests */
    ngx_uint_t    keepalive_disable;       /* keepalive_disable */
    ngx_uint_t    satisfy;                 /* satisfy */ //取值NGX_HTTP_SATISFY_ALL或者NGX_HTTP_SATISFY_ANY  见ngx_http_core_access_phase
/*
lingering_close
语法：lingering_close off | on | always;
默认：lingering_close on;
配置块：http、server、location
该配置控制Nginx关闭用户连接的方式。always表示关闭用户连接前必须无条件地处理连接上所有用户发送的数据。off表示关闭连接时完全不管连接
上是否已经有准备就绪的来自用户的数据。on是中间值，一般情况下在关闭连接前都会处理连接上的用户发送的数据，除了有些情况下在业务上认定这之后的数据是不必要的。
*/
    ngx_uint_t    lingering_close;         /* lingering_close */
    //if_modified_since [off|exact|before];进行配置 //生效见ngx_http_test_if_modified
    ngx_uint_t    if_modified_since;       /* if_modified_since */
    ngx_uint_t    max_ranges;              /* max_ranges */
    ngx_uint_t    client_body_in_file_only; /* client_body_in_file_only */ //取值NGX_HTTP_REQUEST_BODY_FILE_CLEAN等

    ngx_flag_t    client_body_in_single_buffer; //client_body_in_single_buffer on | off;设置
                                           /* client_body_in_singe_buffer */
    //在location{}中配置了internal，表示匹配该uri的location{}必须是进行重定向后匹配的该location,如果不满足条件直接返回NGX_HTTP_NOT_FOUND，
    //生效地方见ngx_http_core_find_config_phase                                        
    ngx_flag_t    internal;                /* internal */ //见"internal"配置，ngx_http_core_internal置1
    ngx_flag_t    sendfile;                /* sendfile */ //sendfile on | off
    //aio解析赋值见ngx_http_core_set_aio
    ngx_flag_t    aio;                     /* aio */ //aio on | off;默认off  aio on | off | threads[=pool];
    // tcp_nopush on | off;只有开启sendfile，nopush才生效，通过设置TCP_CORK实现
    ngx_flag_t    tcp_nopush;              /* tcp_nopush */
    ngx_flag_t    tcp_nodelay;             /* tcp_nodelay */
    ngx_flag_t    reset_timedout_connection; /* reset_timedout_connection */
    ngx_flag_t    server_name_in_redirect; /* server_name_in_redirect */
    ngx_flag_t    port_in_redirect;        /* port_in_redirect */
    ngx_flag_t    msie_padding;            /* msie_padding */
    ngx_flag_t    msie_refresh;            /* msie_refresh */
    ngx_flag_t    log_not_found;           /* log_not_found */
    ngx_flag_t    log_subrequest;          /* log_subrequest */
    ngx_flag_t    recursive_error_pages;   /* recursive_error_pages */
    ngx_flag_t    server_tokens;           /* server_tokens */
    ngx_flag_t    chunked_transfer_encoding; /* chunked_transfer_encoding */ //默认1
    /*
     Etag确定浏览器缓存： Etag的原理是将文件资源编号一个etag值，Response给访问者，访问者再次请求时，带着这个Etag值，与服务端所请求
     的文件的Etag对比，如果不同了就重新发送加载，如果相同，则返回304. HTTP/1.1 304 Not Modified,否则直接返回文件内容，应答头部行 200 OK
     */ //etag设置见ngx_http_set_etag
    ngx_flag_t    etag;                    /* etag */ //可以etag off关闭，则永远不参数etag头部行给浏览器

#if (NGX_HTTP_GZIP)
    ngx_flag_t    gzip_vary;               /* gzip_vary */

    ngx_uint_t    gzip_http_version;       /* gzip_http_version */
    ngx_uint_t    gzip_proxied;            /* gzip_proxied */

#if (NGX_PCRE)
    ngx_array_t  *gzip_disable;            /* gzip_disable */
#endif
#endif

#if (NGX_THREADS)
    ngx_thread_pool_t         *thread_pool;//aio thread 配置的时候，location{}块对应的thread_poll信息，见ngx_http_core_set_aio
    ngx_http_complex_value_t  *thread_pool_value;
#endif

#if (NGX_HAVE_OPENAT)
    ngx_uint_t    disable_symlinks;        /* disable_symlinks */
    ngx_http_complex_value_t  *disable_symlinks_from; //disable_symlinks on | if_not_owner [from=part];中from携带的参数part
#endif
    //由error_page进行配置，创建空间和赋值见ngx_http_core_error_page   
    //clcf->error_pages赋值参考ngx_http_core_error_page    生效见ngx_http_special_response_handler  ngx_http_send_error_page(error_pages内容是从ngx_http_error_pages中取的)
    //error_page 401 404 =200 /empty.gif;  =前面有两个编码号，因此占用两个数组成员，成员类型ngx_http_err_page_t，见ngx_http_core_error_page
    ngx_array_t  *error_pages;             /* error_page */
    ngx_http_try_file_t    *try_files;     /* try_files */ //创建空间和赋值见ngx_http_core_try_files，相当于一个数组，用来存取try_files aaa bbb ccc中的 aaa bbb ccc

    ngx_path_t   *client_body_temp_path;   /* client_body_temp_path */ //"client_body_temp_path"设置
    //ngx_http_core_open_file_cache中创建空间和赋值  
    ngx_open_file_cache_t  *open_file_cache; //如果不配置open_file_cache max=1000 inactive=20s;则默认指向NULL
//多久检测一次缓存有效性  赋值可以参考ngx_http_file_cache_open
    time_t        open_file_cache_valid; //默认为每60秒检查一次缓存中的元素是否仍有效。 open_file_cache_valid可以配置  实际该配置在ngx_open_cached_file
/*
例如open_file_cache max=102400 inactive=20s;则在20s内如果至少有open_file_cache_min_uses次请求，则缓存中的文件更改信息不变,
这时候的情况是:请求带有If-Modified-Since，得到的是304且Last-Modified时间没变
*/
    ngx_uint_t    open_file_cache_min_uses; //一般赋值给ngx_open_file_info_t->min_uses   赋值可以参考ngx_http_file_cache_open
    //此配置项表示是否在文件缓存中缓存打开文件时出现的找不到路径、没有权限等错误信息。 默认off
    ngx_flag_t    open_file_cache_errors;
    ngx_flag_t    open_file_cache_events;

     /* 全局中配置的error_log xxx存储在ngx_cycle_s->new_log，http{}、server{}、local{}配置的error_log保存在ngx_http_core_loc_conf_t->error_log,
    见ngx_log_set_log,如果只配置全局error_log，不配置http{}、server{}、local{}则在ngx_http_core_merge_loc_conf conf->error_log = &cf->cycle->new_log;  */
    //ngx_log_insert插入，在ngx_log_error_core找到对应级别的日志配置进行输出，因为可以配置error_log不同级别的日志存储在不同的日志文件中
    ngx_log_t    *error_log;

    ngx_uint_t    types_hash_max_size;
    ngx_uint_t    types_hash_bucket_size;

/*
每一个server块可以对应着多个location块，而一个location块还可以继续嵌套多个location块。每一批location块是通过双向链表与它的父配置块（要
么属于server块，要么属于location块{}关联起来的
*/
    //头部是ngx_queue_t，next开始的成员为ngx_http_location_queue_t
    //location{}中的配置存储连接在父级server{}上下文的ctx->loc_conf[ngx_http_core_module.ctx_index]->locations中
    /*
     ngx_http_init_locations中把name location加入到named_locations，正则表达式location加入到regex_locations  完全匹配和前缀匹配location存入locations
     static_locations把locations中的节点从新组成新的static_locations三叉树
     */ 
    ngx_queue_t  *locations;//ngx_http_add_location函数中分配空间  location中的loc配置通过这个链接到父级的server{}里面分配的loc_conf中，见ngx_http_add_location

#if 0
    ngx_http_core_loc_conf_t  *prev_location;
#endif
};

//初始化赋值见ngx_http_add_location  
//拆分以提高http请求，见ngx_http_init_locations
typedef struct {  //图形化参考http://blog.chinaunix.net/uid-27767798-id-3759557.html，这个博客很好理解
    ngx_queue_t                      queue;//所有的loc配置通过该队列链接在一起

    //下面这两个字段指明是精确匹配还是前缀匹配，因为该ngx_http_location_queue_t结构体是最后在ngx_http_init_locations拆分后的locations，因此只有精确匹配和前缀匹配这两种location
    //完全匹配 重命名(@name)location  noname  正则表达式匹配,节点都添加到exact指针
    ngx_http_core_loc_conf_t        *exact; //正则表达式 noname name 往前匹配的location都是存到这里面，见ngx_http_add_location
    //ngx_http_add_location中赋值     精确匹配 正则表达式 name  noname配置以外的其他配置都算做前缀匹配  例如//location ^~  xxx{}      location /XXX {}
    ngx_http_core_loc_conf_t        *inclusive; //location ^~   应该是前缀匹配  前缀匹配ngx_http_core_loc_conf_t节点添加到该指针上
    
    ngx_str_t                       *name;//当前location /xxx {}中的/XXXX
    u_char                          *file_name; //所在的配置文件名
    ngx_uint_t                       line; //在配置文件中的行号
    ngx_queue_t                      list; //三叉树排序用到，主要是前缀一样的字符串通过list添加，//很好的图解，参考http://blog.csdn.net/fengmo_q/article/details/6683377
} ngx_http_location_queue_t;//ngx_http_init_static_location_trees

/*
    对于精确匹配的location不会放在公共前缀节点的tree节点中，会单拉出来一个node和前缀节点平行。也就是说对于精确匹
    配 ＝/abcd 和前缀匹配的/abc两个location配置，=/abcd不会是/abc节点的tree节点。=/abcd 只能是／abc的right节点 
*/
//ngx_http_init_locations中name noname regex以外的location(exact/inclusive 完全匹配/前缀匹配)
//图解参考:http://blog.csdn.net/fengmo_q/article/details/6683377
//参考ngx_http_core_find_static_location
struct ngx_http_location_tree_node_s {//图解好的参考http://blog.chinaunix.net/uid-27767798-id-3759557.html
/*
static location tree大大优化了精准匹配和前缀匹配的location的查找过程，线性递归查找效率低下，三叉树的左节点代表当前比node节点的name小的节点，
右节点代表比当前node节点name大的节点，tree节点表示拥有相同前缀的节点。
*/
    ngx_http_location_tree_node_t   *left; //左节点代表当前比node节点的name小的节点
    ngx_http_location_tree_node_t   *right; //右节点代表比当前node节点name大的节点
    ngx_http_location_tree_node_t   *tree; //tree节点表示拥有相同前缀的节点。  普通location中某节点的list成员形成的数  无法完全匹配的location组成的树

    /*
    如果location对应的URI匹配字符串属于能够完全匹配的类型，则exact指向其对应的ngx_http_core_loc_conf_t结构体，否则为NULL空指针
     */ //这两个指针的最终来源见ngx_http_add_location
    ngx_http_core_loc_conf_t        *exact;  //精确匹配   指向形成三叉树的队列ngx_http_location_queue_t->exact
    ngx_http_core_loc_conf_t        *inclusive; //前缀匹配节点

    u_char                           auto_redirect; //自动重定向标志
    u_char                           len;  //name字符串的实际长度
    u_char                           name[1]; //name指向location对应的URI匹配表达式  location xxx {}中的xxx字符串
};

void ngx_http_core_run_phases(ngx_http_request_t *r);
const char* ngx_http_phase_2str(ngx_uint_t phase);
ngx_int_t ngx_http_core_generic_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);
ngx_int_t ngx_http_core_rewrite_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);
ngx_int_t ngx_http_core_find_config_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);
ngx_int_t ngx_http_core_post_rewrite_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);
ngx_int_t ngx_http_core_access_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);
ngx_int_t ngx_http_core_post_access_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);
ngx_int_t ngx_http_core_try_files_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);
ngx_int_t ngx_http_core_content_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph);


void *ngx_http_test_content_type(ngx_http_request_t *r, ngx_hash_t *types_hash);
ngx_int_t ngx_http_set_content_type(ngx_http_request_t *r);
void ngx_http_set_exten(ngx_http_request_t *r);
ngx_int_t ngx_http_set_etag(ngx_http_request_t *r);
void ngx_http_weak_etag(ngx_http_request_t *r);
ngx_int_t ngx_http_send_response(ngx_http_request_t *r, ngx_uint_t status,
    ngx_str_t *ct, ngx_http_complex_value_t *cv);
u_char *ngx_http_map_uri_to_path(ngx_http_request_t *r, ngx_str_t *name,
    size_t *root_length, size_t reserved);
ngx_int_t ngx_http_auth_basic_user(ngx_http_request_t *r);
#if (NGX_HTTP_GZIP)
ngx_int_t ngx_http_gzip_ok(ngx_http_request_t *r);
#endif


ngx_int_t ngx_http_subrequest(ngx_http_request_t *r,
    ngx_str_t *uri, ngx_str_t *args, ngx_http_request_t **sr,
    ngx_http_post_subrequest_t *psr, ngx_uint_t flags);
ngx_int_t ngx_http_internal_redirect(ngx_http_request_t *r,
    ngx_str_t *uri, ngx_str_t *args);
ngx_int_t ngx_http_named_location(ngx_http_request_t *r, ngx_str_t *name);


ngx_http_cleanup_t *ngx_http_cleanup_add(ngx_http_request_t *r, size_t size);

/*
过滤模块的调用顺序
    既然一个请求会被所有的HTTP过滤模块依次处理，那么下面来看一下这些HTTP过滤模块是如何组织到一起的，以及它们的调用顺序是如何确定的。
6.2.1  过滤链表是如何构成的
    在编译Nginx源代码时，已经定义了一个由所有HTTP过滤模块组成的单链表，这个单链表与一般的链表是不一样的，它有另类的风格：链表的每一个
元素都是一个独立的C源代码文件，而这个C源代码文件会通过两个static静态指针（分别用于处理HTTP头部和HTTP包体）再指向下一个文件中的过滤方法。
在HTTP框架中定义了两个指针，指向整个链表的第一个元素，也就是第一个处理HTTP头部、HTTP包体的方法。
*/

/*
 注意对于HTTP过滤模块来说，在ngx_modules数组中的位置越靠后，在实陈执行请
求时就越优先执行。因为在初始化HTTP过滤模块时，每一个http过滤模块都是将自己插入
到整个单链表的首部的。
*/
//每个过滤模块处理HTTP头部的方法，它仅接收1个参数r，也就是当前的请求
typedef ngx_int_t (*ngx_http_output_header_filter_pt)(ngx_http_request_t *r);  //见ngx_http_top_header_filter
//每个过滤模块处理HTTP包体的方法原型，它接收两个参数-r和chain，共中r是当前的请求，chain是要发送的HTTP包体
typedef ngx_int_t (*ngx_http_output_body_filter_pt)(ngx_http_request_t *r, ngx_chain_t *chain);//见ngx_http_top_body_filter
typedef ngx_int_t (*ngx_http_request_body_filter_pt)
    (ngx_http_request_t *r, ngx_chain_t *chain);


ngx_int_t ngx_http_output_filter(ngx_http_request_t *r, ngx_chain_t *chain);
ngx_int_t ngx_http_write_filter(ngx_http_request_t *r, ngx_chain_t *chain);
ngx_int_t ngx_http_request_body_save_filter(ngx_http_request_t *r,
   ngx_chain_t *chain);


ngx_int_t ngx_http_set_disable_symlinks(ngx_http_request_t *r,
    ngx_http_core_loc_conf_t *clcf, ngx_str_t *path, ngx_open_file_info_t *of);

ngx_int_t ngx_http_get_forwarded_addr(ngx_http_request_t *r, ngx_addr_t *addr,
    ngx_array_t *headers, ngx_str_t *value, ngx_array_t *proxies,
    int recursive);


extern ngx_module_t  ngx_http_core_module;

extern ngx_uint_t ngx_http_max_module;

extern ngx_str_t  ngx_http_core_get_method;


#define ngx_http_clear_content_length(r)                                      \
                                                                              \
    r->headers_out.content_length_n = -1;                                     \
    if (r->headers_out.content_length) {                                      \
        r->headers_out.content_length->hash = 0;                              \
        r->headers_out.content_length = NULL;                                 \
    }              
 
#define ngx_http_clear_accept_ranges(r)                                       \
                                                                              \
    r->allow_ranges = 0;                                                      \
    if (r->headers_out.accept_ranges) {                                       \
        r->headers_out.accept_ranges->hash = 0;                               \
        r->headers_out.accept_ranges = NULL;                                  \
    }

#define ngx_http_clear_last_modified(r)                                       \
                                                                              \
    r->headers_out.last_modified_time = -1;                                   \
    if (r->headers_out.last_modified) {                                       \
        r->headers_out.last_modified->hash = 0;                               \
        r->headers_out.last_modified = NULL;                                  \
    }

#define ngx_http_clear_location(r)                                            \
                                                                              \
    if (r->headers_out.location) {                                            \
        r->headers_out.location->hash = 0;                                    \
        r->headers_out.location = NULL;                                       \
    }

#define ngx_http_clear_etag(r)                                                \
                                                                              \
    if (r->headers_out.etag) {                                                \
        r->headers_out.etag->hash = 0;                                        \
        r->headers_out.etag = NULL;                                           \
    }


#endif /* _NGX_HTTP_CORE_H_INCLUDED_ */
