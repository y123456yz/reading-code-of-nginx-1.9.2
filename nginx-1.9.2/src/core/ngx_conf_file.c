
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>

#define NGX_CONF_BUFFER  4096

static ngx_int_t ngx_conf_handler(ngx_conf_t *cf, ngx_int_t last);
static ngx_int_t ngx_conf_read_token(ngx_conf_t *cf);
static void ngx_conf_flush_files(ngx_cycle_t *cycle);

/*
配置类型模块是唯一一种只有1个模块的模块类型。配置模块的类型叫做NGX_CONF_MODULE，它仅有的模块叫做ngx_conf_module，这是Nginx最
底层的模块，它指导着所有模块以配置项为核心来提供功能。因此，它是其他所有模块的基础。
*/
static ngx_command_t  ngx_conf_commands[] = { //嵌入其他配置文件

    { ngx_string("include"),
      NGX_ANY_CONF|NGX_CONF_TAKE1,
      ngx_conf_include,
      0,
      0,
      NULL },

      ngx_null_command
};


ngx_module_t  ngx_conf_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    ngx_conf_commands,                     /* module directives */
    NGX_CONF_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    ngx_conf_flush_files,                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


/* The eight fixed arguments */

static ngx_uint_t argument_number[] = {
    NGX_CONF_NOARGS,
    NGX_CONF_TAKE1,
    NGX_CONF_TAKE2,
    NGX_CONF_TAKE3,
    NGX_CONF_TAKE4,
    NGX_CONF_TAKE5,
    NGX_CONF_TAKE6,
    NGX_CONF_TAKE7
};

/* 解析命令行参数信息到内存结构中 */
char *
ngx_conf_param(ngx_conf_t *cf)
{
    char             *rv;
    ngx_str_t        *param;
    ngx_buf_t         b;
    ngx_conf_file_t   conf_file;

    param = &cf->cycle->conf_param;

    if (param->len == 0) {
        return NGX_CONF_OK;
    }

    ngx_memzero(&conf_file, sizeof(ngx_conf_file_t));

    ngx_memzero(&b, sizeof(ngx_buf_t));

    b.start = param->data;
    b.pos = param->data;
    b.last = param->data + param->len;
    b.end = b.last;
    b.temporary = 1;

    conf_file.file.fd = NGX_INVALID_FILE;
    conf_file.file.name.data = NULL;
    conf_file.line = 0;

    cf->conf_file = &conf_file;
    cf->conf_file->buffer = &b;

    rv = ngx_conf_parse(cf, NULL);

    cf->conf_file = NULL;

    return rv;
}

/*
    8) HTTP框架开始循环解析nginx.conf文件中http{．．．}里面的所有配置项，
过程到第19步才会返回。
    9)配置文件解析器在检测到1个配置项后，会遍历所有的HTTP模块，
ngx_command_t数组中的name项是否与配置项名相同。
    10)如果找到有1个HTTP模块（如mytest模块）对这个配置项感兴趣（如test- myconfig
配置项），就调用ngx_command_t结构中的set方法来处理。
    11) set方法返回是否处理成功。如果处理失败，那么Nginx进程会停止。
    12)配置文件解析器继续检测配置项。如果发现server{．．．）配置项，就会调用ngx_http_
core_module模块来处理。因为ngx_http_core_module模块明确表示希望处理server{}块下
的配置项。注意，这次调用到第18步才会返回。
    13) ngx_http_core_module棋块在解析server{...}之前，也会如第3步一样建立ngx_
http_conf_ctx_t结构，并建立数组保存所有HTTP模块返回的指针地址。然后，它会调用每
个HTTP模块的create_srv_conf、create_loc_conf方法（如果实现的话）。
    14)将上一步各HTTP模块返回的指针地址保存到ngx_http_conf_ctx_t对应的数组中。
    15)开始调用配置文件解析器来处理server{．．．}里面的配置项，注意，这个过程在第17
步返回。
    16)继续重复第9步的过程，遍历nginx.conf中当前server{．．．）内的所有配置项。
    17)配置文件解析器继续解析配置项，发现当前server块已经遍历到尾部，说明server
块内的配置项处理完毕，返回ngx_http_core_module模块。
    18) http core模块也处理完server配置项了，返回至配置文件解析器继续解析后面的配
置项。
    19)配置文件解析器继续解析配置项，这时发现处理到了http{．．．）的尾部，返回给
HTTP框架继续处理。
*/

/*
它是一个间接的递归函数，也就是说虽然我们在该函数体内看不到直接的对其本身的调用，但是它执行的一些函数（比如ngx_conf_handler）内又会
调用ngx_conf_parse函数，因此形成递归，这一般在处理一些特殊配置指令或复杂配置项，比如指令include、events、http、 server、location等的处理时。
*/ //ngx配置解析数据结构图解参考:http://tech.uc.cn/?p=300  这个比较全
char *
ngx_conf_parse(ngx_conf_t *cf, ngx_str_t *filename)  //参考:http://blog.chinaunix.net/uid-26335251-id-3483044.html
{
    char             *rv;
    u_char           *p;
    off_t             size;
    ngx_fd_t          fd;
    ngx_int_t         rc;
    ngx_buf_t         buf, *tbuf;
    ngx_conf_file_t  *prev, conf_file;
    ngx_conf_dump_t  *cd;

    /* ngx_conf_parse 这个函数来完成对配置文件的解析，其实这个函数不仅仅解析文件，还可以用来解析参数和块 */
    /*
    当执行到ngx_conf_parse函数内时，配置的解析可能处于三种状态：
    第一种，刚开始解析一个配置文件，即此时的参数filename指向一个配置文件路径字符串，需要函数ngx_conf_parse打开该文件并获取相关
    的文件信息以便下面代码读取文件内容并进行解析，除了在上面介绍的nginx启动时开始主配置文件解析时属于这种情况，还有当遇到include
    指令时也将以这种状态调用ngx_conf_parse函数，因为include指令表示一个新的配置文件要开始解析。状态标记为type = parse_file;。
    
    第二种，开始解析一个配置块，即此时配置文件已经打开并且也已经对文件部分进行了解析，当遇到复杂配置项比如events、http等时，
    这些复杂配置项的处理函数又会递归的调用ngx_conf_parse函数，此时解析的内容还是来自当前的配置文件，因此无需再次打开它，状态标记为type = parse_block;。
    
    第三种，开始解析配置项，这在对用户通过命令行-g参数输入的配置信息进行解析时处于这种状态，如：
    nginx -g ‘daemon on;’
    nginx在调用ngx_conf_parse函数对配置信息’daemon on;’进行解析时就是这种状态，状态标记为type = parse_param;。
    前面说过，nginx配置是由标记组成的，在区分好了解析状态之后，接下来就要读取配置内容，而函数ngx_conf_read_token就是做这个事情的：
    */
    enum {
        parse_file = 0,
        parse_block,
        parse_param
    } type;

#if (NGX_SUPPRESS_WARN)
    fd = NGX_INVALID_FILE;
    prev = NULL;
#endif

    if (filename) {

        /* open configuration file */

        fd = ngx_open_file(filename->data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
        if (fd == NGX_INVALID_FILE) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                               ngx_open_file_n " \"%s\" failed",
                               filename->data);
            return NGX_CONF_ERROR;
        }

        
        /* 保存cf->conf_file 的上文 */
        prev = cf->conf_file; //解析该配置文件之前的配置用prev暂存

        cf->conf_file = &conf_file;

        if (ngx_fd_info(fd, &cf->conf_file->file.info) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_EMERG, cf->log, ngx_errno,
                          ngx_fd_info_n " \"%s\" failed", filename->data);
        }

        cf->conf_file->buffer = &buf;

    /*
    函数ngx_conf_read_token对配置文件内容逐个字符扫描并解析为单个的token，当然，该函数并不会频繁的去读取配置文件，它每次从
    文件内读取足够多的内容以填满一个大小为NGX_CONF_BUFFER的缓存区（除了最后一次，即配置文件剩余内容本来就不够了），这个缓存
    区在函数 ngx_conf_parse内申请并保存引用到变量cf->conf_file->buffer内，函数 ngx_conf_read_token反复使用该缓存区
    */
        buf.start = ngx_alloc(NGX_CONF_BUFFER, cf->log);
        if (buf.start == NULL) {
            goto failed;
        }

        buf.pos = buf.start;
        buf.last = buf.start;
        buf.end = buf.last + NGX_CONF_BUFFER;
        buf.temporary = 1;

        cf->conf_file->file.fd = fd;
        cf->conf_file->file.name.len = filename->len;
        cf->conf_file->file.name.data = filename->data;
        cf->conf_file->file.offset = 0;
        cf->conf_file->file.log = cf->log;
        cf->conf_file->line = 1;

        type = parse_file;

        if (ngx_dump_config
#if (NGX_DEBUG)
            || 1
#endif
           )
        {
            p = ngx_pstrdup(cf->cycle->pool, filename);
            if (p == NULL) {
                goto failed;
            }

            size = ngx_file_size(&cf->conf_file->file.info);

            tbuf = ngx_create_temp_buf(cf->cycle->pool, (size_t) size);
            if (tbuf == NULL) {
                goto failed;
            }

            cd = ngx_array_push(&cf->cycle->config_dump);
            if (cd == NULL) {
                goto failed;
            }

            cd->name.len = filename->len;
            cd->name.data = p;
            cd->buffer = tbuf;

            cf->conf_file->dump = tbuf;

        } else {
            cf->conf_file->dump = NULL;
        }

    } else if (cf->conf_file->file.fd != NGX_INVALID_FILE) {

        type = parse_block;

    } else {
        type = parse_param; //解析命令行参数
    }

    for ( ;; ) {

        /* 读取文件中的内容放到缓冲区中，并进行解析，把解析的结果放到了cf->args 里面， 指令的每个单词都在数组中占一个位置，比如 set debug off  ，那么数组中存三个位置。*/
        rc = ngx_conf_read_token(cf); //读取文件 

        /*
         * ngx_conf_read_token() may return
         *
         *    NGX_ERROR             there is error
         *    NGX_OK                the token terminated by ";" was found
         *    NGX_CONF_BLOCK_START  the token terminated by "{" was found
         *    NGX_CONF_BLOCK_DONE   the "}" was found
         *    NGX_CONF_FILE_DONE    the configuration file is done
         */

        if (rc == NGX_ERROR) {
            goto done;
        }

        if (rc == NGX_CONF_BLOCK_DONE) {

            if (type != parse_block) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "unexpected \"}\"");
                goto failed;
            }

            goto done;
        }

        if (rc == NGX_CONF_FILE_DONE) {

            if (type == parse_block) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "unexpected end of file, expecting \"}\"");
                goto failed;
            }

            goto done;
        }

        if (rc == NGX_CONF_BLOCK_START) {

            if (type == parse_param) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "block directives are not supported "
                                   "in -g option");
                goto failed;
            }
        }

        /* rc == NGX_OK || rc == NGX_CONF_BLOCK_START */
        if (cf->handler) {
            /*
             * the custom handler, i.e., that is used in the http's
             * "types { ... }" directive
             */

            if (rc == NGX_CONF_BLOCK_START) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "unexpected \"{\"");
                goto failed;
            }

            rv = (*cf->handler)(cf, NULL, cf->handler_conf);
            if (rv == NGX_CONF_OK) {
                continue;
            }

            if (rv == NGX_CONF_ERROR) {
                goto failed;
            }

            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, rv);

            goto failed;
        }

        /*
        这个功能是在函数ngx_conf_handle中实现的，整个过程中需要遍历所有模块中的所有指令，如果找到一个，就直接调用指令的set函数，
        完成对模块的配置信息的设置。 这里主要的过程就是判断是否是找到，需要判断下面一些条件： 
          a  名字一致。配置文件中指令的名字和模块指令中的名字需要一致
          b  模块类型一致。配置文件指令处理的模块类型和当前模块一致
          c  指令类型一致。 配置文件指令类型和当前模块指令一致
          d  参数个数一致。配置文件中参数的个数和当前模块的当前指令参数一致。 
        */
        rc = ngx_conf_handler(cf, rc); //上面的ngx_conf_read_token返回的是解析到的一行配置，任何在ngx_modules中查找匹配该条配置的命令，执行相应的set

        if (rc == NGX_ERROR) {
            goto failed;
        }
    }

failed:

    rc = NGX_ERROR;

done:

    if (filename) {
        if (cf->conf_file->buffer->start) {
            ngx_free(cf->conf_file->buffer->start);
        }

        if (ngx_close_file(fd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, cf->log, ngx_errno,
                          ngx_close_file_n " %s failed",
                          filename->data);
            rc = NGX_ERROR;
        }
        
        /* 恢复上下文 */
        cf->conf_file = prev;
    }

    if (rc == NGX_ERROR) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

/*
    这个功能是在函数ngx_conf_handle中实现的，整个过程中需要遍历所有模块中的所有指令，如果找到一个，就直接调用指令的set 函数，
    完成对模块的配置信息的设置。 这里主要的过程就是判断是否是找到，需要判断下面一些条件： 

  a  名字一致。配置文件中指令的名字和模块指令中的名字需要一致
  b  模块类型一致。配置文件指令处理的模块类型和当前模块一致
  c  指令类型一致。 配置文件指令类型和当前模块指令一致
  d  参数个数一致。配置文件中参数的个数和当前模块的当前指令参数一致。 
*/
static ngx_int_t
ngx_conf_handler(ngx_conf_t *cf, ngx_int_t last)
{
    char           *rv;
    void           *conf, **confp;
    ngx_uint_t      i, found;
    ngx_str_t      *name;
    ngx_command_t  *cmd;

    name = cf->args->elts;

    ngx_uint_t ia;
    char buf[2560];
    char tmp[256];
    memset(buf, 0, sizeof(buf));
   
   for(ia = 0; ia < cf->args->nelts; ia++) {
        memset(tmp, 0, sizeof(tmp));
        snprintf(tmp, sizeof(tmp), "%s ", name[ia].data);
        strcat(buf, tmp);
    }
    //printf("yang test:%p %s <%s, %u>\n", cf->ctx, buf, __FUNCTION__, __LINE__); //这个打印出来的内容为整行的内容，如yang test:error_log logs/error.log debug   <ngx_conf_handler, 407>
   
   
    found = 0;

    for (i = 0; ngx_modules[i]; i++) { //所有模块都扫描一遍
        cmd = ngx_modules[i]->commands;
        if (cmd == NULL) {
            continue;
        }
       
        for ( /* void */ ; cmd->name.len; cmd++) {

            if (name->len != cmd->name.len) {
                continue;
            }

            if (ngx_strcmp(name->data, cmd->name.data) != 0) {
                continue;
            }

            found = 1;

            if (ngx_modules[i]->type != NGX_CONF_MODULE
                && ngx_modules[i]->type != cf->module_type)
            {
                continue;
            }

            /* is the directive's location right ? */

            if (!(cmd->type & cf->cmd_type)) {
                continue;
            }

            if (!(cmd->type & NGX_CONF_BLOCK) && last != NGX_OK) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                  "directive \"%s\" is not terminated by \";\"",
                                  name->data);
                return NGX_ERROR;
            }

            if ((cmd->type & NGX_CONF_BLOCK) && last != NGX_CONF_BLOCK_START) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "directive \"%s\" has no opening \"{\"",
                                   name->data);
                return NGX_ERROR;
            }

            /* is the directive's argument count right ? */

            if (!(cmd->type & NGX_CONF_ANY)) {

                if (cmd->type & NGX_CONF_FLAG) {

                    if (cf->args->nelts != 2) {
                        goto invalid;
                    }

                } else if (cmd->type & NGX_CONF_1MORE) {

                    if (cf->args->nelts < 2) {
                        goto invalid;
                    }

                } else if (cmd->type & NGX_CONF_2MORE) {

                    if (cf->args->nelts < 3) {
                        goto invalid;
                    }

                } else if (cf->args->nelts > NGX_CONF_MAX_ARGS) {

                    goto invalid;

                } else if (!(cmd->type & argument_number[cf->args->nelts - 1]))
                {
                    goto invalid;
                }
            }

            /* set up the directive's configuration context */

            conf = NULL;

            /* 例如执行到http行，会走第二个if，确定http一级NGX_CORE_MODULE类型在ngx_cycle_s->conf_ctx中的位置，然后在继续后面的set函数，在
                该函数中开辟空间，并让conf_ctx[]数组里面的具体成员指针指向该空间，从而使http{}空间和ngx_cycle_s关联起来 */

            /*
            第一个if中执行的命令主要有(NGX_DIRECT_CONF):ngx_core_commands  ngx_openssl_commands  ngx_google_perftools_commands   ngx_regex_commands  ngx_thread_pool_commands
            第二个if中执行的命令主要有(NGX_MAIN_CONF):http   events include等
            第三个if中执行的命令主要有(其他):http{}   events{} server server{} location及其location{}内部的命令
            */
            
            //conf为开辟的main_conf  srv_conf loc_conf空间指针，真正的空间为各个模块module的ctx成员中,
            //可以参考ngx_http_mytest_config_module_ctx, http{}对应的空间开辟在ngx_http_block
            //注意:通过在ngx_init_cycle中打印conf.ctx以及在这里打印cf->cxt，发现所有第一层(http{}外的配置，包括http这一行)的地址是一样的，也就是这里的conf.ctx始终等于cycle->conf_ctx;
            //每到一层新的{}，cf->cxt地址就会指向这一层中对应的ngx_http_conf_ctx_t。退出{}中后，会把cf->cxt恢复到上层的ngx_http_conf_ctx_t地址
            //这时的cf->ctx一定等于ngx_cycle_s->conf_ctx，见ngx_init_cycle
            if (cmd->type & NGX_DIRECT_CONF) {//使用全局配置，主要包括以下命令//ngx_core_commands  ngx_openssl_commands  ngx_google_perftools_commands   ngx_regex_commands  ngx_thread_pool_commands
                conf = ((void **) cf->ctx)[ngx_modules[i]->index]; //ngx_core_commands对应的空间分配的地方参考ngx_core_module->ngx_core_module_ctx
            } else if (cmd->type & NGX_MAIN_CONF) { //例如ngx_http_commands ngx_errlog_commands  ngx_events_commands  ngx_conf_commands  这些配置command一般只有一个参数
                conf = &(((void **) cf->ctx)[ngx_modules[i]->index]); //指向ngx_cycle_s->conf_ctx
            } else if (cf->ctx) {
     /*http{}内部行相关命令可以参考:ngx_http_core_commands,通过这些命令里面的NGX_HTTP_MAIN_CONF_OFFSET NGX_HTTP_SRV_CONF_OFFSET NGX_HTTP_LOC_CONF_OFFSET
     可以确定出该命令行的地址对应在ngx_http_conf_ctx_t中的地址空间头部指针位置, 就是确定出该命令为ngx_http_conf_ctx的成员main srv loc中的那一个
     */      
                confp = *(void **) ((char *) cf->ctx + cmd->conf);   //如果是http{}内部的行，则cf->ctx已经在ngx_http_block中被重新赋值为新的ngx_http_conf_ctx_t空间
                //这里的cf->ctx为二级或者三级里面分配的空间了，而不是ngx_cycle_s->conf_ctx,例如为存储http{}内部配置项的空间，见ngx_http_block分配的空间

                if (confp) { //图形化参考:深入理解NGINX中的图9-2  图10-1  图4-2，结合图看,并可以配合http://tech.uc.cn/?p=300看
                    conf = confp[ngx_modules[i]->ctx_index]; //上一行是确定在ngx_http_conf_ctx_t中main srv loc中的那个成员头，这个则是对应头部下面的数组指针中的具体那一个
                }
            }

            /* 例如解析到http {，则执行ngx_http_block，在该函数中会把http模块对应的srv local main配置空间赋值给conf,也就是ngx_cycle_s->conf_ctx[]对应的模块 */
            rv = cmd->set(cf, cmd, conf); 
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  
            if (rv == NGX_CONF_OK) {
                return NGX_OK;
            }

            if (rv == NGX_CONF_ERROR) {
                return NGX_ERROR;
            }

            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "\"%s\" directive %s", name->data, rv);

            return NGX_ERROR;
        }
    }

    if (found) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%s\" directive is not allowed here", name->data);

        return NGX_ERROR;
    }

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "unknown directive \"%s\"", name->data);

    return NGX_ERROR;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid number of arguments in \"%s\" directive",
                       name->data);

    return NGX_ERROR;
}

/*
函数ngx_conf_read_token在读取了合适数量的标记token之后就开始下一步骤即对这些标记进行实际的处理。那多少才算是读取了合适数量的标记呢？区别对待，对于简单配置项则是读取其全部的标记，也就是遇到结束标记分号;为止，此时一条简单配置项的所有标记都被读取并存放在 cf->args数组内，因此可以调用其对应的回调函数进行实际的处理；对于复杂配置项则是读完其配置块前的所有标记，即遇到大括号{为止，此时复杂配置项处理函数所需要的标记都已读取到，而对于配置块{}内的标记将在接下来的函数ngx_conf_parse递归调用中继续处理，这可能是一个反复的过程。
当然，函数ngx_conf_read_token也可能在其它情况下返回，比如配置文件格式出错、文件处理完（遇到文件结束）、块配置处理完（遇到大括号}），这几种返回情况的处理都很简单，不多详叙。
对于简单/复杂配置项的处理，一般情况下，这是通过函数ngx_conf_handler来进行的，而也有特殊的情况，也就是配置项提供了自定义的处理函数，
比如types指令。函数ngx_conf_handler也做了三件事情，首先，它需要找到当前解析出来的配置项所对应的 ngx_command_s结构体，
前面说过该ngx_command_s包含有配置项的相关信息以及对应的回调实际处理函数。如果没找到配置项所对应的 ngx_command_s结构体，
那么谁来处理这个配置项呢？自然是不行的，因此nginx就直接进行报错并退出程序。其次，找到当前解析出来的配置项所对应的ngx_command_s
结构体之后还需进行一些有效性验证，因为ngx_command_s结构体内包含有配置项的相关信息，因此有效性验证是可以进行的，比如配置项的类型、
位置、带参数的个数等等。只有经过了严格有效性验证的配置项才调用其对应的回调函数：
rv = cmd->set(cf, cmd, conf);
进行处理，这也就是第三件事情。在处理函数内，根据实际的需要又可能再次调用函数ngx_conf_parse，如此反复直至所有配置信息都被处理完。

*/

/*
 首先明确,什么是一个token: 
 token是处在两个相邻空格,换行符,双引号,单引号等之间的字符串. 
*/  

/****************************************** 
1.读取文件内容,每次读取一个buf大小(4K),如果文件内容不足4K则全部读取到buf中. 
2.扫描buf中的内容,每次扫描一个token就会存入cf->args中,然后返回. 
3.返回后调用ngx_conf_parse函数会调用*cf->handler和ngx_conf_handler(cf, rc)函数处理. 
3.如果是复杂配置项,会调用上次执行的状态继续解析配置文件. 
.*****************************************/ 

static ngx_int_t
ngx_conf_read_token(ngx_conf_t *cf)//参考http://blog.chinaunix.net/uid-26335251-id-3483044.html
{
    u_char      *start, ch, *src, *dst;
    off_t        file_size;
    size_t       len;
    ssize_t      n, size;
    ngx_uint_t   found, need_space, last_space, sharp_comment, variable;
    ngx_uint_t   quoted, s_quoted, d_quoted, start_line;
    ngx_str_t   *word;
    ngx_buf_t   *b, *dump; //b为在函数外层开辟的4096字节的存储file文件内容的空间，dump为当解析到尾部的时候不足以构成一个token的时候，零时把这部分内存存起来

    found = 0;//标志位,表示找到一个token 

    
    /************************* 
    标志位,表示此时需要一个token分隔符,即token前面的分隔符 
    一般刚刚解析完一对双引号或者单引号,此时设置need_space为1, 
    表示期待下一个字符为分隔符 
    **********************/  
    need_space = 0;
    last_space = 1; //标志位,表示上一个字符为token分隔符   
    sharp_comment = 0; //注释(#)   
    variable = 0; //遇到字符$后,表示一个变量   
    quoted = 0; //标志位,表示上一个字符为反引号   
    s_quoted = 0; //标志位,表示已扫描一个双引号,期待另一个双引号   
    d_quoted = 0; //标志位,表示已扫描一个单引号,期待另一个单引号  

    cf->args->nelts = 0;
    b = cf->conf_file->buffer;
    dump = cf->conf_file->dump;
    start = b->pos;
    start_line = cf->conf_file->line;

    file_size = ngx_file_size(&cf->conf_file->file.info);

    for ( ;; ) {

        if (b->pos >= b->last) { //当buf内的数据全部扫描后   

            if (cf->conf_file->file.offset >= file_size) {

                if (cf->args->nelts > 0 || !last_space) {

                    if (cf->conf_file->file.fd == NGX_INVALID_FILE) {
                        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                           "unexpected end of parameter, "
                                           "expecting \";\"");
                        return NGX_ERROR;
                    }

                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                  "unexpected end of file, "
                                  "expecting \";\" or \"}\"");
                    return NGX_ERROR;
                }

                //配置文件读取完毕. 
                return NGX_CONF_FILE_DONE;
            }

            //已扫描的buf的长度   
            len = b->pos - start; //表示上一个4096的buffer中未解析完毕的空间大小

            if (len == NGX_CONF_BUFFER) {//已扫描全部buf   
                cf->conf_file->line = start_line;

                if (d_quoted) { //缺少右双引号   
                    ch = '"';

                } else if (s_quoted) { //缺少右单引号   
                    ch = '\'';

                } else { //字符串太长,一个buf都无法存储一个token   
                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                       "too long parameter \"%*s...\" started",
                                       10, start);
                    return NGX_ERROR;
                }

                
                //参数太多,可能缺少右双引号或者右单引号.   
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "too long parameter, probably "
                                   "missing terminating \"%c\" character", ch);
                return NGX_ERROR;
            }

            if (len) {//长度有效,赋值已扫描buf到buf首部  
                ngx_memmove(b->start, start, len);
            }

            //size等于配置文件未读入的长度    
            size = (ssize_t) (file_size - cf->conf_file->file.offset);

            //size不能大于可用buf长度，因为上一个4096空间可能有部分空间需要在这一个4096空间中用
            if (size > b->end - (b->start + len)) {
                size = b->end - (b->start + len);
            }

            n = ngx_read_file(&cf->conf_file->file, b->start + len, size,
                              cf->conf_file->file.offset);

            if (n == NGX_ERROR) {
                return NGX_ERROR;
            }

            if (n != size) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   ngx_read_file_n " returned "
                                   "only %z bytes instead of %z",
                                   n, size);
                return NGX_ERROR;
            }

            //重置pos,last,start   //参考http://blog.chinaunix.net/uid-26335251-id-3483044.html
            b->pos = b->start + len;//指向新读取空间的头，这里的len为上一个4096中末尾空间为满足一个stoken部分的空间大小
            b->last = b->pos + n;//指向新读取空间的尾
            start = b->start;

            if (dump) {
                dump->last = ngx_cpymem(dump->last, b->pos, size);
            }
        }

        ch = *b->pos++;

        if (ch == LF) {//遇到换行符 
            cf->conf_file->line++;

            /* 如果该行为注释,则注释行结束,即取消注释标识 
                */
            if (sharp_comment) {
                sharp_comment = 0;
            }
        }

        if (sharp_comment) {//如果该行为注释行,则不再对字符判断,继续读取字符执行   
            continue;
        }

        /*
        如果为反引号,则设置反引号标识,并且不对该字符进行解析   
        继续扫描下一个字符   
        */
        if (quoted) {
            quoted = 0;
            continue;
        }

        if (need_space) { //上一个字符为单引号或者双引号,期待一个分隔符   
            if (ch == ' ' || ch == '\t' || ch == CR || ch == LF) {
                last_space = 1;
                need_space = 0;
                continue;
            }

            if (ch == ';') {  //遇到分号,表示一个简单配置项解析结束.  
                return NGX_OK;
            }

            if (ch == '{') { //遇到{表示一个复杂配置项开始   
                return NGX_CONF_BLOCK_START;
            }

            if (ch == ')') {
                last_space = 1;
                need_space = 0;

            } else {
                 ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                    "unexpected \"%c\"", ch);
                 return NGX_ERROR;
            }
        }

        if (last_space) {//如果上一个字符是空格,换行符等分割token的字符(间隔符).  
            if (ch == ' ' || ch == '\t' || ch == CR || ch == LF) {//两个分割token的字符相邻,依旧表示一个间隔符.   
                continue;
            }

            start = b->pos - 1;
            start_line = cf->conf_file->line;

            switch (ch) {

            case ';':
            case '{':
                if (cf->args->nelts == 0) {
                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                       "unexpected \"%c\"", ch);
                    return NGX_ERROR;
                }

                if (ch == '{') {
                    return NGX_CONF_BLOCK_START;
                }

                return NGX_OK;

            case '}':
                if (cf->args->nelts != 0) {
                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                       "unexpected \"}\"");
                    return NGX_ERROR;
                }

                return NGX_CONF_BLOCK_DONE;

            case '#':
                sharp_comment = 1;
                continue;

            case '\\':
                quoted = 1;
                last_space = 0;
                continue;

            case '"':
                start++;
                d_quoted = 1;
                last_space = 0;
                continue;

            case '\'':
                start++;
                s_quoted = 1;
                last_space = 0;
                continue;

            default:
                last_space = 0;
            }

        } else {
            if (ch == '{' && variable) {
                continue;
            }

            variable = 0;

            if (ch == '\\') {
                quoted = 1;
                continue;
            }

            if (ch == '$') { //变量标志位为1   
                variable = 1;
                continue;
            }

            if (d_quoted) {
                if (ch == '"') {  //已经找到成对双引号,期望一个间隔符   
                    d_quoted = 0;
                    need_space = 1;
                    found = 1;
                }

            } else if (s_quoted) {//已经找到成对单引号,期望一个间隔符   
                if (ch == '\'') {
                    s_quoted = 0;
                    need_space = 1;
                    found = 1;
                }

            } else if (ch == ' ' || ch == '\t' || ch == CR || ch == LF
                       || ch == ';' || ch == '{')
            {
                last_space = 1;
                found = 1;
            }

            if (found) {
                word = ngx_array_push(cf->args);
                if (word == NULL) {
                    return NGX_ERROR;
                }

                word->data = ngx_pnalloc(cf->pool, b->pos - 1 - start + 1);
                if (word->data == NULL) {
                    return NGX_ERROR;
                }

                for (dst = word->data, src = start, len = 0;
                     src < b->pos - 1;
                     len++)
                {
                    if (*src == '\\') { //遇到反斜杠(转意字符)   
                        switch (src[1]) {
                        case '"':
                        case '\'':
                        case '\\':
                            src++;
                            break;

                        case 't':
                            *dst++ = '\t';
                            src += 2;
                            continue;

                        case 'r':
                            *dst++ = '\r';
                            src += 2;
                            continue;

                        case 'n':
                            *dst++ = '\n';
                            src += 2;
                            continue;
                        }

                    }
                    *dst++ = *src++;
                }
                *dst = '\0';
                word->len = len;

                if (ch == ';') {//遇到分号,表示一个简单配置项解析结束.   
                    return NGX_OK;
                }

                if (ch == '{') {
                    return NGX_CONF_BLOCK_START;
                }

                found = 0;
            }
        }
    }
}


char *
ngx_conf_include(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char        *rv;
    ngx_int_t    n;
    ngx_str_t   *value, file, name;
    ngx_glob_t   gl;

    value = cf->args->elts;
    file = value[1];

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, cf->log, 0, "include %s", file.data);

    if (ngx_conf_full_name(cf->cycle, &file, 1) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    if (strpbrk((char *) file.data, "*?[") == NULL) {

        ngx_log_debug1(NGX_LOG_DEBUG_CORE, cf->log, 0, "include %s", file.data);

        return ngx_conf_parse(cf, &file);
    }

    ngx_memzero(&gl, sizeof(ngx_glob_t));

    gl.pattern = file.data;
    gl.log = cf->log;
    gl.test = 1;

    if (ngx_open_glob(&gl) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                           ngx_open_glob_n " \"%s\" failed", file.data);
        return NGX_CONF_ERROR;
    }

    rv = NGX_CONF_OK;

    for ( ;; ) {
        n = ngx_read_glob(&gl, &name);

        if (n != NGX_OK) {
            break;
        }

        file.len = name.len++;
        file.data = ngx_pstrdup(cf->pool, &name);
        if (file.data == NULL) {
            return NGX_CONF_ERROR;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_CORE, cf->log, 0, "include %s", file.data);

        rv = ngx_conf_parse(cf, &file);

        if (rv != NGX_CONF_OK) {
            break;
        }
    }

    ngx_close_glob(&gl);

    return rv;
}

//获取配置文件全面，包括路径，存放到cycle->conf_prefix或者cycle->prefix中
ngx_int_t
ngx_conf_full_name(ngx_cycle_t *cycle, ngx_str_t *name, ngx_uint_t conf_prefix)
{
    ngx_str_t  *prefix;

    prefix = conf_prefix ? &cycle->conf_prefix : &cycle->prefix;

    return ngx_get_full_name(cycle->pool, prefix, name);
}

//查找cycle->open_files链表中的所有文件名，看是否有name文件名存在，如果存在直接返回该文件名对应的项，否则从链表数组中从新获取一个file,把name文件加入到其中
ngx_open_file_t *
ngx_conf_open_file(ngx_cycle_t *cycle, ngx_str_t *name) //access_log error_log配置都在这里创建ngx_open_file_t数组结构，由数组cycle->open_files统一管理
{
    ngx_str_t         full;
    ngx_uint_t        i;
    ngx_list_part_t  *part;
    ngx_open_file_t  *file;

#if (NGX_SUPPRESS_WARN)
    ngx_str_null(&full);
#endif

    if (name->len) {
        full = *name;

        if (ngx_conf_full_name(cycle, &full, 0) != NGX_OK) {
            return NULL;
        }

        part = &cycle->open_files.part;
        file = part->elts;

        for (i = 0; /* void */ ; i++) {

            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }
                part = part->next;
                file = part->elts;
                i = 0;
            }

            if (full.len != file[i].name.len) {
                continue;
            }

            if (ngx_strcmp(full.data, file[i].name.data) == 0) {
                return &file[i];
            }
        }
    }

    file = ngx_list_push(&cycle->open_files);
    if (file == NULL) {
        return NULL;
    }

    if (name->len) {
        file->fd = NGX_INVALID_FILE;
        file->name = full;

    } else {
        file->fd = ngx_stderr;
        file->name = *name;
    }

    file->flush = NULL;
    file->data = NULL;

    return file;
}


static void
ngx_conf_flush_files(ngx_cycle_t *cycle)
{
    ngx_uint_t        i;
    ngx_list_part_t  *part;
    ngx_open_file_t  *file;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0, "flush files");

    part = &cycle->open_files.part;
    file = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            file = part->elts;
            i = 0;
        }

        if (file[i].flush) {
            file[i].flush(&file[i], cycle->log);
        }
    }
}


void ngx_cdecl
ngx_conf_log_error(ngx_uint_t level, ngx_conf_t *cf, ngx_err_t err,
    const char *fmt, ...)
{
    u_char   errstr[NGX_MAX_CONF_ERRSTR], *p, *last;
    va_list  args;

    last = errstr + NGX_MAX_CONF_ERRSTR;

    va_start(args, fmt);
    p = ngx_vslprintf(errstr, last, fmt, args);
    va_end(args);

    if (err) {
        p = ngx_log_errno(p, last, err);
    }

    if (cf->conf_file == NULL) {
        ngx_log_error(level, cf->log, 0, "%*s", p - errstr, errstr);
        return;
    }

    if (cf->conf_file->file.fd == NGX_INVALID_FILE) {
        ngx_log_error(level, cf->log, 0, "%*s in command line",
                      p - errstr, errstr);
        return;
    }

    ngx_log_error(level, cf->log, 0, "%*s in %s:%ui",
                  p - errstr, errstr,
                  cf->conf_file->file.name.data, cf->conf_file->line);
}

/*
char*(*set)(ngx_conf_t *cf, ngx_commandj 'vcmd,void *conf)
    如果处理配置项，我们既可以自己实现一个回调方法来处理配置项（），也可以使用Nginx预设的14个解析配置项方法，这会少
写许多代码，表4-2列出了这些预设的解析配置项方法。

表4-2预设的14个配置项解析方法
┏━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名              ┃    行为                                                                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果nginx．conf文件中某个配置项的参数是on或者off（即希望配置项表达打开      ┃
┃                            ┃或者关闭某个功能的意思），而且在Nginx模块的代码中使用ngx_flag_t变量来保       ┃
┃ngx_conf_set_flag_slot      ┃存这个配置项的参数，就可以将set回调方法设为ngx_conf_set_flag_slot。当nginx.   ┃
┃                            ┃conf文件中参数是on时，代码中的ngx_flag_t类型变量将设为1，参数为off时则        ┃
┃                            ┃设为O                                                                         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果配置项后只有1个参数，同时在代码中我们希望用ngx_str_t类型的变量来保      ┃
┃ngx_conf_set_str_slot       ┃                                                                              ┃
┃                            ┃存这个配置项的参数，则可以使用ngx_conf_set_ str slot方法                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果这个配置项会出现多次，每个配置项后面都跟着1个参数，而在程序中我们       ┃
┃                            ┃希望仅用一个ngx_array_t动态数组（           ）来存储所有的参数，且数组中      ┃
┃ngx_conf_set_str_array_slot ┃                                                                              ┃
┃                            ┃的每个参数都以ngx_str_t来存储，那么预设的ngx_conf_set_str_array_slot有法可    ┃
┃                            ┃以帮我们做到                                                                  ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_str_array_slot类似，也是用一个ngx_array_t数组来存储所有同    ┃
┃                            ┃名配置项的参数。只是每个配置项的参数不再只是1个，而必须是两个，且以“配       ┃
┃ngx_conf_set_keyval_slot    ┃                                                                              ┃
┃                            ┃置项名关键字值；”的形式出现在nginx．conf文件中，同时，ngx_conf_set_keyval    ┃
┃                            ┃ slot将把这蜱配置项转化为数组，其中每个元素都存储着key/value键值对            ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_conf_set_num_slot       ┃  配置项后必须携带1个参数，且只能是数字。存储这个参数的变量必须是整型         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间大小，可以是一个数字，这时表示字节数       ┃
┃                            ┃(Byte)。如果数字后跟着k或者K，就表示Kilobyt，IKB=1024B；如果数字后跟          ┃
┃ngx_conf_set_size_slot      ┃                                                                              ┃
┃                            ┃着m或者M，就表示Megabyte，1MB=1024KB。ngx_conf_set_ size slot解析后将         ┃
┃                            ┃把配置项后的参数转化成以字节数为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间上的偏移量。它与设置的参数非常类似，       ┃
┃                            ┃其参数是一个数字时表示Byte，也可以在后面加单位，但与ngx_conf_set_size slot    ┃
┃ngx_conf_set_off_slot       ┃不同的是，数字后面的单位不仅可以是k或者K、m或者M，还可以是g或者G，            ┃
┃                            ┃这时表示Gigabyte，IGB=1024MB。ngx_conf_set_off_slot解析后将把配置项后的       ┃
┃                            ┃参数转化成以字节数为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示时间。这个参数可以在数字后面加单位，如         ┃
┃                            ┃果单位为s或者没有任何单位，那么这个数字表示秒；如果单位为m，则表示分          ┃
┃                            ┃钟，Im=60s：如果单位为h，则表示小时，th=60m：如果单位为d，则表示天，          ┃
┃ngx_conf_set_msec_slot      ┃                                                                              ┃
┃                            ┃ld=24h：如果单位为w，则表示周，lw=7d；如果单位为M，则表示月，1M=30d；         ┃
┃                            ┃如果单位为y，则表示年，ly=365d。ngx_conf_set_msec_slot解析后将把配置项后     ┃
┃                            ┃的参数转化成以毫秒为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_msec slot非常类似，唯一的区别是ngx_conf_set msec—slot解析   ┃
┃ngx_conf_set_sec_slot       ┃后将把配置项后的参数转化成以毫秒为单位的数字，而ngx_conf_set_sec一slot解析    ┃
┃                            ┃后会把配置项后的参数转化成以秒为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带一两个参数，第1个参数是数字，第2个参数表示空间大小。        ┃
┃                            ┃例如，“gzip_buffers 4 8k;”（通常用来表示有多少个ngx_buf_t缓冲区），其中第1  ┃
┃                            ┃个参数不可以携带任何单位，第2个参数不带任何单位时表示Byte，如果以k或          ┃
┃ngx_conf_set_bufs_slot      ┃者K作为单位，则表示Kilobyte，如果以m或者M作为单位，则表示Megabyte。           ┃
┃                            ┃ngx_conf_set- bufs—slot解析后会把配置项后的两个参数转化成ngx_bufs_t结构体下  ┃
┃                            ┃的两个成员。这个配置项对应于Nginx最喜欢用的多缓冲区的解决方案（如接收连       ┃
┃                            ┃接对端发来的TCP流）                                                           ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，其取值范围必须是我们设定好的字符串之一（就像       ┃
┃                            ┃C语言中的枚举一样）。首先，我们要用ngx_conf_enum_t结构定义配置项的取值范      ┃
┃ngx_conf_set_enum_slot      ┃                                                                              ┃
┃                            ┃围，并设定每个值对应的序列号。然后，ngx_conf_set enum slot将会把配置项参      ┃
┃                            ┃数转化为对应的序列号                                                          ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  写ngx_conf_set bitmask slot类似，配置项后必须携带1个参数，其取值范围必      ┃
┃                            ┃须是设定好的字符串之一。首先，我们要用ngx_conf_bitmask_t结构定义配置项的      ┃
┃ngx_conf_set_bitmask_slot   ┃                                                                              ┃
┃                            ┃取值范围，并设定每个值对应的比特位。注意，每个值所对应的比特位都要不同。      ┃
┃                            ┃然后ngx_conf_set_bitmask_ slot将会把配置项参数转化为对应的比特位              ┃
┗━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
┏━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名            ┃    行为                                                                    ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置目录或者文件的读写权限。配置项后可以携带1～3个参数，      ┃
┃                          ┃可以是如下形式：user:rw group:rw all:rw。注意，它的意义与Linux上文件或者目  ┃
┃ngx_conf_set_access_slot  ┃录的权限意义是一致的，但是user/group/all后面的权限只可以设为rw（读／写）或  ┃
┃                          ┃者r（只读），不可以有其他任何形式，如w或者rx等。ngx_conf_set- access slot   ┃
┃                          ┃将会把汶曲参数转化为一个整型                                                ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置路径，配置项后必须携带1个参数，表示1个有意义的路径。      ┃
┃ngx_conf_set_path_slot    ┃                                                                            ┃
┃                          ┃ngx_conf_set_path_slot将会把参数转化为ngx_path_t结构                        ┃
┗━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛*/
char *
ngx_conf_set_flag_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_str_t        *value;//value[0]对应参数名 value[1]对应配置后面的参数值
    ngx_flag_t       *fp;
    ngx_conf_post_t  *post;

    fp = (ngx_flag_t *) (p + cmd->offset);

    if (*fp != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;
    if (ngx_strcasecmp(value[1].data, (u_char *) "on") == 0) {
        *fp = 1;

    } else if (ngx_strcasecmp(value[1].data, (u_char *) "off") == 0) {
        *fp = 0;

    } else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                     "invalid value \"%s\" in \"%s\" directive, "
                     "it must be \"on\" or \"off\"",
                     value[1].data, cmd->name.data);
        return NGX_CONF_ERROR;
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, fp);
    }

    return NGX_CONF_OK;
}


char *
ngx_conf_set_str_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_str_t        *field, *value;
    ngx_conf_post_t  *post;

    field = (ngx_str_t *) (p + cmd->offset);

    if (field->data) {
        return "is duplicate";
    }

    value = cf->args->elts;

    *field = value[1];

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, field);
    }

    return NGX_CONF_OK;
}

/*
char*(*set)(ngx_conf_t *cf, ngx_commandj 'vcmd,void *conf)
    如果处理配置项，我们既可以自己实现一个回调方法来处理配置项（），也可以使用Nginx预设的14个解析配置项方法，这会少
写许多代码，表4-2列出了这些预设的解析配置项方法。

表4-2预设的14个配置项解析方法
┏━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名              ┃    行为                                                                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果nginx．conf文件中某个配置项的参数是on或者off（即希望配置项表达打开      ┃
┃                            ┃或者关闭某个功能的意思），而且在Nginx模块的代码中使用ngx_flag_t变量来保       ┃
┃ngx_conf_set_flag_slot      ┃存这个配置项的参数，就可以将set回调方法设为ngx_conf_set_flag_slot。当nginx.   ┃
┃                            ┃conf文件中参数是on时，代码中的ngx_flag_t类型变量将设为1，参数为off时则        ┃
┃                            ┃设为O                                                                         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果配置项后只有1个参数，同时在代码中我们希望用ngx_str_t类型的变量来保      ┃
┃ngx_conf_set_str_slot       ┃                                                                              ┃
┃                            ┃存这个配置项的参数，则可以使用ngx_conf_set_ str slot方法                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果这个配置项会出现多次，每个配置项后面都跟着1个参数，而在程序中我们       ┃
┃                            ┃希望仅用一个ngx_array_t动态数组（           ）来存储所有的参数，且数组中      ┃
┃ngx_conf_set_str_array_slot ┃                                                                              ┃
┃                            ┃的每个参数都以ngx_str_t来存储，那么预设的ngx_conf_set_str_array_slot有法可    ┃
┃                            ┃以帮我们做到                                                                  ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_str_array_slot类似，也是用一个ngx_array_t数组来存储所有同    ┃
┃                            ┃名配置项的参数。只是每个配置项的参数不再只是1个，而必须是两个，且以“配       ┃
┃ngx_conf_set_keyval_slot    ┃                                                                              ┃
┃                            ┃置项名关键字值；”的形式出现在nginx．conf文件中，同时，ngx_conf_set_keyval    ┃
┃                            ┃ slot将把这蜱配置项转化为数组，其中每个元素都存储着key/value键值对            ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_conf_set_num_slot       ┃  配置项后必须携带1个参数，且只能是数字。存储这个参数的变量必须是整型         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间大小，可以是一个数字，这时表示字节数       ┃
┃                            ┃(Byte)。如果数字后跟着k或者K，就表示Kilobyt，IKB=1024B；如果数字后跟          ┃
┃ngx_conf_set_size_slot      ┃                                                                              ┃
┃                            ┃着m或者M，就表示Megabyte，1MB=1024KB。ngx_conf_set_ size slot解析后将         ┃
┃                            ┃把配置项后的参数转化成以字节数为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间上的偏移量。它与设置的参数非常类似，       ┃
┃                            ┃其参数是一个数字时表示Byte，也可以在后面加单位，但与ngx_conf_set_size slot    ┃
┃ngx_conf_set_off_slot       ┃不同的是，数字后面的单位不仅可以是k或者K、m或者M，还可以是g或者G，            ┃
┃                            ┃这时表示Gigabyte，IGB=1024MB。ngx_conf_set_off_slot解析后将把配置项后的       ┃
┃                            ┃参数转化成以字节数为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示时间。这个参数可以在数字后面加单位，如         ┃
┃                            ┃果单位为s或者没有任何单位，那么这个数字表示秒；如果单位为m，则表示分          ┃
┃                            ┃钟，Im=60s：如果单位为h，则表示小时，th=60m：如果单位为d，则表示天，          ┃
┃ngx_conf_set_msec_slot      ┃                                                                              ┃
┃                            ┃ld=24h：如果单位为w，则表示周，lw=7d；如果单位为M，则表示月，1M=30d；         ┃
┃                            ┃如果单位为y，则表示年，ly=365d。ngx_conf_set_msec_slot解析后将把配置项后     ┃
┃                            ┃的参数转化成以毫秒为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_msec slot非常类似，唯一的区别是ngx_conf_set msec—slot解析   ┃
┃ngx_conf_set_sec_slot       ┃后将把配置项后的参数转化成以毫秒为单位的数字，而ngx_conf_set_sec一slot解析    ┃
┃                            ┃后会把配置项后的参数转化成以秒为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带一两个参数，第1个参数是数字，第2个参数表示空间大小。        ┃
┃                            ┃例如，“gzip_buffers 4 8k;”（通常用来表示有多少个ngx_buf_t缓冲区），其中第1  ┃
┃                            ┃个参数不可以携带任何单位，第2个参数不带任何单位时表示Byte，如果以k或          ┃
┃ngx_conf_set_bufs_slot      ┃者K作为单位，则表示Kilobyte，如果以m或者M作为单位，则表示Megabyte。           ┃
┃                            ┃ngx_conf_set- bufs—slot解析后会把配置项后的两个参数转化成ngx_bufs_t结构体下  ┃
┃                            ┃的两个成员。这个配置项对应于Nginx最喜欢用的多缓冲区的解决方案（如接收连       ┃
┃                            ┃接对端发来的TCP流）                                                           ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，其取值范围必须是我们设定好的字符串之一（就像       ┃
┃                            ┃C语言中的枚举一样）。首先，我们要用ngx_conf_enum_t结构定义配置项的取值范      ┃
┃ngx_conf_set_enum_slot      ┃                                                                              ┃
┃                            ┃围，并设定每个值对应的序列号。然后，ngx_conf_set enum slot将会把配置项参      ┃
┃                            ┃数转化为对应的序列号                                                          ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  写ngx_conf_set bitmask slot类似，配置项后必须携带1个参数，其取值范围必      ┃
┃                            ┃须是设定好的字符串之一。首先，我们要用ngx_conf_bitmask_t结构定义配置项的      ┃
┃ngx_conf_set_bitmask_slot   ┃                                                                              ┃
┃                            ┃取值范围，并设定每个值对应的比特位。注意，每个值所对应的比特位都要不同。      ┃
┃                            ┃然后ngx_conf_set_bitmask_ slot将会把配置项参数转化为对应的比特位              ┃
┗━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
┏━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名            ┃    行为                                                                    ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置目录或者文件的读写权限。配置项后可以携带1～3个参数，      ┃
┃                          ┃可以是如下形式：user:rw group:rw all:rw。注意，它的意义与Linux上文件或者目  ┃
┃ngx_conf_set_access slot  ┃录的权限意义是一致的，但是user/group/all后面的权限只可以设为rw（读／写）或  ┃
┃                          ┃者r（只读），不可以有其他任何形式，如w或者rx等。ngx_conf_set- access slot   ┃
┃                          ┃将会把汶曲参数转化为一个整型                                                ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置路径，配置项后必须携带1个参数，表示1个有意义的路径。      ┃
┃ngx_conf_set_path_slot    ┃                                                                            ┃
┃                          ┃ngx_conf_set_path_slot将会把参数转化为ngx_path_t结构                        ┃
┗━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛*/

/*
test_str_array配置项也只能出现在location{．．．}块内。如果有以下配置：
    location ... {
        test_str_array      Content-Length ;
        test_str_array      Content-Encoding ;
    }
    那么，my_str_array->nelts的值将是2，表示出现了两个test_str_array配置项。而且my_str_array->elts指向
ngx_str_t类型组成的数组，这样就可以按以下方式访问这两个值。
ngx_str_t*  pstr  =  mycf->my_str_array->elts ;
于是，pstr[0]和pstr[l]可以取到参数值，分别是{len=14;data=“Content-Length”；}和
{len=16;data=“Content-Encoding”；)。从这里可以看到，当处理HTTP头部这样的配置项
时是很适合使用ngx_conf_set_str_array_slot预设方法的。
*/
char *
ngx_conf_set_str_array_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_str_t         *value, *s;
    ngx_array_t      **a;
    ngx_conf_post_t   *post;

    a = (ngx_array_t **) (p + cmd->offset);

    if (*a == NGX_CONF_UNSET_PTR) {
        *a = ngx_array_create(cf->pool, 4, sizeof(ngx_str_t));
        if (*a == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    s = ngx_array_push(*a);
    if (s == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;
/*
    location /ttt {			
        test_str_array      Content-Length ;			
        test_str_array      Content-Encoding ;		
    }

    printf("yang test ...%u, [%s..%u]..[%s...%u].. <%s, %u>\n", cf->args->nelts, 
        value[0].data, value[0].len , value[1].data, value[1].len, __FUNCTION__, __LINE__);
    打印如下:
    yang test ...2, [test_str_array..14]..[Content-Length...14].. <ngx_conf_set_str_array_slot, 1274>
    yang test ...2, [test_str_array..14]..[Content-Encoding...16].. <ngx_conf_set_str_array_slot, 1274>
    
    如果是这个，则会调用两次ngx_conf_set_str_array_slot函数，
    第一次:value[0]对应的name对应test_str_array，value[1]的name对应Content-Length
    第一次:value[1]对应的name对应test_str_array，value[1]的name对应Content-Encoding
*/
    *s = value[1];

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, s);
    }

    return NGX_CONF_OK;
}

/*
注意  在ngx_http_mytest_create_loc_conf创建结构体时，如果想使用ngx_conf_set_
keyval_slot，必须把my_keyval初始化为NULL空指针，
否则ngx_conf_set_keyval_slot在解析时会报错。
*/
char *
ngx_conf_set_keyval_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_str_t         *value;
    ngx_array_t      **a;
    ngx_keyval_t      *kv;
    ngx_conf_post_t   *post;

    a = (ngx_array_t **) (p + cmd->offset);

    if (*a == NULL) {
        *a = ngx_array_create(cf->pool, 4, sizeof(ngx_keyval_t));
        if (*a == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    kv = ngx_array_push(*a);
    if (kv == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    kv->key = value[1];
    kv->value = value[2];

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, kv);
    }

    return NGX_CONF_OK;
}

/*
char*(*set)(ngx_conf_t *cf, ngx_commandj 'vcmd,void *conf)
    关于set回调方法，在处理mytest配置项时已经使用过，其中mytest配置项是
不带参数的。如果处理配置项，我们既可以自己实现一个回调方法来处理配置项（），也可以使用Nginx预设的14个解析配置项方法，这会少
写许多代码
┏━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名              ┃    行为                                                                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果nginx．conf文件中某个配置项的参数是on或者off（即希望配置项表达打开      ┃
┃                            ┃或者关闭某个功能的意思），而且在Nginx模块的代码中使用ngx_flag_t变量来保       ┃
┃ngx_conf_set_flag_slot      ┃存这个配置项的参数，就可以将set回调方法设为ngx_conf_set_flag_slot。当nginx.   ┃
┃                            ┃conf文件中参数是on时，代码中的ngx_flag_t类型变量将设为1，参数为off时则        ┃
┃                            ┃设为O                                                                         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果配置项后只有1个参数，同时在代码中我们希望用ngx_str_t类型的变量来保      ┃
┃ngx_conf_set_str_slot       ┃                                                                              ┃
┃                            ┃存这个配置项的参数，则可以使用ngx_conf_set_ str slot方法                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果这个配置项会出现多次，每个配置项后面都跟着1个参数，而在程序中我们       ┃
┃                            ┃希望仅用一个ngx_array_t动态数组（           ）来存储所有的参数，且数组中      ┃
┃ngx_conf_set_str_array_slot ┃                                                                              ┃
┃                            ┃的每个参数都以ngx_str_t来存储，那么预设的ngx_conf_set_str_array_slot有法可    ┃
┃                            ┃以帮我们做到                                                                  ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_str_array_slot类似，也是用一个ngx_array_t数组来存储所有同    ┃
┃                            ┃名配置项的参数。只是每个配置项的参数不再只是1个，而必须是两个，且以“配       ┃
┃ngx_conf_set_keyval_slot    ┃                                                                              ┃
┃                            ┃置项名关键字值；”的形式出现在nginx．conf文件中，同时，ngx_conf_set_keyval    ┃
┃                            ┃ slot将把这蜱配置项转化为数组，其中每个元素都存储着key/value键值对            ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_conf_set_num_slot       ┃  配置项后必须携带1个参数，且只能是数字。存储这个参数的变量必须是整型         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间大小，可以是一个数字，这时表示字节数       ┃
┃                            ┃(Byte)。如果数字后跟着k或者K，就表示Kilobyt，IKB=1024B；如果数字后跟          ┃
┃ngx_conf_set_size_slot      ┃                                                                              ┃
┃                            ┃着m或者M，就表示Megabyte，1MB=1024KB。ngx_conf_set_ size slot解析后将         ┃
┃                            ┃把配置项后的参数转化成以字节数为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间上的偏移量。它与设置的参数非常类似，       ┃
┃                            ┃其参数是一个数字时表示Byte，也可以在后面加单位，但与ngx_conf_set_size slot    ┃
┃ngx_conf_set_off_slot       ┃不同的是，数字后面的单位不仅可以是k或者K、m或者M，还可以是g或者G，            ┃
┃                            ┃这时表示Gigabyte，IGB=1024MB。ngx_conf_set_off_slot解析后将把配置项后的       ┃
┃                            ┃参数转化成以字节数为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示时间。这个参数可以在数字后面加单位，如         ┃
┃                            ┃果单位为s或者没有任何单位，那么这个数字表示秒；如果单位为m，则表示分          ┃
┃                            ┃钟，Im=60s：如果单位为h，则表示小时，th=60m：如果单位为d，则表示天，          ┃
┃ngx_conf_set_msec_slot      ┃                                                                              ┃
┃                            ┃ld=24h：如果单位为w，则表示周，lw=7d；如果单位为M，则表示月，1M=30d；         ┃
┃                            ┃如果单位为y，则表示年，ly=365d。ngx_conf_set_msec_slot解析后将把配置项后     ┃
┃                            ┃的参数转化成以毫秒为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_msec slot非常类似，唯一的区别是ngx_conf_set msec—slot解析   ┃
┃ngx_conf_set_sec_slot       ┃后将把配置项后的参数转化成以毫秒为单位的数字，而ngx_conf_set_sec一slot解析    ┃
┃                            ┃后会把配置项后的参数转化成以秒为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带一两个参数，第1个参数是数字，第2个参数表示空间大小。        ┃
┃                            ┃例如，“gzip_buffers 4 8k;”（通常用来表示有多少个ngx_buf_t缓冲区），其中第1  ┃
┃                            ┃个参数不可以携带任何单位，第2个参数不带任何单位时表示Byte，如果以k或          ┃
┃ngx_conf_set_bufs_slot      ┃者K作为单位，则表示Kilobyte，如果以m或者M作为单位，则表示Megabyte。           ┃
┃                            ┃ngx_conf_set- bufs—slot解析后会把配置项后的两个参数转化成ngx_bufs_t结构体下  ┃
┃                            ┃的两个成员。这个配置项对应于Nginx最喜欢用的多缓冲区的解决方案（如接收连       ┃
┃                            ┃接对端发来的TCP流）                                                           ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，其取值范围必须是我们设定好的字符串之一（就像       ┃
┃                            ┃C语言中的枚举一样）。首先，我们要用ngx_conf_enum_t结构定义配置项的取值范      ┃
┃ngx_conf_set_enum_slot      ┃                                                                              ┃
┃                            ┃围，并设定每个值对应的序列号。然后，ngx_conf_set enum slot将会把配置项参      ┃
┃                            ┃数转化为对应的序列号                                                          ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  写ngx_conf_set bitmask slot类似，配置项后必须携带1个参数，其取值范围必      ┃
┃                            ┃须是设定好的字符串之一。首先，我们要用ngx_conf_bitmask_t结构定义配置项的      ┃
┃ngx_conf_set_bitmask_slot   ┃                                                                              ┃
┃                            ┃取值范围，并设定每个值对应的比特位。注意，每个值所对应的比特位都要不同。      ┃
┃                            ┃然后ngx_conf_set_bitmask_ slot将会把配置项参数转化为对应的比特位              ┃
┗━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
┏━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名            ┃    行为                                                                    ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置目录或者文件的读写权限。配置项后可以携带1～3个参数，      ┃
┃                          ┃可以是如下形式：user:rw group:rw all:rw。注意，它的意义与Linux上文件或者目  ┃
┃ngx_conf_set_access slot  ┃录的权限意义是一致的，但是user/group/all后面的权限只可以设为rw（读／写）或  ┃
┃                          ┃者r（只读），不可以有其他任何形式，如w或者rx等。ngx_conf_set- access slot   ┃
┃                          ┃将会把汶曲参数转化为一个整型                                                ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置路径，配置项后必须携带1个参数，表示1个有意义的路径。      ┃
┃ngx_conf_set_path_slot    ┃                                                                            ┃
┃                          ┃ngx_conf_set_path_slot将会把参数转化为ngx_path_t结构                        ┃
┗━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
*/

/*
注意在ngx_http_mytest_create loc conf创建结构体时，如果想使用ngx_conf_set_num_slot,
必须把my_num初始化为NGX_CONF_UNSET宏  否则ngx_conf_set_num_slot在解析时会报错。
*/
char *
ngx_conf_set_num_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_int_t        *np;
    ngx_str_t        *value;
    ngx_conf_post_t  *post;


    np = (ngx_int_t *) (p + cmd->offset);

    if (*np != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;
    *np = ngx_atoi(value[1].data, value[1].len);
    if (*np == NGX_ERROR) {
        return "invalid number";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, np);
    }

    return NGX_CONF_OK;
}

/*
如果希望配置项表达的含义是空间大小，那么用ngx_conf_set_size_slot来解析配置项是
非常合适的，因为ngx_conf_set_size_slot允许配置项的参数后有单位，例如，k或者K表示
Kilobyte，m或者M表示Megabyte。用ngx_http_mytest_conf_t结构中的size_t my_size;来
存储参数，解析后的my_size表示的单位是字节。

如果在nginx.conf中配置了test_size lOk;，那么my_size将会设置为10240。如果配置
为test_size lOm;，则my_size会设置为10485760。
    ngx_conf_set size slot只允许配置项后的参数携带单位k或者K、m或者M，不允许有
g或者G的出现，这与ngx_conf_set_off_slot是不同的。
    注意  在ngx_http_mytest_create_loc_conf创建结构体时，如果想使用ngx_conf_set_size_slot，
    必须把my_size初始化为NGX_CONF_UNSET_SIZE
    mycf->my_size= NGX_CONF_UNSET_SIZE;，否则ngx_conf_set_size_slot在解析时会报错。
*/
char *
ngx_conf_set_size_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    size_t           *sp;
    ngx_str_t        *value;
    ngx_conf_post_t  *post;


    sp = (size_t *) (p + cmd->offset);
    if (*sp != NGX_CONF_UNSET_SIZE) {
        return "is duplicate";
    }

    value = cf->args->elts;

    *sp = ngx_parse_size(&value[1]);
    if (*sp == (size_t) NGX_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, sp);
    }

    return NGX_CONF_OK;
}

/*
char*(*set)(ngx_conf_t *cf, ngx_commandj 'vcmd,void *conf)
    关于set回调方法，在处理mytest配置项时已经使用过，其中mytest配置项是
不带参数的。如果处理配置项，我们既可以自己实现一个回调方法来处理配置项（），也可以使用Nginx预设的14个解析配置项方法，这会少
写许多代码，
┏━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名              ┃    行为                                                                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果nginx．conf文件中某个配置项的参数是on或者off（即希望配置项表达打开      ┃
┃                            ┃或者关闭某个功能的意思），而且在Nginx模块的代码中使用ngx_flag_t变量来保       ┃
┃ngx_conf_set_flag_slot      ┃存这个配置项的参数，就可以将set回调方法设为ngx_conf_set_flag_slot。当nginx.   ┃
┃                            ┃conf文件中参数是on时，代码中的ngx_flag_t类型变量将设为1，参数为off时则        ┃
┃                            ┃设为O                                                                         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果配置项后只有1个参数，同时在代码中我们希望用ngx_str_t类型的变量来保      ┃
┃ngx_conf_set_str_slot       ┃                                                                              ┃
┃                            ┃存这个配置项的参数，则可以使用ngx_conf_set_ str slot方法                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果这个配置项会出现多次，每个配置项后面都跟着1个参数，而在程序中我们       ┃
┃                            ┃希望仅用一个ngx_array_t动态数组（           ）来存储所有的参数，且数组中      ┃
┃ngx_conf_set_str_array_slot ┃                                                                              ┃
┃                            ┃的每个参数都以ngx_str_t来存储，那么预设的ngx_conf_set_str_array_slot有法可    ┃
┃                            ┃以帮我们做到                                                                  ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_str_array_slot类似，也是用一个ngx_array_t数组来存储所有同    ┃
┃                            ┃名配置项的参数。只是每个配置项的参数不再只是1个，而必须是两个，且以“配       ┃
┃ngx_conf_set_keyval_slot    ┃                                                                              ┃
┃                            ┃置项名关键字值；”的形式出现在nginx．conf文件中，同时，ngx_conf_set_keyval    ┃
┃                            ┃ slot将把这蜱配置项转化为数组，其中每个元素都存储着key/value键值对            ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_conf_set_num_slot       ┃  配置项后必须携带1个参数，且只能是数字。存储这个参数的变量必须是整型         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间大小，可以是一个数字，这时表示字节数       ┃
┃                            ┃(Byte)。如果数字后跟着k或者K，就表示Kilobyt，IKB=1024B；如果数字后跟          ┃
┃ngx_conf_set_size_slot      ┃                                                                              ┃
┃                            ┃着m或者M，就表示Megabyte，1MB=1024KB。ngx_conf_set_ size slot解析后将         ┃
┃                            ┃把配置项后的参数转化成以字节数为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间上的偏移量。它与设置的参数非常类似，       ┃
┃                            ┃其参数是一个数字时表示Byte，也可以在后面加单位，但与ngx_conf_set_size slot    ┃
┃ngx_conf_set_off_slot       ┃不同的是，数字后面的单位不仅可以是k或者K、m或者M，还可以是g或者G，            ┃
┃                            ┃这时表示Gigabyte，IGB=1024MB。ngx_conf_set_off_slot解析后将把配置项后的       ┃
┃                            ┃参数转化成以字节数为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示时间。这个参数可以在数字后面加单位，如         ┃
┃                            ┃果单位为s或者没有任何单位，那么这个数字表示秒；如果单位为m，则表示分          ┃
┃                            ┃钟，Im=60s：如果单位为h，则表示小时，th=60m：如果单位为d，则表示天，          ┃
┃ngx_conf_set_msec_slot      ┃                                                                              ┃
┃                            ┃ld=24h：如果单位为w，则表示周，lw=7d；如果单位为M，则表示月，1M=30d；         ┃
┃                            ┃如果单位为y，则表示年，ly=365d。ngx_conf_set_msec_slot解析后将把配置项后     ┃
┃                            ┃的参数转化成以毫秒为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_msec slot非常类似，唯一的区别是ngx_conf_set msec—slot解析   ┃
┃ngx_conf_set_sec_slot       ┃后将把配置项后的参数转化成以毫秒为单位的数字，而ngx_conf_set_sec一slot解析    ┃
┃                            ┃后会把配置项后的参数转化成以秒为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带一两个参数，第1个参数是数字，第2个参数表示空间大小。        ┃
┃                            ┃例如，“gzip_buffers 4 8k;”（通常用来表示有多少个ngx_buf_t缓冲区），其中第1  ┃
┃                            ┃个参数不可以携带任何单位，第2个参数不带任何单位时表示Byte，如果以k或          ┃
┃ngx_conf_set_bufs_slot      ┃者K作为单位，则表示Kilobyte，如果以m或者M作为单位，则表示Megabyte。           ┃
┃                            ┃ngx_conf_set- bufs—slot解析后会把配置项后的两个参数转化成ngx_bufs_t结构体下  ┃
┃                            ┃的两个成员。这个配置项对应于Nginx最喜欢用的多缓冲区的解决方案（如接收连       ┃
┃                            ┃接对端发来的TCP流）                                                           ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，其取值范围必须是我们设定好的字符串之一（就像       ┃
┃                            ┃C语言中的枚举一样）。首先，我们要用ngx_conf_enum_t结构定义配置项的取值范      ┃
┃ngx_conf_set_enum_slot      ┃                                                                              ┃
┃                            ┃围，并设定每个值对应的序列号。然后，ngx_conf_set enum slot将会把配置项参      ┃
┃                            ┃数转化为对应的序列号                                                          ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  写ngx_conf_set bitmask slot类似，配置项后必须携带1个参数，其取值范围必      ┃
┃                            ┃须是设定好的字符串之一。首先，我们要用ngx_conf_bitmask_t结构定义配置项的      ┃
┃ngx_conf_set_bitmask_slot   ┃                                                                              ┃
┃                            ┃取值范围，并设定每个值对应的比特位。注意，每个值所对应的比特位都要不同。      ┃
┃                            ┃然后ngx_conf_set_bitmask_ slot将会把配置项参数转化为对应的比特位              ┃
┗━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
┏━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名            ┃    行为                                                                    ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置目录或者文件的读写权限。配置项后可以携带1～3个参数，      ┃
┃                          ┃可以是如下形式：user:rw group:rw all:rw。注意，它的意义与Linux上文件或者目  ┃
┃ngx_conf_set_access slot  ┃录的权限意义是一致的，但是user/group/all后面的权限只可以设为rw（读／写）或  ┃
┃                          ┃者r（只读），不可以有其他任何形式，如w或者rx等。ngx_conf_set- access slot   ┃
┃                          ┃将会把汶曲参数转化为一个整型                                                ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置路径，配置项后必须携带1个参数，表示1个有意义的路径。      ┃
┃ngx_conf_set_path_slot    ┃                                                                            ┃
┃                          ┃ngx_conf_set_path_slot将会把参数转化为ngx_path_t结构                        ┃
┗━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛*/

/*
如果希望配置项表达的含义是空间的偏移位置，那么可以使用ngx_conf_set_off_slot预
设方法。事实上，ngx_conf_set_off_slot与ngx_conf_set_size_slot是非常相似的，最大的区
别是ngx_conf_set_off_slot支持的参数单位还要多1个g或者G，表示Gigabyte。用ngx_http_mytest_conf_t
结构中的off_t my_off;来存储参数，解析后的my_off表示的偏移量单位是字节。

如果在nginx.conf中配置了test_off lg，那么my_off将会设置为1073741824。当它的
单位为k、K、m、M时，其意义与ngx_conf_set_size_slot相同。

注意  在ngx_http_mytest_create loc conf创建结构体时，如果想使用ngx_conf_set_
off_slot，必须把my_off初始化为NGX_CONF_UNSET宏，否则ngx_conf_set off slot在解析时会报错。

*/
char *
ngx_conf_set_off_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    off_t            *op;
    ngx_str_t        *value;
    ngx_conf_post_t  *post;


    op = (off_t *) (p + cmd->offset);
    if (*op != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    *op = ngx_parse_offset(&value[1]);
    if (*op == (off_t) NGX_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, op);
    }

    return NGX_CONF_OK;
}

/*
ngx_conf_set sec slot与ngx_conf_set msec slot非常桕似，  只是ngx_conf_set sec slot
在用ngx_http_mytest_conf_t结构体中的time_t my_sec;来存储参数时，解析后的my_sec表
示的时间单位是秒，而ngx_conf_set_msec_slot为毫秒。

注意  在ngx_http_mytest_create_loc_conf创建结构体时，如果想使用ngx_c onf_set_
sec slot，那么必须把my_sec初始化为NGX CONF UNSET宏    否则ngx_conf_set_sec_slot在解析时会报错。
*/
char *
ngx_conf_set_msec_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_msec_t       *msp;
    ngx_str_t        *value;
    ngx_conf_post_t  *post;


    msp = (ngx_msec_t *) (p + cmd->offset);
    if (*msp != NGX_CONF_UNSET_MSEC) {
        return "is duplicate";
    }

    value = cf->args->elts;

    *msp = ngx_parse_time(&value[1], 0);
    if (*msp == (ngx_msec_t) NGX_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, msp);
    }

    return NGX_CONF_OK;
}

/*
char*(*set)(ngx_conf_t *cf, ngx_commandj 'vcmd,void *conf)
    关于set回调方法，在处理mytest配置项时已经使用过，其中mytest配置项是
不带参数的。如果处理配置项，我们既可以自己实现一个回调方法来处理配置项（），也可以使用Nginx预设的14个解析配置项方法，这会少
写许多代码
┏━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名              ┃    行为                                                                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果nginx．conf文件中某个配置项的参数是on或者off（即希望配置项表达打开      ┃
┃                            ┃或者关闭某个功能的意思），而且在Nginx模块的代码中使用ngx_flag_t变量来保       ┃
┃ngx_conf_set_flag_slot      ┃存这个配置项的参数，就可以将set回调方法设为ngx_conf_set_flag_slot。当nginx.   ┃
┃                            ┃conf文件中参数是on时，代码中的ngx_flag_t类型变量将设为1，参数为off时则        ┃
┃                            ┃设为O                                                                         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果配置项后只有1个参数，同时在代码中我们希望用ngx_str_t类型的变量来保      ┃
┃ngx_conf_set_str_slot       ┃                                                                              ┃
┃                            ┃存这个配置项的参数，则可以使用ngx_conf_set_ str slot方法                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果这个配置项会出现多次，每个配置项后面都跟着1个参数，而在程序中我们       ┃
┃                            ┃希望仅用一个ngx_array_t动态数组（           ）来存储所有的参数，且数组中      ┃
┃ngx_conf_set_str_array_slot ┃                                                                              ┃
┃                            ┃的每个参数都以ngx_str_t来存储，那么预设的ngx_conf_set_str_array_slot有法可    ┃
┃                            ┃以帮我们做到                                                                  ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_str_array_slot类似，也是用一个ngx_array_t数组来存储所有同    ┃
┃                            ┃名配置项的参数。只是每个配置项的参数不再只是1个，而必须是两个，且以“配       ┃
┃ngx_conf_set_keyval_slot    ┃                                                                              ┃
┃                            ┃置项名关键字值；”的形式出现在nginx．conf文件中，同时，ngx_conf_set_keyval    ┃
┃                            ┃ slot将把这蜱配置项转化为数组，其中每个元素都存储着key/value键值对            ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_conf_set_num_slot       ┃  配置项后必须携带1个参数，且只能是数字。存储这个参数的变量必须是整型         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间大小，可以是一个数字，这时表示字节数       ┃
┃                            ┃(Byte)。如果数字后跟着k或者K，就表示Kilobyt，IKB=1024B；如果数字后跟          ┃
┃ngx_conf_set_size_slot      ┃                                                                              ┃
┃                            ┃着m或者M，就表示Megabyte，1MB=1024KB。ngx_conf_set_ size slot解析后将         ┃
┃                            ┃把配置项后的参数转化成以字节数为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间上的偏移量。它与设置的参数非常类似，       ┃
┃                            ┃其参数是一个数字时表示Byte，也可以在后面加单位，但与ngx_conf_set_size slot    ┃
┃ngx_conf_set_off_slot       ┃不同的是，数字后面的单位不仅可以是k或者K、m或者M，还可以是g或者G，            ┃
┃                            ┃这时表示Gigabyte，IGB=1024MB。ngx_conf_set_off_slot解析后将把配置项后的       ┃
┃                            ┃参数转化成以字节数为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示时间。这个参数可以在数字后面加单位，如         ┃
┃                            ┃果单位为s或者没有任何单位，那么这个数字表示秒；如果单位为m，则表示分          ┃
┃                            ┃钟，Im=60s：如果单位为h，则表示小时，th=60m：如果单位为d，则表示天，          ┃
┃ngx_conf_set_msec_slot      ┃                                                                              ┃
┃                            ┃ld=24h：如果单位为w，则表示周，lw=7d；如果单位为M，则表示月，1M=30d；         ┃
┃                            ┃如果单位为y，则表示年，ly=365d。ngx_conf_set_msec_slot解析后将把配置项后     ┃
┃                            ┃的参数转化成以毫秒为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_msec slot非常类似，唯一的区别是ngx_conf_set msec—slot解析   ┃
┃ngx_conf_set_sec_slot       ┃后将把配置项后的参数转化成以毫秒为单位的数字，而ngx_conf_set_sec一slot解析    ┃
┃                            ┃后会把配置项后的参数转化成以秒为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带一两个参数，第1个参数是数字，第2个参数表示空间大小。        ┃
┃                            ┃例如，“gzip_buffers 4 8k;”（通常用来表示有多少个ngx_buf_t缓冲区），其中第1  ┃
┃                            ┃个参数不可以携带任何单位，第2个参数不带任何单位时表示Byte，如果以k或          ┃
┃ngx_conf_set_bufs_slot      ┃者K作为单位，则表示Kilobyte，如果以m或者M作为单位，则表示Megabyte。           ┃
┃                            ┃ngx_conf_set- bufs—slot解析后会把配置项后的两个参数转化成ngx_bufs_t结构体下  ┃
┃                            ┃的两个成员。这个配置项对应于Nginx最喜欢用的多缓冲区的解决方案（如接收连       ┃
┃                            ┃接对端发来的TCP流）                                                           ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，其取值范围必须是我们设定好的字符串之一（就像       ┃
┃                            ┃C语言中的枚举一样）。首先，我们要用ngx_conf_enum_t结构定义配置项的取值范      ┃
┃ngx_conf_set_enum_slot      ┃                                                                              ┃
┃                            ┃围，并设定每个值对应的序列号。然后，ngx_conf_set enum slot将会把配置项参      ┃
┃                            ┃数转化为对应的序列号                                                          ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  写ngx_conf_set bitmask slot类似，配置项后必须携带1个参数，其取值范围必      ┃
┃                            ┃须是设定好的字符串之一。首先，我们要用ngx_conf_bitmask_t结构定义配置项的      ┃
┃ngx_conf_set_bitmask_slot   ┃                                                                              ┃
┃                            ┃取值范围，并设定每个值对应的比特位。注意，每个值所对应的比特位都要不同。      ┃
┃                            ┃然后ngx_conf_set_bitmask_ slot将会把配置项参数转化为对应的比特位              ┃
┗━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
┏━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名            ┃    行为                                                                    ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置目录或者文件的读写权限。配置项后可以携带1～3个参数，      ┃
┃                          ┃可以是如下形式：user:rw group:rw all:rw。注意，它的意义与Linux上文件或者目  ┃
┃ngx_conf_set_access slot  ┃录的权限意义是一致的，但是user/group/all后面的权限只可以设为rw（读／写）或  ┃
┃                          ┃者r（只读），不可以有其他任何形式，如w或者rx等。ngx_conf_set- access slot   ┃
┃                          ┃将会把汶曲参数转化为一个整型                                                ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置路径，配置项后必须携带1个参数，表示1个有意义的路径。      ┃
┃ngx_conf_set_path_slot    ┃                                                                            ┃
┃                          ┃ngx_conf_set_path_slot将会把参数转化为ngx_path_t结构                        ┃
┗━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

*/

char *
ngx_conf_set_sec_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    time_t           *sp;
    ngx_str_t        *value;
    ngx_conf_post_t  *post;


    sp = (time_t *) (p + cmd->offset);
    if (*sp != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    *sp = ngx_parse_time(&value[1], 1);
    if (*sp == (time_t) NGX_ERROR) {
        return "invalid value";
    }

    if (cmd->post) {
        post = cmd->post;
        return post->post_handler(cf, post, sp);
    }

    return NGX_CONF_OK;
}

//例如fastcgi_buffers  5 3K     output_buffers  5   3K
char *
ngx_conf_set_bufs_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char *p = conf;

    ngx_str_t   *value;
    ngx_bufs_t  *bufs;


    bufs = (ngx_bufs_t *) (p + cmd->offset);
    if (bufs->num) {
        return "is duplicate";
    }

    value = cf->args->elts;

    bufs->num = ngx_atoi(value[1].data, value[1].len);
    if (bufs->num == NGX_ERROR || bufs->num == 0) {
        return "invalid value";
    }

    bufs->size = ngx_parse_size(&value[2]);
    if (bufs->size == (size_t) NGX_ERROR || bufs->size == 0) {
        return "invalid value";
    }

    return NGX_CONF_OK;
}

/*
ngx_conf_set enum_ slot表示枚举配置项，也就是说，Nginx模块代码中将会指定配置项
的参数值只能是已经定义好的ngx_conf_enumt数组中name字符串中的一个。先看看ngx_conf enum_t的定义如下所示。
typedef struct {
      ngx_s t r_t    name ;
     ngx_uint_t value;
} ngx_conf_enum_t;
    其中，name表示配置项后的参数只能与name指向的字符串相等，而value表示如果参
数中出现了name，ngx_conf_set enum slot方法将会把对应的value设置到存储的变量中。
例如：
static ngx_conf_enum_t   test_enums []  =
    { ngx_string("apple") ,  1 },
     { ngx_string("banana"), 2 },
    { ngx_string("orange") ,  3 },
    { ngx_null_string, O }
    土面这个例子表示，配置项中的参数必须是apple、banana、orange其中之一。注意，必
须以ngx_null_string结尾。需要用ngx_uint_t来存储解析后的参数，在设置ngx_
command t时，需要把上面例子中定义的test enums数组传给post指针，如下所示。
static ngx_command_t ngx_http_mytest_commands []  =  {
    {   ngx_string ( " test_enum" ) ,
              NGX_HTTP_LOC_CONF I NGX_CONF_TAKEI,
            ngx_conf set enum_slot, NGX_HTTP LOC_CONF_OFFSET,
                offsetof (ngx_http_mytest conf_t,  my_enum_seq) ,
                 test enums },
		ngx_null_command
    }

    这样，如果在nginx.conf中出现了配置项test enum banana;，my_enum_seq的值是2。
如果配置项test enum幽现了除apple、banana、orange之外的值，Nginx将会报“invalid value”错误。
*/
char *
ngx_conf_set_enum_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_uint_t       *np, i;
    ngx_str_t        *value;
    ngx_conf_enum_t  *e;

    np = (ngx_uint_t *) (p + cmd->offset);

    if (*np != NGX_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elts;
    e = cmd->post;//post为ngx_command_t的最后一个参数，需要我们提供空间存储  

    for (i = 0; e[i].name.len != 0; i++) { //这也就是为什么在提交定义post中的ngx_conf_enum_t中为什么最后一个成员一定要为{ ngx_null_string, 0 }
        if (e[i].name.len != value[1].len
            || ngx_strcasecmp(e[i].name.data, value[1].data) != 0)
        {
            continue;
        }

        *np = e[i].value;

        return NGX_CONF_OK;
    }

    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "invalid value \"%s\"", value[1].data);

    return NGX_CONF_ERROR;
}

/*
char*(*set)(ngx_conf_t *cf, ngx_commandj 'vcmd,void *conf)
    关于set回调方法，在处理mytest配置项时已经使用过，其中mytest配置项是
不带参数的。如果处理配置项，我们既可以自己实现一个回调方法来处理配置项（），也可以使用Nginx预设的14个解析配置项方法，这会少
写许多代码
┏━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名              ┃    行为                                                                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果nginx．conf文件中某个配置项的参数是on或者off（即希望配置项表达打开      ┃
┃                            ┃或者关闭某个功能的意思），而且在Nginx模块的代码中使用ngx_flag_t变量来保       ┃
┃ngx_conf_set_flag_slot      ┃存这个配置项的参数，就可以将set回调方法设为ngx_conf_set_flag_slot。当nginx.   ┃
┃                            ┃conf文件中参数是on时，代码中的ngx_flag_t类型变量将设为1，参数为off时则        ┃
┃                            ┃设为O                                                                         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果配置项后只有1个参数，同时在代码中我们希望用ngx_str_t类型的变量来保      ┃
┃ngx_conf_set_str_slot       ┃                                                                              ┃
┃                            ┃存这个配置项的参数，则可以使用ngx_conf_set_ str slot方法                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果这个配置项会出现多次，每个配置项后面都跟着1个参数，而在程序中我们       ┃
┃                            ┃希望仅用一个ngx_array_t动态数组（           ）来存储所有的参数，且数组中      ┃
┃ngx_conf_set_str_array_slot ┃                                                                              ┃
┃                            ┃的每个参数都以ngx_str_t来存储，那么预设的ngx_conf_set_str_array_slot有法可    ┃
┃                            ┃以帮我们做到                                                                  ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_str_array_slot类似，也是用一个ngx_array_t数组来存储所有同    ┃
┃                            ┃名配置项的参数。只是每个配置项的参数不再只是1个，而必须是两个，且以“配       ┃
┃ngx_conf_set_keyval_slot    ┃                                                                              ┃
┃                            ┃置项名关键字值；”的形式出现在nginx．conf文件中，同时，ngx_conf_set_keyval    ┃
┃                            ┃ slot将把这蜱配置项转化为数组，其中每个元素都存储着key/value键值对            ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_conf_set_num_slot       ┃  配置项后必须携带1个参数，且只能是数字。存储这个参数的变量必须是整型         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间大小，可以是一个数字，这时表示字节数       ┃
┃                            ┃(Byte)。如果数字后跟着k或者K，就表示Kilobyt，IKB=1024B；如果数字后跟          ┃
┃ngx_conf_set_size_slot      ┃                                                                              ┃
┃                            ┃着m或者M，就表示Megabyte，1MB=1024KB。ngx_conf_set_ size slot解析后将         ┃
┃                            ┃把配置项后的参数转化成以字节数为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间上的偏移量。它与设置的参数非常类似，       ┃
┃                            ┃其参数是一个数字时表示Byte，也可以在后面加单位，但与ngx_conf_set_size slot    ┃
┃ngx_conf_set_off_slot       ┃不同的是，数字后面的单位不仅可以是k或者K、m或者M，还可以是g或者G，            ┃
┃                            ┃这时表示Gigabyte，IGB=1024MB。ngx_conf_set_off_slot解析后将把配置项后的       ┃
┃                            ┃参数转化成以字节数为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示时间。这个参数可以在数字后面加单位，如         ┃
┃                            ┃果单位为s或者没有任何单位，那么这个数字表示秒；如果单位为m，则表示分          ┃
┃                            ┃钟，Im=60s：如果单位为h，则表示小时，th=60m：如果单位为d，则表示天，          ┃
┃ngx_conf_set_msec_slot      ┃                                                                              ┃
┃                            ┃ld=24h：如果单位为w，则表示周，lw=7d；如果单位为M，则表示月，1M=30d；         ┃
┃                            ┃如果单位为y，则表示年，ly=365d。ngx_conf_set_msec_slot解析后将把配置项后     ┃
┃                            ┃的参数转化成以毫秒为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_msec slot非常类似，唯一的区别是ngx_conf_set msec—slot解析   ┃
┃ngx_conf_set_sec_slot       ┃后将把配置项后的参数转化成以毫秒为单位的数字，而ngx_conf_set_sec一slot解析    ┃
┃                            ┃后会把配置项后的参数转化成以秒为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带一两个参数，第1个参数是数字，第2个参数表示空间大小。        ┃
┃                            ┃例如，“gzip_buffers 4 8k;”（通常用来表示有多少个ngx_buf_t缓冲区），其中第1  ┃
┃                            ┃个参数不可以携带任何单位，第2个参数不带任何单位时表示Byte，如果以k或          ┃
┃ngx_conf_set_bufs_slot      ┃者K作为单位，则表示Kilobyte，如果以m或者M作为单位，则表示Megabyte。           ┃
┃                            ┃ngx_conf_set- bufs—slot解析后会把配置项后的两个参数转化成ngx_bufs_t结构体下  ┃
┃                            ┃的两个成员。这个配置项对应于Nginx最喜欢用的多缓冲区的解决方案（如接收连       ┃
┃                            ┃接对端发来的TCP流）                                                           ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，其取值范围必须是我们设定好的字符串之一（就像       ┃
┃                            ┃C语言中的枚举一样）。首先，我们要用ngx_conf_enum_t结构定义配置项的取值范      ┃
┃ngx_conf_set_enum_slot      ┃                                                                              ┃
┃                            ┃围，并设定每个值对应的序列号。然后，ngx_conf_set enum slot将会把配置项参      ┃
┃                            ┃数转化为对应的序列号                                                          ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  写ngx_conf_set bitmask slot类似，配置项后必须携带1个参数，其取值范围必      ┃
┃                            ┃须是设定好的字符串之一。首先，我们要用ngx_conf_bitmask_t结构定义配置项的      ┃
┃ngx_conf_set_bitmask_slot   ┃                                                                              ┃
┃                            ┃取值范围，并设定每个值对应的比特位。注意，每个值所对应的比特位都要不同。      ┃
┃                            ┃然后ngx_conf_set_bitmask_ slot将会把配置项参数转化为对应的比特位              ┃
┗━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
┏━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名            ┃    行为                                                                    ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置目录或者文件的读写权限。配置项后可以携带1～3个参数，      ┃
┃                          ┃可以是如下形式：user:rw group:rw all:rw。注意，它的意义与Linux上文件或者目  ┃
┃ngx_conf_set_access slot  ┃录的权限意义是一致的，但是user/group/all后面的权限只可以设为rw（读／写）或  ┃
┃                          ┃者r（只读），不可以有其他任何形式，如w或者rx等。ngx_conf_set- access slot   ┃
┃                          ┃将会把汶曲参数转化为一个整型                                                ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置路径，配置项后必须携带1个参数，表示1个有意义的路径。      ┃
┃ngx_conf_set_path_slot    ┃                                                                            ┃
┃                          ┃ngx_conf_set_path_slot将会把参数转化为ngx_path_t结构                        ┃
┗━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

*/

/*
ngx_conf_set_bitmask_slot与ngx_conf_set_enum_slot也是非常相似的，配置项的参数
都必须是枚举成员，唯一的差别在于效率方面，ngx_conf_set_enum_slot中枚举成员的对应
值是整型，表示序列号，它的取值范围是整型的范围；而ngx_conf_set_bitmask_slot中枚举
成员的对应值虽然也是整型，但可以按位比较，它的效率要高得多。也就是说，整型是4字
节（32位）的话，在这个枚举配置项中最多只能有32项。
    由于ngx_conf_set- bitmask- slot与ngx_conf_set enum- slot这两个预设解析方法在名称
上的差别，用来表示配置项参数的枚举取值结构体也由ngx_conf_enum t变成了ngx_conf_
bitmask t，但它们并没有区别。
typedef struct {
     ngx_str t name;
     ngx_uint_t mask;
  ngx_conf_bitmask_t ;
下面以定义test_ bitmasks数组为例来进行说明。
static ngx_conf_bitmask_t   test_bitmasks Ll  = {
	{ngx_string ("good") ,  Ox0002  } ,
	{ngx_string ( "better") ,  Ox0004  } ,
	{ngx_string ( "best") ,  Ox0008  } ,
	{ngx_null_string, O}
}
    如果配置顼名称定义为test_bitmask，在nginx.conf文件中test bitmask配置项后的参数
只能是good、better、best这3个值之一。我们用ngx_http_myte st_conf_t中的以下成员：
ngx_uint_t my_bitmask;
来存储test bitmask的参数，如下所示。
static ngx_command_t   ngx_http_mytest_commands []  =  {
    {   ngx_string ( " test_bitmask " )  ,
                NGX_HTTP LOC CONF I NGX_CONF_TAKEI,
            ngx_conf set bitmask _slot,
                 NGX HTTP LOC CONF_OFFSET,
                offsetof (ngx_http_mytest conf_t, my_bitmask) ,
                test bitmasks 
		},
		ngx_null_command
    }

如果在nginx.conf中出现配置项test- bitmask best;，那么my_bitmask的值是Ox8。
*/
char *
ngx_conf_set_bitmask_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ngx_uint_t          *np, i, m;
    ngx_str_t           *value;
    ngx_conf_bitmask_t  *mask;


    np = (ngx_uint_t *) (p + cmd->offset);
    value = cf->args->elts;
    mask = cmd->post; //参考test_bitmasks  把该数组中的字符串转换为对应的位图

    for (i = 1; i < cf->args->nelts; i++) {
        for (m = 0; mask[m].name.len != 0; m++) {

            if (mask[m].name.len != value[i].len
                || ngx_strcasecmp(mask[m].name.data, value[i].data) != 0)
            {
                continue;
            }

            if (*np & mask[m].mask) {
                ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                                   "duplicate value \"%s\"", value[i].data);

            } else {
                *np |= mask[m].mask;
            }

            break;
        }

        if (mask[m].name.len == 0) {
            ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                               "invalid value \"%s\"", value[i].data);

            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


#if 0

char *
ngx_conf_unsupported(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    return "unsupported on this platform";
}

#endif


char *
ngx_conf_deprecated(ngx_conf_t *cf, void *post, void *data)
{
    ngx_conf_deprecated_t  *d = post;

    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "the \"%s\" directive is deprecated, "
                       "use the \"%s\" directive instead",
                       d->old_name, d->new_name);

    return NGX_CONF_OK;
}


char *
ngx_conf_check_num_bounds(ngx_conf_t *cf, void *post, void *data)
{
    ngx_conf_num_bounds_t  *bounds = post;
    ngx_int_t  *np = data;

    if (bounds->high == -1) {
        if (*np >= bounds->low) {
            return NGX_CONF_OK;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "value must be equal to or greater than %i",
                           bounds->low);

        return NGX_CONF_ERROR;
    }

    if (*np >= bounds->low && *np <= bounds->high) {
        return NGX_CONF_OK;
    }

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "value must be between %i and %i",
                       bounds->low, bounds->high);

    return NGX_CONF_ERROR;
}
