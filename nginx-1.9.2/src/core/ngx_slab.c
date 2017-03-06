
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

#include <ngx_config.h>
#include <ngx_core.h>

/*
ÓÉÓÚÖ¸ÕëÊÇ4µÄ±¶Êı,ÄÇÃ´ºóÁ½Î»Ò»¶¨Îª0,´ËÊ±ÎÒÃÇ¿ÉÒÔÀûÓÃÖ¸ÕëµÄºóÁ½Î»×ö±ê¼Ç,³ä·ÖÀûÓÃ¿Õ¼ä.
ÔÚnginxµÄslabÖĞ,ÎÒÃÇÊ¹ÓÃngx_slab_page_s½á¹¹ÌåÖĞµÄÖ¸ÕëpreµÄºóÁ½Î»×ö±ê¼Ç,ÓÃÓÚÖ¸Ê¾¸ÃpageÒ³ÃæµÄslot¿éÊıÓëngx_slab_exact_sizeµÄ¹ØÏµ.
µ±page»®·ÖµÄslot¿éĞ¡ÓÚ32Ê±ºò,preµÄºóÁ½Î»ÎªNGX_SLAB_SMALL.
µ±page»®·ÖµÄslot¿éµÈÓÚ32Ê±ºò,preµÄºóÁ½Î»ÎªNGX_SLAB_EXACT
µ±page»®·ÖµÄslot´óÓÚ32¿éÊ±ºò,preµÄºóÁ½Î»ÎªNGX_SLAB_BIG
µ±pageÒ³Ãæ²»»®·ÖslotÊ±ºò,¼´½«Õû¸öÒ³Ãæ·ÖÅä¸øÓÃ»§,preµÄºóÁ½Î»ÎªNGX_SLAB_PAGE
*/
#define NGX_SLAB_PAGE_MASK   3
#define NGX_SLAB_PAGE        0
#define NGX_SLAB_BIG         1
#define NGX_SLAB_EXACT       2
#define NGX_SLAB_SMALL       3

#if (NGX_PTR_SIZE == 4)

#define NGX_SLAB_PAGE_FREE   0
//±ê¼ÇÕâÊÇÁ¬Ğø·ÖÅä¶à¸öpage£¬²¢ÇÒÎÒ²»ÊÇÊ×page£¬ÀıÈçÒ»´Î·ÖÅä3¸öpage,·ÖÅäµÄpageÎª[1-3]£¬Ôòpage[1].slab=3  page[2].slab=page[3].slab=NGX_SLAB_PAGE_BUSY¼ÇÂ¼
#define NGX_SLAB_PAGE_BUSY   0xffffffff
#define NGX_SLAB_PAGE_START  0x80000000

#define NGX_SLAB_SHIFT_MASK  0x0000000f
#define NGX_SLAB_MAP_MASK    0xffff0000
#define NGX_SLAB_MAP_SHIFT   16

#define NGX_SLAB_BUSY        0xffffffff

#else /* (NGX_PTR_SIZE == 8) */

#define NGX_SLAB_PAGE_FREE   0
#define NGX_SLAB_PAGE_BUSY   0xffffffffffffffff
#define NGX_SLAB_PAGE_START  0x8000000000000000

#define NGX_SLAB_SHIFT_MASK  0x000000000000000f
#define NGX_SLAB_MAP_MASK    0xffffffff00000000
#define NGX_SLAB_MAP_SHIFT   32

#define NGX_SLAB_BUSY        0xffffffffffffffff

#endif


#if (NGX_DEBUG_MALLOC)

#define ngx_slab_junk(p, size)     ngx_memset(p, 0xA5, size)

#elif (NGX_HAVE_DEBUG_MALLOC)

#define ngx_slab_junk(p, size)                                                \
    if (ngx_debug_malloc)          ngx_memset(p, 0xA5, size)

#else

#define ngx_slab_junk(p, size)

#endif

static ngx_slab_page_t *ngx_slab_alloc_pages(ngx_slab_pool_t *pool,
    ngx_uint_t pages);
static void ngx_slab_free_pages(ngx_slab_pool_t *pool, ngx_slab_page_t *page,
    ngx_uint_t pages);
static void ngx_slab_error(ngx_slab_pool_t *pool, ngx_uint_t level,
    char *text);

//slabÒ³ÃæµÄ´óĞ¡,32Î»LinuxÖĞÎª4k,  
static ngx_uint_t  ngx_slab_max_size;//ÉèÖÃngx_slab_max_size = 2048B¡£Èç¹ûÒ»¸öÒ³Òª´æ·Å¶à¸öobj£¬Ôòobj sizeÒªĞ¡ÓÚÕâ¸öÊıÖµ 
/*
ÓÉÓÚÖ¸ÕëÊÇ4µÄ±¶Êı,ÄÇÃ´ºóÁ½Î»Ò»¶¨Îª0,´ËÊ±ÎÒÃÇ¿ÉÒÔÀûÓÃÖ¸ÕëµÄºóÁ½Î»×ö±ê¼Ç,³ä·ÖÀûÓÃ¿Õ¼ä.
ÔÚnginxµÄslabÖĞ,ÎÒÃÇÊ¹ÓÃngx_slab_page_s½á¹¹ÌåÖĞµÄÖ¸ÕëpreµÄºóÁ½Î»×ö±ê¼Ç,ÓÃÓÚÖ¸Ê¾¸ÃpageÒ³ÃæµÄslot¿éÊıÓëngx_slab_exact_sizeµÄ¹ØÏµ.
µ±page»®·ÖµÄslot¿éĞ¡ÓÚ32Ê±ºò,preµÄºóÁ½Î»ÎªNGX_SLAB_SMALL.
µ±page»®·ÖµÄslot¿éµÈÓÚ32Ê±ºò,preµÄºóÁ½Î»ÎªNGX_SLAB_EXACT
µ±page»®·ÖµÄslot´óÓÚ32¿éÊ±ºò,preµÄºóÁ½Î»ÎªNGX_SLAB_BIG
µ±pageÒ³Ãæ²»»®·ÖslotÊ±ºò,¼´½«Õû¸öÒ³Ãæ·ÖÅä¸øÓÃ»§,preµÄºóÁ½Î»ÎªNGX_SLAB_PAGE
*/ //»®·Ö32¸öslot¿é,Ã¿¸öslotµÄ´óĞ¡¾ÍÊÇngx_slab_exact_size  
static ngx_uint_t  ngx_slab_exact_size;//ÉèÖÃngx_slab_exact_size = 128B¡£·Ö½çÊÇ·ñÒªÔÚ»º´æÇø·ÖÅä¶îÍâ¿Õ¼ä¸øbitmap  
static ngx_uint_t  ngx_slab_exact_shift;//ngx_slab_exact_shift = 7£¬¼´128µÄÎ»±íÊ¾ //Ã¿¸öslot¿é´óĞ¡µÄÎ»ÒÆÊÇngx_slab_exact_shift  

/*
×¢Òâ£¬ÔÚngx_slab_pool_tÀïÃæÓĞÁ½ÖÖÀàĞÍµÄslab page£¬ËäÈ»¶¼ÊÇngx_slab_page_t¶¨ÒåµÄ½á¹¹£¬µ«ÊÇ¹¦ÄÜ²»¾¡ÏàÍ¬¡£Ò»ÖÖÊÇslots£¬ÓÃÀ´±íÊ¾´æ
·Å½ÏĞ¡objµÄÄÚ´æ¿é(Èç¹ûÒ³´óĞ¡ÊÇ 4096B£¬ÔòÊÇ<2048BµÄobj£¬¼´Ğ¡ÓÚ1/2Ò³)£¬ÁíÒ»ÖÖÀ´±íÊ¾ËùÒª·ÖÅäµÄ¿Õ¼äÔÚ»º´æÇøµÄÎ»ÖÃ¡£Nginx°Ñ»º´æobj·Ö
³É´óµÄ (>=2048B)ºÍĞ¡µÄ(<2048B)¡£Ã¿´Î¸ø´óµÄobj·ÖÅäÒ»Õû¸öÒ³£¬¶ø°Ñ¶à¸öĞ¡obj´æ·ÅÔÚÒ»¸öÒ³ÖĞ¼ä£¬ÓÃbitmapµÈ·½·¨À´±íÊ¾ ÆäÕ¼ÓÃÇé¿ö¡£¶øĞ¡
µÄobjÓÖ·ÖÎª3ÖÖ£ºĞ¡ÓÚ128B£¬µÈÓÚ128B£¬´óÓÚ128BÇÒĞ¡ÓÚ2048B¡£ÆäÖĞĞ¡ÓÚ128BµÄobjĞèÒªÔÚÊµ¼Ê»º³åÇø¶îÍâ·ÖÅä bitmap¿Õ¼äÀ´±íÊ¾ÄÚ´æÊ¹ÓÃÇé¿ö
(ÒòÎªslab³ÉÔ±Ö»ÓĞ4¸öbyte¼´32bit£¬Ò»¸ö»º´æÒ³4KB¿ÉÒÔ´æ·ÅµÄobj³¬¹ı32¸ö£¬ËùÒÔ²»ÄÜÓÃslab À´±íÊ¾)£¬ÕâÑù»áÔì³ÉÒ»¶¨µÄ¿Õ¼äËğÊ§¡£µÈÓÚ»ò´ó
ÓÚ128BµÄobjÒòÎª¿ÉÒÔÓÃÒ»¸ö32bitµÄÕûĞÎÀ´±íÊ¾Æä×´Ì¬£¬ËùÒÔ¾Í¿ÉÒÔÖ±½ÓÓÃslab³ÉÔ±¡£Ã¿´Î·ÖÅä µÄ¿Õ¼äÊÇ2^n£¬×îĞ¡ÊÇ8byte£¬8£¬16£¬32£¬64£¬
128£¬256£¬512£¬1024£¬2048¡£Ğ¡ÓÚ2^iÇÒ´óÓÚ2^(i-1)µÄobj»á±»·Ö ÅäÒ»¸ö2^iµÄ¿Õ¼ä£¬±ÈÈç56byteµÄobj¾Í»á·ÖÅäÒ»¸ö64byteµÄ¿Õ¼ä¡£
*/ //http://adam281412.blog.163.com/blog/static/33700067201111283235134/
/*
¹²ÏíÄÚ´æµÄÆäÊµµØÖ·¿ªÊ¼´¦Êı¾İ:ngx_slab_pool_t + 9 * sizeof(ngx_slab_page_t)(slots_m[]) + pages * sizeof(ngx_slab_page_t)(pages_m[]) +pages*ngx_pagesize(ÕâÊÇÊµ¼ÊµÄÊı¾İ²¿·Ö£¬
Ã¿¸öngx_pagesize¶¼ÓÉÇ°ÃæµÄÒ»¸öngx_slab_page_t½øĞĞ¹ÜÀí£¬²¢ÇÒÃ¿¸öngx_pagesize×îÇ°¶ËµÚÒ»¸öobj´æ·ÅµÄÊÇÒ»¸ö»òÕß¶à¸öintÀàĞÍbitmap£¬ÓÃÓÚ¹ÜÀíÃ¿¿é·ÖÅä³öÈ¥µÄÄÚ´æ)
*/

    //Í¼ĞÎ»¯Àí½â²Î¿¼:http://blog.csdn.net/u013009575/article/details/17743261
void
ngx_slab_init(ngx_slab_pool_t *pool)//poolÖ¸ÏòµÄÊÇÕû¸ö¹²ÏíÄÚ´æ¿Õ¼äµÄÆğÊ¼µØÖ·   slab½á¹¹ÊÇÅäºÏ¹²ÏíÄÚ´æÊ¹ÓÃµÄ ¿ÉÒÔÒÔlimit reqÄ£¿éÎªÀı£¬²Î¿¼ngx_http_limit_req_module
{
    u_char           *p;
    size_t            size;
    ngx_int_t         m;
    ngx_uint_t        i, n, pages;
    ngx_slab_page_t  *slots;

    /*
     //¼ÙÉèÃ¿¸öpageÊÇ4KB  
    //ÉèÖÃngx_slab_max_size = 2048B¡£Èç¹ûÒ»¸öÒ³Òª´æ·Å¶à¸öobj£¬Ôòobj sizeÒªĞ¡ÓÚÕâ¸öÊıÖµ  
    //ÉèÖÃngx_slab_exact_size = 128B¡£·Ö½çÊÇ·ñÒªÔÚ»º´æÇø·ÖÅä¶îÍâ¿Õ¼ä¸øbitmap  
    //ngx_slab_exact_shift = 7£¬¼´128µÄÎ»±íÊ¾  
     */
    /* STUB */
    if (ngx_slab_max_size == 0) {
        ngx_slab_max_size = ngx_pagesize / 2;
        ngx_slab_exact_size = ngx_pagesize / (8 * sizeof(uintptr_t));
        for (n = ngx_slab_exact_size; n >>= 1; ngx_slab_exact_shift++) {
            /* void */
        }
    }
    /**/

    pool->min_size = 1 << pool->min_shift; //×îĞ¡·ÖÅäµÄ¿Õ¼äÊÇ8byte  

    p = (u_char *) pool + sizeof(ngx_slab_pool_t); //¹²ÏíÄÚ´æÇ°ÃæµÄsizeof(ngx_slab_pool_t)ÊÇ¸øslab pollµÄ
    size = pool->end - p; //sizeÊÇ×ÜµÄ¹²ÏíÄÚ´æ - sizeof(ngx_slab_pool_t)×Ö½ÚºóµÄ³¤¶È

    ngx_slab_junk(p, size);

    slots = (ngx_slab_page_t *) p;
    n = ngx_pagesize_shift - pool->min_shift;//12-3=9

/*
ÕâĞ©slab pageÊÇ¸ø´óĞ¡Îª8£¬16£¬32£¬64£¬128£¬256£¬512£¬1024£¬2048byteµÄÄÚ´æ¿é ÕâĞ©slab pageµÄÎ»ÖÃÊÇÔÚpool->pagesµÄÇ°Ãæ³õÊ¼»¯  
¹²ÏíÄÚ´æµÄÆäÊµµØÖ·¿ªÊ¼´¦Êı¾İ:ngx_slab_pool_t + 9 * sizeof(ngx_slab_page_t)(slots_m[]) + pages * sizeof(ngx_slab_page_t)(pages_m[]) +pages*ngx_pagesize(ÕâÊÇÊµ¼ÊµÄÊı¾İ²¿·Ö£¬
Ã¿¸öngx_pagesize¶¼ÓÉÇ°ÃæµÄÒ»¸öngx_slab_page_t½øĞĞ¹ÜÀí£¬²¢ÇÒÃ¿¸öngx_pagesize×îÇ°¶ËµÚÒ»¸öobj´æ·ÅµÄÊÇÒ»¸ö»òÕß¶à¸öintÀàĞÍbitmap£¬ÓÃÓÚ¹ÜÀíÃ¿¿é·ÖÅä³öÈ¥µÄÄÚ´æ)
*/
    for (i = 0; i < n; i++) { //Õâ9¸öslots[]ÓÉ9 * sizeof(ngx_slab_page_t)Ö¸Ïò
        slots[i].slab = 0;
        slots[i].next = &slots[i];
        slots[i].prev = 0;
    }

    p += n * sizeof(ngx_slab_page_t); //Ìø¹ıÉÏÃæÄÇĞ©slab page  

    //**¼ÆËãÕâ¸ö¿Õ¼ä×Ü¹²¿ÉÒÔ·ÖÅäµÄ»º´æÒ³(4KB)µÄÊıÁ¿£¬Ã¿¸öÒ³µÄoverheadÊÇÒ»¸öslab pageµÄ´óĞ¡  
    //**Õâ¶ùµÄoverhead»¹²»°üÀ¨Ö®ºó¸ø<128BÎïÌå·ÖÅäµÄbitmapµÄËğºÄ  

    //ÕâÀï + sizeof(ngx_slab_page_t)µÄÔ­ÒòÊÇÃ¿¸öngx_pagesize¶¼ÓĞ¶ÔÓ¦µÄngx_slab_page_t½øĞĞ¹ÜÀí
    pages = (ngx_uint_t) (size / (ngx_pagesize + sizeof(ngx_slab_page_t))); //ÕâÀïµÄsizeÊÇ²»ÊÇÓ¦¸Ã°ÑÍ·²¿n * sizeof(ngx_slab_page_t)¼õÈ¥ºóÔÚ×ö¼ÆËã¸ü¼Ó×¼È·?
    //°ÑÃ¿¸ö»º´æÒ³×îÇ°ÃæµÄsizeof(ngx_slab_page_t)×Ö½Ú¶ÔÓ¦µÄslab page¹é0  
    ngx_memzero(p, pages * sizeof(ngx_slab_page_t));

    pool->pages = (ngx_slab_page_t *) p;

    //³õÊ¼»¯free£¬free.nextÊÇÏÂ´Î·ÖÅäÒ³Ê±ºòµÄÈë¿Ú  
    pool->free.prev = 0;
    pool->free.next = (ngx_slab_page_t *) p;

    //¸üĞÂµÚÒ»¸öslab pageµÄ×´Ì¬£¬Õâ¶ùslab³ÉÔ±¼ÇÂ¼ÁËÕû¸ö»º´æÇøµÄÒ³ÊıÄ¿  
    pool->pages->slab = pages; //µÚÒ»¸öpages->slabÖ¸¶¨ÁË¹²ÏíÄÚ´æÖĞ³ıÈ¥Í·²¿ÍâÊ£ÓàÒ³µÄ¸öÊı
    pool->pages->next = &pool->free;
    pool->pages->prev = (uintptr_t) &pool->free;

    //Êµ¼Ê»º´æÇø(Ò³)µÄ¿ªÍ·£¬¶ÔÆë   //ÒòÎª¶ÔÆëµÄÔ­Òò,Ê¹µÃm_pageÊı×éºÍÊı¾İÇøÓòÖ®¼ä¿ÉÄÜÓĞĞ©ÄÚ´æÎŞ·¨Ê¹ÓÃ,  
    pool->start = (u_char *)
                  ngx_align_ptr((uintptr_t) p + pages * sizeof(ngx_slab_page_t),
                                 ngx_pagesize);

    //¸ù¾İÊµ¼Ê»º´æÇøµÄ¿ªÊ¼ºÍ½áÎ²ÔÙ´Î¸üĞÂÄÚ´æÒ³µÄÊıÄ¿  
    //ÓÉÓÚÄÚ´æ¶ÔÆë²Ù×÷(pool->start´¦ÄÚ´æ¶ÔÆë),¿ÉÄÜµ¼ÖÂpages¼õÉÙ,  
    //ËùÒÔÒªµ÷Õû.mÎªµ÷ÕûºópageÒ³ÃæµÄ¼õĞ¡Á¿.  
    //ÆäÊµÏÂÃæ¼¸ĞĞ´úÂë¾ÍÏàµ±ÓÚ:  
    // pages =(pool->end - pool->start) / ngx_pagesize  
    //pool->pages->slab = pages;  
    m = pages - (pool->end - pool->start) / ngx_pagesize;
    if (m > 0) {
        pages -= m;
        pool->pages->slab = pages;
    }

    //Ìø¹ıpages * sizeof(ngx_slab_page_t)£¬Ò²¾ÍÊÇÖ¸ÏòÊµ¼ÊµÄÊı¾İÒ³pages*ngx_pagesize
    pool->last = pool->pages + pages;

    pool->log_nomem = 1;
    pool->log_ctx = &pool->zero;
    pool->zero = '\0';
}

//ÓÉÓÚÊÇ¹²ÏíÄÚ´æ£¬ËùÒÔÔÚ½ø³Ì¼äĞèÒªÓÃËøÀ´±£³ÖÍ¬²½
void *
ngx_slab_alloc(ngx_slab_pool_t *pool, size_t size)
{
    void  *p;

    ngx_shmtx_lock(&pool->mutex);

    p = ngx_slab_alloc_locked(pool, size);

    ngx_shmtx_unlock(&pool->mutex);

    return p;
}

/*
¶ÔÓÚ¸ø¶¨size,´Óslab_poolÖĞ·ÖÅäÄÚ´æ.
1.Èç¹ûsize´óÓÚµÈÓÚÒ»Ò³,ÄÇÃ´´Óm_pageÖĞ²éÕÒ,Èç¹ûÓĞÔòÖ±½Ó·µ»Ø,·ñÔòÊ§°Ü.
2.Èç¹ûsizeĞ¡ÓÚÒ»Ò³,Èç¹ûÁ´±íÖĞÓĞ¿ÕÓàslot¿é.
     (1).Èç¹ûsize´óÓÚngx_slab_exact_size,
a.ÉèÖÃslabµÄ¸ß16Î»(32Î»ÏµÍ³)´æ·Åsolt¶ÔÓ¦µÄmap,²¢ÇÒ¸Ã¶ÔÓ¦ÎªmapµÄµØÎ»¶ÔÓ¦pageÖĞ¸ßÎ»µÄslot¿é.ÀıÈç1110¶ÔÓ¦ÎªµÚ1¿éslotÊÇ¿ÉÓÃµÄ,2-4¿é²»¿ÉÓÃ.slabµÄµÍ16Îª´æ´¢slot¿é´óĞ¡µÄÎ»ÒÆ.
b.ÉèÖÃm_pageÔªËØµÄpreÖ¸ÕëÎªNGX_SLAB_BIG.
c.Èç¹ûpageµÄÈ«²¿slot¶¼±»Ê¹ÓÃÁË,ÄÇÃ´½«´ËÒ³Ãæ´Óm_slotÊı×éÔªËØµÄÁ´±íÖĞÒÆ³ı.
   (2).Èç¹ûsizeµÈÓÚngx_slab_exact_size
a.ÉèÖÃslab´æ´¢slotµÄmap,Í¬ÑùslabÖĞµÄµÍÎ»¶ÔÓ¦¸ßÎ»ÖÃµÄslot.
b.ÉèÖÃm_pageÔªËØµÄpreÖ¸ÕëÎªNGX_SLAB_EXACT.
c.Èç¹ûpageµÄÈ«²¿slot¶¼±»Ê¹ÓÃÁË,ÄÇÃ´½«´ËÒ³Ãæ´Óm_slotÊı×éÔªËØµÄÁ´±íÖĞÒÆ³ı.
   (3).Èç¹ûsizeĞ¡ÓÚngx_slab_exact_size
a.ÓÃpageÖĞµÄÇ°¼¸¸öslot´æ·ÅslotµÄmap,Í¬ÑùµÍÎ»¶ÔÓ¦¸ßÎ».
b.ÉèÖÃm_pageÔªËØµÄpreÖ¸ÕëÎªNGX_SLAB_SMALL.
b.Èç¹ûpageµÄÈ«²¿slot¶¼±»Ê¹ÓÃÁË,ÄÇÃ´½«´ËÒ³Ãæ´Óm_slotÊı×éÔªËØµÄÁ´±íÖĞÒÆ³ı.
 3.Èç¹ûÁ´±íÖĞÃ»ÓĞ¿ÕÓàµÄslot¿é,ÔòÔÚfreeÁ´±íÖĞÕÒµ½Ò»¸ö¿ÕÏĞµÄÒ³Ãæ·ÖÅä¸øm_slotÊı×éÔªËØÖĞµÄÁ´±í.
   (1).Èç¹ûsize´óÓÚngx_slab_exact_size,
a.ÉèÖÃslabµÄ¸ß16Î»(32Î»ÏµÍ³)´æ·Åsolt¶ÔÓ¦µÄmap,²¢ÇÒ¸Ã¶ÔÓ¦ÎªmapµÄµØÎ»¶ÔÓ¦pageÖĞ¸ßÎ»µÄslot¿é.slabµÄµÍ16Îª´æ´¢slot¿é´óĞ¡µÄÎ»ÒÆ.
b.ÉèÖÃm_pageÔªËØµÄpreÖ¸ÕëÎªNGX_SLAB_BIG.
c.½«·ÖÅäµÄÒ³ÃæÁ´Èëm_slotÊı×éÔªËØµÄÁ´±íÖĞ.
   (2).Èç¹ûsizeµÈÓÚngx_slab_exact_size
a.ÉèÖÃslab´æ´¢slotµÄmap,Í¬ÑùslabÖĞµÄµÍÎ»¶ÔÓ¦¸ßÎ»ÖÃµÄslot.
b.ÉèÖÃm_pageÔªËØµÄpreÖ¸ÕëÎªNGX_SLAB_EXACT.
c.½«·ÖÅäµÄÒ³ÃæÁ´Èëm_slotÊı×éÔªËØµÄÁ´±íÖĞ.
   (3).Èç¹ûsizeĞ¡ÓÚngx_slab_exact_size
a.ÓÃpageÖĞµÄÇ°¼¸¸öslot´æ·ÅslotµÄmap,Í¬ÑùµÍÎ»¶ÔÓ¦¸ßÎ».
b.ÉèÖÃm_pageÔªËØµÄpreÖ¸ÕëÎªNGX_SLAB_SMALL.
c.½«·ÖÅäµÄÒ³ÃæÁ´Èëm_slotÊı×éÔªËØµÄÁ´±íÖĞ.
4.³É¹¦Ôò·µ»Ø·ÖÅäµÄÄÚ´æ¿é,¼´Ö¸Õëp,·ñÔòÊ§°Ü,·µ»Ønull.

*/
//Í¼ĞÎ»¯Àí½â²Î¿¼:http://blog.csdn.net/u013009575/article/details/17743261
//·µ»ØµÄÖµÊÇËùÒª·ÖÅäµÄ¿Õ¼äÔÚÄÚ´æ»º´æÇøµÄÎ»ÖÃ  

/*
ÔÚÒ»¸öpageÒ³ÖĞ»ñÈ¡Ğ¡Óë2048µÄobj¿éµÄÊ±ºò£¬¶¼»áÉèÖÃpage->next = &slots[slot]; page->prev = &slots[slot]£¬slots[slot].next = page;Ò²¾ÍÊÇ×÷Îªobj¿éµÄÒ³
page[]¶¼»áºÍ¶ÔÓ¦µÄslots[]¹ØÁª£¬Èç¹ûÊÇ·ÖÅä´óÓÚ2048µÄ¿Õ¼ä£¬Ôò»á·ÖÅäÕû¸öÒ³£¬Æäpage[]ºÍslots¾ÍÃ»ÓĞ¹ØÏµ
µ±¸ÃpageÒ³ÓÃÍêºó£¬Ôò»áÖØĞÂ°Ñpage[]µÄnextºÍprevÖÃÎªNULL£¬Í¬Ê±°Ñ¶ÔÓ¦µÄslot[]µÄnextºÍprevÖ¸Ïòslot[]±¾Éí
µ±pageÓÃÍêºóÊÍ·ÅÆäÖĞÒ»¸öobjºó£¬ÓĞ»Ö¸´Îªpage->next = &slots[slot]; page->prev = &slots[slot]£¬slots[slot].next = page;
*/
void * 
ngx_slab_alloc_locked(ngx_slab_pool_t *pool, size_t size)
{ //Õâ¶ù¼ÙÉèpage_sizeÊÇ4KB  
    size_t            s;
    uintptr_t         p, n, m, mask, *bitmap;
    ngx_uint_t        i, slot, shift, map;
    ngx_slab_page_t  *page, *prev, *slots;

    if (size > ngx_slab_max_size) { //Èç¹ûÊÇlarge obj, size >= 2048B  

        ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0,
                       "slab alloc: %uz", size);

        //·ÖÅä1¸ö»ò¶à¸öÄÚ´æÒ³  
        page = ngx_slab_alloc_pages(pool, (size >> ngx_pagesize_shift)
                                          + ((size % ngx_pagesize) ? 1 : 0)); //ÀıÈçsize¸ÕºÃÊÇ4K,Ôòpage=1,Èç¹ûÊÇ4K+1£¬Ôòpage=2
        if (page) {
            //»ñµÃpageÏò¶ÔÓÚpage[0]µÄÆ«ÒÆÁ¿ÓÉÓÚm_pageºÍpageÊı×éÊÇÏà»¥¶ÔÓ¦µÄ,¼´m_page[0]¹ÜÀípage[0]Ò³Ãæ,m_page[1]¹ÜÀípage[1]Ò³Ãæ.  
            //ËùÒÔ»ñµÃpageÏà¶ÔÓÚm_page[0]µÄÆ«ÒÆÁ¿¾Í¿ÉÒÔ¸ù¾İstartµÃµ½ÏàÓ¦Ò³ÃæµÄÆ«ÒÆÁ¿.  
            p = (page - pool->pages) << ngx_pagesize_shift;
            p += (uintptr_t) pool->start; //µÃµ½Êµ¼Ê·ÖÅäµÄÒ³µÄÆğÊ¼µØÖ·

        } else {
            p = 0;
        }

        goto done;
    }

    //½ÏĞ¡µÄobj, size < 2048B¸ù¾İĞèÒª·ÖÅäµÄsizeÀ´È·¶¨ÔÚslotsµÄÎ»ÖÃ£¬Ã¿¸öslot´æ·ÅÒ»ÖÖ´óĞ¡µÄobjµÄ¼¯ºÏ£¬Èçslots[0]±íÊ¾8byteµÄ¿Õ¼ä£¬
    //slots[3]±íÊ¾64byteµÄ¿Õ¼äÈç¹ûobj¹ıĞ¡(<1B)£¬slotµÄÎ»ÖÃÊÇ1B¿Õ¼äµÄÎ»ÖÃ£¬¼´×îĞ¡·ÖÅä1B  
    if (size > pool->min_size) { //¼ÆËãÊ¹ÓÃÄÄ¸öslots[]£¬Ò²¾ÍÊÇĞèÒª·ÖÅäµÄ¿Õ¼äÊÇ¶àÉÙ  ÀıÈçsize=9,Ôò»áÊ¹ÓÃslot[1]£¬Ò²¾ÍÊÇ16×Ö½Ú
        shift = 1;
        for (s = size - 1; s >>= 1; shift++) { /* void */ }
        slot = shift - pool->min_shift;

    } else {
        size = pool->min_size;
        shift = pool->min_shift;
        slot = 0;
    }

    //ngx_slab_pool_t + 9 * sizeof(ngx_slab_page_t) + pages * sizeof(ngx_slab_page_t) +pages*ngx_pagesize(ÕâÊÇÊµ¼ÊµÄÊı¾İ²¿·Ö)
    ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0,
                   "slab alloc: %uz slot: %ui", size, slot);
                   
    //Ö¸Ïò9 * sizeof(ngx_slab_page_t) £¬Ò²¾ÍÊÇslots[0-8]Êı×é = 8 - 2048
    slots = (ngx_slab_page_t *) ((u_char *) pool + sizeof(ngx_slab_pool_t));
    //ÔÚngx_slab_initÖĞslots[]->nextÄ¬ÈÏÊÇÖ¸Ïò×Ô¼ºµÄ
    page = slots[slot].next;//¸ù¾İm_slot[slot]»ñµÃÏàÓ¦µÄm_pageÔªËØ,È»ºóÕÒµ½ÏàÓ¦Ò³Ãæ.  

    if (page->next != page) { //Èç¹ûÖ®Ç°ÒÑ¾­ÓĞ´ËÀà´óĞ¡objÇÒÄÇ¸öÒÑ¾­·ÖÅäµÄÄÚ´æ»º´æÒ³»¹Î´Âú  9 ¸öngx_slab_page_t¶¼»¹Ã»ÓĞÓÃÍê

        if (shift < ngx_slab_exact_shift) { //Ğ¡obj£¬size < 128B£¬¸üĞÂÄÚ´æ»º´æÒ³ÖĞµÄbitmap£¬²¢·µ»Ø´ı·ÖÅäµÄ¿Õ¼äÔÚ»º´æµÄÎ»ÖÃ  

            do {
                p = (page - pool->pages) << ngx_pagesize_shift; 
                bitmap = (uintptr_t *) (pool->start + p);//pool->start¿ªÊ¼µÄ128×Ö½ÚÎªĞèÒª·ÖÅäµÄ¿Õ¼ä£¬Æäºó½ô¸úÒ»¸öint 4×Ö½Ú¿Õ¼äÓÃÓÚbitmap

                /*  
               ÀıÈçÒª·ÖÅäµÄsizeÎª54×Ö½Ú£¬ÔòÔÚÇ°Ãæ¼ÆËã³öµÄshift¶ÔÓ¦µÄ×Ö½ÚÊıÓ¦¸ÃÊÇ64×Ö½Ú£¬ÓÉÓÚÒ»¸öÒ³ÃæÈ«ÊÇ64×Ö½Úobj´óĞ¡£¬ËùÒÔÒ»¹²ÓĞ64
               ¸ö64×Ö½ÚµÄobj£¬64¸öobjÖÁÉÙĞèÒª64Î»À´±íÊ¾Ã¿¸öobjÊÇ·ñÒÑÊ¹ÓÃ£¬Òò´ËÖÁÉÙĞèÒª64Î»(Ò²¾ÍÊÇ8×Ö½Ú£¬2¸öint),ËùÒÔÖÁÉÙÒªÔİÓÃÒ»¸ö64
               ×Ö½ÚobjÀ´´æ´¢¸ÃbitmapĞÅÏ¢£¬µÚÒ»¸ö64×Ö½ÚobjÊµ¼ÊÉÏÖ»ÓÃÁË8×Ö½Ú£¬ÆäËû56×Ö½ÚÎ´ÓÃ
               */
                //¼ÆËãĞèÒª¶àÉÙ¸ö×Ö½ÚÀ´´æ´¢bitmap  
                map = (1 << (ngx_pagesize_shift - shift))
                          / (sizeof(uintptr_t) * 8);

                for (n = 0; n < map; n++) {

                    if (bitmap[n] != NGX_SLAB_BUSY) {//Èç¹ûpageµÄobj¿é¿ÕÏĞ,Ò²¾ÍÊÇbitmapÖ¸ÏòµÄ32¸öobjÊÇ·ñ¶¼ÒÑ¾­±»·ÖÅä³öÈ¥ÁË  
                        //Èç¹ûÕû¸öpageÒ³µÄËùÓĞslabÒÑ¾­ÓÃÍê£¬Ôò»áÔÚºóÃæµÄngx_slab_alloc_pages´ÓĞÂ»ñÈ¡¿Õ¼ä²¢»®·Öslab£¬È»ºó·ÖÅä
                        for (m = 1, i = 0; m; m <<= 1, i++) {//Èç¹ûobj¿é±»Õ¼ÓÃ,¼ÌĞø²éÕÒ  ´ÓÕâ32¸öobjÖĞÕÒ³öµÚÒ»¸öÎ´±»·ÖÅä³öÈ¥µÄobj
                            if ((bitmap[n] & m)) {
                                continue;
                            }

                            //ÕÒµ½ÁË£¬¸Ãbitmap¶ÔÓ¦µÄµÚn¸ö(×¢ÒâÊÇÎ»ÒÆ²Ù×÷ºóµÄm)Î´Ê¹ÓÃ£¬Ê¹ÓÃËû£¬Í¬Ê±ÖÃÎ»¸ÃÎ»£¬±íÊ¾¸Ãbitmp[n]ÒÑ¾­²»ÄÜÔÙ±»·ÖÅä£¬ÒòÎªÒÑ¾­±¾´Î·ÖÅä³öÈ¥ÁË
                            bitmap[n] |= m;

                            i = ((n * sizeof(uintptr_t) * 8) << shift)
                                + (i << shift); //¸ÃbitËù´¦µÄÕû¸öbitmapÖĞµÄµÚ¼¸Î»(ÀıÈçĞèÒª3¸öbitmap±íÊ¾ËùÓĞµÄslab¿é£¬ÔòÏÖÔÚÊÇµÚÈı¸öbitmapµÄµÚÒ»Î»£¬Ôòi=32+32+1-1,bit´Ó0¿ªÊ¼)

                            if (bitmap[n] == NGX_SLAB_BUSY) { //Èç¹û¸Ã32Î»Í¼ÔÚÕâ´ÎÈ¡µ½×îºóµÚ31Î»(0-31)µÄÊ±ºò£¬Ç°ÃæµÄbitmap[n] |= mºó;Ê¹Æä¸ÕºÃNGX_SLAB_BUSY£¬Ò²¾ÍÊÇÎ»Í¼ÌîÂú
                                for (n = n + 1; n < map; n++) {
                                     if (bitmap[n] != NGX_SLAB_BUSY) {//Èç¹û¸ÃbitmapºóÃæµÄ¼¸¸öbitmap»¹Ã»ÓĞÓÃÍê£¬ÔòÖ±½Ó·µ»Ø¸ÃbitmapµØÖ·
                                         p = (uintptr_t) bitmap + i;

                                         goto done;
                                     }
                                }
                                //& ~NGX_SLAB_PAGE_MASKÕâ¸öµÄÔ­ÒòÊÇĞèÒª»Ö¸´Ô­À´µÄµØÖ·£¬ÒòÎªµÍÁ½Î»ÔÚµÚÒ»´Î»ñÈ¡¿Õ¼äµÄÊ±ºò£¬×öÁËÌØÊâÒâÒå´¦Àí
                                prev = (ngx_slab_page_t *)
                                            (page->prev & ~NGX_SLAB_PAGE_MASK); //Ò²¾ÍÊÇ¸Ãobj¶ÔÓ¦µÄslot_m[]

                                //pages_m[i]ºÍslot_m[i]È¡Ïû¶ÔÓ¦µÄÒıÓÃ¹ØÏµ£¬ÒòÎª¸Ãpages_m[i]Ö¸ÏòµÄÒ³pageÒÑ¾­ÓÃÍêÁË
                                prev->next = page->next;
                                page->next->prev = page->prev; //slot_m[i]½á¹¹µÄnextºÍprevÖ¸Ïò×Ô¼º

                                page->next = NULL; //pageµÄnextºÍprevÖ¸ÏòNULL£¬±íÊ¾²»ÔÙ¿ÉÓÃÀ´·ÖÅäslot[]¶ÔÓ¦µÄobj
                                page->prev = NGX_SLAB_SMALL;
                            }

                            p = (uintptr_t) bitmap + i;

                            goto done;
                        }
                    }
                }

                page = page->next;

            } while (page);

        } else if (shift == ngx_slab_exact_shift) {
            //size == 128B£¬ÒòÎªÒ»¸öÒ³¿ÉÒÔ·Å32¸ö£¬ÓÃslab pageµÄslab³ÉÔ±À´±ê×¢Ã¿¿éÄÚ´æµÄÕ¼ÓÃÇé¿ö£¬²»ĞèÒªÁíÍâÔÚÄÚ´æ»º´æÇø·ÖÅäbitmap£¬
            //²¢·µ»Ø´ı·ÖÅäµÄ¿Õ¼äÔÚ»º´æµÄÎ»ÖÃ  

            do {
                if (page->slab != NGX_SLAB_BUSY) { //¸ÃpageÊÇ·ñÒÑ¾­ÓÃÍê

                    for (m = 1, i = 0; m; m <<= 1, i++) { //Èç¹ûÕû¸öpageÒ³µÄËùÓĞslabÒÑ¾­ÓÃÍê£¬Ôò»áÔÚºóÃæµÄngx_slab_alloc_pages´ÓĞÂ»ñÈ¡¿Õ¼ä²¢»®·Öslab£¬È»ºó·ÖÅä
                        if ((page->slab & m)) {
                            continue;
                        }

                        page->slab |= m; //±ê¼ÇµÚm¸öslabÏÖÔÚ·ÖÅä³öÈ¥ÁË

                        if (page->slab == NGX_SLAB_BUSY) {//Ö´ĞĞÍêpage->slab |= m;ºó£¬ÓĞ¿ÉÄÜpage->slab == NGX_SLAB_BUSY£¬±íÊ¾×îºóÒ»¸öobjÒÑ¾­·ÖÅä³öÈ¥ÁË
                            //pages_m[i]ºÍslot_m[i]È¡Ïû¶ÔÓ¦µÄÒıÓÃ¹ØÏµ£¬ÒòÎª¸Ãpages_m[i]Ö¸ÏòµÄÒ³pageÒÑ¾­ÓÃÍêÁË
                            prev = (ngx_slab_page_t *)
                                            (page->prev & ~NGX_SLAB_PAGE_MASK);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = NGX_SLAB_EXACT;
                        }

                        p = (page - pool->pages) << ngx_pagesize_shift;
                        p += i << shift;
                        p += (uintptr_t) pool->start; //·µ»Ø¸Ãobj¶ÔÓ¦µÄµØÖ·

                        goto done; 
                    }
                }

                page = page->next;

            } while (page);

        } else { /* shift > ngx_slab_exact_shift */

   //size > 128B£¬Ò²ÊÇ¸üĞÂslab pageµÄslab³ÉÔ±£¬µ«ÊÇĞèÒªÔ¤ÏÈÉèÖÃslabµÄ²¿·Öbit£¬ÒòÎªÒ»¸öÒ³µÄobjÊıÁ¿Ğ¡ÓÚ32¸ö£¬²¢·µ»Ø´ı·ÖÅäµÄ¿Õ¼äÔÚ»º´æµÄÎ»ÖÃ  
            n = ngx_pagesize_shift - (page->slab & NGX_SLAB_SHIFT_MASK);
            n = 1 << n;
            n = ((uintptr_t) 1 << n) - 1;
            mask = n << NGX_SLAB_MAP_SHIFT;

            do {//Èç¹ûÕû¸öpageÒ³µÄËùÓĞslabÒÑ¾­ÓÃÍê£¬Ôò»áÔÚºóÃæµÄngx_slab_alloc_pages´ÓĞÂ»ñÈ¡¿Õ¼ä²¢»®·Öslab£¬È»ºó·ÖÅä
                if ((page->slab & NGX_SLAB_MAP_MASK) != mask) {

                    for (m = (uintptr_t) 1 << NGX_SLAB_MAP_SHIFT, i = 0;
                         m & mask;
                         m <<= 1, i++)
                    {
                        if ((page->slab & m)) {
                            continue;
                        }

                        page->slab |= m;

                        if ((page->slab & NGX_SLAB_MAP_MASK) == mask) {
                            prev = (ngx_slab_page_t *)
                                            (page->prev & ~NGX_SLAB_PAGE_MASK);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = NGX_SLAB_BIG;
                        }

                        p = (page - pool->pages) << ngx_pagesize_shift;
                        p += i << shift;
                        p += (uintptr_t) pool->start;

                        goto done;
                    }
                }

                page = page->next;

            } while (page);
        }
    }

    //·Ö³öÒ»Ò³¼ÓÈëµ½m_slotÊı×é¶ÔÓ¦ÔªËØÖĞ  
    page = ngx_slab_alloc_pages(pool, 1);

    /*  
       ÀıÈçÒª·ÖÅäµÄsizeÎª54×Ö½Ú£¬ÔòÔÚÇ°Ãæ¼ÆËã³öµÄshift¶ÔÓ¦µÄ×Ö½ÚÊıÓ¦¸ÃÊÇ64×Ö½Ú£¬ÓÉÓÚÒ»¸öÒ³ÃæÈ«ÊÇ64×Ö½Úobj´óĞ¡£¬ËùÒÔÒ»¹²ÓĞ64
       ¸ö64×Ö½ÚµÄobj£¬64¸öobjÖÁÉÙĞèÒª64Î»À´±íÊ¾Ã¿¸öobjÊÇ·ñÒÑÊ¹ÓÃ£¬Òò´ËÖÁÉÙĞèÒª64Î»(Ò²¾ÍÊÇ8×Ö½Ú£¬2¸öint),ËùÒÔÖÁÉÙÒªÔİÓÃÒ»¸ö64
       ×Ö½ÚobjÀ´´æ´¢¸ÃbitmapĞÅÏ¢£¬µÚÒ»¸ö64×Ö½ÚobjÊµ¼ÊÉÏÖ»ÓÃÁË8×Ö½Ú£¬ÆäËû56×Ö½ÚÎ´ÓÃ
     */
    if (page) {
        //size<128
        if (shift < ngx_slab_exact_shift) {
            p = (page - pool->pages) << ngx_pagesize_shift;//slot¿éµÄmap´æ´¢ÔÚpageµÄslotÖĞ¶¨Î»µ½¶ÔÓ¦µÄpage  
            bitmap = (uintptr_t *) (pool->start + p); //pageÒ³µÄÆğÊ¼µØÖ·µÄÒ»¸öuintptr_tÀàĞÍ4×Ö½ÚÓÃÀ´´æ´¢bitmapĞÅÏ¢

            /*  
               ÀıÈçÒª·ÖÅäµÄsizeÎª54×Ö½Ú£¬ÔòÔÚÇ°Ãæ¼ÆËã³öµÄshift¶ÔÓ¦µÄ×Ö½ÚÊıÓ¦¸ÃÊÇ64×Ö½Ú£¬ÓÉÓÚÒ»¸öÒ³ÃæÈ«ÊÇ64×Ö½Úobj´óĞ¡£¬ËùÒÔÒ»¹²ÓĞ64
               ¸ö64×Ö½ÚµÄobj£¬64¸öobjÖÁÉÙĞèÒª64Î»À´±íÊ¾Ã¿¸öobjÊÇ·ñÒÑÊ¹ÓÃ£¬Òò´ËÖÁÉÙĞèÒª64Î»(Ò²¾ÍÊÇ8×Ö½Ú£¬2¸öint),ËùÒÔÖÁÉÙÒªÔİÓÃÒ»¸ö64
               ×Ö½ÚobjÀ´´æ´¢¸ÃbitmapĞÅÏ¢£¬µÚÒ»¸ö64×Ö½ÚobjÊµ¼ÊÉÏÖ»ÓÃÁË8×Ö½Ú£¬ÆäËû56×Ö½ÚÎ´ÓÃ
               */
            s = 1 << shift;//s±íÊ¾Ã¿¸öslot¿éµÄ´óĞ¡,×Ö½ÚÎªµ¥Î»  
            n = (1 << (ngx_pagesize_shift - shift)) / 8 / s;  //¼ÆËãbitmapĞèÒª¶àÉÙ¸öslot obj¿é  
        
            if (n == 0) {
                n = 1; //ÖÁÉÙĞèÒªÒ»¸ö4MÒ³Ãæ´óĞ¡µÄÒ»¸öobj(2<<shift×Ö½Ú)À´´æ´¢bitmapĞÅÏ¢
            }

            //ÉèÖÃ¶ÔÓ¦µÄslot¿éµÄmap,¶ÔÓÚ´æ·ÅmapµÄslotÉèÖÃÎª1,±íÊ¾ÒÑÊ¹ÓÃ²¢ÇÒÉèÖÃµÚÒ»¸ö¿ÉÓÃµÄslot¿é(²»ÊÇÓÃÓÚ¼ÇÂ¼mapµÄslot¿é)
            //±ê¼ÇÎª1,ÒòÎª±¾´Î¼´½«Ê¹ÓÃ.
            bitmap[0] = (2 << n) - 1; //ÕâÀïÊÇ»ñÈ¡¸ÃÒ³µÄµÚ¶ş¸öobj£¬ÒòÎªµÚÒ»¸öÒÑ¾­ÓÃÓÚbitmapÁË£¬ËùÒÔµÚÒ»¸öºÍµÚ¶ş¸öÔÚÕâÀï±íÊ¾ÒÑÓÃ£¬bitmap[0]=3

            //¼ÆËãËùÓĞobjµÄÎ»Í¼ĞèÒª¶àÉÙ¸öuintptr_t´æ´¢¡£ÀıÈçÃ¿¸öobj´óĞ¡Îª64×Ö½Ú£¬Ôò4KÀïÃæÓĞ64¸öobj£¬Ò²¾ÍĞèÒª8×Ö½Ú£¬Á½¸öbitmap
            map = (1 << (ngx_pagesize_shift - shift)) / (sizeof(uintptr_t) * 8);

            for (i = 1; i < map; i++) {
                bitmap[i] = 0; //´ÓµÚ¶ş¸öbitmap¿ªÊ¼µÄËùÓĞÎ»ÏÈÇå0
            }

            page->slab = shift; //¸ÃÒ³µÄÒ»¸öobj¶ÔÓ¦µÄ×Ö½ÚÒÆÎ»Êı´óĞ¡
            /* ngx_slab_pool_t + 9 * sizeof(ngx_slab_page_t)(slots_m[]) + pages * sizeof(ngx_slab_page_t)(pages_m[]) +pages*ngx_pagesize */
            page->next = &slots[slot];//Ö¸ÏòÉÏÃæµÄslots_m[i],ÀıÈçobj´óĞ¡64×Ö½Ú£¬ÔòÖ¸Ïòslots[2]   slots[0-8] -----8-2048
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_SMALL; //±ê¼Ç¸ÃÒ³ÖĞ´æ´¢µÄÊÇĞ¡Óë128µÄobj

            slots[slot].next = page;

            p = ((page - pool->pages) << ngx_pagesize_shift) + s * n;
            //·µ»Ø¶ÔÓ¦µØÖ·.  ÀıÈçÎª64×Ö½Úobj£¬Ôò·µ»ØµÄstartÎªµÚ¶ş¸ö¿ªÊ¼´¦obj£¬ÏÂ´Î·ÖÅä´ÓµÚ¶ş¸ö¿ªÊ¼»ñÈ¡µØÖ·¿Õ¼äobj
            p += (uintptr_t) pool->start;//·µ»Ø¶ÔÓ¦µØÖ·.,

            goto done;

        } else if (shift == ngx_slab_exact_shift) {

            page->slab = 1;//slabÉèÖÃÎª1   page->slab´æ´¢objµÄbitmap,ÀıÈçÕâÀïÎª1£¬±íÊ¾ËµµÚÒ»¸öobj·ÖÅä³öÈ¥ÁË 4KÓĞ32¸ö128×Ö½Úobj,Òò´ËÒ»¸öslabÎ»Í¼¸ÕºÃ¿ÉÒÔ±íÊ¾Õâ32¸öobj
            page->next = &slots[slot];
            //ÓÃÖ¸ÕëµÄºóÁ½Î»×ö±ê¼Ç,ÓÃNGX_SLAB_SMALL±íÊ¾slot¿éĞ¡ÓÚngx_slab_exact_shift  
            /*
                ÉèÖÃslabµÄ¸ß16Î»(32Î»ÏµÍ³)´æ·Åsolt¶ÔÓ¦µÄmap,²¢ÇÒ¸Ã¶ÔÓ¦ÎªmapµÄµØÎ»¶ÔÓ¦pageÖĞ¸ßÎ»µÄslot¿é.slabµÄµÍ16Îª´æ´¢slot¿é´óĞ¡µÄÎ»ÒÆ.
               */ 
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_EXACT; 

            slots[slot].next = page;

            p = (page - pool->pages) << ngx_pagesize_shift;
            p += (uintptr_t) pool->start;//·µ»Ø¶ÔÓ¦µØÖ·.  

            goto done;

        } else { /* shift > ngx_slab_exact_shift */

            //´óÓÚ128£¬Ò²¾ÍÊÇÖÁÉÙ256£,4K×î¶àÒ²¾Í16¸ö256£¬Òò´ËÖ»ĞèÒªslabµÄ¸ß16Î»±íÊ¾objÎ»Í¼¼´¿É
            page->slab = ((uintptr_t) 1 << NGX_SLAB_MAP_SHIFT) | shift;
            page->next = &slots[slot];
            //ÓÃÖ¸ÕëµÄºóÁ½Î»×ö±ê¼Ç,ÓÃNGX_SLAB_BIG±íÊ¾slot¿é´óÓÚngx_slab_exact_shift  
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_BIG;

            slots[slot].next = page;

            p = (page - pool->pages) << ngx_pagesize_shift;
            p += (uintptr_t) pool->start;

            goto done;
        }
    }

    p = 0;

done:

    ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0, "slab alloc: %p", p);

    return (void *) p;
}


void *
ngx_slab_calloc(ngx_slab_pool_t *pool, size_t size)
{
    void  *p;

    ngx_shmtx_lock(&pool->mutex);

    p = ngx_slab_calloc_locked(pool, size);

    ngx_shmtx_unlock(&pool->mutex);

    return p;
}


void *
ngx_slab_calloc_locked(ngx_slab_pool_t *pool, size_t size)
{
    void  *p;

    p = ngx_slab_alloc_locked(pool, size);
    if (p) {
        ngx_memzero(p, size);
    }

    return p;
}


void
ngx_slab_free(ngx_slab_pool_t *pool, void *p)
{
    ngx_shmtx_lock(&pool->mutex);

    ngx_slab_free_locked(pool, p);

    ngx_shmtx_unlock(&pool->mutex);
}

/*
¸ù¾İ¸ø¶¨µÄÖ¸Õëp,ÊÍ·ÅÏàÓ¦ÄÚ´æ¿é.
1.ÕÒµ½p¶ÔÓ¦µÄÄÚ´æ¿éºÍ¶ÔÓ¦µÄm_pageÊı×éÔªËØ,
2.¸ù¾İm_pageÊı×éÔªËØµÄpreÖ¸ÕëÈ·¶¨Ò³ÃæÀàĞÍ
     (1).Èç¹ûNGX_SLAB_SMALLÀàĞÍ,¼´sizeĞ¡ÓÚngx_slab_exact_size
a.ÉèÖÃÏàÓ¦slot¿éÎª¿ÉÓÃ
b.ÉèÈç¹ûÕû¸öÒ³Ãæ¶¼¿ÉÓÃ,Ôò½«Ò³Ãæ¹éÈëfreeÖĞ
    (2).Èç¹ûNGX_SLAB_EXACTÀàĞÍ,¼´sizeµÈÓÚngx_slab_exact_size
a.ÉèÖÃÏàÓ¦slot¿éÎª¿ÉÓÃ
b.ÉèÈç¹ûÕû¸öÒ³Ãæ¶¼¿ÉÓÃ,Ôò½«Ò³Ãæ¹éÈëfreeÖĞ
    (3).Èç¹ûNGX_SLAB_BIGÀàĞÍ,¼´size´óÓÚngx_slab_exact_size
a.ÉèÖÃÏàÓ¦slot¿éÎª¿ÉÓÃ
b.ÉèÈç¹ûÕû¸öÒ³Ãæ¶¼¿ÉÓÃ,Ôò½«Ò³Ãæ¹éÈëfreeÖĞ
     (4).Èç¹ûNGX_SLAB_PAGEÀàĞÍ,¼´size´óĞ¡´óÓÚµÈÓÚÒ»¸öÒ³Ãæ
a.ÉèÖÃÏàÓ¦Ò³Ãæ¿éÎª¿ÉÓÃ
b.½«Ò³Ãæ¹éÈëfreeÖĞ
*/
void
ngx_slab_free_locked(ngx_slab_pool_t *pool, void *p)
{
    size_t            size;
    uintptr_t         slab, m, *bitmap;
    ngx_uint_t        n, type, slot, shift, map;
    ngx_slab_page_t  *slots, *page;

    ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0, "slab free: %p", p);

    if ((u_char *) p < pool->start || (u_char *) p > pool->end) {
        ngx_slab_error(pool, NGX_LOG_ALERT, "ngx_slab_free(): outside of pool");
        goto fail;
    }

    //¸ù¾İpÕÒµ½ĞèÒªÊÍ·ÅµÄm_pageÔªËØ  
    n = ((u_char *) p - pool->start) >> ngx_pagesize_shift;
    page = &pool->pages[n];
    slab = page->slab; //Èç¹û·ÖÅäµÄÊ±ºòÒ»´ÎĞÔ·ÖÅä¶à¸öpage£¬ÔòµÚÒ»¸öpageµÄslabÖ¸¶¨±¾´ÎÒ»´ÎĞÔ·ÖÅäÁË¶àÉÙ¸öÒ³page
    //¾İpreµÄµÍÁ½Î»À´ÅĞ¶Ï¸ÃÒ³ÃæÖĞµÄslot´óĞ¡ºÍngx_slab_exact_sizeµÄ´óĞ¡¹ØÏµ  
    type = page->prev & NGX_SLAB_PAGE_MASK;

    switch (type) {

    case NGX_SLAB_SMALL://slotĞ¡ÓÚngx_slab_exact_size  

        //¿éobj´óĞ¡µÄÆ«ÒÆ  
        shift = slab & NGX_SLAB_SHIFT_MASK;
        size = 1 << shift;//¼ÆËã¿éobjµÄ´óĞ¡  

        if ((uintptr_t) p & (size - 1)) {//ÓÉÓÚ¶ÔÆë,pµÄµØÖ·Ò»¶¨ÊÇobj´óĞ¡µÄÕûÊı±¶  
            goto wrong_chunk;
        }

        n = ((uintptr_t) p & (ngx_pagesize - 1)) >> shift;//Çó³öp¶ÔÓ¦µÄobj¿éµÄÎ»ÖÃ  
        m = (uintptr_t) 1 << (n & (sizeof(uintptr_t) * 8 - 1));////Çó³öÔÚuintptr_tÖĞ,pµÄÆ«ÒÆ,¼´Çó³öÔÚuintptr_tÖĞµÄµÚ¼¸Î»  
        //ÓÉÓÚmapÊÇ¸ù¾İuintptr_t»®·ÖµÄ,ËùÒÔÇó³ö¸Ã¿é¶ÔÓ¦µÄuintptr_tµÄÆ«ÒÆ,¼´Çó³öµÚ¼¸¸öuintptr_t  
        n /= (sizeof(uintptr_t) * 8); //¸ÃobjËùÔÚµÄÊÇÄÇ¸öbitmap£¬ÀıÈçÒ»¸öÒ³64¸öobj£¬ÔòĞèÒª2¸öbitmap±íÊ¾Õâ64¸öobjµÄÎ»Í¼
        bitmap = (uintptr_t *)
                             ((uintptr_t) p & ~((uintptr_t) ngx_pagesize - 1));//Çó³öp¶ÔÓ¦µÄpageÒ³ËùÔÚÎ»Í¼µÄÎ»ÖÃ  

        if (bitmap[n] & m) {//Èç¹ûµÚmÎ»È·ÊµÎª1,  

            if (page->next == NULL) { //Èç¹ûÒ³ÃæµÄµ±Ç°×´Ì¬ÊÇÈ«²¿ÒÑÊ¹ÓÃ,¾Í°ÑËûÁ´Èëslot_m[]ÖĞ
                slots = (ngx_slab_page_t *)
                                   ((u_char *) pool + sizeof(ngx_slab_pool_t));//¶¨Î»slot_mÊı×é  

                //ÕÒµ½¶ÔÓ¦µÄslot_m[]µÄÔªËØ  
                slot = shift - pool->min_shift;

                //Á´Èë¶ÔÓ¦µÄslot[]ÖĞ£¬±íÊ¾¸ÃÒ³¿ÉÒÔ¼ÌĞøÊ¹ÓÃÁË£¬ÔÚngx_slab_calloc_lockedÓÖ¿ÉÒÔ±éÀúµ½¸ÃÒ³£¬´ÓÖĞ·ÖÅäobj
                page->next = slots[slot].next;
                slots[slot].next = page;

                //ÉèÖÃm_pageµÄpre  
                page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_SMALL;
                page->next->prev = (uintptr_t) page | NGX_SLAB_SMALL;
            }

             //Ò³ÃæµÄµ±Ç°×´Ì¬ÊÇ²¿·ÖÒÑÊ¹ÓÃ,¼´ÒÑ¾­ÔÚslotÖĞ  ÉèÖÃslot¶ÔÓ¦Î»ÖÃÎª¿ÉÓÃ,¼´0  
            bitmap[n] &= ~m;

            //ÏÂÃæµÄ²Ù×÷Ö÷ÒªÊÇ²é¿´Õâ¸öÒ³ÃæÊÇ·ñ¶¼Ã»ÓÃ, Èç¹û¶¼Ã»Ê¹ÓÃ,Ôò½«Ò³Ãæ¹éÈëfreeÖĞ  
            
            n = (1 << (ngx_pagesize_shift - shift)) / 8 / (1 << shift);

            if (n == 0) {//¼ÆËãĞèÒª¶àÉÙ¸ö4×Ö½ÚbitmapÀ´´æ´¢Î»Í¼£¬ÀıÈç64¸öobjĞèÒª2¸öbitmapÀ´±íÊ¾64Î»,ĞèÒª¶àÉÙ¸öobjÀ´´æ´¢ÕâÁ½¸öbitmap£¬Ò»°ãµÚÒ»¸öobj¹»´æ´¢Á½¸öbitmap£¬Ò²¾ÍÊÇÓÃ64×Ö½ÚÖĞµÄ8×Ö½Ú
                n = 1;
            }

            //²é¿´µÚÒ»¸öuintptr_tÖĞµÄ¿éobjÊÇ·ñ¶¼¿ÉÓÃ  
            if (bitmap[0] & ~(((uintptr_t) 1 << n) - 1)) {
                goto done;
            }

             //¼ÆËãÎ»Í¼Ê¹ÓÃÁË¶àÉÙ¸öuintptr_tÀ´´æ´¢
            map = (1 << (ngx_pagesize_shift - shift)) / (sizeof(uintptr_t) * 8);

            for (n = 1; n < map; n++) {//²é¿´ÆäËûuintptr_tÊÇ·ñ¶¼Ã»Ê¹ÓÃ  
                if (bitmap[n]) {
                    goto done;
                }
            }

            ngx_slab_free_pages(pool, page, 1); //Õû¸öÒ³Ãæ¶¼Ã»ÓĞÊ¹ÓÃ£¬¹é»¹¸øfree 

            goto done;
        }

        goto chunk_already_free;

    case NGX_SLAB_EXACT:
        //pËù¶ÔÓ¦µÄÔÚslab(objÎ»Í¼)ÖĞµÄÎ»ÖÃ.  
        m = (uintptr_t) 1 <<
                (((uintptr_t) p & (ngx_pagesize - 1)) >> ngx_slab_exact_shift);
        size = ngx_slab_exact_size;

        if ((uintptr_t) p & (size - 1)) {//Èç¹ûpÎªpageÖĞµÄobj¿é,ÄÇÃ´Ò»¶¨ÊÇsizeÕûÊı±¶  
            goto wrong_chunk;
        }

        if (slab & m) {//slab(Î»Í¼)ÖĞ¶ÔÓ¦µÄÎ»Îª1  
            if (slab == NGX_SLAB_BUSY) {//Èç¹ûÕû¸öÒ³ÃæÖĞµÄËùÓĞobj¿é¶¼±»Ê¹ÓÃ,Ôò¸ÃÒ³page[]ºÍslot[]Ã»ÓĞ¶ÔÓ¦¹ØÏµ,Òò´ËĞèÒª°ÑÒ³page[]ºÍslot[]¶ÔÓ¦¹ØÏµ¼ÓÉÏ
                 //¶¨Î»slot[]Êı×é  
                slots = (ngx_slab_page_t *)
                                   ((u_char *) pool + sizeof(ngx_slab_pool_t));
                slot = ngx_slab_exact_shift - pool->min_shift;//¼ÆËã¸ÃÒ³ÃæÒªÁ´Èëslot[]µÄÄÄ¸ö²ÛÖĞ  

                //ÉèÖÃpage[]ÔªËØºÍslot[]µÄ¶ÔÓ¦¹ØÏµ£¬Í¨¹ıprevºÍnextÖ¸Ïò
                page->next = slots[slot].next;
                slots[slot].next = page;
                
                page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_EXACT;
                page->next->prev = (uintptr_t) page | NGX_SLAB_EXACT;
            }

            page->slab &= ~m;//½«slab¿é¶ÔÓ¦Î»ÖÃÉèÖÃÎª0  

            if (page->slab) {//pageÒ³ÃæÖĞ»¹ÓĞÕıÔÚÊ¹ÓÃµÄobj¿é,ÒòÎªslabÎ»Í¼²»Îª0
                goto done;
            }

            ngx_slab_free_pages(pool, page, 1);//pageÒ³ÃæÖĞËùÓĞslab¿é¶¼Ã»ÓĞÊ¹ÓÃ  

            goto done;
        }

        goto chunk_already_free;

    case NGX_SLAB_BIG://ÓÃ»§ÇëÇóÄÚ´æµÄ´óĞ¡´óÓÚngx_slab_exact_size  
        //slabµÄ¸ß16Î»ÊÇslot¿éµÄÎ»Í¼,µÍ16Î»ÓÃÓÚ´æ´¢slot¿é´óĞ¡µÄÆ«ÒÆ  

        shift = slab & NGX_SLAB_SHIFT_MASK;
        size = 1 << shift;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }
        //ÕÒµ½¸Ãslab¿éÔÚÎ»Í¼ÖĞµÄÎ»ÖÃ.ÕâÀïÒª×¢ÒâÒ»ÏÂ,  
        //Î»Í¼´æ´¢ÔÚslabµÄ¸ß16Î»,ËùÒÔÒª+16(¼´+ NGX_SLAB_MAP_SHIFT)  
        m = (uintptr_t) 1 << ((((uintptr_t) p & (ngx_pagesize - 1)) >> shift)
                              + NGX_SLAB_MAP_SHIFT);

        if (slab & m) {//¸Ãslab¿éÈ·ÊµÕıÔÚ±»Ê¹ÓÃ  

            if (page->next == NULL) {//Èç¹ûÕû¸öÒ³ÃæÖĞµÄËùÓĞobj¿é¶¼±»Ê¹ÓÃ,Ôò¸ÃÒ³page[]ºÍslot[]Ã»ÓĞ¶ÔÓ¦¹ØÏµ,Òò´ËĞèÒª°ÑÒ³page[]ºÍslot[]¶ÔÓ¦¹ØÏµ¼ÓÉÏ
                //¶¨Î»slot[]Êı×é  
                slots = (ngx_slab_page_t *)
                                   ((u_char *) pool + sizeof(ngx_slab_pool_t));
                slot = shift - pool->min_shift;
                //ÕÒµ½slot[]Êı×éÖĞ¶ÔÓ¦µÄÎ»ÖÃ,Ìí¼Óslot[]ºÍpage[]µÄ¶ÔÓ¦¹ØÏµ
                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_BIG;
                page->next->prev = (uintptr_t) page | NGX_SLAB_BIG;
            }

            page->slab &= ~m;//ÉèÖÃslab¿é¶ÔÓ¦µÄÎ»Í¼Î»ÖÃÎª0,¼´¿ÉÓÃ  
            
            //Èç¹ûslabÒ³ÖĞÓĞslot¿é»¹ÔÚ±»Ê¹ÓÃ  
            if (page->slab & NGX_SLAB_MAP_MASK) {
                goto done;
            }

            //Èç¹ûpageÒ³ÖĞËùÓĞslab¿é¶¼²»ÔÚÊ¹ÓÃ¾Í½«¸ÃÒ³ÃæÁ´ÈëfreeÖĞ  
            ngx_slab_free_pages(pool, page, 1);

            goto done;
        }

        goto chunk_already_free;

    case NGX_SLAB_PAGE://ÓÃ»§¹é»¹Õû¸öÒ³Ãæ  

        if ((uintptr_t) p & (ngx_pagesize - 1)) { //pÊÇÒ²¶ÔÆëµÄ£¬¼ì²éÏÂ
            goto wrong_chunk;
        }

        if (slab == NGX_SLAB_PAGE_FREE) { 
        
            ngx_slab_error(pool, NGX_LOG_ALERT,
                           "ngx_slab_free(): page is already free");
            goto fail;
        }

        if (slab == NGX_SLAB_PAGE_BUSY) {
        //ËµÃ÷ÊÇÁ¬Ğø·ÖÅä¶à¸öpageµÄ·ÇÊ×¸öpage£¬²»ÄÜÖ±½ÓÊÍ·Å£¬²»ĞíÕâ¼¸¸öpageÒ»ÆğÊÍ·Å£¬Òò´ËpÖ¸ÕëÖ¸Ïò±ØĞëÊÇÊ×page
            ngx_slab_error(pool, NGX_LOG_ALERT,
                           "ngx_slab_free(): pointer to wrong page");
            goto fail;
        }

        //¼ÆËãÒ³Ãæ¶ÔÓ¦µÄpage[]²Û  
        n = ((u_char *) p - pool->start) >> ngx_pagesize_shift;
        size = slab & ~NGX_SLAB_PAGE_START;//¼ÆËã¹é»¹pageµÄ¸öÊı  

        ngx_slab_free_pages(pool, &pool->pages[n], size); //¹é»¹Ò³Ãæ  

        ngx_slab_junk(p, size << ngx_pagesize_shift);

        return;
    }

    /* not reached */

    return;

done:

    ngx_slab_junk(p, size);

    return;

wrong_chunk:

    ngx_slab_error(pool, NGX_LOG_ALERT,
                   "ngx_slab_free(): pointer to wrong chunk");

    goto fail;

chunk_already_free:

    ngx_slab_error(pool, NGX_LOG_ALERT,
                   "ngx_slab_free(): chunk is already free");

fail:

    return;
}

/*
·µ»ØÒ»¸öslab page£¬Õâ¸öslab pageÖ®ºó»á±»ÓÃÀ´È·¶¨ËùĞè·ÖÅäµÄ¿Õ¼äÔÚÄÚ´æ»º´æµÄÎ»ÖÃ 

ÀıÈç×Ü¹²ÓĞ6¸öpage[]£¬ngx_slab_initÖĞpage[0]µÄnextºÍprev¶¼Ö¸Ïòfree£¬freeµÄnextÒ²Ö¸Ïòpage[0]¡£µ±µ÷ÓÃngx_slab_alloc_pagesÏò»ñÈ¡3¸öpagesµÄÊ±ºò
ÔòÇ°Èı¸öpages(page[0], page[1], page[2])»á±»·ÖÅäºÃ£¬×îÄ©Î²page[5]µÄprevÖ¸Ïòpage[3],²¢ÇÒpage[3]µÄslabÖ¸¶¨ÏÖÔÚÖ»ÓĞ6-3=3¸öpage¿ÉÒÔ·ÖÅäÁË£¬
È»ºópage[3]µÄnextºÍprevÖ¸Ïòfree,freeµÄnextºÍprevÒ²Ö¸Ïòpage[3]£¬Ò²¾ÍÊÇÏÂ´ÎÖ»ÄÜ´Ópage[3]¿ªÊ¼»ñÈ¡Ò³

*/ //·ÖÅäÒ»¸öÒ³Ãæ,²¢½«Ò³Ãæ´ÓfreeÖĞÕª³ı.

/* 
-------------------------------------------------------------------
| page1  | page2 | page3 | page4| page5) | page6 | page7 | page8 |
--------------------------------------------------------------------
³õÊ¼×´Ì¬: pool->freeÖ¸Ïòpage1,page1Ö¸Ïòpoll->free,ÆäËûµÄpageµÄnextºÍprevÄ¬ÈÏÖ¸ÏòNULL,Ò²¾ÍÊÇpool->freeÖ¸ÏòÕû¸öpageÌå£¬page1->slab=8

1.¼ÙÉèµÚÒ»´Îngx_slab_alloc_pages»ñÈ¡Á½¸öpage£¬Ôòpage1ºÍpage2»á·ÖÅä³öÈ¥£¬page1ÎªÕâÁ½¸öpageµÄÊ×page1->slabÖ»³öÕâÁ½¸öÁ¬ĞøpageÊÇÒ»Æğ·ÖÅäµÄ,
    page2->slab = NGX_SLAB_PAGE_BUSY;±íÊ¾ÕâÊÇ¸úËæpage1Ò»Æğ·ÖÅä³öÈ¥µÄ£¬²¢ÇÒ±¾page²»ÊÇÊ×page¡£ÕâÊ±ºòpool->freeÖ¸Ïòpage3,²¢±íÊ¾page3->slab=6£¬
    ±íÊ¾page3¿ªÊ¼»¹ÓĞ6¸öpage¿ÉÒÔÊ¹ÓÃ
2.¼ÙÉèµÚ¶ş´ÎÓÖ»ñÈ¡ÁË3¸öpage(page3-5),Ôòpage3ÊÇÊ×page,page3->slab=3,Í¬Ê±page4,page5->slab = NGX_SLAB_PAGE_BUSY;±íÊ¾ÕâÊÇ¸úËæpage1Ò»
    Æğ·ÖÅä³öÈ¥µÄ£¬²¢ÇÒ±¾page²»ÊÇÊ×page¡£ÕâÊ±ºòpool->freeÖ¸Ïòpage6,²¢±íÊ¾page6->slab=3£¬±íÊ¾page6¿ªÊ¼»¹ÓĞ3¸öpage¿ÉÒÔÊ¹ÓÃ
3. Í¬ÀíÔÙ»ñÈ¡1¸öpage,page6¡£ÕâÊ±ºòpool->freeÖ¸Ïòpage7,²¢±íÊ¾page7->slab=2£¬±íÊ¾page7¿ªÊ¼»¹ÓĞ2¸öpage¿ÉÒÔÊ¹ÓÃ
4. ÏÖÔÚÊÍ·ÅµÚ1²½page1¿ªÊ¼µÄ2¸öpage£¬ÔòÔÚngx_slab_free_pagesÖĞ»á°ÑµÚ3²½ºóÊ£ÓàµÄÁ½¸öÎ´·ÖÅäµÄpage7(Êµ¼ÊÉÏÊÇ°Ñpage7¿ªÊ¼µÄ2¸öpage±êÊ¶Îª1¸ö´ópage)
   ºÍpage1(page1Êµ¼ÊÉÏÕâÊ±ºò±êÊ¶µÄÊÇµÚÒ»²½ÖĞµÄpage1¿ªÊ¼µÄ2¸öpage)ºÍpool->freeĞÎ³ÉÒ»¸öË«ÏòÁ´±í»·£¬¿ÉÒÔ¼û
  
                      pool->free  
                    ----------  /                 
 -----------------\|         | --------------------------------------  
|  ----------------|         | -----------------------------------   |
|  |                 -----------                                  |  |
|  |                                                              |  |
|  |     page1 2                                   page7 2        |  |
|  |  \ ----------                          \  -----------   /    |  |
|   --- |    |    |----------------------------|    |     | -------  | 
------- |    |    |--------------------------- |    |     | ---------- 
        ----------  \                           ---------

5.µ±ÊÍ·Ångx_slab_free_pagespage3¿ªÊ¼µÄ3¸öpageÒ³ºó£¬page3Ò²»áÁ¬½Óµ½Ë«Ïò»·±íÖĞ£¬Á´Èçpool->freeÓëpage1[i]Ö®¼ä£¬×¢ÒâÕâÊ±ºòµÄpage1ºÍpage2ÊÇ½ô¿¿Ò»ÆğµÄpage
  µ«Ã»ÓĞ¶ÔËûÃÇ½øĞĞºÏ²¢£¬page1->slab»¹ÊÇ=2  page2->slab»¹ÊÇ=3¡£²¢Ã»ÓĞ°ÑËûÃÇºÏ²¢ÎªÒ»¸öÕûÌåpage->slab=5,Èç¹ûÏÂ´ÎÏëallocÒ»¸ö4pageµÄ¿Õ¼ä£¬ÊÇ
  ·ÖÅä²»³É¹¦µÄ
*/
static ngx_slab_page_t *
ngx_slab_alloc_pages(ngx_slab_pool_t *pool, ngx_uint_t pages) //Óëngx_slab_free_pagesÅäºÏÔÄ¶ÁÀí½â
{
    ngx_slab_page_t  *page, *p;

    //³õÊ¼»¯µÄÊ±ºòpool->free.nextÄ¬ÈÏÖ¸ÏòµÚÒ»¸öpool->pages
    //´Ópool->free.next¿ªÊ¼£¬Ã¿´ÎÈ¡(slab page) page = page->next  
    for (page = pool->free.next; page != &pool->free; page = page->next) { //Èç¹ûÒ»¸ö¿ÉÓÃpageÒ³¶¼Ã»ÓĞ£¬¾Í²»»á½øÈëÑ­»·Ìå

    /*
    ±¾¸öslab pageÊ£ÏÂµÄ»º´æÒ³ÊıÄ¿>=ĞèÒª·ÖÅäµÄ»º´æÒ³ÊıÄ¿pagesÔò¿ÉÒÔ·ÖÅä£¬·ñÔò¼ÌĞø±éÀúfree,Ö±µ½ÏÂÒ»¸öÊ×page¼°ÆäºóÁ¬ĞøpageÊıºÍ´óÓÚµÈÓÚĞèÒª·ÖÅäµÄpagesÊı£¬²Å¿ÉÒÔ·ÖÅä 
    slabÊÇÊ×´Î·ÖÅäpage¿ªÊ¼µÄslab¸öÒ³µÄÊ±ºòÖ¸¶¨µÄ£¬ÔÚÊÍ·ÅµÄÊ±ºòslab»¹ÊÇÊ×´Î·ÖÅäÊ±ºòµÄslab£¬²»»á±ä£¬Ò²¾ÍÊÇËµÊÍ·Åpageºó²»»á°ÑÏàÁÚµÄÁ½¸öpageÒ³µÄslabÊıºÏ²¢£¬
    ÀıÈçÊ×´Î¿ª±Ùpage1¿ªÊ¼µÄ3¸öpageÒ³¿Õ¼ä£¬page1->slab=3,½ô½Ó×Å¿ª±Ùpage2¿ªÊ¼µÄ2¸öpageÒ³¿Õ¼ä£¬page2->slab=2,µ±Á¬ĞøÊÍ·Åpage1ºÍpage2¶ÔÓ¦µÄ¿Õ¼äºó£¬ËûÃÇ»¹ÊÇ
    Á½¸ö¶ÀÁ¢µÄpage[]¿Õ¼ä£¬slab·Ö±ğÊÇ2ºÍ3,¶ø²»»á°ÑÕâÁ½¿éÁ¬Ğø¿Õ¼ä½øĞĞºÏ²¢Îª1¸ö(Ò²¾ÍÊÇĞÂµÄpage3,page3Ê×µØÖ·µÈÓÚpage2£¬²¢ÇÒpage3->slab=3+2=5)
    */
        if (page->slab >= pages) {

            //ÀıÈç×Ü¹²ÓĞ6¸öpage[]£¬ngx_slab_initÖĞpage[0]µÄnextºÍprev¶¼Ö¸Ïòfree£¬freeµÄnextÒ²Ö¸Ïòpage[0]¡£µ±µ÷ÓÃngx_slab_alloc_pagesÏò»ñÈ¡3¸öpagesµÄÊ±ºò
            //ÔòÇ°Èı¸öpages(page[0], page[1], page[2])»á±»·ÖÅäºÃ£¬×îÄ©Î²page[5]µÄprevÖ¸Ïòpage[3],²¢ÇÒpage[3]µÄslabÖ¸¶¨ÏÖÔÚÖ»ÓĞ6-3=3¸öpage¿ÉÒÔ·ÖÅäÁË£¬
            //È»ºópage[3]µÄnextºÍprevÖ¸Ïòfree,freeµÄnextºÍprevÒ²Ö¸Ïòpage[3]£¬Ò²¾ÍÊÇÏÂ´ÎÖ»ÄÜ´Ópage[3]¿ªÊ¼»ñÈ¡Ò³
            if (page->slab > pages) {  
                page[page->slab - 1].prev = (uintptr_t) &page[pages];

                //¸üĞÂ´Ó±¾¸öslab page¿ªÊ¼ÍùÏÂµÚpages¸öslab pageµÄ»º´æÒ³ÊıÄ¿Îª±¾¸öslab pageÊıÄ¿¼õÈ¥pages  
                //ÈÃÏÂ´Î¿ÉÒÔ´Ópage[pages]¿ªÊ¼·ÖÅäµÄÒ³µÄnextºÍprevÖ¸Ïòpool->free,Ö»ÒªÒ³µÄnextºÍprevÖ¸ÏòÁËfree£¬Ôò±íÊ¾¿ÉÒÔ´Ó¸ÃÒ³¿ªÊ¼·ÖÅäÒ³page
                page[pages].slab = page->slab - pages; //ÏÂ´Î¿ªÊ¼´ÓÕâ¸öpage[]´¦¿ªÊ¼»ñÈ¡Ò³£¬²¢ÇÒÏÖÔÚ¿ÉÓÃpageÖ»ÓĞpage->slab - pages¶àÁË
                page[pages].next = page->next; //¸Ã¿ÉÓÃÒ³µÄnextÖ¸Ïòpool->free.next
                page[pages].prev = page->prev; //¸Ã¿ÉÓÃÒ³µÄprevÖ¸Ïòpool->free.next

                p = (ngx_slab_page_t *) page->prev;
                p->next = &page[pages];//¸üĞÂpool->free.next = &page[pages]£¬ÏÂ´Î´ÓµÚpages¸öslab page¿ªÊ¼½øĞĞÉÏÃæµÄfor()Ñ­»·±éÀú
                page->next->prev = (uintptr_t) &page[pages]; //poll->free->prev = &page[pages]Ö¸ÏòÏÂ´Î¿ÉÒÔ·ÖÅäÒ³µÄÒ³µØÖ·

            } else { //pageÒ³²»¹»ÓÃÁË£¬ÔòfreeµÄnextºÍprev¶¼Ö¸Ïò×Ô¼º£¬ËùÒÔÏÂ´ÎÔÙ½øÈë¸Ãº¯Êı½øÈëfor()Ñ­»·µÄÊ±ºòÎŞ·¨½øÈëÑ­»·ÌåÖĞ£¬Ò²·ÖÅä²»µ½page
                //Èç¹ûfreeÖ¸ÏòµÄpageÒ³¿ÉÓÃÒ³´óĞ¡Îª2£¬µ¥¸Ãº¯ÊıÒªÇó»ñÈ¡3¸öÒ³£¬ÔòÖ±½Ó°ÑÕâÁ½¸öÒ³·µ»Ø³öÈ¥£¬¾ÍÊÇËµÕâÁ½¸öÒ³Äã¿ÉÒÔÏÈÓÃ×Å£¬×Ü±ÈÃ»ÓĞºÃ¡£
                p = (ngx_slab_page_t *) page->prev; //»ñÈ¡poll->free
                p->next = page->next; //poll->free->next = poll->free
                page->next->prev = page->prev; ////poll->free->prev = poll->free   freeµÄnextºÍprev¶¼Ö¸ÏòÁË×Ô¼º£¬ËµÃ÷Ã»ÓĞ¶àÓà¿Õ¼ä·ÖÅäÁË
            }

            //NGX_SLAB_PAGE_START±ê¼ÇpageÊÇ·ÖÅäµÄpages¸öÒ³µÄµÚÒ»¸öÒ³£¬²¢ÔÚµÚÒ»¸öÒ³pageÖĞ¼ÇÂ¼³öÆäºóÁ¬ĞøµÄpages¸öÒ³ÊÇÒ»Æğ·ÖÅäµÄ
            page->slab = pages | NGX_SLAB_PAGE_START; //¸üĞÂ±»·ÖÅäµÄpage slabÖĞµÄµÚÒ»¸öµÄslab³ÉÔ±£¬¼´Ò³µÄ¸öÊıºÍÕ¼ÓÃÇé¿ö  
            //pageµÄnextºÍprev¶¼Ïàµ±ÓÚÖ¸ÏòÁËNULLÁË£¬
            page->next = NULL; 
            page->prev = NGX_SLAB_PAGE; //pageÒ³Ãæ²»»®·ÖslotÊ±ºò,¼´½«Õû¸öÒ³Ãæ·ÖÅä¸øÓÃ»§,preµÄºóÁ½Î»ÎªNGX_SLAB_PAGE

            if (--pages == 0) { //pagesÎª1¡£ÔòÖ±½Ó·µ»Ø¸Ãpage
                return page;
            }

            for (p = page + 1; pages; pages--) {
                //Èç¹û·ÖÅäµÄÒ³Êıpages>1£¬¸üĞÂºóÃæpage slabµÄslab³ÉÔ±ÎªNGX_SLAB_PAGE_BUSY  
                p->slab = NGX_SLAB_PAGE_BUSY; 
                //±ê¼ÇÕâÊÇÁ¬Ğø·ÖÅä¶à¸öpage£¬²¢ÇÒÎÒ²»ÊÇÊ×page£¬ÀıÈçÒ»´Î·ÖÅä3¸öpage,·ÖÅäµÄpageÎª[1-3]£¬Ôòpage[1].slab=3  page[2].slab=page[3].slab=NGX_SLAB_PAGE_BUSY¼ÇÂ¼
                p->next = NULL;
                p->prev = NGX_SLAB_PAGE;
                p++;
            }

            return page;
        }
    }

    if (pool->log_nomem) {
        ngx_slab_error(pool, NGX_LOG_CRIT,
                       "ngx_slab_alloc() failed: no memory");
    }

    //Ã»ÓĞÕÒµ½¿ÕÓàµÄÒ³  
    return NULL;
}

//ÊÍ·ÅpageÒ³¿ªÊ¼µÄpages¸öÒ³Ãæ
static void
ngx_slab_free_pages(ngx_slab_pool_t *pool, ngx_slab_page_t *page,
    ngx_uint_t pages) //ÊÍ·Åpages¸öÒ³Ãæ,²¢½«Ò³Ãæ·ÅÈëfreeÖĞ
{
    ngx_uint_t        type;
    ngx_slab_page_t  *prev, *join;

    page->slab = pages--; //ÊÍ·ÅµÄpagesÒ³Êı-1£¬ÕâÊÇÒª¸ÉÂï?

    if (pages) {
        ngx_memzero(&page[1], pages * sizeof(ngx_slab_page_t)); //Èç¹ûÒ»´ÎĞÔ·ÖÅäÁË´óÓÚµÈÓÚ2µÄpage£¬ÔòĞèÒª°ÑÊ×pageºóµÄÆäËûpage»Ö¸´Çå0²Ù×÷
    }

    if (page->next) { 
        //½â³ıslot[]ºÍpage[]µÄ¹ØÁª£¬ÈÃslot[]µÄnextºÍprevÖ¸Ïòslot[]×ÔÉí
        prev = (ngx_slab_page_t *) (page->prev & ~NGX_SLAB_PAGE_MASK);
        prev->next = page->next;
        page->next->prev = page->prev;
    }
    
    //ÕâÊÇÒªÊÍ·ÅµÄÒ³pagesµÄ×îºóÒ»¸öpage£¬ÓĞ¿ÉÄÜÒ»´ÎÊÍ·Å3¸öÒ³£¬Ôòjoin´ú±íµÚÈı¸öÒ³µÄÆğÊ¼µØÖ·,Èç¹ûpagesÎª1£¬ÔòjoinÖ±½ÓÖ¸Ïò·ÖÅäµÄpage
    join = page + page->slab; 

    if (join < pool->last) { //join²»ÊÇpoolÖĞ×îºóÒ»¸öpage 
        type = join->prev & NGX_SLAB_PAGE_MASK;
        
        if (type == NGX_SLAB_PAGE) {//ºÍngx_slab_alloc_pagesÅäºÏÔÄ¶Á£¬
            
            if (join->next != NULL) { //¸ÃifÓ¦¸ÃÊ¼ÖÕÂú×ã²»ÁË???/  ÔÚallocµÄÊ±ºòÊ×Ò³pageµÄnextÊÇÖ¸ÏòNULL
                pages += join->slab;
                page->slab += join->slab;

                prev = (ngx_slab_page_t *) (join->prev & ~NGX_SLAB_PAGE_MASK);
                prev->next = join->next;
                join->next->prev = join->prev;

                join->slab = NGX_SLAB_PAGE_FREE;
                join->next = NULL;
                join->prev = NGX_SLAB_PAGE;
            }
        }
    }

    if (page > pool->pages) {
        join = page - 1;
        type = join->prev & NGX_SLAB_PAGE_MASK;

        if (type == NGX_SLAB_PAGE) {

            if (join->slab == NGX_SLAB_PAGE_FREE) {
                join = (ngx_slab_page_t *) (join->prev & ~NGX_SLAB_PAGE_MASK);
            }

            if (join->next != NULL) {
                pages += join->slab;
                join->slab += page->slab;

                prev = (ngx_slab_page_t *) (join->prev & ~NGX_SLAB_PAGE_MASK);
                prev->next = join->next;
                join->next->prev = join->prev;

                page->slab = NGX_SLAB_PAGE_FREE;
                page->next = NULL;
                page->prev = NGX_SLAB_PAGE;

                page = join;
            }
        }
    }

    if (pages) { //ÀıÈçÒ»´Îalloc 3¸öpage£¬·ÖÅäÊÇpage[3 4 5],Ôòpage[5]µÄprevÖ¸Ïòpage[3]
        page[pages].prev = (uintptr_t) page;
    }

    /*
    freepage[i]Îª±¾´ÎÊÍ·ÅµÄfreepage[i]¿ªÊ¼µÄpages¸öpageÒ³
    unusepage[i]Îª»¹Î´·ÖÅäÊ¹ÓÃµÄÔ­Ê¼pageÒ³¿ªÊ¼´¦£¬¿ÉÄÜ»áÓĞ¶à¸öpage[]½ô¸úÆäºó¡£
                          pool->free  
                        ----------  /                 
     -----------------\|         | --------------------------------------  
    |  ----------------|         | -----------------------------------   |
    |  |                 -----------                                  |  |
    |  |                                                              |  |
    |  |     freepage[i]                           unusepage[i]       |  |
    |  |  \ ----------                          \  -----------   /    |  |
    |   --- |    |    |----------------------------|    |     | -------  | 
    ------- |    |    |--------------------------- |    |     | ---------- 
            ----------  \                           ---------

    ×ßµ½ÕâÀïºó£¬pool->freeÖ¸ÏòÁË¸ÃÊÍ·ÅµÄpage[]£¬×¢ÒâÕâÊ±ºòÊ×page->slab¾ÍÊÇ

   
     */
    

    //ºÍngx_slab_alloc_pagesÅäºÏÔÄ¶Á£¬ ÕâÀïÊÍ·Åºó¾Í»á°ÑÖ®Ç°freeÖ¸ÏòµÄ¿ÉÓÃpageÒ³ÓëÊÍ·ÅµÄpageÒ³ÒÔ¼°pool->freeĞÎ³ÉÒ»¸ö»·ĞÎÁ´±í
    page->prev = (uintptr_t) &pool->free;
    page->next = pool->free.next;

    page->next->prev = (uintptr_t) page;

    pool->free.next = page;
}


static void
ngx_slab_error(ngx_slab_pool_t *pool, ngx_uint_t level, char *text)
{
    ngx_log_error(level, ngx_cycle->log, 0, "%s%s", text, pool->log_ctx);
}
