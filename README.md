# reading-code-of-nginx-1.9.2  
  
nginx高并发设计优秀思想应用于其他高并发代理中间件:   
===================================   
|#|内容|
|:-|:-|
|1|[高性能 -Nginx 多进程高并发、低时延、高可靠机制在百万级缓存 (redis、memcache) 代理中间件中的应用](https://xie.infoq.cn/article/2ee961483c66a146709e7e861)|
  
redis、nginx、memcache、twemproxy、mongodb等更多中间件，分布式系统，高性能服务端核心思想实现博客:   
===================================   
|#|内容|
|:-|:-|
|1|[中间件、高性能服务器、分布式存储等(redis、memcache、pika、rocksdb、mongodb、wiredtiger、高性能代理中间件)二次开发、性能优化，逐步整理文档说明并配合demo指导](https://github.com/y123456yz/middleware_development_learning)| 
  
    
### 对外演讲   
|#|对外演讲内容|
|:-|:-|
|1|[Qcon全球软件开发大会分享：OPPO万亿级文档数据库MongoDB集群性能优化实践](https://qcon.infoq.cn/2020/shenzhen/track/916)|
|2|[2019年mongodb年终盛会：OPPO百万级高并发MongoDB集群性能数十倍提升优化实践](https://www.shangyexinzhi.com/article/428874.html)|
|3|[2020年mongodb年终盛会：万亿级文档数据库集群性能优化实践](https://mongoing.com/archives/76151)|
|4|[2021年dbaplus分享：万亿级文档数据库集群性能优化实践](http://dbaplus.cn/news-162-3666-1.html)|
|5|[2021年度Gdevops全球敏捷运维峰会：PB级万亿数据库性能优化及最佳实践](https://gdevops.com/index.php?m=content&c=index&a=lists&catid=87)|
  
### 专栏  
|#|专栏名内容|
|:-|:-|
|1|[infoq专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.infoq.cn/profile/8D2D4D588D3D8A/publish)|
|2|[oschina专栏:《mongodb内核源码中文注释详细分析及性能优化实践系列》](https://my.oschina.net/u/4087916)|
|3|[知乎专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.zhihu.com/people/yang-ya-zhou-42/columns)|
|4|[itpub专栏:《mongodb内核源码设计实现、性能优化、最佳运维实践》](http://blog.itpub.net/column/150)|

   

分阶段分享  
===================================  
|#|阶段|内容|说明|
|:-|:-|:-|:-|
|1|[第一阶段|分布式缓存源码学习、二次开发、性能及稳定性优化|主要涉及网络实现、memcache redis重要模块源码分析、memcache redis性能稳定性优化及二次开发等|
|2|[第二阶段|高性能代理中间件开发(nginx、wemproxy、dbproxy、mongos等源码进行二次开发)|主要涉及代理中间件源码分析、性能优化、二次开发等|
|3|[第三阶段|分布式大容量nosql存储系统二次开发(突破缓存内存容量限制)|主要涉及pika、tendis源码、rocksdb存储引擎源码分析及pika性能优化等|
|4|[第四阶段|mongodb数据库内核开发|主要涉及mongodb源码、mongos源码、rocksdb存储引擎源码、wiredtiger存储引擎源码分析及二次开发|
   
  
  
## 第一阶段：分布式缓存开发、性能稳定性优化:    
|#|内容|
|:-|:-|
|1|[memcached源码详细分析注释，带详尽中文注释及函数调用关系](https://github.com/y123456yz/Reading-and-comprehense-redis-cluster)|
|2|[借助redis已有的网络相关.c和.h文件，半小时快速实现一个epoll异步网络框架，程序demo](https://github.com/y123456yz/middleware_development_learning/tree/master/%E7%AC%AC%E4%B8%80%E9%98%B6%E6%AE%B5-%E6%89%8B%E6%8A%8A%E6%89%8B%E6%95%99%E4%BD%A0%E5%81%9A%E5%88%86%E5%B8%83%E5%BC%8F%E7%BC%93%E5%AD%98%E4%BA%8C%E6%AC%A1%E5%BC%80%E5%8F%91%E3%80%81%E6%80%A7%E8%83%BD%E4%BC%98%E5%8C%96/%E5%BC%82%E6%AD%A5%E7%BD%91%E7%BB%9C%E6%A1%86%E6%9E%B6%E9%9B%B6%E5%9F%BA%E7%A1%80%E5%AD%A6%E4%B9%A0/asyn_network)|
|3|[借助redis已有的网络相关.c和.h文件，半小时快速实现一个epoll异步网络框架，程序demo-文档说明](https://github.com/y123456yz/middleware_development_learning/blob/master/%E7%AC%AC%E4%B8%80%E9%98%B6%E6%AE%B5-%E6%89%8B%E6%8A%8A%E6%89%8B%E6%95%99%E4%BD%A0%E5%81%9A%E5%88%86%E5%B8%83%E5%BC%8F%E7%BC%93%E5%AD%98%E4%BA%8C%E6%AC%A1%E5%BC%80%E5%8F%91%E3%80%81%E6%80%A7%E8%83%BD%E4%BC%98%E5%8C%96/%E5%BC%82%E6%AD%A5%E7%BD%91%E7%BB%9C%E6%A1%86%E6%9E%B6%E9%9B%B6%E5%9F%BA%E7%A1%80%E5%AD%A6%E4%B9%A0/asyn_network.md)|
|4|[阻塞、非阻塞程序demo](https://github.com/y123456yz/middleware_development_learning/tree/master/%E7%AC%AC%E4%B8%80%E9%98%B6%E6%AE%B5-%E6%89%8B%E6%8A%8A%E6%89%8B%E6%95%99%E4%BD%A0%E5%81%9A%E5%88%86%E5%B8%83%E5%BC%8F%E7%BC%93%E5%AD%98%E4%BA%8C%E6%AC%A1%E5%BC%80%E5%8F%91%E3%80%81%E6%80%A7%E8%83%BD%E4%BC%98%E5%8C%96/block_noblock_demo)|
|5|[阻塞、非阻塞、同步、异步、epoll说明](https://github.com/y123456yz/middleware_development_learning/blob/master/%E7%AC%AC%E4%B8%80%E9%98%B6%E6%AE%B5-%E6%89%8B%E6%8A%8A%E6%89%8B%E6%95%99%E4%BD%A0%E5%81%9A%E5%88%86%E5%B8%83%E5%BC%8F%E7%BC%93%E5%AD%98%E4%BA%8C%E6%AC%A1%E5%BC%80%E5%8F%91%E3%80%81%E6%80%A7%E8%83%BD%E4%BC%98%E5%8C%96/%E5%BC%82%E6%AD%A5%E7%BD%91%E7%BB%9C%E6%A1%86%E6%9E%B6%E9%9B%B6%E5%9F%BA%E7%A1%80%E5%AD%A6%E4%B9%A0/asyn_network.md)|
|6|[借助redis的配置解析模块，快速实现一个配置文件解析程序demo](https://github.com/y123456yz/middleware_development_learning/tree/master/%E7%AC%AC%E4%B8%80%E9%98%B6%E6%AE%B5-%E6%89%8B%E6%8A%8A%E6%89%8B%E6%95%99%E4%BD%A0%E5%81%9A%E5%88%86%E5%B8%83%E5%BC%8F%E7%BC%93%E5%AD%98%E4%BA%8C%E6%AC%A1%E5%BC%80%E5%8F%91%E3%80%81%E6%80%A7%E8%83%BD%E4%BC%98%E5%8C%96/redis%E6%BA%90%E7%A0%81%E5%88%86%E6%A8%A1%E5%9D%97%E5%88%86%E6%9E%90/%E5%9F%BA%E4%BA%8Eredis%E9%85%8D%E7%BD%AE%E6%96%87%E4%BB%B6%E8%A7%A3%E6%9E%90%E7%A8%8B%E5%BA%8F%EF%BC%8C%E5%BF%AB%E9%80%9F%E5%AE%9E%E7%8E%B0%E4%B8%80%E4%B8%AA%E9%85%8D%E7%BD%AE%E6%96%87%E4%BB%B6%E8%A7%A3%E6%9E%90%E7%A8%8B%E5%BA%8Fdemo)|
|7|[借助redis的日志模块，快速实现一个同步日志写、异步日志写程序demo](https://github.com/y123456yz/middleware_development_learning/tree/master/%E7%AC%AC%E4%B8%80%E9%98%B6%E6%AE%B5-%E6%89%8B%E6%8A%8A%E6%89%8B%E6%95%99%E4%BD%A0%E5%81%9A%E5%88%86%E5%B8%83%E5%BC%8F%E7%BC%93%E5%AD%98%E4%BA%8C%E6%AC%A1%E5%BC%80%E5%8F%91%E3%80%81%E6%80%A7%E8%83%BD%E4%BC%98%E5%8C%96/redis%E6%BA%90%E7%A0%81%E5%88%86%E6%A8%A1%E5%9D%97%E5%88%86%E6%9E%90/%E5%9F%BA%E4%BA%8Eredis%E6%97%A5%E5%BF%97%E4%BB%A3%E7%A0%81%EF%BC%8C%E5%BF%AB%E9%80%9F%E5%AE%9E%E7%8E%B0%E6%97%A5%E5%BF%97%E5%90%8C%E6%AD%A5%E5%86%99%E5%92%8C%E5%BC%82%E6%AD%A5%E5%86%99%EF%BC%8C%E4%BD%93%E9%AA%8C%E5%90%8C%E6%AD%A5%E5%86%99%E5%92%8C%E5%BC%82%E6%AD%A5%E5%86%99%E5%8C%BA%E5%88%AB)|
|8|[借助redis的bio模块，快速实现线程池组demo](https://github.com/y123456yz/middleware_development_learning/tree/master/%E7%AC%AC%E4%B8%80%E9%98%B6%E6%AE%B5-%E6%89%8B%E6%8A%8A%E6%89%8B%E6%95%99%E4%BD%A0%E5%81%9A%E5%88%86%E5%B8%83%E5%BC%8F%E7%BC%93%E5%AD%98%E4%BA%8C%E6%AC%A1%E5%BC%80%E5%8F%91%E3%80%81%E6%80%A7%E8%83%BD%E4%BC%98%E5%8C%96/redis%E6%BA%90%E7%A0%81%E5%88%86%E6%A8%A1%E5%9D%97%E5%88%86%E6%9E%90/%E5%9F%BA%E4%BA%8Eredis%E7%9A%84bio%E4%BB%A3%E7%A0%81%EF%BC%8C%E5%BF%AB%E9%80%9F%E5%AE%9E%E7%8E%B0%E4%B8%80%E4%B8%AA%E7%BA%BF%E7%A8%8B%E6%B1%A0demo)|
|9|[常用高并发网络线程模型设计(最全高并发网络IO线程模型设计及优化)](https://my.oschina.net/u/4087916/blog/4431422)  |
  
  
## 第二阶段：高性能代理中间件开发  
|#|内容|
|:-|:-|
|1|[redis、memcached缓存代理twemproxy源码详细分析注释，带详尽中文注释及函数调用关系](https://github.com/y123456yz/Reading-and-comprehense-twemproxy0.4.1)|
|2|[nginx-1.9.2源码通读分析注释，带详尽函数中文分析注释](https://github.com/y123456yz/reading-code-of-nginx-1.9.2)|
|3|[nginx多进程、高性能、低时延、高可靠机制应用于缓存中间件twemproxy，对twemproxy进行多进程优化改造，提升TPS，降低时延，代理中间件长连接百万TPS/短连接五十万TPS实现原理](https://github.com/y123456yz/middleware_development_learning/blob/master/%E7%AC%AC%E4%BA%8C%E9%98%B6%E6%AE%B5-%E6%89%8B%E6%8A%8A%E6%89%8B%E6%95%99%E4%BD%A0%E5%81%9A%E9%AB%98%E6%80%A7%E8%83%BD%E4%BB%A3%E7%90%86%E4%B8%AD%E9%97%B4%E4%BB%B6%E5%BC%80%E5%8F%91/nginx%E5%A4%9A%E8%BF%9B%E7%A8%8B%E9%AB%98%E5%B9%B6%E5%8F%91%E4%BD%8E%E6%97%B6%E5%BB%B6%E6%9C%BA%E5%88%B6%E5%9C%A8%E7%BC%93%E5%AD%98%E4%BB%A3%E7%90%86%E4%B8%AD%E9%97%B4%E4%BB%B6twemproxy%E4%B8%AD%E7%9A%84%E5%BA%94%E7%94%A8/nginx_twemproxy.md)|
|4|[常用高并发网络线程模型设计](https://my.oschina.net/u/4087916/blog/4431422)|
  
  
  
## 第三阶段：wiredtiger、rocksdb存储引擎开发，大容量nosql存储系统二次开发   
|#|内容|
|:-|:-|
|1|[文档数据库mongodb kv存储引擎wiredtiger源码详细分析注释](https://github.com/y123456yz/reading-and-annotate-wiredtiger-3.0.0)|
|2|[rocksdb-6.1.2 KV存储引擎源码中文注释分析](https://github.com/y123456yz/reading-and-annotate-rocksdb-6.1.2)|
|3|[百万级高并发mongodb集群性能数十倍提升优化实践(上篇)](https://my.oschina.net/u/4087916/blog/3141909)|
   
   
## 第四阶段：mongodb数据库源码学习，二次开发等   
###《mongodb内核源码设计与实现》源码模块化分析  
#### 第一阶段：单机内核源码分析  
![mongodb单机模块化架构图](/img/单机模块架构.png)  
|#|单机模块名|核心代码中文注释|说明|模块文档输出|
|:-|:-|:-|:-|:-|
|1|[网络收发处理(含工作线程模型)](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/blob/master/mongo/README.md#L8)|网络处理模块核心代码实现(100%注释分析)|完成ASIO库、网络数据收发、同步线程模型、动态线程池模型等功能|[详见infoq专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.infoq.cn/profile/8D2D4D588D3D8A/publish)|
|2|[command命令处理模块](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/blob/master/mongo/README.md#L85)|命令处理相关模块源码分析(100%注释分析)|完成命令注册、命令执行、命令分析、命令统计等功能|[详见oschina专栏:《mongodb内核源码中文注释详细分析及性能优化实践系列》](https://www.infoq.cn/profile/8D2D4D588D3D8A/publish)|
|3|[write写(增删改操作)模块](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/blob/master/mongo/README.md#L115))|增删改写模块(100%注释分析)|完成增删改对应命令解析回调处理、事务封装、storage存储模块对接等功能|[详见知乎专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.zhihu.com/people/yang-ya-zhou-42/columns)|
|4|[query查询引擎模块](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/blob/master/mongo/README.md#L131))|query查询引擎模块(核心代码注释)|完成expression tree解析优化处理、querySolution生成、最优索引选择等功能|[详见知乎专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.zhihu.com/people/yang-ya-zhou-42/columns)|
|5|[concurrency并发控制模块](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/tree/master/mongo/src/mongo/db/concurrency)|并发控制模块(核心代码注释)|完成信号量、读写锁、读写意向锁相关实现及封装|[详见infoq专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.infoq.cn/profile/8D2D4D588D3D8A/publish)|
|6|[index索引模块](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/blob/master/mongo/README.md#L240)|index索引模块(100%注释分析)|完成索引解析、索引管理、索引创建、文件排序等功能|[详见oschina专栏:《mongodb内核源码中文注释详细分析及性能优化实践系列》](https://www.infoq.cn/profile/8D2D4D588D3D8A/publish)|
|7|[storage存储模块](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6/blob/master/mongo/README.md#L115))|storage存储模块(100%注释分析)|完成存储引擎注册、引擎选择、中间层实现、KV实现、wiredtiger接口实现等功能|[详见知乎专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://www.zhihu.com/people/yang-ya-zhou-42/columns)|
|8|[wiredtiger存储引擎](https://github.com/y123456yz/reading-and-annotate-wiredtiger-3.0.0)) |wiredtiger存储引擎设计与实现专栏分析(已分析部分)|完成expression tree解析优化处理、querySolution生成、最优索引选择等功能|[详见知乎专栏：《MongoDB内核源码设计、性能优化、最佳运维实践》](https://github.com/y123456yz/reading-and-annotate-wiredtiger-3.0.0)|
  
#### 第二阶段：复制集内核源码分析(已分析部分源码，待整理)  
     
     
#### 第三阶段：sharding分片内核源码分析(已分析部分源码，待整理)  
  
  
### <<千万级峰值tps/十万亿级数据量文档数据库内核研发及运维之路>>   
|#|文章内容|
|:-|:-|
|1|[盘点 2020 - 我要为分布式数据库 mongodb 在国内影响力提升及推广做点事](https://xie.infoq.cn/article/372320c6bb93ddc5b7ecd0b6b)|
|2|[万亿级数据库 MongoDB 集群性能数十倍提升及机房多活容灾实践](https://xie.infoq.cn/article/304a748ad3dead035a449bd51)|
|3|[Qcon现代数据架构 -《万亿级数据库 MongoDB 集群性能数十倍提升优化实践》核心 17 问详细解答](https://xie.infoq.cn/article/0c51f3951f3f10671d7d7123e)|
|4|[话题讨论 - mongodb 相比 mysql 拥有十大核心优势，为何国内知名度不高？](https://xie.infoq.cn/article/180d98535bfa0c3e71aff1662)|
|5|[万亿级数据库 MongoDB 集群性能数十倍提升及机房多活容灾实践](https://xie.infoq.cn/article/304a748ad3dead035a449bd51)|
|6|[百万级高并发mongodb集群性能数十倍提升优化实践(上篇)](https://my.oschina.net/u/4087916/blog/3141909)|
|7|[Mongodb网络传输处理源码实现及性能调优-体验内核性能极致设计](https://my.oschina.net/u/4087916/blog/4295038)|
|8|[常用高并发网络线程模型设计及mongodb线程模型优化实践(最全高并发网络IO线程模型设计及优化)](https://my.oschina.net/u/4087916/blog/4431422) |
|9|[Mongodb集群搭建一篇就够了-复制集模式、分片模式、带认证、不带认证等(带详细步骤说明)](https://my.oschina.net/u/4087916/blog/4661542)|
|10|[Mongodb特定场景性能数十倍提升优化实践(记一次mongodb核心集群雪崩故障)](https://blog.51cto.com/14951246)|
|11|[mongodb内核源码设计实现、性能优化、最佳运维系列-mongodb网络传输层模块源码实现二](https://zhuanlan.zhihu.com/p/265701877)|
|12|[为何需要对开源mongodb社区版本做二次开发，需要做哪些必备二次开发](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/development_mongodb.md)|
|13|[对开源mongodb社区版本做二次开发收益列表](https://my.oschina.net/u/4087916/blog/3063529)|
|14|[盘点 2020 - 我要为分布式数据库 mongodb 在国内影响力提升及推广做点事](https://xie.infoq.cn/article/372320c6bb93ddc5b7ecd0b6b)|
|15|[mongodb内核源码实现、性能调优、最佳运维实践系列-数百万行mongodb内核源码阅读经验分享](https://my.oschina.net/u/4087916/blog/4696104)|
|16|[mongodb内核源码实现、性能调优、最佳运维实践系列-mongodb网络传输层模块源码实现一](https://my.oschina.net/u/4087916/blog/4295038)|
|17|[mongodb内核源码实现、性能调优、最佳运维实践系列-mongodb网络传输层模块源码实现二](https://my.oschina.net/u/4087916/blog/4674521)|
|18|[mongodb内核源码实现、性能调优、最佳运维实践系列-mongodb网络传输层模块源码实现三](https://my.oschina.net/u/4087916/blog/4678616)|
|19|[mongodb内核源码实现、性能调优、最佳运维实践系列-mongodb网络传输层模块源码实现四](https://my.oschina.net/u/4087916/blog/4685419)|
|20|[mongodb内核源码实现、性能调优、最佳运维实践系列-command命令处理模块源码实现一](https://my.oschina.net/u/4087916/blog/4709503)|
|21|[mongodb内核源码实现、性能调优、最佳运维实践系列-command命令处理模块源码实现二](https://my.oschina.net/u/4087916/blog/4748286)|
|22|[mongodb内核源码实现、性能调优、最佳运维实践系列-command命令处理模块源码实现三](https://my.oschina.net/u/4087916/blog/4782741)|
|23|[mongodb内核源码实现、性能调优、最佳运维实践系列-记mongodb详细表级操作及详细时延统计实现原理(教你如何快速进行表级时延问题分析)](https://xie.infoq.cn/article/3184cdc42c26c86e2749c3e5c)|
|24|[mongodb内核源码实现、性能调优、最佳运维实践系列-Mongodb write写(增、删、改)模块设计与实现](https://my.oschina.net/u/4087916/blog/4974132)|
   
    
## 其他分享   
|#|内容|
|:-|:-|
|1|[阿里巴巴分布式消息队列中间件rocketmq-3.4.6源码分析](https://github.com/y123456yz/reading-and-annotate-rocketmq-3.4.6)|
|2|[服务器时延统计工具tcprstat,增加时延阈值统计，记录超过阈值的包个数，并把数据包时间戳记录到日志文件，这样可以根据时间戳快速定位到抓包文件中对应的包，从而可以快速定位到大时延包，避免了人肉搜索抓包文件，提高问题排查效率](https://github.com/y123456yz/tcprstat)|
|3|[linux内核网络协议栈源码阅读分析注释](https://github.com/y123456yz/Reading-and-comprehense-linux-Kernel-network-protocol-stack)|
|4|[docker-17.05.0源码中文注释详细分析](https://github.com/y123456yz/reading-and-annotate-docker-17.05.0)|
|5|[lxc源码详细注释分析](https://github.com/y123456yz/reading-and-annotate-lxc-1.0.9)|
|6|[source insight代码中文注释乱码、背景色等配置调整](https://github.com/y123456yz/middleware_development_learning/tree/master/source%20insight%20configure)|
|7|[linux内核协议栈TCP time_wait原理、优化、副作用](https://my.oschina.net/u/4087916/blog/3051356)|
|8|[为何需要对开源社区版本mongodb做二次开发，需要做哪些二次开发](https://github.com/y123456yz/middleware_development_learning/blob/master/%E7%AC%AC%E5%9B%9B%E9%98%B6%E6%AE%B5-mongodb%E6%95%B0%E6%8D%AE%E5%BA%93/development_mongodb.md)|
|9|[在线引流工具Tcpcopy原理、环境搭建、使用、采坑](https://my.oschina.net/u/4087916/blog/3064268)|
  
  
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


  
  
nginx高性能特性应用于其他项目  
===================================    
https://github.com/y123456yz/reading-code-of-nginx-1.9.2/blob/master/%E5%80%9F%E9%89%B4nginx%E7%89%B9%E6%80%A7%E5%BA%94%E7%94%A8%E4%BA%8E%E5%85%B6%E4%BB%96%E9%A1%B9%E7%9B%AE-Nginx%E5%A4%9A%E8%BF%9B%E7%A8%8B%E9%AB%98%E5%B9%B6%E5%8F%91%E3%80%81%E4%BD%8E%E6%97%B6%E5%BB%B6%E3%80%81%E9%AB%98%E5%8F%AF%E9%9D%A0%E6%9C%BA%E5%88%B6%E5%9C%A8%E7%BC%93%E5%AD%98%E4%BB%A3%E7%90%86%E4%B8%AD%E7%9A%84%E5%BA%94%E7%94%A8.docx  

