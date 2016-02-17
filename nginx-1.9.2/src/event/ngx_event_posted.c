
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>

/*
    实际上，上述问题的解决离不开Nginx的post事件处理机制。这个post事件是什么意思呢？它表示允许事件延后执行。Nginx设计了两个post队列，一
个是由被触发的监听连接的读事件构成的ngx_posted_accept_events队列，另一个是由普通读／写事件构成的ngx_posted_events队列。这样的post事
件可以让用户完成什么样的功能呢？
   将epoll_wait产生的一批事件，分到这两个队列中，让存放着新连接事件的ngx_posted_accept_events队列优先执行，存放普通事件的ngx_posted_events队
列最后执行，这是解决“惊群”和负载均衡两个问题的关键。如果在处理一个事件的过程中产生了另一个事件，而我们希望这个事件随后执行（不是立刻执行），
就可以把它放到post队列中。
*/
ngx_queue_t  ngx_posted_accept_events; //延后处理的新建连接accept事件
ngx_queue_t  ngx_posted_events; //普通延后连接建立成功后的读写事件

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
//从posted队列中却出所有ev并执行各个事件的handler
void
ngx_event_process_posted(ngx_cycle_t *cycle, ngx_queue_t *posted)
{
    ngx_queue_t  *q;
    ngx_event_t  *ev; 

    while (!ngx_queue_empty(posted)) {

        q = ngx_queue_head(posted);
        ev = ngx_queue_data(q, ngx_event_t, queue);

        ngx_log_debug1(NGX_LOG_DEBUG_EVENT, cycle->log, 0,
                      "begin to run befor posted event %p", ev);

        ngx_delete_posted_event(ev);

        ev->handler(ev);
    }
}
