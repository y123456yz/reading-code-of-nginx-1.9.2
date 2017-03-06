
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_PROCESS_CYCLE_H_INCLUDED_
#define _NGX_PROCESS_CYCLE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

//打开频道，使用频道这种方式通信前必须发送的命令
#define NGX_CMD_OPEN_CHANNEL   1
//关闭已经打开的频道，实际上也就是关闭套接字
#define NGX_CMD_CLOSE_CHANNEL  2
//要求接收方正常地退出进程
#define NGX_CMD_QUIT           3
//要求接收方强制地结束进程
#define NGX_CMD_TERMINATE      4
//要求接收方重新打开进程已经打开过的文件
#define NGX_CMD_REOPEN         5


#define NGX_PROCESS_SINGLE     0 //单进程方式  //如果配置的是单进程工作模式  
#define NGX_PROCESS_MASTER     1 //正常运行的master+多worker进程模式中的主进程是该模式
#define NGX_PROCESS_SIGNALLER  2 //是nginx -s发送信号的进程
#define NGX_PROCESS_WORKER     3 //正常运行的master+多worker进程模式中的worker子进程是该模式
#define NGX_PROCESS_HELPER     4 //做过期缓存文件清理的cache_manager处于该模式

/*
static ngx_cache_manager_ctx_t  ngx_cache_manager_ctx = {
    ngx_cache_manager_process_handler, "cache manager process", 0
};
static ngx_cache_manager_ctx_t  ngx_cache_loader_ctx = {
    ngx_cache_loader_process_handler, "cache loader process", 60000  //进程创建后60000m秒执行ngx_cache_loader_process_handler,在ngx_cache_manager_process_cycle中添加的定时器
};
*/
typedef struct { //ngx_cache_manager_process_cycle
    ngx_event_handler_pt       handler; //ngx_cache_manager_process_handler  ngx_cache_loader_process_handler
    char                      *name; //进程名
    ngx_msec_t                 delay; //延迟多长时间执行上面的handler，通过定时器实现，见ngx_cache_manager_process_cycle
} ngx_cache_manager_ctx_t;


void ngx_master_process_cycle(ngx_cycle_t *cycle);
void ngx_single_process_cycle(ngx_cycle_t *cycle);


extern ngx_uint_t      ngx_process;
extern ngx_uint_t      ngx_worker;
extern ngx_pid_t       ngx_pid;
extern ngx_pid_t       ngx_new_binary;
extern ngx_uint_t      ngx_inherited;
extern ngx_uint_t      ngx_daemonized;
extern ngx_uint_t      ngx_exiting;

extern sig_atomic_t    ngx_reap;
extern sig_atomic_t    ngx_sigio;
extern sig_atomic_t    ngx_sigalrm;
extern sig_atomic_t    ngx_quit;
extern sig_atomic_t    ngx_debug_quit;
extern sig_atomic_t    ngx_terminate;
extern sig_atomic_t    ngx_noaccept;
extern sig_atomic_t    ngx_reconfigure;
extern sig_atomic_t    ngx_reopen;
extern sig_atomic_t    ngx_change_binary;


#endif /* _NGX_PROCESS_CYCLE_H_INCLUDED_ */
