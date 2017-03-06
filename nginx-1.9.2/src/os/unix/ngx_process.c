
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_channel.h>


//信号发送见ngx_os_signal_process
typedef struct { //signals
    int     signo;   //需要处理的信号
    char   *signame; //信号对应的字符串名称
    char   *name;    //这个信号对应着的Nginx命令
    void  (*handler)(int signo); //收到signo信号后就会回调handler方法
} ngx_signal_t;

static void ngx_execute_proc(ngx_cycle_t *cycle, void *data);
static void ngx_signal_handler(int signo);
static void ngx_process_get_status(void);
static void ngx_unlock_mutexes(ngx_pid_t pid);

int              ngx_argc;
char           **ngx_argv; //存放执行nginx时候所带的参数， 见ngx_save_argv
char           **ngx_os_argv; //指向nginx运行时候所带的参数，见ngx_save_argv

//当前操作的进程在ngx_processes数组中的下标
ngx_int_t        ngx_process_slot;
ngx_socket_t     ngx_channel;  //存储所有子进程的数组  ngx_spawn_process中赋值  ngx_channel = ngx_processes[s].channel[1]

//ngx_processes数组中有意义的ngx_process_t元素中最大的下标
ngx_int_t        ngx_last_process;

/*
在解释master工作流程前，还需要对master进程管理子进程的数据结构有个初步了解。下面定义了pgx_processes全局数组，虽然子进程中也会
有ngx_processes数组，但这个数组仅仅是给master进程使用的
*/
ngx_process_t    ngx_processes[NGX_MAX_PROCESSES]; //存储所有子进程的数组  ngx_spawn_process中赋值

//信号发送见ngx_os_signal_process 信号处理在ngx_signal_handler
ngx_signal_t  signals[] = {
    { ngx_signal_value(NGX_RECONFIGURE_SIGNAL),
      "SIG" ngx_value(NGX_RECONFIGURE_SIGNAL),
      "reload",
      /* reload实际上是执行reload的nginx进程向原master+worker中的master进程发送reload信号，源master收到后，启动新的worker进程，同时向源worker
         进程发送quit信号，等他们处理完已有的数据信息后，退出，这样就只有新的worker进程运行。
      */
      ngx_signal_handler }, 

    { ngx_signal_value(NGX_REOPEN_SIGNAL),
      "SIG" ngx_value(NGX_REOPEN_SIGNAL),
      "reopen",
      ngx_signal_handler },

    { ngx_signal_value(NGX_NOACCEPT_SIGNAL),
      "SIG" ngx_value(NGX_NOACCEPT_SIGNAL),
      "",
      ngx_signal_handler },

    { ngx_signal_value(NGX_TERMINATE_SIGNAL),
      "SIG" ngx_value(NGX_TERMINATE_SIGNAL),
      "stop",
      ngx_signal_handler },

    { ngx_signal_value(NGX_SHUTDOWN_SIGNAL),
      "SIG" ngx_value(NGX_SHUTDOWN_SIGNAL),
      "quit",
      ngx_signal_handler },

    { ngx_signal_value(NGX_CHANGEBIN_SIGNAL),
      "SIG" ngx_value(NGX_CHANGEBIN_SIGNAL),
      "",
      ngx_signal_handler },

    { SIGALRM, "SIGALRM", "", ngx_signal_handler },

    { SIGINT, "SIGINT", "", ngx_signal_handler },

    { SIGIO, "SIGIO", "", ngx_signal_handler },

    { SIGCHLD, "SIGCHLD", "", ngx_signal_handler },

    { SIGSYS, "SIGSYS, SIG_IGN", "", SIG_IGN },

    { SIGPIPE, "SIGPIPE, SIG_IGN", "", SIG_IGN },

    { 0, NULL, "", NULL }
};

/*
master进程怎样启动一个子进程呢？其实很简单，fork系统调用即可以完成。ngx_spawn_process方法封装了fork系统调用，
并且会从ngx_processes数组中选择一个还未使用的ngx_process_t元素存储这个子进程的相关信息。如果所有1024个数纽元素中已经没有空
余的元素，也就是说，子进程个数超过了最大值1024，那么将会返回NGX_INVALID_PID。因此，ngx_processes数组中元素的初始化将在ngx_spawn_process方法中进行。
*/
//第一个参数是全局的配置，第二个参数是子进程需要执行的函数，第三个参数是proc的参数。第四个类型。  name是子进程的名称
ngx_pid_t ngx_spawn_process(ngx_cycle_t *cycle, ngx_spawn_proc_pt proc, void *data,
    char *name, ngx_int_t respawn) //respawn取值为NGX_PROCESS_RESPAWN等，或者为进程在ngx_processes[]中的序号
{
    u_long     on;
    ngx_pid_t  pid;
    ngx_int_t  s; //将要创建的子进程在进程表中的位置   

    // 如果respawn不小于0，则视为当前进程已经退出，需要重启  
    if (respawn >= 0) {  //替换进程ngx_processes[respawn],可安全重用该进程表项  
        s = respawn; 

    } else {
        for (s = 0; s < ngx_last_process; s++) { 
            if (ngx_processes[s].pid == -1) { //先找到一个被回收的进程表象   
                break;
            }
        }

        if (s == NGX_MAX_PROCESSES) { //最多只能创建1024个子进程
            ngx_log_error(NGX_LOG_ALERT, cycle->log, 0,
                          "no more than %d processes can be spawned",
                          NGX_MAX_PROCESSES);
            return NGX_INVALID_PID;
        }
    }


    if (respawn != NGX_PROCESS_DETACHED) { //不是分离的子进程      /* 不是热代码替换 */

        /* Solaris 9 still has no AF_LOCAL */
       
        /* 
          这里相当于Master进程调用socketpair()为新的worker进程创建一对全双工的socket 
            
          实际上socketpair 函数跟pipe 函数是类似的，也只能在同个主机上具有亲缘关系的进程间通信，但pipe 创建的匿名管道是半双工的，
          而socketpair 可以认为是创建一个全双工的管道。

          int socketpair(int domain, int type, int protocol, int sv[2]);
          这个方法可以创建一对关联的套接字sv[2]。下面依次介绍它的4个参数：参数d表示域，在Linux下通常取值为AF UNIX；type取值为SOCK。
          STREAM或者SOCK。DGRAM，它表示在套接字上使用的是TCP还是UDP; protocol必须传递0；sv[2]是一个含有两个元素的整型数组，实际上就
          是两个套接字。当socketpair返回0时，sv[2]这两个套接字创建成功，否则socketpair返回一1表示失败。
             当socketpair执行成功时，sv[2]这两个套接字具备下列关系：向sv[0]套接字写入数据，将可以从sv[l]套接字中读取到刚写入的数据；
          同样，向sv[l]套接字写入数据，也可以从sv[0]中读取到写入的数据。通常，在父、子进程通信前，会先调用socketpair方法创建这样一组
          套接字，在调用fork方法创建出子进程后，将会在父进程中关闭sv[l]套接字，仅使用sv[0]套接字用于向子进程发送数据以及接收子进程发
          送来的数据：而在子进程中则关闭sv[0]套接字，仅使用sv[l]套接字既可以接收父进程发来的数据，也可以向父进程发送数据。
          注意socketpair的协议族为AF_UNIX UNXI域
          */  
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, ngx_processes[s].channel) == -1) //在ngx_worker_process_init中添加到事件集
        {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          "socketpair() failed while spawning \"%s\"", name);
            return NGX_INVALID_PID;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                       "channel %d:%d",
                       ngx_processes[s].channel[0],
                       ngx_processes[s].channel[1]);

        /* 设置master的channel[0](即写端口)，channel[1](即读端口)均为非阻塞方式 */  
        if (ngx_nonblocking(ngx_processes[s].channel[0]) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          ngx_nonblocking_n " failed while spawning \"%s\"",
                          name);
            ngx_close_channel(ngx_processes[s].channel, cycle->log);
            return NGX_INVALID_PID;
        }

        if (ngx_nonblocking(ngx_processes[s].channel[1]) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          ngx_nonblocking_n " failed while spawning \"%s\"",
                          name);
            ngx_close_channel(ngx_processes[s].channel, cycle->log);
            return NGX_INVALID_PID;
        }
        
        /* 
            设置异步模式： 这里可以看下《网络编程卷一》的ioctl函数和fcntl函数 or 网上查询 
          */  
        on = 1; // 标记位，ioctl用于清除（0）或设置（非0）操作  

        /* 
          设置channel[0]的信号驱动异步I/O标志 
          FIOASYNC：该状态标志决定是否收取针对socket的异步I/O信号（SIGIO） 
          其与O_ASYNC文件状态标志等效，可通过fcntl的F_SETFL命令设置or清除 
         */ 
        if (ioctl(ngx_processes[s].channel[0], FIOASYNC, &on) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          "ioctl(FIOASYNC) failed while spawning \"%s\"", name);
            ngx_close_channel(ngx_processes[s].channel, cycle->log);
            return NGX_INVALID_PID;
        }

        /* F_SETOWN：用于指定接收SIGIO和SIGURG信号的socket属主（进程ID或进程组ID） 
          * 这里意思是指定Master进程接收SIGIO和SIGURG信号 
          * SIGIO信号必须是在socket设置为信号驱动异步I/O才能产生，即上一步操作 
          * SIGURG信号是在新的带外数据到达socket时产生的 
         */ 
        if (fcntl(ngx_processes[s].channel[0], F_SETOWN, ngx_pid) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          "fcntl(F_SETOWN) failed while spawning \"%s\"", name);
            ngx_close_channel(ngx_processes[s].channel, cycle->log);
            return NGX_INVALID_PID;
        }
            
        
        /* FD_CLOEXEC：用来设置文件的close-on-exec状态标准 
          *             在exec()调用后，close-on-exec标志为0的情况下，此文件不被关闭；非零则在exec()后被关闭 
          *             默认close-on-exec状态为0，需要通过FD_CLOEXEC设置 
          *     这里意思是当Master父进程执行了exec()调用后，关闭socket        
          */
        if (fcntl(ngx_processes[s].channel[0], F_SETFD, FD_CLOEXEC) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          "fcntl(FD_CLOEXEC) failed while spawning \"%s\"",
                           name);
            ngx_close_channel(ngx_processes[s].channel, cycle->log);
            return NGX_INVALID_PID;
        }

        
        /* 同上，这里意思是当Worker子进程执行了exec()调用后，关闭socket */  
        if (fcntl(ngx_processes[s].channel[1], F_SETFD, FD_CLOEXEC) == -1) {
            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          "fcntl(FD_CLOEXEC) failed while spawning \"%s\"",
                           name);
            ngx_close_channel(ngx_processes[s].channel, cycle->log);
            return NGX_INVALID_PID;
        }
        
       /* 
         设置即将创建子进程的channel ，这个在后面会用到，在后面创建的子进程的cycle循环执行函数中会用到，例如ngx_worker_process_init -> ngx_add_channel_event   
         从而把子进程的channel[1]读端添加到epool中，用于读取父进程发送的ngx_channel_t信息
        */  
        ngx_channel = ngx_processes[s].channel[1];

    } else {
        ngx_processes[s].channel[0] = -1;
        ngx_processes[s].channel[1] = -1;
    }

    ngx_process_slot = s; // 这一步将在ngx_pass_open_channel()中用到，就是设置下标，用于寻找本次创建的子进程  

    pid = fork();

    switch (pid) {

    case -1:
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      "fork() failed while spawning \"%s\"", name);
        ngx_close_channel(ngx_processes[s].channel, cycle->log);
        return NGX_INVALID_PID;

    case 0: //子进程
        ngx_pid = ngx_getpid();  // 设置子进程ID  
        //printf(" .....slave......pid:%u, %u\n", pid, ngx_pid); slave......pid:0, 14127
        proc(cycle, data); // 调用proc回调函数，即ngx_worker_process_cycle。之后worker子进程从这里开始执行  
        break;

    default: //父进程，但是这时候打印的pid为子进程ID
        //printf(" ......master.....pid:%u, %u\n", pid, ngx_pid); master.....pid:14127, 14126
        break;
    }

    ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "start %s %P", name, pid);

    /* 这一部分用来设置ngx_process_t的成员变量 */  
    ngx_processes[s].pid = pid;
    ngx_processes[s].exited = 0;

    if (respawn >= 0) { /* 如果大于0,则说明是在重启子进程，因此下面的初始化不用再重复做 */  
        return pid;
    }

    ngx_processes[s].proc = proc;
    ngx_processes[s].data = data;
    ngx_processes[s].name = name;
    ngx_processes[s].exiting = 0;

    switch (respawn) {/* OK，也不多说了，用来设置状态信息 */  

    case NGX_PROCESS_NORESPAWN:
        ngx_processes[s].respawn = 0;
        ngx_processes[s].just_spawn = 0;
        ngx_processes[s].detached = 0;
        break;

    case NGX_PROCESS_JUST_SPAWN:
        ngx_processes[s].respawn = 0;
        ngx_processes[s].just_spawn = 1;
        ngx_processes[s].detached = 0;
        break;

    case NGX_PROCESS_RESPAWN:
        ngx_processes[s].respawn = 1;
        ngx_processes[s].just_spawn = 0;
        ngx_processes[s].detached = 0;
        break;

    case NGX_PROCESS_JUST_RESPAWN:
        ngx_processes[s].respawn = 1;
        ngx_processes[s].just_spawn = 1;
        ngx_processes[s].detached = 0;
        break;

    case NGX_PROCESS_DETACHED:
        ngx_processes[s].respawn = 0;
        ngx_processes[s].just_spawn = 0;
        ngx_processes[s].detached = 1;
        break;
    }

    if (s == ngx_last_process) {
        ngx_last_process++;
    }

    return pid;
}


ngx_pid_t
ngx_execute(ngx_cycle_t *cycle, ngx_exec_ctx_t *ctx)
{
    return ngx_spawn_process(cycle, ngx_execute_proc, ctx, ctx->name,
                             NGX_PROCESS_DETACHED);
}


static void
ngx_execute_proc(ngx_cycle_t *cycle, void *data)
{
    ngx_exec_ctx_t  *ctx = data;

    /*
    execve()用来执行参数filename字符串所代表的文件路径，第二个参数是利用指针数组来传递给执行文件，并且需
    要以空指针(NULL)结束，最后一个参数则为传递给执行文件的新环境变量数组。
    */
    if (execve(ctx->path, ctx->argv, ctx->envp) == -1) { //把旧master进程bind监听的fd写入到环境变量NGINX_VAR中
        ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                      "execve() failed while executing %s \"%s\"",
                      ctx->name, ctx->path);
    }

    exit(1);
}

/*
一、信号及信号来源
信号本质

信号是在软件层次上对中断机制的一种模拟，在原理上，一个进程收到一个信号与处理器收到一个中断请求可以说是一样的。信号是异步的，一个进程不必通过任何操作来等待信号的到达，事实上，进程也不知道信号到底什么时候到达。

信号是进程间通信机制中唯一的异步通信机制，可以看作是异步通知，通知接收信号的进程有哪些事情发生了。信号机制经过POSIX实时扩展后，功能更加强大，除了基本通知功能外，还可以传递附加信息。

信号来源

信号事件的发生有两个来源：硬件来源(比如我们按下了键盘或者其它硬件故障)；软件来源，最常用发送信号的系统函数是kill, raise, alarm和setitimer以及sigqueue函数，软件来源还包括一些非法运算等操作。


--------------------------------------------------------------------------------
回页首
二、信号的种类
可以从两个不同的分类角度对信号进行分类：（1）可靠性方面：可靠信号与不可靠信号；（2）与时间的关系上：实时信号与非实时信号。在《Linux环境进程间通信（一）：管道及有名管道》的附1中列出了系统所支持的所有信号。

1、可靠信号与不可靠信号
"不可靠信号"

Linux信号机制基本上是从Unix系统中继承过来的。早期Unix系统中的信号机制比较简单和原始，后来在实践中暴露出一些问题，因此，把那些建立在早期机制上的信号叫做"不可靠信号"，信号值小于SIGRTMIN(Red hat 7.2中，SIGRTMIN=32，SIGRTMAX=63)的信号都是不可靠信号。这就是"不可靠信号"的来源。它的主要问题是：

进程每次处理信号后，就将对信号的响应设置为默认动作。在某些情况下，将导致对信号的错误处理；因此，用户如果不希望这样的操作，那么就要在信号处理函数结尾再一次调用signal()，重新安装该信号。
信号可能丢失，后面将对此详细阐述。 
因此，早期unix下的不可靠信号主要指的是进程可能对信号做出错误的反应以及信号可能丢失。 Linux支持不可靠信号，但是对不可靠信号机制做了改进：在调用完信号处理函数后，不必重新调用该信号的安装函数（信号安装函数是在可靠机制上的实现）。因此，Linux下的不可靠信号问题主要指的是信号可能丢失。

"可靠信号"

随着时间的发展，实践证明了有必要对信号的原始机制加以改进和扩充。所以，后来出现的各种Unix版本分别在这方面进行了研究，力图实现"可靠信号"。由于原来定义的信号已有许多应用，不好再做改动，最终只好又新增加了一些信号，并在一开始就把它们定义为可靠信号，这些信号支持排队，不会丢失。同时，信号的发送和安装也出现了新版本：信号发送函数sigqueue()及信号安装函数sigaction()。POSIX.4对可靠信号机制做了标准化。但是，POSIX只对可靠信号机制应具有的功能以及信号机制的对外接口做了标准化，对信号机制的实现没有作具体的规定。

信号值位于SIGRTMIN和SIGRTMAX之间的信号都是可靠信号，可靠信号克服了信号可能丢失的问题。Linux在支持新版本的信号安装函数sigation（）以及信号发送函数sigqueue()的同时，仍然支持早期的signal（）信号安装函数，支持信号发送函数kill()。

注：不要有这样的误解：由sigqueue()发送、sigaction安装的信号就是可靠的。事实上，可靠信号是指后来添加的新信号（信号值位于SIGRTMIN及SIGRTMAX之间）；不可靠信号是信号值小于SIGRTMIN的信号。信号的可靠与不可靠只与信号值有关，与信号的发送及安装函数无关。目前linux中的signal()是通过sigation()函数实现的，因此，即使通过signal（）安装的信号，在信号处理函数的结尾也不必再调用一次信号安装函数。同时，由signal()安装的实时信号支持排队，同样不会丢失。

对于目前linux的两个信号安装函数:signal()及sigaction()来说，它们都不能把SIGRTMIN以前的信号变成可靠信号（都不支持排队，仍有可能丢失，仍然是不可靠信号），而且对SIGRTMIN以后的信号都支持排队。这两个函数的最大区别在于，经过sigaction安装的信号都能传递信息给信号处理函数（对所有信号这一点都成立），而经过signal安装的信号却不能向信号处理函数传递信息。对于信号发送函数来说也是一样的。

2、实时信号与非实时信号
早期Unix系统只定义了32种信号，Ret hat7.2支持64种信号，编号0-63(SIGRTMIN=31，SIGRTMAX=63)，将来可能进一步增加，这需要得到内核的支持。前32种信号已经有了预定义值，每个信号有了确定的用途及含义，并且每种信号都有各自的缺省动作。如按键盘的CTRL ^C时，会产生SIGINT信号，对该信号的默认反应就是进程终止。后32个信号表示实时信号，等同于前面阐述的可靠信号。这保证了发送的多个实时信号都被接收。实时信号是POSIX标准的一部分，可用于应用进程。

非实时信号都不支持排队，都是不可靠信号；实时信号都支持排队，都是可靠信号。


--------------------------------------------------------------------------------
回页首
三、进程对信号的响应
进程可以通过三种方式来响应一个信号：（1）忽略信号，即对信号不做任何处理，其中，有两个信号不能忽略：SIGKILL及SIGSTOP；（2）捕捉信号。定义信号处理函数，当信号发生时，执行相应的处理函数；（3）执行缺省操作，Linux对每种信号都规定了默认操作，详细情况请参考[2]以及其它资料。注意，进程对实时信号的缺省反应是进程终止。

Linux究竟采用上述三种方式的哪一个来响应信号，取决于传递给相应API函数的参数。


--------------------------------------------------------------------------------
回页首
四、信号的发送
发送信号的主要函数有：kill()、raise()、 sigqueue()、alarm()、setitimer()以及abort()。

1、kill() 
#include <sys/types.h> 
#include <signal.h> 
int kill(pid_t pid,int signo) 

参数pid的值 信号的接收进程 
pid>0 进程ID为pid的进程 
pid=0 同一个进程组的进程 
pid<0 pid!=-1 进程组ID为 -pid的所有进程 
pid=-1 除发送进程自身外，所有进程ID大于1的进程 

Sinno是信号值，当为0时（即空信号），实际不发送任何信号，但照常进行错误检查，因此，可用于检查目标进程是否存在，以及当前进程是否具有向目标发送信号的权限（root权限的进程可以向任何进程发送信号，非root权限的进程只能向属于同一个session或者同一个用户的进程发送信号）。

Kill()最常用于pid>0时的信号发送，调用成功返回 0； 否则，返回 -1。注：对于pid<0时的情况，对于哪些进程将接受信号，各种版本说法不一，其实很简单，参阅内核源码kernal/signal.c即可，上表中的规则是参考red hat 7.2。

2、raise（） 
#include <signal.h> 
int raise(int signo) 
向进程本身发送信号，参数为即将发送的信号值。调用成功返回 0；否则，返回 -1。 

3、sigqueue（） 
#include <sys/types.h> 
#include <signal.h> 
int sigqueue(pid_t pid, int sig, const union sigval val) 
调用成功返回 0；否则，返回 -1。 

sigqueue()是比较新的发送信号系统调用，主要是针对实时信号提出的（当然也支持前32种），支持信号带有参数，与函数sigaction()配合使用。

sigqueue的第一个参数是指定接收信号的进程ID，第二个参数确定即将发送的信号，第三个参数是一个联合数据结构union sigval，指定了信号传递的参数，即通常所说的4字节值。

 	typedef union sigval {
 		int  sival_int;
 		void *sival_ptr;
 	}sigval_t;sigqueue()比kill()传递了更多的附加信息，但sigqueue()只能向一个进程发送信号，而不能发送信号给一个进程组。如果signo=0，将会执行错误检查，但实际上不发送任何信号，0值信号可用于检查pid的有效性以及当前进程是否有权限向目标进程发送信号。

在调用sigqueue时，sigval_t指定的信息会拷贝到3参数信号处理函数（3参数信号处理函数指的是信号处理函数由sigaction安装，并设定了sa_sigaction指针，稍后将阐述）的siginfo_t结构中，这样信号处理函数就可以处理这些信息了。由于sigqueue系统调用支持发送带参数信号，所以比kill()系统调用的功能要灵活和强大得多。

注：sigqueue（）发送非实时信号时，第三个参数包含的信息仍然能够传递给信号处理函数； sigqueue（）发送非实时信号时，仍然不支持排队，即在信号处理函数执行过程中到来的所有相同信号，都被合并为一个信号。

4、alarm（） 
#include <unistd.h> 
unsigned int alarm(unsigned int seconds) 
专门为SIGALRM信号而设，在指定的时间seconds秒后，将向进程本身发送SIGALRM信号，又称为闹钟时间。进程调用alarm后，任何以前的alarm()调用都将无效。如果参数seconds为零，那么进程内将不再包含任何闹钟时间。 
返回值，如果调用alarm（）前，进程中已经设置了闹钟时间，则返回上一个闹钟时间的剩余时间，否则返回0。 

5、setitimer（） 
#include <sys/time.h> 
int setitimer(int which, const struct itimerval *value, struct itimerval *ovalue)); 
setitimer()比alarm功能强大，支持3种类型的定时器： 

ITIMER_REAL： 设定绝对时间；经过指定的时间后，内核将发送SIGALRM信号给本进程；
ITIMER_VIRTUAL 设定程序执行时间；经过指定的时间后，内核将发送SIGVTALRM信号给本进程；
ITIMER_PROF 设定进程执行以及内核因本进程而消耗的时间和，经过指定的时间后，内核将发送ITIMER_VIRTUAL信号给本进程；
Setitimer()第一个参数which指定定时器类型（上面三种之一）；第二个参数是结构itimerval的一个实例，结构itimerval形式见附录1。第三个参数可不做处理。

Setitimer()调用成功返回0，否则返回-1。

6、abort() 
#include <stdlib.h> 
void abort(void); 

向进程发送SIGABORT信号，默认情况下进程会异常退出，当然可定义自己的信号处理函数。即使SIGABORT被进程设置为阻塞信号，调用abort()后，SIGABORT仍然能被进程接收。该函数无返回值。


--------------------------------------------------------------------------------
回页首
五、信号的安装（设置信号关联动作）
如果进程要处理某一信号，那么就要在进程中安装该信号。安装信号主要用来确定信号值及进程针对该信号值的动作之间的映射关系，即进程将要处理哪个信号；该信号被传递给进程时，将执行何种操作。

linux主要有两个函数实现信号的安装：signal()、sigaction()。其中signal()在可靠信号系统调用的基础上实现, 是库函数。它只有两个参数，不支持信号传递信息，主要是用于前32种非实时信号的安装；而sigaction()是较新的函数（由两个系统调用实现：sys_signal以及sys_rt_sigaction），有三个参数，支持信号传递信息，主要用来与 sigqueue() 系统调用配合使用，当然，sigaction()同样支持非实时信号的安装。sigaction()优于signal()主要体现在支持信号带有参数。

1、signal() 
#include <signal.h> 
void (*signal(int signum, void (*handler))(int)))(int); 
如果该函数原型不容易理解的话，可以参考下面的分解方式来理解： 
typedef void (*sighandler_t)(int)； 
sighandler_t signal(int signum, sighandler_t handler)); 
第一个参数指定信号的值，第二个参数指定针对前面信号值的处理，可以忽略该信号（参数设为SIG_IGN）；可以采用系统默认方式处理信号(参数设为SIG_DFL)；也可以自己实现处理方式(参数指定一个函数地址)。 
如果signal()调用成功，返回最后一次为安装信号signum而调用signal()时的handler值；失败则返回SIG_ERR。 

2、sigaction() 
#include <signal.h> 
int sigaction(int signum,const struct sigaction *act,struct sigaction *oldact)); 

sigaction函数用于改变进程接收到特定信号后的行为。该函数的第一个参数为信号的值，可以为除SIGKILL及SIGSTOP外的任何一个特定有效的信号（为这两个信号定义自己的处理函数，将导致信号安装错误）。第二个参数是指向结构sigaction的一个实例的指针，在结构sigaction的实例中，指定了对特定信号的处理，可以为空，进程会以缺省方式对信号处理；第三个参数oldact指向的对象用来保存原来对相应信号的处理，可指定oldact为NULL。如果把第二、第三个参数都设为NULL，那么该函数可用于检查信号的有效性。

第二个参数最为重要，其中包含了对指定信号的处理、信号所传递的信息、信号处理函数执行过程中应屏蔽掉哪些函数等等。

sigaction结构定义如下：

 struct sigaction {
          union{
            __sighandler_t _sa_handler;
            void (*_sa_sigaction)(int,struct siginfo *, void *)；
            }_u
                     sigset_t sa_mask；
                    unsigned long sa_flags； 
                  void (*sa_restorer)(void)；
                  }其中，sa_restorer，已过时，POSIX不支持它，不应再被使用。

1、联合数据结构中的两个元素_sa_handler以及*_sa_sigaction指定信号关联函数，即用户指定的信号处理函数。除了可以是用户自定义的处理函数外，还可以为SIG_DFL(采用缺省的处理方式)，也可以为SIG_IGN（忽略信号）。

2、由_sa_handler指定的处理函数只有一个参数，即信号值，所以信号不能传递除信号值之外的任何信息；由_sa_sigaction是指定的信号处理函数带有三个参数，是为实时信号而设的（当然同样支持非实时信号），它指定一个3参数信号处理函数。第一个参数为信号值，第三个参数没有使用（posix没有规范使用该参数的标准），第二个参数是指向siginfo_t结构的指针，结构中包含信号携带的数据值，参数所指向的结构如下：

 siginfo_t {
                  int      si_signo;   信号值，对所有信号有意义
                  int      si_errno;   errno值，对所有信号有意义
                  int      si_code;    信号产生的原因，对所有信有意义
        union{          联合数据结构，不同成员适应不同信号
          //确保分配足够大的存储空间
          int _pad[SI_PAD_SIZE];
          //对SIGKILL有意义的结构
          struct{
              ...
              }...
            ... ...
            ... ...          
          //对SIGILL, SIGFPE, SIGSEGV, SIGBUS有意义的结构
              struct{
              ...
              }...
            ... ...
            }
      }注：为了更便于阅读，在说明问题时常把该结构表示为附录2所表示的形式。

siginfo_t结构中的联合数据成员确保该结构适应所有的信号，比如对于实时信号来说，则实际采用下面的结构形式：

	typedef struct {
		int si_signo;
		int si_errno;			
		int si_code;			
		union sigval si_value;	
		} siginfo_t;结构的第四个域同样为一个联合数据结构：

	union sigval {
		int sival_int;		
		void *sival_ptr;	
		}采用联合数据结构，说明siginfo_t结构中的si_value要么持有一个4字节的整数值，要么持有一个指针，这就构成了与信号相关的数据。在信号的处理函数中，包含这样的信号相关数据指针，但没有规定具体如何对这些数据进行操作，操作方法应该由程序开发人员根据具体任务事先约定。

前面在讨论系统调用sigqueue发送信号时，sigqueue的第三个参数就是sigval联合数据结构，当调用sigqueue时，该数据结构中的数据就将拷贝到信号处理函数的第二个参数中。这样，在发送信号同时，就可以让信号传递一些附加信息。信号可以传递信息对程序开发是非常有意义的。

信号参数的传递过程可图示如下：


3、sa_mask指定在信号处理程序执行过程中，哪些信号应当被阻塞。缺省情况下当前信号本身被阻塞，防止信号的嵌套发送，除非指定SA_NODEFER或者SA_NOMASK标志位。

注：请注意sa_mask指定的信号阻塞的前提条件，是在由sigaction（）安装信号的处理函数执行过程中由sa_mask指定的信号才被阻塞。

4、sa_flags中包含了许多标志位，包括刚刚提到的SA_NODEFER及SA_NOMASK标志位。另一个比较重要的标志位是SA_SIGINFO，当设定了该标志位时，表示信号附带的参数可以被传递到信号处理函数中，因此，应该为sigaction结构中的sa_sigaction指定处理函数，而不应该为sa_handler指定信号处理函数，否则，设置该标志变得毫无意义。即使为sa_sigaction指定了信号处理函数，如果不设置SA_SIGINFO，信号处理函数同样不能得到信号传递过来的数据，在信号处理函数中对这些信息的访问都将导致段错误（Segmentation fault）。

注：很多文献在阐述该标志位时都认为，如果设置了该标志位，就必须定义三参数信号处理函数。实际不是这样的，验证方法很简单：自己实现一个单一参数信号处理函数，并在程序中设置该标志位，可以察看程序的运行结果。实际上，可以把该标志位看成信号是否传递参数的开关，如果设置该位，则传递参数；否则，不传递参数。


--------------------------------------------------------------------------------
回页首
六、信号集及信号集操作函数：
信号集被定义为一种数据类型：

	typedef struct {
			unsigned long sig[_NSIG_WORDS]；
			} sigset_t信号集用来描述信号的集合，linux所支持的所有信号可以全部或部分的出现在信号集中，主要与信号阻塞相关函数配合使用。下面是为信号集操作定义的相关函数：

	#include <signal.h>
int sigemptyset(sigset_t *set)；
int sigfillset(sigset_t *set)；
int sigaddset(sigset_t *set, int signum)
int sigdelset(sigset_t *set, int signum)；
int sigismember(const sigset_t *set, int signum)；
sigemptyset(sigset_t *set)初始化由set指定的信号集，信号集里面的所有信号被清空；
sigfillset(sigset_t *set)调用该函数后，set指向的信号集中将包含linux支持的64种信号；
sigaddset(sigset_t *set, int signum)在set指向的信号集中加入signum信号；
sigdelset(sigset_t *set, int signum)在set指向的信号集中删除signum信号；
sigismember(const sigset_t *set, int signum)判定信号signum是否在set指向的信号集中。
--------------------------------------------------------------------------------
回页首
七、信号阻塞与信号未决:
每个进程都有一个用来描述哪些信号递送到进程时将被阻塞的信号集，该信号集中的所有信号在递送到进程后都将被阻塞。下面是与信号阻塞相关的几个函数：

#include <signal.h>
int  sigprocmask(int  how,  const  sigset_t *set, sigset_t *oldset))；
int sigpending(sigset_t *set));
int sigsuspend(const sigset_t *mask))；sigprocmask()函数能够根据参数how来实现对信号集的操作，操作主要有三种：

参数how 进程当前信号集 
SIG_BLOCK 在进程当前阻塞信号集中添加set指向信号集中的信号 
SIG_UNBLOCK 如果进程阻塞信号集中包含set指向信号集中的信号，则解除对该信号的阻塞 
SIG_SETMASK 更新进程阻塞信号集为set指向的信号集 

sigpending(sigset_t *set))获得当前已递送到进程，却被阻塞的所有信号，在set指向的信号集中返回结果。

sigsuspend(const sigset_t *mask))用于在接收到某个信号之前, 临时用mask替换进程的信号掩码, 并暂停进程执行，直到收到信号为止。sigsuspend 返回后将恢复调用之前的信号掩码。信号处理函数完成后，进程将继续执行。该系统调用始终返回-1，并将errno设置为EINTR。

附录1：结构itimerval：

            struct itimerval {
                struct timeval it_interval;  next value 
                struct timeval it_value;     current value 
            };
            struct timeval {
                long tv_sec;                 seconds 
                long tv_usec;                microseconds 
            };附录2：三参数信号处理函数中第二个参数的说明性描述：

siginfo_t {
int      si_signo;   信号值，对所有信号有意义
int      si_errno;   errno值，对所有信号有意义
int      si_code;    信号产生的原因，对所有信号有意义
pid_t    si_pid;     发送信号的进程ID,对kill(2),实时信号以及SIGCHLD有意义 
uid_t    si_uid;    发送信号进程的真实用户ID，对kill(2),实时信号以及SIGCHLD有意义 
int      si_status;  退出状态，对SIGCHLD有意义
clock_t  si_utime;   用户消耗的时间，对SIGCHLD有意义 
clock_t  si_stime;   内核消耗的时间，对SIGCHLD有意义 
sigval_t si_value;   信号值，对所有实时有意义，是一个联合数据结构，
                          可以为一个整数（由si_int标示，也可以为一个指针，由si_ptr标示）
	
void *   si_addr;    触发fault的内存地址，对SIGILL,SIGFPE,SIGSEGV,SIGBUS 信号有意义
int      si_band;   SIGPOLL信号有意义 
int      si_fd;     SIGPOLL信号有意义 
}实际上，除了前三个元素外，其他元素组织在一个联合结构中，在联合数据结构中，又根据不同的信号组织成不同的结构。注释中提到的对某种信号有意义指的是，在该信号的处理函数中可以访问这些域来获得与信号相关的有意义的信息，只不过特定信号只对特定信息感兴趣而已。

*/ //信号处理在ngx_signal_handler
ngx_int_t
ngx_init_signals(ngx_log_t *log)
{
    ngx_signal_t      *sig;
    struct sigaction   sa;

    for (sig = signals; sig->signo != 0; sig++) {
        ngx_memzero(&sa, sizeof(struct sigaction));
        sa.sa_handler = sig->handler;
        sigemptyset(&sa.sa_mask);
        if (sigaction(sig->signo, &sa, NULL) == -1) {
#if (NGX_VALGRIND)
            ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                          "sigaction(%s) failed, ignored", sig->signame);
#else
            ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                          "sigaction(%s) failed", sig->signame);
            return NGX_ERROR;
#endif
        }
    }

    return NGX_OK;
}

/*
在Linux中，需要使用命令行来控制Nginx服务器的启动与停止、重载配置文件、回滚日志文件、平滑升级等行为。默认情况下，Nginx被安装在目录
/usr/local/nginx/中，其二进制文件路径为/usr/local/nginc/sbin/nginx，配置文件路径为/usr/local/nginx/conf/nginx.conf。当然，
在configure执行时是可以指定把它们安装在不同目录的。为了简单起见，本节只说明默认安装情况下的命令行的使用情况，如果读者安装的
目录发生了变化，那么替换一下即可。

（1）默认方式启动

直接执行Nginx二进制程序。例如：

/usr/local/nginx/sbin/nginx

这时，会读取默认路径下的配置文件：/usr/local/nginx/conf/nginx.conf。

实际上，在没有显式指定nginx.conf配置文件路径时，将打开在configure命令执行时使用--conf-path=PATH指定的nginx.conf文件（参见1.5.1节）。

（2）另行指定配置文件的启动方式

使用-c参数指定配置文件。例如：

/usr/local/nginx/sbin/nginx -c /tmp/nginx.conf

这时，会读取-c参数后指定的nginx.conf配置文件来启动Nginx。

（3）另行指定安装目录的启动方式

使用-p参数指定Nginx的安装目录。例如：

/usr/local/nginx/sbin/nginx -p /usr/local/nginx/

（4）另行指定全局配置项的启动方式

可以通过-g参数临时指定一些全局配置项，以使新的配置项生效。例如：

/usr/local/nginx/sbin/nginx -g "pid /var/nginx/test.pid;"

上面这行命令意味着会把pid文件写到/var/nginx/test.pid中。

-g参数的约束条件是指定的配置项不能与默认路径下的nginx.conf中的配置项相冲突，否则无法启动。就像上例那样，类似这样的配置项：pid logs/nginx.pid，
是不能存在于默认的nginx.conf中的。

另一个约束条件是，以-g方式启动的Nginx服务执行其他命令行时，需要把-g参数也带上，否则可能出现配置项不匹配的情形。例如，如果要停止Nginx服务，
那么需要执行下面代码：

/usr/local/nginx/sbin/nginx -g "pid /var/nginx/test.pid;" -s stop

如果不带上-g "pid /var/nginx/test.pid;"，那么找不到pid文件，也会出现无法停止服务的情况。

（5）测试配置信息是否有错误

在不启动Nginx的情况下，使用-t参数仅测试配置文件是否有错误。例如：

/usr/local/nginx/sbin/nginx -t

执行结果中显示配置是否正确。

（6）在测试配置阶段不输出信息

测试配置选项时，使用-q参数可以不把error级别以下的信息输出到屏幕。例如：

/usr/local/nginx/sbin/nginx -t -q

（7）显示版本信息

使用-v参数显示Nginx的版本信息。例如：

/usr/local/nginx/sbin/nginx -v

（8）显示编译阶段的参数

使用-V参数除了可以显示Nginx的版本信息外，还可以显示配置编译阶段的信息，如GCC编译器的版本、操作系统的版本、执行configure时的参数等。例如：

/usr/local/nginx/sbin/nginx -V

（9）快速地停止服务

使用-s stop可以强制停止Nginx服务。-s参数其实是告诉Nginx程序向正在运行的Nginx服务发送信号量，Nginx程序通过nginx.pid文件中得到master进程的进程ID，
再向运行中的master进程发送TERM信号来快速地关闭Nginx服务。例如：

/usr/local/nginx/sbin/nginx -s stop

实际上，如果通过kill命令直接向nginx master进程发送TERM或者INT信号，效果是一样的。例如，先通过ps命令来查看nginx master的进程ID：

:ahf5wapi001:root > ps -ef | grep nginx

root     10800     1  0 02:27 ?        00:00:00 nginx: master process ./nginx

root     10801 10800  0 02:27 ?        00:00:00 nginx: worker process

接下来直接通过kill命令来发送信号：

kill -s SIGTERM 10800

或者：

kill -s SIGINT 10800

上述两条命令的效果与执行/usr/local/nginx/sbin/nginx -s stop是完全一样的。

（10）“优雅”地停止服务

如果希望Nginx服务可以正常地处理完当前所有请求再停止服务，那么可以使用-s quit参数来停止服务。例如：

/usr/local/nginx/sbin/nginx -s quit

该命令与快速停止Nginx服务是有区别的。当快速停止服务时，worker进程与master进程在收到信号后会立刻跳出循环，退出进程。而“优雅”地停止服务时，
首先会关闭监听端口，停止接收新的连接，然后把当前正在处理的连接全部处理完，最后再退出进程。

与快速停止服务相似，可以直接发送QUIT信号给master进程来停止服务，其效果与执行-s quit命令是一样的。例如：

kill -s SIGQUIT <nginx master pid>

如果希望“优雅”地停止某个worker进程，那么可以通过向该进程发送WINCH信号来停止服务。例如：

kill -s SIGWINCH <nginx worker pid>

（11）使运行中的Nginx重读配置项并生效

使用-s reload参数可以使运行中的Nginx服务重新加载nginx.conf文件。例如：

/usr/local/nginx/sbin/nginx -s reload

事实上，Nginx会先检查新的配置项是否有误，如果全部正确就以“优雅”的方式关闭，再重新启动Nginx来实现这个目的。类似的，-s是发送信号，
仍然可以用kill命令发送HUP信号来达到相同的效果。

kill -s SIGHUP <nginx master pid>

（12）日志文件回滚

使用-s reopen参数可以重新打开日志文件，这样可以先把当前日志文件改名或转移到其他目录中进行备份，再重新打开时就会生成新的日志文件。
这个功能使得日志文件不至于过大。例如：

/usr/local/nginx/sbin/nginx -s reopen

当然，这与使用kill命令发送USR1信号效果相同。

kill -s SIGUSR1 <nginx master pid>

（13）平滑升级Nginx

当Nginx服务升级到新的版本时，必须要将旧的二进制文件Nginx替换掉，通常情况下这是需要重启服务的，但Nginx支持不重启服务来完成新版本的平滑升级。

升级时包括以下步骤：

1）通知正在运行的旧版本Nginx准备升级。通过向master进程发送USR2信号可达到目的。例如：

kill -s SIGUSR2 <nginx master pid>

这时，运行中的Nginx会将pid文件重命名，如将/usr/local/nginx/logs/nginx.pid重命名为/usr/local/nginx/logs/nginx.pid.oldbin，这样新的Nginx才有可能启动成功。

2）启动新版本的Nginx，可以使用以上介绍过的任意一种启动方法。这时通过ps命令可以发现新旧版本的Nginx在同时运行。

3）通过kill命令向旧版本的master进程发送SIGQUIT信号，以“优雅”的方式关闭旧版本的Nginx。随后将只有新版本的Nginx服务运行，此时平滑升级完毕。

（14）显示命令行帮助

使用-h或者-?参数会显示支持的所有命令行参数。
*/
//注册新号在ngx_init_signals
void
ngx_signal_handler(int signo)
{
    char            *action;
    ngx_int_t        ignore;
    ngx_err_t        err;
    ngx_signal_t    *sig;

    ignore = 0;

    err = ngx_errno;

    for (sig = signals; sig->signo != 0; sig++) {
        if (sig->signo == signo) {
            break;
        }
    }

    ngx_time_sigsafe_update();

    action = "";

    switch (ngx_process) {

    case NGX_PROCESS_MASTER:
    case NGX_PROCESS_SINGLE:
        switch (signo) {
        //当接收到QUIT信号时，ngx_quit标志位会设为1，这是在告诉worker进程需要优雅地关闭进程
        case ngx_signal_value(NGX_SHUTDOWN_SIGNAL):
            ngx_quit = 1;
            action = ", shutting down";
            break;

        //当接收到TERM信号时，ngx_terminate标志位会设为1，这是在告诉worker进程需要强制关闭进程
        case ngx_signal_value(NGX_TERMINATE_SIGNAL):
        case SIGINT:
            ngx_terminate = 1;
            action = ", exiting";
            break;

        case ngx_signal_value(NGX_NOACCEPT_SIGNAL):
            if (ngx_daemonized) {
                ngx_noaccept = 1;
                action = ", stop accepting connections";
            }
            break;

        case ngx_signal_value(NGX_RECONFIGURE_SIGNAL): //reload信号，是由master处理
            ngx_reconfigure = 1;
            action = ", reconfiguring";
            break;

        //当接收到USRI信号时，ngx_reopen标志位会设为1，这是在告诉Nginx需要重新打开文件（如切换日志文件时）
        case ngx_signal_value(NGX_REOPEN_SIGNAL):
            ngx_reopen = 1;
            action = ", reopening logs";
            break;

        case ngx_signal_value(NGX_CHANGEBIN_SIGNAL):
            //if (getppid() > 1 || ngx_new_binary > 0) { 
            if (ngx_new_binary > 0) {  //yang add change，为了调试，故意注释
           //nginx热升级通过发送该信号,这里必须保证父进程大于1，父进程小于等于1的话，说明已经由就master启动了本master，则就不能热升级
           //所以如果通过crt登录启动nginx的话，可以看到其PPID大于1,所以不能热升级

                /*
                 * Ignore the signal in the new binary if its parent is
                 * not the init process, i.e. the old binary's process
                 * is still running.  Or ignore the signal in the old binary's
                 * process if the new binary's process is already running.
                 */

                action = ", ignoring";
                ignore = 1;
                break;
            }

            ngx_change_binary = 1;
            action = ", changing binary";
            break;

        case SIGALRM: 
            ngx_sigalrm = 1; //子进程会重新设置定时器信号，见ngx_timer_signal_handler
            break;

        case SIGIO:
            ngx_sigio = 1;
            break;

        case SIGCHLD: //子进程终止, 这时候内核同时向父进程发送个sigchld信号.等待父进程waitpid回收，避免僵死进程
            ngx_reap = 1;
            break;
        }

        break;

    case NGX_PROCESS_WORKER:
    case NGX_PROCESS_HELPER:
        switch (signo) {

        case ngx_signal_value(NGX_NOACCEPT_SIGNAL):
            if (!ngx_daemonized) {
                break;
            }
            ngx_debug_quit = 1;
        case ngx_signal_value(NGX_SHUTDOWN_SIGNAL): //工作进程收到quit信号
            ngx_quit = 1;
            action = ", shutting down";
            break;

        case ngx_signal_value(NGX_TERMINATE_SIGNAL):
        case SIGINT:
            ngx_terminate = 1;
            action = ", exiting";
            break;

        case ngx_signal_value(NGX_REOPEN_SIGNAL):
            ngx_reopen = 1;
            action = ", reopening logs";
            break;

        case ngx_signal_value(NGX_RECONFIGURE_SIGNAL):
        case ngx_signal_value(NGX_CHANGEBIN_SIGNAL):
        case SIGIO:
            action = ", ignoring";
            break;
        }

        break;
    }

    ngx_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0,
                  "signal %d (%s) received%s", signo, sig->signame, action);

    if (ignore) {
        ngx_log_error(NGX_LOG_CRIT, ngx_cycle->log, 0,
                      "the changing binary signal is ignored: "
                      "you should shutdown or terminate "
                      "before either old or new binary's process");
    }

    if (signo == SIGCHLD) { //回收子进程资源waitpid
        ngx_process_get_status();
    }

    ngx_set_errno(err);
}


static void ngx_process_get_status(void)
{
    int              status;
    char            *process;
    ngx_pid_t        pid;
    ngx_err_t        err;
    ngx_int_t        i;
    ngx_uint_t       one;

    one = 0;

    for ( ;; ) {
        pid = waitpid(-1, &status, WNOHANG);

        if (pid == 0) {
            return;
        }

        if (pid == -1) {
            err = ngx_errno;

            if (err == NGX_EINTR) {
                continue;
            }

            if (err == NGX_ECHILD && one) {
                return;
            }

            /*
             * Solaris always calls the signal handler for each exited process
             * despite waitpid() may be already called for this process.
             *
             * When several processes exit at the same time FreeBSD may
             * erroneously call the signal handler for exited process
             * despite waitpid() may be already called for this process.
             */

            if (err == NGX_ECHILD) {
                ngx_log_error(NGX_LOG_INFO, ngx_cycle->log, err,
                              "waitpid() failed");
                return;
            }

            ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, err,
                          "waitpid() failed");
            return;
        }


        one = 1;
        process = "unknown process";

        for (i = 0; i < ngx_last_process; i++) {
            if (ngx_processes[i].pid == pid) {
                ngx_processes[i].status = status;
                ngx_processes[i].exited = 1;
                process = ngx_processes[i].name;
                break;
            }
        }

        if (WTERMSIG(status)) {
#ifdef WCOREDUMP
            ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0,
                          "%s %P exited on signal %d%s",
                          process, pid, WTERMSIG(status),
                          WCOREDUMP(status) ? " (core dumped)" : "");
#else
            ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0,
                          "%s %P exited on signal %d",
                          process, pid, WTERMSIG(status));
#endif

        } else {
            ngx_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0,
                          "%s %P exited with code %d",
                          process, pid, WEXITSTATUS(status));
        }

        if (WEXITSTATUS(status) == 2 && ngx_processes[i].respawn) {
            ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0,
                          "%s %P exited with fatal code %d "
                          "and cannot be respawned",
                          process, pid, WEXITSTATUS(status));
            ngx_processes[i].respawn = 0;
        }

        ngx_unlock_mutexes(pid);
    }
}


static void
ngx_unlock_mutexes(ngx_pid_t pid)
{
    ngx_uint_t        i;
    ngx_shm_zone_t   *shm_zone;
    ngx_list_part_t  *part;
    ngx_slab_pool_t  *sp;

    /*
     * unlock the accept mutex if the abnormally exited process
     * held it
     */

    if (ngx_accept_mutex_ptr) {
        (void) ngx_shmtx_force_unlock(&ngx_accept_mutex, pid);
    }

    /*
     * unlock shared memory mutexes if held by the abnormally exited
     * process
     */

    part = (ngx_list_part_t *) &ngx_cycle->shared_memory.part;
    shm_zone = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            shm_zone = part->elts;
            i = 0;
        }

        sp = (ngx_slab_pool_t *) shm_zone[i].shm.addr;

        if (ngx_shmtx_force_unlock(&sp->mutex, pid)) {
            ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0,
                          "shared memory zone \"%V\" was locked by %P",
                          &shm_zone[i].shm.name, pid);
        }
    }
}


void
ngx_debug_point(void)//让自己停止，通知父进程
{
    ngx_core_conf_t  *ccf;

    ccf = (ngx_core_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                           ngx_core_module);

    switch (ccf->debug_points) {

    case NGX_DEBUG_POINTS_STOP:
        raise(SIGSTOP);
        break;

    case NGX_DEBUG_POINTS_ABORT:
        ngx_abort();
    }
}

/*
ngx_os_signal_process()函数处理
遍历signals数组，根据给定信号name，找到对应signo；
调用kill向该pid发送signo号信号；
*/
ngx_int_t 
ngx_os_signal_process(ngx_cycle_t *cycle, char *name, ngx_int_t pid)
{
    ngx_signal_t  *sig;

    for (sig = signals; sig->signo != 0; sig++) {
        if (ngx_strcmp(name, sig->name) == 0) {
            if (kill(pid, sig->signo) != -1) { //这里发送signal，信号接收处理在signals->handler
                return 0;
            }

            ngx_log_error(NGX_LOG_ALERT, cycle->log, ngx_errno,
                          "kill(%P, %d) failed", pid, sig->signo);
        }
    }

    return 1;
}

