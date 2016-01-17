
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>

/*
To quickly process static sets of data such as server names, map directive’s values, MIME types, names of request header strings, nginx 
uses hash tables. During the start and each re-configuration nginx selects the minimum possible sizes of hash tables such that the bucket 
size that stores keys with identical hash values does not exceed the configured parameter (hash bucket size). The size of a table is 
expressed in buckets. The adjustment is continued until the table size exceeds the hash max size parameter. Most hashes have the 
corresponding directives that allow changing these parameters, for example, for the server names hash they are server_names_hash_max_size 
and server_names_hash_bucket_size. 

The hash bucket size parameter is aligned to the size that is a multiple of the processor’s cache line size. This speeds up key search in 
a hash on modern processors by reducing the number of memory accesses. If hash bucket size is equal to one processor’s cache line size 
then the number of memory accesses during the key search will be two in the worst case — first to compute the bucket address, and second 
during the key search inside the bucket. Therefore, if nginx emits the message requesting to increase either hash max size or hash bucket 
size then the first parameter should first be increased. 
*/
void *
ngx_hash_find(ngx_hash_t *hash, ngx_uint_t key, u_char *name, size_t len)
{
    ngx_uint_t       i;
    ngx_hash_elt_t  *elt;

#if 0
    ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0, "hf:\"%*s\"", len, name);
#endif

    elt = hash->buckets[key % hash->size];

    if (elt == NULL) {
        return NULL;
    }

    while (elt->value) {
        if (len != (size_t) elt->len) {
            goto next;
        }

        for (i = 0; i < len; i++) {
            if (name[i] != elt->name[i]) {
                goto next;
            }
        }

        return elt->value;

    next:

        elt = (ngx_hash_elt_t *) ngx_align_ptr(&elt->name[0] + elt->len,
                                               sizeof(void *));
        continue;
    }

    return NULL;
}

/*
nginx为了处理带有通配符的域名的匹配问题，实现了ngx_hash_wildcard_t这样的hash表。他可以支持两种类型的带有通配符的域名。一种是通配符在前的，
例如：“*.abc.com”，也可以省略掉星号，直接写成”.abc.com”。这样的key，可以匹配www.abc.com，qqq.www.abc.com之类的。另外一种是通配符在末
尾的，例如：“mail.xxx.*”，请特别注意通配符在末尾的不像位于开始的通配符可以被省略掉。这样的通配符，可以匹配mail.xxx.com、mail.xxx.com.cn、
mail.xxx.net之类的域名。

有一点必须说明，就是一个ngx_hash_wildcard_t类型的hash表只能包含通配符在前的key或者是通配符在后的key。不能同时包含两种类型的通配符
的key。ngx_hash_wildcard_t类型变量的构建是通过函数ngx_hash_wildcard_init完成的，而查询是通过函数ngx_hash_find_wc_head或者
ngx_hash_find_wc_tail来做的。ngx_hash_find_wc_head是查询包含通配符在前的key的hash表的，而ngx_hash_find_wc_tail是查询包含通配符在后的key的hash表的。
*/
void *
ngx_hash_find_wc_head(ngx_hash_wildcard_t *hwc, u_char *name, size_t len)
{
    void        *value;
    ngx_uint_t   i, n, key;

#if 0
    ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0, "wch:\"%*s\"", len, name);
#endif

    n = len;
    
    //从后往前搜索第一个dot，则n 到 len-1 即为关键字中最后一个 子关键字
    while (n) { //name中最后面的字符串，如 AA.BB.CC.DD，则这里获取到的就是DD
        if (name[n - 1] == '.') {
            break;
        }

        n--;
    }

    key = 0;
    
    //n 到 len-1 即为关键字中最后一个 子关键字，计算其hash值
    for (i = n; i < len; i++) {
        key = ngx_hash(key, name[i]);
    }

#if 0
    ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0, "key:\"%ui\"", key);
#endif

    //调用普通查找找到关键字的value（用户自定义数据指针）
    value = ngx_hash_find(&hwc->hash, key, &name[n], len - n);

#if 0
    ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0, "value:\"%p\"", value);
#endif

    if (value) {

        /*
         * the 2 low bits of value have the special meaning:
         *     00 - value is data pointer for both "example.com"
         *          and "*.example.com";
         *     01 - value is data pointer for "*.example.com" only;
         *     10 - value is pointer to wildcard hash allowing
         *          both "example.com" and "*.example.com";
         *     11 - value is pointer to wildcard hash allowing
         *          "*.example.com" only.
         */

        if ((uintptr_t) value & 2) {

            if (n == 0) { //搜索到了最后一个子关键字且没有通配符，如"example.com"的example

                /* "example.com" */

                if ((uintptr_t) value & 1) {//value低两位为11，仅为"*.example.com"的指针，这里没有通配符，没招到，返回NULL
                    return NULL;
                }
             //value低两位为10，为"example.com"的指针，value就在下一级的ngx_hash_wildcard_t 的value中，去掉携带的低2位11    参考ngx_hash_wildcard_init
                hwc = (ngx_hash_wildcard_t *)
                                          ((uintptr_t) value & (uintptr_t) ~3);
                return hwc->value;
            }

            //还未搜索完，低两位为11或10，继续去下级ngx_hash_wildcard_t中搜索
            hwc = (ngx_hash_wildcard_t *) ((uintptr_t) value & (uintptr_t) ~3); //把最低的两个地址为清0还回去，参考ngx_hash_wildcard_init

            //继续搜索 关键字中剩余部分，如"example.com"，搜索 0 到 n -1 即为 example
            value = ngx_hash_find_wc_head(hwc, name, n - 1);

            if (value) {//若找到，则返回
                return value;
            }

            return hwc->value; //低两位为00 找到，即为wc->value
        }

        if ((uintptr_t) value & 1) { //低两位为01

            if (n == 0) { //关键字没有通配符，错误返回空

                /* "example.com" */

                return NULL;
            }

            return (void *) ((uintptr_t) value & (uintptr_t) ~3);//有通配符，直接返回
        }

        return value; //低两位为00，直接返回
    }

    return hwc->value;
}

/*
nginx为了处理带有通配符的域名的匹配问题，实现了ngx_hash_wildcard_t这样的hash表。他可以支持两种类型的带有通配符的域名。一种是通配符在前的，
例如：“*.abc.com”，也可以省略掉星号，直接写成”.abc.com”。这样的key，可以匹配www.abc.com，qqq.www.abc.com之类的。另外一种是通配符在末
尾的，例如：“mail.xxx.*”，请特别注意通配符在末尾的不像位于开始的通配符可以被省略掉。这样的通配符，可以匹配mail.xxx.com、mail.xxx.com.cn、
mail.xxx.net之类的域名。

有一点必须说明，就是一个ngx_hash_wildcard_t类型的hash表只能包含通配符在前的key或者是通配符在后的key。不能同时包含两种类型的通配符
的key。ngx_hash_wildcard_t类型变量的构建是通过函数ngx_hash_wildcard_init完成的，而查询是通过函数ngx_hash_find_wc_head或者
ngx_hash_find_wc_tail来做的。ngx_hash_find_wc_head是查询包含通配符在前的key的hash表的，而ngx_hash_find_wc_tail是查询包含通配符在后的key的hash表的。

hinit: 构造一个通配符hash表的一些参数的一个集合。关于该参数对应的类型的说明，请参见ngx_hash_t类型中ngx_hash_init函数的说明。 

names: 构造此hash表的所有的通配符key的数组。特别要注意的是这里的key已经都是被预处理过的。例如：“*.abc.com”或者“.abc.com”
被预处理完成以后，变成了“com.abc.”。而“mail.xxx.*”则被预处理为“mail.xxx.”。为什么会被处理这样？这里不得不简单地描述一下
通配符hash表的实现原理。当构造此类型的hash表的时候，实际上是构造了一个hash表的一个“链表”，是通过hash表中的key“链接”起来的。
比如：对于“*.abc.com”将会构造出2个hash表，第一个hash表中有一个key为com的表项，该表项的value包含有指向第二个hash表的指针，
而第二个hash表中有一个表项abc，该表项的value包含有指向*.abc.com对应的value的指针。那么查询的时候，比如查询www.abc.com的时候，
先查com，通过查com可以找到第二级的hash表，在第二级hash表中，再查找abc，依次类推，直到在某一级的hash表中查到的表项对应的value对
应一个真正的值而非一个指向下一级hash表的指针的时候，查询过程结束。这里有一点需要特别注意的，就是names数组中元素的value所对应的
值（也就是真正的value所在的地址）必须是能被4整除的，或者说是在4的倍数的地址上是对齐的。因为这个value的值的低两位bit是有用的，
所以必须为0。如果不满足这个条件，这个hash表查询不出正确结果。 

nelts: names数组元素的个数。 

*/
/* 
@hwc  表示支持通配符的哈希表的结构体   
@name 表示实际关键字地址  
@len  表示实际关键字长度   

ngx_hash_find_wc_tail与前置通配符查找差不多，这里value低两位仅有两种标志，更加简单：

00 - value 是指向 用户自定义数据
11 - value的指向下一个哈希表 
*/
void *
ngx_hash_find_wc_tail(ngx_hash_wildcard_t *hwc, u_char *name, size_t len)
{
    void        *value;
    ngx_uint_t   i, key;

#if 0
    ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0, "wct:\"%*s\"", len, name);
#endif

    key = 0;

    //从前往后搜索第一个dot，则0 到 i 即为关键字中第一个 子关键字
    for (i = 0; i < len; i++) {
        if (name[i] == '.') {
            break;
        }

        key = ngx_hash(key, name[i]); //计算哈希值
    }

    if (i == len) {  //没有通配符，返回NULL
        return NULL;
    }

#if 0
    ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0, "key:\"%ui\"", key);
#endif

    value = ngx_hash_find(&hwc->hash, key, name, i); //调用普通查找找到关键字的value（用户自定义数据指针）

#if 0
    ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0, "value:\"%p\"", value);
#endif

    /*
    还记得上节在ngx_hash_wildcard_init中，用value指针低2位来携带信息吗？其是有特殊意义的，如下：  
    * 00 - value 是数据指针   
    * 11 - value的指向下一个哈希表   
    */
    if (value) {

        /*
         * the 2 low bits of value have the special meaning:
         *     00 - value is data pointer;
         *     11 - value is pointer to wildcard hash allowing "example.*".
         */

        if ((uintptr_t) value & 2) {//低2位为11，value的指向下一个哈希表，递归搜索

            i++;

            hwc = (ngx_hash_wildcard_t *) ((uintptr_t) value & (uintptr_t) ~3);

            value = ngx_hash_find_wc_tail(hwc, &name[i], len - i);

            if (value) { //找到低两位00，返回
                return value;
            }

            return hwc->value; //找打低两位11，返回hwc->value
        }

        return value;
    }

    return hwc->value; //低2位为00，直接返回数据
}

//从hash表中查找对应的key - name
void *
ngx_hash_find_combined(ngx_hash_combined_t *hash, ngx_uint_t key, u_char *name,
    size_t len)
{
    void  *value;

    if (hash->hash.buckets) {  //在普通hash表中查找
        value = ngx_hash_find(&hash->hash, key, name, len);

        if (value) {
            return value;
        }
    }

    if (len == 0) {
        return NULL;
    }

    if (hash->wc_head && hash->wc_head->hash.buckets) { //在前置通配符哈希表中查找
        value = ngx_hash_find_wc_head(hash->wc_head, name, len);

        if (value) {
            return value;
        }
    }

    if (hash->wc_tail && hash->wc_tail->hash.buckets) { //在后置通配符哈希表中查找
        value = ngx_hash_find_wc_tail(hash->wc_tail, name, len);

        if (value) {
            return value;
        }
    }

    return NULL;
}

/*
NGX_HASH_ELT_SIZE宏用来计算ngx_hash_elt_t结构大小，定义如下。
在32位平台上，sizeof(void*)=4，(name)->key.len即是ngx_hash_elt_t结构中name数组保存的内容的长度，其中的"+2"是要加上该结构中len字段(u_short类型)的大小。
*/
#define NGX_HASH_ELT_SIZE(name)                                               \
    (sizeof(void *) + ngx_align((name)->key.len + 2, sizeof(void *)))

/*
其names参数是ngx_hash_key_t结构的数组，即键-值对<key,value>数组，nelts表示该数组元素的个数

该函数初始化的结果就是将names数组保存的键-值对<key,value>，通过hash的方式将其存入相应的一个或多个hash桶(即代码中的buckets)中，
该hash过程用到的hash函数一般为ngx_hash_key_lc等。hash桶里面存放的是ngx_hash_elt_t结构的指针(hash元素指针)，该指针指向一个基本
连续的数据区。该数据区中存放的是经hash之后的键-值对<key',value'>，即ngx_hash_elt_t结构中的字段<name,value>。每一个这样的数据
区存放的键-值对<key',value'>可以是一个或多个。
*/ //ngx_hash_init中names数组存入hash桶前，其结构是ngx_hash_key_t形式，在往hash桶里面存数据的时候，会把ngx_hash_key_t里面的成员拷贝到ngx_hash_elt_t中相应成员
//源代码，比较长，总的流程即为：预估需要的桶数量 –> 搜索需要的桶数量->分配桶内存->初始化每一个ngx_hash_elt_t
 /* nginx hash结构大致是这样: 
11.           hash结构中有N个桶, 每个桶存放N个元素(即<k,v>),在内存中, 
12.        用一个指针数组记录N个桶的地址,每个桶又是一个 ngx_hash_elt_t 数组 
13.        指针数组 和 ngx_hash_elt_t 数组 在一个连续的内存中. 
14.        优点: 使用数组提高寻址速度 
15.        缺点: hash表初始化后,只能查询,不能修改. 
16. 
17.        当然hash结构本来就是数组形式,但对于冲突的元素大多是用链表形式存放,再挂载到hash数组上. 
18.        nginx的hash结构过程: 
19.        首先 每个桶的空间大小固定 通过 ngx_hash_init_t.bucket_size 指定; 
20.        然后 根据元素的个数和桶的固定大小计算出需要多少个桶. 
21.        然后 计算哪些元素存放到哪个桶中,方法就是 (元素的hash值 % 桶的个数) 
22.        这时 需要多少个桶,这些桶需要多少内存空间,每个桶存放多少元素，需要多少内存空间就知道, 
23.        申请所有桶的内存空间,即为 ngx_hash_init_t.hash.buckets 指针数组. 
24.        申请每个桶存放元素的存储空间 = 该桶元素占用的内存空间 + void指针 
25.        为了提高查询效率,申请一个连续内存空间存放 所有桶的元素. 
26.        然后把这片连续的内存空间映射到 ngx_hash_init_t.hash.buckets 指针数组. 
27.        然后为每个桶的元素赋值. 
28.        最后将每个桶的"结束元素"置为NULL 
29. 
30.        void指针的用途: 为桶的结束标记, 在 ngx_hash_find() 遍历桶是判断 etl->value 是否为NULL时用到. 
31.        你可能有以为 "结束元素"只有一个void*指针的空间转换成 ngx_hash_elt_t 后会不会越界操作? 
32.        答案是不会的, void*指针的空间转换成 ngx_hash_elt_t 后只操作 ngx_hash_elt_t.value , 
33.        而 ngx_hash_elt_t.value 刚好只占 void指针空间大小. 
34. 
35.        指定桶的大小的好处: 保证每个桶存放元素的个数不超过一定值,目的是为了提高查询效率. 
36.         
37.     */  
    //使用方法可以参考ngx_http_server_names
ngx_int_t
ngx_hash_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names, ngx_uint_t nelts)//使用方法可以参考ngx_http_server_names
{
    //参考http://www.oschina.net/question/234345_42065  http://www.bkjia.com/ASPjc/905190.html
    u_char          *elts;
    size_t           len;
    u_short         *test;
    ngx_uint_t       i, n, key, 
                     size,  //size表示实际需要桶的个数
                     start, bucket_size;
    ngx_hash_elt_t  *elt, **buckets;

    for (n = 0; n < nelts; n++) {
        //检查names数组的每一个元素，判断桶的大小是否够分配 
        //names[n]成员空间一定要小于等于bucket_size /* 每个桶至少能存放一个元素 + 一个void指针  
        //要加上sizeof(void *)，因为bucket最后需要k-v对结束标志，是void * value来做的。
        if (hinit->bucket_size < NGX_HASH_ELT_SIZE(&names[n]) + sizeof(void *))
        {
            //有任何一个元素，桶的大小不够为该元素分配空间，则退出   
            ngx_log_error(NGX_LOG_EMERG, hinit->pool->log, 0,
                          "could not build the %s, you should "
                          "increase %s_bucket_size: %i",
                          hinit->name, hinit->name, hinit->bucket_size);
            return NGX_ERROR;
        }
    }

    //分配2*max_size个字节的空间保存hash数据(该内存分配操作不在nginx的内存池中进行，因为test只是临时的)    
    /* 用于记录每个桶的临时大小 */  
    test = ngx_alloc(hinit->max_size * sizeof(u_short), hinit->pool->log);
    if (test == NULL) {
        return NGX_ERROR;
    }

    // 实际可用空间为定义的bucket_size减去末尾的void *(结尾标识)，末尾的void* 指向NULL
    bucket_size = hinit->bucket_size - sizeof(void *);

    /* 下面这几行是大改估算一下，桶个数应该从多少个开始算 */
    start = nelts / (bucket_size / (2 * sizeof(void *)));
    start = start ? start : 1;
    if (hinit->max_size > 10000 && nelts && hinit->max_size / nelts < 100) {
        start = hinit->max_size - 1000;
    }

    //start表示计算桶的个数，桶的个数是算出来的，从start开始到max_size一个一个的试，最终要保证每个桶中的实际空间hinit->bucket_size - sizeof(void *);要能够
    //存放所有散列到该桶中的ngx_hash_elt_t空间个数和。
    //其实算这个桶的个数(通过所有元素空间小于hinit->max_size)就是为了保证每个桶中的元素个数别太多，这样可以保证在遍历hash表的时候，能够快速找到具体桶中的元素


    /*  max_size和bucket_size的意义
    max_size表示最多分配max_size个桶，每个桶中的元素(ngx_hash_elt_t)个数 * NGX_HASH_ELT_SIZE(&names[n])不能超过bucket_size大小
    实际ngx_hash_init处理的时候并不是直接用max_size个桶，而是从size=1到max_size去试，只要ngx_hash_init参数中的names[]数组数据能全部hash
    到这size个桶中，并且满足条件:每个桶中的元素(ngx_hash_elt_t)个数 * NGX_HASH_ELT_SIZE(&names[n])不超过bucket_size大小,则说明用size
    个桶就够用了，然后直接使用x个桶存储。 见ngx_hash_init
     */
    
    for (size = start; size <= hinit->max_size; size++) { //size表示实际需要桶的个数

        ngx_memzero(test, size * sizeof(u_short));

        
        //标记1：此块代码是检查bucket大小是否够分配hash数据   
        for (n = 0; n < nelts; n++) {
            if (names[n].key.data == NULL) {
                continue;
            }

            
            //计算key和names中所有name长度，并保存在test[key]中   
            key = names[n].key_hash % size;//若size=1，则key一直为0  
            //test[i]表示第i个桶中已经使用了的ngx_hash_elt_t空间总大小
            test[key] = (u_short) (test[key] + NGX_HASH_ELT_SIZE(&names[n])); 

#if 0
            ngx_log_error(NGX_LOG_ALERT, hinit->pool->log, 0,
                          "%ui: %ui %ui \"%V\"",
                          size, key, test[key], &names[n].key);
#endif
            //这里终于用到了bucket_size，大于这个值，则说明这个size不合适啊goto next，调整下桶的数目
            if (test[key] > (u_short) bucket_size) { 
                //若超过了桶的大小，则到下一个桶重新计算   
                goto next;
            }
        }

        goto found;

    next:

        continue;
    }

    size = hinit->max_size;
    
    //走到这里表面，在names中的元素入hash桶的时候，可能会造成某些hash桶的暂用空间会比实际的bucket_size大
    ngx_log_error(NGX_LOG_WARN, hinit->pool->log, 0,
                  "could not build optimal %s, you should increase "
                  "either %s_max_size: %i or %s_bucket_size: %i; "
                  "ignoring %s_bucket_size",
                  hinit->name, hinit->name, hinit->max_size,
                  hinit->name, hinit->bucket_size, hinit->name);

found://找到合适的bucket   

    //到这里后把所有的test[i]数组赋值为4，预留给NULL指针
    for (i = 0; i < size; i++) {
        test[i] = sizeof(void *);//将test数组前size个元素初始化为4，提前赋值4的原因是，hash桶的成员列表尾部会有一个NULL，提前把这4字节空间预留 
    }

   /* 标记2：与标记1代码基本相同，但此块代码是再次计算所有hash数据的总长度(标记1的检查已通过)  
      但此处的test[i]已被初始化为4，即相当于后续的计算再加上一个void指针的大小。  
    */ //计算每个桶中的成员空间大小总和
    for (n = 0; n < nelts; n++) {
        if (names[n].key.data == NULL) {
            continue;
        }

        //计算key和names中所有name长度，并保存在test[key]中   
        key = names[n].key_hash % size;//若size=1，则key一直为0   
        test[key] = (u_short) (test[key] + NGX_HASH_ELT_SIZE(&names[n]));
    }
    
    //计算hash数据的总长度，所有桶的数据空间长度和   
    len = 0;

    //len表示所有names[]数组数据一共需要x个ngx_hash_elt_t结构存储，这x个ngx_hash_elt_t暂用的空间总和，len是实际存储数据元素的空间，也就是存入所有桶中的元素暂用的空间
    for (i = 0; i < size; i++) {
        if (test[i] == sizeof(void *)) {//若test[i]仍为初始化的值4，即没有变化，则继续  
            continue;
        }
        
        //对test[i]按ngx_cacheline_size对齐(32位平台，ngx_cacheline_size=32)   
        test[i] = (u_short) (ngx_align(test[i], ngx_cacheline_size));

        len += test[i];
    }

    //分配每一个桶中指向具体数据部分的指针头
    if (hinit->hash == NULL) {
        //在内存池中分配hash头及buckets数组(size个ngx_hash_elt_t*结构)   
        hinit->hash = ngx_pcalloc(hinit->pool, sizeof(ngx_hash_wildcard_t)
                                             + size * sizeof(ngx_hash_elt_t *)); //size表示实际需要桶的个数，这里的空间刚好就是每个桶的头部指针
        if (hinit->hash == NULL) {
            ngx_free(test);
            return NGX_ERROR;
        }
        
        //计算buckets的启示位置(在ngx_hash_wildcard_t结构之后)  
        buckets = (ngx_hash_elt_t **)
                      ((u_char *) hinit->hash + sizeof(ngx_hash_wildcard_t));

    } else { //在内存池中分配buckets数组(size个ngx_hash_elt_t*结构)   
        buckets = ngx_pcalloc(hinit->pool, size * sizeof(ngx_hash_elt_t *)); //size表示实际需要桶的个数，这里的空间刚好就是每个桶的头部指针
        if (buckets == NULL) {
            ngx_free(test);
            return NGX_ERROR;
        }
    }
    
    //接着分配elts，大小为len+ngx_cacheline_size，此处为什么+32？——下面要按32字节对齐   
    elts = ngx_palloc(hinit->pool, len + ngx_cacheline_size); 
    //len表示所有names[](假设数组有x个成员)数组数据一共需要x个ngx_hash_elt_t结构存储，这x个ngx_hash_elt_t暂用的空间总和
    if (elts == NULL) {
        ngx_free(test);
        return NGX_ERROR;
    }
    
    //将elts地址按ngx_cacheline_size=32对齐   
    elts = ngx_align_ptr(elts, ngx_cacheline_size);

    for (i = 0; i < size; i++) {//将buckets数组与相应elts对应起来  
        if (test[i] == sizeof(void *)) {
            continue;
        }

        //每个桶头指针buckets[i]指向自己桶中的成员首地址
        buckets[i] = (ngx_hash_elt_t *) elts;
        elts += test[i];
    }

    for (i = 0; i < size; i++) {
        //test数组置0   
        test[i] = 0;
    }

    //把所有的name数据入队hash表中
    for (n = 0; n < nelts; n++) {//将传进来的每一个hash数据存入hash表 
        if (names[n].key.data == NULL) {
            continue;
        }

        
        //计算key，即将被hash的数据在第几个bucket，并计算其对应的elts位置，也就是在该buckets[i]桶中的具体位置
        key = names[n].key_hash % size;
        elt = (ngx_hash_elt_t *) ((u_char *) buckets[key] + test[key]);

        
        //对ngx_hash_elt_t结构赋值   
        elt->value = names[n].value;
        elt->len = (u_short) names[n].key.len;

        //每次移动test[]的时候，都是移动NGX_HASH_ELT_SIZE(&names[n])，里面有给name预留name字符串长度空间
        ngx_strlow(elt->name, names[n].key.data, names[n].key.len); 
        
        //计算下一个要被hash的数据的长度偏移，下一次就从该桶的下一个位置存储   
        test[key] = (u_short) (test[key] + NGX_HASH_ELT_SIZE(&names[n]));
    }

    //为每个桶的成员列表最尾部添加一个ngx_hash_elt_t成员，起value=NULL，标识这是该桶中的最后一个ngx_hash_elt_t
    for (i = 0; i < size; i++) {
        if (buckets[i] == NULL) {
            continue;
        }
        
        //test[i]相当于所有被hash的数据总长度   
        elt = (ngx_hash_elt_t *) ((u_char *) buckets[i] + test[i]);

        elt->value = NULL;
    }

    ngx_free(test);//释放该临时空间   

    hinit->hash->buckets = buckets;
    hinit->hash->size = size;

#if 0

    for (i = 0; i < size; i++) {
        ngx_str_t   val;
        ngx_uint_t  key;

        elt = buckets[i];

        if (elt == NULL) {
            ngx_log_error(NGX_LOG_ALERT, hinit->pool->log, 0,
                          "%ui: NULL", i);
            continue;
        }

        while (elt->value) {
            val.len = elt->len;
            val.data = &elt->name[0];

            key = hinit->key(val.data, val.len);

            ngx_log_error(NGX_LOG_ALERT, hinit->pool->log, 0,
                          "%ui: %p \"%V\" %ui", i, elt, &val, key);

            elt = (ngx_hash_elt_t *) ngx_align_ptr(&elt->name[0] + elt->len,
                                                   sizeof(void *));
        }
    }

#endif

    return NGX_OK;
}

/*
nginx为了处理带有通配符的域名的匹配问题，实现了ngx_hash_wildcard_t这样的hash表。他可以支持两种类型的带有通配符的域名。一种是通配符在前的，
例如：“*.abc.com”，也可以省略掉星号，直接写成”.abc.com”。这样的key，可以匹配www.abc.com，qqq.www.abc.com之类的。另外一种是通配符在末
尾的，例如：“mail.xxx.*”，请特别注意通配符在末尾的不像位于开始的通配符可以被省略掉。这样的通配符，可以匹配mail.xxx.com、mail.xxx.com.cn、
mail.xxx.net之类的域名。

有一点必须说明，就是一个ngx_hash_wildcard_t类型的hash表只能包含通配符在前的key或者是通配符在后的key。不能同时包含两种类型的通配符
的key。ngx_hash_wildcard_t类型变量的构建是通过函数ngx_hash_wildcard_init完成的，而查询是通过函数ngx_hash_find_wc_head或者
ngx_hash_find_wc_tail来做的。ngx_hash_find_wc_head是查询包含通配符在前的key的hash表的，而ngx_hash_find_wc_tail是查询包含通配符在后的key的hash表的。

特别要注意的是这里的key已经都是被预处理过的。例如：“*.abc.com”或者“.abc.com”被预处理完成以后，变成了“com.abc.”。而“mail.xxx.*”则被预处理为“mail.xxx.”

首先看一下ngx_hash_wildcard_init 的内存结构，当构造此类型的hash表的时候，实际上是构造了一个hash表的一个“链表”，是通过hash表中的key“链接”
起来的。比如：对于“*.abc.com”将会构造出2个hash表，第一个hash表中有一个key为com的表项，该表项的value包含有指向第二个hash表的指针，
而第二个hash表中有一个表项abc，该表项的value包含有指向*.abc.com对应的value的指针。那么查询的时候，比如查询www.abc.com的时候，先查com，
通过查com可以找到第二级的hash表，在第二级hash表中，再查找abc，依次类推，直到在某一级的hash表中查到的表项对应的value对应一个真正的值而非
一个指向下一级hash表的指针的时候，查询过程结束。
*/
ngx_int_t
ngx_hash_wildcard_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names,
    ngx_uint_t nelts) //参考:http://www.bkjia.com/ASPjc/905190.html    //使用方法可以参考ngx_http_server_names
{//参考http://www.bkjia.com/ASPjc/905190.html 图解
    size_t                len, dot_len;
    ngx_uint_t            i, 
                          n,  //n表示当前所处理的names[]数组中的第几个成员
                          dot; //当前解析到names[i]中的第i个元素字符串的.字符串位置
    ngx_array_t           curr_names, next_names;
    ngx_hash_key_t       *name, *next_name;
    ngx_hash_init_t       h;
    ngx_hash_wildcard_t  *wdc;

    //初始化临时动态数组curr_names,curr_names是存放当前关键字的数组
    if (ngx_array_init(&curr_names, hinit->temp_pool, nelts, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    //初始化临时动态数组next_names,next_names是存放关键字去掉后剩余关键字
    if (ngx_array_init(&next_names, hinit->temp_pool, nelts,
                       sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    for (n = 0; n < nelts; n = i) {

#if 0
        ngx_log_error(NGX_LOG_ALERT, hinit->pool->log, 0,
                      "wc0: \"%V\"", &names[n].key);
#endif

        dot = 0;

        for (len = 0; len < names[n].key.len; len++) {//查找 dot，len的长度为.前面的字符串长度
            if (names[n].key.data[len] == '.') {
                dot = 1;
                break;
            }
        }

        name = ngx_array_push(&curr_names);
        if (name == NULL) {
            return NGX_ERROR;
        }

        //将关键字dot以前的关键字放入curr_names
        //names[]字符串放在key中存储， 

        /* 取值aa.bb.cc中的aa字符串存储到key中，并计算aa对应的key_hash值，后面会进行递归，然后取出bb和cc字符串分别存到name数组中 */
        name->key.len = len; //len为.dot前面的字符串
        name->key.data = names[n].key.data;
        name->key_hash = hinit->key(name->key.data, name->key.len);
        name->value = names[n].value; //如果有子hash，则value会在后面指向子hash

#if 0
        ngx_log_error(NGX_LOG_ALERT, hinit->pool->log, 0,
                      "wc1: \"%V\" %ui", &name->key, dot);
#endif

        dot_len = len + 1;

        if (dot) {
            len++;//len指向dot后剩余关键字
        }

        next_names.nelts = 0;

        //如果names[n] dot后还有剩余关键字，将剩余关键字放入next_names中
        if (names[n].key.len != len) {//取出了aa.bb.cc中的aa字符串存到curr_names[]数组中，剩下的bb.cc字符串存到next_names数组中
            next_name = ngx_array_push(&next_names);
            if (next_name == NULL) {
                return NGX_ERROR;
            }

            next_name->key.len = names[n].key.len - len;
            next_name->key.data = names[n].key.data + len;
            next_name->key_hash = 0;
            next_name->value = names[n].value;

#if 0
            ngx_log_error(NGX_LOG_ALERT, hinit->pool->log, 0,
                          "wc2: \"%V\"", &next_name->key);
#endif
        }

        //如果上面搜索到的关键字没有dot，从n+1遍历names，将关键字比它长的全部放入next_name

        /*  
            例如names[0]为aa.bb,names[1]为aa.cc，并且当前处理的是names[0],则aa存到curr_names[]数组中，bb和cc存到next_name数组中，
            也就是把aa.bb和aa.cc合并了
          */
        for (i = n + 1; i < nelts; i++) {
            if (ngx_strncmp(names[n].key.data, names[i].key.data, len) != 0) {//前len个关键字相同
                break;
            }

            if (!dot
                && names[i].key.len > len
                && names[i].key.data[len] != '.')
            {
                break;
            }

            next_name = ngx_array_push(&next_names);
            if (next_name == NULL) {
                return NGX_ERROR;
            }

            next_name->key.len = names[i].key.len - dot_len;
            next_name->key.data = names[i].key.data + dot_len;
            next_name->key_hash = 0;
            next_name->value = names[i].value;

#if 0
            ngx_log_error(NGX_LOG_ALERT, hinit->pool->log, 0,
                          "wc3: \"%V\"", &next_name->key);
#endif
        }

        //例如加上name[i]为aa.bb.cc，前面取出了aa,剩下的bb.cc存到了next_names数组中，则递归该函数从而对bb和cc进行同样的操作
        if (next_names.nelts) {//如果next_name非空

            h = *hinit;
            h.hash = NULL;

            if (ngx_hash_wildcard_init(&h, (ngx_hash_key_t *) next_names.elts,
                                       next_names.nelts)//递归，创建一个新的哈西表
                != NGX_OK)
            {
                return NGX_ERROR;
            }

            wdc = (ngx_hash_wildcard_t *) h.hash;

            if (names[n].key.len == len) { //如上图，将用户value值放入新的hash表，也就是hinit中
                wdc->value = names[n].value;
            }

            /*
                由于指针都对void*（大小为4）字节对齐了，低2位肯定为0，这种操作（name->value = (void *) ((uintptr_t) wdc | (dot ? 3 : 2)) ） 
                巧妙的使用了指针的低位携带额外信息，节省了内存，让人不得不佩服ngx设计者的想象力。
                */    
            name->value = (void *) ((uintptr_t) wdc | (dot ? 3 : 2)); //并将当前value值指向新的hash表

        } else if (dot) {
            name->value = (void *) ((uintptr_t) name->value | 1); //表示后面已经没有子hash了,value指向具体的key-value中的value字符串
        }
    }
    
    //将最外层hash初始化
    if (ngx_hash_init(hinit, (ngx_hash_key_t *) curr_names.elts,
                      curr_names.nelts)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}

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
/*
ngx_hash_key函数的计算可表述为下列公式。

Key[0] = data[0]
Key[1] = data[0]*31 + data[1]
Key[2] = (data[0]*31 + data[1])*31 + data[2]
...
Key[len-1] = ((((data[0]*31 + data[1])*31 + data[2])*31) ... data[len-2])*31 + data[len-1]key[len-1]即为传入的参数data对应的hash值
*/
//对数据字符串data计算出key值
ngx_uint_t
ngx_hash_key(u_char *data, size_t len)
{
    ngx_uint_t  i, key;

    key = 0;

    for (i = 0; i < len; i++) {
        key = ngx_hash(key, data[i]);
    }

    return key;
}

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
ngx_uint_t
ngx_hash_key_lc(u_char *data, size_t len)
{
    ngx_uint_t  i, key;

    key = 0;

    for (i = 0; i < len; i++) {
        key = ngx_hash(key, ngx_tolower(data[i]));
    }

    return key;
}


ngx_uint_t
ngx_hash_strlow(u_char *dst, u_char *src, size_t n)
{
    ngx_uint_t  key;

    key = 0;

    while (n--) {
        *dst = ngx_tolower(*src);
        key = ngx_hash(key, *dst);
        dst++;
        src++;
    }

    return key;
}

/*
初始化ngx_hash_keys_arrays_t 结构体，type的取值范围只有两个，NGX_HASH_SMALL表示初始化元素较少，NGX_HASH_LARGE表示初始化元素较多，
在向ha中加入时必须调用此方法。
*/ //ngx_hash_keys_array_init一般和ngx_hash_add_key配合使用，前者表示初始化ngx_hash_keys_arrays_t数组空间，后者用来存储对应的key到数组中的对应hash和数组中
ngx_int_t
ngx_hash_keys_array_init(ngx_hash_keys_arrays_t *ha, ngx_uint_t type) //使用方法可以参考ngx_http_server_names
{
    ngx_uint_t  asize;

    if (type == NGX_HASH_SMALL) {
        asize = 4;
        ha->hsize = 107;

    } else {
        asize = NGX_HASH_LARGE_ASIZE;
        ha->hsize = NGX_HASH_LARGE_HSIZE;
    }
    
    //初始化 存放非通配符关键字的数组
    if (ngx_array_init(&ha->keys, ha->temp_pool, asize, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    //初始化存放前置通配符处理好的关键字 数组
    if (ngx_array_init(&ha->dns_wc_head, ha->temp_pool, asize, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }
    
    //初始化存放后置通配符处理好的关键字 数组
    if (ngx_array_init(&ha->dns_wc_tail, ha->temp_pool, asize, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

//下面这几个实际上是hash通的各个桶的头部指针，每个hash有ha->hsize个桶头部指针，在ngx_hash_add_key的时候头部指针指向每个桶中具体的成员列表
    
  /*
  初始化二位数组，这个数组存放的第一个维度代表的是bucket的编号，那么keys_hash[i]中存放的是所有的key算出来的hash值对hsize取
  模以后的值为i的key。假设有3个key,分别是key1,key2和key3假设hash值算出来以后对hsize取模的值都是i，那么这三个key的值就顺序存
  放在keys_hash[i][0],keys_hash[i][1], keys_hash[i][2]。该值在调用的过程中用来保存和检测是否有冲突的key值，也就是是否有重复。
  */  
    ha->keys_hash = ngx_pcalloc(ha->temp_pool, sizeof(ngx_array_t) * ha->hsize); 
    //只开辟有多少个桶对应的头，每个桶中用来存储数据的空间在后面的ngx_hash_add_key分片空间
    if (ha->keys_hash == NULL) {
        return NGX_ERROR;
    }
    
    // 该数组在调用的过程中用来保存和检测是否有冲突的前向通配符的key值，也就是是否有重复。
    ha->dns_wc_head_hash = ngx_pcalloc(ha->temp_pool,
                                       sizeof(ngx_array_t) * ha->hsize);
    if (ha->dns_wc_head_hash == NULL) {
        return NGX_ERROR;
    }
    
   // 该数组在调用的过程中用来保存和检测是否有冲突的后向通配符的key值，也就是是否有重复。 
    ha->dns_wc_tail_hash = ngx_pcalloc(ha->temp_pool,
                                       sizeof(ngx_array_t) * ha->hsize);
    if (ha->dns_wc_tail_hash == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

/*
把key value添加到ha对应的array变量数组中
*/ //ngx_hash_add_key是将带或不带通配符的key转换后存放在ngx_hash_keys_arrays_t对应的
/*
哈希通配符的查找: 
Nginx哈希支持三种类型的通配: 
"*.example.com", ".example.com", and "www.example.*"

对这些字符串进行哈希前进行了预处理(ngx_hash_add_key): 
"*.example.com", 经过预处理后变成了: "com.example.\0"
".example.com"  经过预处理后变成了: "com.example\0"
"www.example.*" 经过预处理后变成了:  "www.example\0"

通配符hash表的实现原理 ： 当构造此类型的hash表的时候，实际上是构造了一个hash表的一个“链表”，是通过hash表中的key“链接”起来的。
比如：对于“*.example.com”将会构造出2个hash表，第一个hash表中有一个key为com的表项，该表项的value包含有指向第二个hash表的指针，
而第二个hash表中有一个表项abc，该表项的value包含有指*.example.com对应的value的指针。那么查询的时候，比如查询www.example.com的时候，
先查com，通过查com可以找到第二级的hash表，在第二级hash表中，再查找example，依次类推，直到在某一级的hash表中查到的表项对应的value对
应一个真正的值而非一个指向下一级hash表的指针的时候，查询过程结束。而查找到哪里是由value地址的最低两bit表示: (这也是在申请内存时要
求4字节对齐的原因, 最后两bit是0, 可以被修改来表示下述情况)

头部通配情况:
        / *
         * the 2 low bits of value have the special meaning:
         *     00 - value is data pointer for both "example.com"
         *          and "*.example.com";
         *     01 - value is data pointer for "*.example.com" only;
         *     10 - value is pointer to wildcard hash allowing
         *          both "example.com" and "*.example.com";
         *     11 - value is pointer to wildcard hash allowing
         *          "*.example.com" only.
         * /
尾部通配情况:

        / *
         * the 2 low bits of value have the special meaning:
         *     00 - value is data pointer;
         *     11 - value is pointer to wildcard hash allowing "example.*".
         * /

*/
/*
ngx_hash_add_key是将带或不带通配符的key转换后存放在上述结构中的，其过程是:
    先看传入的第三个参数标志标明的key是不是NGX_HASH_WILDCARD_KEY， 
    如果不是，则在ha->keys_hash中检查是否冲突，冲突就返回NGX_BUSY，否则，就将这一项插入到ha->keys中。
    如果是，就判断通配符类型，支持的统配符有三种”*.example.com”, “.example.com”, and “www.example.*“，
    然后将第一种转换为"com.example.“并插入到ha->dns_wc_head中，将第三种转换为"www.example"并插入到ha->dns_wc_tail中，
    对第二种比较特殊，因为它等价于”*.example.com”+“example.com”,所以会一份转换为"com.example.“插入到ha->dns_wc_head，
    一份为"example.com"插入到ha->keys中。当然插入前都会检查是否冲突。
*/ //ngx_hash_keys_array_init一般和ngx_hash_add_key配合使用，前者表示初始化ngx_hash_keys_arrays_t数组空间，后者用来存储对应的key到数组中的对应hash和数组中

/*

    赋值见ngx_hash_add_key
    
    原始key                  存放到hash桶(keys_hash或dns_wc_head_hash                 存放到数组中(keys或dns_wc_head或
                                    或dns_wc_tail_hash)                                     dns_wc_tail)
                                    
 www.example.com                 www.example.com(存入keys_hash)                        www.example.com (存入keys数组成员ngx_hash_key_t对应的key中)
  .example.com             example.com(存到keys_hash，同时存入dns_wc_tail_hash)        com.example  (存入dns_wc_head数组成员ngx_hash_key_t对应的key中)
 www.example.*                     www.example. (存入dns_wc_tail_hash)                 www.example  (存入dns_wc_tail数组成员ngx_hash_key_t对应的key中)
 *.example.com                     example.com  (存入dns_wc_head_hash)                 com.example. (存入dns_wc_head数组成员ngx_hash_key_t对应的key中)
*/
ngx_int_t
ngx_hash_add_key(ngx_hash_keys_arrays_t *ha, ngx_str_t *key, void *value,
    ngx_uint_t flags)//使用方法可以参考ngx_http_server_names
{
    size_t           len;
    u_char          *p;
    ngx_str_t       *name;
    ngx_uint_t       i, k, n, 
        skip, // 1 -- ".example.com"  2 -- "*.example.com"  0 -- "www.example.*"
        last;
    ngx_array_t     *keys, *hwc;
    ngx_hash_key_t  *hk;

    last = key->len;

    if (flags & NGX_HASH_WILDCARD_KEY) {

        /*
         * supported wildcards:
         *     "*.example.com", ".example.com", and "www.example.*"
         */

        n = 0;

        for (i = 0; i < key->len; i++) {

            if (key->data[i] == '*') {
                if (++n > 1) { //通配符*只能出现一次，出现多次说明错误返回
                    return NGX_DECLINED;
                }
            }

            //不能出现两个连续的..，便是出错，直接返回
            if (key->data[i] == '.' && key->data[i + 1] == '.') {
                return NGX_DECLINED;
            }
        }
        
        if (key->len > 1 && key->data[0] == '.') {//首字符是.，".example.com"说明是前向通配符
            skip = 1;
            goto wildcard;
        }

        if (key->len > 2) {

            if (key->data[0] == '*' && key->data[1] == '.') {//"*.example.com"
                skip = 2;
                goto wildcard;
            }

            if (key->data[i - 2] == '.' && key->data[i - 1] == '*') {//"www.example.*"
                skip = 0;
                last -= 2;
                goto wildcard;
            }
        }

        if (n) {
            return NGX_DECLINED;
        }
    }

    /* exact hash */
    /* 说明是精确匹配server_name类型，例如"www.example.com" */
    k = 0;

    //把字符串key为源来计算hash，一个字符一个字符的算
    for (i = 0; i < last; i++) {
        if (!(flags & NGX_HASH_READONLY_KEY)) {
            key->data[i] = ngx_tolower(key->data[i]);
        }
        k = ngx_hash(k, key->data[i]);
    }

    k %= ha->hsize;

    /* check conflicts in exact hash */

    name = ha->keys_hash[k].elts;

    if (name) {
        for (i = 0; i < ha->keys_hash[k].nelts; i++) {
            if (last != name[i].len) {
                continue;
            }

            if (ngx_strncmp(key->data, name[i].data, last) == 0) { //已经存在一个相同的
                return NGX_BUSY;
            }
        }

    } else {
        //每个桶中的元素个数默认
        if (ngx_array_init(&ha->keys_hash[k], ha->temp_pool, 4,
                           sizeof(ngx_str_t)) //桶的头部指针在ngx_hash_keys_array_init分配，桶中存储数据的空间在这里分配
            != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    /*
    key存到ha->keys_hash[]对应的hash桶中, key-value存到ha->keys[]数组中的对应key成员和value成员中

     */

    //存放key到ha->keys_hash[]桶中
    name = ngx_array_push(&ha->keys_hash[k]);
    if (name == NULL) {
        return NGX_ERROR;
    }

    *name = *key;

    hk = ngx_array_push(&ha->keys);
    if (hk == NULL) {
        return NGX_ERROR;
    }
    //ha->keys中存放的是key  value对
    hk->key = *key;
    hk->key_hash = ngx_hash_key(key->data, last);
    hk->value = value;

    return NGX_OK;

wildcard:

    /* wildcard hash */
    //以参数中的key字符串计算hash key
    k = ngx_hash_strlow(&key->data[skip], &key->data[skip], last - skip);

    k %= ha->hsize;

    if (skip == 1) { //".example.com"，".example.com"除了添加到hash桶keys_hash[]外，还会添加到dns_wc_tail_hash[]桶中，

        /* check conflicts in exact hash for ".example.com" */

        name = ha->keys_hash[k].elts;

        if (name) {
            len = last - skip; //出去开头的.
            //字符串长度和内容完全匹配
            for (i = 0; i < ha->keys_hash[k].nelts; i++) {
                if (len != name[i].len) {
                    continue;
                }

                if (ngx_strncmp(&key->data[1], name[i].data, len) == 0) {
                    return NGX_BUSY;
                }
            }

        } else {
            if (ngx_array_init(&ha->keys_hash[k], ha->temp_pool, 4,
                               sizeof(ngx_str_t)) //每个槽中的元素个数默认4字节
            //开辟每个槽中的空间，用来存放对应的节点到该槽中，槽的头节点在ngx_hash_keys_array_init中已经分配好
                != NGX_OK)
            {
                return NGX_ERROR;
            }
        }

        name = ngx_array_push(&ha->keys_hash[k]);
        if (name == NULL) {
            return NGX_ERROR;
        }

        name->len = last - 1; //把".example.com"字符串开头的.去掉
        name->data = ngx_pnalloc(ha->temp_pool, name->len);
        if (name->data == NULL) {
            return NGX_ERROR;
        }

        //".example.com"去掉开头的.后变为"example.com"存储到name中，但是key还是原来的".example.com"
        ngx_memcpy(name->data, &key->data[1], name->len);//".example.com"去掉开头的.后变为"example.com"存储到ha->keys_hash[i]桶中
    }


    if (skip) { //前置匹配的通配符"*.example.com"  ".example.com"
        /*
         * convert "*.example.com" to "com.example.\0"
         *      and ".example.com" to "com.example\0"
         */
        p = ngx_pnalloc(ha->temp_pool, last);
        if (p == NULL) {
            return NGX_ERROR;
        }

        len = 0;
        n = 0;

        for (i = last - 1; i; i--) {
            if (key->data[i] == '.') {
                ngx_memcpy(&p[n], &key->data[i + 1], len);
                n += len;
                p[n++] = '.';
                len = 0;
                continue;
            }

            len++;
        }

        if (len) {
            ngx_memcpy(&p[n], &key->data[1], len);
            n += len;
        }

        /* key中数据"*.example.com"，p中数据"com.example.\0"   key中数据".example.com" p中数据"com.example\0" */
        p[n] = '\0'; 

        hwc = &ha->dns_wc_head;
        keys = &ha->dns_wc_head_hash[k];
    
    }  else {//后置匹配
        last++; //+1是用来存储\0字符

        p = ngx_pnalloc(ha->temp_pool, last);
        if (p == NULL) {
            return NGX_ERROR;
        }

        //key中数据为"www.example.*"， p中数据为"www.example\0"
        ngx_cpystrn(p, key->data, last); 

        hwc = &ha->dns_wc_tail;
        keys = &ha->dns_wc_tail_hash[k];
    }

    /* check conflicts in wildcard hash */  
    name = keys->elts;

    if (name) {
        len = last - skip;
        //查看是否已经有存在的了
        for (i = 0; i < keys->nelts; i++) {
            if (len != name[i].len) {
                continue;
            }

            if (ngx_strncmp(key->data + skip, name[i].data, len) == 0) {
                return NGX_BUSY;
            }
        }

    } else {//说明是第一次出现前置通配符或者后置通配符
        //初始化桶ha->dns_wc_head_hash[i]或者桶ha->dns_wc_tail_hash[i]中的元素个数
        if (ngx_array_init(keys, ha->temp_pool, 4, sizeof(ngx_str_t)) != NGX_OK)
        {
            return NGX_ERROR;
        }
    }

    name = ngx_array_push(keys);
    if (name == NULL) {
        return NGX_ERROR;
    }

    name->len = last - skip;
    name->data = ngx_pnalloc(ha->temp_pool, name->len);
    if (name->data == NULL) {
        return NGX_ERROR;
    }
    
    /* 前置匹配key字符串存放到&ha->dns_wc_head; 后置匹配key字符串存放到&ha->dns_wc_tail hash表中 */
    ngx_memcpy(name->data, key->data + skip, name->len);

    /*
    配置valid_referers none blocked server_names .example.com  www.example.*
    或者配置valid_referers none blocked server_names *.example.com  www.example.*
    name:example.com, kye:*.example.com
    name:www.example., kye:www.example.*
    name:example.com, kye:.example.com

    *.example.com  --- example.com
    www.example.*  --- www.example.
    .example.com   --- example.com
    ngx_log_debugall(ngx_cycle->log, 0, "name:%V, kye:%V", name, key);
     */
    /* add to wildcard hash */

    hk = ngx_array_push(hwc);
    if (hk == NULL) {
        return NGX_ERROR;
    }

    hk->key.len = last - 1;
    //到这里,p中的数据就有源key"*.example.com", ".example.com", and "www.example.*"变为了"com.example.\0" "com.example\0"  "www.example\0"
    hk->key.data = p;
    hk->key_hash = 0;
    hk->value = value; //以ngx_http_add_referer为例，假设key为，*.example.com/test/xxx,则value为字符串/test/xxx，否则为NGX_HTTP_REFERER_NO_URI_PART

/*
    配置valid_referers none blocked server_names .example.com  www.example.*
    或者配置valid_referers none blocked server_names *.example.com  www.example.*

    [yangya  [debug] 25843#25843: name:example.com, kye:.example.com, p:com.example
    [yangya  [debug] 25843#25843: name:www.example., kye:www.example.*, p:www.example
    [yangya  [debug] 25844#25844: name:example.com, kye:*.example.com, p:com.example.


    ngx_log_debugall(ngx_cycle->log, 0, "name:%V, kye:%V, p:%V", name, key, &hk->key);
*/
    return NGX_OK;
}

