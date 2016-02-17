
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static ngx_int_t ngx_http_write_filter_init(ngx_conf_t *cf);


static ngx_http_module_t  ngx_http_write_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_write_filter_init,            /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL,                                  /* merge location configuration */
};

/*
表6-1  默认即编译进Nginx的HTTP过滤模块
┏━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃默认即编译进Nginx的HTTP过滤模块     ┃    功能                                                          ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP头部做处理。在返回200成功时，根据请求中If-              ┃
┃                                    ┃Modified-Since或者If-Unmodified-Since头部取得浏览器缓存文件的时   ┃
┃ngx_http_not_modified_filter_module ┃                                                                  ┃
┃                                    ┃间，再分析返回用户文件的最后修改时间，以此决定是否直接发送304     ┃
┃                                    ┃ Not Modified响应给用户                                           ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  处理请求中的Range信息，根据Range中的要求返回文件的一部分给      ┃
┃ngx_http_range_body_filter_module   ┃                                                                  ┃
┃                                    ┃用户                                                              ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP包体做处理。将用户发送的ngx_chain_t结构的HTTP包         ┃
┃                                    ┃体复制到新的ngx_chain_t结构中（都是各种指针的复制，不包括实际     ┃
┃ngx_http_copy_filter_module         ┃                                                                  ┃
┃                                    ┃HTTP响应内容），后续的HTTP过滤模块处埋的ngx_chain_t类型的成       ┃
┃                                    ┃员都是ngx_http_copy_filter_module模块处理后的变量                 ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP头部做处理。允许通过修改nginx.conf配置文件，在返回      ┃
┃ngx_http_headers_filter_module      ┃                                                                  ┃
┃                                    ┃给用户的响应中添加任意的HTTP头部                                  ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP头部做处理。这就是执行configure命令时提到的http_        ┃
┃ngx_http_userid_filter_module       ┃                                                                  ┃
┃                                    ┃userid module模块，它基于cookie提供了简单的认证管理功能           ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  可以将文本类型返回给用户的响应包，按照nginx．conf中的配置重新   ┃
┃ngx_http_charset_filter_module      ┃                                                                  ┃
┃                                    ┃进行编码，再返回给用户                                            ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  支持SSI（Server Side Include，服务器端嵌入）功能，将文件内容包  ┃
┃ngx_http_ssi_filter_module          ┃                                                                  ┃
┃                                    ┃含到网页中并返回给用户                                            ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP包体做处理。                             它仅应用于     ┃
┃ngx_http_postpone_filter_module     ┃subrequest产生的子请求。它使得多个子请求同时向客户端发送响应时    ┃
┃                                    ┃能够有序，所谓的“有序”是揩按照构造子请求的顺序发送响应          ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  对特定的HTTP响应包体（如网页或者文本文件）进行gzip压缩，再      ┃
┃ngx_http_gzip_filter_module         ┃                                                                  ┃
┃                                    ┃把压缩后的内容返回给用户                                          ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_http_range_header_filter_module ┃  支持range协议                                                   ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_http_chunked_filter_module      ┃  支持chunk编码                                                   ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP头部做处理。该过滤模块将会把r->headers out结构体        ┃
┃                                    ┃中的成员序列化为返回给用户的HTTP响应字符流，包括响应行(如         ┃
┃ngx_http_header_filter_module       ┃                                                                  ┃
┃                                    ┃HTTP/I.1 200 0K)和响应头部，并通过调用ngx_http_write filter       ┃
┃                                    ┃ module过滤模块中的过滤方法直接将HTTP包头发送到客户端             ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_http_write_filter_module        ┃  仅对HTTP包体做处理。该模块负责向客户端发送HTTP响应              ┃
┗━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

*/ngx_module_t  ngx_http_write_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_write_filter_module_ctx,     /* module context */
    NULL,                                  /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

/*
ngx_http_header_filter发送头部内容是通过调用ngx_http_write_filter方法来发送响应头部的。事实上，这个方法是包体过滤模块链表中的
最后一个模块ngx_http_write_filter_module的处理方法，当HTTP模块调用ngx_http_output_filter方法发送包体时，最终也是通过该方法发送响应的
。当一次无法发送全部的缓冲区内容时，ngx_http_write_filter方法是会返回NGX_AGAIN的（同时将未发送完成的缓冲区放到请求的out成员
中），也就是说，发送响应头部的ngx_http_header_filter方法会返回NGX_AGAIN。如果不需要再发送包体，那么这时就需要调用
ngx_http_finalize_request方法来结束请求，其中第2个参数务必要传递NGX_AGAIN，这样HTTP框架才会继续将可写事件注册到epoll，并持
续地把请求的out成员中缓冲区里的HTTP响应发送完毕才会结束请求。
*/ //发送数据的时候调用ngx_http_write_filter写数据，如果返回NGX_AGAIN,则以后的写数据触发通过在ngx_http_writer添加epoll write事件来触发
ngx_int_t
ngx_http_write_filter(ngx_http_request_t *r, ngx_chain_t *in)
//ngx_http_write_filter把in中的数据拼接到out后面，然后调用writev发送，没有发送完的数据最后留在out中
{//将r->out里面的数据，和参数里面的数据一并以writev的机制发送给客户端，如果没有发送完所有的，则将剩下的放在r->out

//调用ngx_http_write_filter写数据，如果返回NGX_AGAIN,则以后的写数据触发通过在ngx_http_set_write_handler->ngx_http_writer添加epoll write事件来触发

    off_t                      size, sent, nsent, limit;
    ngx_uint_t                 last, flush, sync;
    ngx_msec_t                 delay;
    ngx_chain_t               *cl, *ln, **ll, *chain;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;
    //如果error为1表示请求出错，那么直接返回NGX_ERROR
    if (c->error) {
        return NGX_ERROR;
    }

    size = 0;
    flush = 0;
    sync = 0;
    last = 0;
    ll = &r->out;

    /* find the size, the flush point and the last link of the saved chain */
//找到请求的ngx_http_request_t结构体中存放的等待发送的缓冲区链表out，遍历这个ngx_chain_t类型的缓冲区链表，
//计算出out缓冲区共占用了多大的字节数，这个out链表通常都保存着待发送的响应。例如，在调用ngx_http_send header方法时，
//如果HTTP响应头部过大导致无法一次性发送完，那么剩余的响应头部就会在out链表中。
    for (cl = r->out; cl; cl = cl->next) { //out里面是上次发送没有发送完的内容
        ll = &cl->next;

        ngx_log_debug7(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "write old buf t:%d f:%d %p, pos %p, size: %z "
                       "file: %O, size: %O",
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);

#if 1
        if (ngx_buf_size(cl->buf) == 0 && !ngx_buf_special(cl->buf)) {
            ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                          "zero size buf in writer "
                          "t:%d r:%d f:%d %p %p-%p %p %O-%O",
                          cl->buf->temporary,
                          cl->buf->recycled,
                          cl->buf->in_file,
                          cl->buf->start,
                          cl->buf->pos,
                          cl->buf->last,
                          cl->buf->file,
                          cl->buf->file_pos,
                          cl->buf->file_last);

            ngx_debug_point();
            return NGX_ERROR;
        }
#endif

        size += ngx_buf_size(cl->buf);

        if (cl->buf->flush || cl->buf->recycled) {
            flush = 1;
        }

        if (cl->buf->sync) {
            sync = 1;
        }

        if (cl->buf->last_buf) {
            last = 1;
        }
    }

    /* add the new chain to the existent one */
    //遍历这个ngx_chain_t类型的缓存链表in，将in中的缓冲区加入到out链表的末尾，并计算out缓冲区共占用多大的字节数
    for (ln = in; ln; ln = ln->next) { //in表示这次新加进来需要发送的内容
        cl = ngx_alloc_chain_link(r->pool);
        if (cl == NULL) {
            return NGX_ERROR;
        }

        cl->buf = ln->buf;
        *ll = cl;
        ll = &cl->next;

//注意从后端接收的数据到缓存文件中后，在filter模块中，有可能是新的buf数据指针了，因为ngx_http_copy_filter->ngx_output_chain中会重新分配内存读取缓存文件内容
        ngx_log_debug7(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "write new buf temporary:%d buf-in-file:%d, buf->start:%p, buf->pos:%p, buf_size: %z "
                       "file_pos: %O, in_file_size: %O",
                       cl->buf->temporary, cl->buf->in_file,
                       cl->buf->start, cl->buf->pos,
                       cl->buf->last - cl->buf->pos,
                       cl->buf->file_pos,
                       cl->buf->file_last - cl->buf->file_pos);

#if 1
        if (ngx_buf_size(cl->buf) == 0 && !ngx_buf_special(cl->buf)) {
            ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                          "zero size buf in writer "
                          "t:%d r:%d f:%d %p %p-%p %p %O-%O",
                          cl->buf->temporary,
                          cl->buf->recycled,
                          cl->buf->in_file,
                          cl->buf->start,
                          cl->buf->pos,
                          cl->buf->last,
                          cl->buf->file,
                          cl->buf->file_pos,
                          cl->buf->file_last);

            ngx_debug_point();
            return NGX_ERROR;
        }
#endif

        size += ngx_buf_size(cl->buf);

        if (cl->buf->flush || cl->buf->recycled) {
            flush = 1;
        }

        if (cl->buf->sync) {
            sync = 1;
        }

        if (cl->buf->last_buf) {
            last = 1;
        }
    }

    *ll = NULL;

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http write filter: last:%d flush:%d size:%O", last, flush, size);

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    /*
     * avoid the output if there are no last buf, no flush point,
     * there are the incoming bufs and the size of all bufs
     * is smaller than "postpone_output" directive
     */
    /*
2025/02/08 00:56:18 [debug] 13330#0: *1 HTTP/1.1 200 OK
Server: nginx/1.9.2
Date: Fri, 07 Feb 2025 16:56:18 GMT
Content-Type: text/plain
Content-Length: 43
Connection: keep-alive

2025/02/08 00:56:18 [debug] 13330#0: *1 write new buf t:1 f:0 080E68BC, pos 080E68BC, size: 147 file: 0, size: 0
2025/02/08 00:56:18 [debug] 13330#0: *1 http write filter: l:0 f:0 s:147  
2025/02/08 00:56:18 [debug] 13330#0: *1 http output filter "/mytest?" //这前面的都是头部打印，注意到实际上并没有writev，而是和下面的包体一起writev的
2025/02/08 00:56:18 [debug] 13330#0: *1 http copy filter: "/mytest?"  //ngx_http_copy_filter函数开始
2025/02/08 00:56:18 [debug] 13330#0: *1 http postpone filter "/mytest?" 080E6A40
2025/02/08 00:56:18 [debug] 13330#0: *1 write old buf t:1 f:0 080E68BC, pos 080E68BC, size: 147 file: 0, size: 0
2025/02/08 00:56:18 [debug] 13330#0: *1 write new buf t:1 f:0 080C82EC, pos 080C82EC, size: 18 file: 0, size: 0
2025/02/08 00:56:18 [debug] 13330#0: *1 write new buf t:1 f:0 080E69A0, pos 080E69A0, size: 25 file: 0, size: 0
2025/02/08 00:56:18 [debug] 13330#0: *1 http write filter: l:1 f:0 s:190
2025/02/08 00:56:18 [debug] 13330#0: *1 http write filter limit 0
2025/02/08 00:56:18 [debug] 13330#0: *1 writev: 190 of 190
 2025/02/08 00:56:18 [debug] 13330#0: *1 http write filter 00000000
 2025/02/08 00:56:18 [debug] 13330#0: *1 http copy filter: 0 "/mytest?" //ngx_http_copy_filter函数结尾，也就是中间的filter都是在ngx_http_copy_filter调用执行的
 2025/02/08 00:56:18 [debug] 13330#0: *1 http finalize request: 0, "/mytest?" a:1, c:1
 2025/02/08 00:56:18 [debug] 13330#0: *1 set http keepalive handler
 2025/02/08 00:56:18 [debug] 13330#0: *1 http close request

     */
    
    /*
    3个标志位同时为0（即待发送的out链表中没有一个缓冲区表示响应已经结束或需要立刻发送出去），而且本次要发送的缓冲区in虽然不为空，
    但以上两步骤中计算出的待发送响应的大小又小于配置文件中的postpone_output参数，那么说明当前的缓冲区是不完整的且没有必要立刻发送
     */ //例如如果有头部，又有包体，则一般最尾部的头部filter函数ngx_http_header_filter->ngx_http_write_filter到这里的时候一般头部字段
     //过少，这里直接返回NGX_OK，这样就可以让头部和包体在最尾部的包体filter函数ngx_http_write_filter->ngx_http_write_filter和包体在一个报文中发送出去
    if (!last && !flush && in && size < (off_t) clcf->postpone_output) {
        ngx_log_debugall(c->log, 0, "send size:%O < min postpone_output:%O, do not send", size, (off_t) clcf->postpone_output);
        //如果last flush都等于0，并且in不为NULL，并且输出链中的数据小于postpone_output，则直接返回，表示等数据跟多(达到postpone_output)，或者指定last flush则输出
        return NGX_OK;
    }

/*
 首先检查连接上写事件的标志位delayed，如果delayed为1，则表示这一次的epoll调度中请求仍需要减速，是不可以发送响应的，delayed为1
 指明了响应需要延迟发送；如果delayed为0，表示本次不需要减速，那么再检查ngx_http_request_t结构体中的limit_rate
 发送响应的速率，如果limit_rate为0，表示这个请求不需要限制发送速度；如果limit rate大干0，则说明发送响应的速度不能超过limit_rate指定的速度。
 */
    if (c->write->delayed) { //在后面的限速中置1
//将客户端对应的buffered标志位放上NGX_HTTP_WRITE_BUFFERED宏，同时返回NGX AGAIN，这是在告诉HTTP框架out缓冲区中还有响应等待发送。
        c->buffered |= NGX_HTTP_WRITE_BUFFERED;
        return NGX_AGAIN; 
        //调用ngx_http_write_filter写数据，如果返回NGX_AGAIN,则以后的写数据触发通过在ngx_http_set_write_handler->ngx_http_writer添加epoll write事件来触发
    }

    if (size == 0
        && !(c->buffered & NGX_LOWLEVEL_BUFFERED)
        && !(last && c->need_last_buf))
    {
        if (last || flush || sync) {
            for (cl = r->out; cl; /* void */) {
                ln = cl;
                cl = cl->next;
                ngx_free_chain(r->pool, ln);
            }

            r->out = NULL;
            c->buffered &= ~NGX_HTTP_WRITE_BUFFERED;

            return NGX_OK;
        }

        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "the http output chain is empty");

        ngx_debug_point();

        return NGX_ERROR;
    }

    if (r->limit_rate) {
        if (r->limit_rate_after == 0) {
            r->limit_rate_after = clcf->limit_rate_after;
        }

    /*
    ngx_time()方法，它取出了当前时间，而start sec表示开始接收到客户端请求内容的时间，c->sent表示这条连接上已经发送了的HTTP响
    应长度，这样计算出的变量limit就表示本次可以发送的字节数了。如果limit小于或等于0，它表示这个连接上的发送响应速度已经超出
    了limit_rate配置项的限制，所以本次不可以继续发送，跳到第7步执行；如果limit大于0，表示本次可以发送limit字节的响应，开始发送响应。
      */
        limit = (off_t) r->limit_rate * (ngx_time() - r->start_sec + 1)
                - (c->sent - r->limit_rate_after); 
                //实际上这发送过程中就比实际的limit_rate多发送limit_rate_after，也就是先发送limit_rate_after后才开始计算是否超速了

        if (limit <= 0) {
            c->write->delayed = 1; //由于达到发送响应的速度上限，这时将连接上写事件的delayed标志位置为1。

            /* limit是已经超发的字节数，它是0或者负数。这个定时器的超时时间是超发字节数按照limit_rate速率算出需要等待的时间
            再加上l毫秒，它可以使Nginx定时器准确地在允许发送响应时激活请求。 */
            delay = (ngx_msec_t) (- limit * 1000 / r->limit_rate + 1);
            //添加定时器的时候为什么没有ngx_handle_write_event? 因为一旦添加wrie epoll事件，那么只要内核数据发送出去就会触发write事件，
            //从而执行ngx_http_writer，这个过程是很快的，这样就起不到限速的作用了
            ngx_add_timer(c->write, delay, NGX_FUNC_LINE); //handle应该是ngx_http_request_handler

            c->buffered |= NGX_HTTP_WRITE_BUFFERED;

            return NGX_AGAIN;
            //调用ngx_http_write_filter写数据，如果返回NGX_AGAIN,则以后的写数据触发通过在ngx_http_set_write_handler->ngx_http_writer添加epoll write事件来触发
        }

    /*
        本步将把响应发送给客户端。然而，缓冲区中的响应可能非常大，那么这一次应该发送多少字节呢？这要根据前面计算出的limit变量，
    前面取得的配置项sendfile_max_chunk来计算，同时要根据遍历缓冲区计算出的待发送字节数来决定，这3个值中的最小值即作为本
    次发送的响应长度。 实际最后通过ngx_writev_chain发送数据的时候，还会限制一次
    */
        if (clcf->sendfile_max_chunk
            && (off_t) clcf->sendfile_max_chunk < limit)
        {
            limit = clcf->sendfile_max_chunk;
        }

    } else {
        limit = clcf->sendfile_max_chunk;
    }

    sent = c->sent;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http write filter limit %O", limit);
    //注意这里发送的时候可能会出现超速，所以在发送成功后会重新计算是否超速，从而决定是否需要启动定时器延迟发送
    //返回值应该是out中还没有发送出去的数据存放在chain中
    chain = c->send_chain(c, r->out, limit); //这里面会重新计算实际已经发送出去了多少字节

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http write filter %p", chain);

    if (chain == NGX_CHAIN_ERROR) {
        c->error = 1;
        return NGX_ERROR;
    }

    //发送响应后再次检查请求的limit_rate标志位，如果limit_rate为0，则表示不需要限速，如果limit_rate大干0，则表示需要限速。 
    if (r->limit_rate) {

        nsent = c->sent;//在上面执行c->send_chain后实际发送的字节数会加到c->send上面

        if (r->limit_rate_after) {

            sent -= r->limit_rate_after;
            if (sent < 0) {
                sent = 0;
            }

            nsent -= r->limit_rate_after;
            if (nsent < 0) {
                nsent = 0;
            }
        }

        //从新计算是否超速了，如果超速了则启动延迟定时器延迟发送
        delay = (ngx_msec_t) ((nsent - sent) * 1000 / r->limit_rate);

        /*
          前面调用c->send_chain发送的响应速度还是过快了，已经超发了一些响应，从新计算出至少要经过多少毫秒后才可以继续发送，
          调用ngx_add_timer方法将写事件按照上面计算出的毫秒作为超时时间添加到定时器中。同时，把写事件的delayed标志位置为1。
          */
        if (delay > 0) {
            limit = 0;
            c->write->delayed = 1;
            //添加定时器的时候为什么没有ngx_handle_write_event? 因为一旦添加wrie epoll事件，那么只要内核数据发送出去就会触发write事件，
            //从而执行ngx_http_writer，这个过程是很快的，这样就起不到限速的作用了
            ngx_add_timer(c->write, delay, NGX_FUNC_LINE);
        }
    }

    if (limit
        && c->write->ready
        && c->sent - sent >= limit - (off_t) (2 * ngx_pagesize)) //如果超速的字节数超过了
    {
        c->write->delayed = 1;
        //添加定时器的时候为什么没有ngx_handle_write_event? 因为一旦添加wrie epoll事件，那么只要内核数据发送出去就会触发write事件，
        //从而执行ngx_http_writer，这个过程是很快的，这样就起不到限速的作用了
        ngx_add_timer(c->write, 1, NGX_FUNC_LINE);
    }

    /*
     重置ngx_http_request_t结构体的out缓冲区，把已经发送成功的缓冲区归还给内存池。如果out链表中还有剩余的没有发送出去的缓冲区，
     则添加到out链表头部；如果已经将out链表中的所有缓冲区都发送给客户端了,则r->out链上为空
     */
    for (cl = r->out; cl && cl != chain; /* void */) { //chain为r->out中还未发送的数据不符
        ln = cl;
        cl = cl->next;
        ngx_free_chain(r->pool, ln);
    }

/*实际上p->busy最终指向的是ngx_http_write_filter中未发送完的r->out中保存的数据，这部分数据始终在r->out的最前面，后面在读到数据后在
ngx_http_write_filter中会把新来的数据加到r->out后面，也就是未发送的数据在r->out前面新数据在链后面，所以实际write是之前未发送的先发送出去*/

    r->out = chain; //把还没有发送完的数据从新添加到out中，实际上in中的相关chain和buf与r->out中的相关chain和buf指向了相同的还为发送出去的数据内存

    if (chain) { //还没有发送完成，需要继续发送
        c->buffered |= NGX_HTTP_WRITE_BUFFERED;
        return NGX_AGAIN; 
        //调用ngx_http_write_filter写数据，如果返回NGX_AGAIN,则以后的写数据触发通过在ngx_http_set_write_handler->ngx_http_writer添加epoll write事件来触发
    }

    c->buffered &= ~NGX_HTTP_WRITE_BUFFERED;
    
    /* 如果其他filter模块buffer了chain并且postponed为NULL，那么返回NGX_AGAIN，需要继续处理buf */  
    if ((c->buffered & NGX_LOWLEVEL_BUFFERED) && r->postponed == NULL) {
        return NGX_AGAIN;
        //调用ngx_http_write_filter写数据，如果返回NGX_AGAIN,则以后的写数据触发通过在ngx_http_set_write_handler->ngx_http_writer添加epoll write事件来触发
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_write_filter_init(ngx_conf_t *cf)
{
    ngx_http_top_body_filter = ngx_http_write_filter;

    return NGX_OK;
}

