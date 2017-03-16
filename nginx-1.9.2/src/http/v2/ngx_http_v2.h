/*
 * Copyright (C) Nginx, Inc.
 * Copyright (C) Valentin V. Bartenev
 */


#ifndef _NGX_HTTP_V2_H_INCLUDED_
#define _NGX_HTTP_V2_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/*
一。帧通用格式

下图为HTTP/2帧通用格式：帧头+负载的比特位通用结构：

+-----------------------------------------------+
|                Length (24)                    |
+---------------+---------------+---------------+
|  Type (8)     |  Flags (8)    |
+-+-------------+---------------+-------------------------------+
|R|                Stream Identifier (31)                       |
+=+=============================================================+
|                  Frame Payload (0...)                       ...
+---------------------------------------------------------------+
帧头为固定的9个字节（(24+8+8+1+31)/8=9）呈现，变化的为帧的负载(payload)，负载内容是由帧类型（Type）定义。

    帧长度Length：无符号的自然数，24个比特表示，仅表示帧负载所占用字节数，不包括帧头所占用的9个字节。默认大
小区间为为0~16,384(2^14)，一旦超过默认最大值2^14(16384)，发送方将不再允许发送，除非接收到接收方定义的
SETTINGS_MAX_FRAME_SIZE（一般此值区间为2^14 ~ 2^24）值的通知。
    帧类型Type：8个比特表示，定义了帧负载的具体格式和帧的语义，HTTP/2规范定义了10个帧类型，这里不包括实验类型帧和扩展类型帧
    帧的标志位Flags：8个比特表示，服务于具体帧类型，默认值为0x0。有一个小技巧需要注意，一般来讲，8个比特可以容纳8个不同的标
志，比如，PADDED值为0x8，二进制表示为00001000；END_HEADERS值为0x4，二进制表示为00000100；END_STREAM值为0X1，二进制为00000001。
可以同时在一个字节中传达三种标志位，二进制表示为00001101，即0x13。因此，后面的帧结构中，标志位一般会使用8个比特表示，若某位
不确定，使用问号?替代，表示此处可能会被设置标志位
    帧保留比特为R：在HTTP/2语境下为保留的比特位，固定值为0X0
    流标识符Stream Identifier：无符号的31比特表示无符号自然数。0x0值表示为帧仅作用于连接，不隶属于单独的流。
    关于帧长度，需要稍加关注： - 0 ~ 2^14（16384）为默认约定长度，所有端点都需要遵守 - 2^14 (16,384) ~ 2^24-1(16,777,215)
此区间数值，需要接收方设置SETTINGS_MAX_FRAME_SIZE参数单独赋值 - 一端接收到的帧长度超过设定上限或帧太小，需要发送FRAME_SIZE_ERR
错误 - 当帧长错误会影响到整个连接状态时，须以连接错误对待之；比如HEADERS，PUSH_PROMISE，CONTINUATION，SETTINGS，以及帧标识符
不该为0的帧等，都需要如此处理 - 任一端都没有义务必须使用完一个帧的所有可用空间 - 大帧可能会导致延迟，针对时间敏感的帧，比
如RST_STREAM, WINDOW_UPDATE, PRIORITY，需要快速发送出去，以免延迟导致性能等问题

帧的标志位:
Frame Types vs Flags and Stream ID
    Table represent possible combination of frame types and flags.
    Last column -- Stream ID of frame types (x -- sid >= 1, 0 -- sid = 0)


                        +-END_STREAM 0x1
                        |   +-ACK 0x1
                        |   |   +-END_HEADERS 0x4
                        |   |   |   +-PADDED 0x8
                        |   |   |   |   +-PRIORITY 0x20
                        |   |   |   |   |        +-stream id (value)
                        |   |   |   |   |        |
    | frame type\flag | V | V | V | V | V |   |  V  |
    | --------------- |:-:|:-:|:-:|:-:|:-:| - |:---:|
    | DATA            | x |   |   | x |   |   |  x  |
    | HEADERS         | x |   | x | x | x |   |  x  |
    | PRIORITY        |   |   |   |   |   |   |  x  |
    | RST_STREAM      |   |   |   |   |   |   |  x  |
    | SETTINGS        |   | x |   |   |   |   |  0  |
    | PUSH_PROMISE    |   |   | x | x |   |   |  x  |
    | PING            |   | x |   |   |   |   |  0  |
    | GOAWAY          |   |   |   |   |   |   |  0  |
    | WINDOW_UPDATE   |   |   |   |   |   |   | 0/x |
    | CONTINUATION    |   |   | x | x |   |   |  x  |


三。HTTP/2定义的帧

规范定义了10个正式使用到帧类型，扩展实验类型的ALTSVC、BLOCKED等不在介绍之列。下面按照优先使用顺序重新排排序。

1. SETTINGS

+-----------------------------------------------+
|                Length (24)                    |
+---------------+---------------+---------------+
|     0x4 (8)   | 0000 000? (8) |
+-+-------------+---------------+-------------------------------+
|R|                Stream Identifier/0x0 (32)                   |
+=+=============================+===============================+
|       Identifier (16)         |
+-------------------------------+-------------------------------+
|                        Value (32)                             |
+---------------------------------------------------------------+
|       Identifier (16)         |
+-------------------------------+-------------------------------+
|                        Value (32)                             |
+---------------------------------------------------------------+  
设置帧，接收者向发送者通告己方设定，服务器端在连接成功后必须第一个发送的帧。

字段Identifier定义了如下参数： 
    - SETTINGS_HEADER_TABLE_SIZE (0x1)，通知接收者报头表的字节数最大值，报头块解码使用；初始值为4096个字节，默认可不用设置 
    - SETTINGS_ENABLE_PUSH (0x2)，0：禁止服务器推送，1：允许推送；其它值非法，PROTOCOL_ERROR错误 
    - SETTINGS_MAX_CONCURRENT_STREAMS (0x3)，发送者允许可打开流的最大值，建议值100，默认可不用设置；0值为禁止创建新流 
    - SETTINGS_INITIAL_WINDOW_SIZE (0x4)，发送端流控窗口大小，默认值2^16-1 (65,535)个字节大小；最大值为2^31-1个字节大小，
        若溢出需要报FLOW_CONTROL_ERROR错误 
    - SETTINGS_MAX_FRAME_SIZE (0x5)，单帧负载最大值，默认为2^14（16384）个字节，两端所发送帧都会收到此设定影响；
        值区间为2^14（16384）-2^24-1(16777215) 
    - SETTINGS_MAX_HEADER_LIST_SIZE (0x6)，发送端通告自己准备接收的报头集合最大值，即字节数。此值依赖于未压缩
        报头字段，包含字段名称、字段值以及每一个报头字段的32个字节的开销等；文档里面虽说默认值不受限制，因为受
        到报头集合大小不限制的影响，个人认为不要多于2 SETTINGS_MAX_FRAME_SIZE（即2^142=32768），否则包头太大，隐患多多。

标志位： * ACK (0x1)，表示接收者已经接收到SETTING帧，作为确认必须设置此标志位，此时负载为空，否则需要报FRAME_SIZE_ERROR错误

注意事项： 
- 在连接开始阶段必须允许发送SETTINGS帧，但不一定要发送 
- 在连接的生命周期内可以允许任一端点发送 
- 接收者不需要维护参数的状态，只需要记录当前值即可 
- SETTINGS帧仅作用于当前连接，不针对单个流，因此流标识符为0x0 
- 不完整或不合规范的SETTINGS帧需要抛出PROTOCOL_ERROR类型连接错误 
- 负载字节数为6个字节的倍数，6*N (N>=0)

处理流程： 
- 发送端发送需要两端都需要携带有遵守的SETTINGS设置帧，不能够带有ACK标志位 
- 接收端接收到无ACK标志位的SETTINGS帧，必须按照帧内字段出现顺序一一进行处理，中间不能够处理其它帧 
- 接收端处理时，针对不受支持的参数需忽略 
- 接收端处理完毕之后，必须响应一个包含有ACK确认标志位、无负载的SETTINGS帧 
- 发送端接收到确认的SETTINGS帧，表示两端设置已生效 
- 发送端等待确认若超时，报SETTINGS_TIMEOUT类型连接错误


2. HEADER

+-----------------------------------------------+
|                Length (24)                    |
+---------------+---------------+---------------+
|    0x1 (8)    | 00?0 ??0? (8) |
+-+-------------+---------------+-------------------------------+
|R|                Stream Identifier (31)                       |
+=+=============+===============================================+
|Pad Length? (8)|
+-+-------------+-----------------------------------------------+
|E|                 Stream Dependency? (31)                     |
+-+-------------+-----------------------------------------------+
|  Weight? (8)  |
+-+-------------+-----------------------------------------------+
|                   Header Block Fragment (*)                 ...
+---------------------------------------------------------------+
|                           Padding (*)                       ...
+---------------------------------------------------------------+
报头主要载体，请求头或响应头，同时呢也用于打开一个流，在流处于打开"open"或者远程半关闭"half closed (remote)"状态都可以发送。

字段列表： 
- Pad Length：受制于PADDED标志控制是否显示，8个比特表示填充的字节数。 
- E：一个比特表示流依赖是否专用，可选项，只在流优先级PRIORITY被设置时有效 
- Stream Dependency：31个比特表示流依赖，只在流优先级PRIORITY被设置时有效 
Weight：8个比特（一个字节）表示无符号的自然数流优先级，值范围自然是(1~256)，或称之为权重。只在流优先级PRIORITY被设置时有效 
- Header Block Fragment：报头块分片 
- Padding：填充的字节，受制于PADDED标志控制是否显示，长度由Pad Length字段决定

所需标志位： 
END_STREAM (0x1): 报头块为最后一个，意味着流的结束。后续可紧接着CONTINUATION帧在当前的流中，需要把CONTINUATION帧作为HEADERS帧的一部分对待 
END_HEADERS (0x4): 此报头帧不需分片，完整的一个帧。后续不再需要CONTINUATION帧帮忙凑齐。若没有此标志的HEADER帧，后续帧必须是以CONTINUATION帧传递在当前的流中，否则接收者需要响应PROTOCOL_ERROR类型的连接错误。 
PADDED (0x8): 需要填充的标志 
PRIORITY (0x20): 优先级标志位，控制独立标志位E，流依赖，和流权重。

注意事项： 
- 其负载为报头块分片，若内容过大，需要借助于CONTINUATION帧继续传输。若流标识符为0x0，结束段需要返回
    PROTOCOL_ERROR连接异常。HEADERS帧包含优先级信息是为了避免潜在的不同流之间优先级顺序的干扰。 
- 其实一般来讲，报文头部不大的情况下，一个HEADERS就可以完成了，特殊情况就是Cookie字段超过16KiB大小，不常见。


3. CONTINUATION

+-----------------------------------------------+
|                Length (24)                    |
+---------------+---------------+---------------+
|  0x9 (8)      |  0x0/0x4  (8) |
+-+-------------+---------------+-------------------------------+
|R|                Stream Identifier (32)                       |
+=+=============================================================+
|                  Header Block Fragment (*)                    |
+---------------------------------------------------------------+
字段列表： - Header Block Fragment，用于协助HEADERS/PUSH_PROMISE等单帧无法包含完整的报头剩余部分数据。

注意事项： 
- 一个HEADERS/PUSH_PROMISE帧后面会跟随零个或多个CONTINUATION，只要上一个帧没有设置END_HEADERS标志位，就不算一个帧完整数据的结束。 
- 接收端处理此种情况，从开始的HEADERS/PUSH_PROMISE帧到最后一个包含有END_HEADERS标志位帧结束，合并的数据才算是一份完整数据拷贝 
- 在HEADERS/PUSH_PROMISE（没有END_HEADERS标志位）和CONTINUATION帧中间，是不能够掺杂其它帧的，否则需要报PROTOCOL_ERROR错误

标志位： * END_HEADERS(0X4)：表示报头块的最后一个帧，否则后面还会跟随CONTINUATION帧。


4. DATA

一个或多个DATA帧作为请求、响应内容载体，较为完整的结构如下：

+-----------------------------------------------+
|                Length (24)                    |
+---------------+---------------+---------------+
| 0x0 (8)       | 0000 ?00? (8) |
+-+-------------+---------------+-------------------------------+
|R|                Stream Identifier (31)                       |
+=+=============+===============================================+
|Pad Length? (8)|
+---------------+-----------------------------------------------+
|                            Data (*)                         ...
+---------------------------------------------------------------+
|                          Padding? (*)                       ...
+---------------------------------------------------------------+
字段： 
Pad Length: 一个字节表示填充的字节长度。取决于PADDED标志是否被设置. 
Data: 这里是应用数据，真正大小需要减去其他字段（比如填充长度和填充内容）长度。 * 
Padding: 填充内容为若干个0x0字节，受PADDED标志控制是否显示。接收端处理时可忽略验证填充内容。
    若验证，可以对非0x0内容填充回应PROTOCOL_ERROR类型连接异常。

标志位： 
END_STREAM (0x1): 标志此帧为对应标志流最后一个帧，流进入了半关闭/关闭状态。 
PADDED (0x8): 负载需要填充，Padding Length + Data + Padding组成。

注意事项： 
- 若流标识符为0x0，接收者需要响应PROTOCOL_ERROR连接错误 
- DATA帧只能在流处于"open" or "half closed (remote)"状态时被发送出去，否则接收端必须响应一个STREAM_CLOSED的
    连接错误。若填充长度不小于负载长度，接收端必须响应一个PROTOCOL_ERROR连接错误。


5. PUSH_PROMISE

+-----------------------------------------------+
|                Length (24)                    |
+---------------+---------------+---------------+
|  0x5 (8)      | 0000 ??00 (8) |
+-+-------------+---------------+-------------------------------+
|R|                Stream Identifier (32)                       |
+=+=============================================================+
|Pad Length? (8)|
+-+-------------+-----------------------------------------------+
|R|                Promised Stream ID (31)                      |
+-+-------------------------------------------------------------+
|                  Header Block Fragment (*)                . . .
+---------------------------------------------------------------+
|                           Padding (*)                     . . .
+---------------------------------------------------------------+
服务器端通知对端初始化一个新的推送流准备稍后推送数据： 
- 要求推送流为打开或远端半关闭（half closed (remote)）状态，否则报PROTOCOL_ERROR错误： 
- 承诺的流不一定要按照其流打开顺序进行使用，仅用作稍后使用 
- 受对端所设置SETTINGS_ENABLE_PUSH标志位决定是否发送，否则作为PROTOCOL_ERROR错误对待 
- 接收端一旦拒绝接收推送，会发送RST_STREAM帧告知对方推送无效

字段列表： 
- Promised Stream ID，31个比特表示无符号的自然数，为推送保留的流标识符，后续适用于发送推送数据 
- Header Block Fragment，请求头部字段值，可看做是服务器端模拟客户端发起一次资源请求

标志位： 
END_HEADERS（0x4/00000010），此帧包含完整的报头块，不用后面跟随CONTINUATION帧了 
PADDED（0x8/00000100），填充开关，决定了下面的Pad Length和Padding是否要填充，具体和HEADERS帧内容一致，不多说

6. PING

优先级帧，类型值为0x6，8个字节表示。发送者测量最小往返时间，心跳机制用于检测空闲连接是否有效。

+-----------------------------------------------+
|                0x8 (24)                       |
+---------------+---------------+---------------+
|  0x6 (8)      | 0000 000? (8) |
+-+-------------+---------------+-------------------------------+
|R|                          0x0 (32)                           |
+=+=============================================================+
|                        Opaque Data (64)                       |
+---------------------------------------------------------------+
字段列表：
- Opaque Data：8个字节负载，值随意填写。

标志位： * 
ACK(0x1)：一旦设置，表示此PING帧为接收者响应的PING帧，非发送者。

注意事项： 
- PING帧发送方ACK标志位为0x0，接收方响应的PING帧ACK标志位为0x1。否则直接丢弃。其优先级要高于其它类型帧。 
- PING帧不和具体流相关联，若流标识符为0x0，接收方需要响应PROTOCOL_ERROR类型连接错误。 
- 超过负载长度，接收者需要响应FRAME_SIZE_ERROR类型连接错误。


7. PRIORITY

优先级帧，类型值为0x2，5个字节表示。表达了发送方对流优先级权重的建议值，在流的任何状态下都可以发送，包括空闲或关闭的流。

+-----------------------------------------------+
|                   0x5 (24)                    |
+---------------+---------------+---------------+
|   0x2 (8)     |    0x0 (8)    |
+-+-------------+---------------+-------------------------------+
|R|                  Stream Identifier (31)                     |
+=+=============================================================+
|E|                  Stream Dependency (31)                     |
+-+-------------+-----------------------------------------------+
| Weight (8)    |
+---------------+
字段列表： 
- E：流是否独立 
- Stream Dependency：流依赖，值为流的标识符，自然也是31个比特表示。 
- Weight：权重/优先级，一个字节表示自然数，范围1~256

注意事项：
- PRIORITY帧其流标识符为0x0，接收方需要响应PROTOCOL_ERROR类型的连接错误。 
- PRIORITY帧可在流的任何状态下发送，但限制是不能够在一个包含有报头块连续的帧里面出现，
其发送时刻需要，若流已经结束，虽然可以发送，但已经没有什么效果。 

- 超过5个字节PRIORITY帧接收方响应FRAME_SIZE_ERROR类型流错误。

8. WINDOW_UPDATE

+-----------------------------------------------+
|                0x4 (24)                       |
+---------------+---------------+---------------+
|   0x8 (8)     |    0x0 (8)    |
+-+-------------+---------------+-------------------------------+
|R|                Stream Identifier (31)                       |
+=+=============================================================+
|R|              Window Size Increment (31)                     |
+-+-------------------------------------------------------------+
流量控制帧，作用于单个流以及整个连接，但只能影响两个端点之间传输的DATA数据帧。但需注意，中介不转发此帧。

字段列表： 
- Window Size Increment，31个比特位无符号自然数，范围为1-2^31-1（2,147,483,647）个字节数，
  表明发送者可以发送的最大字节数，以及接收者可以接收到的最大字节数。

注意事项： 
- 目前流控只会影响到DATA数据帧 
- 流标识符为0，影响整个连接，非单个流 
- 流标识符不为空，具体流的标识符，将只能够影响到具体流 
- WINDOW_UPDATE在某个携带有END_STREAM帧的后面被发送（当前流处于关闭或远程关闭状态），接收端可忽略，但不能作为错误对待 
- 发送者不能发送一个窗口值大于接收者已持有（接收端已经拥有一个流控窗口值）可用空间大小的WINDOW_UPDATE帧 
- 当流控窗口所设置可用空间已耗尽时，对端发送一个零负载带有END_STREAM标志位的DATA数据帧，这是允许的行为 
- 流量控制不会计算帧头所占用的9个字节空间 - 若窗口值溢出，针对单独流，响应RST_STREAM（错误码FLOW_CONTROL_ERROR）帧；
  针对整个连接的，响应GOAWAY（错误码FLOW_CONTROL_ERROR）帧 - DATA数据帧的接收方在接收到数据帧之后，需要计算已消耗
  的流控窗口可用空间，同时要把最新可用窗口空间发送给对端 - DATA数据帧发送方接收到WINDOW_UPDATE帧之后，获取最新可用窗口值 
- 接收方异步更新发送方窗口值，避免流停顿/失速 - 默认情况下流量控制窗口值为65535，除非接收到SETTINGS帧SETTINGS_INITIAL_WINDOW_SIZE参数，
  或者WINDOWS_UPDATE帧携带的窗口值大小，否则不会改变 - SETTINGS_INITIAL_WINDOW_SIZE值的改变会导致窗口可用空间不明晰，易出问题，
  发送者必须停止受流控影响的DATA数据帧的发送直到接收到WINDOW_UPDATE帧获得新的窗口值，才会继续发送。

eg：客户端在连接建立的瞬间一口气发送了60KB的数据，但来自服务器SETTINGS设置帧的初始窗口值为16KB，客户端只能够等
到WINDOW_UPDATE帧告知新的窗口值，然后继续发送传送剩下的44KB数据 - SETTINGS帧无法修改针对整个连接的流量控制窗口值 

- 任一端点在处理SETTINGS_INITIAL_WINDOW_SIZE值时一旦导致流控窗口值超出最大值，都需要作为一个FLOW_CONTROL_ERROR类型连接错误对待


9. RST_STREAWM
优先级帧，类型值为0x3，4个字节表示。表达了发送方对流优先级权重的建议值，任何时间任何流都可以发送，包括空闲或关闭的流。

+-----------------------------------------------+
|                0x4 (24)                       |
+---------------+---------------+---------------+
|  0x3  (8)     |  0x0 (8)      |
+-+-------------+---------------+-------------------------------+
|R|                Stream Identifier (31)                       |
+=+=============================================================+
|                        Error Code (32)                        |
+---------------------------------------------------------------+
字段列表： 
- Error Code：错误代码，32位无符号的自然数表示流被关闭的错误原因。

注意事项： 
- 接收到RST_STREAM帧，需要关闭对应流，因此流也要处于关闭状态。 
- 接收者不能够在此流上发送任何帧。 
- 发送端需要做好准备接收接收端接收到RST_STREAM帧之前发送的帧，这个空隙的帧需要处理。 
- 若流标识符为0x0，接收方需要响应PROTOCOL_ERROR类型连接错误。 
- 当流处于空闲状态idle状态时是不能够发送RST_STREAM帧，否则接收方会报以PROOTOCOL_ERROR类型连接错误。


10. GOAWAY
+-----------------------------------------------+
|                Length (24)                    |
+---------------+---------------+---------------+
|  0x7 (8)      |     0x0 (8)   |
+-+-------------+---------------+-------------------------------+
|R|                Stream Identifier (32)                       |
+=+=============================================================+
|R|                  Last-Stream-ID (31)                        |
+-+-------------------------------------------------------------+
|                      Error Code (32)                          |
+---------------------------------------------------------------+
|                  Additional Debug Data (*)                    |
+---------------------------------------------------------------+
一端通知对端较为优雅的方式停止创建流，同时还要完成之前已建立流的任务。

一旦发送，发送者将忽略接收到的流标识符大于Last-Stream-ID任何帧
接收者不能够在当前流上创建新流，若创建新流则创建新的连接
可用于服务器的管理行为，比如服务器进入维护阶段，不再准备接收新的连接
字段Last-Stream-ID为发送方取自最后一个正在处理或已经处理流的标识符
后续创建的流标识符高于Last-Stream-ID数据帧都不会被处理
终端应被鼓励在关闭连接之前发送GOAWAY隐式方式告知对方某些流是否已经被处理
终端可以选择关闭连接，针对行为不当的终端不发送GOAWAY帧
GOAWAY应用于当前连接，非具体流
没有处理任何流的情况下，Last-Stream-ID值可为0，也是合法
流（标识符小于或等于已有编号的标识符）在连接关闭之前没有被完全关闭，需要创建新的连接进行重试
发送端在发送GOAWAY时还有一些流任务没有完成，将保持连接为打开状态直到任务完成
终端可以在自身环境发生改变时发送多个GOAWAY帧，但Last-Stream-ID不允许增长
Additional Debug Data没有语义，仅用于联机测试诊断目的。若携带登陆或持久化调试数据，需要有安全保证避免未经授权访问。



四。帧的扩展
HTTP/2协议的扩展是允许存在的，在于提供额外服务。扩展包括： 
- 新类型帧，需要遵守通用帧格式 
- 新的设置参数，用于设置新帧相关属性 
- 新的错误代码，约定帧可能触发的错误

当定义一个新帧，需要注意 
1. 规范建议新的扩展需要经过双方协商后才能使用 
1. 在SETTINGS帧添加新的参数项，可在连接序言时发送给对端，或者适当机会发送 
1. 双方协商成功，可以使用新的扩展

已知ALTSVC、BLOCKED属于扩展帧。

1. ALTSVC
服务器提供给客户端当前可用的替代服务，类似于CNAME，客户端不支持可用选择忽略

+-----------------------------------------------+
|                Length (24)                    |
+---------------+---------------+---------------+
|  0xa (8)      |     0x0 (8)   |
+-+-------------+---------------+-------------------------------+
|R|                Stream Identifier (32)                       |
+=+=============================+===============================+
|         Origin-Len (16)       | Origin? (*)                 ...
+-------------------------------+-------------------------------+
|                   Alt-Svc-Field-Value (*)                   ...
+---------------------------------------------------------------+
字段列表： 
- Origin-Len: 16比特位整数，说明了Origin字段字节数 
- Origin: ASCII字符串表示替代服务 
- Alt-Svc-Field-Value: 包含了Alt-Svc HTTP Header Field，长度=Length (24) 
- Origin-Len (16)

需要注意： - 中介设备不能转发给客户端，原因就是中介自身替换处理，转发正常的业务数据给客户端就行
具体可参考：https://tools.ietf.org/html/draft-ietf-httpbis-alt-svc-06

2. BLOCKED

一端告诉另一端因为受到流量控制的作用有数据但无法发送。

+-----------------------------------------------+
|                Length (24)                    |
+---------------+---------------+---------------+
|  0xb (8)      |     0x0 (8)   |
+-+-------------+---------------+-------------------------------+
|R|                Stream Identifier/0x0 (32)                   |
+=+=============================================================+
Stream Identifier若为0x0，则表示针对整个连接，否则针对具体流
在流量控制窗口生效之前不能发送BLOCKED
一旦遇到此项问题，说明我们的实现可能有缺陷，无法得到理想的传输速率
只能够在WINDOW_UPDATE帧接收之前或SETTINGS_INITIAL_WINDOW_SIZE参数增加之前发送

五。小结

以上记录了HTTP/2帧基本结构，10个文档定义的正式帧，以及额外的两个扩展帧。
*/
#define NGX_HTTP_V2_ALPN_ADVERTISE       "\x02h2"
#define NGX_HTTP_V2_NPN_ADVERTISE        NGX_HTTP_V2_ALPN_ADVERTISE

#define NGX_HTTP_V2_STATE_BUFFER_SIZE    16

#define NGX_HTTP_V2_MAX_FRAME_SIZE       ((1 << 24) - 1) /* http2头部长度字段最大值 */

#define NGX_HTTP_V2_INT_OCTETS           4
#define NGX_HTTP_V2_MAX_FIELD            ((1 << NGX_HTTP_V2_INT_OCTETS * 7) - 1)

#define NGX_HTTP_V2_DATA_DISCARD         1
#define NGX_HTTP_V2_DATA_ERROR           2
#define NGX_HTTP_V2_DATA_INTERNAL_ERROR  3

#define NGX_HTTP_V2_FRAME_HEADER_SIZE    9 //HTTP2头部长度9字节

/* frame types  HTTP2报文头部的type字段  针对时间敏感的帧，比如RST_STREAM, WINDOW_UPDATE, PRIORITY，需要快速发送出去 */
#define NGX_HTTP_V2_DATA_FRAME           0x0
#define NGX_HTTP_V2_HEADERS_FRAME        0x1
#define NGX_HTTP_V2_PRIORITY_FRAME       0x2
#define NGX_HTTP_V2_RST_STREAM_FRAME     0x3
#define NGX_HTTP_V2_SETTINGS_FRAME       0x4
#define NGX_HTTP_V2_PUSH_PROMISE_FRAME   0x5
#define NGX_HTTP_V2_PING_FRAME           0x6
#define NGX_HTTP_V2_GOAWAY_FRAME         0x7
#define NGX_HTTP_V2_WINDOW_UPDATE_FRAME  0x8
#define NGX_HTTP_V2_CONTINUATION_FRAME   0x9

/* frame flags HTTP2报文头部的flag字段 */
#define NGX_HTTP_V2_NO_FLAG              0x00
//ACK (0x1)，表示接收者已经接收到SETTING帧，作为确认必须设置此标志位，此时负载为空
#define NGX_HTTP_V2_ACK_FLAG             0x01 
#define NGX_HTTP_V2_END_STREAM_FLAG      0x01
#define NGX_HTTP_V2_END_HEADERS_FLAG     0x04
#define NGX_HTTP_V2_PADDED_FLAG          0x08 /* 说明HTTP2内容部分带有pad数据 */
#define NGX_HTTP_V2_PRIORITY_FLAG        0x20 /* flag带有该标识，表示内容部分带有Stream Dependency和weight */


typedef struct ngx_http_v2_connection_s   ngx_http_v2_connection_t;
typedef struct ngx_http_v2_node_s         ngx_http_v2_node_t;
typedef struct ngx_http_v2_out_frame_s    ngx_http_v2_out_frame_t;


typedef u_char *(*ngx_http_v2_handler_pt) (ngx_http_v2_connection_t *h2c,
    u_char *pos, u_char *end);

/* 动态表存储节点，最终存入ngx_http_v2_hpack_t.entries中 */
typedef struct {
    ngx_str_t                        name;
    ngx_str_t                        value;
} ngx_http_v2_header_t;


typedef struct {
    ngx_uint_t                       sid; //http2头部sid，见ngx_http_v2_state_head
    size_t                           length; //http2头部leng，见ngx_http_v2_state_head
    size_t                           padding; //HTTP2头部flag带有NGX_HTTP_V2_PADDED_FLAG标识，则padding为pad内容的长度
    unsigned                         flags:8; //http2头部flag，见ngx_http_v2_state_head

    unsigned                         incomplete:1;

    /* HPACK */
    unsigned                         parse_name:1;
    unsigned                         parse_value:1;
    /* ngx_http_v2_state_header_block中置1，表示需要把name:value通过ngx_http_v2_add_header加入动态表中，然后置0 */
    unsigned                         index:1; //需要添加name:value到动态表中
    ngx_http_v2_header_t             header; //赋值见ngx_http_v2_get_indexed_header
    size_t                           header_limit;
    size_t                           field_limit;
    u_char                           field_state;
    u_char                          *field_start;
    u_char                          *field_end;
    size_t                           field_rest;
    /* 赋值见ngx_http_v2_state_headers */
    ngx_pool_t                      *pool;

    /* 当前正在处理的stream，赋值见ngx_http_v2_state_headers */
    ngx_http_v2_stream_t            *stream;

    u_char                           buffer[NGX_HTTP_V2_STATE_BUFFER_SIZE];
    size_t                           buffer_used;

    /* 
    赋值可能为:ngx_http_v2_state_preface  ngx_http_v2_state_header_complete ngx_http_v2_state_skip_headers  ngx_http_v2_state_head
    执行在ngx_http_v2_read_handler
    读取客户端发送过来的内容后，在这个handler进行解析处理
    */
    ngx_http_v2_handler_pt           handler;
} ngx_http_v2_state_t;


/* ngx_http_v2_connection_t.hpack */
typedef struct {
    /* 开辟空间和赋值见ngx_http_v2_add_header，默认64个指针数组，指向storage中的多个ngx_http_v2_header_t结构，
    通过entries[i]指针就可以直接访问到storage中的某个name:value,这样通过entries数组指针就可以访问到所有的name:value */
    ngx_http_v2_header_t           **entries;

    /* storage中总的name:value个数，ngx_http_v2_add_header中自增 */
    ngx_uint_t                       added;
    /* 当空间不够的时候，会重复利用storage空间，也就是把部分name:value设置为无效，好让新的name:value添加到storage中，
       deleted表示因为storage空间不够的情况下又有新的name:value加进来，则清除掉最老的name:value来腾出空间，见ngx_http_v2_add_header
    */
    ngx_uint_t                       deleted;
    ngx_uint_t                       reused;
    /* entries指针数组中指针的总数 */
    ngx_uint_t                       allocated;

    //默认NGX_HTTP_V2_TABLE_SIZE
    size_t                           size;
    //默认NGX_HTTP_V2_TABLE_SIZE   storage中的可用空间
    size_t                           free;
    /* 真正的空间在这里 */
    u_char                          *storage;
    /* 指向storage中的有效空间起始位置 */
    u_char                          *pos;
} ngx_http_v2_hpack_t;

/* ngx_http_v2_init中分配空间 */
struct ngx_http_v2_connection_s {
    ngx_connection_t                *connection;//对应的客户端连接，赋值见ngx_http_v2_init
    ngx_http_connection_t           *http_connection;

    /* ngx_http_v2_create_stream中自增，表示创建的流的数量，ngx_http_v2_close_stream自减 */
    ngx_uint_t                       processing;

    size_t                           send_window;//默认NGX_HTTP_V2_DEFAULT_WINDOW
    size_t                           recv_window;//默认NGX_HTTP_V2_MAX_WINDOW
    // 接收到对端的setting帧后，会做调整，见ngx_http_v2_state_settings_params
    size_t                           init_window;//默认NGX_HTTP_V2_DEFAULT_WINDOW

    //接收到对端的setting帧后，会做调整，见ngx_http_v2_state_settings_params
    size_t                           frame_size;//默认NGX_HTTP_V2_DEFAULT_FRAME_SIZE  接收到对端的setting帧后，会做调整，见ngx_http_v2_state_settings_params

    ngx_queue_t                      waiting;

    ngx_http_v2_state_t              state;
    /* hpack动态表，创建空间和赋值见ngx_http_v2_add_header */
    ngx_http_v2_hpack_t              hpack;

    ngx_pool_t                      *pool;
    /* frame通过该free链表来实现重复利用，可以参考ngx_http_v2_get_frame */
    ngx_http_v2_out_frame_t         *free_frames;
    /* 创建空间和赋值见ngx_http_v2_create_stream，根据客户端连接伪造的一个连接 */
    ngx_connection_t                *free_fake_connections;
    
    /* ngx_http_v2_node_t类型的数组指针，ngx_http_v2_init中创建空间和赋值，真正的ngx_http_v2_node_t赋值见ngx_http_v2_get_node_by_id */
    ngx_http_v2_node_t             **streams_index;
    /* 在ngx_http_v2_queue_blocked_frame中把帧加入该链表，在ngx_http_v2_send_output_queue中发送队列中的数据 */
    ngx_http_v2_out_frame_t         *last_out;

    ngx_queue_t                      posted;
    ngx_queue_t                      dependencies;
    /* 队列成员为ngx_http_v2_node_t，赋值见ngx_http_v2_state_priority */
    ngx_queue_t                      closed;
    /* 最末尾的一个流ID号 */
    ngx_uint_t                       last_sid;

    unsigned                         closed_nodes:8;
    unsigned                         blocked:1;
};

/* ngx_http_v2_connection_t.streams_index指针数组成员为该类型，一个流ID对应一个该结构，见ngx_http_v2_get_node_by_id */
/* 一个ngx_http_v2_stream_s.node流对应一个ngx_http_v2_node_t */
struct ngx_http_v2_node_s { 
/* PRIORITY帧通告的某个流ID对应的weight和dependecy都保存在该结构中，见ngx_http_v2_state_priority */
    ngx_uint_t                       id; /* stream id */
    ngx_http_v2_node_t              *index;
    /* 如果没有parent，则该指针指向NGX_HTTP_V2_ROOT */
    ngx_http_v2_node_t              *parent; 
    /* child通过该queue加入到parent->children，那么parent就可以通过children队列获取到所有的child
    如果该node没有parent则通过该queue加入到&h2c->dependencies，见ngx_http_v2_set_dependency,*/
    ngx_queue_t                      queue;
    /* 所有的children节点挂到该队列中 */
    ngx_queue_t                      children;
    ngx_queue_t                      reuse;
    ngx_uint_t                       rank;
    ngx_uint_t                       weight;
    double                           rel_weight;
    /* 该node对应的流，一个node对应一个流，赋值见ngx_http_v2_state_headers */
    ngx_http_v2_stream_t            *stream; 
};

/*
创建空间和赋值见ngx_http_v2_create_stream
*/
struct ngx_http_v2_stream_s {   
    /*初始赋值见ngx_http_v2_create_stream*/
    ngx_http_request_t              *request;
    /*初始赋值见ngx_http_v2_create_stream*/
    ngx_http_v2_connection_t        *connection;
    /* 一个ngx_http_v2_stream_s.node流对应一个ngx_http_v2_node_t,流id等存在该node中 */
    /* 该node对应的流，一个node对应一个流，赋值见ngx_http_v2_state_headers */
    ngx_http_v2_node_t              *node;

    ngx_uint_t                       header_buffers;
    ngx_uint_t                       queued;

    /*
     * A change to SETTINGS_INITIAL_WINDOW_SIZE could cause the
     * send_window to become negative, hence it's signed.
     */
    ssize_t                          send_window;
    size_t                           recv_window;

    ngx_http_v2_out_frame_t         *free_frames;
    ngx_chain_t                     *free_data_headers;
    ngx_chain_t                     *free_bufs;

    ngx_queue_t                      queue;

    ngx_array_t                     *cookies;

    size_t                           header_limit;

    unsigned                         handled:1;
    unsigned                         blocked:1;
    unsigned                         exhausted:1;
    unsigned                         end_headers:1;
    unsigned                         in_closed:1;
    unsigned                         out_closed:1;
    unsigned                         skip_data:2;
};


/* 创建空间和赋值见ngx_http_v2_send_settings，对应一种setting、HEADER等帧，每个帧对应一个该结构，最终加入ngx_http_v2_connection_t->last_out */
struct ngx_http_v2_out_frame_s {
    ngx_http_v2_out_frame_t         *next;
    ngx_chain_t                     *first;
    ngx_chain_t                     *last;

    /* 
    赋值:ngx_http_v2_settings_frame_handler ngx_http_v2_data_frame_handler  ngx_http_v2_headers_frame_handler  ngx_http_v2_frame_handler
    执行在ngx_http_v2_send_output_queue
    */
    ngx_int_t                      (*handler)(ngx_http_v2_connection_t *h2c,
                                        ngx_http_v2_out_frame_t *frame);

    ngx_http_v2_stream_t            *stream;
    size_t                           length;

    unsigned                         blocked:1;
    unsigned                         fin:1;
};

static ngx_inline void
ngx_http_v2_queue_frame(ngx_http_v2_connection_t *h2c,
    ngx_http_v2_out_frame_t *frame)
{
    ngx_http_v2_out_frame_t  **out;

    for (out = &h2c->last_out; *out; out = &(*out)->next) {

        if ((*out)->blocked || (*out)->stream == NULL) {
            break;
        }

        if ((*out)->stream->node->rank < frame->stream->node->rank
            || ((*out)->stream->node->rank == frame->stream->node->rank
                && (*out)->stream->node->rel_weight
                   >= frame->stream->node->rel_weight))
        {
            break;
        }
    }

    frame->next = *out;
    *out = frame;
}

/* 添加frame到last_out链表 */
static ngx_inline void
ngx_http_v2_queue_blocked_frame(ngx_http_v2_connection_t *h2c,
    ngx_http_v2_out_frame_t *frame)
{
    ngx_http_v2_out_frame_t  **out;

    for (out = &h2c->last_out; *out; out = &(*out)->next)
    {
        if ((*out)->blocked || (*out)->stream == NULL) {
            break;
        }
    }

    frame->next = *out;
    *out = frame;
}


void ngx_http_v2_init(ngx_event_t *rev);
void ngx_http_v2_request_headers_init(void);

ngx_int_t ngx_http_v2_read_request_body(ngx_http_request_t *r,
    ngx_http_client_body_handler_pt post_handler);

void ngx_http_v2_close_stream(ngx_http_v2_stream_t *stream, ngx_int_t rc);

ngx_int_t ngx_http_v2_send_output_queue(ngx_http_v2_connection_t *h2c);


ngx_int_t ngx_http_v2_get_indexed_header(ngx_http_v2_connection_t *h2c,
    ngx_uint_t index, ngx_uint_t name_only);
ngx_int_t ngx_http_v2_add_header(ngx_http_v2_connection_t *h2c,
    ngx_http_v2_header_t *header);
ngx_int_t ngx_http_v2_table_size(ngx_http_v2_connection_t *h2c, size_t size);


ngx_int_t ngx_http_v2_huff_decode(u_char *state, u_char *src, size_t len,
    u_char **dst, ngx_uint_t last, ngx_log_t *log);

/* 低bits - 1位全为1  例如bits为4，则结果为bit:1111   例如bits为5，则结果为bit:1111*/
#define ngx_http_v2_prefix(bits)  ((1 << (bits)) - 1)


#if (NGX_HAVE_NONALIGNED)

#define ngx_http_v2_parse_uint16(p)  ntohs(*(uint16_t *) (p))
#define ngx_http_v2_parse_uint32(p)  ntohl(*(uint32_t *) (p))

#else

#define ngx_http_v2_parse_uint16(p)  ((p)[0] << 8 | (p)[1])
#define ngx_http_v2_parse_uint32(p)                                           \
    ((p)[0] << 24 | (p)[1] << 16 | (p)[2] << 8 | (p)[3])

#endif

#define ngx_http_v2_parse_length(p)  ((p) >> 8)
#define ngx_http_v2_parse_type(p)    ((p) & 0xff)
#define ngx_http_v2_parse_sid(p)     (ngx_http_v2_parse_uint32(p) & 0x7fffffff)
#define ngx_http_v2_parse_window(p)  (ngx_http_v2_parse_uint32(p) & 0x7fffffff)


#define ngx_http_v2_write_uint16_aligned(p, s)                                \
    (*(uint16_t *) (p) = htons((uint16_t) (s)), (p) + sizeof(uint16_t))
#define ngx_http_v2_write_uint32_aligned(p, s)                                \
    (*(uint32_t *) (p) = htonl((uint32_t) (s)), (p) + sizeof(uint32_t))

#if (NGX_HAVE_NONALIGNED)

#define ngx_http_v2_write_uint16  ngx_http_v2_write_uint16_aligned
#define ngx_http_v2_write_uint32  ngx_http_v2_write_uint32_aligned

#else

#define ngx_http_v2_write_uint16(p, s)                                        \
    ((p)[0] = (u_char) ((s) >> 8),                                            \
     (p)[1] = (u_char)  (s),                                                  \
     (p) + sizeof(uint16_t))

#define ngx_http_v2_write_uint32(p, s)                                        \
    ((p)[0] = (u_char) ((s) >> 24),                                           \
     (p)[1] = (u_char) ((s) >> 16),                                           \
     (p)[2] = (u_char) ((s) >> 8),                                            \
     (p)[3] = (u_char)  (s),                                                  \
     (p) + sizeof(uint32_t))

#endif

/* http2头部type和len一共刚好4字节，http2头部type和len赋值 */
#define ngx_http_v2_write_len_and_type(p, l, t)                               \
    ngx_http_v2_write_uint32_aligned(p, (l) << 8 | (t))

#define ngx_http_v2_write_sid  ngx_http_v2_write_uint32

#endif /* _NGX_HTTP_V2_H_INCLUDED_ */
