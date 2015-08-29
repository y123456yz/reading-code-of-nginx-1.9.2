/*
服务器上的一些统计数据：

1)统计80端口连接数
netstat -nat|grep -i "80"|wc -l

2）统计httpd协议连接数
ps -ef|grep httpd|wc -l

3）、统计已连接上的，状态为“established
netstat -na|grep ESTABLISHED|wc -l

4)、查出哪个IP地址连接最多,将其封了.
netstat -na|grep ESTABLISHED|awk {print $5}|awk -F: {print $1}|sort|uniq -c|sort -r +0n

netstat -na|grep SYN|awk {print $5}|awk -F: {print $1}|sort|uniq -c|sort -r +0n

---------------------------------------------------------------------------------------------

1、查看apache当前并发访问数：
netstat -an | grep ESTABLISHED | wc -l

对比httpd.conf中MaxClients的数字差距多少。

2、查看有多少个进程数：
ps aux|grep httpd|wc -l

3、可以使用如下参数查看数据
server-status?auto

#ps -ef|grep httpd|wc -l
1388
统计httpd进程数，连个请求会启动一个进程，使用于Apache服务器。
表示Apache能够处理1388个并发请求，这个值Apache可根据负载情况自动调整。

#netstat -nat|grep -i "80"|wc -l
4341
netstat -an会打印系统当前网络链接状态，而grep -i "80"是用来提取与80端口有关的连接的，wc -l进行连接数统计。
最终返回的数字就是当前所有80端口的请求总数。

#netstat -na|grep ESTABLISHED|wc -l
376
netstat -an会打印系统当前网络链接状态，而grep ESTABLISHED 提取出已建立连接的信息。 然后wc -l统计。
最终返回的数字就是当前所有80端口的已建立连接的总数。

netstat -nat||grep ESTABLISHED|wc - 可查看所有建立连接的详细记录

查看Apache的并发请求数及其TCP连接状态：
　　Linux命令：
netstat -n | awk '/^tcp/ {++S[$NF]} END {for(a in S) print a, S[a]}'

（这条语句是从 新浪互动社区事业部 新浪互动社区事业部技术总监王老大那儿获得的，非常不错）返回结果示例：
　　LAST_ACK 5
　　SYN_RECV 30
　　ESTABLISHED 1597
　　FIN_WAIT1 51
　　FIN_WAIT2 504
　　TIME_WAIT 1057
　　其中的
SYN_RECV表示正在等待处理的请求数；
ESTABLISHED表示正常数据传输状态；
TIME_WAIT表示处理完毕，等待超时结束的请求数。

---------------------------------------------------------------------------------------------

查看Apache并发请求数及其TCP连接状态

查看httpd进程数（即prefork模式下Apache能够处理的并发请求数）：
　　Linux命令：

ps -ef | grep httpd | wc -l

　　返回结果示例：
　　1388
　　表示Apache能够处理1388个并发请求，这个值Apache可根据负载情况自动调整，我这组服务器中每台的峰值曾达到过2002。

查看Apache的并发请求数及其TCP连接状态：
　　Linux命令：

netstat -n | awk '/^tcp/ {++S[$NF]} END {for(a in S) print a, S[a]}'
返回结果示例：
　　LAST_ACK 5
　　SYN_RECV 30
　　ESTABLISHED 1597
　　FIN_WAIT1 51
　　FIN_WAIT2 504
　　TIME_WAIT 1057
　　其中的SYN_RECV表示正在等待处理的请求数；ESTABLISHED表示正常数据传输状态；TIME_WAIT表示处理完毕，等待超时结束的请求数。
　　状态：描述

　　CLOSED：无连接是活动 的或正在进行

　　LISTEN：服务器在等待进入呼叫

　　SYN_RECV：一个连接请求已经到达，等待确认

　　SYN_SENT：应用已经开始，打开一个连接

　　ESTABLISHED：正常数据传输状态

　　FIN_WAIT1：应用说它已经完成

　　FIN_WAIT2：另一边已同意释放

　　ITMED_WAIT：等待所有分组死掉

　　CLOSING：两边同时尝试关闭

　　TIME_WAIT：另一边已初始化一个释放

　　LAST_ACK：等待所有分组死

*/
