
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_EVENT_PIPE_H_INCLUDED_
#define _NGX_EVENT_PIPE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


typedef struct ngx_event_pipe_s  ngx_event_pipe_t;

//处理接收自上游的包体的回调函数原型
typedef ngx_int_t (*ngx_event_pipe_input_filter_pt)(ngx_event_pipe_t *p,
                                                    ngx_buf_t *buf);
//向下游发送响应的回调函数原型                                                    
typedef ngx_int_t (*ngx_event_pipe_output_filter_pt)(void *data,
                                                     ngx_chain_t *chain);

//成员初始化及赋值见ngx_http_upstream_send_response中赋值
struct ngx_event_pipe_s { //ngx_http_XXX_handler(ngx_http_fastcgi_handler)中创建
    ngx_connection_t  *upstream;// Nginx与上游服务器之间的连接 p->upstream = u->peer.connection;
    ngx_connection_t  *downstream;//Nginx与下游客户端之间的连接 p->downstream = c
    
    /* 直接接收自上游服务器的缓冲区链表，保存的是未经任何处理的数据。这个链表是逆序的，后接受的响应插在链表头处 */
    /*
       1.如果ngx_event_pipe_read_upstream中ngx_readv_chain返回NGX_AGAIN,则下次再次读的时候，就直接用上次没用的chain
    
       2.还有种情况是在ngx_event_pipe_read_upstream分配buf空间，但是却发现buf中没有读取到实际的网页包体内容(例如收到一个fastcgi END)，因此需要把该buf指向内存
         放入free_raw_bufs链表中，以备在下次读取后端包体的时候直接从上面取,见ngx_http_fastcgi_input_filter->ngx_event_pipe_add_free_buf
       3.还有种情况就是如果读取后端的数据没有塞满一块buf指向的内存，则会暂时添加到free_raw_bufs的头部，见ngx_event_pipe_read_upstream
     */
    ngx_chain_t       *free_raw_bufs; //赋值见ngx_event_pipe_read_upstream
    // 表示接收到的上游响应缓冲区，其数据是经过input_filter处理的
    //ngx_event_pipe_read_upstream读取数据后通过ngx_http_fastcgi_input_filter把读取到的数据加入到p->in链表
    //ngx_http_write_filter把in中的数据拼接到out后面，然后调用writev发送，没有发送出去的数据buf还是会留在out链表中
    ngx_chain_t       *in;//每次读取数据后，调用input_filter对协议格式进行解析，解析完后的数据部分放到in里面形成一个链表。参考ngx_http_fastcgi_input_filter
    /*关于p->in和shadow，in指向一堆chain链表，每个链表指向一块实实在在的fcgi DATA数据，多个这样的php等代码块共享一块大的裸FCGI数据块；
    属于某个大的裸FCGI数据块的最后一个数据节点的last_shadow成员为1，表示我是这个大FCGI数据块的最后一个，并且我的shadow指针指向这个裸FCGI数据块的buf指针
	释放这些大数据块的时候，可以参考ngx_event_pipe_drain_chains进行释放。
    */ // 指向刚收到的一个缓冲区
    ngx_chain_t      **last_in; //执行ngx_event_pipe_t->in的最后一个chain节点  参考ngx_http_fastcgi_input_filter

    //ngx_event_pipe_read_upstream读取数据后通过ngx_http_fastcgi_input_filter把读取到的数据加入到p->in链表
    //ngx_http_write_filter把p->in中的数据拼接到ngx_http_request_t->out后面，然后调用writev发送，没有发送出去的数据buf还是会留在ngx_http_request_t->out链表中
    
    //保存着将要发给客户端的缓冲区链表。在写入临时文件成功时，会把in中的缓冲区添加到out中
    //buf到tempfile的数据会放到out里面，并用新的chain指向。在ngx_event_pipe_write_chain_to_temp_file函数里面设置的。ngx_event_pipe_write_chain_to_temp_file

/*
bufferin方式如果配置xxx_buffers  XXX_buffer_size指定的空间都用完了，则会把缓存中的数据写入临时文件，然后继续读，读到ngx_event_pipe_write_chain_to_temp_file
后写入临时文件，直到read返回NGX_AGAIN,然后在ngx_event_pipe_write_to_downstream->ngx_output_chain->ngx_output_chain_copy_buf中读取临时文件内容
发送到后端，当数据继续到来，通过epoll read继续循环该流程
*/
    ngx_chain_t       *out;  //如果后端数据存入临时文件，会用out指向 参考ngx_event_pipe_write_chain_to_temp_file
    // 等待释放的缓冲区
    ngx_chain_t       *free;
    // 表示上次调用ngx_http_output_filter函数发送响应时没有发送完的缓冲区链表
    //根据out中各个节点指向的buf空余空间的大小是否为0，来决定out中各个节点buf的数据是否已经发送出去，如果out中各个buf已经发送完毕，则移动到
    //free中，如果还有剩余则添加到busy中。见ngx_chain_update_chains

    /*实际上p->busy最终指向的是ngx_http_write_filter中未发送完的r->out中保存的数据，这部分数据始终在r->out的最前面，后面在读到数据后在
    ngx_http_write_filter中会把新来的数据加到r->out后面，也就是未发送的数据在r->out前面新数据在链后面，所以实际write是之前未发送的先发送出去*/
    ngx_chain_t       *busy;  //该变量存在的原因就是要现在换成后端数据的最大长度，参考ngx_event_pipe_write_to_downstream(if (bsize >= (size_t) p->busy_size) {)
    //这个链中的各个buf指向的指针其实和ngx_http_request_t->out中的各个buf指向的指针都指向同样的内存块，就是调用ngx_http_output_filter还没有发送出去的数据


    /*
     * the input filter i.e. that moves HTTP/1.1 chunks
     * from the raw bufs to an incoming chain
     */
     //buffering方式后端响应包体使用ngx_event_pipe_t->input_filter  非buffering方式响应后端包体使用ngx_http_upstream_s->input_filter，在ngx_http_upstream_send_response分叉
    // 处理接收到的、来自上游服务器的数据 //FCGI为ngx_http_fastcgi_input_filter，其他为ngx_event_pipe_copy_input_filter 。用来解析特定格式数据
    //在ngx_event_pipe_read_upstream中执行
    ngx_event_pipe_input_filter_pt    input_filter;//见ngx_http_fastcgi_handler(如见ngx_http_xxx_handler) ngx_http_proxy_copy_filter ngx_http_fastcgi_input_filter
    // 用于input_filter的的参数，一般是ngx_http_request_t的地址
    void                             *input_ctx; //指向对应的客户端连接ngx_http_request_t，见ngx_http_fastcgi_handler ngx_http_proxy_handler

    // 向下游发送响应的函数 注意如果一次调用ngx_http_output_filter没有发送完毕，则剩余的数据会添加到ngx_http_request_t->out中
    ngx_event_pipe_output_filter_pt   output_filter;//ngx_http_output_filter
    // output_filter的参数，指向ngx_http_request_t                      
    void                             *output_ctx; //ngx_http_upstream_send_response中赋值

    // 1：表示当前已读取到上游的响应  也就是有读到后端服务器的包体
    unsigned           read:1; //只要从n = p->upstream->recv_chain()有读到数据，也就是n大于0，则read=1;
    unsigned           cacheable:1; // 1：启用文件缓存 p->cacheable = u->cacheable || u->store;
    unsigned           single_buf:1;  // 1：表示接收上游响应时，一次只能接收一个ngx_buf_t缓冲区
    unsigned           free_bufs:1; // 1：一旦不再接收上游包体，将尽可能地释放缓冲区
    //网页包体已经读完    proxy读取完后端包体参考ngx_http_proxy_copy_filter
    unsigned           upstream_done:1; // 1：表示Nginx与上游交互已经结束 后后端短连接的情况下收到NGX_HTTP_FASTCGI_END_REQUEST标识报文置1
    unsigned           upstream_error:1;// 1：Nginx与上游服务器的连接出现错误
    //p->upstream->recv_chain(p->upstream, chain, limit);返回0的时候置1
    unsigned           upstream_eof:1;//p->upstream->recv_chain(p->upstream, chain, limit);返回0的时候置1
    /* 1：表示暂时阻塞读取上游响应的的流程。此时会先调用ngx_event_pipe_write_to_downstream
    函数发送缓冲区中的数据给下游，从而腾出缓冲区空间，再调用ngx_event_pipe_read_upstream
    函数读取上游信息 */ ////非cachable方式下，指定分配的空间用完会置1
    unsigned           upstream_blocked:1; //非cachable方式下，本端指定的空间(例如fastcgi_buffers  5 3K  )已经用完，因此需要阻塞一下，等发送一部分数据到客户端浏览器后，就可以继续读了
    unsigned           downstream_done:1;  // 1：与下游的交互已结束
    unsigned           downstream_error:1; // 1：与下游的连接出现错误
    //默认0，由//fastcgi_cyclic_temp_file  XXX_cyclic_temp_file设置
    unsigned           cyclic_temp_file:1; // 1：复用临时文件。它是由ngx_http_upstream_conf_t中的同名成员赋值的

    ngx_int_t          allocated;    // 已分配的缓冲区数据
    // 记录了接收上游响应的内存缓冲区大小，bufs.size表示每个内存缓冲区大小，bufs.num表示最多可以有num个缓冲区 
    //以缓存响应的方式转发上游服务器的包体时所使用的内存大小 
    //在ngx_event_pipe_read_upstream中创建空间  默认fastcgi_buffers 8  ngx_pagesize  只针对buffing方式生效
    ngx_bufs_t         bufs;//例如fastcgi_buffers  5 3K   成员初始化及赋值见ngx_http_upstream_send_response中赋值
    // 用于设置、比较缓冲区链表中的ngx_buf_t结构体的tag标志位
    ngx_buf_tag_t      tag; //p->tag = u->output.tag //成员初始化及赋值见ngx_http_upstream_send_response中赋值

    /* busy缓冲区中待发送响应长度的最大值，当到达busy_size时，必须等待busy缓冲区发送了足够的数据，才能继续发送out和in中的内容 */
    //该配置生效地方在ngx_event_pipe_write_to_downstream
    ssize_t            busy_size; // p->busy_size = u->conf->busy_buffers_size; //仅当buffering标志位为1，并且向下游转发响应时生效。

    // 已经接收到来自上游响应包体的长度
    off_t              read_length;
    //初始值-1  flcf->keep_conn //fastcgi_keep_conn  on | off 和后端长连接的时候才会发生赋值
    //如果是chunk的传送发送，则一直为-1  fastcgi包体部分长度赋值在ngx_http_fastcgi_input_filter  
    //proxy包体长度赋值在ngx_http_proxy_input_filter_init  读取部分包体后就ngx_http_proxy_copy_filter减去读取的这部分，表示还需要读多少才能读完
    /* fastcgi情况下，后端短连接的情况下收到NGX_HTTP_FASTCGI_END_REQUEST标识报文置upstream_done为1，表示后端数据读取完毕，因此对fastcgi来说length一直未-1
        如果为proxy情况下，读取部分包体后就ngx_http_proxy_copy_filter中p->length减去读取的这部分，表示还需要读多少才能读完
      */
    off_t              length; //表示还需要读取多少包体才表示整个网页包体读完  可以参考ngx_http_fastcgi_input_filter

    /*
在buffering标志位为1时，如果上游速度快于下游速度，将有可能把来自上游的响应存储到临时文件中，而max_temp_file_size指定了临时文件的
最大长度。实际上，它将限制ngx_event_pipe_t结构体中的temp_file     fastcgi_max_temp_file_size配置
*/
    // 表示临时文件的最大长度 如果上游速度快于下游速度，将有可能把来自上游的响应存储到临时文件中，而max_temp_file_size指定了临时文件的最大长度。
    off_t              max_temp_file_size; //p->max_temp_file_size = u->conf->max_temp_file_size;
    // 表示一次写入文件时数据的最大长度 //fastcgi_temp_file_write_size配置表示将缓冲区中的响应写入临时文件时一次写入字符流的最大长度
    ssize_t            temp_file_write_size; //p->temp_file_write_size = u->conf->temp_file_write_size;

    // 读取上游响应的超时时间
    ngx_msec_t         read_timeout;
    // 向下游发送响应的超时时间
    ngx_msec_t         send_timeout;
    // 向下游发送响应时，TCP连接中设置的send_lowat“水位” 内核缓冲区数据只有达到该值才能从缓冲区发送出去
    ssize_t            send_lowat;

    ngx_pool_t        *pool;
    ngx_log_t         *log;

    // 表示在接收上游服务器响应头部阶段，已经读取到响应包体
    ngx_chain_t       *preread_bufs; //ngx_http_upstream_send_response中创建空间和赋值
    // 表示在接收上游服务器响应头部阶段，已经读取到响应包体长度
    size_t             preread_size;
    
    // 用于缓存文件 if(u->cacheable == 1) ngx_http_upstream_send_response中创建空间和赋值
    //指向的是为获取后端头部行的时候分配的第一个缓冲区的头部行部分，buf大小由xxx_buffer_size(fastcgi_buffer_size proxy_buffer_size memcached_buffer_size)指定
  /*
    这里面只存储了头部行buffer中头部行的内容部分，因为后面写临时文件的时候，需要把后端头部行也写进来，由于前面读取头部行后指针已经指向了数据部分
    因此需要临时用buf_to_file->start指向头部行部分开始，pos指向数据部分开始，也就是头部行部分结尾
    ngx_event_pipe_write_chain_to_temp_file写入临时文件后，buf_to_file会被置为NULL
  */
    ngx_buf_t         *buf_to_file; //后端头部行部分写入临时文件后在ngx_event_pipe_write_chain_to_temp_file中会把buf_to_file置为NULL

    size_t             limit_rate;////默认值0 fastcgi_limit_rate 或者proxy memcached等进行限速配置  限制的是与客户端浏览器的速度，不是与后端的速度
    time_t             start_sec;

    // 存放上游响应的临时文件  默认值ngx_http_fastcgi_temp_path
/*
默认情况下p->temp_file->path = u->conf->temp_path; 也就是由ngx_http_fastcgi_temp_path指定路径，但是如果是缓存方式(p->cacheable=1)并且配置
proxy_cache_path(fastcgi_cache_path) /a/b的时候带有use_temp_path=off(表示不使用ngx_http_fastcgi_temp_path配置的path)，
则p->temp_file->path = r->cache->file_cache->temp_path; 也就是临时文件/a/b/temp。use_temp_path=off表示不使用ngx_http_fastcgi_temp_path
配置的路径，而使用指定的临时路径/a/b/temp   见ngx_http_upstream_send_response  
ngx_event_pipe_write_chain_to_temp_file->ngx_write_chain_to_temp_file中创建并写入临时文件
*/    
    /* 当前fastcgi_buffers 和fastcgi_buffer_size配置的空间都已经用完了，则需要把数据写道临时文件中去，参考ngx_event_pipe_read_upstream */
    //写临时文件和max_temp_file_size  temp_file_write_size也有关系
/*
如果配置xxx_buffers  XXX_buffer_size指定的空间都用完了，则会把缓存中的数据写入临时文件，然后继续读，读到ngx_event_pipe_write_chain_to_temp_file
后写入临时文件，直到read返回NGX_AGAIN,然后在ngx_event_pipe_write_to_downstream->ngx_output_chain->ngx_output_chain_copy_buf中读取临时文件内容
发送到后端，当数据继续到来，通过epoll read继续循环该流程
*/ //temp_file默认在文件内容发送到客户端后，会删除文件，见ngx_create_temp_file->ngx_pool_delete_file
//从ngx_http_file_cache_update可以看出，后端数据先写到临时文件后，在写入xxx_cache_path中，见ngx_http_file_cache_update

    /*ngx_http_upstream_init_request->ngx_http_upstream_cache 客户端获取缓存 后端应答回来数据后在ngx_http_upstream_send_response->ngx_http_file_cache_create
    中创建临时文件，然后在ngx_event_pipe_write_chain_to_temp_file把读取的后端数据写入临时文件，最后在
    ngx_http_upstream_send_response->ngx_http_upstream_process_request->ngx_http_file_cache_update中把临时文件内容rename(相当于mv)到proxy_cache_path指定
    的cache目录下面
    */
    /*后端数据读取完毕，并且全部写入临时文件后才会执行rename过程，为什么需要临时文件的原因是:例如之前的缓存过期了，现在有个请求正在从后端
    获取数据写入临时文件，如果是直接写入缓存文件，则在获取后端数据过程中，如果在来一个客户端请求，如果允许proxy_cache_use_stale updating，则
    后面的请求可以直接获取之前老旧的过期缓存，从而可以避免冲突(前面的请求写文件，后面的请求获取文件内容) 
    */
    ngx_temp_file_t   *temp_file;  //tempfile创建见ngx_create_temp_file  最终会通过ngx_create_hashed_filename把path和level=N:N组织到一起


    // 已使用的ngx_buf_t缓冲区数目/* STUB */ 
    int     num; //当前读取到了第几块内存
};


ngx_int_t ngx_event_pipe(ngx_event_pipe_t *p, ngx_int_t do_write);
ngx_int_t ngx_event_pipe_copy_input_filter(ngx_event_pipe_t *p, ngx_buf_t *buf);
ngx_int_t ngx_event_pipe_add_free_buf(ngx_event_pipe_t *p, ngx_buf_t *b);


#endif /* _NGX_EVENT_PIPE_H_INCLUDED_ */
