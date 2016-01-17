
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 http://www.cnblogs.com/chengxuyuancc/p/3782808.html
 http://www.bkjia.com/ASPjc/905190.html
 http://www.oschina.net/question/234345_42065
 */
 
#ifndef _NGX_HASH_H_INCLUDED_
#define _NGX_HASH_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

/*
ngx_hash_elt_t中uchar name[1]的设计，如果在name很短的情况下，name和 ushort 的字节对齐可能只用占到一个字节，这样就比放一
个uchar* 的指针少占用一个字节，可以看到ngx是真的在内存上考虑，节省每一分内存来提高并发。
*/
//hash表中元素ngx_hash_elt_t 预添加哈希散列元素结构 ngx_hash_key_t
typedef struct {//实际一个ngx_hash_elt_t暂用的空间为NGX_HASH_ELT_SIZE，比sizeof(ngx_hash_elt_t)大，因为需要存储具体的字符串name
    //如果value=NULL,表示是该具体hash桶bucket[i]中的最后一个ngx_hash_elt_t成员
    void             *value;//value，即某个key对应的值，即<key,value>中的value   
    u_short           len;//name长度  key的长度    
    //每次移动test[]的时候，都是移动NGX_HASH_ELT_SIZE(&names[n])，里面有给name预留name字符串长度空间，见ngx_hash_init
    u_char            name[1]; //某个要hash的数据(在nginx中表现为字符串)，即<key,value>中的key     都是小写字母

//ngx_hash_elt_t是ngx_hash_t->buckets[i]桶中的具体成员
} ngx_hash_elt_t; //hash元素结构   //ngx_hash_init中names数组存入hash桶前，其结构是ngx_hash_key_t形式，在往hash桶里面存数据的时候，会把ngx_hash_key_t里面的成员拷贝到ngx_hash_elt_t中相应成员  


/*
这里面的ngx_hash_keys_arrays_t桶keys_hash  dns_wc_head_hash  dns_wc_tail_hash，对应的具体桶中的空间是数组，数组大小是提前进行数组初始化的时候设置好的
ngx_hash_t->buckets[]中的具体桶中的成员是根据实际成员个数创建的空间
*/

//在创建hash桶的时候赋值，见ngx_hash_init
typedef struct { //hash桶遍历可以参考ngx_hash_find  
    ngx_hash_elt_t  **buckets; //hash桶(有size个桶)    指向各个桶的头部指针，也就是bucket[]数组，bucket[I]又指向每个桶中的第一个ngx_hash_elt_t成员，见ngx_hash_init
    ngx_uint_t        size;//hash桶个数，注意是桶的个数，不是每个桶中的成员个数，见ngx_hash_init
} ngx_hash_t;

/*
这个结构主要用于包含通配符的hash的这个结构相比ngx_hash_t结构就是多了一个value指针，value这个字段是用来存放某个已经达到末尾的通配符url对应的value值，
如果通配符url没有达到末尾，这个字段为NULL。

ngx_hash_wildcard_t专用于表示牵制或后置通配符的哈希表，如：前置*.test.com，后置:www.test.* ，它只是对ngx_hash_t的简单封装，
是由一个基本哈希表hash和一个额外的value指针，当使用ngx_hash_wildcard_t通配符哈希表作为容器元素时，可以使用value指向用户数据。
*/
/*
nginx为了处理带有通配符的域名的匹配问题，实现了ngx_hash_wildcard_t这样的hash表。他可以支持两种类型的带有通配符的域名。一种是通配符在前的，
例如：“*.abc.com”，也可以省略掉星号，直接写成”.abc.com”。这样的key，可以匹配www.abc.com，qqq.www.abc.com之类的。另外一种是通配符在末
尾的，例如：“mail.xxx.*”，请特别注意通配符在末尾的不像位于开始的通配符可以被省略掉。这样的通配符，可以匹配mail.xxx.com、mail.xxx.com.cn、
mail.xxx.net之类的域名。

有一点必须说明，就是一个ngx_hash_wildcard_t类型的hash表只能包含通配符在前的key或者是通配符在后的key。不能同时包含两种类型的通配符
的key。ngx_hash_wildcard_t类型变量的构建是通过函数ngx_hash_wildcard_init完成的，而查询是通过函数ngx_hash_find_wc_head或者
ngx_hash_find_wc_tail来做的。ngx_hash_find_wc_head是查询包含通配符在前的key的hash表的，而ngx_hash_find_wc_tail是查询包含通配符在后的key的hash表的。
*/
typedef struct {
    ngx_hash_t        hash;
    void             *value;
} ngx_hash_wildcard_t;


//hash表中元素ngx_hash_elt_t 预添加哈希散列元素结构 ngx_hash_key_t
typedef struct { //赋值参考ngx_hash_add_key
    ngx_str_t         key; //key，为nginx的字符串结构   
    ngx_uint_t        key_hash; //由该key计算出的hash值(通过hash函数如ngx_hash_key_lc())  
    void             *value;  //该key对应的值，组成一个键-值对<key,value>    在通配符hash中也可能指向下一个通配符hash,见ngx_hash_wildcard_init
} ngx_hash_key_t; //ngx_hash_init中names数组存入hash桶前，其结构是ngx_hash_key_t形式，在往hash桶里面存数据的时候，会把ngx_hash_key_t里面的成员拷贝到ngx_hash_elt_t中相应成员


/*
┏━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃  ┃    表7-8 Nginx提供的两种散列方法                                                                     ┃
┣━╋━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃  ┃    散列方法                                      ┃    意义                                          ┃
┣━┻━━━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_uint_t ngx_hash_key(u_char *data, size_t len)     ┃  使用BKDR算法将任意长度的字符串映射为整型        ┃
┣━━━━━━━━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                                      ┃  将字符串全小写后，再使用BKDR算法将任意长度的字  ┃
┃.gx_uint_t ngx_hash_key_lc(I_char *data, size_t len)  ┃符串映射为整型                                    ┃
┗━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━┛
*/
typedef ngx_uint_t (*ngx_hash_key_pt) (u_char *data, size_t len);

/*
Nginx对于server- name主机名通配符的支持规则。
    首先，选择所有字符串完全匹配的server name，如www.testweb.com。
    其次，选择通配符在前面的server name，如*.testweb.com。
    再次，选择通配符在后面的server name，如www.testweb.*。

ngx_hash_combined_t是由3个哈希表组成，一个普通hash表hash，一个包含前向通配符的hash表wc_head和一个包含后向通配符的hash表 wc_tail。
*/ //ngx_http_virtual_names_t中包含该结构
typedef struct { //这里面的hash信息是ngx_http_server_names中存储到hash表中的server_name及其所在server{}上下文ctx,server_name为key，上下文ctx为value
    //该hash中存放的是ngx_hash_keys_arrays_t->keys[]数组中的成员
    ngx_hash_t            hash; //普通hash，完全匹配
    //该wc_head hash中存储的是ngx_hash_keys_arrays_t->dns_wc_head数组中的成员
    ngx_hash_wildcard_t  *wc_head; //前置通配符hash
    //该wc_head hash中存储的是ngx_hash_keys_arrays_t->dns_wc_tail数组中的成员
    ngx_hash_wildcard_t  *wc_tail; //后置通配符hash
} ngx_hash_combined_t;

//参考http://www.oschina.net/question/234345_42065  http://www.bkjia.com/ASPjc/905190.html
typedef struct {//如何使用以及使用方法可以参考ngx_http_referer_merge_conf
    ngx_hash_t       *hash;//指向待初始化的hash结构
    ngx_hash_key_pt   key; //hash函数指针   

    /*  max_size和bucket_size的意义
    max_size表示最多分配max_size个桶，每个桶中的元素(ngx_hash_elt_t)个数 * NGX_HASH_ELT_SIZE(&names[n])不能超过bucket_size大小
    实际ngx_hash_init处理的时候并不是直接用max_size个桶，而是从x=1到max_size去试，只要ngx_hash_init参数中的names[]数组数据能全部hash
    到这x个桶中，并且满足条件:每个桶中的元素(ngx_hash_elt_t)个数 * NGX_HASH_ELT_SIZE(&names[n])不超过bucket_size大小,则说明用x
    个桶就够用了，然后直接使用x个桶存储。 见ngx_hash_init
     */
    
    ngx_uint_t        max_size;//最多需要这么多个桶，实际上桶的个数，是通过计算得到的，参考ngx_hash_init
    //分配到该具体桶中的成员个数 * NGX_HASH_ELT_SIZE(&names[n])要小于bucket_size，表示该桶中最多存储这么多个ngx_hash_elt_t成员
    //生效地方见ngx_hash_init中的:if (test[key] > (u_short) bucket_size) { }
    ngx_uint_t        bucket_size; //表示每个hash桶中(hash->buckets[i->成员[i]])对应的成员所有ngx_hash_elt_t成员暂用空间和的最大值，就是每个桶暂用的所有空间最大值，通过这个值计算需要多少个桶
    char             *name; //该hash结构的名字(仅在错误日志中使用,用于调试打印等)   
    ngx_pool_t       *pool;  //该hash结构从pool指向的内存池中分配   
    ngx_pool_t       *temp_pool; //分配临时数据空间的内存池   
} ngx_hash_init_t; //hash初始化结构   

#define NGX_HASH_SMALL            1 //NGX_HASH_SMALL表示初始化元素较少
#define NGX_HASH_LARGE            2 //NGX_HASH_LARGE表示初始化元素较多，

#define NGX_HASH_LARGE_ASIZE      16384
#define NGX_HASH_LARGE_HSIZE      10007

#define NGX_HASH_WILDCARD_KEY     1 //通配符类型
#define NGX_HASH_READONLY_KEY     2


/*
typedef struct  {
    ／★下面的keys_hash、dna- wc head hash、dns wc tail_ hash都是简易散列表，而hsize指明
了散列表的槽个数，其简易散列方法也需要对hsize求余+／
    ngx_uint_t hsize;
    //内存池，用于分配永久性内存，到目前的Nginx版本为止，该pool成员没有任何意义+／
    ngx_pool_t *pool;
    //临时内存池，下面的动态数组需要的内存都由temp_pool内存池分配
    ngx_pool_t *temp_pool;
    //用动态数组以ngx_hash_key_t结构体保存着不含有通配符关键字的元素
    ngx_array_t keys;
   //一个极其简易的散列表，它以数组的形式保存着hsize个元素，每个元素都是ngx_array_t动态数
组。在用户添加的元素过程中，会根据关键码将用户的ngx_str_t类型的关键字添加到ngx_array_t动态数组中。
这里所有的用户元素的关键字都不可以带通配符，表示精确匹配+／
    ngx—array_t *keys_hash;
   //用动态数组以ngx-- hash- key_七结构体保存着含有前置通配符关键字的元素生成的中间关键字+／
    ngx_array_t dns_wc_head;
    //一个极其简易的散列表，它以数组的形式保存着hsize个元素，每个元索都是ngx—array—t动态数
组。在用户添加元素过程中，会根据关键码将用户的ngx_str_t类型的关键字添加到ngx—array一七动态数组中。
这里所有的用户元素的关键字都带前置通配符
    ngx_array_t   *dns_wc_head_haah;
    //用动态数组以ngx_ha sh_key_t结构体保存着含有后置通配符关键字的元素生成的中间关键字t／
    ngx_array_t dns_wc_tail;
    //一个极其简易的散列表，它以数组的形式保存着hsize个元素，每个元素都是ngx—array_七动态数
组。在用户添加元素过程中，会根据关键码将用户酌ngx_ str_七类型的关键字添加到ngx_array_t动态数组中。
这里所有的用户元素的关键字都带后置通配符．／
    ngx—array_t *dns_wc_tail_hash;
    }  ngx_hash_keys_arrays_t,

*/
/*
hsize: 将要构建的hash表的桶的个数。对于使用这个结构中包含的信息构建的三种类型的hash表都会使用此参数。 
pool: 构建这些hash表使用的pool。 
temp_pool: 在构建这个类型以及最终的三个hash表过程中可能用到临时pool。该temp_pool可以在构建完成以后，被销毁掉。这里只是存放临时的一些内存消耗。 
keys: 存放所有非通配符key的数组。 
keys_hash: 这是个二维数组，第一个维度代表的是bucket的编号，那么keys_hash[i]中存放的是所有的key算出来的hash值对hsize取模以后的值为i的key。假设有3个key,分别是key1,key2和key3假设hash值算出来以后对hsize取模的值都是i，那么这三个key的值就顺序存放在keys_hash[i][0],keys_hash[i][1], keys_hash[i][2]。该值在调用的过程中用来保存和检测是否有冲突的key值，也就是是否有重复。 
dns_wc_head: 放前向通配符key被处理完成以后的值。比如：“*.abc.com” 被处理完成以后，变成 “com.abc.” 被存放在此数组中。 
dns_wc_tail: 存放后向通配符key被处理完成以后的值。比如：“mail.xxx.*” 被处理完成以后，变成 “mail.xxx.” 被存放在此数组中。 
dns_wc_head_hash: 该值在调用的过程中用来保存和检测是否有冲突的前向通配符的key值，也就是是否有重复。 
dns_wc_tail_hash: 该值在调用的过程中用来保存和检测是否有冲突的后向通配符的key值，也就是是否有重复。 
*/

/*
注：keys，dns_wc_head，dns_wc_tail，三个数组中存放的元素时ngx_hash_key_t类型的，而keys_hash,dns_wc_head_hash，dns_wc_tail_hash，
三个二维数组中存放的元素是ngx_str_t类型的。
ngx_hash_keys_array_init就是为上述结构分配空间。
*/ //该结构只是用来把完全匹配   前置匹配  后置匹配通过该结构体全部存储在该结构体对应的hash和数组中
typedef struct { //该结构一般用来存储域名信息
    //散列中槽总数  如果是大hash桶方式，则hsize=NGX_HASH_LARGE_HSIZE,小hash桶方式，hsize=107,见ngx_hash_keys_array_init
    ngx_uint_t        hsize; 

    ngx_pool_t       *pool;//内存池，用于分配永久性的内存
    ngx_pool_t       *temp_pool; //临时内存池，下面的临时动态数组都是由临时内存池分配

    //下面这几个实际上是hash通的各个桶的头部指针，每个hash有ha->hsize个桶头部指针，在ngx_hash_add_key的时候头部指针指向每个桶中具体的成员列表
    //下面的这些可以参考ngx_hash_add_key
     /*
    keys_hash这是个二维数组，第一个维度代表的是bucket的编号，那么keys_hash[i]中存放的是所有的key算出来的hash值对hsize取模以后的值为i的key。
    假设有3个key,分别是key1,key2和key3假设hash值算出来以后对hsize取模的值都是i，那么这三个key的值就顺序存放在keys_hash[i][0],
    keys_hash[i][1], keys_hash[i][2]。该值在调用的过程中用来保存和检测是否有冲突的key值，也就是是否有重复。
    */ //完全匹配keys数组只存放key字符串，节点类型为ngx_str_t，keys_hash hash表存放key对应的key-value中，hash表中数据节点类型为ngx_hash_key_t

   /* 
    hash桶keys_hash  dns_wc_head_hash   dns_wc_tail_hash头部指针信息初始化在ngx_hash_keys_array_init，其中的具体
    桶keys_hash[i] dns_wc_head_hash[i]  dns_wc_tail_hash[i]中的数据类型为ngx_str_t，每个桶的数据成员默认4个，见ngx_hash_add_key，
    桶中存储的数据信息就是ngx_hash_add_key参数中key参数字符串
    
    数组keys[] dns_wc_head[] dns_wc_tail[]中的数据类型为ngx_hash_key_t，见ngx_hash_keys_array_init，
    ngx_hash_key_t中的key和value分别存储ngx_hash_add_key中的key参数和value参数
    */

    /*

    赋值见ngx_hash_add_key
    
    原始key                  存放到hash桶(keys_hash或dns_wc_head_hash                 存放到数组中(keys或dns_wc_head或
                                    或dns_wc_tail_hash)                                     dns_wc_tail)
                                    
 www.example.com                 www.example.com(存入keys_hash)                        www.example.com (存入keys数组成员ngx_hash_key_t对应的key中)
  .example.com             example.com(存到keys_hash，同时存入dns_wc_tail_hash)        com.example  (存入dns_wc_head数组成员ngx_hash_key_t对应的key中)
 www.example.*                     www.example. (存入dns_wc_tail_hash)                 www.example  (存入dns_wc_tail数组成员ngx_hash_key_t对应的key中)
 *.example.com                     example.com  (存入dns_wc_head_hash)                 com.example. (存入dns_wc_head数组成员ngx_hash_key_t对应的key中)

    //这里面的ngx_hash_keys_arrays_t桶keys_hash  dns_wc_head_hash  dns_wc_tail_hash，对应的具体桶中的空间是数组，数组大小是提前进行数组初始化的时候设置好的
    ngx_hash_t->buckets[]中的具体桶中的成员是根据实际成员个数创建的空间
    */
    ngx_array_t       keys;//数组成员ngx_hash_key_t
    ngx_array_t      *keys_hash;//keys_hash[i]对应hash桶头部指针，，具体桶中成员类型ngx_str_t

    ngx_array_t       dns_wc_head; //数组成员ngx_hash_key_t
    ngx_array_t      *dns_wc_head_hash;//dns_wc_head_hash[i]对应hash桶头部指针，具体桶中成员类型ngx_str_t

    ngx_array_t       dns_wc_tail; //数组成员ngx_hash_key_t
    ngx_array_t      *dns_wc_tail_hash; //dns_wc_tail_hash[i]对应hash桶头部指针，具体桶中成员类型ngx_str_t
} ngx_hash_keys_arrays_t; //ngx_http_referer_conf_t中的keys成员

/*
ngx_table_elt_t数据结构如下所示：
typedef struct {
    ngx_uint_t        hash;
    ngx_str_t         key;
    ngx_str_t         value;
    u_char           *lowcase_key;
} ngx_table_elt_t;

可以看到，ngx_table_elt_t就是一个key/value对，ngx_str_t 类型的key、value成员分别存储的是名字、值字符串。
hash成员表明ngx_table_elt_t也可以是某个散列表数据结构（ngx_hash_t类型）中的成员。ngx_uint_t 类型的hash
成员可以在ngx_hash_t中更快地找到相同key的ngx_table_elt_t数据。lowcase_key指向的是全小写的key字符串。

显而易见，ngx_table_elt_t是为HTTP头部“量身订制”的，其中key存储头部名称（如Content-Length），value存储对应的值（如“1024”），
lowcase_key是为了忽略HTTP头部名称的大小写（例如，有些客户端发来的HTTP请求头部是content-length，Nginx希望它与大小写敏感的
Content-Length做相同处理，有了全小写的lowcase_key成员后就可以快速达成目的了），hash用于快速检索头部（它的用法在3.6.3节中进行详述）。
*/
typedef struct {
    ngx_uint_t        hash; //等于ngx_http_request_s->header_hash ,这是通过key value字符串计算出的hash值
    ngx_str_t         key;
    ngx_str_t         value;
    u_char           *lowcase_key; //存放的是本结构体中key的小写字母字符串
} ngx_table_elt_t;


void *ngx_hash_find(ngx_hash_t *hash, ngx_uint_t key, u_char *name, size_t len);
void *ngx_hash_find_wc_head(ngx_hash_wildcard_t *hwc, u_char *name, size_t len);
void *ngx_hash_find_wc_tail(ngx_hash_wildcard_t *hwc, u_char *name, size_t len);
void *ngx_hash_find_combined(ngx_hash_combined_t *hash, ngx_uint_t key,
    u_char *name, size_t len);

ngx_int_t ngx_hash_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names,
    ngx_uint_t nelts);
ngx_int_t ngx_hash_wildcard_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names,
    ngx_uint_t nelts);

#define ngx_hash(key, c)   ((ngx_uint_t) key * 31 + c)
ngx_uint_t ngx_hash_key(u_char *data, size_t len);
ngx_uint_t ngx_hash_key_lc(u_char *data, size_t len);
ngx_uint_t ngx_hash_strlow(u_char *dst, u_char *src, size_t n);


ngx_int_t ngx_hash_keys_array_init(ngx_hash_keys_arrays_t *ha, ngx_uint_t type);
ngx_int_t ngx_hash_add_key(ngx_hash_keys_arrays_t *ha, ngx_str_t *key,
    void *value, ngx_uint_t flags);

#endif /* _NGX_HASH_H_INCLUDED_ */

