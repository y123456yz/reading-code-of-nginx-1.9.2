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

*/

