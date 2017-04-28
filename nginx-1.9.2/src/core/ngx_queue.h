
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


#ifndef _NGX_QUEUE_H_INCLUDED_
#define _NGX_QUEUE_H_INCLUDED_

/*
表7-1  ngx_queue_t双向链表容器所支持的方法
┏━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━┓
┃    方法名                  ┃    参数含义                    ┃    执行意义                            ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  h为链表容器结构体ngx_queue_t  ┃  将链表容器h初始化，这时会自动置为空   ┃
┃ngx_queue_init(h)           ┃                                ┃                                        ┃
┃                            ┃的指针                          ┃链表                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  h为链表容嚣结构体ngx_queue_t  ┃  检测链表容器中是否为空，即是否没有一  ┃
┃ngx_queue_empty(h)          ┃                                ┃个元素存在。如果返回非O，表示链表h是    ┃
┃                            ┃的指针                          ┃                                        ┃
┃                            ┃                                ┃空的                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  h为链表容器结构体ngx_queue_   ┃                                        ┃
┃ngx_queue_insert_head(h, x) ┃t的指针，x为插入元素结构体中    ┃  将元素x插入到链表容器h的头部          ┃
┃                            ┃ngx_queue_t成员的指针           ┃                                        ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  h为链表容器结构体ngx_queue_   ┃                                        ┃
┃ngx_queue_insert_tail(h, x) ┃t的指针，x为插入元素结构体中    ┃  将元素x添加到链表容器h的末尾          ┃
┃                            ┃ngx_queue_t成员的指针           ┃                                        ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  h为链表容器结构体ngx_queue_t  ┃  返回链表容器h中的第一个元素的ngx_     ┃
┃ngx_queue_head(h)           ┃                                ┃                                        ┃
┃                            ┃的指针                          ┃queue_t结构体指针                       ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  h为链表容器结构体ngx_queue_t  ┃  返回链表容器中的最后一个元素的ngx_    ┃
┃ngx_queue_last(h)           ┃                                ┃                                        ┃
┃                            ┃的指针                          ┃ queue_t结构体指针                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  h为链表容器结构体ngx_queue_t  ┃                                        ┃
┃ngx_queue_sentinel(h)       ┃                                ┃  返回链表容器结构体的指针              ┃
┃                            ┃的指针                          ┃                                        ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  x为插入元素结构体中ngx_       ┃                                        ┃
┃ngx_queue_remove(x)         ┃                                ┃  由容器中移除x元素                     ┃
┃                            ┃queue_t成员的指针               ┃                                        ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃                                ┃  ngx_queue_split用于拆分链表，h是链表  ┃
┃                            ┃                                ┃容器，而q是链表h中的一个元素。这个方    ┃
┃                            ┃  h力链表容器结构体ngx_queue_t  ┃法将链表h以元素q为界拆分成两个链表h     ┃
┃ngx_queue_split(h, q, n)    ┃                                ┃                                        ┃
┃                            ┃的指针                          ┃和n，其中h由原链表的前半部分构成（不    ┃
┃                            ┃                                ┃包括q），而n由原链表的后半部分构成，q   ┃
┃                            ┃                                ┃是它的首元素                            ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  h为链表容器结构体ngx_queue_t  ┃                                        ┃
┃ngx_queue_add(h, n)         ┃的指针，n为另一个链表容器结构体 ┃  合并链表，将n链表添加到h链表的末尾    ┃
┃                            ┃ngx_queue_t的指针               ┃                                        ┃
┗━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━┛


（续）
┏━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━┓
┃    方法名                ┃    参数含义                    ┃    执行意义                              ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃                                ┃  返回链表中心元素，如，链表共有N个元     ┃
┃                          ┃  h为链表容器结构体ngx_queue_t  ┃素，ngx_queue_middle方法将返回第N/2+1     ┃
┃ngx_queue_middle(h)       ┃                                ┃                                          ┃
┃                          ┃的指针                          ┃个元素。例如，链表有4个元素，将会返回     ┃
┃                          ┃                                ┃第3个元素（不是第2个元素）                ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  h为链表容器结构体ngx_queue_t  ┃  使用插入排序法对链表进行排序，cmpfunc   ┃
┃                          ┃的指针，cmpfunc是两个链表元素的 ┃需要使用者自己实现，它的原型是这样的：    ┃
┃ngx_queue_sort(h,cmpfunc) ┃                                ┃                                          ┃
┃                          ┃比较方法，如果它返回正数，则表  ┃ngx_int_t(*cmpfunc)(const ngx_queue_t幸， ┃
┃                          ┃示以升序排序                    ┃const ngx_queue_t+)                       ┃
┗━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━┛
    对于链表中的每一个元素，其类型可以是任意的struct结构体，但这个结构体中必须
要有一个ngx_queue_t类型的成员，在向链表容器中添加、删除元素时都是使用的结构体中
ngx_queue_t类型成员的指针。当ngx_queue_t作为链表的元素成员使用时，它具有表7-2中
列出的4种方法。

ngx_queue_t驭向链表中的元素所支持的方法
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━┓
┃    方法名                    ┃    参数含义                            ┃    执行意义                    ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━┫
┃                              ┃  q为链表中某一个元素结构体的ngx_       ┃                                ┃
┃ngx_queue_next(q)             ┃                                        ┃  返回q元素的下一个元素         ┃
┃                              ┃queue_t成员的指针                       ┃                                ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━┫
┃                              ┃  q为链表中某一个元素结构体的ngx_       ┃                                ┃
┃ngx_queue_prev(q)             ┃                                        ┃  返回q元素的上一个元素         ┃
┃                              ┃queue_t成员的指针                       ┃                                ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━┫
┃                              ┃  q为链表中某一个元素结构体的ngx_       ┃                                ┃
┃                              ┃queue_t成员的指针，type为链表元素的结   ┃  返回q元素（ngx_queue_t类型）  ┃
┃ngx_queue_data(q, type, link) ┃构体类型名称（该结构体中必须包含ngx_    ┃所属结构体（任何struct类型，其  ┃
┃                              ┃queue_t类型的成员），link是上面这个结构 ┃中可在任意位置包含ngx_queue_t   ┃
┃                              ┃体中ngx_queue_t类型的成员名字           ┃类型的成员）的地址              ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━┫
┃                              ┃  q为链表中某个元素结构体的ngx_queue_   ┃                                ┃
┃ngx_queue_insert_after(q, x)  ┃t成员的指针，x为插入元素结构体中ngx_    ┃  将元素x插入到元素q之后        ┃
┃                              ┃queue_t成员的指针                       ┃                                ┃
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━┛

*/
typedef struct ngx_queue_s  ngx_queue_t;
/* ngx_list_t和ngx_queue_t的却别在于:ngx_list_t需要负责容器内成员节点内存分配，而ngx_queue_t不需要 */
struct ngx_queue_s {
    ngx_queue_t  *prev;
    ngx_queue_t  *next;
};

#define ngx_queue_init(q)                                                     \
    (q)->prev = q;                                                            \
    (q)->next = q

#define ngx_queue_empty(h)                                                    \
    (h == (h)->prev)

#define ngx_queue_insert_head(h, x)                                           \
    (x)->next = (h)->next;                                                    \
    (x)->next->prev = x;                                                      \
    (x)->prev = h;                                                            \
    (h)->next = x

#define ngx_queue_insert_after   ngx_queue_insert_head

/* x插入h节点前面 */
#define ngx_queue_insert_tail(h, x)                                           \
    (x)->prev = (h)->prev;                                                    \
    (x)->prev->next = x;                                                      \
    (x)->next = h;                                                            \
    (h)->prev = x


#define ngx_queue_head(h)                                                     \
    (h)->next


#define ngx_queue_last(h)                                                     \
    (h)->prev


#define ngx_queue_sentinel(h)                                                 \
    (h)


#define ngx_queue_next(q)                                                     \
    (q)->next


#define ngx_queue_prev(q)                                                     \
    (q)->prev


#if (NGX_DEBUG)

#define ngx_queue_remove(x)                                                   \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next;                                              \
    (x)->prev = NULL;                                                         \
    (x)->next = NULL

#else

#define ngx_queue_remove(x)                                                   \
    (x)->next->prev = (x)->prev;                                              \
    (x)->prev->next = (x)->next

#endif

/*
h为链表容器，q为链表h中的一个元素，这个方法可以将链表h以元素q为界拆分为两个链表h和n，其中h由原链表的前半部分组成（不包含q），
而n由后半部分组成，q为首元素,如果以前n有成员，则新的n为从h中拆分的部分+n原有的数据
*/
#define ngx_queue_split(h, q, n)                                              \
    (n)->prev = (h)->prev;                                                    \
    (n)->prev->next = n;                                                      \
    (n)->next = q;                                                            \
    (h)->prev = (q)->prev;                                                    \
    (h)->prev->next = h;                                                      \
    (q)->prev = n;


//链表合并ngx_queue_add
#define ngx_queue_add(h, n)                                                   \
    (h)->prev->next = (n)->next;                                              \
    (n)->next->prev = (h)->prev;                                              \
    (h)->prev = (n)->prev;                                                    \
    (h)->prev->next = h;


/*
typedef struct {
                      u_char* str;
                      ngx_queue_t qEle;
                      int num;
} TestNode;

链表元素结构体中必须包含ngx_queue_t类型的成员，当然它可以在任意的位置上。本例中它的上面有一个char~指针，下面有一个整型成员num，这样是允许的。
排序方法需要自定义。下面以TestNode结构体中的num成员作为排序依据，实现compTestNode方法作为排序过程中任意两元素间的比较方法。
ngx_int_t compTeBtNode(const ngx_queue_t*a,  const ngx_queue_t*b)
{
    //首先使用ngx_queue_data方法由ngx queue—t变量获取元素结构体TestNode的地址
    TestNodet *aNode=ngx_queue_data(a,  TestNode,  qEle);
    TestNode *bNode=ngx_queue_data (b,  TestNode,  qEle);
    ／／返回num成员的比较结果
    return  aNode- >num>bNode- >num;
}

*/
#define ngx_queue_data(q, type, link)                                         \
    (type *) ((u_char *) q - offsetof(type, link))


ngx_queue_t *ngx_queue_middle(ngx_queue_t *queue);
void ngx_queue_sort(ngx_queue_t *queue,
    ngx_int_t (*cmp)(const ngx_queue_t *, const ngx_queue_t *));


#endif /* _NGX_QUEUE_H_INCLUDED_ */
