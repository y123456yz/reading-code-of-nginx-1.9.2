
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>

//开辟n个size空间
ngx_array_t *
ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size)
{
    ngx_array_t *a;

    a = ngx_palloc(p, sizeof(ngx_array_t));
    if (a == NULL) {
        return NULL;
    }

    if (ngx_array_init(a, p, n, size) != NGX_OK) {
        return NULL;
    }

    return a;
}


void
ngx_array_destroy(ngx_array_t *a)
{
    ngx_pool_t  *p;

    p = a->pool;

    if ((u_char *) a->elts + a->size * a->nalloc == p->d.last) {
        p->d.last -= a->size * a->nalloc;
    }

    if ((u_char *) a + sizeof(ngx_array_t) == p->d.last) {
        p->d.last = (u_char *) a;
    }
}

//检查array数组的elts元素释放已经用完，如果已经用完，则再重新开辟array空间来存储
//ngx_array_push从数组中获取一个数组成员,ngx_array_push_n为一次性获取n个
void *
ngx_array_push(ngx_array_t *a)
{
    void        *elt, *new;
    size_t       size;
    ngx_pool_t  *p;

    if (a->nelts == a->nalloc) {

        /* the array is full */

        size = a->size * a->nalloc;

        p = a->pool;

        if ((u_char *) a->elts + size == p->d.last
            && p->d.last + a->size <= p->d.end)
        {
            /*
             * the array allocation is the last in the pool
            * and there is space for new allocation
如果当前内存池中剩余的空间大于或者等于本次需要新增的空间，那么本次扩容将只扩充新增的空间。例如，对于ngx_array_push方法来说，
就是扩充1个元素，而对于ngx_array_push_n来说，就是扩充n个元素。
 */

            p->d.last += a->size;
            a->nalloc++;

        } else {
/* allocate a new array 
如果当前内存池中剩余的空间小于本次需要新增的空间，那么对ngx_array_push方法来说，会将原先动态数组的容量扩容一倍，而对于
ngx_array_push_n来说，情况更复杂一些，如果参数n小于原先动态数组的容量，将会扩容一倍；如果参数n大于原先动态数组的容量，
这时会分配2×n大小的空间，扩容会超过一倍。
*/

            new = ngx_palloc(p, 2 * size);
            if (new == NULL) {
                return NULL;
            }

            ngx_memcpy(new, a->elts, size);
            a->elts = new;
            a->nalloc *= 2;
        }
    }

    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts++;

    return elt;
}

//ngx_array_push从数组中获取一个数组成员,ngx_array_push_n为一次性获取n个
void *
ngx_array_push_n(ngx_array_t *a, ngx_uint_t n)
{
    void        *elt, *new;
    size_t       size;
    ngx_uint_t   nalloc;
    ngx_pool_t  *p;

    size = n * a->size;

    if (a->nelts + n > a->nalloc) {

        /* the array is full */

        p = a->pool;

        if ((u_char *) a->elts + a->size * a->nalloc == p->d.last
            && p->d.last + size <= p->d.end)
        {
            /*
             * the array allocation is the last in the pool
             * and there is space for new allocation
如果当前内存池中剩余的空间大于或者等于本次需要新增的空间，那么本次扩容将只扩充新增的空间。例如，对于ngx_array_push方法来说，
就是扩充1个元素，而对于ngx_array_push_n来说，就是扩充n个元素。
 */
             
    
            p->d.last += size;
            a->nalloc += n;

        } else {
            /* allocate a new array 
            如果当前内存池中剩余的空间小于本次需要新增的空间，那么对ngx_array_push方法来说，会将原先动态数组的容量扩容一倍，而对于
            ngx_array_push_n来说，情况更复杂一些，如果参数n小于原先动态数组的容量，将会扩容一倍；如果参数n大于原先动态数组的容量，
            这时会分配2×n大小的空间，扩容会超过一倍。
            */
            nalloc = 2 * ((n >= a->nalloc) ? n : a->nalloc);

            new = ngx_palloc(p, nalloc * a->size);
            if (new == NULL) {
                return NULL;
            }

            ngx_memcpy(new, a->elts, a->nelts * a->size);
            a->elts = new;
            a->nalloc = nalloc;
        }
    }

    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts += n; 

    return elt;
}

