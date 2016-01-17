
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_REGEX_H_INCLUDED_
#define _NGX_REGEX_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

#include <pcre.h>


#define NGX_REGEX_NO_MATCHED  PCRE_ERROR_NOMATCH   /* -1 */

#define NGX_REGEX_CASELESS    PCRE_CASELESS


typedef struct {
    pcre        *code;
    pcre_extra  *extra;
} ngx_regex_t;

//ngx_http_rewrite中，rewrite aaa bbb break;配置中，aaa解析使用ngx_regex_compile_t，bbb解析使用ngx_http_script_compile_t
//赋值见ngx_regex_compile
typedef struct { //相关成员和ngx_http_regex_t中的一样
    ngx_str_t     pattern; //pcre_compile获取到的
    ngx_pool_t   *pool;
    ngx_int_t     options;

    ngx_regex_t  *regex;
    /*
    例如rewrite   ^(?<name1>/download/.*)/media/(?<name2>.*)/(abc)\..*$     $name1/mp3/$name2.mp3  break;
    prce编译^(?<name1>/download/.*)/media/(?<name2>.*)/(abc)\..*$的时候的结果是captures为3(1个非命名子模式变量和2个命名子模式变量)，named_captures为2
     */
    int           captures; //得到的是所有子模式的个数,包含命名子模式(?<name>exp) 和非命名子模式(exp)，赋值见ngx_regex_compile
    int           named_captures; //得到的是命名子模式的个数,不包括非命名子模式的个数;赋值见ngx_regex_compile

    /* 这两个是运来存取名称模式字符串用的 */    
    int           name_size;
    u_char       *names;

    ngx_str_t     err;
} ngx_regex_compile_t;


typedef struct {
    ngx_regex_t  *regex; //正则表达式经过ngx_regex_compile转换后的regex信息
    u_char       *name; //正则表达式原字符串
} ngx_regex_elt_t;


void ngx_regex_init(void);
ngx_int_t ngx_regex_compile(ngx_regex_compile_t *rc);

/*
根据正则表达式到指定的字符串中进行查找和匹配,并输出匹配的结果.
使用编译好的模式进行匹配，采用与Perl相似的算法，返回匹配串的偏移位置。。

例如正则表达式语句re.name= ^(/download/.*)/media/(.*)/tt/(.*)$，  s=/download/aa/media/bdb/tt/ad,则他们会匹配，同时匹配的变量数有3个，则返回值为3+1=4,如果不匹配则返回-1
*/
#define ngx_regex_exec(re, s, captures, size)                                \
    pcre_exec(re->code, re->extra, (const char *) (s)->data, (s)->len, 0, 0, \
              captures, size)
#define ngx_regex_exec_n      "pcre_exec()"

ngx_int_t ngx_regex_exec_array(ngx_array_t *a, ngx_str_t *s, ngx_log_t *log);


#endif /* _NGX_REGEX_H_INCLUDED_ */
