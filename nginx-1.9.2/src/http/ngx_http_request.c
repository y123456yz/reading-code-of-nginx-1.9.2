
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static void ngx_http_wait_request_handler(ngx_event_t *ev);
static void ngx_http_process_request_line(ngx_event_t *rev);
static void ngx_http_process_request_headers(ngx_event_t *rev);
static ssize_t ngx_http_read_request_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_alloc_large_header_buffer(ngx_http_request_t *r,
    ngx_uint_t request_line);

static ngx_int_t ngx_http_process_header_line(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_process_unique_header_line(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_process_multi_header_lines(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_process_host(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_process_connection(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);
static ngx_int_t ngx_http_process_user_agent(ngx_http_request_t *r,
    ngx_table_elt_t *h, ngx_uint_t offset);

static ngx_int_t ngx_http_validate_host(ngx_str_t *host, ngx_pool_t *pool,
    ngx_uint_t alloc);
static ngx_int_t ngx_http_set_virtual_server(ngx_http_request_t *r,
    ngx_str_t *host);
static ngx_int_t ngx_http_find_virtual_server(ngx_connection_t *c,
    ngx_http_virtual_names_t *virtual_names, ngx_str_t *host,
    ngx_http_request_t *r, ngx_http_core_srv_conf_t **cscfp);

static void ngx_http_request_handler(ngx_event_t *ev);
static void ngx_http_terminate_request(ngx_http_request_t *r, ngx_int_t rc);
static void ngx_http_terminate_handler(ngx_http_request_t *r);
static void ngx_http_finalize_connection(ngx_http_request_t *r);
static ngx_int_t ngx_http_set_write_handler(ngx_http_request_t *r);
static void ngx_http_writer(ngx_http_request_t *r);
static void ngx_http_request_finalizer(ngx_http_request_t *r);

static void ngx_http_set_keepalive(ngx_http_request_t *r);
static void ngx_http_keepalive_handler(ngx_event_t *ev);
static void ngx_http_set_lingering_close(ngx_http_request_t *r);
static void ngx_http_lingering_close_handler(ngx_event_t *ev);
static ngx_int_t ngx_http_post_action(ngx_http_request_t *r);
static void ngx_http_close_request(ngx_http_request_t *r, ngx_int_t error);
static void ngx_http_log_request(ngx_http_request_t *r);

static u_char *ngx_http_log_error(ngx_log_t *log, u_char *buf, size_t len);
static u_char *ngx_http_log_error_handler(ngx_http_request_t *r,
    ngx_http_request_t *sr, u_char *buf, size_t len);

#if (NGX_HTTP_SSL)
static void ngx_http_ssl_handshake(ngx_event_t *rev);
static void ngx_http_ssl_handshake_handler(ngx_connection_t *c);
#endif


static char *ngx_http_client_errors[] = {

    /* NGX_HTTP_PARSE_INVALID_METHOD */
    "client sent invalid method",

    /* NGX_HTTP_PARSE_INVALID_REQUEST */
    "client sent invalid request",

    /* NGX_HTTP_PARSE_INVALID_09_METHOD */
    "client sent invalid method in HTTP/0.9 request"
};

//∞—ngx_http_headers_in÷–µƒÀ˘”–≥…‘±◊ˆhash‘ÀÀ„£¨»ª∫Û¥Ê∑≈µΩcmcf->headers_in_hash÷–£¨º˚ngx_http_init_headers_in_hash
ngx_http_header_t  ngx_http_headers_in[] = {  
//Õ®π˝ngx_http_variable_header∫Ø ˝ªÒ»°ngx_http_core_variables÷–œ‡πÿ±‰¡øµƒ÷µ£¨’‚–©÷µµƒ¿¥‘¥æÕ «ngx_http_headers_in÷–µƒhanderΩ‚ŒˆµƒøÕªß∂À«Î«ÛÕ∑≤ø

    { ngx_string("Host"), offsetof(ngx_http_headers_in_t, host),
                 ngx_http_process_host }, //ngx_http_process_request_headers÷–÷¥––handler

    { ngx_string("Connection"), offsetof(ngx_http_headers_in_t, connection),
                 ngx_http_process_connection },

    { ngx_string("If-Modified-Since"),
                 offsetof(ngx_http_headers_in_t, if_modified_since),
                 ngx_http_process_unique_header_line },

    { ngx_string("If-Unmodified-Since"),
                 offsetof(ngx_http_headers_in_t, if_unmodified_since),
                 ngx_http_process_unique_header_line },

    { ngx_string("If-Match"),
                 offsetof(ngx_http_headers_in_t, if_match),
                 ngx_http_process_unique_header_line },

    { ngx_string("If-None-Match"),
                 offsetof(ngx_http_headers_in_t, if_none_match),
                 ngx_http_process_unique_header_line },

    { ngx_string("User-Agent"), offsetof(ngx_http_headers_in_t, user_agent),
                 ngx_http_process_user_agent },

    { ngx_string("Referer"), offsetof(ngx_http_headers_in_t, referer),
                 ngx_http_process_header_line },

    { ngx_string("Content-Length"),
                 offsetof(ngx_http_headers_in_t, content_length),
                 ngx_http_process_unique_header_line },

    { ngx_string("Content-Type"),
                 offsetof(ngx_http_headers_in_t, content_type),
                 ngx_http_process_header_line },

/*
 µœ÷∂œµ„–¯¥´π¶ƒ‹µƒœ¬‘ÿ 

“ª. ¡Ω∏ˆ±ÿ“™œÏ”¶Õ∑Accept-Ranges°¢ETag
        øÕªß∂À√ø¥ŒÃ·Ωªœ¬‘ÿ«Î«Û ±£¨∑˛ŒÒ∂À∂º“™ÃÌº”’‚¡Ω∏ˆœÏ”¶Õ∑£¨“‘±£÷§øÕªß∂À∫Õ∑˛ŒÒ∂ÀΩ´¥Àœ¬‘ÿ ∂±Œ™ø…“‘∂œµ„–¯¥´µƒœ¬‘ÿ£∫
Accept-Ranges£∫∏Ê÷™œ¬‘ÿøÕªß∂À’‚ «“ª∏ˆø…“‘ª÷∏¥–¯¥´µƒœ¬‘ÿ£¨¥Ê∑≈±æ¥Œœ¬‘ÿµƒø™ º◊÷Ω⁄Œª÷√°¢Œƒº˛µƒ◊÷Ω⁄¥Û–°£ª
ETag£∫±£¥ÊŒƒº˛µƒŒ®“ª±Í ∂£®Œ“‘⁄”√µƒŒƒº˛√˚+Œƒº˛◊Ó∫Û–ﬁ∏ƒ ±º‰£¨“‘±„–¯¥´«Î«Û ±∂‘Œƒº˛Ω¯––—È÷§£©£ª
≤Œøºhttp://www.cnblogs.com/diyunpeng/archive/2011/12/29/2305702.html
*/
    { ngx_string("Range"), offsetof(ngx_http_headers_in_t, range),
                 ngx_http_process_header_line },

    { ngx_string("If-Range"),
                 offsetof(ngx_http_headers_in_t, if_range),
                 ngx_http_process_unique_header_line },

    { ngx_string("Transfer-Encoding"),
                 offsetof(ngx_http_headers_in_t, transfer_encoding),
                 ngx_http_process_header_line },

    { ngx_string("Expect"),
                 offsetof(ngx_http_headers_in_t, expect),
                 ngx_http_process_unique_header_line },

    { ngx_string("Upgrade"),
                 offsetof(ngx_http_headers_in_t, upgrade),
                 ngx_http_process_header_line },

#if (NGX_HTTP_GZIP)
    { ngx_string("Accept-Encoding"),
                 offsetof(ngx_http_headers_in_t, accept_encoding),
                 ngx_http_process_header_line },

    { ngx_string("Via"), offsetof(ngx_http_headers_in_t, via),
                 ngx_http_process_header_line },
#endif

    { ngx_string("Authorization"),
                 offsetof(ngx_http_headers_in_t, authorization),
                 ngx_http_process_unique_header_line },

    { ngx_string("Keep-Alive"), offsetof(ngx_http_headers_in_t, keep_alive),
                 ngx_http_process_header_line },

#if (NGX_HTTP_X_FORWARDED_FOR)
    { ngx_string("X-Forwarded-For"),
                 offsetof(ngx_http_headers_in_t, x_forwarded_for),
                 ngx_http_process_multi_header_lines },
#endif

#if (NGX_HTTP_REALIP)
    { ngx_string("X-Real-IP"),
                 offsetof(ngx_http_headers_in_t, x_real_ip),
                 ngx_http_process_header_line },
#endif

#if (NGX_HTTP_HEADERS)
    { ngx_string("Accept"), offsetof(ngx_http_headers_in_t, accept),
                 ngx_http_process_header_line },

    { ngx_string("Accept-Language"),
                 offsetof(ngx_http_headers_in_t, accept_language),
                 ngx_http_process_header_line },
#endif

#if (NGX_HTTP_DAV)
    { ngx_string("Depth"), offsetof(ngx_http_headers_in_t, depth),
                 ngx_http_process_header_line },

    { ngx_string("Destination"), offsetof(ngx_http_headers_in_t, destination),
                 ngx_http_process_header_line },

    { ngx_string("Overwrite"), offsetof(ngx_http_headers_in_t, overwrite),
                 ngx_http_process_header_line },

    { ngx_string("Date"), offsetof(ngx_http_headers_in_t, date),
                 ngx_http_process_header_line },
#endif

    { ngx_string("Cookie"), offsetof(ngx_http_headers_in_t, cookies),
                 ngx_http_process_multi_header_lines },

    { ngx_null_string, 0, NULL }
};

//…Ë÷√ngx_listening_tµƒhandler£¨’‚∏ˆhandlerª·‘⁄º‡Ã˝µΩøÕªß∂À¡¨Ω” ±±ªµ˜”√£¨æﬂÃÂæÕ «‘⁄ngx_event_accept∫Ø ˝÷–£¨ngx_http_init_connection∫Ø ˝πÀ√˚Àº“Â£¨æÕ «≥ı ºªØ’‚∏ˆ–¬Ω®µƒ¡¨Ω”
void
ngx_http_init_connection(ngx_connection_t *c) 
//µ±Ω®¡¢¡¨Ω”∫Ûø™±Ÿngx_http_connection_tΩ·ππ£¨’‚¿Ô√Ê¥Ê¥¢∏√∑˛ŒÒ∆˜∂Àip:portÀ˘‘⁄server{}…œœ¬Œƒ≈‰÷√–≈œ¢£¨∫Õserver_name–≈œ¢µ»£¨»ª∫Û»√
//ngx_connection_t->data÷∏œÚ∏√Ω·ππ£¨’‚—˘æÕø…“‘Õ®π˝ngx_connection_t->dataªÒ»°µΩ∑˛ŒÒ∆˜∂Àµƒserv loc µ»≈‰÷√–≈œ¢“‘º∞∏√server{}÷–µƒserver_name–≈œ¢

{
    ngx_uint_t              i;
    ngx_event_t            *rev;
    struct sockaddr_in     *sin;
    ngx_http_port_t        *port;
    ngx_http_in_addr_t     *addr;
    ngx_http_log_ctx_t     *ctx;
    ngx_http_connection_t  *hc;
#if (NGX_HAVE_INET6)
    struct sockaddr_in6    *sin6;
    ngx_http_in6_addr_t    *addr6;
#endif

    //◊¢“‚ngx_connection_t∫Õngx_http_connection_tµƒ«¯±£¨«∞’ﬂ «Ω®¡¢¡¨Ω”accept«∞ π”√µƒΩ·ππ£¨∫Û’ﬂ «¡¨Ω”≥…π¶∫Û π”√µƒΩ·ππ
    hc = ngx_pcalloc(c->pool, sizeof(ngx_http_connection_t));
    if (hc == NULL) {
        ngx_http_close_connection(c);
        return;
    }

    //‘⁄∑˛ŒÒ∆˜∂ÀacceptøÕªß∂À¡¨Ω”≥…π¶(ngx_event_accept)∫Û£¨ª·Õ®π˝ngx_get_connection¥”¡¨Ω”≥ÿªÒ»°“ª∏ˆngx_connection_tΩ·ππ£¨“≤æÕ «√ø∏ˆøÕªß∂À¡¨Ω”∂‘”⁄“ª∏ˆngx_connection_tΩ·ππ£¨
    //≤¢«“Œ™∆‰∑÷≈‰“ª∏ˆngx_http_connection_tΩ·ππ£¨ngx_connection_t->data = ngx_http_connection_t£¨º˚ngx_http_init_connection
    c->data = hc;

    /* find the server configuration for the address:port */

    port = c->listening->servers;  

    if (port->naddrs > 1) {  
    
        /*
         * there are several addresses on this port and one of them
         * is an "*:port" wildcard so getsockname() in ngx_http_server_addr()
         * is required to determine a server address
         */
        //Àµ√˜listen ip:port¥Ê‘⁄º∏Ãı√ª”–bind—°œÓ£¨≤¢«“¥Ê‘⁄Õ®≈‰∑˚≈‰÷√£¨»Álisten *:port,ƒ«√¥æÕ–Ë“™Õ®π˝ngx_connection_local_sockaddr¿¥»∑∂®
    //æøæπøÕªß∂À «∫Õƒ«∏ˆ±æµÿipµÿ÷∑Ω®¡¢µƒ¡¨Ω”
        if (ngx_connection_local_sockaddr(c, NULL, 0) != NGX_OK) { //
            ngx_http_close_connection(c);
            return;
        }

        switch (c->local_sockaddr->sa_family) {

#if (NGX_HAVE_INET6)
        case AF_INET6:
            sin6 = (struct sockaddr_in6 *) c->local_sockaddr;

            addr6 = port->addrs;

            /* the last address is "*" */

            for (i = 0; i < port->naddrs - 1; i++) {
                if (ngx_memcmp(&addr6[i].addr6, &sin6->sin6_addr, 16) == 0) {
                    break;
                }
            }

            hc->addr_conf = &addr6[i].conf;

            break;
#endif

        default: /* AF_INET */
            sin = (struct sockaddr_in *) c->local_sockaddr;

            addr = port->addrs; 

            /* the last address is "*" */
            //∏˘æ›…œ√Êµƒngx_connection_local_sockaddr∫Ø ˝ªÒ»°µΩøÕªß∂À¡¨Ω”µΩ±æµÿ£¨±æµÿIPµÿ÷∑ªÒ»°µΩ∫Û£¨±È¿˙ngx_http_port_t’“µΩ∂‘”¶
            //µƒIPµÿ÷∑∫Õ∂Àø⁄£¨»ª∫Û∏≥÷µ∏¯ngx_http_connection_t->addr_conf£¨’‚¿Ô√Ê¥Ê¥¢”–server_name≈‰÷√–≈œ¢“‘º∞∏√ip:port∂‘”¶µƒ…œœ¬Œƒ–≈œ¢
            for (i = 0; i < port->naddrs - 1; i++) {
                if (addr[i].addr == sin->sin_addr.s_addr) {
                    break;
                }
            }

          /*
                ’‚¿Ô“≤ÃÂœ÷¡À‘⁄ngx_http_init_connection÷–ªÒ»°http{}…œœ¬Œƒctx£¨»Áπ˚øÕªß∂À«Î«Û÷–¥¯”–host≤Œ ˝£¨‘Úª·ºÃ–¯‘⁄ngx_http_set_virtual_server
                ÷–÷ÿ–¬ªÒ»°∂‘”¶µƒserver{}∫Õlocation{}£¨»Áπ˚øÕªß∂À«Î«Û≤ª¥¯hostÕ∑≤ø––£¨‘Ú π”√ƒ¨»œµƒserver{},º˚ ngx_http_init_connection  
            */
            hc->addr_conf = &addr[i].conf;

            break;
        }

    } else {

        switch (c->local_sockaddr->sa_family) {

#if (NGX_HAVE_INET6)
        case AF_INET6:
            addr6 = port->addrs;
            hc->addr_conf = &addr6[0].conf;
            break;
#endif

        default: /* AF_INET */
            addr = port->addrs;
            hc->addr_conf = &addr[0].conf;
            break;
        }
    }

    /* the default server configuration for the address:port */
    //listen add:port∂‘”⁄µƒ server{}≈‰÷√øÈµƒ…œœ¬Œƒctx
    hc->conf_ctx = hc->addr_conf->default_server->ctx;

    ctx = ngx_palloc(c->pool, sizeof(ngx_http_log_ctx_t));
    if (ctx == NULL) {
        ngx_http_close_connection(c);
        return;
    }

    ctx->connection = c;
    ctx->request = NULL;
    ctx->current_request = NULL;

    c->log->connection = c->number;
    c->log->handler = ngx_http_log_error;
    c->log->data = ctx;
    c->log->action = "waiting for request";

    c->log_error = NGX_ERROR_INFO;

    rev = c->read;
    rev->handler = ngx_http_wait_request_handler;
    c->write->handler = ngx_http_empty_handler;

#if (NGX_HTTP_V2) 
    /* ’‚¿Ô∑≈‘⁄SSLµƒ«∞√Ê «£¨»Áπ˚√ª”–≈‰÷√SSL£¨‘Ú÷±Ω”≤ª”√Ω¯––SSL–≠…Ã∂¯Ω¯––HTTP2¥¶¿Ìngx_http_v2_init */
    if (hc->addr_conf->http2) {
        rev->handler = ngx_http_v2_init;
    }
#endif

#if (NGX_HTTP_SSL)
    {
    ngx_http_ssl_srv_conf_t  *sscf;

    sscf = ngx_http_get_module_srv_conf(hc->conf_ctx, ngx_http_ssl_module);

    if (sscf->enable || hc->addr_conf->ssl) {

        c->log->action = "SSL handshaking";

        if (hc->addr_conf->ssl && sscf->ssl.ctx == NULL) {
            ngx_log_error(NGX_LOG_ERR, c->log, 0,
                          "no \"ssl_certificate\" is defined "
                          "in server listening on SSL port");
            ngx_http_close_connection(c);
            return;
        }

        hc->ssl = 1;

        rev->handler = ngx_http_ssl_handshake;
    }
    }
#endif

    if (hc->addr_conf->proxy_protocol) {
        hc->proxy_protocol = 1;
        c->log->action = "reading PROXY protocol";
    }

    /*
     »Áπ˚–¬¡¨Ω”µƒ∂¡ ¬º˛ngx_event_tΩ·ππÃÂ÷–µƒ±Í÷æŒªreadyŒ™1£¨ µº …œ±Ì æ’‚∏ˆ¡¨Ω”∂‘”¶µƒÃ◊Ω”◊÷ª∫¥Ê…œ“—æ≠”–”√ªß∑¢¿¥µƒ ˝æ›£¨
     ’‚ ±æÕø…µ˜”√…œ√ÊÀµπ˝µƒngx_http_init_request∑Ω∑®¥¶¿Ì«Î«Û°£
     */
    //’‚¿Ô÷ªø…ƒ‹ «µ±listenµƒ ±∫ÚÃÌº”¡Àdefered≤Œ ˝≤¢«“ƒ⁄∫À÷ß≥÷£¨‘⁄ngx_event_acceptµƒ ±∫Ú≤≈ª·÷√1£¨≤≈ø…ƒ‹÷¥––œ¬√Êµƒif¿Ô√Êµƒƒ⁄»›£¨∑Ò‘Ú≤ªª·÷ª–Ëif¿Ô√Êµƒƒ⁄»›
    if (rev->ready) {
        /* the deferred accept(), iocp */
        if (ngx_use_accept_mutex) { //»Áπ˚ «≈‰÷√¡Àaccept_mutex£¨‘Ú∞—∏√rev->handler—”∫Û¥¶¿Ì£¨
        // µº …œ÷¥––µƒµÿ∑ΩŒ™ngx_process_events_and_timers÷–µƒngx_event_process_posted
            ngx_post_event(rev, &ngx_posted_events);
            return;
        }

        rev->handler(rev); //ngx_http_wait_request_handler
        return;
    }

/*
‘⁄”––©«Èøˆœ¬£¨µ±TCP¡¨Ω”Ω®¡¢≥…π¶ ±Õ¨ ±“≤≥ˆœ÷¡Àø…∂¡ ¬º˛£®¿˝»Á£¨‘⁄Ã◊Ω”◊÷listen≈‰÷√ ±…Ë÷√¡Àdeferred—°œÓ ±£¨ƒ⁄∫ÀΩˆ‘⁄Ã◊Ω”◊÷…œ»∑ µ ’µΩ«Î«Û ±≤≈ª·Õ®÷™epoll
µ˜∂» ¬º˛µƒªÿµ˜∑Ω∑®°£µ±»ª£¨‘⁄¥Û≤ø∑÷«Èøˆœ¬£¨ngx_http_init_request∑Ω∑®∫Õ
ngx_http_init_connection∑Ω∑®∂º «”…¡Ω∏ˆ ¬º˛£®TCP¡¨Ω”Ω®¡¢≥…π¶ ¬º˛∫Õ¡¨Ω”…œµƒø…∂¡ ¬º˛£©¥•∑¢µ˜”√µƒ
*/

/*
µ˜”√ngx_add_timer∑Ω∑®∞—∂¡ ¬º˛ÃÌº”µΩ∂® ±∆˜÷–£¨…Ë÷√µƒ≥¨ ± ±º‰‘Ú «nginx.conf÷–client_header_timeout≈‰÷√œÓ÷∏∂®µƒ≤Œ ˝°£
“≤æÕ «Àµ£¨»Áπ˚æ≠π˝client_header_timeout ±º‰∫Û’‚∏ˆ¡¨Ω”…œªπ√ª”–”√ªß ˝æ›µΩ¥Ô£¨‘Úª·”…∂® ±∆˜¥•∑¢µ˜”√∂¡ ¬º˛µƒngx_http_init_request¥¶¿Ì∑Ω∑®°£
 */
    ngx_add_timer(rev, c->listening->post_accept_timeout, NGX_FUNC_LINE); //∞—Ω” ’ ¬º˛ÃÌº”µΩ∂® ±∆˜÷–£¨µ±post_accept_timeout√Îªπ√ª”–øÕªß∂À ˝æ›µΩ¿¥£¨æÕπÿ±’¡¨Ω”
    ngx_reusable_connection(c, 1);
    
    if (ngx_handle_read_event(rev, 0, NGX_FUNC_LINE) != NGX_OK) { //µ±œ¬¥Œ”– ˝æ›¥”øÕªß∂À∑¢ÀÕπ˝¿¥µƒ ±∫Ú£¨ª·‘⁄ngx_epoll_process_events∞—∂‘”¶µƒready÷√1°£
        ngx_http_close_connection(c);
        return;
    }
}

//øÕªß∂ÀΩ®¡¢¡¨Ω”∫Û£¨÷ª”–µ⁄“ª¥Œ∂¡»°øÕªß∂À ˝æ›µΩ ˝æ›µƒ ±∫Ú£¨÷¥––µƒhandler÷∏œÚ∏√∫Ø ˝£¨“Ú¥Àµ±øÕªß∂À¡¨Ω”Ω®¡¢≥…π¶∫Û£¨÷ª”–µ⁄“ª¥Œ∂¡»°
//øÕªß∂À ˝æ›≤≈ª·◊ﬂ∏√∫Ø ˝£¨»Áπ˚‘⁄±£ªÓ∆⁄ƒ⁄”÷ ’µΩøÕªß∂À«Î«Û£¨‘Ú≤ªª·‘Ÿ◊ﬂ∏√∫Ø ˝£¨∂¯ «÷¥––ngx_http_process_request_line£¨“ÚŒ™∏√∫Ø ˝
//∞—handler÷∏œÚ¡Àngx_http_process_request_line
static void
ngx_http_wait_request_handler(ngx_event_t *rev)
{
    u_char                    *p;
    size_t                     size;
    ssize_t                    n;
    ngx_buf_t                 *b;
    ngx_connection_t          *c;
    ngx_http_connection_t     *hc;
    ngx_http_core_srv_conf_t  *cscf;

    c = rev->data;
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "http wait request handler");

    if (rev->timedout) { //»Áπ˚tcp¡¨Ω”Ω®¡¢∫Û£¨µ»¡Àclient_header_timeout√Î“ª÷±√ª”– ’µΩøÕªß∂Àµƒ ˝æ›∞¸π˝¿¥£¨‘Úπÿ±’¡¨Ω”
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "client timed out");
        ngx_http_close_connection(c);
        return;
    }

    if (c->close) {
        ngx_http_close_connection(c);
        return;
    }

    hc = c->data;
    cscf = ngx_http_get_module_srv_conf(hc->conf_ctx, ngx_http_core_module);

    size = cscf->client_header_buffer_size; //ƒ¨»œ1024

    b = c->buffer;

    if (b == NULL) {
        b = ngx_create_temp_buf(c->pool, size);
        if (b == NULL) {
            ngx_http_close_connection(c);
            return;
        }

        c->buffer = b;

    } else if (b->start == NULL) {

        b->start = ngx_palloc(c->pool, size);
        if (b->start == NULL) {
            ngx_http_close_connection(c);
            return;
        }

        b->pos = b->start;
        b->last = b->start;
        b->end = b->last + size;
    }

    //’‚¿Ô»Áπ˚“ª¥Œ√ª”–∞—À˘”–øÕªß∂Àµƒ ˝æ›∂¡»°ÕÍ£¨‘Ú‘⁄ngx_http_process_request_line÷–ª·ºÃ–¯∂¡»°
    //”Îngx_http_read_request_header≈‰∫œ∂¡
    n = c->recv(c, b->last, size);  //∂¡»°øÕªß∂À¿¥µƒ ˝æ›    ÷¥––ngx_unix_recv

    if (n == NGX_AGAIN) {
        if (!rev->timer_set) {
            ngx_add_timer(rev, c->listening->post_accept_timeout, NGX_FUNC_LINE);
            ngx_reusable_connection(c, 1);
        }

        if (ngx_handle_read_event(rev, 0, NGX_FUNC_LINE) != NGX_OK) {
            ngx_http_close_connection(c);
            return;
        }

        /*
         * We are trying to not hold c->buffer's memory for an idle connection.
         */

        if (ngx_pfree(c->pool, b->start) == NGX_OK) {
            b->start = NULL;
        }

        return;
    }

    if (n == NGX_ERROR) {
        ngx_http_close_connection(c);
        return;
    }

    if (n == 0) {
        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                      "client closed connection");
        ngx_http_close_connection(c);
        return;
    }

    b->last += n;

    if (hc->proxy_protocol) {
        hc->proxy_protocol = 0;

        p = ngx_proxy_protocol_read(c, b->pos, b->last);

        if (p == NULL) {
            ngx_http_close_connection(c);
            return;
        }

        b->pos = p;

        if (b->pos == b->last) {
            c->log->action = "waiting for request";
            b->pos = b->start;
            b->last = b->start;
            ngx_post_event(rev, &ngx_posted_events);
            return;
        }
    }

    c->log->action = "reading client request line";

    ngx_reusable_connection(c, 0);
    //¥”–¬»√c->data÷∏œÚ–¬ø™±Ÿµƒngx_http_request_t
    c->data = ngx_http_create_request(c);
    if (c->data == NULL) {
        ngx_http_close_connection(c);
        return;
    }

    rev->handler = ngx_http_process_request_line;
    ngx_http_process_request_line(rev);
}

//÷ª”–‘⁄¡¨Ω”Ω®¡¢≤¢Ω” ‹µΩøÕªß∂Àµ⁄“ª¥Œ«Î«Ûµƒ ±∫Ú≤≈ª·¥¥Ω®ngx_connection_t£¨∏√Ω·ππ“ª÷±≥÷–¯µΩ¡¨Ω”πÿ±’≤≈ Õ∑≈
ngx_http_request_t *
ngx_http_create_request(ngx_connection_t *c)
{
    ngx_pool_t                 *pool;
    ngx_time_t                 *tp;
    ngx_http_request_t         *r;
    ngx_http_log_ctx_t         *ctx;
    ngx_http_connection_t      *hc;
    ngx_http_core_srv_conf_t   *cscf;
    ngx_http_core_loc_conf_t   *clcf;
    ngx_http_core_main_conf_t  *cmcf;

    c->requests++;

    hc = c->data;
    //‘⁄ngx_http_wait_request_handlerµƒ ±∫Ú£¨data÷∏œÚngx_http_connection_t,∏√∫Ø ˝∑µªÿ∫Û c->data÷ÿ–¬÷∏œÚ–¬ø™≈Ãµƒngx_http_request_t
    //÷Æ«∞c->data÷∏œÚµƒngx_http_connection_t”√r->http_connection±£¥Ê

    cscf = ngx_http_get_module_srv_conf(hc->conf_ctx, ngx_http_core_module);

    pool = ngx_create_pool(cscf->request_pool_size, c->log);
    if (pool == NULL) {
        return NULL;
    }

    r = ngx_pcalloc(pool, sizeof(ngx_http_request_t));
    if (r == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    r->pool = pool; 

    //µ±¡¨Ω”Ω®¡¢≥…π¶∫Û£¨µ± ’µΩøÕªß∂Àµƒµ⁄“ª∏ˆ«Î«Ûµƒ ±∫Úª·Õ®π˝ngx_http_wait_request_handler->ngx_http_create_request¥¥Ω®ngx_http_request_t
    //Õ¨ ±∞—r->http_connection÷∏œÚacceptøÕªß∂À¡¨Ω”≥…π¶ ±∫Ú¥¥Ω®µƒngx_http_connection_t£¨’‚¿Ô√Ê”–¥Ê¥¢server{}…œœ¬Œƒctx∫Õserver_nameµ»–≈œ¢
    //∏√ngx_http_request_tª·“ª÷±”––ß£¨≥˝∑«πÿ±’¡¨Ω”°£“Ú¥À∏√∫Ø ˝÷ªª·µ˜”√“ª¥Œ£¨“≤æÕ «µ⁄“ª∏ˆøÕªß∂À«Î«Û±®Œƒπ˝¿¥µƒ ±∫Ú¥¥Ω®£¨“ª÷±≥÷–¯µΩ¡¨Ω”πÿ±’
    r->http_connection = hc; //÷ÿ–¬∞—c->data∏≥÷µ∏¯r->http_connection£¨’‚—˘r->http_connectionæÕ±£¥Ê¡Àngx_http_connection_t–≈œ¢
    r->signature = NGX_HTTP_MODULE;
    r->connection = c;

    r->main_conf = hc->conf_ctx->main_conf;
    r->srv_conf = hc->conf_ctx->srv_conf;
    r->loc_conf = hc->conf_ctx->loc_conf;

    r->read_event_handler = ngx_http_block_reading;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    ngx_set_connection_log(r->connection, clcf->error_log);

    r->header_in = hc->nbusy ? hc->busy[0] : c->buffer;

    if (ngx_list_init(&r->headers_out.headers, r->pool, 20,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        ngx_destroy_pool(r->pool);
        return NULL;
    }

    r->ctx = ngx_pcalloc(r->pool, sizeof(void *) * ngx_http_max_module);
    if (r->ctx == NULL) {
        ngx_destroy_pool(r->pool);
        return NULL;
    }

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    r->variables = ngx_pcalloc(r->pool, cmcf->variables.nelts
                                        * sizeof(ngx_http_variable_value_t));
    if (r->variables == NULL) {
        ngx_destroy_pool(r->pool);
        return NULL;
    }

#if (NGX_HTTP_SSL)
    if (c->ssl) {
        r->main_filter_need_in_memory = 1;
    }
#endif

    r->main = r;
    r->count = 1;

    tp = ngx_timeofday();
    //ngx_http_request_tΩ·ππÃÂ÷–”–¡Ω∏ˆ≥…‘±±Ì æ’‚∏ˆ«Î«Ûµƒø™ º¥¶¿Ì ±º‰£∫start_sec≥…‘±∫Õstart_msec≥…‘±
    r->start_sec = tp->sec;
    r->start_msec = tp->msec;

    r->method = NGX_HTTP_UNKNOWN;
    r->http_version = NGX_HTTP_VERSION_10;

    r->headers_in.content_length_n = -1;
    r->headers_in.keep_alive_n = -1;
    r->headers_out.content_length_n = -1;
    r->headers_out.last_modified_time = -1;

    r->uri_changes = NGX_HTTP_MAX_URI_CHANGES + 1;
    r->subrequests = NGX_HTTP_MAX_SUBREQUESTS + 1;

    r->http_state = NGX_HTTP_READING_REQUEST_STATE;

    ctx = c->log->data;
    ctx->request = r;
    ctx->current_request = r;
    r->log_handler = ngx_http_log_error_handler;

#if (NGX_STAT_STUB)
    (void) ngx_atomic_fetch_add(ngx_stat_reading, 1);
    r->stat_reading = 1;
    (void) ngx_atomic_fetch_add(ngx_stat_requests, 1);
#endif

    return r;
}


#if (NGX_HTTP_SSL)
/* »Áπ˚≈‰÷√¡À–Ë“™÷ß≥÷ssl–≠“È£¨‘Ú¡¨Ω”Ω®¡¢∫Ûª·µ˜”√∏√handler¥¶¿Ì∫Û–¯ssl–≠…Ãπ˝≥Ã */
static void
ngx_http_ssl_handshake(ngx_event_t *rev)
{
    u_char                   *p, buf[NGX_PROXY_PROTOCOL_MAX_HEADER + 1];
    size_t                    size;
    ssize_t                   n;
    ngx_err_t                 err;
    ngx_int_t                 rc;
    ngx_connection_t         *c;
    ngx_http_connection_t    *hc;
    ngx_http_ssl_srv_conf_t  *sscf;

    c = rev->data;
    hc = c->data;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0,
                   "http check ssl handshake");

    if (rev->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "client timed out");
        ngx_http_close_connection(c);
        return;
    }

    if (c->close) {
        ngx_http_close_connection(c);
        return;
    }

    size = hc->proxy_protocol ? sizeof(buf) : 1; //»Áπ˚≤ª «◊ˆproxy£¨‘Ú∂¡1◊÷Ω⁄≥ˆ¿¥£¨ø¥ « ≤√¥–≠“È

    /*
    MSG_PEEK±Í÷æª·Ω´Ã◊Ω”◊÷Ω” ’∂”¡–÷–µƒø…∂¡µƒ ˝æ›øΩ±¥µΩª∫≥Â«¯£¨µ´≤ªª· πÃ◊Ω”◊”Ω” ’∂”¡–÷–µƒ ˝æ›ºı…Ÿ£¨
    ≥£º˚µƒ «£∫¿˝»Áµ˜”√recvªÚread∫Û£¨µº÷¬Ã◊Ω”◊÷Ω” ’∂”¡–÷–µƒ ˝æ›±ª∂¡»°∫Û∂¯ºı…Ÿ£¨∂¯÷∏∂®¡ÀMSG_PEEK±Í÷æ£¨
    ø…Õ®π˝∑µªÿ÷µªÒµ√ø…∂¡ ˝æ›≥§∂»£¨≤¢«“≤ªª·ºı…ŸÃ◊Ω”◊÷Ω” ’ª∫≥Â«¯÷–µƒ ˝æ›£¨À˘“‘ø…“‘π©≥Ã–Úµƒ∆‰À˚≤ø∑÷ºÃ–¯∂¡»°°£
    */
    n = recv(c->fd, (char *) buf, size, MSG_PEEK); 

    err = ngx_socket_errno;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, rev->log, 0, "http recv(): %d", n);

    if (n == -1) {
        if (err == NGX_EAGAIN) {
            rev->ready = 0;

            if (!rev->timer_set) {
                ngx_add_timer(rev, c->listening->post_accept_timeout, NGX_FUNC_LINE);
                ngx_reusable_connection(c, 1);
            }

            if (ngx_handle_read_event(rev, 0, NGX_FUNC_LINE) != NGX_OK) {
                ngx_http_close_connection(c);
            }

            return;
        }

        ngx_connection_error(c, err, "recv() failed");
        ngx_http_close_connection(c);

        return;
    }

    if (hc->proxy_protocol) {
        hc->proxy_protocol = 0;

        p = ngx_proxy_protocol_read(c, buf, buf + n);

        if (p == NULL) {
            ngx_http_close_connection(c);
            return;
        }

        size = p - buf;

        if (c->recv(c, buf, size) != (ssize_t) size) {
            ngx_http_close_connection(c);
            return;
        }

        c->log->action = "SSL handshaking";

        if (n == (ssize_t) size) {
            ngx_post_event(rev, &ngx_posted_events);
            return;
        }

        n = 1;
        buf[0] = *p;
    }

    if (n == 1) {
        if (buf[0] & 0x80 /* SSLv2 */ || buf[0] == 0x16 /* SSLv3/TLSv1 */) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, rev->log, 0,
                           "https ssl handshake: 0x%02Xd", buf[0]);

            sscf = ngx_http_get_module_srv_conf(hc->conf_ctx,
                                                ngx_http_ssl_module);

            if (ngx_ssl_create_connection(&sscf->ssl, c, NGX_SSL_BUFFER)
                != NGX_OK)
            {
                ngx_http_close_connection(c);
                return;
            }

            rc = ngx_ssl_handshake(c);

            if (rc == NGX_AGAIN) {

                if (!rev->timer_set) {
                    ngx_add_timer(rev, c->listening->post_accept_timeout, NGX_FUNC_LINE);
                }

                ngx_reusable_connection(c, 0);

                //sslµ•œÚ»œ÷§Àƒ¥ŒŒ’ ÷ÕÍ≥…∫Û÷¥––∏√handler
                c->ssl->handler = ngx_http_ssl_handshake_handler;
                return;
            }

            ngx_http_ssl_handshake_handler(c);

            return;
        }

        //http ∆ΩÃ®µƒ«Î«Û£¨»Áπ˚ «http∆ΩÃ®µƒ«Î«Û£¨æÕ◊ﬂ“ª∞„¡˜≥Ã∑µªÿ¥ÌŒ“–≈œ¢
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0, "plain http");

        c->log->action = "waiting for request";

        rev->handler = ngx_http_wait_request_handler;
        ngx_http_wait_request_handler(rev);

        return;
    }

    ngx_log_error(NGX_LOG_INFO, c->log, 0, "client closed connection");
    ngx_http_close_connection(c);
}

//SSLªÚ’ﬂTLS–≠…Ã≥…π¶∫Û£¨ø™ º∂¡»°øÕªß∂À∞¸ÃÂ¡À
static void
ngx_http_ssl_handshake_handler(ngx_connection_t *c)
{
    if (c->ssl->handshaked) {

        /*
         * The majority of browsers do not send the "close notify" alert.
         * Among them are MSIE, old Mozilla, Netscape 4, Konqueror,
         * and Links.  And what is more, MSIE ignores the server's alert.
         *
         * Opera and recent Mozilla send the alert.
         */

        c->ssl->no_wait_shutdown = 1;

#if (NGX_HTTP_V2                                                              \
     && (defined TLSEXT_TYPE_application_layer_protocol_negotiation           \
         || defined TLSEXT_TYPE_next_proto_neg))
        {
        unsigned int          len;
        const unsigned char  *data;

#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
        SSL_get0_alpn_selected(c->ssl->connection, &data, &len);

#ifdef TLSEXT_TYPE_next_proto_neg
        if (len == 0) {
            SSL_get0_next_proto_negotiated(c->ssl->connection, &data, &len);
        }
#endif

#else /* TLSEXT_TYPE_next_proto_neg */
        SSL_get0_next_proto_negotiated(c->ssl->connection, &data, &len);
#endif

        if (len == 2 && data[0] == 'h' && data[1] == '2') {
            ngx_http_v2_init(c->read);
            return;
        }
        }
#endif

        c->log->action = "waiting for request";

        c->read->handler = ngx_http_wait_request_handler;
        /* STUB: epoll edge */ c->write->handler = ngx_http_empty_handler;

        ngx_reusable_connection(c, 1);

        ngx_http_wait_request_handler(c->read);

        return;
    }

    if (c->read->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "client timed out");
    }

    ngx_http_close_connection(c);
}

#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME

int
ngx_http_ssl_servername(ngx_ssl_conn_t *ssl_conn, int *ad, void *arg)
{
    ngx_str_t                  host;
    const char                *servername;
    ngx_connection_t          *c;
    ngx_http_connection_t     *hc;
    ngx_http_ssl_srv_conf_t   *sscf;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_core_srv_conf_t  *cscf;

    servername = SSL_get_servername(ssl_conn, TLSEXT_NAMETYPE_host_name);

    if (servername == NULL) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    c = ngx_ssl_get_connection(ssl_conn);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "SSL server name: \"%s\"", servername);

    host.len = ngx_strlen(servername);

    if (host.len == 0) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    host.data = (u_char *) servername;

    if (ngx_http_validate_host(&host, c->pool, 1) != NGX_OK) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    hc = c->data;

    if (ngx_http_find_virtual_server(c, hc->addr_conf->virtual_names, &host,
                                     NULL, &cscf)
        != NGX_OK)
    {
        return SSL_TLSEXT_ERR_NOACK;
    }

    hc->ssl_servername = ngx_palloc(c->pool, sizeof(ngx_str_t));
    if (hc->ssl_servername == NULL) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    *hc->ssl_servername = host;

    hc->conf_ctx = cscf->ctx;

    clcf = ngx_http_get_module_loc_conf(hc->conf_ctx, ngx_http_core_module);

    ngx_set_connection_log(c, clcf->error_log);

    sscf = ngx_http_get_module_srv_conf(hc->conf_ctx, ngx_http_ssl_module);

    if (sscf->ssl.ctx) {
        SSL_set_SSL_CTX(ssl_conn, sscf->ssl.ctx);

        /*
         * SSL_set_SSL_CTX() only changes certs as of 1.0.0d
         * adjust other things we care about
         */

        SSL_set_verify(ssl_conn, SSL_CTX_get_verify_mode(sscf->ssl.ctx),
                       SSL_CTX_get_verify_callback(sscf->ssl.ctx));

        SSL_set_verify_depth(ssl_conn, SSL_CTX_get_verify_depth(sscf->ssl.ctx));

#ifdef SSL_CTRL_CLEAR_OPTIONS
        /* only in 0.9.8m+ */
        SSL_clear_options(ssl_conn, SSL_get_options(ssl_conn) &
                                    ~SSL_CTX_get_options(sscf->ssl.ctx));
#endif

        SSL_set_options(ssl_conn, SSL_CTX_get_options(sscf->ssl.ctx));
    }

    return SSL_TLSEXT_ERR_OK;
}

#endif

#endif

/*
’‚—˘µƒ«Î«Û––≥§∂» «≤ª∂®µƒ£¨À¸”ÎURI≥§∂»œ‡πÿ£¨’‚“‚Œ∂◊≈‘⁄∂¡ ¬º˛±ª¥•∑¢ ±£¨ƒ⁄∫ÀÃ◊Ω”◊÷ª∫≥Â«¯µƒ¥Û–°Œ¥±ÿ◊„πªΩ” ’µΩ»´≤øµƒHTTP«Î«Û––£¨”…¥Àø…“‘µ√≥ˆΩ·¬€£∫
µ˜”√“ª¥Œngx_http_process_request_line∑Ω∑®≤ª“ª∂®ƒ‹πª◊ˆÕÍ’‚œÓπ§◊˜°£À˘“‘£¨ngx_http_process_request_line∑Ω∑®“≤ª·◊˜Œ™∂¡ ¬º˛µƒªÿµ˜∑Ω∑®£¨À¸ø…ƒ‹ª·±ª
epoll’‚∏ˆ ¬º˛«˝∂Øª˙÷∆∂‡¥Œµ˜∂»£¨∑¥∏¥µÿΩ” ’TCP¡˜≤¢ π”√◊¥Ã¨ª˙Ω‚ŒˆÀ¸√«£¨÷±µΩ»∑»œΩ” ’µΩ¡ÀÕÍ’˚µƒHTTP«Î«Û––£¨’‚∏ˆΩ◊∂Œ≤≈À„ÕÍ≥…£¨≤≈ª·Ω¯»Îœ¬“ª∏ˆΩ◊∂ŒΩ” ’HTTPÕ∑≤ø°£
*/
/*
‘⁄Ω” ’ÕÍHTTPÕ∑≤ø£¨µ⁄“ª¥Œ‘⁄“µŒÒ…œ¥¶¿ÌHTTP«Î«Û ±£¨HTTPøÚº‹Ã·π©µƒ¥¶¿Ì∑Ω∑® «ngx_http_process_request°£µ´»Áπ˚∏√∑Ω∑®Œﬁ∑®“ª¥Œ¥¶
¿ÌÕÍ∏√«Î«Ûµƒ»´≤ø“µŒÒ£¨‘⁄πÈªπøÿ÷∆»®µΩepoll ¬º˛ƒ£øÈ∫Û£¨∏√«Î«Û‘Ÿ¥Œ±ªªÿµ˜ ±£¨Ω´Õ®π˝ngx_http_request_handler∑Ω∑®¿¥¥¶¿Ì
*/
static void
ngx_http_process_request_line(ngx_event_t *rev) //ngx_http_process_request_line∑Ω∑®¿¥Ω” ’HTTP«Î«Û––
{
    ssize_t              n;
    ngx_int_t            rc, rv;
    ngx_str_t            host;
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

    c = rev->data;
    r = c->data;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0,
                   "http process request line");
    /*
     ºÏ≤È’‚∏ˆ∂¡ ¬º˛ «∑Ò“—æ≠≥¨ ±£¨≥¨ ± ±º‰»‘»ª «nginx.conf≈‰÷√Œƒº˛÷–÷∏∂®µƒclient_header_timeout°£»Áπ˚ngx_event_t ¬º˛µƒtimeout±Í÷æŒ™1£¨
     ‘Ú»œŒ™Ω” ’HTTP«Î«Û“—æ≠≥¨ ±£¨µ˜”√ngx_http_close_request∑Ω∑®πÿ±’«Î«Û£¨Õ¨ ±”…ngx_http_process_request_line∑Ω∑®÷–∑µªÿ°£
     */
    if (rev->timedout) {
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "client timed out");
        c->timedout = 1;
        ngx_http_close_request(r, NGX_HTTP_REQUEST_TIME_OUT);
        return;
    }

    rc = NGX_AGAIN;

//∂¡»°“ª–– ˝æ›£¨∑÷Œˆ≥ˆ«Î«Û––÷–∞¸∫¨µƒmethod°¢uri°¢http_version–≈œ¢°£»ª∫Û‘Ÿ“ª––“ª––¥¶¿Ì«Î«ÛÕ∑£¨≤¢∏˘æ›«Î«Ûmethod”Î«Î«ÛÕ∑µƒ–≈œ¢¿¥æˆ∂®
// «∑Ò”–«Î«ÛÃÂ“‘º∞«Î«ÛÃÂµƒ≥§∂»£¨»ª∫Û‘Ÿ»•∂¡»°«Î«ÛÃÂ
    for ( ;; ) {

        if (rc == NGX_AGAIN) {
            n = ngx_http_read_request_header(r);

            if (n == NGX_AGAIN || n == NGX_ERROR) { 
            //»Áπ˚ƒ⁄∫À÷–µƒ ˝æ›“—æ≠∂¡ÕÍ£¨µ´’‚ ±∫ÚÕ∑≤ø◊÷∂Œªπ√ª”–Ω‚ŒˆÕÍ±œ£¨‘Ú∞—øÿ÷∆∆˜Ωªªπ∏¯HTTP£¨µ± ˝æ›µΩ¿¥µƒ ±∫Ú¥•∑¢
            //ngx_http_process_request_line£¨“ÚŒ™∏√∫Ø ˝Õ‚√Êrev->handler = ngx_http_process_request_line;
                return;
            }
        }
    
        rc = ngx_http_parse_request_line(r, r->header_in);

        if (rc == NGX_OK) { //«Î«Û––Ω‚Œˆ≥…π¶

            /* the request line has been parsed successfully */
            //«Î«Û––ƒ⁄»›º∞≥§∂»    //GET /sample.jsp HTTP/1.1’˚––
            r->request_line.len = r->request_end - r->request_start;
            r->request_line.data = r->request_start;
            r->request_length = r->header_in->pos - r->request_start;

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http request line: \"%V\"", &r->request_line);

            //«Î«Û∑Ω∑® GET  POSTµ»    //GET /sample.jsp HTTP/1.1  ÷–µƒGET
            r->method_name.len = r->method_end - r->request_start + 1;
            r->method_name.data = r->request_line.data;

            //GET /sample.jsp HTTP/1.1  ÷–µƒHTTP/1.1
            if (r->http_protocol.data) {
                r->http_protocol.len = r->request_end - r->http_protocol.data;
            }

            if (ngx_http_process_request_uri(r) != NGX_OK) {
                return;
            }

            if (r->host_start && r->host_end) {

                host.len = r->host_end - r->host_start;
                host.data = r->host_start;

                rc = ngx_http_validate_host(&host, r->pool, 0);

                if (rc == NGX_DECLINED) {
                    ngx_log_error(NGX_LOG_INFO, c->log, 0,
                                  "client sent invalid host in request line");
                    ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
                    return;
                }

                if (rc == NGX_ERROR) {
                    ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                    return;
                }

                if (ngx_http_set_virtual_server(r, &host) == NGX_ERROR) {
                    return;
                }

                r->headers_in.server = host;
            }

            if (r->http_version < NGX_HTTP_VERSION_10) { //1.0“‘œ¬∞Ê±æ√ª”–«Î«ÛÕ∑≤ø◊÷∂Œ£¨
                /*
                    ”√ªß«Î«ÛµƒHTTP∞Ê±æ–°”⁄1.0£®»ÁHTTP 0.9∞Ê±æ£©£¨∆‰¥¶¿Ìπ˝≥ÃΩ´”ÎHTTP l.0∫ÕHTTP l.1µƒÕÍ»´≤ªÕ¨£¨À¸≤ªª·”–Ω” ’HTTP
                    Õ∑≤ø’‚“ª≤Ω÷Ë°£’‚ ±Ω´ª·µ˜”√ngx_http_find_virtual_server∑Ω∑®—∞’“µΩœ‡”¶µƒ–Èƒ‚÷˜ª˙£
                    */
                if (r->headers_in.server.len == 0
                    && ngx_http_set_virtual_server(r, &r->headers_in.server) //http0.9”¶∏√ «¥”«Î«Û––ªÒ»°–Èƒ‚÷˜ª˙?
                       == NGX_ERROR)
                {
                    return;
                }

                ngx_http_process_request(r);
                return;
            }

            //≥ı ºªØ”√”⁄¥Ê∑≈httpÕ∑≤ø––µƒø’º‰£¨”√¿¥¥Ê∑≈httpÕ∑≤ø––
            if (ngx_list_init(&r->headers_in.headers, r->pool, 20,
                              sizeof(ngx_table_elt_t))
                != NGX_OK)
            {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            c->log->action = "reading client request headers";

            rev->handler = ngx_http_process_request_headers;
            ngx_http_process_request_headers(rev);//ø™ ºΩ‚ŒˆhttpÕ∑≤ø––

            return;
        }

        if (rc != NGX_AGAIN) {//∂¡»°ÕÍ±œƒ⁄∫À∏√Ã◊Ω”◊÷…œ√Êµƒ ˝æ›£¨Õ∑≤ø––≤ª»´£¨‘ÚÀµ√˜Õ∑≤ø––≤ª»´πÿ±’¡¨Ω”

            /* there was error while a request line parsing */

            ngx_log_error(NGX_LOG_INFO, c->log, 0,
                          ngx_http_client_errors[rc - NGX_HTTP_CLIENT_ERROR]);
            ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
            return;
        }

        //±Ì æ∏√––ƒ⁄»›≤ªπª£¨¿˝»Árecv∂¡»°µƒ ±∫Ú£¨√ª”–∞—’˚–– ˝æ›∂¡»°≥ˆ¿¥£¨∑µªÿ∫ÛºÃ–¯recv£¨»ª∫ÛΩ”◊≈…œ¥ŒΩ‚ŒˆµƒŒª÷√ºÃ–¯Ω‚Œˆ÷±µΩ«Î«Û––Ω‚ŒˆÕÍ±œ
        /* NGX_AGAIN: a request line parsing is still incomplete */
        /*
             »Áπ˚ngx_http_parse_request_line∑Ω∑®∑µªÿNGX_AGAIN£¨‘Ú±Ì æ–Ë“™Ω” ’∏¸∂‡µƒ◊÷∑˚¡˜£¨’‚ ±–Ë“™∂‘header_inª∫≥Â«¯◊ˆ≈–∂œ£¨ºÏ≤È
          «∑Òªπ”–ø’œ–µƒƒ⁄¥Ê£¨»Áπ˚ªπ”–Œ¥ π”√µƒƒ⁄¥Êø…“‘ºÃ–¯Ω” ’◊÷∑˚¡˜£¨ºÏ≤Èª∫≥Â«¯ «∑Ò”–Œ¥Ω‚Œˆµƒ◊÷∑˚¡˜£¨∑Ò‘Úµ˜”√
         ngx_http_alloc_large_header_buffer∑Ω∑®∑÷≈‰∏¸¥ÛµƒΩ” ’ª∫≥Â«¯°£µΩµ◊∑÷≈‰∂‡¥Ûƒÿ£ø’‚”…nginx.confŒƒº˛÷–µƒlarge_client_header_buffers≈‰÷√œÓ÷∏∂®°£
          */
        if (r->header_in->pos == r->header_in->end) {

            rv = ngx_http_alloc_large_header_buffer(r, 1);

            if (rv == NGX_ERROR) {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            if (rv == NGX_DECLINED) {
                r->request_line.len = r->header_in->end - r->request_start;
                r->request_line.data = r->request_start;

                ngx_log_error(NGX_LOG_INFO, c->log, 0,
                              "client sent too long URI");
                ngx_http_finalize_request(r, NGX_HTTP_REQUEST_URI_TOO_LARGE);
                return;
            }
        }
        //±Ì æÕ∑≤ø––√ª”–Ω‚ŒˆÕÍ≥…£¨ºÃ–¯∂¡ ˝æ›Ω‚Œˆ
    }
}


ngx_int_t
ngx_http_process_request_uri(ngx_http_request_t *r)
{
    ngx_http_core_srv_conf_t  *cscf;

    if (r->args_start) {
        r->uri.len = r->args_start - 1 - r->uri_start;
    } else {
        r->uri.len = r->uri_end - r->uri_start;
    }

    if (r->complex_uri || r->quoted_uri) {

        r->uri.data = ngx_pnalloc(r->pool, r->uri.len + 1);
        if (r->uri.data == NULL) {
            ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_ERROR;
        }

        cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

        if (ngx_http_parse_complex_uri(r, cscf->merge_slashes) != NGX_OK) {
            r->uri.len = 0;

            ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                          "client sent invalid request");
            ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
            return NGX_ERROR;
        }

    } else {
        r->uri.data = r->uri_start;
    }

    r->unparsed_uri.len = r->uri_end - r->uri_start;
    r->unparsed_uri.data = r->uri_start;

    r->valid_unparsed_uri = r->space_in_uri ? 0 : 1;

    if (r->uri_ext) {
        if (r->args_start) {
            r->exten.len = r->args_start - 1 - r->uri_ext;
        } else {
            r->exten.len = r->uri_end - r->uri_ext;
        }

        r->exten.data = r->uri_ext;
    }

    if (r->args_start && r->uri_end > r->args_start) {
        r->args.len = r->uri_end - r->args_start;
        r->args.data = r->args_start;
    }

#if (NGX_WIN32)
    {
    u_char  *p, *last;

    p = r->uri.data;
    last = r->uri.data + r->uri.len;

    while (p < last) {

        if (*p++ == ':') {

            /*
             * this check covers "::$data", "::$index_allocation" and
             * ":$i30:$index_allocation"
             */

            if (p < last && *p == '$') {
                ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                              "client sent unsafe win32 URI");
                ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
                return NGX_ERROR;
            }
        }
    }

    p = r->uri.data + r->uri.len - 1;

    while (p > r->uri.data) {

        if (*p == ' ') {
            p--;
            continue;
        }

        if (*p == '.') {
            p--;
            continue;
        }

        break;
    }

    if (p != r->uri.data + r->uri.len - 1) {
        r->uri.len = p + 1 - r->uri.data;
        ngx_http_set_exten(r);
    }

    }
#endif

/*
2016/01/07 12:38:01[      ngx_http_process_request_line,  1002]  [debug] 20090#20090: *14 http request line: "GET /download/nginx-1.9.2.rar?st=xhWL03HbtjrojpEAfiD6Mw&e=1452139931 HTTP/1.1"
2016/01/07 12:38:01[       ngx_http_process_request_uri,  1223]  [debug] 20090#20090: *14 http uri: "/download/nginx-1.9.2.rar"
2016/01/07 12:38:01[       ngx_http_process_request_uri,  1226]  [debug] 20090#20090: *14 http args: "st=xhWL03HbtjrojpEAfiD6Mw&e=1452139931"
2016/01/07 12:38:01[       ngx_http_process_request_uri,  1229]  [debug] 20090#20090: *14 http exten: "rar"
*/
    //http://10.135.10.167/aaaaaaaa?bbbb  http uri: "/aaaaaaaa"  http args: "bbbb"  Õ¨ ±"GET /aaaaaaaa?bbbb.txt HTTP/1.1"÷–µƒ/aaaaaaa?bbbb.txt∫Õuri÷–µƒ“ª—˘
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http uri: \"%V\"", &r->uri);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http args: \"%V\"", &r->args);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http exten: \"%V\"", &r->exten);

    return NGX_OK;
}

/*
GET /sample.jsp HTTP/1.1

 

Accept:image/gif.image/jpeg,**

Accept-Language:zh-cn

Connection:Keep-Alive

Host:localhost

User-Agent:Mozila/4.0(compatible:MSIE5.01:Windows NT5.0)

Accept-Encoding:gzip,deflate.

*/ //Ω‚Œˆ…œ√ÊµƒGET /sample.jsp HTTP/1.1“‘Õ‚µƒ≈‰÷√

//ngx_http_parse_request_lineΩ‚Œˆ«Î«Û––£¨ ngx_http_process_request_headers(ngx_http_parse_header_line)Ω‚ŒˆÕ∑≤ø––(«Î«ÛÕ∑≤ø) Ω” ’∞¸ÃÂngx_http_read_client_request_body
static void
ngx_http_process_request_headers(ngx_event_t *rev)
{
    u_char                     *p;
    size_t                      len;
    ssize_t                     n;
    ngx_int_t                   rc, rv;
    ngx_table_elt_t            *h;
    ngx_connection_t           *c;
    ngx_http_header_t          *hh;
    ngx_http_request_t         *r;
    ngx_http_core_srv_conf_t   *cscf;
    ngx_http_core_main_conf_t  *cmcf;

    c = rev->data;
    r = c->data;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, rev->log, 0,
                   "http process request header line");

    if (rev->timedout) {//»Áπ˚tcp¡¨Ω”Ω®¡¢∫Û£¨µ»¡Àclient_header_timeout√Î“ª÷±√ª”– ’µΩøÕªß∂Àµƒ ˝æ›∞¸π˝¿¥£¨‘Úπÿ±’¡¨Ω”
        ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT, "client timed out");
        c->timedout = 1;
        ngx_http_close_request(r, NGX_HTTP_REQUEST_TIME_OUT);
        return;
    }

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    rc = NGX_AGAIN;

    for ( ;; ) {

        if (rc == NGX_AGAIN) {

            if (r->header_in->pos == r->header_in->end) {//Àµ√˜header_inø’º‰“—æ≠”√ÕÍ¡À£¨Œﬁ∑®ºÃ–¯¥Ê¥¢recvµƒ ˝æ›£¨‘Ú÷ÿ–¬∑÷≈‰¥Ûø’º‰

                rv = ngx_http_alloc_large_header_buffer(r, 0);

                if (rv == NGX_ERROR) {
                    ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                    return;
                }

                if (rv == NGX_DECLINED) {
                    p = r->header_name_start;

                    r->lingering_close = 1;

                    if (p == NULL) {
                        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                                      "client sent too large request");
                        ngx_http_finalize_request(r,
                                            NGX_HTTP_REQUEST_HEADER_TOO_LARGE);
                        return;
                    }

                    len = r->header_in->end - p;

                    if (len > NGX_MAX_ERROR_STR - 300) {
                        len = NGX_MAX_ERROR_STR - 300;
                    }

                    ngx_log_error(NGX_LOG_INFO, c->log, 0,
                                "client sent too long header line: \"%*s...\"",
                                len, r->header_name_start);

                    ngx_http_finalize_request(r,
                                            NGX_HTTP_REQUEST_HEADER_TOO_LARGE);
                    return;
                }
            }

            //’‚¿Ô÷¡…Ÿ∂¡“ª¥Œ
            n = ngx_http_read_request_header(r); //Àµ√˜ƒ⁄¥Ê÷–µƒ ˝æ›ªπ√ª”–∂¡ÕÍ£¨–Ë“™∂¡ÕÍ∫ÛºÃ–¯Ω‚Œˆ

            if (n == NGX_AGAIN || n == NGX_ERROR) {  
                return;
            }
        }

        /* the host header could change the server configuration context */
        cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

        rc = ngx_http_parse_header_line(r, r->header_in,
                                        cscf->underscores_in_headers);


        if (rc == NGX_OK) {

            r->request_length += r->header_in->pos - r->header_name_start; //request_length‘ˆº”“ª––◊÷∑˚≥§∂»

            if (r->invalid_header && cscf->ignore_invalid_headers) {

                /* there was error while a header line parsing */

                ngx_log_error(NGX_LOG_INFO, c->log, 0,
                              "client sent invalid header line: \"%*s\"",
                              r->header_end - r->header_name_start,
                              r->header_name_start);
                continue;
            }

            /* a header line has been parsed successfully */
            /* Ω‚ŒˆµƒÕ∑≤ªª·KEY:VALUE¥Ê»ÎµΩheaders_in÷– */
            h = ngx_list_push(&r->headers_in.headers);
            if (h == NULL) {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

            h->hash = r->header_hash;
            //øΩ±¥name:value÷–µƒnameµΩkey÷–,name∫Û√Êµƒ√∞∫≈±ª”√\0ÃÊªª¡À
            h->key.len = r->header_name_end - r->header_name_start;
            h->key.data = r->header_name_start;
            h->key.data[h->key.len] = '\0';
            
            //øΩ±¥name:value÷–µƒvalueµΩvalue÷–£¨value∫Ûµƒªª––∑˚±ª”√\0ÃÊªª¡À
            h->value.len = r->header_end - r->header_start;
            h->value.data = r->header_start;
            h->value.data[h->value.len] = '\0';

            h->lowcase_key = ngx_pnalloc(r->pool, h->key.len);
            if (h->lowcase_key == NULL) {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }

             //øΩ±¥name:value÷–µƒnameµΩkey÷–,∞¸¿®name∫Û√Êµƒ√∞∫≈
            if (h->key.len == r->lowcase_index) {
                ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);

            } else {
                ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
            }

            //º˚ngx_http_headers_in  //ºÏ≤ÈΩ‚ŒˆµΩµƒÕ∑≤økey:value÷–µƒ∂‘”¶ngx_http_header_tΩ·ππ£¨º¥ngx_http_headers_in÷–µƒ≥…‘±
            hh = ngx_hash_find(&cmcf->headers_in_hash, h->hash,
                               h->lowcase_key, h->key.len); 
            

            //¥”ngx_http_headers_in’“µΩ”Îname∆•≈‰µƒ◊÷∑˚¥Æ£¨»ª∫Û÷ª–Ë∂‘”¶µƒhandler,“ª∞„æÕ «∞—Ω‚Œˆ≥ˆµƒname:value÷–µƒvalue¥Ê∑≈µΩheaders_in≥…‘±µƒheaders¡¥±Ì÷–
            
            //≤Œøºngx_http_headers_in£¨Õ®π˝∏√ ˝◊È÷–µƒªÿµ˜hander¿¥¥Ê¥¢Ω‚ŒˆµΩµƒ«Î«Û––name:value÷–µƒvalueµΩheaders_inµƒœÏ”¶≥…‘±÷–£¨º˚ngx_http_process_request_headers
            if (hh && hh->handler(r, h, hh->offset) != NGX_OK) {
                return;
            }

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http header: \"%V: %V\"",
                           &h->key, &h->value); //»Áπ˚¥Úø™µ˜ ‘ø™πÿ£¨‘Úname:valueø…“‘¥ÊµΩerror.log÷–

            continue; //ºÃ–¯Ω‚Œˆœ¬“ª––
        }

        if (rc == NGX_HTTP_PARSE_HEADER_DONE) {//Õ∑≤ø––Ω‚ŒˆÕÍ±œ

            /* a whole header has been parsed successfully */

            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http header done");
            /*
                «Î«Û––
                Õ∑≤ø––
                ø’––(’‚¿Ô «\r\n)
                ƒ⁄»›
               */
            r->request_length += r->header_in->pos - r->header_name_start;//∞—ø’––µƒ\r\nº”…œ

            r->http_state = NGX_HTTP_PROCESS_REQUEST_STATE;

            rc = ngx_http_process_request_header(r);

            if (rc != NGX_OK) {
                return;
            }

            ngx_http_process_request(r);

            return;
        }

        if (rc == NGX_AGAIN) {//∑µªÿNGX_AGAIN ±£¨±Ì æªπ–Ë“™Ω” ’µΩ∏¸∂‡µƒ◊÷∑˚¡˜≤≈ƒ‹ºÃ–¯Ω‚Œˆ

            /* a header line parsing is still not complete */

            continue;
        }

        /* rc == NGX_HTTP_PARSE_INVALID_HEADER: "\r" is not followed by "\n" */
        /*
            µ±µ˜”√ngx_http_parse_header_line∑Ω∑®Ω‚Œˆ◊÷∑˚¥Æππ≥…µƒHTTP ±£¨ «”–ø…ƒ‹”ˆµΩ∑«∑®µƒªÚ’ﬂNginxµ±«∞∞Ê±æ≤ª÷ß≥÷µƒHTTPÕ∑≤øµƒ£¨
            ’‚ ±∏√∑Ω∑®ª·∑µªÿ¥ÌŒÛ£¨”⁄ «µ˜”√ngx_http_finalize_request∑Ω∑®£¨œÚøÕªß∂À∑¢ÀÕNGX_ HTTP BAD_REQUEST∫Í∂‘”¶µƒ400¥ÌŒÛ¬ÎœÏ”¶°£
          */
        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                      "client sent invalid header line: \"%*s\\r...\"",
                      r->header_end - r->header_name_start,
                      r->header_name_start);
        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return;
    }
}


static ssize_t
ngx_http_read_request_header(ngx_http_request_t *r)
{
    ssize_t                    n;
    ngx_event_t               *rev;
    ngx_connection_t          *c;
    ngx_http_core_srv_conf_t  *cscf;

    c = r->connection;
    rev = c->read;

    //À¸µƒpos≥…‘±∫Õlast≥…‘±÷∏œÚµƒµÿ÷∑÷Æº‰µƒƒ⁄¥ÊæÕ «Ω” ’µΩµƒªπŒ¥Ω‚Œˆµƒ◊÷∑˚¡˜
    n = r->header_in->last - r->header_in->pos; //header_in÷∏’Î÷∏œÚngx_connection_t->buffer

    //µ⁄“ª¥Œµ˜”√ngx_http_process_request_line∑Ω∑® ±ª∫≥Â«¯¿Ô±ÿ»ª «ø’µƒ£¨’‚ ±ª·µ˜”√∑‚◊∞µƒrecv∑Ω∑®∞—Linuxƒ⁄∫ÀÃ◊Ω”◊÷ª∫≥Â«¯÷–µƒTCP¡˜∏¥÷∆µΩheader_inª∫≥Â«¯÷–
    if (n > 0) {//‘⁄ngx_http_wait_request_handler÷–ª· ◊œ»∂¡»°“ª¥Œ£¨’‚¿Ô“ª∞„ «¥Û”⁄0µƒ
        return n;
    }

    if (rev->ready) { //»Áπ˚¿¥◊‘∂‘∂Àµƒ ˝æ›ªπ√ª”–∂¡»°ªÒ»°ªπ√ª”–∂¡ÕÍ£¨‘ÚºÃ–¯∂¡
        n = c->recv(c, r->header_in->last,
                    r->header_in->end - r->header_in->last); //ngx_unix_recv
    } else {
        n = NGX_AGAIN;
    }

    //√ø¥Œ∂¡»°ÕÍøÕªß∂Àπ˝¿¥µƒ«Î«Û ˝æ›∫Û£¨∂ºª·÷¥––µΩ’‚¿Ô£¨“ª∞„«Èøˆ «µ⁄“ª¥Œ‘⁄ngx_http_wait_request_handler∂¡»°ÕÍÀ˘”– ˝æ›£¨»ª∫Ûª·‘⁄
    //ngx_http_process_request_line÷–‘Ÿ¥Œµ˜”√±ængx_http_read_request_header∫Ø ˝£¨µ⁄∂˛¥Œµƒ ±∫Ú“—æ≠√ª ˝æ›¡À£¨ª·◊ﬂµΩ’‚¿Ô,¿˝»ÁøÕªß∂À∑¢ÀÕπ˝¿¥µƒ
    //Õ∑≤ø––≤ª»´£¨µ•øÕªß∂À“ª÷±≤ª∑¢…˙ £”‡µƒ≤ø∑÷
    if (n == NGX_AGAIN) { //»Áπ˚recv∑µªÿNGX_AGAIN,‘Ú–Ë“™÷ÿ–¬add read event,’‚—˘œ¬¥Œ”– ˝æ›¿¥ø…“‘ºÃ–¯∂¡,
        //µ±“ª¥Œ¥¶¿ÌøÕªß∂À«Î«ÛΩ· ¯∫Û£¨ª·∞—ngx_http_process_request_lineÃÌº”µΩ∂® ±∆˜÷–£¨»Áπ˚µ»client_header_timeoutªπ√ª”––≈µƒ«Î«Û ˝æ›π˝¿¥£¨
        //‘Úª·◊ﬂµΩngx_http_read_request_header÷–µƒngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);¥”∂¯πÿ±’¡¨Ω”
        if (!rev->timer_set) {
            cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
            //◊¢“‚£¨‘⁄Ω‚ŒˆµΩÕÍ’˚µƒÕ∑≤ø––∫Õ«Î«Û––∫Û£¨ª·‘⁄ngx_http_process_request÷–ª·∞—∂¡ ¬º˛≥¨ ±∂® ±∆˜…æ≥˝
            //‘⁄«Î«Û¥¶¿ÌÕÍ∑¢ÀÕ∏¯øÕªß∂À ˝æ›∫Û£¨rev->handler = ngx_http_keepalive_handler
            ngx_add_timer(rev, cscf->client_header_timeout, NGX_FUNC_LINE);
        }

        if (ngx_handle_read_event(rev, 0, NGX_FUNC_LINE) != NGX_OK) {
            ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_ERROR;
        }

        return NGX_AGAIN;
    }

    if (n == 0) {
        ngx_log_error(NGX_LOG_INFO, c->log, 0,
                      "client prematurely closed connection");
    }

    if (n == 0 || n == NGX_ERROR) { //TCP¡¨Ω”≥ˆ¥Ì
        c->error = 1;
        c->log->action = "reading client request headers";

        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return NGX_ERROR;
    }

    r->header_in->last += n;

    return n;
}

/*
//client_header_buffer_sizeŒ™∂¡»°øÕªß∂À ˝æ› ±ƒ¨»œ∑÷≈‰µƒø’º‰£¨»Áπ˚∏√ø’º‰≤ªπª¥Ê¥¢httpÕ∑≤ø––∫Õ«Î«Û––£¨‘Úª·µ˜”√large_client_header_buffers
//¥”–¬∑÷≈‰ø’º‰£¨≤¢∞—÷Æ«∞µƒø’º‰ƒ⁄»›øΩ±¥µΩ–¬ø’º‰÷–£¨À˘“‘£¨’‚“‚Œ∂◊≈ø…±‰≥§∂»µƒHTTP«Î«Û––º”…œHTTPÕ∑≤øµƒ≥§∂»◊‹∫Õ≤ªƒ‹≥¨π˝large_client_ header_
//buffers÷∏∂®µƒ◊÷Ω⁄ ˝£¨∑Ò‘ÚNginxΩ´ª·±®¥Ì°£
*/
//ngx_http_alloc_large_header_buffer∑Ω∑®∑÷≈‰∏¸¥ÛµƒΩ” ’ª∫≥Â«¯°£µΩµ◊∑÷≈‰∂‡¥Ûƒÿ£ø’‚”…nginx.confŒƒº˛÷–µƒlarge_client_header_buffers≈‰÷√œÓ÷∏∂®°£
static ngx_int_t
ngx_http_alloc_large_header_buffer(ngx_http_request_t *r,
    ngx_uint_t request_line)
{
    u_char                    *old, *new;
    ngx_buf_t                 *b;
    ngx_http_connection_t     *hc;
    ngx_http_core_srv_conf_t  *cscf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http alloc large header buffer");

    if (request_line && r->state == 0) {

        /* the client fills up the buffer with "\r\n" */

        r->header_in->pos = r->header_in->start;
        r->header_in->last = r->header_in->start;

        return NGX_OK;
    }

    old = request_line ? r->request_start : r->header_name_start;

    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

    if (r->state != 0
        && (size_t) (r->header_in->pos - old)
                                     >= cscf->large_client_header_buffers.size)
    {
        return NGX_DECLINED;
    }

    hc = r->http_connection;

    if (hc->nfree) {
        b = hc->free[--hc->nfree];

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http large header free: %p %uz",
                       b->pos, b->end - b->last);

    } else if (hc->nbusy < cscf->large_client_header_buffers.num) {

        if (hc->busy == NULL) {
            hc->busy = ngx_palloc(r->connection->pool,
                  cscf->large_client_header_buffers.num * sizeof(ngx_buf_t *));
            if (hc->busy == NULL) {
                return NGX_ERROR;
            }
        }

        b = ngx_create_temp_buf(r->connection->pool,
                                cscf->large_client_header_buffers.size);
        if (b == NULL) {
            return NGX_ERROR;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http large header alloc: %p %uz",
                       b->pos, b->end - b->last);

    } else {
        return NGX_DECLINED;
    }

    hc->busy[hc->nbusy++] = b;

    if (r->state == 0) {
        /*
         * r->state == 0 means that a header line was parsed successfully
         * and we do not need to copy incomplete header line and
         * to relocate the parser header pointers
         */

        r->header_in = b;

        return NGX_OK;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http large header copy: %d", r->header_in->pos - old);

    new = b->start;

    ngx_memcpy(new, old, r->header_in->pos - old);

    b->pos = new + (r->header_in->pos - old);
    b->last = new + (r->header_in->pos - old);

    if (request_line) {
        r->request_start = new;

        if (r->request_end) {
            r->request_end = new + (r->request_end - old);
        }

        r->method_end = new + (r->method_end - old);

        r->uri_start = new + (r->uri_start - old);
        r->uri_end = new + (r->uri_end - old);

        if (r->schema_start) {
            r->schema_start = new + (r->schema_start - old);
            r->schema_end = new + (r->schema_end - old);
        }

        if (r->host_start) {
            r->host_start = new + (r->host_start - old);
            if (r->host_end) {
                r->host_end = new + (r->host_end - old);
            }
        }

        if (r->port_start) {
            r->port_start = new + (r->port_start - old);
            r->port_end = new + (r->port_end - old);
        }

        if (r->uri_ext) {
            r->uri_ext = new + (r->uri_ext - old);
        }

        if (r->args_start) {
            r->args_start = new + (r->args_start - old);
        }

        if (r->http_protocol.data) {
            r->http_protocol.data = new + (r->http_protocol.data - old);
        }

    } else {
        r->header_name_start = new;
        r->header_name_end = new + (r->header_name_end - old);
        r->header_start = new + (r->header_start - old);
        r->header_end = new + (r->header_end - old);
    }

    r->header_in = b;

    return NGX_OK;
}


static ngx_int_t
ngx_http_process_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  **ph;

    ph = (ngx_table_elt_t **) ((char *) &r->headers_in + offset);

    if (*ph == NULL) {
        *ph = h;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_process_unique_header_line(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_table_elt_t  **ph;

    ph = (ngx_table_elt_t **) ((char *) &r->headers_in + offset);

    if (*ph == NULL) {
        *ph = h;
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                  "client sent duplicate header line: \"%V: %V\", "
                  "previous value: \"%V: %V\"",
                  &h->key, &h->value, &(*ph)->key, &(*ph)->value);

    ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);

    return NGX_ERROR;
}

//ngx_http_process_request_headers->ngx_http_process_host->ngx_http_set_virtual_server
/*
handlerµƒ»˝∏ˆ≤Œ ˝∑÷±Œ™(r, h, hh->offset):rŒ™∂‘”¶µƒ¡¨Ω”«Î«Û£¨h¥Ê¥¢Œ™Õ∑≤ø––key:value(»Á:Content-Type: text/html)÷µ£¨hh->offsetº¥
ngx_http_headers_in÷–≥…‘±µƒ∂‘”¶offset(»Á«Î«Û––¥¯”–host£¨‘Úoffset=offsetof(ngx_http_headers_in_t, host))
*/
static ngx_int_t
ngx_http_process_host(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset) //∏˘æ›«Î«Û÷–µƒhost¿¥Ω¯––server≤È’“∫Õ∂®Œª
{
    ngx_int_t  rc;
    ngx_str_t  host;

    if (r->headers_in.host == NULL) {
        r->headers_in.host = h;
    }

    host = h->value;

    rc = ngx_http_validate_host(&host, r->pool, 0);

    if (rc == NGX_DECLINED) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "client sent invalid host header");
        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return NGX_ERROR;
    }

    if (rc == NGX_ERROR) {
        ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_ERROR;
    }

    if (r->headers_in.server.len) { //¿¥◊‘Õ¨“ª∏ˆTCP¡¨Ω”µƒøÕªß∂À£¨À˚√«µƒserver «“ª—˘µƒ
        return NGX_OK;
    }

    if (ngx_http_set_virtual_server(r, &host) == NGX_ERROR) {
        return NGX_ERROR;
    }

    r->headers_in.server = host;

    return NGX_OK;
}


static ngx_int_t
ngx_http_process_connection(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    if (ngx_strcasestrn(h->value.data, "close", 5 - 1)) { 
        r->headers_in.connection_type = NGX_HTTP_CONNECTION_CLOSE;

    } else if (ngx_strcasestrn(h->value.data, "keep-alive", 10 - 1)) {//≥§¡¨Ω”
        r->headers_in.connection_type = NGX_HTTP_CONNECTION_KEEP_ALIVE;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_process_user_agent(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    u_char  *user_agent, *msie;

    if (r->headers_in.user_agent) {
        return NGX_OK;
    }

    r->headers_in.user_agent = h;

    /* check some widespread browsers while the header is in CPU cache */

    user_agent = h->value.data;

    msie = ngx_strstrn(user_agent, "MSIE ", 5 - 1);

    if (msie && msie + 7 < user_agent + h->value.len) {

        r->headers_in.msie = 1;

        if (msie[6] == '.') {

            switch (msie[5]) {
            case '4':
            case '5':
                r->headers_in.msie6 = 1;
                break;
            case '6':
                if (ngx_strstrn(msie + 8, "SV1", 3 - 1) == NULL) {
                    r->headers_in.msie6 = 1;
                }
                break;
            }
        }

#if 0
        /* MSIE ignores the SSL "close notify" alert */
        if (c->ssl) {
            c->ssl->no_send_shutdown = 1;
        }
#endif
    }

    if (ngx_strstrn(user_agent, "Opera", 5 - 1)) {
        r->headers_in.opera = 1;
        r->headers_in.msie = 0;
        r->headers_in.msie6 = 0;
    }

    if (!r->headers_in.msie && !r->headers_in.opera) {

        if (ngx_strstrn(user_agent, "Gecko/", 6 - 1)) {
            r->headers_in.gecko = 1;

        } else if (ngx_strstrn(user_agent, "Chrome/", 7 - 1)) {
            r->headers_in.chrome = 1;

        } else if (ngx_strstrn(user_agent, "Safari/", 7 - 1)
                   && ngx_strstrn(user_agent, "Mac OS X", 8 - 1))
        {
            r->headers_in.safari = 1;

        } else if (ngx_strstrn(user_agent, "Konqueror", 9 - 1)) {
            r->headers_in.konqueror = 1;
        }
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_process_multi_header_lines(ngx_http_request_t *r, ngx_table_elt_t *h,
    ngx_uint_t offset)
{
    ngx_array_t       *headers;
    ngx_table_elt_t  **ph;

    headers = (ngx_array_t *) ((char *) &r->headers_in + offset);

    if (headers->elts == NULL) {
        if (ngx_array_init(headers, r->pool, 1, sizeof(ngx_table_elt_t *))
            != NGX_OK)
        {
            ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_ERROR;
        }
    }

    ph = ngx_array_push(headers);
    if (ph == NULL) {
        ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_ERROR;
    }

    *ph = h;
    return NGX_OK;
}

//ngx_http_process_request_headersÕ∑≤ø––Ω‚ŒˆÕÍ±œ∫Ûµ˜”√∫Ø ˝ngx_http_process_request_header
//Ω‚ŒˆµΩÕ∑≤ø––µƒhost:   ±ª·÷¥––∏√∫Ø ˝
ngx_int_t
ngx_http_process_request_header(ngx_http_request_t *r)
{
    if (r->headers_in.server.len == 0
        && ngx_http_set_virtual_server(r, &r->headers_in.server)
           == NGX_ERROR)  
    {
        return NGX_ERROR;
    }

    if (r->headers_in.host == NULL && r->http_version > NGX_HTTP_VERSION_10) { //1.0“‘…œ∞Ê±æ±ÿ–Î–Ø¥¯host
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                   "client sent HTTP/1.1 request without \"Host\" header");
        ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
        return NGX_ERROR;
    }

    if (r->headers_in.content_length) {
        r->headers_in.content_length_n =
                            ngx_atoof(r->headers_in.content_length->value.data,
                                      r->headers_in.content_length->value.len);

        if (r->headers_in.content_length_n == NGX_ERROR) {
            ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                          "client sent invalid \"Content-Length\" header");
            ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
            return NGX_ERROR;
        }
    }

    if (r->method & NGX_HTTP_TRACE) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                      "client sent TRACE method");
        ngx_http_finalize_request(r, NGX_HTTP_NOT_ALLOWED);
        return NGX_ERROR;
    }

    if (r->headers_in.transfer_encoding) {
        if (r->headers_in.transfer_encoding->value.len == 7
            && ngx_strncasecmp(r->headers_in.transfer_encoding->value.data,
                               (u_char *) "chunked", 7) == 0) //Transfer-Encoding:chunked
        {
            r->headers_in.content_length = NULL;
            r->headers_in.content_length_n = -1;
            r->headers_in.chunked = 1;

        } else if (r->headers_in.transfer_encoding->value.len != 8
            || ngx_strncasecmp(r->headers_in.transfer_encoding->value.data,
                               (u_char *) "identity", 8) != 0)
        {
            ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                          "client sent unknown \"Transfer-Encoding\": \"%V\"",
                          &r->headers_in.transfer_encoding->value);
            ngx_http_finalize_request(r, NGX_HTTP_NOT_IMPLEMENTED);
            return NGX_ERROR;
        }
    }

    if (r->headers_in.connection_type == NGX_HTTP_CONNECTION_KEEP_ALIVE) {
        if (r->headers_in.keep_alive) {//Connection=keep-alive ±≤≈”––ß
            r->headers_in.keep_alive_n =
                            ngx_atotm(r->headers_in.keep_alive->value.data,
                                      r->headers_in.keep_alive->value.len);
        }
    }

    return NGX_OK;
}

/*
ngx_http_process_request∑Ω∑®∏∫‘‘⁄Ω” ’ÕÍHTTPÕ∑≤ø∫Û£¨µ⁄“ª¥Œ”Î∏˜∏ˆHTTPƒ£øÈπ≤Õ¨∞¥Ω◊∂Œ¥¶¿Ì«Î«Û£¨∂¯∂‘”⁄ngx_http_request_handler∑Ω∑®£¨
»Áπ˚ngx_http_process_request√ªƒ‹¥¶¿ÌÕÍ«Î«Û£¨’‚∏ˆ«Î«Û…œµƒ ¬º˛‘Ÿ¥Œ±ª¥•∑¢£¨ƒ«æÕΩ´”…¥À∑Ω∑®ºÃ–¯¥¶¿Ì¡À°£
*/
//ngx_http_process_request_headersÕ∑≤ø––Ω‚ŒˆÕÍ±œ∫Ûµ˜”√∫Ø ˝ngx_http_process_request_header
void
ngx_http_process_request(ngx_http_request_t *r) 
{
    ngx_connection_t  *c;

    c = r->connection;

#if (NGX_HTTP_SSL)

    if (r->http_connection->ssl) {
        long                      rc;
        X509                     *cert;
        ngx_http_ssl_srv_conf_t  *sscf;

        if (c->ssl == NULL) {
            ngx_log_error(NGX_LOG_INFO, c->log, 0,
                          "client sent plain HTTP request to HTTPS port");
            ngx_http_finalize_request(r, NGX_HTTP_TO_HTTPS);
            return;
        }

        sscf = ngx_http_get_module_srv_conf(r, ngx_http_ssl_module);

        if (sscf->verify) {
            rc = SSL_get_verify_result(c->ssl->connection);

            if (rc != X509_V_OK
                && (sscf->verify != 3 || !ngx_ssl_verify_error_optional(rc)))
            {
                ngx_log_error(NGX_LOG_INFO, c->log, 0,
                              "client SSL certificate verify error: (%l:%s)",
                              rc, X509_verify_cert_error_string(rc));

                ngx_ssl_remove_cached_session(sscf->ssl.ctx,
                                       (SSL_get0_session(c->ssl->connection)));

                ngx_http_finalize_request(r, NGX_HTTPS_CERT_ERROR);
                return;
            }

            if (sscf->verify == 1) {
                cert = SSL_get_peer_certificate(c->ssl->connection);

                if (cert == NULL) {
                    ngx_log_error(NGX_LOG_INFO, c->log, 0,
                                  "client sent no required SSL certificate");

                    ngx_ssl_remove_cached_session(sscf->ssl.ctx,
                                       (SSL_get0_session(c->ssl->connection)));

                    ngx_http_finalize_request(r, NGX_HTTPS_NO_CERT);
                    return;
                }

                X509_free(cert);
            }
        }
    }

#endif
    /*
    ”…”⁄œ÷‘⁄“—æ≠ø™ º◊º±∏µ˜”√∏˜HTTPƒ£øÈ¥¶¿Ì«Î«Û¡À£¨“Ú¥À≤ª‘Ÿ¥Ê‘⁄Ω” ’HTTP«Î«ÛÕ∑≤ø≥¨ ±µƒŒ Ã‚£¨ƒ«æÕ–Ë“™¥”∂® ±∆˜÷–∞—µ±«∞¡¨Ω”µƒ∂¡ ¬º˛“∆≥˝¡À°£
    ºÏ≤È∂¡ ¬º˛∂‘”¶µƒtimer_set±Í÷æŒª£¨¡¶1 ±±Ì æ∂¡ ¬º˛“—æ≠ÃÌº”µΩ∂® ±∆˜÷–¡À£¨’‚ ±–Ë“™µ˜”√ngx_del_timer¥”∂® ±∆˜÷–“∆≥˝∂¡ ¬º˛£ª
     */
    if (c->read->timer_set) {//ngx_http_read_request_header÷–∂¡»°≤ªµΩ ˝æ›µƒ ±∫Ú∑µªÿNGX_AGIN£¨ª·ÃÌº”∂® ±∆˜∫Õ∂¡ ¬º˛±Ì æºÃ–¯µ»¥˝øÕªß∂À ˝æ›µΩ¿¥
        ngx_del_timer(c->read, NGX_FUNC_LINE);
    }

#if (NGX_STAT_STUB)
    (void) ngx_atomic_fetch_add(ngx_stat_reading, -1);
    r->stat_reading = 0;
    (void) ngx_atomic_fetch_add(ngx_stat_writing, 1);
    r->stat_writing = 1;
#endif

/*
¥”œ÷‘⁄ø™ º≤ªª·‘Ÿ–Ë“™Ω” ’HTTP«Î«Û––ªÚ’ﬂÕ∑≤ø£¨À˘“‘–Ë“™÷ÿ–¬…Ë÷√µ±«∞¡¨Ω”∂¡/–¥ ¬º˛µƒªÿµ˜∑Ω∑®°£‘⁄’‚“ª≤Ω÷Ë÷–£¨Ω´Õ¨ ±∞—∂¡ ¬º˛°¢–¥ ¬º˛µƒªÿµ˜
∑Ω∑®∂º…Ë÷√Œ™ngx_http_request_handler∑Ω∑®£¨«Î«Ûµƒ∫Û–¯¥¶¿Ì∂º «Õ®π˝ngx_http_request_handler∑Ω∑®Ω¯––µƒ°£
 */
    c->read->handler = ngx_http_request_handler; //”…∂¡–¥ ¬º˛¥•∑¢ngx_http_request_handler  //”…epoll∂¡ ¬º˛‘⁄ngx_epoll_process_events¥•∑¢
    c->write->handler = ngx_http_request_handler;   //”…epoll–¥ ¬º˛‘⁄ngx_epoll_process_events¥•∑¢

/*
…Ë÷√ngx_http_request_tΩ·ππÃÂµƒread_event_handler∑Ω∑®gx_http_block_reading°£µ±‘Ÿ¥Œ”–∂¡ ¬º˛µΩ¿¥ ±£¨Ω´ª·µ˜”√ngx_http_block_reading∑Ω∑®
¥¶¿Ì«Î«Û°£∂¯’‚¿ÔΩ´À¸…Ë÷√Œ™ngx_http_block_reading∑Ω∑®£¨’‚∏ˆ∑Ω∑®ø…»œŒ™≤ª◊ˆ»Œ∫Œ ¬£¨À¸µƒ“‚“Â‘⁄”⁄£¨ƒø«∞“—æ≠ø™ º¥¶¿ÌHTTP«Î«Û£¨≥˝∑«ƒ≥∏ˆHTTPƒ£øÈ÷ÿ–¬
…Ë÷√¡Àread_event_handler∑Ω∑®£¨∑Ò‘Ú»Œ∫Œ∂¡ ¬º˛∂ºΩ´µ√≤ªµΩ¥¶¿Ì£¨“≤ø…À∆»œŒ™∂¡ ¬º˛±ª◊Ë »˚¡À°£
*/
    r->read_event_handler = ngx_http_block_reading; //±Ì æ‘› ±≤ª“™∂¡»°øÕªß∂À«Î«Û    

    /* ngx_http_process_request∫Õngx_http_request_handler’‚¡Ω∏ˆ∑Ω∑®µƒπ≤Õ®÷Æ¥¶‘⁄”⁄£¨À¸√«∂ºª·œ»∞¥Ω◊∂Œµ˜”√∏˜∏ˆHTTPƒ£øÈ¥¶¿Ì«Î«Û£¨‘Ÿ¥¶¿Ìpost«Î«Û */
    ngx_http_handler(r); //’‚¿Ô√Êª·÷¥––ngx_http_core_run_phases,÷¥––11∏ˆΩ◊∂Œ

/*
HTTPøÚº‹Œﬁ¬€ «µ˜”√ngx_http_process_request∑Ω∑®£® ◊¥Œ¥”“µŒÒ…œ¥¶¿Ì«Î«Û£©ªπ «ngx_http_request_handler∑Ω∑®£®TCP¡¨Ω”…œ∫Û–¯µƒ ¬º˛¥•∑¢ ±£©¥¶¿Ì
«Î«Û£¨◊Ó∫Û∂º”–“ª∏ˆ≤Ω÷Ë£¨æÕ «µ˜”√ngx_http_run_posted_requests∑Ω∑®¥¶¿Ìpost«Î«Û

11∏ˆΩ◊∂Œ÷¥––ÕÍ±œ∫Û£¨µ˜”√ngx_http_run_posted_requests∑Ω∑®÷¥––post«Î«Û£¨’‚¿Ô“ª∞„∂º «∂‘subrequestΩ¯––¥¶¿Ì
*/
    ngx_http_run_posted_requests(c); /*  */
}

//ºÏ≤‚Õ∑≤ø––host:∫Û√Êµƒ≤Œ ˝ «∑Ò’˝»∑
static ngx_int_t
ngx_http_validate_host(ngx_str_t *host, ngx_pool_t *pool, ngx_uint_t alloc)
{
    u_char  *h, ch;
    size_t   i, dot_pos, host_len;

    enum {
        sw_usual = 0,
        sw_literal,
        sw_rest
    } state;

    dot_pos = host->len;
    host_len = host->len;

    h = host->data;

    state = sw_usual;

    for (i = 0; i < host->len; i++) {
        ch = h[i];

        switch (ch) {

        case '.':
            if (dot_pos == i - 1) {
                return NGX_DECLINED;
            }
            dot_pos = i;
            break;

        case ':':
            if (state == sw_usual) {
                host_len = i;
                state = sw_rest;
            }
            break;

        case '[':
            if (i == 0) {
                state = sw_literal;
            }
            break;

        case ']':
            if (state == sw_literal) {
                host_len = i + 1;
                state = sw_rest;
            }
            break;

        case '\0':
            return NGX_DECLINED;

        default:

            if (ngx_path_separator(ch)) {
                return NGX_DECLINED;
            }

            if (ch >= 'A' && ch <= 'Z') {
                alloc = 1;
            }

            break;
        }
    }

    if (dot_pos == host_len - 1) {
        host_len--;
    }

    if (host_len == 0) {
        return NGX_DECLINED;
    }

    if (alloc) {
        host->data = ngx_pnalloc(pool, host_len);
        if (host->data == NULL) {
            return NGX_ERROR;
        }

        ngx_strlow(host->data, h, host_len);
    }

    host->len = host_len;

    return NGX_OK;
}

/*
µ±øÕªß∂ÀΩ®¡¢¡¨Ω”∫Û£¨≤¢∑¢ÀÕ«Î«Û ˝æ›π˝¿¥∫Û£¨‘⁄ngx_http_create_request÷–¥”ngx_http_connection_t->conf_ctxªÒ»°’‚»˝∏ˆ÷µ£¨“≤æÕ «∏˘æ›øÕªß∂À¡¨Ω”
±æ∂ÀÀ˘¥¶IP:portÀ˘∂‘”¶µƒƒ¨»œserver{}øÈ…œœ¬Œƒ£¨»Áπ˚ «“‘œ¬«Èøˆ:ip:portœ‡Õ¨£¨µ•‘⁄≤ªÕ¨µƒserver{}øÈ÷–£¨ƒ«√¥”–ø…ƒ‹øÕªß∂À«Î«Ûπ˝¿¥µƒ ±∫Ú–Ø¥¯µƒhost
Õ∑≤øœÓµƒserver_name≤ª‘⁄ƒ¨»œµƒserver{}÷–£¨∂¯‘⁄¡ÌÕ‚µƒserver{}÷–£¨À˘“‘–Ë“™Õ®π˝ngx_http_set_virtual_server÷ÿ–¬ªÒ»°server{}∫Õlocation{}…œœ¬Œƒ≈‰÷√
¿˝»Á:
    server {  #1
        listen 1.1.1.1:80;
        server_name aaa
    }

    server {   #2
        listen 1.1.1.1:80;
        server_name bbb
    }
    ’‚¡Ω∏ˆserver{}’º”√Õ¨“ª∏ˆngx_http_conf_addr_t£¨µ´À˚√«”µ”–¡Ω∏ˆ≤ªÕ¨µƒngx_http_core_srv_conf_t(¥Ê‘⁄”⁄ngx_http_conf_addr_t->servers),
    ’‚∏ˆ≈‰÷√‘⁄ngx_http_init_connection÷–ªÒ»°’‚∏ˆngx_http_port_t(1∏ˆngx_http_port_t∂‘”¶“ª∏ˆngx_http_conf_addr_t)∞—ngx_http_connection_t->conf_ctx
    ÷∏œÚngx_http_addr_conf_s->default_server,“≤æÕ «÷∏œÚ#1,»ª∫Ûngx_http_create_request÷–∞—main_conf srv_conf  loc_conf ÷∏œÚ#1,
    µ´»Áπ˚«Î«Û––µƒÕ∑≤øµƒhost:bbb£¨ƒ«√¥–Ë“™÷ÿ–¬ªÒ»°∂‘”¶µƒserver{} #2,º˚ngx_http_set_virtual_server->ngx_http_find_virtual_server
 */

//‘⁄Ω‚ŒˆµΩhttpÕ∑≤øµƒhost◊÷∂Œ∫Û£¨ª·Õ®π˝//ngx_http_process_request_headers->ngx_http_process_host->ngx_http_set_virtual_server
//ªÒ»°server_name≈‰÷√∫Õhost◊÷∑˚¥ÆÕÍ»´“ª—˘µƒserver{}…œœ¬Œƒ≈‰÷√ngx_http_core_srv_conf_t∫Õngx_http_core_loc_conf_t
static ngx_int_t
ngx_http_set_virtual_server(ngx_http_request_t *r, ngx_str_t *host)
{
    ngx_int_t                  rc;
    ngx_http_connection_t     *hc;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_core_srv_conf_t  *cscf;

#if (NGX_SUPPRESS_WARN)
    cscf = NULL;
#endif

    hc = r->http_connection;

#if (NGX_HTTP_SSL && defined SSL_CTRL_SET_TLSEXT_HOSTNAME)

    if (hc->ssl_servername) {
        if (hc->ssl_servername->len == host->len
            && ngx_strncmp(hc->ssl_servername->data,
                           host->data, host->len) == 0)
        {
#if (NGX_PCRE)
            if (hc->ssl_servername_regex
                && ngx_http_regex_exec(r, hc->ssl_servername_regex,
                                          hc->ssl_servername) != NGX_OK)
            {
                ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_ERROR;
            }
#endif
            return NGX_OK;
        }
    }

#endif
    //≤È’“host◊÷∑˚¥Æ∫Õserver_name≈‰÷√œ‡Õ¨µƒserver{}…œœ¬Œƒ
    rc = ngx_http_find_virtual_server(r->connection,
                                      hc->addr_conf->virtual_names,
                                      host, r, &cscf);

    if (rc == NGX_ERROR) {
        ngx_http_close_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_ERROR;
    }

#if (NGX_HTTP_SSL && defined SSL_CTRL_SET_TLSEXT_HOSTNAME)

    if (hc->ssl_servername) {
        ngx_http_ssl_srv_conf_t  *sscf;

        if (rc == NGX_DECLINED) {
            cscf = hc->addr_conf->default_server;
            rc = NGX_OK;
        }

        sscf = ngx_http_get_module_srv_conf(cscf->ctx, ngx_http_ssl_module);

        if (sscf->verify) {
            ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                          "client attempted to request the server name "
                          "different from that one was negotiated");
            ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
            return NGX_ERROR;
        }
    }

#endif

    if (rc == NGX_DECLINED) {
        return NGX_OK;
    }

    /*
µ±øÕªß∂ÀΩ®¡¢¡¨Ω”∫Û£¨≤¢∑¢ÀÕ«Î«Û ˝æ›π˝¿¥∫Û£¨‘⁄ngx_http_create_request÷–¥”ngx_http_connection_t->conf_ctxªÒ»°’‚»˝∏ˆ÷µ£¨“≤æÕ «∏˘æ›øÕªß∂À¡¨Ω”
±æ∂ÀÀ˘¥¶IP:portÀ˘∂‘”¶µƒƒ¨»œserver{}øÈ…œœ¬Œƒ£¨»Áπ˚ «“‘œ¬«Èøˆ:ip:portœ‡Õ¨£¨µ•‘⁄≤ªÕ¨µƒserver{}øÈ÷–£¨ƒ«√¥”–ø…ƒ‹øÕªß∂À«Î«Ûπ˝¿¥µƒ ±∫Ú–Ø¥¯µƒhost
Õ∑≤øœÓµƒserver_name≤ª‘⁄ƒ¨»œµƒserver{}÷–£¨∂¯‘⁄¡ÌÕ‚µƒserver{}÷–£¨À˘“‘–Ë“™Õ®π˝ngx_http_set_virtual_server÷ÿ–¬ªÒ»°server{}∫Õlocation{}…œœ¬Œƒ≈‰÷√
¿˝»Á:
    server {  #1
        listen 1.1.1.1:80;
        server_name aaa
    }

    server {   #2
        listen 1.1.1.1:80;
        server_name bbb
    }
    ’‚∏ˆ≈‰÷√‘⁄ngx_http_init_connection÷–∞—ngx_http_connection_t->conf_ctx÷∏œÚngx_http_addr_conf_s->default_server,“≤æÕ «÷∏œÚ#1,»ª∫Û
    ngx_http_create_request÷–∞—main_conf srv_conf  loc_conf ÷∏œÚ#1,
    µ´»Áπ˚«Î«Û––µƒÕ∑≤øµƒhost:bbb£¨ƒ«√¥–Ë“™÷ÿ–¬ªÒ»°∂‘”¶µƒserver{} #2,º˚ngx_http_set_virtual_server
 */
    //∞— srv_conf  loc_conf÷∏œÚhost”Îserver_name≈‰÷√œ‡Õ¨µƒserver{]…œœ¬Œƒ≈‰÷√srv_conf  loc_conf

    /*
        ’‚¿Ô“≤ÃÂœ÷¡À‘⁄ngx_http_init_connection÷–ªÒ»°http{}…œœ¬Œƒctx£¨»Áπ˚øÕªß∂À«Î«Û÷–¥¯”–host≤Œ ˝£¨‘Úª·ºÃ–¯‘⁄ngx_http_set_virtual_server÷–÷ÿ–¬ªÒ»°
        ∂‘”¶µƒserver{}∫Õlocation{}£¨»Áπ˚øÕªß∂À«Î«Û≤ª¥¯hostÕ∑≤ø––£¨‘Ú π”√ƒ¨»œµƒserver{},º˚ ngx_http_init_connection
    */
    r->srv_conf = cscf->ctx->srv_conf;
    r->loc_conf = cscf->ctx->loc_conf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    ngx_set_connection_log(r->connection, clcf->error_log);

    return NGX_OK;
}

//¥”virtual_names->names  hash±Ì÷–≤È’“hostÀ˘¥¶◊÷∑˚¥Æ◊˜Œ™key£¨∆‰‘⁄hash÷–∂‘”¶Ω⁄µ„µƒvalue÷µ,“≤æÕ «≤È’“host◊÷∑˚¥Æsever_name≈‰÷√œ‡Õ¨µƒserver{}…œœ¬Œƒ°£
/*
µ±øÕªß∂ÀΩ®¡¢¡¨Ω”∫Û£¨≤¢∑¢ÀÕ«Î«Û ˝æ›π˝¿¥∫Û£¨‘⁄ngx_http_create_request÷–¥”ngx_http_connection_t->conf_ctxªÒ»°’‚»˝∏ˆ÷µ£¨“≤æÕ «∏˘æ›øÕªß∂À¡¨Ω”
±æ∂ÀÀ˘¥¶IP:portÀ˘∂‘”¶µƒƒ¨»œserver{}øÈ…œœ¬Œƒ£¨»Áπ˚ «“‘œ¬«Èøˆ:ip:portœ‡Õ¨£¨µ•‘⁄≤ªÕ¨µƒserver{}øÈ÷–£¨ƒ«√¥”–ø…ƒ‹øÕªß∂À«Î«Ûπ˝¿¥µƒ ±∫Ú–Ø¥¯µƒhost
Õ∑≤øœÓµƒserver_name≤ª‘⁄ƒ¨»œµƒserver{}÷–£¨∂¯‘⁄¡ÌÕ‚µƒserver{}÷–£¨À˘“‘–Ë“™Õ®π˝ngx_http_set_virtual_server÷ÿ–¬ªÒ»°server{}∫Õlocation{}…œœ¬Œƒ≈‰÷√
¿˝»Á:
    server {  #1
        listen 1.1.1.1:80;
        server_name aaa
    }

    server {   #2
        listen 1.1.1.1:80;
        server_name bbb
    }
    ’‚¡Ω∏ˆserver{}’º”√Õ¨“ª∏ˆngx_http_conf_addr_t£¨µ´À˚√«”µ”–¡Ω∏ˆ≤ªÕ¨µƒngx_http_core_srv_conf_t(¥Ê‘⁄”⁄ngx_http_conf_addr_t->servers),
    ’‚∏ˆ≈‰÷√‘⁄ngx_http_init_connection÷–ªÒ»°’‚∏ˆngx_http_port_t(1∏ˆngx_http_port_t∂‘”¶“ª∏ˆngx_http_conf_addr_t)∞—ngx_http_connection_t->conf_ctx
    ÷∏œÚngx_http_addr_conf_s->default_server,“≤æÕ «÷∏œÚ#1,»ª∫Ûngx_http_create_request÷–∞—main_conf srv_conf  loc_conf ÷∏œÚ#1,
    µ´»Áπ˚«Î«Û––µƒÕ∑≤øµƒhost:bbb£¨ƒ«√¥–Ë“™÷ÿ–¬ªÒ»°∂‘”¶µƒserver{} #2,º˚ngx_http_set_virtual_server->ngx_http_find_virtual_server
 */static ngx_int_t
ngx_http_find_virtual_server(ngx_connection_t *c,
    ngx_http_virtual_names_t *virtual_names, ngx_str_t *host,
    ngx_http_request_t *r, ngx_http_core_srv_conf_t **cscfp) //ªÒ»°host◊÷∑˚¥Æ∂‘”¶µƒserver_nameÀ˘∂‘¥¶µƒserver{}≈‰÷√øÈ–≈œ¢
{
    ngx_http_core_srv_conf_t  *cscf;

    if (virtual_names == NULL) {
        return NGX_DECLINED;
    }

    //virtual_names->names hash÷–µƒkeyŒ™◊÷∑˚¥Æserver_name xxx÷–µƒxxx,valueŒ™∏√server_name xxxÀ˘‘⁄µƒserver{}øÈ
    cscf = ngx_hash_find_combined(&virtual_names->names,
                                  ngx_hash_key(host->data, host->len),
                                  host->data, host->len);

    if (cscf) {
        *cscfp = cscf;
        return NGX_OK;
    }

#if (NGX_PCRE)

    if (host->len && virtual_names->nregex) {
        ngx_int_t                n;
        ngx_uint_t               i;
        ngx_http_server_name_t  *sn;

        sn = virtual_names->regex;

#if (NGX_HTTP_SSL && defined SSL_CTRL_SET_TLSEXT_HOSTNAME)

        if (r == NULL) {
            ngx_http_connection_t  *hc;

            for (i = 0; i < virtual_names->nregex; i++) {

                n = ngx_regex_exec(sn[i].regex->regex, host, NULL, 0);

                if (n == NGX_REGEX_NO_MATCHED) {
                    continue;
                }

                if (n >= 0) {
                    hc = c->data;
                    hc->ssl_servername_regex = sn[i].regex;

                    *cscfp = sn[i].server;
                    return NGX_OK;
                }

                ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                              ngx_regex_exec_n " failed: %i "
                              "on \"%V\" using \"%V\"",
                              n, host, &sn[i].regex->name);

                return NGX_ERROR;
            }

            return NGX_DECLINED;
        }

#endif /* NGX_HTTP_SSL && defined SSL_CTRL_SET_TLSEXT_HOSTNAME */

        for (i = 0; i < virtual_names->nregex; i++) {

            n = ngx_http_regex_exec(r, sn[i].regex, host);

            if (n == NGX_DECLINED) {
                continue;
            }

            if (n == NGX_OK) {
                *cscfp = sn[i].server;
                return NGX_OK;
            }

            return NGX_ERROR;
        }
    }

#endif /* NGX_PCRE */

    return NGX_DECLINED;
}

/*
ngx_http_process_request∑Ω∑®∏∫‘‘⁄Ω” ’ÕÍHTTPÕ∑≤ø∫Û£¨µ⁄“ª¥Œ”Î∏˜∏ˆHTTPƒ£øÈπ≤Õ¨∞¥Ω◊∂Œ¥¶¿Ì«Î«Û£¨∂¯∂‘”⁄ngx_http_request_handler∑Ω∑®£¨
»Áπ˚ngx_http_process_request√ªƒ‹¥¶¿ÌÕÍ«Î«Û£¨’‚∏ˆ«Î«Û…œµƒ ¬º˛‘Ÿ¥Œ±ª¥•∑¢£¨ƒ«æÕΩ´”…¥À∑Ω∑®ºÃ–¯¥¶¿Ì¡À°£

µ±Ω‚ŒˆµΩÕÍ’˚µƒÕ∑≤ø––∫Õ«Î«Û––∫Û£¨≤ªª·‘Ÿ–Ë“™Ω” ’HTTP«Î«Û––ªÚ’ﬂÕ∑≤ø£¨À˘“‘–Ë“™÷ÿ–¬…Ë÷√µ±«∞¡¨Ω”∂¡/–¥ ¬º˛µƒªÿµ˜∑Ω∑®°£Ω´Õ¨ ±∞—∂¡ ¬º˛°¢–¥ ¬º˛µƒªÿµ˜
∑Ω∑®∂º…Ë÷√Œ™ngx_http_request_handler∑Ω∑®£¨«Î«Ûµƒ∫Û–¯¥¶¿Ì∂º «Õ®π˝ngx_http_request_handler∑Ω∑®Ω¯––µƒ°£

HTTPøÚº‹Œﬁ¬€ «µ˜”√ngx_http_process_request∑Ω∑®£® ◊¥Œ¥”“µŒÒ…œ¥¶¿Ì«Î«Û£©ªπ «ngx_http_request_handler∑Ω∑®£®TCP¡¨Ω”…œ∫Û–¯µƒ ¬º˛¥•∑¢ ±£©¥¶¿Ì
«Î«Û£¨◊Ó∫Û∂º”–“ª∏ˆ≤Ω÷Ë£¨æÕ «µ˜”√ngx_http_run_posted_requests∑Ω∑®¥¶¿Ìpost«Î«Û
*/ 
//øÕªß∂À ¬º˛¥¶¿Ìhandler“ª∞„(write(read)->handler)“ª∞„Œ™ngx_http_request_handler£¨ ∫Õ∫Û∂Àµƒhandler“ª∞„(write(read)->handler)“ª∞„Œ™ngx_http_upstream_handler
static void
ngx_http_request_handler(ngx_event_t *ev)
{
    ngx_connection_t    *c;
    ngx_http_request_t  *r;

/*
ngx_http_request_handler «HTTP«Î«Û…œ∂¡/–¥ ¬º˛µƒªÿµ˜∑Ω∑®°£‘⁄ngx_event_tΩ·ππÃÂ±Ì æµƒ ¬º˛÷–£¨data≥…‘±÷∏œÚ¡À’‚∏ˆ ¬º˛∂‘”¶µƒngx_connection_t¡¨Ω”£¨
‘⁄HTTPøÚº‹µƒngx_connection_tΩ·ππÃÂ÷–µƒdata≥…‘±‘Ú÷∏œÚ¡Àngx_http_request_tΩ·ππÃÂ
*/
    c = ev->data;
    r = c->data;

    ngx_http_set_log_request(c->log, r);

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http run request(ev->write:%d): \"%V?%V\"", ev->write, &r->uri, &r->args);

/*
 ºÏ≤È’‚∏ˆ ¬º˛µƒwriteø…–¥±Í÷æ£¨»Áπ˚write±Í÷æŒ™l£¨‘Úµ˜”√ngx_http_request_tΩ·ππÃÂ÷–µƒwrite event- handler∑Ω∑®°£◊¢“‚£¨Œ“√«‘⁄ngx_http_handler
 ∑Ω∑®÷–“—æ≠Ω´write_event_handler…Ë÷√Œ™ngx_http_core_run_phases∑Ω∑®£¨∂¯“ª∞„Œ“√«ø™∑¢µƒ≤ªÃ´∏¥‘”µƒHTTPƒ£øÈ «≤ªª·÷ÿ–¬…Ë÷√write_event_handler∑Ω
 ∑®µƒ£¨“Ú¥À£¨“ªµ©”–ø…–¥ ¬º˛ ±£¨æÕª·ºÃ–¯∞¥’’¡˜≥Ã÷¥––ngx_http_core_run_phases∑Ω∑®£¨≤¢ºÃ–¯∞¥Ω◊∂Œµ˜”√∏˜∏ˆHTTPƒ£øÈ µœ÷µƒ∑Ω∑®¥¶¿Ì«Î«Û°£

»Áπ˚“ª∏ˆ ¬º˛µƒ∂¡–¥±Í÷æÕ¨ ±Œ™1 ±£¨Ωˆwrite_event_handler∑Ω∑®ª·±ªµ˜”√£¨º¥ø…–¥ ¬º˛µƒ¥¶¿Ì”≈œ»”⁄ø…∂¡ ¬º˛£®’‚’˝ «Nginx∏ﬂ–‘ƒ‹…Ëº∆µƒÃÂœ÷£¨
”≈œ»¥¶¿Ìø…–¥ ¬º˛ø…“‘æ°øÏ Õ∑≈ƒ⁄¥Ê£¨æ°¡ø±£≥÷∏˜HTTPƒ£øÈ…Ÿ π”√ƒ⁄¥Ê“‘Ã·∏ﬂ≤¢∑¢ƒ‹¡¶£©°£“ÚŒ™∑˛ŒÒ∆˜∑¢ÀÕ∏¯øÕªß∂Àµƒ±®Œƒ≥§∂»“ª∞„±»«Î«Û±®Œƒ¥Û∫‹∂‡
 */
   //µ±evŒ™ngx_connection_t->write ƒ¨»œwriteŒ™1£ªµ±evŒ™ngx_connection_t->read ƒ¨»œwriteŒ™0
    if (ev->write) { //Àµ√˜ev «ngx_connection_t->write
        r->write_event_handler(r); //ngx_http_core_run_phases

    } else {//Àµ√˜ev «ngx_connection_t->read ¬º˛ 
        r->read_event_handler(r);
    }

/*
HTTPøÚº‹Œﬁ¬€ «µ˜”√ngx_http_process_request∑Ω∑®£® ◊¥Œ¥”“µŒÒ…œ¥¶¿Ì«Î«Û£©ªπ «ngx_http_request_handler∑Ω∑®£®TCP¡¨Ω”…œ∫Û–¯µƒ ¬º˛¥•∑¢ ±£©¥¶¿Ì
«Î«Û£¨◊Ó∫Û∂º”–“ª∏ˆ≤Ω÷Ë£¨æÕ «µ˜”√ngx_http_run_posted_requests∑Ω∑®¥¶¿Ìpost«Î«Û
*/
/* ngx_http_process_request∫Õngx_http_request_handler’‚¡Ω∏ˆ∑Ω∑®µƒπ≤Õ®÷Æ¥¶‘⁄”⁄£¨À¸√«∂ºª·œ»∞¥Ω◊∂Œµ˜”√∏˜∏ˆHTTPƒ£øÈ¥¶¿Ì«Î«Û£¨‘Ÿ¥¶¿Ìpost«Î«Û */
    ngx_http_run_posted_requests(c);
}



/*
    sub1_r∫Õsub2_r∂º «Õ¨“ª∏ˆ∏∏«Î«Û£¨æÕ «root_r«Î«Û£¨sub1_r∫Õsub2_ræÕ «ngx_http_postponed_request_s->request≥…‘±
    À¸√«”…ngx_http_postponed_request_s->next¡¨Ω”‘⁄“ª∆£¨≤Œøºngx_http_subrequest
    
        
                          -----root_r(÷˜«Î«Û)     
                          |postponed
                          |                next
            -------------sub1_r(data1)--------------sub2_r(data1)
            |                                       |postponed                    
            |postponed                              |
            |                                     sub21_r-----sub22
            |
            |               next
          sub11_r(data11)-----------sub12_r(data12)

    Õº÷–µƒrootΩ⁄µ„º¥Œ™÷˜«Î«Û£¨À¸µƒpostponed¡¥±Ì¥”◊Û÷¡”“π“‘ÿ¡À3∏ˆΩ⁄µ„£¨SUB1 «À¸µƒµ⁄“ª∏ˆ◊”«Î«Û£¨DATA1 «À¸≤˙…˙µƒ“ª∂Œ ˝æ›£¨SUB2 «À¸µƒµ⁄2∏ˆ◊”«Î«Û£¨
∂¯«“’‚2∏ˆ◊”«Î«Û∑÷±”–À¸√«◊‘º∫µƒ◊”«Î«Ûº∞ ˝æ›°£ngx_connection_t÷–µƒdata◊÷∂Œ±£¥Êµƒ «µ±«∞ø…“‘Õ˘out chain∑¢ÀÕ ˝æ›µƒ«Î«Û£¨Œƒ’¬ø™Õ∑ÀµµΩ∑¢µΩøÕªß∂À
µƒ ˝æ›±ÿ–Î∞¥’’◊”«Î«Û¥¥Ω®µƒÀ≥–Ú∑¢ÀÕ£¨’‚¿Ôº¥ «∞¥∫Û–¯±È¿˙µƒ∑Ω∑®£®SUB11->DATA11->SUB12->DATA12->(SUB1)->DATA1->SUB21->SUB22->(SUB2)->(ROOT)£©£¨
…œÕº÷–µ±«∞ƒ‹πªÕ˘øÕªß∂À£®out chain£©∑¢ÀÕ ˝æ›µƒ«Î«Ûœ‘»ªæÕ «SUB11£¨»Áπ˚SUB12Ã·«∞÷¥––ÕÍ≥…£¨≤¢≤˙…˙ ˝æ›DATA121£¨÷ª“™«∞√ÊÀ¸ªπ”–Ω⁄µ„Œ¥∑¢ÀÕÕÍ±œ£¨
DATA121÷ªƒ‹œ»π“‘ÿ‘⁄SUB12µƒpostponed¡¥±Ìœ¬°£’‚¿Ôªπ“™◊¢“‚“ªœ¬µƒ «c->dataµƒ…Ë÷√£¨µ±SUB11÷¥––ÕÍ≤¢«“∑¢ÀÕÕÍ ˝æ›÷Æ∫Û£¨œ¬“ª∏ˆΩ´“™∑¢ÀÕµƒΩ⁄µ„”¶∏√ «
DATA11£¨µ´ «∏√Ω⁄µ„ µº …œ±£¥Êµƒ « ˝æ›£¨∂¯≤ª «◊”«Î«Û£¨À˘“‘c->data’‚ ±”¶∏√÷∏œÚµƒ «”µ”–∏ƒ ˝æ›Ω⁄µ„µƒSUB1«Î«Û°£

∑¢ÀÕ ˝æ›µΩøÕªß∂À”≈œ»º∂:
1.◊”«Î«Û”≈œ»º∂±»∏∏«Î«Û∏ﬂ
2.Õ¨º∂(“ª∏ˆr≤˙…˙∂‡∏ˆ◊”«Î«Û)«Î«Û£¨¥”◊ÛµΩ”“”≈œ»º∂”…∏ﬂµΩµÕ(“ÚŒ™œ»¥¥Ω®µƒ◊”«Î«Ûœ»∑¢ÀÕ ˝æ›µΩøÕªß∂À)
∑¢ÀÕ ˝æ›µΩøÕªß∂ÀÀ≥–Úøÿ÷∆º˚ngx_http_postpone_filter   nginxÕ®π˝◊”«Î«Û∑¢ÀÕ ˝æ›µΩ∫Û∂Àº˚ngx_http_run_posted_requests
*/

//subrequest◊¢“‚ngx_http_run_posted_requests”Îngx_http_subrequest ngx_http_postpone_filter ngx_http_finalize_request≈‰∫œ‘ƒ∂¡

/*
HTTPøÚº‹Œﬁ¬€ «µ˜”√ngx_http_process_request∑Ω∑®£® ◊¥Œ¥”“µŒÒ…œ¥¶¿Ì«Î«Û£©ªπ «ngx_http_request_handler∑Ω∑®£®TCP¡¨Ω”…œ∫Û–¯µƒ ¬º˛¥•∑¢ ±£©¥¶¿Ì
«Î«Û£¨◊Ó∫Û∂º”–“ª∏ˆ≤Ω÷Ë£¨æÕ «µ˜”√ngx_http_run_posted_requests∑Ω∑®¥¶¿Ìpost«Î«Û

ngx_http_run_posted_requests∫Ø ˝”÷ «‘⁄ ≤√¥ ±∫Úµ˜”√£øÀ¸ µº …œ «‘⁄ƒ≥∏ˆ«Î«Ûµƒ∂¡£®–¥£© ¬º˛µƒhandler÷–£¨÷¥––ÕÍ∏√«Î«Ûœ‡πÿµƒ¥¶¿Ì∫Û±ªµ˜”√£¨
±»»Á÷˜«Î«Û‘⁄◊ﬂÕÍ“ª±ÈPHASEµƒ ±∫Úª·µ˜”√ngx_http_run_posted_requests£¨’‚ ±◊”«Î«Ûµ√“‘‘À––°£

11∏ˆΩ◊∂Œ÷¥––ÕÍ±œ∫Û£¨µ˜”√ngx_http_run_posted_requests∑Ω∑®÷¥––post«Î«Û£¨’‚¿Ô“ª∞„∂º «∂‘subrequestΩ¯––¥¶¿Ì
*/
/* 
ngx_http_upstream_handler∫Õngx_http_process_request∂ºª·÷¥––∏√∫Ø ˝
*/
void //ngx_http_post_requestΩ´∏√◊”«Î«Ûπ“‘ÿ‘⁄÷˜«Î«Ûµƒposted_requests¡¥±Ì∂”Œ≤£¨‘⁄ngx_http_run_posted_requests÷–÷¥––
ngx_http_run_posted_requests(ngx_connection_t *c) //÷¥––r->main->posted_requests¡¥±Ì÷–À˘”–Ω⁄µ„µƒ->write_event_handler()
{ //subrequest◊¢“‚ngx_http_run_posted_requests”Îngx_http_postpone_filter ngx_http_finalize_request≈‰∫œ‘ƒ∂¡
    ngx_http_request_t         *r;
    ngx_http_posted_request_t  *pr;

    /*
    »Áπ˚”≈œ»º∂µÕµƒ◊”«Î«Ûµƒ ˝æ›œ»µΩ¥Ô£¨‘Úœ»Õ®π˝ngx_http_postpone_filter->ngx_http_postpone_filter_addª∫¥ÊµΩr->postpone£¨
    »ª∫ÛrÃÌº”µΩpr->request->posted_requests,◊Ó∫Û‘⁄∏ﬂ”≈œ»º∂«Î«Û∫Û∂À ˝æ›µΩ¿¥∫Û£¨ª·∞—÷Æ«∞ª∫¥Ê∆¿¥µƒµÕ”≈œ»º∂«Î«Ûµƒ ˝æ›“≤“ª
    ∆‘⁄ngx_http_run_posted_requests÷–¥•∑¢∑¢ÀÕ£¨¥”∂¯±£÷§’Ê’˝∑¢ÀÕµΩøÕªß∂À ˝æ› ±∞¥’’◊”«Î«Û”≈œ»º∂À≥–Ú∑¢ÀÕµƒ
    */
    for ( ;; ) {/* ±È¿˙∏√øÕªß∂À«Î«ÛrÀ˘∂‘”¶µƒÀ˘”–subrequest£¨»ª∫Û∞—’‚–©subrequest«Î«Û◊ˆ÷ÿ∂®œÚ¥¶¿Ì */ 

        /*  ◊œ»ºÏ≤È¡¨Ω” «∑Ò“—œ˙ªŸ£¨»Áπ˚¡¨Ω”±ªœ˙ªŸ£¨æÕΩ· ¯ngx_http_run_posted_requests∑Ω∑® */
        if (c->destroyed) {
            return;
        }

        r = c->data;
        pr = r->main->posted_requests;

        /*
        ∏˘æ›ngx_http_request_tΩ·ππÃÂ÷–µƒmain≥…‘±’“µΩ‘≠ ºøÕªß∂À«Î«Û£¨’‚∏ˆ‘≠ º«Î«Ûµƒposted_requests≥…‘±÷∏œÚ¥˝¥¶¿Ìµƒpost«Î«Û◊È≥…µƒµ•¡¥±Ì£¨
        »Áπ˚posted_requests÷∏œÚNULLø’÷∏’Î£¨‘ÚΩ· ¯ngx_http_run_posted_requests∑Ω∑®£¨∑Ò‘Ú»°≥ˆ¡¥±Ì÷– ◊∏ˆ÷∏œÚpost«Î«Ûµƒ÷∏’Î
          */
        if (pr == NULL) { //«Î«Ûr√ª”–◊”«Î«Û
            return;
        }

       /*
        Ω´‘≠ º«Î«Ûµƒposted_requests÷∏’Î÷∏œÚ¡¥±Ì÷–œ¬“ª∏ˆpost«Î«Û£®Õ®π˝µ⁄1∏ˆpost«Î«Ûµƒnext÷∏’Îø…“‘ªÒµ√£©£¨µ±»ª£¨œ¬“ª∏ˆpost«Î«Û”–ø…ƒ‹≤ª¥Ê‘⁄£¨
        ’‚‘⁄œ¬“ª¥Œ—≠ª∑÷–æÕª·ºÏ≤‚µΩ°£
        */
        r->main->posted_requests = pr->next;

        r = pr->request; /* ªÒ»°◊”«Î«Ûµƒr */

        ngx_http_set_log_request(c->log, r);

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "http posted request: \"%V?%V\"", &r->uri, &r->args);

          /*
          µ˜”√’‚∏ˆpost«Î«Ûngx_http_request_tΩ·ππÃÂ÷–µƒwrite event handler∑Ω∑®°£Œ™ ≤√¥≤ª «÷¥––read_ event_ handler∑Ω∑®ƒÿ£ø‘≠“Ú
          ∫‹ºÚµ•£¨◊”«Î«Û≤ª «±ªÕ¯¬Á ¬º˛«˝∂Øµƒ£¨“Ú¥À£¨÷¥––post«Î«Û ±æÕœ‡µ±”⁄”–ø…–¥ ¬º˛£¨”…Nginx÷˜∂Ø◊ˆ≥ˆ∂Ø◊˜°£

          “ª∞„◊”«Î«Ûµƒwrite_event_handler‘⁄ngx_http_set_write_handler÷–…Ë÷√Œ™ngx_http_writer
          */ 

        /* r“—æ≠ «◊”«Î«Ûµƒr¡À£¨“≤æÕ «ngx_http_subrequest÷–¥¥Ω®µƒr,÷¥––◊”«Î«Ûµƒwrite_event_handler£¨ngx_http_handler£¨’‚
        ¿Ô√Êª·∂‘◊”«Î«Ûr◊ˆ÷ÿ∂®œÚ */
        r->write_event_handler(r); 
    }
}

//∞—prÃÌº”µΩr->main->posted_requestsŒ≤≤ø
ngx_int_t //ngx_http_post_requestΩ´∏√◊”«Î«Ûπ“‘ÿ‘⁄÷˜«Î«Ûµƒposted_requests¡¥±Ì∂”Œ≤£¨‘⁄ngx_http_run_posted_requests÷–÷¥––
ngx_http_post_request(ngx_http_request_t *r, ngx_http_posted_request_t *pr)
{ //◊¢“‚ «∞—–¬¥¥Ω®µƒngx_http_posted_request_tÃÌº”µΩ◊Ó…œ≤„rµƒposted_requests÷–(¿˝»Áœ÷‘⁄ «µ⁄Àƒ≤„r£¨‘Ú◊Ó…œ≤„ «µ⁄“ª¥Œr£¨∂¯≤ª «µ⁄»˝≤„r)
    ngx_http_posted_request_t  **p;

    if (pr == NULL) {
        pr = ngx_palloc(r->pool, sizeof(ngx_http_posted_request_t));
        if (pr == NULL) {
            return NGX_ERROR;
        }
    }

    pr->request = r;
    pr->next = NULL;

    for (p = &r->main->posted_requests; *p; p = &(*p)->next) { /* void */ }

    *p = pr;

    return NGX_OK;
}


/*
    ∂‘”⁄ ¬º˛«˝∂Øµƒº‹ππ¿¥Àµ£¨Ω· ¯«Î«Û «“ªœÓ∏¥‘”µƒπ§◊˜°£“ÚŒ™“ª∏ˆ«Î«Ûø…ƒ‹ª·±ª–Ì∂‡∏ˆ ¬º˛¥•∑¢£¨’‚ πµ√NginxøÚº‹µ˜∂»µΩƒ≥∏ˆ«Î«Ûµƒªÿµ˜∑Ω∑®
 ±£¨‘⁄µ±«∞“µŒÒƒ⁄À∆∫ı–Ë“™Ω· ¯HTTP«Î«Û£¨µ´»Áπ˚’ÊµƒΩ· ¯¡À«Î«Û£¨œ˙ªŸ¡À”Î«Î«Ûœ‡πÿµƒƒ⁄¥Ê£¨∂‡∞Îª·‘Ï≥…÷ÿ¥Û¥ÌŒÛ£¨“ÚŒ™’‚∏ˆ«Î«Ûø…ƒ‹ªπ”–∆‰
À˚ ¬º˛‘⁄∂® ±∆˜ªÚ’ﬂepoll÷–°£µ±’‚–© ¬º˛±ªªÿµ˜ ±£¨«Î«Û»¥“—æ≠≤ª¥Ê‘⁄¡À£¨’‚æÕ «—œ÷ÿµƒƒ⁄¥Ê∑√Œ ‘ΩΩÁ¥ÌŒÛ£°»Áπ˚≥¢ ‘‘⁄ Ù”⁄ƒ≥∏ˆHTTPƒ£øÈµƒ
ªÿµ˜∑Ω∑®÷– ‘ÕºΩ· ¯«Î«Û£¨œ»“™∞—’‚∏ˆ«Î«Ûœ‡πÿµƒÀ˘”– ¬º˛£®”––© ¬º˛ø…ƒ‹ Ù”⁄∆‰À˚HTTPƒ£øÈ£©∂º¥”∂® ±∆˜∫Õepoll÷–»°≥ˆ≤¢µ˜”√∆‰handler∑Ω∑®£¨
’‚”÷Ã´∏¥‘”¡À£¨¡ÌÕ‚£¨≤ªÕ¨HTTPƒ£øÈ…œµƒ¥˙¬ÎÒÓ∫œÃ´ΩÙ√‹Ω´ª·ƒ—“‘Œ¨ª§°£
    ƒ«HTTPøÚº‹”÷ «‘ı—˘Ω‚æˆ’‚∏ˆŒ Ã‚µƒƒÿ£øHTTPøÚº‹∞—“ª∏ˆ«Î«Û∑÷Œ™∂‡÷÷∂Ø◊˜£¨»Áπ˚HTTPøÚº‹Ã·π©µƒ∑Ω∑®ª·µº÷¬Nginx‘Ÿ¥Œµ˜∂»µΩ«Î«Û£®¿˝»Á£¨
‘⁄’‚∏ˆ∑Ω∑®÷–≤˙…˙¡À–¬µƒ ¬º˛£¨ªÚ’ﬂ÷ÿ–¬Ω´“—”– ¬º˛ÃÌº”µΩepollªÚ’ﬂ∂® ±∆˜÷–£©£¨ƒ«√¥ø…“‘»œŒ™’‚“ª≤Ωµ˜”√ «“ª÷÷∂¿¡¢µƒ∂Ø◊˜°£¿˝»Á£¨Ω” ’HTTP
«Î«Ûµƒ∞¸ÃÂ°¢µ˜”√upstreamª˙÷∆Ã·π©µƒ∑Ω∑®∑√Œ µ⁄»˝∑Ω∑˛ŒÒ°¢≈……˙≥ˆsubrequest◊”«Î«Ûµ»°£’‚–©À˘ŒΩ∂¿¡¢µƒ∂Ø◊˜£¨∂º «‘⁄∏ÊÀﬂNginx£¨»Áπ˚ª˙ª·∫œ
  æÕ‘Ÿ¥Œµ˜”√À¸√«¥¶¿Ì«Î«Û£¨“ÚŒ™’‚∏ˆ∂Ø◊˜≤¢≤ª «Nginxµ˜”√“ª¥ŒÀ¸√«µƒ∑Ω∑®æÕø…“‘¥¶¿ÌÕÍ±œµƒ°£“Ú¥À£¨√ø“ª÷÷∂Ø◊˜∂‘”⁄’˚∏ˆ«Î«Û¿¥Àµ∂º «∂¿¡¢µƒ£¨
HTTPøÚº‹œ£Õ˚√ø∏ˆ∂Ø◊˜Ω· ¯ ±ΩˆŒ¨ª§◊‘º∫µƒ“µŒÒ£¨≤ª”√»•πÿ–ƒ’‚∏ˆ«Î«Û «∑Òªπ◊ˆ¡À∆‰À˚∂Ø◊˜°£’‚÷÷…Ëº∆¥Û¥ÛΩµµÕ¡À∏¥‘”∂»°£
    ’‚÷÷…Ëº∆æﬂÃÂ”÷ «‘ı√¥ µœ÷µƒƒÿ£ø√ø∏ˆHTTP«Î«Û∂º”–“ª∏ˆ“˝”√º∆ ˝£¨√ø≈……˙≥ˆ“ª÷÷–¬µƒª·∂¿¡¢œÚ ¬º˛ ’ºØ∆˜◊¢≤· ¬º˛µƒ∂Ø◊˜ ±£®»Ángx_http_
read_ client_request_body∑Ω∑®ªÚ’ﬂngx_http_subrequest∑Ω∑®£©£¨∂ºª·∞—“˝”√º∆ ˝º”1£¨’‚—˘√ø∏ˆ∂Ø◊˜Ω· ¯ ±∂ºÕ®π˝µ˜”√ngx_http_finalize_request∑Ω∑®
¿¥Ω· ¯«Î«Û£¨∂¯ngx_http_finalize_request∑Ω∑® µº …œ»¥ª·‘⁄“˝”√º∆ ˝ºı1∫Ûœ»ºÏ≤È“˝”√º∆ ˝µƒ÷µ£¨»Áπ˚≤ªŒ™O «≤ªª·’Ê’˝œ˙ªŸ«Î«Ûµƒ°£
*/ 


//ngx_http_finalize_request -> ngx_http_finalize_connection ,◊¢“‚∫Õngx_http_terminate_requestµƒ«¯±
void
ngx_http_finalize_request(ngx_http_request_t *r, ngx_int_t rc) 
{//subrequest◊¢“‚ngx_http_run_posted_requests”Îngx_http_postpone_filter ngx_http_finalize_request≈‰∫œ‘ƒ∂¡
    ngx_connection_t          *c;
    ngx_http_request_t        *pr;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;

    ngx_log_debug7(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http finalize request rc: %d, \"%V?%V\" a:%d, c:%d, b:%d, p:%p",
                   rc, &r->uri, &r->args, r == c->data, r->main->count, (int)r->buffered, r->postponed);

    /*
    NGX_DONE≤Œ ˝±Ì æ≤ª–Ë“™◊ˆ»Œ∫Œ ¬£¨÷±Ω”µ˜”√ngx_http_finalize_connection∑Ω∑®£¨÷Æ∫Ûngx_http_finalize_request∑Ω∑®Ω· ¯°£µ±ƒ≥“ª÷÷∂Ø◊˜
£®»ÁΩ” ’HTTP«Î«Û∞¸ÃÂ£©’˝≥£Ω· ¯∂¯«Î«Ûªπ”–“µŒÒ“™ºÃ–¯¥¶¿Ì ±£¨∂‡∞Î∂º «¥´µ›NGX_DONE≤Œ ˝°£’‚∏ˆngx_http_finalize_connection∑Ω∑®ªπª·»•ºÏ
≤È“˝”√º∆ ˝«Èøˆ£¨≤¢≤ª“ª∂®ª·œ˙ªŸ«Î«Û°£
     */
    if (rc == NGX_DONE) {
        ngx_http_finalize_connection(r);  
        return;
    }

    if (rc == NGX_OK && r->filter_finalize) {
        c->error = 1;
    }

    /*
   NGX_DECLINED≤Œ ˝±Ì æ«Î«Ûªπ–Ë“™∞¥’’11∏ˆHTTPΩ◊∂ŒºÃ–¯¥¶¿Ìœ¬»•£¨’‚ ±–Ë“™ºÃ–¯µ˜”√ngx_http_core_run_phases∑Ω∑®¥¶¿Ì«Î«Û°£’‚
“ª≤Ω÷– ◊œ»ª·∞—ngx_http_request_tΩ·ππÃÂµƒwrite°™event handler…ËŒ™ngx_http_core_run_phases∑Ω∑®°£Õ¨ ±£¨Ω´«Î«Ûµƒcontent_handler≥…‘±
÷√Œ™NULLø’÷∏’Î£¨À¸ «“ª÷÷”√”⁄‘⁄NGX_HTTP_CONTENT_PHASEΩ◊∂Œ¥¶¿Ì«Î«Ûµƒ∑Ω Ω£¨Ω´∆‰…Ë÷√Œ™NULL◊„Œ™¡À»√ngx_http_core_content_phase∑Ω∑®
ø…“‘ºÃ–¯µ˜”√NGX_HTTP_CONTENT_PHASEΩ◊∂Œµƒ∆‰À˚¥¶¿Ì∑Ω∑®°£
     */
    if (rc == NGX_DECLINED) {
        r->content_handler = NULL;
        r->write_event_handler = ngx_http_core_run_phases;
        ngx_http_core_run_phases(r);
        return;
    }

    /*
    ºÏ≤Èµ±«∞«Î«Û «∑ÒŒ™subrequest◊”«Î«Û£¨»Áπ˚ «◊”«Î«Û£¨ƒ«√¥µ˜”√post_subrequestœ¬µƒhandlerªÿµ˜∑Ω∑®°£subrequestµƒ”√∑®£¨ø…“‘ø¥
    µΩpost_subrequest’˝ «¥À ±±ªµ˜”√µƒ°£
     */  /* »Áπ˚µ±«∞«Î«Û «“ª∏ˆ◊”«Î«Û£¨ºÏ≤ÈÀ¸ «∑Ò”–ªÿµ˜handler£¨”–µƒª∞÷¥––÷Æ */  
    if (r != r->main && r->post_subrequest) {//»Áπ˚µ±«∞«Î«Û Ù”⁄ƒ≥∏ˆ‘≠ º«Î«Ûµƒ◊”«Î«Û
        rc = r->post_subrequest->handler(r, r->post_subrequest->data, rc); //r±‰¡ø «◊”«Î«Û£®≤ª «∏∏«Î«Û£©
    }

    if (rc == NGX_ERROR
        || rc == NGX_HTTP_REQUEST_TIME_OUT
        || rc == NGX_HTTP_CLIENT_CLOSED_REQUEST
        || c->error)
    {
        //÷±Ω”µ˜”√ngx_http_terminate_request∑Ω∑®«ø÷∆Ω· ¯«Î«Û£¨Õ¨ ±£¨ngx_http_finalize_request∑Ω∑®Ω· ¯°£
        if (ngx_http_post_action(r) == NGX_OK) {
            return;
        }

        if (r->main->blocked) {
            r->write_event_handler = ngx_http_request_finalizer;
        }

        ngx_http_terminate_request(r, rc);
        return;
    }

    /*
    »Áπ˚rcŒ™NGX_HTTP_NO_CONTENT°¢NGX_HTTP_CREATEDªÚ’ﬂ¥Û”⁄ªÚµ»”⁄NGX_HTTP_SPECIAL_RESPONSE£¨‘Ú±Ì æ«Î«Ûµƒ∂Ø◊˜ «…œ¥´Œƒº˛£¨
    ªÚ’ﬂHTTPƒ£øÈ–Ë“™HTTPøÚº‹ππ‘Ï≤¢∑¢ÀÕœÏ”¶¬Î¥Û”⁄ªÚµ»”⁄300“‘…œµƒÃÿ ‚œÏ”¶
     */
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE
        || rc == NGX_HTTP_CREATED
        || rc == NGX_HTTP_NO_CONTENT)
    {
        if (rc == NGX_HTTP_CLOSE) {
            ngx_http_terminate_request(r, rc);
            return;
        }

        /*
            ºÏ≤Èµ±«∞«Î«Ûµƒmain «∑Ò÷∏œÚ◊‘º∫£¨»Áπ˚ «£¨’‚∏ˆ«Î«ÛæÕ «¿¥◊‘øÕªß∂Àµƒ‘≠ º«Î«Û£®∑«◊”«Î«Û£©£¨’‚ ±ºÏ≤È∂¡/–¥ ¬º˛µƒtimer_set±Í÷æŒª£¨
            »Áπ˚timer_setŒ™1£¨‘Ú±Ì√˜ ¬º˛‘⁄∂® ±∆˜…Í£¨–Ë“™µ˜”√ngx_del_timer∑Ω∑®∞—∂¡/–¥ ¬º˛¥”∂® ±∆˜÷–“∆≥˝°£
          */
        if (r == r->main) {
            if (c->read->timer_set) {
                ngx_del_timer(c->read, NGX_FUNC_LINE);
            }

            if (c->write->timer_set) {
                ngx_del_timer(c->write, NGX_FUNC_LINE);
            }
        }

        /* …Ë÷√∂¡£Ø–¥ ¬º˛µƒªÿµ˜∑Ω∑®Œ™ngx_http_request_handler∑Ω∑®£¨’‚∏ˆ∑Ω∑®£¨À¸ª·ºÃ–¯¥¶¿ÌHTTP«Î«Û°£ */
        c->read->handler = ngx_http_request_handler;
        c->write->handler = ngx_http_request_handler;

    /*
      µ˜”√ngx_http_special_response_handler∑Ω∑®£¨∏√∑Ω∑®∏∫‘∏˘æ›rc≤Œ ˝ππ‘ÏÕÍ’˚µƒHTTPœÏ”¶°£Œ™ ≤√¥ø…“‘‘⁄’‚“ª≤Ω÷–ππ‘Ï’‚—˘µƒœÏ”¶ƒÿ£ø
      ’‚ ±rc“™√¥ «±Ì æ…œ¥´≥…π¶µƒ201ªÚ’ﬂ204£¨“™√¥æÕ «±Ì æ“Ï≤Ωµƒ300“‘…œµƒœÏ”¶¬Î£¨∂‘”⁄’‚–©«Èøˆ£¨∂º «ø…“‘»√HTTPøÚº‹∂¿¡¢ππ‘ÏœÏ”¶∞¸µƒ°£
      */
        ngx_http_finalize_request(r, ngx_http_special_response_handler(r, rc));
        return;
    }

    if (r != r->main) { //◊”«Î«Û
         /* ∏√◊”«Î«Ûªπ”–Œ¥¥¶¿ÌÕÍµƒ ˝æ›ªÚ’ﬂ◊”«Î«Û */  
        if (r->buffered || r->postponed) { //ºÏ≤Èoutª∫≥Â«¯ƒ⁄ «∑Òªπ”–√ª∑¢ÀÕÕÍµƒœÏ”¶
             /* ÃÌº”“ª∏ˆ∏√◊”«Î«Ûµƒ–¥ ¬º˛£¨≤¢…Ë÷√∫œ  µƒwrite event hander£¨ 
               “‘±„œ¬¥Œ–¥ ¬º˛¿¥µƒ ±∫ÚºÃ–¯¥¶¿Ì£¨’‚¿Ô µº …œœ¬¥Œ÷¥–– ±ª·µ˜”√ngx_http_output_filter∫Ø ˝£¨ 
               ◊Ó÷’ªπ «ª·Ω¯»Îngx_http_postpone_filterΩ¯––¥¶¿Ì£¨‘⁄∏√∫Ø ˝÷–≤ª“ª∂®ª·∞— ˝æ›∑¢ÀÕ≥ˆ»•£¨∂¯ «π“Ω”µΩpostpone¡¥…œ£¨µ»∏ﬂ”≈œ»º∂µƒ◊”«Î«Ûœ»∑¢ÀÕ */ 
            if (ngx_http_set_write_handler(r) != NGX_OK) { 
                ngx_http_terminate_request(r, 0);
            }

            return;
        }

        /*
            ”…”⁄µ±«∞«Î«Û «◊”«Î«Û£¨ƒ«√¥’˝≥£«Èøˆœ¬–Ë“™Ã¯µΩÀ¸µƒ∏∏«Î«Û…œ£¨º§ªÓ∏∏«Î«ÛºÃ–¯œÚœ¬÷¥––£¨À˘“‘’‚“ª≤Ω ◊œ»∏˘æ›ngx_http_request_tΩ·
        ππÃÂµƒparent≥…‘±’“µΩ∏∏«Î«Û£¨‘Ÿππ‘Ï“ª∏ˆngx_http_posted_request_tΩ·ππÃÂ∞—∏∏«Î«Û∑≈÷√∆‰÷–£¨◊Ó∫Û∞—∏√Ω·ππÃÂÃÌº”µΩ‘≠ º«Î«Ûµƒ
        posted_requests¡¥±Ì÷–£¨’‚—˘ngx_http_run_posted_requests∑Ω∑®æÕª·µ˜”√∏∏«Î«Ûµƒwrite_event_handler∑Ω∑®¡À°£
        */
        pr = r->parent;

        /*
          sub1_r∫Õsub2_r∂º «Õ¨“ª∏ˆ∏∏«Î«Û£¨æÕ «root_r«Î«Û£¨sub1_r∫Õsub2_ræÕ «ngx_http_postponed_request_s->request≥…‘±
          À¸√«”…ngx_http_postponed_request_s->next¡¨Ω”‘⁄“ª∆£¨≤Œøºngx_http_subrequest

                          -----root_r(÷˜«Î«Û)     
                          |postponed
                          |                next
            -------------sub1_r(data1)--------------sub2_r(data1)
            |                                       |postponed                    
            |postponed                              |
            |                                     sub21_r-----sub22
            |
            |               next
          sub11_r(data11)-----------sub12_r(data12)

     */
        if (r == c->data) { 
        //’‚∏ˆ”≈œ»º∂◊Ó∏ﬂµƒ◊”«Î«Û ˝æ›∑¢ÀÕÕÍ±œ¡À£¨‘Ú÷±Ω”¥”pr->postponed÷–’™≥˝£¨¿˝»Á’‚¥Œ’™≥˝µƒ «sub11_r£¨‘Úœ¬∏ˆ”≈œ»º∂◊Ó∏ﬂ∑¢ÀÕøÕªß∂À ˝æ›µƒ «sub12_r

            r->main->count--; /* ‘⁄…œ√Êµƒrc = r->post_subrequest->handler()“—æ≠¥¶¿Ì∫√¡À∏√◊”«Î«Û£¨‘Úºı1 */
            r->main->subrequests++;

            if (!r->logged) {

                clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

                if (clcf->log_subrequest) {
                    ngx_http_log_request(r);
                }

                r->logged = 1;

            } else {
                ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                              "subrequest: \"%V?%V\" logged again",
                              &r->uri, &r->args);
            }

            r->done = 1; /* ∏√◊”«Î«Ûµƒhandler“—æ≠¥¶¿ÌÕÍ±œ */
             /* »Áπ˚∏√◊”«Î«Û≤ª «Ã·«∞ÕÍ≥…£¨‘Ú¥”∏∏«Î«Ûµƒpostponed¡¥±Ì÷–…æ≥˝ */  
            if (pr->postponed && pr->postponed->request == r) {
                pr->postponed = pr->postponed->next;
            }

            /* Ω´∑¢ÀÕ»®¿˚“∆Ωª∏¯∏∏«Î«Û£¨∏∏«Î«Ûœ¬¥Œ÷¥––µƒ ±∫Úª·∑¢ÀÕÀ¸µƒpostponed¡¥±Ì÷–ø…“‘ 
               ∑¢ÀÕµƒ ˝æ›Ω⁄µ„£¨ªÚ’ﬂΩ´∑¢ÀÕ»®¿˚“∆Ωª∏¯À¸µƒœ¬“ª∏ˆ◊”«Î«Û */ 
            c->data = pr;
        } else {

            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "http finalize non-active request: \"%V?%V\"",
                           &r->uri, &r->args);
            /* µΩ’‚¿Ô∆‰ µ±Ì√˜∏√◊”«Î«ÛÃ·«∞÷¥––ÕÍ≥…£¨∂¯«“À¸√ª”–≤˙…˙»Œ∫Œ ˝æ›£¨‘ÚÀ¸œ¬¥Œ‘Ÿ¥ŒªÒµ√ 
               ÷¥––ª˙ª· ±£¨Ω´ª·÷¥––ngx_http_request_finalzier∫Ø ˝£¨À¸ µº …œ «÷¥–– 
               ngx_http_finalize_request£®r,0£©£¨“≤æÕ « ≤√¥∂º≤ª∏…£¨÷±µΩ¬÷µΩÀ¸∑¢ÀÕ ˝æ› ±£¨ 
               ngx_http_finalize_request ∫Ø ˝ª·Ω´À¸¥”∏∏«Î«Ûµƒpostponed¡¥±Ì÷–…æ≥˝ */  
            r->write_event_handler = ngx_http_request_finalizer; //“≤æÕ «”≈œ»º∂µÕµƒ«Î«Û±»”≈œ»º∂∏ﬂµƒ«Î«Ûœ»µ√µΩ∫Û∂À∑µªÿµƒ ˝æ›£¨

            if (r->waited) {
                r->done = 1;
            }
        }

         /* Ω´∏∏«Î«Ûº”»Îposted_request∂”Œ≤£¨ªÒµ√“ª¥Œ‘À––ª˙ª·£¨’‚—˘præÕª·º”»ÎµΩposted_requests£¨
         ‘⁄ngx_http_run_posted_requests÷–æÕø…“‘µ˜”√pr->ngx_http_run_posted_requests */  
        if (ngx_http_post_request(pr, NULL) != NGX_OK) {
            r->main->count++;
            ngx_http_terminate_request(r, 0);
            return;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "http wake parent request: \"%V?%V\"",
                       &pr->uri, &pr->args);

        return;
    }

    /* ’‚¿Ô «¥¶¿Ì÷˜«Î«ÛΩ· ¯µƒ¬ﬂº≠£¨»Áπ˚÷˜«Î«Û”–Œ¥∑¢ÀÕµƒ ˝æ›ªÚ’ﬂŒ¥¥¶¿Ìµƒ◊”«Î«Û£¨ ‘Ú∏¯÷˜«Î«ÛÃÌº”–¥ ¬º˛£¨≤¢…Ë÷√∫œ  µƒwrite event hander£¨ 
       “‘±„œ¬¥Œ–¥ ¬º˛¿¥µƒ ±∫ÚºÃ–¯¥¶¿Ì */  
       
    //ngx_http_request_t->out÷–ªπ”–Œ¥∑¢ÀÕµƒ∞¸ÃÂ£¨
    //ngx_http_finalize_request->ngx_http_set_write_handler->ngx_http_writerÕ®π˝’‚÷÷∑Ω Ω∞—Œ¥∑¢ÀÕÕÍ±œµƒœÏ”¶±®Œƒ∑¢ÀÕ≥ˆ»•
    if (r->buffered || c->buffered || r->postponed || r->blocked) { //¿˝»Áªπ”–Œ¥∑¢ÀÕµƒ ˝æ›£¨º˚ngx_http_copy_filter£¨‘Úbuffered≤ªŒ™0

        if (ngx_http_set_write_handler(r) != NGX_OK) { 
            ngx_http_terminate_request(r, 0);
        }

        return;
    }

    if (r != c->data) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0,
                      "http finalize non-active request: \"%V?%V\"",
                      &r->uri, &r->args);
        return;
    }

    r->done = 1;
    r->write_event_handler = ngx_http_request_empty_handler;

    if (!r->post_action) {
        r->request_complete = 1;
    }

    if (ngx_http_post_action(r) == NGX_OK) {
        return;
    }

    /* µΩ¡À’‚¿Ô’Êµƒ“™Ω· ¯«Î«Û¡À°£ ◊œ»≈–∂œ∂¡£Ø–¥ ¬º˛µƒtimer-set±Í÷æŒª£¨»Áπ˚timer-setŒ™1£¨‘Ú–Ë“™∞—œ‡”¶µƒ∂¡/–¥ ¬º˛¥”∂® ±∆˜÷–“∆≥˝ */

    if (c->read->timer_set) {
        ngx_del_timer(c->read, NGX_FUNC_LINE);
    }

    if (c->write->timer_set) {
        c->write->delayed = 0;
        //’‚¿Ôµƒ∂® ±∆˜“ª∞„‘⁄ngx_http_set_write_handler->ngx_add_timer÷–ÃÌº”µƒ
        ngx_del_timer(c->write, NGX_FUNC_LINE);
    }

    if (c->read->eof) {
        ngx_http_close_request(r, 0);
        return;
    }

    ngx_http_finalize_connection(r);
}

/*
ngx_http_terminate_request∑Ω∑® «Ã·π©∏¯HTTPƒ£øÈ π”√µƒΩ· ¯«Î«Û∑Ω∑®£¨µ´À¸ Ù”⁄∑«’˝≥£Ω· ¯µƒ≥°æ∞£¨ø…“‘¿ÌΩ‚Œ™«ø÷∆πÿ±’«Î«Û°£“≤æÕ «Àµ£¨
µ±µ˜”√ngx_http_terminate_request∑Ω∑®Ω· ¯«Î«Û ±£¨À¸ª·÷±Ω”’“≥ˆ∏√«Î«Ûµƒmain≥…‘±÷∏œÚµƒ‘≠ º«Î«Û£¨≤¢÷±Ω”Ω´∏√‘≠ º«Î«Ûµƒ“˝”√º∆ ˝÷√Œ™1£¨
Õ¨ ±ª·µ˜”√ngx_http_close_request∑Ω∑®»•πÿ±’«Î«Û
*/ //ngx_http_finalize_request -> ngx_http_finalize_connection ,◊¢“‚∫Õngx_http_terminate_requestµƒ«¯±
static void
ngx_http_terminate_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_http_cleanup_t    *cln;
    ngx_http_request_t    *mr;
    ngx_http_ephemeral_t  *e;

    mr = r->main;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http terminate request count:%d", mr->count);

    if (rc > 0 && (mr->headers_out.status == 0 || mr->connection->sent == 0)) {
        mr->headers_out.status = rc;
    }

    cln = mr->cleanup;
    mr->cleanup = NULL;

    while (cln) {
        if (cln->handler) {
            cln->handler(cln->data);
        }

        cln = cln->next;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http terminate cleanup count:%d blk:%d",
                   mr->count, mr->blocked);

    if (mr->write_event_handler) {

        if (mr->blocked) {
            return;
        }

        e = ngx_http_ephemeral(mr);
        mr->posted_requests = NULL;
        mr->write_event_handler = ngx_http_terminate_handler;
        (void) ngx_http_post_request(mr, &e->terminal_posted_request);
        return;
    }

    ngx_http_close_request(mr, rc);
}


static void
ngx_http_terminate_handler(ngx_http_request_t *r)
{
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http terminate handler count:%d", r->count);

    r->count = 1;

    ngx_http_close_request(r, 0);
}

/*
ngx_http_finalize_connection∑Ω∑®À‰»ª±»ngx_http_close_request∑Ω∑®∏ﬂ¡À“ª∏ˆ≤„¥Œ£¨µ´HTTPƒ£øÈ“ª∞„ªπ «≤ªª·÷±Ω”µ˜”√À¸°£
ngx_http_finalize_connection∑Ω∑®‘⁄Ω· ¯«Î«Û ±£¨Ω‚æˆ¡ÀkeepaliveÃÿ–‘∫Õ◊”«Î«ÛµƒŒ Ã‚   ngx_http_finalize_request -> ngx_http_finalize_connection ,◊¢“‚∫Õngx_http_terminate_requestµƒ«¯±
*/ 

//∏√∫Ø ˝”√”⁄≈–∂œ «¿ÌΩ‚πÿ±’¡¨Ω”£¨ªπ «Õ®π˝±£ªÓ≥¨ ±πÿ±’¡¨Ω”£¨ªπ «—”≥Ÿπÿ±’¡¨Ω”
static void
ngx_http_finalize_connection(ngx_http_request_t *r) //ngx_http_finalize_request->ngx_http_finalize_connection
{
    ngx_http_core_loc_conf_t  *clcf;

#if (NGX_HTTP_V2)
    if (r->stream) {
        ngx_http_close_request(r, 0);
        return;
    }
#endif

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    /*
    ≤Èø¥‘≠ º«Î«Ûµƒ“˝”√º∆ ˝£Æ»Áπ˚≤ªµ»”⁄1£¨‘Ú±Ì æªπ”–∂‡∏ˆ∂Ø◊˜‘⁄≤Ÿ◊˜◊≈«Î«Û£¨Ω”◊≈ºÃ–¯ºÏ≤Èdiscard_body±Í÷æŒª°£»Áπ˚
discard_bodyŒ™l£¨‘Ú±Ì æ’˝‘⁄∂™∆˙∞¸ÃÂ£¨’‚ ±ª·‘Ÿ“ª¥Œ∞—«Î«Ûµƒread_event_handler≥…‘±…ËŒ™ngx_http_discarded_request_body_handler∑Ω∑®£¨
     */
    if (r->main->count != 1) {

        if (r->discard_body) {
            r->read_event_handler = ngx_http_discarded_request_body_handler;
            //Ω´∂¡ ¬º˛ÃÌº”µΩ∂® ±∆˜÷–£¨∆‰÷–≥¨ ± ±º‰ «lingering_timeout≈‰÷√œÓ°£
            ngx_add_timer(r->connection->read, clcf->lingering_timeout, NGX_FUNC_LINE);

            if (r->lingering_time == 0) {
                r->lingering_time = ngx_time()
                                      + (time_t) (clcf->lingering_time / 1000);
            }
        }

        ngx_http_close_request(r, 0);
        return;
    }

    if (r->reading_body) {
        r->keepalive = 0; // π”√—”≥Ÿπÿ±’¡¨Ω”π¶ƒ‹£¨æÕ≤ª–Ë“™‘Ÿ≈–∂œkeepaliveπ¶ƒ‹πÿ¡¨Ω”¡À
        r->lingering_close = 1;
    }

    /*
    »Áπ˚“˝”√º∆ ˝Œ™1£¨‘ÚÀµ√˜’‚ ±“™’Êµƒ◊º±∏Ω· ¯«Î«Û¡À°£≤ªπ˝£¨ªπ“™ºÏ≤È«Î«Ûµƒkeepalive≥…‘±£¨»Áπ˚keepaliveŒ™1£¨‘ÚÀµ√˜’‚∏ˆ«Î«Û–Ë“™ Õ∑≈£¨
µ´TCP¡¨Ω”ªπ «“™∏¥”√µƒ£ª»Áπ˚keepaliveŒ™0æÕ≤ª–Ë“™øº¬«keepalive«Î«Û¡À£¨µ´ªπ–Ë“™ºÏ≤‚«Î«Ûµƒlingering_close≥…‘±£¨»Áπ˚lingering_closeŒ™1£¨
‘ÚÀµ√˜–Ë“™—”≥Ÿπÿ±’«Î«Û£¨’‚ ±“≤≤ªƒ‹’Êµƒ»•Ω· ¯«Î«Û£¨»Áπ˚lingering_closeŒ™0£¨≤≈’ÊµƒΩ· ¯«Î«Û°£
     */
    if (!ngx_terminate
         && !ngx_exiting
         && r->keepalive
         && clcf->keepalive_timeout > 0) 
         //»Áπ˚øÕªß∂À«Î«Û–Ø¥¯µƒ±®ŒƒÕ∑÷–…Ë÷√¡À≥§¡¨Ω”£¨≤¢«“Œ“√«µƒkeepalive_timeout≈‰÷√œÓ¥Û”⁄0(ƒ¨»œ75s),‘Ú≤ªƒ‹πÿ±’¡¨Ω”£¨÷ª”–µ»’‚∏ˆ ±º‰µΩ∫Ûªπ√ª”– ˝æ›µΩ¿¥£¨≤≈πÿ±’¡¨Ω”
    {
        ngx_http_set_keepalive(r); 
        return;
    }

    if (clcf->lingering_close == NGX_HTTP_LINGERING_ALWAYS
        || (clcf->lingering_close == NGX_HTTP_LINGERING_ON
            && (r->lingering_close
                || r->header_in->pos < r->header_in->last
                || r->connection->read->ready)))
    {
       /*
        µ˜”√ngx_http_set_lingering_close∑Ω∑®—”≥Ÿπÿ±’«Î«Û°£ µº …œ£¨’‚∏ˆ∑Ω∑®µƒ“‚“ÂæÕ‘⁄”⁄∞—“ª–©±ÿ–Î◊ˆµƒ ¬«È◊ˆÕÍ
        £®»ÁΩ” ’”√ªß∂À∑¢¿¥µƒ◊÷∑˚¡˜£©‘Ÿπÿ±’¡¨Ω”°£
        */
        ngx_http_set_lingering_close(r);
        return;
    }

    ngx_http_close_request(r, 0);
}

//µ˜”√ngx_http_write_filter–¥ ˝æ›£¨»Áπ˚∑µªÿNGX_AGAIN,‘Ú“‘∫Ûµƒ–¥ ˝æ›¥•∑¢Õ®π˝‘⁄ngx_http_set_write_handler->ngx_http_writerÃÌº”epoll write ¬º˛¿¥¥•∑¢
static ngx_int_t
ngx_http_set_write_handler(ngx_http_request_t *r)
{
    ngx_event_t               *wev;
    ngx_http_core_loc_conf_t  *clcf;

    r->http_state = NGX_HTTP_WRITING_REQUEST_STATE;

    r->read_event_handler = r->discard_body ?
                                ngx_http_discarded_request_body_handler:
                                ngx_http_test_reading;
    r->write_event_handler = ngx_http_writer;

#if (NGX_HTTP_V2)
    if (r->stream) {
        return NGX_OK;
    }
#endif

    wev = r->connection->write;

    if (wev->ready && wev->delayed) {
        return NGX_OK;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    if (!wev->delayed) { //±æ¥Œngx_http_write_filter√ª”–∞— ˝æ›∑¢ÀÕÕÍ±œ£¨‘Ú–Ë“™ÃÌº”–¥ ¬º˛∂® ±∆˜
    /*
    “ÚŒ™≥¨ ±µƒ‘≠“Ú÷¥––¡À∏√write_event_handler(¿˝»Á‘⁄œÚøÕªß∂À∑¢ÀÕ ˝æ›µƒπ˝≥Ã÷–£¨øÕªß∂À“ª÷±≤ªrecv£¨æÕª·‘Ï≥…ƒ⁄∫Àª∫¥Ê«¯¬˙£¨
     ˝æ›”¿‘∂∑¢ÀÕ≤ª≥ˆ»•£¨”⁄ «æÕ‘⁄ngx_http_set_write_handler÷–ÃÌº”¡À–¥ ¬º˛∂® ±∆˜)£¨¥”∂¯ø…“‘ºÏ≤È «∑Ò–¥≥¨ ±£¨¥”∂¯ø…“‘πÿ±’¡¨Ω”
     */

     /*
        µ± ˝æ›»´≤ø∑¢ÀÕµΩøÕªß∂À∫Û£¨‘⁄ngx_http_finalize_request÷–…æ≥˝
        if (c->write->timer_set) {
            c->write->delayed = 0;
            //’‚¿Ôµƒ∂® ±∆˜“ª∞„‘⁄ngx_http_set_write_handler->ngx_add_timer÷–ÃÌº”µƒ
            ngx_del_timer(c->write, NGX_FUNC_LINE);
        }
       */
        ngx_add_timer(wev, clcf->send_timeout, NGX_FUNC_LINE);
    }

    if (ngx_handle_write_event(wev, clcf->send_lowat, NGX_FUNC_LINE) != NGX_OK) {
        ngx_http_close_request(r, 0);
        return NGX_ERROR;
    }

    return NGX_OK;
}

//ngx_http_finalize_request->ngx_http_set_write_handler->ngx_http_writerÕ®π˝’‚÷÷∑Ω Ω∞—Œ¥∑¢ÀÕÕÍ±œµƒœÏ”¶±®Œƒ∑¢ÀÕ≥ˆ»•
/*
ngx_http_writer∑Ω∑®∂‘∏˜∏ˆHTTPƒ£øÈ∂¯—‘ «≤ªø…º˚µƒ£¨µ´ µº …œÀ¸∑«≥£÷ÿ“™£¨“ÚŒ™Œﬁ¬€ «ngx_http_send_headerªπ «
ngx_http_output_filter∑Ω∑®£¨À¸√«‘⁄µ˜”√ ±“ª∞„∂ºŒﬁ∑®∑¢ÀÕ»´≤øµƒœÏ”¶£¨ £œ¬µƒœÏ”¶ƒ⁄»›∂ºµ√øøngx_http_writer∑Ω∑®¿¥∑¢ÀÕ
*/ //ngx_http_writer∑Ω∑®Ωˆ”√”⁄‘⁄∫ÛÃ®∑¢ÀÕœÏ”¶µΩøÕªß∂À°£
//µ˜”√ngx_http_write_filter–¥ ˝æ›£¨»Áπ˚∑µªÿNGX_AGAIN,‘Ú“‘∫Ûµƒ–¥ ˝æ›¥•∑¢Õ®π˝‘⁄ngx_http_set_write_handler->ngx_http_writerÃÌº”epoll write ¬º˛¿¥¥•∑¢
static void
ngx_http_writer(ngx_http_request_t *r)
{
    int                        rc;
    ngx_event_t               *wev;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;
    wev = c->write;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, wev->log, 0,
                   "http writer handler: \"%V?%V\"", &r->uri, &r->args);

    clcf = ngx_http_get_module_loc_conf(r->main, ngx_http_core_module);

/*
    ºÏ≤È¡¨Ω”…œ–¥ ¬º˛µƒtimedout±Í÷æŒª£¨»Áπ˚timedoutŒ™0£¨‘Ú±Ì æ–¥ ¬º˛Œ¥≥¨ ±£ª»Áπ˚timedoutŒ™1£¨‘Ú±Ì æµ±«∞µƒ–¥ ¬º˛“—æ≠≥¨ ±£¨’‚ ±
”–¡Ω÷÷ø…ƒ‹–‘£∫
µ⁄“ª÷÷£¨”…”⁄Õ¯¬Á“Ï≥£ªÚ’ﬂøÕªß∂À≥§ ±º‰≤ªΩ” ’œÏ”¶£¨µº÷¬’Ê µµƒ∑¢ÀÕœÏ”¶≥¨ ±£ª
µ⁄∂˛÷÷£¨”…”⁄…œ“ª¥Œ∑¢ÀÕœÏ”¶ ±∑¢ÀÕÀŸ¬ π˝øÏ£¨≥¨π˝¡À«Î«Ûµƒlimit_rateÀŸ¬ …œœﬁ£¨ngx_http_write_filter∑Ω∑®æÕª·…Ë÷√“ª∏ˆ≥¨ ±
 ±º‰Ω´–¥ ¬º˛ÃÌº”µΩ∂® ±∆˜÷–£¨’‚ ±±æ¥Œµƒ≥¨ ±÷ª «”…œﬁÀŸµº÷¬£¨≤¢∑«’Ê’˝≥¨ ±°£ƒ«√¥£¨»Á∫Œ≈–∂œ’‚∏ˆ≥¨ ± «’Êµƒ≥¨ ±ªπ «≥ˆ”⁄œﬁÀŸµƒøº¬«ƒÿ£ø’‚
“™ø¥ ¬º˛µƒdelayed±Í÷æŒª°£»Áπ˚ «œﬁÀŸ∞—–¥ ¬º˛º”»Î∂® ±∆˜£¨“ª∂®ª·∞—delayed±Í÷æŒª÷√Œ™1£¨»Áπ˚–¥ ¬º˛µƒdelayed±Í÷æŒªŒ™0£¨ƒ«æÕ «’Êµƒ≥¨ ±
¡À£¨’‚ ±µ˜”√ngx_http_finalize_request∑Ω∑®Ω· ¯«Î«Û£¨¥´»Àµƒ≤Œ ˝ «NGX_HTTP_REQUEST_TIME_OUT£¨±Ì æ–Ë“™œÚøÕªß∂À∑¢ÀÕ408¥ÌŒÛ¬Î£ª
 */
    if (wev->timedout) { 
    /*
    “ÚŒ™≥¨ ±µƒ‘≠“Ú÷¥––¡À∏√write_event_handler(¿˝»Á‘⁄œÚøÕªß∂À∑¢ÀÕ ˝æ›µƒπ˝≥Ã÷–£¨øÕªß∂À“ª÷±≤ªrecv£¨æÕª·‘Ï≥…ƒ⁄∫Àª∫¥Ê«¯¬˙£¨
     ˝æ›”¿‘∂∑¢ÀÕ≤ª≥ˆ»•£¨”⁄ «æÕ‘⁄ngx_http_set_write_handler÷–ÃÌº”¡À–¥ ¬º˛∂® ±∆˜)£¨¥”∂¯ø…“‘ºÏ≤È «∑Ò–¥≥¨ ±£¨¥”∂¯ø…“‘πÿ±’¡¨Ω”
     */
        if (!wev->delayed) { //”…”⁄Õ¯¬Á“Ï≥£ªÚ’ﬂøÕªß∂À≥§ ±º‰≤ªΩ” ’œÏ”¶£¨µº÷¬’Ê µµƒ∑¢ÀÕœÏ”¶≥¨ ±£ª
            ngx_log_error(NGX_LOG_INFO, c->log, NGX_ETIMEDOUT,
                          "client timed out");
            c->timedout = 1;

            ngx_http_finalize_request(r, NGX_HTTP_REQUEST_TIME_OUT);
            return;
        }

        //limit_rateÀŸ¬ “˝∆µƒ≥¨ ±£¨º˚ngx_http_write_filter£¨’‚ Ù”⁄’˝≥£«Èøˆ
        wev->timedout = 0;
        wev->delayed = 0;

        /*
          ‘ŸºÏ≤È–¥ ¬º˛µƒready±Í÷æŒª£¨»Áπ˚Œ™1£¨‘Ú±Ì æ‘⁄”ÎøÕªß∂ÀµƒTCP¡¨Ω”…œø…“‘∑¢ÀÕ ˝æ›£ª»Áπ˚Œ™0£¨‘Ú±Ì æ‘›≤ªø…∑¢ÀÕ ˝æ›°£
          */
        if (!wev->ready) {
            //Ω´–¥ ¬º˛ÃÌº”µΩ∂® ±∆˜÷–£¨’‚¿Ôµƒ≥¨ ± ±º‰æÕ «≈‰÷√Œƒº˛÷–µƒsend_timeout≤Œ ˝£¨”ÎœﬁÀŸπ¶ƒ‹Œﬁπÿ°£
            ngx_add_timer(wev, clcf->send_timeout, NGX_FUNC_LINE);

            if (ngx_handle_write_event(wev, clcf->send_lowat, NGX_FUNC_LINE) != NGX_OK) {
                ngx_http_close_request(r, 0);
            }

            return;
        }

    }

    if (wev->delayed || r->aio) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, wev->log, 0,
                       "http writer delayed");

        if (ngx_handle_write_event(wev, clcf->send_lowat, NGX_FUNC_LINE) != NGX_OK) {
            ngx_http_close_request(r, 0);
        }

        return;
    }

    /*
    µ˜”√ngx_http_output_filter∑Ω∑®∑¢ÀÕœÏ”¶£¨∆‰÷–µ⁄2∏ˆ≤Œ ˝£®“≤æÕ «±Ì æ–Ë“™∑¢ÀÕµƒª∫≥Â«¯£©Œ™NULL÷∏’Î°£’‚“‚Œ∂◊≈£¨–Ë“™µ˜”√∏˜∞¸ÃÂπ˝
    ¬Àƒ£øÈ¥¶¿Ìoutª∫≥Â«¯÷–µƒ £”‡ƒ⁄»›£¨◊Ó∫Ûµ˜”√ngx_http_write filter∑Ω∑®∞—œÏ”¶∑¢ÀÕ≥ˆ»•°£
     */
    rc = ngx_http_output_filter(r, NULL);//NULL±Ì æ’‚¥Œ√ª”––ƒµƒ ˝æ›º”»ÎµΩout÷–£¨÷±Ω”∞—…œ¥Œ√ª”–∑¢ÀÕÕÍµƒout∑¢ÀÕ≥ˆ»•º¥ø…

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http writer output filter: %d, \"%V?%V\"",
                   rc, &r->uri, &r->args);

    if (rc == NGX_ERROR) {
        ngx_http_finalize_request(r, rc);
        return;
    }

/*
∑¢ÀÕœÏ”¶∫Û£¨≤Èø¥ngx_http_request_tΩ·ππÃÂ÷–µƒbuffered∫Õpostponed±Í÷æŒª£¨»Áπ˚»Œ“ª∏ˆ≤ªŒ™0£¨‘Ú“‚Œ∂◊≈√ª”–∑¢ÀÕÕÍout÷–µƒ»´≤øœÏ”¶£¨
«Î«Ûmain÷∏’Î÷∏œÚ«Î«Û◊‘…Ì£¨±Ì æ’‚∏ˆ«Î«Û «‘≠ º«Î«Û£¨‘ŸºÏ≤È”ÎøÕªß∂Àº‰µƒ¡¨Ω”ngx_connection-tΩ·ππÃÂ÷–µƒbuffered±Í÷æŒª£¨»Áπ˚buffered
≤ªŒ™0£¨Õ¨—˘±Ì æ√ª”–∑¢ÀÕÕÍout÷–µƒ»´≤øœÏ”¶£ª≥˝¥À“‘Õ‚£¨∂º±Ì æout÷–µƒ»´≤øœÏ”¶Ω‘∑¢ÀÕÕÍ±œ°£
 */
    if (r->buffered || r->postponed || (r == r->main && c->buffered)) {

#if (NGX_HTTP_V2)
        if (r->stream) {
            return;
        }
#endif

        if (!wev->delayed) {
            ngx_add_timer(wev, clcf->send_timeout, NGX_FUNC_LINE);
        }

        if (ngx_handle_write_event(wev, clcf->send_lowat, NGX_FUNC_LINE) != NGX_OK) {
            ngx_http_close_request(r, 0);
        }

        return;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, wev->log, 0,
                   "http writer done: \"%V?%V\"", &r->uri, &r->args);

/*
Ω´«Î«Ûµƒwrite_event_handler∑Ω∑®÷√Œ™ngx_http_request_empty_handler£¨“≤æÕ «Àµ£¨»Áπ˚’‚∏ˆ«Î«Ûµƒ¡¨Ω”…œ‘Ÿ”–ø…–¥ ¬º˛£¨Ω´≤ª◊ˆ»Œ∫Œ¥¶¿Ì°£
 */
    r->write_event_handler = ngx_http_request_empty_handler;

    ngx_http_finalize_request(r, rc);
}


static void
ngx_http_request_finalizer(ngx_http_request_t *r)
{
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http finalizer done: \"%V?%V\"", &r->uri, &r->args);

    ngx_http_finalize_request(r, 0);
}

/*
∞—∂¡ ¬º˛¥”epoll÷–“∆≥˝°£÷ª∂‘epoll ltƒ£ Ω∆‰◊˜”√À¸µƒ“‚“Â‘⁄”⁄£¨ƒø«∞“—æ≠ø™ º¥¶¿ÌHTTP«Î«Û£¨≥˝∑«ƒ≥∏ˆHTTPƒ£øÈ÷ÿ–¬…Ë÷√¡Àread_event_handler∑Ω∑®£¨
∑Ò‘Ú»Œ∫Œ∂¡ ¬º˛∂ºΩ´µ√≤ªµΩ¥¶¿Ì£¨“≤ø…À∆»œŒ™∂¡ ¬º˛±ª◊Ë »˚¡À°£

◊¢“‚’‚¿Ô√Êª·µ˜”√ngx_del_event£¨“Ú¥À»Áπ˚–Ë“™ºÃ–¯∂¡»°øÕªß∂À«Î«Ûƒ⁄»›£¨–Ë“™º”…œngx_add_event£¨¿˝»Áø…“‘≤Œøºœ¬ngx_http_discard_request_body
*/
void
ngx_http_block_reading(ngx_http_request_t *r)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http reading blocked");

    /* aio does not call this handler */

    if ((ngx_event_flags & NGX_USE_LEVEL_EVENT)
        && r->connection->read->active)
    {
        if (ngx_del_event(r->connection->read, NGX_READ_EVENT, 0) != NGX_OK) {
            ngx_http_close_request(r, 0);
        }
    }
}

//œÛ’˜–‘µƒ∂¡1∏ˆ◊÷Ω⁄£¨∆‰ µ…∂“≤√ª∏…
void ngx_http_test_reading(ngx_http_request_t *r)
{
    int                n;
    char               buf[1];
    ngx_err_t          err;
    ngx_event_t       *rev;
    ngx_connection_t  *c;

    c = r->connection;
    rev = c->read;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "http test reading");

#if (NGX_HTTP_V2)

    if (r->stream) {
        if (c->error) {
            err = 0;
            goto closed;
        }

        return;
    }

#endif

#if (NGX_HAVE_KQUEUE)

    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {

        if (!rev->pending_eof) {
            return;
        }

        rev->eof = 1;
        c->error = 1;
        err = rev->kq_errno;

        goto closed;
    }

#endif

#if (NGX_HAVE_EPOLLRDHUP)

    if ((ngx_event_flags & NGX_USE_EPOLL_EVENT) && rev->pending_eof) {
        socklen_t  len;

        rev->eof = 1;
        c->error = 1;

        err = 0;
        len = sizeof(ngx_err_t);

        /*
         * BSDs and Linux return 0 and set a pending error in err
         * Solaris returns -1 and sets errno
         */

        if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len)
            == -1)
        {
            err = ngx_socket_errno;
        }

        goto closed;
    }

#endif

    n = recv(c->fd, buf, 1, MSG_PEEK);

    if (n == 0) {
        rev->eof = 1;
        c->error = 1;
        err = 0;

        goto closed;

    } else if (n == -1) {
        err = ngx_socket_errno;

        if (err != NGX_EAGAIN) {
            rev->eof = 1;
            c->error = 1;

            goto closed;
        }
    }

    /* aio does not call this handler */

    if ((ngx_event_flags & NGX_USE_LEVEL_EVENT) && rev->active) {

        if (ngx_del_event(rev, NGX_READ_EVENT, 0) != NGX_OK) {
            ngx_http_close_request(r, 0);
        }
    }

    return;

closed:

    if (err) {
        rev->error = 1;
    }

    ngx_log_error(NGX_LOG_INFO, c->log, err,
                  "client prematurely closed connection");

    ngx_http_finalize_request(r, NGX_HTTP_CLIENT_CLOSED_REQUEST);
}

/*
ngx_http_set_keepalive∑Ω∑®Ω´µ±«∞¡¨Ω”…ËŒ™keepalive◊¥Ã¨°£À¸ µº …œª·∞—±Ì æ«Î«Ûµƒngx_http_request_tΩ·ππÃÂ Õ∑≈£¨»¥”÷≤ªª·µ˜”√
ngx_http_close_connection∑Ω∑®πÿ±’¡¨Ω”£¨Õ¨ ±“≤‘⁄ºÏ≤‚keepalive¡¨Ω” «∑Ò≥¨ ±£¨∂‘”⁄’‚∏ˆ∑Ω∑®£¨¥À¥¶≤ª◊ˆœÍœ∏Ω‚ Õ
*/
//ngx_http_finalize_request -> ngx_http_finalize_connection ->ngx_http_set_keepalive
static void
ngx_http_set_keepalive(ngx_http_request_t *r)
{
    int                        tcp_nodelay;
    ngx_int_t                  i;
    ngx_buf_t                 *b, *f;
    ngx_event_t               *rev, *wev;
    ngx_connection_t          *c;
    ngx_http_connection_t     *hc;
    ngx_http_core_srv_conf_t  *cscf;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;
    rev = c->read;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "set http keepalive handler");

    if (r->discard_body) {
        r->write_event_handler = ngx_http_request_empty_handler;
        r->lingering_time = ngx_time() + (time_t) (clcf->lingering_time / 1000);
        ngx_add_timer(rev, clcf->lingering_timeout, NGX_FUNC_LINE);
        return;
    }

    c->log->action = "closing request";

    hc = r->http_connection;
    b = r->header_in;

    if (b->pos < b->last) {

        /* the pipelined request */

        if (b != c->buffer) {

            /*
             * If the large header buffers were allocated while the previous
             * request processing then we do not use c->buffer for
             * the pipelined request (see ngx_http_create_request()).
             *
             * Now we would move the large header buffers to the free list.
             */

            cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

            if (hc->free == NULL) {
                hc->free = ngx_palloc(c->pool,
                  cscf->large_client_header_buffers.num * sizeof(ngx_buf_t *));

                if (hc->free == NULL) {
                    ngx_http_close_request(r, 0);
                    return;
                }
            }

            for (i = 0; i < hc->nbusy - 1; i++) {
                f = hc->busy[i];
                hc->free[hc->nfree++] = f;
                f->pos = f->start;
                f->last = f->start;
            }

            hc->busy[0] = b;
            hc->nbusy = 1;
        }
    }

    /* guard against recursive call from ngx_http_finalize_connection() */
    r->keepalive = 0;

    ngx_http_free_request(r, 0);

    c->data = hc;

    if (ngx_handle_read_event(rev, 0, NGX_FUNC_LINE) != NGX_OK) {
        ngx_http_close_connection(c);
        return;
    }

    wev = c->write;
    wev->handler = ngx_http_empty_handler;

    if (b->pos < b->last) {

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "pipelined request");

        c->log->action = "reading client pipelined request line";

        r = ngx_http_create_request(c);
        if (r == NULL) {
            ngx_http_close_connection(c);
            return;
        }

        r->pipeline = 1;

        c->data = r;

        c->sent = 0;
        c->destroyed = 0;

        if (rev->timer_set) {
            ngx_del_timer(rev, NGX_FUNC_LINE);
        }

        rev->handler = ngx_http_process_request_line;
        ngx_post_event(rev, &ngx_posted_events);
        return;
    }

    /*
     * To keep a memory footprint as small as possible for an idle keepalive
     * connection we try to free c->buffer's memory if it was allocated outside
     * the c->pool.  The large header buffers are always allocated outside the
     * c->pool and are freed too.
     */

    b = c->buffer;

    if (ngx_pfree(c->pool, b->start) == NGX_OK) {

        /*
         * the special note for ngx_http_keepalive_handler() that
         * c->buffer's memory was freed
         */

        b->pos = NULL;

    } else {
        b->pos = b->start;
        b->last = b->start;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0, "hc free: %p %d",
                   hc->free, hc->nfree);

    if (hc->free) {
        for (i = 0; i < hc->nfree; i++) {
            ngx_pfree(c->pool, hc->free[i]->start);
            hc->free[i] = NULL;
        }

        hc->nfree = 0;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0, "hc busy: %p %d",
                   hc->busy, hc->nbusy);

    if (hc->busy) {
        for (i = 0; i < hc->nbusy; i++) {
            ngx_pfree(c->pool, hc->busy[i]->start);
            hc->busy[i] = NULL;
        }

        hc->nbusy = 0;
    }

#if (NGX_HTTP_SSL)
    if (c->ssl) {
        ngx_ssl_free_buffer(c);
    }
#endif

    rev->handler = ngx_http_keepalive_handler;

    if (wev->active && (ngx_event_flags & NGX_USE_LEVEL_EVENT)) {
        if (ngx_del_event(wev, NGX_WRITE_EVENT, 0) != NGX_OK) {
            ngx_http_close_connection(c);
            return;
        }
    }

    c->log->action = "keepalive";

    if (c->tcp_nopush == NGX_TCP_NOPUSH_SET) {
        if (ngx_tcp_push(c->fd) == -1) {
            ngx_connection_error(c, ngx_socket_errno, ngx_tcp_push_n " failed");
            ngx_http_close_connection(c);
            return;
        }

        c->tcp_nopush = NGX_TCP_NOPUSH_UNSET;
        tcp_nodelay = ngx_tcp_nodelay_and_tcp_nopush ? 1 : 0;

    } else {
        tcp_nodelay = 1;
    }

    if (tcp_nodelay
        && clcf->tcp_nodelay
        && c->tcp_nodelay == NGX_TCP_NODELAY_UNSET)
    {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "tcp_nodelay");

        if (setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY,
                       (const void *) &tcp_nodelay, sizeof(int))
            == -1)
        {
#if (NGX_SOLARIS)
            /* Solaris returns EINVAL if a socket has been shut down */
            c->log_error = NGX_ERROR_IGNORE_EINVAL;
#endif

            ngx_connection_error(c, ngx_socket_errno,
                                 "setsockopt(TCP_NODELAY) failed");

            c->log_error = NGX_ERROR_INFO;
            ngx_http_close_connection(c);
            return;
        }

        c->tcp_nodelay = NGX_TCP_NODELAY_SET;
    }

#if 0
    /* if ngx_http_request_t was freed then we need some other place */
    r->http_state = NGX_HTTP_KEEPALIVE_STATE;
#endif

    c->idle = 1;
    ngx_reusable_connection(c, 1);

    ngx_add_timer(rev, clcf->keepalive_timeout, NGX_FUNC_LINE); //∆Ù∂Ø±£ªÓ∂® ±∆˜£¨ ±º‰Õ®π˝keepalive_timeout…Ë÷√

    if (rev->ready) {
        ngx_post_event(rev, &ngx_posted_events);
    }
}

//ngx_http_finalize_request -> ngx_http_finalize_connection ->ngx_http_set_keepalive ->ngx_http_keepalive_handler
/*
ø…“‘Õ®π˝¡Ω÷÷∑Ω Ω÷¥––µΩ∏√∫Ø ˝:
1. ±£ªÓ∂® ±∆˜≥¨ ±ngx_http_set_keepalive
2. øÕªß∂Àœ»∂œø™¡¨Ω”£¨øÕªß∂À∂œø™¡¨Ω”µƒ ±∫Úepoll_wait() error on fd£¨»ª∫Û÷±Ω”µ˜”√∏√∂¡ ¬º˛handler
*/
static void
ngx_http_keepalive_handler(ngx_event_t *rev)
{
    size_t             size;
    ssize_t            n;
    ngx_buf_t         *b;
    ngx_connection_t  *c;

    c = rev->data;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0, "http keepalive handler");

    if (rev->timedout || c->close) { //±£ªÓ≥¨ ±
        ngx_http_close_connection(c);
        return;
    }

#if (NGX_HAVE_KQUEUE)

    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT) {
        if (rev->pending_eof) {
            c->log->handler = NULL;
            ngx_log_error(NGX_LOG_INFO, c->log, rev->kq_errno,
                          "kevent() reported that client %V closed "
                          "keepalive connection", &c->addr_text);
#if (NGX_HTTP_SSL)
            if (c->ssl) {
                c->ssl->no_send_shutdown = 1;
            }
#endif
            ngx_http_close_connection(c);
            return;
        }
    }

#endif

    b = c->buffer;
    size = b->end - b->start;

    if (b->pos == NULL) {

        /*
         * The c->buffer's memory was freed by ngx_http_set_keepalive().
         * However, the c->buffer->start and c->buffer->end were not changed
         * to keep the buffer size.
         */

        b->pos = ngx_palloc(c->pool, size);
        if (b->pos == NULL) {
            ngx_http_close_connection(c);
            return;
        }

        b->start = b->pos;
        b->last = b->pos;
        b->end = b->pos + size;
    }

    /*
     * MSIE closes a keepalive connection with RST flag
     * so we ignore ECONNRESET here.
     */

    c->log_error = NGX_ERROR_IGNORE_ECONNRESET;
    ngx_set_socket_errno(0);

    n = c->recv(c, b->last, size); //±£ªÓ≥¨ ±£¨Õ¨ ±÷¥––“ª¥Œ∂¡≤Ÿ◊˜£¨’‚—˘æÕø…“‘œ»ºÏ≤È∂‘∑Ω «∑Ò“—æ≠πÿ±’¡Àtcp¡¨Ω”£¨ 
    c->log_error = NGX_ERROR_INFO;

    if (n == NGX_AGAIN) {//tcp¡¨Ω”’˝≥££¨≤¢«“√ª”– ˝æ›
        if (ngx_handle_read_event(rev, 0, NGX_FUNC_LINE) != NGX_OK) {
            ngx_http_close_connection(c);
            return;
        }

        /*
         * Like ngx_http_set_keepalive() we are trying to not hold
         * c->buffer's memory for a keepalive connection.
         */

        if (ngx_pfree(c->pool, b->start) == NGX_OK) {

            /*
             * the special note that c->buffer's memory was freed
             */

            b->pos = NULL;
        }

        return;
    }

    if (n == NGX_ERROR) {
        ngx_http_close_connection(c);
        return;
    }

    c->log->handler = NULL;

    if (n == 0) { //∂‘∑Ω“—æ≠πÿ±’¡¨Ω”
        ngx_log_error(NGX_LOG_INFO, c->log, ngx_socket_errno,
                      "client %V closed keepalive connection", &c->addr_text);
        ngx_http_close_connection(c);
        return;
    }

    b->last += n;

    c->log->handler = ngx_http_log_error;
    c->log->action = "reading client request line";

    c->idle = 0;
    ngx_reusable_connection(c, 0);

    c->data = ngx_http_create_request(c);
    if (c->data == NULL) {
        ngx_http_close_connection(c);
        return;
    }

    c->sent = 0;
    c->destroyed = 0;

    ngx_del_timer(rev, NGX_FUNC_LINE);

/*
’‚—˘µƒ«Î«Û––≥§∂» «≤ª∂®µƒ£¨À¸”ÎURI≥§∂»œ‡πÿ£¨’‚“‚Œ∂◊≈‘⁄∂¡ ¬º˛±ª¥•∑¢ ±£¨ƒ⁄∫ÀÃ◊Ω”◊÷ª∫≥Â«¯µƒ¥Û–°Œ¥±ÿ◊„πªΩ” ’µΩ»´≤øµƒHTTP«Î«Û––£¨”…¥Àø…“‘µ√≥ˆΩ·¬€£∫
µ˜”√“ª¥Œngx_http_process_request_line∑Ω∑®≤ª“ª∂®ƒ‹πª◊ˆÕÍ’‚œÓπ§◊˜°£À˘“‘£¨ngx_http_process_request_line∑Ω∑®“≤ª·◊˜Œ™∂¡ ¬º˛µƒªÿµ˜∑Ω∑®£¨À¸ø…ƒ‹ª·±ª
epoll’‚∏ˆ ¬º˛«˝∂Øª˙÷∆∂‡¥Œµ˜∂»£¨∑¥∏¥µÿΩ” ’TCP¡˜≤¢ π”√◊¥Ã¨ª˙Ω‚ŒˆÀ¸√«£¨÷±µΩ»∑»œΩ” ’µΩ¡ÀÕÍ’˚µƒHTTP«Î«Û––£¨’‚∏ˆΩ◊∂Œ≤≈À„ÕÍ≥…£¨≤≈ª·Ω¯»Îœ¬“ª∏ˆΩ◊∂ŒΩ” ’HTTPÕ∑≤ø°£
*/
    rev->handler = ngx_http_process_request_line;
    ngx_http_process_request_line(rev);
}

/*
lingering_close
”Ô∑®£∫lingering_close off | on | always;
ƒ¨»œ£∫lingering_close on;
≈‰÷√øÈ£∫http°¢server°¢location
∏√≈‰÷√øÿ÷∆Nginxπÿ±’”√ªß¡¨Ω”µƒ∑Ω Ω°£always±Ì æπÿ±’”√ªß¡¨Ω”«∞±ÿ–ÎŒﬁÃıº˛µÿ¥¶¿Ì¡¨Ω”…œÀ˘”–”√ªß∑¢ÀÕµƒ ˝æ›°£off±Ì æπÿ±’¡¨Ω” ±ÕÍ»´≤ªπ‹¡¨Ω”
…œ «∑Ò“—æ≠”–◊º±∏æÕ–˜µƒ¿¥◊‘”√ªßµƒ ˝æ›°£on «÷–º‰÷µ£¨“ª∞„«Èøˆœ¬‘⁄πÿ±’¡¨Ω”«∞∂ºª·¥¶¿Ì¡¨Ω”…œµƒ”√ªß∑¢ÀÕµƒ ˝æ›£¨≥˝¡À”––©«Èøˆœ¬‘⁄“µŒÒ…œ»œ∂®’‚÷Æ∫Ûµƒ ˝æ› «≤ª±ÿ“™µƒ°£
*/ //”…lingering_closeæˆ∂®
/*
lingering_close¥Ê‘⁄µƒ“‚“ÂæÕ «¿¥∂¡»° £œ¬µƒøÕªß∂À∑¢¿¥µƒ ˝æ›£¨À˘“‘nginxª·”–“ª∏ˆ∂¡≥¨ ± ±º‰£¨Õ®π˝lingering_timeout—°œÓ¿¥…Ë÷√£¨»Áπ˚‘⁄
lingering_timeout ±º‰ƒ⁄ªπ√ª”– ’µΩ ˝æ›£¨‘Ú÷±Ω”πÿµÙ¡¨Ω”°£nginxªπ÷ß≥÷…Ë÷√“ª∏ˆ◊‹µƒ∂¡»° ±º‰£¨Õ®π˝lingering_time¿¥…Ë÷√£¨’‚∏ˆ ±º‰“≤æÕ «
nginx‘⁄πÿ±’–¥÷Æ∫Û£¨±£¡Ùsocketµƒ ±º‰£¨øÕªß∂À–Ë“™‘⁄’‚∏ˆ ±º‰ƒ⁄∑¢ÀÕÕÍÀ˘”–µƒ ˝æ›£¨∑Ò‘Únginx‘⁄’‚∏ˆ ±º‰π˝∫Û£¨ª·÷±Ω”πÿµÙ¡¨Ω”°£µ±»ª£¨nginx
 «÷ß≥÷≈‰÷√ «∑Ò¥Úø™lingering_close—°œÓµƒ£¨Õ®π˝lingering_close—°œÓ¿¥≈‰÷√°£ ƒ«√¥£¨Œ“√«‘⁄ µº ”¶”√÷–£¨ «∑Ò”¶∏√¥Úø™lingering_closeƒÿ£ø’‚
∏ˆæÕ√ª”–πÃ∂®µƒÕ∆ºˆ÷µ¡À£¨»ÁMaxim DouninÀ˘Àµ£¨lingering_closeµƒ÷˜“™◊˜”√ «±£≥÷∏¸∫√µƒøÕªß∂ÀºÊ»›–‘£¨µ´ «»¥–Ë“™œ˚∫ƒ∏¸∂‡µƒ∂ÓÕ‚◊ ‘¥£®±»»Á
¡¨Ω”ª·“ª÷±’º◊≈£©°£
*/
static void
ngx_http_set_lingering_close(ngx_http_request_t *r)
{
    ngx_event_t               *rev, *wev;
    ngx_connection_t          *c;
    ngx_http_core_loc_conf_t  *clcf;

    c = r->connection;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    rev = c->read;
    rev->handler = ngx_http_lingering_close_handler;

    r->lingering_time = ngx_time() + (time_t) (clcf->lingering_time / 1000);
    //‘⁄◊º±∏∂œø™¡¨Ω”µƒ ±∫Ú£¨»Áπ˚…Ë÷√¡Àlingering_close always£¨ƒ«√¥‘⁄¥¶¿ÌÕÍ∏˜÷÷ ¬«È∫Û£¨ª·—”≥Ÿlingering_timeout’‚√¥∂‡ ±º‰ƒ⁄£¨»Áπ˚øÕªß∂Àªπ « ˝æ›µΩ¿¥£¨∑˛ŒÒ∆˜∂Àø…“‘ºÃ–¯∂¡»°
    ngx_add_timer(rev, clcf->lingering_timeout, NGX_FUNC_LINE); //rev->handler = ngx_http_lingering_close_handler;

    if (ngx_handle_read_event(rev, 0, NGX_FUNC_LINE) != NGX_OK) {
        ngx_http_close_request(r, 0);
        return;
    }

    wev = c->write;
    wev->handler = ngx_http_empty_handler;

    if (wev->active && (ngx_event_flags & NGX_USE_LEVEL_EVENT)) {
        if (ngx_del_event(wev, NGX_WRITE_EVENT, 0) != NGX_OK) {
            ngx_http_close_request(r, 0);
            return;
        }
    }

    if (ngx_shutdown_socket(c->fd, NGX_WRITE_SHUTDOWN) == -1) { //πÿ±’–¥∂À£¨shutdown,µ´ «ªπø…“‘ºÃ–¯∂¡
        ngx_connection_error(c, ngx_socket_errno,
                             ngx_shutdown_socket_n " failed");
        ngx_http_close_request(r, 0);
        return;
    }

    if (rev->ready) {
        ngx_http_lingering_close_handler(rev);
    }
}


static void
ngx_http_lingering_close_handler(ngx_event_t *rev)
{
    ssize_t                    n;
    ngx_msec_t                 timer;
    ngx_connection_t          *c;
    ngx_http_request_t        *r;
    ngx_http_core_loc_conf_t  *clcf;
    u_char                     buffer[NGX_HTTP_LINGERING_BUFFER_SIZE];

    c = rev->data;
    r = c->data;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http lingering close handler");

    if (rev->timedout) { //∏√∂® ±∆˜‘⁄ngx_http_set_lingering_close…Ë÷√µƒ
        ngx_http_close_request(r, 0);
        return;
    }

    timer = (ngx_msec_t) r->lingering_time - (ngx_msec_t) ngx_time();
    if ((ngx_msec_int_t) timer <= 0) {
        ngx_http_close_request(r, 0);
        return;
    }

    do {
        n = c->recv(c, buffer, NGX_HTTP_LINGERING_BUFFER_SIZE);

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0, "lingering read: %d", n);

        if (n == NGX_ERROR || n == 0) {
            ngx_http_close_request(r, 0);
            return;
        }

    } while (rev->ready);

    if (ngx_handle_read_event(rev, 0, NGX_FUNC_LINE) != NGX_OK) {
        ngx_http_close_request(r, 0);
        return;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    timer *= 1000;

    if (timer > clcf->lingering_timeout) {
        timer = clcf->lingering_timeout; 
    }

    //»Áπ˚‘⁄lingering_timeout ±º‰ƒ⁄”÷ ˝æ›¥ÔµΩ£¨‘Ú∞—÷ÿ–¬ÃÌº”∂® ±∆˜£¨∆‰ ±º‰Œ™timer£¨“Ú¥À÷ª“™lingering_timeout ±º‰ƒ⁄”– ˝æ›¥ÔµΩ£¨
    //‘Ú¡¨Ω”‘⁄lingering_timeoutµƒ ±∫Úπÿ±’
    ngx_add_timer(rev, timer, NGX_FUNC_LINE); //ngx_http_lingering_close_handler
}

/*
’‚∏ˆ∑Ω∑®Ωˆ”““ª∏ˆ”√Õæ£∫µ±“µŒÒ…œ≤ª–Ë“™¥¶¿Ìø…–¥ ¬º˛ ±£¨æÕ∞—ngx_http_empty_handler∑Ω∑®…Ë÷√µΩ¡¨Ω”µƒø…–¥ ¬º˛µƒhandler÷–£¨
’‚—˘ø…–¥ ¬º˛±ª∂® ±∆˜ªÚ’ﬂepoll¥•∑¢∫Û «≤ª◊ˆ»Œ∫Œπ§◊˜µƒ°£
*/
void
ngx_http_empty_handler(ngx_event_t *wev)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, wev->log, 0, "http empty handler");

    return;
}


void
ngx_http_request_empty_handler(ngx_http_request_t *r)
{
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http request empty handler");

    return;
}

//‘⁄∑¢ÀÕ∫Û∂Àµƒ∑µªÿµƒ ˝æ›µΩøÕªß∂À≥…π¶∫Û£¨ª·µ˜”√∏√∫Ø ˝£¨“ª∞„‘⁄chunk¥´ÀÕ∑Ω Ωµƒ ±∫Ú”––ß£¨µ˜”√ngx_http_chunked_body_filter±Í ∂∏√chunk¥´ÀÕ∑Ω Ωµƒ∞¸ÃÂ¥´ÀÕΩ” ’

// µº …œ‘⁄Ω” ‹ÕÍ∫Û∂À ˝æ›∫Û£¨‘⁄œÎøÕªß∂À∑¢ÀÕ∞¸ÃÂ≤ø∑÷µƒ ±∫Ú£¨ª·¡Ω¥Œµ˜”√∏√∫Ø ˝£¨“ª¥Œ «ngx_event_pipe_write_to_downstream-> p->output_filter(),
//¡Ì“ª¥Œ «ngx_http_upstream_finalize_request->ngx_http_send_special,

//Õ¯“≥∞¸ÃÂ“ª∞„Ω¯π˝∏√∫Ø ˝¥•∑¢Ω¯––∑¢ÀÕ£¨Õ∑≤ø––‘⁄∏√∫Ø ˝Õ‚“—æ≠∑¢ÀÕ≥ˆ»•¡À£¨¿˝»Ángx_http_upstream_send_response->ngx_http_send_header
ngx_int_t
ngx_http_send_special(ngx_http_request_t *r, ngx_uint_t flags)
{//∏√∫Ø ˝ µº …œæÕ «◊ˆ“ªº˛ ¬£¨æÕ «‘⁄“™∑¢ÀÕ≥ˆ»•µƒchain¡¥ƒ©Œ≤ÃÌº”“ª∏ˆø’chain(º˚ngx_output_chain->ngx_output_chain_add_copy)£¨µ±±È¿˙chain∑¢ÀÕ ˝æ›µƒ ±∫Ú£¨“‘¥À¿¥±Íº« ˝æ›∑¢ÀÕÕÍ±œ£¨’‚ «◊Ó∫Û“ª∏ˆchain£¨

//º˚ngx_http_write_filter -> if (!last && !flush && in && size < (off_t) clcf->postpone_output) {
    ngx_buf_t    *b;
    ngx_chain_t   out;

    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_ERROR;
    }

    //º˚ngx_http_write_filter -> if (!last && !flush && in && size < (off_t) clcf->postpone_output) {
    if (flags & NGX_HTTP_LAST) {

        if (r == r->main && !r->post_action) {//“ª∞„Ω¯»Î’‚∏ˆif¿Ô√Ê
            b->last_buf = 1; 

        } else {
            b->sync = 1;
            b->last_in_chain = 1;
        }
    }

    if (flags & NGX_HTTP_FLUSH) {
        b->flush = 1;
    }

    out.buf = b;
    out.next = NULL;

    ngx_int_t last_buf = b->last_buf;
    ngx_int_t sync = b->sync;
    ngx_int_t last_in_chain = b->last_in_chain;
    ngx_int_t flush = b->flush;
    
    ngx_log_debugall(r->connection->log, 0, "ngx http send special, flags:%ui, last_buf:%i, sync:%i, last_in_chain:%i, flush:%i",
        flags, last_buf, sync,  last_in_chain, flush);
    return ngx_http_output_filter(r, &out);
}

//
static ngx_int_t
ngx_http_post_action(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (clcf->post_action.data == NULL) {//post_action XXX√ª”–≈‰÷√÷±Ω”∑µªÿ
        return NGX_DECLINED;
    }

    if (r->post_action && r->uri_changes == 0) {
        return NGX_DECLINED;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post action: \"%V\"", &clcf->post_action);

    r->main->count--;

    r->http_version = NGX_HTTP_VERSION_9;
    r->header_only = 1;
    r->post_action = 1;

    r->read_event_handler = ngx_http_block_reading;

    if (clcf->post_action.data[0] == '/') {
        ngx_http_internal_redirect(r, &clcf->post_action, NULL);

    } else {
        ngx_http_named_location(r, &clcf->post_action);
    }

    return NGX_OK;
}

/*
ngx_http_close_request∑Ω∑® «∏¸∏ﬂ≤„µƒ”√”⁄πÿ±’«Î«Ûµƒ∑Ω∑®£¨µ±»ª£¨HTTPƒ£øÈ“ª∞„“≤≤ªª·÷±Ω”µ˜”√À¸µƒ°£‘⁄…œ√Êº∏Ω⁄÷–∑¥∏¥Ã·µΩµƒ“˝”√º∆ ˝£¨
æÕ «”…ngx_http_close_request∑Ω∑®∏∫‘ºÏ≤‚µƒ£¨Õ¨ ±À¸ª·‘⁄“˝”√º∆ ˝«Â¡„ ±’˝ Ωµ˜”√ngx_http_free_request∑Ω∑®∫Õngx_http_close_connection(ngx_close_connection)
∑Ω∑®¿¥ Õ∑≈«Î«Û°¢πÿ±’¡¨Ω”,º˚ngx_http_close_request,◊¢“‚∫Õngx_http_finalize_connectionµƒ«¯±
*/
static void
ngx_http_close_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_connection_t  *c;

    //“˝”√º∆ ˝“ª∞„∂º◊˜”√”⁄’‚∏ˆ«Î«Ûµƒ‘≠ º«Î«Û…œ£¨“Ú¥À£¨‘⁄Ω· ¯«Î«Û ±Õ≥“ªºÏ≤È‘≠ º«Î«Ûµƒ“˝”√º∆ ˝æÕø…“‘¡À
    r = r->main;
    c = r->connection;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http request count:%d blk:%d", r->count, r->blocked);

    if (r->count == 0) {
        ngx_log_error(NGX_LOG_ALERT, c->log, 0, "http request count is zero");
    }

    r->count--;

    /* 
     ‘⁄HTTPƒ£øÈ÷–√øΩ¯––“ª¿‡–¬µƒ≤Ÿ◊˜£¨∞¸¿®Œ™“ª∏ˆ«Î«ÛÃÌº”–¬µƒ ¬º˛£¨ªÚ’ﬂ∞—“ª–©“—“Ô”…∂® ±∆˜°¢epoll÷–“∆≥˝µƒ ¬º˛÷ÿ–¬º”»Î∆‰÷–£¨∂º–Ë“™∞—’‚∏ˆ
     «Î«Ûµƒ“˝”√º∆ ˝º”1°£’‚ «“ÚŒ™–Ë“™»√HTTPøÚº‹÷™µ¿£¨HTTPƒ£øÈ∂‘”⁄∏√«Î«Û”–∂¿¡¢µƒ“Ï≤Ω¥¶¿Ìª˙÷∆£¨Ω´”…∏√HTTPƒ£øÈæˆ∂®’‚∏ˆ≤Ÿ◊˜ ≤√¥ ±∫ÚΩ· ¯£¨∑¿
     ÷π‘⁄’‚∏ˆ≤Ÿ◊˜ªπŒ¥Ω· ¯ ±HTTPøÚº‹»¥∞—’‚∏ˆ«Î«Ûœ˙ªŸ¡À£®»Á∆‰À˚HTTPƒ£øÈÕ®π˝µ˜”√ngx_http_finalize_request∑Ω∑®“™«ÛHTTPøÚº‹Ω· ¯«Î«Û£©£¨µº÷¬
     «Î«Û≥ˆœ÷≤ªø…÷™µƒ—œ÷ÿ¥ÌŒÛ°£’‚æÕ“™«Û√ø∏ˆ≤Ÿ◊˜‘⁄°∞»œŒ™°±◊‘…Ìµƒ∂Ø◊˜Ω· ¯ ±£¨∂ºµ√◊Ó÷’µ˜”√µΩngx_http_close_request∑Ω∑®£¨∏√∑Ω∑®ª·◊‘∂ØºÏ≤È“˝”√
     º∆ ˝£¨µ±“˝”√º∆ ˝Œ™0 ±≤≈’Ê’˝µÿœ˙ªŸ«Î«Û

         ”…ngx_http_request_tΩ·ππÃÂµƒmain≥…‘±÷–»°≥ˆ∂‘”¶µƒ‘≠ º«Î«Û£®µ±»ª£¨ø…ƒ‹æÕ «’‚∏ˆ«Î«Û±æ…Ì£©£¨‘Ÿ»°≥ˆcount“˝”√º∆ ˝≤¢ºıl°£
	»ª∫Û£¨ºÏ≤Ècount“˝”√º∆ ˝ «∑Ò“—æ≠Œ™0£¨“‘º∞blocked±Í÷æŒª «∑ÒŒ™0°£»Áπ˚count“—æ≠Œ™O£¨‘Ú÷§√˜«Î«Û√ª”–∆‰À˚∂Ø◊˜“™ π”√¡À£¨Õ¨ ±blocked±Í
	÷æŒª“≤Œ™0£¨±Ì æ√ª”–HTTPƒ£øÈªπ–Ë“™¥¶¿Ì«Î«Û£¨À˘“‘¥À ±«Î«Ûø…“‘’Ê’˝ Õ∑≈£ª»Áπ˚count“˝”√º∆ ˝¥Û∏…0£¨ªÚ’ﬂblocked¥Û”⁄0£¨’‚—˘∂º≤ªø…“‘Ω·
	 ¯«Î«Û£¨ngx_http_close_reques't∑Ω∑®÷±Ω”Ω· ¯°£
     */
    if (r->count || r->blocked) {
        return;
    }

    //÷ª”–countŒ™0≤≈ƒ‹ºÃ–¯∫Û–¯µƒ Õ∑≈◊ ‘¥≤Ÿ◊˜
#if (NGX_HTTP_SPDY)
    if (r->spdy_stream) {
        ngx_http_spdy_close_stream(r->spdy_stream, rc);
        return;
    }
#endif

    #if (NGX_HTTP_V2)
    if (r->stream) {
        ngx_http_v2_close_stream(r->stream, rc);
        return;
    }
    #endif

    ngx_http_free_request(r, rc);
    ngx_http_close_connection(c);
}

/*
ngx_http_free_request«Ó∑®Ω´ª· Õ∑≈«Î«Û∂‘”¶µƒngx_http_request_t ˝æ›Ω·ππ£¨À¸≤¢≤ªª·œÒngx_http_close_connection∑Ω∑®“ª—˘»• Õ∑≈≥–‘ÿ«Î«Ûµƒ
TCP¡¨Ω”£¨√ø“ª∏ˆTCP¡¨Ω”ø…“‘∑¥∏¥µÿ≥–‘ÿ∂‡∏ˆHTTP«Î«Û£¨“Ú¥À£¨ngx_http_free_request «±»ngx_http_close_connection∏¸∏ﬂ≤„¥Œµƒ∑Ω∑®£¨«∞’ﬂ±ÿ»ªœ»”⁄∫Û’ﬂµ˜”√

ngx_http_close_request∑Ω∑® «∏¸∏ﬂ≤„µƒ”√”⁄πÿ±’«Î«Ûµƒ∑Ω∑®£¨µ±»ª£¨HTTPƒ£øÈ“ª∞„“≤≤ªª·÷±Ω”µ˜”√À¸µƒ°£‘⁄…œ√Êº∏Ω⁄÷–∑¥∏¥Ã·µΩµƒ“˝”√º∆ ˝£¨
æÕ «”…ngx_http_close_request∑Ω∑®∏∫‘ºÏ≤‚µƒ£¨Õ¨ ±À¸ª·‘⁄“˝”√º∆ ˝«Â¡„ ±’˝ Ωµ˜”√ngx_http_free_request∑Ω∑®∫Õngx_http_close_connection(ngx_close_connection)
∑Ω∑®¿¥ Õ∑≈«Î«Û°¢πÿ±’¡¨Ω”,º˚ngx_http_close_request
*/
void
ngx_http_free_request(ngx_http_request_t *r, ngx_int_t rc) // Õ∑≈requestµƒœ‡πÿ◊ ‘¥
{
    ngx_log_t                 *log;
    ngx_pool_t                *pool;
    struct linger              linger;
    ngx_http_cleanup_t        *cln;
    ngx_http_log_ctx_t        *ctx;
    ngx_http_core_loc_conf_t  *clcf;

    log = r->connection->log;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0, "http close request");

    if (r->pool == NULL) {
        ngx_log_error(NGX_LOG_ALERT, log, 0, "http request already closed");
        return;
    }

    cln = r->cleanup;
    r->cleanup = NULL;
    /* —≠ª∑µÿ±È¿˙«Î«Ûngx_http_request_tΩ·ππÃÂ÷–µƒcleanup¡¥±Ì£¨“¿¥Œµ˜”√√ø“ª∏ˆngx_http_cleanup_pt∑Ω∑® Õ∑≈◊ ‘¥°£ */
    while (cln) {
        if (cln->handler) {
            cln->handler(cln->data);
        }

        cln = cln->next;
    }

#if (NGX_STAT_STUB)

    if (r->stat_reading) {
        (void) ngx_atomic_fetch_add(ngx_stat_reading, -1);
    }

    if (r->stat_writing) {
        (void) ngx_atomic_fetch_add(ngx_stat_writing, -1);
    }

#endif

    if (rc > 0 && (r->headers_out.status == 0 || r->connection->sent == 0)) {
        r->headers_out.status = rc;
    }

    log->action = "logging request";

    /* 
    ‘⁄11∏ˆngx_http_phasesΩ◊∂Œ÷–£¨◊Ó∫Û“ª∏ˆΩ◊∂ŒΩ–◊ˆNGX_HTTP_LOG_PHASE£¨À¸ «”√¿¥º«¬ºøÕªß∂Àµƒ∑√Œ »’÷æµƒ°£‘⁄’‚“ª≤Ω÷Ë÷–£¨Ω´ª·“¿¥Œ
    µ˜”√NGX_HTTP_LOG_PHASEΩ◊∂ŒµƒÀ˘”–ªÿµ˜∑Ω∑®º«¬º»’÷æ°£πŸ∑Ωµƒngx_http_log_moduleƒ£øÈæÕ «‘⁄’‚¿Ôº«¬ºaccess_logµƒ°£
     */
    ngx_http_log_request(r); //¥Ú”°http log handler  Ω”»Î∞¥’’access_format∏Ò Ω»’÷æ–¥»ÎΩ”»Î»’÷æŒƒº˛

    log->action = "closing request";

    /*
     socketstructtcpwindows ˝æ›Ω·ππ¥À—°œÓ÷∏∂®∫Ø ˝close∂‘√ÊœÚ¡¨Ω”µƒ–≠“È»Á∫Œ≤Ÿ◊˜£®»ÁTCP£©°£ƒ⁄∫À»± °close≤Ÿ◊˜ «¡¢º¥∑µªÿ£¨»Áπ˚”–
      ˝æ›≤–¡Ù‘⁄Ã◊Ω”ø⁄ª∫≥Â«¯÷–‘ÚœµÕ≥Ω´ ‘◊≈Ω´’‚–© ˝æ›∑¢ÀÕ∏¯∂‘∑Ω°£ 

     SO_LINGER—°œÓ”√¿¥∏ƒ±‰¥À»± °…Ë÷√°£ π”√»Áœ¬Ω·ππ£∫ 
     struct linger { 
     
          int l_onoff; / * 0 = off, nozero = on * / 
     
          int l_linger; / * linger time * / 
     
     }; 
     
     ”–œ¬¡–»˝÷÷«Èøˆ£∫ 
     
     1°¢…Ë÷√ l_onoffŒ™0£¨‘Ú∏√—°œÓπÿ±’£¨l_lingerµƒ÷µ±ª∫ˆ¬‘£¨µ»”⁄ƒ⁄∫À»± °«Èøˆ£¨closeµ˜”√ª·¡¢º¥∑µªÿ∏¯µ˜”√’ﬂ£¨»Áπ˚ø…ƒ‹Ω´ª·¥´ ‰»Œ∫ŒŒ¥∑¢ÀÕµƒ ˝æ›£ª 
     
     2°¢…Ë÷√ l_onoffŒ™∑«0£¨l_lingerŒ™0£¨‘ÚÃ◊Ω”ø⁄πÿ±’ ±TCPÿ≤’€¡¨Ω”£¨TCPΩ´∂™∆˙±£¡Ù‘⁄Ã◊Ω”ø⁄∑¢ÀÕª∫≥Â«¯÷–µƒ»Œ∫Œ ˝æ›≤¢∑¢ÀÕ“ª∏ˆRST∏¯∂‘∑Ω£¨∂¯≤ª «Õ®≥£µƒÀƒ
     ∑÷◊È÷’÷π–Ú¡–£¨’‚±‹√‚¡ÀTIME_WAIT◊¥Ã¨£ª 
     3°¢…Ë÷√ l_onoff Œ™∑«0£¨l_lingerŒ™∑«0£¨µ±Ã◊Ω”ø⁄πÿ±’ ±ƒ⁄∫ÀΩ´Õœ—”“ª∂Œ ±º‰£®”…l_lingeræˆ∂®£©°£»Áπ˚Ã◊Ω”ø⁄ª∫≥Â«¯÷–»‘≤–¡Ù ˝æ›£¨Ω¯≥ÃΩ´¥¶”⁄ÀØ√ﬂ◊¥Ã¨£¨
     ÷±µΩ£®a£©À˘”– ˝æ›∑¢ÀÕÕÍ«“±ª∂‘∑Ω»∑»œ£¨÷Æ∫ÛΩ¯––’˝≥£µƒ÷’÷π–Ú¡–£®√Ë ˆ◊÷∑√Œ º∆ ˝Œ™0£©ªÚ£®b£©—”≥Ÿ ±º‰µΩ°£¥À÷÷«Èøˆœ¬£¨”¶”√≥Ã–ÚºÏ≤Ècloseµƒ∑µªÿ÷µ «∑«≥£÷ÿ“™µƒ£¨
     »Áπ˚‘⁄ ˝æ›∑¢ÀÕÕÍ≤¢±ª»∑»œ«∞ ±º‰µΩ£¨closeΩ´∑µªÿEWOULDBLOCK¥ÌŒÛ«“Ã◊Ω”ø⁄∑¢ÀÕª∫≥Â«¯÷–µƒ»Œ∫Œ ˝æ›∂º∂™ ß °£ closeµƒ≥…π¶∑µªÿΩˆ∏ÊÀﬂŒ“√«∑¢ÀÕµƒ ˝æ›£®∫ÕFIN£©“—
     ”…∂‘∑ΩTCP»∑»œ£¨À¸≤¢≤ªƒ‹∏ÊÀﬂŒ“√«∂‘∑Ω”¶”√Ω¯≥Ã «∑Ò“—∂¡¡À ˝æ›°£»Áπ˚Ã◊Ω”ø⁄…ËŒ™∑«◊Ë»˚µƒ£¨À¸Ω´≤ªµ»¥˝closeÕÍ≥…°£ 

     SO_LINGER «“ª∏ˆsocket—°œÓ£¨Õ®π˝setsockopt APIΩ¯––…Ë÷√£¨ π”√∆¿¥±»ΩœºÚµ•£¨µ´∆‰ µœ÷ª˙÷∆±»Ωœ∏¥‘”£¨«“◊÷√Ê“‚Àº…œ±»Ωœƒ—¿ÌΩ‚°£
     Ω‚ Õ◊Ó«Â≥˛µƒµ± Ù°∂UnixÕ¯¬Á±‡≥ÃæÌ1°∑÷–µƒÀµ√˜£®7.5’¬Ω⁄£©£¨’‚¿ÔºÚµ•’™¬º£∫
     SO_LINGERµƒ÷µ”√»Áœ¬ ˝æ›Ω·ππ±Ì æ£∫
     struct linger {
          int l_onoff; / * 0 = off, nozero = on * /
          int l_linger; / * linger time * /
     
     };
     
     
     
     
     ∆‰»°÷µ∫Õ¥¶¿Ì»Áœ¬£∫
     1°¢…Ë÷√ l_onoffŒ™0£¨‘Ú∏√—°œÓπÿ±’£¨l_lingerµƒ÷µ±ª∫ˆ¬‘£¨µ»”⁄ƒ⁄∫À»± °«Èøˆ£¨closeµ˜”√ª·¡¢º¥∑µªÿ∏¯µ˜”√’ﬂ£¨»Áπ˚ø…ƒ‹Ω´ª·¥´ ‰»Œ∫ŒŒ¥∑¢ÀÕµƒ ˝æ›£ª
     2°¢…Ë÷√ l_onoffŒ™∑«0£¨l_lingerŒ™0£¨‘ÚÃ◊Ω”ø⁄πÿ±’ ±TCPÿ≤’€¡¨Ω”£¨TCPΩ´∂™∆˙±£¡Ù‘⁄Ã◊Ω”ø⁄∑¢ÀÕª∫≥Â«¯÷–µƒ»Œ∫Œ ˝æ›≤¢∑¢ÀÕ“ª∏ˆRST∏¯∂‘∑Ω£¨
        ∂¯≤ª «Õ®≥£µƒÀƒ∑÷◊È÷’÷π–Ú¡–£¨’‚±‹√‚¡ÀTIME_WAIT◊¥Ã¨£ª
     3°¢…Ë÷√ l_onoff Œ™∑«0£¨l_lingerŒ™∑«0£¨µ±Ã◊Ω”ø⁄πÿ±’ ±ƒ⁄∫ÀΩ´Õœ—”“ª∂Œ ±º‰£®”…l_lingeræˆ∂®£©°£
        »Áπ˚Ã◊Ω”ø⁄ª∫≥Â«¯÷–»‘≤–¡Ù ˝æ›£¨Ω¯≥ÃΩ´¥¶”⁄ÀØ√ﬂ◊¥Ã¨£¨÷± µΩ£®a£©À˘”– ˝æ›∑¢ÀÕÕÍ«“±ª∂‘∑Ω»∑»œ£¨÷Æ∫ÛΩ¯––’˝≥£µƒ÷’÷π–Ú¡–£®√Ë ˆ◊÷∑√Œ º∆ ˝Œ™0£©
        ªÚ£®b£©—”≥Ÿ ±º‰µΩ°£¥À÷÷«Èøˆœ¬£¨”¶”√≥Ã–ÚºÏ≤Ècloseµƒ∑µªÿ÷µ «∑«≥£÷ÿ“™µƒ£¨»Áπ˚‘⁄ ˝æ›∑¢ÀÕÕÍ≤¢±ª»∑»œ«∞ ±º‰µΩ£¨closeΩ´∑µªÿEWOULDBLOCK¥ÌŒÛ«“Ã◊Ω”ø⁄∑¢ÀÕª∫≥Â«¯÷–µƒ»Œ∫Œ ˝æ›∂º∂™ ß°£
        closeµƒ≥…π¶∑µªÿΩˆ∏ÊÀﬂŒ“√«∑¢ÀÕµƒ ˝æ›£®∫ÕFIN£©“—”…∂‘∑ΩTCP»∑»œ£¨À¸≤¢≤ªƒ‹∏ÊÀﬂŒ“√«∂‘∑Ω”¶”√Ω¯≥Ã «∑Ò“—∂¡¡À ˝æ›°£»Áπ˚Ã◊Ω”ø⁄…ËŒ™∑«◊Ë»˚µƒ£¨À¸Ω´≤ªµ»¥˝closeÕÍ≥…°£
        
     µ⁄“ª÷÷«Èøˆ∆‰ µ∫Õ≤ª…Ë÷√√ª”–«¯±£¨µ⁄∂˛÷÷«Èøˆø…“‘”√”⁄±‹√‚TIME_WAIT◊¥Ã¨£¨µ´‘⁄Linux…œ≤‚ ‘µƒ ±∫Ú£¨≤¢Œ¥∑¢œ÷∑¢ÀÕ¡ÀRST—°œÓ£¨∂¯ «’˝≥£Ω¯––¡ÀÀƒ≤Ωπÿ±’¡˜≥Ã£¨
     ≥ı≤ΩÕ∆∂œ «°∞÷ª”–‘⁄∂™∆˙ ˝æ›µƒ ±∫Ú≤≈∑¢ÀÕRST°±£¨»Áπ˚√ª”–∂™∆˙ ˝æ›£¨‘Ú◊ﬂ’˝≥£µƒπÿ±’¡˜≥Ã°£
     ≤Èø¥Linux‘¥¬Î£¨»∑ µ”–’‚√¥“ª∂Œ◊¢ Õ∫Õ‘¥¬Î£∫
     =====linux-2.6.37 net/ipv4/tcp.c 1915=====
     / * As outlined in RFC 2525, section 2.17, we send a RST here because
     * data was lost. To witness the awful effects of the old behavior of
     * always doing a FIN, run an older 2.1.x kernel or 2.0.x, start a bulk
     * GET in an FTP client, suspend the process, wait for the client to
     * advertise a zero window, then kill -9 the FTP client, wheee...
     * Note: timeout is always zero in such a case.
     * /
     if (data_was_unread) {
     / * Unread data was tossed, zap the connection. * /
     NET_INC_STATS_USER(sock_net(sk), LINUX_MIB_TCPABORTONCLOSE);
     tcp_set_state(sk, TCP_CLOSE);
     tcp_send_active_reset(sk, sk->sk_allocation);
     } 
     ¡ÌÕ‚£¨¥”‘≠¿Ì…œ¿¥Àµ£¨’‚∏ˆ—°œÓ”–“ª∂®µƒŒ£œ’–‘£¨ø…ƒ‹µº÷¬∂™ ˝æ›£¨ π”√µƒ ±∫Ú“™–°–ƒ“ª–©£¨µ´Œ“√«‘⁄ µ≤‚libmemcachedµƒπ˝≥Ã÷–£¨√ª”–∑¢œ÷¥À¿‡œ÷œÛ£¨
     ”¶∏√ «∫ÕlibmemcachedµƒÕ®—∂–≠“È…Ë÷√”–πÿ£¨“≤ø…ƒ‹ «Œ“√«µƒ—π¡¶≤ªπª¥Û£¨≤ªª·≥ˆœ÷’‚÷÷«Èøˆ°£
     
     
     µ⁄»˝÷÷«Èøˆ∆‰ µæÕ «µ⁄“ª÷÷∫Õµ⁄∂˛÷÷µƒ’€÷–¥¶¿Ì£¨«“µ±socketŒ™∑«◊Ë»˚µƒ≥°æ∞œ¬ «√ª”–◊˜”√µƒ°£
     ∂‘”⁄”¶∂‘∂Ã¡¨Ω”µº÷¬µƒ¥Û¡øTIME_WAIT¡¨Ω”Œ Ã‚£¨∏ˆ»À»œŒ™µ⁄∂˛÷÷¥¶¿Ì «◊Ó”≈µƒ—°‘Ò£¨libmemcachedæÕ «≤…”√’‚÷÷∑Ω Ω£¨
     ¥” µ≤‚«Èøˆ¿¥ø¥£¨¥Úø™’‚∏ˆ—°œÓ∫Û£¨TIME_WAIT¡¨Ω” ˝Œ™0£¨«“≤ª ‹Õ¯¬Á◊ÈÕ¯£®¿˝»Á «∑Ò–Èƒ‚ª˙µ»£©µƒ”∞œÏ°£ 
     */
    if (r->connection->timedout) { //ºı…Ÿtime_wait◊¥Ã¨£¨ƒ⁄∫ÀµƒÀƒ¥Œª” ÷,º”øÏÀƒ¥Œª” ÷°£÷ª”–‘⁄πÿ±’Ã◊Ω”◊÷µƒ ±∫Úƒ⁄∫Àªπ”– ˝æ›√ª”–Ω” ’ªÚ’ﬂ∑¢ÀÕ≤≈ª·∑¢ÀÕRST
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        if (clcf->reset_timedout_connection) {
            linger.l_onoff = 1;
            linger.l_linger = 0;

            if (setsockopt(r->connection->fd, SOL_SOCKET, SO_LINGER,
                           (const void *) &linger, sizeof(struct linger)) == -1)
            {
                ngx_log_error(NGX_LOG_ALERT, log, ngx_socket_errno,
                              "setsockopt(SO_LINGER) failed");
            }
        }
    }

    /* the various request strings were allocated from r->pool */
    ctx = log->data;
    ctx->request = NULL;

    r->request_line.len = 0;

    r->connection->destroyed = 1;

    /*
     * Setting r->pool to NULL will increase probability to catch double close
     * of request since the request object is allocated from its own pool.
     */

    pool = r->pool;
    r->pool = NULL;

    ngx_destroy_pool(pool); /*  Õ∑≈request->pool */
}


//µ±∞¸ÃÂ”¶¥∏¯øÕªß∂À∫Û£¨‘⁄ngx_http_free_request÷–µ˜”√»’÷æµƒhandler¿¥º«¬º«Î«Û–≈œ¢µΩlog»’÷æ
static void
ngx_http_log_request(ngx_http_request_t *r)
{
    ngx_uint_t                  i, n;
    ngx_http_handler_pt        *log_handler;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    log_handler = cmcf->phases[NGX_HTTP_LOG_PHASE].handlers.elts;
    n = cmcf->phases[NGX_HTTP_LOG_PHASE].handlers.nelts;

    for (i = 0; i < n; i++) {
        log_handler[i](r); //ngx_http_log_handler
    }
}

/*
ngx_http_close connection∑Ω∑® «HTTPøÚº‹Ã·π©µƒ“ª∏ˆ”√”⁄ Õ∑≈TCP¡¨Ω”µƒ∑Ω∑®£¨À¸µƒƒøµƒ∫‹ºÚµ•£¨æÕ «πÿ±’’‚∏ˆTCP¡¨Ω”£¨µ±«“Ωˆµ±HTTP
«Î«Û’Ê’˝Ω· ¯ ±≤≈ª·µ˜”√’‚∏ˆ∑Ω∑®
*/
/*
ngx_http_free_request«Ó∑®Ω´ª· Õ∑≈«Î«Û∂‘”¶µƒngx_http_request_t ˝æ›Ω·ππ£¨À¸≤¢≤ªª·œÒngx_http_close_connection∑Ω∑®“ª—˘»• Õ∑≈≥–‘ÿ«Î«Ûµƒ
TCP¡¨Ω”£¨√ø“ª∏ˆTCP¡¨Ω”ø…“‘∑¥∏¥µÿ≥–‘ÿ∂‡∏ˆHTTP«Î«Û£¨“Ú¥À£¨ngx_http_free_request «±»ngx_http_close_connection∏¸∏ﬂ≤„¥Œµƒ∑Ω∑®£¨«∞’ﬂ±ÿ»ªœ»”⁄∫Û’ﬂµ˜”√

ngx_http_close_request∑Ω∑® «∏¸∏ﬂ≤„µƒ”√”⁄πÿ±’«Î«Ûµƒ∑Ω∑®£¨µ±»ª£¨HTTPƒ£øÈ“ª∞„“≤≤ªª·÷±Ω”µ˜”√À¸µƒ°£‘⁄…œ√Êº∏Ω⁄÷–∑¥∏¥Ã·µΩµƒ“˝”√º∆ ˝£¨
æÕ «”…ngx_http_close_request∑Ω∑®∏∫‘ºÏ≤‚µƒ£¨Õ¨ ±À¸ª·‘⁄“˝”√º∆ ˝«Â¡„ ±’˝ Ωµ˜”√ngx_http_free_request∑Ω∑®∫Õngx_http_close_connection(ngx_close_connection)
∑Ω∑®¿¥ Õ∑≈«Î«Û°¢πÿ±’¡¨Ω”,º˚ngx_http_close_request
*/
void
ngx_http_close_connection(ngx_connection_t *c)
{
    ngx_pool_t  *pool;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "close http connection: %d", c->fd);

#if (NGX_HTTP_SSL)

    if (c->ssl) {
        if (ngx_ssl_shutdown(c) == NGX_AGAIN) {
            c->ssl->handler = ngx_http_close_connection;
            return;
        }
    }

#endif

#if (NGX_STAT_STUB)
    (void) ngx_atomic_fetch_add(ngx_stat_active, -1);
#endif

    c->destroyed = 1; //±Ì æconnectionœ˙ªŸ

    pool = c->pool;

    ngx_close_connection(c);

    ngx_destroy_pool(pool);
}


static u_char *
ngx_http_log_error(ngx_log_t *log, u_char *buf, size_t len)
{
    u_char              *p;
    ngx_http_request_t  *r;
    ngx_http_log_ctx_t  *ctx;

    if (log->action) {
        p = ngx_snprintf(buf, len, " while %s", log->action);
        len -= p - buf;
        buf = p;
    }

    ctx = log->data;

    p = ngx_snprintf(buf, len, ", client: %V", &ctx->connection->addr_text);
    len -= p - buf;

    r = ctx->request;

    if (r) {
        return r->log_handler(r, ctx->current_request, p, len); //ngx_http_log_error_handler

    } else {
        p = ngx_snprintf(p, len, ", server: %V",
                         &ctx->connection->listening->addr_text);
    }

    return p;
}


static u_char *
ngx_http_log_error_handler(ngx_http_request_t *r, ngx_http_request_t *sr,
    u_char *buf, size_t len)
{
    char                      *uri_separator;
    u_char                    *p;
    ngx_http_upstream_t       *u;
    ngx_http_core_srv_conf_t  *cscf;

    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

    p = ngx_snprintf(buf, len, ", server: %V", &cscf->server_name);
    len -= p - buf;
    buf = p;

    if (r->request_line.data == NULL && r->request_start) {
        for (p = r->request_start; p < r->header_in->last; p++) {
            if (*p == CR || *p == LF) {
                break;
            }
        }

        r->request_line.len = p - r->request_start;
        r->request_line.data = r->request_start;
    }

    if (r->request_line.len) {
        p = ngx_snprintf(buf, len, ", request: \"%V\"", &r->request_line);
        len -= p - buf;
        buf = p;
    }

    if (r != sr) {
        p = ngx_snprintf(buf, len, ", subrequest: \"%V\"", &sr->uri);
        len -= p - buf;
        buf = p;
    }

    u = sr->upstream;

    if (u && u->peer.name) {

        uri_separator = "";

#if (NGX_HAVE_UNIX_DOMAIN)
        if (u->peer.sockaddr && u->peer.sockaddr->sa_family == AF_UNIX) {
            uri_separator = ":";
        }
#endif

        p = ngx_snprintf(buf, len, ", upstream: \"%V%V%s%V\"",
                         &u->schema, u->peer.name,
                         uri_separator, &u->uri);
        len -= p - buf;
        buf = p;
    }

    if (r->headers_in.host) {
        p = ngx_snprintf(buf, len, ", host: \"%V\"",
                         &r->headers_in.host->value);
        len -= p - buf;
        buf = p;
    }

    if (r->headers_in.referer) {
        p = ngx_snprintf(buf, len, ", referrer: \"%V\"",
                         &r->headers_in.referer->value);
        buf = p;
    }

    return buf;
}
