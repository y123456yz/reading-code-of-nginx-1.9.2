
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


static void *ngx_palloc_block(ngx_pool_t *pool, size_t size);
static void *ngx_palloc_large(ngx_pool_t *pool, size_t size);

/*
ngx_create_pool：创建pool
ngx_destory_pool：销毁 pool
ngx_reset_pool：重置pool中的部分数据
ngx_palloc/ngx_pnalloc：从pool中分配一块内存
ngx_pool_cleanup_add：为pool添加cleanup数据
*/
ngx_pool_t *
ngx_create_pool(size_t size, ngx_log_t *log)
{
    ngx_pool_t  *p;

    p = ngx_memalign(NGX_POOL_ALIGNMENT, size, log); //// 分配一块 size 大小的内存  内存空间16字节对齐
    if (p == NULL) {
        return NULL;
    }

    // 对pool中的数据项赋初始值
    p->d.last = (u_char *) p + sizeof(ngx_pool_t); //可用空间要减去这个头部 首sizeof(ngx_pool_t)便是pool的header信息，header信息中的各个字段用于管理整个pool
    p->d.end = (u_char *) p + size;
    p->d.next = NULL;
    p->d.failed = 0;

    size = size - sizeof(ngx_pool_t);
    p->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL; //不能超过NGX_MAX_ALLOC_FROM_POOL// pool 中最大可用大小
    
    p->current = p;
    p->chain = NULL;
    p->large = NULL;
    p->cleanup = NULL;
    p->log = log;

    return p; //指向空间最顶部头部
}

/*
ngx_create_pool：创建pool
ngx_destroy_pool：销毁 pool
ngx_reset_pool：重置pool中的部分数据
ngx_palloc/ngx_pnalloc：从pool中分配一块内存
ngx_pool_cleanup_add：为pool添加cleanup数据
*/
void
ngx_destroy_pool(ngx_pool_t *pool)
{
    ngx_pool_t          *p, *n;
    ngx_pool_large_t    *l;
    ngx_pool_cleanup_t  *c;

    for (c = pool->cleanup; c; c = c->next) { //cleanup在ngx_pool_cleanup_add赋值
        if (c->handler) {
            ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
                           "run cleanup: %p", c);
            c->handler(c->data);
        }
    }

    for (l = pool->large; l; l = l->next) {

        ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0, "free: %p", l->alloc);

        if (l->alloc) {
            ngx_free(l->alloc);
        }
    }

#if (NGX_DEBUG)

    /*
     * we could allocate the pool->log from this pool
     * so we cannot use this log while free()ing the pool
     */

    for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next) {
        ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
                       "free: %p, unused: %uz", p, p->d.end - p->d.last);

        if (n == NULL) {
            break;
        }
    }

#endif

    for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next) {
        ngx_free(p);

        if (n == NULL) {
            break;
        }
    }
}

/*
ngx_create_pool：创建pool
ngx_destory_pool：销毁 pool
ngx_reset_pool：重置pool中的部分数据
ngx_palloc/ngx_pnalloc：从pool中分配一块内存
ngx_pool_cleanup_add：为pool添加cleanup数据
*/ //这个主要是释放大块内存数据，同时可以从新使用数据块中的数据
void
ngx_reset_pool(ngx_pool_t *pool)
{
    ngx_pool_t        *p;
    ngx_pool_large_t  *l;

    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            ngx_free(l->alloc);
        }
    }

    for (p = pool; p; p = p->d.next) {
        p->d.last = (u_char *) p + sizeof(ngx_pool_t);
        p->d.failed = 0;
    }

    pool->current = pool;
    pool->chain = NULL;
    pool->large = NULL;
}

/*
实际上，在r中可以获得许多内存池对象，这些内存池的大小、意义及生存期各不相同。第3部分会涉及许多内存池，本章使用r->pool内存池即可。有了ngx_pool_t对象后，
可以从内存池中分配内存。
其中，ngx_palloc函数将会从pool内存池中分配到size字节的内存，并返回这段内存的起始地址。如果返回NULL空指针，则表示分配失败。
还有一个封装了ngx_palloc的函数ngx_pcalloc，它多做了一件事，就是把ngx_palloc申请到的内存块全部置为0，虽然，多数情况下更适合用ngx_pcalloc来分配内存。
*/ //我们使用ngx_palloc从内存池中获取一块内存
//ngx_palloc和ngx_palloc的区别是分片小块内存时是否需要内存对齐
void *
ngx_palloc(ngx_pool_t *pool, size_t size)
{
    u_char      *m;
    ngx_pool_t  *p;
    
    // 判断 size 是否大于 pool 最大可使用内存大小
    if (size <= pool->max) {

        p = pool->current; //从current所在的pool数据节点开始往后遍历寻找那个节点可以分配size内存

        do {
            m = ngx_align_ptr(p->d.last, NGX_ALIGNMENT);// 将 m 对其到内存对齐地址
            if ((size_t) (p->d.end - m) >= size) {// 判断 pool 中剩余内存是否够用
                p->d.last = m + size;

                return m;
            }

            p = p->d.next;//如果当前内存不够，则在下一个内存快中分配空间

        } while (p);

        return ngx_palloc_block(pool, size);
    }

    /*
    我们讨论最后一种情况，当需要的内存大于pool最大可分配内存大小时，此时首先判断size已经大于pool->max的大小了，所以直接调用ngx_palloc_large进行大内存分配，我们将注意力转向这个函数
    本篇文章来源于 Linux公社网站(www.linuxidc.com)  原文链接：http://www.linuxidc.com/Linux/2011-08/41860.htm
    */
    return ngx_palloc_large(pool, size);
}

//ngx_palloc和ngx_palloc的区别是分片小块内存时是否需要内存对齐
void *
ngx_pnalloc(ngx_pool_t *pool, size_t size)
{
    u_char      *m;
    ngx_pool_t  *p;

    if (size <= pool->max) {

        p = pool->current;

        do {
            m = p->d.last;

            if ((size_t) (p->d.end - m) >= size) {
                p->d.last = m + size;

                return m;
            }

            p = p->d.next;

        } while (p);

        return ngx_palloc_block(pool, size);
    }

    return ngx_palloc_large(pool, size);
}

//如果前面开辟的pool空间已经用完，则从新开辟空间ngx_pool_t
static void *
ngx_palloc_block(ngx_pool_t *pool, size_t size)
{
    u_char      *m;
    size_t       psize;
    ngx_pool_t  *p, *new;

    // 先前的整个 pool 的大小
    psize = (size_t) (pool->d.end - (u_char *) pool);

    //// 在内存对齐了的前提下，新分配一块内存
    m = ngx_memalign(NGX_POOL_ALIGNMENT, psize, pool->log);
    if (m == NULL) {
        return NULL;
    }

    new = (ngx_pool_t *) m;

    new->d.end = m + psize;
    new->d.next = NULL;
    new->d.failed = 0;

    m += sizeof(ngx_pool_data_t);
    m = ngx_align_ptr(m, NGX_ALIGNMENT);
    new->d.last = m + size;

    // 判断在当前 pool 分配内存的失败次数，即：不能复用当前 pool 的次数，
    // 如果大于 4 次，这放弃在此 pool 上再次尝试分配内存，以提高效率
    //如果失败次数大于4（不等于4），则更新current指针，放弃对老pool的内存进行再使用
    for (p = pool->current; p->d.next; p = p->d.next) {
        if (p->d.failed++ > 4) {
            pool->current = p->d.next;// 更新 current 指针， 每次从pool中分配内存的时候都是从curren开始遍历pool节点获取内存的
        }
    }

    // 让旧指针数据区的 next 指向新分配的 pool
    p->d.next = new;

    return m;
}

/*
当需要的内存大于pool最大可分配内存大小时，此时首先判断size已经大于pool->max的大小了，所以直接调用ngx_palloc_large进行大内存分配，
本篇文章来源于 Linux公社网站(www.linuxidc.com)  原文链接：http://www.linuxidc.com/Linux/2011-08/41860.htm
*/
static void *
ngx_palloc_large(ngx_pool_t *pool, size_t size)
{
    void              *p;
    ngx_uint_t         n;
    ngx_pool_large_t  *large;

    /*
    // 重新申请一块大小为 size 的新内存
    // 注意：此处不使用 ngx_memalign 的原因是，新分配的内存较大，对其也没太大必要
    //  而且后面提供了 ngx_pmemalign 函数，专门用户分配对齐了的内存
    */
    p = ngx_alloc(size, pool->log);
    if (p == NULL) {
        return NULL;
    }

    n = 0;

    // 查找largt链表上空余的large 指针
    for (large = pool->large; large; large = large->next) {
        if (large->alloc == NULL) { //就用这个没用的large
            large->alloc = p;
            return p;
        }

        /*
         // 如果当前 large 后串的 large 内存块数目大于 3 （不等于3），
        // 则直接去下一步分配新内存，不再查找了
        */
        if (n++ > 3) {//也就是说如果pool->large头后面连续4个large的alloc指针都被用了，则重新申请一个新的pool_larg并放到pool->large头部
            break; //????? 感觉没啥用，因为后面每次alloc的large对应的alloc都是赋值了的
        }
    }

    large = ngx_palloc(pool, sizeof(ngx_pool_large_t));
    if (large == NULL) {
        ngx_free(p);
        return NULL;
    }

    // 将新分配的 large 串到链表后面
    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}


void *
ngx_pmemalign(ngx_pool_t *pool, size_t size, size_t alignment)
{
    void              *p;
    ngx_pool_large_t  *large;

    p = ngx_memalign(alignment, size, pool->log);
    if (p == NULL) {
        return NULL;
    }

    large = ngx_palloc(pool, sizeof(ngx_pool_large_t));
    if (large == NULL) {
        ngx_free(p);
        return NULL;
    }

    large->alloc = p;
    large->next = pool->large;
    pool->large = large;

    return p;
}


ngx_int_t
ngx_pfree(ngx_pool_t *pool, void *p)
{
    ngx_pool_large_t  *l;

    for (l = pool->large; l; l = l->next) {
        if (p == l->alloc) {
            ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, pool->log, 0,
                           "free: %p", l->alloc);
            ngx_free(l->alloc);
            l->alloc = NULL;

            return NGX_OK;
        }
    }

    return NGX_DECLINED;
}


void *
ngx_pcalloc(ngx_pool_t *pool, size_t size)
{
    void *p;

    p = ngx_palloc(pool, size);
    if (p) {
        ngx_memzero(p, size);
    }

    return p;
}

/*
ngx_create_pool：创建pool
ngx_destory_pool：销毁 pool
ngx_reset_pool：重置pool中的部分数据
ngx_palloc/ngx_pnalloc：从pool中分配一块内存
ngx_pool_cleanup_add：为pool添加cleanup数据
*/ //在pool中分配带有handler的内存，
/*
以回收file为例:

可以看到，ngx_pool_cleanup_file_t中的对象在ngx_buf_t缓冲区的file结构体中都出现过了，意义也是相同的。对于file结构体，我们在内存池中已经为它分配过内存，
只有在请求结束时才会释放，因此，这里简单地引用file里的成员即可。清理文件句柄的完整代码如下。
ngx_pool_cleanup_t* cln = ngx_pool_cleanup_add(r->pool, sizeof(ngx_pool_cleanup_file_t));
if (cln == NULL) {
 return NGX_ERROR;
}

cln->handler = ngx_pool_cleanup_file;
ngx_pool_cleanup_file_t  *clnf = cln->data;

clnf->fd = b->file->fd;
clnf->name = b->file->name.data;
clnf->log = r->pool->log;

ngx_pool_cleanup_add用于告诉HTTP框架，在请求结束时调用cln的handler方法清理资源。
*///poll的清理用ngx_pool_cleanup_add, ngx_http_request_t的清理用ngx_http_cleanup_add
ngx_pool_cleanup_t *
ngx_pool_cleanup_add(ngx_pool_t *p, size_t size)
{
    ngx_pool_cleanup_t  *c;

    c = ngx_palloc(p, sizeof(ngx_pool_cleanup_t));
    if (c == NULL) {
        return NULL;
    }

    if (size) {
        c->data = ngx_palloc(p, size);
        if (c->data == NULL) {
            return NULL;
        }

    } else {
        c->data = NULL;
    }

    c->handler = NULL;
    c->next = p->cleanup;

    p->cleanup = c;

    ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, p->log, 0, "add cleanup: %p", c);

    return c;
}


void
ngx_pool_run_cleanup_file(ngx_pool_t *p, ngx_fd_t fd)
{
    ngx_pool_cleanup_t       *c;
    ngx_pool_cleanup_file_t  *cf;

    for (c = p->cleanup; c; c = c->next) {
        if (c->handler == ngx_pool_cleanup_file) {

            cf = c->data;

            if (cf->fd == fd) {
                c->handler(cf);
                c->handler = NULL;
                return;
            }
        }
    }
}

/*
Nginx会异步地将整个文件高效地发送给用户，但是我们必须要求HTTP框架在响应发送完毕后关闭已经打开的文件句柄，否则将会出现句柄泄露问题。
设置清理文件句柄也很简单，只需要定义一个ngx_pool_cleanup_t结构体（这是最简单的方法，HTTP框架还提供了其他方式，在请求结束时回调各个HTTP模块的cleanup方法，将在第11章介绍），
将我们刚得到的文件句柄等信息赋给它，并将Nginx提供的ngx_pool_cleanup_file函数设置到它的handler回调方法中即可。

*/
//ngx_pool_cleanup_file的作用是把文件句柄关闭。从上面的实现中可以看出，ngx_pool_cleanup_file方法需要一个ngx_pool_cleanup_file_t类型的参数，
//那么，如何提供这个参数呢？在ngx_pool_cleanup_t结构体的data成员上赋值即可。下面介绍一下ngx_pool_cleanup_file_t的结构。

/*
可以看到，ngx_pool_cleanup_file_t中的对象在ngx_buf_t缓冲区的file结构体中都出现过了，意义也是相同的。对于file结构体，我们在内存池中已经为它分配过内存，
只有在请求结束时才会释放，因此，这里简单地引用file里的成员即可。清理文件句柄的完整代码如下。
ngx_pool_cleanup_t* cln = ngx_pool_cleanup_add(r->pool, sizeof(ngx_pool_cleanup_file_t));
if (cln == NULL) {
 return NGX_ERROR;
}

cln->handler = ngx_pool_cleanup_file;
ngx_pool_cleanup_file_t  *clnf = cln->data;

clnf->fd = b->file->fd;
clnf->name = b->file->name.data;
clnf->log = r->pool->log;

ngx_pool_cleanup_add用于告诉HTTP框架，在请求结束时调用cln的handler方法清理资源。
*/
void
ngx_pool_cleanup_file(void *data)
{
    ngx_pool_cleanup_file_t  *c = data;

    ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, c->log, 0, "file cleanup: fd:%d",
                   c->fd);

    if (ngx_close_file(c->fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, c->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", c->name);
    }
}


void
ngx_pool_delete_file(void *data)
{
    ngx_pool_cleanup_file_t  *c = data;

    ngx_err_t  err;

    ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, c->log, 0, "file cleanup: fd:%d %s",
                   c->fd, c->name);

    if (ngx_delete_file(c->name) == NGX_FILE_ERROR) {
        err = ngx_errno;

        if (err != NGX_ENOENT) {
            ngx_log_error(NGX_LOG_CRIT, c->log, err,
                          ngx_delete_file_n " \"%s\" failed", c->name);
        }
    }

    if (ngx_close_file(c->fd) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, c->log, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", c->name);
    }
}


#if 0

static void *
ngx_get_cached_block(size_t size)
{
    void                     *p;
    ngx_cached_block_slot_t  *slot;

    if (ngx_cycle->cache == NULL) {
        return NULL;
    }

    slot = &ngx_cycle->cache[(size + ngx_pagesize - 1) / ngx_pagesize];

    slot->tries++;

    if (slot->number) {
        p = slot->block;
        slot->block = slot->block->next;
        slot->number--;
        return p;
    }

    return NULL;
}

#endif

