
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_TIME_H_INCLUDED_
#define _NGX_TIME_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef ngx_rbtree_key_t      ngx_msec_t;
typedef ngx_rbtree_key_int_t  ngx_msec_int_t;

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
/*
struct   tm{
    秒一取值区间为[0，59]
    int tm_sec;
    分一取值区间为[0，59]
    int tm__ min;
    时一取值区间为[o，23]
    int tm hour;
    一个月中的日期一取值区间为[1，31]
    iAt tm_mday;】
    月份（从一月开始，0代表一月）一取值区间为[0，II]
    int tm- mon,
    年份，其值等于实际年份减去1900
    int tm_year,
    星期一取值区间为[0，6]，其中0代表星期天，1代表星期一，依此类推
    int tm_wday;
    从每年的1月1日开始的天数一取值区间为[0，365】，其中0代表1月1日，1代表1月2日依此类推
    int tm_yday;
    夏令时标识符。在实行夏令时的时候，tm_isdst为正；不实行夏令时的时候，tm_isdst为o在不了解情况时，tm_ isdst为负
    int tm isdst,
    )；
    ngx_tmj与tm用法是完全一致的，如下所示。
typedef struct tm    ngx_tm_t;
#define ngx_tm_sec
#define ngx_tm_min
#define ngx_tm_hour
#define ngx_tm_mday
#define ngx_tm_mon
#define ngx_tm_year
#define ngx_tm_wday
#define ngx_tm_isdst
*/
typedef struct tm             ngx_tm_t;

#define ngx_tm_sec            tm_sec
#define ngx_tm_min            tm_min
#define ngx_tm_hour           tm_hour
#define ngx_tm_mday           tm_mday
#define ngx_tm_mon            tm_mon
#define ngx_tm_year           tm_year
#define ngx_tm_wday           tm_wday
#define ngx_tm_isdst          tm_isdst

#define ngx_tm_sec_t          int
#define ngx_tm_min_t          int
#define ngx_tm_hour_t         int
#define ngx_tm_mday_t         int
#define ngx_tm_mon_t          int
#define ngx_tm_year_t         int
#define ngx_tm_wday_t         int


#if (NGX_HAVE_GMTOFF)
#define ngx_tm_gmtoff         tm_gmtoff
#define ngx_tm_zone           tm_zone
#endif


#if (NGX_SOLARIS)

#define ngx_timezone(isdst) (- (isdst ? altzone : timezone) / 60)

#else

#define ngx_timezone(isdst) (- (isdst ? timezone + 3600 : timezone) / 60)

#endif


void ngx_timezone_update(void);
void ngx_localtime(time_t s, ngx_tm_t *tm);
void ngx_libc_localtime(time_t s, struct tm *tm);
void ngx_libc_gmtime(time_t s, struct tm *tm);

#define ngx_gettimeofday(tp)  (void) gettimeofday(tp, NULL);
#define ngx_msleep(ms)        (void) usleep(ms * 1000)
#define ngx_sleep(s)          (void) sleep(s)


#endif /* _NGX_TIME_H_INCLUDED_ */
