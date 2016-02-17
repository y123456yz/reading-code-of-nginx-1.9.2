
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * The time may be updated by signal handler or by several threads.
 * The time update operations are rare and require to hold the ngx_time_lock.
 * The time read operations are frequent, so they are lock-free and get time
 * values and strings from the current slot.  Thus thread may get the corrupted
 * values only if it is preempted while copying and then it is not scheduled
 * to run more than NGX_TIME_SLOTS seconds.
 */

#define NGX_TIME_SLOTS   64

/*
nginx这里采用了64个slot时间，也就是每次更新时间的时候都是更新下一个
slot，如果读操作同时进行，读到的还是之前的slot，并没有被改变，当然这里只能是尽量减少了时间混乱的几率，因为slot的个数不是无限的，slot是循环的，
写操作总有几率会写到读操作的slot上。不过nginx现在实际上并没有采用多线程的方式，而且在信号处理中只是更新cached_err_log_time，所以对其他时间变量
的读访问是不会发生混乱的 
*/
static ngx_uint_t        slot;
static ngx_atomic_t      ngx_time_lock;

volatile ngx_msec_t      ngx_current_msec; //格林威治时间1970年1月1日凌晨0点0分0秒到当前时间的毫秒数
volatile ngx_time_t     *ngx_cached_time; //ngx_time_t结构体形式的当前时间
volatile ngx_str_t       ngx_cached_err_log_time; //用于记录error_log的当前时间字符串，它的格式类似于：”1970/09/28  12：OO：OO”

//用于HTTP相关的当前时间字符串，它的格式类似于：”Mon，28  Sep  1970  06：OO：OO  GMT”
volatile ngx_str_t       ngx_cached_http_time;
//用于记录HTTP曰志的当前时间字符串，它的格式类似于：”28/Sep/1970：12：OO：00  +0600n"
volatile ngx_str_t       ngx_cached_http_log_time;
//以IS0 8601标准格式记录下的字符串形式的当前时间
volatile ngx_str_t       ngx_cached_http_log_iso8601;
volatile ngx_str_t       ngx_cached_syslog_time;

#if !(NGX_WIN32)

/*
 * localtime() and localtime_r() are not Async-Signal-Safe functions, therefore,
 * they must not be called by a signal handler, so we use the cached
 * GMT offset value. Fortunately the value is changed only two times a year.
 */

static ngx_int_t         cached_gmtoff;
#endif

static ngx_time_t        cached_time[NGX_TIME_SLOTS]; //系统当前时间，见ngx_time_update
static u_char            cached_err_log_time[NGX_TIME_SLOTS]
                                    [sizeof("1970/09/28 12:00:00")];
static u_char            cached_http_time[NGX_TIME_SLOTS]
                                    [sizeof("Mon, 28 Sep 1970 06:00:00 GMT")]; //年月日 时分秒 星期 格式
static u_char            cached_http_log_time[NGX_TIME_SLOTS]
                                    [sizeof("28/Sep/1970:12:00:00 +0600")];
static u_char            cached_http_log_iso8601[NGX_TIME_SLOTS]
                                    [sizeof("1970-09-28T12:00:00+06:00")];
static u_char            cached_syslog_time[NGX_TIME_SLOTS]
                                    [sizeof("Sep 28 12:00:00")];


static char  *week[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char  *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
/*
表9-4 Nginx缓存时间的操作方法
┏━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓
┃    时间方法名                  ┃    参数含义                      ┃    执行意义                                  ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃void ngx_time_init(void);       ┃    无                            ┃    初始化当前进程中缓存的时间变量，同        ┃
┃                                ┃                                  ┃时会第一次根据gettimeofday调用刷新缓          ┃
┃                                ┃                                  ┃存时间                                        ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃void ngx_time_update(void)      ┃    无                            ┃    使用gettimeofday调用以系统时间            ┃
┃                                ┃                                  ┃更新缓存的时间，上述的ngx_current_            ┃
┃                                ┃                                  ┃msec. ngx_cached time. ngx_cached err         ┃
┃                                ┃                                  ┃ log_time. ngx_cached_http_time. ngx_         ┃
┃                                ┃                                  ┃cached_http_log_time. ngx_cached_http         ┃
┃                                ┃                                  ┃ log_is08601这6个全局变量都会得到更新         ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t是需要转换的时间，它是格     ┃                                              ┃
┃u_char *ngx_http_time           ┃林威治时间1970年1月1日凌晨        ┃    将时间t转换成“Mon, 28 Sep 1970 06:00:00  ┃
┃                                ┃0点O分O秒到某一时间的秒数，       ┃ GMT”形式的时间，返回值与buf是相同           ┃
┃(u_char *buf, time_t t)         ┃                                  ┃的，都是指向存放时间的字符串                  ┃
┃                                ┃buf是t时间转换成字符串形式的      ┃                                              ┃
┃                                ┃r-rrIP时间后用来存放字符串的内存  ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t悬需要转换的时间，它是格     ┃                                              ┃
┃                                ┃林威治时间1970年1月1日凌晨        ┃    将时间t转换成“Mon. 28-Sep-70 06:00:00    ┃
┃u_char *ngx_http_cookie_time    ┃0点0分0秒到某一时间的秒数，       ┃ GMT”形式适用于cookie的时间，返回值          ┃
┃(u_char *buf, time_t t)         ┃buf是t时间转换成字符串形式适      ┃与buf是相同的，都是指向存放时间的字           ┃
┃                                ┃用于cookie的时间后用来存放字      ┃符串                                          ┃
┃                                ┃符串的内存                        ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t是需要转换的时间，它是格林   ┃                                              ┃
┃void ngx_gmtime                 ┃威治时间1970年1月1日凌晨O         ┃    将时间t转换成ngx_tm_t类型的时间。         ┃
┃                                ┃点0分0秒到某一时间的秒数，tp      ┃                                              ┃
┃(time_t t, ngx_tm_t *tp)        ┃                                  ┃下面会说明ngx_tm_t类型                        ┃
┃                                ┃是ngx_tm_t类型的时间，实际上      ┃                                              ┃
┃                                ┃就是标准的tm类型时间              ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃                                  ┃    返回一1表示失败，否则会返回：①如         ┃
┃                                ┃                                  ┃果when表示当天时间秒数，当它合并到            ┃
┃                                ┃                                  ┃实际时间后，已经超过当前时间，那么就          ┃
┃                                ┃                                  ┃返回when合并到实际时间后的秒数（相            ┃
┃time_t ngx_next_time            ┃    when表不期待过期的时间，它    ┃对于格林威治时间1970年1月1日凌晨O             ┃
┃(time_t when)    :              ┃仅表示一天内的秒数                ┃点O分O秒到某一时间的耖数）；                  ┃
┃                                ┃                                  ┃  ②反之，如果合并后的时间早于当前            ┃
┃                                ┃                                  ┃时间，则返回下一天的同一时刻（当天时          ┃
┃                                ┃                                  ┃刻）的时间。它目前仅具有与expires配置         ┃
┃                                ┃                                  ┃项相关的缓存过期功能                          ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃#define ngx_time                ┃    无                            ┃    获取到格林威治时间1970年1月1日            ┃
┃ngx_cached_time->sec            ┃                                  ┃凌晨0点0分0秒到当前时间的秒数                 ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃#define ngx_timeofday           ┃    无                            ┃    获取缓存的ngx_time_t类型时间              ┃
┃(ngx_time_t *) ngxLcached_time  ┃                                  ┃                                              ┃
┗━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
*/

void
ngx_time_init(void) //初始化nginx环境的当前时间
{
    ngx_cached_err_log_time.len = sizeof("1970/09/28 12:00:00") - 1;
    ngx_cached_http_time.len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1;
    ngx_cached_http_log_time.len = sizeof("28/Sep/1970:12:00:00 +0600") - 1;
    ngx_cached_http_log_iso8601.len = sizeof("1970-09-28T12:00:00+06:00") - 1;
    ngx_cached_syslog_time.len = sizeof("Sep 28 12:00:00") - 1;

    ngx_cached_time = &cached_time[0];

    ngx_time_update();
}
/*
表9-4 Nginx缓存时间的操作方法
┏━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓
┃    时间方法名                  ┃    参数含义                      ┃    执行意义                                  ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃void ngx_time_init(void);       ┃    无                            ┃    初始化当前进程中缓存的时间变量，同        ┃
┃                                ┃                                  ┃时会第一次根据gettimeofday调用刷新缓          ┃
┃                                ┃                                  ┃存时间                                        ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃void ngx_time_update(void)      ┃    无                            ┃    使用gettimeofday调用以系统时间            ┃
┃                                ┃                                  ┃更新缓存的时间，上述的ngx_current_            ┃
┃                                ┃                                  ┃msec. ngx_cached time. ngx_cached err         ┃
┃                                ┃                                  ┃ log_time. ngx_cached_http_time. ngx_         ┃
┃                                ┃                                  ┃cached_http_log_time. ngx_cached_http         ┃
┃                                ┃                                  ┃ log_is08601这6个全局变量都会得到更新         ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t是需要转换的时间，它是格     ┃                                              ┃
┃u_char *ngx_http_time           ┃林威治时间1970年1月1日凌晨        ┃    将时间t转换成“Mon, 28 Sep 1970 06:00:00  ┃
┃                                ┃0点O分O秒到某一时间的秒数，       ┃ GMT”形式的时间，返回值与buf是相同           ┃
┃(u_char *buf, time_t t)         ┃                                  ┃的，都是指向存放时间的字符串                  ┃
┃                                ┃buf是t时间转换成字符串形式的      ┃                                              ┃
┃                                ┃r-rrIP时间后用来存放字符串的内存  ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t悬需要转换的时间，它是格     ┃                                              ┃
┃                                ┃林威治时间1970年1月1日凌晨        ┃    将时间t转换成“Mon. 28-Sep-70 06:00:00    ┃
┃u_char *ngx_http_cookie_time    ┃0点0分0秒到某一时间的秒数，       ┃ GMT”形式适用于cookie的时间，返回值          ┃
┃(u_char *buf, time_t t)         ┃buf是t时间转换成字符串形式适      ┃与buf是相同的，都是指向存放时间的字           ┃
┃                                ┃用于cookie的时间后用来存放字      ┃符串                                          ┃
┃                                ┃符串的内存                        ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t是需要转换的时间，它是格林   ┃                                              ┃
┃void ngx_gmtime                 ┃威治时间1970年1月1日凌晨O         ┃    将时间t转换成ngx_tm_t类型的时间。         ┃
┃                                ┃点0分0秒到某一时间的秒数，tp      ┃                                              ┃
┃(time_t t, ngx_tm_t *tp)        ┃                                  ┃下面会说明ngx_tm_t类型                        ┃
┃                                ┃是ngx_tm_t类型的时间，实际上      ┃                                              ┃
┃                                ┃就是标准的tm类型时间              ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃                                  ┃    返回一1表示失败，否则会返回：①如         ┃
┃                                ┃                                  ┃果when表示当天时间秒数，当它合并到            ┃
┃                                ┃                                  ┃实际时间后，已经超过当前时间，那么就          ┃
┃                                ┃                                  ┃返回when合并到实际时间后的秒数（相            ┃
┃time_t ngx_next_time            ┃    when表不期待过期的时间，它    ┃对于格林威治时间1970年1月1日凌晨O             ┃
┃(time_t when)    :              ┃仅表示一天内的秒数                ┃点O分O秒到某一时间的耖数）；                  ┃
┃                                ┃                                  ┃  ②反之，如果合并后的时间早于当前            ┃
┃                                ┃                                  ┃时间，则返回下一天的同一时刻（当天时          ┃
┃                                ┃                                  ┃刻）的时间。它目前仅具有与expires配置         ┃
┃                                ┃                                  ┃项相关的缓存过期功能                          ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃#define ngx_time                ┃    无                            ┃    获取到格林威治时间1970年1月1日            ┃
┃ngx_cached_time->sec            ┃                                  ┃凌晨0点0分0秒到当前时间的秒数                 ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃#define ngx_timeofday           ┃    无                            ┃    获取缓存的ngx_time_t类型时间              ┃
┃(ngx_time_t *) ngxLcached_time  ┃                                  ┃                                              ┃
┗━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
*/

//ngx_time_update（）函数在master进程中的ngx_master_process_cycle（）主循环中被调用，具体位置为sigsuspend（）函数之后，也就是说master进程捕捉到并处理完一个信号返回的时候会更新时间缓存
/*
更新时间缓存
为避免每次都调用OS的gettimeofday，nginx采用时间缓存，每个worker进程都能自行维护；为控制并发访问，每次更新时间缓存前需申请锁，而读时间缓存无须加锁；
为避免分裂读，即某worker进程读时间缓存过程中接受中断请求，期间时间缓存被其他worker更新，导致前后读取时间不一致；nginx引入时间缓存数组(共64个成员)，每次都更新数组中的下一个元素；
更新时间通过ngx_time_update()实现
ngx_time_update()调用最频繁的是在worker进程处理事件时
ngx_worker_process_cycle -- ngx_process_events_and_timers -- ngx_process_events
#define ngx_process_events  ngx_event_actions.process_events
以epoll为例，其对应API为ngx_epoll_process_events
ngx_epoll_process_events(ngx_cycle_t *cycle, ngx_msec_t timer, ngx_uint_t flags)
    events = epoll_wait(ep, event_list, (int) nevents, timer); 
    err = (events == -1) ? ngx_errno : 0;
    if (flags & NGX_UPDATE_TIME || ngx_event_timer_alarm) {  
        ngx_time_update();
    }

nginx使用了原子变量ngx_time_lock来对时间变量进行写加锁，而且nginx考虑到读时间的操作比较多，出于性能的原因没有对读进行加锁，而是采用维护多个时间
slot的方式来尽量减少读访问冲突，基本原理就是，当读操作和写操作同时发生时（1，多线程时可能发生；2，当进程正在读时间缓存时，被一信号中断去执行
信号处理函数，信号处理函数中会更新时间缓存），也就是读操作正在进行时（比如刚拷贝完ngx_cached_time->sec，或者拷贝ngx_cached_http_time.data进行
到一半时），如果写操作改变了读操作的时间，读操作最终得到的时间就变混乱了。nginx这里采用了64个slot时间，也就是每次更新时间的时候都是更新下一个
slot，如果读操作同时进行，读到的还是之前的slot，并没有被改变，当然这里只能是尽量减少了时间混乱的几率，因为slot的个数不是无限的，slot是循环的，
写操作总有几率会写到读操作的slot上。不过nginx现在实际上并没有采用多线程的方式，而且在信号处理中只是更新cached_err_log_time，所以对其他时间变量
的读访问是不会发生混乱的。 另一个地方是两个函数中都调用了 ngx_memory_barrier() ，实际上这个也是一个宏，它的具体定义和编译器及体系结构有关，gcc
和x86环境下，定义如下：
#define ngx_memory_barrier()    __asm__ volatile ("" ::: "memory")
它的作用实际上还是和防止读操作混乱有关，它告诉编译器不要将其后面的语句进行优化，不要打乱其执行顺序
*/

/*
这个缓存时间什么时候会更新呢？对于worker进程而言，除了Nginx启动时更新一次时间外，任何更新时间的操作都只能由ngx_epoll_process_events方法
执行。回顾一下ngx_epoll_process_events方法的代码，当flags参数中有NGX_UPDATE_TIME标志位，或者ngx_event_timer_alarm标志
位为1时，就会调用ngx_time_update方法更新缓存时间。
*/ //如果没有设置timer_resolution则定时器可能永远不超时，因为epoll_wait不返回，无法更新时间
void
ngx_time_update(void)
{
    u_char          *p0, *p1, *p2, *p3, *p4;
    ngx_tm_t         tm, gmt;
    time_t           sec;
    ngx_uint_t       msec;
    ngx_time_t      *tp;
    struct timeval   tv;

    if (!ngx_trylock(&ngx_time_lock)) {
        return;
    }

    ngx_gettimeofday(&tv);

    sec = tv.tv_sec;
    msec = tv.tv_usec / 1000;

    ngx_current_msec = (ngx_msec_t) sec * 1000 + msec;

    tp = &cached_time[slot];//读当前时间缓存 

    if (tp->sec == sec) {//如果缓存的时间秒=当前时间秒，直接更新当前slot元素的msec并返回，否则更新下一个slot数组元素；
        tp->msec = msec;
        ngx_unlock(&ngx_time_lock);
        return;
    }

    if (slot == NGX_TIME_SLOTS - 1) {
        slot = 0;
    } else {
        slot++;
    }

    tp = &cached_time[slot];

    tp->sec = sec;
    tp->msec = msec;

    ngx_gmtime(sec, &gmt);


    p0 = &cached_http_time[slot][0];

    (void) ngx_sprintf(p0, "%s, %02d %s %4d %02d:%02d:%02d GMT",
                       week[gmt.ngx_tm_wday], gmt.ngx_tm_mday,
                       months[gmt.ngx_tm_mon - 1], gmt.ngx_tm_year,
                       gmt.ngx_tm_hour, gmt.ngx_tm_min, gmt.ngx_tm_sec);

#if (NGX_HAVE_GETTIMEZONE)

    tp->gmtoff = ngx_gettimezone();
    ngx_gmtime(sec + tp->gmtoff * 60, &tm);

#elif (NGX_HAVE_GMTOFF)

    ngx_localtime(sec, &tm);
    cached_gmtoff = (ngx_int_t) (tm.ngx_tm_gmtoff / 60);
    tp->gmtoff = cached_gmtoff;

#else

    ngx_localtime(sec, &tm);
    cached_gmtoff = ngx_timezone(tm.ngx_tm_isdst);
    tp->gmtoff = cached_gmtoff;

#endif


    p1 = &cached_err_log_time[slot][0];

    (void) ngx_sprintf(p1, "%4d/%02d/%02d %02d:%02d:%02d",
                       tm.ngx_tm_year, tm.ngx_tm_mon,
                       tm.ngx_tm_mday, tm.ngx_tm_hour,
                       tm.ngx_tm_min, tm.ngx_tm_sec);


    p2 = &cached_http_log_time[slot][0];

    (void) ngx_sprintf(p2, "%02d/%s/%d:%02d:%02d:%02d %c%02d%02d",
                       tm.ngx_tm_mday, months[tm.ngx_tm_mon - 1],
                       tm.ngx_tm_year, tm.ngx_tm_hour,
                       tm.ngx_tm_min, tm.ngx_tm_sec,
                       tp->gmtoff < 0 ? '-' : '+',
                       ngx_abs(tp->gmtoff / 60), ngx_abs(tp->gmtoff % 60));

    p3 = &cached_http_log_iso8601[slot][0];

    (void) ngx_sprintf(p3, "%4d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
                       tm.ngx_tm_year, tm.ngx_tm_mon,
                       tm.ngx_tm_mday, tm.ngx_tm_hour,
                       tm.ngx_tm_min, tm.ngx_tm_sec,
                       tp->gmtoff < 0 ? '-' : '+',
                       ngx_abs(tp->gmtoff / 60), ngx_abs(tp->gmtoff % 60));

    p4 = &cached_syslog_time[slot][0];

    (void) ngx_sprintf(p4, "%s %2d %02d:%02d:%02d",
                       months[tm.ngx_tm_mon - 1], tm.ngx_tm_mday,
                       tm.ngx_tm_hour, tm.ngx_tm_min, tm.ngx_tm_sec);


      /*
    可以看到ngx_memory_barrier()之后是四条赋值语句，如果没有 ngx_memory_barrier()，编译器可能会将 ngx_cached_time = tp ，
    ngx_cached_http_time.data = p0，ngx_cached_err_log_time.data = p1， ngx_cached_http_log_time.data = p2 分别和之前的 
    tp = &cached_time[slot] , p0 = &cached_http_time[slot][0] , p1 = &cached_err_log_time[slot][0] , p2 = &cached_http_log_time[slot][0] 
    合并优化掉，这样的后果是 ngx_cached_time，ngx_cached_http_time，ngx_cached_err_log_time， ngx_cached_http_log_time这四个时间缓存的
    不一致性时长增大了，因为在最后一个ngx_sprintf执行完后这四个时间缓存才一致，在这之前如果有其他地方正在读时间缓存就可能导致读到的时间
    不正确或者不一致，而采用ngx_memory_barrier() 后，时间缓存更新到一致的 状态只需要几个时钟周期，因为只有四条赋值指令，显然在这么短的时
    间内发生读时间缓存的概率会小的多了。从这里可以看出Igor考虑是非常细致的。 
    */
    ngx_memory_barrier();//禁止编译器对后面的语句优化，如果没有这个限制，编译器可能将前后两部分代码合并，可能导致这6个时间更新出现间隔，期间若被读取会出现时间不一致的情况 

    ngx_cached_time = tp;
    ngx_cached_http_time.data = p0;
    ngx_cached_err_log_time.data = p1;
    ngx_cached_http_log_time.data = p2;
    ngx_cached_http_log_iso8601.data = p3;
    ngx_cached_syslog_time.data = p4;

    ngx_unlock(&ngx_time_lock);
}

/*
nginx出于性能考虑采用类似lib_event的方式，自己对时间进行了cache，用来减少对gettimeofday（）的调用，因为一般来说服务
器对时间的精度要求不是特别的高，不过如果需要比较精确的timer，nginx还提供了一个timer_resolution指令用来设置时间精度，
具体的机制再后面会做介绍
*/
/*
ngx_time_update（）和ngx_time_sigsafe_update（）这两个函数的实现比较简单，但是还是有几个值得注意的地方，首先由于时间可能在信号处理中被更新，
另外多线程的时候也可能同时更新时间（nginx现在虽然没有开放多线程，但是代码中有考虑），nginx使用了原子变量ngx_time_lock来对时间变量进行写加锁，
而且nginx考虑到读时间的操作比较多，出于性能的原因没有对读进行加锁，而是采用维护多个时间slot的方式来尽量减少读访问冲突，基本原理就是，当读操作
和写操作同时发生时（1，多线程时可能发生；2，当进程正在读时间缓存时，被一信号中断去执行信号处理函数，信号处理函数中会更新时间缓存），也就是读
操作正在进行时（比如刚拷贝完ngx_cached_time->sec，或者拷贝ngx_cached_http_time.data进行到一半时），如果写操作改变了读操作的时间，读操作最终得
到的时间就变混乱了。nginx这里采用了64个slot时间，也就是每次更新时间的时候都是更新下一个slot，如果读操作同时进行，读到的还是之前的slot，并没有
被改变，当然这里只能是尽量减少了时间混乱的几率，因为slot的个数不是无限的，slot是循环的，写操作总有几率会写到读操作的slot上。不过nginx现在实际
上并没有采用多线程的方式，而且在信号处理中只是更新cached_err_log_time，所以对其他时间变量的读访问是不会发生混乱的。 另一个地方是两个函数中都
调用了 ngx_memory_barrier() ，实际上这个也是一个宏，它的具体定义和编译器及体系结构有关，gcc和x86环境下，定义如下：
*/
#if !(NGX_WIN32)
//它会在每次执行信号处理函数的时候被调用，也就是在ngx_signal_handler（）函数中。
void
ngx_time_sigsafe_update(void)
{
    u_char          *p, *p2;
    ngx_tm_t         tm;
    time_t           sec;
    ngx_time_t      *tp;
    struct timeval   tv;

    if (!ngx_trylock(&ngx_time_lock)) {
        return;
    }

    ngx_gettimeofday(&tv);

    sec = tv.tv_sec;

    tp = &cached_time[slot];

    if (tp->sec == sec) {
        ngx_unlock(&ngx_time_lock);
        return;
    }

    if (slot == NGX_TIME_SLOTS - 1) {
        slot = 0;
    } else {
        slot++;
    }

    tp = &cached_time[slot];

    tp->sec = 0;

    ngx_gmtime(sec + cached_gmtoff * 60, &tm);

    p = &cached_err_log_time[slot][0];

    (void) ngx_sprintf(p, "%4d/%02d/%02d %02d:%02d:%02d",
                       tm.ngx_tm_year, tm.ngx_tm_mon,
                       tm.ngx_tm_mday, tm.ngx_tm_hour,
                       tm.ngx_tm_min, tm.ngx_tm_sec);

    p2 = &cached_syslog_time[slot][0];

    (void) ngx_sprintf(p2, "%s %2d %02d:%02d:%02d",
                       months[tm.ngx_tm_mon - 1], tm.ngx_tm_mday,
                       tm.ngx_tm_hour, tm.ngx_tm_min, tm.ngx_tm_sec);


    /*
    可以看到ngx_memory_barrier()之后是四条赋值语句，如果没有 ngx_memory_barrier()，编译器可能会将 ngx_cached_time = tp ，
    ngx_cached_http_time.data = p0，ngx_cached_err_log_time.data = p1， ngx_cached_http_log_time.data = p2 分别和之前的 
    tp = &cached_time[slot] , p0 = &cached_http_time[slot][0] , p1 = &cached_err_log_time[slot][0] , p2 = &cached_http_log_time[slot][0] 
    合并优化掉，这样的后果是 ngx_cached_time，ngx_cached_http_time，ngx_cached_err_log_time， ngx_cached_http_log_time这四个时间缓存的
    不一致性时长增大了，因为在最后一个ngx_sprintf执行完后这四个时间缓存才一致，在这之前如果有其他地方正在读时间缓存就可能导致读到的时间
    不正确或者不一致，而采用ngx_memory_barrier() 后，时间缓存更新到一致的 状态只需要几个时钟周期，因为只有四条赋值指令，显然在这么短的时
    间内发生读时间缓存的概率会小的多了。从这里可以看出Igor考虑是非常细致的。 
    */
    ngx_memory_barrier();

    //在信号处理中只是更新cached_err_log_time ngx_cached_syslog_time，所以对其他时间变量的读访问是不会发生混乱的
    ngx_cached_err_log_time.data = p;
    ngx_cached_syslog_time.data = p2;

    ngx_unlock(&ngx_time_lock);
}

#endif

/*
表9-4 Nginx缓存时间的操作方法
┏━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓
┃    时间方法名                  ┃    参数含义                      ┃    执行意义                                  ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃void ngx_time_init(void);       ┃    无                            ┃    初始化当前进程中缓存的时间变量，同        ┃
┃                                ┃                                  ┃时会第一次根据gettimeofday调用刷新缓          ┃
┃                                ┃                                  ┃存时间                                        ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃void ngx_time_update(void)      ┃    无                            ┃    使用gettimeofday调用以系统时间            ┃
┃                                ┃                                  ┃更新缓存的时间，上述的ngx_current_            ┃
┃                                ┃                                  ┃msec. ngx_cached time. ngx_cached err         ┃
┃                                ┃                                  ┃ log_time. ngx_cached_http_time. ngx_         ┃
┃                                ┃                                  ┃cached_http_log_time. ngx_cached_http         ┃
┃                                ┃                                  ┃ log_is08601这6个全局变量都会得到更新         ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t是需要转换的时间，它是格     ┃                                              ┃
┃u_char *ngx_http_time           ┃林威治时间1970年1月1日凌晨        ┃    将时间t转换成“Mon, 28 Sep 1970 06:00:00  ┃
┃                                ┃0点O分O秒到某一时间的秒数，       ┃ GMT”形式的时间，返回值与buf是相同           ┃
┃(u_char *buf, time_t t)         ┃                                  ┃的，都是指向存放时间的字符串                  ┃
┃                                ┃buf是t时间转换成字符串形式的      ┃                                              ┃
┃                                ┃r-rrIP时间后用来存放字符串的内存  ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t悬需要转换的时间，它是格     ┃                                              ┃
┃                                ┃林威治时间1970年1月1日凌晨        ┃    将时间t转换成“Mon. 28-Sep-70 06:00:00    ┃
┃u_char *ngx_http_cookie_time    ┃0点0分0秒到某一时间的秒数，       ┃ GMT”形式适用于cookie的时间，返回值          ┃
┃(u_char *buf, time_t t)         ┃buf是t时间转换成字符串形式适      ┃与buf是相同的，都是指向存放时间的字           ┃
┃                                ┃用于cookie的时间后用来存放字      ┃符串                                          ┃
┃                                ┃符串的内存                        ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t是需要转换的时间，它是格林   ┃                                              ┃
┃void ngx_gmtime                 ┃威治时间1970年1月1日凌晨O         ┃    将时间t转换成ngx_tm_t类型的时间。         ┃
┃                                ┃点0分0秒到某一时间的秒数，tp      ┃                                              ┃
┃(time_t t, ngx_tm_t *tp)        ┃                                  ┃下面会说明ngx_tm_t类型                        ┃
┃                                ┃是ngx_tm_t类型的时间，实际上      ┃                                              ┃
┃                                ┃就是标准的tm类型时间              ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃                                  ┃    返回一1表示失败，否则会返回：①如         ┃
┃                                ┃                                  ┃果when表示当天时间秒数，当它合并到            ┃
┃                                ┃                                  ┃实际时间后，已经超过当前时间，那么就          ┃
┃                                ┃                                  ┃返回when合并到实际时间后的秒数（相            ┃
┃time_t ngx_next_time            ┃    when表不期待过期的时间，它    ┃对于格林威治时间1970年1月1日凌晨O             ┃
┃(time_t when)    :              ┃仅表示一天内的秒数                ┃点O分O秒到某一时间的耖数）；                  ┃
┃                                ┃                                  ┃  ②反之，如果合并后的时间早于当前            ┃
┃                                ┃                                  ┃时间，则返回下一天的同一时刻（当天时          ┃
┃                                ┃                                  ┃刻）的时间。它目前仅具有与expires配置         ┃
┃                                ┃                                  ┃项相关的缓存过期功能                          ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃#define ngx_time                ┃    无                            ┃    获取到格林威治时间1970年1月1日            ┃
┃ngx_cached_time->sec            ┃                                  ┃凌晨0点0分0秒到当前时间的秒数                 ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃#define ngx_timeofday           ┃    无                            ┃    获取缓存的ngx_time_t类型时间              ┃
┃(ngx_time_t *) ngxLcached_time  ┃                                  ┃                                              ┃
┗━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
*/

u_char *
ngx_http_time(u_char *buf, time_t t)
{
    ngx_tm_t  tm;

    ngx_gmtime(t, &tm);

    return ngx_sprintf(buf, "%s, %02d %s %4d %02d:%02d:%02d GMT",
                       week[tm.ngx_tm_wday],
                       tm.ngx_tm_mday,
                       months[tm.ngx_tm_mon - 1],
                       tm.ngx_tm_year,
                       tm.ngx_tm_hour,
                       tm.ngx_tm_min,
                       tm.ngx_tm_sec);
}

/*
表9-4 Nginx缓存时间的操作方法
┏━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓
┃    时间方法名                  ┃    参数含义                      ┃    执行意义                                  ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃void ngx_time_init(void);       ┃    无                            ┃    初始化当前进程中缓存的时间变量，同        ┃
┃                                ┃                                  ┃时会第一次根据gettimeofday调用刷新缓          ┃
┃                                ┃                                  ┃存时间                                        ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃void ngx_time_update(void)      ┃    无                            ┃    使用gettimeofday调用以系统时间            ┃
┃                                ┃                                  ┃更新缓存的时间，上述的ngx_current_            ┃
┃                                ┃                                  ┃msec. ngx_cached time. ngx_cached err         ┃
┃                                ┃                                  ┃ log_time. ngx_cached_http_time. ngx_         ┃
┃                                ┃                                  ┃cached_http_log_time. ngx_cached_http         ┃
┃                                ┃                                  ┃ log_is08601这6个全局变量都会得到更新         ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t是需要转换的时间，它是格     ┃                                              ┃
┃u_char *ngx_http_time           ┃林威治时间1970年1月1日凌晨        ┃    将时间t转换成“Mon, 28 Sep 1970 06:00:00  ┃
┃                                ┃0点O分O秒到某一时间的秒数，       ┃ GMT”形式的时间，返回值与buf是相同           ┃
┃(u_char *buf, time_t t)         ┃                                  ┃的，都是指向存放时间的字符串                  ┃
┃                                ┃buf是t时间转换成字符串形式的      ┃                                              ┃
┃                                ┃r-rrIP时间后用来存放字符串的内存  ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t悬需要转换的时间，它是格     ┃                                              ┃
┃                                ┃林威治时间1970年1月1日凌晨        ┃    将时间t转换成“Mon. 28-Sep-70 06:00:00    ┃
┃u_char *ngx_http_cookie_time    ┃0点0分0秒到某一时间的秒数，       ┃ GMT”形式适用于cookie的时间，返回值          ┃
┃(u_char *buf, time_t t)         ┃buf是t时间转换成字符串形式适      ┃与buf是相同的，都是指向存放时间的字           ┃
┃                                ┃用于cookie的时间后用来存放字      ┃符串                                          ┃
┃                                ┃符串的内存                        ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t是需要转换的时间，它是格林   ┃                                              ┃
┃void ngx_gmtime                 ┃威治时间1970年1月1日凌晨O         ┃    将时间t转换成ngx_tm_t类型的时间。         ┃
┃                                ┃点0分0秒到某一时间的秒数，tp      ┃                                              ┃
┃(time_t t, ngx_tm_t *tp)        ┃                                  ┃下面会说明ngx_tm_t类型                        ┃
┃                                ┃是ngx_tm_t类型的时间，实际上      ┃                                              ┃
┃                                ┃就是标准的tm类型时间              ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃                                  ┃    返回一1表示失败，否则会返回：①如         ┃
┃                                ┃                                  ┃果when表示当天时间秒数，当它合并到            ┃
┃                                ┃                                  ┃实际时间后，已经超过当前时间，那么就          ┃
┃                                ┃                                  ┃返回when合并到实际时间后的秒数（相            ┃
┃time_t ngx_next_time            ┃    when表不期待过期的时间，它    ┃对于格林威治时间1970年1月1日凌晨O             ┃
┃(time_t when)    :              ┃仅表示一天内的秒数                ┃点O分O秒到某一时间的耖数）；                  ┃
┃                                ┃                                  ┃  ②反之，如果合并后的时间早于当前            ┃
┃                                ┃                                  ┃时间，则返回下一天的同一时刻（当天时          ┃
┃                                ┃                                  ┃刻）的时间。它目前仅具有与expires配置         ┃
┃                                ┃                                  ┃项相关的缓存过期功能                          ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃#define ngx_time                ┃    无                            ┃    获取到格林威治时间1970年1月1日            ┃
┃ngx_cached_time->sec            ┃                                  ┃凌晨0点0分0秒到当前时间的秒数                 ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃#define ngx_timeofday           ┃    无                            ┃    获取缓存的ngx_time_t类型时间              ┃
┃(ngx_time_t *) ngxLcached_time  ┃                                  ┃                                              ┃
┗━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
*/

u_char *
ngx_http_cookie_time(u_char *buf, time_t t)
{
    ngx_tm_t  tm;

    ngx_gmtime(t, &tm);

    /*
     * Netscape 3.x does not understand 4-digit years at all and
     * 2-digit years more than "37"
     */

    return ngx_sprintf(buf,
                       (tm.ngx_tm_year > 2037) ?
                                         "%s, %02d-%s-%d %02d:%02d:%02d GMT":
                                         "%s, %02d-%s-%02d %02d:%02d:%02d GMT",
                       week[tm.ngx_tm_wday],
                       tm.ngx_tm_mday,
                       months[tm.ngx_tm_mon - 1],
                       (tm.ngx_tm_year > 2037) ? tm.ngx_tm_year:
                                                 tm.ngx_tm_year % 100,
                       tm.ngx_tm_hour,
                       tm.ngx_tm_min,
                       tm.ngx_tm_sec);
}

/*
表9-4 Nginx缓存时间的操作方法
┏━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓
┃    时间方法名                  ┃    参数含义                      ┃    执行意义                                  ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃void ngx_time_init(void);       ┃    无                            ┃    初始化当前进程中缓存的时间变量，同        ┃
┃                                ┃                                  ┃时会第一次根据gettimeofday调用刷新缓          ┃
┃                                ┃                                  ┃存时间                                        ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃void ngx_time_update(void)      ┃    无                            ┃    使用gettimeofday调用以系统时间            ┃
┃                                ┃                                  ┃更新缓存的时间，上述的ngx_current_            ┃
┃                                ┃                                  ┃msec. ngx_cached time. ngx_cached err         ┃
┃                                ┃                                  ┃ log_time. ngx_cached_http_time. ngx_         ┃
┃                                ┃                                  ┃cached_http_log_time. ngx_cached_http         ┃
┃                                ┃                                  ┃ log_is08601这6个全局变量都会得到更新         ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t是需要转换的时间，它是格     ┃                                              ┃
┃u_char *ngx_http_time           ┃林威治时间1970年1月1日凌晨        ┃    将时间t转换成“Mon, 28 Sep 1970 06:00:00  ┃
┃                                ┃0点O分O秒到某一时间的秒数，       ┃ GMT”形式的时间，返回值与buf是相同           ┃
┃(u_char *buf, time_t t)         ┃                                  ┃的，都是指向存放时间的字符串                  ┃
┃                                ┃buf是t时间转换成字符串形式的      ┃                                              ┃
┃                                ┃r-rrIP时间后用来存放字符串的内存  ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t悬需要转换的时间，它是格     ┃                                              ┃
┃                                ┃林威治时间1970年1月1日凌晨        ┃    将时间t转换成“Mon. 28-Sep-70 06:00:00    ┃
┃u_char *ngx_http_cookie_time    ┃0点0分0秒到某一时间的秒数，       ┃ GMT”形式适用于cookie的时间，返回值          ┃
┃(u_char *buf, time_t t)         ┃buf是t时间转换成字符串形式适      ┃与buf是相同的，都是指向存放时间的字           ┃
┃                                ┃用于cookie的时间后用来存放字      ┃符串                                          ┃
┃                                ┃符串的内存                        ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t是需要转换的时间，它是格林   ┃                                              ┃
┃void ngx_gmtime                 ┃威治时间1970年1月1日凌晨O         ┃    将时间t转换成ngx_tm_t类型的时间。         ┃
┃                                ┃点0分0秒到某一时间的秒数，tp      ┃                                              ┃
┃(time_t t, ngx_tm_t *tp)        ┃                                  ┃下面会说明ngx_tm_t类型                        ┃
┃                                ┃是ngx_tm_t类型的时间，实际上      ┃                                              ┃
┃                                ┃就是标准的tm类型时间              ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃                                  ┃    返回一1表示失败，否则会返回：①如         ┃
┃                                ┃                                  ┃果when表示当天时间秒数，当它合并到            ┃
┃                                ┃                                  ┃实际时间后，已经超过当前时间，那么就          ┃
┃                                ┃                                  ┃返回when合并到实际时间后的秒数（相            ┃
┃time_t ngx_next_time            ┃    when表不期待过期的时间，它    ┃对于格林威治时间1970年1月1日凌晨O             ┃
┃(time_t when)    :              ┃仅表示一天内的秒数                ┃点O分O秒到某一时间的耖数）；                  ┃
┃                                ┃                                  ┃  ②反之，如果合并后的时间早于当前            ┃
┃                                ┃                                  ┃时间，则返回下一天的同一时刻（当天时          ┃
┃                                ┃                                  ┃刻）的时间。它目前仅具有与expires配置         ┃
┃                                ┃                                  ┃项相关的缓存过期功能                          ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃#define ngx_time                ┃    无                            ┃    获取到格林威治时间1970年1月1日            ┃
┃ngx_cached_time->sec            ┃                                  ┃凌晨0点0分0秒到当前时间的秒数                 ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃#define ngx_timeofday           ┃    无                            ┃    获取缓存的ngx_time_t类型时间              ┃
┃(ngx_time_t *) ngxLcached_time  ┃                                  ┃                                              ┃
┗━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
*/

//把time_t时间转换为tm时间
void
ngx_gmtime(time_t t, ngx_tm_t *tp)
{
    ngx_int_t   yday;
    ngx_uint_t  n, sec, min, hour, mday, mon, year, wday, days, leap;

    /* the calculation is valid for positive time_t only */

    n = (ngx_uint_t) t;

    days = n / 86400;

    /* January 1, 1970 was Thursday */

    wday = (4 + days) % 7;

    n %= 86400;
    hour = n / 3600;
    n %= 3600;
    min = n / 60;
    sec = n % 60;

    /*
     * the algorithm based on Gauss' formula,
     * see src/http/ngx_http_parse_time.c
     */

    /* days since March 1, 1 BC */
    days = days - (31 + 28) + 719527;

    /*
     * The "days" should be adjusted to 1 only, however, some March 1st's go
     * to previous year, so we adjust them to 2.  This causes also shift of the
     * last February days to next year, but we catch the case when "yday"
     * becomes negative.
     */

    year = (days + 2) * 400 / (365 * 400 + 100 - 4 + 1);

    yday = days - (365 * year + year / 4 - year / 100 + year / 400);

    if (yday < 0) {
        leap = (year % 4 == 0) && (year % 100 || (year % 400 == 0));
        yday = 365 + leap + yday;
        year--;
    }

    /*
     * The empirical formula that maps "yday" to month.
     * There are at least 10 variants, some of them are:
     *     mon = (yday + 31) * 15 / 459
     *     mon = (yday + 31) * 17 / 520
     *     mon = (yday + 31) * 20 / 612
     */

    mon = (yday + 31) * 10 / 306;

    /* the Gauss' formula that evaluates days before the month */

    mday = yday - (367 * mon / 12 - 30) + 1;

    if (yday >= 306) {

        year++;
        mon -= 10;

        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday -= 306;
         */

    } else {

        mon += 2;

        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday += 31 + 28 + leap;
         */
    }

    tp->ngx_tm_sec = (ngx_tm_sec_t) sec;
    tp->ngx_tm_min = (ngx_tm_min_t) min;
    tp->ngx_tm_hour = (ngx_tm_hour_t) hour;
    tp->ngx_tm_mday = (ngx_tm_mday_t) mday;
    tp->ngx_tm_mon = (ngx_tm_mon_t) mon;
    tp->ngx_tm_year = (ngx_tm_year_t) year;
    tp->ngx_tm_wday = (ngx_tm_wday_t) wday;
}

/*
表9-4 Nginx缓存时间的操作方法
┏━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓
┃    时间方法名                  ┃    参数含义                      ┃    执行意义                                  ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃void ngx_time_init(void);       ┃    无                            ┃    初始化当前进程中缓存的时间变量，同        ┃
┃                                ┃                                  ┃时会第一次根据gettimeofday调用刷新缓          ┃
┃                                ┃                                  ┃存时间                                        ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃void ngx_time_update(void)      ┃    无                            ┃    使用gettimeofday调用以系统时间            ┃
┃                                ┃                                  ┃更新缓存的时间，上述的ngx_current_            ┃
┃                                ┃                                  ┃msec. ngx_cached time. ngx_cached err         ┃
┃                                ┃                                  ┃ log_time. ngx_cached_http_time. ngx_         ┃
┃                                ┃                                  ┃cached_http_log_time. ngx_cached_http         ┃
┃                                ┃                                  ┃ log_is08601这6个全局变量都会得到更新         ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t是需要转换的时间，它是格     ┃                                              ┃
┃u_char *ngx_http_time           ┃林威治时间1970年1月1日凌晨        ┃    将时间t转换成“Mon, 28 Sep 1970 06:00:00  ┃
┃                                ┃0点O分O秒到某一时间的秒数，       ┃ GMT”形式的时间，返回值与buf是相同           ┃
┃(u_char *buf, time_t t)         ┃                                  ┃的，都是指向存放时间的字符串                  ┃
┃                                ┃buf是t时间转换成字符串形式的      ┃                                              ┃
┃                                ┃r-rrIP时间后用来存放字符串的内存  ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t悬需要转换的时间，它是格     ┃                                              ┃
┃                                ┃林威治时间1970年1月1日凌晨        ┃    将时间t转换成“Mon. 28-Sep-70 06:00:00    ┃
┃u_char *ngx_http_cookie_time    ┃0点0分0秒到某一时间的秒数，       ┃ GMT”形式适用于cookie的时间，返回值          ┃
┃(u_char *buf, time_t t)         ┃buf是t时间转换成字符串形式适      ┃与buf是相同的，都是指向存放时间的字           ┃
┃                                ┃用于cookie的时间后用来存放字      ┃符串                                          ┃
┃                                ┃符串的内存                        ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃    t是需要转换的时间，它是格林   ┃                                              ┃
┃void ngx_gmtime                 ┃威治时间1970年1月1日凌晨O         ┃    将时间t转换成ngx_tm_t类型的时间。         ┃
┃                                ┃点0分0秒到某一时间的秒数，tp      ┃                                              ┃
┃(time_t t, ngx_tm_t *tp)        ┃                                  ┃下面会说明ngx_tm_t类型                        ┃
┃                                ┃是ngx_tm_t类型的时间，实际上      ┃                                              ┃
┃                                ┃就是标准的tm类型时间              ┃                                              ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                ┃                                  ┃    返回一1表示失败，否则会返回：①如         ┃
┃                                ┃                                  ┃果when表示当天时间秒数，当它合并到            ┃
┃                                ┃                                  ┃实际时间后，已经超过当前时间，那么就          ┃
┃                                ┃                                  ┃返回when合并到实际时间后的秒数（相            ┃
┃time_t ngx_next_time            ┃    when表不期待过期的时间，它    ┃对于格林威治时间1970年1月1日凌晨O             ┃
┃(time_t when)    :              ┃仅表示一天内的秒数                ┃点O分O秒到某一时间的耖数）；                  ┃
┃                                ┃                                  ┃  ②反之，如果合并后的时间早于当前            ┃
┃                                ┃                                  ┃时间，则返回下一天的同一时刻（当天时          ┃
┃                                ┃                                  ┃刻）的时间。它目前仅具有与expires配置         ┃
┃                                ┃                                  ┃项相关的缓存过期功能                          ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃#define ngx_time                ┃    无                            ┃    获取到格林威治时间1970年1月1日            ┃
┃ngx_cached_time->sec            ┃                                  ┃凌晨0点0分0秒到当前时间的秒数                 ┃
┣━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━┫
┃#define ngx_timeofday           ┃    无                            ┃    获取缓存的ngx_time_t类型时间              ┃
┃(ngx_time_t *) ngxLcached_time  ┃                                  ┃                                              ┃
┗━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
*/

time_t
ngx_next_time(time_t when)
{
    time_t     now, next;
    struct tm  tm;

    now = ngx_time();

    ngx_libc_localtime(now, &tm);

    tm.tm_hour = (int) (when / 3600);
    when %= 3600;
    tm.tm_min = (int) (when / 60);
    tm.tm_sec = (int) (when % 60);

    next = mktime(&tm);

    if (next == -1) {
        return -1;
    }

    if (next - now > 0) {
        return next;
    }

    tm.tm_mday++;

    /* mktime() should normalize a date (Jan 32, etc) */

    next = mktime(&tm);

    if (next != -1) {
        return next;
    }

    return -1;
}
