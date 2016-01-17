/*

--with-aio_module 使用AIO方式处理事件驱动
注意：这里的aio module只能与FreeBSD操作系统上的kqueue事件处理机制合作，Linux上无法使用
默认情况下是不安装aio module的

（2）默认即编译进入Nginx的HTTP模块

表1-10列出了默认就会编译进Nginx的核心HTTP模块，以及如何把这些HTTP模块从产品中去除。

表1-10　configure中默认编译到Nginx中的HTTP模块参数
默认安装的HTTP模块                        意　义
--without-http_charset_module 不安装http charset module。这个模块可以将服务器发出的HTTP响应重编码
--without-http_gzip_module 不安装http gzip module。在服务器发出的HTTP响应包中，这个模块可以按照配置文件指定的content-type对特定大小的HTTP响应包体执行gzip压缩
--without-http_ssi_module 不安装http ssi module。该模块可以在向用户返回的HTTP响应包体中加入特定的内容，如HTML文件中固定的页头和页尾
--without-http_userid_module 不安装http userid module。这个模块可以通过HTTP请求头部信息里的一些字段认证用户信息，以确定请求是否合法
--without-http_access_module 不安装http access module。这个模块可以根据IP地址限制能够访问服务器的客户端
--without-http_auth_basic_module 不安装http auth basic module。这个模块可以提供最简单的用户名/密码认证
--without-http_autoindex_module 不安装http autoindex module。该模块提供简单的目录浏览功能
--without-http_geo_module 不安装http geo module。这个模块可以定义一些变量，这些变量的值将与客户端IP地址关联，这样Nginx针对不同的地区的客户端（根据IP地址判断）返回不一样的结果，例如不同地区显示不同语言的网页
--without-http_map_module 不安装http map module。这个模块可以建立一个key/value映射表，不同的key得到相应的value，这样可以针对不同的URL做特殊处理。例如，返回302重定向响应时，可以期望URL不同时返回的Location字段也不一样
--without-http_split_clients_module 不安装http split client module。该模块会根据客户端的信息，例如IP地址、header头、cookie等，来区分处理
--without-http_referer_module 不安装http referer module。该模块可以根据请求中的referer字段来拒绝请求
--without-http_rewrite_module 不安装http rewrite module。该模块提供HTTP请求在Nginx服务内部的重定向功能，依赖PCRE库
--without-http_proxy_module 不安装http proxy module。该模块提供基本的HTTP反向代理功能
--without-http_fastcgi_module 不安装http fastcgi module。该模块提供FastCGI功能
--without-http_uwsgi_module 不安装http uwsgi module。该模块提供uWSGI功能
--without-http_scgi_module 不安装http scgi module。该模块提供SCGI功能
--without-http_memcached_module 不安装http memcached module。该模块可以使得Nginx直接由上游的memcached服务读取数据，并简单地适配成HTTP响应返回给客户端
--without-http_limit_zone_module 不安装http limit zone module。该模块针对某个IP地址限制并发连接数。例如，使Nginx对一个IP地址仅允许一个连接
--without-http_limit_req_module 不安装http limit req module。该模块针对某个IP地址限制并发请求数
--without-http_empty_gif_module 不安装http empty gif module。该模块可以使得Nginx在收到无效请求时，立刻返回内存中的1×1像素的GIF图片。这种好处在于，对于明显的无效请求不会去试图浪费服务器资源
--without-http_browser_module 不安装http browser module。该模块会根据HTTP请求中的user-agent字段（该字段通常由浏览器填写）来识别浏览器
--without-http_upstream_ip_hash_module 不安装http upstream ip hash module。该模块提供当Nginx与后端server建立连接时，会根据IP做散列运算来决定与后端哪台server通信，这样可以实现负载均衡

（3）默认不会编译进入Nginx的HTTP模块

表1-11列出了默认不会编译至Nginx中的HTTP模块以及把它们加入产品中的方法。

表1-11　configure中默认不会编译到Nginx中的HTTP模块参数
可选的HTTP 模块 意　义
--with-http_ssl_module 安装http ssl module。该模块使Nginx支持SSL协议，提供HTTPS服务。
注意：该模块的安装依赖于OpenSSL开源软件，即首先应确保已经在之前的参数中配置了OpenSSL
--with-http_realip_module 安装http realip module。该模块可以从客户端请求里的header信息（如X-Real-IP或者X-Forwarded-For）中获取真正的客户端IP地址
--with-http_addition_module 安装http addtion module。该模块可以在返回客户端的HTTP包体头部或者尾部增加内容
--with-http_xslt_module 安装http xslt module。这个模块可以使XML格式的数据在发给客户端前加入XSL渲染
注意：这个模块依赖于libxml2和libxslt库，安装它前首先确保上述两个软件已经安装
--with-http_image_filter_module 安装http image_filter module。这个模块将符合配置的图片实时压缩为指定大小（width*height）的缩略图再发送给用户，目前支持JPEG、PNG、GIF格式。
注意：这个模块依赖于开源的libgd库，在安装前确保操作系统已经安装了libgd
--with-http_geoip_module 安装http geoip module。该模块可以依据MaxMind GeoIP的IP地址数据库对客户端的IP地址得到实际的地理位置信息
注意：该库依赖于MaxMind GeoIP的库文件，可访问http://geolite.maxmind.com/download/geoip/database/GeoLiteCity.dat.gz获取
--with-http_sub_module 安装http sub module。该模块可以在Nginx返回客户端的HTTP响应包中将指定的字符串替换为自己需要的字符串
例如，在HTML的返回中，将</head>替换为</head><script language="javascript" src="$script"></script>
--with-http_dav_module 安装http dav module。这个模块可以让Nginx支持Webdav标准，如支持Webdav协议中的PUT、DELETE、COPY、MOVE、MKCOL等请求
--with-http_flv_module 安装http flv module。这个模块可以在向客户端返回响应时，对FLV格式的视频文件在header头做一些处理，使得客户端可以观看、拖动FLV视频
--with-http_mp4_module 安装http mp4 module。该模块使客户端可以观看、拖动MP4视频
--with-http_gzip_static_module 安装http gzip static module。如果采用gzip模块把一些文档进行gzip格式压缩后再返回给客户端，那么对同一个文件每次都会重新压缩，这是比较消耗服务器CPU资源的。gzip static模块可以在做gzip压缩前，先查看相同位置是否有已经做过gzip压缩的.gz文件，如果有，就直接返回。这样就可以预先在服务器上做好文档的压缩，给CPU减负
--with-http_random_index_module 安装http random index module。该模块在客户端访问某个目录时，随机返回该目录下的任意文件
--with-http_secure_link_module 安装http secure link module。该模块提供一种验证请求是否有效的机制。例如，它会验证URL中需要加入的token参数是否属于特定客户端发来的，以及检查时间戳是否过期
--with-http_degradation_module 安装http degradation module。该模块针对一些特殊的系统调用（如sbrk）做一些优化，如直接返回HTTP响应码为204或者444。目前不支持Linux系统
--with-http_stub_status_module 安装http stub status module。该模块可以让运行中的Nginx提供性能统计页面，获取相关的并发连接、请求的信息（14.2.1节中简单介绍了该模块的原理）
--with-google_perftools_module 安装google perftools module。该模块提供Google的性能测试工具

（4）邮件代理服务器相关的mail模块

表1-12列出了把邮件模块编译到产品中的参数。

表1-12　configure提供的邮件模块参数
可选的mail 模块 意　义
--with-mail 安装邮件服务器反向代理模块，使Nginx可以反向代理IMAP、POP3、SMTP等协议。该模块默认不安装
--with-mail_ssl_module 安装mail ssl module。该模块可以使IMAP、POP3、SMTP等协议基于SSL/TLS协议之上使用。该模块默认不安装并依赖于OpenSSL库
--without-mail_pop3_module 不安装mail pop3 module。在使用--with-mail参数后，pop3 module是默认安装的，以使Nginx支持POP3协议
--without-mail_imap_module 不安装mail imap module。在使用--with-mail参数后，imap module是默认安装的，以使Nginx支持IMAP
--without-mail_smtp_module 不安装mail smtp module。在使用--with-mail参数后，smtp module是默认安装的，以使Nginx支持SMTP

*/
