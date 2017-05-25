
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_SHMEM_H_INCLUDED_
#define _NGX_SHMEM_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

//共享内存空间在ngx_shm_alloc中开辟
typedef struct {
/*
 * 共享内存的起始地址开始处数据:
 * ngx_slab_pool_t + 
 * 9 * sizeof(ngx_slab_page_t)(slots_m[]) + 
 * pages * sizeof(ngx_slab_page_t)(pages_m[]) +
 * pages * ngx_pagesize
 * (这是实际的数据部分，每个 ngx_pagesize 都由前面的一个 ngx_slab_page_t 进行管理，
 * 并且每个 ngx_pagesize 最前端第一个 obj 存放的是一个或者多个 int 类型 bitmap，
 * 用于管理每块分配出去的内存)
 *
 * 详见 ngx_init_zone_pool：
 * 共享内存的起始地址开始的 sizeof(ngx_slab_pool_t) 字节是用来存储管理共享内存的 slab poll 的
 */ 
    u_char      *addr; //共享内存起始地址  
    size_t       size; //共享内存空间大小
    ngx_str_t    name; //这块共享内存的名称
    ngx_log_t   *log;  //shm.log = cycle->log; 记录日志的ngx_log_t对象
    ngx_uint_t   exists;   /* unsigned  exists:1;  */ //表示共享内存是否已经分配过的标志位，为1时表示已经存在
} ngx_shm_t;


ngx_int_t ngx_shm_alloc(ngx_shm_t *shm);
void ngx_shm_free(ngx_shm_t *shm);


#endif /* _NGX_SHMEM_H_INCLUDED_ */
