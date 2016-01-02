
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


u_char  ngx_linux_kern_ostype[50];
u_char  ngx_linux_kern_osrelease[50];
/*
#define ngx_recv             ngx_io.recv
#define ngx_recv_chain       ngx_io.recv_chain
#define ngx_udp_recv         ngx_io.udp_recv
#define ngx_send             ngx_io.send
#define ngx_send_chain       ngx_io.send_chain //epoll方式ngx_io = ngx_os_io;
*/
//如果是linux并且编译过程使能了sendfile这里面ngx_os_specific_init赋值ngx_os_io = ngx_linux_io;
static ngx_os_io_t ngx_linux_io = {
    ngx_unix_recv, //ngx_recv
    ngx_readv_chain, //ngx_recv_chain   ->recv_chain(相关指针的地方调用
    ngx_udp_unix_recv, //ngx_udp_recv
    ngx_unix_send, //ngx_send
#if (NGX_HAVE_SENDFILE)
    ngx_linux_sendfile_chain, //ngx_send_chain
    NGX_IO_SENDFILE  //./configure配置了sendfile，编译的时候加上sendfile选项,，就会在ngx_linux_io把flag置为该值
#else
    ngx_writev_chain, //ngx_send_chain
    0
#endif
};


ngx_int_t
ngx_os_specific_init(ngx_log_t *log)
{
    struct utsname  u;
    
    if (uname(&u) == -1) {
        ngx_log_error(NGX_LOG_ALERT, log, ngx_errno, "uname() failed");
        return NGX_ERROR;
    }

    (void) ngx_cpystrn(ngx_linux_kern_ostype, (u_char *) u.sysname,
                       sizeof(ngx_linux_kern_ostype));

    (void) ngx_cpystrn(ngx_linux_kern_osrelease, (u_char *) u.release,
                       sizeof(ngx_linux_kern_osrelease));

    ngx_os_io = ngx_linux_io;

    return NGX_OK;
}


void
ngx_os_specific_status(ngx_log_t *log)
{
    ngx_log_error(NGX_LOG_NOTICE, log, 0, "OS: %s %s",
                  ngx_linux_kern_ostype, ngx_linux_kern_osrelease);
}
