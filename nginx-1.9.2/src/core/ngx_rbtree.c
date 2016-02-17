
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * The red-black tree code is based on the algorithm described in
 * the "Introduction to Algorithms" by Cormen, Leiserson and Rivest.
 */


static ngx_inline void ngx_rbtree_left_rotate(ngx_rbtree_node_t **root,
    ngx_rbtree_node_t *sentinel, ngx_rbtree_node_t *node);
static ngx_inline void ngx_rbtree_right_rotate(ngx_rbtree_node_t **root,
    ngx_rbtree_node_t *sentinel, ngx_rbtree_node_t *node);

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


第7章Nginx提供的高级数据结构专233
表7-6红黑树节点提供的方法
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
    表7-5中的方法大部分用于实现或者扩展红黑树的功能，如果只是使用红黑树，那么一
般情况下只会使用ngx_rbtre e_min方法。
    本节介绍的方法或者结构体的简单用法的实现可参见7.5.4节的相关示例。


使用红黑树的简单例子
    本节以一个简单的例子来说明如何使用红黑树容器。首先在栈中分配rbtree红黑树容器
结构体以及哨兵节点sentinel（当然，也可以使用内存池或者从进程堆中分配），本例中的节
点完全以key关键字作为每个节点的唯一标识，这样就可以采用预设的ngx_rbtree insert
value方法了。最后可调用ngx_rbtree_init方法初始化红黑树，代码如下所示。
    ngx_rbtree_node_t  sentinel ;
    ngx_rbtree_init ( &rbtree, &sentinel,ngx_str_rbtree_insert_value)
    例中树节点的结构体将使用TestRBTreeNode结构体，树中的所有节
点都取自图7-7，每个元素的key关键字按照1、6、8、11、13、15、17、22、25、27的顺
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
    以这种顺序添加完的红黑树形态。如果需要找出当前红黑树中最小的节
点，可以调用ngx_rbtree_min方法获取。
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

void
ngx_rbtree_insert(ngx_rbtree_t *tree, ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  **root, *temp, *sentinel;

    /* a binary tree insert */

    root = (ngx_rbtree_node_t **) &tree->root;
    sentinel = tree->sentinel;

    if (*root == sentinel) {
        node->parent = NULL;
        node->left = sentinel;
        node->right = sentinel;
        ngx_rbt_black(node);
        *root = node;

        return;
    }

    tree->insert(*root, node, sentinel);

    /* re-balance tree */

    while (node != *root && ngx_rbt_is_red(node->parent)) {

        if (node->parent == node->parent->parent->left) {
            temp = node->parent->parent->right;

            if (ngx_rbt_is_red(temp)) {
                ngx_rbt_black(node->parent);
                ngx_rbt_black(temp);
                ngx_rbt_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->right) {
                    node = node->parent;
                    ngx_rbtree_left_rotate(root, sentinel, node);
                }

                ngx_rbt_black(node->parent);
                ngx_rbt_red(node->parent->parent);
                ngx_rbtree_right_rotate(root, sentinel, node->parent->parent);
            }

        } else {
            temp = node->parent->parent->left;

            if (ngx_rbt_is_red(temp)) {
                ngx_rbt_black(node->parent);
                ngx_rbt_black(temp);
                ngx_rbt_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    ngx_rbtree_right_rotate(root, sentinel, node);
                }

                ngx_rbt_black(node->parent);
                ngx_rbt_red(node->parent->parent);
                ngx_rbtree_left_rotate(root, sentinel, node->parent->parent);
            }
        }
    }

    ngx_rbt_black(*root);
}

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
    奉例中树节点的结构体将使用TestRBTreeNode结构体，树中的所有节
点都取自图7-7，每个元素的key关键字按照1、6、8、11、13、15、17、22、25、27的顺
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

7.5.5如何自定义添加成员方法
    由于节点的key关键字必须是整型，这导致很多情况下不同的节点会具有相同的key关
键字。如果不希望出现具有相同key关键字的不同节点在向红黑树添加时出现覆盖原节点的
情况，就需要实现自有的ngx_rbtree_insert_pt艿法。
    许多Nginx模块在使用红黑树时都自定义了ngx_rbtree_insert_pt方法（如geo、
filecache模块等），以ngx_str_rbtree insert value为例，来说明如何
*/

void
ngx_rbtree_insert_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t  **p;

    for ( ;; ) {

        p = (node->key < temp->key) ? &temp->left : &temp->right;

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}

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
    表7-5中的方法大部分用于实现或者扩展红黑树的功能，如果只是使用红黑树，那么一
般情况下只会使用ngx_rbtre e_min方法。
    本节介绍的方法或者结构体的简单用法的实现可参见7.5.4节的相关示例。


使用红黑树的简单例子
    本节以一个简单的例子来说明如何使用红黑树容器。首先在栈中分配rbtree红黑树容器
结构体以及哨兵节点sentinel（当然，也可以使用内存池或者从进程堆中分配），本例中的节
点完全以key关键字作为每个节点的唯一标识，这样就可以采用预设的ngx_rbtree insert
value方法了。最后可调用ngx_rbtree_init方法初始化红黑树，代码如下所示。
    ngx_rbtree_node_t  sentinel ;
    ngx_rbtree_init ( &rbtree, &sentinel,ngx_str_rbtree_insert_value)
    奉例中树节点的结构体将使用的TestRBTreeNode结构体，每个元素的key关键字按照1、6、8、11、13、15、17、22、25、27的顺
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

如何自定义添加成员方法
    由于节点的key关键字必须是整型，这导致很多情况下不同的节点会具有相同的key关
键字。如果不希望出现具有相同key关键字的不同节点在向红黑树添加时出现覆盖原节点的
情况，就需要实现自有的ngx_rbtree_insert_pt艿法。
    许多Nginx模块在使用红黑树时都自定义了ngx_rbtree_insert_pt方法（如geo、
filecache模块等），本节以7.5.3节中介绍过的ngx_str_rbtree insert value为例，来说明如何

*/

void
ngx_rbtree_insert_timer_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t  **p;

    for ( ;; ) {

        /*
         * Timer values
         * 1) are spread in small range, usually several minutes,
         * 2) and overflow each 49 days, if milliseconds are stored in 32 bits.
         * The comparison takes into account that overflow.
         */

        /*  node->key < temp->key */

        p = ((ngx_rbtree_key_int_t) (node->key - temp->key) < 0)
            ? &temp->left : &temp->right;

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}

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

红黑树节点提供的方法
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
    以这种顺序添加完的红黑树形态如图7-7所示。如果需要找出当前红黑树中最小的节
点，可以调用ngx_rbtree_min方法获取。
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

    由于节点的key关键字必须是整型，这导致很多情况下不同的节点会具有相同的key关
键字。如果不希望出现具有相同key关键字的不同节点在向红黑树添加时出现覆盖原节点的
情况，就需要实现自有的ngx_rbtree_insert_pt艿法。
  
*/

void
ngx_rbtree_delete(ngx_rbtree_t *tree, ngx_rbtree_node_t *node)
{
    ngx_uint_t           red;
    ngx_rbtree_node_t  **root, *sentinel, *subst, *temp, *w;

    /* a binary tree delete */

    root = (ngx_rbtree_node_t **) &tree->root;
    sentinel = tree->sentinel;

    if (node->left == sentinel) {
        temp = node->right;
        subst = node;

    } else if (node->right == sentinel) {
        temp = node->left;
        subst = node;

    } else {
        subst = ngx_rbtree_min(node->right, sentinel);

        if (subst->left != sentinel) {
            temp = subst->left;
        } else {
            temp = subst->right;
        }
    }

    if (subst == *root) {
        *root = temp;
        ngx_rbt_black(temp);

        /* DEBUG stuff */
        node->left = NULL;
        node->right = NULL;
        node->parent = NULL;
        node->key = 0;

        return;
    }

    red = ngx_rbt_is_red(subst);

    if (subst == subst->parent->left) {
        subst->parent->left = temp;

    } else {
        subst->parent->right = temp;
    }

    if (subst == node) {

        temp->parent = subst->parent;

    } else {

        if (subst->parent == node) {
            temp->parent = subst;

        } else {
            temp->parent = subst->parent;
        }

        subst->left = node->left;
        subst->right = node->right;
        subst->parent = node->parent;
        ngx_rbt_copy_color(subst, node);

        if (node == *root) {
            *root = subst;

        } else {
            if (node == node->parent->left) {
                node->parent->left = subst;
            } else {
                node->parent->right = subst;
            }
        }

        if (subst->left != sentinel) {
            subst->left->parent = subst;
        }

        if (subst->right != sentinel) {
            subst->right->parent = subst;
        }
    }

    /* DEBUG stuff */
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->key = 0;

    if (red) {
        return;
    }

    /* a delete fixup */

    while (temp != *root && ngx_rbt_is_black(temp)) {

        if (temp == temp->parent->left) {
            w = temp->parent->right;

            if (ngx_rbt_is_red(w)) {
                ngx_rbt_black(w);
                ngx_rbt_red(temp->parent);
                ngx_rbtree_left_rotate(root, sentinel, temp->parent);
                w = temp->parent->right;
            }

            if (ngx_rbt_is_black(w->left) && ngx_rbt_is_black(w->right)) {
                ngx_rbt_red(w);
                temp = temp->parent;

            } else {
                if (ngx_rbt_is_black(w->right)) {
                    ngx_rbt_black(w->left);
                    ngx_rbt_red(w);
                    ngx_rbtree_right_rotate(root, sentinel, w);
                    w = temp->parent->right;
                }

                ngx_rbt_copy_color(w, temp->parent);
                ngx_rbt_black(temp->parent);
                ngx_rbt_black(w->right);
                ngx_rbtree_left_rotate(root, sentinel, temp->parent);
                temp = *root;
            }

        } else {
            w = temp->parent->left;

            if (ngx_rbt_is_red(w)) {
                ngx_rbt_black(w);
                ngx_rbt_red(temp->parent);
                ngx_rbtree_right_rotate(root, sentinel, temp->parent);
                w = temp->parent->left;
            }

            if (ngx_rbt_is_black(w->left) && ngx_rbt_is_black(w->right)) {
                ngx_rbt_red(w);
                temp = temp->parent;

            } else {
                if (ngx_rbt_is_black(w->left)) {
                    ngx_rbt_black(w->right);
                    ngx_rbt_red(w);
                    ngx_rbtree_left_rotate(root, sentinel, w);
                    w = temp->parent->left;
                }

                ngx_rbt_copy_color(w, temp->parent);
                ngx_rbt_black(temp->parent);
                ngx_rbt_black(w->left);
                ngx_rbtree_right_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        }
    }

    ngx_rbt_black(temp);
}


static ngx_inline void
ngx_rbtree_left_rotate(ngx_rbtree_node_t **root, ngx_rbtree_node_t *sentinel,
    ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  *temp;

    temp = node->right;
    node->right = temp->left;

    if (temp->left != sentinel) {
        temp->left->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->left) {
        node->parent->left = temp;

    } else {
        node->parent->right = temp;
    }

    temp->left = node;
    node->parent = temp;
}


static ngx_inline void
ngx_rbtree_right_rotate(ngx_rbtree_node_t **root, ngx_rbtree_node_t *sentinel,
    ngx_rbtree_node_t *node)
{
    ngx_rbtree_node_t  *temp;

    temp = node->left;
    node->left = temp->right;

    if (temp->right != sentinel) {
        temp->right->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->right) {
        node->parent->right = temp;

    } else {
        node->parent->left = temp;
    }

    temp->right = node;
    node->parent = temp;
}
