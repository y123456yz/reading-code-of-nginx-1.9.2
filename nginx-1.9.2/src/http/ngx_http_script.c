
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static ngx_int_t ngx_http_script_init_arrays(ngx_http_script_compile_t *sc);
static ngx_int_t ngx_http_script_done(ngx_http_script_compile_t *sc);
static ngx_int_t ngx_http_script_add_copy_code(ngx_http_script_compile_t *sc,
    ngx_str_t *value, ngx_uint_t last);
static ngx_int_t ngx_http_script_add_var_code(ngx_http_script_compile_t *sc,
    ngx_str_t *name);
static ngx_int_t ngx_http_script_add_args_code(ngx_http_script_compile_t *sc);
#if (NGX_PCRE)
static ngx_int_t ngx_http_script_add_capture_code(ngx_http_script_compile_t *sc,
     ngx_uint_t n);
#endif
static ngx_int_t
     ngx_http_script_add_full_name_code(ngx_http_script_compile_t *sc);
static size_t ngx_http_script_full_name_len_code(ngx_http_script_engine_t *e);
static void ngx_http_script_full_name_code(ngx_http_script_engine_t *e);


#define ngx_http_script_exit  (u_char *) &ngx_http_script_exit_code

static uintptr_t ngx_http_script_exit_code = (uintptr_t) NULL;


void
ngx_http_script_flush_complex_value(ngx_http_request_t *r,
    ngx_http_complex_value_t *val)
{
    ngx_uint_t *index;

    index = val->flushes;

    if (index) {
        while (*index != (ngx_uint_t) -1) {

            if (r->variables[*index].no_cacheable) {
                r->variables[*index].valid = 0;
                r->variables[*index].not_found = 0;
            }

            index++;
        }
    }
}

//从val->lengths和val->values(一般都是一些变量或者正则表达式)  这里面的code中解析出对应的普通字符串值到value中
ngx_int_t
ngx_http_complex_value(ngx_http_request_t *r, ngx_http_complex_value_t *val,
    ngx_str_t *value)
{
    size_t                        len;
    ngx_http_script_code_pt       code;
    ngx_http_script_len_code_pt   lcode;
    ngx_http_script_engine_t      e;

    if (val->lengths == NULL) {
        *value = val->value;
        return NGX_OK;
    }

    ngx_http_script_flush_complex_value(r, val);

    ngx_memzero(&e, sizeof(ngx_http_script_engine_t));

    e.ip = val->lengths;
    e.request = r;
    e.flushed = 1;

    len = 0;

    while (*(uintptr_t *) e.ip) {
        lcode = *(ngx_http_script_len_code_pt *) e.ip;
        len += lcode(&e);
    }

    value->len = len;
    value->data = ngx_pnalloc(r->pool, len);
    if (value->data == NULL) {
        return NGX_ERROR;
    }

    e.ip = val->values;
    e.pos = value->data;
    e.buf = *value;

    while (*(uintptr_t *) e.ip) {
        code = *(ngx_http_script_code_pt *) e.ip;
        code((ngx_http_script_engine_t *) &e);
    }

    *value = e.buf;

    return NGX_OK;
}


ngx_int_t
ngx_http_compile_complex_value(ngx_http_compile_complex_value_t *ccv)
{
    ngx_str_t                  *v;
    ngx_uint_t                  i, n, nv, nc;
    ngx_array_t                 flushes, lengths, values, *pf, *pl, *pv;
    ngx_http_script_compile_t   sc;

    v = ccv->value;

    nv = 0;
    nc = 0;

    for (i = 0; i < v->len; i++) {
        if (v->data[i] == '$') {
            if (v->data[i + 1] >= '1' && v->data[i + 1] <= '9') {
                nc++;

            } else {
                nv++;
            }
        }
    }

    if ((v->len == 0 || v->data[0] != '$')
        && (ccv->conf_prefix || ccv->root_prefix))
    {
        if (ngx_conf_full_name(ccv->cf->cycle, v, ccv->conf_prefix) != NGX_OK) {
            return NGX_ERROR;
        }

        ccv->conf_prefix = 0;
        ccv->root_prefix = 0;
    }

    ccv->complex_value->value = *v;
    ccv->complex_value->flushes = NULL;
    ccv->complex_value->lengths = NULL;
    ccv->complex_value->values = NULL;

    if (nv == 0 && nc == 0) {
        return NGX_OK;
    }

    n = nv + 1;

    if (ngx_array_init(&flushes, ccv->cf->pool, n, sizeof(ngx_uint_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    n = nv * (2 * sizeof(ngx_http_script_copy_code_t)
                  + sizeof(ngx_http_script_var_code_t))
        + sizeof(uintptr_t);

    if (ngx_array_init(&lengths, ccv->cf->pool, n, 1) != NGX_OK) {
        return NGX_ERROR;
    }

    n = (nv * (2 * sizeof(ngx_http_script_copy_code_t)
                   + sizeof(ngx_http_script_var_code_t))
                + sizeof(uintptr_t)
                + v->len
                + sizeof(uintptr_t) - 1)
            & ~(sizeof(uintptr_t) - 1);

    if (ngx_array_init(&values, ccv->cf->pool, n, 1) != NGX_OK) {
        return NGX_ERROR;
    }

    pf = &flushes;
    pl = &lengths;
    pv = &values;

    ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

    sc.cf = ccv->cf;
    sc.source = v;
    sc.flushes = &pf;
    sc.lengths = &pl;
    sc.values = &pv;
    sc.complete_lengths = 1;
    sc.complete_values = 1;
    sc.zero = ccv->zero;
    sc.conf_prefix = ccv->conf_prefix;
    sc.root_prefix = ccv->root_prefix;

    if (ngx_http_script_compile(&sc) != NGX_OK) {
        return NGX_ERROR;
    }

    if (flushes.nelts) {
        ccv->complex_value->flushes = flushes.elts;
        ccv->complex_value->flushes[flushes.nelts] = (ngx_uint_t) -1;
    }

    ccv->complex_value->lengths = lengths.elts;
    ccv->complex_value->values = values.elts;

    return NGX_OK;
}

//secure_link md5_str, 120配置
char *
ngx_http_set_complex_value_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_str_t                          *value;
    ngx_http_complex_value_t          **cv;
    ngx_http_compile_complex_value_t    ccv;

    cv = (ngx_http_complex_value_t **) (p + cmd->offset);

    if (*cv != NULL) {
        return "duplicate";
    }

    *cv = ngx_palloc(cf->pool, sizeof(ngx_http_complex_value_t));
    if (*cv == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = *cv;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

//ngx_http_set_predicate_slot赋值,在ngx_http_test_predicates解析
ngx_int_t
ngx_http_test_predicates(ngx_http_request_t *r, ngx_array_t *predicates)
{
    ngx_str_t                  val;
    ngx_uint_t                 i;
    ngx_http_complex_value_t  *cv;

    if (predicates == NULL) {
        return NGX_OK;
    }

    cv = predicates->elts;

    for (i = 0; i < predicates->nelts; i++) {
        if (ngx_http_complex_value(r, &cv[i], &val) != NGX_OK) {
            return NGX_ERROR;
        }

        if (val.len && (val.len != 1 || val.data[0] != '0')) {
            return NGX_DECLINED;
        }
    }

    return NGX_OK;
}

//proxy_cache_bypass fastcgi_cache_bypass 调用ngx_http_set_predicate_slot赋值
//proxy_cache_bypass  xx1 xx2设置的xx2不为空或者不为0，则不会从缓存中取，而是直接冲后端读取
//proxy_no_cache  xx1 xx2设置的xx2不为空或者不为0，则后端回来的数据不会被缓存
char * //ngx_http_set_predicate_slot赋值,在ngx_http_test_predicates解析
ngx_http_set_predicate_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_str_t                          *value;
    ngx_uint_t                          i;
    ngx_array_t                       **a;
    ngx_http_complex_value_t           *cv;
    ngx_http_compile_complex_value_t    ccv;

    a = (ngx_array_t **) (p + cmd->offset);

    if (*a == NGX_CONF_UNSET_PTR) {
        *a = ngx_array_create(cf->pool, 1, sizeof(ngx_http_complex_value_t));
        if (*a == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {
        cv = ngx_array_push(*a);
        if (cv == NULL) {
            return NGX_CONF_ERROR;
        }

        ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

        ccv.cf = cf;
        ccv.value = &value[i];
        ccv.complex_value = cv;

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}

/*
ngx_http_script_compile_t:/脚本化辅助结构体 - 作为脚本化包含变量的参数时的统一输入。
ngx_http_script_copy_code_t - 负责将配置参数项中的固定字符串原封不动的拷贝到最终字符串。
ngx_http_script_var_code_t - 负责将配置参数项中的变量取值后添加到最终字符串。

脚本引擎相关函数
ngx_http_script_variables_count - 根据 $ 出现在的次数统计出一个配置项参数中出现了多少个变量。
ngx_http_script_compile - 将包含变量的参数脚本化，以便需要对参数具体化时调用脚本进行求值。
ngx_http_script_done - 添加结束标志之类的收尾工作。
ngx_http_script_add_copy_code - 处理参数中的固定字符串。这些字符串要和变量的值拼接出最终参数值。
ngx_http_script_run - 
ngx_http_script_add_var_code - 为变量创建取值需要的脚本。在实际变量取值过程中，为了确定包含变量的参数在参数取值后需要的内存块大小，
Nginx 将取值过程分成两个脚本，一个负责计算变量的值长度，另一个负责取出对应的值。
*/
//一般用来获取配置项参数列表中有几个$变量 根据 $ 出现在的次数统计出一个配置项参数中出现了多少个变量。
ngx_uint_t
ngx_http_script_variables_count(ngx_str_t *value)
{
    ngx_uint_t  i, n;

    for (n = 0, i = 0; i < value->len; i++) {
        if (value->data[i] == '$') {
            n++;
        }
    }

    return n;
}

/*
ngx_http_script_compile_t:/脚本化辅助结构体 - 作为脚本化包含变量的参数时的统一输入。
ngx_http_script_copy_code_t - 负责将配置参数项中的固定字符串原封不动的拷贝到最终字符串。
ngx_http_script_var_code_t - 负责将配置参数项中的变量取值后添加到最终字符串。

脚本引擎相关函数
ngx_http_script_variables_count - 根据 $ 出现在的次数统计出一个配置项参数中出现了多少个变量。
ngx_http_script_compile - 将包含变量的参数脚本化，以便需要对参数具体化时调用脚本进行求值。
ngx_http_script_done - 添加结束标志之类的收尾工作。
ngx_http_script_add_copy_code - 处理参数中的固定字符串。这些字符串要和变量的值拼接出最终参数值。
ngx_http_script_run - 
ngx_http_script_add_var_code - 为变量创建取值需要的脚本。在实际变量取值过程中，为了确定包含变量的参数在参数取值后需要的内存块大小，
Nginx 将取值过程分成两个脚本，一个负责计算变量的值长度，另一个负责取出对应的值。
*/
//将包含变量的参数脚本化，以便需要对参数具体化时调用脚本进行求值。

/*例如set $name ${value}abc这种，则name变量的值会通过连续调用ngx_http_script_add_var_code和ngx_http_script_add_copy_code把对应的code放入
ngx_http_rewrite_loc_conf_t->codes中，在ngx_http_rewrite_handler中遍历执行codes就可以把$value字符串和abc字符串拼接在一起，存入name变量
对应的数组中r->variables[code->index]
*/
//参数中的变量 字符串常量等都通过该函数把对应的code添加到codes数组中
ngx_int_t 
ngx_http_script_compile(ngx_http_script_compile_t *sc)
//ngx_http_script_compile 函数对参数项进行整理，存入 ngx_http_log_script_t 变量中：可以参考access_log，ngx_http_log_set_log
{
    u_char       ch;
    ngx_str_t    name;
    ngx_uint_t   i, bracket;

    if (ngx_http_script_init_arrays(sc) != NGX_OK) {
        return NGX_ERROR;
    }

    for (i = 0; i < sc->source->len; /* void */ ) {

        name.len = 0;

        if (sc->source->data[i] == '$') {
             // 以'$'结尾，是有错误的，因为这里处理的都是变量，而不是正则(正则里面末尾带$是有意思的) 
            if (++i == sc->source->len) { //$后面没有名称字符了，
                goto invalid_variable;
            }

#if (NGX_PCRE)
            {
                ngx_uint_t  n;
               /* 
                  注意，在这里所谓的变量有两种，一种是$后面跟字符串的，一种是跟数字的。 
                  这里判断是否是数字形式的变量。 
                 */ 
                if (sc->source->data[i] >= '1' && sc->source->data[i] <= '9') {//以位移的形式保存$1,$2...$9等变量

                    n = sc->source->data[i] - '0';

                    //以位移的形式保存$1,$2...$9等变量，即响应位置上置1来表示，主要的作用是为dup_capture准备， 
                    if (sc->captures_mask & (1 << n)) {
                        sc->dup_capture = 1;
                    }

                        
                     /* 
                        在sc->captures_mask中将数字对应的位置1，那么captures_mask的作用是什么？ 在后面对sc结构体分析时会提到。 
                        */  
                    sc->captures_mask |= 1 << n;
                
                    if (ngx_http_script_add_capture_code(sc, n) != NGX_OK) {
                        return NGX_ERROR;
                    }

                    i++;

                    continue;
                }
            }
#endif

          /* 
             * 这里是个有意思的地方，举个例子，假设有个这样一个配置proxy_pass $host$uritest， 
             * 我们这里其实是想用nginx的两个内置变量，host和uri，但是对于$uritest来说，如果我们 
             * 不加处理，那么在函数里很明显会将uritest这个整体作为一个变量，这显然不是我们想要的。 
             * 那怎么办呢？nginx里面使用"{}"来把一些变量包裹起来，避免跟其他的字符串混在一起，在此处 
             * 我们可以这样用${uri}test，当然变量之后是数字，字母或者下划线之类的字符才有必要这样处理 
             * 代码中体现的很明显。 
             */
            if (sc->source->data[i] == '{') {
                bracket = 1;

                if (++i == sc->source->len) {
                    goto invalid_variable;
                }

                //name用来保存一个分离出的变量   
                name.data = &sc->source->data[i]; 

            } else {
                bracket = 0;
                name.data = &sc->source->data[i];
            }

            for ( /* void */ ; i < sc->source->len; i++, name.len++) {
                ch = sc->source->data[i];
                
                // 在"{}"中的字符串会被分离出来(即break语句)，避免跟后面的字符串混在一起   
                if (ch == '}' && bracket) {
                    i++;
                    bracket = 0;
                    break;
                }

                 /* 
                     变量中允许出现的字符，其他字符都不是变量的字符，所以空格是可以区分变量的。 
                     这个我们在配置里经常可以感觉到，而它的原理就是这里所显示的了 
                    */ 
                if ((ch >= 'A' && ch <= 'Z')
                    || (ch >= 'a' && ch <= 'z')
                    || (ch >= '0' && ch <= '9')
                    || ch == '_')
                {
                    continue;
                }

                break;
            }

            if (bracket) { //如果带有{就必须以}结尾
                ngx_conf_log_error(NGX_LOG_EMERG, sc->cf, 0,
                                   "the closing bracket in \"%V\" "
                                   "variable is missing", &name);
                return NGX_ERROR;
            }

            if (name.len == 0) {
                goto invalid_variable;
            }

            sc->variables++;// 变量计数   

            // 得到一个变量，做处理   
            if (ngx_http_script_add_var_code(sc, &name) != NGX_OK) { 
                return NGX_ERROR;
            }

            continue;
        }

       /*  
          程序到这里意味着一个变量分离出来(是普通字符串)，或者还没有碰到变量，一些非变量的字符串，这里不妨称为”常量字符串“ 
          这里涉及到请求参数部分的处理，比较简单。这个地方一般是在一次分离变量或者常量结束后，后面紧跟'?'的情况 
          相关的处理子在ngx_http_script_add_args_code会设置。 
         */ 
        if (sc->source->data[i] == '?' && sc->compile_args) {
            sc->args = 1;
            sc->compile_args = 0;

            if (ngx_http_script_add_args_code(sc) != NGX_OK) {
                return NGX_ERROR;
            }

            i++;

            continue;
        }
        
        // 这里name保存一段所谓的”常量字符串“   
        name.data = &sc->source->data[i];
        // 分离该常量字符串   
        while (i < sc->source->len) {

            if (sc->source->data[i] == '$') {// 碰到'$'意味着碰到了下一个变量 
                break;
            }

            
            /* 
                 此处意味着我们在一个常量字符串分离过程中遇到了'?'，如果我们不需要对请求参数做特殊处理的话， 
                 即sc->compile_args = 0，那么我们就将其作为常量字符串的一部分来处理。否则，当前的常量字符串会 
                 从'?'处，截断，分成两部分。
               */  
            if (sc->source->data[i] == '?') {

                sc->args = 1;

                if (sc->compile_args) {
                    break;
                }
            }

            i++;
            name.len++;
        }
        
        // 一个常量字符串分离完毕，sc->size统计整个字符串(即sc->source)中，常量字符串的总长度   
        sc->size += name.len;
        // 常量字符串的处理子由这个函数来设置
        if (ngx_http_script_add_copy_code(sc, &name, (i == sc->source->len))
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    // 本次compile结束，做一些收尾善后工作。  
    return ngx_http_script_done(sc);

invalid_variable:

    ngx_conf_log_error(NGX_LOG_EMERG, sc->cf, 0, "invalid variable name");

    return NGX_ERROR;
}

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
u_char *
ngx_http_script_run(ngx_http_request_t *r, ngx_str_t *value,
    void *code_lengths, size_t len, void *code_values)
{
    ngx_uint_t                    i;
    ngx_http_script_code_pt       code;
    ngx_http_script_len_code_pt   lcode;
    ngx_http_script_engine_t      e;
    ngx_http_core_main_conf_t    *cmcf;

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    for (i = 0; i < cmcf->variables.nelts; i++) {
        if (r->variables[i].no_cacheable) {
            r->variables[i].valid = 0;
            r->variables[i].not_found = 0;
        }
    }

    ngx_memzero(&e, sizeof(ngx_http_script_engine_t));

    e.ip = code_lengths;
    e.request = r;
    e.flushed = 1;

    while (*(uintptr_t *) e.ip) {
        lcode = *(ngx_http_script_len_code_pt *) e.ip;
        len += lcode(&e);
    }


    value->len = len;
    value->data = ngx_pnalloc(r->pool, len);
    if (value->data == NULL) {
        return NULL;
    }

    e.ip = code_values;
    e.pos = value->data;

    while (*(uintptr_t *) e.ip) {
        code = *(ngx_http_script_code_pt *) e.ip;
        code((ngx_http_script_engine_t *) &e);
    }

    return e.pos;
}


void
ngx_http_script_flush_no_cacheable_variables(ngx_http_request_t *r,
    ngx_array_t *indices)
{
    ngx_uint_t  n, *index;

    if (indices) {
        index = indices->elts;
        for (n = 0; n < indices->nelts; n++) {
            if (r->variables[index[n]].no_cacheable) {
                r->variables[index[n]].valid = 0;
                r->variables[index[n]].not_found = 0;
            }
        }
    }
}

static ngx_int_t
ngx_http_script_init_arrays(ngx_http_script_compile_t *sc)
{
    ngx_uint_t   n;

    if (sc->flushes && *sc->flushes == NULL) {
        n = sc->variables ? sc->variables : 1;
        *sc->flushes = ngx_array_create(sc->cf->pool, n, sizeof(ngx_uint_t));
        if (*sc->flushes == NULL) {
            return NGX_ERROR;
        }
    }

    if (*sc->lengths == NULL) {
        n = sc->variables * (2 * sizeof(ngx_http_script_copy_code_t)
                             + sizeof(ngx_http_script_var_code_t))
            + sizeof(uintptr_t);

        //开辟n个1字节空间的数组，每个数组成员就1个字节
        *sc->lengths = ngx_array_create(sc->cf->pool, n, 1);
        if (*sc->lengths == NULL) {
            return NGX_ERROR;
        }
    }

    if (*sc->values == NULL) {
        n = (sc->variables * (2 * sizeof(ngx_http_script_copy_code_t)
                              + sizeof(ngx_http_script_var_code_t))
                + sizeof(uintptr_t)
                + sc->source->len
                + sizeof(uintptr_t) - 1)
            & ~(sizeof(uintptr_t) - 1);

        *sc->values = ngx_array_create(sc->cf->pool, n, 1);
        if (*sc->values == NULL) {
            return NGX_ERROR;
        }
    }

    sc->variables = 0;

    return NGX_OK;
}

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
static ngx_int_t
ngx_http_script_done(ngx_http_script_compile_t *sc)//添加结束标志之类的收尾工作。
{
    ngx_str_t    zero;
    uintptr_t   *code;

    if (sc->zero) {

        zero.len = 1;
        zero.data = (u_char *) "\0";

        if (ngx_http_script_add_copy_code(sc, &zero, 0) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (sc->conf_prefix || sc->root_prefix) {
        if (ngx_http_script_add_full_name_code(sc) != NGX_OK) {
            return NGX_ERROR;
        }
    }

    if (sc->complete_lengths) {
        code = ngx_http_script_add_code(*sc->lengths, sizeof(uintptr_t), NULL);
        if (code == NULL) {
            return NGX_ERROR;
        }

        *code = (uintptr_t) NULL;
    }

    if (sc->complete_values) {
        code = ngx_http_script_add_code(*sc->values, sizeof(uintptr_t),
                                        &sc->main);
        if (code == NULL) {
            return NGX_ERROR;
        }

        *code = (uintptr_t) NULL;
    }

    return NGX_OK;
}


void *
ngx_http_script_start_code(ngx_pool_t *pool, ngx_array_t **codes, size_t size)
{
    if (*codes == NULL) {
        *codes = ngx_array_create(pool, 256, 1);
        if (*codes == NULL) {
            return NULL;
        }
    }

    return ngx_array_push_n(*codes, size);
}

/* 从 codes内存块中开辟 size 字节 的空间存放用于获取变量对应的值长度的脚本
 */
void *
ngx_http_script_add_code(ngx_array_t *codes, size_t size, void *code)
{
    u_char  *elts, **p;
    void    *new;

    elts = codes->elts;

    new = ngx_array_push_n(codes, size);//codes->elts可能会变化的。如果数组已经满了需要申请一块大的内存
    if (new == NULL) {
        return NULL;
    }

    if (code) {
        if (elts != codes->elts) { //如果内存变化了，
            p = code; //因为code参数表的是&sc->main这种，也就是指向本数组的数据，因此需要更新一下位移信息。
            *p += (u_char *) codes->elts - elts; //这是什么意思，加上了新申请的内存的位移。
        }
    }

    return new;
}

//配置中的$name这种变量字符串调用ngx_http_script_add_var_code，普通的字符串调用ngx_http_script_add_copy_code
//ngx_http_script_add_copy_code - 处理参数中的固定字符串。这些字符串要和变量的值拼接出最终参数值。
static ngx_int_t
ngx_http_script_add_copy_code(ngx_http_script_compile_t *sc, ngx_str_t *value,
    ngx_uint_t last) //last标记是否是参数列表中的的最后一个参数
{
    u_char                       *p;
    size_t                        size, len, zero;
    ngx_http_script_copy_code_t  *code;

    zero = (sc->zero && last);
    len = value->len + zero;

    //最终lengths数组中只是存的变量长度和code，values数组中存有变量长度，变量值和对应的code

    
    /* 从 lengths 内存块中开辟 sizeof(ngx_http_script_copy_code_t) 字节的空间用于存放固定字符串的长度 */
    code = ngx_http_script_add_code(*sc->lengths,
                                    sizeof(ngx_http_script_copy_code_t), NULL);
    if (code == NULL) {
        return NGX_ERROR;
    }

    
    /* 被调用时返回 len */ //给lengths中的code赋值
    code->code = (ngx_http_script_code_pt) ngx_http_script_copy_len_code;
    code->len = len;

    
    /* 固定字符串和用于获取此固定字符串的脚本需要的存储空间 ，这里比ngx_http_script_copy_code_t结构多分配了size - ngx_http_script_copy_code_t空间来存放value数据*/
    size = (sizeof(ngx_http_script_copy_code_t) + len + sizeof(uintptr_t) - 1)
            & ~(sizeof(uintptr_t) - 1);
    
    /* 从 values 内存块中开辟 size 字节的空间用于存储固定字符串和操作脚本 */
    code = ngx_http_script_add_code(*sc->values, size, &sc->main);
    if (code == NULL) {
        return NGX_ERROR;
    }
    /* 被调用时将其后的固定字符串返回 */
    code->code = ngx_http_script_copy_code;
    code->len = len;

    
    /* 将固定字符串暂存入 values 中 */
    p = ngx_cpymem((u_char *) code + sizeof(ngx_http_script_copy_code_t),
                   value->data, value->len); //把value数据拷贝到ngx_http_script_copy_code_t屁股后面，

    if (zero) {
        *p = '\0';
        sc->zero = 0;
    }

    return NGX_OK;
}


size_t
ngx_http_script_copy_len_code(ngx_http_script_engine_t *e)
{
    ngx_http_script_copy_code_t  *code;

    code = (ngx_http_script_copy_code_t *) e->ip;

    e->ip += sizeof(ngx_http_script_copy_code_t);

    return code->len;
}

//ngx_http_script_copy_code为拷贝变量到p buf中，ngx_http_script_copy_var_code为拷贝变量对应的value
void
ngx_http_script_copy_code(ngx_http_script_engine_t *e)
{
    u_char                       *p;
    ngx_http_script_copy_code_t  *code;

    code = (ngx_http_script_copy_code_t *) e->ip;

    p = e->pos;

    if (!e->skip) {//在该函数中无需拷贝数据
        e->pos = ngx_copy(p, e->ip + sizeof(ngx_http_script_copy_code_t),
                          code->len);
    }

    e->ip += sizeof(ngx_http_script_copy_code_t)
          + ((code->len + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1));

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script copy: \"%*s\"", e->pos - p, p);
}

/*
ngx_http_script_compile_t:/脚本化辅助结构体 - 作为脚本化包含变量的参数时的统一输入。
ngx_http_script_copy_code_t - 负责将配置参数项中的固定字符串原封不动的拷贝到最终字符串。
ngx_http_script_var_code_t - 负责将配置参数项中的变量取值后添加到最终字符串。

脚本引擎相关函数
ngx_http_script_variables_count - 根据 $ 出现在的次数统计出一个配置项参数中出现了多少个变量。
ngx_http_script_compile - 将包含变量的参数脚本化，以便需要对参数具体化时调用脚本进行求值。
ngx_http_script_done - 添加结束标志之类的收尾工作。
ngx_http_script_run - 
ngx_http_script_add_copy_code - 处理参数中的固定字符串。这些字符串要和变量的值拼接出最终参数值。
ngx_http_script_add_var_code - 为变量创建取值需要的脚本。在实际变量取值过程中，为了确定包含变量的参数在参数取值后需要的内存块大小，
Nginx 将取值过程分成两个脚本，一个负责计算变量的值长度，另一个负责取出对应的值。
*/ //通过name查找到variables中的小标，然后从数组中获取ngx_http_script_var_code_t节点，把index和code赋值给他
//配置中的$name这种变量字符串调用ngx_http_script_add_var_code，普通的字符串调用ngx_http_script_add_copy_code

/*
如果是set $xxx $bbb该函数配合ngx_http_script_complex_value_code读取，ngx_http_script_complex_value_code根据code->lengths(ngx_http_script_copy_var_len_code)
来确定变量字符串的长度，然后开辟对应的e->buf空间，最后在ngx_http_script_copy_var_code中把变量名拷贝到e->buf空间中
*/
static ngx_int_t
ngx_http_script_add_var_code(ngx_http_script_compile_t *sc, ngx_str_t *name)//name添加到flushes  lengths  values中
{
    ngx_int_t                    index, *p;
    ngx_http_script_var_code_t  *code;

    //根据变量名字，获取其在&cmcf->variables里面的下标。如果没有，就新建它。
    index = ngx_http_get_variable_index(sc->cf, name);

    if (index == NGX_ERROR) {
        return NGX_ERROR;
    }

    if (sc->flushes) {
        p = ngx_array_push(*sc->flushes);
        if (p == NULL) {
            return NGX_ERROR;
        }

        *p = index;
    }

    
    /* 从 lengths 内存块中开辟 sizeof(ngx_http_script_var_code_t) 字节
          的空间存放用于获取变量对应的值长度的脚本
        */ //lengths数组中的每个成员就一字节
    code = ngx_http_script_add_code(*sc->lengths,
                                    sizeof(ngx_http_script_var_code_t), NULL); //lengths中的节点在ngx_http_script_complex_value_code执行
    if (code == NULL) {
        return NGX_ERROR;
    }

    code->code = (ngx_http_script_code_pt) ngx_http_script_copy_var_len_code;
    code->index = (uintptr_t) index;

    code = ngx_http_script_add_code(*sc->values,
                                    sizeof(ngx_http_script_var_code_t),
                                    &sc->main);
    if (code == NULL) {
        return NGX_ERROR;
    }
    
    /* ngx_http_script_copy_var_code 用于获取 index 对应的变量取值 */
    code->code = ngx_http_script_copy_var_code;
    code->index = (uintptr_t) index; //ngx_http_variable_t->index

    return NGX_OK;
}

//获取inex对应变量值的字符串长度
size_t
ngx_http_script_copy_var_len_code(ngx_http_script_engine_t *e)
{
    ngx_http_variable_value_t   *value;
    ngx_http_script_var_code_t  *code;

    code = (ngx_http_script_var_code_t *) e->ip;

    e->ip += sizeof(ngx_http_script_var_code_t);

    if (e->flushed) {
        value = ngx_http_get_indexed_variable(e->request, code->index);

    } else {
        value = ngx_http_get_flushed_variable(e->request, code->index);
    }

    if (value && !value->not_found) {
        return value->len;
    }

    return 0;
}

// /* ngx_http_script_copy_var_code 用于获取 index 对应的变量取值 */
void //获取变量值  //ngx_http_script_copy_code为拷贝变量到p buf中，ngx_http_script_copy_var_code为拷贝变量对应的value
ngx_http_script_copy_var_code(ngx_http_script_engine_t *e)
{
    u_char                      *p;
    ngx_http_variable_value_t   *value;
    ngx_http_script_var_code_t  *code;

    code = (ngx_http_script_var_code_t *) e->ip;

    e->ip += sizeof(ngx_http_script_var_code_t);

    if (!e->skip) {

        if (e->flushed) {
            value = ngx_http_get_indexed_variable(e->request, code->index);

        } else {
            value = ngx_http_get_flushed_variable(e->request, code->index);
        }

        if (value && !value->not_found) {
            p = e->pos;
            e->pos = ngx_copy(p, value->data, value->len); //转存index对应的变量值到e->buf中

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP,
                           e->request->connection->log, 0,
                           "http script var: \"%*s\"", e->pos - p, p);
        }
    }
}

//set $ttt  xxxx?bbb这种，解析的参数中带有?会执行该函数，
static ngx_int_t
ngx_http_script_add_args_code(ngx_http_script_compile_t *sc)
{
    uintptr_t   *code;

    code = ngx_http_script_add_code(*sc->lengths, sizeof(uintptr_t), NULL);
    if (code == NULL) {
        return NGX_ERROR;
    }

    *code = (uintptr_t) ngx_http_script_mark_args_code;

    code = ngx_http_script_add_code(*sc->values, sizeof(uintptr_t), &sc->main);
    if (code == NULL) {
        return NGX_ERROR;
    }

    *code = (uintptr_t) ngx_http_script_start_args_code;

    return NGX_OK;
}


size_t
ngx_http_script_mark_args_code(ngx_http_script_engine_t *e)
{
    e->is_args = 1;
    e->ip += sizeof(uintptr_t);

    return 1;
}


void
ngx_http_script_start_args_code(ngx_http_script_engine_t *e)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script args");

    e->is_args = 1;
    e->args = e->pos;
    e->ip += sizeof(uintptr_t);
}


#if (NGX_PCRE)

/*
1.	调用正则表达式引擎编译URL参数行，如果匹配失败，则e->ip += code->next;让调用方调到下一个表达式块进行解析。
2.如果成功，调用code->lengths，从而获取正则表达式替换后的字符串长度，以备在此函数返回后的code函数调用中能够存储新字符串长度。
*/
//ngx_http_script_regex_start_code与ngx_http_script_regex_end_code配对使用
void
ngx_http_script_regex_start_code(ngx_http_script_engine_t *e)
{
//匹配正则表达式，计算目标字符串长度并分配空间。这个函数是每条rewrite语句最先调用的解析函数，
//本函数负责匹配，和目标字符串长度计算，依据lengths lcodes数组进行
    size_t                         len;
    ngx_int_t                      rc;
    ngx_uint_t                     n;
    ngx_http_request_t            *r;
    ngx_http_script_engine_t       le;
    ngx_http_script_len_code_pt    lcode;
    ngx_http_script_regex_code_t  *code;

    code = (ngx_http_script_regex_code_t *) e->ip;

    r = e->request;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http script regex: \"%V\"", &code->name);

    if (code->uri) { //rewrite配置,就是比较uri和rewrite xxx yyy break;中的yyy是否匹配，
        e->line = r->uri;
    } else { //if配置
        e->sp--;
        e->line.len = e->sp->len;
        e->line.data = e->sp->data;
    }

    //下面用已经编译的regex 跟e->line去匹配，看看是否匹配成功。
    rc = ngx_http_regex_exec(r, code->regex, &e->line);

    if (rc == NGX_DECLINED) {
        if (e->log || (r->connection->log->log_level & NGX_LOG_DEBUG_HTTP)) {
            ngx_log_error(NGX_LOG_NOTICE, r->connection->log, 0,
                          "\"%V\" does not match \"%V\"",
                          &code->name, &e->line);
        }

        r->ncaptures = 0;

        if (code->test) { //if{}配置才会置1
            if (code->negative_test) {   
                e->sp->len = 1;
                e->sp->data = (u_char *) "1";

            } else {
                e->sp->len = 0;
                e->sp->data = (u_char *) "";
            }

            e->sp++; //移动到下一个节点。返回。

            e->ip += sizeof(ngx_http_script_regex_code_t); 
            return;
        }
        
        //next的含义为;如果当前code匹配失败，那么下一个code的位移是在什么地方，这些东西全部放在一个数组里面的。
        e->ip += code->next;
        return;
    }

    if (rc == NGX_ERROR) {
        e->ip = ngx_http_script_exit;
        e->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        return;
    }

    if (e->log || (r->connection->log->log_level & NGX_LOG_DEBUG_HTTP)) {
        ngx_log_error(NGX_LOG_NOTICE, r->connection->log, 0,
                      "\"%V\" matches \"%V\"", &code->name, &e->line);
    }

    if (code->test) {//if{}配置才会置1 //如果匹配成功了，那设置一个标志吧，这样比如做if匹配的时候就能通过查看堆栈的值来知道是否成功。
        if (code->negative_test) {
            e->sp->len = 0;
            e->sp->data = (u_char *) "";

        } else {
            e->sp->len = 1;
            e->sp->data = (u_char *) "1";
        }

        e->sp++;

        e->ip += sizeof(ngx_http_script_regex_code_t);
        return;
    }

    if (code->status) { //如果是rewrite命令配置，见ngx_http_rewrite赋值
        e->status = code->status;

        if (!code->redirect) {
            e->ip = ngx_http_script_exit;
            return;
        }
    }

    if (code->uri) {
        r->internal = 1;
        r->valid_unparsed_uri = 0;

        if (code->break_cycle) { //rewrite最后的参数是break，将rewrite后的地址在当前location标签中执行
            r->valid_location = 0;
            r->uri_changed = 0; //将uri_changed设置为0后，也就标志说URL没有变化，那么，
            //在ngx_http_core_post_rewrite_phase中就不会执行里面的if语句，也就不会再次走到find config的过程了，而是继续处理后面的。
            //不然正常情况，rewrite成功后是会重新来一次的，相当于一个全新的请求。

        } else {
            r->uri_changed = 1; //需要rewrite从新循环，见ngx_http_core_post_rewrite_phase  重新进行重定向查找过程
        }
    }

    if (code->lengths == NULL) {//如果后面部分是简单字符串比如 rewrite ^(.*)$ http://chenzhenianqing.cn break;
        e->buf.len = code->size;

        if (code->uri) {
            if (r->ncaptures && (r->quoted_uri || r->plus_in_uri)) {
                e->buf.len += 2 * ngx_escape_uri(NULL, r->uri.data, r->uri.len,
                                                 NGX_ESCAPE_ARGS);
            }
        }

        for (n = 2; n < r->ncaptures; n += 2) {
            e->buf.len += r->captures[n + 1] - r->captures[n];
        }

    } else { /*一个个去处理复杂表达式，但是这里其实只是算一下大小的，
        真正的数据拷贝在上层的code获取。比如 rewrite ^(.*)$ http://$http_host.mp4 break;
        //下面会分步的，拼装出后面的url,对于上面的例子，为
			ngx_http_script_copy_len_code		7
			ngx_http_script_copy_var_len_code 	18
			ngx_http_script_copy_len_code		4	=== 29 
		这里只是求一下长度，调用lengths求长度。数据拷贝在ngx_http_rewrite_handler中，本函数返回后就调用如下过程拷贝数据: 
			ngx_http_script_copy_code		拷贝"http://" 到e->buf
			ngx_http_script_copy_var_code	拷贝"115.28.34.175:8881"
			ngx_http_script_copy_code 		拷贝".mp4"
        */
        ngx_memzero(&le, sizeof(ngx_http_script_engine_t));

        le.ip = code->lengths->elts;
        le.line = e->line;
        le.request = r;
        le.quote = code->redirect;

        len = 0;
 
        while (*(uintptr_t *) le.ip) { //求字符串的总长度
            lcode = *(ngx_http_script_len_code_pt *) le.ip;  
            len += lcode(&le);
        }

        e->buf.len = len;//记住总长度。
    }

    if (code->add_args && r->args.len) { //是否需要自动增加参数。如果配置行的后面显示的加上了?符号，则nginx不会追加参数。
        e->buf.len += r->args.len + 1; //为后面的ngx_http_script_regex_end_code做准备
    }

    //为前面这些code解析的到的参数值的总长度计算出来了，用于在执行ngx_http_script_regex_end_code的时候把相关字符串拷贝到e->buf
    e->buf.data = ngx_pnalloc(r->pool, e->buf.len);
    if (e->buf.data == NULL) {
        e->ip = ngx_http_script_exit;
        e->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        return;
    }

    e->quote = code->redirect;

    e->pos = e->buf.data;

    e->ip += sizeof(ngx_http_script_regex_code_t);
}


//ngx_http_script_regex_start_code与ngx_http_script_regex_end_code配对使用
void
ngx_http_script_regex_end_code(ngx_http_script_engine_t *e)
{//貌似没干什么事情，如果是redirect，急设置了一下头部header的location，该302了。
    u_char                            *dst, *src;
    ngx_http_request_t                *r;
    ngx_http_script_regex_end_code_t  *code;

    code = (ngx_http_script_regex_end_code_t *) e->ip;

    r = e->request;

    e->quote = 0;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http script regex end");

    if (code->redirect) {

        dst = e->buf.data;
        src = e->buf.data;

        ngx_unescape_uri(&dst, &src, e->pos - e->buf.data,
                         NGX_UNESCAPE_REDIRECT);

        if (src < e->pos) {
            dst = ngx_movemem(dst, src, e->pos - src); //获取反编码的结果
        }

        e->pos = dst;

        if (code->add_args && r->args.len) {//如果uri有带?,则添加?和问号后面的参数到反编码的字符串后面，其实就是把请求的uri中?前面的字符串反编码
            *e->pos++ = (u_char) (code->args ? '&' : '?');
            e->pos = ngx_copy(e->pos, r->args.data, r->args.len); //
        }

        e->buf.len = e->pos - e->buf.data;

        if (e->log || (r->connection->log->log_level & NGX_LOG_DEBUG_HTTP)) {
            ngx_log_error(NGX_LOG_NOTICE, r->connection->log, 0,
                          "rewritten redirect: \"%V\"", &e->buf);
        }

        ngx_http_clear_location(r);

        r->headers_out.location = ngx_list_push(&r->headers_out.headers);
        if (r->headers_out.location == NULL) {
            e->ip = ngx_http_script_exit;
            e->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
            return;
        }

        /*
          rewrite ^(.*)$ http://$1.mp4 break; 如果uri为http://10.135.0.1/aaa,则location中存储的是aaa.mp4
          */
        r->headers_out.location->hash = 1;
        ngx_str_set(&r->headers_out.location->key, "Location");
        r->headers_out.location->value = e->buf; //指向rewrite后的新的重定向uri

        e->ip += sizeof(ngx_http_script_regex_end_code_t);
        return;
    }

    if (e->args) {
        e->buf.len = e->args - e->buf.data;

        if (code->add_args && r->args.len) {
            *e->pos++ = '&';
            e->pos = ngx_copy(e->pos, r->args.data, r->args.len);
        }

        r->args.len = e->pos - e->args;
        r->args.data = e->args;

        e->args = NULL;

    } else {
        e->buf.len = e->pos - e->buf.data;

        if (!code->add_args) {
            r->args.len = 0;
        }
    }

    /* 
      配置为:
      location ~* /1mytest  {			
            rewrite   ^.*$ www.11.com/ last;		
       }  
      uri为:http://10.135.10.167/1mytest ,则走到这里后，uri会变为www.11.com/
     */
    if (e->log || (r->connection->log->log_level & NGX_LOG_DEBUG_HTTP)) {
        ngx_log_error(NGX_LOG_NOTICE, r->connection->log, 0,
                      "rewritten data: \"%V\", args: \"%V\"",
                      &e->buf, &r->args);
    }

    if (code->uri) {
        r->uri = e->buf; //uri指向新的rewrite后的uri

        if (r->uri.len == 0) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "the rewritten URI has a zero length");
            e->ip = ngx_http_script_exit;
            e->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
            return;
        }

        ngx_http_set_exten(r);
    }

    e->ip += sizeof(ngx_http_script_regex_end_code_t);
}

//$1,$2...$9等变量  配合ngx_http_script_complex_value_code阅读
static ngx_int_t
ngx_http_script_add_capture_code(ngx_http_script_compile_t *sc, ngx_uint_t n) //n为$3中的3
{
    ngx_http_script_copy_capture_code_t  *code;

    code = ngx_http_script_add_code(*sc->lengths,
                                    sizeof(ngx_http_script_copy_capture_code_t),
                                    NULL);
    if (code == NULL) {
        return NGX_ERROR;
    }

    code->code = (ngx_http_script_code_pt)
                      ngx_http_script_copy_capture_len_code; //定位$1变量值的字符串长度
    code->n = 2 * n;


    code = ngx_http_script_add_code(*sc->values,
                                    sizeof(ngx_http_script_copy_capture_code_t),
                                    &sc->main);
    if (code == NULL) {
        return NGX_ERROR;
    }

    code->code = ngx_http_script_copy_capture_code; //获取变量$1变量值的字符串
    code->n = 2 * n;

    if (sc->ncaptures < n) {
        sc->ncaptures = n;
    }

    return NGX_OK;
}


size_t
ngx_http_script_copy_capture_len_code(ngx_http_script_engine_t *e)
{
    int                                  *cap;
    u_char                               *p;
    ngx_uint_t                            n;
    ngx_http_request_t                   *r;
    ngx_http_script_copy_capture_code_t  *code;

    r = e->request;

    code = (ngx_http_script_copy_capture_code_t *) e->ip;

    e->ip += sizeof(ngx_http_script_copy_capture_code_t);

    n = code->n;

    if (n < r->ncaptures) {

        cap = r->captures;

        if ((e->is_args || e->quote)
            && (e->request->quoted_uri || e->request->plus_in_uri))
        {
            p = r->captures_data;

            return cap[n + 1] - cap[n]
                   + 2 * ngx_escape_uri(NULL, &p[cap[n]], cap[n + 1] - cap[n],
                                        NGX_ESCAPE_ARGS);
        } else {
            return cap[n + 1] - cap[n];
        }
    }

    return 0;
}


void
ngx_http_script_copy_capture_code(ngx_http_script_engine_t *e)
{
    int                                  *cap;
    u_char                               *p, *pos;
    ngx_uint_t                            n;
    ngx_http_request_t                   *r;
    ngx_http_script_copy_capture_code_t  *code;

    r = e->request;

    code = (ngx_http_script_copy_capture_code_t *) e->ip;

    e->ip += sizeof(ngx_http_script_copy_capture_code_t);

    n = code->n;

    pos = e->pos;

    if (n < r->ncaptures) {

        cap = r->captures;
        p = r->captures_data;

        if ((e->is_args || e->quote)
            && (e->request->quoted_uri || e->request->plus_in_uri))
        {
            e->pos = (u_char *) ngx_escape_uri(pos, &p[cap[n]],
                                               cap[n + 1] - cap[n],
                                               NGX_ESCAPE_ARGS);
        } else {
            e->pos = ngx_copy(pos, &p[cap[n]], cap[n + 1] - cap[n]);
        }
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script capture: \"%*s\"", e->pos - pos, pos);
}

#endif


static ngx_int_t
ngx_http_script_add_full_name_code(ngx_http_script_compile_t *sc)
{
    ngx_http_script_full_name_code_t  *code;

    code = ngx_http_script_add_code(*sc->lengths,
                                    sizeof(ngx_http_script_full_name_code_t),
                                    NULL);
    if (code == NULL) {
        return NGX_ERROR;
    }

    code->code = (ngx_http_script_code_pt) ngx_http_script_full_name_len_code;
    code->conf_prefix = sc->conf_prefix;

    code = ngx_http_script_add_code(*sc->values,
                                    sizeof(ngx_http_script_full_name_code_t),
                                    &sc->main);
    if (code == NULL) {
        return NGX_ERROR;
    }

    code->code = ngx_http_script_full_name_code;
    code->conf_prefix = sc->conf_prefix;

    return NGX_OK;
}


static size_t
ngx_http_script_full_name_len_code(ngx_http_script_engine_t *e)
{
    ngx_http_script_full_name_code_t  *code;

    code = (ngx_http_script_full_name_code_t *) e->ip;

    e->ip += sizeof(ngx_http_script_full_name_code_t);

    return code->conf_prefix ? ngx_cycle->conf_prefix.len:
                               ngx_cycle->prefix.len;
}


static void
ngx_http_script_full_name_code(ngx_http_script_engine_t *e)
{
    ngx_http_script_full_name_code_t  *code;

    ngx_str_t  value, *prefix;

    code = (ngx_http_script_full_name_code_t *) e->ip;

    value.data = e->buf.data;
    value.len = e->pos - e->buf.data;

    prefix = code->conf_prefix ? (ngx_str_t *) &ngx_cycle->conf_prefix:
                                 (ngx_str_t *) &ngx_cycle->prefix;

    if (ngx_get_full_name(e->request->pool, prefix, &value) != NGX_OK) {
        e->ip = ngx_http_script_exit;
        e->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        return;
    }

    e->buf = value;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script fullname: \"%V\"", &value);

    e->ip += sizeof(ngx_http_script_full_name_code_t);
}

//根据return code；配置发送响应。
//和ngx_http_rewrite_handler(ngx_http_request_t *r) 配合阅读，e->ip = rlcf->codes->elts; 
void
ngx_http_script_return_code(ngx_http_script_engine_t *e)
{
    ngx_http_script_return_code_t  *code;

    code = (ngx_http_script_return_code_t *) e->ip;

    if (code->status < NGX_HTTP_BAD_REQUEST
        || code->text.value.len
        || code->text.lengths)
    {
        e->status = ngx_http_send_response(e->request, code->status, NULL,
                                           &code->text);
    } else {
        e->status = code->status;
    }

    e->ip = ngx_http_script_exit; //执行该code后，不会再执行之前本来在该code后面的其他所有code
}


void
ngx_http_script_break_code(ngx_http_script_engine_t *e)
{
    e->request->uri_changed = 0; //该值为0，表示不会再重复rewrite find

    e->ip = ngx_http_script_exit; //停止后面的各种变量赋值解析等，也就是停止脚本引擎
}


void
ngx_http_script_if_code(ngx_http_script_engine_t *e)
{
    ngx_http_script_if_code_t  *code;

    code = (ngx_http_script_if_code_t *) e->ip;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script if");

    e->sp--;

    if (e->sp->len && (e->sp->len != 1 || e->sp->data[0] != '0')) {
        if (code->loc_conf) {
            e->request->loc_conf = code->loc_conf;
            ngx_http_update_location_config(e->request);
        }

        e->ip += sizeof(ngx_http_script_if_code_t);
        return;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script if: false");

    e->ip += code->next;
}


void
ngx_http_script_equal_code(ngx_http_script_engine_t *e)
{
    ngx_http_variable_value_t  *val, *res;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script equal");

    e->sp--;
    val = e->sp;
    res = e->sp - 1;

    e->ip += sizeof(uintptr_t);

    if (val->len == res->len
        && ngx_strncmp(val->data, res->data, res->len) == 0)
    {
        *res = ngx_http_variable_true_value;
        return;
    }

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script equal: no");

    *res = ngx_http_variable_null_value;
}


void
ngx_http_script_not_equal_code(ngx_http_script_engine_t *e)
{
    ngx_http_variable_value_t  *val, *res;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script not equal");

    e->sp--;
    val = e->sp;
    res = e->sp - 1;

    e->ip += sizeof(uintptr_t);

    if (val->len == res->len
        && ngx_strncmp(val->data, res->data, res->len) == 0)
    {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                       "http script not equal: no");

        *res = ngx_http_variable_null_value;
        return;
    }

    *res = ngx_http_variable_true_value;
}


void
ngx_http_script_file_code(ngx_http_script_engine_t *e)
{
    ngx_str_t                     path;
    ngx_http_request_t           *r;
    ngx_open_file_info_t          of;
    ngx_http_core_loc_conf_t     *clcf;
    ngx_http_variable_value_t    *value;
    ngx_http_script_file_code_t  *code;

    value = e->sp - 1;

    code = (ngx_http_script_file_code_t *) e->ip;
    e->ip += sizeof(ngx_http_script_file_code_t);

    path.len = value->len - 1;
    path.data = value->data;

    r = e->request;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http script file op %p \"%V\"", code->op, &path);

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    ngx_memzero(&of, sizeof(ngx_open_file_info_t));

    of.read_ahead = clcf->read_ahead;
    of.directio = clcf->directio;
    of.valid = clcf->open_file_cache_valid;
    of.min_uses = clcf->open_file_cache_min_uses;
    of.test_only = 1;
    of.errors = clcf->open_file_cache_errors;
    of.events = clcf->open_file_cache_events;

    if (ngx_http_set_disable_symlinks(r, clcf, &path, &of) != NGX_OK) {
        e->ip = ngx_http_script_exit;
        e->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        return;
    }

    if (ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool)
        != NGX_OK)
    {
        if (of.err != NGX_ENOENT
            && of.err != NGX_ENOTDIR
            && of.err != NGX_ENAMETOOLONG)
        {
            ngx_log_error(NGX_LOG_CRIT, r->connection->log, of.err,
                          "%s \"%s\" failed", of.failed, value->data);
        }

        switch (code->op) {

        case ngx_http_script_file_plain:
        case ngx_http_script_file_dir:
        case ngx_http_script_file_exists:
        case ngx_http_script_file_exec:
             goto false_value;

        case ngx_http_script_file_not_plain:
        case ngx_http_script_file_not_dir:
        case ngx_http_script_file_not_exists:
        case ngx_http_script_file_not_exec:
             goto true_value;
        }

        goto false_value;
    }

    switch (code->op) {
    case ngx_http_script_file_plain:
        if (of.is_file) {
             goto true_value;
        }
        goto false_value;

    case ngx_http_script_file_not_plain:
        if (of.is_file) {
            goto false_value;
        }
        goto true_value;

    case ngx_http_script_file_dir:
        if (of.is_dir) {
             goto true_value;
        }
        goto false_value;

    case ngx_http_script_file_not_dir:
        if (of.is_dir) {
            goto false_value;
        }
        goto true_value;

    case ngx_http_script_file_exists:
        if (of.is_file || of.is_dir || of.is_link) {
             goto true_value;
        }
        goto false_value;

    case ngx_http_script_file_not_exists:
        if (of.is_file || of.is_dir || of.is_link) {
            goto false_value;
        }
        goto true_value;

    case ngx_http_script_file_exec:
        if (of.is_exec) {
             goto true_value;
        }
        goto false_value;

    case ngx_http_script_file_not_exec:
        if (of.is_exec) {
            goto false_value;
        }
        goto true_value;
    }

false_value:

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http script file op false");

    *value = ngx_http_variable_null_value;
    return;

true_value:

    *value = ngx_http_variable_true_value;
    return;
}

//ngx_http_rewrite_handler中执行
/*
如果value值是$name变量参数，则该函数配合ngx_http_script_add_var_code读取，ngx_http_script_complex_value_code根据code->lengths(ngx_http_script_copy_var_len_code)
来确定变量字符串的长度，然后开辟对应的e->buf空间，最后在ngx_http_script_copy_var_code中把变量名拷贝到e->buf空间中

如果参数中带有?，则最终会在执行ngx_http_script_add_args_code

如果参数是$1,则最终会执行ngx_http_script_add_capture_code
*/void
ngx_http_script_complex_value_code(ngx_http_script_engine_t *e)
{
    size_t                                 len;
    ngx_http_script_engine_t               le;
    ngx_http_script_len_code_pt            lcode;
    ngx_http_script_complex_value_code_t  *code;

    code = (ngx_http_script_complex_value_code_t *) e->ip;// e->ip就是之前在解析时设置的各种结构体  

    e->ip += sizeof(ngx_http_script_complex_value_code_t);

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script complex value");

    ngx_memzero(&le, sizeof(ngx_http_script_engine_t));

    le.ip = code->lengths->elts;
    le.line = e->line;
    le.request = e->request;
    le.quote = e->quote;

    //配合ngx_http_rewrite_value->ngx_http_script_compile->ngx_http_script_add_var_code阅读
    for (len = 0; *(uintptr_t *) le.ip; len += lcode(&le)) {//获取该变量字符串的长度
        lcode = *(ngx_http_script_len_code_pt *) le.ip; 
    }

    e->buf.len = len;
    e->buf.data = ngx_pnalloc(e->request->pool, len);
    if (e->buf.data == NULL) {
        e->ip = ngx_http_script_exit;
        e->status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        return;
    }

    e->pos = e->buf.data;

    e->sp->len = e->buf.len;
    e->sp->data = e->buf.data;
    e->sp++;
}

//ngx_http_rewrite_handler中执行   
void
ngx_http_script_value_code(ngx_http_script_engine_t *e) //e是从ngx_http_rewrite_loc_conf_t->codes->elts遍历获取到的
//获取value值存放到e->sp中，sp向下移动一位指向下一个需要处理的ngx_http_script_xxx_code_t,一般value_code_t的下一个为set_var_code
{
    ngx_http_script_value_code_t  *code;

    code = (ngx_http_script_value_code_t *) e->ip;

    e->ip += sizeof(ngx_http_script_value_code_t);

    e->sp->len = code->text_len;
    e->sp->data = (u_char *) code->text_data;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script value: \"%v\"", e->sp);

    e->sp++;
}

//ngx_http_rewrite_handler中执行
void //先执行ngx_http_script_value_code后，获取到value值存入e中，这样就可以通过set_val_code把e中的value赋值给对应的请求ngx_http_request_t
ngx_http_script_set_var_code(ngx_http_script_engine_t *e) //e是从ngx_http_rewrite_loc_conf_t->codes->elts遍历获取到的
{
    ngx_http_request_t          *r;
    ngx_http_script_var_code_t  *code;

    code = (ngx_http_script_var_code_t *) e->ip;// e->ip就是之前在解析时设置的各种结构体   

    e->ip += sizeof(ngx_http_script_var_code_t);

    r = e->request;

    // e->sp是通过解析得到的变量处理结果的一个数组，变量的放置顺序跟ip中的顺序一致，而且随着处理而递增，所以为了保持中处理的一致性(这样   
    // 就可以保证许多地方使用一致的处理方式)。这里sp—就可以得到之前的处理值，得到我们想要的结果了。 
    e->sp--; 
    //一般_value_code和set_var是一对，所以这里要回到前面的sp，要不set_var就暂用一个sp节点了，而sp是不会获取值的，所以不应该占用一个节点，可以确保只在value_code中增加


    /*
     变量code->index表示Nginx变量$file在cmcf->variables数组内的下标，对应每个请求的变量值存储空间就为r->variables[code->index]，
     这里从e->sp栈中取出数据并进行C语言变量普通意义上的赋值
     */
    //把从前面的_value_code中获取到的值赋值给对应的r->variables[code->index]
    r->variables[code->index].len = e->sp->len;
    r->variables[code->index].valid = 1;
    r->variables[code->index].no_cacheable = 0;
    r->variables[code->index].not_found = 0;
    r->variables[code->index].data = e->sp->data;

#if (NGX_DEBUG)
    {
    ngx_http_variable_t        *v;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    v = cmcf->variables.elts;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script set $%V", &v[code->index].name);
    }
#endif
}


void
ngx_http_script_var_set_handler_code(ngx_http_script_engine_t *e)
{
    ngx_http_script_var_handler_code_t  *code;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script set var handler");

    code = (ngx_http_script_var_handler_code_t *) e->ip;

    e->ip += sizeof(ngx_http_script_var_handler_code_t);

    e->sp--;

    code->handler(e->request, e->sp, code->data);
}


void
ngx_http_script_var_code(ngx_http_script_engine_t *e)
{
    ngx_http_variable_value_t   *value;
    ngx_http_script_var_code_t  *code;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                   "http script var");

    code = (ngx_http_script_var_code_t *) e->ip;

    e->ip += sizeof(ngx_http_script_var_code_t);

    value = ngx_http_get_flushed_variable(e->request, code->index);

    if (value && !value->not_found) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, e->request->connection->log, 0,
                       "http script var: \"%v\"", value);

        *e->sp = *value;
        e->sp++;

        return;
    }

    *e->sp = ngx_http_variable_null_value;
    e->sp++;
}


void
ngx_http_script_nop_code(ngx_http_script_engine_t *e)
{
    e->ip += sizeof(uintptr_t);
}
