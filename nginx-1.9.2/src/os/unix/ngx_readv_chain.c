
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


ssize_t
ngx_readv_chain(ngx_connection_t *c, ngx_chain_t *chain, off_t limit)
{//Õâ¸öº¯ÊıÓÃreadv½«½«Á¬½ÓµÄÊı¾İ¶ÁÈ¡·Åµ½chainµÄÁ´±íÀïÃæ£¬Èç¹ûÓĞ´í±ê¼Çerror»òÕßeof¡£
//·µ»Ø¶ÁÈ¡µ½µÄ×Ö½ÚÊı¡
    u_char        *prev;
    ssize_t        n, size;
    ngx_err_t      err;
    ngx_array_t    vec;
    ngx_event_t   *rev;
    struct iovec  *iov, iovs[NGX_IOVS_PREALLOCATE];//16¸ö¿é

    rev = c->read;

#if (NGX_HAVE_KQUEUE)

    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {
        ngx_log_debug3(NGX_LOG_DEBUG_EVENT, c->log, 0,
                       "readv: eof:%d, avail:%d, err:%d",
                       rev->pending_eof, rev->available, rev->kq_errno);

        if (rev->available == 0) {
            if (rev->pending_eof) {
                rev->ready = 0;
                rev->eof = 1;

                ngx_log_error(NGX_LOG_INFO, c->log, rev->kq_errno,
                              "kevent() reported about an closed connection");

                if (rev->kq_errno) {
                    rev->error = 1;
                    ngx_set_socket_errno(rev->kq_errno);
                    return NGX_ERROR;
                }

                return 0;

            } else {
                return NGX_AGAIN;
            }
        }
    }

#endif

    prev = NULL;
    iov = NULL;
    size = 0;

    vec.elts = iovs; //vecÊı×éÖĞ°üÀ¨NGX_IOVS_PREALLOCATE¸östruct iovec½á¹¹
    vec.nelts = 0;
    vec.size = sizeof(struct iovec);
    vec.nalloc = NGX_IOVS_PREALLOCATE;
    vec.pool = c->pool;

    /* coalesce the neighbouring bufs */

    while (chain) {//±éÀúchain»º³åÁ´±í£¬²»¶ÏµÄÉêÇëstruct iovec½á¹¹Îª´ı»áµÄreadv×ö×¼±¸£¬Åöµ½ÁÙ½ü2¿éÄÚ´æÈç¹ûÕıºÃ½ÓÔÚÒ»Æğ£¬¾Í¹«ÓÃÖ®¡£
        n = chain->buf->end - chain->buf->last; //¸Ãchain->bufÖĞ¿ÉÒÔÊ¹ÓÃµÄÄÚ´æÓĞÕâÃ´¶à

        if (limit) {
            if (size >= limit) {
                break;
            }

            if (size + n > limit) {
                n = (ssize_t) (limit - size);
            }
        }

        if (prev == chain->buf->last) { //ËµÃ÷Ç°ÃæÒ»¸öchainµÄendºóºÍÃæÒ»¸öchainµÄlast¸ÕºÃÏàµÈ£¬Ò²¾ÍÊÇÕâÁ½¸öchainÄÚ´æÊÇÁ¬ĞøµÄ ÁÙ½ü2¿éÄÚ´æÈç¹ûÕıºÃ½ÓÔÚÒ»Æğ£¬¾Í¹«ÓÃÖ®¡£
            iov->iov_len += n;

        } else {
            if (vec.nelts >= IOV_MAX) {
                break;
            }

            iov = ngx_array_push(&vec);
            if (iov == NULL) {
                return NGX_ERROR;
            }

            //Ö¸ÏòÕâ¿éÄÚ´æÆğÊ¼Î»ÖÃ£¬ÆäÊµÖ®Ç°¿ÉÄÜ»¹ÓĞÊı¾İ£¬×¢ÒâÕâ²»ÊÇÄÚ´æ¿éµÄ¿ªÊ¼£¬¶øÊÇÊı¾İµÄÄ©Î²¡£ÓĞÊı¾İÊÇÒòÎªÉÏ´ÎÃ»ÓĞÌîÂúÒ»¿éÄÚ´æ¿éµÄÊı¾İ¡£
            iov->iov_base = (void *) chain->buf->last;
            iov->iov_len = n;//¸³ÖµÕâ¿éÄÚ´æµÄ×î´ó´óĞ¡¡£
        }

        size += n;
        prev = chain->buf->end;
        chain = chain->next;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_EVENT, c->log, 0,
                   "readv: %d, last(iov_len):%d", vec.nelts, iov->iov_len);

    do {
        //readÏµÁĞº¯Êı·µ»Ø0±íÊ¾¶Ô¶Ë·¢ËÍÁËFIN°ü
		//If any portion of a regular file prior to the end-of-file has not been written, read() shall return bytes with value 0.
		//Èç¹ûÊÇÃ»ÓĞÊı¾İ¿É¶ÁÁË£¬»á·µ»Ø-1£¬È»ºóerrnoÎªEAGAIN±íÊ¾ÔİÊ±Ã»ÓĞÊı¾İ¡£
		//´ÓÉÏÃæ¿ÉÒÔ¿´³öreadv¿ÉÒÔ½«¶Ô¶ËµÄÊı¾İ¶ÁÈëµ½±¾¶ËµÄ¼¸¸ö²»Á¬ĞøµÄÄÚ´æÖĞ£¬¶øreadÔòÖ»ÄÜ¶ÁÈëµ½Á¬ĞøµÄÄÚ´æÖĞ
        /* On success, the readv() function returns the number of bytes read; the writev() function returns the number of bytes written.  
        On error, -1 is returned, and errno is  set appropriately. readv·µ»Ø±»¶ÁµÄ×Ö½Ú×ÜÊı¡£Èç¹ûÃ»ÓĞ¸ü¶àÊı¾İºÍÅöµ½ÎÄ¼şÄ©Î²Ê±·µ»Ø0µÄ¼ÆÊı¡£ */
        n = readv(c->fd, (struct iovec *) vec.elts, vec.nelts);

        if (n >= 0) {

#if (NGX_HAVE_KQUEUE)

            if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {
                rev->available -= n;

                /*
                 * rev->available may be negative here because some additional
                 * bytes may be received between kevent() and recv()
                 */

                if (rev->available <= 0) {
                    if (!rev->pending_eof) {
                        rev->ready = 0;
                    }

                    if (rev->available < 0) {
                        rev->available = 0;
                    }
                }

                if (n == 0) {//readv·µ»Ø0±íÊ¾¶Ô¶ËÒÑ¾­¹Ø±ÕÁ¬½Ó£¬Ã»ÓĞÊı¾İÁË¡£

                    /*
                     * on FreeBSD recv() may return 0 on closed socket
                     * even if kqueue reported about available data
                     */

#if 0
                    ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                                  "readv() returned 0 while kevent() reported "
                                  "%d available bytes", rev->available);
#endif

                    rev->ready = 0;
                    rev->eof = 1;
                    rev->available = 0;
                }

                return n;
            }

#endif /* NGX_HAVE_KQUEUE */

            if (n < size && !(ngx_event_flags & NGX_USE_GREEDY_EVENT)) {
                rev->ready = 0; //ËµÃ÷¶Ô¶Ë·¢ËÍ¹ıÀ´´æ´¢ÔÚ±¾¶ËÄÚºË»º³åÇøµÄÊı¾İÒÑ¾­¶ÁÍê£¬  epoll²»Âú×ãÕâ¸öifÌõ¼ş
            }

            if (n == 0) {//°´ÕÕreadv·µ»ØÖµ£¬Õâ¸öÓ¦¸Ã²»ÊÇ´íÎó£¬Ö»ÊÇ±íÊ¾Ã»Êı¾İÁË readv·µ»Ø±»¶ÁµÄ×Ö½Ú×ÜÊı¡£Èç¹ûÃ»ÓĞ¸ü¶àÊı¾İºÍÅöµ½ÎÄ¼şÄ©Î²Ê±·µ»Ø0µÄ¼ÆÊı¡£
                
                rev->eof = 1; //¸Ãº¯ÊıµÄÍâ²ãº¯Êı·¢ÏÖÊÇ0£¬ÔòÈÏÎªÊı¾İ¶ÁÈ¡Íê±Ï
            }

            return n; //¶ÔÓÚepollÀ´Ëµ£¬»¹ÊÇ¿É¶ÁµÄ£¬Ò²¾ÍÊÇreadvÎª1
        }

        //ËµÃ÷n<0   On error, -1 is returned, and errno is  set appropriately
    
        err = ngx_socket_errno;

        //readv·µ»Ø-1£¬Èç¹û²»ÊÇEAGAIN¾ÍÓĞÎÊÌâ¡£ ÀıÈçÄÚºË»º³åÇøÖĞÃ»ÓĞÊı¾İ£¬ÄãÒ²È¥readv£¬Ôò»á·µ»ØNGX_EAGAIN
        if (err == NGX_EAGAIN || err == NGX_EINTR) {
            ngx_log_debug0(NGX_LOG_DEBUG_EVENT, c->log, err,
                           "readv() not ready");
            n = NGX_AGAIN;

        } else {
            n = ngx_connection_error(c, err, "readv() failed");
            break;
        }

    } while (err == NGX_EINTR);

    rev->ready = 0;//²»¿É¶ÁÁË¡£

    if (n == NGX_ERROR) {
        c->read->error = 1;//Á¬½ÓÓĞ´íÎó·¢Éú¡£
    }

    return n;
}
