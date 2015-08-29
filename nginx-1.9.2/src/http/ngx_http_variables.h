
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_VARIABLES_H_INCLUDED_
#define _NGX_HTTP_VARIABLES_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef ngx_variable_value_t  ngx_http_variable_value_t;

#define ngx_http_variable(v)     { sizeof(v) - 1, 1, 0, 0, 0, (u_char *) v }

typedef struct ngx_http_variable_s  ngx_http_variable_t;
//参考<输入剖析nginx-变量>
typedef void (*ngx_http_set_variable_pt) (ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
typedef ngx_int_t (*ngx_http_get_variable_pt) (ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);


/*
NGX_HTTP_VAR_CHANGEABLE：允许重复定义；
NGX_HTTP_VAR_NOCACHEABLE：变量值不可以被缓存，每次使用必须计算；
NGX_HTTP_VAR_INDEXED：指示变量值存储在数组中，当使用ngx_http_get_variable函数获取变量时不会每次都为变量分配值空间；
NGX_HTTP_VAR_NOHASH：配置解析完以后，变量名不进hash索引，处理请求时不可以用变量名访问变量。


NGX_HTTP_VAR_CHANGEABLE表示变量是可以更改的。就例如set命令定义的变量就是可改变的，同时也有不可改变的。如果是可改变的那
也就是说这个变量是不能缓存的(NGX_HTTP_VAR_NOCACHEABLE)。
*/ //ngx_http_add_variable函数的flag参数

/*
代码片段8.2-6，文件名：nginx．conf
50：    set $file t_a;
51：    set $file t_b;
    此时，set指令会重复添加变量$file（其实，第51行并不会新增变量$file，因为在新增的过程中发现已经有该变量了，并且是NGX_HTTP_VAR_CHANGEABLE
的，所以就返回该变量使刚），并且其最终值将为t_b。如果新增一个不是NGX_HTTP_VAR_CHANGEABLE的变量$t_var，那么Nginx将提示the duplicate”t_var”variable后退出执行。
*/
#define NGX_HTTP_VAR_CHANGEABLE   1

/*
另—个标记为NGX_HTTP_VAR_NOCACHEABLE，表示该变量不可缓存。我们都知道，所有这些变量基本都是跟随客户端请求的每个连接而变的，比如
变量$http_user_agent会随着客户端使用浏览器的不同而不同，但只在客户端的同一个连接里，这个变量肯定不会发生改变，即不可能一个连接
前半个是IE浏览器而后半个是Opera浏览器，所以这个变量是可缓存的，住处理这个客户端连接的整个过程中，变最$http_user_agent值计算一次就行了，后续使
用可直接使用其缓存。然而，有一些变量，因为Nginx本身的内部处理会发生改变，比如变量$uri，虽然客户端发过来的请求连接URI是/thread-3760675-2-l.html．
但通过rewrite转换却变成了/thread.php?id=3760675&page=2&flOOFI，也即是变量$uri发生了改变，所以对于变量$uri，每次使用都必须进行主动计算（即调用
回调get_handler0函数），该标记影响的逻辑主要是变量取值函数ngx_http_get_flushed_variable。当然，如果我们明确知道当前的细节情况，此时从性能上考虑，
也不一定就非要重新计算获取值，比如刚刚通过主动计算获取了变量$uri的值，接着马上又去获取变量$uri的值（这种情况当然有，例如连续将$uri变量的值赋
值给另外两个不同变量），此时可使用另外一个取值函数ngx_http_get_indexed_variable，直接取值而不考虑是否可缓存标记。
*/
#define NGX_HTTP_VAR_NOCACHEABLE  2

/*
NGX HTTP_VAR_INDEXED、NGXHTTP_VARNOHASH、变量cmcf->variables_hash以及取值函数ngx_http_get_variable等，它们都是为SSI模块实现而设计的
*/
#define NGX_HTTP_VAR_INDEXED      4
#define NGX_HTTP_VAR_NOHASH       8

/* 
变量的定义是由模块代码完成的 (内建变量)，或者由配置指令完成 (用户变量)，在书写配置文件时，只能使用已经定义过的变量。
变量的定义由提供变量的模块或者用户配置指令项 (如 set, geo 等) 完成：

然而，Nginx 各个模块定义的变量可能只有部分出现到了配置文件 (nginx.conf) 中，也就是说，在对配置文件中的 http {} 作用域进行解析
的过程中，出现在配置项中的变量才是在 Nginx 运行过程中被使用到的变量。Nginx 调用 ngx_http_get_variable_index 在数组 cmcf->variables 
中给配置项中出现的变量依次分配 ngx_http_variable_t 类型的空间，并将对应的数组索引返回给引用此变量的配置项的解析回调函数。

核心模块 ngx_http_core_module 提供的变量使用 ngx_http_core_variables 描述，由 preconfiguration 回调函数 ngx_http_variables_add_core_vars 进行定义：
非核心模块比如 ngx_http_fastcgi_module 提供的变量使用 ngx_http_fastcgi_vars 描述，由 preconfiguration 回调函数 ngx_http_fastcgi_add_variables 进行定义：
由 ngx_http_rewrite_module 提供的 set 指令定义的自定义变量由其配置解析函数 ngx_http_rewrite_set 进行定义：


获取变量的值
脚本引擎
什么是引擎？从机械工程上说，是把能量转化成机械运动的设备。在计算科学领域，引擎一般是指将输入数据转换成其它格式或形式的输出的软件模块。

    Nginx 将包含变量的配置项参数转换成一系列脚本，并在合适的时机，通过脚本引擎运行这些脚本，得到参数的最终值。换句话说，脚本引擎负责将参
数中的变量在合适的时机取值并和参数中的固定字符串拼接成最终字符串。

    对参数的脚本化工作，也在配置项解析过程中完成。为了表达具体，选用下面的一条配置语句进行分析。此配置项使 Nginx 在拿到请求对应的 Host 
信息后，比如，"example.com"，经脚本引擎处理拼接成 "example.com/access.log" 作用后续 access log 的目标文件名。
    access_log  ${host}/access.log;            变量获取，脚本处理过程可以参考ngx_http_script_compile_t；变量定义过程见ngx_http_variable_s相关章节
*/ //参考:http://ialloc.org/posts/2013/10/20/ngx-notes-http-variables/

/*
ngx_http_core_main_conf_t->variables数组成员的结构式ngx_http_variable_s， ngx_http_request_s->variables数组成员结构是
ngx_variable_value_t这两个结构的关系很密切，一个所谓变量，一个所谓变量值

r->variables这个变量和cmcf->variables是一一对应的，形成var_ name与var_value对，所以两个数组里的同一个下标位置元素刚好就是
相互对应的变量名和变量值，而我们在使用某个变量时总会先通过函数ngx_http_get_variable_index获得它在变量名数组里的index下标，也就是变
量名里的index字段值，然后利用这个index下标进而去变量值数组里取对应的值
*/ //参考<输入剖析nginx-变量>
struct ngx_http_variable_s { //ngx_http_add_variable  ngx_http_get_variable_index中创建空间并赋值 
//variables_hash表和variables数组中的成员在ngx_http_variables_init_vars中从variables_keys中获取响应值，这些值的来源是临时的variables_keys

    ngx_str_t                     name;   /* must be first to build the hash */

    /*
    get_handler体现的是lazy_handle的策略，只有使用到变量，才会计算变量值；
    set_handler体现的是active_handle的策略，每执行一个请求，都会计算变量值。

        set_handlerr()回调目前只在使用set配置指令构造脚本引擎时才会用到，而那里直接使用cmcf->variables_keys里对应变量的该字段，并
    且一旦配置文件解析完毕，set_handlerr()回调也就用不上了

    set_handler0，这个回调目前只被使用在set指令里，组成脚本引擎的一个步骤，提供给用户在配置文件里可以修改内置变量的值，带有set_handler0接口的变量非常少，
如变量$args、$limitrate．且这类变量一定会带上NGX_HTTP_VAR_CHANGEABLE标记，否则这个接口毫无意义，因为既然不能修改，何必提供修改接口？也会带上NGX_HTTP
VAR_NOCACHEABLE标记，因为既然会被修改，自然也是不可缓存的
     */ //
    ngx_http_set_variable_pt      set_handler; //
    //在ngx_http_variables_init_vars中会对"http_"  "send_http_"等设置默认的get_handler和data
    /*
    gethandler()回调字段，这个字段主要实现获取变量值的功能。前面讲了Nginx内置变量的值都是有默认来源的，如果是简单地直接存放在某个地
方（上面讲的内部变最$args情况），那么不要这个get_handler()回调函数倒还可以，通过data字段指向的地址读取。但是如果比较复杂，虽然知道
这个值存放在哪儿，但是却需要比较复杂的逻辑获取（上面讲的内部变量$remote_port情况），此时就必须靠回调函数get_handler()来执行这部分逻辑。
总之，不管简单或复杂，回调函数get_handler0帮我们去在合适的地方通过合适的方式，获取到该内部变量的值，这也是为什么我们并没有给Nginx内部变量赋值，
却义能读到值，因为有这个回调函数的存在。来看看这两个示例变量的data字段与get_handler()回调字段情况。
     */ //gethandler()回调字段，这个字段主要实现获取变量值的功能   例如ngx_http_core_variables
     //ngx_http_rewrite_set中设置为ngx_http_rewrite_var  args的get_handler是ngx_http_variable_request ngx_http_variables_init_vars中设置"http_"等的get_handler
    ngx_http_get_variable_pt      get_handler; 

    /*
    举个例子，Nginx内部变量$args表示的是客户端GET请求时uri里的参数，结构体ngxhttp_request_t有一个ngx_str_t类型字段为
args，内存放的就是GET请求参数，所以内部变量$args的这个data字段就是指向变量r里的args字段，表示其数据来之这里。这是直接的情况，那么间接的
情况呢？看Nginx内部变量$remoteport，这个变量表示客户端端口号，这个值在结构体ngxhttprequest_t内没有直接的字段对应，但足肯定同样也是来自
ngxhttp_request_t变量r里，怎么去获取就看gethandler函数的实现，此时data数据字段没什么作用，值为0。
     */
    uintptr_t                     data;//例如:在args中data的值是offsetof(ngx_http_request_t, args)，这个就是args在ngx_http_request_t中的偏移。
    
    ngx_uint_t                    flags; //赋值为NGX_HTTP_VAR_CHANGEABLE等
    //在variables数组中的位置，在ngx_http_get_variable_index的时候已经确定了在variables数组中的位置
    ngx_uint_t                    index;  //通过函数ngx_http_get_variable_index获得
};


ngx_http_variable_t *ngx_http_add_variable(ngx_conf_t *cf, ngx_str_t *name,
    ngx_uint_t flags);
ngx_int_t ngx_http_get_variable_index(ngx_conf_t *cf, ngx_str_t *name);
ngx_http_variable_value_t *ngx_http_get_indexed_variable(ngx_http_request_t *r,
    ngx_uint_t index);
ngx_http_variable_value_t *ngx_http_get_flushed_variable(ngx_http_request_t *r,
    ngx_uint_t index);

ngx_http_variable_value_t *ngx_http_get_variable(ngx_http_request_t *r,
    ngx_str_t *name, ngx_uint_t key);

ngx_int_t ngx_http_variable_unknown_header(ngx_http_variable_value_t *v,
    ngx_str_t *var, ngx_list_part_t *part, size_t prefix);


#if (NGX_PCRE)

//ngx_http_regex_compile中分配空间,保存变量   ngx_http_regex_exec获取对应的值
typedef struct {
    ngx_uint_t                    capture;
    ngx_int_t                     index;
} ngx_http_regex_variable_t; //ngx_http_regex_t中包含该成员结构

//该结构包含在ngx_http_script_regex_code_t中
typedef struct {//ngx_http_regex_compile函数中使用，赋值见ngx_http_regex_compile
    ngx_regex_t                  *regex; //ngx_regex_compile_t->regex一样
    ngx_uint_t                    ncaptures;//ngx_regex_compile_t->captures一样 命名子模式和非命名子模式的总个数
    ngx_http_regex_variable_t    *variables; //命名子模式对应的变量，赋值见ngx_http_regex_compile
    ngx_uint_t                    nvariables; //有多少个命名子模式，赋值见ngx_http_regex_compile， ngx_http_regex_exec中使用
    ngx_str_t                     name;//ngx_regex_compile_t->pattern一样
} ngx_http_regex_t;


typedef struct {
    ngx_http_regex_t             *regex;
    void                         *value;
} ngx_http_map_regex_t;


ngx_http_regex_t *ngx_http_regex_compile(ngx_conf_t *cf,
    ngx_regex_compile_t *rc);
ngx_int_t ngx_http_regex_exec(ngx_http_request_t *r, ngx_http_regex_t *re,
    ngx_str_t *s);

#endif


typedef struct {
    ngx_hash_combined_t           hash;
#if (NGX_PCRE)
    ngx_http_map_regex_t         *regex;
    ngx_uint_t                    nregex;
#endif
} ngx_http_map_t;


void *ngx_http_map_find(ngx_http_request_t *r, ngx_http_map_t *map,
    ngx_str_t *match);


ngx_int_t ngx_http_variables_add_core_vars(ngx_conf_t *cf);
ngx_int_t ngx_http_variables_init_vars(ngx_conf_t *cf);


extern ngx_http_variable_value_t  ngx_http_variable_null_value;
extern ngx_http_variable_value_t  ngx_http_variable_true_value;


#endif /* _NGX_HTTP_VARIABLES_H_INCLUDED_ */
