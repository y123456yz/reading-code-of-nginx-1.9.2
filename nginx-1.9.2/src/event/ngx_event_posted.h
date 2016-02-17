
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_POSTED_H_INCLUDED_
#define _NGX_EVENT_POSTED_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>

/*
post事件队列的操作方法
┏━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━┓
┃    方法名                                        ┃    参数含义                  ┃    执行意义                        ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃  ngx_locked_post_event(ev,                       ┃  ev是要添加到post事件队列    ┃  向queue事件队列中添加事件ev，注   ┃
┃queue)                                            ┃的事件，queue是post事件队列   ┃意，ev将插入到事件队列的首部        ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃                                                  ┃                              ┃  线程安全地向queue事件队列中添加   ┃
┃                                                  ┃    ev是要添加到post队列的事  ┃事件ev。在目前不使用多线程的情况    ┃
┃ngx_post_event(ev, queue)                         ┃                              ┃                                    ┃
┃                                                  ┃件，queue是post事件队列       ┃下，它与ngx_locked_post_event的功能 ┃
┃                                                  ┃                              ┃是相同的                            ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃                                                  ┃    ev是要从某个post事件队列  ┃  将事件ev从其所属的post事件队列    ┃
┃ngx_delete_posted_event(ev)                       ┃                              ┃                                    ┃
┃                                                  ┃移除的事件                    ┃中删除                              ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃                                                  ┃  cycle是进程的核心结构体     ┃                                    ┃
┃   void ngx_event_process_posted                  ┃ngx_cycle_t的指针．posted是要 ┃  谰用posted事件队列中所有事件      ┃
┃(ngx_cycle_t *cycle,ngx_thread_                   ┃操作的post事件队列，它的取值  ┃的handler回调方法。每个事件调用完   ┃
┃volatile ngx_event_t **posted);                   ┃目前仅可以为ngx_posted_events ┃handler方法后，就会从posted事件队列 ┃
┃                                               I  ┃                              ┃中删除                              ┃
┃                                                  ┃或者ngx_posted_accept_events  ┃                                    ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━┛
*/
#define ngx_post_event(ev, q)                                                 \
                                                                              \
    if (!(ev)->posted) {                                                      \
        (ev)->posted = 1;                                                     \
        ngx_queue_insert_tail(q, &(ev)->queue);                               \
                                                                              \
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, (ev)->log, 0, "post event %p", ev);\
                                                                              \
    } else  {                                                                 \
        ngx_log_debug1(NGX_LOG_DEBUG_CORE, (ev)->log, 0,                      \
                       "update posted event %p", ev);                         \
    }


#define ngx_delete_posted_event(ev)                                           \
                                                                              \
    (ev)->posted = 0;                                                         \
    ngx_queue_remove(&(ev)->queue);                                           \
                                                                              \
    ngx_log_debug1(NGX_LOG_DEBUG_CORE, (ev)->log, 0,                          \
                   "delete posted event %p", ev);



void ngx_event_process_posted(ngx_cycle_t *cycle, ngx_queue_t *posted);


extern ngx_queue_t  ngx_posted_accept_events;
extern ngx_queue_t  ngx_posted_events;


#endif /* _NGX_EVENT_POSTED_H_INCLUDED_ */
