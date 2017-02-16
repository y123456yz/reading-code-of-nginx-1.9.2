
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CHANNEL_H_INCLUDED_
#define _NGX_CHANNEL_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>

//封装了父子进程之间传递的信息。  
//父进程创建好子进程后，通过这个把该子进程的相关信息全部传递给其他所有进程，这样其他子进程就知道该进程的channel信息了。
//因为Nginx仅用这个频道同步master进程与worker进程间的状态
//见ngx_start_worker_processes->ngx_pass_open_channel
typedef struct { 
     ngx_uint_t  command; //对端将要做得命令  取值为NGX_CMD_OPEN_CHANNEL等
     ngx_pid_t   pid;  //当前的子进程id   进程ID，一般是发送命令方的进程ID
     ngx_int_t   slot; //在全局进程表中的位置    表示发送命令方在ngx_processes进程数组间的序号
     ngx_fd_t    fd; //传递的fd   通信的套接字句柄
} ngx_channel_t;


ngx_int_t ngx_write_channel(ngx_socket_t s, ngx_channel_t *ch, size_t size,
    ngx_log_t *log);
ngx_int_t ngx_read_channel(ngx_socket_t s, ngx_channel_t *ch, size_t size,
    ngx_log_t *log);
ngx_int_t ngx_add_channel_event(ngx_cycle_t *cycle, ngx_fd_t fd,
    ngx_int_t event, ngx_event_handler_pt handler);
void ngx_close_channel(ngx_fd_t *fd, ngx_log_t *log);

#endif /* _NGX_CHANNEL_H_INCLUDED_ */

