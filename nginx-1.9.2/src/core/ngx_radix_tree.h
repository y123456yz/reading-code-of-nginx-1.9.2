
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_RADIX_TREE_H_INCLUDED_
#define _NGX_RADIX_TREE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#define NGX_RADIX_NO_VALUE   (uintptr_t) -1

typedef struct ngx_radix_node_s  ngx_radix_node_t;

//参考<深入理解nginx> 图7-9
struct ngx_radix_node_s {
    ngx_radix_node_t  *right; //指向右子树，如果没有右子树，则值为null空指针
    ngx_radix_node_t  *left; //指向左子树，如果没有左子树，则值为null空指针
    ngx_radix_node_t  *parent; //指向父节点，如果没有父节点，则（如根节点）值为null空指针
    uintptr_t          value; //value存储的是指针的值，它指向用户定义的数据结构。如果这个节点还未使用，value的值将是NGX_RADIX_NO_VALUE
};

/*
每次删除1个节点时，ngx_radix_treej基数树并不会释放这个节点占用的内存，而是把它添加到free单链表中。这样，在添加新的节点时，会首
先查看free中是否还有节点，如果free中有未使用的节点，则会优先使用，如果没有，就会再从pool内存池中分配新内存存储节点。

对于ngx_radix_tree-t结构体来说，仅从使用的角度来看，我们不需要了解pool、free、start、size这些成员的意义，仅了解如何使用root根节点即可。
*/
typedef struct {
    ngx_radix_node_t  *root;  //指向根节点
    ngx_pool_t        *pool;  //内存池，它负责给基数树的节点分配内存
    ngx_radix_node_t  *free;  //管理已经分配但暂时未使用（不在树中）的节点，free实际上是所有不在树中节点的单链表
    char              *start; //已分配内存中还未使用内存的首地址
    size_t             size;  //已分配内存中还未使用的内存大小
} ngx_radix_tree_t;


ngx_radix_tree_t *ngx_radix_tree_create(ngx_pool_t *pool,
    ngx_int_t preallocate);

ngx_int_t ngx_radix32tree_insert(ngx_radix_tree_t *tree,
    uint32_t key, uint32_t mask, uintptr_t value);
ngx_int_t ngx_radix32tree_delete(ngx_radix_tree_t *tree,
    uint32_t key, uint32_t mask);
uintptr_t ngx_radix32tree_find(ngx_radix_tree_t *tree, uint32_t key);

#if (NGX_HAVE_INET6)
ngx_int_t ngx_radix128tree_insert(ngx_radix_tree_t *tree,
    u_char *key, u_char *mask, uintptr_t value);
ngx_int_t ngx_radix128tree_delete(ngx_radix_tree_t *tree,
    u_char *key, u_char *mask);
uintptr_t ngx_radix128tree_find(ngx_radix_tree_t *tree, u_char *key);
#endif


#endif /* _NGX_RADIX_TREE_H_INCLUDED_ */
