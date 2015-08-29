#ifndef _TC_PALLOC_H_INCLUDED_
#define _TC_PALLOC_H_INCLUDED_


#include <xcopy.h>

typedef struct tc_pool_large_s  tc_pool_large_t;

struct tc_pool_large_s {
    tc_pool_large_t     *next;
    void                *alloc;
};


typedef struct {
    u_char              *last;
    u_char              *end;
    tc_pool_t           *next;
    tc_uint_t            failed;
} tc_pool_data_t;


struct tc_pool_s {
    tc_pool_data_t       d;
    size_t               max;
    tc_pool_t           *current;
    tc_pool_large_t     *large;
};


tc_pool_t *tc_create_pool(size_t size, size_t pool_max);
void tc_destroy_pool(tc_pool_t *pool);

void *tc_palloc(tc_pool_t *pool, size_t size);
void *tc_pcalloc(tc_pool_t *pool, size_t size);
tc_int_t tc_pfree(tc_pool_t *pool, void *p);


#endif /* _TC_PALLOC_H_INCLUDED_ */
