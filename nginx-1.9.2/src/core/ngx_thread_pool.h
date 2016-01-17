
/*
 * Copyright (C) Nginx, Inc.
 * Copyright (C) Valentin V. Bartenev
 */


#ifndef _NGX_THREAD_POOL_H_INCLUDED_
#define _NGX_THREAD_POOL_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>

//在中添加到了ngx_thread_pool_s->queue队列中，也就是添加到ngx_thread_pool_s对应的线程池队列中
struct ngx_thread_task_s {
    ngx_thread_task_t   *next; //指向下一个提交的任务  
    ngx_uint_t           id; //任务id  没添加一个任务就自增加，见ngx_thread_pool_task_id
    void                *ctx; //执行回调函数的参数  
    //ngx_thread_pool_cycle中执行
    void               (*handler)(void *data, ngx_log_t *log); //回调函数   执行完handler后会通过ngx_notify执行event->handler 
    //执行完handler后会通过ngx_notify执行event->handler 
    ngx_event_t          event; //一个任务和一个事件对应  事件在通过ngx_notify在ngx_thread_pool_handler中执行
};


typedef struct ngx_thread_pool_s  ngx_thread_pool_t;//一个该结构对应一个threads_pool线程池配置

ngx_thread_pool_t *ngx_thread_pool_add(ngx_conf_t *cf, ngx_str_t *name);
ngx_thread_pool_t *ngx_thread_pool_get(ngx_cycle_t *cycle, ngx_str_t *name);

ngx_thread_task_t *ngx_thread_task_alloc(ngx_pool_t *pool, size_t size);
ngx_int_t ngx_thread_task_post(ngx_thread_pool_t *tp, ngx_thread_task_t *task);


#endif /* _NGX_THREAD_POOL_H_INCLUDED_ */
