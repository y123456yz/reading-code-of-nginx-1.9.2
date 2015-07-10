
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_SHMTX_H_INCLUDED_
#define _NGX_SHMTX_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct {
    ngx_atomic_t   lock;
#if (NGX_HAVE_POSIX_SEM)
    ngx_atomic_t   wait;
#endif
} ngx_shmtx_sh_t;

/*
ngx_shmtx_t结构体涉及两个宏：NGX_HAVE_ATOMIC_OPS、NGX_HAVE_POSIX_SEM，这两个宏对应着互斥锁的3种不同实现。
    第1种实现，当不支持原子操作时，会使用文件锁来实现ngx_shmtx_t互斥锁，这时它仅有fd和name成员（实际上还有spin成员，
    但这时没有任何意义）。这两个成员使用14.7节介绍的文件锁来提供阻塞、非阻塞的互斥锁。
    第2种实现，支持原子操作却又不支持信号量。
    第3种实现，在支持原子操作的同时，操作系统也支持信号量。
    后两种实现的唯一区别是ngx_shmtx_lock方法执行时的效果，也就是说，支持信号量
只会影响阻塞进程的ngx_shmtx_lock方法持有锁的方式。当不支持信号量时，ngx_shmtx_
lock取锁与14.3.3节中介绍的自旋锁是一致的，而支持信号量后，ngx_shmtx_lock将在spin
指定的一段时间内自旋等待其他处理器释放锁，如果达到spin上限还没有获取到锁，那么将
会使用sem_wait使得刍前进程进入睡眠状态，等其他进程释放了锁内核后才会唤醒这个进
程。当然，在实际实现过程中，Nginx做了非常巧妙的设计，它使得ngx_shmtx_lock方法在
运行一段时间后，如果其他进程始终不放弃锁，那么当前进程将有可能强制性地获得到这把
锁，这也是出于Nginx不宜使用阻塞进程的睡眠锁方面的考虑。
*/
//创建在ngx_shmtx_create中  原子变量锁的优先级高于文件锁
typedef struct {
#if (NGX_HAVE_ATOMIC_OPS)
/*
原子变量锁  当lock值为0或者正数时表示没有进程持有锁；当lock值为负数时表示有进程正持有锁（这里的正、负数仅相对于32位系统下有符号的整型变量）
Nginx是怎样快速判断lock值为“正数”或者“负数”的呢？很简单，因为有符号整型的最高位是用于表示符号的，其中0表示正数，1表示负数，所以，在确
定整型val是负数或者正数时，可通过判断(val&Ox80000000)==0语句的真假进行。
*/
    ngx_atomic_t  *lock;  //如果支持原子锁的话，那么使用它，它指向的是一段共享内存空间  为0表示可以获得锁
#if (NGX_HAVE_POSIX_SEM)
    ngx_atomic_t  *wait; //如果lock锁原先的值为o，也就是说，并没有让某个进程持有锁，这时直接返回；或者，semaphore标志位为0，表示不需要使用信号量，也立即返回
    ngx_uint_t     semaphore; //信号量的值，这个值大于0表示该新号量可用，默认为1，  semaphore为1时表示获取锁将可能使用到的信号量
    sem_t          sem;// sem就是信号量锁
#endif
#else
    ngx_fd_t       fd;   //不支持原子操作的话就使用文件锁来实现     使用文件锁时fd表示使用的文件句柄
    u_char        *name; // name表示文件名
#endif
    //自旋次数，表示在自旋状态下等待其他处理器执行结果中释放锁的时间。由文件锁实现时，spin没有任何意义
    /*
        注意读者可能会觉得奇怪，既然ngx_shmtx_t结构体中的spin成员对于文件锁没有任何意义，为什么不放在#if (NGX_HAVE_ ATOMIC_OPS)宏
    内呢？这是因为，对于使用ngx_shmtx_t互斥锁的代码来说，它们并不想知道互斥锁是由文件锁、原子变量或者信号量实现的。同时，spin的
    值又具备非常多的含义（C语言的编程风格导致可读性比面向对象语言差些），当仅用原子变量实现互斥锁时，spin只表示自旋等待其他处理
    器的时间，达到spin值后就会“让出”当前处理器。如果spin为0或者负值，则不会存在调用PAUSE的机会，而是直接调用sched_yield“让出”
    处理器。假设同时使用信号量，spin会多一种含义，即当spin值为(ngx_uint_t) -1时，相当于告诉这个互斥锁绝不要使用信号量使得进程进入睡氓状态。
    这点很重要，实际上，在实现第9章提到的负载均衡锁时，spin的值就是(ngx_uint_t) -1。

    由于原子变量实现的这5种互斥锁方法是Nginx中使用最广泛的同步方式，当需要Nginx支持数以万计的并发TCP请求时，通常都会把spin值设为(ngx_uint_t)一1。
    这时的互斥锁在取锁时都会采用自旋锁，对于Nginx这种单进程处理大量请求的场景来说是非常适合的，能够大量降低不必要的进程间切换带来的消耗。
    见ngx_shmtx_create
     */
     
    ngx_uint_t     spin; //spin值为-1则是告诉Nginx这把锁不可以使进程进入睡眠状态  spin值默认为2048
} ngx_shmtx_t;


ngx_int_t ngx_shmtx_create(ngx_shmtx_t *mtx, ngx_shmtx_sh_t *addr,
    u_char *name);
void ngx_shmtx_destroy(ngx_shmtx_t *mtx);
ngx_uint_t ngx_shmtx_trylock(ngx_shmtx_t *mtx);
void ngx_shmtx_lock(ngx_shmtx_t *mtx);
void ngx_shmtx_unlock(ngx_shmtx_t *mtx);
ngx_uint_t ngx_shmtx_force_unlock(ngx_shmtx_t *mtx, ngx_pid_t pid);


#endif /* _NGX_SHMTX_H_INCLUDED_ */
