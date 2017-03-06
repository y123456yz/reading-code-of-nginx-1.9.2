
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_PROCESS_H_INCLUDED_
#define _NGX_PROCESS_H_INCLUDED_


#include <ngx_setaffinity.h>
#include <ngx_setproctitle.h>


typedef pid_t       ngx_pid_t;

#define NGX_INVALID_PID  -1

/*
Worker进程的工作循环ngx_worker_process_cycle方法也是依照ngx_spawn_proc_pt来定义的
cacheManage进程或者cache_loader进程的工作循环ngx_cache_manager_process_cycle方法也是如此
*/
//ngx_spawn_process函数中调用
typedef void (*ngx_spawn_proc_pt) (ngx_cycle_t *cycle, void *data);

/*
在解释master工作流程前，还需要对master进程管理子进程的数据结构有个初步了解。下面定义了pgx_processes全局数组，虽然子进程中也会
有ngx_processes数组，但这个数组仅仅是给master进程使用的

master进程中所有子进程相关的状态信息都保存在ngx_processes数组中。再来看一下数组元素的类型ngx_process_t结构体的定义，代码如下。
*/
typedef struct {
    ngx_pid_t           pid; //进程ID
    int                 status; //子进程退出后，父进程收到SIGCHLD后，开始waitpid，父进程由waitpid系统调用获取到的进程状态，见ngx_process_get_status

    /*
    这是由socketpair系统调用产生出的用于进程间通信的socket句柄，这一对socket句柄可以互相通信，目前用于master父进程与worker子进程问的通信，详见14.4节
    */
    ngx_socket_t        channel[2];//socketpair实际上是通过管道封装实现的 ngx_spawn_process中赋值

    //子进程的循环执行方法，当父进程调用ngx_spawn_proces生成子进程时使用
    ngx_spawn_proc_pt   proc;

    /*
    上面的ngx_spawn_proc_pt方法中第2个参数雷要传递1个指针，它是可选的。例如，worker子进程就不需要，而cache manage进程
    就需要ngx_cache_manager_ctx上下文成员。这时，data一般与ngx_spawn_proc_pt方法中第2个参数是等价的
    */
    void               *data;
    char               *name;//进程名称。操作系统中显示的进程名称与name相同

    //一下这些前三个标记在ngx_spawn_process中赋值
    unsigned            respawn:1; //标志位，为1时表示在重新生成子进程
    unsigned            just_spawn:1; //标志位，为1时表示正在生成子进程
    unsigned            detached:1; //标志位，为1时表示在进行父、子进程分离
    unsigned            exiting:1;//标志位，为1时表示进程正在退出
    unsigned            exited:1;//标志位，为1时表示进程已经退出  当子进程退出后，父进程收到SIGCHLD后，开始waitpid,见ngx_process_get_status
} ngx_process_t;

typedef struct {//赋值见ngx_exec_new_binary
    char         *path; 
    char         *name;
    char *const  *argv;
    char *const  *envp;
} ngx_exec_ctx_t;

//定义1024个元素的ngx_processes数组，也就是最多只能有1024个子进程
#define NGX_MAX_PROCESSES         1024

/*
在分析ngx_spawn_process()创建新进程时，先了解下进程属性。通俗点说就是进程挂了需不需要重启。
在源码中，nginx_process.h中，有以下几种属性标识：

NGX_PROCESS_NORESPAWN    ：子进程退出时,父进程不会再次重启
NGX_PROCESS_JUST_SPAWN   ：--
NGX_PROCESS_RESPAWN      ：子进程异常退出时,父进程需要重启
NGX_PROCESS_JUST_RESPAWN ：--
NGX_PROCESS_DETACHED     ：热代码替换，暂时估计是用于在不重启Nginx的情况下进行软件升级

NGX_PROCESS_JUST_RESPAWN标识最终会在ngx_spawn_process()创建worker进程时，将ngx_processes[s].just_spawn = 1，以此作为区别旧的worker进程的标记。
*/
#define NGX_PROCESS_NORESPAWN     -1  //子进程退出时,父进程不会再次重启
#define NGX_PROCESS_JUST_SPAWN    -2
#define NGX_PROCESS_RESPAWN       -3  //子进程异常退出时,父进程需要重启
#define NGX_PROCESS_JUST_RESPAWN  -4   
#define NGX_PROCESS_DETACHED      -5  //热代码替换，暂时估计是用于在不重启Nginx的情况下进行软件升级


#define ngx_getpid   getpid

#ifndef ngx_log_pid
#define ngx_log_pid  ngx_pid 
//ngx_log_pid, ngx_log_tid进程ID和线程ID(主线程号和进程号相同，在开启线程池的时候线程ID和进程ID不同),日志文件中会记录
#endif


ngx_pid_t ngx_spawn_process(ngx_cycle_t *cycle,
    ngx_spawn_proc_pt proc, void *data, char *name, ngx_int_t respawn);
ngx_pid_t ngx_execute(ngx_cycle_t *cycle, ngx_exec_ctx_t *ctx);
ngx_int_t ngx_init_signals(ngx_log_t *log);
void ngx_debug_point(void);


#if (NGX_HAVE_SCHED_YIELD)
#define ngx_sched_yield()  sched_yield()
#else
#define ngx_sched_yield()  usleep(1)
#endif


extern int            ngx_argc;
extern char         **ngx_argv;
extern char         **ngx_os_argv;

extern ngx_pid_t      ngx_pid;
extern ngx_socket_t   ngx_channel;
extern ngx_int_t      ngx_process_slot;
extern ngx_int_t      ngx_last_process;
extern ngx_process_t  ngx_processes[NGX_MAX_PROCESSES];


#endif /* _NGX_PROCESS_H_INCLUDED_ */
