
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>

//定时器是通过一棵红黑树实现的。ngx_event_timer_rbtree就是所有定时器事件组成的红黑树，而ngx_event_timer_sentinel就是这棵红黑树的哨兵节点
/*
这棵红黑树中的每个节点都是ngx_event_t事件中的timer成员，而ngx_rbtree_node-t节点的关键字就是事件的超时时间，以这个超时时间的大小组成
了二叉排序树ngx_event_timer rbtree。这样，如果需要找出最有可能超时的事件，那么将ngx_event timer- rbtree树中最左边的节点取出来即可。
只要用当前时间去比较这个最左边节点的超时时间，就会知道这个事件有没有触发超时，如果还没有触发超时，那么会知道最少还要经过多少毫秒满足超
时条件而触发超时。先看一下定时器的操作方法，见表9-5。表9-5定时器的操作方法
┏━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━┓
┃    方法名                                          ┃    参数含义                    ┃    执行意义                        ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃ngx_int_t ngx_event_timer_init                      ┃  log足可以记录日志的ngx_log_t  ┃  初始化定时器                      ┃
┃(ngx_log_t *log);                                   ┃对象                            ┃                                    ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃ngx_msec_t ngx_event_find_timer(void);              ┃  无                            ┃  找出红黑树中最左边的节点，如果    ┃
┃                                                    ┃                                ┃它的超时时间大于当前时间，也就表明  ┃
┃                                                    ┃                                ┃目前的定时器中没有一个事件满足触发  ┃
┃                                                    ┃                                ┃条件，这时返回这个超时与当前时间的  ┃
┃                                     			    ┃                                ┃差值，也就是需要经过多少毫秒会有事  ┃
┃                                                  I ┃                                ┃件超时触发；如果它的超时时间小于或  ┃
┃                                                    ┃                                ┃等于当前时间，则返回0，表示定时器   ┃
┃                                                    ┃                                ┃中已经存在超时需要触发的事件        ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃                                                    ┃                                ┃  检查定时器中的所有事件，按照红    ┃
┃                                                    ┃                                ┃黑树关键字由小到大的顺序依次调用    ┃
┃ngx_event_expire_timers                             ┃  无                            ┃                                    ┃
┃                                                    ┃                                ┃已经满足超时条件需要被触发事件的    ┃
┃                                                    ┃                                ┃handler回调方法                     ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━┛
┏━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━┓
┃    方法名                      ┃    参数含义                    ┃    执行意义                      ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃static ngx_inline void          ┃                                ┃                                  ┃
┃ngx_event_del_timer             ┃  ev是需要操作的事件            ┃  从定时器中移除一个事件          ┃
┃(ngx_event_t ev)                ┃                                ┃                                  ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃static ngx_inline void          ┃  ev是需要操作的事件，timer的   ┃                                  ┃
┃ngx_event_add_timer(ngx_        ┃单位是毫秒，它告诉定时器事件ev  ┃  添加一个定时器事件，超时时间为  ┃
┃                                ┃希望timer毫秒后超时，同时需要   ┃timer毫秒                         ┃
┃event_t *ev, ngx_msec_t timer)  ┃                                ┃                                  ┃
┃                                ┃回调ev的handler方法             ┃                                  ┃
┗━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━┛
    事实上，还有两个宏与ngx_event add—timer方法和ngx_event del timer方法的用法是完全一样的，如下所示。
#define ngx_add_timer ngx_event_add_timer
#define ngx_del_timer ngx_event_del timer
*/
ngx_rbtree_t              ngx_event_timer_rbtree;
static ngx_rbtree_node_t  ngx_event_timer_sentinel;
//哨兵节点是所有最下层的叶子节点都指向一个NULL空节点，图形化参考:http://blog.csdn.net/xzongyuan/article/details/22389185

/*
 * the event timer rbtree may contain the duplicate keys, however,
 * it should not be a problem, because we use the rbtree to find
 * a minimum timer value only
 */
//初始化红黑树实现的定时器。
ngx_int_t
ngx_event_timer_init(ngx_log_t *log)
{
    ngx_rbtree_init(&ngx_event_timer_rbtree, &ngx_event_timer_sentinel,
                    ngx_rbtree_insert_timer_value);

    return NGX_OK;
}

//获取离现在最近的超时定时器时间
ngx_msec_t
ngx_event_find_timer(void)
{
    ngx_msec_int_t      timer;
    ngx_rbtree_node_t  *node, *root, *sentinel;

    if (ngx_event_timer_rbtree.root == &ngx_event_timer_sentinel) {
        return NGX_TIMER_INFINITE;
    }

    root = ngx_event_timer_rbtree.root;
    sentinel = ngx_event_timer_rbtree.sentinel;

    node = ngx_rbtree_min(root, sentinel);

    timer = (ngx_msec_int_t) (node->key - ngx_current_msec);

    return (ngx_msec_t) (timer > 0 ? timer : 0);
}

/*
1.ngx_event_s可以是普通的epoll读写事件(参考ngx_event_connect_peer->ngx_add_conn或者ngx_add_event)，通过读写事件触发

2.也可以是普通定时器事件(参考ngx_cache_manager_process_handler->ngx_add_timer(ngx_event_add_timer))，通过ngx_process_events_and_timers中的
epoll_wait返回，可以是读写事件触发返回，也可能是因为没获取到共享锁，从而等待0.5s返回重新获取锁来跟新事件并执行超时事件来跟新事件并且判断定
时器链表中的超时事件，超时则执行从而指向event的handler，然后进一步指向对应r或者u的->write_event_handler  read_event_handler

3.也可以是利用定时器expirt实现的读写事件(参考ngx_http_set_write_handler->ngx_add_timer(ngx_event_add_timer)),触发过程见2，只是在handler中不会执行write_event_handler  read_event_handler
*/

void
ngx_event_expire_timers(void)
{
    ngx_event_t        *ev;
    ngx_rbtree_node_t  *node, *root, *sentinel;

    sentinel = ngx_event_timer_rbtree.sentinel;

    for ( ;; ) {
        root = ngx_event_timer_rbtree.root;

        if (root == sentinel) {
            return;
        }

        node = ngx_rbtree_min(root, sentinel);

        /* node->key > ngx_current_time */

        if ((ngx_msec_int_t) (node->key - ngx_current_msec) > 0) {
            return;
        }

        ev = (ngx_event_t *) ((char *) node - offsetof(ngx_event_t, timer));

        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                       "event timer del: %d: %M",
                       ngx_event_ident(ev->data), ev->timer.key);

        ngx_rbtree_delete(&ngx_event_timer_rbtree, &ev->timer);

#if (NGX_DEBUG)
        ev->timer.left = NULL;
        ev->timer.right = NULL;
        ev->timer.parent = NULL;
#endif

        ev->timer_set = 0;

        ev->timedout = 1;

        ev->handler(ev); //超时的时候出发读写事件回调函数，从而在里面判断timedout标志位
    }
}


void
ngx_event_cancel_timers(void)
{
    ngx_event_t        *ev;
    ngx_rbtree_node_t  *node, *root, *sentinel;

    sentinel = ngx_event_timer_rbtree.sentinel;

    for ( ;; ) {
        root = ngx_event_timer_rbtree.root;

        if (root == sentinel) {
            return;
        }

        node = ngx_rbtree_min(root, sentinel);

        ev = (ngx_event_t *) ((char *) node - offsetof(ngx_event_t, timer));

        if (!ev->cancelable) {
            return;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_EVENT, ev->log, 0,
                       "event timer cancel: %d: %M",
                       ngx_event_ident(ev->data), ev->timer.key);

        ngx_rbtree_delete(&ngx_event_timer_rbtree, &ev->timer);

#if (NGX_DEBUG)
        ev->timer.left = NULL;
        ev->timer.right = NULL;
        ev->timer.parent = NULL;
#endif

        ev->timer_set = 0;

        ev->handler(ev);
    }
}
