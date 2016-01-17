
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_TIMER_H_INCLUDED_
#define _NGX_EVENT_TIMER_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


#define NGX_TIMER_INFINITE  (ngx_msec_t) -1

#define NGX_TIMER_LAZY_DELAY  300


ngx_int_t ngx_event_timer_init(ngx_log_t *log);
ngx_msec_t ngx_event_find_timer(void);
void ngx_event_expire_timers(void);
void ngx_event_cancel_timers(void);


extern ngx_rbtree_t  ngx_event_timer_rbtree;


static ngx_inline void
ngx_event_del_timer(ngx_event_t *ev, const char *func, unsigned int line)
{//ngx_del_timer
    char tmpbuf[128];

    snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> ", func, line);
    ngx_log_debug3(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "%s event timer del: %d: %M", tmpbuf,
                    ngx_event_ident(ev->data), ev->timer.key);
                    
    ngx_rbtree_delete(&ngx_event_timer_rbtree, &ev->timer);

#if (NGX_DEBUG)
    ev->timer.left = NULL;
    ev->timer.right = NULL;
    ev->timer.parent = NULL;
#endif

    ev->timer_set = 0;
}

/*
1.ngx_event_s可以是普通的epoll读写事件(参考ngx_event_connect_peer->ngx_add_conn或者ngx_add_event)，通过读写事件触发

2.也可以是普通定时器事件(参考ngx_cache_manager_process_handler->ngx_add_timer(ngx_event_add_timer))，通过ngx_process_events_and_timers中的
epoll_wait返回，可以是读写事件触发返回，也可能是因为没获取到共享锁，从而等待0.5s返回重新获取锁来跟新事件并执行超时事件来跟新事件并且判断定
时器链表中的超时事件，超时则执行从而指向event的handler，然后进一步指向对应r或者u的->write_event_handler  read_event_handler

3.也可以是利用定时器expirt实现的读写事件(参考ngx_http_set_write_handler->ngx_add_timer(ngx_event_add_timer)),触发过程见2，只是在handler中不会执行write_event_handler  read_event_handler
*/


//ngx_event_expire_timers中执行ev->handler
//在ngx_process_events_and_timers中，当有事件使epoll_wait返回，则会执行超时的定时器
//注意定时器的超时处理，不一定就是timer时间超时，超时误差可能为timer_resolution，如果没有设置timer_resolution则定时器可能永远不超时，因为epoll_wait不返回，无法更新时间
static ngx_inline void
ngx_event_add_timer(ngx_event_t *ev, ngx_msec_t timer, const char *func, unsigned int line)
{//ngx_add_timer
    ngx_msec_t      key;
    ngx_msec_int_t  diff;
    char tmpbuf[128];

    key = ngx_current_msec + timer;
    
    if (ev->timer_set) { //如果之前该ev已经添加过，则先把之前的ev定时器del掉，然后在重新添加

        /*
         * Use a previous timer value if difference between it and a new
         * value is less than NGX_TIMER_LAZY_DELAY milliseconds: this allows
         * to minimize the rbtree operations for fast connections.
         */

        diff = (ngx_msec_int_t) (key - ev->timer.key);

        if (ngx_abs(diff) < NGX_TIMER_LAZY_DELAY) {
            snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> ", func, line);
            ngx_log_debug4(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                           "%s event timer: %d, old: %M, new: %M, ", tmpbuf,
                            ngx_event_ident(ev->data), ev->timer.key, key);
            return;
        }

        ngx_del_timer(ev, NGX_FUNC_LINE);
    }

    ev->timer.key = key;
    snprintf(tmpbuf, sizeof(tmpbuf), "<%25s, %5d> ", func, line);
    ngx_log_debug4(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                   "%s event timer add fd:%d, expire-time:%M s, timer.key:%M", tmpbuf,
                    ngx_event_ident(ev->data), timer / 1000, ev->timer.key);

    ngx_rbtree_insert(&ngx_event_timer_rbtree, &ev->timer);

    ev->timer_set = 1;
}


#endif /* _NGX_EVENT_TIMER_H_INCLUDED_ */
