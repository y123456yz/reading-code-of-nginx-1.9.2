/* 
FastCGI是对CGI的开放的扩展，它为所有因特网应用提供高性能，且没有Web服务器API的缺点（penalty）。 

本规范具有有限的（narrow）目标：从应用的视角规定FastCGI应用和支持FastCGI的Web服务器之间的接口。Web服务器的很多特性涉及FastCGI，
举例来说，应用管理设施与应用到Web服务器的接口无关，因此不在这儿描述。 

本规范适用于Unix（更确切地说，适用于支持伯克利socket的POSIX系统）。本规范大半是简单的通信协议，与字节序无关，并且将扩展到其他系统。 

我们将通过与CGI/1.1的常规Unix实现的比较来介绍FastCGI。FastCGI被设计用来支持常驻（long-lived）应用进程，也就是应用服务器。那是
与CGI/1.1的常规Unix实现的主要区别，后者构造应用进程，用它响应一个请求，以及让它退出。 

FastCGI进程的初始状态比CGI/1.1进程的初始状态更简洁，因为FastCGI进程开始不会连接任何东西。它没有常规的打开的文件stdin、stdout和stderr，
而且它不会通过环境变量接收大量的信息。FastCGI进程的初始状态的关键部分是个正在监听的socket，通过它来接收来自Web服务器的连接。 

FastCGI进程在其正在监听的socket上收到一个连接之后，进程执行简单的协议来接收和发送数据。协议服务于两个目的。首先，协议在多个独立的
FastCGI请求间多路复用单个传输线路。这可支持能够利用事件驱动或多线程编程技术处理并发请求的应用。第二，在每个请求内部，协议在每个方向
上提供若干独立的数据流。这种方式，例如，stdout和stderr数据通过从应用到Web服务器的单个传输线路传递，而不是像CGI/1.1那样需要独立的管道。 

一个FastCGI应用扮演几个明确定义的角色中的一个。最常用的是响应器（Responder）角色，其中应用接收所有与HTTP请求相关的信息，并产生一个
HTTP响应；那是CGI/1.1程序扮演的角色。第二个角色是认证器（Authorizer），其中应用接收所有与HTTP请求相关的信息，并产生一个认可/未经认
可的判定。第三个角色是过滤器（Filter），其中应用接收所有与HTTP请求相关的信息，以及额外的来自存储在Web服务器上的文件的数据流，并产生
"已过滤"版的数据流作为HTTP响应。框架是易扩展的，因而以后可定义更多的FastCGI。 

在本规范的其余部分，只要不致引起混淆，术语"FastCGI应用"、"应用进程"或"应用服务器"简写为"应用"。 

2. 初始进程状态 2.1 参数表 
Web服务器缺省创建一个含有单个元素的参数表，该元素是应用的名字，用作可执行路径名的最后一部分。Web服务器可提供某种方式来指定不同的
应用名，或更详细的参数表。 

注意，被Web服务器执行的文件可能是解释程序文件（以字符#!开头的文本文件），此情形中的应用参数表的构造在execve man页中描述。 

2.2 文件描述符 
当应用开始执行时，Web服务器留下一个打开的文件描述符，FCGI_LISTENSOCK_FILENO。该描述符引用Web服务器创建的一个正在监听的socket。 

FCGI_LISTENSOCK_FILENO等于STDIN_FILENO。当应用开始执行时，标准的描述符STDOUT_FILENO和STDERR_FILENO被关闭。一个用于应用确定它是用
CGI调用的还是用FastCGI调用的可靠方法是调用getpeername(FCGI_LISTENSOCK_FILENO)，对于FastCGI应用，它返回-1，并设置errno为ENOTCONN。 

Web服务器对于可靠传输的选择，Unix流式管道（AF_UNIX）或TCP/IP（AF_INET），是内含于FCGI_LISTENSOCK_FILENO socket的内部状态中的。 

2.3 环境变量 
Web服务器可用环境变量向应用传参数。本规范定义了一个这样的变量，FCGI_WEB_SERVER_ADDRS；我们期望随着规范的发展定义更多。Web服务器
可提供某种方式绑定其他环境变量，例如PATH变量。 

2.4 其他状态 
Web服务器可提供某种方式指定应用的初始进程状态的其他组件，例如进程的优先级、用户ID、组ID、根目录和工作目录。 

3. 协议基础 3.1 符号（Notation） 
我们用C语言符号来定义协议消息格式。所有的结构元素按照unsigned char类型定义和排列，这样ISO C编译器以明确的方式将它们展开，不带填
充。结构中定义的第一字节第一个被传送，第二字节排第二个，依次类推。 

我们用两个约定来简化我们的定义。 

首先，当两个相邻的结构组件除了后缀“B1”和“B0”之外命名相同时，它表示这两个组件可视为估值为B1<<8 + B0的单个数字。该单个数字的
名字是这些组件减去后缀的名字。这个约定归纳了一个由超过两个字节表示的数字的处理方式。 

第二，我们扩展C结构（struct）来允许形式 

struct {
unsigned char mumbleLengthB1;
unsigned char mumbleLengthB0;
... /* 其他东西 * /
unsigned char mumbleData[mumbleLength];
};

表示一个变长结构，此处组件的长度由较早的一个或多个组件指示的值确定。 

3.2 接受传输线路 
FastCGI应用在文件描述符FCGI_LISTENSOCK_FILENO引用的socket上调用accept()来接收新的传输线路。如果accept()成功，而且也绑定了FCGI_WEB_SERVER_ADDRS
环境变量，则应用立刻执行下列特殊处理： 


FCGI_WEB_SERVER_ADDRS：值是一列有效的用于Web服务器的IP地址。 
如果绑定了FCGI_WEB_SERVER_ADDRS，应用校验新线路的同级IP地址是否列表中的成员。如果校验失败（包括线路不是用TCP/IP传输的可能性），应
用关闭线路作为响应。 

FCGI_WEB_SERVER_ADDRS被表示成逗号分隔的IP地址列表。每个IP地址写成四个由小数点分隔的在区间[0..255]中的十进制数。所以该变量的一个
合法绑定是FCGI_WEB_SERVER_ADDRS=199.170.183.28,199.170.183.71。 



应用可接受若干个并行传输线路，但不是必须的。 

3.3 记录 
应用利用简单的协议执行来自Web服务器的请求。协议细节依赖应用的角色，但是大致说来，Web服务器首先发送参数和其他数据到应用，然后应
用发送结果数据到Web服务器，最后应用向Web服务器发送一个请求完成的指示。 

通过传输线路流动的所有数据在FastCGI记录中运载。FastCGI记录实现两件事。首先，记录在多个独立的FastCGI请求间多路复用传输线路。该多
路复用技术支持能够利用事件驱动或多线程编程技术处理并发请求的应用。第二，在单个请求内部，记录在每个方向上提供若干独立的数据流。这
种方式，例如，stdout和stderr数据能通过从应用到Web服务器的单个传输线路传递，而不需要独立的管道。 

typedef struct {
unsigned char version;
unsigned char type;
unsigned char requestIdB1;
unsigned char requestIdB0;
unsigned char contentLengthB1;
unsigned char contentLengthB0;
unsigned char paddingLength;
unsigned char reserved;
unsigned char contentData[contentLength];
unsigned char paddingData[paddingLength];
} FCGI_Record;

FastCGI记录由一个定长前缀后跟可变数量的内容和填充字节组成。记录包含七个组件： 


version: 标识FastCGI协议版本。本规范评述（document）FCGI_VERSION_1。 
type: 标识FastCGI记录类型，也就是记录执行的一般职能。特定记录类型和它们的功能在后面部分详细说明。 
requestId: 标识记录所属的FastCGI请求。 
contentLength: 记录的contentData组件的字节数。 
paddingLength: 记录的paddingData组件的字节数。 
contentData: 在0和65535字节之间的数据，依据记录类型进行解释。 
paddingData: 在0和255字节之间的数据，被忽略。

我们用不严格的C结构初始化语法来指定常量FastCGI记录。我们省略version组件，忽略填充（Padding），并且把requestId视为数字。因而
{FCGI_END_REQUEST, 1, {FCGI_REQUEST_COMPLETE,0}}是个type == FCGI_END_REQUEST、requestId == 1且contentData == {FCGI_REQUEST_COMPLETE,0}的记录。 

填充（Padding） 
协议允许发送者填充它们发送的记录，并且要求接受者解释paddingLength并跳过paddingData。填充允许发送者为更有效地处理保持对齐的数据。
X窗口系统协议上的经验显示了这种对齐方式的性能优势。 

我们建议记录被放置在八字节倍数的边界上。FCGI_Record的定长部分是八字节。 

管理请求ID 
Web服务器重用FastCGI请求ID；应用明了给定传输线路上的每个请求ID的当前状态。当应用收到一个记录{FCGI_BEGIN_REQUEST, R, ...}时，
请求ID R变成有效的，而且当应用向Web服务器发送记录{FCGI_END_REQUEST, R, ...}时变成无效的。 

当请求ID R无效时，应用会忽略requestId == R的记录，除了刚才描述的FCGI_BEGIN_REQUEST记录。 

Web服务器尝试保持小的FastCGI请求ID。那种方式下应用能利用短数组而不是长数组或哈希表来明了请求ID的状态。应用也有每次接受一个请求
的选项。这种情形下，应用只是针对当前的请求ID检查输入的requestId值。 

记录类型的类型 
有两种有用的分类FastCGI记录类型的方式。

第一个区别在管理（management）记录和应用（application）记录之间。管理记录包含不特定于任何Web服务器请求的信息，例如关于应用的协议
容量的信息。应用记录包含关于特定请求的信息，由requestId组件标识。 

管理记录有0值的requestId，也称为null请求ID。应用记录有非0的requestId。 

第二个区别在离散和连续记录之间。一个离散记录包含一个自己的所有数据的有意义的单元。一个流记录是stream的部分，也就是一连串流类型
的0或更多非空记录（length != 0），后跟一个流类型的空记录（length == 0）。当连接流记录的多个contentData组件时，形成一个字节序列；
该字节序列是流的值。因此流的值独立于它包含多少个记录或它的字节如何在非空记录间分配。 

这两种分类是独立的。在本版的FastCGI协议定义的记录类型中，所有管理记录类型也是离散记录类型，而且几乎所有应用记录类型都是流记录类型。
但是三种应用记录类型是离散的，而且没有什么能防止在某些以后的协议版本中定义一个流式的管理记录类型。 

3.4 名-值对 
FastCGI应用的很多角色需要读写可变数量的可变长度的值。所以为编码名-值对提供标准格式很有用。 

FastCGI以名字长度，后跟值的长度，后跟名字，后跟值的形式传送名-值对。127字节或更少的长度能在一字节中编码，而更长的长度总是在四字节中编码： 

typedef struct {
unsigned char nameLengthB0; /* nameLengthB0 >> 7 == 0 * /
unsigned char valueLengthB0; /* valueLengthB0 >> 7 == 0 * /
unsigned char nameData[nameLength];
unsigned char valueData[valueLength];
} FCGI_NameValuePair11;
typedef struct {
unsigned char nameLengthB0; /* nameLengthB0 >> 7 == 0 * /
unsigned char valueLengthB3; /* valueLengthB3 >> 7 == 1 * /
unsigned char valueLengthB2;
unsigned char valueLengthB1;
unsigned char valueLengthB0;
unsigned char nameData[nameLength];
unsigned char valueData[valueLength
((B3 & 0x7f) << 24) + (B2 << 16) + (B1 << 8) + B0];
} FCGI_NameValuePair14;
typedef struct {
unsigned char nameLengthB3; /* nameLengthB3 >> 7 == 1 * /
unsigned char nameLengthB2;
unsigned char nameLengthB1;
unsigned char nameLengthB0;
unsigned char valueLengthB0; /* valueLengthB0 >> 7 == 0 * /
unsigned char nameData[nameLength
((B3 & 0x7f) << 24) + (B2 << 16) + (B1 << 8) + B0];
unsigned char valueData[valueLength];
} FCGI_NameValuePair41;
typedef struct {
unsigned char nameLengthB3; /* nameLengthB3 >> 7 == 1 * /
unsigned char nameLengthB2;
unsigned char nameLengthB1;
unsigned char nameLengthB0;
unsigned char valueLengthB3; /* valueLengthB3 >> 7 == 1 * /
unsigned char valueLengthB2;
unsigned char valueLengthB1;
unsigned char valueLengthB0;
unsigned char nameData[nameLength
((B3 & 0x7f) << 24) + (B2 << 16) + (B1 << 8) + B0];
unsigned char valueData[valueLength
((B3 & 0x7f) << 24) + (B2 << 16) + (B1 << 8) + B0];
} FCGI_NameValuePair44;

长度的第一字节的高位指示长度的编码方式。高位为0意味着一个字节的编码方式，1意味着四字节的编码方式。 

名-值对格式允许发送者不用额外的编码方式就能传输二进制值，并且允许接收者立刻分配正确数量的内存，即使对于巨大的值。 

3.5 关闭传输线路 
Web服务器控制传输线路的生存期。当没有活动的请求时Web服务器能关闭线路。或者Web服务器也能把关闭的职权委托给应用（见FCGI_BEGIN_REQUEST）。
该情形下，应用在指定的请求结束时关闭线路。 

这种灵活性提供了多种应用风格。简单的应用会一次处理一个请求，并且为每个请求接受一个新的传输线路。更复杂的应用会通过一个或多个传输
线路处理并发的请求，而且会长期保持传输线路为打开状态。 

简单的应用通过在写入响应结束后关闭传输线路可得到重大的性能提升。Web服务器需要控制常驻线路的生命期。 

当应用关闭一个线路或发现一个线路关闭了，它就初始化一个新线路。 

4. 管理（Management）记录类型 4.1 FCGI_GET_VALUES, FCGI_GET_VALUES_RESULT 
Web服务器能查询应用内部的具体的变量。典型地，服务器会在应用启动上执行查询以使系统配置的某些方面自动化。 

应用把收到的查询作为记录{FCGI_GET_VALUES, 0, ...}。FCGI_GET_VALUES记录的contentData部分包含一系列值为空的名-值对。 

应用通过发送补充了值的{FCGI_GET_VALUES_RESULT, 0, ...}记录来响应。如果应用不理解查询中包含的一个变量名，它从响应中忽略那个名字。 

FCGI_GET_VALUES被设计为允许可扩充的变量集。初始集提供信息来帮助服务器执行应用和线路的管理： 


FCGI_MAX_CONNS：该应用将接受的并发传输线路的最大值，例如"1"或"10"。 
FCGI_MAX_REQS：该应用将接受的并发请求的最大值，例如"1"或"50"。 
FCGI_MPXS_CONNS：如果应用不多路复用线路（也就是通过每个线路处理并发请求）则为 "0"，其他则为"1"。

应用可在任何时候收到FCGI_GET_VALUES记录。除了FastCGI库，应用的响应不能涉及应用固有的库。 

4.2 FCGI_UNKNOWN_TYPE 
在本协议的未来版本中，管理记录类型集可能会增长。为了这种演变作准备，协议包含FCGI_UNKNOWN_TYPE管理记录。当应用收到无法理解的类型
为T的管理记录时，它用{FCGI_UNKNOWN_TYPE, 0, {T}}响应。 

FCGI_UNKNOWN_TYPE记录的contentData组件具有形式： 

typedef struct {
unsigned char type; 
unsigned char reserved[7];
} FCGI_UnknownTypeBody;

type组件是无法识别的管理记录的类型。 

5. 应用（Application）记录类型 5.1 FCGI_BEGIN_REQUEST 
Web服务器发送FCGI_BEGIN_REQUEST记录开始一个请求。 

FCGI_BEGIN_REQUEST记录的contentData组件具有形式： 

typedef struct {
unsigned char roleB1;
unsigned char roleB0;
unsigned char flags;
unsigned char reserved[5];
} FCGI_BeginRequestBody;

role组件设置Web服务器期望应用扮演的角色。当前定义的角色有： 


FCGI_RESPONDER 
FCGI_AUTHORIZER 
FCGI_FILTER 

角色在下面的第6章中作更详细地描述。 

flags组件包含一个控制线路关闭的位： 


flags & FCGI_KEEP_CONN：如果为0，则应用在对本次请求响应后关闭线路。如果非0，应用在对本次请求响应后不会关闭线路；Web服务器为线路保持响应性。
5.2 名-值对流：FCGI_PARAMS FCGI_PARAMS 
是流记录类型，用于从Web服务器向应用发送名-值对。名-值对被相继地沿着流发送，没有特定顺序。 

5.3 字节流：FCGI_STDIN, FCGI_DATA, FCGI_STDOUT, FCGI_STDERR FCGI_STDIN 
是流记录类型，用于从Web服务器向应用发送任意数据。FCGI_DATA是另一种流记录类型，用于向应用发送额外数据。 

FCGI_STDOUT和FCGI_STDERR都是流记录类型，分别用于从应用向Web服务器发送任意数据和错误数据。 

5.4 FCGI_ABORT_REQUEST 
Web服务器发送FCGI_ABORT_REQUEST记录来中止请求。收到{FCGI_ABORT_REQUEST, R}后，应用尽快用{FCGI_END_REQUEST, R, {FCGI_REQUEST_COMPLETE, 
appStatus}}响应。这是真实的来自应用的响应，而不是来自FastCGI库的低级确认。 

当HTTP客户端关闭了它的传输线路，可是受客户端委托的FastCGI请求仍在运行时，Web服务器中止该FastCGI请求。这种情况看似不太可能；多数FastCGI
请求具有很短的响应时间，同时如果客户端很慢则Web服务器提供输出缓冲。但是FastCGI应用与其他系统的通信或执行服务器端进栈可能被延期。 

当不是通过一个传输线路多路复用请求时，Web服务器能通过关闭请求的传输线路来中止请求。但使用多路复用请求时，关闭传输线路具有不幸的结果，
中止线路上的所有请求。 

5.5 FCGI_END_REQUEST 
不论已经处理了请求，还是已经拒绝了请求，应用发送FCGI_END_REQUEST记录来终止请求。 

FCGI_END_REQUEST记录的contentData组件具有形式： 

typedef struct {
unsigned char appStatusB3;
unsigned char appStatusB2;
unsigned char appStatusB1;
unsigned char appStatusB0;
unsigned char protocolStatus;
unsigned char reserved[3];
} FCGI_EndRequestBody;

appStatus组件是应用级别的状态码。每种角色说明其appStatus的用法。 

protocolStatus组件是协议级别的状态码；可能的protocolStatus值是： 


FCGI_REQUEST_COMPLETE：请求的正常结束。 
FCGI_CANT_MPX_CONN：拒绝新请求。这发生在Web服务器通过一条线路向应用发送并发的请求时，后者被设计为每条线路每次处理一个请求。 
FCGI_OVERLOADED：拒绝新请求。这发生在应用用完某些资源时，例如数据库连接。 
FCGI_UNKNOWN_ROLE：拒绝新请求。这发生在Web服务器指定了一个应用不能识别的角色时。
6. 角色 6.1 角色协议 
角色协议只包括带应用记录类型的记录。它们本质上利用流传输所有数据。 

为了让协议可靠以及简化应用编程，角色协议被设计使用近似顺序编组（nearly sequential marshalling）。在严格顺序编组的协议中，应用接收
其第一个输入，然后是第二个，依次类推。直到收到全部。同样地，应用发送其第一个输出，然后是第二个，依次类推。直到发出全部。输入不是
相互交叉的，输出也不是。 

对于某些FastCGI角色，顺序编组规则有太多限制，因为CGI程序能不受时限地（timing restriction）写入stdout和stderr。所以用到了FCGI_STDOUT和
FCGI_STDERR的角色协议允许交叉这两个流。 

所有角色协议使用FCGI_STDERR流的方式恰是stderr在传统的应用编程中的使用方式：以易理解的方式报告应用级错误。FCGI_STDERR流的使用总是可选
的。如果没有错误要报告，应用要么不发送FCGI_STDERR记录，要么发送一个0长度的FCGI_STDERR记录。 

当角色协议要求传输不同于FCGI_STDERR的流时，总是至少传输一个流类型的记录，即使流是空的。 

再次关注可靠的协议和简化的应用编程技术，角色协议被设计为近似请求-响应。在真正的请求-响应协议中，应用在发送其输出记录前接收其所有的
输入记录。请求-响应协议不允许流水线技术（pipelining）。 

对于某些FastCGI角色，请求响应规则约束太强；毕竟，CGI程序不限于在开始写stdout前读取全部stdin。所以某些角色协议允许特定的可能性。首
先，除了结尾的流输入，应用接收其所有输入。当开始接收结尾的流输入时，应用开始写其输出。 

当角色协议用FCGI_PARAMS传输文本值时，例如CGI程序从环境变量得到的值，其长度不包括结尾的null字节，而且它本身不包含null字节。需要提供
environ(7)格式的名-值对的应用必须在名和值间插入等号，并在值后添加null字节。 

角色协议不支持CGI的未解析的（non-parsed）报头特性。FastCGI应用使用CGI报头Status和Location设置响应状态。 

6.2 响应器（Responder） 
作为响应器的FastCGI应用具有同CGI/1.1一样的目的：它接收与HTTP请求关联的所有信息并产生HTTP响应。 

它足以解释怎样用响应器模拟CGI/1.1的每个元素： 


响应器应用通过FCGI_PARAMS接收来自Web服务器的CGI/1.1环境变量。 
接下来响应器应用通过FCGI_STDIN接收来自Web服务器的CGI/1.1 stdin数据。在收到流尾指示前，应用从该流接收最多CONTENT_LENGTH字节。
（只当HTTP客户端未能提供时，例如因为客户端崩溃了，应用才收到少于CONTENT_LENGTH的字节。） 
响应器应用通过FCGI_STDOUT向Web服务器发送CGI/1.1 stdout数据，以及通过FCGI_STDERR发送CGI/1.1 stderr数据。应用同时发送这些，而
非一个接一个。在开始写FCGI_STDOUT和FCGI_STDERR前，应用必须等待读取FCGI_PARAMS完成，但是不需要在开始写这两个流前完成从FCGI_STDIN读取。 
在发送其所有stdout和stderr数据后，响应器应用发送FCGI_END_REQUEST记录。应用设置protocolStatus组件为FCGI_REQUEST_COMPLETE，并设
置appStatus组件为CGI程序通过exit系统调用返回的状态码。 

响应器执行更新，例如实现POST方法，应该比较在FCGI_STDIN上收到的字节数和CONTENT_LENGTH，并且如果两数不等则中止更新。 

6.3 认证器（Authorizer） 
作为认证器的FastCGI应用接收所有与HTTP请求相关的信息，并产生一个认可/未经认可的判定。对于认可的判定，认证器也能把名-值对同HTTP
请求相关联；当给出未经认可的判定时，认证器向HTTP客户端发送结束响应。 

由于CGI/1.1定义了与HTTP请求相关联的信息的极好的表示方式，认证器使用同样的表示法： 


认证器应用在FCGI_PARAMS流上接收来自Web服务器的HTTP信息，格式同响应器一样。Web服务器不会发送报头CONTENT_LENGTH、PATH_INFO、
PATH_TRANSLATED和SCRIPT_NAME。 
认证器应用以同响应器一样的方式发送stdout和stderr数据。CGI/1.1响应状态指定对结果的处理。如果应用发送状态200（OK），Web服务器允
许访问。 依赖于其配置，Web服务器可继续进行其他的访问检查，包括对其他认证器的请求。 
认证器应用的200响应可包含以Variable-为名字前缀的报头。这些报头从应用向Web服务器传送名-值对。例如，响应报头 

Variable-AUTH_METHOD: database lookup
传输名为AUTH-METHOD的值"database lookup"。服务器把这样的名-值对同HTTP请求相关联，并且把它们包含在后续的CGI或FastCGI请求中，这
些请求在处理HTTP请求的过程中执行。当应用给出200响应时，服务器忽略名字不以Variable-为前缀的响应报头，并且忽略任何响应内容。 
对于“200”（OK）以外的认证器响应状态值，Web服务器拒绝访问并将响应状态、报头和内容发回HTTP客户端。 


6.4 过滤器（Filter） 
作为过滤器的FastCGI应用接收所有与HTTP请求相关联的信息，以及额外的来自存储在Web服务器上的文件的数据流，并产生数据流的“已过滤”
版本作为HTTP响应。 

过滤器在功能上类似响应器，接受一个数据文件作为参数。区别是，过滤器使得数据文件和过滤器本身都能用Web服务器的访问控制机制进行访
问控制，而响应器接受数据文件名作为参数，必须在数据文件上执行自己的访问控制检查。 

过滤器采取的步骤与响应器的相似。服务器首先提供环境变量，然后是标准输入（常规形式的POST数据），最后是数据文件输入： 


如同响应器，过滤器应用通过FCGI_PARAMS接收来自Web服务器的名-值对。过滤器应用接收两个过滤器特定的变量：FCGI_DATA_LAST_MOD和FCGI_DATA_LENGTH。 
接下来，过滤器应用通过FCGI_STDIN接收来自Web服务器的CGI/1.1 stdin数据。在收到流尾指示以前，应用从该流接收最多CONTENT_LENGTH字节。
（只有HTTP客户端未能提供时，应用收到的才少于CONTENT_LENGTH字节，例如因为客户端崩溃了。） 
下一步，过滤器应用通过FCGI_DATA接收来自Web服务器的文件数据。该文件的最后修改时间（表示成自UTC 1970年1月1日以来的整秒数）是FCGI_DATA_LAST_MOD；
应用可能查阅该变量并从缓存作出响应，而不读取文件数据。在收到流尾指示以前，应用从该流接收最多FCGI_DATA_LENGTH字节。 
过滤器应用通过FCGI_STDOUT向Web服务器发送CGI/1.1 stdout数据，以及通过FCGI_STDERR的CGI/1.1 stderr数据。应用同时发送这些，而非相继地。在开
始写入FCGI_STDOUT和FCGI_STDERR以前，应用必须等待读取FCGI_STDIN完成，但是不需要在开始写入这两个流以前完成从FCGI_DATA的读取。 
在发送其所有的stdout和stderr数据之后，应用发送FCGI_END_REQUEST记录。应用设定protocolStatus组件为FCGI_REQUEST_COMPLETE，以及appStatus组
件为类似的CGI程序通过exit系统调用返回的状态代码。

过滤器应当把在FCGI_STDIN上收到的字节数同CONTENT_LENGTH比较，以及把FCGI_DATA上的同FCGI_DATA_LENGTH比较。如果数字不匹配且过滤器是个查询，
过滤器响应应当提供数据丢失的指示。如果数字不匹配且过滤器是个更新，过滤器应当中止更新。 

7. 错误 
FastCGI应用以0状态退出来指出它故意结束了，例如，为了执行原始形式的垃圾收集。FastCGI应用以非0状态退出被假定为崩溃了。以0或非0状态退出的
Web服务器或其他的应用管理器如何响应应用超出了本规范的范围。 

Web服务器能通过向FastCGI应用发送SIGTERM来要求它退出。如果应用忽略SIGTERM，Web服务器能采用SIGKILL。 

FastCGI应用使用FCGI_STDERR流和FCGI_END_REQUEST记录的appStatus组件报告应用级别错误。在很多情形中，错误会通过FCGI_STDOUT流直接报告给用户。 

在Unix上，应用向syslog报告低级错误，包括FastCGI协议错误和FastCGI环境变量中的语法错误。依赖于错误的严重性，应用可能继续或以非0状态退出。 

8. 类型和常量 /*
* 正在监听的socket文件编号
* /
#define FCGI_LISTENSOCK_FILENO 0
typedef struct {
unsigned char version;
unsigned char type;
unsigned char requestIdB1;
unsigned char requestIdB0;
unsigned char contentLengthB1;
unsigned char contentLengthB0;
unsigned char paddingLength;
unsigned char reserved;
} FCGI_Header;
/*
* FCGI_Header中的字节数。协议的未来版本不会减少该数。
* /
#define FCGI_HEADER_LEN 8
/*
* 可用于FCGI_Header的version组件的值
* /
#define FCGI_VERSION_1 1
/*
* 可用于FCGI_Header的type组件的值
* /
#define FCGI_BEGIN_REQUEST 1
#define FCGI_ABORT_REQUEST 2
#define FCGI_END_REQUEST 3
#define FCGI_PARAMS 4
#define FCGI_STDIN 5
#define FCGI_STDOUT 6
#define FCGI_STDERR 7
#define FCGI_DATA 8
#define FCGI_GET_VALUES 9
#define FCGI_GET_VALUES_RESULT 10
#define FCGI_UNKNOWN_TYPE 11
#define FCGI_MAXTYPE (FCGI_UNKNOWN_TYPE)
/*
* 可用于FCGI_Header的requestId组件的值
* /
#define FCGI_NULL_REQUEST_ID 0
typedef struct {
unsigned char roleB1;
unsigned char roleB0;
unsigned char flags;
unsigned char reserved[5];
} FCGI_BeginRequestBody;
typedef struct {
FCGI_Header header;
FCGI_BeginRequestBody body;
} FCGI_BeginRequestRecord;
/*
* 可用于FCGI_BeginRequestBody的flags组件的掩码
* /
#define FCGI_KEEP_CONN 1
/*
* 可用于FCGI_BeginRequestBody的role组件的值
* /
#define FCGI_RESPONDER 1
#define FCGI_AUTHORIZER 2
#define FCGI_FILTER 3
typedef struct {
unsigned char appStatusB3;
unsigned char appStatusB2;
unsigned char appStatusB1;
unsigned char appStatusB0;
unsigned char protocolStatus;
unsigned char reserved[3];
} FCGI_EndRequestBody;
typedef struct {
FCGI_Header header;
FCGI_EndRequestBody body;
} FCGI_EndRequestRecord;
/*
* 可用于FCGI_EndRequestBody的protocolStatus组件的值
* /
#define FCGI_REQUEST_COMPLETE 0
#define FCGI_CANT_MPX_CONN 1
#define FCGI_OVERLOADED 2
#define FCGI_UNKNOWN_ROLE 3
/*
* 可用于FCGI_GET_VALUES/FCGI_GET_VALUES_RESULT记录的变量名
* /
#define FCGI_MAX_CONNS "FCGI_MAX_CONNS"
#define FCGI_MAX_REQS "FCGI_MAX_REQS"
#define FCGI_MPXS_CONNS "FCGI_MPXS_CONNS"
typedef struct {
unsigned char type; 
unsigned char reserved[7];
} FCGI_UnknownTypeBody;
typedef struct {
FCGI_Header header;
FCGI_UnknownTypeBody body;
} FCGI_UnknownTypeRecord;
9. 参考 
National Center for Supercomputer Applications, The Common Gateway Interface, version CGI/1.1. 

D.R.T. Robinson, The WWW Common Gateway Interface Version 1.1, Internet-Draft, 15 February 1996. 

A. 表：记录类型的属性 
下面的图表列出了所有记录类型，并指出各自的这些属性： 


WS->App：该类型的记录只能由Web服务器发送到应用。其他类型的记录只能由应用发送到Web服务器。 
management：该类型的记录含有非特定于某个Web服务器请求的信息，而且使用null请求ID。其他类型的记录含有请求特定的信息，而且不能使用null请求ID。 
stream：该类型的记录组成一个由带有空contentData的记录结束的流。其他类型的记录是离散的；各自携带一个有意义的数据单元。
WS->App management stream
FCGI_GET_VALUES x x
FCGI_GET_VALUES_RESULT x
FCGI_UNKNOWN_TYPE x
FCGI_BEGIN_REQUEST x
FCGI_ABORT_REQUEST x
FCGI_END_REQUEST
FCGI_PARAMS x x
FCGI_STDIN x x
FCGI_DATA x x
FCGI_STDOUT x 
FCGI_STDERR x 
B. 典型的协议消息流程 
用于示例的补充符号约定： 


流记录的contentData（FCGI_PARAMS、FCGI_STDIN、FCGI_STDOUT和FCGI_STDERR）被描述成一个字符串。以" ... "结束的字符串是太长而无法显示的，所以只显示前缀。 
发送到Web服务器的消息相对于收自Web服务器的消息缩进排版。 
消息以应用经历的时间顺序显示。 

1. 在stdin上不带数据的简单请求，以及成功的响应： 

{FCGI_BEGIN_REQUEST, 1, {FCGI_RESPONDER, 0}}
{FCGI_PARAMS, 1, "\013\002SERVER_PORT80\013\016SERVER_ADDR199.170.183.42 ... "}
{FCGI_PARAMS, 1, ""}
{FCGI_STDIN, 1, ""}
{FCGI_STDOUT, 1, "Content-type: text/html\r\n\r\n<html>\n<head> ... "}
{FCGI_STDOUT, 1, ""}
{FCGI_END_REQUEST, 1, {0, FCGI_REQUEST_COMPLETE}}

2. 类似例1，但这次在stdin有数据。Web服务器选择用比之前更多的FCGI_PARAMS记录发送参数： 

{FCGI_BEGIN_REQUEST, 1, {FCGI_RESPONDER, 0}}
{FCGI_PARAMS, 1, "\013\002SERVER_PORT80\013\016SER"}
{FCGI_PARAMS, 1, "VER_ADDR199.170.183.42 ... "}
{FCGI_PARAMS, 1, ""}
{FCGI_STDIN, 1, "quantity=100&item=3047936"}
{FCGI_STDIN, 1, ""}
{FCGI_STDOUT, 1, "Content-type: text/html\r\n\r\n<html>\n<head> ... "}
{FCGI_STDOUT, 1, ""}
{FCGI_END_REQUEST, 1, {0, FCGI_REQUEST_COMPLETE}}

3. 类似例1，但这次应用发现了错误。应用把一条消息记录到stderr，向客户端返回一个页面，并且向Web服务器返回非0退出状态。应用选择用更多FCGI_STDOUT记录发送页面： 

{FCGI_BEGIN_REQUEST, 1, {FCGI_RESPONDER, 0}}
{FCGI_PARAMS, 1, "\013\002SERVER_PORT80\013\016SERVER_ADDR199.170.183.42 ... "}
{FCGI_PARAMS, 1, ""}
{FCGI_STDIN, 1, ""}
{FCGI_STDOUT, 1, "Content-type: text/html\r\n\r\n<ht"}
{FCGI_STDERR, 1, "config error: missing SI_UID\n"}
{FCGI_STDOUT, 1, "ml>\n<head> ... "}
{FCGI_STDOUT, 1, ""}
{FCGI_STDERR, 1, ""}
{FCGI_END_REQUEST, 1, {938, FCGI_REQUEST_COMPLETE}}

4. 在单条线路上多路复用的两个例1实例。第一个请求比第二个难，所以应用颠倒次序完成这些请求： 

{FCGI_BEGIN_REQUEST, 1, {FCGI_RESPONDER, FCGI_KEEP_CONN}}
{FCGI_PARAMS, 1, "\013\002SERVER_PORT80\013\016SERVER_ADDR199.170.183.42 ... "}
{FCGI_PARAMS, 1, ""}
{FCGI_BEGIN_REQUEST, 2, {FCGI_RESPONDER, FCGI_KEEP_CONN}}
{FCGI_PARAMS, 2, "\013\002SERVER_PORT80\013\016SERVER_ADDR199.170.183.42 ... "}
{FCGI_STDIN, 1, ""}
{FCGI_STDOUT, 1, "Content-type: text/html\r\n\r\n"}
{FCGI_PARAMS, 2, ""}
{FCGI_STDIN, 2, ""}
{FCGI_STDOUT, 2, "Content-type: text/html\r\n\r\n<html>\n<head> ... "}
{FCGI_STDOUT, 2, ""}
{FCGI_END_REQUEST, 2, {0, FCGI_REQUEST_COMPLETE}}
{FCGI_STDOUT, 1, "<html>\n<head> ... "}
{FCGI_STDOUT, 1, ""}
{FCGI_END_REQUEST, 1, {0, FCGI_REQUEST_COMPLETE}}




Fastcgi协议定义解释与说明 
http://wangnow.com/article/28-fastcgi-protocol-specification

 

首先介绍响应的数据，比较简单，再者我们对返回的数据比较敏感……
1 响应格式
如（十六进制方式显示）

序列 0  1  2  3  4  5  6  7 ...
数值 01 06 00 01 01 1D 03 00...

序列0（值01）为version，固定取1即可
序列1（值06）为type，代表FCGI_STDOUT，表示应用的输出
序列2 3（00 01）代表2字节的请求id，默认取1即可（准确说应该是和请求应用时发送的id一致，这里假设请求和响应的id都是1）
序列4 5（01 1D）代表2字节的输出长度，最大为65535，例如当前内容长度为(0x01 << 8) + 0x1D = 285
序列6（03）代表填充padding字节数（填充为满8字节的整数倍），例如当前填充（以0填充）长度为8 - 285 % 8 = 3，即获取输出长度（285）的内容后要跳过的字节数，当然如果为8就无需填充了
序列7（00）为保留字节
8字节（序列7）之后为具体内容（contentData）和填充内容（paddingData）

最后为通知web服务器的请求结束记录，具体内容如下

序列 0  1  2  3  4  5  6  7 ...
数值 01 03 00 01 00 08 00 00...

其中序列1（03）type代表FCGI_END_REQUEST，即请求结束，8字节之后为contentData（EndRequestBody）和paddingData
EndRequestBody的内容也比较个性，是单独定义的

typedef struct {
     unsigned char appStatusB3;
     unsigned char appStatusB2;
     unsigned char appStatusB1;
     unsigned char appStatusB0;
     unsigned char protocolStatus;
     unsigned char reserved[3];
} FCGI_EndRequestBody;


appStatus占了四个字节，定义为cgi通过调用系统退出返回的状态码（The application sets the protocolStatus component to FCGI_REQUEST_COMPLETE and the appStatus component to the status code that the CGI program would have returned via the exit system call.）Linux正常的程序退出默认是返回0（应该是吧？我记着是……）

protocolStatus的值可以是

#define FCGI_REQUEST_COMPLETE 0
#define FCGI_CANT_MPX_CONN 1
#define FCGI_OVERLOADED 2
#define FCGI_UNKNOWN_ROLE 3


因此最后FCGI_END_REQUEST的contentData为

序列 0  1  2  3  4  5  6  7
数值 00 00 00 00 00 00 00 00

0-3序列为appStatus
4序列protocolStatus为0（FCGI_REQUEST_COMPLETE）
5-7序列为保留的3字节reserved[3]

2 请求格式

序列 0  1  2  3  4  5  6  7 ...
数值 01 01 00 01 00 08 00 00...

序列0（值01）为version
序列1（值01）为type，代表FCGI_BEGIN_REQUES，表示开始发送请求 
序列2 3（00 01）代表2字节的请求id，默认取1即可
请求开始的记录稍微特殊，发送的内容（contentData）如下格式

typedef struct {
     unsigned char roleB1;
     unsigned char roleB0;
     unsigned char flags;
     unsigned char reserved[5];
} FCGI_BeginRequestBody;
 

#role的可以取如下三个值
#define FCGI_RESPONDER 1
#define FCGI_AUTHORIZER 2
#define FCGI_FILTER 3

我们取1（FCGI_RESPONDER）为啥？说是和经典的CGI/1.1作用一样（http那些东西）
flags取0表示本次请求完毕后即关闭链接。

序列 0  1  2  3  4  5  6  7
数值 00 01 00 00 00 00 00 00

0和1序列代表role为1（FCGI_RESPONDER）
2序列为flags 0
3-7序列为reserved[5]

再说下协议中FCGI_PARAMS中的Name-Value Pairs，目的是提供应用层一些必要的变量（类似http中的header：headerName-headerValue，当然可以为多个），详细定义见http://www.fastcgi.com/devkit/doc/fcgi-spec.html#S3.4
其中一种定义格式如下：

typedef struct {
     unsigned char nameLengthB0; /* nameLengthB0 >> 7 == 0 * /
     unsigned char valueLengthB3; /* valueLengthB3 >> 7 == 1 * /
     unsigned char valueLengthB2;
     unsigned char valueLengthB1;
     unsigned char valueLengthB0;
     unsigned char nameData[nameLength];
     unsigned char valueData[valueLength
     ((B3 & 0x7f) << 24) + (B2 << 16) + (B1 << 8) + B0];
} FCGI_NameValuePair14;

结合实例说明下

序列 0  1  2  3  4  5  6  7 ...
数值 00 04 00 01 04 EB 05 00...

序列1（04）代表FCGI_PARAMS
序列7之后的为相应的名字（Name）长度（nameLength）、值（Value）长度（valueLength）、名字（nameData）、值（valueData）
其中规定名字或者值的长度如果大于127字节，则要以4字节存储，如下

序列 0  1  2  3  4  5  6  7 ............
数值 0F 80 00 00 91 S  C  R IPT_FILENAME/data/www/......

序列0的0F即十进制的15（SCRIPT_FILENAME的长度），不大于127所以占一个字节
序列1的80即十进制的128，大于127，说明要占用4字节（80 00 00 91），长度为

((B3 & 0x7f) << 24) + (B2 << 16) + (B1 << 8) + B0
算算等于多少呢？如果对位移、与等操作符号不熟悉的话，更详细的介绍见之前的文章

3 其他说明
各个值的详细定义参见http://www.fastcgi.com/devkit/doc/fcgi-spec.html#S8
以下做一些概要说明
记录（Records，可以顺序发送或者接受多个记录）的格式具体定义如下

typedef struct {
     unsigned char version;
     unsigned char type;
     unsigned char requestIdB1;
     unsigned char requestIdB0;
     unsigned char contentLengthB1;
     unsigned char contentLengthB0;
     unsigned char paddingLength;
     unsigned char reserved;
     unsigned char contentData[contentLength];
     unsigned char paddingData[paddingLength];
} FCGI_Record;

#前八字节定义为Header（可以这么理解，头信息+响应内容，想想htpp协议中的header+body就明白了）
#协议说明中把这部分定义为FCGI_Header（以上红色字体部分），即：

typedef struct {
     unsigned char version;
     unsigned char type;
     unsigned char requestIdB1;
     unsigned char requestIdB0;
     unsigned char contentLengthB1;
     unsigned char contentLengthB0;
     unsigned char paddingLength;
     unsigned char reserved;
} FCGI_Header;
 

#version定义为1
#define FCGI_VERSION_1 1


#type具体值定义，主要关注FCGI_BEGIN_REQUEST（请求开始） FCGI_END_REQUEST（请求结束） FCGI_PARAMS（fastcgi参数，即一些服务器变量，如HTTP_USER_AGENT） FCGI_STDOUT（fastcgi标准输出，即请求后返回的内容）

#define FCGI_BEGIN_REQUEST 1
#define FCGI_ABORT_REQUEST 2
#define FCGI_END_REQUEST 3
#define FCGI_PARAMS 4
#define FCGI_STDIN 5
#define FCGI_STDOUT 6
#define FCGI_STDERR 7
#define FCGI_DATA 8
#define FCGI_GET_VALUES 9
#define FCGI_GET_VALUES_RESULT 10
#define FCGI_UNKNOWN_TYPE 11
#define FCGI_MAXTYPE (FCGI_UNKNOWN_TYPE)











fastcgi_param 详解 .
分类： nginx 2012-12-28 11:18 14537人阅读 评论(0) 收藏 举报 
[php] view plaincopyprint?
01.fastcgi_param  SCRIPT_FILENAME    $document_root$fastcgi_script_name;#脚本文件请求的路径   请求获取的后端PHP服务器的文件目录，通过文件目录+uri获取指定文件
02.fastcgi_param  QUERY_STRING       $query_string; #请求的参数;如?app=123  
03.fastcgi_param  REQUEST_METHOD     $request_method; #请求的动作(GET,POST)  
04.fastcgi_param  CONTENT_TYPE       $content_type; #请求头中的Content-Type字段  
05.fastcgi_param  CONTENT_LENGTH     $content_length; #请求头中的Content-length字段。  
06.  
07.fastcgi_param  SCRIPT_NAME        $fastcgi_script_name; #脚本名称   
08.fastcgi_param  REQUEST_URI        $request_uri; #请求的地址不带参数  
09.fastcgi_param  DOCUMENT_URI       $document_uri; #与$uri相同。   
10.fastcgi_param  DOCUMENT_ROOT      $document_root; #网站的根目录。在server配置中root指令中指定的值   
11.fastcgi_param  SERVER_PROTOCOL    $server_protocol; #请求使用的协议，通常是HTTP/1.0或HTTP/1.1。    
12.  
13.fastcgi_param  GATEWAY_INTERFACE  CGI/1.1;#cgi 版本  
14.fastcgi_param  SERVER_SOFTWARE    nginx/$nginx_version;#nginx 版本号，可修改、隐藏  
15.  
16.fastcgi_param  REMOTE_ADDR        $remote_addr; #客户端IP  
17.fastcgi_param  REMOTE_PORT        $remote_port; #客户端端口  
18.fastcgi_param  SERVER_ADDR        $server_addr; #服务器IP地址  
19.fastcgi_param  SERVER_PORT        $server_port; #服务器端口  
20.fastcgi_param  SERVER_NAME        $server_name; #服务器名，域名在server配置中指定的server_name  
21.  
22.#fastcgi_param  PATH_INFO           $path_info;#可自定义变量  
23.  
24.# PHP only, required if PHP was built with --enable-force-cgi-redirect  
25.#fastcgi_param  REDIRECT_STATUS    200;  











FastCGI协议报文的分析 
http://xiaoxia.org/?p=1891

不知道什么时候，就开始有了让HomeServer支持PHP的念头。于是分析起了FastCGI协议。FastCGI用于WebServer与WebApplication之间的通讯，例如Apache与PHP程序。

FastCGI协议数据包是8字节对齐的，由包头(Header)和包体(Body)组成。例如要请求一个index.php的页面，WebServer首先向WebApp发送一个Request数据包。包头有个请求ID用于并行工作时，区别不同的请求。

包头

[版本:1][类型:1][请求ID:2][数据长度:2][填充字节数:1][保留:1]

包体

[角色:2][参数:1][保留:5]

接着，再发送一个Params数据包，用于传递执行页面所需要的参数和环境变量。

包头

[版本:1][类型:1][请求ID:2][数据长度:2][填充字节数:1][保留:1]

包体

[名称长度:1或4][值长度:1或4][名称:变长][值:变长] ...

其中，名称和值的长度占用的字节数是可变，取决于第一个字节（高位）的最高位是否为1，为1则长度是4个字节，否则为1个字节。即如果长度不超过128字节，就用一个字节来保存长度足够了。

参数发送后还要发送一个没有包体，只有包头的空的Params数据包，用来表示参数发送结束。

如果请求页面时POST方式，还会发送表单数据。这就要用到Stdin数据包了。

包头

[版本:1][类型:1][请求ID:2][数据长度:2][填充字节数:1][保留:1]

包体

[数据内容:长度在包头中设置，8字节对齐]

有时候POST的数据大于或等于64KB，就不能使用一个Stdin数据包发送完毕了，需要使用多次Stdin数据包来完成所有数据的传输。与Params数据包一样，结尾要发送一个没有包体，只有包头的空的Stdin数据包，用来表示参数发送结束。

至此，WebServer要提供给WebApplication的数据已经发送完毕。接着就接收来自WebApplication的数据了。

数据接收包Stdout与Stdin是差不多的，这里不再描述。不过接收到的数据由HTTP头和网页数据两部分组成，WebServer要对其做一定的处理后才能发送到浏览器。同Stdin数据包一样，WebServer会接收到一个来自WebApplication的Stdout的空数据包，表示接收的Stdout数据已经完毕。

最后，WebApplication会发送一个包含状态的EndRequest数据包，至此，一次页面请求处理完毕。

下面给出一些相关结构参考。

通用包头：

typedef struct {
    unsigned char version;
    unsigned char type;
    unsigned char requestIdB1;
    unsigned char requestIdB0;
    unsigned char contentLengthB1;
    unsigned char contentLengthB0;
    unsigned char paddingLength;
    unsigned char reserved;
}FCGI_Header;
 
typedef struct {
    unsigned char roleB1;
    unsigned char roleB0;
    unsigned char flags;
    unsigned char reserved[5];
} FCGI_BeginRequestBody;
 
typedef struct {
    FCGI_Header header;
    FCGI_BeginRequestBody body;
} FCGI_BeginRequestRecord;
 
typedef struct {
    unsigned char appStatusB3;
    unsigned char appStatusB2;
    unsigned char appStatusB1;
    unsigned char appStatusB0;
    unsigned char protocolStatus;
    unsigned char reserved[3];
} FCGI_EndRequestBody;
每次请求页面时，传递给PHP程序的参数：

SCRIPT_FILENAME,

QUERY_STRING,

REQUEST_METHOD,

CONTENT_TYPE,

CONTENT_LENGTH,

SCRIPT_NAME,

REQUEST_URI,

DOCUMENT_URI,

DOCUMENT_ROOT,

SERVER_PROTOCOL,

GATEWAY_INTERFACE,

SERVER_SOFTWARE,

REMOTE_ADDR,

REMOTE_PORT,

SERVER_ADDR,

SERVER_PORT,

SERVER_NAME,

REDIRECT_STATUS,

HTTP_ACCEPT,

HTTP_ACCEPT_LANGUAGE,

HTTP_ACCEPT_ENCODING,

HTTP_USER_AGENT,

HTTP_HOST,

HTTP_CONNECTION,

HTTP_CONTENT_TYPE,

HTTP_CONTENT_LENGTH,

HTTP_CACHE_CONTROL,

HTTP_COOKIE,

HTTP_FCGI_PARAMS_MAX











您可能会说基于HTTP接口的开发效率不错。是的，基于HTTP协议的开发效率很高，而且它适合各种网络环境。但是由于HTTP协议需要发送大量的头部，
所以导致性能不是很理想。那么有没有一种比HTTP协议性能好并且比基于TCP接口的开发效率高的解决方案呢？答案是肯定的，就是本文接下来要介绍的基于FastCGI的接口开发。


那么CGI程序的性能问题在哪呢？PHP解析器每次都会解析php.ini文件，初始化执行环境。标准的CGI对每个请求都会执行这些步骤，所以处理每个时间的时间会比较长。
那么FastCGI是怎么做的呢？首先，FastCGI会先启一个master，解析配置文件，初始化执行环境，然后再启动多个worker。当请求过来时，master会传递给一个worker，
然后立即可以接受下一个请求。这样就避免了重复的劳动，效率自然是高。而且当worker不够用时，master可以根据配置预先启动几个worker等着；当然空闲worker太多时，
也会停掉一些，这样就提高了性能，也节约了资源。这就是FastCGI的对进程的管理。











Fastcgi官方文档：http://www.fastcgi.com/devkit/doc/fcgi-spec.html

fastcgi中文详解:http://fuzhong1983.blog.163.com/blog/static/1684705201051002951763/
http://www.cppblog.com/woaidongmao/archive/2011/06/21/149097.html

http://my.oschina.net/goal/blog/196599  这里有图解说明，不错


nginx和php安装调试:http://www.cnblogs.com/jsckdao/archive/2011/05/05/2038265.html  
http://www.nginx.cn/231.html      nginx php-fpm安装配置
http://www.cnblogs.com/jsckdao/archive/2011/05/05/2038265.html nginx+php的配置 

php-cgi连接数多了后挂死原因:http://bbs.csdn.net/topics/380138192  修改源码php-5.3.8\sapi\cgi\cgi_main.c： int max_requests = 500;
改大点，或者直接修改环境变量PHP_FCGI_MAX_REQUESTS

/*


export PATH=/usr/local/php/bin:$PATH
export PATH=/usr/local/php/sbin:$PATH

注：修改文件后要想马上生效还要运行# source /etc/profile不然只能在下次重进此用户时生效。
php -m 检查有哪些模块加入
mem-php要先执行 phpize 然后在config make make install
PHP Warning:  PHP Startup: memcache: Unable to initialize module  出现这个一般是PHP和mem的版本不一致引起，可以通过看时间得到

PHP-FPM启动:http://www.4wei.cn/archives/1002061
找不到php-fpm.conf配置文件:http://blog.sina.com.cn/s/blog_ac08ce040101j2vi.html
测试mem-php是否装成功:http://blog.csdn.net/poechant/article/details/6802312

*/


*/
