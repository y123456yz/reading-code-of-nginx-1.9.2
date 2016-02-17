
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>

//n size分别对应ngx_list_t中的size和nalloc
ngx_list_t *
ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size) //实际上就是为nginx_list_t的part成员创建指定的n*size空间，并且创建了空间sizeof(ngx_list_t)
{
    ngx_list_t  *list;

    list = ngx_palloc(pool, sizeof(ngx_list_t));
    if (list == NULL) {
        return NULL;
    }

    if (ngx_list_init(list, pool, n, size) != NGX_OK) {
        return NULL;
    }

    return list;
}

/*
ngx_list_t也是一个顺序容器，它实际上相当于动态数组与单向链表的结
合体，只是扩容起来比动态数组简单得多，它可以一次性扩容1个数组。
*/
//如果l中的last的elts用完了，则在l链表中新创建一个ngx_list_part_t，起实际数据部分空间大小为l->nalloc * l->size。
//如果l的last还有剩余，则返回last中未用的空间
void *
ngx_list_push(ngx_list_t *l)
{
    void             *elt;
    ngx_list_part_t  *last;

    last = l->last;

    if (last->nelts == l->nalloc) {

        /* the last part is full, allocate a new list part */

        last = ngx_palloc(l->pool, sizeof(ngx_list_part_t));
        if (last == NULL) {
            return NULL;
        }

        last->elts = ngx_palloc(l->pool, l->nalloc * l->size);
        if (last->elts == NULL) {
            return NULL;
        }

        last->nelts = 0;
        last->next = NULL;

        l->last->next = last;
        l->last = last;
    }

    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;

    return elt;
}

