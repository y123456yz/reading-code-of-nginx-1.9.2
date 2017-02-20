
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_REQUEST_H_INCLUDED_
#define _NGX_HTTP_REQUEST_H_INCLUDED_


#define NGX_HTTP_MAX_URI_CHANGES           10
#define NGX_HTTP_MAX_SUBREQUESTS           200

/* must be 2^n */
#define NGX_HTTP_LC_HEADER_LEN             32


#define NGX_HTTP_DISCARD_BUFFER_SIZE       4096
#define NGX_HTTP_LINGERING_BUFFER_SIZE     4096

/*
HTTP 1.0及HTTP0.9规定浏览器与服务器只保持短暂的连接，浏览器的每次请求都需要与服务器建立一个TCP连接，服务器完成请求处理后立即断开TCP连接，
服务器不跟踪每个客户也不记录过去的请求。
*/
//如果和后端的tcp连接使用HTTP1.1以下版本，则会置connection_close为1，见ngx_http_proxy_process_status_line
#define NGX_HTTP_VERSION_9                 9    //HTTP 0/9
#define NGX_HTTP_VERSION_10                1000 //HTTP 1/0
#define NGX_HTTP_VERSION_11                1001 //HTTP 1/1

/*
HTTP PUT方法和POST方法的区别
    这两个方法看起来都是讲一个资源附加到服务器端的请求，但其实是不一样的。一些狭窄的意见认为，POST方法用来创建资源，
而PUT方法则用来更新资源。这个说法本身没有问题，但是并没有从根本上解释了二者的区别。事实上，它们最根本的区别就是：
POST方法不是幂等的，而PUT方法则有幂等性。那这又衍生出一个问题，什么是幂等？
    幂等（idempotent、idempotence）是一个抽象代数的概念。在计算机中，可以这么理解，一个幂等操作的特点就是其任意多次
执行所产生的影响均与依次一次执行的影响相同。
    POST在请求的时候，服务器会每次都创建一个文件，但是在PUT方法的时候只是简单地更新，而不是去重新创建。因此PUT是幂等的。



对应客户端请求行中的请求方法:
请求方法（所有方法全为大写）有多种，各个方法的解释如下：
GET     请求获取Request-URI所标识的资源
POST    在Request-URI所标识的资源后附加新的数据
HEAD    请求获取由Request-URI所标识的资源的响应消息报头
PUT     请求服务器存储一个资源，并用Request-URI作为其标识
DELETE  请求服务器删除Request-URI所标识的资源
TRACE   请求服务器回送收到的请求信息，主要用于测试或诊断
CONNECT 保留将来使用
OPTIONS 请求查询服务器的性能，或者查询与资源相关的选项和需求
*/
#define NGX_HTTP_UNKNOWN                   0x0001
#define NGX_HTTP_GET                       0x0002
#define NGX_HTTP_HEAD                      0x0004
#define NGX_HTTP_POST                      0x0008
#define NGX_HTTP_PUT                       0x0010
#define NGX_HTTP_DELETE                    0x0020
#define NGX_HTTP_MKCOL                     0x0040
#define NGX_HTTP_COPY                      0x0080
#define NGX_HTTP_MOVE                      0x0100
#define NGX_HTTP_OPTIONS                   0x0200
#define NGX_HTTP_PROPFIND                  0x0400
#define NGX_HTTP_PROPPATCH                 0x0800
#define NGX_HTTP_LOCK                      0x1000
#define NGX_HTTP_UNLOCK                    0x2000
#define NGX_HTTP_PATCH                     0x4000
#define NGX_HTTP_TRACE                     0x8000

#define NGX_HTTP_CONNECTION_CLOSE          1
#define NGX_HTTP_CONNECTION_KEEP_ALIVE     2


#define NGX_NONE                           1

//返回NGX_HTTP_PARSE_HEADER_DONE表示响应中所有的http头部都解析完毕，接下来再接收到的都将是http包体
#define NGX_HTTP_PARSE_HEADER_DONE         1

#define NGX_HTTP_CLIENT_ERROR              10
#define NGX_HTTP_PARSE_INVALID_METHOD      10
#define NGX_HTTP_PARSE_INVALID_REQUEST     11
#define NGX_HTTP_PARSE_INVALID_09_METHOD   12

#define NGX_HTTP_PARSE_INVALID_HEADER      13


/* unused                                  1 */
/* 而flag我们一般只会感兴趣下面这个NGX_HTTP_SUBREQUEST_IN_MEMORY，flag设为这个宏时，表示发起的子请求，访问的网络资源返回的响应将全部放在内存
中，我们可以从upstream->buffer里取到响应内容 */
#define NGX_HTTP_SUBREQUEST_IN_MEMORY      2
//表示如果该子请求提前完成(按后续遍历的顺序)，是否设置将它的状态设为done，当设置该参数时，提前完成就会设置done，不设时，会让该子
//请求等待它之前的子请求处理完毕才会将状态设置为done。
#define NGX_HTTP_SUBREQUEST_WAITED         4
#define NGX_HTTP_LOG_UNSAFE                8


/*
http响应状态码大全

 

http状态返回代码 1xx（临时响应）
表示临时响应并需要请求者继续执行操作的状态代码。

http状态返回代码 代码   说明
100   （继续） 请求者应当继续提出请求。服务器返回此代码表示已收到请求的第一部分，正在等待其余部分。 
101   （切换协议）请求者已要求服务器切换协议，服务器已确认并准备切换。

http状态返回代码 2xx （成功）
表示成功处理了请求的状态代码。

http状态返回代码 代码   说明
200   （成功）  服务器已成功处理了请求。 通常，这表示服务器提供了请求的网页。
201   （已创建）  请求成功并且服务器创建了新的资源。
202   （已接受）  服务器已接受请求，但尚未处理。
203   （非授权信息）  服务器已成功处理了请求，但返回的信息可能来自另一来源。
204   （无内容）  服务器成功处理了请求，但没有返回任何内容。
205   （重置内容）服务器成功处理了请求，但没有返回任何内容。
206   （部分内容）  服务器成功处理了部分 GET 请求。

http状态返回代码 3xx （重定向）
表示要完成请求，需要进一步操作。 通常，这些状态代码用来重定向。

http状态返回代码 代码   说明
300   （多种选择）  针对请求，服务器可执行多种操作。 服务器可根据请求者 (user agent) 选择一项操作，或提供操作列表供请求者选择。
301   （永久移动）  请求的网页已永久移动到新位置。 服务器返回此响应（对 GET 或 HEAD 请求的响应）时，会自动将请求者转到新位置。
302   （临时移动）  服务器目前从不同位置的网页响应请求，但请求者应继续使用原有位置来进行以后的请求。
303   （查看其他位置） 请求者应当对不同的位置使用单独的 GET 请求来检索响应时，服务器返回此代码。


304   （未修改）自从上次请求后，请求的网页未修改过。 服务器返回此响应时，不会返回网页内容。
305   （使用代理） 请求者只能使用代理访问请求的网页。如果服务器返回此响应，还表示请求者应使用代理。
307   （临时重定向）  服务器目前从不同位置的网页响应请求，但请求者应继续使用原有位置来进行以后的请求。 
http状态返回代码 4xx（请求错误）
这些状态代码表示请求可能出错，妨碍了服务器的处理。

http状态返回代码 代码   说明
400   （错误请求） 服务器不理解请求的语法。
401   （未授权） 请求要求身份验证。对于需要登录的网页，服务器可能返回此响应。
403   （禁止） 服务器拒绝请求。
404   （未找到） 服务器找不到请求的网页。
405   （方法禁用） 禁用请求中指定的方法。
406   （不接受）无法使用请求的内容特性响应请求的网页。
407   （需要代理授权） 此状态代码与 401（未授权）类似，但指定请求者应当授权使用代理。
408   （请求超时）  服务器等候请求时发生超时。
409   （冲突）  服务器在完成请求时发生冲突。 服务器必须在响应中包含有关冲突的信息。
410   （已删除）  如果请求的资源已永久删除，服务器就会返回此响应。
411   （需要有效长度）服务器不接受不含有效内容长度标头字段的请求。
412   （未满足前提条件）服务器未满足请求者在请求中设置的其中一个前提条件。
413   （请求实体过大）服务器无法处理请求，因为请求实体过大，超出服务器的处理能力。
414   （请求的 URI 过长） 请求的 URI（通常为网址）过长，服务器无法处理。
415   （不支持的媒体类型）请求的格式不受请求页面的支持。
416   （请求范围不符合要求）如果页面无法提供请求的范围，则服务器会返回此状态代码。
417   （未满足期望值）服务器未满足"期望"请求标头字段的要求。

http状态返回代码 5xx（服务器错误）
这些状态代码表示服务器在尝试处理请求时发生内部错误。 这些错误可能是服务器本身的错误，而不是请求出错。

http状态返回代码 代码   说明
500   （服务器内部错误）  服务器遇到错误，无法完成请求。
501   （尚未实施） 服务器不具备完成请求的功能。例如，服务器无法识别请求方法时可能会返回此代码。
502   （错误网关）服务器作为网关或代理，从上游服务器收到无效响应。
503   （服务不可用）服务器目前无法使用（由于超载或停机维护）。 通常，这只是暂时状态。
504   （网关超时）  服务器作为网关或代理，但是没有及时从上游服务器收到请求。
505   （HTTP 版本不受支持） 服务器不支持请求中所用的 HTTP 协议版本。 

一些常见的http状态返回代码为：
200 - 服务器成功返回网页
404 - 请求的网页不存在
503 - 服务不可用

*/

/*
http状态返回代码 1xx（临时响应）
表示临时响应并需要请求者继续执行操作的状态代码。

http状态返回代码 代码   说明
100   （继续） 请求者应当继续提出请求。服务器返回此代码表示已收到请求的第一部分，正在等待其余部分。 
101   （切换协议）请求者已要求服务器切换协议，服务器已确认并准备切换。
*/
#define NGX_HTTP_CONTINUE                  100
#define NGX_HTTP_SWITCHING_PROTOCOLS       101 //HTTP/1.1 101  
#define NGX_HTTP_PROCESSING                102


/*
http状态返回代码 2xx （成功）
表示成功处理了请求的状态代码。

http状态返回代码 代码   说明
200   （成功）  服务器已成功处理了请求。 通常，这表示服务器提供了请求的网页。
201   （已创建）  请求成功并且服务器创建了新的资源。
202   （已接受）  服务器已接受请求，但尚未处理。
203   （非授权信息）  服务器已成功处理了请求，但返回的信息可能来自另一来源。
204   （无内容）  服务器成功处理了请求，但没有返回任何内容。
205   （重置内容）服务器成功处理了请求，但没有返回任何内容。
206   （部分内容）  服务器成功处理了部分 GET 请求。
*/
#define NGX_HTTP_OK                        200
#define NGX_HTTP_CREATED                   201
#define NGX_HTTP_ACCEPTED                  202
#define NGX_HTTP_NO_CONTENT                204
#define NGX_HTTP_PARTIAL_CONTENT           206

/*
http状态返回代码 3xx （重定向）
表示要完成请求，需要进一步操作。 通常，这些状态代码用来重定向。

http状态返回代码 代码   说明
300   （多种选择）  针对请求，服务器可执行多种操作。 服务器可根据请求者 (user agent) 选择一项操作，或提供操作列表供请求者选择。
301   （永久移动）  请求的网页已永久移动到新位置。 服务器返回此响应（对 GET 或 HEAD 请求的响应）时，会自动将请求者转到新位置。
302   （临时移动）  服务器目前从不同位置的网页响应请求，但请求者应继续使用原有位置来进行以后的请求。
303   （查看其他位置） 请求者应当对不同的位置使用单独的 GET 请求来检索响应时，服务器返回此代码。
304   （未修改）自从上次请求后，请求的网页未修改过。 服务器返回此响应时，不会返回网页内容。
305   （使用代理） 请求者只能使用代理访问请求的网页。如果服务器返回此响应，还表示请求者应使用代理。
307   （临时重定向）  服务器目前从不同位置的网页响应请求，但请求者应继续使用原有位置来进行以后的请求。 

302报文
HTTP/1.1 302 Moved Temporarily
Server: nginx/1.9.2
Date: Wed, 12 Feb 2025 15:21:45 GMT
Content-Type: text/html
Content-Length: 160
Connection: keep-alive
Location: http://10.10.0.103:8080  浏览器收到该ack报文后会再次请求http://10.10.0.103:8080

301报文，永久重定向
HTTP/1.1 301 Moved Permanently
Server: nginx/1.9.2
Date: Wed, 12 Feb 2025 15:25:58 GMT
Content-Type: text/html
Content-Length: 184
Connection: keep-alive
Location: http://10.10.0.103:8080  浏览器收到该报文后，以后只要是到服务器的地址

*/
#define NGX_HTTP_SPECIAL_RESPONSE          300
#define NGX_HTTP_MOVED_PERMANENTLY         301
#define NGX_HTTP_MOVED_TEMPORARILY         302 //服务器返回该302，浏览器收到后，会把从新请求浏览器发送回来的新的重定向地址
#define NGX_HTTP_SEE_OTHER                 303
#define NGX_HTTP_NOT_MODIFIED              304
#define NGX_HTTP_TEMPORARY_REDIRECT        307

/*
http状态返回代码 4xx（请求错误）
这些状态代码表示请求可能出错，妨碍了服务器的处理。

http状态返回代码 代码   说明
400   （错误请求） 服务器不理解请求的语法。
401   （未授权） 请求要求身份验证。对于需要登录的网页，服务器可能返回此响应。
403   （禁止） 服务器拒绝请求。
404   （未找到） 服务器找不到请求的网页。
405   （方法禁用） 禁用请求中指定的方法。
406   （不接受）无法使用请求的内容特性响应请求的网页。
407   （需要代理授权） 此状态代码与 401（未授权）类似，但指定请求者应当授权使用代理。
408   （请求超时）  服务器等候请求时发生超时。
409   （冲突）  服务器在完成请求时发生冲突。 服务器必须在响应中包含有关冲突的信息。
410   （已删除）  如果请求的资源已永久删除，服务器就会返回此响应。
411   （需要有效长度）服务器不接受不含有效内容长度标头字段的请求。
412   （未满足前提条件）服务器未满足请求者在请求中设置的其中一个前提条件。
413   （请求实体过大）服务器无法处理请求，因为请求实体过大，超出服务器的处理能力。
414   （请求的 URI 过长） 请求的 URI（通常为网址）过长，服务器无法处理。
415   （不支持的媒体类型）请求的格式不受请求页面的支持。
416   （请求范围不符合要求）如果页面无法提供请求的范围，则服务器会返回此状态代码。
417   （未满足期望值）服务器未满足"期望"请求标头字段的要求。
*/
#define NGX_HTTP_BAD_REQUEST               400
#define NGX_HTTP_UNAUTHORIZED              401
#define NGX_HTTP_FORBIDDEN                 403
#define NGX_HTTP_NOT_FOUND                 404
#define NGX_HTTP_NOT_ALLOWED               405
#define NGX_HTTP_REQUEST_TIME_OUT          408
#define NGX_HTTP_CONFLICT                  409
#define NGX_HTTP_LENGTH_REQUIRED           411
#define NGX_HTTP_PRECONDITION_FAILED       412
#define NGX_HTTP_REQUEST_ENTITY_TOO_LARGE  413
#define NGX_HTTP_REQUEST_URI_TOO_LARGE     414
#define NGX_HTTP_UNSUPPORTED_MEDIA_TYPE    415
#define NGX_HTTP_RANGE_NOT_SATISFIABLE     416


/* Our own HTTP codes */

/* The special code to close connection without any response */
#define NGX_HTTP_CLOSE                     444

#define NGX_HTTP_NGINX_CODES               494

#define NGX_HTTP_REQUEST_HEADER_TOO_LARGE  494

#define NGX_HTTPS_CERT_ERROR               495
#define NGX_HTTPS_NO_CERT                  496

/*
 * We use the special code for the plain HTTP requests that are sent to
 * HTTPS port to distinguish it from 4XX in an error page redirection
 */
#define NGX_HTTP_TO_HTTPS                  497

/* 498 is the canceled code for the requests with invalid host name */

/*
 * HTTP does not define the code for the case when a client closed
 * the connection while we are processing its request so we introduce
 * own code to log such situation when a client has closed the connection
 * before we even try to send the HTTP header to it
 */
#define NGX_HTTP_CLIENT_CLOSED_REQUEST     499


/*
http状态返回代码 5xx（服务器错误）
这些状态代码表示服务器在尝试处理请求时发生内部错误。 这些错误可能是服务器本身的错误，而不是请求出错。

http状态返回代码 代码   说明
500   （服务器内部错误）  服务器遇到错误，无法完成请求。
501   （尚未实施） 服务器不具备完成请求的功能。例如，服务器无法识别请求方法时可能会返回此代码。
502   （错误网关）服务器作为网关或代理，从上游服务器收到无效响应。
503   （服务不可用）服务器目前无法使用（由于超载或停机维护）。 通常，这只是暂时状态。
504   （网关超时）  服务器作为网关或代理，但是没有及时从上游服务器收到请求。
505   （HTTP 版本不受支持） 服务器不支持请求中所用的 HTTP 协议版本。 
*/
#define NGX_HTTP_INTERNAL_SERVER_ERROR     500
#define NGX_HTTP_NOT_IMPLEMENTED           501
#define NGX_HTTP_BAD_GATEWAY               502
#define NGX_HTTP_SERVICE_UNAVAILABLE       503
#define NGX_HTTP_GATEWAY_TIME_OUT          504
#define NGX_HTTP_INSUFFICIENT_STORAGE      507

/*
缓存中的业务类型。任何事件消费模块都可以自定义需要的标志位。这个buffered字段有8位，最多可以同时表示8个不同的业务。第三方模
块在自定义buffered标志位时注意不要与可能使用的模块定义的标志位冲突。目前openssl模块定义了一个标志位：
    #define NGX_SSL_BUFFERED    Ox01
    
    HTTP官方模块定义了以下标志位：
    #define blGX HTTP_LOWLEVEL_BUFFERED   0xf0
    #define NGX_HTTP_WRITE_BUFFERED       0x10
    #define NGX_HTTP_GZIP_BUFFERED        0x20
    #define NGX_HTTP_SSI_BUFFERED         0x01
    #define NGX_HTTP_SUB_BUFFERED         0x02
    #define NGX_HTTP_COPY_BUFFERED        0x04
    #define NGX_HTTP_IMAGE_BUFFERED       Ox08
同时，对于HTTP模块而言，buffered的低4位要慎用，在实际发送响应的ngx_http_write_filter_module过滤模块中，低4位标志位为1则惫味着
Nginx会一直认为有HTTP模块还需要处理这个请求，必须等待HTTP模块将低4位全置为0才会正常结束请求。检查低4位的宏如下：
    #define NGX_LOWLEVEL_BUFFERED  OxOf
 */
//一下这些在ngx_connection_s中的buffered标志中使用
#define NGX_HTTP_LOWLEVEL_BUFFERED         0xf0
//告诉HTTP框架out缓冲区中还有响应等待发送。 ngx_http_request_t->out中的数据是否发送完毕，参考ngx_http_write_filter
#define NGX_HTTP_WRITE_BUFFERED            0x10 
#define NGX_HTTP_GZIP_BUFFERED             0x20
#define NGX_HTTP_SSI_BUFFERED              0x01
#define NGX_HTTP_SUB_BUFFERED              0x02
#define NGX_HTTP_COPY_BUFFERED             0x04


typedef enum {
    NGX_HTTP_INITING_REQUEST_STATE = 0,
    NGX_HTTP_READING_REQUEST_STATE,   /* ngx_http_create_request中赋值 */
    NGX_HTTP_PROCESS_REQUEST_STATE, /* 解析玩头部行后在ngx_http_process_request_headers中赋值 */

    NGX_HTTP_CONNECT_UPSTREAM_STATE,
    NGX_HTTP_WRITING_UPSTREAM_STATE,
    NGX_HTTP_READING_UPSTREAM_STATE,

    NGX_HTTP_WRITING_REQUEST_STATE, /* ngx_http_set_write_handler */
    NGX_HTTP_LINGERING_CLOSE_STATE,
    NGX_HTTP_KEEPALIVE_STATE
} ngx_http_state_e;


typedef struct { //见ngx_http_headers_in
    ngx_str_t                         name; //匹配头部行name:value中的name，见ngx_http_process_request_headers
    ngx_uint_t                        offset;
    //handler的三个参数分别为(r, h, hh->offset):r为对应的连接请求，h存储为头部行key:value(如:Content-Type: text/html)值，
    //hh->offset即ngx_http_headers_in中成员的对应offset(如请求行带有host，则offset=offsetof(ngx_http_headers_in_t, host))
    ngx_http_header_handler_pt        handler; //ngx_http_process_request_headers中执行 
} ngx_http_header_t;


typedef struct {
    ngx_str_t                         name;
    ngx_uint_t                        offset;
} ngx_http_header_out_t;

/*
  HTTP 头部解释
 
 1. Accept：告诉WEB服务器自己接受什么介质类型，* / * 表示任何类型，type/ * 表示该类型下的所有子类型，type/sub-type。
  
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
                          min-fresh：（接受其新鲜生命期大于其当前 Age 跟 min-fresh 值之和的缓存对象）
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
              跟WEB服务器验证了其有效性后，才能用来响应客户请求。是 HTTP/1.0 的头部。
              例如：Expires：Sat, 23 May 2009 10:02:12 GMT
  
 11. Host：客户端指定自己想访问的WEB服务器的域名/IP 地址和端口号。
                 例如：Host：rss.sina.com.cn
  
 12. If-Match：如果对象的 ETag 没有改变，其实也就意味著对象没有改变，才执行请求的动作。
     If-None-Match：如果对象的 ETag 改变了，其实也就意味著对象也改变了，才执行请求的动作。
  
 13. If-Modified-Since：如果请求的对象在该头部指定的时间之后修改了，才执行请求
                        的动作（比如返回对象），否则返回代码304，告诉浏览器该对象没有修改。
                        例如：If-Modified-Since：Thu, 10 Apr 2008 09:14:42 GMT
     If-Unmodified-Since：如果请求的对象在该头部指定的时间之后没修改过，才执行请求的动作（比如返回对象）。
  
 14. If-Range：浏览器告诉 WEB 服务器，如果我请求的对象没有改变，就把我缺少的部分
                给我，如果对象改变了，就把整个对象给我。 浏览器通过发送请求对象的 
                ETag 或者 自己所知道的最后修改时间给 WEB 服务器，让其判断对象是否
                改变了。总是跟 Range 头部一起使用。
  
 15. Last-Modified：WEB 服务器认为对象的最后修改时间，比如文件的最后修改时间，
                    动态页面的最后产生时间等等。例如：Last-Modified：Tue, 06 May 2008 02:42:43 GMT
  
 16. Location：WEB 服务器告诉浏览器，试图访问的对象已经被移到别的位置了，到该头部指定的位置去取。
                         例如：Location：http://i0.sinaimg.cn/dy/deco/2008/0528/sinahome_0803_ws_005_text_0.gif
  
 17. Pramga：主要使用 Pramga: no-cache，相当于 Cache-Control： no-cache。例如：Pragma：no-cache
  
 18. Proxy-Authenticate： 代理服务器响应浏览器，要求其提供代理身份验证信息。
       Proxy-Authorization：浏览器响应代理服务器的身份验证请求，提供自己的身份信息。
  
 19. Range：浏览器（比如 Flashget 多线程下载时）告诉 WEB 服务器自己想取对象的哪部分。
                     例如：Range: bytes=1173546-
  
 20. Referer：浏览器向 WEB 服务器表明自己是从哪个 网页/URL 获得/点击 当前请求中的网址/URL。
                    例如：Referer：http://www.sina.com/
  
 21. Server: WEB 服务器表明自己是什么软件及版本等信息。例如：Server：Apache/2.0.61 (Unix)
  
 22. User-Agent: 浏览器表明自己的身份（是哪种浏览器）。
                         例如：User-Agent：Mozilla/5.0 (Windows; U; Windows NT 5.1; zh-CN;   
                                   rv:1.8.1.14) Gecko/20080404 Firefox/2.0.0.14
  
 23. Transfer-Encoding: WEB 服务器表明自己对本响应消息体（不是消息体里面的对象）作了怎样的编码，比如是否分块（chunked）。
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
         什么协议（和版本）发送的请求。当客户端请求到达第一个代理服务器时，该服务器会在自己发出的请求里面
         添加 Via 头部，并填上自己的相关信息，当下一个代理服务器 收到第一个代理服务器的请求时，会在自己发
         出的请求里面复制前一个代理服务器的请求的Via头部，并把自己的相关信息加到后面， 以此类推，当 OCS 
         收到最后一个代理服务器的请求时，检查 Via 头部，就知道该请求所经过的路由。
         例如：Via：1.0 236-81.D07071953.sina.com.cn:80 (squid/2.6.STABLE13)

*/
/*
常用的HTTP头部信息可以通过r->headers_in获取，不常用的HTTP头部则需要遍历r->headers_in.headers来遍历获取
*/

//类型的headers_in则存储已经解析过的HTTP头部。下面介绍ngx_http_headers_in_t结构体中的成员。 ngx_http_request_s请求内容中包含该结构
//参考ngx_http_headers_in，通过该数组中的回调hander来存储解析到的请求行name:value中的value到headers_in的响应成员中，见ngx_http_process_request_headers
typedef struct { 
    /*
    获取HTTP头部时，直接使用r->headers_in的相应成员就可以了。这里举例说明一下如何通过遍历headers链表获取非RFC2616标准的HTTP头部，
    可以先回顾一下ngx_list_t链表和ngx_table_elt_t结构体的用法。headers是一个ngx_list_t链表，它存储着解析过的所有HTTP头部，链表中
    的元素都是ngx_table_elt_t类型。下面尝试在一个用户请求中找到“Rpc-Description”头部，首先判断其值是否为“uploadFile”，再决定
    后续的服务器行为，代码如下。
    ngx_list_part_t *part = &r->headers_in.headers.part;
    ngx_table_elt_t *header = part->elts;
    
    //开始遍历链表
    for (i = 0; ; i++) {
     //判断是否到达链表中当前数组的结尾处
     if (i >= part->nelts) {
      //是否还有下一个链表数组元素
      if (part->next == NULL) {
       break;
      }
    
       part设置为next来访问下一个链表数组；header也指向下一个链表数组的首地址；i设置为0时，表示从头开始遍历新的链表数组
      part = part->next;
      header = part->elts;
      i = 0;
     }
    
     //hash为0时表示不是合法的头部
     if (header[i].hash == 0) {
      continue;
     }
    
     判断当前的头部是否是“Rpc-Description”。如果想要忽略大小写，则应该先用header[i].lowcase_key代替header[i].key.data，然后比较字符串
     if (0 == ngx_strncasecmp(header[i].key.data,
       (u_char*) "Rpc-Description",
       header[i].key.len))
     {
      //判断这个HTTP头部的值是否是“uploadFile”
      if (0 == ngx_strncmp(header[i].value.data,
        "uploadFile",
        header[i].value.len))
      {
       //找到了正确的头部，继续向下执行
      }
     }
    }
    
    对于常见的HTTP头部，直接获取r->headers_in中已经由HTTP框架解析过的成员即可，而对于不常见的HTTP头部，需要遍历r->headers_in.headers链表才能获得。
*/
    /*常用的HTTP头部信息可以通过r->headers_in获取，不常用的HTTP头部则需要遍历r->headers_in.headers来遍历获取*/
    /*所有解析过的HTTP头部都在headers链表中，可以使用遍历链表的方法来获取所有的HTTP头部。注意，这里headers链表的
    每一个元素都是ngx_table_elt_t成员*/ //从ngx_http_headers_in获取变量后存储到该链表中，链表中的成员就是下面的各个ngx_table_elt_t成员
    ngx_list_t                        headers; //在ngx_http_process_request_line初始化list空间  ngx_http_process_request_headers中存储解析到的请求行value和key

    /*以下每个ngx_table_elt_t成员都是RFC1616规范中定义的HTTP头部， 它们实际都指向headers链表中的相应成员。注意，
    当它们为NULL空指针时，表示没有解析到相应的HTTP头部*/ //server和host指向内容一样，都是头部中携带的host头部
    ngx_table_elt_t                  *host; //http1.0以上必须带上host头部行，见ngx_http_process_request_header
    ngx_table_elt_t                  *connection;

/*
If-Modified-Since:从字面上看, 就是说: 如果从某个时间点算起, 如果文件被修改了. 
    1.如果真的被修改: 那么就开始传输, 服务器返回:200 OK  
    2.如果没有被修改: 那么就无需传输, 服务器返回: 403 Not Modified.
用途:客户端尝试下载最新版本的文件. 比如网页刷新, 加载大图的时候。很明显: 如果从图片下载以后都没有再被修改, 当然就没必要重新下载了!

If-Unmodified-Since: 从字面上看, 意思是: 如果从某个时间点算起, 文件没有被修改.....
    1. 如果没有被修改: 则开始`继续'传送文件: 服务器返回: 200 OK
    2. 如果文件被修改: 则不传输, 服务器返回: 412 Precondition failed (预处理错误)
用途:断点续传(一般会指定Range参数). 要想断点续传, 那么文件就一定不能被修改, 否则就不是同一个文件了

总之一句话: 一个是修改了才下载, 一个是没修改才下载.
*/
    ngx_table_elt_t                  *if_modified_since;
    ngx_table_elt_t                  *if_unmodified_since;

/*
ETags和If-None-Match是一种常用的判断资源是否改变的方法。类似于Last-Modified和HTTP-If-Modified-Since。但是有所不同的是Last-Modified和HTTP-If-Modified-Since只判断资源的最后修改时间，而ETags和If-None-Match可以是资源任何的任何属性。
ETags和If-None-Match的工作原理是在HTTPResponse中添加ETags信息。当客户端再次请求该资源时，将在HTTPRequest中加入If-None-Match信息（ETags的值）。如果服务器验证资源的ETags没有改变（该资源没有改变），将返回一个304状态；否则，服务器将返回200状态，并返回该资源和新的ETags。
*/
    ngx_table_elt_t                  *if_match; //和etag配对
    ngx_table_elt_t                  *if_none_match; //和etag配对
    ngx_table_elt_t                  *user_agent;
    ngx_table_elt_t                  *referer;
    ngx_table_elt_t                  *content_length;
    ngx_table_elt_t                  *content_type;

    ngx_table_elt_t                  *range;
    ngx_table_elt_t                  *if_range;

    ngx_table_elt_t                  *transfer_encoding; //Transfer-Encoding
    ngx_table_elt_t                  *expect;
    //Upgrade 允许服务器指定一种新的协议或者新的协议版本，与响应编码101（切换协议）配合使用。例如：Upgrade: HTTP/2.0 
    ngx_table_elt_t                  *upgrade;

#if (NGX_HTTP_GZIP)
    ngx_table_elt_t                  *accept_encoding;
    ngx_table_elt_t                  *via;
#endif

    ngx_table_elt_t                  *authorization;
    //只有在connection_type == NGX_HTTP_CONNECTION_KEEP_ALIVE的情况下才生效    Connection=keep-alive时才有效
    ngx_table_elt_t                  *keep_alive; //赋值ngx_http_process_header_line

#if (NGX_HTTP_X_FORWARDED_FOR)
    ngx_array_t                       x_forwarded_for;
#endif

#if (NGX_HTTP_REALIP)
    ngx_table_elt_t                  *x_real_ip;
#endif

#if (NGX_HTTP_HEADERS)
    ngx_table_elt_t                  *accept;
    ngx_table_elt_t                  *accept_language;
#endif

#if (NGX_HTTP_DAV)
    ngx_table_elt_t                  *depth;
    ngx_table_elt_t                  *destination;
    ngx_table_elt_t                  *overwrite;
    ngx_table_elt_t                  *date;
#endif

    /* user和passwd是只有ngx_http_auth_basic_module才会用到的成员 */
    ngx_str_t                         user;
    ngx_str_t                         passwd;

/*
 Cookie的实现
Cookie是web server下发给浏览器的任意的一段文本，在后续的http 请求中，浏览器会将cookie带回给Web Server。
同时在浏览器允许脚本执行的情况下，Cookie是可以被JavaScript等脚本设置的。


a. 如何种植Cookie

http请求cookie下发流程
http方式:以访问http://www.webryan.net/index.php为例
Step1.客户端发起http请求到Server

GET /index.php HTTP/1.1
 Host: www.webryan.net
 (这里是省去了User-Agent,Accept等字段)

Step2. 服务器返回http response,其中可以包含Cookie设置

HTTP/1.1 200 OK
 Content-type: text/html
 Set-Cookie: name=value
 Set-Cookie: name2=value2; Expires=Wed, 09 Jun 2021 10:18:14 GMT
 (content of page)

Step3. 后续访问webryan.net的相关页面

GET /spec.html HTTP/1.1
 Host: www.webryan.net
 Cookie: name=value; name2=value2
 Accept: * / *
需要修改cookie的值的话，只需要Set-Cookie: name=newvalue即可，浏览器会用新的值将旧的替换掉。
*/
    /*cookies是以ngx_array_t数组存储的，本章先不介绍这个数据结构，感兴趣的话可以直接跳到7.3节了解ngx_array_t的相关用法*/
    ngx_array_t                       cookies;
    //server和host指向内容一样，都是头部中携带的host头部
    ngx_str_t                         server;//server名称   ngx_http_process_host

    /* 在丢弃包体的时候(见ngx_http_read_discarded_request_body)，headers_in成员里的content_length_n，最初它等于content-length头部，而每丢弃一部分包体，就会在content_length_n变量
    中减去相应的大小。因此，content_length_n表示还需要丢弃的包体长度，这里首先检查请求的content_length_n成员，如果它已经等于0，则表示已经接收到完整的包体 
    */
    //解析完头部行后通过ngx_http_process_request_header来开辟空间从而来存储请求体中的内容，表示请求包体的大小，如果为-1表示请求中不带包体
    off_t                             content_length_n; //根据ngx_table_elt_t *content_length计算出的HTTP包体大小  默认赋值-1
    time_t                            keep_alive_n; //头部行Keep-Alive:所带参数 Connection=keep-alive时才有效  赋值ngx_http_process_request_header

    /*HTTP连接类型，它的取值范围是0、NGX_http_CONNECTION_CLOSE或者NGX_HTTP_CONNECTION_KEEP_ALIVE*/
    unsigned                          connection_type:2; //NGX_HTTP_CONNECTION_KEEP_ALIVE等，表示长连接 短连接

/*以下7个标志位是HTTP框架根据浏览器传来的“useragent”头部，它们可用来判断浏览器的类型，值为1时表示是相应的浏览器发来的请求，值为0时则相反*/
    unsigned                          chunked:1;//Transfer-Encoding:chunked的时候置1
    unsigned                          msie:1; //IE Internet Explorer，简称IE
    unsigned                          msie6:1; //
    unsigned                          opera:1; //Opera 浏览器－免费、快速、安全。来自挪威，风行全球
    unsigned                          gecko:1; //Gecko是一套自由及 开放源代码、以C++编写网页 排版引擎，目前为Mozilla Firefox 网页浏览器以
    unsigned                          chrome:1; //Google Chrome是一款快速、简单且安全的网络浏览器
    unsigned                          safari:1; //Safari（苹果公司研发的网络浏览器）_
    unsigned                          konqueror:1;  //Konqueror v4.8.2. 当前最快速的浏览器之一
} ngx_http_headers_in_t;


/*
在向headers链表中添加自定义的HTTP头部时，可以参考ngx_list_push的使用方法。这里有一个简单的例子，如下所示。
ngx_table_elt_t* h = ngx_list_push(&r->headers_out.headers);
if (h == NULL) {
 return NGX_ERROR;
}

h->hash = 1;
h->key.len = sizeof("TestHead") - 1;
h->key.data = (u_char *) "TestHead";
h->value.len = sizeof("TestValue") - 1;
h->value.data = (u_char *) "TestValue";

这样将会在响应中新增一行HTTP头部：
TestHead: TestValud\r\n

如果发送的是一个不含有HTTP包体的响应，这时就可以直接结束请求了（例如，在ngx_http_mytest_handler方法中，直接在
ngx_http_send_header方法执行后将其返回值return即可）。

注意　ngx_http_send_header方法会首先调用所有的HTTP过滤模块共同处理headers_out中定义的HTTP响应头部，全部处理完
毕后才会序列化为TCP字符流发送到客户端，
*/
typedef struct { //包含在ngx_http_request_s结构headers_out中,ngx_http_send_header中把HTTP头部发出
    /*常用的HTTP头部信息可以通过r->headers_in获取，不常用的HTTP头部则需要遍历r->headers_in.headers来遍历获取*/ 
    //使用查找方法可以参考headers_in
    //如果连接了后端(例如fastcgi到PHP服务器),里面存储的是后端服务器返回的一行一行的头部行信息,赋值在ngx_http_upstream_process_headers->ngx_http_upstream_copy_header_line
    ngx_list_t                        headers;//待发送的HTTP头部链表，与headers_in中的headers成员类似  

    ngx_uint_t                        status;/*响应中的状态值，如200表示成功。NGX_HTTP_OK */ //真正发送给客户端的头部行组包生效在ngx_http_status_lines
    ngx_str_t                         status_line;//响应的状态行，如“HTTP/1.1 201 CREATED”

/*
      HTTP 头部解释
     
     1. Accept：告诉WEB服务器自己接受什么介质类型，* / * 表示任何类型，type/ * 表示该类型下的所有子类型，type/sub-type。
      
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
                              min-fresh：（接受其新鲜生命期大于其当前 Age 跟 min-fresh 值之和的缓存对象）
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
                  跟WEB服务器验证了其有效性后，才能用来响应客户请求。是 HTTP/1.0 的头部。
                  例如：Expires：Sat, 23 May 2009 10:02:12 GMT
      
     11. Host：客户端指定自己想访问的WEB服务器的域名/IP 地址和端口号。
                     例如：Host：rss.sina.com.cn
      
     12. If-Match：如果对象的 ETag 没有改变，其实也就意味著对象没有改变，才执行请求的动作。
         If-None-Match：如果对象的 ETag 改变了，其实也就意味著对象也改变了，才执行请求的动作。
      
     13. If-Modified-Since：如果请求的对象在该头部指定的时间之后修改了，才执行请求
                            的动作（比如返回对象），否则返回代码304，告诉浏览器该对象没有修改。
                            例如：If-Modified-Since：Thu, 10 Apr 2008 09:14:42 GMT
         If-Unmodified-Since：如果请求的对象在该头部指定的时间之后没修改过，才执行请求的动作（比如返回对象）。
      
     14. If-Range：浏览器告诉 WEB 服务器，如果我请求的对象没有改变，就把我缺少的部分
                    给我，如果对象改变了，就把整个对象给我。 浏览器通过发送请求对象的 
                    ETag 或者 自己所知道的最后修改时间给 WEB 服务器，让其判断对象是否
                    改变了。总是跟 Range 头部一起使用。
      
     15. Last-Modified：WEB 服务器认为对象的最后修改时间，比如文件的最后修改时间，
                        动态页面的最后产生时间等等。例如：Last-Modified：Tue, 06 May 2008 02:42:43 GMT
      
     16. Location：WEB 服务器告诉浏览器，试图访问的对象已经被移到别的位置了，到该头部指定的位置去取。
                             例如：Location：http://i0.sinaimg.cn/dy/deco/2008/0528/sinahome_0803_ws_005_text_0.gif
      
     17. Pramga：主要使用 Pramga: no-cache，相当于 Cache-Control： no-cache。例如：Pragma：no-cache
      
     18. Proxy-Authenticate： 代理服务器响应浏览器，要求其提供代理身份验证信息。
           Proxy-Authorization：浏览器响应代理服务器的身份验证请求，提供自己的身份信息。
      
     19. Range：浏览器（比如 Flashget 多线程下载时）告诉 WEB 服务器自己想取对象的哪部分。
                         例如：Range: bytes=1173546-
      
     20. Referer：浏览器向 WEB 服务器表明自己是从哪个 网页/URL 获得/点击 当前请求中的网址/URL。
                        例如：Referer：http://www.sina.com/
      
     21. Server: WEB 服务器表明自己是什么软件及版本等信息。例如：Server：Apache/2.0.61 (Unix)
      
     22. User-Agent: 浏览器表明自己的身份（是哪种浏览器）。
                             例如：User-Agent：Mozilla/5.0 (Windows; U; Windows NT 5.1; zh-CN;   
                                       rv:1.8.1.14) Gecko/20080404 Firefox/2.0.0.14
      
     23. Transfer-Encoding: WEB 服务器表明自己对本响应消息体（不是消息体里面的对象）作了怎样的编码，比如是否分块（chunked）。
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
             什么协议（和版本）发送的请求。当客户端请求到达第一个代理服务器时，该服务器会在自己发出的请求里面
             添加 Via 头部，并填上自己的相关信息，当下一个代理服务器 收到第一个代理服务器的请求时，会在自己发
             出的请求里面复制前一个代理服务器的请求的Via头部，并把自己的相关信息加到后面， 以此类推，当 OCS 
             收到最后一个代理服务器的请求时，检查 Via 头部，就知道该请求所经过的路由。
             例如：Via：1.0 236-81.D07071953.sina.com.cn:80 (squid/2.6.STABLE13)

    */
    /*以下每个ngx_table_elt_t成员都是RFC1616规范中定义的HTTP头部， 它们实际都指向headers链表中的相应成员。注意，
    当它们为NULL空指针时，表示没有解析到相应的HTTP头部*/

/*以下成员（包括ngx_table_elt_t）都是RFC1616规范中定义的HTTP头部，设置后，ngx_http_header_filter_module过滤模块可以把它们加到待发送的网络包中*/
    ngx_table_elt_t                  *server;
    ngx_table_elt_t                  *date;
    ngx_table_elt_t                  *content_length;
    ngx_table_elt_t                  *content_encoding;
    /* rewrite ^(.*)$ http://$1.mp4 break; 如果uri为http://10.135.0.1/aaa,则location中存储的是aaa.mp4 */
    //反向代理后端应答头中带有location:xxx也会进行重定向，见ngx_http_upstream_rewrite_location
    ngx_table_elt_t                  *location; //存储redirect时候，转换后的字符串值，见ngx_http_script_regex_end_code
    ngx_table_elt_t                  *refresh;
    ngx_table_elt_t                  *last_modified;
    ngx_table_elt_t                  *content_range;
    ngx_table_elt_t                  *accept_ranges;//Accept-Ranges: bytes  应该是content_length的单位
    ngx_table_elt_t                  *www_authenticate;
    //expires促发条件是expires配置，见ngx_http_set_expires ，头部行在ngx_http_set_expires进行创建空间以及头部行组装
    ngx_table_elt_t                  *expires;//expires xx配置存储函数为ngx_http_headers_expires，真正组包生效函数为ngx_http_set_expires
    /*
     ETag是一个可以与Web资源关联的记号（token）。典型的Web资源可以一个Web页，但也可能是JSON或XML文档。服务器单独负责判断记号是什么
     及其含义，并在HTTP响应头中将其传送到客户端，以下是服务器端返回的格式：ETag:"50b1c1d4f775c61:df3"客户端的查询更新格式是这样
     的：If-None-Match : W / "50b1c1d4f775c61:df3"如果ETag没改变，则返回状态304然后不返回，这也和Last-Modified一样。测试Etag主要
     在断点下载时比较有用。 "etag:XXX" ETag值的变更说明资源状态已经被修改

     
     Etag确定浏览器缓存： Etag的原理是将文件资源编号一个etag值，Response给访问者，访问者再次请求时，带着这个Etag值，与服务端所请求
     的文件的Etag对比，如果不同了就重新发送加载，如果相同，则返回304. HTTP/1.1304 Not Modified
     */ //见ngx_http_set_etag ETag: "569204ba-4e0924 //etag设置见ngx_http_set_etag 如果客户端在第一次请求文件和第二次请求文件这段时间，文件修改了，则etag就变了
    ngx_table_elt_t                  *etag;

    ngx_str_t                        *override_charset;

/*可以调用ngx_http_set_content_type(r)方法帮助我们设置Content-Type头部，这个方法会根据URI中的文件扩展名并对应着mime.type来设置Content-Type值,取值如:image/jpeg*/
    size_t                            content_type_len;
    ngx_str_t                         content_type;//见ngx_http_set_content_type
    ngx_str_t                         charset; //是从content_type中解析出来的，见ngx_http_upstream_copy_content_type
    u_char                           *content_type_lowcase;
    ngx_uint_t                        content_type_hash;
    //cache_control促发条件是expires配置或者add_head cache_control value配置，见ngx_http_set_expires ，头部行在ngx_http_set_expires进行创建空间以及头部行组装
    ngx_array_t                       cache_control;
    /*在这里指定过content_length_n后，不用再次到ngx_table_elt_t *content_length中设置响应长度*/
    off_t                             content_length_n; //这个标示应答体的长度  不包括头部行长度，只包含消息体长度
    time_t                            date_time;

    //实际上该时间是通过ngx_open_and_stat_file->stat获取的文件最近修改的时间，客户端每次请求都会从新通过stat获取，如果客户端第一次请求该文件和第二次请求该文件过程中修改了该文件，则
    //最通过stat信息获取的时间就会和之前通过last_modified_time发送给客户端的时间不一样
    time_t                            last_modified_time; //表示文件最后被修改的实际，可以参考ngx_http_static_handler
} ngx_http_headers_out_t;

//在ngx_http_read_client_request_body中，包体接收完毕后会执行回调方法ngx_http_client_body_handler_pt
/*
其中，有参数ngx_http_request_t *r，这个请求的信息都可以从r中获得。这样可以定义一个方法void func(ngx_http_request_t *r)，在Nginx接收完包体时调用它，
另外，后续的流程也都会写在这个方法中，例如：
void ngx_http_mytest_body_handler(ngx_http_request_t *r)
{
 …
}
*/
typedef void (*ngx_http_client_body_handler_pt)(ngx_http_request_t *r);
//用于保存HTTP包体的结构体ngx_http_request_body_t
/*
这个ngx_http_request_body_t结构体就存放在保存着请求的ngx_http_request_t结构体
的request_body成员中，接收HTTP包体就是围绕着这个数据结构进行的。
*/
typedef struct {//在ngx_http_read_client_request_body中分配存储空间
    //读取客户包体即使是存入临时文件中，当所有包体读取完毕后(ngx_http_do_read_client_request_body)，还是会让r->request_body->bufs指向文件中的相关偏移内存地址
    //临时文件资源回收函数为ngx_pool_run_cleanup_file
    ngx_temp_file_t                  *temp_file; //存放HTTP包体的临时文件  ngx_http_write_request_body分配空间  //如果配置"client_body_in_file_only" on | clean 表示包体存储在磁盘文件中
    /* 接收HTTP包体的缓冲区链表。当包体需要全部存放在内存中时，如果一块ngx_buf_t缓冲区无法存放完，这时就需要使用ngx_chain_t链表来存放 */
    //该空间上分配的各种内存信息，最终会调用ngx_free_chain把之前分配的内存加入的poll->chain中，从而在poll中同一回收释放资源。
    //在ngx_http_write_request_body中会把bufs链表中的所有ngx_buf_t节点信息里面的各个指针指向的数据区域(从网络中读取的)写入到临时文件temp_file中，然后把这些ngx_buf_t从bufs中删除，
    //通过ngx_free_chain加入到poll->chain，见ngx_http_write_request_body把bufs中的内容写入临时文件后，会把bufs(ngx_chain_t)节点放入r->pool->chain中
    
    //读取客户包体即使是存入临时文件中，当所有包体读取完毕后(ngx_http_do_read_client_request_body)，还是会让r->request_body->bufs指向文件中的相关偏移内存地址
    ngx_chain_t                      *bufs;//如果包体存入临时文件中，则bufs指向的ngx_chain_t中的各个指针指向文件中的相关偏移//在ngx_http_read_client_request_body中赋值
    //当buf中的数据填满后(buf->end = buf->last)，就会写入到临时文件后或者发送到上游服务器，该内存空间就可以继续存储读取到的数据了
    //ngx_http_read_client_request_body分配空间，应该是临时用的，如果需要多次读取才会读取完毕的时候，每次读取到的数据临时存放在buf中，
    //最终还是会存放到上面的bufs中，见ngx_http_request_body_length_filter
    ngx_buf_t                        *buf;
    off_t                             rest;//根据content-length头部和已接收到的包体长度，计算出的还需要接收的包体长度
    ngx_chain_t                      *free; //free  busy 和bufs链表的关系可以参考ngx_http_request_body_length_filter
    ngx_chain_t                      *busy;
    ngx_http_chunked_t               *chunked;
    /* HTTP包体接收完毕后执行的回调方法，也就是ngx_http_read_client_request_body方法传递的第2个参数 */ 
    //POST数据读取完毕后需要调用的HANDLER函数，也就是ngx_http_upstream_init  
    ngx_http_client_body_handler_pt   post_handler; //在ngx_http_read_client_request_body中赋值，执行在ngx_http_do_read_client_request_body
} ngx_http_request_body_t;


typedef struct ngx_http_addr_conf_s  ngx_http_addr_conf_t;

//空间创建和赋值见ngx_http_init_connection
//该结构存储了服务器端接收客户端连接时，服务器端所在的server{]上下文ctx  server_name等配置信息   ngx_http_request_t->http_connection和ngx_connection_t->data都可以指向该空间
typedef struct {  //获取请求对应的server块信息，server_name信息可以参考ngx_http_set_virtual_server作为例子
    //addr_conf = ngx_http_port_t->addrs[].conf 
    ngx_http_addr_conf_t             *addr_conf; //赋值见ngx_http_init_connection，根据本地IP地址获取到对应的addr_conf_t  这里面存储serv loc等配置信息以及server_name配置信息
    
    //客户端连接服务端accept成功后，该accept对应的listen所在的server{}配置块所在的ctx上下文//赋值见ngx_http_init_connection中hc->conf_ctx = hc->addr_conf->default_server->ctx;
    ngx_http_conf_ctx_t              *conf_ctx;  //该server{}块的上下文获取可以参考ngx_http_wait_request_handler
#if (NGX_HTTP_SSL && defined SSL_CTRL_SET_TLSEXT_HOSTNAME)  
    ngx_str_t                        *ssl_servername;
#if (NGX_PCRE)
    ngx_http_regex_t                 *ssl_servername_regex;
#endif
#endif

    ngx_buf_t                       **busy;
    ngx_int_t                         nbusy;

    ngx_buf_t                       **free;
    ngx_int_t                         nfree;

#if (NGX_HTTP_SSL)
    unsigned                          ssl:1;
#endif
    unsigned                          proxy_protocol:1; //listen配置项中是否携带该参数
} ngx_http_connection_t;


typedef void (*ngx_http_cleanup_pt)(void *data);

typedef struct ngx_http_cleanup_s  ngx_http_cleanup_t;
/*
 ngx_pool_cleanup_t与ngx_http_cleanup_pt是不同的，ngx_pool_cleanup_t仅在所用的内存池销毁时才会被调用来清理资源，它何时释放资
 源将视所使用的内存池而定，而ngx_http_cleanup_pt是在ngx_http_request_t结构体释放时被调用来释放资源的。


 如果需要在请求释放时执行一些回调方法，首先需要实现一个ngx_http_cleanup_pt方法。当然，HTTP框架还很友好地提供了一个工具
 方法ngx_http_cleanup_add，用于向请求中添加ngx_http_cleanup_t绪构体
*/
//任何一个请求的ngx_http_request_t结构体中都有一个ngx_http_cleanup_t类型的成员cleanup
struct ngx_http_cleanup_s {
    ngx_http_cleanup_pt               handler; //由HTTP模块提供的清理资源的回调方法  ngx_http_free_request ngx_http_terminate_request调用释放空间
    void                             *data; //希望给上面的handler方法传递的参数
    //一个请求可能会有多个ngx_http_cleanup_t清理方法，这些清理方法间就是通过next指针连接成单链表的
    ngx_http_cleanup_t               *next;
};

//Nginx在子请求正常或者异常结束时，都会调用ngx_http_post_subrequest_pt回调方法
typedef ngx_int_t (*ngx_http_post_subrequest_pt)(ngx_http_request_t *r,
    void *data, ngx_int_t rc);

/*
在生成ngx_http_post_subrequest_t结构体时，可以把任意数据赋给这里的data指针，ngx_http_post_subrequest_pt回调方法执行时的
data参数就是ngx_http_po st_subrequest_t结构体中的data成员指针。
    ngx_http_post_subrequest_pt回调方法中的rc参数是子请求在结束时的状态，它的取值则是执行ngx_http_finalize_request销毁请
求时传递的rc参数（对于本例来说，由于子请求使用反向代理模块访问上游HTTP服务器，所以rc此时是HTTP响应码。例如，在正常情况
下，rc会是200）。
*/
typedef struct {
    ngx_http_post_subrequest_pt       handler; //在函数ngx_http_finalize_request中执行  一般用来发送数据到客户端
    void                             *data;//data为传递给handler的额外参数
} ngx_http_post_subrequest_t;


typedef struct ngx_http_postponed_request_s  ngx_http_postponed_request_t;
/*
当派生一个子请求访问第三方服务附，如果只是希望接收到完整的响应后在Nginx中解析、处理，那么这里就不需要postpone模块；
如果原始请求派生出许多子请求，并且希望将所有子请求的响应依次转发给客户端，当然，这里的“依次”就是按照创建
子请求的顺序来发送响应，这时，postpone模块就有了“用武之地”。Nginx中的所有请求都是异步执行的，后创建的子请求可能优先执行，这样
转发到客户端的响应就会产生混乱。而postpone模块会强制地把待转发的响应包体放在一个链表中发送，只有优先转发的子请求结束后才会开始
转发下一个子请求中的响应。下面介绍一下它是如何实现的。每个请求的ngx_http_request_t结构体中都有一个postponed成员：


从上述代码可以看出，多个ngx_http_postponed_request_t之间使用next指针连接成一个单向链表。ngx_http_postponed_request_t中
的out成员是ngx_chain_t结构，它指向的是来自上游的、将要转发给下游的响应包体。
    每当使用ngx_http_output_filter方法（反向代理模块也使用该方法转发响应）向下游昀客户端发送响应包体时，都会调用到
ngx_http_postpone_filter_module过滤模块处理这段要发送的包体。
*/ //ngx_http_request_s中的postponed为该结构类型        
//ngx_http_subrequest中创建ngx_http_postponed_request_s空间,通过该结构的next成员把子请求挂载在其父请求的postponed链表的队尾 
//该结构可以表示子请求信息，通过request指向对应的子请求，也可以表示单纯的数据信息in链表，见ngx_http_postpone_filter_add
struct ngx_http_postponed_request_s { //参考ngx_http_postpone_filter方法 
    //如果通过ngx_http_postpone_filter_add添加In链数据，则request为NULL
    ngx_http_request_t               *request; //ngx_http_subrequest创建subrequest的时候，request指向子请求r,
    ngx_chain_t                      *out;//out成员是ngx_chain_t结构，它指向的是来自上游的、将要转发给下游的响应包体。
    ngx_http_postponed_request_t     *next;
};


typedef struct ngx_http_posted_request_s  ngx_http_posted_request_t;
//ngx_http_request_t中的posted_requests使用该结构        //ngx_http_post_request中创建ngx_http_posted_request_t空间
//ngx_http_post_request中创建ngx_http_posted_request_t空间  ngx_http_post_request将该子请求挂载在主请求的posted_requests链表队尾 
struct ngx_http_posted_request_s { //通过posted_requests就把各个子请求以单向链表的数据结构形式组织起来
    ngx_http_request_t               *request; //指向当前待处理子请求的ngx_http_request_t结构体
    ngx_http_posted_request_t        *next; //指向下一个子请求，如果没有，则为NULL空指针
};

/*
#define NGX_HTTP_OK                        200
#define NGX_HTTP_CREATED                   201
#define NGX_HTTP_ACCEPTED                  202
#define NGX_HTTP_NO_CONTENT                204
#define NGX_HTTP_PARTIAL_CONTENT           206

#define NGX_HTTP_SPECIAL_RESPONSE          300
#define NGX_HTTP_MOVED_PERMANENTLY         301
#define NGX_HTTP_MOVED_TEMPORARILY         302
#define NGX_HTTP_SEE_OTHER                 303
#define NGX_HTTP_NOT_MODIFIED              304
#define NGX_HTTP_TEMPORARY_REDIRECT        307

#define NGX_HTTP_BAD_REQUEST               400
#define NGX_HTTP_UNAUTHORIZED              401
#define NGX_HTTP_FORBIDDEN                 403
#define NGX_HTTP_NOT_FOUND                 404
#define NGX_HTTP_NOT_ALLOWED               405
#define NGX_HTTP_REQUEST_TIME_OUT          408
#define NGX_HTTP_CONFLICT                  409
#define NGX_HTTP_LENGTH_REQUIRED           411
#define NGX_HTTP_PRECONDITION_FAILED       412
#define NGX_HTTP_REQUEST_ENTITY_TOO_LARGE  413
#define NGX_HTTP_REQUEST_URI_TOO_LARGE     414
#define NGX_HTTP_UNSUPPORTED_MEDIA_TYPE    415
#define NGX_HTTP_RANGE_NOT_SATISFIABLE     416

 The special code to close connection without any response 
#define NGX_HTTP_CLOSE                     444
#define NGX_HTTP_NGINX_CODES               494
#define NGX_HTTP_REQUEST_HEADER_TOO_LARGE  494
#define NGX_HTTPS_CERT_ERROR               495
#define NGX_HTTPS_NO_CERT                  496

#define NGX_HTTP_TO_HTTPS                  497
#define NGX_HTTP_CLIENT_CLOSED_REQUEST     499


#define NGX_HTTP_INTERNAL_SERVER_ERROR     500
#define NGX_HTTP_NOT_IMPLEMENTED           501
#define NGX_HTTP_BAD_GATEWAY               502
#define NGX_HTTP_SERVICE_UNAVAILABLE       503
#define NGX_HTTP_GATEWAY_TIME_OUT          504
#define NGX_HTTP_INSUFFICIENT_STORAGE      507

注意　以上返回值除了RFC2616规范中定义的返回码外，还有Nginx自身定义的HTTP返回码。例如，NGX_HTTP_CLOSE就是用于要求HTTP框架直接关闭用户连接的。

在处理方法中除了返回HTTP响应码外，还可以返回Nginx全局定义的几个错误码，包括：
#define  NGX_OK          0
#define  NGX_ERROR      -1
#define  NGX_AGAIN      -2
#define  NGX_BUSY       -3
#define  NGX_DONE       -4
#define  NGX_DECLINED   -5
#define  NGX_ABORT      -6

*/
//这个返回值可以是HTTP中响应包的返回码，其中包括了HTTP框架已经在src/http/ngx_http_request.h文件中定义好的宏
//由HTTP模块实现的handler处理方法，这个方法在就是我们写自己的测试模块时曾经用ngx_http_mytest_handler方法实现过

/*
有7个HTTP阶段(NGX_HTTP_POST_READ_PHASE、NGX_HTTP_SERVER_REWRITE_PHASE、NGX_HTTP_REWRITE_PHASE、NGX_HTTP_PREACCESS_PHASE、
NGX_HTTP_ACCESS_PHASE、NGX_HTTP_CONTENT_PHASE、NGX_HTTP_LOG_PHASE)是允许任何一个HTTP模块实现自己的ngx_http_handler_pt处
理方法，并将其加入到这7个阶段中去的。在调用HTTP模块的postconfiguration方法向这7个阶段中添加处理方法前，需要先将phases数
组中这7个阶段里的handlers动态数组初始化（ngx_array_t类型需要执行ngx_array_init方法初始化），在这一步骤中，通过调
用ngx_http_init_phases方法来初始化这7个动态数组。

    通过调用所有HTTP模块的postconfiguration方法。HTTP模块可以在这一步骤中将自己的ngx_http_handler_pt处理方法添加到以上7个HTTP阶段中。
这样在具体的每个阶段就可以执行到我们的handler回调

NGX_HTTP_CONTENT_PHASE阶段与其他阶段都不相同的是，它向HTTP模块提供了两种介入该阶段的方式：第一种与其他10个阶段一样，
通过向全局的ngx_http_core_main_conf_t结构体的phases数组中添加ngx_http_handler_pt赴理方法来实现，而第二种是本阶段独有的，把希望处理请求的
ngx_http_handler_pt方法设置到location相关的ngx_http_core_loc_conf_t结构体的handler指针中，这正是第3章中mytest例子的用法。
*/ //CONTENT_PHASE阶段的处理回调函数ngx_http_handler_pt比较特殊，见ngx_http_core_content_phase 
typedef ngx_int_t (*ngx_http_handler_pt)(ngx_http_request_t *r); //参数r包含了哪些Nginx已经解析完的用户请求信息

/* 注意ngx_http_event_handler_pt和ngx_event_handler_pt的区别 */
typedef void (*ngx_http_event_handler_pt)(ngx_http_request_t *r);

/*
POST / HTTP/1.1
Host: www.baidu.com
User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.7.6)
Gecko/20050225 Firefox/1.0.1
Content-Type: application/x-www-form-urlencoded
Content-Length: 40
Connection: Keep-Alive
name=Professional%20Ajax&publisher=Wiley

*/
/*
请求的所有信息（如方法、URI、协议版本号和头部等）都可以在传入的ngx_http_request_t类型参数r中取得。
ngx_http_request_t结构体的内容很多，本节不会探讨ngx_http_request_t中所有成员的意义
*/
/*
ngx_http_core_main_conf_t->variabels数组成员的结构式ngx_http_variable_s， ngx_http_request_s->variabels数组成员结构是
ngx_variable_value_t这两个结构的关系很密切，一个所谓变量，一个所谓变量值
*/

/*
对于每个ngx_http_request_t请求来说，只能访问一个上游服务器，但对于一个客户端请求来说，可以派生出许多子请求，任何一个子请求都
可以访问一个上游服务器，这些子请求的结果组合起来就可以使来自客户端的请求处理复杂的业务。
*/
//ngx_http_parse_request_line解析请求行， ngx_http_process_request_headers解析头部行(请求头部)
//请求的所有信息都可以都可以在ngx_http_request_s结构获取到
struct ngx_http_request_s { //当接收到客户端请求数据后，调用ngx_http_create_request中创建并赋值
    uint32_t                          signature;         /* "HTTP" */ 

    //在接收到客户端数据后，会创建一个ngx_http_request_s，其connection成员指向对应的accept成功后获取到的连接信息ngx_connection_t，见ngx_http_create_request
    ngx_connection_t                 *connection; //这个请求对应的客户端连接  如果该r是子请求，起connection成员指向顶层root父请求的ngx_connection_t

    /*
    ctx与ngx_http_conf_ctxt结构的3个数组成员非常相似，它们都
    表示指向void指针的数组。HTTP框架就是在ctx数组中保存所有HTTP模块上下文结构体的指针的,所有模块的请求上下文空间在
    ngx_http_create_request中创建。获取和设置分别在ngx_http_get_module_ctx和ngx_http_set_ctx，为每个请求创建ngx_http_request_s的时候
    都会为该请求的ctx[]为所有的模块创建一个指针，也就是每个模块在ngx_http_request_s中有一个ctx
    */ //在应答完后，在ngx_http_filter_finalize_request会把ctx指向的空间全部清0  参考4.5节 
    void                            **ctx; //指向存放所有HTTP模块的上下文结构体的指针数组,实际上发送给客户端的应答完成后，会把ctx全部置0
    /*
    当客户端建立连接后，并发送请求数据过来后，在ngx_http_create_request中从ngx_http_connection_t->conf_ctx获取这三个值，也就是根据客户端连接
    本端所处IP:port所对应的默认server{}块上下文，如果是以下情况:ip:port相同，单在不同的server{}块中，那么有可能客户端请求过来的时候携带的host
    头部项的server_name不在默认的server{}中，而在另外的server{}中，所以需要通过ngx_http_set_virtual_server重新获取server{}和location{}上下文配置
    例如:
        server {  #1
            listen 1.1.1.1:80;
            server_name aaa
        }

        server {   #2
            listen 1.1.1.1:80;
            server_name bbb
        }
        这个配置在ngx_http_init_connection中把ngx_http_connection_t->conf_ctx指向ngx_http_addr_conf_s->default_server,也就是指向#1,然后
        ngx_http_create_request中把main_conf srv_conf  loc_conf 指向#1,
        但如果请求行的头部的host:bbb，那么需要重新获取对应的server{} #2,见ngx_http_set_virtual_server
*/ //ngx_http_create_request和ngx_http_set_virtual_server 已经rewrite过程中(例如ngx_http_core_find_location 
//ngx_http_core_post_rewrite_phase ngx_http_internal_redirect ngx_http_internal_redirect 子请求ngx_http_subrequest)都可能对他们赋值
    void                            **main_conf; //指向请求对应的存放main级别配置结构体的指针数组
    void                            **srv_conf; //指向请求对应的存放srv级别配置结构体的指针数组   赋值见ngx_http_set_virtual_server
    void                            **loc_conf; //指向请求对应的存放loc级别配置结构体的指针数组   赋值见ngx_http_set_virtual_server

    /*
     在接收完HTTP头部，第一次在业务上处理HTTP请求时，HTTP框架提供的处理方法是ngx_http_process_request。但如果该方法无法一次处
     理完该请求的全部业务，在归还控制权到epoll事件模块后，该请求再次被回调时，将通过ngx_http_request_handler方法来处理，而这个
     方法中对于可读事件的处理就是调用read_event_handler处理请求。也就是说，HTTP模块希望在底层处理请求的读事件时，重新实现read_event_handler方法

     //在读取客户端来的包体时，赋值为ngx_http_read_client_request_body_handler
     丢弃客户端的包体时，赋值为ngx_http_discarded_request_body_handler
     */ //注意ngx_http_upstream_t和ngx_http_request_t都有该成员 分别在ngx_http_request_handler和ngx_http_upstream_handler中执行
    ngx_http_event_handler_pt         read_event_handler;  

    /* 与read_event_handler回调方法类似，如果ngx_http_request_handler方法判断当前事件是可写事件，则调用write_event_handler处理请求 */
    /*请求行和请求头部解析完成后，会在ngx_http_handler中赋值为ngx_http_core_run_phases
       当发送响应的时候，如果一次没有发送完，则设在为ngx_http_writer
     */ //注意ngx_http_upstream_t和ngx_http_request_t都有该成员 分别在ngx_http_request_handler和ngx_http_upstream_handler中执行
     //如果采用buffer方式缓存后端包体，则在发送包体给客户端浏览器的时候，会把客户端连接的write_e_hand置为ngx_http_upstream_process_downstream
     //在触发epoll_in的同时也会触发epoll_out，从而会执行该函数
    ngx_http_event_handler_pt         write_event_handler;//父请求重新激活后的回调方法

#if (NGX_HTTP_CACHE)
//通过ngx_http_upstream_cache_get获取
    ngx_http_cache_t                 *cache;//在客户端请求过来后，在ngx_http_upstream_cache->ngx_http_file_cache_new中赋值r->caceh = ngx_http_cache_t
#endif

    /* 
    如果没有使用upstream机制，那么ngx_http_request_t中的upstream成员是NULL空指针,在ngx_http_upstream_create中创建空间
    */
    ngx_http_upstream_t              *upstream; //upstream机制用到的结构体
    ngx_array_t                      *upstream_states; //创建空间和赋值见ngx_http_upstream_init_request
                                         /* of ngx_http_upstream_state_t */

    /*
    表示这个请求的内存池，在ngx_http_free_request方法中销毁。它与ngx_connection-t中的内存池意义不同，当请求释放时，TCP连接可能并
    没有关闭，这时请求的内存池会销毁，但ngx_connection_t的内存池并不会销毁
     */
    ngx_pool_t                       *pool;
    //其中，header_in指向Nginx收到的未经解析的HTTP头部，这里暂不关注它（header_in就是接收HTTP头部的缓冲区）。 header_in存放请求行，headers_in存放头部行
    //请求行和请求头部内容都在该buffer中
    ngx_buf_t                        *header_in;//用于接收HTTP请求内容的缓冲区，主要用于接收HTTP头部，该指针指向ngx_connection_t->buffer

    //类型的headers_in则存储已经解析过的HTTP头部。
    /*常用的HTTP头部信息可以通过r->headers_in获取，不常用的HTTP头部则需要遍历r->headers_in.headers来遍历获取*/
/*
 ngx_http_process_request_headers方法在接收、解析完HTTP请求的头部后，会把解析完的每一个HTTP头部加入到headers_in的headers链表中，同时会构造headers_in中的其他成员
 */ //参考ngx_http_headers_in，通过该数组中的回调hander来存储解析到的请求行name:value中的value到headers_in的响应成员中，见ngx_http_process_request_headers
    //注意:在需要把客户端请求头发送到后端的话，在请求头后面可能添加有HTTP_相关变量，例如fastcgi，见ngx_http_fastcgi_create_request
    ngx_http_headers_in_t             headers_in; //http头部行解析后的内容都由该成员存储  header_in存放请求行，headers_in存放头部行
    //只要指定headers_out中的成员，就可以在调用ngx_http_send_header时正确地把HTTP头部发出
    //HTTP模块会把想要发送的HTTP响应信息放到headers_out中，期望HTTP框架将headers_out中的成员序列化为HTTP响应包发送给用户
    ngx_http_headers_out_t            headers_out; 
    //如果是upstream赋值的来源是后端服务器会有的头部行中拷贝，参考ngx_http_upstream_headers_in中的copy_handler

/*
接收完请求的包体后，可以在r->request_body->temp_file->file中获取临时文件（假定将r->request_body_in_file_only标志位设为1，那就一定可以
在这个变量获取到包体。）。file是一个ngx_file_t类型。这里，我们可以从
r->request_body->temp_file->file.name中获取Nginx接收到的请求包体所在文件的名称（包括路径）。
*/ //在ngx_http_read_client_request_body中分配存储空间 读取的客户端包体存储在r->request_body->bufs链表和临时文件r->request_body->temp_file中 ngx_http_read_client_request_body
//读取客户包体即使是存入临时文件中，当所有包体读取完毕后(见ngx_http_do_read_client_request_body)，还是会让r->request_body->bufs指向文件中的相关偏移内存地址
//向上游发送包体u->request_bufs(ngx_http_fastcgi_create_request),接收客户端的包体在r->request_body
    ngx_http_request_body_t          *request_body; //接收HTTP请求中包体的数据结构，为NULL表示还没有分配空间
    //min(lingering_time,lingering_timeout)这段时间内可以继续读取数据，如果客户端有发送数据过来，见ngx_http_set_lingering_close
    time_t                            lingering_time; //延迟关闭连接的时间
    //ngx_http_request_t结构体中有两个成员表示这个请求的开始处理时间：start sec成员和start msec成员
    /*
     当前请求初始化时的时间。start sec是格林威治时间1970年1月1日凌晨0点0分0秒到当前时间的秒数。如果这个请求是子请求，则该时间
     是子请求的生成时间；如果这个请求是用户发来的请求，则是在建立起TCP连接后，第一次接收到可读事件时的时间
     */
    time_t                            start_sec;
    ngx_msec_t                        start_msec;//与start_sec配合使用，表示相对于start_set秒的毫秒偏移量



//以下9个成员都是ngx_http_proces s_request_line方法在接收、解析HTTP请求行时解析出的信息
/*
注意　Nginx中对内存的控制相当严格，为了避免不必要的内存开销，许多需要用到的成员都不是重新分配内存后存储的，而是直接指向用户请求中的相应地址。
例如，method_name.data、request_start这两个指针实际指向的都是同一个地址。而且，因为它们是简单的内存指针，不是指向字符串的指针，所以，在大部分情况下，都不能将这些u_char*指针当做字符串使用。
*/ //NGX_HTTP_GET | NGX_HTTP_HEAD等,为NGX_HTTP_HEAD表示只需要发送HTTP头部字段
    ngx_uint_t                        method; //对应客户端请求中请求行的请求方法GET、POS等，取值见NGX_HTTP_GET,也可以用下面的method_name进行字符串比较
/*
http_protocol指向用户请求中HTTP的起始地址。
http_version是Nginx解析过的协议版本，它的取值范围如下：
#define NGX_HTTP_VERSION_9                 9
#define NGX_HTTP_VERSION_10                1000
#define NGX_HTTP_VERSION_11                1001
建议使用http_version分析HTTP的协议版本。
最后，使用request_start和request_end可以获取原始的用户请求行。
*/
    ngx_uint_t                        http_version;//http_version是Nginx解析过的协议版本，它的取值范围如下：

    ngx_str_t                         request_line; //请求行内容


/*
2016/01/07 12:38:01[      ngx_http_process_request_line,  1002]  [debug] 20090#20090: *14 http request line: "GET /download/nginx-1.9.2.rar?st=xhWL03HbtjrojpEAfiD6Mw&e=1452139931 HTTP/1.1"
2016/01/07 12:38:01[       ngx_http_process_request_uri,  1223]  [debug] 20090#20090: *14 http uri: "/download/nginx-1.9.2.rar"
2016/01/07 12:38:01[       ngx_http_process_request_uri,  1226]  [debug] 20090#20090: *14 http args: "st=xhWL03HbtjrojpEAfiD6Mw&e=1452139931"
2016/01/07 12:38:01[       ngx_http_process_request_uri,  1229]  [debug] 20090#20090: *14 http exten: "rar"
*/

    
//ngx_str_t类型的uri成员指向用户请求中的URI。同理，u_char*类型的uri_start和uri_end也与request_start、method_end的用法相似，唯一不
//同的是，method_end指向方法名的最后一个字符，而uri_end指向URI结束后的下一个地址，也就是最后一个字符的下一个字符地址（HTTP框架的行为），
//这是大部分u_char*类型指针对“xxx_start”和“xxx_end”变量的用法。
    //http://10.135.10.167/mytest中的/mytest  http://10.135.10.167/mytest?abc?ttt中的/mytest  
    //同时"GET /mytest?abc?ttt HTTP/1.1"中的mytest和uri中的一样    
    ngx_str_t                         uri; 
    //arg指向用户请求中的URL参数。  http://10.135.10.167/mytest?abc?ttt中的abc?ttt   
    //同时"GET /mytest?abc?ttt HTTP/1.1"中的mytest?abc?ttt和uri中的一样    

 /*把请求中GET /download/nginx-1.9.2.rar?st=xhWL03HbtjrojpEAfiD6Mw&e=1452139931 HTTP/1.1的st和e形成变量$arg_st #arg_e，value分别
为xhWL03HbtjrojpEAfiD6Mw 1452139931即$arg_st=xhWL03HbtjrojpEAfiD6Mw，#arg_e=1452139931，见ngx_http_arg */
    ngx_str_t                         args;
    /*
    ngx_str_t类型的extern成员指向用户请求的文件扩展名。例如，在访问“GET /a.txt HTTP/1.1”时，extern的值是{len = 3, data = "txt"}，
    而在访问“GET /a HTTP/1.1”时，extern的值为空，也就是{len = 0, data = 0x0}。
    uri_ext指针指向的地址与extern.data相同。
    */
    ngx_str_t                         exten; //http://10.135.10.167/mytest/ac.txt中的txt
/*
url参数中出现+、空格、=、%、&、#等字符的解决办法 
url出现了有+，空格，/，?，%，#，&，=等特殊符号的时候，可能在服务器端无法获得正确的参数值，如何是好？
解决办法
将这些字符转化成服务器可以识别的字符，对应关系如下：
URL字符转义

用其它字符替代吧，或用全角的。

+    URL 中+号表示空格                      %2B   
空格 URL中的空格可以用+号或者编码           %20 
/   分隔目录和子目录                        %2F     
?    分隔实际的URL和参数                    %3F     
%    指定特殊字符                           %25     
#    表示书签                               %23     
&    URL 中指定的参数间的分隔符             %26     
=    URL 中指定参数的值                     %3D
*/
//unparsed_uri表示没有进行URL解码的原始请求。例如，当uri为“/a b”时，unparsed_uri是“/a%20b”（空格字符做完编码后是%20）。
    ngx_str_t                         unparsed_uri;//参考:为什么要对URI进行编码:
    ngx_str_t                         method_name;//见method   GET  POST等
    ngx_str_t                         http_protocol;//GET /sample.jsp HTTP/1.1  中的HTTP/1.1


/* 当ngx_http_header_filter方法无法一次性发送HTTP头部时，将会有以下两个现象同时发生:请求的out成员中将会保存剩余的响应头部,见ngx_http_header_filter */    
/* 表示需要发送给客户端的HTTP响应。out中保存着由headers_out中序列化后的表示HTTP头部的TCP流。在调用ngx_http_output_filter方法后，
out中还会保存待发送的HTTP包体，它是实现异步发送HTTP响应的关键 */
    ngx_chain_t                      *out;//ngx_http_write_filter把in中的数据拼接到out后面，然后调用writev发送，没有发送完
/* 当前请求既可能是用户发来的请求，也可能是派生出的子请求，而main则标识一系列相关的派生子请
求的原始请求，我们一般可通过main和当前请求的地址是否相等来判断当前请求是否为用户发来的原始请求 */
    //main成员始终指向一系列有亲缘关系的请求中的唯一的那个原始请求,初始赋值见ngx_http_create_request
    //客户端的建立连接的时候r->main =r(ngx_http_create_request),如果是创建子请求，sr->main = r->main(ngx_http_subrequest)子请求->main=父请求r
    /* 主请求保存在main字段中，这里其实就是最上层跟请求，例如当前是四层子请求，则main始终指向第一层父请求，
        而不是第三次父请求，parent指向第三层父请求 */  
    ngx_http_request_t               *main;//可以参考ngx_http_core_access_phase
    ngx_http_request_t               *parent;//当前请求的父请求。注意，父请求未必是原始请求 赋值见ngx_http_subrequest

/*
    当派生一个子请求访问第三方服务附，如果只是希望接收到完整的响应后在Nginx中解析、处理，那么这里就不需要postpone模块，
    如果原始请求派生出许多子请求，并且希望将所有子请求的响应依次转发给客户端，当然，这里的“依次”就是按照创建
子请求的顺序来发送响应，这时，postpone模块就有了“用武之地”。Nginx中的所有请求都是异步执行的，后创建的子请求可能优先执行，这样
转发到客户端的响应就会产生混乱。而postpone模块会强制地把待转发的响应包体放在一个链表中发送，只有优先转发的子请求结束后才会开始
转发下一个子请求中的响应。下面介绍一下它是如何实现的。每个请求的ngx_http_request_t结构体中都有一个postponed成员：

    从上述代码可以看出，多个ngx_http_postponed_request_t之间使用next指针连接成一个单向链表。ngx_http_postponed_request_t中
的out成员是ngx_chain_t结构，它指向的是来自上游的、将要转发给下游的响应包体。
    每当使用ngx_http_output_filter方法（反向代理模块也使用该方法转发响应）向下游昀客户端发送响应包体时，都会调用到
ngx_http_postpone_filter_module过滤模块处理这段要发送的包体。下面看一下过滤包体的ngx_http_postpone_filter方法
*/ //ngx_http_subrequest中赋值，表示对应的子请求r，该结构可以表示子请求信息，通过request指向对应的子请求，也可以表示单纯的数据信息in链表，见ngx_http_postpone_filter_add
    //postponed删除在ngx_http_finalize_request     当客户端请求需要通过多个subrequest访问后端的时候，就需要对这多个后端的应答进行合适的顺序整理才能发往客户端，就和twemproxy的mget类似
    ngx_http_postponed_request_t     *postponed; //与subrequest子请求相关的功能  postponed中数据依次发送参考ngx_http_postpone_filter方法
    ngx_http_post_subrequest_t       *post_subrequest;/* 保存回调handler及数据，在子请求执行完，将会调用 */  
/* 所有的子请求都是通过posted_requests这个单链表来链接起来的，执行post子请求时调用的
ngx_http_run_posted_requests方法就是通过遍历该单链表来执行子请求的 */ 
//ngx_http_post_request中创建ngx_http_posted_request_t空间  
//ngx_http_post_request将该子请求挂载在主请求的posted_requests链表队尾，在ngx_http_run_posted_requests中执行
    ngx_http_posted_request_t        *posted_requests; //通过posted_requests就把各个子请求以单向链表的数据结构形式组织起来

/*
全局的ngx_http_phase_engine_t结构体中定义了一个ngx_http_phase_handler_t回调方法组成的数组，而phase_handler成员则与该数组配合使用，
表示请求下次应当执行以phase_handler作为序号指定的数组中的回调方法。HTTP框架正是以这种方式把各个HTTP摸块集成起来处理请求的
*///phase_handler实际上是该阶段的处理方法函数在ngx_http_phase_engine_t->handlers数组中的位置
    ngx_int_t                         phase_handler; 
    //表示NGX HTTP CONTENT PHASE阶段提供给HTTP模块处理请求的一种方式，content handler指向HTTP模块实现的请求处理方法,在ngx_http_core_content_phase中执行
    //ngx_http_proxy_handler  ngx_http_redis2_handler  ngx_http_fastcgi_handler等
    ngx_http_handler_pt               content_handler; ////在ngx_http_update_location_config中赋值给r->content_handler = clcf->handler;
/*
    在NGX_HTTP_ACCESS_PHASE阶段需要判断请求是否具有访问权限时，通过access_code来传递HTTP模块的handler回调方法的返回值，如果access_code为0，
则表示请求具备访问权限，反之则说明请求不具备访问权限

    NGXHTTPPREACCESSPHASE、NGX_HTTP_ACCESS_PHASE、NGX HTTPPOST_ACCESS_PHASE，很好理解，做访问权限检查的前期、中期、后期工作，
其中后期工作是固定的，判断前面访问权限检查的结果（状态码存故在字段r->access_code内），如果当前请求没有访问权限，那么直接返回状
态403错误，所以这个阶段也无法去挂载额外的回调函数。
*/
    ngx_uint_t                        access_code; //赋值见ngx_http_core_access_phase
    /*
    ngx_http_core_main_conf_t->variables数组成员的结构式ngx_http_variable_s， ngx_http_request_s->variables数组成员结构是ngx_variable_value_t,
    这两个结构的关系很密切，一个所谓变量，一个所谓变量值

    r->variables这个变量和cmcf->variables是一一对应的，形成var_ name与var_value对，所以两个数组里的同一个下标位置元素刚好就是
相互对应的变量名和变量值，而我们在使用某个变量时总会先通过函数ngx_http_get_variable_index获得它在变量名数组里的index下标，也就是变
量名里的index字段值，然后利用这个index下标进而去变量值数组里取对应的值
    */ //分配的节点数见ngx_http_create_request，和ngx_http_core_main_conf_t->variables一一对应
    //变量ngx_http_script_var_code_t->index表示Nginx变量$file在ngx_http_core_main_conf_t->variables数组内的下标，对应每个请求的变量值存储空间就为r->variables[code->index],参考ngx_http_script_set_var_code
    ngx_http_variable_value_t        *variables; //注意和ngx_http_core_main_conf_t->variables的区别

#if (NGX_PCRE)
    /*  
     例如正则表达式语句re.name= ^(/download/.*)/media/(.*)/tt/(.*)$，  s=/download/aa/media/bdb/tt/ad,则他们会匹配，同时匹配的
     变量数有3个，则返回值为3+1=4,如果不匹配则返回-1

     这里*2是因为获取前面例子中的3个变量对应的值需要成对使用r->captures，参考ngx_http_script_copy_capture_code等
     */
    ngx_uint_t                        ncaptures; //赋值见ngx_http_regex_exec   //最大的$n*2
    int                              *captures; //每个不同的正则解析之后的结果，存放在这里。$1,$2等
    u_char                           *captures_data; //进行正则表达式匹配的原字符串，例如http://10.135.2.1/download/aaa/media/bbb.com中的/download/aaa/media/bbb.com
#endif

/* limit_rate成员表示发送响应的最大速率，当它大于0时，表示需要限速。limit rate表示每秒可以发送的字节数，超过这个数字就需要限速；
然而，限速这个动作必须是在发送了limit_rate_after字节的响应后才能生效（对于小响应包的优化设计） */
//实际最后通过ngx_writev_chain发送数据的时候，还会限制一次
    size_t                            limit_rate; //限速的相关计算方法参考ngx_http_write_filter
    size_t                            limit_rate_after;

    /* used to learn the Apache compatible response length without a header */
    size_t                            header_size; //所有头部行内容之和，可以参考ngx_http_header_filter

    off_t                             request_length; //HTTP请求的全部长度，包括HTTP包体

    ngx_uint_t                        err_status; //错误码，取值为NGX_HTTP_BAD_REQUEST等

    //当连接建立成功后，当收到客户端的第一个请求的时候会通过ngx_http_wait_request_handler->ngx_http_create_request创建ngx_http_request_t
    //同时把r->http_connection指向accept客户端连接成功时候创建的ngx_http_connection_t，这里面有存储server{}上下文ctx和server_name等信息
    //该ngx_http_request_t会一直有效，除非关闭连接。因此该函数只会调用一次，也就是第一个客户端请求报文过来的时候创建，一直持续到连接关闭
    //该结构存储了服务器端接收客户端连接时，服务器端所在的server{]上下文ctx  server_name等配置信息
    ngx_http_connection_t            *http_connection; //存储ngx_connection_t->data指向的ngx_http_connection_t，见ngx_http_create_request
#if (NGX_HTTP_SPDY)
    ngx_http_spdy_stream_t           *spdy_stream;
#endif

    ngx_http_log_handler_pt           log_handler;
    //在这个请求中如果打开了某些资源，并需要在请求结束时释放，那么都需要在把定义的释放资源方法添加到cleanup成员中
    /* 
    如果没有需要清理的资源，则cleanup为空指针，否则HTTP模块可以向cleanup中以单链表的形式无限制地添加ngx_http_cleanup_t结构体，
    用以在请求结束时释放资源 */
    ngx_http_cleanup_t               *cleanup;
    //默认值r->subrequests = NGX_HTTP_MAX_SUBREQUESTS + 1;见ngx_http_create_request
    unsigned                          subrequests:8; //该r最多有多少个子请求

/*
在阅读HTTP反向代理模块(ngx_http_proxy_module)源代码时，会发现它并没有调用r->main->count++，其中proxy模块是这样启动upstream机制的：
ngx_http_read_client_request_body(r，ngx_http_upstream_init);，这表示读取完用户请求的HTTP包体后才会调用ngx_http_upstream_init方法
启动upstream机制。由于ngx_http_read_client_request_body的第一行有效语句是r->maln->count++，所以HTTP反向代理模块不能
再次在其代码中执行r->main->count++。

这个过程看起来似乎让人困惑。为什么有时需要把引用计数加1，有时却不需要呢？因为ngx_http_read- client_request_body读取请求包体是
一个异步操作（需要epoll多次调度方能完成的可称其为异步操作），ngx_http_upstream_init方法启用upstream机制也是一个异步操作，因此，
从理论上来说，每执行一次异步操作应该把引用计数加1，而异步操作结束时应该调用ngx_http_finalize_request方法把引用计数减1。另外，
ngx_http_read_client_request_body方法内是加过引用计数的，而ngx_http_upstream_init方法内却没有加过引用计数（或许Nginx将来会修改
这个问题）。在HTTP反向代理模块中，它的ngx_http_proxy_handler方法中用“ngx_http_read- client_request_body(r，ngx_http_upstream_init);”
语句同时启动了两个异步操作，注意，这行语句中只加了一次引用计数。执行这行语句的ngx_http_proxy_handler方法返回时只调用
ngx_http_finalize_request方法一次，这是正确的。对于mytest模块也一样，务必要保证对引用计数的增加和减少是配对进行的。
*/
/*
表示当前请求的引用次数。例如，在使用subrequest功能时，依附在这个请求上的子请求数目会返回到count上，每增加一个子请求，count数就要加1。
其中任何一个子请求派生出新的子请求时，对应的原始请求（main指针指向的请求）的count值都要加1。又如，当我们接收HTTP包体时，由于这也是
一个异步调用，所以count上也需要加1，这样在结束请求时，就不会在count引用计数未清零时销毁请求
*/
    unsigned                          count:8; //应用计数   ngx_http_close_request中-1
    /* 如果AIO上下文中还在处理这个请求，blocked必然是大于0的，这时ngx_http_close_request方法不能结束请求 
        ngx_http_copy_aio_handler会自增，当内核把数据发送出去后会在ngx_http_copy_aio_event_handler自剪
     */
    unsigned                          blocked:8; //阻塞标志位，目前仅由aio使用  为0，表示没有HTTP模块还需要处理请求
    //ngx_http_copy_aio_handler handler ngx_http_copy_aio_event_handler执行后，会置回到0   
    //ngx_http_copy_thread_handler ngx_http_copy_thread_event_handler置0
    //ngx_http_cache_thread_handler置1， ngx_http_cache_thread_event_handler置0
    //ngx_http_file_cache_aio_read中置1，
    unsigned                          aio:1;  //标志位，为1时表示当前请求正在使用异步文件IO

    unsigned                          http_state:4; //赋值见ngx_http_state_e中的成员

    /* URI with "/." and on Win32 with "//" */
    unsigned                          complex_uri:1;

    /* URI with "%" */
    unsigned                          quoted_uri:1;

    /* URI with "+" */
    unsigned                          plus_in_uri:1;

    /* URI with " " */
    unsigned                          space_in_uri:1; //uri中是否带有空格

    unsigned                          invalid_header:1; //头部行解析不正确，见ngx_http_parse_header_line

    unsigned                          add_uri_to_alias:1;
    unsigned                          valid_location:1; //ngx_http_handler中置1
    //如果有rewrite 内部重定向 uri带有args等会直接置0，否则如果uri中有空格会置1
    unsigned                          valid_unparsed_uri:1;//r->valid_unparsed_uri = r->space_in_uri ? 0 : 1;

    /*
    将uri_changed设置为0后，也就标志说URL没有变化，那么，在ngx_http_core_post_rewrite_phase中就不会执行里面的if语句，也就不会
    再次走到find config的过程了，而是继续处理后面的。不然正常情况，rewrite成功后是会重新来一次的，相当于一个全新的请求。
     */ // 例如rewrite   ^.*$ www.galaxywind.com last;就会多次执行rewrite       ngx_http_script_regex_start_code中置1
    unsigned                          uri_changed:1; //标志位，为1时表示URL发生过rewrite重写  只要不是rewrite xxx bbb sss;aaa不是break结束都会置1
    //表示使用rewrite重写URL的次数。因为目前最多可以更改10次，所以uri_changes初始化为11，而每重写URL -次就把uri_changes减1，
    //一旦uri_changes等于0，则向用户返回失败
    unsigned                          uri_changes:4; //NGX_HTTP_MAX_URI_CHANGES + 1;

    unsigned                          request_body_in_single_buf:1;//client_body_in_single_buffer on | off;设置
    //置1包体需要存入临时文件中  如果request_body_no_buffering为1表示不用缓存包体，那么request_body_in_file_only也为0，因为不用缓存包体，那么就不用写到临时文件中
    /*注意:如果每次开辟的client_body_buffer_size空间都存储满了还没有读取到完整的包体，则还是会把之前读满了的buf中的内容拷贝到临时文件，参考
        ngx_http_do_read_client_request_body -> ngx_http_request_body_filter和ngx_http_read_client_request_body -> ngx_http_request_body_filter
     */
    unsigned                          request_body_in_file_only:1; //"client_body_in_file_only on |clean"设置 和request_body_no_buffering是互斥的
    unsigned                          request_body_in_persistent_file:1; //"client_body_in_file_only on"设置
    unsigned                          request_body_in_clean_file:1;//"client_body_in_file_only clean"设置
    unsigned                          request_body_file_group_access:1; //是否有组权限，如果有一般为0600
    unsigned                          request_body_file_log_level:3;
    //默认是为0的表示需要缓存客户端包体，如果request_body_no_buffering为1表示不用缓存包体，那么request_body_in_file_only也为0，因为不用缓存包体，那么就不用写到临时文件中
    unsigned                          request_body_no_buffering:1; //是否缓存HTTP包体，如果不缓存包体，和request_body_in_file_only是互斥的，见ngx_http_read_client_request_body

    /*
        upstream有3种处理上游响应包体的方式，但HTTP模块如何告诉upstream使用哪一种方式处理上游的响应包体呢？
    当请求的ngx_http_request_t结构体中subrequest_in_memory标志位为1时，将采用第1种方式，即upstream不转发响应包体
    到下游，由HTTP模块实现的input_filter方法处理包体；当subrequest_in_memory为0时，upstream会转发响应包体。当ngx_http_upstream_conf_t
    配置结构体中的buffering标志位为1时，将开启更多的内存和磁盘文件用于缓存上游的响应包体，这意味上游网速更快；当buffering
    为0时，将使用固定大小的缓冲区（就是上面介绍的buffer缓冲区）来转发响应包体。
    */
    unsigned                          subrequest_in_memory:1; //ngx_http_subrequest中赋值 NGX_HTTP_SUBREQUEST_IN_MEMORY
    unsigned                          waited:1; //ngx_http_subrequest中赋值 NGX_HTTP_SUBREQUEST_WAITED

#if (NGX_HTTP_CACHE)
    unsigned                          cached:1;//如果客户端请求过来有读到缓存文件，则置1，见ngx_http_file_cache_read  ngx_http_upstream_cache_send
#endif

#if (NGX_HTTP_GZIP)
    unsigned                          gzip_tested:1;
    unsigned                          gzip_ok:1;
    unsigned                          gzip_vary:1;
#endif

    unsigned                          proxy:1;
    unsigned                          bypass_cache:1;
    unsigned                          no_cache:1;

    /*
     * instead of using the request context data in
     * ngx_http_limit_conn_module and ngx_http_limit_req_module
     * we use the single bits in the request structure
     */
    unsigned                          limit_conn_set:1;
    unsigned                          limit_req_set:1;

#if 0
    unsigned                          cacheable:1;
#endif

    unsigned                          pipeline:1;
    //如果后端发送过来的头部行中不带有Content-length:xxx 这种情况1.1版本HTTP直接设置chunked为1， 见ngx_http_chunked_header_filter
    //如果后端带有Transfer-Encoding: chunked会置1
    unsigned                          chunked:1; //chunk编码方式组包实际组包过程参考ngx_http_chunked_body_filter
    //当下游的r->method == NGX_HTTP_HEAD请求方法只请求头部行，则会在ngx_http_header_filter中置1
    unsigned                          header_only:1; //表示是否只有行、头部，没有包体  ngx_http_header_filter中置1
    //在1.0以上版本默认是长连接，1.0以上版本默认置1，如果在请求头里面没有设置连接方式，见ngx_http_handler
    //标志位，为1时表示当前请求是keepalive请求  1长连接   0短连接  长连接时间通过请求头部的Keep-Alive:设置，参考ngx_http_headers_in_t
    unsigned                          keepalive:1;  //赋值见ngx_http_handler
//延迟关闭标志位，为1时表示需要延迟关闭。例如，在接收完HTTP头部时如果发现包体存在，该标志位会设为1，而放弃接收包体时则会设为o
    unsigned                          lingering_close:1; 
    //如果discard_body为1，则证明曾经执行过丢弃包体的方法，现在包体正在被丢弃中，见ngx_http_read_client_request_body
    unsigned                          discard_body:1;//标志住，为1时表示正在丢弃HTTP请求中的包体
    unsigned                          reading_body:1; //标记包体还没有读完，需要继续读取包体，见ngx_http_read_client_request_body

    /* 在这一步骤中，把phase_handler序号设为server_rewrite_index，这意味着无论之前执行到哪一个阶段，马上都要重新从NGX_HTTP_SERVER_REWRITE_PHASE
阶段开始再次执行，这是Nginx的请求可以反复rewrite重定向的基础。见ngx_http_handler */ //ngx_http_internal_redirect置1
//内部重定向是从NGX_HTTP_SERVER_REWRITE_PHASE处继续执行(ngx_http_internal_redirect)，而重新rewrite是从NGX_HTTP_FIND_CONFIG_PHASE处执行(ngx_http_core_post_rewrite_phase)
    unsigned                          internal:1;//t标志位，为1时表示请求的当前状态是在做内部跳转， 
    unsigned                          error_page:1; //默认0，在ngx_http_special_response_handler中可能置1
    unsigned                          filter_finalize:1;
    unsigned                          post_action:1;//ngx_http_post_action中置1 默认为0，除非post_action XXX配置
    unsigned                          request_complete:1;
    unsigned                          request_output:1;//表示有数据需要往客户端发送，ngx_http_copy_filter中置1
    //为I时表示发送给客户端的HTTP响应头部已经发送。在调用ngx_http_send_header方法后，若已经成功地启动响应头部发送流程，
    //该标志位就会置为1，用来防止反复地发送头部
    unsigned                          header_sent:1;
    unsigned                          expect_tested:1;
    unsigned                          root_tested:1;
    unsigned                          done:1;
    unsigned                          logged:1;

    unsigned                          buffered:4;//表示缓冲中是否有待发送内容的标志位，参考ngx_http_copy_filter

    unsigned                          main_filter_need_in_memory:1;
    unsigned                          filter_need_in_memory:1;
    unsigned                          filter_need_temporary:1;
    unsigned                          allow_ranges:1;  //支持断点续传 参考3.8.3节
    unsigned                          single_range:1;
    //
    unsigned                          disable_not_modified:1; //r->disable_not_modified = !u->cacheable;因此默认为0

#if (NGX_STAT_STUB)
    unsigned                          stat_reading:1;
    unsigned                          stat_writing:1;
#endif

    /* used to parse HTTP headers */ //状态机解析HTTP时使用state来表示当前的解析状态
    ngx_uint_t                        state; //解析状态，见ngx_http_parse_header_line
    //header_hash为Accept-Language:zh-cn中Accept-Language所有字符串做hash运算的结果
    ngx_uint_t                        header_hash; //头部行中一行所有内容计算ngx_hash的结构，参考ngx_http_parse_header_line
    //lowcase_index为Accept-Language:zh-cn中Accept-Language字符数，也就是15个字节
    ngx_uint_t                        lowcase_index; // 参考ngx_http_parse_header_line
    //存储Accept-Language:zh-cn中的Accept-Language字符串到lowcase_header。如果是AAA_BBB:CCC,则该数组存储的是_BBB
    u_char                            lowcase_header[NGX_HTTP_LC_HEADER_LEN]; //http头部内容，不包括应答行或者请求行，参考ngx_http_parse_header_line

/*
例如:Accept:image/gif.image/jpeg,** 
Accept对应于key，header_name_start header_name_end分别指向这个Accept字符串的头和尾
image/gif.image/jpeg,** 为value部分，header_start header_end分别对应value的头和尾，可以参考mytest_upstream_process_header
*/
    //header_name_start指向Accept-Language:zh-cn中的A处
    u_char                           *header_name_start; //解析到的一行http头部行中的一行的name开始处 //赋值见ngx_http_parse_header_line
    //header_name_start指向Accept-Language:zh-cn中的:处
    u_char                           *header_name_end; //解析到的一行http头部行中的一行的name的尾部 //赋值见ngx_http_parse_header_line
    u_char                           *header_start;//header_start指向Accept-Language:zh-cn中的z字符处
    u_char                           *header_end;//header_end指向Accept-Language:zh-cn中的末尾换行处

    /*
     * a memory that can be reused after parsing a request line
     * via ngx_http_ephemeral_t
     */

//ngx_str_t类型的uri成员指向用户请求中的URI。同理，u_char*类型的uri_start和uri_end也与request_start、request_end的用法相似，唯一不
//同的是，method_end指向方法名的最后一个字符，而uri_end指向URI结束后的下一个地址，也就是最后一个字符的下一个字符地址（HTTP框架的行为），
//这是大部分u_char*类型指针对“xxx_start”和“xxx_end”变量的用法。
    u_char                           *uri_start;
    u_char                           *uri_end;

/*
ngx_str_t类型的extern成员指向用户请求的文件扩展名。例如，在访问“GET /a.txt HTTP/1.1”时，extern的值是{len = 3, data = "txt"}，
而在访问“GET /a HTTP/1.1”时，extern的值为空，也就是{len = 0, data = 0x0}。
uri_ext指针指向的地址与extern.data相同。
*/ //GET /sample.jsp HTTP/1.1 后面的文件如果有.字符，则指向该.后面的jsp字符串，表示文件扩展名
    u_char                           *uri_ext;
    //"GET /aaaaaaaa?bbbb.txt HTTP/1.1"中的bbb.txt字符串头位置处
    u_char                           *args_start;//args_start指向URL参数的起始地址，配合uri_end使用也可以获得URL参数。

    /* 通过request_start和request_end可以获得用户完整的请求行 */
    u_char                           *request_start; //请求行开始处
    u_char                           *request_end;  //请求行结尾处
    u_char                           *method_end;  //GET  POST字符串结尾处
    u_char                           *schema_start;
    u_char                           *schema_end;
    u_char                           *host_start;
    u_char                           *host_end;
    u_char                           *port_start;
    u_char                           *port_end;

    // HTTP/1.1前面的1代表major，后面的1代表minor
    unsigned                          http_minor:16;
    unsigned                          http_major:16;
};


typedef struct {
    ngx_http_posted_request_t         terminal_posted_request;
} ngx_http_ephemeral_t;


#define ngx_http_ephemeral(r)  (void *) (&r->uri_start)


extern ngx_http_header_t       ngx_http_headers_in[];
extern ngx_http_header_out_t   ngx_http_headers_out[];


#define ngx_http_set_log_request(log, r)                                      \
    ((ngx_http_log_ctx_t *) log->data)->current_request = r


#endif /* _NGX_HTTP_REQUEST_H_INCLUDED_ */
