/*
 HTTP 头部解释

1. Accept：告诉WEB服务器自己接受什么介质类型，* /* 表示任何类型，type/ * 表示该类型下的所有子类型，type/sub-type。
 
2. Accept-Charset：   浏览器申明自己接收的字符集
   Accept-Encoding：  浏览器申明自己接收的编码方法，通常指定压缩方法，是否支持压缩，支持什么压缩方法  （gzip，deflate）
   Accept-Language：：浏览器申明自己接收的语言语言跟字符集的区别：中文是语言，中文有多种字符集，比如big5，gb2312，gbk等等。
 
3. Accept-Ranges：WEB服务器表明自己是否接受获取其某个实体的一部分（比如文件的一部分）的请求。bytes：表示接受，none：表示不接受。
 
4. Age：当代理服务器用自己缓存的实体去响应请求时，用该头部表明该实体从产生到现在经过多长时间了。
 
5. Authorization：当客户端接收到来自WEB服务器的 WWW-Authenticate 响应时，该头部来回应自己的身份验证信息给WEB服务器。
 
6. Cache-Control：请求：no-cache（不要缓存的实体，要求现在从WEB服务器去取）
                                     max-age：（只接受 Age 值小于 max-age 值，并且没有过期的对象）
                                     max-stale：（可以接受过去的对象，但是过期时间必须小于 
                                                        max-stale 值）
                                     min-fresh：（接受其新鲜生命期大于其当前 Age 跟 min-fresh 值之和的
                                                        缓存对象）
                            响应：public(可以用 Cached 内容回应任何用户)
                                      private（只能用缓存内容回应先前请求该内容的那个用户）
                                      no-cache（可以缓存，但是只有在跟WEB服务器验证了其有效后，才能返回给客户端）
                                      max-age：（本响应包含的对象的过期时间）
                                      ALL:  no-store（不允许缓存）
 
7. Connection：请求：close（告诉WEB服务器或者代理服务器，在完成本次请求的响应
                                                  后，断开连接，不要等待本次连接的后续请求了）。
                                 keepalive（告诉WEB服务器或者代理服务器，在完成本次请求的
                                                         响应后，保持连接，等待本次连接的后续请求）。
                       响应：close（连接已经关闭）。
                                 keepalive（连接保持着，在等待本次连接的后续请求）。
   Keep-Alive：如果浏览器请求保持连接，则该头部表明希望 WEB 服务器保持
                      连接多长时间（秒）。
                      例如：Keep-Alive：300
 
8. Content-Encoding：WEB服务器表明自己使用了什么压缩方法（gzip，deflate）压缩响应中的对象。 
                                 例如：Content-Encoding：gzip                   
   Content-Language：WEB 服务器告诉浏览器自己响应的对象的语言。
   Content-Length：    WEB 服务器告诉浏览器自己响应的对象的长度。
                                例如：Content-Length: 26012
   Content-Range：    WEB 服务器表明该响应包含的部分对象为整个对象的哪个部分。
                                例如：Content-Range: bytes 21010-47021/47022
   Content-Type：      WEB 服务器告诉浏览器自己响应的对象的类型。
                                例如：Content-Type：application/xml
 
9. ETag：就是一个对象（比如URL）的标志值，就一个对象而言，比如一个 html 文件，
              如果被修改了，其 Etag 也会别修改， 所以，ETag 的作用跟 Last-Modified 的
              作用差不多，主要供 WEB 服务器 判断一个对象是否改变了。
              比如前一次请求某个 html 文件时，获得了其 ETag，当这次又请求这个文件时， 
              浏览器就会把先前获得的 ETag 值发送给  WEB 服务器，然后 WEB 服务器
              会把这个 ETag 跟该文件的当前 ETag 进行对比，然后就知道这个文件
              有没有改变了。
         
10. Expired：WEB服务器表明该实体将在什么时候过期，对于过期了的对象，只有在
                    跟WEB服务器验证了其有效性后，才能用来响应客户请求。
                    是 HTTP/1.0 的头部。
                    例如：Expires：Sat, 23 May 2009 10:02:12 GMT
 
11. Host：客户端指定自己想访问的WEB服务器的域名/IP 地址和端口号。
                例如：Host：rss.sina.com.cn
 
12. If-Match：如果对象的 ETag 没有改变，其实也就意味著对象没有改变，才执行请求的动作。
    If-None-Match：如果对象的 ETag 改变了，其实也就意味著对象也改变了，才执行请求的动作。
 
13. If-Modified-Since：如果请求的对象在该头部指定的时间之后修改了，才执行请求
                                 的动作（比如返回对象），否则返回代码304，告诉浏览器该对象
                                 没有修改。
                                 例如：If-Modified-Since：Thu, 10 Apr 2008 09:14:42 GMT
    If-Unmodified-Since：如果请求的对象在该头部指定的时间之后没修改过，才执行
                                   请求的动作（比如返回对象）。
 
14. If-Range：浏览器告诉 WEB 服务器，如果我请求的对象没有改变，就把我缺少的部分
                       给我，如果对象改变了，就把整个对象给我。 浏览器通过发送请求对象的 
                       ETag 或者 自己所知道的最后修改时间给 WEB 服务器，让其判断对象是否
                       改变了。
                       总是跟 Range 头部一起使用。
 
15. Last-Modified：WEB 服务器认为对象的最后修改时间，比如文件的最后修改时间，
                                 动态页面的最后产生时间等等。
                                 例如：Last-Modified：Tue, 06 May 2008 02:42:43 GMT
 
16. Location：WEB 服务器告诉浏览器，试图访问的对象已经被移到别的位置了，
                        到该头部指定的位置去取。
                        例如：Location：
                                  http://i0.sinaimg.cn/dy/deco/2008/0528/sinahome_0803_ws_005_text_0.gif
 
17. Pramga：主要使用 Pramga: no-cache，相当于 Cache-Control： no-cache。
                      例如：Pragma：no-cache
 
18. Proxy-Authenticate： 代理服务器响应浏览器，要求其提供代理身份验证信息。
      Proxy-Authorization：浏览器响应代理服务器的身份验证请求，提供自己的身份信息。
 
19. Range：浏览器（比如 Flashget 多线程下载时）告诉 WEB 服务器自己想取对象的哪部分。
                    例如：Range: bytes=1173546-
 
20. Referer：浏览器向 WEB 服务器表明自己是从哪个 网页/URL 获得/点击 当前请求中的网址/URL。
                   例如：Referer：http://www.sina.com/
 
21. Server: WEB 服务器表明自己是什么软件及版本等信息。
                  例如：Server：Apache/2.0.61 (Unix)
 
22. User-Agent: 浏览器表明自己的身份（是哪种浏览器）。
                        例如：User-Agent：Mozilla/5.0 (Windows; U; Windows NT 5.1; zh-CN;   
                                  rv:1.8.1.14) Gecko/20080404 Firefox/2.0.0.14
 
23. Transfer-Encoding: WEB 服务器表明自己对本响应消息体（不是消息体里面的对象）
                                  作了怎样的编码，比如是否分块（chunked）。
                                  例如：Transfer-Encoding: chunked
 
24. Vary: WEB服务器用该头部的内容告诉 Cache 服务器，在什么条件下才能用本响应
                 所返回的对象响应后续的请求。
                 假如源WEB服务器在接到第一个请求消息时，其响应消息的头部为：
                 Content-Encoding: gzip; Vary: Content-Encoding  那么 Cache 服务器会分析后续
                 请求消息的头部，检查其 Accept-Encoding，是否跟先前响应的 Vary 头部值
                 一致，即是否使用相同的内容编码方法，这样就可以防止 Cache 服务器用自己
                 Cache 里面压缩后的实体响应给不具备解压能力的浏览器。
                 例如：Vary：Accept-Encoding
 
25. Via： 列出从客户端到 OCS 或者相反方向的响应经过了哪些代理服务器，他们用
                什么协议（和版本）发送的请求。
                当客户端请求到达第一个代理服务器时，该服务器会在自己发出的请求里面
                添加 Via 头部，并填上自己的相关信息，当下一个代理服务器 收到第一个代理
                服务器的请求时，会在自己发出的请求里面复制前一个代理服务器的请求的Via 
                头部，并把自己的相关信息加到后面， 以此类推，当 OCS 收到最后一个代理服
                务器的请求时，检查 Via 头部，就知道该请求所经过的路由。
                例如：Via：1.0 236-81.D07071953.sina.com.cn:80 (squid/2.6.STABLE13)
Cookie: 坦白的说，一个cookie就是存储在用户主机浏览器中的一小段文本文件。Cookies是纯文本形式，它们不包含任何可执行代码。一个
        Web页面或服务器告之浏览器来将这些信息存储并且基于一系列规则在之后的每个请求中都将该信息返回至服务器。Web服务器之后可以利用
        这些信息来标识用户。多数需要登录的站点通常会在你的认证信息通过后来设置一个cookie，之后只要这个cookie存在并且合法，你就可以
        自由的浏览这个站点的所有部分。再次，cookie只是包含了数据，就其本身而言并不有害。

Date: 头域表示消息发送的时间，时间的描述格式由rfc822定义。例如，Date:Mon,31Dec200104:25:57GMT。Date描述的时间表示世界标准时，换算成本地时间，需要知道用户所在的时区。








http://kb.cnblogs.com/page/92320/

HTTP Header 详解发布时间: 2011-02-27 21:07  阅读: 71790 次  推荐: 23   原文链接   [收藏]   
HTTP（HyperTextTransferProtocol）即超文本传输协议，目前网页传输的的通用协议。HTTP协议采用了请求/响应模型，浏览器或其他客户端发出请求，服务器给与响应。就整个网络资源传输而言，包括message-header和message-body两部分。首先传递message- header，即http header消息 。http header 消息通常被分为4个部分：general  header, request header, response header, entity header。但是这种分法就理解而言，感觉界限不太明确。根据维基百科对http header内容的组织形式，大体分为Request和Response两部分。

Requests部分
Header 解释 示例 
Accept 指定客户端能够接收的内容类型 Accept: text/plain, text/html 
Accept-Charset 浏览器可以接受的字符编码集。 Accept-Charset: iso-8859-5 
Accept-Encoding 指定浏览器可以支持的web服务器返回内容压缩编码类型。 Accept-Encoding: compress, gzip 
Accept-Language 浏览器可接受的语言 Accept-Language: en,zh 
Accept-Ranges 可以请求网页实体的一个或者多个子范围字段 Accept-Ranges: bytes 
Authorization HTTP授权的授权证书 Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ== 
Cache-Control 指定请求和响应遵循的缓存机制 Cache-Control: no-cache 
Connection 表示是否需要持久连接。（HTTP 1.1默认进行持久连接） Connection: close 
Cookie HTTP请求发送时，会把保存在该请求域名下的所有cookie值一起发送给web服务器。 Cookie: $Version=1; Skin=new; 
Content-Length 请求的内容长度 Content-Length: 348 
Content-Type 请求的与实体对应的MIME信息 Content-Type: application/x-www-form-urlencoded 
Date 请求发送的日期和时间 Date: Tue, 15 Nov 2010 08:12:31 GMT 
Expect 请求的特定的服务器行为 Expect: 100-continue 
From 发出请求的用户的Email From: user@email.com 
Host 指定请求的服务器的域名和端口号 Host: www.zcmhi.com 
If-Match 只有请求内容与实体相匹配才有效 If-Match: “737060cd8c284d8af7ad3082f209582d” 
If-Modified-Since 如果请求的部分在指定时间之后被修改则请求成功，未被修改则返回304代码 If-Modified-Since: Sat, 29 Oct 2010 19:43:31 GMT 
If-None-Match 如果内容未改变返回304代码，参数为服务器先前发送的Etag，与服务器回应的Etag比较判断是否改变 If-None-Match: “737060cd8c284d8af7ad3082f209582d” 
If-Range 如果实体未改变，服务器发送客户端丢失的部分，否则发送整个实体。参数也为Etag If-Range: “737060cd8c284d8af7ad3082f209582d” 
If-Unmodified-Since 只在实体在指定时间之后未被修改才请求成功 If-Unmodified-Since: Sat, 29 Oct 2010 19:43:31 GMT 
Max-Forwards 限制信息通过代理和网关传送的时间 Max-Forwards: 10 
Pragma 用来包含实现特定的指令 Pragma: no-cache 
Proxy-Authorization 连接到代理的授权证书 Proxy-Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ== 
Range 只请求实体的一部分，指定范围 Range: bytes=500-999 
Referer 先前网页的地址，当前请求网页紧随其后,即来路 Referer: http://www.zcmhi.com/archives/71.html 
TE 客户端愿意接受的传输编码，并通知服务器接受接受尾加头信息 TE: trailers,deflate;q=0.5 
Upgrade 向服务器指定某种传输协议以便服务器进行转换（如果支持） Upgrade: HTTP/2.0, SHTTP/1.3, IRC/6.9, RTA/x11 
User-Agent User-Agent的内容包含发出请求的用户信息 User-Agent: Mozilla/5.0 (Linux; X11) 
Via 通知中间网关或代理服务器地址，通信协议 Via: 1.0 fred, 1.1 nowhere.com (Apache/1.1) 
Warning 关于消息实体的警告信息 Warn: 199 Miscellaneous warning 

Responses 部分 

Header 解释 示例 
Accept-Ranges 表明服务器是否支持指定范围请求及哪种类型的分段请求 Accept-Ranges: bytes 
Age 从原始服务器到代理缓存形成的估算时间（以秒计，非负） Age: 12 
Allow 对某网络资源的有效的请求行为，不允许则返回405 Allow: GET, HEAD 
Cache-Control 告诉所有的缓存机制是否可以缓存及哪种类型 Cache-Control: no-cache 
Content-Encoding web服务器支持的返回内容压缩编码类型。 Content-Encoding: gzip 
Content-Language 响应体的语言 Content-Language: en,zh 
Content-Length 响应体的长度 Content-Length: 348 
Content-Location 请求资源可替代的备用的另一地址 Content-Location: /index.htm 
Content-MD5 返回资源的MD5校验值 Content-MD5: Q2hlY2sgSW50ZWdyaXR5IQ== 
Content-Range 在整个返回体中本部分的字节位置 Content-Range: bytes 21010-47021/47022 
Content-Type 返回内容的MIME类型 Content-Type: text/html; charset=utf-8 
Date 原始服务器消息发出的时间 Date: Tue, 15 Nov 2010 08:12:31 GMT 
ETag 请求变量的实体标签的当前值 ETag: “737060cd8c284d8af7ad3082f209582d” 
Expires 响应过期的日期和时间 Expires: Thu, 01 Dec 2010 16:00:00 GMT 
Last-Modified 请求资源的最后修改时间 Last-Modified: Tue, 15 Nov 2010 12:45:26 GMT 
Location 用来重定向接收方到非请求URL的位置来完成请求或标识新的资源 Location: http://www.zcmhi.com/archives/94.html 
Pragma 包括实现特定的指令，它可应用到响应链上的任何接收方 Pragma: no-cache 
Proxy-Authenticate 它指出认证方案和可应用到代理的该URL上的参数 Proxy-Authenticate: Basic 
refresh 应用于重定向或一个新的资源被创造，在5秒之后重定向（由网景提出，被大部分浏览器支持）  
Refresh: 5; url=http://www.zcmhi.com/archives/94.html 
Retry-After 如果实体暂时不可取，通知客户端在指定时间之后再次尝试 Retry-After: 120 
Server web服务器软件名称 Server: Apache/1.3.27 (Unix) (Red-Hat/Linux) 
Set-Cookie 设置Http Cookie Set-Cookie: UserID=JohnDoe; Max-Age=3600; Version=1 
Trailer 指出头域在分块传输编码的尾部存在 Trailer: Max-Forwards 
Transfer-Encoding 文件传输编码 Transfer-Encoding:chunked 
Vary 告诉下游代理是使用缓存响应还是从原始服务器请求 Vary: * 
Via 告知代理客户端响应是通过哪里发送的 Via: 1.0 fred, 1.1 nowhere.com (Apache/1.1) 
Warning 警告实体可能存在的问题 Warning: 199 Miscellaneous warning 
WWW-Authenticate 表明客户端请求实体应该使用的授权方案 WWW-Authenticate: Basic 


所有linux版本:http://mirrors.163.com/

淘宝中文地址:http://tengine.taobao.org/nginx_docs/cn/docs/http/ngx_http_core_module.html

#define NGX_CONFIGURE " --add-module=./src/mytest_config --add-module=./src/my_test_module --add-module=./src/mytest_subrequest --add-module=./src/mytest_upstream --add-module=./src/ngx_http_myfilter_module --with-debug --with-file-aio --add-module=./src/sendfile_test --with-threads --add-module=/var/yyz/nginx-1.9.2/src/echo-nginx-module-master --add-module=./src/nginx-requestkey-module-master/






#define NGX_CONFIGURE " --add-module=./src/mytest_config --add-module=./src/my_test_module --add-module=./src/mytest_subrequest --add-module=./src/mytest_upstream --add-module=./src/ngx_http_myfilter_module --with-debug --with-file-aio --add-module=./src/sendfile_test --with-threads"
#define NGX_CONFIGURE " --add-module=./src/mytest_config --add-module=./src/my_test_module --add-module=./src/mytest_subrequest --add-module=./src/mytest_upstream --add-module=./src/ngx_http_myfilter_module --with-debug --with-file-aio --add-module=./src/sendfile_test --with-threads --add-module=/var/yyz/nginx-1.9.2/src/echo-nginx-module-master"

I/O测试工具，iostat  网络接口流量测试工具ifstat






改造点及可疑问题:
        和后端服务器通过检查套接字连接状态来判断是否失效，如果失效则连接下一个服务器。这种存在缺陷，例如如果后端服务器直接拔掉网线或者后端服务器断
    电了，则检测套接字是判断不出来的，协议栈需要长时间过后才能判断出，如果关闭掉协议栈的keepalive可能永远检测不出，这是个严重bug。
*/

