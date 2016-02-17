
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static ngx_uint_t ngx_http_test_if_unmodified(ngx_http_request_t *r);
static ngx_uint_t ngx_http_test_if_modified(ngx_http_request_t *r);
static ngx_uint_t ngx_http_test_if_match(ngx_http_request_t *r,
    ngx_table_elt_t *header, ngx_uint_t weak);
static ngx_int_t ngx_http_not_modified_filter_init(ngx_conf_t *cf);


static ngx_http_module_t  ngx_http_not_modified_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_not_modified_filter_init,     /* postconfiguration */

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

If-Modified-Since:
     从字面上看, 就是说: 如果从某个时间点算起, 如果文件被修改了....
            1. 如果真的被修改: 那么就开始传输, 服务器返回:200 OK
            2. 如果没有被修改: 那么就无需传输, 服务器返回: 403 Not Modified.
     用途:
             客户端尝试下载最新版本的文件. 比如网页刷新, 加载大图的时候.
             很明显: 如果从图片下载以后都没有再被修改, 当然就没必要重新下载了!

If-Unmodified-Since:
     从字面上看, 意思是: 如果从某个时间点算起, 文件没有被修改.....
            1. 如果没有被修改: 则开始`继续'传送文件: 服务器返回: 200 OK
            2. 如果文件被修改: 则不传输, 服务器返回: 412 Precondition failed (预处理错误)
     用途:

            断点续传(一般会指定Range参数). 要想断点续传, 那么文件就一定不能被修改, 否则就不是同一个文件了, 断续还有啥意义?

    总之一句话: 一个是修改了才下载, 一个是没修改才下载.
*/
ngx_module_t  ngx_http_not_modified_filter_module = { 
//该模块就是结合If-None-Match和ETag , If-Modified-Since和Last-Modified请求行来判断是返回304 no modified还是直接把文件内容传送给客户端
    NGX_MODULE_V1,
    &ngx_http_not_modified_filter_module_ctx, /* module context */
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

/*
{If-None-Match和ETag , If-Modified-Since和Last-Modified
    If-Modified-Since（浏览器） = Last-Modified（服务器）
    作用：浏览器端第一次访问获得服务器的Last-Modified，第2次访问把浏览器端缓存页面的最后修改时间发送到服务器去，服务器会把这
    个时间与服务器上实际文件的最后修改时间进行对比。如果时间一致，那么返回304，客户端就直接使用本地缓存文件。如果时间不一致，就
    会返回200和新的文件内容。客户端接到之后，会丢弃旧文件，把新文件缓存起来，并显示在浏览器中.


    If-None-Match（浏览器） = ETag（服务器）
    作用: If-None-Match和ETag一起工作，工作原理是在HTTP Response中添加ETag信息。 当用户再次请求该资源时，将在HTTP Request 中加入If-None-Match
    信息(ETag的值)。如果服务器验证资源的ETag没有改变（该资源没有更新），将返回一个304状态告诉客户端使用本地缓存文件。否则将返回200状态和新的资源和Etag. 
}


{
    ETags和If-None-Match是一种常用的判断资源是否改变的方法。类似于Last-Modified和HTTP-If-Modified-Since。但是有所不同的是Last-Modified和HTTP-If-Modified-Since只判断资源的最后修改时间，而ETags和If-None-Match可以是资源任何的任何属性。
    ETags和If-None-Match的工作原理是在HTTPResponse中添加ETags信息。当客户端再次请求该资源时，将在HTTPRequest中加入If-None-Match信息（ETags的值）。如果服务器验证资源的ETags没有改变（该资源没有改变），将返回一个304状态；否则，服务器将返回200状态，并返回该资源和新的ETags。
}


{  
http响应Last-Modified和ETag

　　基础知识
1) 什么是”Last-Modified”?
　　    在浏览器第一次请求某一个URL时，服务器端的返回状态会是200，内容是你请求的资源，同时有一个Last-Modified的属性标记此文件在服务期端
    最后被修改的时间，格式类似这样：Last-Modified: Fri, 12 May 2006 18:53:33 GMT
　　客户端第二次请求此URL时，根据 HTTP 协议的规定，浏览器会向服务器传送 If-Modified-Since 报头，询问该时间之后文件是否有被修改过：
　　If-Modified-Since: Fri, 12 May 2006 18:53:33 GMT
　　如果服务器端的资源没有变化，则自动返回 HTTP 304 （Not Changed.）状态码，内容为空，这样就节省了传输数据量。当服务器端代码发生改
    变或者重启服务器时，则重新发出资源，返回和第一次请求时类似。从而保证不向客户端重复发出资源，也保证当服务器有变化时，客户端能够得到最新的资源。
2) 什么是”Etag”?
　　HTTP 协议规格说明定义ETag为“被请求变量的实体值” （参见 —— 章节 14.19）。 另一种说法是，ETag是一个可以与Web资源关联的记号（token）。典型的Web资源可以一个Web页，但也可能是JSON或XML文档。服务器单独负责判断记号是什么及其含义，并在HTTP响应头中将其传送到客户端，以下是服务器端返回的格式：
　　ETag: "50b1c1d4f775c61:df3"
　　客户端的查询更新格式是这样的：
　　If-None-Match: W/"50b1c1d4f775c61:df3"
　　如果ETag没改变，则返回状态304然后不返回，这也和Last-Modified一样。本人测试Etag主要在断点下载时比较有用。
　　
Last-Modified和Etags如何帮助提高性能?
　　聪明的开发者会把Last-Modified 和ETags请求的http报头一起使用，这样可利用客户端（例如浏览器）的缓存。因为服务器首先产生 
Last-Modified/Etag标记，服务器可在稍后使用它来判断页面是否已经被修改。本质上，客户端通过将该记号传回服务器要求服务器验证其（客户端）缓存。过程如下:
1.客户端请求一个页面（A）。
2.服务器返回页面A，并在给A加上一个Last-Modified/ETag。
3.客户端展现该页面，并将页面连同Last-Modified/ETag一起缓存。
4.客户再次请求页面A，并将上次请求时服务器返回的Last-Modified/ETag一起传递给服务器。
5.服务器检查该Last-Modified或ETag，并判断出该页面自上次客户端请求之后还未被修改，直接返回响应304和一个空的响应体。
}
*/
static ngx_int_t
ngx_http_not_modified_header_filter(ngx_http_request_t *r)
{
    if (r->headers_out.status != NGX_HTTP_OK  //只有返回类型为OK的才进行304 not modified判断
        || r != r->main
        || r->disable_not_modified)
    {
        return ngx_http_next_header_filter(r);
    }
    

    /*
If-Unmodified-Since: 从字面上看, 意思是: 如果从某个时间点算起, 文件没有被修改.....
    1. 如果没有被修改: 则开始`继续'传送文件: 服务器返回: 200 OK
    2. 如果文件被修改: 则不传输, 服务器返回: 412 Precondition failed (预处理错误)
用途:断点续传(一般会指定Range参数). 要想断点续传, 那么文件就一定不能被修改, 否则就不是同一个文件了
*/
    if (r->headers_in.if_unmodified_since
        && !ngx_http_test_if_unmodified(r)) 
    /*
    如果从某个时间点算起, 文件被修改了，客户端请求行中带有if_unmodified_since，表示如果文件如果从某个时间段起没有被修改，
    则继续200 OK传送给客户端包体。但是现在文件却被修改了，因此返回错误
    */
    {
        return ngx_http_filter_finalize_request(r, NULL,
                                                NGX_HTTP_PRECONDITION_FAILED);
    }

    if (r->headers_in.if_match
        && !ngx_http_test_if_match(r, r->headers_in.if_match, 0))
    {
        return ngx_http_filter_finalize_request(r, NULL,
                                                NGX_HTTP_PRECONDITION_FAILED);
    }

    if (r->headers_in.if_modified_since || r->headers_in.if_none_match) {

        if (r->headers_in.if_modified_since
            && ngx_http_test_if_modified(r)) //文件有发生了修改
        {
            return ngx_http_next_header_filter(r);
        }

        if (r->headers_in.if_none_match
            && !ngx_http_test_if_match(r, r->headers_in.if_none_match, 1)) //etag不匹配，说明文件也发生了修改
        {
            return ngx_http_next_header_filter(r);
        }

        /* not modified */
        //回送304表示文件没有修改
        r->headers_out.status = NGX_HTTP_NOT_MODIFIED;
        r->headers_out.status_line.len = 0;
        r->headers_out.content_type.len = 0;
        ngx_http_clear_content_length(r);
        ngx_http_clear_accept_ranges(r);

        if (r->headers_out.content_encoding) {
            r->headers_out.content_encoding->hash = 0;
            r->headers_out.content_encoding = NULL;
        }

        return ngx_http_next_header_filter(r);
    }

    return ngx_http_next_header_filter(r);
}

//如果从某个时间点算起, 文件没有被修改，返回1
static ngx_uint_t
ngx_http_test_if_unmodified(ngx_http_request_t *r)
{
    time_t  iums;

    if (r->headers_out.last_modified_time == (time_t) -1) { //如果输出头部行不带last_modified_time
        return 0;
    }

    iums = ngx_parse_http_time(r->headers_in.if_unmodified_since->value.data,
                               r->headers_in.if_unmodified_since->value.len);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                 "http iums:%T lm:%T", iums, r->headers_out.last_modified_time);

    if (iums >= r->headers_out.last_modified_time) {
        return 1;
    }

    return 0;
}

/* 某个时间点起文件被修改 */
static ngx_uint_t
ngx_http_test_if_modified(ngx_http_request_t *r)
{
    time_t                     ims;
    ngx_http_core_loc_conf_t  *clcf;

    if (r->headers_out.last_modified_time == (time_t) -1) {
        return 1;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (clcf->if_modified_since == NGX_HTTP_IMS_OFF) { //如果禁用了if_modified_since，直接返回1
        return 1;
    }

    ims = ngx_parse_http_time(r->headers_in.if_modified_since->value.data,
                              r->headers_in.if_modified_since->value.len);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http ims:%T lm:%T", ims, r->headers_out.last_modified_time);

    if (ims == r->headers_out.last_modified_time) { 
//浏览器通过if_modified_since传送过来的时间就是nginx服务器端之前通过last_modified_time发送给浏览器的时间,说明文件在客户端两次获取过程中还没有修改
        return 0;
    }

    if (clcf->if_modified_since == NGX_HTTP_IMS_EXACT //配置为精确匹配，就是是ims和r->headers_out.last_modified_time相等，但实际上不相等
        || ims < r->headers_out.last_modified_time)
    {
        return 1;
    }

    return 0;
}

//比较etag是否发生了变化   r->headers_out.etag为nginx服务器端最新的etag，header->value为浏览器请求头部行中携带的etag信息
static ngx_uint_t
ngx_http_test_if_match(ngx_http_request_t *r, ngx_table_elt_t *header,
    ngx_uint_t weak) //ngx_http_test_if_match验证客户端过来的etag, ngx_http_set_etag设置最新etag
{
    u_char     *start, *end, ch;
    ngx_str_t   etag, *list;

    list = &header->value;

    if (list->len == 1 && list->data[0] == '*') {
        return 1;
    }

    //如果客户端在第一次请求文件和第二次请求文件这段时间，文件修改了，则etag会发生变化
    if (r->headers_out.etag == NULL) {
        return 0;
    }

    etag = r->headers_out.etag->value;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http im:\"%V\" etag:%V", list, &etag);

    if (weak
        && etag.len > 2
        && etag.data[0] == 'W'
        && etag.data[1] == '/') //见ngx_http_weak_etag
    {
        etag.len -= 2;
        etag.data += 2;
    } //去掉前面的W/字符，

    start = list->data;
    end = list->data + list->len;

    while (start < end) {

        if (weak
            && end - start > 2
            && start[0] == 'W'
            && start[1] == '/')
        {
            start += 2; //去掉前面的W/字符，
        }

        if (etag.len > (size_t) (end - start)) {
            return 0;
        }

        if (ngx_strncmp(start, etag.data, etag.len) != 0) {
            goto skip;
        }

        start += etag.len;

        while (start < end) {
            ch = *start;

            if (ch == ' ' || ch == '\t') {
                start++;
                continue;
            }

            break;
        }

        if (start == end || *start == ',') {
            return 1;
        }

    skip:

        while (start < end && *start != ',') { start++; }
        while (start < end) {
            ch = *start;

            if (ch == ' ' || ch == '\t' || ch == ',') {
                start++;
                continue;
            }

            break;
        }
    }

    return 0;
}


static ngx_int_t
ngx_http_not_modified_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_not_modified_header_filter;

    return NGX_OK;
}
