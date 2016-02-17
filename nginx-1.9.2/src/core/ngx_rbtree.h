
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_RBTREE_H_INCLUDED_
#define _NGX_RBTREE_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>

/*
表7-4 Nginx为红黑树已经实现好的3种数据添加方法 (ngx_rbtree_insert_pt指向以下三种方法)
┏━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┓
┃    方法名                          ┃    参数含义                          ┃    执行意义                  ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━┫
┃void ngx_rbtree_insert_value        ┃  root是红黑树容器的指针；node是      ┃  向红黑树添加数据节点，每个  ┃
┃(ngx_rbtree_node_t *root,           ┃待添加元素的ngx_rbtree_node_t成员     ┃数据节点的关键字都是唯一的，  ┃
┃ngx_rbtree_node_t *node,            ┃的指针；sentinel是这棵红黑树初始化    ┃不存在同一个关键字有多个节点  ┃
┃ngx_rbtree_node_t *sentinel)        ┃时哨兵节点的指针                      ┃的问题                        ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━┫
┃void ngx_rbtree_insert_timer_value  ┃  root是红黑树容器的指针；node是      ┃                              ┃
┃(ngx_rbtree_node_t *root,           ┃待添加元素的ngx_rbtree_node_t成员     ┃  向红黑树添加数据节点，每个  ┃
┃ngx_rbtree_node_t *node,            ┃的指针，它对应的关键字是时间或者      ┃数据节点的关键字表示时间戎者  ┃
┃                                    ┃时间差，可能是负数；sentinel是这棵    ┃时间差                        ┃
┃ngx_rbtree_node_t *sentinel)        ┃                                      ┃                              ┃
┃                                    ┃红黑树初始化时的哨兵节点              ┃                              ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━┫
┃void ngx_str_rbtree_insert_value    ┃  root是红黑树容器的指针；node是      ┃  向红黑树添加数据节点，每个  ┃
┃(ngx_rbtree_node_t *temp,           ┃待添加元素的ngx_str_node_t成员的      ┃数据节点的关键字可以不是唯一  ┃
┃ngx_rbtree_node_t *node,            ┃指针（ngx- rbtree_node_t类型会强制转  ┃的，但它们是以字符串作为唯一  ┃
┃                                    ┃化为ngx_str_node_t类型）；sentinel是  ┃的标识，存放在ngx_str_node_t  ┃
┃ngx_rbtree_node t *sentinel)        ┃                                      ┃                              ┃
┃                                    ┃这棵红黑树初始化时哨兵节点的指针      ┃结构体的str成员中             ┃
┗━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━┛
    同时，对于ngx_str_node_t节点，Nginx还提供了ngx_str_rbtree_lookup方法用于检索
红黑树节点，下面来看一下它的定义，代码如下。
    ngx_str_node_t  *ngx_str_rbtree_lookup(ngx_rbtree t  *rbtree,  ngx_str_t *name, uint32_t hash)，
    其中，hash参数是要查询节点的key关键字，而name是要查询的字符串（解决不同宇
符串对应相同key关键字的问题），返回的是查询到的红黑树节点结构体。
    关于红黑树操作的方法见表7-5。
表7-5  红黑树容器提供的方法
┏━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━┓
┃    方法名                          ┃    参数含义                    ┃    执行意义                        ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  tree是红黑树容器的指针；s是   ┃  初始化红黑树，包括初始化根节      ┃
┃                                    ┃哨兵节点的指针；i是ngx_rbtree_  ┃                                    ┃
┃ngx_rbtree_init(tree, s, i)         ┃                                ┃点、哨兵节点、ngx_rbtree_insert_pt  ┃
┃                                    ┃insert_pt类型的节点添加方法，具 ┃节点添加方法                        ┃
┃                                    ┃体见表7-4                       ┃                                    ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃void ngx_rbtree_insert(ngx_rbtree_t ┃  tree是红黑树容器的指针；node  ┃  向红黑树中添加节点，该方法会      ┃
┃*tree, ngx_rbtree node_t *node)     ┃是需要添加到红黑树的节点指针    ┃通过旋转红黑树保持树的平衡          ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━┫
┃void ngx_rbtree_delete(ngx_rbtree_t ┃  tree是红黑树容器的指针；node  ┃  从红黑树中删除节点，该方法会      ┃
┃*tree, ngx_rbtree node_t *node)     ┃是红黑树中需要删除的节点指针    ┃通过旋转红黑树保持树的平衡          ┃
┗━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━┛
    在初始化红黑树时，需要先分配好保存红黑树的ngx_rbtree_t结构体，以及ngx_rbtree_
node_t粪型的哨兵节点，并选择或者自定义ngx_rbtree_insert_pt类型的节点添加函数。
    对于红黑树的每个节点来说，它们都具备表7-6所列的7个方法，如果只是想了解如何
使用红黑树，那么只需要了解ngx_rbtree_min方法。


┏━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━┓
┃    方法名                          ┃    参数含义                      ┃    执行意义                          ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  node是红黑树中ngx_rbtree_node_  ┃                                      ┃
┃ngx_rbt_red(node)                   ┃                                  ┃  设置node节点的颜色为红色            ┃
┃                                    ┃ t类型的节点指针                  ┃                                      ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  node是红黑树中ngx_rbtree_node_  ┃                                      ┃
┃ngx_rbt_black(node)                 ┃                                  ┃  设置node节点的颜色为黑色            ┃
┃                                    ┃t类型的节点指针                   ┃                                      ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  node是红黑树中ngx_rbtree_node_  ┃  若node节点的颜色为红色，则返回非O   ┃
┃ngx_rbt_is_red(node)                ┃                                  ┃                                      ┃
┃                                    ┃t类型的节点指针                   ┃数值，否则返回O                       ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃ngx_rbt_is_black(node)              ┃  node是红黑树中ngx_rbtree_node_  ┃  若node节点的颜色为黑色，则返回非0   ┃
┃                        I           ┃t类型的节点指针                   ┃数值，否则返回O                       ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃               I                    ┃  nl、n2都是红黑树中ngx_rbtree_   ┃                                      ┃
┃ngx_rbt_copy_color(nl, n2)          ┃                                  ┃  将n2节点的颜色复制到nl节点          ┃
┃                                 I  ┃nodej类型的节点指针               ┃                                      ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃ngx_rbtree_node_t *                 ┃  node是红黑树中ngx_rbtree_node_  ┃                                      ┃
┃ngx_rbtree_min                      ┃t类型的节点指针；sentinel是这棵红 ┃  找到当前节点及其子树中的最小节点    ┃
┃(ngx_rbtree_node_t木node,           ┃黑树的哨兵节点                    ┃（按照key关键字）                     ┃
┃ngx_rbtree_node_t *sentinel)        ┃                                  ┃                                      ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  node是红黑树中ngx_rbtree_node_  ┃  初始化哨兵节点，实际上就是将该节点  ┃
┃ngx_rbtree_sentinel_init(node)      ┃                                  ┃                                      ┃
┃                                    ┃t类型的节点指针                   ┃颜色置为黑色                          ┃
┗━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━┛

使用红黑树的简单例子
    本节以一个简单的例子来说明如何使用红黑树容器。首先在栈中分配rbtree红黑树容器
结构体以及哨兵节点sentinel（当然，也可以使用内存池或者从进程堆中分配），本例中的节
点完全以key关键字作为每个节点的唯一标识，这样就可以采用预设的ngx_rbtree insert
value方法了。最后可调用ngx_rbtree_init方法初始化红黑树，代码如下所示。
    ngx_rbtree_node_t  sentinel ;
    ngx_rbtree_init ( &rbtree, &sentinel,ngx_str_rbtree_insert_value)
    奉例中树节点的结构体将使用7.5.3节中介绍的TestRBTreeNode结构体，每个元素的key关键字按照1、6、8、11、13、15、17、22、25、27的顺
序一一向红黑树中添加，代码如下所示。
    rbTreeNode [0] .num=17;
    rbTreeNode [1] .num=22;
    rbTreeNode [2] .num=25;
    rbTreeNode [3] .num=27;
    rbTreeNode [4] .num=17;
    rbTreeNode [7] .num=22;
    rbTreeNode [8] .num=25;
    rbTreeNode [9] .num=27;
    for(i=0j i<10; i++)
    {
        rbTreeNode [i].node. key=rbTreeNode[i]. num;
        ngx_rbtree_insert(&rbtree,&rbTreeNode[i].node);
    )

ngx_rbtree_node_t *tmpnode   =   ngx_rbtree_min ( rbtree . root ,    &sentinel )  ;
    当然，参数中如果不使用根节点而是使用任一个节点也是可以的。下面来看一下如何
检索1个节点，虽然Nginx对此并没有提供预设的方法（仅对字符串类型提供了ngx_str_
rbtree_lookup检索方法），但实际上检索是非常简单的。下面以寻找key关键字为13的节点
为例来加以说明。
    ngx_uint_t lookupkey=13;
    tmpnode=rbtree.root;
    TestRBTreeNode *lookupNode;
    while (tmpnode  !=&sentinel)  {
        if (lookupkey!-tmpnode->key)  (
        ／／根据key关键字与当前节点的大小比较，决定是检索左子树还是右子树
        tmpnode=  (lookupkey<tmpnode->key)  ?tmpnode->left:tmpnode->right;
        continue：
        )
        ／／找到了值为13的树节点
        lookupNode=  (TestRBTreeNode*)  tmpnode;
        break;
    )
    从红黑树中删除1个节点也是非常简单的，如把刚刚找到的值为13的节点从rbtree中
删除，只需调用ngx_rbtree_delete方法。
ngx_rbtree_delete ( &rbtree , &lookupNode->node);

*/


typedef ngx_uint_t  ngx_rbtree_key_t;
typedef ngx_int_t   ngx_rbtree_key_int_t;


typedef struct ngx_rbtree_node_s  ngx_rbtree_node_t;

/*
ngx_rbtree_node_t是红黑树实现中必须用到的数据结构，一般我们把它放到结构体中的
第1个成员中，这样方便把自定义的结构体强制转换成ngx_rbtree_node_t类型。例如：
typedef struct  {
    //一般都将ngx_rbtree_node_t节点结构体放在自走义数据类型的第1位，以方便类型的强制转换 
    ngx_rbtree_node_t node;
    ngx_uint_t num;
) TestRBTreeNode;
    如果这里希望容器中元素的数据类型是TestRBTreeNode，那么只需要在第1个成员中
放上ngx_rbtree_node_t类型的node即可。在调用图7-7中ngx_rbtree_t容器所提供的方法
时，需要的参数都是ngx_rbtree_node_t类型，这时将TestRBTreeNode类型的指针强制转换
成ngx_rbtree_node_t即可。
*/
struct ngx_rbtree_node_s {
    /* key成员是每个红黑树节点的关键字，它必须是整型。红黑树的排序主要依据key成员 */
    ngx_rbtree_key_t       key; //无符号整型的关键字  参考ngx_http_file_cache_exists  其实就是ngx_http_cache_t->key的前4字节
    ngx_rbtree_node_t     *left;
    ngx_rbtree_node_t     *right;
    ngx_rbtree_node_t     *parent;
    u_char                 color; //节点的颜色，0表示黑色，l表示红色
    u_char                 data;//仅1个字节的节点数据。由于表示的空间太小，所以一般很少使用
};

typedef struct ngx_rbtree_s  ngx_rbtree_t;

/*
红黑树是一个通用的数据结构，它的节点（或者称为容器的元素）可以是包含基本红黑树节点的任意结构体。对于不同的结构体，很多场合
下是允许不同的节点拥有相同的关键字的（）。例如，不同的字符串可能
会散列出相同的关键字，这时它们在红黑树中的关键字是相同的，然而它们又是不同的节点，这样在添加时就不可以覆盖原有同名关键字节点，
而是作为新插入的节点存在。因此，在添加元素时，需要考虑到这种情况。将添加元素的方法抽象出ngx_rbtree_insert_pt函数指针可以很好
地实现这一思想，用户也可以灵活地定义自己的行为。Nginx帮助用户实现了3种简单行为的添加节点方法

为解决不同节点含有相同关键字的元素冲突问题，红黑树设置了ngx_rbtree_insert_pt指针，这样可灵活地添加冲突元素
*/

/*
表7-4 Nginx为红黑树已经实现好的3种数据添加方法 (ngx_rbtree_insert_pt指向以下三种方法)
┏━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┓
┃    方法名                          ┃    参数含义                          ┃    执行意义                  ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━┫
┃void ngx_rbtree_insert_value        ┃  root是红黑树容器的指针；node是      ┃  向红黑树添加数据节点，每个  ┃
┃(ngx_rbtree_node_t *root,           ┃待添加元素的ngx_rbtree_node_t成员     ┃数据节点的关键字都是唯一的，  ┃
┃ngx_rbtree_node_t *node,            ┃的指针；sentinel是这棵红黑树初始化    ┃不存在同一个关键字有多个节点  ┃
┃ngx_rbtree_node_t *sentinel)        ┃时哨兵节点的指针                      ┃的问题                        ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━┫
┃void ngx_rbtree_insert_timer_value  ┃  root是红黑树容器的指针；node是      ┃                              ┃
┃(ngx_rbtree_node_t *root,           ┃待添加元素的ngx_rbtree_node_t成员     ┃  向红黑树添加数据节点，每个  ┃
┃ngx_rbtree_node_t *node,            ┃的指针，它对应的关键字是时间或者      ┃数据节点的关键字表示时间戎者  ┃
┃                                    ┃时间差，可能是负数；sentinel是这棵    ┃时间差                        ┃
┃ngx_rbtree_node_t *sentinel)        ┃                                      ┃                              ┃
┃                                    ┃红黑树初始化时的哨兵节点              ┃                              ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━┫
┃void ngx_str_rbtree_insert_value    ┃  root是红黑树容器的指针；node是      ┃  向红黑树添加数据节点，每个  ┃
┃(ngx_rbtree_node_t *temp,           ┃待添加元素的ngx_str_node_t成员的      ┃数据节点的关键字可以不是唯一  ┃
┃ngx_rbtree_node_t *node,            ┃指针（ngx- rbtree_node_t类型会强制转  ┃的，但它们是以字符串作为唯一  ┃
┃                                    ┃化为ngx_str_node_t类型）；sentinel是  ┃的标识，存放在ngx_str_node_t  ┃
┃ngx_rbtree_node t *sentinel)        ┃                                      ┃                              ┃
┃                                    ┃这棵红黑树初始化时哨兵节点的指针      ┃结构体的str成员中             ┃
┗━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━┛
*/
typedef void (*ngx_rbtree_insert_pt) (ngx_rbtree_node_t *root,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel); //node插入root的方法  执行地方在ngx_rbtree_insert

struct ngx_rbtree_s {
    ngx_rbtree_node_t     *root;      //指向树的根节点。注意，根节点也是数据元素
    ngx_rbtree_node_t     *sentinel;  //指向NIL峭兵节点  哨兵节点是所有最下层的叶子节点都指向一个NULL空节点，图形化参考:http://blog.csdn.net/xzongyuan/article/details/22389185
    ngx_rbtree_insert_pt   insert;    //表示红黑树添加元素的函数指针，它决定在添加新节点时的行为究竟是替换还是新增
};

//sentinel哨兵代表外部节点，所有的叶子以及根部的父节点，都指向这个唯一的哨兵nil，哨兵的颜色为黑色

#define ngx_rbtree_init(tree, s, i)                                           \
    ngx_rbtree_sentinel_init(s);                                              \
    (tree)->root = s;                                                         \
    (tree)->sentinel = s;                                                     \
    (tree)->insert = i

void ngx_rbtree_insert(ngx_rbtree_t *tree, ngx_rbtree_node_t *node);
void ngx_rbtree_delete(ngx_rbtree_t *tree, ngx_rbtree_node_t *node);
void ngx_rbtree_insert_value(ngx_rbtree_node_t *root, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel);
void ngx_rbtree_insert_timer_value(ngx_rbtree_node_t *root,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel); 
    


#define ngx_rbt_red(node)               ((node)->color = 1)
#define ngx_rbt_black(node)             ((node)->color = 0)
#define ngx_rbt_is_red(node)            ((node)->color)
#define ngx_rbt_is_black(node)          (!ngx_rbt_is_red(node))
#define ngx_rbt_copy_color(n1, n2)      (n1->color = n2->color)


/* a sentinel must be black */

#define ngx_rbtree_sentinel_init(node)  ngx_rbt_black(node)


static ngx_inline ngx_rbtree_node_t *
ngx_rbtree_min(ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    while (node->left != sentinel) {
        node = node->left;
    }

    return node;
}

#endif /* _NGX_RBTREE_H_INCLUDED_ */

