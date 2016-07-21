/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


#if (NGX_THREADS)
#include <ngx_thread_pool.h>
static void ngx_thread_read_handler(void *data, ngx_log_t *log);
#endif


#if (NGX_HAVE_FILE_AIO)

/*
文件异步IO
    事件驱动模块都是在处理网络事件，而没有涉及磁盘上文件的操作。本
节将讨论Linux内核2.6.2x之后版本中支持的文件异步I/O，以及ngx_epoll_module模块是
如何与文件异步I/O配合提供服务的。这里提到的文件异步I/O并不是glibc库提供的文件异
步I/O。glibc库提供的异步I/O是基于多线程实现的，它不是真正意义上的异步I/O。而本节
说明的异步I/O是由Linux内核实现，只有在内核中成功地完成了磁盘操作，内核才会通知
进程，进而使得磁盘文件的处理与网络事件的处理同样高效。
    使用这种方式的前提是Linux内核版本中必须支持文件异步I/O。当然，它带来的好处
也非常明显，Nginx把读取文件的操作异步地提交给内核后，内核会通知I/O设备独立地执
行操作，这样，Nginx进程可以继续充分地占用CPU。而且，当大量读事件堆积到I/O设备
的队列中时，将会发挥出内核中“电梯算法”的优势，从而降低随机读取磁盘扇区的成本。
    注意Linux内核级别的文件异步I/O是不支持缓存操作的，也就是说，即使需要操作
的文件块在Linux文件缓存中存在，也不会通过读取、更改缓存中的文件块来代替实际对磁
盘的操作，虽然从阻塞worker进程的角度上来说有了很大好转，但是对单个请求来说，还是
有可能降低实际处理的速度，因为原先可以从内存中快速获取的文件块在使用了异步I/O后
则一定会从磁盘上读取。异步文件I/O是把“双刃剑”，关键要看使用场景，如果大部分用户
请求对文件的操作都会落到文件缓存中，那么不要使用异步I/O，反之则可以试着使用文件
异步I/O，看一下是否会为服务带来并发能力上的提升。
    目前，Nginx仅支持在读取文件时使用异步I/O，因为正常写入文件时往往是写入内存
中就立刻返回，效率很高，而使用异步I/O写入时速度会明显下降。
文件异步AIO优点:
        异步I/O是由Linux内核实现，只有在内核中成功地完成了磁盘操作，内核才会通知
    进程，进而使得磁盘文件的处理与网络事件的处理同样高效。这样就不会阻塞worker进程。
缺点:
        不支持缓存操作的，也就是说，即使需要操作的文件块在Linux文件缓存中存在，也不会通过读取、
    更改缓存中的文件块来代替实际对磁盘的操作。有可能降低实际处理的速度，因为原先可以从内存中快速
    获取的文件块在使用了异步I/O后则一定会从磁盘上读取
究竟是选择异步I/O还是普通I/O操作呢?
        异步文件I/O是把“双刃剑”，关键要看使用场景，如果大部分用户
    请求对文件的操作都会落到文件缓存中，那么不要使用异步I/O，反之则可以试着使用文件
    异步I/O，看一下是否会为服务带来并发能力上的提升。
        目前，Nginx仅支持在读取文件时使用异步I/O，因为正常写入文件时往往是写入内存
    中就立刻返回，效率很高，而使用异步I/O写入时速度会明显下降。异步I/O不支持写操作，因为
    异步I/O无法利用缓存，而写操作通常是落到缓存上，linux会自动将文件中缓存中的数据写到磁盘
    
    普通文件读写过程:
    正常的系统调用read/write的流程是怎样的呢？
    - 读取：内核缓存有需要的文件数据:内核缓冲区->用户缓冲区;没有:硬盘->内核缓冲区->用户缓冲区;
    - 写回：数据会从用户地址空间拷贝到操作系统内核地址空间的页缓存中去，这是write就会直接返回，操作系统会在恰当的时机写入磁盘，这就是传说中的
*/
//direct AIO可以参考http://blog.csdn.net/bengda/article/details/21871413

ngx_uint_t  ngx_file_aio = 1; //如果创建ngx_eventfd失败，置0，表示不支持AIO

#endif


ssize_t
ngx_read_file(ngx_file_t *file, u_char *buf, size_t size, off_t offset)
{
    ssize_t  n;

    ngx_log_debug5(NGX_LOG_DEBUG_CORE, file->log, 0,
                   "read file %V: %d, %p, %uz, %O", &file->name, file->fd, buf, size, offset);

#if (NGX_HAVE_PREAD)  //在配置脚本中赋值auto/unix:ngx_feature_name="NGX_HAVE_PREAD"

    n = pread(file->fd, buf, size, offset);//pread() 从文件 fd 指定的偏移 offset (相对文件开头) 上读取 count 个字节到 buf 开始位置。文件当前位置偏移保持不变。 

    if (n == -1) {
        ngx_log_error(NGX_LOG_CRIT, file->log, ngx_errno,
                      "pread() \"%s\" failed", file->name.data);
        return NGX_ERROR;
    }

#else

    if (file->sys_offset != offset) {
        if (lseek(file->fd, offset, SEEK_SET) == -1) {
            ngx_log_error(NGX_LOG_CRIT, file->log, ngx_errno,
                          "lseek() \"%s\" failed", file->name.data);
            return NGX_ERROR;
        }

        file->sys_offset = offset;
    }

    n = read(file->fd, buf, size);

    if (n == -1) {
        ngx_log_error(NGX_LOG_CRIT, file->log, ngx_errno,
                      "read() \"%s\" failed", file->name.data);
        return NGX_ERROR;
    }

    file->sys_offset += n;

#endif

    file->offset += n;//每读n字节，文件读取偏移量就加n

    return n;
}


#if (NGX_THREADS)

//ngx_thread_read中创建空间和赋值
typedef struct {
    ngx_fd_t     fd; //文件fd
    u_char      *buf; //读取文件内容到该buf中
    size_t       size; //读取文件内容大小
    off_t        offset; //从文件offset开始处读取size字节到buf中

    size_t       read; //通过ngx_thread_read_handler读取到的字节数
    ngx_err_t    err; //ngx_thread_read_handler读取返回后的错误信息
} ngx_thread_read_ctx_t; //见ngx_thread_read，该结构由ngx_thread_task_t->ctx指向

//第一次进来的时候表示开始把读任务加入线程池中处理，表示正在开始读，第二次进来的时候表示数据已经通过notify_epoll通知读取完毕，可以处理了，第一次返回NAX_AGAIN
//第二次放回线程池中的线程处理读任务读取到的字节数
ssize_t
ngx_thread_read(ngx_thread_task_t **taskp, ngx_file_t *file, u_char *buf,
    size_t size, off_t offset, ngx_pool_t *pool)
{
    /*
        该函数一般会进来两次，第一次是通过原始数据发送触发走到这里，这时候complete = 0，第二次是当线程池读取数据完成，则会通过
        ngx_thread_pool_handler->ngx_http_copy_thread_event_handler->ngx_http_request_handler->ngx_http_writer在次走到这里
     */
    ngx_thread_task_t      *task;
    ngx_thread_read_ctx_t  *ctx;

    ngx_log_debug4(NGX_LOG_DEBUG_CORE, file->log, 0,
                   "thread read: fd:%d, buf:%p, size:%uz, offset:%O",
                   file->fd, buf, size, offset);

    task = *taskp;

    if (task == NULL) {
        task = ngx_thread_task_alloc(pool, sizeof(ngx_thread_read_ctx_t));
        if (task == NULL) {
            return NGX_ERROR;
        }

        task->handler = ngx_thread_read_handler;

        *taskp = task;
    }

    ctx = task->ctx;

    if (task->event.complete) {
    /*
    该函数一般会进来两次，第一次是通过原始数据发送触发走到这里，这时候complete = 0，第二次是当线程池读取数据完成，则会通过
    ngx_thread_pool_handler->ngx_http_copy_thread_event_handler->ngx_http_request_handler->ngx_http_writer在次走到这里，不过
    这次complete已经在ngx_thread_pool_handler置1
     */   
        task->event.complete = 0;

        if (ctx->err) {
            ngx_log_error(NGX_LOG_CRIT, file->log, ctx->err,
                          "pread() \"%s\" failed", file->name.data);
            return NGX_ERROR;
        }

        return ctx->read;
    }

    ctx->fd = file->fd;
    ctx->buf = buf;
    ctx->size = size;
    ctx->offset = offset;

    //这里添加task->event信息到task中，当task->handler指向完后，通过nginx_notify可以继续通过epoll_wait返回执行task->event
    //客户端过来后如果有缓存存在，则ngx_http_file_cache_aio_read中赋值为ngx_http_cache_thread_handler;  
    //如果是从后端获取的数据，然后发送给客户端，则ngx_output_chain_as_is中赋值未ngx_http_copy_thread_handler
    if (file->thread_handler(task, file) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_AGAIN;
}


#if (NGX_HAVE_PREAD)
//在ngx_thread_read把该handler添加到线程池中
static void //ngx_thread_read->ngx_thread_read_handler
ngx_thread_read_handler(void *data, ngx_log_t *log)
{//该函数执行后，会通过ngx_notify执行event.handler = ngx_http_cache_thread_event_handler;
    ngx_thread_read_ctx_t *ctx = data;

    ssize_t  n;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, log, 0, "thread read handler");

    //缓存文件数据会拷贝到dst中，也就是ngx_output_chain_ctx_t->buf,然后在ngx_output_chain_copy_buf函数外层会重新把ctx->buf赋值给新的chain，然后write出去
    n = pread(ctx->fd, ctx->buf, ctx->size, ctx->offset);

    if (n == -1) {
        ctx->err = ngx_errno;

    } else {
        ctx->read = n;
        ctx->err = 0;
    }

#if 0
    ngx_time_update();
#endif

    ngx_log_debug4(NGX_LOG_DEBUG_CORE, log, 0,
                   "pread read return read size: %z (err: %i) of buf-size%uz offset@%O",
                   n, ctx->err, ctx->size, ctx->offset);
}

#else

#error pread() is required!

#endif

#endif /* NGX_THREADS */


ssize_t
ngx_write_file(ngx_file_t *file, u_char *buf, size_t size, off_t offset)
{
    ssize_t  n, written;

    ngx_log_debug5(NGX_LOG_DEBUG_CORE, file->log, 0,
                   "write to filename:%V,fd: %d, buf:%p, size:%uz, offset:%O", &file->name, file->fd, buf, size, offset);

    written = 0;

#if (NGX_HAVE_PWRITE)

    for ( ;; ) {
        //pwrite() 把缓存区 buf 开头的 count 个字节写入文件描述符 fd offset 偏移位置上。文件偏移没有改变。
        n = pwrite(file->fd, buf + written, size, offset);

        if (n == -1) {
            ngx_log_error(NGX_LOG_CRIT, file->log, ngx_errno,
                          "pwrite() \"%s\" failed", file->name.data);
            return NGX_ERROR;
        }

        file->offset += n;
        written += n;

        if ((size_t) n == size) {
            return written;
        }

        offset += n;
        size -= n;
    }

#else

    if (file->sys_offset != offset) {
        if (lseek(file->fd, offset, SEEK_SET) == -1) {
            ngx_log_error(NGX_LOG_CRIT, file->log, ngx_errno,
                          "lseek() \"%s\" failed", file->name.data);
            return NGX_ERROR;
        }

        file->sys_offset = offset;
    }

    for ( ;; ) {
        n = write(file->fd, buf + written, size);

        if (n == -1) {
            ngx_log_error(NGX_LOG_CRIT, file->log, ngx_errno,
                          "write() \"%s\" failed", file->name.data);
            return NGX_ERROR;
        }

        file->offset += n;
        written += n;

        if ((size_t) n == size) {
            return written;
        }

        size -= n;
    }
#endif
}


ngx_fd_t
ngx_open_tempfile(u_char *name, ngx_uint_t persistent, ngx_uint_t access)
{
    ngx_fd_t  fd;

    fd = open((const char *) name, O_CREAT|O_EXCL|O_RDWR,
              access ? access : 0600);

    if (fd != -1 && !persistent) {
        /*
        unlink函数使文件引用数减一，当引用数为零时，操作系统就删除文件。但若有进程已经打开文件，则只有最后一个引用该文件的文件
        描述符关闭，该文件才会被删除。
          */
        (void) unlink((const char *) name); //如果一个文件名有unlink，则当关闭fd的时候，会删除该文件
    }

    return fd;
}


#define NGX_IOVS  8
/*
如果配置xxx_buffers  XXX_buffer_size指定的空间都用完了，则会把缓存中的数据写入临时文件，然后继续读，读到ngx_event_pipe_write_chain_to_temp_file
后写入临时文件，直到read返回NGX_AGAIN,然后在ngx_event_pipe_write_to_downstream->ngx_output_chain->ngx_output_chain_copy_buf中读取临时文件内容
发送到后端，当数据继续到来，通过epoll read继续循环该流程
*/

/*ngx_http_upstream_init_request->ngx_http_upstream_cache 客户端获取缓存 后端应答回来数据后在ngx_http_upstream_send_response->ngx_http_file_cache_create
中创建临时文件，然后在ngx_event_pipe_write_chain_to_temp_file把读取的后端数据写入临时文件，最后在
ngx_http_upstream_send_response->ngx_http_upstream_process_request->ngx_http_file_cache_update中把临时文件内容rename(相当于mv)到proxy_cache_path指定
的cache目录下面
*/

ssize_t
ngx_write_chain_to_file(ngx_file_t *file, ngx_chain_t *cl, off_t offset,
    ngx_pool_t *pool)
{
    u_char        *prev;
    size_t         size;
    ssize_t        total, n;
    ngx_array_t    vec;
    struct iovec  *iov, iovs[NGX_IOVS];

    /* use pwrite() if there is the only buf in a chain */

    if (cl->next == NULL) { //只有一个buf节点
        return ngx_write_file(file, cl->buf->pos,
                              (size_t) (cl->buf->last - cl->buf->pos),
                              offset);
    }

    total = 0; //本次总共写道文件中的字节数，和cl中所有buf指向的内存空间大小相等

    vec.elts = iovs;
    vec.size = sizeof(struct iovec);
    vec.nalloc = NGX_IOVS;
    vec.pool = pool;

    do {
        prev = NULL;
        iov = NULL;
        size = 0;

        vec.nelts = 0;

        /* create the iovec and coalesce the neighbouring bufs */

        while (cl && vec.nelts < IOV_MAX) { //把cl链中的所有每一个chain节点连接到一个iov中
            if (prev == cl->buf->pos) { //把一个chain链中的所有buf放到一个iov中
                iov->iov_len += cl->buf->last - cl->buf->pos;

            } else {
                iov = ngx_array_push(&vec);
                if (iov == NULL) {
                    return NGX_ERROR;
                }

                iov->iov_base = (void *) cl->buf->pos;
                iov->iov_len = cl->buf->last - cl->buf->pos;
            }

            size += cl->buf->last - cl->buf->pos; //cl为所有数据的长度和
            prev = cl->buf->last;
            cl = cl->next;
        } //如果cl链中的所有chain个数超过了IOV_MAX个，则需要下次继续在后面while (cl);回过来处理

        /* use pwrite() if there is the only iovec buffer */

        if (vec.nelts == 1) {
            iov = vec.elts;

            n = ngx_write_file(file, (u_char *) iov[0].iov_base,
                               iov[0].iov_len, offset);

            if (n == NGX_ERROR) {
                return n;
            }

            return total + n;
        }

        if (file->sys_offset != offset) {
            if (lseek(file->fd, offset, SEEK_SET) == -1) {
                ngx_log_error(NGX_LOG_CRIT, file->log, ngx_errno,
                              "lseek() \"%s\" failed", file->name.data);
                return NGX_ERROR;
            }

            file->sys_offset = offset;
        }

        n = writev(file->fd, vec.elts, vec.nelts);

        if (n == -1) {
            ngx_log_error(NGX_LOG_CRIT, file->log, ngx_errno,
                          "writev() \"%s\" failed", file->name.data);
            return NGX_ERROR;
        }

        if ((size_t) n != size) {
            ngx_log_error(NGX_LOG_CRIT, file->log, 0,
                          "writev() \"%s\" has written only %z of %uz",
                          file->name.data, n, size);
            return NGX_ERROR;
        }

        ngx_log_debug3(NGX_LOG_DEBUG_CORE, file->log, 0,
                       "writev to filename:%V,fd: %d, readsize: %z", &file->name, file->fd, n);

        file->sys_offset += n;
        file->offset += n;
        offset += n;
        total += n;

    } while (cl);//如果cl链中的所有chain个数超过了IOV_MAX个，则需要下次继续在后面while (cl);回过来处理

    return total;
}


ngx_int_t
ngx_set_file_time(u_char *name, ngx_fd_t fd, time_t s)
{
    struct timeval  tv[2];

    tv[0].tv_sec = ngx_time();
    tv[0].tv_usec = 0;
    tv[1].tv_sec = s;
    tv[1].tv_usec = 0;

    if (utimes((char *) name, tv) != -1) {
        return NGX_OK;
    }

    return NGX_ERROR;
}


ngx_int_t
ngx_create_file_mapping(ngx_file_mapping_t *fm)
{
    fm->fd = ngx_open_file(fm->name, NGX_FILE_RDWR, NGX_FILE_TRUNCATE,
                           NGX_FILE_DEFAULT_ACCESS);
    if (fm->fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_CRIT, fm->log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", fm->name);
        return NGX_ERROR;
    }

    if (ftruncate(fm->fd, fm->size) == -1) {
        ngx_log_error(NGX_LOG_CRIT, fm->log, ngx_errno,
                      "ftruncate() \"%s\" failed", fm->name);
        goto failed;
    }

    fm->addr = mmap(NULL, fm->size, PROT_READ|PROT_WRITE, MAP_SHARED,
                    fm->fd, 0);
    if (fm->addr != MAP_FAILED) {
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_CRIT, fm->log, ngx_errno,
                  "mmap(%uz) \"%s\" failed", fm->size, fm->name);

failed:

    if (ngx_close_file(fm->fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, fm->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", fm->name);
    }

    return NGX_ERROR;
}


void
ngx_close_file_mapping(ngx_file_mapping_t *fm)
{
    if (munmap(fm->addr, fm->size) == -1) {
        ngx_log_error(NGX_LOG_CRIT, fm->log, ngx_errno,
                      "munmap(%uz) \"%s\" failed", fm->size, fm->name);
    }

    if (ngx_close_file(fm->fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, fm->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", fm->name);
    }
}


ngx_int_t
ngx_open_dir(ngx_str_t *name, ngx_dir_t *dir)
{
    dir->dir = opendir((const char *) name->data);

    if (dir->dir == NULL) {
        return NGX_ERROR;
    }

    dir->valid_info = 0;

    return NGX_OK;
}


ngx_int_t
ngx_read_dir(ngx_dir_t *dir)
{
    dir->de = readdir(dir->dir);

    if (dir->de) {
#if (NGX_HAVE_D_TYPE)
        dir->type = dir->de->d_type;
#else
        dir->type = 0;
#endif
        return NGX_OK;
    }

    return NGX_ERROR;
}


ngx_int_t
ngx_open_glob(ngx_glob_t *gl)
{
    int  n;

    n = glob((char *) gl->pattern, 0, NULL, &gl->pglob);

    if (n == 0) {
        return NGX_OK;
    }

#ifdef GLOB_NOMATCH

    if (n == GLOB_NOMATCH && gl->test) {
        return NGX_OK;
    }

#endif

    return NGX_ERROR;
}


ngx_int_t
ngx_read_glob(ngx_glob_t *gl, ngx_str_t *name)
{
    size_t  count;

#ifdef GLOB_NOMATCH
    count = (size_t) gl->pglob.gl_pathc;
#else
    count = (size_t) gl->pglob.gl_matchc;
#endif

    if (gl->n < count) {

        name->len = (size_t) ngx_strlen(gl->pglob.gl_pathv[gl->n]);
        name->data = (u_char *) gl->pglob.gl_pathv[gl->n];
        gl->n++;

        return NGX_OK;
    }

    return NGX_DONE;
}


void
ngx_close_glob(ngx_glob_t *gl)
{
    globfree(&gl->pglob);
}











































/*
Linux fcntl函数详解(2011-07-18 20:22:14)转载▼标签： fcntllinuxcit  
功能描述：根据文件描述词来操作文件的特性。
#include <unistd.h>
#include <fcntl.h>
int fcntl(int fd, int cmd);
int fcntl(int fd, int cmd, long arg);
int fcntl(int fd, int cmd, struct flock *lock);
[描述]
fcntl()针对(文件)描述符提供控制。参数fd是被参数cmd操作(如下面的描述)的描述符。针对cmd的值，fcntl能够接受第三个参数int arg。
[返回值]
fcntl()的返回值与命令有关。如果出错，所有命令都返回－1，如果成功则返回某个其他值。下列三个命令有特定返回值：F_DUPFD , F_GETFD , F_GETFL以及F_GETOWN。
    F_DUPFD   返回新的文件描述符
    F_GETFD   返回相应标志
    F_GETFL , F_GETOWN   返回一个正的进程ID或负的进程组ID
 
fcntl函数有5种功能：
1. 复制一个现有的描述符(cmd=F_DUPFD).
2. 获得／设置文件描述符标记(cmd=F_GETFD或F_SETFD).
3. 获得／设置文件状态标记(cmd=F_GETFL或F_SETFL).
4. 获得／设置异步I/O所有权(cmd=F_GETOWN或F_SETOWN).
5. 获得／设置记录锁(cmd=F_GETLK , F_SETLK或F_SETLKW).
1. cmd值的F_DUPFD ：
F_DUPFD    返回一个如下描述的(文件)描述符：
        ·最小的大于或等于arg的一个可用的描述符
        ·与原始操作符一样的某对象的引用
        ·如果对象是文件(file)的话，则返回一个新的描述符，这个描述符与arg共享相同的偏移量(offset)
        ·相同的访问模式(读，写或读/写)
        ·相同的文件状态标志(如：两个文件描述符共享相同的状态标志)
        ·与新的文件描述符结合在一起的close-on-exec标志被设置成交叉式访问execve(2)的系统调用
实际上调用dup(oldfd)；
等效于
        fcntl(oldfd, F_DUPFD, 0);
而调用dup2(oldfd, newfd)；
等效于
        close(oldfd)；
        fcntl(oldfd, F_DUPFD, newfd)；
2. cmd值的F_GETFD和F_SETFD：     
F_GETFD    取得与文件描述符fd联合的close-on-exec标志，类似FD_CLOEXEC。如果返回值和FD_CLOEXEC进行与运算结果是0的话，文件保持交叉式访问exec()，否则如果通过exec运行的话，文件将被关闭(arg 被忽略)       
F_SETFD    设置close-on-exec标志，该标志以参数arg的FD_CLOEXEC位决定，应当了解很多现存的涉及文件描述符标志的程序并不使用常数 FD_CLOEXEC，而是将此标志设置为0(系统默认，在exec时不关闭)或1(在exec时关闭)    
在修改文件描述符标志或文件状态标志时必须谨慎，先要取得现在的标志值，然后按照希望修改它，最后设置新标志值。不能只是执行F_SETFD或F_SETFL命令，这样会关闭以前设置的标志位。 
3. cmd值的F_GETFL和F_SETFL：  
F_GETFL    取得fd的文件状态标志，如同下面的描述一样(arg被忽略)，在说明open函数时，已说明
了文件状态标志。不幸的是，三个存取方式标志 (O_RDONLY , O_WRONLY , 以及O_RDWR)并不各占1位。(这三种标志的值各是0 , 1和2，由于历史原因，这三种值互斥 — 一个文件只能有这三种值之一。) 因此首先必须用屏蔽字O_ACCMODE相与取得存取方式位，然后将结果与这三种值相比较。      
F_SETFL    设置给arg描述符状态标志，可以更改的几个标志是：O_APPEND，O_NONBLOCK，O_SYNC 和 O_ASYNC。而fcntl的文件状态标志总共有7个：O_RDONLY , O_WRONLY , O_RDWR , O_APPEND , O_NONBLOCK , O_SYNC和O_ASYNC
可更改的几个标志如下面的描述：
    O_NONBLOCK   非阻塞I/O，如果read(2)调用没有可读取的数据，或者如果write(2)操作将阻塞，则read或write调用将返回-1和EAGAIN错误
    O_APPEND     强制每次写(write)操作都添加在文件大的末尾，相当于open(2)的O_APPEND标志
    O_DIRECT     最小化或去掉reading和writing的缓存影响。系统将企图避免缓存你的读或写的数据。如果不能够避免缓存，那么它将最小化已经被缓存了的数据造成的影响。如果这个标志用的不够好，将大大的降低性能
    O_ASYNC      当I/O可用的时候，允许SIGIO信号发送到进程组，例如：当有数据可以读的时候
4. cmd值的F_GETOWN和F_SETOWN：  
F_GETOWN   取得当前正在接收SIGIO或者SIGURG信号的进程id或进程组id，进程组id返回的是负值(arg被忽略)    
F_SETOWN   设置将接收SIGIO和SIGURG信号的进程id或进程组id，进程组id通过提供负值的arg来说明(arg绝对值的一个进程组ID)，否则arg将被认为是进程id
 5. cmd值的F_GETLK, F_SETLK或F_SETLKW： 获得／设置记录锁的功能，成功则返回0，若有错误则返回-1，错误原因存于errno。
F_GETLK    通过第三个参数arg(一个指向flock的结构体)取得第一个阻塞lock description指向的锁。取得的信息将覆盖传到fcntl()的flock结构的信息。如果没有发现能够阻止本次锁(flock)生成的锁，这个结构将不被改变，除非锁的类型被设置成F_UNLCK   
F_SETLK    按照指向结构体flock的指针的第三个参数arg所描述的锁的信息设置或者清除一个文件的segment锁。F_SETLK被用来实现共享(或读)锁(F_RDLCK)或独占(写)锁(F_WRLCK)，同样可以去掉这两种锁(F_UNLCK)。如果共享锁或独占锁不能被设置，fcntl()将立即返回EAGAIN    
F_SETLKW   除了共享锁或独占锁被其他的锁阻塞这种情况外，这个命令和F_SETLK是一样的。如果共享锁或独占锁被其他的锁阻塞，进程将等待直到这个请求能够完成。当fcntl()正在等待文件的某个区域的时候捕捉到一个信号，如果这个信号没有被指定SA_RESTART, fcntl将被中断
当一个共享锁被set到一个文件的某段的时候，其他的进程可以set共享锁到这个段或这个段的一部分。共享锁阻止任何其他进程set独占锁到这段保护区域的任何部分。如果文件描述符没有以读的访问方式打开的话，共享锁的设置请求会失败。
独占锁阻止任何其他的进程在这段保护区域任何位置设置共享锁或独占锁。如果文件描述符不是以写的访问方式打开的话，独占锁的请求会失败。
结构体flock的指针：
struct flcok
{
short int l_type;
//以下的三个参数用于分段对文件加锁，若对整个文件加锁，则：l_whence=SEEK_SET, l_start=0, l_len=0
short int l_whence;
off_t l_start;
off_t l_len;
pid_t l_pid;
};
l_type 有三种状态：
F_RDLCK   建立一个供读取用的锁定
F_WRLCK   建立一个供写入用的锁定
F_UNLCK   删除之前建立的锁定
l_whence 也有三种方式：
SEEK_SET   以文件开头为锁定的起始位置
SEEK_CUR   以目前文件读写位置为锁定的起始位置
SEEK_END   以文件结尾为锁定的起始位置
fcntl文件锁有两种类型：建议性锁和强制性锁
建议性锁是这样规定的：每个使用上锁文件的进程都要检查是否有锁存在，当然还得尊重已有的锁。内核和系统总体上都坚持不使用建议性锁，它们依靠程序员遵守这个规定。
强制性锁是由内核执行的：当文件被上锁来进行写入操作时，在锁定该文件的进程释放该锁之前，内核会阻止任何对该文件的读或写访问，每次读或写访问都得检查锁是否存在。
系统默认fcntl都是建议性锁，强制性锁是非POSIX标准的。如果要使用强制性锁，要使整个系统可以使用强制性锁，那么得需要重新挂载文件系统，mount使用参数 -0 mand 打开强制性锁，或者关闭已加锁文件的组执行权限并且打开该文件的set-GID权限位。
建议性锁只在cooperating processes之间才有用。对cooperating process的理解是最重要的，它指的是会影响其它进程的进程或被别的进程所影响的进程，举两个例子：
(1) 我们可以同时在两个窗口中运行同一个命令，对同一个文件进行操作，那么这两个进程就是cooperating  processes
(2) cat file | sort，那么cat和sort产生的进程就是使用了pipe的cooperating processes
使用fcntl文件锁进行I/O操作必须小心：进程在开始任何I/O操作前如何去处理锁，在对文件解锁前如何完成所有的操作，是必须考虑的。如果在设置锁之前打开文件，或者读取该锁之后关闭文件，另一个进程就可能在上锁/解锁操作和打开/关闭操作之间的几分之一秒内访问该文件。当一个进程对文件加锁后，无论它是否释放所加的锁，只要文件关闭，内核都会自动释放加在文件上的建议性锁(这也是建议性锁和强制性锁的最大区别)，所以不要想设置建议性锁来达到永久不让别的进程访问文件的目的(强制性锁才可以)；强制性锁则对所有进程起作用。
fcntl使用三个参数 F_SETLK/F_SETLKW， F_UNLCK和F_GETLK 来分别要求、释放、测试record locks。record locks是对文件一部分而不是整个文件的锁，这种细致的控制使得进程更好地协作以共享文件资源。fcntl能够用于读取锁和写入锁，read lock也叫shared lock(共享锁)， 因为多个cooperating process能够在文件的同一部分建立读取锁；write lock被称为exclusive lock(排斥锁)，因为任何时刻只能有一个cooperating process在文件的某部分上建立写入锁。如果cooperating processes对文件进行操作，那么它们可以同时对文件加read lock，在一个cooperating process加write lock之前，必须释放别的cooperating process加在该文件的read lock和wrtie lock，也就是说，对于文件只能有一个write lock存在，read lock和wrtie lock不能共存。
下面的例子使用F_GETFL获取fd的文件状态标志。
#include<fcntl.h>
#include<unistd.h>
#include<iostream>
#include<errno.h>
using namespace std;
int main(int argc,char* argv[])
{
  int fd, var;
  //  fd=open("new",O_RDWR);
  if (argc!=2)
  {
      perror("--");
      cout<<"请输入参数，即文件名！"<<endl;
  }
  if((var=fcntl(atoi(argv[1]), F_GETFL, 0))<0)
  {
     strerror(errno);
     cout<<"fcntl file error."<<endl;
  }
  switch(var & O_ACCMODE)
  {
   case O_RDONLY : cout<<"Read only.."<<endl;
                   break;
   case O_WRONLY : cout<<"Write only.."<<endl;
                   break;
   case O_RDWR   : cout<<"Read wirte.."<<endl;
                   break;
   default  : break;
  }
 if (val & O_APPEND)
    cout<<",append"<<endl;
 if (val & O_NONBLOCK)
    cout<<",noblocking"<<endl;
 cout<<"exit 0"<<endl;
 exit(0);
}
Linux fcntl函数详解 .
分类： fcntl 2013-12-07 16:43 183人阅读 评论(0) 收藏 举报 
功能描述：根据文件描述词来操作文件的特性。
文件控制函数          fcntl -- file control
头文件：
#include <unistd.h>
#include <fcntl.h>
函数原型：          
int fcntl(int fd, int cmd);
int fcntl(int fd, int cmd, long arg);         
int fcntl(int fd, int cmd, struct flock *lock);
描述：
           fcntl()针对(文件)描述符提供控制.参数fd是被参数cmd操作(如下面的描述)的描述符.            
　　　　针对cmd的值,fcntl能够接受第三个参数（arg）
fcntl函数有5种功能：
　　　　 1.复制一个现有的描述符（cmd=F_DUPFD）.
　　       2.获得／设置文件描述符标记(cmd=F_GETFD或F_SETFD).
            3.获得／设置文件状态标记(cmd=F_GETFL或F_SETFL).
            4.获得／设置异步I/O所有权(cmd=F_GETOWN或F_SETOWN).
            5.获得／设置记录锁(cmd=F_GETLK,F_SETLK或F_SETLKW).
 
 cmd 选项：
            F_DUPFD      返回一个如下描述的(文件)描述符:                            
         　　　　　　　　　　（1）最小的大于或等于arg的一个可用的描述符                          
   　　　　　　　　　　　　（2）与原始操作符一样的某对象的引用               
            　　　　　　　　  （3）如果对象是文件(file)的话,返回一个新的描述符,这个描述符与arg共享相同的偏移量(offset)                    
　　　　　　　　　　　　   （4）相同的访问模式(读,写或读/写)                          
　　　　　　　　　　　　　（5）相同的文件状态标志(如:两个文件描述符共享相同的状态标志)                            
　　　　　　　　　　　　　（6）与新的文件描述符结合在一起的close-on-exec标志被设置成交叉式访问execve(2)的系统调用                     
             F_GETFD     取得与文件描述符fd联合close-on-exec标志,类似FD_CLOEXEC.如果返回值和FD_CLOEXEC进行与运算结果是0的话,文件保持交叉式访问exec(),　　　　　　                      否则如果通过exec运行的话,文件将被关闭(arg被忽略)                  
             F_SETFD     设置close-on-exec旗标。该旗标以参数arg的FD_CLOEXEC位决定。                   
             F_GETFL     取得fd的文件状态标志,如同下面的描述一样(arg被忽略)                    
             F_SETFL     设置给arg描述符状态标志,可以更改的几个标志是：O_APPEND， O_NONBLOCK，O_SYNC和O_ASYNC。
             F_GETOWN 取得当前正在接收SIGIO或者SIGURG信号的进程id或进程组id,进程组id返回成负值(arg被忽略)                    
             F_SETOWN 设置将接收SIGIO和SIGURG信号的进程id或进程组id,进程组id通过提供负值的arg来说明,否则,arg将被认为是进程id
              
命令字(cmd)F_GETFL和F_SETFL的标志如下面的描述:            
             O_NONBLOCK        非阻塞I/O;如果read(2)调用没有可读取的数据,或者如果write(2)操作将阻塞,read或write调用返回-1和EAGAIN错误               　　　　       　　O_APPEND             强制每次写(write)操作都添加在文件大的末尾,相当于open(2)的O_APPEND标志         
             O_DIRECT             最小化或去掉reading和writing的缓存影响.系统将企图避免缓存你的读或写的数据.
                             如果不能够避免缓存,那么它将最小化已经被缓存了的数 据造成的影响.如果这个标志用的不够好,将大大的降低性能                      
             O_ASYNC              当I/O可用的时候,允许SIGIO信号发送到进程组,例如:当有数据可以读的时候
 注意：      在修改文件描述符标志或文件状态标志时必须谨慎，先要取得现在的标志值，然后按照希望修改它，最后设置新标志值。不能只是执行F_SETFD或F_SETFL命令，这样会关闭以前设置的标志位。
fcntl的返回值：  与命令有关。如果出错，所有命令都返回－1，如果成功则返回某个其他值。下列三个命令有特定返回值：F_DUPFD,F_GETFD,F_GETFL以及F_GETOWN。第一个返回新的文件描述符，第二个返回相应标志，最后一个返回一个正的进程ID或负的进程组ID。
 
一：第一种类似于dup操作，在这里不做举例。（fcnlt(oldfd, F_DUPFD, 0) <==>dup2(oldfd, newfd)）
二：设置close-on-exec旗标
在此函数中创建子进程，调用execl
 1 #include <stdio.h>
 2 #include <stdlib.h>
 3 #include <string.h>
 4 
 5 int main()
 6 {
 7     pid_t pid;
 8     //以追加的形式打开文件
 9     int fd = fd = open("test.txt", O_TRUNC | O_RDWR | O_APPEND | O_CREAT, 0777);
10     if(fd < 0)
11     {
12         perror("open");
13         return -1;
14     }
15     printf("fd = %d\n", fd);
16     
17     fcntl(fd, F_SETFD, 0);//关闭fd的close-on-exec标志
18 
19     write(fd, "hello c program\n", strlen("hello c program!\n"));
20 
21     pid = fork();
22     if(pid < 0)
23     {
24             perror("fork");
25             return -1;
26     }
27     if(pid == 0)
28     {
29         printf("fd = %d\n", fd);
30         
31         int ret = execl("./main", "./main", (char *)&fd, NULL);
32         if(ret < 0)
33         {
34             perror("execl");
35             exit(-1);
36         }
37         exit(0);
38     }
39 
40     wait(NULL);
41 
42     write(fd, "hello c++ program!\n", strlen("hello c++ program!\n"));
43 
44     close(fd);
45 
46     return 0;
47 }main测试函数
 1 int main(int argc, char *argv[])
 2 {
 3     int fd = (int)(*argv[1]);//描述符
 4     
 5     printf("fd = %d\n", fd);
 6 
 7     int ret = write(fd, "hello linux\n", strlen("hello linux\n"));
 8     if(ret < 0)
 9     {
10         perror("write");
11         return -1;
12     }
13 
14     close(fd);
15 
16     return 0;
17 }执行后文件结果：
[root@centOS5 class_2]# cat test.txt 
hello c program
hello linux
hello c++ program!
 
三：用命令F_GETFL和F_SETFL设置文件标志，比如阻塞与非阻塞
 1 #include <stdio.h>
 2 #include <sys/types.h>
 3 #include <unistd.h>
 4 #include <sys/stat.h>
 5 #include <fcntl.h>
 6 #include <string.h>
 7 
 8 / **********************使能非阻塞I/O********************
 9 *int flags;
10 *if(flags = fcntl(fd, F_GETFL, 0) < 0)
11 *{
12 *    perror("fcntl");
13 *    return -1;
14 *}
15 *flags |= O_NONBLOCK;
16 *if(fcntl(fd, F_SETFL, flags) < 0)
17 *{
18 *    perror("fcntl");
19 *    return -1;
20 *}
21 ******************************************************* /
22 
23 / **********************关闭非阻塞I/O******************
24 flags &= ~O_NONBLOCK;
25 if(fcntl(fd, F_SETFL, flags) < 0)
26 {
27     perror("fcntl");
28     return -1;
29 }
30 ******************************************************* /
31 
32 int main()
33 {
34     char buf[10] = {0};
35     int ret;
36     int flags;
37     
38     //使用非阻塞io
39     if(flags = fcntl(STDIN_FILENO, F_GETFL, 0) < 0)
40     {
41         perror("fcntl");
42         return -1;
43     }
44     flags |= O_NONBLOCK;
45     if(fcntl(STDIN_FILENO, F_SETFL, flags) < 0)
46     {
47         perror("fcntl");
48         return -1;
49     }
50 
51     while(1)
52     {
53         sleep(2);
54         ret = read(STDIN_FILENO, buf, 9);
55         if(ret == 0)
56         {
57             perror("read--no");
58         }
59         else
60         {
61             printf("read = %d\n", ret);
62         }
63         
64         write(STDOUT_FILENO, buf, 10);
65         memset(buf, 0, 10);
66     }
67 
68     return 0;
69 }四：设置异步IO还没想好以后实现（呵呵呵。。。。。）
五：设置获取记录锁
结构体flock的指针：
struct flcok
{
　　 short int l_type;  锁定的状态
　　　　//这三个参数用于分段对文件加锁，若对整个文件加锁，则：l_whence=SEEK_SET,l_start=0,l_len=0;
　　 short int l_whence;决定l_start位置* /
　　 off_t l_start; / * 锁定区域的开头位置 * /
　　 off_t l_len; / *锁定区域的大小* /
　　 pid_t l_pid; / *锁定动作的进程* /
};
l_type 有三种状态:
　　 F_RDLCK 建立一个供读取用的锁定
　　 F_WRLCK 建立一个供写入用的锁定
       F_UNLCK 删除之前建立的锁定
l_whence 也有三种方式:
　　SEEK_SET 以文件开头为锁定的起始位置。
     SEEK_CUR 以目前文件读写位置为锁定的起始位置
     SEEK_END 以文件结尾为锁定的起始位置。
 
 
 1 #include "filelock.h"
 2 
 3 / * 设置一把读锁  * /
 4 int readLock(int fd, short start, short whence, short len) 
 5 {
 6     struct flock lock;
 7     lock.l_type = F_RDLCK;
 8     lock.l_start = start;
 9     lock.l_whence = whence;//SEEK_CUR,SEEK_SET,SEEK_END
10     lock.l_len = len;
11     lock.l_pid = getpid();
12 //  阻塞方式加锁
13     if(fcntl(fd, F_SETLKW, &lock) == 0)
14         return 1;
15     
16     return 0;
17 }
18 
19 / * 设置一把读锁 , 不等待 * /
20 int readLocknw(int fd, short start, short whence, short len) 
21 {
22     struct flock lock;
23     lock.l_type = F_RDLCK;
24     lock.l_start = start;
25     lock.l_whence = whence;//SEEK_CUR,SEEK_SET,SEEK_END
26     lock.l_len = len;
27     lock.l_pid = getpid();
28 //  非阻塞方式加锁
29     if(fcntl(fd, F_SETLK, &lock) == 0)
30         return 1;
31     
32     return 0;
33 }
34 / * 设置一把写锁 * /
35 int writeLock(int fd, short start, short whence, short len) 
36 {
37     struct flock lock;
38     lock.l_type = F_WRLCK;
39     lock.l_start = start;
40     lock.l_whence = whence;
41     lock.l_len = len;
42     lock.l_pid = getpid();
43 
44     //阻塞方式加锁
45     if(fcntl(fd, F_SETLKW, &lock) == 0)
46         return 1;
47     
48     return 0;
49 }
50 
51 / * 设置一把写锁  * /
52 int writeLocknw(int fd, short start, short whence, short len) 
53 {
54     struct flock lock;
55     lock.l_type = F_WRLCK;
56     lock.l_start = start;
57     lock.l_whence = whence;
58     lock.l_len = len;
59     lock.l_pid = getpid();
60 
61     //非阻塞方式加锁
62     if(fcntl(fd, F_SETLK, &lock) == 0)
63         return 1;
64     
65     return 0;
66 }
67 
68 / * 解锁 * /
69 int unlock(int fd, short start, short whence, short len) 
70 {
71     struct flock lock;
72     lock.l_type = F_UNLCK;
73     lock.l_start = start;
74     lock.l_whence = whence;
75     lock.l_len = len;
76     lock.l_pid = getpid();
77 
78     if(fcntl(fd, F_SETLKW, &lock) == 0)
79         return 1;
80 
81     return 0;
82 }
*/


/*
参数fd必须是已经成功抒开的文件句柄。实际上，nginx.conf文件中的lock_file配置项指定的文件路径，就是用于文件互斥锁的，
这个文件被打开后得到的句柄，将会作为fd参数传递给fcntl方法，提供一种锁机制。
struct flcok
{
　　 short int l_type;  锁定的状态 //这三个参数用于分段对文件加锁，若对整个文件加锁，则：l_whence=SEEK_SET,l_start=0,l_len=0;
　　 short int l_whence;决定l_start位置* /  锁区域起始地址的相对位置
　　 off_t l_start; / * 锁定区域的开头位置 * /  锁区域起始地址偏移量，同1_whence共同确定锁区域
　　 off_t l_len; / *锁定区域的大小* /  锁的长度，O表示锁至文件末
　　 pid_t l_pid; / *锁定动作的进程* /  拥有锁的进程ID
};
这里的cmd参数在Nginx中只会有两个值：F—SETLK和F—SETLKW，它们都表示试图获得互斥锁，但使用F—SETLK时如果互斥锁已经被其他进程占用，
fcntl方法不会等待其他进程释放锁且自己拿到锁后才返回，而是立即返回获取互斥锁失败；使用F—SETLKW时则不同，锁被占用后fcntl方法会一直
等待，在其他进程没有释放锁时，当前进程就会阻塞在fcntl方法中，这种阻塞会导致当前进程由可执行状态转为睡眠状态。
 从flock结构体中可以看出，文件锁的功能绝不仅仅局限于普通的互斥锁，它还可以锁住文件中的部分内容。但Nginx封装的文件锁仅用于保护代
码段的顺序执行（例如，在进行负载均衡时，使用互斥锁保证同一时刻仅有一个worker进程可以处理新的TCP连接），使用方式要简单得多：一个
lock_file文件对应一个全局互斥锁，而且它对master进程或者worker进程都生效。因此，对于Lstart、l_len、l_pid，都填为0，而1_whence则填为
SEEK_SET，只需要这个文件提供一个锁。l_type的值则取决于用户是想实现阻塞睡眠锁还是想实现非阻塞不会睡眠的锁。
*/


//当关闭fd句柄对应的文件时，当前进程将自动释放已经拿到的锁。

/*
对于文件锁，Nginx封装了3个方法：ngx_trylock_fd实现了不会阻塞进程、不会便得进程进入睡眠状态的互斥锁；ngx_lock_fd提供的互斥锁在锁
已经被其他进程拿到时将会导致当前进程进入睡眠状态，直到顺利拿到这个锁后，当前进程才会被Linux内核重新调度，所以它是阻塞操作；
ngx_unlock fd用于释放互斥锁。
*/
ngx_err_t
ngx_trylock_fd(ngx_fd_t fd)
{
    struct flock  fl;

    ngx_memzero(&fl, sizeof(struct flock)); //这个文件锁并不用于锁文件中的内容，填充为0
    fl.l_type = F_WRLCK; //F_WRLCK意味着不会导致进程睡眠
    fl.l_whence = SEEK_SET; //

    //获取fd对应的互斥锁，如果返回
    /*
    使用ngx_trylock_fd方法获取互斥锁成功时会返回0，否则返回的其实是errno错误码，而这个错误码为NGX- EAGAIN或者NGX EACCESS时
    表示当前没有拿到互斥锁，否则可以认为fcntl执行错误。
     */
    if (fcntl(fd, F_SETLK, &fl) == -1) {
        return ngx_errno;
    }

    return 0;
}

/*
ngx_lock_fd方法将会阻塞进程的执行，使用时需要非常谨慎，它可能会导致worker进程宁可睡眠也不处理其他正常请求
*/
ngx_err_t
ngx_lock_fd(ngx_fd_t fd)
{
    struct flock  fl;

    ngx_memzero(&fl, sizeof(struct flock));

    //F_WRLCK会导致进程睡眠
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;

    //如果返回-1，则表示fcntl执行错误。一旦返回0，表示成功地拿到了锁
    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        return ngx_errno;
    }

    return 0;
}

/*
ngx_unlock_fd方法用于释放当前进程已经拿到的互斥锁
*/ //当关闭fd句柄对应的文件时，当前进程将自动释放已经拿到的锁。
ngx_err_t
ngx_unlock_fd(ngx_fd_t fd)
{
    struct flock  fl;

    ngx_memzero(&fl, sizeof(struct flock));
    fl.l_type = F_UNLCK;//F_UNLCK表示将要释放锁
    fl.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLK, &fl) == -1) {
        return  ngx_errno;
    }

    return 0;
}


#if (NGX_HAVE_POSIX_FADVISE) && !(NGX_HAVE_F_READAHEAD)

ngx_int_t
ngx_read_ahead(ngx_fd_t fd, size_t n)
{
    int  err;

    err = posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

    if (err == 0) {
        return 0;
    }

    ngx_set_errno(err);
    return NGX_FILE_ERROR;
}

#endif


#if (NGX_HAVE_O_DIRECT)
/*
文件异步IO
    事件驱动模块都是在处理网络事件，而没有涉及磁盘上文件的操作。本
节将讨论Linux内核2.6.2x之后版本中支持的文件异步I/O，以及ngx_epoll_module模块是
如何与文件异步I/O配合提供服务的。这里提到的文件异步I/O并不是glibc库提供的文件异
步I/O。glibc库提供的异步I/O是基于多线程实现的，它不是真正意义上的异步I/O。而本节
说明的异步I/O是由Linux内核实现，只有在内核中成功地完成了磁盘操作，内核才会通知
进程，进而使得磁盘文件的处理与网络事件的处理同样高效。
    使用这种方式的前提是Linux内核版本中必须支持文件异步I/O。当然，它带来的好处
也非常明显，Nginx把读取文件的操作异步地提交给内核后，内核会通知I/O设备独立地执
行操作，这样，Nginx进程可以继续充分地占用CPU。而且，当大量读事件堆积到I/O设备
的队列中时，将会发挥出内核中“电梯算法”的优势，从而降低随机读取磁盘扇区的成本。
    注意Linux内核级别的文件异步I/O是不支持缓存操作的，也就是说，即使需要操作
的文件块在Linux文件缓存中存在，也不会通过读取、更改缓存中的文件块来代替实际对磁
盘的操作，虽然从阻塞worker进程的角度上来说有了很大好转，但是对单个请求来说，还是
有可能降低实际处理的速度，因为原先可以从内存中快速获取的文件块在使用了异步I/O后
则一定会从磁盘上读取。异步文件I/O是把“双刃剑”，关键要看使用场景，如果大部分用户
请求对文件的操作都会落到文件缓存中，那么不要使用异步I/O，反之则可以试着使用文件
异步I/O，看一下是否会为服务带来并发能力上的提升。
    目前，Nginx仅支持在读取文件时使用异步I/O，因为正常写入文件时往往是写入内存
中就立刻返回，效率很高，而使用异步I/O写入时速度会明显下降。
文件异步AIO优点:
        异步I/O是由Linux内核实现，只有在内核中成功地完成了磁盘操作，内核才会通知
    进程，进而使得磁盘文件的处理与网络事件的处理同样高效。这样就不会阻塞worker进程。
缺点:
        不支持缓存操作的，也就是说，即使需要操作的文件块在Linux文件缓存中存在，也不会通过读取、
    更改缓存中的文件块来代替实际对磁盘的操作。有可能降低实际处理的速度，因为原先可以从内存中快速
    获取的文件块在使用了异步I/O后则一定会从磁盘上读取
究竟是选择异步I/O还是普通I/O操作呢?
        异步文件I/O是把“双刃剑”，关键要看使用场景，如果大部分用户
    请求对文件的操作都会落到文件缓存中，那么不要使用异步I/O，反之则可以试着使用文件
    异步I/O，看一下是否会为服务带来并发能力上的提升。
    目前，Nginx仅支持在读取文件时使用异步I/O，因为正常写入文件时往往是写入内存
中就立刻返回，效率很高，而使用异步I/O写入时速度会明显下降。异步I/O不支持写操作，因为
异步I/O无法利用缓存，而写操作通常是落到缓存上，linux会自动将文件中缓存中的数据写到磁盘
普通文件读写过程:
正常的系统调用read/write的流程是怎样的呢？
- 读取：内核缓存有需要的文件数据:内核缓冲区->用户缓冲区;没有:硬盘->内核缓冲区->用户缓冲区;
- 写回：数据会从用户地址空间拷贝到操作系统内核地址空间的页缓存中去，这是write就会直接返回，操作系统会在恰当的时机写入磁盘，这就是传说中的
*/

//direct AIO可以参考http://blog.csdn.net/bengda/article/details/21871413
ngx_int_t
ngx_directio_on(ngx_fd_t fd)
{
    int  flags;

    flags = fcntl(fd, F_GETFL);

    if (flags == -1) {
        return NGX_FILE_ERROR;
    }

    /* 
    普通缓存I/O优点: 缓存 I/O 使用了操作系统内核缓冲区，在一定程度上分离了应用程序空间和实际的物理设备。缓存 I/O 可以减少读盘的次数，从而提高性能。
    缺点：在缓存 I/O 机制中，DMA 方式可以将数据直接从磁盘读到页缓存中，或者将数据从页缓存直接写回到磁盘上，而不能直接在应用程序地址空间和磁盘之间
        进行数据传输，这样的话，数据在传输过程中需要在应用程序地址空间和页缓存之间进行多次数据拷贝操作，这些数据拷贝操作所带来的 CPU 以及内存开销是非常大的。
    direct I/O优点:直接 I/O 最主要的优点就是通过减少操作系统内核缓冲区和应用程序地址空间的数据拷贝次数，降低了对文件读取和写入时所带来的 CPU 
        的使用以及内存带宽的占用。这对于某些特殊的应用程序，比如自缓存应用程序来说，不失为一种好的选择。如果要传输的数据量很大，使用直接 I/O 
        的方式进行数据传输，而不需要操作系统内核地址空间拷贝数据操作的参与，这将会大大提高性能。
    direct I/O缺点: 设置直接 I/O 的开销非常大，而直接 I/O 又不能提供缓存 I/O 的优势。缓存 I/O 的读操作可以从高速缓冲存储器中获取数据，而直接 
        I/O 的读数据操作会造成磁盘的同步读，这会带来性能上的差异 , 并且导致进程需要较长的时间才能执行完
    总结:
    Linux 中的直接 I/O 访问文件方式可以减少 CPU 的使用率以及内存带宽的占用，但是直接 I/O 有时候也会对性能产生负面影响。所以在使用
    直接 I/O 之前一定要对应用程序有一个很清醒的认识，只有在确定了设置缓冲 I/O 的开销非常巨大的情况下，才考虑使用直接 I/O。直接 I/O 
    经常需要跟异步 I/O 结合起来使用
    
    普通缓存I/O: 硬盘->内核缓冲区->用户缓冲区 写数据写道缓冲区中就返回，一般由内核定期写道磁盘(或者直接调用API指定要写入磁盘)，
    读操作首先检查缓冲区是否有所需的文件内容，没有就冲磁盘读到内核缓冲区，在从内核缓冲区到用户缓冲区
    O_DIRECT为直接I/O方式，硬盘->用户缓冲区，少了内核缓冲区操作，但是直接磁盘操作很费时,所以直接I/O一般借助AIO和EPOLL实现
    参考:http://blog.csdn.net/bengda/article/details/21871413  http://www.ibm.com/developerworks/cn/linux/l-cn-directio/index.html
    */
    return fcntl(fd, F_SETFL, flags | O_DIRECT);
}


ngx_int_t
ngx_directio_off(ngx_fd_t fd)
{
    int  flags;

    flags = fcntl(fd, F_GETFL);

    if (flags == -1) {
        return NGX_FILE_ERROR;
    }

    return fcntl(fd, F_SETFL, flags & ~O_DIRECT);
}

#endif


#if (NGX_HAVE_STATFS)

/*
功能描述：   
查询文件系统相关的信息。 
    
用法：   
#include <sys/vfs.h>    / * 或者 <sys/statfs.h> * / 
int statfs(const char *path, struct statfs *buf); 
int fstatfs(int fd, struct statfs *buf); 
  
  参数：   
path: 位于需要查询信息的文件系统的文件路径名。     
fd： 位于需要查询信息的文件系统的文件描述词。 
buf：以下结构体的指针变量，用于储存文件系统相关的信息 
struct statfs { 
    long    f_type;     / * 文件系统类型  * / 
   long    f_bsize;    / * 经过优化的传输块大小  * / 
   long    f_blocks;   / * 文件系统数据块总数 * / 
   long    f_bfree;    / * 可用块数 * / 
     long    f_bavail;   / * 非超级用户可获取的块数 * / 
   long    f_files;    / * 文件结点总数 * / 
   long    f_ffree;    / * 可用文件结点数 * / 
   fsid_t  f_fsid;     / * 文件系统标识 * / 
   long    f_namelen;  / * 文件名的最大长度 * / 
}; 
 
返回说明：   
成功执行时，返回0。失败返回-1，errno被设为以下的某个值   
  
EACCES： (statfs())文件或路径名中包含的目录不可访问 
EBADF ： (fstatfs()) 文件描述词无效 
EFAULT： 内存地址无效 
EINTR ： 操作由信号中断 
EIO    ： 读写出错 
ELOOP ： (statfs())解释路径名过程中存在太多的符号连接 
ENAMETOOLONG：(statfs()) 路径名太长 
ENOENT：(statfs()) 文件不存在 
ENOMEM： 核心内存不足 
ENOSYS： 文件系统不支持调用 
ENOTDIR：(statfs())路径名中当作目录的组件并非目录 
EOVERFLOW：信息溢出
 
一个简单的例子：
#include <sys/vfs.h>
#include <stdio.h>
int main()
{
    struct statfs diskInfo;
    statfs("/",&diskInfo);
    unsigned long long blocksize = diskInfo.f_bsize;// 每个block里面包含的字节数
    unsigned long long totalsize = blocksize * diskInfo.f_blocks;//总的字节数
    printf("TOTAL_SIZE == %lu MB/n",totalsize>>20); // 1024*1024 =1MB  换算成MB单位
    unsigned long long freeDisk = diskInfo.f_bfree*blocksize; //再计算下剩余的空间大小
    printf("DISK_FREE == %ld MB/n",freeDisk>>20);
 return 0;
}
*/

//获取文件系统的block size  
size_t
ngx_fs_bsize(u_char *name)
{
    struct statfs  fs;

    if (statfs((char *) name, &fs) == -1) {
        return 512;
    }

    if ((fs.f_bsize % 512) != 0) {
        return 512;
    }

    return (size_t) fs.f_bsize; // 每个block里面包含的字节数
}

#elif (NGX_HAVE_STATVFS)

size_t
ngx_fs_bsize(u_char *name)
{
    struct statvfs  fs;

    if (statvfs((char *) name, &fs) == -1) {
        return 512;
    }

    if ((fs.f_frsize % 512) != 0) {
        return 512;
    }

    return (size_t) fs.f_frsize;
}

#else

size_t
ngx_fs_bsize(u_char *name)
{
    return 512;
}

#endif

