
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


ngx_int_t
ngx_thread_cond_create(ngx_thread_cond_t *cond, ngx_log_t *log)
{
    ngx_err_t  err;

    err = pthread_cond_init(cond, NULL);
    if (err == 0) {
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, log, 0,
                       "pthread_cond_init(%p)", cond);
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_EMERG, log, err, "pthread_cond_init() failed");
    return NGX_ERROR;
}


ngx_int_t
ngx_thread_cond_destroy(ngx_thread_cond_t *cond, ngx_log_t *log)
{
    ngx_err_t  err;

    err = pthread_cond_destroy(cond);
    if (err == 0) {
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, log, 0,
                       "pthread_cond_destroy(%p)", cond);
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_EMERG, log, err, "pthread_cond_destroy() failed");
    return NGX_ERROR;
}


/*
pthread_cond_wait必须放在pthread_mutex_lock和pthread_mutex_unlock之间，因为他要根据共享变量的状态来决定是否要等待，而为了不永
远等待下去所以必须要在lock/unlock队中

共享变量的状态改变必须遵守lock/unlock的规则
pthread_cond_signal即可以放在pthread_mutex_lock和pthread_mutex_unlock之间，也可以放在pthread_mutex_lock和pthread_mutex_unlock之后，但是各有各缺点。

   


之间

pthread_mutex_lock
xxxxxxx
pthread_cond_signal
pthread_mutex_unlock

缺点：在某些线程的实现中，会造成等待线程从内核中唤醒（由于cond_signal)然后又回到内核空间（因为cond_wait返回后会有原子加锁的行为），所以一来
一回会有性能的问题。但是在LinuxThreads或者NPTL里面，就不会有这个问题，因为在Linux 线程中，有两个队列，分别是cond_wait队列和mutex_lock队列， 
cond_signal只是让线程从cond_wait队列移到mutex_lock队列，而不用返回到用户空间，不会有性能的损耗。
所以在Linux中推荐使用这种模式。


之后

pthread_mutex_lock
xxxxxxx
pthread_mutex_unlock
pthread_cond_signal

优点：不会出现之前说的那个潜在的性能损耗，因为在signal之前就已经释放锁了

缺点：如果unlock和signal之前，有个低优先级的线程正在mutex上等待的话，那么这个低优先级的线程就会抢占高优先级的线程（cond_wait的线程)，
而这在上面的放中间的模式下是不会出现的。

所以，在Linux下最好pthread_cond_signal放中间，但从编程规则上说，其他两种都可以
*/
ngx_int_t
ngx_thread_cond_signal(ngx_thread_cond_t *cond, ngx_log_t *log)
{
    ngx_err_t  err;

    err = pthread_cond_signal(cond);
    if (err == 0) {
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, log, 0,
                       "pthread_cond_signal(%p)", cond);
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_EMERG, log, err, "pthread_cond_signal() failed");
    return NGX_ERROR;
}


ngx_int_t
ngx_thread_cond_wait(ngx_thread_cond_t *cond, ngx_thread_mutex_t *mtx,
    ngx_log_t *log)
{
    ngx_err_t  err;

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, log, 0,
                   "pthread_cond_wait(%p) enter", cond);

/*
    pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t*mutex)函数传入的参数mutex用于保护条件，因为我们在调用pthread_cond_wait时，
如果条件不成立我们就进入阻塞，但是进入阻塞这个期间，如果条件变量改变了的话，那我们就漏掉了这个条件。因为这个线程还没有放到等待队列上，
所以调用pthread_cond_wait前要先锁互斥量，即调用pthread_mutex_lock()，pthread_cond_wait在把线程放进阻塞队列后，自动对mutex进行解锁，
使得其它线程可以获得加锁的权利。这样其它线程才能对临界资源进行访问并在适当的时候唤醒这个阻塞的进程。当pthread_cond_wait返回的时候
又自动给mutex加锁。
实际上边代码的加解锁过程如下：
/ ***********pthread_cond_wait()的使用方法********** /
pthread_mutex_lock(&qlock); / *lock* /
pthread_cond_wait(&qready, &qlock); / *block-->unlock-->wait() return-->lock* /
pthread_mutex_unlock(&qlock); / *unlock* /

*/
//pthread_cond_wait内部执行流程是:进入阻塞--解锁--进入线程等待队列，添加满足(其他线程ngx_thread_cond_signal)后，再次加锁，然后返回，
//为什么在调用该函数的时候外层要先加锁，原因是在pthread_cond_wait内部进入阻塞状态有个过程，这个过程中其他线程cond signal，本线程
//检测不到该信号signal，所以外层加锁就是让本线程进入wait线程等待队列后，才允许其他线程cond signal唤醒本线程，就可以避免漏掉信号
    err = pthread_cond_wait(cond, mtx); //该函数内部执行过程:block-->unlock-->wait() return-->lock

#if 0
    ngx_time_update();
#endif

    if (err == 0) {
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, log, 0,
                       "pthread_cond_wait(%p) exit", cond);
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_ALERT, log, err, "pthread_cond_wait() failed");

    return NGX_ERROR;
}
