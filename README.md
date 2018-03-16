# reading-code-of-nginx-1.9.2  
nginx-1.9.2代码理解及详细注释  
 
   
   
说明:  
===================================   
nginx的以下功能模块的相关代码已经阅读，并对其源码及相关数据结构进行了详细备注，主要参考书籍为淘宝陶辉先生
的《深入理解Nginx:模块开发与架构解析》，并对书中没有讲到的相关部分功能进行了扩展，通过边阅读边调试的方法
通读了以下功能模块，并对其进行了详细的注释，同时加了自己的理解在里面，注释比较详细，由于是周末和下班时间阅读，再加上自己
文采限制，代码及数据结构注释都比较白话，注释中肯定会有理解错误的地方。  
  
阅读工具source insight,如果中文乱码，按照source insight configure目录中说明操作  
  
      
阅读过程  
===================================  
截止15.9.19，已经分析并注释完成的主要功能如下:  
	.配置编译过程中相关脚本调用过程详细注释  
	.用户自由模块编译添加过程  
	.nginx使用的进程间通信方式(包括共享内存 原子操作 自旋锁 信号 信号两 文件锁 互斥锁 channel通信)  
	.nginx自定义高级数据结构详解(队列 链表 红黑树 散列表等)  
	.配置文件解析流程  
	.nginx启动、退出过程分析  
	.连接池 内存池分配管理过程  
	.对客户端链接如何实现负载均衡,“惊群”问题避免  
	.主动链接 被动链接过程分析  
	.epoll事件驱动机制，epoll模型详解，包括读写异步非阻塞处理机制及流程。  
	.时间机制及定时器详解.  
	.异步I/O，sendfile代码分析  
	.http框架处理流程及11个阶段处理及调用过程  
	.HTTP请求行、请求头部、包体异步读取解析过程  
	.HTTP异步非阻塞发送HTTP头部和包体过程  
	.HTTP阻塞任务如何拆分。  
	.任务处理完毕执行流程。  
	.资源回收过程  
	 
  
15.10.2  
	nginx变量(内部自定义变量以及配置文件中添加的变量)的定义解析过程分析。    
	脚本引擎代码分析  
	nginx十一个阶段处理重新详细分析。  
	rewrite机制详细分析，以及location{}定位进行进一步分析。  
  
15.11.29  
	把之前的读取客户端包体进行了重新分析，并做了详细解释，配合图形说明  
	上游服务器连接细化分析，细化upstream和fastcgi配合过程细节，  
	output_chain代码详细分析   
	writev功能分析。  
	负载均衡算法(轮询和iphash)分析以及和upstream、fastcgi配合调用整体流程分析  
	fastacgi协议细化，fastcgi格式组包过程分析。   
	writev_chain分析     
	变量中的位用%d %u之类打印，容易引起段错误    
	fastcgi应答头部行解析过程以及赋值给request的headers_out过程  
    新增每行日志打印函数名和行号功能，有利于分析程序执行流程。  
	  
15.12.25  
	buffering方式和非buffering方式处理后端数据的流程详细分析。   
    buffering方式pipe处理流程详细分析  
    keepalive模块代码理解，后端连接缓存原理分析。  
    分析后端服务器失效判断方法,已经再次恢复使用检测方法分析  
    反向代理proxy-module详细分析，proxy_pass相关接收方式格式组包解析分析，proxy模块非阻塞发送接收原理分析  
    chunk编码方式分析，触发按照chunk方式发送包体到后端条件，以及组包过程分析  
    keepalive-module后端服务器连接缓存进一步分析  
    子连接subrequest及其相应的postpone分析  
    多级subrequest如何保证数据按照指定先后顺序发送的客户端浏览器代码分析。  
    临时文件创建管理缓存过程分析，以及相关配置详细分析。  
    指定的缓存不够的情况下，后端数据写入临时文件过程分析  
    proxy_cache原理分析，包括共享内存创建，管理，参数解析等  
    slab原理分析，以及slab管理共享内存过程分析  
	
16.1.2  
	推敲分析缓存后端数据的时候为什么需要临时文件，代码详细中做了详细流程分析  
    loader进程，cache manager进程处理过程分析,响应handler定时指向机制分析  
    缓存文件命中发送过程分析  
    缓存文件stat状态信息红黑树存储及老化过程分析    
    缓存文件红黑树存储过程及老化分析  
    缓存文件loader加载过程分析  
    aio异步文件读取发送分析  
    sendfile流程分析  
    线程池详解分析  
    aio sendfile 线程池分界点详细分析  
	 
16.1.10  
	推敲为什么每次在发送后端数据最后都会调用ngx_http_send_special的原因分析  
    进一步分析aio异步事件触发流程  
    关键点新增打印，利用分析日志。  
    分析在ngx_http_write_filter的时候，明明从后端接收的数据到了缓存文件，并且理论上出去的数据时in file的，单实际上在out的时候却是in mem而非in file  
    sendfile与普通ngx_writev分界点进一步分析  
    缓存情况下的copy filter执行流程和非缓存情况下的copy filter执行流程进一步分析注释。  
    filter后端数据到客户端的时候，是否需要重新开辟内存空间分界点详细分析。  
    分析直接direct I/O的生效场景，以及结合相关资料分析说明时候使用direct I/O  
    direct I/O和异步AIO结合提升效率代码分析及高压测试  
    directio真正生效过程代码分析  
    同时配置sendfile on;  aio on; dirction xxx;的情况下sendfile和aio选择判断流程详细分析  
    线程池thread_pool_module源码详细分析注释   
    aio thread执行流程详细分析  
    进一步分析获取缓存文件头部部分和文件后半部包体部分的发送流程。  
    secure_link权限访问模块原理分析，以及代码分析，并举例测试分析  
    从新走到index和static模块， 配合secure_link测试。  
    添加第三方模块-防盗链模块，了解原理后走读代码分析  
	
16.1.17  
	分析uri中携带的arg_xxx变量的解析形成过程，配合secure link配合使用，起到安全防护作用,对该模块进行详细分析注释  
    aio thread_pool使用流程分析，线程池读取文件过程分析。  
    普通读方式下大文件(10M以上)下载过程流程分析，以及与小文件(小于32768   65535/2)下载过程不同流程比较分析  
    AIO on方式下大文件(10M以上)下载过程流程分析，以及与小文件(小于32768   65535/2)下载过程不同流程比较分析  
    AIO thread_pool方式下大文件(10M 以上)下载过程流程分析，以及与小文件(小于32768)下载过程不同流程比较分析  
    新增小文件(小于65535/2)下载全过程和大文件下载全过程日志，可以直观得到函数调用流程，完善日志  
    结合refer防盗链模块重新把hash走读理解注释一遍，加深理解  
    重新理解前置通配符后置通配符hash存储过程  
    添加lua库和lua-module 把定时器 事件机制函数相关修改添加到lua模块和redis模块  
		
16.2.17  
	分析注释errlog_module模块，研究errlog_module和Ngx_http_core_module中error_log配置的区别，同时分析在这两个模块配置生效前的日志记录过程  
    新增ngx_log_debugall功能，用于自己添加调试代码，以免影响原有代码，减少耦合，便于新增功能代码错误排查。  
    同时配置多条error_log配置信息，每条文件名和等级不一样，也就是不同等级日志输出到不同日志文件的过程分析及注释。  
    ngx_http_log_module详细分析，以及access_log如果是常量路径和变量路径的优化处理过程。  
    NGX_HTTP_LOG_PHASE阶段ngx_http_log_module生效过程及日志记录过程细化分析注释,同时明确ngx_errlog_module和ngx_http_log_module的区别  
    进一步细化分析ngx_http_index_module代码，以及配合rewrite-phase进行内部重定向流程  
    autoindex搭建目录服务器过程源码分析流程  
    结合代码理解If-None-Match和ETag , If-Modified-Since和Last-Modified这几个头部行配合浏览器缓存生效过程,也就是决定是否(304 NOT modified)，  
        并且分析注释ngx_http_not_modified_filter_module实现过程
    Cache-Control expire 头部行代码实现流程，详细分析expires和add_header配置，分析注释ngx_http_headers_filter_module模块  
    从新结合代码对比分析并总结几种负债均衡算法(hash  ip_hash least_conn rr)  
    结合error_pages配置，对ngx_http_special_response_handler进行分析，同时对内部重定向功能和@new_location进行分析  
  
16.3.16  
	进一步分析internal对location{}访问权限控制  
    重新为nginx全局核心模块，标准HTTP模块的遗漏的配置项添加中文注释  
    结合types{}配置，对content_type头部行的形成过程进行详细分析  
    从新分析配置解析过程， 未完  
  
16.4.3  
	重新分析写超时定时器原理，生效方法，  
	结合写超时定时器重新分析后端服务器拔掉网线或者直接断电情况下的后端服务器异常判断方法，参考ngx_http_upstream_send_request  
	 
	
16.7.31   
	添加了新增模块的编译方法  
	由于近期做分布式缓存，需要把nginx的高并发机制移植到twemproxy上，从新对多进程多核绑定、异步网络机制进行了重新梳理  
	   
16.9.31   
	热启动reload、优雅退出quit、热升级代码分析注释  
	subrequest重新分析注释  
	
17.3.15  
    path http2代码到nginx-1.9.2  
	nginx http2数据读取过程分析,ssl tls握手协商流程分析  
	http2初始流程、setting帧处理过程、连接序言、header帧处理流程、HPACK实现方法分析、完善理解HTTP2调用过程的必要打印。    
	
17.3.21  
    HTTP2 header帧处理流程、stream建立过程、伪request结构处理过程分析  
	HTTP2 header帧处理完成后与NGINX phase框架衔接过程分析    
	HTTP2 报文输出过程与nginx框架结合流程分析,DATA帧输出流程分析  
	多sream同时请求，多流交互情况下DATA帧发送过程分析   
    WINDOW_UPDATE帧和流量控制原理结合分析。	
	
17.4.28  
	HTTP2流的优先级、流依赖过程代码分析注释  

    ?????
    NGINX不支持PUSH、做反向代理情况下和后端为什么还是走HTTP1.X协议？  
  	 
  
18.1.26  
	多worker进程reuserport原理重新分析       
	   
改造点及可疑问题:   
===================================     
1. 和后端服务器通过检查套接字连接状态来判断后端服务器是否down机，如果失效则连接下一个服务器。这种存在缺陷，例如如果后端服务器直接拔掉网线或者后端服务器断
电了，则检测套接字是判断不出来的，协议栈需要长时间过后才能判断出，如果关闭掉协议栈的keepalive可能永远检测不出，这时候nginx还是会把客户端请求发往后端服务器，
如果发往后端服务器数据大小很大，可能需要多次write，这时候会由write timeout来判断出后端出现问题。但是如果发往后端数据长度小，则不会添加write定时器，而是通过
写定时器超时来判断，这样不能立刻判断出后端异常，因为读写定时器默认都是60s，参考ngx_http_upstream_send_request，  

2.[限流不准确分析](https://github.com/alibaba/tengine/issues/855)<br />  
	 

http2 quic学习参考:    
===================================     
libquic goquic编译安装，源码分析注释：https://github.com/y123456yz/reading-and-annotate-quic      
nghttp2相关参考：https://github.com/y123456yz/reading-and-annotate-nghttp2  

	 
     
编译方法：
===================================    
步骤1：这里根据需要编译自己的模块  
cd nginx-1.9.2  
 ./configure --add-module=./src/mytest_config --add-module=./src/my_test_module --add-module=./src/mytest_subrequest --add-module=./src/mytest_upstream --add-module=./src/ngx_http_myfilter_module --with-debug --with-file-aio --add-module=./src/sendfile_test --with-threads  --add-module=./src/nginx-requestkey-module-master/ --with-http_secure_link_module --add-module=./src/redis2-nginx-module-master/ 
 
步骤2：make && make install  
