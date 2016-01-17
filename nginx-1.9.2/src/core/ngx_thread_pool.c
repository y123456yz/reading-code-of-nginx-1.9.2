
/*
 * Copyright (C) Nginx, Inc.
 * Copyright (C) Valentin V. Bartenev
 * Copyright (C) Ruslan Ermilov
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_thread_pool.h>


typedef struct {
    //数组中的每个成员在ngx_thread_pool_init中初始化，
    //数组成员结构式ngx_thread_pool_t
    ngx_array_t               pools; //所有的thread_pool name配置信息ngx_thread_pool_t存放在该数组中，见ngx_thread_pool_add
} ngx_thread_pool_conf_t;//创建空间在ngx_thread_pool_create_conf


typedef struct { //见ngx_thread_pool_done
    ngx_thread_task_t        *first;

    /*
     *ngx_thread_pool_t->queue.last = task;  新添加的任务通过last连接在一起
     ngx_thread_pool_t->queue.last = &task->next;  下次在添加新任务就让task->next指向新任务了
     */
    ngx_thread_task_t       **last;
} ngx_thread_pool_queue_t; //线程池队列  初始化在ngx_thread_pool_queue_init

#define ngx_thread_pool_queue_init(q)                                         \
    (q)->first = NULL;                                                        \
    (q)->last = &(q)->first

//一个该结构对应一个threads_pool配置
struct ngx_thread_pool_s {//该结构式存放在ngx_thread_pool_conf_t->pool数组中的，见ngx_thread_pool_init_worker
    ngx_thread_mutex_t        mtx; //线程锁  ngx_thread_pool_init中初始化
    //ngx_thread_task_post中添加的任务被添加到该队列中
    ngx_thread_pool_queue_t   queue;//ngx_thread_pool_init  ngx_thread_pool_queue_init中初始化
    //在该线程池poll中每添加一个线程，waiting子减，当线程全部正在执行任务后，waiting会恢复到0
    //如果所有线程都已经在执行任务(也就是waiting>-0)，又来了任务，那么任务就只能等待。所以waiting表示等待被执行的任务数
    ngx_int_t                 waiting;//等待的任务数   ngx_thread_task_post加1   ngx_thread_pool_cycle减1
    ngx_thread_cond_t         cond;//条件变量  ngx_thread_pool_init中初始化

    ngx_log_t                *log;//ngx_thread_pool_init中初始化

    ngx_str_t                 name;//thread_pool name threads=number [max_queue=number];中的name  ngx_thread_pool
    //如果没有配置，在ngx_thread_pool_init_conf默认赋值为32
    ngx_uint_t                threads;//thread_pool name threads=number [max_queue=number];中的number  ngx_thread_pool
    //如果没有配置，在ngx_thread_pool_init_conf默认赋值为65535  
    //指的是线程已经全部用完的情况下，还可以添加多少个任务到等待队列
    ngx_int_t                 max_queue;//thread_pool name threads=number [max_queue=number];中的max_queue  ngx_thread_pool

    u_char                   *file;//配置文件名
    ngx_uint_t                line;//thread_pool配置在配置文件中的行号
};


static ngx_int_t ngx_thread_pool_init(ngx_thread_pool_t *tp, ngx_log_t *log,
    ngx_pool_t *pool);
static void ngx_thread_pool_destroy(ngx_thread_pool_t *tp);
static void ngx_thread_pool_exit_handler(void *data, ngx_log_t *log);

static void *ngx_thread_pool_cycle(void *data);
static void ngx_thread_pool_handler(ngx_event_t *ev);

static char *ngx_thread_pool(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static void *ngx_thread_pool_create_conf(ngx_cycle_t *cycle);
static char *ngx_thread_pool_init_conf(ngx_cycle_t *cycle, void *conf);

static ngx_int_t ngx_thread_pool_init_worker(ngx_cycle_t *cycle);
static void ngx_thread_pool_exit_worker(ngx_cycle_t *cycle);


static ngx_command_t  ngx_thread_pool_commands[] = {
    /*
    Syntax: thread_pool name threads=number [max_queue=number];
    Default: thread_pool default threads=32 max_queue=65536; threads参数为该pool中线程个数，max_queue表示等待被线程调度的任务数
    
    Defines named thread pools used for multi-threaded reading and sending of files without blocking worker processes. 
    The threads parameter defines the number of threads in the pool. 
    In the event that all threads in the pool are busy, a new task will wait in the queue. The max_queue parameter limits the 
    number of tasks allowed to be waiting in the queue. By default, up to 65536 tasks can wait in the queue. When the queue 
    overflows, the task is completed with an error. 
    */
    { ngx_string("thread_pool"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE23,
      ngx_thread_pool,
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_core_module_t  ngx_thread_pool_module_ctx = {
    ngx_string("thread_pool"),
    ngx_thread_pool_create_conf,
    ngx_thread_pool_init_conf
};


ngx_module_t  ngx_thread_pool_module = {
    NGX_MODULE_V1,
    &ngx_thread_pool_module_ctx,           /* module context */
    ngx_thread_pool_commands,              /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    ngx_thread_pool_init_worker,           /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    ngx_thread_pool_exit_worker,           /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_str_t  ngx_thread_pool_default = ngx_string("default");

static ngx_uint_t               ngx_thread_pool_task_id;
static ngx_atomic_t             ngx_thread_pool_done_lock;//为0，可以获取锁，不为0，则不能获取到该锁
static ngx_thread_pool_queue_t  ngx_thread_pool_done; //所有的

//根据thread_pool name threads=number [max_queue=number];中的number来创建这么多个线程
static ngx_int_t
ngx_thread_pool_init(ngx_thread_pool_t *tp, ngx_log_t *log, ngx_pool_t *pool)
{
    int             err;
    pthread_t       tid;
    ngx_uint_t      n;
    pthread_attr_t  attr;

    if (ngx_notify == NULL) {
        ngx_log_error(NGX_LOG_ALERT, log, 0,
               "the configured event method cannot be used with thread pools");
        return NGX_ERROR;
    }

    ngx_thread_pool_queue_init(&tp->queue);

    if (ngx_thread_mutex_create(&tp->mtx, log) != NGX_OK) {
        return NGX_ERROR;
    }

    if (ngx_thread_cond_create(&tp->cond, log) != NGX_OK) {
        (void) ngx_thread_mutex_destroy(&tp->mtx, log);
        return NGX_ERROR;
    }

    tp->log = log;

    err = pthread_attr_init(&attr);
    if (err) {
        ngx_log_error(NGX_LOG_ALERT, log, err,
                      "pthread_attr_init() failed");
        return NGX_ERROR;
    }

#if 0
    err = pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);
    if (err) {
        ngx_log_error(NGX_LOG_ALERT, log, err,
                      "pthread_attr_setstacksize() failed");
        return NGX_ERROR;
    }
#endif

    for (n = 0; n < tp->threads; n++) {
        /*
        线程原语：pthread_create()，pthread_self()，pthread_exit(),pthread_join(),pthread_cancel(),pthread_detach( .
        好的线程理解大全参考(有图解例子，很好):http://blog.csdn.net/tototuzuoquan/article/details/39553427
         */
        err = pthread_create(&tid, &attr, ngx_thread_pool_cycle, tp);
        if (err) {
            ngx_log_error(NGX_LOG_ALERT, log, err,
                          "pthread_create() failed");
            return NGX_ERROR;
        }
    }

    (void) pthread_attr_destroy(&attr);

    return NGX_OK;
}


static void
ngx_thread_pool_destroy(ngx_thread_pool_t *tp)
{
    ngx_uint_t           n;
    ngx_thread_task_t    task;
    volatile ngx_uint_t  lock;

    ngx_memzero(&task, sizeof(ngx_thread_task_t));

    task.handler = ngx_thread_pool_exit_handler;//任务退出执行函数
    task.ctx = (void *) &lock;//指向传入的参数  

    // 没有赋值task->event.handler  task->event.data   (void) ngx_notify(ngx_thread_pool_handler); 中会不会段错误??? /
    for (n = 0; n < tp->threads; n++) {  //tp中所有的线程池添加该任务  
        lock = 1;

        if (ngx_thread_task_post(tp, &task) != NGX_OK) {
            return;
        }

        while (lock) { //主进程判断如果lock没有改变，就让CPU给其他线程执行，以此等待，相当于pthread_join  
            ngx_sched_yield();
        }

        //只有线程池中的一个线程执行了exit_handler后才能会继续for循环

        task.event.active = 0;
    }

    //此时到这边，所有的线程都已经退出   //条件变量销毁   互斥锁

    (void) ngx_thread_cond_destroy(&tp->cond, tp->log);

    (void) ngx_thread_mutex_destroy(&tp->mtx, tp->log);
}


static void
ngx_thread_pool_exit_handler(void *data, ngx_log_t *log)
{
    ngx_uint_t *lock = data;

    *lock = 0;

    pthread_exit(0);
}


ngx_thread_task_t *
ngx_thread_task_alloc(ngx_pool_t *pool, size_t size)
{
    ngx_thread_task_t  *task;

    task = ngx_pcalloc(pool, sizeof(ngx_thread_task_t) + size);
    if (task == NULL) {
        return NULL;
    }

    task->ctx = task + 1;

    return task;
}

/*
一般主线程通过ngx_thread_task_post添加任务，线程队列中的线程执行任务，主线程号和进程号一样，线程队列中的线程和进程号不一样，例如
2016/01/07 12:38:01[               ngx_thread_task_post,   280][yangya  [debug] 20090#20090: ngx add task to thread, task id:183
20090#20090前面是进程号，后面是主线程号，他们相同
*/
//任务添加到对应的线程池任务队列中
ngx_int_t //ngx_thread_pool_cycle和ngx_thread_task_post配合阅读
ngx_thread_task_post(ngx_thread_pool_t *tp, ngx_thread_task_t *task)
{
    if (task->event.active) {
        ngx_log_error(NGX_LOG_ALERT, tp->log, 0,
                      "task #%ui already active", task->id);
        return NGX_ERROR;
    }
    
    if (ngx_thread_mutex_lock(&tp->mtx, tp->log) != NGX_OK) {
        return NGX_ERROR;
    }

    if (tp->waiting >= tp->max_queue) {
        (void) ngx_thread_mutex_unlock(&tp->mtx, tp->log);

        ngx_log_error(NGX_LOG_ERR, tp->log, 0,
                      "thread pool \"%V\" queue overflow: %i tasks waiting",
                      &tp->name, tp->waiting);
        return NGX_ERROR;
    }

    task->event.active = 1;

    task->id = ngx_thread_pool_task_id++;
    task->next = NULL;
    ngx_log_debugall(tp->log, 0, "ngx add task to thread, task id:%ui", task->id);

    if (ngx_thread_cond_signal(&tp->cond, tp->log) != NGX_OK) {
        (void) ngx_thread_mutex_unlock(&tp->mtx, tp->log);
        return NGX_ERROR;
    }

    //添加到任务队列
    *tp->queue.last = task;
    tp->queue.last = &task->next;

    tp->waiting++;

    (void) ngx_thread_mutex_unlock(&tp->mtx, tp->log);

    ngx_log_debug2(NGX_LOG_DEBUG_CORE, tp->log, 0,
                   "task #%ui added to thread pool name: \"%V\" complete",
                   task->id, &tp->name);

    return NGX_OK;
}

//ngx_thread_pool_cycle和ngx_thread_task_post配合阅读
static void *
ngx_thread_pool_cycle(void *data)
{
    ngx_thread_pool_t *tp = data; //一个该结构对应一个threads_pool配置

    int                 err;
    sigset_t            set;
    ngx_thread_task_t  *task;

#if 0
    ngx_time_update();
#endif

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, tp->log, 0,
                   "thread in pool \"%V\" started", &tp->name);

    sigfillset(&set);

    sigdelset(&set, SIGILL);
    sigdelset(&set, SIGFPE);
    sigdelset(&set, SIGSEGV);
    sigdelset(&set, SIGBUS);

    err = pthread_sigmask(SIG_BLOCK, &set, NULL); //
    if (err) {
        ngx_log_error(NGX_LOG_ALERT, tp->log, err, "pthread_sigmask() failed");
        return NULL;
    }

    /*
    一般主线程通过ngx_thread_task_post添加任务，线程队列中的线程执行任务，主线程号和进程号一样，线程队列中的线程和进程号不一样，例如
    2016/01/07 12:38:01[               ngx_thread_task_post,   280][yangya  [debug] 20090#20090: ngx add task to thread, task id:183
    20090#20090前面是进程号，后面是主线程号，他们相同
    */
    for ( ;; ) {//一次任务执行完后又会走到这里，循环
        if (ngx_thread_mutex_lock(&tp->mtx, tp->log) != NGX_OK) {
            return NULL;
        }

        /* the number may become negative */
        tp->waiting--;

        //如果队列中有任务，则直接执行任务，不会在while中等待conf signal
        while (tp->queue.first == NULL) { //此时任务队列为空，在条件变量上等待  配合ngx_thread_task_post阅读
            //在添加任务的时候唤醒ngx_thread_task_post -> ngx_thread_cond_signal
            if (ngx_thread_cond_wait(&tp->cond, &tp->mtx, tp->log) //等待ngx_thread_cond_signal后才会返回
                != NGX_OK)
            {
                (void) ngx_thread_mutex_unlock(&tp->mtx, tp->log);
                return NULL;
            }
        }

        //取出队首任务，然后执行
        task = tp->queue.first;
        tp->queue.first = task->next;

        if (tp->queue.first == NULL) {
            tp->queue.last = &tp->queue.first;
        }

        //这一段加锁是因为线程池是共享资源，多个线程都从队列中取线程，并且主线程会添加任务到队列中
        if (ngx_thread_mutex_unlock(&tp->mtx, tp->log) != NGX_OK) {
            return NULL;
        }

#if 0
        ngx_time_update();
#endif

        ngx_log_debug2(NGX_LOG_DEBUG_CORE, tp->log, 0,
                       "run task #%ui in thread pool name:\"%V\"",
                       task->id, &tp->name);

        task->handler(task->ctx, tp->log); //每个任务有各自的ctx,因此这里不需要加锁

        ngx_log_debug2(NGX_LOG_DEBUG_CORE, tp->log, 0,
                       "complete task #%ui in thread pool name: \"%V\"",
                       task->id, &tp->name);

        task->next = NULL;

        ngx_spinlock(&ngx_thread_pool_done_lock, 1, 2048);

        //task添加到队列尾部，同时可以保证多次添加的时候，让新task和以前的task形成一个还，first执行第一个task，last指向最后一个task
        *ngx_thread_pool_done.last = task;
        ngx_thread_pool_done.last = &task->next;

        ngx_unlock(&ngx_thread_pool_done_lock);

//ngx_notify通告主线程，该任务处理完毕，ngx_thread_pool_handler由主线程执行，也就是进程cycle{}通过epoll_wait返回执行，而不是由线程池中的线程执行
        (void) ngx_notify(ngx_thread_pool_handler); 
    }
}

//任务处理完后，epoll的通知读事件会调用该函数 
////ngx_notify通告主线程，该任务处理完毕，ngx_thread_pool_handler由主线程执行，也就是进程cycle{}通过epoll_wait返回执行，而不是由线程池中的线程执行
static void
ngx_thread_pool_handler(ngx_event_t *ev)
{
    ngx_event_t        *event;
    ngx_thread_task_t  *task;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ev->log, 0, "thread pool handler");

    ngx_spinlock(&ngx_thread_pool_done_lock, 1, 2048);

    /* 这里是不是有问题?
        如果线程池中的线程执行任务比较快，而主进程在执行epoll_wait过程中有点阻塞，那么就检测不到ngx_notify中的epoll事件，有可能下次检测到该事件的时候
        ngx_thread_pool_done上已经积累了很多执行完的任务事件，见ngx_thread_pool_cycle。
        单这里好像只取了队列首部的任务啊?????? 队首外的任务丢弃了???????????不对吧

        答案是，这里面所有的任务都在下面的while{}中得到了执行
     */

    task = ngx_thread_pool_done.first;
    ngx_thread_pool_done.first = NULL;
    //尾部指向头，但是头已经变为空，即不执行任务  
    ngx_thread_pool_done.last = &ngx_thread_pool_done.first;

    ngx_unlock(&ngx_thread_pool_done_lock);

    while (task) {//遍历执行前面队列ngx_thread_pool_done中的每一个任务  
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, ev->log, 0,
                       "run completion handler for task #%ui", task->id);

        event = &task->event;
        task = task->next;

        event->complete = 1;
        event->active = 0;

      /*如果是小文件，则一次可以读完，函数指向可以参考ngx_http_cache_thread_handler  ngx_http_copy_thread_handler  ngx_thread_read

        如果是大文件下载，则第一次走这里函数式上面的几个函数，但是由于一次最多获取32768字节，因此需要多次读取文件，就是由一次tread执行完任务后
        触发ngx_notify通道epoll，然后走到这里继续读 
        */
        event->handler(event);//这里是否应该检查event->handler是否为空，例如参考ngx_thread_pool_destroy
    }
}


static void *
ngx_thread_pool_create_conf(ngx_cycle_t *cycle)
{
    ngx_thread_pool_conf_t  *tcf;

    tcf = ngx_pcalloc(cycle->pool, sizeof(ngx_thread_pool_conf_t));
    if (tcf == NULL) {
        return NULL;
    }

    if (ngx_array_init(&tcf->pools, cycle->pool, 4,
                       sizeof(ngx_thread_pool_t *))
        != NGX_OK)
    {
        return NULL;
    }

    return tcf;
}

//如果配置thread_poll default，则指定默认的threads和max_queue
static char *
ngx_thread_pool_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_thread_pool_conf_t *tcf = conf;

    ngx_uint_t           i;
    ngx_thread_pool_t  **tpp;

    tpp = tcf->pools.elts;

    for (i = 0; i < tcf->pools.nelts; i++) {

        if (tpp[i]->threads) {
            continue;
        }

        if (tpp[i]->name.len == ngx_thread_pool_default.len
            && ngx_strncmp(tpp[i]->name.data, ngx_thread_pool_default.data,
                           ngx_thread_pool_default.len)
               == 0)
        {
            tpp[i]->threads = 32;
            tpp[i]->max_queue = 65536;
            continue;
        }

        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                      "unknown thread pool \"%V\" in %s:%ui",
                      &tpp[i]->name, tpp[i]->file, tpp[i]->line);

        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

/*
Syntax: thread_pool name threads=number [max_queue=number];
Default: thread_pool default threads=32 max_queue=65536; threads参数为该pool中线程个数，max_queue表示等待被线程调度的任务数
*/
static char *
ngx_thread_pool(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t          *value;
    ngx_uint_t          i;
    ngx_thread_pool_t  *tp;

    value = cf->args->elts;

    tp = ngx_thread_pool_add(cf, &value[1]);

    if (tp == NULL) {
        return NGX_CONF_ERROR;
    }

    if (tp->threads) { //说明配置了同样的thread_pool name threads xx，重复，如果threads数不同，还是可以满足条件的，后配置的会覆盖前面配置的
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "duplicate thread pool \"%V\"", &tp->name);
        return NGX_CONF_ERROR;
    }

    tp->max_queue = 65536;

    for (i = 2; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "threads=", 8) == 0) {

            tp->threads = ngx_atoi(value[i].data + 8, value[i].len - 8);

            if (tp->threads == (ngx_uint_t) NGX_ERROR || tp->threads == 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid threads value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "max_queue=", 10) == 0) {

            tp->max_queue = ngx_atoi(value[i].data + 10, value[i].len - 10);

            if (tp->max_queue == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid max_queue value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }
    }

    if (tp->threads == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"threads\" parameter",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

//检查是否已经有该name的ngx_thread_pool_t，有则直接返回，没有则创建并添加到ngx_thread_pool_conf_t中
ngx_thread_pool_t *
ngx_thread_pool_add(ngx_conf_t *cf, ngx_str_t *name)
{
    ngx_thread_pool_t       *tp, **tpp;
    ngx_thread_pool_conf_t  *tcf;

    if (name == NULL) {
        name = &ngx_thread_pool_default;
    }
    
    //检查该名字的线程池是否已经存在，存在则直接返回以前的线程池ngx_thread_pool_t，没有返回NULL
    tp = ngx_thread_pool_get(cf->cycle, name);

    if (tp) {
        return tp;
    }

    //创建新的ngx_thread_pool_t
    tp = ngx_pcalloc(cf->pool, sizeof(ngx_thread_pool_t));
    if (tp == NULL) {
        return NULL;
    }

    tp->name = *name;
    tp->file = cf->conf_file->file.name.data;
    tp->line = cf->conf_file->line;

    tcf = (ngx_thread_pool_conf_t *) ngx_get_conf(cf->cycle->conf_ctx,
                                                  ngx_thread_pool_module);

    tpp = ngx_array_push(&tcf->pools);
    if (tpp == NULL) {
        return NULL;
    }

    *tpp = tp;

    return tp;
}

//检查该名字的线程池是否已经存在，存在则直接返回以前的线程池ngx_thread_pool_t，没有返回NULL
ngx_thread_pool_t *
ngx_thread_pool_get(ngx_cycle_t *cycle, ngx_str_t *name)
{
    ngx_uint_t                i;
    ngx_thread_pool_t       **tpp;
    ngx_thread_pool_conf_t   *tcf;

    tcf = (ngx_thread_pool_conf_t *) ngx_get_conf(cycle->conf_ctx,
                                                  ngx_thread_pool_module);

    tpp = tcf->pools.elts;

    for (i = 0; i < tcf->pools.nelts; i++) {

        if (tpp[i]->name.len == name->len
            && ngx_strncmp(tpp[i]->name.data, name->data, name->len) == 0)
        {
            return tpp[i];
        }
    }

    return NULL;
}

//在ngx_thread_pool_init_worker和 ngx_thread_pool_exit_worker分别会创建每一个线程池和销毁每一个线程池；
static ngx_int_t
ngx_thread_pool_init_worker(ngx_cycle_t *cycle)
{
    ngx_uint_t                i;
    ngx_thread_pool_t       **tpp;
    ngx_thread_pool_conf_t   *tcf;

    if (ngx_process != NGX_PROCESS_WORKER
        && ngx_process != NGX_PROCESS_SINGLE)
    {
        return NGX_OK;
    }

    tcf = (ngx_thread_pool_conf_t *) ngx_get_conf(cycle->conf_ctx,
                                                  ngx_thread_pool_module);

    if (tcf == NULL) {
        return NGX_OK;
    }

    ngx_thread_pool_queue_init(&ngx_thread_pool_done);

    tpp = tcf->pools.elts;

    for (i = 0; i < tcf->pools.nelts; i++) { //遍历所有的线程池
        if (ngx_thread_pool_init(tpp[i], cycle->log, cycle->pool) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}

//在ngx_thread_pool_init_worker和 ngx_thread_pool_exit_worker分别会创建每一个线程池和销毁每一个线程池；
static void
ngx_thread_pool_exit_worker(ngx_cycle_t *cycle)
{
    ngx_uint_t                i;
    ngx_thread_pool_t       **tpp;
    ngx_thread_pool_conf_t   *tcf;

    if (ngx_process != NGX_PROCESS_WORKER
        && ngx_process != NGX_PROCESS_SINGLE)
    {
        return;
    }

    tcf = (ngx_thread_pool_conf_t *) ngx_get_conf(cycle->conf_ctx,
                                                  ngx_thread_pool_module);

    if (tcf == NULL) {
        return;
    }

    tpp = tcf->pools.elts;

    for (i = 0; i < tcf->pools.nelts; i++) {
        ngx_thread_pool_destroy(tpp[i]);
    }
}
