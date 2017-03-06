
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_SLAB_H_INCLUDED_
#define _NGX_SLAB_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_slab_page_s  ngx_slab_page_t;
//Í¼ĞÎ»¯Àí½â²Î¿¼:http://blog.csdn.net/u013009575/article/details/17743261
struct ngx_slab_page_s { //³õÊ¼»¯¸³ÖµÔÚngx_slab_init
    //¶àÖÖÇé¿ö£¬¶à¸öÓÃÍ¾  
    //µ±ĞèÒª·ÖÅäĞÂµÄÒ³µÄÊ±ºò£¬·ÖÅäN¸öÒ³ngx_slab_page_s½á¹¹ÖĞµÚÒ»¸öÒ³µÄslab±íÊ¾Õâ´ÎÒ»¹²·ÖÅäÁË¶àÉÙ¸öÒ³ //±ê¼ÇÕâÊÇÁ¬Ğø·ÖÅä¶à¸öpage£¬²¢ÇÒÎÒ²»ÊÇÊ×page£¬ÀıÈçÒ»´Î·ÖÅä3¸öpage,·ÖÅäµÄpageÎª[1-3]£¬Ôòpage[1].slab=3  page[2].slab=page[3].slab=NGX_SLAB_PAGE_BUSY¼ÇÂ¼
    //Èç¹ûOBJ<128Ò»¸öÒ³ÖĞ´æ·ÅµÄÊÇ¶à¸öobj(ÀıÈç128¸ö32×Ö½Úobj),Ôòslab¼ÇÂ¼ÀïÃæµÄobjµÄ´óĞ¡£¬¼ûngx_slab_alloc_locked
    //Èç¹ûobjÒÆÎ»´óĞ¡Îªngx_slab_exact_shift£¬Ò²¾ÍÊÇobj128×Ö½Ú£¬page->slab = 1;page->slab´æ´¢objµÄbitmap,ÀıÈçÕâÀïÎª1£¬±íÊ¾ËµµÚÒ»¸öobj·ÖÅä³öÈ¥ÁË   ¼ûngx_slab_alloc_locked
    //Èç¹ûobjÒÆÎ»´óĞ¡Îªngx_slab_exact_shift£¬Ò²¾ÍÊÇobj>128×Ö½Ú£¬page->slab = ((uintptr_t) 1 << NGX_SLAB_MAP_SHIFT) | shift;//´óÓÚ128£¬Ò²¾ÍÊÇÖÁÉÙ256£,4K×î¶àÒ²¾Í16¸ö256£¬Òò´ËÖ»ĞèÒªslabµÄ¸ß16Î»±íÊ¾objÎ»Í¼¼´¿É
    //µ±·ÖÅäÄ³Ğ©´óĞ¡µÄobjµÄÊ±ºò(Ò»¸ö»º´æÒ³´æ·Å¶à¸öobj)£¬slab±íÊ¾±»·ÖÅäµÄ»º´æµÄÕ¼ÓÃÇé¿ö(ÊÇ·ñ¿ÕÏĞ)£¬ÒÔbitÎ»À´±íÊ¾
    //Èç¹û
    uintptr_t         slab; //ngx_slab_initÖĞ³õÊ¼¸³ÖµÎª¹²ÏíÄÚ´æÖĞÊ£ÓàÒ³µÄ¸öÊı
    //ÔÚngx_slab_initÖĞ³õÊ¼»¯µÄ9¸öngx_slab_page_sÍ¨¹ınextÁ¬½ÓÔÚÒ»Æğ
    //Èç¹ûÒ³ÖĞµÄojb<128 = 128 »òÕß>128 ,ÔònextÖ±½ÓÖ¸Ïò¶ÔÓ¦µÄÒ³slots[slot].next = page; Í¬Ê±pages_m[]Ö¸Ïòpage->next = &slots[slot];
    ngx_slab_page_t  *next; //ÔÚ·ÖÅä½ÏĞ¡objµÄÊ±ºò£¬nextÖ¸Ïòslab pageÔÚpool->pagesµÄÎ»ÖÃ    //ÏÂÒ»¸öpageÒ³  
    //ÓÉÓÚÖ¸ÕëÊÇ4µÄ±¶Êı,ÄÇÃ´ºóÁ½Î»Ò»¶¨Îª0,´ËÊ±ÎÒÃÇ¿ÉÒÔÀûÓÃÖ¸ÕëµÄºóÁ½Î»×ö±ê¼Ç,³ä·ÖÀûÓÃ¿Õ¼ä. ÓÃµÍÁ½Î»¼ÇÂ¼NGX_SLAB_PAGEµÈ±ê¼Ç
    //Èç¹ûÒ³ÖĞµÄobj<128,±ê¼Ç¸ÃÒ³ÖĞ´æ´¢µÄÊÇĞ¡Óë128µÄobj page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_SMALL 
    //obj=128 page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_EXACT; 
    uintptr_t         prev;//ÉÏÒ»¸öpageÒ³  
};
/*
¹²ÏíÄÚ´æµÄÆäÊµµØÖ·¿ªÊ¼´¦Êı¾İ:ngx_slab_pool_t + 9 * sizeof(ngx_slab_page_t)(slots_m[]) + pages * sizeof(ngx_slab_page_t)(pages_m[]) +pages*ngx_pagesize(ÕâÊÇÊµ¼ÊµÄÊı¾İ²¿·Ö£¬
Ã¿¸öngx_pagesize¶¼ÓÉÇ°ÃæµÄÒ»¸öngx_slab_page_t½øĞĞ¹ÜÀí£¬²¢ÇÒÃ¿¸öngx_pagesize×îÇ°¶ËµÚÒ»¸öobj´æ·ÅµÄÊÇÒ»¸ö»òÕß¶à¸öintÀàĞÍbitmap£¬ÓÃÓÚ¹ÜÀíÃ¿¿é·ÖÅä³öÈ¥µÄÄÚ´æ)

m_slot[0]:Á´½ÓpageÒ³Ãæ,²¢ÇÒpageÒ³Ãæ»®·ÖµÄslot¿é´óĞ¡Îª2^3
m_slot[1]:Á´½ÓpageÒ³Ãæ,²¢ÇÒpageÒ³Ãæ»®·ÖµÄslot¿é´óĞ¡Îª2^4
m_slot[2]:Á´½ÓpageÒ³Ãæ,²¢ÇÒpageÒ³Ãæ»®·ÖµÄslot¿é´óĞ¡Îª2^5
¡­¡­¡­¡­¡­¡­.
m_slot[8]:Á´½ÓpageÒ³Ãæ,²¢ÇÒpageÒ³Ãæ»®·ÖµÄslot¿é´óĞ¡Îª2k(2048)

m_pageÊı×é:Êı×éÖĞÃ¿¸öÔªËØ¶ÔÓ¦Ò»¸öpageÒ³.
m_page[0]¶ÔÓ¦page[0]Ò³Ãæ
m_page[1]¶ÔÓ¦page[1]Ò³Ãæ
m_page[2]¶ÔÓ¦page[2]Ò³Ãæ
¡­¡­¡­¡­¡­¡­¡­¡­¡­¡­.
m_page[k]¶ÔÓ¦page[k]Ò³Ãæ
ÁíÍâ¿ÉÄÜÓĞµÄm_page[]Ã»ÓĞÏàÓ¦Ò³ÃæÓëËûÏà¶ÔÓ¦.

*/
//Í¼ĞÎ»¯Àí½â²Î¿¼:http://blog.csdn.net/u013009575/article/details/17743261
typedef struct { //³õÊ¼»¯¸³ÖµÔÚngx_slab_init  slab½á¹¹ÊÇÅäºÏ¹²ÏíÄÚ´æÊ¹ÓÃµÄ  ¿ÉÒÔÒÔlimit reqÄ£¿éÎªÀı£¬²Î¿¼ngx_http_limit_req_module
    ngx_shmtx_sh_t    lock; //mutexµÄËø  

    size_t            min_size; //ÄÚ´æ»º´æobj×îĞ¡µÄ´óĞ¡£¬Ò»°ãÊÇ1¸öbyte   //×îĞ¡·ÖÅäµÄ¿Õ¼äÊÇ8byte ¼ûngx_slab_init 
    //slab poolÒÔshiftÀ´±È½ÏºÍ¼ÆËãËùĞè·ÖÅäµÄobj´óĞ¡¡¢Ã¿¸ö»º´æÒ³ÄÜ¹»ÈİÄÉobj¸öÊıÒÔ¼°Ëù·ÖÅäµÄÒ³ÔÚ»º´æ¿Õ¼äµÄÎ»ÖÃ  
    size_t            min_shift; //ngx_init_zone_poolÖĞÄ¬ÈÏÎª3
/*
¹²ÏíÄÚ´æµÄÆäÊµµØÖ·¿ªÊ¼´¦Êı¾İ:ngx_slab_pool_t + 9 * sizeof(ngx_slab_page_t)(slots_m[]) + pages * sizeof(ngx_slab_page_t)(pages_m[]) +pages*ngx_pagesize(ÕâÊÇÊµ¼ÊµÄÊı¾İ²¿·Ö£¬
Ã¿¸öngx_pagesize¶¼ÓÉÇ°ÃæµÄÒ»¸öngx_slab_page_t½øĞĞ¹ÜÀí£¬²¢ÇÒÃ¿¸öngx_pagesize×îÇ°¶ËµÚÒ»¸öobj´æ·ÅµÄÊÇÒ»¸ö»òÕß¶à¸öintÀàĞÍbitmap£¬ÓÃÓÚ¹ÜÀíÃ¿¿é·ÖÅä³öÈ¥µÄÄÚ´æ)
*/
    //Ö¸Ïòngx_slab_pool_t + 9 * sizeof(ngx_slab_page_t) + pages * sizeof(ngx_slab_page_t) +pages*ngx_pagesize(ÕâÊÇÊµ¼ÊµÄÊı¾İ²¿·Ö)ÖĞµÄpages * sizeof(ngx_slab_page_t)¿ªÍ·´¦
    ngx_slab_page_t  *pages; //slab page¿Õ¼äµÄ¿ªÍ·   ³õÊ¼Ö¸Ïòpages * sizeof(ngx_slab_page_t)Ê×µØÖ·
    ngx_slab_page_t  *last; // Ò²¾ÍÊÇÖ¸ÏòÊµ¼ÊµÄÊı¾İÒ³pages*ngx_pagesize£¬Ö¸Ïò×îºóÒ»¸öpagesÒ³
    //¹ÜÀífreeµÄÒ³Ãæ   ÊÇÒ»¸öÁ´±íÍ·,ÓÃÓÚÁ¬½Ó¿ÕÏĞÒ³Ãæ.
    ngx_slab_page_t   free; //³õÊ¼»¯¸³ÖµÔÚngx_slab_init  free->nextÖ¸Ïòpages * sizeof(ngx_slab_page_t)  ÏÂ´Î´Ófree.nextÊÇÏÂ´Î·ÖÅäÒ³Ê±ºòµÄÈë¿Ú¿ªÊ¼·ÖÅäÒ³¿Õ¼ä

    u_char           *start; //Êµ¼Ê»º´æobjµÄ¿Õ¼äµÄ¿ªÍ·   Õâ¸öÊÇ¶ÔµØÖ·¿Õ¼ä½øĞĞngx_pagesize¶ÔÆëºóµÄÆğÊ¼µØÖ·£¬¼ûngx_slab_init
    u_char           *end;

    ngx_shmtx_t       mutex; //ngx_init_zone_pool->ngx_shmtx_create->sem_init½øĞĞ³õÊ¼»¯

    u_char           *log_ctx;//pool->log_ctx = &pool->zero;
    u_char            zero;

    unsigned          log_nomem:1; //ngx_slab_initÖĞÄ¬ÈÏÎª1

    //ngx_http_file_cache_initÖĞcache->shpool->data = cache->sh;
    void             *data; //Ö¸Ïòngx_http_file_cache_t->sh
    void             *addr; //Ö¸Ïòngx_slab_pool_tµÄ¿ªÍ·    //Ö¸Ïò¹²ÏíÄÚ´ængx_shm_zone_tÖĞµÄaddr+sizeÎ²²¿µØÖ·
} ngx_slab_pool_t;

//Í¼ĞÎ»¯Àí½â²Î¿¼:http://blog.csdn.net/u013009575/article/details/17743261
void ngx_slab_init(ngx_slab_pool_t *pool);
void *ngx_slab_alloc(ngx_slab_pool_t *pool, size_t size);
void *ngx_slab_alloc_locked(ngx_slab_pool_t *pool, size_t size);
void *ngx_slab_calloc(ngx_slab_pool_t *pool, size_t size);
void *ngx_slab_calloc_locked(ngx_slab_pool_t *pool, size_t size);
void ngx_slab_free(ngx_slab_pool_t *pool, void *p);
void ngx_slab_free_locked(ngx_slab_pool_t *pool, void *p);


#endif /* _NGX_SLAB_H_INCLUDED_ */
