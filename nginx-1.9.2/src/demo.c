#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

/*
[nginx]# ps -ef | grep nginx
liubin     726  3469  0 20:00 pts/4    00:00:00 ./nginx
liubin     727   726  0 20:00 pts/4    00:00:00 nginx:worker process             
liubin     728   726  0 20:00 pts/4    00:00:00 nginx:worker process             
liubin     740   726  0 20:00 pts/4    00:00:00 nginx:dispatcher process         
liubin     745   726  0 20:00 pts/4    00:00:00 nginx:worker process  

[nginx]# kill -9 745
[nginx]# ps -ef | grep nginx
liubin 726 3469 0 20:00 pts/4 00:00:00 ./nginx
liubin 727 726 0 20:00 pts/4 00:00:00 nginx:worker process 
liubin 728 726 0 20:00 pts/4 00:00:00 nginx:worker process 
liubin 740 726 0 20:00 pts/4 00:00:00 nginx:dispatcher process 
liubin 2714 726 0 20:30 pts/4 00:00:00 nginx:worker process

//可以看到，杀掉745后，马上又起来新的2714进程
*/

#define DefaultConfigFile "/var/conf/nginx_lunbo.conf"
#define PID_FILENAME "/var/run/nginx_lunbo.pid"
#define VERSION "1.0.0"

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
#define MAX_PROCESSES 128
#define PROCESS_NORESPAWN     -1
#define PROCESS_JUST_SPAWN    -2
#define PROCESS_RESPAWN       -3
#define PROCESS_JUST_RESPAWN  -4
#define PROCESS_DETACHED      -5

const char *appname="nginx_lunbo";
char *ConfigFile = NULL;

typedef void (*spawn_proc_pt) (void *data);
typedef struct {
    pid_t           pid; //该进程pid
    int                 status; //进程状态，通过sig_child获取
    //int        channel[2];

    spawn_proc_pt   proc; //创建进程时候的回调
    void               *data; //创建进程时候传递的参数
    char               *name; //进程名

    unsigned            respawn:1; //PROCESS_RESPAWN 创建进程的时候释放指定需要master拉起一个新的进程，如果进程挂了
    unsigned            just_spawn:1;
    unsigned            detached:1;
    unsigned            exiting:1;
    unsigned            exited:1;
} process_t;
process_t    processes[MAX_PROCESSES];
int process_slot;

/* getopt args*/
int opt_no_daemon = 0;
int opt_debug_stderr = -1;
int opt_parse_cfg_only = 0;
int opt_send_signal = -1;

sig_atomic_t  reap;
sig_atomic_t  terminate;
sig_atomic_t  quit;
int last_process; //每创建一个进程就++
int exiting;

char **os_argv;
char  *os_argv_last; 
init_setproctitle(void)
{
    char      *p; 
    size_t       size;
    int   i;  

    size = 0;

    os_argv_last = os_argv[0];

    for (i = 0; os_argv[i]; i++) {
        if (os_argv_last == os_argv[i]) {
            os_argv_last = os_argv[i] + strlen(os_argv[i]) + 1;
        }   
    }   
    os_argv_last += strlen(os_argv_last);
}

void setproctitle(char *title)
{
    char  *p;
    os_argv[1] = NULL;

    p = strncpy((char *) os_argv[0], (char *) title,
            strlen(title));
    p += strlen(title);

    if (os_argv_last - (char *) p > 0) {
        memset(p, ' ', os_argv_last - (char *) p);
    }

}

void worker_process_init(int worker)
{

}

void worker_process_exit(void)
{

}

void
worker_process_cycle(void *data)
{
    int worker = (intptr_t) data;

    worker_process_init(worker);

    setproctitle("nginx:worker process");

    for ( ;; ) {
        if (exiting) {
                fprintf(stderr, "exiting");
                worker_process_exit();
        }
        sleep(60);
        fprintf(stderr, "\tworker cycle pid: %d\n", getpid());

        if (terminate) {
            fprintf(stderr, "exiting");
            worker_process_exit();
        }

        if (quit) {
            quit = 0;
            fprintf(stderr, "gracefully shutting down");
            setproctitle("worker process is shutting down");

            if (!exiting) {
                exiting = 1;
            }
        }
    }
}

void dispatcher_process_init(int worker)
{

}

void dispatcher_process_exit()
{

}

void
dispatcher_process_cycle(void *data)
{
    int worker = (intptr_t) data;

    dispatcher_process_init(worker);

    setproctitle("nginx:dispatcher process");

    for ( ;; ) {
        if (exiting) {
                fprintf(stderr, "exiting\n");
                dispatcher_process_exit();
        }

        sleep(60);
        fprintf(stderr, "\tdispatcher cycle pid: %d\n", getpid());

        if (terminate) {
            fprintf(stderr, "exiting\n");
            dispatcher_process_exit();
        }

        if (quit) {
            quit = 0;
            fprintf(stderr, "gracefully shutting down\n");
            setproctitle("worker process is shutting down");

            if (!exiting) {
                exiting = 1;
            }
        }
    }
}


void 
start_worker_processes(int n, int type)
{
    int i;   
    fprintf(stderr, "start worker processes\n");

    for (i = 0; i < n; i++) {
        spawn_process(worker_process_cycle, (void *) (intptr_t) i, "worker process", type);
    }
}

void start_dispatcher_process(int type)
{
    fprintf(stderr, "start dispatcher processes\n");
    spawn_process(dispatcher_process_cycle, (void *) (intptr_t) 0, "dispatcher process", type);
}


void save_argv(int argc, char *const *argv)
{
    os_argv = (char **) argv;
}

void
sig_child(int sig)
{
    reap = 1; //通知主进程重新创建一个worker进程

    int status;
    int i;
    pid_t pid;

    do {
        pid = waitpid(-1, &status, WNOHANG);
        for (i = 0; i < last_process; i++) {
            if (processes[i].pid == pid) {
                processes[i].status = status;
                processes[i].exited = 1;
                //process = processes[i].name;
                break;
            }   
        }
    } while (pid > 0);
    signal(sig, sig_child);
}

typedef void SIGHDLR(int sig);
void signal_set(int sig, SIGHDLR * func, int flags)
{
    struct sigaction sa;
    sa.sa_handler = func;
    sa.sa_flags = flags;
    sigemptyset(&sa.sa_mask);
    if (sigaction(sig, &sa, NULL) < 0) 
        fprintf(stderr, "sigaction: sig=%d func=%p: %s\n", sig, func, strerror(errno));
}

void init_signals(void)
{
    signal_set(SIGCHLD, sig_child, SA_NODEFER | SA_RESTART);
}

static pid_t readPidFile(void)
{
    FILE *pid_fp = NULL;
    const char *f = PID_FILENAME;
    pid_t pid = -1;
    int i;

    if (f == NULL) {
        fprintf(stderr, "%s: error: no pid file name defined\n", appname);
        exit(1);
    }

    pid_fp = fopen(f, "r");
    if (pid_fp != NULL) {
        pid = 0;
        if (fscanf(pid_fp, "%d", &i) == 1)
            pid = (pid_t) i;
        fclose(pid_fp);
    } else {
        if (errno != ENOENT) {
            fprintf(stderr, "%s: error: could not read pid file\n", appname);
            fprintf(stderr, "\t%s: %s\n", f, strerror(errno));
            exit(1);
        }
    }
    return pid;
}

int checkRunningPid(void)
{
    pid_t pid;
    pid = readPidFile();
    if (pid < 2)
        return 0;
    if (kill(pid, 0) < 0)
        return 0;
    fprintf(stderr, "nginx_master is already running!  process id %ld\n", (long int) pid);
    return 1;
}

void writePidFile(void)
{
    FILE *fp;
    const char *f = PID_FILENAME;
    fp = fopen(f, "w+");
    if (!fp) {
        fprintf(stderr, "could not write pid file '%s': %s\n", f, strerror(errno));
        return;
    }
    fprintf(fp, "%d\n", (int) getpid());
    fclose(fp);
}


void usage(void)
{
    fprintf(stderr,
            "Usage: %s [-?hvVN] [-d level] [-c config-file] [-k signal]\n"
            "       -h        Print help message.\n"
            "       -v        Show Version and exit.\n"
            "       -N        No daemon mode.\n"
            "       -c file   Use given config-file instead of\n"
            "                 %s\n"
            "       -k reload|rotate|kill|parse\n"
            "                 kill is fast shutdown\n"
            "                 Parse configuration file, then send signal to \n"
            "                 running copy (except -k parse) and exit.\n",
            appname, DefaultConfigFile);
    exit(1);
}

static void show_version(void)
{
    fprintf(stderr, "%s version: %s\n", appname,VERSION);
    exit(1);
}


void mainParseOptions(int argc, char *argv[])
{
    extern char *optarg;
    int c;

    while ((c = getopt(argc, argv, "hvNc:k:?")) != -1) {
        switch (c) {
            case 'h':
                usage();
                break;
            case 'v':
                show_version();
                break;
            case 'N':
                opt_no_daemon = 1;
                break;
            case 'c':
                ConfigFile = strdup(optarg);
                break;
            case 'k':
                if ((int) strlen(optarg) < 1)
                    usage();
                if (!strncmp(optarg, "reload", strlen(optarg)))
                    opt_send_signal = SIGHUP;
                else if (!strncmp(optarg, "rotate", strlen(optarg)))
                    opt_send_signal = SIGUSR1;
                else if (!strncmp(optarg, "shutdown", strlen(optarg)))
                    opt_send_signal = SIGTERM;
                else if (!strncmp(optarg, "kill", strlen(optarg)))
                    opt_send_signal = SIGKILL;
                else if (!strncmp(optarg, "parse", strlen(optarg)))
                    opt_parse_cfg_only = 1;        /* parse cfg file only */
                else
                    usage();
                break;
            case '?':
            default:
                usage();
                break;
        }
    }
}

void enableCoredumps(void)
{
    /* Set Linux DUMPABLE flag */
    if (prctl(PR_SET_DUMPABLE, 1, 0, 0, 0) != 0)
        fprintf(stderr, "prctl: %s\n", strerror(errno));

    /* Make sure coredumps are not limited */
    struct rlimit rlim;

    if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
        rlim.rlim_cur = rlim.rlim_max;
        if (setrlimit(RLIMIT_CORE, &rlim) == 0) {
            fprintf(stderr, "Enable Core Dumps OK!\n");
            return;
        }
    }
    fprintf(stderr, "Enable Core Dump failed: %s\n",strerror(errno));
}

int
reap_children(void)
{
    int         i, n;
    int        live;

    live = 0;
    for (i = 0; i < last_process; i++) { //之前挂掉的进程
                //child[0] 26718 e:0 t:0 d:0 r:1 j:0
        fprintf(stderr,"child[%d] %d exiting:%d exited:%d detached:%d respawn:%d just_spawn:%d\n",
                       i,
                       processes[i].pid,
                       processes[i].exiting,
                       processes[i].exited,
                       processes[i].detached,
                       processes[i].respawn,
                       processes[i].just_spawn);

        if (processes[i].pid == -1) {
            continue;
        }

        if (processes[i].exited) {
            if (!processes[i].detached) {
                for (n = 0; n < last_process; n++) {
                    if (processes[n].exited
                        || processes[n].pid == -1)
                    {
                        continue;
                    }

                    fprintf(stderr,"detached:%d\n", processes[n].pid);
                }
            }

            if (processes[i].respawn //需要重启进程
                && !processes[i].exiting
                && !terminate
                && !quit)
            {
                if (spawn_process(processes[i].proc, processes[i].data, processes[i].name, i) == -1)
                {
                    fprintf(stderr, "could not respawn %s\n", processes[i].name);
                    continue;
                }

                live = 1;

                continue;
            }

            if (i == last_process - 1) {
                last_process--;

            } else {
                processes[i].pid = -1;
            }

        } else if (processes[i].exiting || !processes[i].detached) {
            live = 1;
        }
    }

    return live;
}

pid_t
spawn_process(spawn_proc_pt proc, void *data, char *name, int respawn)
{
    long     on;
    pid_t  pid;
    int  s;

    if (respawn >= 0) {
        s = respawn;

    } else {
        for (s = 0; s < last_process; s++) {
            if (processes[s].pid == -1) {
                break;
            }
        }

        if (s == MAX_PROCESSES) {
            fprintf(stderr, "no more than %d processes can be spawned",
                          MAX_PROCESSES);
            return -1;
        }
    }

    process_slot = s;

    pid = fork();

    switch (pid) {

    case -1:
        fprintf(stderr, "fork() failed while spawning \"%s\" :%s", name, errno);
        return -1;

    case 0:
        pid = getpid();
        proc(data);
        break;

    default:
        break;
    }

    fprintf(stderr, "start %s %d\n", name, pid);

    processes[s].pid = pid;
    processes[s].exited = 0;

    if (respawn >= 0) {
        return pid;
    }

    processes[s].proc = proc;
    processes[s].data = data;
    processes[s].name = name;
    processes[s].exiting = 0;

    switch (respawn) {

    case PROCESS_NORESPAWN:
        processes[s].respawn = 0;
        processes[s].just_spawn = 0;
        processes[s].detached = 0;
        break;

    case PROCESS_JUST_SPAWN:
        processes[s].respawn = 0;
        processes[s].just_spawn = 1;
        processes[s].detached = 0;
        break;

    case PROCESS_RESPAWN:
        processes[s].respawn = 1;
        processes[s].just_spawn = 0;
        processes[s].detached = 0;
        break;

    case PROCESS_JUST_RESPAWN:
        processes[s].respawn = 1;
        processes[s].just_spawn = 1;
        processes[s].detached = 0;
        break;

    case PROCESS_DETACHED:
        processes[s].respawn = 0;
        processes[s].just_spawn = 0;
        processes[s].detached = 1;
        break;
    }

    if (s == last_process) {
        last_process++;
    }

    return pid;
}

int main(int argc, char **argv)
{
    int fd;
    int i;
    pid_t pid;
    sigset_t set;

    mainParseOptions(argc, argv);
    printf("yang test ... opt_send_signal:%d\r\n", opt_send_signal);
    if (-1 == opt_send_signal)
        if (checkRunningPid())
            exit(1);

    enableCoredumps();
    writePidFile();
    save_argv(argc, argv);
    init_setproctitle();
    init_signals();
    sigemptyset(&set);

    printf("father pid1=%d\n",getpid());
    int worker_processes = 3;
    start_worker_processes(worker_processes, PROCESS_RESPAWN);

    start_dispatcher_process(PROCESS_RESPAWN);
    printf("father pid2=%d\n", getpid());

    setproctitle("nginx:master");
    int live = 1;
    for (;;) {
        printf("father before suspend\n");
        sigsuspend(&set);
        printf("father after suspend\n");
        if (reap) {
            reap = 0;             
            fprintf(stderr, "reap children\n");
            live = reap_children();
        }
    }
    return 0;
}

