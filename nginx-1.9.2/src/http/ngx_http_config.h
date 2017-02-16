
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_CONFIG_H_INCLUDED_
#define _NGX_HTTP_CONFIG_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/*
Nginx配置文件详解

#运行用户 
user nobody nobody; 
#启动进程 
worker_processes 2; 
#全局错误日志及PID文件 
error_log logs/error.log notice; 
pid logs/nginx.pid; 
#工作模式及连接数上限 
events { 
use epoll; 
worker_connections 1024; 
} 
#设定http服务器，利用它的反向代理功能提供负载均衡支持 
http { 
#设定mime类型 
include conf/mime.types; 
default_type application/octet-stream; 
#设定日志格式 
log_format main ‘$remote_addr – $remote_user [$time_local] ‘ 
‘”$request” $status $bytes_sent ‘ 
‘”$http_referer” “$http_user_agent” ‘ 
‘”$gzip_ratio”‘; 
log_format download ‘$remote_addr – $remote_user [$time_local] ‘ 
‘”$request” $status $bytes_sent ‘ 
‘”$http_referer” “$http_user_agent” ‘ 
‘”$http_range” “$sent_http_content_range”‘; 
#设定请求缓冲 
client_header_buffer_size 1k; 
large_client_header_buffers 4 4k;

#开启gzip模块 
gzip on; 
gzip_min_length 1100; 
gzip_buffers 4 8k; 
gzip_types text/plain; 
output_buffers 1 32k; 
postpone_output 1460; 
#设定access log 
access_log logs/access.log main; 
client_header_timeout 3m; 
client_body_timeout 3m; 
send_timeout 3m; 
sendfile on; 
tcp_nopush on; 
tcp_nodelay on; 
keepalive_timeout 65; 
#设定负载均衡的服务器列表 
upstream mysvr { 
#weigth参数表示权值，权值越高被分配到的几率越大 
#本机上的Squid开启3128端口 
server 192.168.8.1:3128 weight=5; 
server 192.168.8.2:80 weight=1; 
server 192.168.8.3:80 weight=6; 
} 

#设定虚拟主机 
server { 
listen 80; 
server_name 192.168.8.1 www.hahaer.com; 
charset gb2312; 
#设定本虚拟主机的访问日志 
access_log logs/www.hahaer.com.access.log main; 
#如果访问 /img/ *, /js/ *, /css/ * 资源，则直接取本地文件，不通过squid 
#如果这些文件较多，不推荐这种方式，因为通过squid的缓存效果更好 
location ~ ^/(img|js|css)/ { 
root /data3/Html; 
expires 24h; 
}

#对 “/” 启用负载均衡 
location / { 
proxy_pass http://mysvr; 
proxy_redirect off; 
proxy_set_header Host $host; 
proxy_set_header X-Real-IP $remote_addr; 
proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for; 
client_max_body_size 10m; 
client_body_buffer_size 128k; 
proxy_connect_timeout 90; 
proxy_send_timeout 90; 
proxy_read_timeout 90; 
proxy_buffer_size 4k; 
proxy_buffers 4 32k; 
proxy_busy_buffers_size 64k; 
proxy_temp_file_write_size 64k; 
}

#设定查看Nginx状态的地址 
location /NginxStatus { 
stub_status on; 
access_log on; 
auth_basic “NginxStatus”; 
auth_basic_user_file conf/htpasswd; 
} 
} 
} 

备注：conf/htpasswd 文件的内容用 apache 提供的 htpasswd 工具来产生即可。

3.) 查看 Nginx 运行状态

输入地址 http://192.168.8.1/NginxStatus/，输入验证帐号密码，即可看到类似如下内容：

 

Active connections: 328 
server accepts handled requests 
9309 8982 28890 
Reading: 1 Writing: 3 Waiting: 324 
*/

/*
这时，HTTP框架会为所有的HTTP模块建立3个数组，分别存放所有HTTP模块的ngx_http_module_t中的
create_main_conf、create_srv_conf、create_loc_conf方法返回的地址指针（就像本章的例子
中mytest模块在create_loc_conf中生成了ngx_http_mytest_conf_t结构，并在create_lo_conf方法返回时将指针传递给HTTP框架）。
当然，如果HTTP模块对于配置项不感兴趣，
它没有实现create_main_conf、create_srv_conf、create_loc_conf等方法，那么数组中相应位
置存储的指针是NULL。ngx_http_conf_ctx_t的3个成员main_conf、srv_conf、loc_conf分
别指向这3个数组。下面看一段简化的代码，了解如何设置create_loc_conf返回的地址。
ngx_http_conf_ctx_t *ctx;
//HTTP框架生成了1个ngx_ht tp_conf_ctxt结构
ctx=ngx_pcalloc (cf->pool,  sizeof (ngx_http_conf_ctx_t))j
if (ctx==NULL){
    return NGX_CONF_ERROR;
)
／／生成1个数组存储所有的HTTP模块create_loc_conf方法返回的地址
ctx->loc_conf=ngx_pcalloc (cf->pool,  sizeof (void*)  * ngx_http_max_module);
if (ctx->loc conf==NULL)  {
    return NGX_CONF_ERROR;
)
*/
/*当Nginx检测到http{．．．）这个关键配置项时，HTTP配置模型就启动了，这时会首先建立1个ngx_http_conf_ ctx_t结构。
在http{．．．）块中就通过1个ngx_http_conf_ctx t结构保存了所有HTTP模块的配
置数据结构的入口。以后遇到任何server{．．．）块或者location{．．．）块时，也会建立ngx_http_
conf_ctx_t结构，生成同样的数组来保存所有HTTP模块通过create_srv_ conf  create_loc_
conf等方法返回的指针地址。ngx_http_conf_ctx_t是了解http配置块的基础
*/ //该结构中的变量直接指向ngx_http_module_t中的三个create_main_conf  create_srv_conf  create_loc_conf
/*
  http {
       xxxx
       server {
            location /xxx {
            }
       }
  }
  这种情况的配置文件，在执行到http的时候开辟ngx_http_conf_ctx_t会分别调用一次main srv loc_creat，执行到server时开辟ngx_http_conf_ctx_t会调用srv_creat loc_creat, 执行到location时开辟ngx_http_conf_ctx_t会调用一次loc_creat
  所以这种情况会调用1次main_creat 2才srv_creat 3次loc_creat。

  http {
       xxxx
       server {
            location /xxx {
            }
       }

       server {
            location /yyy {
            }
       }
  }
  这种情况的配置文件，在执行到http的时候开辟ngx_http_conf_ctx_t会分别调用一次main srv loc_creat，执行到server时开辟ngx_http_conf_ctx_t会调用srv_creat loc_creat, 执行到location时开辟ngx_http_conf_ctx_t会调用一次loc_creat
  所以这种情况会调用1次main_creat 1+2才srv_creat 1+2+2次loc_creat。
*/

/*
http{}中会调用main_conf srv_conf loc_conf分配空间，见ngx_http_block。server{}中会调用srv_conf loc_conf创
建空间,见ngx_http_core_server， location{}中会创建loc_conf空间,见ngx_http_core_location
图形化参考:深入理解NGINX中的图9-2(P302)  图10-1(P353) 图10-1(P356) 图10-1(P359)  图4-2(P145)

ngx_http_conf_ctx_t、ngx_http_core_main_conf_t、ngx_http_core_srv_conf_t、ngx_http_core_loc_conf_s和ngx_cycle_s->conf_ctx的关系见:
Nginx的http配置结构体的组织结构:http://tech.uc.cn/?p=300
*/ 
typedef struct { //相关空间创建和赋值见ngx_http_block, 该结构是ngx_conf_t->ctx成员。所有的配置所处内存的源头在ngx_cycle_t->conf_ctx,见ngx_init_cycle
/* 指向一个指针数组，数组中的每个成员都是由所有HTTP模块的create_main_conf方法创建的存放全局配置项的结构体，它们存放着解析直属http{}块内的main级别的配置项参数 */
    void        **main_conf;  /* 指针数组，数组中的每个元素指向所有HTTP模块ngx_http_module_t->create_main_conf方法产生的结构体 */

/* 指向一个指针数组，数组中的每个成员都是由所有HTTP模块的create_srv_conf方法创建的与server相关的结构体，它们或存放main级别配置项，
或存放srv级别的server配置项，这与当前的ngx_http_conf_ctx_t是在解析http{}或者server{}块时创建的有关 */
    void        **srv_conf;/* 指针数组，数组中的每个元素指向所有HTTP模块ngx_http_module_t->create->srv->conf方法产生的结构体 */

/*
指向一个指针数组，数组中的每个成员都是由所有HTTP模块的create_loc_conf方法创建的与location相关的结构体，它们可能存放着main、srv、loc级
别的配置项，这与当前的ngx_http_conf_ctx_t是在解析http{}、server{}或者location{}块时创建的有关存放location{}配置项
*/
    void        **loc_conf;/* 指针数组，数组中的每介元素指向所有HTTP模块ngx_http_module_t->create->loc->conf方法产生的结构体 */
} ngx_http_conf_ctx_t; //ctx是content的简称，表示上下文  
//参考:http://tech.uc.cn/?p=300   ngx_http_conf_ctx_t变量的指针ctx存储在ngx_cycle_t的conf_ctx所指向的指针数组，以ngx_http_module的index为下标的数组元素
//http://tech.uc.cn/?p=300参数解析相关数据结构参考

/*
Nginx安装完毕后，会有响应的安装目录，安装目录里nginx.conf为nginx的主配置文件，ginx主配置文件分为4部分，main（全局配置）、server（主机设置）、upstream（负载均衡服务器设）和location（URL匹配特定位置的设置），这四者关系为：server继承main，location继承server，upstream既不会继承其他设置也不会被继承。

一、Nginx的main（全局配置）文件

[root@rhel6u3-7 server]# vim /usr/local/nginx/conf/nginx.conf 
user nginx nginx; //指定nginx运行的用户及用户组为nginx，默认为nobody 
worker_processes 2； //开启的进程数，一般跟逻辑cpu核数一致 
error_log logs/error.log notice; //定于全局错误日志文件，级别以notice显示。还有debug、info、warn、error、crit模式，debug输出最多，crit输出最少，更加实际环境而定。 
pid logs/nginx.pid; //指定进程id的存储文件位置 
worker_rlimit_nofile 65535; //指定一个nginx进程打开的最多文件描述符数目，受系统进程的最大打开文件数量限制 
events { 
use epoll; 设置工作模式为epoll，除此之外还有select、poll、kqueue、rtsig和/dev/poll模式 
worker_connections 65535; //定义每个进程的最大连接数 受系统进程的最大打开文件数量限制 
} 
…….

[root@rhel6u3-7 server]# cat /proc/cpuinfo | grep "processor" | wc –l //查看逻辑CPU核数 
[root@rhel6u3-7 server]# ulimit -n 65535 //设置系统进程的最大打开文件数量

二、Nginx的HTTP服务器配置，Gzip配置。

http { 
*****************************以下是http服务器全局配置********************************* 
include mime.types; //主模块指令，实现对配置文件所包含的文件的设定，可以减少主配置文件的复杂度，DNS主配置文件中的zonerfc1912，acl基本上都是用的include语句 
default_type application/octet-stream; //核心模块指令，这里默认设置为二进制流，也就是当文件类型未定义时使用这种方式 
//下面代码为日志格式的设定，main为日志格式的名称，可自行设置，后面引用。 
log_format main '$remote_addr - $remote_user [$time_local] "$request" ' 
'$status $body_bytes_sent "$http_referer" ' 
'"$http_user_agent" "$http_x_forwarded_for"'; 
access_log logs/access.log main; //引用日志main 
client_max_body_size 20m; //设置允许客户端请求的最大的单个文件字节数 
client_header_buffer_size 32k; //指定来自客户端请求头的headebuffer大小 
client_body_temp_path /dev/shm/client_body_temp; //指定连接请求试图写入缓存文件的目录路径 
large_client_header_buffers 4 32k; //指定客户端请求中较大的消息头的缓存最大数量和大小，目前设置为4个32KB 
sendfile on; //开启高效文件传输模式 
tcp_nopush on; //开启防止网络阻塞 
tcp_nodelay on; //开启防止网络阻塞 
keepalive_timeout 65; //设置客户端连接保存活动的超时时间 
client_header_timeout 10; //用于设置客户端请求读取超时时间 
client_body_timeout 10; //用于设置客户端请求主体读取超时时间 
send_timeout 10; //用于设置相应客户端的超时时间 
//以下是httpGzip模块配置 
#httpGzip modules 
gzip on; //开启gzip压缩 
gzip_min_length 1k; //设置允许压缩的页面最小字节数 
gzip_buffers 4 16k; //申请4个单位为16K的内存作为压缩结果流缓存 
gzip_http_version 1.1; //设置识别http协议的版本,默认是1.1 
gzip_comp_level 2; //指定gzip压缩比,1-9 数字越小,压缩比越小,速度越快. 
gzip_types text/plain application/x-javascript text/css application/xml; //指定压缩的类型 
gzip_vary on; //让前端的缓存服务器存经过gzip压缩的页面

三、nginx的server虚拟主机配置

两种方式一种是直接在主配置文件中设置server字段配置虚拟主机，另外一种是使用include字段设置虚拟主机，这样可以减少主配置文件的复杂性。

*****************************以下是server主机设置********************************* 
server { 
listen 80; //监听端口为80 
server_name www.88181.com; //设置主机域名 
charset gb2312; //设置访问的语言编码 
access_log logs/www.88181.com.access.log main; //设置虚拟主机访问日志的存放路径及日志的格式为main 
location / { //设置虚拟主机的基本信息 
root sites/www; //设置虚拟主机的网站根目录 
index index.html index.htm; //设置虚拟主机默认访问的网页 
} 
location /status { // 查看nginx当前的状态情况,需要模块 “--with-http_stub_status_module”支持 
stub_status on; 
access_log /usr/local/nginx/logs/status.log; 
auth_basic "NginxStatus"; } 
} 
include /usr/local/nginx/server/www1.88181.com; //使用include字段设置server,内容如下 
[root@rhel6u3-7 ~]# cat /usr/local/nginx/server/www1.88181.com 
server { 
listen 80; 
server_name www1.88181.com; 
location / { 
root sites/www1; 
index index.html index.htm; 
} 
}
本篇文章来源于 Linux公社网站(www.linuxidc.com)  原文链接：http://www.linuxidc.com/Linux/2013-02/80069p2.htm
*/
/*
HTTP框架在读取、重载配置文件时定义了由ngx_http_module_t接口描述的8个阶段，HTTP框架在启动过程中会在每个阶段中调用ngx_http_module_t
中相应的方法。当然，如果ngx_http_module_t中的某个回调方法设为NULL空指针，那么HTTP框架是不会调用它的。
*/
/*
不过，这8个阶段的调用顺序与上述定义的顺序是不同的。在Nginx启动过程中，HTTP框架调用这些回调方法的实际顺序有可能是这样的（与nginx.conf配置项有关）：
1）create_main_conf
2）create_srv_conf
3）create_loc_conf
4）preconfiguration
5）init_main_conf
6）merge_srv_conf
7）merge_loc_conf
8）postconfiguration
当遇到http{}配置块时，HTTP框架会调用所有HTTP模块可能实现的create main conf、create_srv_conf、
create_loc_conf方法生成存储main级别配置参数的结构体；在遇到servero{}块时会再次调用所有HTTP模
块的create_srv conf、create loc_conf回调方法生成存储srv级别配置参数的结构体；在遇到location{}时
则会再次调用create_loc_conf回调方法生成存储loc级别配置参数的结构体。
例如如下配置:
{
  http {
     server{
        location xx {
            mytest;
        }
     }
  }
}
则在解析到http{}的时候会调用一次
location{}中，则在解析到http{}的时候调用一次create_loc_conf，解析到server{}的时候会调用一次create_loc_conf
解析到location{}的时候还会调用一次create_loc_conf

事实上，Nginx预设的配置项合并方法有10个，它们的行为与上述的ngx_conf_merge_
str- value是相似的。Nginx已经实现好的10个简单的配置项合并宏，它们的
参数类型与ngx_conf_merge_str_ value -致，而且除了ngx_conf_merge_bufs value外，它们
都将接收3个参数，分别表示父配置块参数、子配置块参数、默认值。
  Nginx预设的10种配置项合并宏
┏━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    配置项合并塞          ┃    意义                                                                  ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  合并可以使用等号(=)直接赋值的变量，并且该变量在create loc conf等分      ┃
┃ngx_conf_merge_value      ┃配方法中初始化为NGX CONF UNSET，这样类型的成员可以使用ngx_conf_           ┃
┃                          ┃merge_value合并宏                                                         ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  合并指针类型的变量，并且该变量在create loc conf等分配方法中初始化为     ┃
┃ngx_conf_merge_ptr_value  ┃NGX CONF UNSET PTR，这样类型的成员可以使用ngx_conf_merge_ptr_value        ┃
┃                          ┃合并宏                                                                    ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  合并整数类型的变量，并且该变量在create loc conf等分配方法中初始化为     ┃
┃ngx_conf_merge_uint_value ┃NGX CONF UNSET UINT，这样类型的成员可以使用ngx_conf_merge_uint_           ┃
┃                          ┃ value合并宏                                                              ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  合并表示毫秒的ngx_msec_t类型的变量，并且该变量在create loc conf等分     ┃
┃ngx_conf_merge_msec_value ┃配方法中初始化为NGX CONF UNSET MSEC，这样类型的成员可以使用ngx_           ┃
┃                          ┃conf_merge_msec_value合并宏                                               ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  舍并表示秒的timej类型的变量，并且该变量在create loc conf等分配方法中    ┃
┃ngx_conf_merge_sec_value  ┃初始化为NGX CONF UNSET，这样类型的成员可以使用ngx_conf_merge_sec_         ┃
┃                          ┃value合并宏                                                               ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  合并size-t等表示空间长度的变量，并且该变量在create- loc_ conf等分配方   ┃
┃ngx_conf_merge_size_value ┃法中初始化为NGX。CONF UNSET SIZE，这样类型的成员可以使用ngx_conf_         ┃
┃                          ┃merge_size_value合并宏                                                    ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  合并off等表示偏移量的变量，并且该变最在create loc conf等分配方法中      ┃
┃ngx_conf_merge_off_value  ┃初始化为NGX CONF UNSET．这样类型的成员可以使用ngx_conf_merge_off_         ┃
┃                          ┃value合并宏                                                               ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  ngx_str_t类型的成员可以使用ngx_conf_merge_str_value合并，这时传人的     ┃
┃ngx_conf_merge_str_value  ┃                                                                          ┃
┃                          ┃default参数必须是一个char水字符串                                         ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  ngx_bufs t类型的成员可以使用ngx_conf merge_str_value舍并宏，这时传人的  ┃
┃ngx_conf_merge_bufs_value ┃                                                                          ┃
┃                          ┃default参数是两个，因为ngx_bufsj类型有两个成员，所以需要传人两个默认值    ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_conf_merge_bitmask_   ┃  以二进制位来表示标志位的整型成员，可以使用ngx_conf_merge_bitmask_       ┃
┃value                     ┃value合并宏                                                               ┃
┗━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
*/ 

/*
当遇到http{}配置块时，HTTP框架会调用所有HTTP模块可能实现的create main conf、create_srv_conf、
create_loc_conf方法生成存储main级别配置参数的结构体；在遇到servero{}块时会再次调用所有HTTP模
块的create_srv conf、create loc_conf回调方法生成存储srv级别配置参数的结构体；在遇到location{}时
则会再次调用create_loc_conf回调方法生成存储loc级别配置参数的结构体。
例如如下配置:
{
  http {
     server{
        location xx { //location主要用于uri请求匹配
            mytest;
        }
     }
  }
}
则在解析到http{}的时候会调用一次
location{}中，则在解析到http{}的时候调用一次create_loc_conf，解析到server{}的时候会调用一次create_loc_conf
解析到location{}的时候还会调用一次create_loc_conf

以ngx_http_mytest_config_module为例:
HTTP框架在解析nginx.conf文件时只要遇到http{}、server{}、location{}配置块就会立刻分配一个
http_mytest_conf_t结构体。
*/

//所有的核心模块NGX_CORE_MODULE对应的上下文ctx为ngx_core_module_t，子模块，例如http{} NGX_HTTP_MODULE模块对应的为上下文为ngx_http_module_t
//events{} NGX_EVENT_MODULE模块对应的为上下文为ngx_event_module_t

//ginx主配置文件分为4部分，main（全局配置）、server（主机设置）、upstream（负载均衡服务器设）和location（URL匹配特定位置的设置），这四者关系为：server继承main，location继承server，upstream既不会继承其他设置也不会被继承。
//成员中的create一般在解析前执行函数，merge在函数后执行
typedef struct { //注意和ngx_http_conf_ctx_t结构配合        初始化赋值执行，如果为"http{}"中的配置，在ngx_http_block中, ,所有的NGX_HTTP_MODULE模块都在ngx_http_block中执行
    ngx_int_t   (*preconfiguration)(ngx_conf_t *cf); //解析配置文件前调用
    //一般用来把对应的模块加入到11个阶段对应的阶段去ngx_http_phases,例如ngx_http_realip_module的ngx_http_realip_init
    ngx_int_t   (*postconfiguration)(ngx_conf_t *cf); //完成配置文件的解析后调用  

/*当需要创建数据结构用于存储main级别（直属于http{...}块的配置项）的全局配置项时，可以通过create_main_conf回调方法创建存储全局配置项的结构体*/
    void       *(*create_main_conf)(ngx_conf_t *cf);
    char       *(*init_main_conf)(ngx_conf_t *cf, void *conf);//常用于初始化main级别配置项

/*当需要创建数据结构用于存储srv级别（直属于虚拟主机server{...}块的配置项）的配置项时，可以通过实现create_srv_conf回调方法创建存储srv级别配置项的结构体*/
    void       *(*create_srv_conf)(ngx_conf_t *cf);
// merge_srv_conf回调方法主要用于合并main级别和srv级别下的同名配置项
    char       *(*merge_srv_conf)(ngx_conf_t *cf, void *prev, void *conf);

    /*当需要创建数据结构用于存储loc级别（直属于location{...}块的配置项）的配置项时，可以实现create_loc_conf回调方法*/
    void       *(*create_loc_conf)(ngx_conf_t *cf);

    /*
    typedef struct {
           void * (*create_loc_conf) (ngx_conf_t *cf) ;
          char*(*merge_loc_conf) (ngx_conf_t *cf, void *prev,
    }ngx_http_module_t
        上面这段代码定义了create loc_conf方法，意味着HTTP框架会建立loc级别的配置。
    什么意思呢？就是说，如果没有实现merge_loc_conf方法，也就是在构造ngx_http_module_t
    时将merge_loc_conf设为NULL了，那么在4.1节的例子中server块或者http块内出现的
    配置项都不会生效。如果我们希望在server块或者http块内的配置项也生效，那么可以通过
    merge_loc_conf方法来实现。merge_loc_conf会把所属父配置块的配置项与子配置块的同名
    配置项合并，当然，如何合并取决于具体的merge_loc_conf实现。
        merge_loc_conf有3个参数，第1个参数仍然是ngx_conf_t *cf，提供一些基本的数据
    结构，如内存池、日志等。我们需要关注的是第2、第3个参数，其中第2个参数void *prev
    悬指解析父配置块时生成的结构体，而第3个参数void:-leconf则指出的是保存子配置块的结
    构体。
    */
    // merge_loc_conf回调方法主要用于合并srv级别和loc级别下的同名配置项
    char       *(*merge_loc_conf)(ngx_conf_t *cf, void *prev, void *conf); //nginx提供了10个预设合并宏，见上面
} ngx_http_module_t; //

//所有的核心模块NGX_CORE_MODULE对应的上下文ctx为ngx_core_module_t，子模块，例如http{} NGX_HTTP_MODULE模块对应的为上下文为ngx_http_module_t
//events{} NGX_EVENT_MODULE模块对应的为上下文为ngx_event_module_t
//http{}内部的所有属性都属于NGX_HTTP_MODULE
#define NGX_HTTP_MODULE           0x50545448   /* "HTTP" */

/*
NGX_MAIN_CONF：配置项可以出现在全局配置中，即不属于任何{}配置块。
NGX_EVET_CONF：配置项可以出现在events{}块内。
NGX_HTTP_MAIN_CONF： 配置项可以出现在http{}块内。
NGX_HTTP_SRV_CONF:：配置项可以出现在server{}块内，该server块必需属于http{}块。
NGX_HTTP_LOC_CONF：配置项可以出现在location{}块内，该location块必需属于server{}块。
NGX_HTTP_UPS_CONF： 配置项可以出现在upstream{}块内，该location块必需属于http{}块。
NGX_HTTP_SIF_CONF：配置项可以出现在server{}块内的if{}块中。该if块必须属于http{}块。
NGX_HTTP_LIF_CONF: 配置项可以出现在location{}块内的if{}块中。该if块必须属于http{}块。
NGX_HTTP_LMT_CONF: 配置项可以出现在limit_except{}块内,该limit_except块必须属于http{}块。
*/
#define NGX_HTTP_MAIN_CONF        0x02000000
#define NGX_HTTP_SRV_CONF         0x04000000
#define NGX_HTTP_LOC_CONF         0x08000000
#define NGX_HTTP_UPS_CONF         0x10000000
#define NGX_HTTP_SIF_CONF         0x20000000
#define NGX_HTTP_LIF_CONF         0x40000000 
#define NGX_HTTP_LMT_CONF         0x80000000


#define NGX_HTTP_MAIN_CONF_OFFSET  offsetof(ngx_http_conf_ctx_t, main_conf)
#define NGX_HTTP_SRV_CONF_OFFSET   offsetof(ngx_http_conf_ctx_t, srv_conf)
#define NGX_HTTP_LOC_CONF_OFFSET   offsetof(ngx_http_conf_ctx_t, loc_conf)


//注意ngx_http_get_module_main_conf ngx_http_get_module_loc_conf和ngx_http_get_module_ctx的区别

//获取该条request对应的自己所处的main,例如http{}的对应模块的配置信息
#define ngx_http_get_module_main_conf(r, module)                             \
    (r)->main_conf[module.ctx_index]
//获取该条request对应的自己所处的srv，例如server{} upstream{}的对应模块的配置信息
#define ngx_http_get_module_srv_conf(r, module)  (r)->srv_conf[module.ctx_index]

//ngx_http_get_module_ctx存储运行过程中的各种状态(例如读取后端数据，可能需要多次读取)  ngx_http_get_module_loc_conf获取该模块在local{}中的配置信息
//获取该条request对应的自己所处的loc，例如location{} 的对应模块的配置信息
#define ngx_http_get_module_loc_conf(r, module)  (r)->loc_conf[module.ctx_index]


#define ngx_http_conf_get_module_main_conf(cf, module)                        \
    ((ngx_http_conf_ctx_t *) cf->ctx)->main_conf[module.ctx_index]
#define ngx_http_conf_get_module_srv_conf(cf, module)                         \
    ((ngx_http_conf_ctx_t *) cf->ctx)->srv_conf[module.ctx_index]
#define ngx_http_conf_get_module_loc_conf(cf, module)                         \
    ((ngx_http_conf_ctx_t *) cf->ctx)->loc_conf[module.ctx_index]

#define ngx_http_cycle_get_module_main_conf(cycle, module)                    \
    (cycle->conf_ctx[ngx_http_module.index] ?                                 \
        ((ngx_http_conf_ctx_t *) cycle->conf_ctx[ngx_http_module.index])      \
            ->main_conf[module.ctx_index]:                                    \
        NULL)


#endif /* _NGX_HTTP_CONFIG_H_INCLUDED_ */
