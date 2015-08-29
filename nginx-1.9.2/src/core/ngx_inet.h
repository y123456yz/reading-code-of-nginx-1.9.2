
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_INET_H_INCLUDED_
#define _NGX_INET_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * TODO: autoconfigure NGX_SOCKADDRLEN and NGX_SOCKADDR_STRLEN as
 *       sizeof(struct sockaddr_storage)
 *       sizeof(struct sockaddr_un)
 *       sizeof(struct sockaddr_in6)
 *       sizeof(struct sockaddr_in)
 */

#define NGX_INET_ADDRSTRLEN   (sizeof("255.255.255.255") - 1)
#define NGX_INET6_ADDRSTRLEN                                                 \
    (sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") - 1)
#define NGX_UNIX_ADDRSTRLEN                                                  \
    (sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path))

#if (NGX_HAVE_UNIX_DOMAIN)
#define NGX_SOCKADDR_STRLEN   (sizeof("unix:") - 1 + NGX_UNIX_ADDRSTRLEN)
#else
#define NGX_SOCKADDR_STRLEN   (NGX_INET6_ADDRSTRLEN + sizeof("[]:65535") - 1)
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
#define NGX_SOCKADDRLEN       sizeof(struct sockaddr_un)
#else
#define NGX_SOCKADDRLEN       512
#endif


typedef struct {
    in_addr_t                 addr;
    in_addr_t                 mask;
} ngx_in_cidr_t;


#if (NGX_HAVE_INET6)

typedef struct {
    struct in6_addr           addr;
    struct in6_addr           mask;
} ngx_in6_cidr_t;

#endif


typedef struct {
    ngx_uint_t                family;
    union {
        ngx_in_cidr_t         in;
#if (NGX_HAVE_INET6)
        ngx_in6_cidr_t        in6;
#endif
    } u;
} ngx_cidr_t;


typedef struct {
    struct sockaddr          *sockaddr;
    socklen_t                 socklen;
    ngx_str_t                 name;
} ngx_addr_t;


typedef struct { //通配符解析见ngx_parse_inet_url
    ngx_str_t                 url;//保存IP地址+端口信息（e.g. 192.168.124.129:8011 或 money.163.com）
    ngx_str_t                 host;//保存IP地址信息 //proxy_pass  http://10.10.0.103:8080/tttxx; 中的10.10.0.103
    ngx_str_t                 port_text;//保存port字符串
    ngx_str_t                 uri;//uri部分，在函数ngx_parse_inet_url()中设置  http://10.10.0.103:8080/tttxx;中的/tttxx

    in_port_t                 port;//端口，e.g. listen指令中指定的端口（listen 192.168.124.129:8011）
    //默认端口（当no_port字段为真时，将默认端口赋值给port字段， 默认端口通常是80）
    in_port_t                 default_port; //ngx_http_core_listen中设置为80 //默认端口（当no_port字段为真时，将默认端口赋值给port字段， 默认端口通常是80）
    int                       family;//address family, AF_xxx  //AF_UNIX代表域套接字  AF_INET代表普通网络套接字

    unsigned                  listen:1; //ngx_http_core_listen中置1 //是否为指监听类的设置
    unsigned                  uri_part:1;
    unsigned                  no_resolve:1; //根据情况决定是否解析域名（将域名解析到IP地址）
    unsigned                  one_addr:1;  /* compatibility */ ///等于1时，仅有一个IP地址

    unsigned                  no_port:1;//标识url中没有显示指定端口(为1时没有指定)  uri中是否有指定端口
    unsigned                  wildcard:1; //如listen  *:80则该位置1 //标识是否使用通配符（e.g. listen *:8000;）

    socklen_t                 socklen;//sizeof(struct sockaddr_in)
    u_char                    sockaddr[NGX_SOCKADDRLEN];//sockaddr_in结构指向它

    ngx_addr_t               *addrs;//数组大小是naddrs字段；每个元素对应域名的IP地址信息(struct sockaddr_in)，在函数中赋值（ngx_inet_resolve_host()）
    ngx_uint_t                naddrs;//url对应的IP地址个数,IP格式的地址将默认为1

    char                     *err;//错误信息字符串
} ngx_url_t;


in_addr_t ngx_inet_addr(u_char *text, size_t len);
#if (NGX_HAVE_INET6)
ngx_int_t ngx_inet6_addr(u_char *p, size_t len, u_char *addr);
size_t ngx_inet6_ntop(u_char *p, u_char *text, size_t len);
#endif
size_t ngx_sock_ntop(struct sockaddr *sa, socklen_t socklen, u_char *text,
    size_t len, ngx_uint_t port);
size_t ngx_inet_ntop(int family, void *addr, u_char *text, size_t len);
ngx_int_t ngx_ptocidr(ngx_str_t *text, ngx_cidr_t *cidr);
ngx_int_t ngx_parse_addr(ngx_pool_t *pool, ngx_addr_t *addr, u_char *text,
    size_t len);
ngx_int_t ngx_parse_url(ngx_pool_t *pool, ngx_url_t *u);
ngx_int_t ngx_inet_resolve_host(ngx_pool_t *pool, ngx_url_t *u);
ngx_int_t ngx_cmp_sockaddr(struct sockaddr *sa1, socklen_t slen1,
    struct sockaddr *sa2, socklen_t slen2, ngx_uint_t cmp_port);


#endif /* _NGX_INET_H_INCLUDED_ */
