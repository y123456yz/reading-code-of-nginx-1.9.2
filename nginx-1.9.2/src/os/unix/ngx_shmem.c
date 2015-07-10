
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


/*
14.2共享内存
共享内存是Linux下提供的最基本的进程间通信方法，它通过mmap或者shmget系统调用在内存中创建了一块连续的线性地址空间，而通过munmap或
者shmdt系统调用可以释放这块内存。使用共享内存的好处是当多个进程使用同一块共享内存时，在任何一个进程修改了共享内存中的内容后，其
他进程通过访问这段共享内存都能够得到修改后的内容。
    注意:虽然mmap可以以磁盘文件的方式映射共享内存，但在Nginx封装的共享内存操作方法中是没有使用到映射文件功能的。

    操作ngx_shm_t结构体的方法有以下两个：ngx_shm_alloc用于分配新的共享内存，而
ngx_shm_free用于释放已经存在的共享内存。在描述这两个方法前，先以mmap为例说朗
Linux是怎样向应用程序提供共享内存的，如下所示。
void *mmap (void *start, size_t length,  int prot,  int flags, int fd, off_t offset) ;
    mmap可以将磁盘文件映射到内存中，直接操作内存时Linux内核将负责同步内存和磁
盘文件中的数据，fd参数就指向需要同步的磁盘文件，而offset则代表从文件的这个偏移量
处开始共享，当然Nginx没有使用这一特性。当flags参数中加入MAP ANON或者MAP—
ANONYMOUS参数时表示不使用文件映射方式，这时fd和offset参数就没有意义，也不
需要传递了，此时的mmap方法和ngx_shm_alloc的功能几乎完全相同。length参数就是将
要在内存中开辟的线性地址空间大小，而prot参数则是操作这段共享内存的方式（如只读
或者可读可写），start参数说明希望的共享内存起始映射地址，当然，通常都会把start设为
NULL空指针。
    先来看看如何使用mmap实现ngx_shm_alloc方法，代码如下。
ngx_int_t ngx_shm_ alloc (ngx_shm_t  ~shm)
{
    ／／开辟一块shm- >size大小且可以读／写的共享内存，内存首地址存放在addr中
    shm->addr=  (uchar *)mmap (NULL,  shm->size,
    PROT_READ l PROT_WRITE,
    MAP_ANONIMAP_SHARED,  -1,o);
if (shm->addr == MAP_FAILED)
     return NGX ERROR;
}
    return NGX OK;
    )
    这里不再介绍shmget方法申请共享内存的方式，它与上述代码相似。
    当不再使用共享内存时，需要调用munmap或者shmdt来释放共享内存，这里还是以与
mmap配对的munmap为例来说明。
    其中，start参数指向共享内存的首地址，而length参数表示这段共享内存的长度。下面
看看ngx_shm_free方法是怎样通过munmap来释放共享内存的。
    void  ngx_shm—free (ngx_shm_t★shm)
    {
    ／／使用ngx_shm_t中的addr和size参数调用munmap释放共享内存即可
    if  (munmap( (void★)  shm->addr,  shm- >size)  ==~1)  (
    ngx_log_error (NGX—LOG__  ALERT,  shm- >log,    ngx_errno,  ”munmap(%p,  %uz)
failed",  shm- >addr,   shm- >size)j
    )
    )
    Nginx各进程间共享数据的主要方式就是使用共享内存（在使用共享内存时，Nginx -
般是由master进程创建，在master进程fork出worker子进程后，所有的进程开始使用这
块内存中的数据）。在开发Nginx模块时如果需要使用它，不妨用Nginx已经封装好的ngx_
shm—alloc方法和ngx_shm_free方法，它们有3种实现（不映射文件使用mmap分配共享
内存、以/dev/zero文件使用mmap映射共享内存、用shmget调用来分配共享内存），对于
Nginx的跨平台特性考虑得很周到。下面以一个统计HTTP框架连接状况的例子来说明共享
内存的用法。

*/

#include <ngx_config.h>
#include <ngx_core.h>

/*
在开发Nginx模块时如果需要使用它，不妨用Nginx已经封装好的ngx_shm_alloc方法和ngx_shm_free方法，它们有3种实现（不映射文件使用mmap分配共享
内存、以/dev/zero文件使用mmap映射共享内存、用shmget(system-v标准)调用来分配共享内存）
*/
//#if这里的三个都define为1，所以首先满足第一个条件，选择第一个if中的
#if (NGX_HAVE_MAP_ANON)

ngx_int_t
ngx_shm_alloc(ngx_shm_t *shm)
{
    shm->addr = (u_char *) mmap(NULL, shm->size,
                                PROT_READ|PROT_WRITE,
                                MAP_ANON|MAP_SHARED, -1, 0);

    if (shm->addr == MAP_FAILED) {
        ngx_log_error(NGX_LOG_ALERT, shm->log, ngx_errno,
                      "mmap(MAP_ANON|MAP_SHARED, %uz) failed", shm->size);
        return NGX_ERROR;
    }

    return NGX_OK;
}


void
ngx_shm_free(ngx_shm_t *shm)
{
    if (munmap((void *) shm->addr, shm->size) == -1) {
        ngx_log_error(NGX_LOG_ALERT, shm->log, ngx_errno,
                      "munmap(%p, %uz) failed", shm->addr, shm->size);
    }
}

#elif (NGX_HAVE_MAP_DEVZERO)
/*
在开发Nginx模块时如果需要使用它，不妨用Nginx已经封装好的ngx_shm_alloc方法和ngx_shm_free方法，它们有3种实现（不映射文件使用mmap分配共享
内存、以/dev/zero文件使用mmap映射共享内存、用shmget(system-v标准)调用来分配共享内存）
*/

ngx_int_t
ngx_shm_alloc(ngx_shm_t *shm)
{
    ngx_fd_t  fd;
  
    fd = open("/dev/zero", O_RDWR);

    if (fd == -1) {
        ngx_log_error(NGX_LOG_ALERT, shm->log, ngx_errno,
                      "open(\"/dev/zero\") failed");
        return NGX_ERROR;
    }

    shm->addr = (u_char *) mmap(NULL, shm->size, PROT_READ|PROT_WRITE,
                                MAP_SHARED, fd, 0);

    if (shm->addr == MAP_FAILED) {
        ngx_log_error(NGX_LOG_ALERT, shm->log, ngx_errno,
                      "mmap(/dev/zero, MAP_SHARED, %uz) failed", shm->size);
    }

    if (close(fd) == -1) {
        ngx_log_error(NGX_LOG_ALERT, shm->log, ngx_errno,
                      "close(\"/dev/zero\") failed");
    }

    return (shm->addr == MAP_FAILED) ? NGX_ERROR : NGX_OK;
}


void
ngx_shm_free(ngx_shm_t *shm)
{
    if (munmap((void *) shm->addr, shm->size) == -1) {
        ngx_log_error(NGX_LOG_ALERT, shm->log, ngx_errno,
                      "munmap(%p, %uz) failed", shm->addr, shm->size);
    }
}

#elif (NGX_HAVE_SYSVSHM)
/*
在开发Nginx模块时如果需要使用它，不妨用Nginx已经封装好的ngx_shm_alloc方法和ngx_shm_free方法，它们有3种实现（不映射文件使用mmap分配共享
内存、以/dev/zero文件使用mmap映射共享内存、用shmget(system-v标准)调用来分配共享内存）
*/

#include <sys/ipc.h>
#include <sys/shm.h>


ngx_int_t
ngx_shm_alloc(ngx_shm_t *shm)
{
    int  id;
 
    id = shmget(IPC_PRIVATE, shm->size, (SHM_R|SHM_W|IPC_CREAT));

    if (id == -1) {
        ngx_log_error(NGX_LOG_ALERT, shm->log, ngx_errno,
                      "shmget(%uz) failed", shm->size);
        return NGX_ERROR;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, shm->log, 0, "shmget id: %d", id);

    shm->addr = shmat(id, NULL, 0);

    if (shm->addr == (void *) -1) {
        ngx_log_error(NGX_LOG_ALERT, shm->log, ngx_errno, "shmat() failed");
    }

    if (shmctl(id, IPC_RMID, NULL) == -1) {
        ngx_log_error(NGX_LOG_ALERT, shm->log, ngx_errno,
                      "shmctl(IPC_RMID) failed");
    }

    return (shm->addr == (void *) -1) ? NGX_ERROR : NGX_OK;
}


void
ngx_shm_free(ngx_shm_t *shm)
{
    if (shmdt(shm->addr) == -1) {
        ngx_log_error(NGX_LOG_ALERT, shm->log, ngx_errno,
                      "shmdt(%p) failed", shm->addr);
    }
}

#endif
