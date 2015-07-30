/*
主模块（Main Module）


·摘要
包含一些Nginx的基本控制功能
·指令
daemon
语法：daemon on | off
默认值：on

daemon off;
生产环境中不要使用"daemon"和"master_process"指令，这些指令仅用于开发调试。虽然可以使用daemon off在生产环境中，但对性能提升没有任何帮助，但是在生产环境中永远不要使用master_process off。 
env
语法：env VAR|VAR=VALUE
默认值：TZ
使用字段：main
这个命令允许其限定一些环境变量的值，在以下的情况下会创建或修改变量的值：

·在不停机情况下升级、增加或删除一些模块时继承的变量
·使用嵌入式perl模块
·使用工作中的进程，必须记住，某些类似系统库的行为管理仅在变量初始化时频繁地使用库文件，即仍然可以用之前给定的命令设置，上面提到
的零停机更新文件是一个例外（此句不知怎么翻，原文：for use by working processes. However it is necessary to keep in mind, that 
management of behaviour of system libraries in a similar way probably not always as frequently libraries use variables only during 
initialization, that is still before they can be set by means of the given instruction. Exception to it is the above described updating 
an executed file with zero downtime. ）
如果没有明确的定义TZ的值,默认情况下它总是继承的，并且内置的Perl模块总是可以使用TZ的值。
例：

env  MALLOC_OPTIONS;
env  PERL5LIB=/data/site/modules;
env  OPENSSL_ALLOW_PROXY_CERTS=1;debug_points
语法：debug_points [stop | abort] 
默认值：none（无）

debug_points stop;在Nginx内部有很多断言，如果debug_points的值设为stop时，那么触发断言时将停止Nginx并附加调试器。如果debug_point的值
设为abort,那么触发断言时将创建内核文件。
error_log
语法：error_log file [ debug | info | notice | warn | error | crit ] 
默认值：${prefix}/logs/error.log
指定Nginx服务（与FastCGI）错误日志文件位置。
每个字段的错误日志等级默认值：

1、main字段 - error
2、HTTP字段 - crit
3、server字段 - crit

Nginx支持为每个虚拟主机设置不同的错误日志文件，这一点要好于lighttpd，详细为每个虚拟主机配置不同错误日志的例子请参考：
SeparateErrorLoggingPerVirtualHost和mailing list thread on separating error logging per virtual host
如果你在编译安装Nginx时加入了--with-debug参数，你可以使用以下配置：

error_log LOGFILE [debug_core | debug_alloc | debug_mutex | debug_event | debug_http | debug_imap];注意error_log off并
不能关闭日志记录功能，而会将日志文件写入一个文件名为off的文件中，如果你想关闭错误日志记录功能，应使用以下配置：

error_log /dev/null crit;同样注意0.7.53版本，nginx在使用配置文件指定的错误日志路径前将使用编译时指定的默认日志位置，如果
运行nginx的用户对该位置没有写入权限，nginx将输出如下错误：

[alert]: could not open error log file: open() "/var/log/nginx/error.log" failed (13: Permission denied)log_not_found
语法：log_not_found on | off 
默认值：on 
使用字段：location 
这个参数指定了是否记录客户端的请求出现404错误的日志，通常用于不存在的robots.txt和favicon.ico文件，例如：

location = /robots.txt {
  log_not_found  off;
}
include
语法：include file | * 
默认值：none 
你可以包含一些其他的配置文件来完成你想要的功能。
0.4.4版本以后，include指令已经能够支持文件通配符：

include vhosts/*.conf;注意：直到0.6.7版本，这个参数包含的文件相对路径随你在编译时指定的--prefix=PATH目录而决定，默认是
/usr/local/nginx，如果你不想指定这个目录下的文件，请写绝对路径。
0.6.7版本以后指定的文件相对路径根据nginx.conf所在的目录而决定，而不是prefix目录的路径。

lock_file
语法：lock_file file 
默认值：编译时指定 

lock_file  /var/log/lock_file;Nginx使用连接互斥锁进行顺序的accept()系统调用，如果Nginx在i386,amd64,sparc64,与ppc64环境下使用
gcc,Intel C++,或SunPro C++进行编译，Nginx将采用异步互斥进行访问控制，在其他情况下锁文件会被使用。

master_process
语法：master_process on | off 
默认值：on 

master_process  off;生产环境中不要使用"daemon"和"master_process"指令，这些选项仅用于开发调试。

pid
语法：pid file 
默认值：编译时指定 

pid /var/log/nginx.pid;指定pid文件，可以使用kill命令来发送相关信号，例如你如果想重新读取配置文件，则可以使用：kill -HUP `cat /var/log/nginx.pid`

ssl_engine
语法：ssl_engine engine 
默认值：依赖于系统环境 
这里可以指定你想使用的OpenSSL引擎，你可以使用这个命令找出哪个是可用的：openssl engine -t

$ openssl engine -t
(cryptodev) BSD cryptodev engine
  [ 可用 ] 
(dynamic) Dynamic engine loading support
  [ 不可用 ] timer_resolution
语法：timer_resolution t 
默认值：none 

timer_resolution  100ms;这个参数允许缩短gettimeofday()系统调用的时间，默认情况下gettimeofday()在下列都调用完成后才会被调用：kevent(), epoll, /dev/poll, select(), poll()。
如果你需要一个比较准确的时间来记录$upstream_response_time或者$msec变量，你可能会用到timer_resolution

try_files 
语法：try_files path1 [ path2] uri 
默认值：none 
可用版本：0.7.27 
按照顺序检查存在的文件，并且返回找到的第一个文件，斜线指目录：$uri / 。如果在没有找到文件的情况下，会启用一个参数为last的内部
重定向，这个last参数“必须”被设置用来返回URL，否则会产生一个内部错误。
在代理Mongrel中使用：

location / {
  try_files /system/maintenance.html
  $uri $uri/index.html $uri.html @mongrel;
}
 
location @mongrel {
  proxy_pass http://mongrel;
}在Drupal / FastCGI中： 
location / {
  try_files $uri $uri/ @drupal;
}
 
location ~ \.php$ {
  try_files $uri @drupal;
  fastcgi_pass 127.0.0.1:8888;
  fastcgi_param  SCRIPT_FILENAME  /path/to$fastcgi_script_name;
  # other fastcgi_param
}
 
location @drupal {
  fastcgi_pass 127.0.0.1:8888;
  fastcgi_param  SCRIPT_FILENAME  /path/to/index.php;
  fastcgi_param  QUERY_STRING     q=$request_uri;
  # other fastcgi_param
}在这个例子中，这个try_files指令：

location / {
  try_files $uri $uri/ @drupal;
}等同于下列配置：

location / {
  error_page     404 = @drupal;
  log_not_found  off;
}这段：

location ~ \.php$ {
  try_files $uri @drupal;
 
  fastcgi_pass 127.0.0.1:8888;
  fastcgi_param  SCRIPT_FILENAME  /path/to$fastcgi_script_name;
  # other fastcgi_param
}指try_files在将请求提交到FastCGI服务之前检查存在的php文件。
一个在Wordpress和Joomla中的例子：

location / {
  try_files $uri $uri/ @wordpress;
}
 
location ~ \.php$ {
  try_files $uri @wordpress;
 
  fastcgi_pass 127.0.0.1:8888;
  fastcgi_param SCRIPT_FILENAME /path/to$fastcgi_script_name;
  # other fastcgi_param
}
 
location @wordpress {
    fastcgi_pass 127.0.0.1:8888;
    fastcgi_param SCRIPT_FILENAME /path/to/index.php;
    # other fastcgi_param
}user
语法：user user [group] 
默认值：nobody nobody 
如果主进程以root运行，Nginx将会调用setuid()/setgid()来设置用户/组，如果没有指定组，那么将使用与用户名相同的组，默认情况下会使用
nobody用户与nobody组（或者nogroup），或者在编译时指定的--user=USER和--group=GROUP的值。

user www users;worker_cpu_affinity
语法：worker_cpu_affinity cpumask [cpumask...] 
默认值：none 
仅支持linux系统。
这个参数允许将工作进程指定到cpu，它调用sched_setaffinity()函数

worker_processes     4;
worker_cpu_affinity 0001 0010 0100 1000;指定每个进程到一个CPU：

worker_processes     2;
worker_cpu_affinity 0101 1010;指定第一个进程到CPU0/CPU2，指定第二个进程到CPU1/CPU3，对于HTT处理器来说是一个不错的选择。

worker_priority
语法：worker_priority [-] number 
默认值：on 
这个选项可以用来设置所有工作进程的优先级（即linux系统中的nice），它调用setpriority()。
worker_processes
语法：worker_processes number 
默认值：1 

worker_processes 5;由于以下几点原因，Nginx可能需要运行不止一个进程

·使用了SMP（对称多处理技术）。
·当服务器在磁盘I/O出现瓶颈时为了减少响应时间。
·当使用select()/poll()限制了每个进程的最大连接数时。

在事件模块这一章中我们将使用worker_processes和worker_connections来计算理论最大连接数（max_clients）：
max_clients = worker_processes * worker_connections 

worker_rlimit_core
语法：worker_rlimit_core size 
默认值： 
允许的每个进程核心文件最大值。

worker_rlimit_nofile
语法：worker_rlimit_nofile limit 
默认值： 
进程能够打开的最多文件描述符数

worker_rlimit_sigpending 
语法：worker_rlimit_sigpending limit 
默认值： 
linux内核2.6.8以后，限制调用的进程中真实用户队列可能使用的信号数量。

working_directory 
语法：working_directory path 
默认值：--prefix 
程序的工作目录，一般只用来指定核心文件位置，Nginx仅使用绝对路径，所有在配置文件中的相对路径会转移到--prefix==PATH

·变量
$pid
进程ID号
$realpath_root
未标记
·参考文档
前进->事件模块（Events Module）













事件模块（Events Module）


·摘要
控制Nginx处理连接的方式
·指令
accept_mutex
语法：accept_mutex [ on | off ] 
默认值：on 
Nginx使用连接互斥锁进行顺序的accept()系统调用
accept_mutex_delay 
语法：accept_mutex_delay Nms; 
默认值：500ms
如果一个进程没有互斥锁，它将至少在这个值的时间后被回收，默认是500ms

debug_connection
语法：debug_connection [ip | CIDR] 
默认值：none 
0.3.54版本后，这个参数支持CIDR地址池格式。
这个参数可以指定只记录由某个客户端IP产生的debug信息。
当然你也可以指定多个参数。

error_log /var/log/nginx/errors;
events {
  debug_connection   192.168.1.1;
}devpoll_changes
devpoll_events 
kqueue_changes
kqueue_events 
epoll_events
语法：devpoll_changes 
默认值：
这些参数指定了按照规定方式传递到或者来自内核的事件数，默认devpoll的值为32，其余为512。
multi_accept
语法：multi_accept [ on | off ] 
默认值：off 
multi_accept在Nginx接到一个新连接通知后调用accept()来接受尽量多的连接
rtsig_signo
语法：rtsig_signo 
默认值：
Nginx在rtsig模式启用后使用两个信号，该指令指定第一个信号编号，第二个信号编号为第一个加1
默认rtsig_signo的值为SIGRTMIN+10 (40)。
rtsig_overflow_events
rtsig_overflow_test
rtsig_overflow_threshold

语法：rtsig_overflow_* 
默认值：
这些参数指定如何处理rtsig队列溢出。当溢出发生在nginx清空rtsig队列时，它们将连续调用poll()和 rtsig.poll()来处理未完成的事件，
直到rtsig被排空以防止新的溢出，当溢出处理完毕，nginx再次启用rtsig模式。
rtsig_overflow_events specifies指定经过poll()的事件数，默认为16
rtsig_overflow_test指定poll()处理多少事件后nginx将排空rtsig队列，默认值为32
rtsig_overflow_threshold只能运行在Linux 2.4.x内核下，在排空rtsig队列前nginx检查内核以确定队列是怎样被填满的
默认值为1/10，“rtsig_overflow_threshold 3”意为1/3。
use
语法：use [ kqueue | rtsig | epoll | /dev/poll | select | poll | eventport ] 
默认值：
如果你在./configure的时候指定了不止一个事件模型，你可以通过这个参数告诉nginx你想使用哪一个事件模型，默认情况下nginx在编译时会检
查最适合你系统的事件模型。
你可以在这里看到所有可用的事件模型并且如果在./configure时激活它们。

worker_connections
语法：worker_connections 
默认值：
worker_connections和worker_proceses（见主模块）允许你计算理论最大连接数：
最大连接数 = worker_processes * worker_connections 
在反向代理环境下：
最大连接数 = worker_processes * worker_connections/4
由于浏览器默认打开2个连接到服务器，nginx使用来自相同地址池的fds（文件描述符）与前后端相连接










HTTP核心模块（HTTP Core）


·摘要
Nginx处理HTTP的核心功能模块
·指令
alias
语法：alias file-path|directory-path; 
默认值：no
使用字段：location
这个指令指定一个路径使用某个某个，注意它可能类似于root，但是document root没有改变，请求只是使用了别名目录的文件。

location  /i/ {
  alias  /spool/w3/images/;
}
上个例子总，请求"/i/top.gif"将返回这个文件: "/spool/w3/images/top.gif"。
Alias同样可以用于带正则表达式的location，如：

location ~ ^/download/(.*)$ {
  alias /home/website/files/$1;
}请求"/download/book.pdf"将返回"/home/website/files/book.pdf"。
同样，也可以在别名目录字段中使用变量。

client_body_in_file_only 
语法：client_body_in_file_only on|off 
默认值：off 
使用字段：http, server, location 
这个指令始终存储一个连接请求实体到一个文件即使它只有0字节。
注意：如果这个指令打开，那么一个连接请求完成后，所存储的文件并不会删除。
这个指令可以用于debug调试和嵌入式Perl模块中的$r->request_body_file。

client_body_in_single_buffer 
语法：client_body_in_single_buffer 
默认值：off 
使用字段：http, server, location 
这个指令(0.7.58版本)指定是否将客户端连接请求完整的放入一个缓冲区，当使用变量$request_body时推荐使用这个指令以减少复制操作。
如果无法将一个请求放入单个缓冲区，将会被放入磁盘。
client_body_buffer_size
语法：client_body_buffer_size the_size 
默认值：8k/16k 
使用字段：http, server, location 
这个指令可以指定连接请求实体的缓冲区大小。
如果连接请求超过缓存区指定的值，那么这些请求实体的整体或部分将尝试写入一个临时文件。
默认值为两个内存分页大小值，根据平台的不同，可能是8k或16k。
client_body_temp_path
语法：client_body_temp_path dir-path [ level1 [ level2 [ level3 ] 
默认值：client_body_temp 
使用字段：http, server, location 
指令指定连接请求实体试图写入的临时文件路径。
可以指定三级目录结构，如：

client_body_temp_path  /spool/nginx/client_temp 1 2;那么它的目录结构可能是这样：

/spool/nginx/client_temp/7/45/00000123457client_body_timeout
语法：client_body_timeout time
默认值：60 
使用字段：http, server, location 
指令指定读取请求实体的超时时间。
这里的超时是指一个请求实体没有进入读取步骤，如果连接超过这个时间而客户端没有任何响应，Nginx将返回一个"Request time out" (408)错误

client_header_buffer_size
语法：client_header_buffer_size size 
默认值：1k 
使用字段：http, server 
指令指定客户端请求头部的缓冲区大小
绝大多数情况下一个请求头不会大于1k
不过如果有来自于wap客户端的较大的cookie它可能会大于1k，Nginx将分配给它一个更大的缓冲区，这个值可以在large_client_header_buffers里面设置。

client_header_timeout 
语法：client_header_timeout time 
默认值：60 
使用字段：http, server 
指令指定读取客户端请求头标题的超时时间。
这里的超时是指一个请求头没有进入读取步骤，如果连接超过这个时间而客户端没有任何响应，Nginx将返回一个"Request time out" (408)错误。

client_max_body_size 
语法：client_max_body_size size 
默认值：client_max_body_size 1m 
使用字段：http, server, location 
指令指定允许客户端连接的最大请求实体大小，它出现在请求头部的Content-Length字段。
如果请求大于指定的值，客户端将收到一个"Request Entity Too Large" (413)错误。
记住，浏览器并不知道怎样显示这个错误。

default_type
语法： default_type MIME-type 
默认值：default_type text/plain 
使用字段：http, server, location 
某个文件在标准MIME视图没有指定的情况下的默认MIME类型。
参考types。

location = /proxy.pac {
  default_type application/x-ns-proxy-autoconfig;
}
location = /wpad.dat {
  rewrite . /proxy.pac;
  default_type application/x-ns-proxy-autoconfig;
}
directio
语法：directio [size|off] 
默认值：directio off 
使用字段：http, server, location 
这个参数指定在读取文件大小大于指定值的文件时使用O_DIRECT（FreeBSD, Linux），F_NOCACHE（Mac OS X）或者调用directio()函数（Solaris），
当一个请求用到这个参数时会禁用sendfile，通常这个参数用于大文件。

directio  4m;error_page
语法：error_page code [ code... ] [ = | =answer-code ] uri | @named_location 
默认值：no 
使用字段：http, server, location, location 中的if字段 
这个参数可以为错误代码指定相应的错误页面

error_page   404          /404.html;
error_page   502 503 504  /50x.html;
error_page   403          http://example.com/forbidden.html;
error_page   404          = @fetch;同样，你也可以修改返回的错误代码：

error_page 404 =200 /.empty.gif;如果一个错误的响应经过代理或者FastCGI服务器并且这个服务器可以返回不同的响应码，如200, 302, 401或404，
那么可以指定响应码返回：

error_page   404  =  /404.php;如果在重定向时不需要改变URI，可以将错误页面重定向到一个命名的location字段中：

location / (
    error_page 404 = @fallback;
)

location @fallback (
    proxy_pass http://backend;
)if_modified_since 
语法：if_modified_since [off|exact|before]
默认值：if_modified_since exact 
使用字段：http, server, location 
指令（0.7.24）定义如何将文件最后修改时间与请求头中的"If-Modified-Since"时间相比较。

·off ：不检查请求头中的"If-Modified-Since"（0.7.34）。
·exact：精确匹配
·before：文件修改时间应小于请求头中的"If-Modified-Since"时间

internal 
语法：internal 
默认值：no 
使用字段： location 
internal指令指定某个location只能被“内部的”请求调用，外部的调用请求会返回"Not found" (404)
“内部的”是指下列类型：

·指令error_page重定向的请求。
·ngx_http_ssi_module模块中使用include virtual指令创建的某些子请求。
·ngx_http_rewrite_module模块中使用rewrite指令修改的请求。

一个防止错误页面被用户直接访问的例子：

error_page 404 /404.html;
location  /404.html {
  internal;
}

keepalive_timeout 
语法：keepalive_timeout [ time ] [ time ]
默认值：keepalive_timeout 75 
使用字段：http, server, location 
参数的第一个值指定了客户端与服务器长连接的超时时间，超过这个时间，服务器将关闭连接。
参数的第二个值（可选）指定了应答头中Keep-Alive: timeout=time的time值，这个值可以使一些浏览器知道什么时候关闭连接，以便服务器
不用重复关闭，如果不指定这个参数，nginx不会在应答头中发送Keep-Alive信息。（但这并不是指怎样将一个连接“Keep-Alive”）
参数的这两个值可以不相同
下面列出了一些服务器如何处理包含Keep-Alive的应答头：

·MSIE和Opera将Keep-Alive: timeout=N头忽略。
·MSIE保持一个连接大约60-65秒，然后发送一个TCP RST。
·Opera将一直保持一个连接处于活动状态。
·Mozilla将一个连接在N的基础上增加大约1-10秒。
·Konqueror保持一个连接大约N秒。

keepalive_requests 
语法：keepalive_requests n 
默认值：keepalive_requests 100 
使用字段：http, server, location 
服务器保持长连接的请求数。

large_client_header_buffers 
语法：large_client_header_buffers number size 
默认值：large_client_header_buffers 4 4k/8k 
使用字段：http, server 
指定客户端一些比较大的请求头使用的缓冲区数量和大小。
请求字段不能大于一个缓冲区大小，如果客户端发送一个比较大的头，nginx将返回"Request URI too large" (414)
同样，请求的头部最长字段不能大于一个缓冲区，否则服务器将返回"Bad request" (400)。
缓冲区只在需求时分开。
默认一个缓冲区大小为操作系统中分页文件大小，通常是4k或8k，如果一个连接请求最终将状态转换为keep-alive，它所占用的缓冲区将被释放。

limit_except 
语法：limit_except methods {...} 
默认值：no 
使用字段：location 
指令可以在location字段中做一些http动作的限制。
ngx_http_access_module和ngx_http_auth_basic_module模块有很强的访问控制功能。

limit_except  GET {
  allow  192.168.1.0/32;
  deny   all;
}limit_rate 
语法：limit_rate speed 
默认值：no 
使用字段：http, server, location, location中的if字段 
限制将应答传送到客户端的速度，单位为字节/秒，限制仅对一个连接有效，即如果一个客户端打开2个连接，则它的速度是这个值乘以二。
由于一些不同的状况，可能要在server字段来限制部分连接的速度，那么这个参数并不适用，不过你可以选择设置$limit_rate变量的值来达到目的：

server {
  if ($slow) {
    set $limit_rate  4k;
  }
}同样可以通过设置X-Accel-Limit-Rate头（NginxXSendfile）来控制proxy_pass返回的应答。并且不借助X-Accel-Redirect头来完成。

limit_rate_after 
语法：limit_rate_after time 
默认值：limit_rate_after 1m 
使用字段：http, server, location, location中的if字段 
在应答一部分被传递后限制速度：

limit_rate_after 1m;
limit_rate 100k;listen
语法(0.7.x)：listen address:port [ default [ backlog=num | rcvbuf=size | sndbuf=size | accept_filter=filter | deferred | bind | ssl ] ] 
语法(0.8.x)：listen address:port [ default_server [ backlog=num | rcvbuf=size | sndbuf=size | accept_filter=filter | deferred | bind | ssl ] ] 
默认值：listen 80 
使用字段：server 
listen指令指定了server{...}字段中可以被访问到的ip地址及端口号，可以只指定一个ip，一个端口，或者一个可解析的服务器名。

listen 127.0.0.1:8000;
listen 127.0.0.1;
listen 8000;
listen *:8000;
listen localhost:8000;ipv6地址格式（0.7.36）在一个方括号中指定：

listen [::]:8000;
listen [fe80::1];当linux（相对于FreeBSD）绑定IPv6[::]，那么它同样将绑定相应的IPv4地址，如果在一些非ipv6服务器上仍然这样设置，将
会绑定失败，当然可以使用完整的地址来代替[::]以免发生问题，也可以使用"default ipv6only=on" 选项来制定这个listen字段仅绑定ipv6地址，
注意这个选项仅对这行listen生效，不会影响server块中其他listen字段指定的ipv4地址。

listen [2a02:750:5::123]:80;
listen [::]:80 default ipv6only=on;如果只有ip地址指定，则默认端口为80。
如果指令有default参数，那么这个server块将是通过“地址:端口”来进行访问的默认服务器，这对于你想为那些不匹配server_name指令中的主机
名指定默认server块的虚拟主机（基于域名）非常有用，如果没有指令带有default参数，那么默认服务器将使用第一个server块。
listen允许一些不同的参数，即系统调用listen(2)和bind(2)中指定的参数，这些参数必须用在default参数之后：
backlog=num -- 指定调用listen(2)时backlog的值，默认为-1。
rcvbuf=size -- 为正在监听的端口指定SO_RCVBUF。
sndbuf=size -- 为正在监听的端口指定SO_SNDBUF。
accept_filter=filter -- 指定accept-filter。

·仅用于FreeBSD，可以有两个过滤器，dataready与httpready，仅在最终版本的FreeBSD（FreeBSD: 6.0, 5.4-STABLE与4.11-STABLE）上，为
他们发送-HUP信号可能会改变accept-filter。
deferred -- 在linux系统上延迟accept(2)调用并使用一个辅助的参数： TCP_DEFER_ACCEPT。
bind -- 将bind(2)分开调用。
·主要指这里的“地址:端口”，实际上如果定义了不同的指令监听同一个端口，但是每个不同的地址和某条指令均监听为这个端口的所有地址
（*:port），那么nginx只将bind(2)调用于*:port。这种情况下通过系统调用getsockname()确定哪个地址上有连接到达，但是如果使用了
parameters backlog, rcvbuf, sndbuf, accept_filter或deferred这些参数，那么将总是将这个“地址:端口”分开调用。
ssl -- 参数（0.7.14）不将listen(2)和bind(2)系统调用关联。
·被指定这个参数的listen将被允许工作在SSL模式，这将允许服务器同时工作在HTTP和HTTPS两种协议下，例如：
listen  80;
listen  443 default ssl;一个使用这些参数的完整例子： 
listen  127.0.0.1 default accept_filter=dataready backlog=1024;0.8.21版本以后nginx可以监听unix套接口： 
listen unix:/tmp/nginx1.sock;location 
语法：location [=|~|~*|^~|@] /uri/ { ... } 
默认值：no 
使用字段：server 
这个参数根据URI的不同需求来进行配置，可以使用字符串与正则表达式匹配，如果要使用正则表达式，你必须指定下列前缀：
1、~* 不区分大小写。
2、~ 区分大小写。
要确定该指令匹配特定的查询，程序将首先对字符串进行匹配，字符串匹配将作为查询的开始，最确切的匹配将被使用。然后，正则表达式的
匹配查询开始，匹配查询的第一个正则表达式找到后会停止搜索，如果没有找到正则表达式，将使用字符串的搜索结果。
在一些操作系统，如Mac OS X和Cygwin，字符串将通过不区分大小写的方式完成匹配（0.7.7），但是，比较仅限于单字节的语言环境。
正则表达式可以包含捕获（0.7.40），并用于其它指令中。
可以使用“^~”标记禁止在字符串匹配后检查正则表达式，如果最确切的匹配location有这个标记，那么正则表达式不会被检查。
使用“=”标记可以在URI和location之间定义精确的匹配，在精确匹配完成后并不进行额外的搜索，例如有请求“/”发生，则可以使用
“location = /”来加速这个处理。
即使没有“=”和“^~”标记，精确的匹配location在找到后同样会停止查询。
下面是各种查询方式的总结：
1、前缀“=”表示精确匹配查询，如果找到，立即停止查询。
2、指令仍然使用标准字符串，如果匹配使用“^~”前缀，停止查询。
3、正则表达式按照他们在配置文件中定义的顺序。
4、如果第三条产生一个匹配，这个匹配将被使用，否则将使用第二条的匹配。
例：

location  = / {
  # 只匹配 / 的查询.
  [ configuration A ]
}
location  / {
  # 匹配任何以 / 开始的查询，但是正则表达式与一些较长的字符串将被首先匹配。
  [ configuration B ]
}
location ^~ /images/ {
  # 匹配任何以 /images/ 开始的查询并且停止搜索，不检查正则表达式。
  [ configuration C ]
}
location ~* \.(gif|jpg|jpeg)$ {
  # 匹配任何以gif, jpg, or jpeg结尾的文件，但是所有 /images/ 目录的请求将在Configuration C中处理。
  [ configuration D ]
}各请求的处理如下例：
·/ -> configuration A 
·/documents/document.html -> configuration B 
·/images/1.gif -> configuration C 
·/documents/1.jpg -> configuration D 
注意你可以以任何顺序定义这4个配置并且匹配结果是相同的，但当使用嵌套的location结构时可能会将配置文件变的复杂并且产生一些比较意外的结果。
标记“@”指定一个命名的location，这种location并不会在正常请求中执行，它们仅使用在内部重定向请求中。（查看error_page和try_files）
log_not_found 
语法：log_not_found [on|off] 
默认值：log_not_found on 
使用字段：http, server, location 
指令指定是否将一些文件没有找到的错误信息写入error_log指定的文件中。

log_subrequest 
语法：log_subrequest [on|off]
默认值：log_subrequest off 
使用字段：http, server, location 
指令指定是否将一些经过rewrite rules和/或SSI requests的子请求日志写入access_log指定的文件中。

msie_padding 
语法：msie_padding [on|off] 
默认值：msie_padding on 
使用字段：http, server, location 
指令指定开启或关闭MSIE浏览器和chrome浏览器（0.8.25+）的msie_padding特征，当这个功能开启，nginx将为响应实体分配最小512字节，以
便响应大于或等于400的状态代码。
指令预防在MSIE和chrome浏览器中激活“友好的”HTTP错误页面，以便不在服务器端隐藏更多的错误信息。

msie_refresh 
语法： msie_refresh [on|off] 
默认值：msie_refresh off 
使用字段：http, server, location 
指令允许或拒绝为MSIE发布一个refresh而不是做一次redirect

open_file_cache 
语法：open_file_cache max = N [inactive = time] | off 
默认值：open_file_cache off 
使用字段：http, server, location 
这个指令指定缓存是否启用，如果启用，将记录文件以下信息：
·打开的文件描述符，大小信息和修改时间。
·存在的目录信息。
·在搜索文件过程中的错误信息 -- 没有这个文件、无法正确读取，参考open_file_cache_errors
指令选项：

·max - 指定缓存的最大数目，如果缓存溢出，最长使用过的文件（LRU）将被移除。
·inactive - 指定缓存文件被移除的时间，如果在这段时间内文件没被下载，默认为60秒。
·off - 禁止缓存。

例: 
 open_file_cache max=1000 inactive=20s;
 open_file_cache_valid    30s;
 open_file_cache_min_uses 2;
 open_file_cache_errors   on;open_file_cache_errors 
语法：open_file_cache_errors on | off 
默认值：open_file_cache_errors off 
使用字段：http, server, location 
这个指令指定是否在搜索一个文件是记录cache错误。
open_file_cache_min_uses 
语法：open_file_cache_min_uses number 
默认值：open_file_cache_min_uses 1 
使用字段：http, server, location 
这个指令指定了在open_file_cache指令无效的参数中一定的时间范围内可以使用的最小文件数，如果使用更大的值，文件描述符在cache中总是打开状态。
open_file_cache_valid 
语法：open_file_cache_valid time 
默认值：open_file_cache_valid 60 
使用字段：http, server, location 
这个指令指定了何时需要检查open_file_cache中缓存项目的有效信息。
optimize_server_names 
语法：optimize_server_names [ on|off ] 
默认值：optimize_server_names on 
使用字段：http, server 
这个指令指定是否在基于域名的虚拟主机中开启最优化的主机名检查。
尤其是检查影响到使用主机名的重定向，如果开启最优化，那么所有基于域名的虚拟主机监听的一个“地址：端口对”具有相同的配置，这样
在请求执行的时候并不进行再次检查，重定向会使用第一个server name。
如果重定向必须使用主机名并且在客户端检查通过，那么这个参数必须设置为off。
注意：这个参数不建议在nginx 0.7.x版本中使用，请使用server_name_in_redirect。
port_in_redirect 
语法：port_in_redirect [ on|off ] 
默认值：port_in_redirect on 
使用字段：http, server, location 
这个指令指定是否在让nginx在重定向操作中对端口进行操作。
如果这个指令设置为off，在重定向的请求中nginx不会在url中添加端口。
recursive_error_pages 
语法：recursive_error_pages [on|off] 
默认值：recursive_error_pages off 
使用字段：http, server, location 
recursive_error_pages指定启用除第一条error_page指令以外其他的error_page。
resolver 
语法：resolver address 
默认值：no 
使用字段：http, server, location 
指定DNS服务器地址，如： 
resolver 127.0.0.1;resolver_timeout 
语法：resolver_timeout time 
默认值：30s 
使用字段：http, server, location 
解析超时时间。如： 
resolver_timeout 5s;root
语法：root path 
默认值：root html 
使用字段：http, server, location ,location中的if字段
请求到达后的文件根目录。
下例中：

location  /i/ {
  root  /spool/w3;
}如果请求"/i/top.gif"文件，nginx将转到"/spool/w3/i/top.gif"文件。你可以在参数中使用变量。
注意：在请求中root会添加这个location到它的值后面，即"/i/top.gif"并不会请求"/spool/w3/top.gif"文件，如果要实现上述类似于
apache alias的功能，可以使用alias指令。
satisfy_any
语法：satisfy_any [ on|off ] 
默认值：satisfy_any off 
使用字段：location 
指令可以检查至少一个成功的密码效验，它在NginxHttpAccessModule或NginxHttpAuthBasicModule这两个模块中执行。

location / {
  satisfy_any  on;
  allow  192.168.1.0/32;
  deny   all;
  auth_basic            "closed site";
  auth_basic_user_file  conf/htpasswd;
}send_timeout 
语法：send_timeout the time 
默认值：send_timeout 60 
使用字段：http, server, location 
指令指定了发送给客户端应答后的超时时间，Timeout是指没有进入完整established状态，只完成了两次握手，如果超过这个时间客户端没有
任何响应，nginx将关闭连接。

sendfile 
语法：sendfile [ on|off ] 
默认值：sendfile off 
使用字段：http, server, location 
是否启用sendfile()函数。

server 
语法：server {...} 
默认值：no 
使用字段：http 
server字段包含虚拟主机的配置。
没有明确的机制来分开基于域名（请求中的主机头）和基于IP的虚拟主机。
可以通过listen指令来指定必须连接到这个server块的所有地址和端口，并且在server_name指令中可以指定所有的域名。

server_name
语法：server_name name [... ] 
默认值：server_name hostname 
使用字段：server 
这个指令有两个作用：
·将HTTP请求的主机头与在nginx配置文件中的server{...}字段中指定的参数进行匹配，并且找出第一个匹配结果。这就是如何定义虚拟主机
的方法，域名遵循下述优先级规则：
1、完整匹配的名称。
2、名称开始于一个文件通配符：*.example.com。
3、名称结束于一个文件通配符：www.example.*。
4、使用正则表达式的名称。
如果没有匹配的结果，nginx配置文件将安装以下优先级使用[#server server { ... }]字段：
1、listen指令被标记为default的server字段。
2、第一个出现listen（或者默认的listen 80）的server字段。
·如果server_name_in_redirect被设置，这个指令将用于设置HTTP重定向的服务器名。
例：

server {
  server_name   example.com  www.example.com;
}第一个名称为服务器的基本名称，默认名称为机器的hostname。
当然，可以使用文件通配符：

server {
  server_name   example.com  *.example.com  www.example.*;
}上述例子中的前两个名称可以合并为一个：

server {
  server_name  .example.com;
}同样可以使用正则表达式。名称前面加“~”：

server {
  server_name   www.example.com   ~^www\d+\.example\.com$;
}如果客户端请求中没有主机头或者没有匹配server_name的主机头，服务器基本名称将被用于一个HTTP重定向，你可以只使用“*”来强制nginx
在HTTP重定向中使用Host头（注意*不能用于第一个名称，不过你可以用一个很傻逼的名称代替，如“_”）

server {
  server_name example.com *;
}
server {
  server_name _ *;
}在nginx0.6.x中有稍许改变：

server {
  server_name _;
}0.7.12版本以后已经可以支持空服务器名，以处理那些没有主机头的请求：

server {
  server_name "";
}

server_name_in_redirect
语法：server_name_in_redirect on|off 
默认值：server_name_in_redirect on 
使用字段：http, server, location 
如果这个指令打开，nginx将使用server_name指定的基本服务器名作为重定向地址，如果关闭，nginx将使用请求中的主机头。
server_names_hash_max_size 
语法：server_names_hash_max_size number 
默认值：server_names_hash_max_size 512 
使用字段：http 
服务器名称哈希表的最大值，更多信息请参考nginx Optimizations。
server_names_hash_bucket_size 
语法：server_names_hash_bucket_size number 
默认值：server_names_hash_bucket_size 32/64/128 
使用字段：http 
服务器名称哈希表每个页框的大小，这个指令的默认值依赖于cpu缓存。更多信息请参考nginx Optimizations。
server_tokens 
语法：server_tokens on|off 
默认值：server_tokens on 
使用字段：http, server, location 
是否在错误页面和服务器头中输出nginx版本信息。
tcp_nodelay 
语法：tcp_nodelay [on|off] 
默认值：tcp_nodelay on 
使用字段：http, server, location 
这个指令指定是否使用socket的TCP_NODELAY选项，这个选项只对keep-alive连接有效。
点击这里了解更多关于TCP_NODELAY选项的信息。
tcp_nopush 
语法：tcp_nopush [on|off] 
默认值：tcp_nopush off 
使用字段：http, server, location 
这个指令指定是否使用socket的TCP_NOPUSH（FreeBSD）或TCP_CORK（linux）选项，这个选项只在使用sendfile时有效。
设置这个选项的将导致nginx试图将它的HTTP应答头封装到一个包中。
点击这里查看关于TCP_NOPUSH和TCP_CORK选项的更多信息。
try_files 
语法：try_files file1 [file2 ... filen] fallback 
默认值：none 
使用字段：location
这个指令告诉nginx将测试参数中每个文件的存在，并且URI将使用第一个找到的文件，如果没有找到文件，将请求名为fallback（可以使任何名称）
的location字段，fallback是一个必须的参数，它可以是一个命名的location或者可靠的URI。
例：

location / {
  try_files index.html index.htm @fallback;
}

location @fallback {
  root /var/www/error;
  index index.html;
}types 
语法：types {...} 
使用字段：http, server, location 
这个字段指定一些扩展的文件对应方式与应答的MIME类型，一个MIME类型可以有一些与其类似的扩展。默认使用以下文件对应方式：

types {
  text/html    html;
  image/gif    gif;
  image/jpeg   jpg;
}完整的对应视图文件为conf/mime.types，并且将被包含。
如果你想让某些特定的location的处理方式使用MIME类型：application/octet-stream，可以使用以下配置：

location /download/ {
  types         { }
  default_type  application/octet-stream;
}·变量
core module 支持一些内置的变量，与apache使用的变量相一致。
首先，一些变量代表了客户端请求头部的一些字段，如：$http_user_agent, $http_cookie等等。注意，由于这些变量会在请求中定义，所以
可能无法保证他们是存在的或者说可以定义到一些别的地方（例如遵循一定的规范）。
除此之外，下列是一些其他变量：
$arg_PARAMETER 
这个变量包含在查询字符串时GET请求PARAMETER的值。
$args
这个变量等于请求行中的参数。
$binary_remote_addr 
二进制码形式的客户端地址。
$body_bytes_sent 
未知。
$content_length 
请求头中的Content-length字段。
$content_type
请求头中的Content-Type字段。
$cookie_COOKIE 
cookie COOKIE的值。
$document_root 
当前请求在root指令中指定的值。
$document_uri 
与$uri相同。
$host
请求中的主机头字段，如果请求中的主机头不可用，则为服务器处理请求的服务器名称。
$is_args 
如果$args设置，值为"?"，否则为""。
$limit_rate 
这个变量可以限制连接速率。
$nginx_version 
当前运行的nginx版本号。
$query_string 
与$args相同。
$remote_addr 
客户端的IP地址。
$remote_port 
客户端的端口。
$remote_user 
已经经过Auth Basic Module验证的用户名。
$request_filename 
当前连接请求的文件路径，由root或alias指令与URI请求生成。
$request_body 
这个变量（0.7.58+）包含请求的主要信息。在使用proxy_pass或fastcgi_pass指令的location中比较有意义。
$request_body_file 
客户端请求主体信息的临时文件名。
$request_completion 
未知。
$request_method 
这个变量是客户端请求的动作，通常为GET或POST。
包括0.8.20及之前的版本中，这个变量总为main request中的动作，如果当前请求是一个子请求，并不使用这个当前请求的动作。
$request_uri 
这个变量等于包含一些客户端请求参数的原始URI，它无法修改，请查看$uri更改或重写URI。
$scheme 
HTTP方法（如http，https）。按需使用，例：

rewrite  ^(.+)$  $scheme://example.com$1  redirect;$server_addr 
服务器地址，在完成一次系统调用后可以确定这个值，如果要绕开系统调用，则必须在listen中指定地址并且使用bind参数。
$server_name 
服务器名称。
$server_port 
请求到达服务器的端口号。
$server_protocol 
请求使用的协议，通常是HTTP/1.0或HTTP/1.1。
$uri 
请求中的当前URI(不带请求参数，参数位于$args)，可以不同于浏览器传递的$request_uri的值，它可以通过内部重定向，或者使用index指令进行修改。
·参考文档
Original Documentation
Nginx Http Core Module
前进->HTTP负载均衡模块（HTTP Upstream）




URL重写模块（Rewrite）

·摘要
这个模块允许使用正则表达式重写URI（需PCRE库），并且可以根据相关变量重定向和选择不同的配置。
如果这个指令在server字段中指定，那么将在被请求的location确定之前执行，如果在指令执行后所选择的location中有其他的重写规则，那么它们也被执行。如果在location中执行这个指令产生了新的URI，那么location又一次确定了新的URI。
这样的循环可以最多执行10次，超过以后nginx将返回500错误。
·指令
break 
语法：break
默认值：none
使用字段：server, location, if 
完成当前设置的规则，停止执行其他的重写指令。
示例：
if ($slow) {
  limit_rate  10k;
  break;
}if 
语法：if (condition) { ... } 
默认值：none
使用字段：server, location 
判断一个条件，如果条件成立，则后面的大括号内的语句将执行，相关配置从上级继承。
可以在判断语句中指定下列值：

·一个变量的名称；不成立的值为：空字符传""或者一些用“0”开始的字符串。
·一个使用=或者!=运算符的比较语句。
·使用符号~*和~模式匹配的正则表达式：
·~为区分大小写的匹配。
·~*不区分大小写的匹配（firefox匹配FireFox）。
·!~和!~*意为“不匹配的”。
·使用-f和!-f检查一个文件是否存在。
·使用-d和!-d检查一个目录是否存在。
·使用-e和!-e检查一个文件，目录或者软链接是否存在。 
·使用-x和!-x检查一个文件是否为可执行文件。 

正则表达式的一部分可以用圆括号，方便之后按照顺序用$1-$9来引用。
示例配置：
if ($http_user_agent ~ MSIE) {
  rewrite  ^(.*)$  /msie/$1  break;
}
 
if ($http_cookie ~* "id=([^;] +)(?:;|$)" ) {
  set  $id  $1;
}
 
if ($request_method = POST ) {
  return 405;
}
 
if (!-f $request_filename) {
  break;
  proxy_pass  http://127.0.0.1;
}
 
if ($slow) {
  limit_rate  10k;
}
 
if ($invalid_referer) {
  return   403;
}
 
if ($args ~ post=140){
  rewrite ^ http://example.com/ permanent;
}内置变量$invalid_referer用指令valid_referers指定。
return
语法：return code 
默认值：none
使用字段：server, location, if 
这个指令结束执行配置语句并为客户端返回状态代码，可以使用下列的值：204，400，402-406，408，410, 411, 413, 416与500-504。此外，非标准代码444将关闭连接并且不发送任何的头部。
rewrite 
语法：rewrite regex replacement flag 
默认值：none
使用字段：server, location, if 
按照相关的正则表达式与字符串修改URI，指令按照在配置文件中出现的顺序执行。
注意重写规则只匹配相对路径而不是绝对的URL，如果想匹配主机名，可以加一个if判断，如：

if ($host ~* www\.(.*)) {
  set $host_without_www $1;
  rewrite ^(.*)$ http://$host_without_www$1 permanent; # $1为'/foo'，而不是'www.mydomain.com/foo'
}可以在重写指令后面添加标记。
如果替换的字符串以http://开头，请求将被重定向，并且不再执行多余的rewrite指令。
标记可以是以下的值： 
·last - 完成重写指令，之后搜索相应的URI或location。
·break - 完成重写指令。
·redirect - 返回302临时重定向，如果替换字段用http://开头则被使用。
·permanent - 返回301永久重定向。
注意如果一个重定向是相对的（没有主机名部分），nginx将在重定向的过程中使用匹配server_name指令的“Host”头或者server_name指令指定的第一个名称，如果头不匹配或不存在，如果没有设置server_name，将使用本地主机名，如果你总是想让nginx使用“Host”头，可以在server_name使用“*”通配符（查看http核心模块中的server_name）。例如： 
rewrite  ^(/download/.*)/media/(.*)\..*$  $1/mp3/$2.mp3  last;
rewrite  ^(/download/.*)/audio/(.*)\..*$  $1/mp3/$2.ra   last;
return   403;但是如果我们将其放入一个名为/download/的location中，则需要将last标记改为break，否则nginx将执行10次循环并返回500错误。 
location /download/ {
  rewrite  ^(/download/.*)/media/(.*)\..*$  $1/mp3/$2.mp3  break;
  rewrite  ^(/download/.*)/audio/(.*)\..*$  $1/mp3/$2.ra   break;
  return   403;
}如果替换字段中包含参数，那么其余的请求参数将附加到后面，为了防止附加，可以在最后一个字符后面跟一个问号：

rewrite  ^/users/(.*)$  /show?user=$1?  last;注意：大括号（{和}），可以同时用在正则表达式和配置块中，为了防止冲突，正则表达式使用大括号需要用双引号（或者单引号）。例如要重写以下的URL：

/photos/123456 为: 
/path/to/photos/12/1234/123456.png 则使用以下正则表达式（注意引号）： 
rewrite  "/photos/([0-9] {2})([0-9] {2})([0-9] {2})" /path/to/photos/$1/$1$2/$1$2$3.png;同样，重写只对路径进行操作，而不是参数，如果要重写一个带参数的URL，可以使用以下代替： 
if ($args ^~ post=100){
  rewrite ^ http://example.com/new-address.html? permanent;
}注意$args变量不会被编译，与location过程中的URI不同（参考http核心模块中的location）。
set
语法：set variable value 
默认值：none
使用字段：server, location, if 
指令设置一个变量并为其赋值，其值可以是文本，变量和它们的组合。
你可以使用set定义一个新的变量，但是不能使用set设置$http_xxx头部变量的值。具体可以查看这个例子
uninitialized_variable_warn 
语法：uninitialized_variable_warn on|off 
默认值：uninitialized_variable_warn on 
使用字段：http, server, location, if 
开启或关闭在未初始化变量中记录警告日志。
事实上，rewrite指令在配置文件加载时已经编译到内部代码中，在解释器产生请求时使用。
这个解释器是一个简单的堆栈虚拟机，如下列指令： 
location /download/ {
  if ($forbidden) {
    return   403;
  }
  if ($slow) {
    limit_rate  10k;
  }
  rewrite  ^/(download/.*)/media/(.*)\..*$  /$1/mp3/$2.mp3  break;
将被编译成以下顺序： 
  variable $forbidden
  checking to zero
  recovery 403
  completion of entire code
  variable $slow
  checking to zero
  checkings of regular expression
  copying "/"
  copying $1
  copying "/mp3/"
  copying $2
  copying "..mpe"
  completion of regular expression
  completion of entire sequence
注意并没有关于limit_rate的代码，因为它没有提及ngx_http_rewrite_module模块，“if”块可以类似"location"指令在配置文件的相同部分同时存在。
如果$slow为真，对应的if块将生效，在这个配置中limit_rate的值为10k。
指令： 
rewrite  ^/(download/.*)/media/(.*)\..*$  /$1/mp3/$2.mp3  break;如果我们将第一个斜杠括入圆括号，则可以减少执行顺序：

rewrite  ^(/download/.*)/media/(.*)\..*$  $1/mp3/$2.mp3  break;之后的顺序类似如下：

  checking regular expression
  copying $1
  copying "/mp3/"
  copying $2
  copying "..mpe"
  completion of regular expression
  completion of entire code
·参考文档
Original Documentation
Nginx Http Rewrite Module
前进->SSI模块（SSI）

*/
