
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_ARRAY_H_INCLUDED_
#define _NGX_ARRAY_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

/*cookies是以ngx_array_t数组存储的，本章先不介绍这个数据结构，感兴趣的话可以直接跳到7.3节了解ngx_array_t的相关用法*/
//如果某个配置项在nginx.conf文件中可能出现多次，则用这个来进行动态存储，参考ngx_conf_set_str_array_slot
typedef struct { //可以通过ngx_array_create函数创建空间，并初始化各个成员
    void        *elts; //可以是ngx_keyval_t  ngx_str_t  ngx_bufs_t ngx_hash_key_t等
    ngx_uint_t   nelts; //已经使用了多少个
    size_t       size; //每个elts的空间大小，
    ngx_uint_t   nalloc; //最多有多少个elts元素
    ngx_pool_t  *pool; //赋值见ngx_init_cycle，为cycle的时候分配的pool空间
} ngx_array_t;

ngx_array_t *ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size);
void ngx_array_destroy(ngx_array_t *a);
void *ngx_array_push(ngx_array_t *a);
void *ngx_array_push_n(ngx_array_t *a, ngx_uint_t n);


static ngx_inline ngx_int_t
ngx_array_init(ngx_array_t *array, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    /*
     * set "array->nelts" before "array->elts", otherwise MSVC thinks
     * that "array->nelts" may be used without having been initialized
     */

    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;

    array->elts = ngx_palloc(pool, n * size);
    if (array->elts == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

#endif /* _NGX_ARRAY_H_INCLUDED_ */

