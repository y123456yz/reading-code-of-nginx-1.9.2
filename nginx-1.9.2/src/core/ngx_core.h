
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CORE_H_INCLUDED_
#define _NGX_CORE_H_INCLUDED_


#include <ngx_config.h>

typedef struct ngx_module_s      ngx_module_t;
typedef struct ngx_conf_s        ngx_conf_t;
typedef struct ngx_cycle_s       ngx_cycle_t;
typedef struct ngx_pool_s        ngx_pool_t;
typedef struct ngx_chain_s       ngx_chain_t;
typedef struct ngx_log_s         ngx_log_t;
typedef struct ngx_open_file_s   ngx_open_file_t;
typedef struct ngx_command_s     ngx_command_t;
typedef struct ngx_file_s        ngx_file_t;
typedef struct ngx_event_s       ngx_event_t;
typedef struct ngx_event_aio_s   ngx_event_aio_t;
typedef struct ngx_connection_s  ngx_connection_t;

#if (NGX_THREADS)
typedef struct ngx_thread_task_s  ngx_thread_task_t;
#endif

/*
每一个事件最核心的部分是handler回调方法，它将由每一个事件消费模块实现，以此决定这个事件究竟如何“消费”
*/ /* 注意ngx_http_event_handler_pt和ngx_event_handler_pt的区别 */
typedef void (*ngx_event_handler_pt)(ngx_event_t *ev);

//ngx_connection_handler_pt粪型的handler成员表示在这个监听端口上成功建立新的TCP连接后，就会回调handler方法
typedef void (*ngx_connection_handler_pt)(ngx_connection_t *c);

/*
这4个返回码引发的Nginx一系列动作。

NGX_OK：表示成功。Nginx将会继续执行该请求的后续动作（如执行subrequest或撤销这个请求）。

NGX_DECLINED：继续在NGX_HTTP_CONTENT_PHASE阶段寻找下一个对于该请求感兴趣的HTTP模块来再次处理这个请求。

NGX_DONE：表示到此为止，同时HTTP框架将暂时不再继续执行这个请求的后续部分。事实上，这时会检查连接的类型，如果是keepalive类型的用户请求，
    就会保持住HTTP连接，然后把控制权交给Nginx。这个返回码很有用，考虑以下场景：在一个请求中我们必须访问一个耗时极长的操作（比如某个网络调用），
    这样会阻塞住Nginx，又因为我们没有把控制权交还给Nginx，而是在ngx_http_mytest_handler中让Nginx worker进程休眠了（如等待网络的回包），
    所以，这就会导致Nginx出现性能问题，该进程上的其他用户请求也得不到响应。可如果我们把这个耗时极长的操作分为上下两个部分（就像Linux内核
    中对中断处理的划分），上半部分和下半部分都是无阻塞的（耗时很少的操作），这样，在ngx_http_mytest_handler进入时调用上半部分，然后返回NGX_DONE，
    把控制交还给Nginx，从而让Nginx继续处理其他请求。在下半部分被触发时（这里不探讨具体的实现方式，事实上使用upstream方式做反向代理时用的就是
    这种思想），再回调下半部分处理方法，这样就可以保证Nginx的高性能特性了。

NGX_ERROR：表示错误。这时会调用ngx_http_terminate_request终止请求。如果还有POST子请求，那么将会在执行完POST请求后再终止本次请求。
*/
#define  NGX_OK          0
#define  NGX_ERROR      -1
#define  NGX_AGAIN      -2
#define  NGX_BUSY       -3
#define  NGX_DONE       -4
#define  NGX_DECLINED   -5
#define  NGX_ABORT      -6


#include <ngx_errno.h>
#include <ngx_atomic.h>
#include <ngx_thread.h>
#include <ngx_rbtree.h>
#include <ngx_time.h>
#include <ngx_socket.h>
#include <ngx_string.h>
#include <ngx_files.h>
#include <ngx_shmem.h>
#include <ngx_process.h>
#include <ngx_user.h>
#include <ngx_parse.h>
#include <ngx_parse_time.h>
#include <ngx_log.h>
#include <ngx_alloc.h>
#include <ngx_palloc.h>
#include <ngx_buf.h>
#include <ngx_queue.h>
#include <ngx_array.h>
#include <ngx_list.h>
#include <ngx_hash.h>
#include <ngx_file.h>
#include <ngx_crc.h>
#include <ngx_crc32.h>
#include <ngx_murmurhash.h>
#if (NGX_PCRE)
#include <ngx_regex.h>
#endif
#include <ngx_radix_tree.h>
#include <ngx_times.h>
#include <ngx_rwlock.h>
#include <ngx_shmtx.h>
#include <ngx_slab.h>
#include <ngx_inet.h>
#include <ngx_cycle.h>
#include <ngx_resolver.h>
#if (NGX_OPENSSL)
#include <ngx_event_openssl.h>
#endif
#include <ngx_process_cycle.h>
#include <ngx_conf_file.h>
#include <ngx_open_file_cache.h>
#include <ngx_os.h>
#include <ngx_connection.h>
#include <ngx_syslog.h>
#include <ngx_proxy_protocol.h>


#define LF     (u_char) '\n'
#define CR     (u_char) '\r'
#define CRLF   "\r\n"


#define ngx_abs(value)       (((value) >= 0) ? (value) : - (value))
#define ngx_max(val1, val2)  ((val1 < val2) ? (val2) : (val1))
#define ngx_min(val1, val2)  ((val1 > val2) ? (val2) : (val1))

void ngx_cpuinfo(void);

#if (NGX_HAVE_OPENAT)
#define NGX_DISABLE_SYMLINKS_OFF        0
#define NGX_DISABLE_SYMLINKS_ON         1
#define NGX_DISABLE_SYMLINKS_NOTOWNER   2
#endif

#endif /* _NGX_CORE_H_INCLUDED_ */
