
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */
/*
便用双向链表排序的例子
    本节定义一个简单的链表，并使用ngx_queue_sort方法对所有元素排序。在这个例子
中，可以看到如何定义、初始化ngx_queue_t容器，如何定义任意类型的链表元素，如何遍
历链表，如何自定义排序方法并执行排序。
    首先，定义链表元素的结构体，如下面的TestNode结构体：
typedef struct {
                      u_char* str;
                      ngx_queue_t qEle;
                    I           int num;
} TestNode;

220◇第二部分如何编写HTTP模块
    链表元素结构体中必须包含ngx_queue_t类型的成员，当然它可以在任意的位置上。本
例中它的上面有一个char~指针，下面有一个整型成员num，这样是允许的。
    排序方法需要自定义。下面以TestNode结构体中的num成员作为排序依据，实现
compTestNode方法作为排序过程中任意两元素间的比较方法。
    ngx_int_t compTeBtNode(const ngx_queue_t*a,  const ngx_queue_t*b)
    {
        //首先使用ngx_queue_data方法由ngx queue—t变量获取元素结构体TestNode的地址奇／
        TestNodet aNode=ngx_queue_data(a,  TestNode,  qEle)j
        TestNode★bNode=ngx_queue_data (b,  TestNode,  qEle)j
        
       //返回num成员的比较结果
        return  aNode- >num>bNode- >num;
    )
    这个比较方法结合ngx_queue_sort方法可以把链表中的元素按照num的大小以升序排
列。在此例中，可以看到ngx_queue_data的用法，即呵以根据链表元素结构体TestNode中
的qEle成员地址换算出TestNode结构体变量的地址，这是面向过程的C语言编写的ngx_
queue_t链表之所以能够通用化的关键。下面来看一下ngx_queue_data的定义：
#define  ngx_queue_data (q, type, link)   \
(type  *) 《u char  *)  q  -  offsetof (type,  link》
    在4.2.2节中曾经提到过offsetof函数是如何实现的，即它会返回link成员在type结
构体中的偏移量。例如，在上例中，可以通过ngx_queue_t类型的指针减去qEle相对于
TestNode的地址偏移量，得到TestNode结构体的地址。
    下面开始定义双向链表容器queueContainer，并将其初始化为空链表，如下所示。
ngx_queue_t queueContainer;
ngx_queue_init ( &queueContainer )  ;
    链表容器以ngx_queue_t定义即可。注意，对于表示链表容器的ngx_queue_t结构体，
必须调用ngx_queue_init进行初始化。
    ngx_queue_t双向链表是完全不负责俞配内存的，每一个链表元素必须自己管理自己所
占用的内存。因此，本例在进程栈中定义了5个TestNode结构体作为链表元素，并把它们
的num成员初始化为0、1、2、3、4，如下所示。
下面把这5个TestNode结构体添加到queueContainer链表中，注意，这里同时使用了
ngx_queue—insert tail、ngx_queue—insert head、ngx_queue—insert after 3个添加方法，读者
不妨思考一下链表中元素的顺序是什么样的。
ngx_queue_insert_tail ( &queueContainer ,    &node [ O ]  . qEle )  ;
ngx_queue_insert_head ( &queueContainer ,    &node [l]  . qEle )  ;
ngx_queue_insert_tail ( &queueContainer ,    &node [2 ]  . qEle)  ;
ngx_queue_insert_af ter ( &queueContainer ,   &node [ 3 ]  . qEle)  ;
ngx_queue_insert_tail ( &queueContainer,   &node [4 ]  . qEle) ;
    根据表7-1中介绍的方法可以得出，如果此时的链表元素顺序以num成员标识，那么应
该是这样的：3、1、0、2、4。如果有疑问，不妨写个遍历链表的程序检验一下顺序是否如此。
下面就根据表7-1中的方法说明编写一段简单的遍历链表的程序。
ngx_queue_t*qf
for  (q=ngx_queue_head(&queueContainer);
    q  !=  ngx_queue_sentinel( &queueContainer);
    q= ngx_queue_next (q))
{
    TeetNodek eleNode;ngx_queue_data(q, TestNode, qEle),
    ／／处理当前的链表元素eleNode
)
    上面这段程序将会依次从链表头部遍历到尾部。反向遍历也很简单。读者可以尝试使用
ngx_queue_last和ngx_queue_prev方法编写相关代码。
    下面开始执行排序，代码如下所示。
    ngx_queue_sort4( &queueContainer,    compTestNode);
    这样，链表中的元素就会以0、1、2、3、4（num成员的值）的升序排列了。

*/

#include <ngx_config.h>
#include <ngx_core.h>


/*
 * find the middle queue element if the queue has odd number of elements
 * or the first element of the queue's second part otherwise
 */
//链表中心元素ngx_queue_middle
/*
这里用到的技巧是每次middle向后移动一步，next向后移动两步，这样next指到队尾的时候，middle就指到了中间，时间复杂度就是O(N)
*/
ngx_queue_t *
ngx_queue_middle(ngx_queue_t *queue)
{
    ngx_queue_t  *middle, *next;

    middle = ngx_queue_head(queue);

    if (middle == ngx_queue_last(queue)) {
        return middle;
    }

    next = ngx_queue_head(queue);

    for ( ;; ) {
        middle = ngx_queue_next(middle);

        next = ngx_queue_next(next);

        if (next == ngx_queue_last(queue)) {
            return middle;
        }

        next = ngx_queue_next(next);

        if (next == ngx_queue_last(queue)) {
            return middle;
        }
    }
}

/*
双向队列的各个操作都很简单，函数名即操作意图：
1. ngx_queue_init(q)初始化哨兵结点，令prev字段和next字段均指向其自身；
2. ngx_queue_empty(q)检查哨兵结点的prev字段是否指向其自身，以判断队列是否为空；
3. ngx_queue_insert_head(h, x)在哨兵结点和第一个结点之间插入新结点x；
4. ngx_queue_insert_after(h, x)是ngx_queue_insert_head的别名；
5. ngx_queue_insert_tail(h, x)在最后一个结点和哨兵结点之间插入新结点；
6. ngx_queue_head(h)获取第一个结点；
7. ngx_queue_last(h)获取最后一个结点；
8. ngx_queue_sentinel(h)获取哨兵结点（即参数h）；
9. ngx_queue_next(q)获取下一个结点；
10. ngx_queue_prev(q)获取上一个结点；
11. ngx_queue_remove(x)将结点x从队列中移除；
12. ngx_queue_split(h, q, n)将h为哨兵结点的队列中q结点开始到队尾结点的整个链拆分、链接到空的n队列中，h队列中的剩余结点组成新队列；
13. ngx_queue_add(h, n)将n队列中的所有结点按顺序链接到h队列末尾，n队列清空；
14. ngx_queue_middle(queue)使用双倍步进算法寻找queue队列的中间结点；
15. ngx_queue_sort(queue, cmd)使用插入排序算法对queue队列进行排序，完成后在next方向上为升序，prev方向为降序。 
*/

/* the stable insertion sort 
ngx_queue_sort(queue, cmd)使用插入排序算法对queue队列进行排序，完成后在next方向上为升序，prev方向为降序。 
*/
//对locationqueue进行排序，即ngx_queue_sort，排序的规则可以读一下ngx_http_cmp_locations函数。
//该函数功能就是按照cmp比较，然后对珍格格queue进行从新排序，ngx_queue使用例子可以参考http://blog.chinaunix.net/uid-26284395-id-3134435.html
//可以看到，这里采用的是插入排序算法，时间复杂度为O(n)，整个代码非常简洁。
void
ngx_queue_sort(ngx_queue_t *queue,
    ngx_int_t (*cmp)(const ngx_queue_t *, const ngx_queue_t *))
{
    ngx_queue_t  *q, *prev, *next;

    q = ngx_queue_head(queue);

    if (q == ngx_queue_last(queue)) {
        return;
    }

    for (q = ngx_queue_next(q); q != ngx_queue_sentinel(queue); q = next) {

        prev = ngx_queue_prev(q);
        next = ngx_queue_next(q);

        ngx_queue_remove(q); //这三行是依次序从前到后找出队列中需要排序的元素

        do {
            if (cmp(prev, q) <= 0) {//从大到下排序
                break;
            }

            prev = ngx_queue_prev(prev);

        } while (prev != ngx_queue_sentinel(queue)); //查找这个元素需要插入到前面依据拍好序的队列的那个地方

        ngx_queue_insert_after(prev, q);
    }
}

