
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_POSIX_CONFIG_H_INCLUDED_
#define _NGX_POSIX_CONFIG_H_INCLUDED_


#if (NGX_HPUX)
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED  1
#define _HPUX_ALT_XOPEN_SOCKET_API
#endif


#if (NGX_TRU64)
#define _REENTRANT
#endif


#if (NGX_GNU_HURD)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE             /* accept4() */
#endif
#define _FILE_OFFSET_BITS       64
#endif


#ifdef __CYGWIN__
#define timezonevar             /* timezone is variable */
#define NGX_BROKEN_SCM_RIGHTS   1
#endif


#include <sys/types.h>
#include <sys/time.h>
#if (NGX_HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if (NGX_HAVE_INTTYPES_H)
#include <inttypes.h>
#endif
#include <stdarg.h>
#include <stddef.h>             /* offsetof() */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <glob.h>
#include <time.h>
#if (NGX_HAVE_SYS_PARAM_H)
#include <sys/param.h>          /* statfs() */
#endif
#if (NGX_HAVE_SYS_MOUNT_H)
#include <sys/mount.h>          /* statfs() */
#endif
#if (NGX_HAVE_SYS_STATVFS_H)
#include <sys/statvfs.h>        /* statvfs() */
#endif

#if (NGX_HAVE_SYS_FILIO_H)
#include <sys/filio.h>          /* FIONBIO */
#endif
#include <sys/ioctl.h>          /* FIONBIO */

#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sched.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>        /* TCP_NODELAY */
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>

#if (NGX_HAVE_LIMITS_H)
#include <limits.h>             /* IOV_MAX */
#endif

#ifdef __CYGWIN__
#include <malloc.h>             /* memalign() */
#endif

#if (NGX_HAVE_CRYPT_H)
#include <crypt.h>
#endif


#ifndef IOV_MAX
#define IOV_MAX   16
#endif


#include <ngx_auto_config.h>


#if (NGX_HAVE_POSIX_SEM)
#include <semaphore.h>
#endif


#if (NGX_HAVE_POLL)
#include <poll.h>
#endif


#if (NGX_HAVE_KQUEUE)
#include <sys/event.h>
#endif


#if (NGX_HAVE_DEVPOLL)
#include <sys/ioctl.h>
#include <sys/devpoll.h>
#endif

/*
 struct iocb  f
//存储着业务需要的指针。例如，在Nginx中，这个字段通常存储着对应的ngx_event_t亭件的指针。它实际上与io_getevents方法中返回的io event结构体的data成员是完全一致的+／
    u int64 t aio_data;
//不需要设置
u  int32_t PADDED (aio_key,  aio_raservedl)j
//操作码，其取值范围是io iocb cmd t中的枚举命令
u int16_t aio lio_opcode;
//请求的优先级
int16 t aio_reqprio,
//文件描述符
u int32 t aio fildes;
//读／写操作对应的用户态缓冲区
u int64 t aio buf;
//读／写操作的字节长度
u int64 t aio_nbytes;
//读／写操作对应于文件中的偏移量
int64 t aio offset;
//保留字段
u int64_t aio reserved2;
//表示可以设置为IOCB FLAG RESFD，它会告诉内核当有异步I/O请求处理完成时使用eventfd进行通知，可与epoll配合使用，其在Nginx中的使用方法可参见9.9.2节+／
    u int32_t aio_flags；
//表示当使用IOCB FLAG RESFD标志位时，用于进行事件通知的句柄
U int32 t aio resfd;
}
    因此，在设置好iocb结构体后，就可以向异步I/O提交事件了。aio_lio_opcode操作码
指定了这个事件的操作类型，它的取值范围如下。
    typedef enum io_iocb_cmd{
    ／／异步读操作
    IO_CMD_PREAD=O，
    ／／异步写操作
    IO_CMD_PWRITE=1，
    ／／强制同步
    IO_CMD_FSYNC=2，
    ／／目前采使用
    IO_CMD_FDSYNC=3，
    ／／目前未使用
    IO_CMD_POLL=5，
    ／／不做任何事情
    IO_CMD_NOOP=6，
    )  io_iocb_cmd_t;

*/
#if (NGX_HAVE_FILE_AIO)  //  --with-file-aio)                 NGX_FILE_AIO=YES           ;;
#include <aio.h>
typedef struct aiocb  ngx_aiocb_t;
#endif


#define NGX_LISTEN_BACKLOG  511

#define ngx_debug_init()


#if (__FreeBSD__) && (__FreeBSD_version < 400017)

#include <sys/param.h>          /* ALIGN() */

/*
 * FreeBSD 3.x has no CMSG_SPACE() and CMSG_LEN() and has the broken CMSG_DATA()
 */

#undef  CMSG_SPACE
#define CMSG_SPACE(l)       (ALIGN(sizeof(struct cmsghdr)) + ALIGN(l))

#undef  CMSG_LEN
#define CMSG_LEN(l)         (ALIGN(sizeof(struct cmsghdr)) + (l))

#undef  CMSG_DATA
#define CMSG_DATA(cmsg)     ((u_char *)(cmsg) + ALIGN(sizeof(struct cmsghdr)))

#endif


extern char **environ;


#endif /* _NGX_POSIX_CONFIG_H_INCLUDED_ */
