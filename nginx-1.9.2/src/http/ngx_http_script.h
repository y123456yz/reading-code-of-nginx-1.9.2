
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_SCRIPT_H_INCLUDED_
#define _NGX_HTTP_SCRIPT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    /*
    关于pos && code: 每次调用code,都会将解析到的新的字符串放入pos指向的字符串处，然后将pos向后移动，下次进入的时候，会自动将数据追加到后面的。
	对于ip也是这个原理，code里面会将e->ip向后移动。移动的大小根据不同的变量类型相关。ip指向一快内存，其内容为变量相关的一个结构体，比
	如ngx_http_script_copy_capture_code_t，结构体之后，又是下一个ip的地址。比如移动时是这样的 :code = (ngx_http_script_copy_capture_code_t *) e->ip;
     e->ip += sizeof(ngx_http_script_copy_capture_code_t);//移动这么多位移。
	*/ 
	/* 包含了在配置解析过程中设置的一些处理结构体，下面的rlcf->codes是一个数组，注意的是，这些结构体的第一个成员就是一个处理handler，
    这里处理时，都会将该结构体类型强转，拿到其处理handler，然后按照顺序依次执行之   */
    u_char                     *ip; //参考ngx_http_rewrite_handler  IP实际上是函数指针数组
    u_char                     *pos; //pos之前的数据就是解析成功的，后面的数据将追加到pos后面。pos指向的是后面的buf数据末尾处
    //这里貌似是用sp来保存中间结果，比如保存当前这一步的进度，到下一步好用e->sp--来找到上一步的结果。
    /* sp是一个ngx_http_variable_value_t的数组，里面保存了从配置中分离出的一些变量   
    和一些中间结果，在当前处理中可以可以方便的拿到之前或者之后的变量(通过sp--或者sp++)  */
    ngx_http_variable_value_t  *sp; //参考ngx_http_rewrite_handler分配空间, 是用来零食存放value值的，最终都会通过ngx_http_script_set_var_code拷贝到r->variables[code->index]中

    ngx_str_t                   buf;//存放结果，也就是buffer，pos指向其中。  参考ngx_http_script_complex_value_code
    
    ngx_str_t                   line; //记录请求行URI  e->line = r->uri;

    /* the start of the rewritten arguments */
    u_char                     *args; //记录?后面的参数信息

    unsigned                    flushed:1;
    unsigned                    skip:1; //生效见ngx_http_script_copy_code，置1表示没有需要拷贝的数据，直接跳过数据拷贝步骤
    unsigned                    quote:1; //和ngx_http_script_regex_code_t->redirect一样，见ngx_http_script_regex_start_code
    unsigned                    is_args:1; //ngx_http_script_mark_args_code和ngx_http_script_start_args_code中置1，表示参数中是否带有?
    unsigned                    log:1;

    ngx_int_t                   status; //在执行完ngx_http_rewrite_handler中的所有code时，会返回该status参数
    ngx_http_request_t         *request; //所属的请求   // 需要处理的请求  
    
} ngx_http_script_engine_t;


/*
获取变量的值
脚本引擎
什么是引擎？从机械工程上说，是把能量转化成机械运动的设备。在计算科学领域，引擎一般是指将输入数据转换成其它格式或形式的输出的软件模块。

    Nginx 将包含变量的配置项参数转换成一系列脚本，并在合适的时机，通过脚本引擎运行这些脚本，得到参数的最终值。换句话说，脚本引擎负责将参
数中的变量在合适的时机取值并和参数中的固定字符串拼接成最终字符串。

    对参数的脚本化工作，也在配置项解析过程中完成。为了表达具体，选用下面的一条配置语句进行分析。此配置项使 Nginx 在拿到请求对应的 Host 
信息后，比如，"example.com"，经脚本引擎处理拼接成 "example.com/access.log" 作用后续 access log 的目标文件名。
    access_log  ${host}/access.log;            变量获取，脚本处理过程可以参考ngx_http_script_compile_t；变量定义过程见ngx_http_variable_s相关章节
*/ 

/*
ngx_http_script_compile_t:/脚本化辅助结构体 - 作为脚本化包含变量的参数时的统一输入。
ngx_http_script_copy_code_t - 负责将配置参数项中的固定字符串原封不动的拷贝到最终字符串。
ngx_http_script_var_code_t - 负责将配置参数项中的变量取值后添加到最终字符串。

脚本引擎相关函数
ngx_http_script_variables_count - 根据 $ 出现在的次数统计出一个配置项参数中出现了多少个变量。
ngx_http_script_compile - 将包含变量的参数脚本化，以便需要对参数具体化时调用脚本进行求值。
ngx_http_script_done - 添加结束标志之类的收尾工作。
ngx_http_script_run - 
ngx_http_script_add_var_code - 为变量创建取值需要的脚本。在实际变量取值过程中，为了确定包含变量的参数在参数取值后需要的内存块大小，
Nginx 将取值过程分成两个脚本，一个负责计算变量的值长度，另一个负责取出对应的值。

样例配置指令 access_log ${host}access.log 从配置指令解析描述一下变量从定义到使用的整个过程。
当配置解析代码碰到 access_log 指令后，会调用配置项回调函数 ngx_http_log_set_log 解析配置项参数。
*/
//可以以access_log为例，参考ngx_http_log_set_log
//参考:http://ialloc.org/posts/2013/10/20/ngx-notes-http-variables/    http://blog.csdn.net/brainkick/article/details/7065244
//ngx_http_rewrite中，rewrite aaa bbb break;配置中，aaa解析使用ngx_regex_compile_t，bbb解析使用ngx_http_script_compile_t
//赋值见ngx_http_rewrite
typedef struct {//脚本化辅助结构体 - 作为脚本化包含变量的参数时的统一输入。   ngx_http_script_init_arrays中分配相关内存
    
    ngx_conf_t                 *cf;
    ngx_str_t                  *source; /* 配置文件中的原始参数字符串  比如http://$http_host/aa.mp4*/
        
    //ngx_http_script_add_copy_code   ngx_http_script_add_var_code  ngx_http_script_add_args_code
    ngx_array_t               **flushes;//从ngx_http_variable_t->variables中获取， 这里面存储的是变量的index序号，见ngx_http_script_add_var_code
    /*
    ngx_http_rewrite_value中赋值为ngx_http_script_complex_value_code_t->lengths，ngx_http_script_complex_value_code会执行该数组中的节点pt,
     */  
    ngx_array_t               **lengths; /* 存放用于获取变量对应的值长度的脚本 */ //  数组中的每个成员就1字节 
    /*
    ngx_http_rewrite_value中赋值为ngx_http_rewrite_loc_conf_t->codes,节点pt在在ngx_http_rewrite_handler会得到执行
     */ //成员pt可以是ngx_http_script_copy_var_code，见ngx_http_script_add_var_code
     //和ngx_http_script_compile_t->values一样，见ngx_http_rewrite_value  
    ngx_array_t               **values; /* 存放用于获取变量对应的值的脚本 */ // 数组中的每个成员就1字节  
    
    
    // 普通变量的个数，而非其他三种(args变量，$n变量以及常量字符串)   
    ngx_uint_t                  variables; /* 原始参数字符中出现的变量个数 */  //参考ngx_http_script_compile
    ngx_uint_t                  ncaptures; // 当前处理时，出现的$n变量的最大值，如配置的最大为$3，那么ncaptures就等于3   
    
    /* 
       * 以位移的形式保存$1,$2...$9等变量，即响应位置上置1来表示，主要的作用是为dup_capture准备， 
       * 正是由于这个mask的存在，才比较容易得到是否有重复的$n出现。 
     */  
    ngx_uint_t                  captures_mask; //赋值见ngx_http_script_compile
    ngx_uint_t                  size;// 待compile的字符串中，”常量字符串“的总长度  

    /*  
     对于main这个成员，有许多要挖掘的东西。main一般用来指向一个ngx_http_script_regex_code_t的结构 
     */  //ngx_http_rewrite指向ngx_http_script_regex_code_t
    void                       *main; //正则表达式结构这是顶层的表达式，里面包含了lengths等。

    // 是否需要处理请求参数 
    unsigned                    compile_args:1; //标记?做普通字符还是做特殊字符，参考ngx_http_script_compile

    //下面这些是收尾工作，见ngx_http_script_done
    unsigned                    complete_lengths:1;// 是否设置lengths数组的终止符，即NULL     见ngx_http_script_done
    unsigned                    complete_values:1; // 是否设置values数组的终止符  
    unsigned                    zero:1; // values数组运行时，得到的字符串是否追加'\0'结尾   
    unsigned                    conf_prefix:1; // 是否在生成的文件名前，追加路径前缀   
    unsigned                    root_prefix:1; // 同conf_prefix   

   /* 
     这个标记位主要在rewrite模块里使用，在ngx_http_rewrite中， 
     if (sc.variables == 0 && !sc.dup_capture) { 
         regex->lengths = NULL; 
     } 
     没有重复的$n，那么regex->lengths被置为NULL，这个设置很关键，在函数 ngx_http_script_regex_start_code中就是通过对regex->lengths的判断，
     来做不同的处理，因为在没有重复的$n的时候，可以通过正则自身的captures机制来获取$n，一旦出现重复的， 那么pcre正则自身的captures并不能
     满足我们的要求，我们需要用自己handler来处理。 
    */
    unsigned                    dup_capture:1;
    unsigned                    args:1; //标记参数是否带有?  参考ngx_http_script_compile   // 待compile的字符串中是否发现了'?'
} ngx_http_script_compile_t;


typedef struct {
    ngx_str_t                   value;
    ngx_uint_t                 *flushes;
    void                       *lengths;
    void                       *values;
} ngx_http_complex_value_t;


typedef struct {
    ngx_conf_t                 *cf;
    ngx_str_t                  *value;
    ngx_http_complex_value_t   *complex_value;

    unsigned                    zero:1;
    unsigned                    conf_prefix:1;
    unsigned                    root_prefix:1;
} ngx_http_compile_complex_value_t;


typedef void (*ngx_http_script_code_pt) (ngx_http_script_engine_t *e);
typedef size_t (*ngx_http_script_len_code_pt) (ngx_http_script_engine_t *e);

/*
ngx_http_script_compile_t:/脚本化辅助结构体 - 作为脚本化包含变量的参数时的统一输入。
ngx_http_script_copy_code_t - 负责将配置参数项中的固定字符串原封不动的拷贝到最终字符串。
ngx_http_script_var_code_t - 负责将配置参数项中的变量取值后添加到最终字符串。

脚本引擎相关函数
ngx_http_script_variables_count - 根据 $ 出现在的次数统计出一个配置项参数中出现了多少个变量。
ngx_http_script_compile - 将包含变量的参数脚本化，以便需要对参数具体化时调用脚本进行求值。
ngx_http_script_done - 添加结束标志之类的收尾工作。
ngx_http_script_run - 
ngx_http_script_add_var_code - 为变量创建取值需要的脚本。在实际变量取值过程中，为了确定包含变量的参数在参数取值后需要的内存块大小，
Nginx 将取值过程分成两个脚本，一个负责计算变量的值长度，另一个负责取出对应的值。
*/
typedef struct {
    ngx_http_script_code_pt     code;
    uintptr_t                   len;
} ngx_http_script_copy_code_t; //实际上从ngx_http_script_compile_t->values分配空间的时候，会多分配用来存放数据的空间，见ngx_http_script_add_copy_code


typedef struct {
    /* 赋值为以下三个ngx_http_script_copy_var_len_code   ngx_http_script_set_var_code  ngx_http_script_var_code*/
    ngx_http_script_code_pt     code;
    //变量ngx_http_script_var_code_t->index表示Nginx变量$file在cmcf->variables数组内的下标，对应每个请求的变量值存储空间就为r->variables[code->index],参考ngx_http_script_set_var_code
    uintptr_t                   index; //在ngx_http_variable_t->index中的位置
} ngx_http_script_var_code_t;


typedef struct {
    ngx_http_script_code_pt     code;
    ngx_http_set_variable_pt    handler;
    uintptr_t                   data;
} ngx_http_script_var_handler_code_t;


typedef struct {
    ngx_http_script_code_pt     code;
    uintptr_t                   n;
} ngx_http_script_copy_capture_code_t;


#if (NGX_PCRE)

typedef struct { //创建空间赋值见ngx_http_rewrite
    //当前的code，第一个函数，为ngx_http_script_regex_start_code
    ngx_http_script_code_pt     code; //rewrite xxx bbb break；的时候为ngx_http_script_regex_start_code，if配置的时候为ngx_http_script_regex_start_code
    ngx_http_regex_t           *regex;//解析后的正则表达式。
    //以rewrite为例，如果后面部分是简单字符串比如 rewrite ^(.*)$ http://chenzhenianqing.cn break;则length为NULL
    ngx_array_t                *lengths; //我这个正则表达式对应的lengths。依靠它来解析 第二部分 rewrite ^(.*)$ http://$http_host.mp4 break;
    									//lengths里面包含一系列code,用来求目标url的大小的。
    uintptr_t                   size; // 待compile的字符串中，”常量字符串“的总长度   来源见ngx_http_rewrite->ngx_http_script_compile
    //rewrite aaa bbb op;中的bbb为"http://" "https://" "$scheme"或者op为redirect permanent则status会赋值，见ngx_http_rewrite
    uintptr_t                   status; //例如NGX_HTTP_MOVED_TEMPORARILY  
    uintptr_t                   next; //next的含义为;如果当前code匹配失败，那么下一个code的位移是在什么地方，这些东西全部放在一个数组里面的。

    uintptr_t                   test:1;//我是要看看是否正则匹配成功，你待会匹配的时候记得放个变量到堆栈里。 if{}配置的时候需要
    uintptr_t                   negative_test:1;
    uintptr_t                   uri:1;//是否是URI匹配。  rewrite配置时会置1表示需要进行uri匹配
    uintptr_t                   args:1;

    /* add the r->args to the new arguments */
    uintptr_t                   add_args:1;//是否自动追加参数到rewrite后面。如果目标结果串后面用问好结尾，则nginx不会拷贝参数到后面的

    //rewrite aaa bbb op;中的bbb为"http://" "https://" "$scheme"或者op为redirect permanent则redirect置1，见ngx_http_rewrite
    uintptr_t                   redirect:1;//nginx判断，如果是用http://等开头的rewrite，就代表是垮域重定向。会做302处理。
    //rewrite最后的参数是break，将rewrite后的地址在当前location标签中执行。具体参考ngx_http_script_regex_start_code
    uintptr_t                   break_cycle:1;

    //正则表达式语句，例如rewrite  ^(?<name1>/download/.*)/media/(?<name2>.*)/(abc)\..*$   xxx  break;中的 ^(?<name1>/download/.*)/media/(?<name2>.*)/(abc)\..*$ 
    ngx_str_t                   name; 
} ngx_http_script_regex_code_t;


typedef struct { //ngx_http_rewrite中分配空间，赋值
    ngx_http_script_code_pt     code;

    uintptr_t                   uri:1;
    uintptr_t                   args:1;

    /* add the r->args to the new arguments */
    uintptr_t                   add_args:1; //和ngx_http_script_regex_code_t->add_args相等，见ngx_http_rewrite

    uintptr_t                   redirect:1;//和ngx_http_script_regex_code_t->redirect相等，见ngx_http_rewrite
} ngx_http_script_regex_end_code_t;

#endif


typedef struct {
    ngx_http_script_code_pt     code;
    uintptr_t                   conf_prefix;
} ngx_http_script_full_name_code_t;


typedef struct {
    ngx_http_script_code_pt     code;
    uintptr_t                   status; //return code中的code转换后的数字
    ngx_http_complex_value_t    text;
} ngx_http_script_return_code_t; //创建空间和赋值见ngx_http_rewrite_return


typedef enum {
    ngx_http_script_file_plain = 0,
    ngx_http_script_file_not_plain,
    ngx_http_script_file_dir,
    ngx_http_script_file_not_dir,
    ngx_http_script_file_exists,
    ngx_http_script_file_not_exists,
    ngx_http_script_file_exec,
    ngx_http_script_file_not_exec
} ngx_http_script_file_op_e;


typedef struct {
    ngx_http_script_code_pt     code;
    uintptr_t                   op;
} ngx_http_script_file_code_t;


typedef struct {
    ngx_http_script_code_pt     code;
    uintptr_t                   next;
    void                      **loc_conf;
} ngx_http_script_if_code_t;

/*解析set $variable value，如果value是普通字符串，存入ngx_http_script_value_code_t，如果value是一个$xx变量，则存入
ngx_http_script_complex_value_code_t，见ngx_http_rewrite_value,最终都会加入ngx_http_rewrite_loc_conf_t->codes中，参考ngx_http_rewrite_value */
typedef struct {
    ngx_http_script_code_pt     code; //ngx_http_script_complex_value_code
    //和ngx_http_script_compile_t->lengths一样，见ngx_http_rewrite_value  
    ngx_array_t                *lengths; //ngx_http_script_complex_value_code会执行该数组中的节点pt,
} ngx_http_script_complex_value_code_t;

/*解析set $variable value，如果value是普通字符串，存入ngx_http_script_value_code_t，如果value是一个$xx变量，则存入
ngx_http_script_complex_value_code_t，参考ngx_http_rewrite_value */
typedef struct { //见ngx_http_rewrite_value
    ngx_http_script_code_pt     code; 
    uintptr_t                   value; //如果是数字字符串，则该值为数字字符串转换为数字后的值
    uintptr_t                   text_len; //value字符串的长度
    uintptr_t                   text_data; //value字符串
} ngx_http_script_value_code_t;


void ngx_http_script_flush_complex_value(ngx_http_request_t *r,
    ngx_http_complex_value_t *val);
ngx_int_t ngx_http_complex_value(ngx_http_request_t *r,
    ngx_http_complex_value_t *val, ngx_str_t *value);
ngx_int_t ngx_http_compile_complex_value(ngx_http_compile_complex_value_t *ccv);
char *ngx_http_set_complex_value_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


ngx_int_t ngx_http_test_predicates(ngx_http_request_t *r,
    ngx_array_t *predicates);
char *ngx_http_set_predicate_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

ngx_uint_t ngx_http_script_variables_count(ngx_str_t *value);
ngx_int_t ngx_http_script_compile(ngx_http_script_compile_t *sc);
u_char *ngx_http_script_run(ngx_http_request_t *r, ngx_str_t *value,
    void *code_lengths, size_t reserved, void *code_values);
void ngx_http_script_flush_no_cacheable_variables(ngx_http_request_t *r,
    ngx_array_t *indices);

void *ngx_http_script_start_code(ngx_pool_t *pool, ngx_array_t **codes,
    size_t size);
void *ngx_http_script_add_code(ngx_array_t *codes, size_t size, void *code);

size_t ngx_http_script_copy_len_code(ngx_http_script_engine_t *e);
void ngx_http_script_copy_code(ngx_http_script_engine_t *e);
size_t ngx_http_script_copy_var_len_code(ngx_http_script_engine_t *e);
void ngx_http_script_copy_var_code(ngx_http_script_engine_t *e);
size_t ngx_http_script_copy_capture_len_code(ngx_http_script_engine_t *e);
void ngx_http_script_copy_capture_code(ngx_http_script_engine_t *e);
size_t ngx_http_script_mark_args_code(ngx_http_script_engine_t *e);
void ngx_http_script_start_args_code(ngx_http_script_engine_t *e);
#if (NGX_PCRE)
void ngx_http_script_regex_start_code(ngx_http_script_engine_t *e);
void ngx_http_script_regex_end_code(ngx_http_script_engine_t *e);
#endif
void ngx_http_script_return_code(ngx_http_script_engine_t *e);
void ngx_http_script_break_code(ngx_http_script_engine_t *e);
void ngx_http_script_if_code(ngx_http_script_engine_t *e);
void ngx_http_script_equal_code(ngx_http_script_engine_t *e);
void ngx_http_script_not_equal_code(ngx_http_script_engine_t *e);
void ngx_http_script_file_code(ngx_http_script_engine_t *e);
void ngx_http_script_complex_value_code(ngx_http_script_engine_t *e);
void ngx_http_script_value_code(ngx_http_script_engine_t *e);
void ngx_http_script_set_var_code(ngx_http_script_engine_t *e);
void ngx_http_script_var_set_handler_code(ngx_http_script_engine_t *e);
void ngx_http_script_var_code(ngx_http_script_engine_t *e);
void ngx_http_script_nop_code(ngx_http_script_engine_t *e);


#endif /* _NGX_HTTP_SCRIPT_H_INCLUDED_ */
