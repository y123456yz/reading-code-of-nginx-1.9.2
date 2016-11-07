
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>

/*
信号量的数据类型为结构sem_t，它本质上是一个长整型的数。函数sem_init（）用来初始化一个信号量。它的原型为：　　 
extern int sem_init __P ((sem_t *__sem, int __pshared, unsigned int __value));　　

sem为指向信号量结构的一个指针；pshared不为０时此信号量在进程间共享，否则只能为当前进程的所有线程共享；value给出了信号量的初始值。　　

函数sem_post( sem_t *sem )用来增加信号量的值。当有线程阻塞在这个信号量上时，调用这个函数会使其中的一个线程不在阻塞，选择机制同样是由线程的调度策略决定的。　　

函数sem_wait( sem_t *sem )被用来阻塞当前线程直到信号量sem的值大于0，解除阻塞后将sem的值减一，表明公共资源经使用后减少。函数sem_trywait ( sem_t *sem )是函数sem_wait（）的非阻塞版本，它直接将信号量sem的值减一。　　

函数sem_destroy(sem_t *sem)用来释放信号量sem。　

信号量用sem_init函数创建的，下面是它的说明：
#include<semaphore.h>
       int sem_init (sem_t *sem, int pshared, unsigned int value);

       这个函数的作用是对由sem指定的信号量进行初始化，设置好它的 共享选项，并指定一个整数类型的初始值。pshared参数控制着信号量的类型。如果 pshared的值是０，就表示它是当前里程的局部信号量；否则，其它进程就能够共享这个信号量。我们现在只对不让进程共享的信号量感兴趣。　（这个参数 受版本影响），　pshared传递一个非零将会使函数调用失败。

这两个函数控制着信号量的值，它们的定义如下所示： 

#include <semaphore.h> 
int sem_wait(sem_t * sem);
int sem_post(sem_t * sem);
这两个函数都要用一个由sem_init调用初始化的信号量对象的指针做参数。
sem_post函数的作用是给信号量的值加上一个“1”，它是 一个“原子操作”－－－即同时对同一个信号量做加“1”操作的两个线程是不会冲突的；而同 时对同一个文件进行读、加和写操作的两个程序就有可能会引起冲突。信号量的值永远会正确地加一个“2”－－因为有两个线程试图改变它。
sem_wait函数也是一个原子操作，它的作用是从信号量的值 减去一个“1”，但它永远会先等待该信号量为一个非零值才开始做减法。也就是说，如果你对 一个值为2的信号量调用sem_wait(),线程将会继续执行，介信号量的值将减到1。如果对一个值为0的信号量调用sem_wait()，这个函数就 会地等待直到有其它线程增加了这个值使它不再是0为止。如果有两个线程都在sem_wait()中等待同一个信号量变成非零值，那么当它被第三个线程增加 一个“1”时，等待线程中只有一个能够对信号量做减法并继续执行，另一个还将处于等待状态。
信号量这种“只用一个函数就能原子化地测试和设置”的能力下正是它的价值所在。 还有另外一个信号量函数sem_trywait，它是sem_wait的非阻塞搭档。

最后一个信号量函数是sem_destroy。这个函数的作用是在我们用完信号量对它进行清理。下面的定义：
 #include<semaphore.h>
 int sem_destroy (sem_t *sem);
 这个函数也使用一个信号量指针做参数，归还自己战胜的一切资源。在清理信号量的时候如果还有线程在等待它，用户就会收到一个错误。
与其它的函数一样，这些函数在成功时都返回“0”。


信号量是如何实现互斥锁功能的呢？例如，最初的信号量sem值为0，调用sem_post方
法将会把sem值加1，这个操作不会有任何阻塞；调用sem__ wait方浩将会把信号量sem的值
减l，如果sem值已经小于或等于0了，则阻塞住当前进程（进程会进入睡眠状态），直到
其他进程将信号量sem的值改变为正数后，这时才能继续通过将sem减1而使得当前进程
继续向下执行。因此，sem_post方法可以实现解锁的功能，而sem wait方法可以实现加锁
的功能。

nginx是原子变量和信号量合作以实现高效互斥锁的

表14-1互斥锁的5种操作方法
┏━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━┓
┃    方法名        ┃    参数                                      ┃    意义                        ┃
┣━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━┫
┃                  ┃  参数mtx表示待操作的ngx_shmtx_t类型互斥锁；  ┃                                ┃
┃                  ┃当互斥锁由原子变量实现时，参数addr表示要操作  ┃                                ┃
┃ngx_shmtx_create  ┃的原子变量锁，而互斥锁由文件实现时，参数addr  ┃  初始化mtx互斥锁               ┃
┃                  ┃没有任何意义；参数name仅当互斥锁由文件实现时  ┃                                ┃
┃                  ┃才有意义，它表示这个文件所在的路径及文件名    ┃                                ┃
┣━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━┫
┃ngx_shmtx_destory ┃  参数mtx表示待操作的ngx_shmtx_t类型互斥锁    ┃  销毁mtx互斥锁                 ┃
┣━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━┫
┃                  ┃                                              ┃  无阻塞地试图获取互斥锁，返回  ┃
┃ngx_shmtx_trylock ┃  参数mtx表示待操作的ngx_shmtx_t类型互斥锁    ┃1表示获取互斥锁成功，返回0表示  ┃
┃                  ┃                                              ┃获取互斥锁失败                  ┃
┣━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━┫
┃                  ┃                                              ┃  以阻塞进程的方武获取互斥锁，  ┃
┃ngx_shmtx_lock    ┃  参数mtx表示待操作的ngx_shmtx_t类型互斥锁    ┃                                ┃
┃                  ┃                                              ┃在方法返回时就已经持有互斥锁了  ┃
┣━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━┫
┃ngx_shmtx_unlock  ┃  参数mtx表示待操作的ngx_shmtx_t类型互斥锁    ┃  释放互斥锁                    ┃
┗━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━┛
*/

#if (NGX_HAVE_ATOMIC_OPS) //支持原子操作  //支持原子操作，则通过原子操作实现锁


static void ngx_shmtx_wakeup(ngx_shmtx_t *mtx);
/*
ngx_shmtx_t结构体涉及两个宏：NGX_HAVE_ATOMIC_OPS、NGX_HAVE_POSIX_SEM，这两个宏对应着互斥锁的3种不同实现。
    第1种实现，当不支持原子操作时，会使用文件锁来实现ngx_shmtx_t互斥锁，这时它仅有fd和name成员（实际上还有spin成员，
    但这时没有任何意义）。这两个成员使用文件锁来提供阻塞、非阻塞的互斥锁。
    第2种实现，支持原子操作却又不支持信号量。
    第3种实现，在支持原子操作的同时，操作系统也支持信号量。
    后两种实现的唯一区别是ngx_shmtx_lock方法执行时的效果，也就是说，支持信号量
只会影响阻塞进程的ngx_shmtx_lock方法持有锁的方式。当不支持信号量时，ngx_shmtx_
lock取锁与自旋锁是一致的，而支持信号量后，ngx_shmtx_lock将在spin
指定的一段时间内自旋等待其他处理器释放锁，如果达到spin上限还没有获取到锁，那么将
会使用sem_wait使得刍前进程进入睡眠状态，等其他进程释放了锁内核后才会唤醒这个进
程。当然，在实际实现过程中，Nginx做了非常巧妙的设计，它使得ngx_shmtx_lock方法在
运行一段时间后，如果其他进程始终不放弃锁，那么当前进程将有可能强制性地获得到这把
锁，这也是出于Nginx不宜使用阻塞进程的睡眠锁方面的考虑。
*/
//addr为共享内存ngx_shm_alloc开辟的空间中的一个128字节首地址  nginx是原子变量和信号量合作以实现高效互斥锁的
ngx_int_t
ngx_shmtx_create(ngx_shmtx_t *mtx, ngx_shmtx_sh_t *addr, u_char *name)
{
    mtx->lock = &addr->lock;    //直接执行共享内存空间addr中的lock区间中

    if (mtx->spin == (ngx_uint_t) -1) { //注意，当spin值为-1时，表示不能使用信号量，这时直接返回成功
        return NGX_OK;
    }

    mtx->spin = 2048; //spin值默认为2048

//同时使用信号量
#if (NGX_HAVE_POSIX_SEM)
    mtx->wait = &addr->wait;

    /*
    int  sem init (sem_t  sem,  int pshared,  unsigned int value) ,
    其中，参数sem即为我们定义的信号量，而参数pshared将指明sem信号量是用于进程间同步还是用于线程间同步，当pshared为0时表示线程间同步，
    而pshared为1时表示进程间同步。由于Nginx的每个进程都是单线程的，因此将参数pshared设为1即可。参数value表示信号量sem的初始值。
     */
    //以多进程使用的方式初始化sem信号量，sem初始值为0
    if (sem_init(&mtx->sem, 1, 0) == -1) {
        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_errno,
                      "sem_init() failed");
    } else {
        mtx->semaphore = 1; //在信号量初始化成功后，设置semaphore标志位为1
    }

#endif

    return NGX_OK;
}


void
ngx_shmtx_destroy(ngx_shmtx_t *mtx)
{
#if (NGX_HAVE_POSIX_SEM) //支持信号量时才有代码需要执行

    if (mtx->semaphore) { //当这把锁的spin值不为(ngx_uint_t)    -1时，且初始化信号量成功，semaphore标志位才为l
        if (sem_destroy(&mtx->sem) == -1) { //销毁信号量
            ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_errno,
                          "sem_destroy() failed");
        }
    }

#endif
}

/*
首先是判断mtx的lock域是否等于0，如果不等于，那么就直接返回false好了，如果等于的话，那么就要调用原子操作ngx_atomic_cmp_set了，
它用于比较mtx的lock域，如果等于零，那么设置为当前进程的进程id号，否则返回false。
*/ //不管能不能获得到锁都返回  nginx是原子变量和信号量合作以实现高效互斥锁的
ngx_uint_t
ngx_shmtx_trylock(ngx_shmtx_t *mtx)
{
    return (*mtx->lock == 0 && ngx_atomic_cmp_set(mtx->lock, 0, ngx_pid));
}

/*
阻塞式获取互斥锁的ngx_shmtx_lock方法较为复杂，在不支持信号量时它与14.3.3节介绍的自旋锁几乎完全相同，但在支持了信号量后，它
将有可能使进程进入睡眠状态。
*/
//这里可以看出支持原子操作的系统，他的信号量其实就是自旋和信号量的结合   nginx是原子变量和信号量合作以实现高效互斥锁的
void
ngx_shmtx_lock(ngx_shmtx_t *mtx)
{
    ngx_uint_t         i, n;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0, "shmtx lock");
    
    //一个死循环，不断的去看是否获取了锁，直到获取了之后才退出   
    //所以支持原子变量的
    for ( ;; ) {
 //lock值是当前的锁状态。注意，lock一般是在共享内存中的，它可能会时刻变化，而val是当前进程的栈中变量，下面代码的执行中它可能与lock值不一致
        if (*mtx->lock == 0 && ngx_atomic_cmp_set(mtx->lock, 0, ngx_pid)) {
            return;
        }
        //仅在多处理器状态下spin值才有意义，否则PAUSE指令是不会执行的
        if (ngx_ncpu > 1) {
            //循环执行PAUSE，检查锁是否已经释放
            for (n = 1; n < mtx->spin; n <<= 1) {
                //随着长时间没有获得到锁，将会执行更多次PAUSE才会检查锁
                for (i = 0; i < n; i++) {
                    ngx_cpu_pause();
                }

                //再次由共享内存中获得lock原子变量的值
                if (*mtx->lock == 0
                    && ngx_atomic_cmp_set(mtx->lock, 0, ngx_pid))
                {
                    return;
                }
            }
        }

#if (NGX_HAVE_POSIX_SEM) //支持信号量时才继续执行

        if (mtx->semaphore) {//semaphore标志位为1才使用信号量
            (void) ngx_atomic_fetch_add(mtx->wait, 1);

            //重新获取一次可能虚共享内存中的lock原子变量
            if (*mtx->lock == 0 && ngx_atomic_cmp_set(mtx->lock, 0, ngx_pid)) {
                (void) ngx_atomic_fetch_add(mtx->wait, -1);
                return;
            }

            ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                           "shmtx wait %uA", *mtx->wait);

            //如果没有拿到锁，这时Nginx进程将会睡眠，直到其他进程释放了锁
            /*
                检查信号量sem的值，如果sem值为正数，则sem值减1，表示拿到了信号量互斥锁，同时sem wait方法返回o。如果sem值为0或
                者负数，则当前进程进入睡眠状态，等待其他进程使用ngx_shmtx_unlock方法释放锁（等待sem信号量变为正数），到时Linux内核
                会重新调度当前进程，继续检查sem值是否为正，重复以上流程
               */
            while (sem_wait(&mtx->sem) == -1) {
                ngx_err_t  err;

                err = ngx_errno;

                if (err != NGX_EINTR) {//当EINTR信号出现时，表示sem wait只是被打断，并不是出错
                    ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, err,
                                  "sem_wait() failed while waiting on shmtx");
                    break;
                }
            }

            ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                           "shmtx awoke");

            continue; //循环检查lock锁的值，注意，当使用信号量后不会调用sched_yield
        }

#endif

        ngx_sched_yield(); //在不使用信号量时，调用sched_yield将会使当前进程暂时“让出”处理器
    }
}

/*
ngx_shmtx_unlock方法会释放锁，虽然这个释放过程不会阻塞进程，但设置原子变量lock值时是可能失败的，因为多进程在同时修改lock值，
而ngx_atomic_cmp_s et方法要求参数old的值与lock值相同时才能修改成功，因此，ngx_atomic_cmp_set方法会在循环中反复执行，直到返回
成功为止。
*/
//判断锁的lock域与当前进程的进程id是否相等，如果相等的话，那么就将lock设置为0，然后就相当于释放了锁。
void
ngx_shmtx_unlock(ngx_shmtx_t *mtx)
{
    if (mtx->spin != (ngx_uint_t) -1) {
        ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0, "shmtx unlock");
    }

    if (ngx_atomic_cmp_set(mtx->lock, ngx_pid, 0)) {
        ngx_shmtx_wakeup(mtx);
    }
}


ngx_uint_t
ngx_shmtx_force_unlock(ngx_shmtx_t *mtx, ngx_pid_t pid)
{
    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                   "shmtx forced unlock");

    if (ngx_atomic_cmp_set(mtx->lock, pid, 0)) {
        ngx_shmtx_wakeup(mtx);
        return 1;
    }

    return 0;
}


static void
ngx_shmtx_wakeup(ngx_shmtx_t *mtx)
{
#if (NGX_HAVE_POSIX_SEM)
    ngx_atomic_uint_t  wait;

    if (!mtx->semaphore) {
        return;
    }

    for ( ;; ) {

        wait = *mtx->wait;
        //如果lock锁原先的值为o，也就是说，并没有让某个进程持有锁，这时直接返回；或者，semaphore标志位为0，表示不需要使用信号量，也立即返回
        if ((ngx_atomic_int_t) wait <= 0) {
            return;
        }

        if (ngx_atomic_cmp_set(mtx->wait, wait, wait - 1)) {
            break;
        }
    }

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                   "shmtx wake %uA", wait);

    //释放信号量锁时是不会使进程睡眠的
    //通过sem_post将信号量sem加1，表示当前进程释放了信号量互斥锁，通知其他进程的sem_wait继续执行
    if (sem_post(&mtx->sem) == -1) {
        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_errno,
                      "sem_post() failed while wake shmtx");
    }

#endif
}


#else  //else后的锁是文件锁实现的ngx_shmtx_t锁   //不支持原子操作，则通过文件锁实现


ngx_int_t
ngx_shmtx_create(ngx_shmtx_t *mtx, ngx_shmtx_sh_t *addr, u_char *name)
{
    //不用在调用ngx_shmtx_create方法前先行赋值给ngx_shmtx_t结构体中的成员
    if (mtx->name) {
        /*
         如果ngx_shmtx_t中的name成员有值，那么如果与name参数相同，意味着mtx互斥锁已经初始化过了；否则，需要先销毁mtx中的互斥锁再重新分配mtx
          */
        if (ngx_strcmp(name, mtx->name) == 0) {
            mtx->name = name; //如果name参数与ngx_shmtx_t中的name成员桐同，则表示已经初始化了
            return NGX_OK; //既然曾经初始化过，证明fd句柄已经打开过，直接返回成功即可
        }

        /*
           如果ngx_s hmtx_t中的name与参数name不一致，说明这一次使用了一个新的文件作为文件锁，那么先调用ngx_shmtx_destory方法销毁原文件锁
          */
        ngx_shmtx_destroy(mtx);
    }

    mtx->fd = ngx_open_file(name, NGX_FILE_RDWR, NGX_FILE_CREATE_OR_OPEN,
                            NGX_FILE_DEFAULT_ACCESS);

    if (mtx->fd == NGX_INVALID_FILE) { //一旦文件因为各种原因（如权限不够）无法打开，通常会出现无法运行错误
        ngx_log_error(NGX_LOG_EMERG, ngx_cycle->log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", name);
        return NGX_ERROR;
    }

    //由于只需要这个文件在内核中的INODE信息，所以可以把文件删除，只要fd可用就行
    if (ngx_delete_file(name) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_errno,
                      ngx_delete_file_n " \"%s\" failed", name);
    }

    mtx->name = name;

    return NGX_OK;
}


void
ngx_shmtx_destroy(ngx_shmtx_t *mtx)
{
    if (ngx_close_file(mtx->fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", mtx->name);
    }
}

//ngx_shmtx_trylock方法试图使用非阻塞的方式获得锁，返回1时表示获取锁成功，返回0表示获取锁失败
ngx_uint_t
ngx_shmtx_trylock(ngx_shmtx_t *mtx)  //ngx_shmtx_unlock和ngx_shmtx_lock对应
{
    ngx_err_t  err;
    printf("yang test xxxxxxxxxxxxxxxxxxxxxx 1111111111111111111111\r\n");

    //由14.7节介绍过的ngx_t rylock_fd方法实现非阻塞互斥文件锁的获取
    err = ngx_trylock_fd(mtx->fd);

    if (err == 0) {
        return 1;
    }

    if (err == NGX_EAGAIN) { //如果err错误码是NGX EAGAIN，则农示现在锁已经被其他进程持有了
        return 0;
    }

#if __osf__ /* Tru64 UNIX */

    if (err == NGX_EACCES) {
        return 0;
    }

#endif

    ngx_log_abort(err, ngx_trylock_fd_n " %s failed", mtx->name);

    return 0;
}

/*
ngx_shmtx_lock方法将会在获取锁失败时阻塞代码的继续执行，它会使当前进程处于睡眠状态，等待其他进程释放锁后内核唤醒它。
可见，它是通过14.7节介绍的ngx_lock_fd方法实现的，如下所示。
*/
void
ngx_shmtx_lock(ngx_shmtx_t *mtx) //ngx_shmtx_unlock和ngx_shmtx_lock对应
{
    ngx_err_t  err;

    err = ngx_lock_fd(mtx->fd);

    if (err == 0) { //ngx_lock_fd方法返回0时表示成功地持有锁，返回-1时表示出现错误
        return;
    }

    ngx_log_abort(err, ngx_lock_fd_n " %s failed", mtx->name);
}

//ngx_shmtx_lock方法没有返回值，因为它一旦返回就相当于获取到互斥锁了，这会使得代码继续向下执行。
//ngx_shmtx unlock方法通过调用ngx_unlock_fd方法来释放文件锁
void
ngx_shmtx_unlock(ngx_shmtx_t *mtx)
{
    ngx_err_t  err;

    err = ngx_unlock_fd(mtx->fd);

    if (err == 0) {
        return;
    }

    ngx_log_abort(err, ngx_unlock_fd_n " %s failed", mtx->name);
}


ngx_uint_t
ngx_shmtx_force_unlock(ngx_shmtx_t *mtx, ngx_pid_t pid)
{
    return 0;
}

#endif
