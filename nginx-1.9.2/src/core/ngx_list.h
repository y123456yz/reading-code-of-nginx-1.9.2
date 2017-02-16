
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_LIST_H_INCLUDED_
#define _NGX_LIST_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_list_part_s  ngx_list_part_t; //ngx_list_part_t只描述链表的一个元素

//链表图形化见<<输入理解nginx>> 3.2.3　ngx_list_t数据结构
//内存分配参考ngx_list_push
struct ngx_list_part_s { //ngx_list_part_t只描述链表的一个元素   数据部分总的空间大小为size * nalloc字节
    void             *elts; //指向数组的起始地址。
    ngx_uint_t        nelts; //数组当前已使用了多少容量  表示数组中已经使用了多少个元素。当然，nelts必须小于ngx_list_t 结构体中的nalloc。
    ngx_list_part_t  *next; //下一个链表元素ngx_list_part_t的地址。
};

/* ngx_list_t和ngx_queue_t的却别在于:ngx_list_t需要负责容器内成员节点内存分配，而ngx_queue_t不需要 */
//用法和数组遍历方法可以参考//ngx_http_request_s->headers_in.headers，例如可以参考函数ngx_http_fastcgi_create_request
typedef struct { //ngx_list_t描述整个链表
    ngx_list_part_t  *last; //指向链表的最后一个数组元素。
    ngx_list_part_t   part; //链表的首个数组元素。 part可能指向多个数组，通过part->next来指向当前数组所在的下一个数组的头部
    /*
    链表中的每个ngx_list_part_t元素都是一个数组。因为数组存储的是某种类型的数据结构，且ngx_list_t 是非常灵活的数据结构，所以它不会限制存储
    什么样的数据，只是通过size限制每一个数组元素的占用的空间大小，也就是用户要存储的一个数据所占用的字节数必须小于或等于size。
    */ //size就是数组元素中的每个子元素的大小最大这么大
    size_t            size;  //创建list的时候在ngx_list_create->ngx_list_init中需要制定n和size大小
    ngx_uint_t        nalloc; //链表的数组元素一旦分配后是不可更改的。nalloc表示每个ngx_list_part_t数组的容量，即最多可存储多少个数据。
    ngx_pool_t       *pool; //链表中管理内存分配的内存池对象。用户要存放的数据占用的内存都是由pool分配的，下文中会详细介绍。
} ngx_list_t;

//n size分别对应ngx_list_t中的size和nalloc
ngx_list_t *ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size);

static ngx_inline ngx_int_t
ngx_list_init(ngx_list_t *list, ngx_pool_t *pool, ngx_uint_t n, size_t size) //n size分别对应ngx_list_t中的size和nalloc
{
    list->part.elts = ngx_palloc(pool, n * size); 
    if (list->part.elts == NULL) {
        return NGX_ERROR;
    }

    list->part.nelts = 0;
    list->part.next = NULL;
    list->last = &list->part;
    list->size = size;
    list->nalloc = n;
    list->pool = pool;

    return NGX_OK;
}


/* 遍历list链表
 *
 *  the iteration through the list:
 *
 *  part = &list.part;
 *  data = part->elts;
 *
 *  for (i = 0 ;; i++) {
 *
 *      if (i >= part->nelts) {
 *          if (part->next == NULL) {
 *              break;
 *          }
 *
 *          part = part->next;
 *          data = part->elts;
 *          i = 0;
 *      }
 *
 *      ...  data[i] ...
 *
 *  }
 */


void *ngx_list_push(ngx_list_t *list);


#endif /* _NGX_LIST_H_INCLUDED_ */
