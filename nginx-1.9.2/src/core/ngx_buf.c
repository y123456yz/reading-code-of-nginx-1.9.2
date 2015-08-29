
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


ngx_buf_t *
ngx_create_temp_buf(ngx_pool_t *pool, size_t size)
{
    ngx_buf_t *b;

    b = ngx_calloc_buf(pool); //这里面是为ngx_buf_t头部分配的空间
    if (b == NULL) {
        return NULL;
    }

    b->start = ngx_palloc(pool, size); //这里面才是真正存储数据的空间
    if (b->start == NULL) {
        return NULL;
    }

    /*
     * set by ngx_calloc_buf():
     *
     *     b->file_pos = 0;
     *     b->file_last = 0;
     *     b->file = NULL;
     *     b->shadow = NULL;
     *     b->tag = 0;
     *     and flags
     */

    b->pos = b->start;
    b->last = b->start;
    b->end = b->last + size;
    b->temporary = 1;

    return b;
}


ngx_chain_t *
ngx_alloc_chain_link(ngx_pool_t *pool)
{
    ngx_chain_t  *cl;

    cl = pool->chain;

    if (cl) {
        pool->chain = cl->next; //被释放的ngx_chain_t是通过ngx_free_chain添加到poll->chain上的
        return cl;
    }

    cl = ngx_palloc(pool, sizeof(ngx_chain_t));
    if (cl == NULL) {
        return NULL;
    }

    return cl;
}


ngx_chain_t *
ngx_create_chain_of_bufs(ngx_pool_t *pool, ngx_bufs_t *bufs)
{
    u_char       *p;
    ngx_int_t     i;
    ngx_buf_t    *b;
    ngx_chain_t  *chain, *cl, **ll;

    p = ngx_palloc(pool, bufs->num * bufs->size);
    if (p == NULL) {
        return NULL;
    }

    ll = &chain;

    for (i = 0; i < bufs->num; i++) {

        b = ngx_calloc_buf(pool);
        if (b == NULL) {
            return NULL;
        }

        /*
         * set by ngx_calloc_buf():
         *
         *     b->file_pos = 0;
         *     b->file_last = 0;
         *     b->file = NULL;
         *     b->shadow = NULL;
         *     b->tag = 0;
         *     and flags
         *
         */

        b->pos = p;
        b->last = p;
        b->temporary = 1;

        b->start = p;
        p += bufs->size;
        b->end = p;

        cl = ngx_alloc_chain_link(pool);
        if (cl == NULL) {
            return NULL;
        }

        cl->buf = b;
        *ll = cl;
        ll = &cl->next;
    }

    *ll = NULL;

    return chain;
}

//把in添加到chain的后面，拼接起来
ngx_int_t
ngx_chain_add_copy(ngx_pool_t *pool, ngx_chain_t **chain, ngx_chain_t *in)
{
    ngx_chain_t  *cl, **ll;

    ll = chain;

    for (cl = *chain; cl; cl = cl->next) { // 循环结束后，ll 指向最后一个 chain 的 next，next 又是指针，所以 ll 是二级指针
        ll = &cl->next;
    }

    while (in) {
        cl = ngx_alloc_chain_link(pool);
        if (cl == NULL) {
            return NGX_ERROR;
        }

        cl->buf = in->buf;
        *ll = cl;
        ll = &cl->next;
        in = in->next;
    }

    *ll = NULL;

    return NGX_OK;
}


ngx_chain_t *
ngx_chain_get_free_buf(ngx_pool_t *p, ngx_chain_t **free)
{
    ngx_chain_t  *cl;

    if (*free) { //如果free 链表中有有空余的ngx_chain_t节点，则直接使用该链表中的ngx_chain_t节点空间，否则后面开辟空间
        cl = *free;
        *free = cl->next;
        cl->next = NULL;
        return cl;
    }

    cl = ngx_alloc_chain_link(p);
    if (cl == NULL) {
        return NULL;
    }

    cl->buf = ngx_calloc_buf(p);
    if (cl->buf == NULL) {
        return NULL;
    }

    cl->next = NULL;

    return cl;
}

/*
因为nginx可以提前flush输出，所以这些buf被输出后就可以重复使用，可以避免重分配，提高系统性能，被称为free_buf，而没有被输出的
buf就是busy_buf。nginx没有特别的集成这个特性到自身，但是提供了一个函数ngx_chain_update_chains来帮助开发者维护这两个缓冲区队列
*/
//该函数功能就是把新读到的out数据添加到busy表尾部，然后把busy表中已经处理完毕的buf节点从busy表中摘除，然后放到free表头部
//未发送出去的buf节点既会在out链表中，也会在busy链表中
void
ngx_chain_update_chains(ngx_pool_t *p, ngx_chain_t **free, ngx_chain_t **busy,
    ngx_chain_t **out, ngx_buf_tag_t tag)
{
    ngx_chain_t  *cl;

    if (*busy == NULL) {
        // busy 指向 out 指向的地方
        *busy = *out;

    } else {
        for (cl = *busy; cl->next; cl = cl->next) { /* void */ } // cl 指向 busy chain 链条的最后一个
        

        cl->next = *out; //out节点添加到busy表中的最后一个节点
    }

    *out = NULL;

    while (*busy) {
        cl = *busy;

        // buf 大小不是 0，说明还没有输出；request body 中的 bufs 是输出用的，如上所述，bufs 中指向的 buf 和 busy 指向的 buf 对象是一模一样的
        if (ngx_buf_size(cl->buf) != 0) { //pos和last不相等，说明该buf中的内容没有处理完
            break;
        }

        if (cl->buf->tag != tag) {// tag 中存储的是 函数指针
            *busy = cl->next;
            ngx_free_chain(p, cl);
            continue;
        }

        //把该空间的pos last都指向start开始处，表示该ngx_buf_t没有数据在里面，因此可以把他加到free表中，可以继续读取数据到free中的ngx_buf_t节点了
        cl->buf->pos = cl->buf->start;
        cl->buf->last = cl->buf->start;

        *busy = cl->next; //把cl从busy中拆除，然后添加到free头部
        cl->next = *free;
        *free = cl; // 这个 chain 放到 free 列表的最前面，添加到free头部
    }
}


off_t
ngx_chain_coalesce_file(ngx_chain_t **in, off_t limit)
{
    off_t         total, size, aligned, fprev;
    ngx_fd_t      fd;
    ngx_chain_t  *cl;

    total = 0;

    cl = *in;
    fd = cl->buf->file->fd;

    do {
        size = cl->buf->file_last - cl->buf->file_pos;

        if (size > limit - total) {
            size = limit - total;

            aligned = (cl->buf->file_pos + size + ngx_pagesize - 1)
                       & ~((off_t) ngx_pagesize - 1);

            if (aligned <= cl->buf->file_last) {
                size = aligned - cl->buf->file_pos;
            }
        }

        total += size;
        fprev = cl->buf->file_pos + size;
        cl = cl->next;

    } while (cl
             && cl->buf->in_file
             && total < limit
             && fd == cl->buf->file->fd
             && fprev == cl->buf->file_pos);

    *in = cl;

    return total;
}

//计算本次掉用ngx_writev发送出去的send字节在in链表中所有数据的那个位置
ngx_chain_t *
ngx_chain_update_sent(ngx_chain_t *in, off_t sent)
{
    off_t  size;

    for ( /* void */ ; in; in = in->next) {
        //又遍历一次这个链接，为了找到那块只成功发送了一部分数据的内存块，从它继续开始发送。
        if (ngx_buf_special(in->buf)) {
            continue;
        }

        if (sent == 0) {
            break;
        }

        size = ngx_buf_size(in->buf);

        if (sent >= size) { //说明该in->buf数据已经全部发送出去
            sent -= size;//标记后面还有多少数据是我发送过的

            if (ngx_buf_in_memory(in->buf)) {//说明该in->buf数据已经全部发送出去
                in->buf->pos = in->buf->last;//清空这段内存。继续找下一个
            }

            if (in->buf->in_file) {
                in->buf->file_pos = in->buf->file_last;
            }

            continue;
        }

        if (ngx_buf_in_memory(in->buf)) { //说明发送出去的最后一字节数据的下一字节数据在in->buf->pos+send位置，下次从这个位置开始发送
            in->buf->pos += (size_t) sent;//这块内存没有完全发送完毕，悲剧，下回得从这里开始。
        }

        if (in->buf->in_file) {
            in->buf->file_pos += sent;
        }

        break;
    }

    return in; //下次从这个in开始发送in->buf->pos
}
