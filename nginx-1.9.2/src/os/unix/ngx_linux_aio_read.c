
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>

/*
 Linux内核提供了5个系统调用完成文件操作的异步I/O功能，见表9-7。
表9-7  Linux内核提供的文件异步1/0操作方法
┏━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┓
┃    方法名                              ┃    参数含义                        ┃    执行意义                  ┃
┣━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━┫
┃                                        ┃  nr events表示需要初始化的异步     ┃  初始化文件异步I/O的上下文， ┃
┃int io_setup(unsigned nr_events,        ┃1/0上下文可以处理的事件的最小个     ┃执行成功后ctxp就是分配的上下  ┃
┃aio context_t *ctxp)                    ┃数，ctxp是文件异步1/0的上下文描述   ┃文描述符，这个异步1/0上下文   ┃
┃                                        ┃                                    ┃将至少可以处理nr- events个事  ┃
┃                                        ┃符指针                              ┃                              ┃
┃                                        ┃                                    ┃件。返回0表示成功             ┃
┣━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━┫
┃                                        ┃                                    ┃  销毁文件异步1/0的上下文。   ┃
┃int io_destroy (aio_context_t ctx)      ┃  ctx是文件异步1/0的上下文描述符    ┃                              ┃
┃                                        ┃                                    ┃返回0表示成功                 ┃
┣━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━┫
┃int io_submit(aiio_context_t ctx,       ┃  ctx是文件异步1/0的上下文描述符，  ┃  提交文件异步1/0操作。返回   ┃
┃                                        ┃nr是一次提交的事件个数，cbp是需要   ┃                              ┃
┃long nr, struct iocb *cbp[])            ┃                                    ┃值表示成功提交的事件个数      ┃
┃                                        ┃提交的事件数组中的酋个元素地址      ┃                              ┃
┣━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━┫
┃int io_cancel(aio_context_t ctx, struct ┃  ctx是文件异步1/0的上下文描述      ┃  取消之前使用io—sumbit提交  ┃
┃                                        ┃符，iocb是要取消的异步1/0操作，而   ┃的一个文件异步I/O操作。返回   ┃
┃iocb *iocb, struct io_event *result)    ┃                                    ┃                              ┃
┃                                        ┃result表示这个操作的执行结果        ┃O表示成功                     ┃
┣━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━┫
┃                I                       ┃  ctx是文件异步1/0的上下文描述      ┃                              ┃
┃int io_getevents(aio_context_t ctx,     ┃符；min_nr表示至少要获取mln_ nr个   ┃                              ┃
┃long min_nr, lon, g nr,                 ┃事件；而nr表示至多获取nr个事件，    ┃  从已经完成的文件异步I/O操   ┃
┃struct io  event "*events, struct       ┃它与events数组的个数一般是相同的：  ┃作队列中读取操作              ┃
┃timespec *timeout)                      ┃events是执行完成的事件数组；tlmeout ┃                              ┃
┃         I "                            ┃是超时时间，也就是在获取mm- nr个    ┃                              ┃
┃                                        ┃事件前的等待时间                    ┃                              ┃
┗━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━┛
    表9-7中列举的这5种方法提供了内核级别的文件异步I/O机制，使用前需要先调用io_setup方法初始化异步I/O上下文。虽然一个进程可以拥有
多个异步I/O上下文，但通常有一个就足够了。调用io_setup方法后会获得这个异步I/O上下文的描述符（aio—context_t类
型），这个描述符和epoll_create返回的描述符一样，是贯穿始终的。注意，nr_ events只是
指定了异步I/O至少初始化的上下文容量，它并没有限制最大可以处理的异步I/O事件数目。
为了便于理解，不妨将io_setup与epoll_create进行对比，它们还是很相似的。
    既然把epoll和异步I/O进行对比，那么哪些调用相当于epoll_ctrl呢？就是io—submit
和io cancel。其中io- submit相当于向异步I/O中添加事件，而io- cancel则相当于从异步I/O
中移除事件。io—submit中用到了一个结构体iocb，下面简单地看一下它的定义。
    struct iocb  f
    ／t存储着业务需要的指针。例如，在Nginx中，这个字段通常存储着对应的ngx_event_t亭件的指
针。它实际上与io_getevents方法中返回的io event结构体的data成员是完全一致的+／
    u int64 t aio data;
7 7不需要设置
u  int32七PADDED (aio_key,  aio_raservedl)j
／／操作码，其取值范围是io iocb cmd t中的枚举命令
u int16 t aio lio_opcode;
／／请求的优先级
int16 t aio_reqprio,
，／i41-描述符
u int32 t aio fildes;
／／读／写操作对应的用户态缓冲区
u int64 t aio buf;
／／读／写操作的字节长度
u int64 t aio_nbytes;
／／读／写操作对应于文件中的偏移量
int64 t aio offset;
7 7保售手段
u int64七aio reserved2;
    ／+表示可以设置为IOCB FLAG RESFD，它会告诉内核当有异步I/O请求处理完成时使用eventfd进
行通知，可与epoll配合使用+／
    u int32七aio_flags；
／／表示当使用IOCB FLAG RESFD标志位时，用于进行事件通知的句柄
U int32 t aio resfd;
    因此，在设置好iocb结构体后，就可以向异步I/O提交事件了。aio_lio_opcode操作码
指定了这个事件的操作类型，它的取值范围如下。
    typedef enum io_iocb_cmd{
    ／／异步读操作
    IOL CMD—PREAD=O，
    ／／异步写操作
    IO—CMD—PWRITE=1，
    ／／强制同步
    IO_ CMD—FSYNC=2，
    ／／目前采使用
    IO—CMD- FDSYNC=3，
    ／／目前未使用
    IO—CMD—POLL=5，
    ／／不做任何事情
    IO一CMD- NOOP=6，
    )  io_iocIb_cmd_t j
    在Nginx中，仅使用了IO—CMD_ PREAD命令，这是因为目前仅支持文件的异步I/O读
取，不支持异步I/O的写入。这其中一个重要的原因是文件的异步I/O无法利用缓存，而写
文件操作通常是落到缓存中的，Linux存在统一将缓存中“脏”数据刷新到磁盘的机制。
    这样，使用io submit向内核提交了文件异步I/O操作的事件后，再使用io_ cancel则可
以将已经提交的事件取消。
    如何获取已经完成的异步I/O事件呢？io_getevents方法可以做到，它相当于epoll中的
epoll_wait方法，这里用到了io一event结构体，下面看一下它的定义。
struct io event  {
    ／／与提交事件时对应的iocb结构体中的aio_ data是一致的
    uint64 t  data;
    ／／指向提交事件时对应的iocb结构体
    uint64_t   obj；
    ／／异步I/O请求的结构。res大于或等于0时表示成功，小于0时表示失败
    int64一t    res；
    ／7保留卑段
    int64一七    res2 j
)；
    这样，根据获取的io—event结构体数组，就可以获得已经完成的异步I/O操作了，特别
是iocb结构体中的aio data成员和io—event中的data，可用于传递指针，也就是说，业务中
的数据结构、事件完成后的回调方法都在这里。
    进程退出时需要调用io_destroy方法销毁异步I/O上下文，这相当于调用close关闭
epoll的描述符。
    Linux内核提供的文件异步I/O机制用法非常简单，它充分利用了在内核中CPU与I/O
设备是各自独立工作的这一特性，在提交了异步I/O操作后，进程完全可以做其他工作，直
到空闲再来查看异步I/O操作是否完成。

*/

extern int            ngx_eventfd;
extern aio_context_t  ngx_aio_ctx;


static void ngx_file_aio_event_handler(ngx_event_t *ev);


static int
io_submit(aio_context_t ctx, long n, struct iocb **paiocb) //相当于epoll中的epoll_ctrl 其中io_submit相当于向异步I/O中添加事件
{
    return syscall(SYS_io_submit, ctx, n, paiocb);
}

//file为要读取的file文件信息
ngx_int_t
ngx_file_aio_init(ngx_file_t *file, ngx_pool_t *pool)
{
    ngx_event_aio_t  *aio;

    aio = ngx_pcalloc(pool, sizeof(ngx_event_aio_t));
    if (aio == NULL) {
        return NGX_ERROR;
    }

    aio->file = file;
    aio->fd = file->fd;
    aio->event.data = aio;
    aio->event.ready = 1;
    aio->event.log = file->log;

    file->aio = aio;

    return NGX_OK;
}

/*
而io_cancel则相当于从异步I/O中移除事件。io_submit中用到了一个结构体iocb，下面简单地看一下它的定义。
    struct iocb  f
    ／t存储着业务需要的指针。例如，在Nginx中，这个字段通常存储着对应的ngx_event_t亭件的指
针。它实际上与io_getevents方法中返回的io event结构体的data成员是完全一致的+／
    u int64 t aio data;
7 7不需要设置
u  int32七PADDED (aio_key,  aio_raservedl)j
／／操作码，其取值范围是io_iocb_cmd_t中的枚举命令
u int16 t aio lio_opcode;
／／请求的优先级
int16 t aio_reqprio,
，／i41-描述符
u int32 t aio fildes;
／／读／写操作对应的用户态缓冲区
u int64 t aio buf;
／／读／写操作的字节长度
u int64 t aio_nbytes;
／／读／写操作对应于文件中的偏移量
int64 t aio offset;
7 7保售手段
u int64七aio reserved2;
    ／+表示可以设置为IOCB FLAG RESFD，它会告诉内核当有异步I/O请求处理完成时使用eventfd进
行通知，可与epoll配合使用+／
    u int32七aio_flags；
／／表示当使用IOCB FLAG RESFD标志位时，用于进行事件通知的句柄
U int32 t aio resfd;
    因此，在设置好iocb结构体后，就可以向异步I/O提交事件了。aio_lio_opcode操作码
指定了这个事件的操作类型，它的取值范围如下。
    typedef enum io_iocb_cmd_t{
    ／／异步读操作
    IO_CMD_PREAD=O，
    ／／异步写操作
    IO_CMD_PWRITE=1，
    ／／强制同步
    IO_CMD_FSYNC=2，
    ／／目前采使用
    IO_CMD_FDSYNC=3，
    ／／目前未使用
    IO_CMD_POLL=5，
    ／／不做任何事情
    IO_CMD_NOOP=6，
    )  io_iocb_cmd_t
    在Nginx中，仅使用了IO_CMD_PREAD命令，这是因为目前仅支持文件的异步I/O读取，不支持异步I/O的写入。这其中一个重要的原因是文件的
异步I/O无法利用缓存，而写文件操作通常是落到缓存中的，Linux存在统一将缓存中“脏”数据刷新到磁盘的机制。
    这样，使用io submit向内核提交了文件异步I/O操作的事件后，再使用io_cancel则可以将已经提交的事件取消。
    如何获取已经完成的异步I/O事件呢？io_getevents方法可以做到，它相当于epoll中的epoll_wait方法，这里用到了io_event结构体，下面看一下它的定义。
struct io event  {
    ／／与提交事件时对应的iocb结构体中的aio_data是一致的
    uint64 t  data;
    ／／指向提交事件时对应的iocb结构体
    uint64_t   obj；
    ／／异步I/O请求的结构。res大于或等于0时表示成功，小于0时表示失败
    int64一t    res；
    ／7保留卑段
    int64一七    res2 j
)；

*/

/*
怎样向异步I/O上下文中提交异步I/O操作呢？ngx_linux_aio read.c文件中的ngx_file_aio read方法，在打开文件异步I/O后，这个方法将会负责磁盘文件的读取
*/
/*
ngx_epoll_aio_init初始化aio事件列表， ngx_file_aio_read添加读文件事件，当读取完毕后epoll会触发
ngx_epoll_eventfd_handler->ngx_file_aio_event_handler 
nginx file aio只提供了read接口，不提供write接口，因为异步aio只从磁盘读和写，而非aio方式一般写会落到
磁盘缓存，所以不提供该接口，如果异步io写可能会更慢
*/

//和ngx_epoll_eventfd_handler对应,先执行ngx_file_aio_read想异步I/O中添加读事件，然后通过epoll触发返回读取数据成功，再执行ngx_epoll_eventfd_handler
ssize_t
ngx_file_aio_read(ngx_file_t *file, u_char *buf, size_t size, off_t offset,
    ngx_pool_t *pool)
{//注意aio内核读取完毕后，是放在ngx_output_chain_ctx_t->buf中的，见ngx_output_chain_copy_buf->ngx_file_aio_read
    ngx_err_t         err;
    struct iocb      *piocb[1];
    ngx_event_t      *ev; //如果是文件异步i/o中的ngx_event_aio_t，则它来自ngx_event_aio_t->ngx_event_t(只有读),如果是网络事件中的event,则为ngx_connection_s中的event(包括读和写)
    ngx_event_aio_t  *aio;

    if (!ngx_file_aio) { //不支持文件异步I/O
        return ngx_read_file(file, buf, size, offset);
    }

    if (file->aio == NULL && ngx_file_aio_init(file, pool) != NGX_OK) {
        return NGX_ERROR;
    }

    aio = file->aio;
    ev = &aio->event;

    if (!ev->ready) { //ngx_epoll_eventfd_handler中置1
        ngx_log_error(NGX_LOG_ALERT, file->log, 0,
                      "second aio post for \"%V\"", &file->name);
        return NGX_AGAIN;
    }

    ngx_log_debug4(NGX_LOG_DEBUG_CORE, file->log, 0,
                   "aio complete:%d @%O:%z %V",
                   ev->complete, offset, size, &file->name);

    //在向客户端发送后端包体的时候，会两次执行该函数，一次是ngx_event_pipe_write_to_downstream-> p->output_filter(),另一次是aio读事件
    //通知内核读取完毕，从ngx_file_aio_event_handler走到这里，第一次到这里的时候complete为0，第二次的时候为1
    if (ev->complete) { //ngx_epoll_eventfd_handler中置1
        ev->active = 0;
        ev->complete = 0;

        if (aio->res >= 0) {
            ngx_set_errno(0);
            return aio->res;
        }

        ngx_set_errno(-aio->res);

        ngx_log_error(NGX_LOG_CRIT, file->log, ngx_errno,
                      "aio read \"%s\" failed", file->name.data);

        return NGX_ERROR;
    }

    ngx_memzero(&aio->aiocb, sizeof(struct iocb));
    /*
    注意，aio_data已经设置为这个ngx_event_t事件的指针，这样，从io_getevents方法获取的io_event对象中的data也是这个指针
     */
    aio->aiocb.aio_data = (uint64_t) (uintptr_t) ev; //在ngx_epoll_eventfd_handler中会获取该ev
    aio->aiocb.aio_lio_opcode = IOCB_CMD_PREAD;  //只对文件异步I/O进行读操作  目前NGINX仅支持异步读取，不支持异步AIO写入
    aio->aiocb.aio_fildes = file->fd; //文件句柄
    aio->aiocb.aio_buf = (uint64_t) (uintptr_t) buf; //读/写操作对应的用户态缓冲区
    aio->aiocb.aio_nbytes = size; //读/写操作的字节长度
    aio->aiocb.aio_offset = offset; //读 写操作对应于文件中的偏移量
//表示可以设置为IOCB_FLAG_RESFD，它会告诉内核当有异步I/O请求处理完成时使用eventfd进行通知，可与epoll配合使用，其在Nginx中的使用方法可参见9.9.2节
    aio->aiocb.aio_flags = IOCB_FLAG_RESFD;
//表示当使用IOCB_FLAG_RESFD标志位时，用于进行事件通知的句柄
    aio->aiocb.aio_resfd = ngx_eventfd;
/*
异步文件i/o设置事件的回调方法为ngx_file_aio_event_handler，它的调用关系类似这样：epoll_wait中调用ngx_epoll_eventfd_handler方法将当前事件
放入到ngx_posted_events队列中，在延后执行的队列中调用ngx_file_aio_event_handler方法
*/
    ev->handler = ngx_file_aio_event_handler; //在ngx_epoll_eventfd_handler中会获取该ev，然后会执行该handler

    piocb[0] = &aio->aiocb;
    //调用io_submit向ngx_aio_ctx异步1/0上下文中添加1个事件，返回1表示成功
    if (io_submit(ngx_aio_ctx, 1, piocb) == 1) {
        ev->active = 1;
        ev->ready = 0;
        ev->complete = 0;

        return NGX_AGAIN;
    }

    err = ngx_errno;

    if (err == NGX_EAGAIN) {
        return ngx_read_file(file, buf, size, offset);
    }

    ngx_log_error(NGX_LOG_CRIT, file->log, err,
                  "io_submit(\"%V\") failed", &file->name);

    if (err == NGX_ENOSYS) {
        ngx_file_aio = 0;
        return ngx_read_file(file, buf, size, offset);
    }

    return NGX_ERROR;
}

/*
异步文件i/o设置事件的回调方法为ngx_file_aio_event_handler，它的调用关系类似这样：epoll_wait中调用ngx_epoll_eventfd_handler方法将当前事件
放入到ngx_posted_events队列中，在延后执行的队列中调用ngx_file_aio_event_handler方法
*/
static void
ngx_file_aio_event_handler(ngx_event_t *ev)
{
    ngx_event_aio_t  *aio;

    aio = ev->data;

    ngx_log_debug2(NGX_LOG_DEBUG_CORE, ev->log, 0,
                   "aio event handler fd:%d %V", aio->fd, &aio->file->name);

    /* 这里调用了ngx_event_aio_t结构体的handler回调方法，这个回调方法是由真正的业务
模块实现的，也就是说，任一个业务模块想使用文件异步I/O，就可以实现handler穷法，这
样，在文件异步操作完成后，该方法就会被回调。 */
    aio->handler(ev);//例如ngx_output_chain_copy_buf->ngx_http_copy_aio_handler
}

