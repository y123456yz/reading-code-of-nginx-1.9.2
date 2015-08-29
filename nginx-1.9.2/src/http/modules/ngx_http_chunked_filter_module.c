
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_chain_t         *free;
    ngx_chain_t         *busy;
} ngx_http_chunked_filter_ctx_t;


static ngx_int_t ngx_http_chunked_filter_init(ngx_conf_t *cf);

/*
格式:

十六进制ea5表明这个暑假块有3749字节
          这个块为3749字节，块数结束后\r\n表明这个块已经结束               这个块为3752字节，块数结束后\r\n表明这个块已经结束 
                                                                                                                                 0表示最后一个块，最后跟两个\r\n
ea5\r\n........................................................\r\n ea8\r\n..................................................\r\n 0\r\n\r\n

参考:http://blog.csdn.net/zhangboyj/article/details/6236780
*/
static ngx_http_module_t  ngx_http_chunked_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_chunked_filter_init,          /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
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
┃                                    ┃  仅对HTTP包体做处理。5.5.2节详细介绍过该过滤模块。它仅应用于     ┃
┃ngx_http_postpone_filter_module     ┃subrequest产生的子请求。它使得多个子请求同时向客户端发送响应时    ┃
┃                                    ┃能够有序，所谓的“有序”是揩按照构造子请求的顺序发送响应            ┃
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



格式:

十六进制ea5表明这个暑假块有3749字节
          这个块为3749字节，块数结束后\r\n表明这个块已经结束               这个块为3752字节，块数结束后\r\n表明这个块已经结束 
                                                                                                                                 0表示最后一个块，最后跟两个\r\n
ea5\r\n........................................................\r\n ea8\r\n..................................................\r\n 0\r\n\r\n

参考:http://blog.csdn.net/zhangboyj/article/details/6236780

*/ngx_module_t  ngx_http_chunked_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_chunked_filter_module_ctx,   /* module context */
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


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;

//是否按照chunked编码方式组包发送包体
static ngx_int_t
ngx_http_chunked_header_filter(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t       *clcf;
    ngx_http_chunked_filter_ctx_t  *ctx;

    if (r->headers_out.status == NGX_HTTP_NOT_MODIFIED
        || r->headers_out.status == NGX_HTTP_NO_CONTENT
        || r->headers_out.status < NGX_HTTP_OK
        || r != r->main
        || (r->method & NGX_HTTP_HEAD))
    {
        return ngx_http_next_header_filter(r); //如果没有包体，则就不存在包体是否安装chunked编码方式发送组包解包问题了
    }

    if (r->headers_out.content_length_n == -1) { //表示后端发送过来的头部行中不带有Content-length:xxx 这种情况1.1版本HTTP直接设置chunked为1
        if (r->http_version < NGX_HTTP_VERSION_11) {
            r->keepalive = 0;

        } else {
            clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

            if (clcf->chunked_transfer_encoding) {
                r->chunked = 1;//头部行中不带有Content-length:xxx 这种情况1.1版本HTTP直接设置chunked为1

                ctx = ngx_pcalloc(r->pool,
                                  sizeof(ngx_http_chunked_filter_ctx_t));
                if (ctx == NULL) {
                    return NGX_ERROR;
                }

                ngx_http_set_ctx(r, ctx, ngx_http_chunked_filter_module);

            } else {
                r->keepalive = 0;
            }
        }
    }

    return ngx_http_next_header_filter(r);
}


static ngx_int_t
ngx_http_chunked_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    u_char                         *chunk;
    off_t                           size;
    ngx_int_t                       rc;
    ngx_buf_t                      *b;
    ngx_chain_t                    *out, *cl, *tl, **ll;
    ngx_http_chunked_filter_ctx_t  *ctx;

    if (in == NULL || !r->chunked || r->header_only) { //
        return ngx_http_next_body_filter(r, in);
    }

/*
格式:

十六进制ea5表明这个暑假块有3749字节
            这个块为3749字节，块数结束后\r\n表明这个块已经结束     这个块为3752字节，块数结束后\r\n表明这个块已经结束 
                                                                                                                                 0表示最后一个块，最后跟两个\r\n
.....ea5\r\n........................................................\r\n ea8\r\n..................................................\r\n 0\r\n\r\n

*/
    //以下一般是从ngx_http_send_special走到这里
    ctx = ngx_http_get_module_ctx(r, ngx_http_chunked_filter_module);

    out = NULL;
    ll = &out;

    size = 0;
    cl = in;

    for ( ;; ) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http chunk: %d", ngx_buf_size(cl->buf));

        size += ngx_buf_size(cl->buf);

        if (cl->buf->flush
            || cl->buf->sync
            || ngx_buf_in_memory(cl->buf)
            || cl->buf->in_file) //创建新的chain来指向in中的每一个buf，然后添加到out链中
        {
            tl = ngx_alloc_chain_link(r->pool);
            if (tl == NULL) {
                return NGX_ERROR;
            }

            tl->buf = cl->buf;
            *ll = tl;
            ll = &tl->next;
        }

        if (cl->next == NULL) {
            break;
        }

        cl = cl->next;
    }

    if (size) {//size为要发送的所有in链中的所有buf指向的数据的长度和，大于0所有有数据
        //chain链in中有多少个成员chain这里就为其分配多少个存储起数据长度的16字节十六进制字符串
        tl = ngx_chain_get_free_buf(r->pool, &ctx->free);
        if (tl == NULL) {
            return NGX_ERROR;
        }

        b = tl->buf;
        chunk = b->start;

        if (chunk == NULL) { //16字节的空间用来存储一个chunk编码方式包体的16进制字符串格式
            /* the "0000000000000000" is 64-bit hexadecimal string */

            chunk = ngx_palloc(r->pool, sizeof("0000000000000000" CRLF) - 1);
            if (chunk == NULL) {
                return NGX_ERROR;
            }

            b->start = chunk;
            b->end = chunk + sizeof("0000000000000000" CRLF) - 1;
        }

        b->tag = (ngx_buf_tag_t) &ngx_http_chunked_filter_module;
        b->memory = 0;
        b->temporary = 1;
        b->pos = chunk;
        //size按照16进制字符串存放在chunk中，标识整个in链中的所有buf的数据之和
        b->last = ngx_sprintf(chunk, "%xO" CRLF, size); 
        
        tl->next = out; //这个in的chain tl添加到out链最前面
        out = tl;
    }

    int lastbuf = cl->buf->last_buf;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "yang test ..........xxxxxxxx ################## lstbuf:%d", lastbuf);
    if (cl->buf->last_buf) { //如果是in链中的最后一个chain成员，则以一个标识0\r\n\r\n标识该in数据包体结束
        tl = ngx_chain_get_free_buf(r->pool, &ctx->free);
        if (tl == NULL) {
            return NGX_ERROR;
        }

        b = tl->buf;

        b->tag = (ngx_buf_tag_t) &ngx_http_chunked_filter_module;
        b->temporary = 0;
        b->memory = 1;
        b->last_buf = 1;
        b->pos = (u_char *) CRLF "0" CRLF CRLF;
        b->last = b->pos + 7;

        cl->buf->last_buf = 0;

        *ll = tl;

        if (size == 0) {
            b->pos += 2;
        }

    } else if (size > 0) {
        tl = ngx_chain_get_free_buf(r->pool, &ctx->free);
        if (tl == NULL) {
            return NGX_ERROR;
        }

        b = tl->buf;

        b->tag = (ngx_buf_tag_t) &ngx_http_chunked_filter_module;
        b->temporary = 0;
        b->memory = 1;
        b->pos = (u_char *) CRLF;
        b->last = b->pos + 2;

        *ll = tl;

    } else {
        *ll = NULL;
    }

    rc = ngx_http_next_body_filter(r, out);

    ngx_chain_update_chains(r->pool, &ctx->free, &ctx->busy, &out,
                            (ngx_buf_tag_t) &ngx_http_chunked_filter_module);

    return rc;
}


static ngx_int_t
ngx_http_chunked_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_chunked_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_chunked_body_filter;

    return NGX_OK;
}
