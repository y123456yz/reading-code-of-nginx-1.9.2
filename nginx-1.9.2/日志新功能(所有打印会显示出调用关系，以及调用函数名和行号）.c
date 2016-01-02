/*
2015/12/31 06:36:01[           ngx_epoll_process_events,  1713]  [debug] 21360#21360: epoll: fd:9 EPOLLIN  (ev:0001) d:B24FD078
2015/12/31 06:36:01[                ngx_channel_handler,  1472]  [debug] 21360#21360: channel handler
2015/12/31 06:36:01[                ngx_channel_handler,  1478]  [debug] 21360#21360: channel: 16
2015/12/31 06:36:01[                ngx_channel_handler,  1505]  [debug] 21360#21360: channel command: 1
2015/12/31 06:36:01[                ngx_channel_handler,  1525]  [debug] 21360#21360: get channel s:2 pid:21361 fd:7
2015/12/31 06:36:01[                ngx_channel_handler,  1478]  [debug] 21360#21360: channel: 16
2015/12/31 06:36:01[                ngx_channel_handler,  1505]  [debug] 21360#21360: channel command: 1
2015/12/31 06:36:01[                ngx_channel_handler,  1525]  [debug] 21360#21360: get channel s:3 pid:21362 fd:8
2015/12/31 06:36:01[                ngx_channel_handler,  1478]  [debug] 21360#21360: channel: -2
2015/12/31 06:36:01[           ngx_trylock_accept_mutex,   425]  [debug] 21360#21360: accept mutex lock failed: 0
2015/12/31 06:36:11[            ngx_event_expire_timers,   133]  [debug] 21361#21361: event timer del: -1: 4110992280
2015/12/31 06:36:11[         ngx_http_file_cache_expire,  1951]  [debug] 21361#21361: http file cache expire
2015/12/31 06:36:11[                     ngx_shmtx_lock,   168]  [debug] 21361#21361: shmtx lock
2015/12/31 06:36:11[                   ngx_shmtx_unlock,   249]  [debug] 21361#21361: shmtx unlock
2015/12/31 06:36:11[                     ngx_shmtx_lock,   168]  [debug] 21361#21361: shmtx lock
2015/12/31 06:36:11[                   ngx_shmtx_unlock,   249]  [debug] 21361#21361: shmtx unlock
2015/12/31 06:36:11[        ngx_http_file_cache_manager,  2150]  [debug] 21361#21361: http file cache size: 0, max_size:262144
2015/12/31 06:36:11[                ngx_event_add_timer,   100]  [debug] 21361#21361: <ngx_cache_manager_process_handler,  1646>  event timer add fd:-1, expire-time:10 s, timer.key:4111002281
2015/12/31 06:36:20[           ngx_epoll_process_events,  1713]  [debug] 21359#21359: epoll: fd:6 EPOLLIN  (ev:0001) d:B24FD008
2015/12/31 06:36:20[           ngx_epoll_process_events,  1759]  [debug] 21359#21359: post event B06FD008
2015/12/31 06:36:20[           ngx_event_process_posted,    65]  [debug] 21359#21359: begin to run befor posted event B06FD008
2015/12/31 06:36:20[           ngx_event_process_posted,    67]  [debug] 21359#21359: delete posted event B06FD008
2015/12/31 06:36:20[                   ngx_event_accept,    72]  [debug] 21359#21359: accept on 10.2.13.167:80, ready: 1
2015/12/31 06:36:20[                   ngx_event_accept,   370]  [debug] 21359#21359: *1 accept: 10.2.13.167:48224 fd:13
2015/12/31 06:36:20[                ngx_event_add_timer,   100]  [debug] 21359#21359: *1 < ngx_http_init_connection,   393>  event timer add fd:13, expire-time:20 s, timer.key:4111021272
2015/12/31 06:36:20[            ngx_reusable_connection,  1177]  [debug] 21359#21359: *1 reusable connection: 1
2015/12/31 06:36:20[              ngx_handle_read_event,   499]  [debug] 21359#21359: *1 < ngx_http_init_connection,   396> epoll NGX_USE_CLEAR_EVENT(et) read add
2015/12/31 06:36:20[                ngx_epoll_add_event,  1409]  [debug] 21359#21359: *1 epoll add read event: fd:13 op:1 ev:80002001
2015/12/31 06:36:20[                   ngx_event_accept,    99]  [debug] 21359#21359: accept() not ready (11: Resource temporarily unavailable)
2015/12/31 06:36:20[           ngx_trylock_accept_mutex,   405]  [debug] 21359#21359: accept mutex locked
2015/12/31 06:36:20[           ngx_epoll_process_events,  1713]  [debug] 21359#21359: epoll: fd:13 EPOLLIN  (ev:0001) d:B24FD0E8
2015/12/31 06:36:20[           ngx_epoll_process_events,  1759]  [debug] 21359#21359: *1 post event B06FD068
2015/12/31 06:36:20[           ngx_event_process_posted,    65]  [debug] 21359#21359: begin to run befor posted event B06FD068
2015/12/31 06:36:20[           ngx_event_process_posted,    67]  [debug] 21359#21359: *1 delete posted event B06FD068
2015/12/31 06:36:20[      ngx_http_wait_request_handler,   417]  [debug] 21359#21359: *1 http wait request handler
2015/12/31 06:36:20[                      ngx_unix_recv,   204]  [debug] 21359#21359: *1 recv: fd:13 read-size:156 of 1024, ready:1
2015/12/31 06:36:20[            ngx_reusable_connection,  1177]  [debug] 21359#21359: *1 reusable connection: 0
2015/12/31 06:36:20[      ngx_http_process_request_line,   963]  [debug] 21359#21359: *1 http process request line
2015/12/31 06:36:20[      ngx_http_process_request_line,  1002]  [debug] 21359#21359: *1 http request line: "GET /test2.php HTTP/1.1"
2015/12/31 06:36:20[       ngx_http_process_request_uri,  1223]  [debug] 21359#21359: *1 http uri: "/test2.php"
2015/12/31 06:36:20[       ngx_http_process_request_uri,  1226]  [debug] 21359#21359: *1 http args: ""
2015/12/31 06:36:20[       ngx_http_process_request_uri,  1229]  [debug] 21359#21359: *1 http exten: "php"
2015/12/31 06:36:20[   ngx_http_process_request_headers,  1272]  [debug] 21359#21359: *1 http process request header line
2015/12/31 06:36:20[   ngx_http_process_request_headers,  1404]  [debug] 21359#21359: *1 http header: "User-Agent: curl/7.20.1 (i686-pc-linux-gnu) libcurl/7.20.1 OpenSSL/0.9.8n zlib/1.2.3 libidn/1.5"
2015/12/31 06:36:20[   ngx_http_process_request_headers,  1404]  [debug] 21359#21359: *1 http header: "Host: 10.2.13.167"
2015/12/31 06:36:20[   ngx_http_process_request_headers,  1404]  [debug] 21359#21359: *1 http header: "Accept: * / *"
2015/12/31 06:36:20[   ngx_http_process_request_headers,  1414]  [debug] 21359#21359: *1 http header done
2015/12/31 06:36:20[                ngx_event_del_timer,    39]  [debug] 21359#21359: *1 < ngx_http_process_request,  2015>  event timer del: 13: 4111021272
2015/12/31 06:36:20[        ngx_http_core_rewrite_phase,  1945]  [debug] 21359#21359: *1 rewrite phase: 0 (NGX_HTTP_SERVER_REWRITE_PHASE)
2015/12/31 06:36:20[    ngx_http_core_find_config_phase,  2003]  [debug] 21359#21359: *1 find config phase: 1 (NGX_HTTP_FIND_CONFIG_PHASE), uri:/test2.php
2015/12/31 06:36:20[ ngx_http_core_find_static_location,  2890]  [debug] 21359#21359: *1 static_locations test location: "/", client uri:/test2.php
2015/12/31 06:36:20[ ngx_http_core_find_static_location,  2890]  [debug] 21359#21359: *1 static_locations test location: "mytest_upstream", client uri:/test2.php
2015/12/31 06:36:20[ ngx_http_core_find_static_location,  2890]  [debug] 21359#21359: *1 static_locations test location: "proxy1/", client uri:/test2.php
2015/12/31 06:36:20[        ngx_http_core_find_location,  2830]  [debug] 21359#21359: *1 ngx pcre test location: ~ "\.php$"
2015/12/31 06:36:20[    ngx_http_core_find_config_phase,  2023]  [debug] 21359#21359: *1 using configuration "\.php$"
2015/12/31 06:36:20[    ngx_http_core_find_config_phase,  2030]  [debug] 21359#21359: *1 http cl:-1 max:1048576, rc:0
2015/12/31 06:36:20[        ngx_http_core_rewrite_phase,  1945]  [debug] 21359#21359: *1 rewrite phase: 2 (NGX_HTTP_REWRITE_PHASE)
2015/12/31 06:36:20[   ngx_http_core_post_rewrite_phase,  2098]  [debug] 21359#21359: *1 post rewrite phase: 3 (NGX_HTTP_POST_REWRITE_PHASE)
2015/12/31 06:36:20[        ngx_http_core_generic_phase,  1881]  [debug] 21359#21359: *1 generic phase: 4 (NGX_HTTP_PREACCESS_PHASE)
2015/12/31 06:36:20[        ngx_http_core_generic_phase,  1881]  [debug] 21359#21359: *1 generic phase: 5 (NGX_HTTP_PREACCESS_PHASE)
2015/12/31 06:36:20[         ngx_http_core_access_phase,  2196]  [debug] 21359#21359: *1 access phase: 6 (NGX_HTTP_ACCESS_PHASE)
2015/12/31 06:36:20[         ngx_http_core_access_phase,  2196]  [debug] 21359#21359: *1 access phase: 7 (NGX_HTTP_ACCESS_PHASE)
2015/12/31 06:36:20[    ngx_http_core_post_access_phase,  2298]  [debug] 21359#21359: *1 post access phase: 8 (NGX_HTTP_POST_ACCESS_PHASE)
2015/12/31 06:36:20[        ngx_http_core_content_phase,  2620]  [debug] 21359#21359: *1 content phase(content_handler): 9 (NGX_HTTP_CONTENT_PHASE)
2015/12/31 06:36:20[             ngx_http_upstream_init,   632]  [debug] 21359#21359: *1 http init upstream, client timer: 0
2015/12/31 06:36:20[             ngx_http_upstream_init,   666]  [debug] 21359#21359: *1 <   ngx_http_upstream_init,   665> epoll NGX_WRITE_EVENT(et) write add
2015/12/31 06:36:20[                ngx_epoll_add_event,  1405]  [debug] 21359#21359: *1 epoll modify read and write event: fd:13 op:3 ev:80002005
2015/12/31 06:36:20[        ngx_http_upstream_cache_get,  1155][yangya  [debug] 21359#21359: *1 ngx http upstream cache get use keys_zone:fcgi
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "/test2.php"
2015/12/31 06:36:20[     ngx_http_file_cache_create_key,   253]  [debug] 21359#21359: *1 http cache key: "/test2.php"
2015/12/31 06:36:20[                     ngx_shmtx_lock,   168]  [debug] 21359#21359: shmtx lock
2015/12/31 06:36:20[                   ngx_shmtx_unlock,   249]  [debug] 21359#21359: shmtx unlock
2015/12/31 06:36:20[           ngx_http_file_cache_open,   318]  [debug] 21359#21359: *1 http file cache exists, rc: -5 if exist:0
2015/12/31 06:36:20[           ngx_http_file_cache_name,  1058]  [debug] 21359#21359: *1 cache file: "/var/yyz/cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f"
2015/12/31 06:36:20[           ngx_http_file_cache_open,   381][yangya  [debug] 21359#21359: *1 ngx_open_cached_file return:NGX_ERROR
2015/12/31 06:36:20[            ngx_http_upstream_cache,  1060]  [debug] 21359#21359: *1 http upstream cache: -5
2015/12/31 06:36:20[     ngx_http_upstream_init_request,   710][yangya  [debug] 21359#21359: *1 ngx http cache, conf->cache:1, rc:-5
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "SCRIPT_FILENAME"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "/var/yyz/www"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "/"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "/test2.php"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "SCRIPT_FILENAME: /var/yyz/www//test2.php"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "QUERY_STRING"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "QUERY_STRING: "
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "REQUEST_METHOD"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "GET"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "REQUEST_METHOD: GET"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "CONTENT_TYPE"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "CONTENT_TYPE: "
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "CONTENT_LENGTH"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "CONTENT_LENGTH: "
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "SCRIPT_NAME"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "/test2.php"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "SCRIPT_NAME: /test2.php"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "REQUEST_URI"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "/test2.php"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "REQUEST_URI: /test2.php"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "DOCUMENT_URI"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "/test2.php"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "DOCUMENT_URI: /test2.php"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "DOCUMENT_ROOT"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "/var/yyz/www"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "DOCUMENT_ROOT: /var/yyz/www"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "SERVER_PROTOCOL"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "HTTP/1.1"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "SERVER_PROTOCOL: HTTP/1.1"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "REQUEST_SCHEME"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "http"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "REQUEST_SCHEME: http"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: ""
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "GATEWAY_INTERFACE"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "CGI/1.1"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "GATEWAY_INTERFACE: CGI/1.1"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "SERVER_SOFTWARE"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "nginx/"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "1.9.2"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "SERVER_SOFTWARE: nginx/1.9.2"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "REMOTE_ADDR"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "10.2.13.167"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "REMOTE_ADDR: 10.2.13.167"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "REMOTE_PORT"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "48224"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "REMOTE_PORT: 48224"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "SERVER_ADDR"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "10.2.13.167"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "SERVER_ADDR: 10.2.13.167"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "SERVER_PORT"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "80"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "SERVER_PORT: 80"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "SERVER_NAME"
2015/12/31 06:36:20[      ngx_http_script_copy_var_code,   989]  [debug] 21359#21359: *1 http script var: "localhost"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "SERVER_NAME: localhost"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "REDIRECT_STATUS"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: "200"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1543]  [debug] 21359#21359: *1 fastcgi param: "REDIRECT_STATUS: 200"
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: ""
2015/12/31 06:36:20[          ngx_http_script_copy_code,   865]  [debug] 21359#21359: *1 http script copy: ""
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1616]  [debug] 21359#21359: *1 fastcgi param: "HTTP_USER_AGENT: curl/7.20.1 (i686-pc-linux-gnu) libcurl/7.20.1 OpenSSL/0.9.8n zlib/1.2.3 libidn/1.5"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1616]  [debug] 21359#21359: *1 fastcgi param: "HTTP_HOST: 10.2.13.167"
2015/12/31 06:36:20[    ngx_http_fastcgi_create_request,  1616]  [debug] 21359#21359: *1 fastcgi param: "HTTP_ACCEPT: * / *"
2015/12/31 06:36:20[               ngx_http_cleanup_add,  4229]  [debug] 21359#21359: *1 http cleanup add: 080F2670
2015/12/31 06:36:20[ngx_http_upstream_get_round_robin_peer,   620]  [debug] 21359#21359: *1 get rr peer, try: 1
2015/12/31 06:36:20[             ngx_event_connect_peer,    31]  [debug] 21359#21359: *1 socket 14
2015/12/31 06:36:20[           ngx_epoll_add_connection,  1500]  [debug] 21359#21359: *1 epoll add connection(read and write): fd:14 ev:80002005
2015/12/31 06:36:20[             ngx_event_connect_peer,   129]  [debug] 21359#21359: *1 connect to 127.0.0.1:3666, fd:14 #2
2015/12/31 06:36:20[          ngx_http_upstream_connect,  1676]  [debug] 21359#21359: *1 http upstream connect: -2
2015/12/31 06:36:20[                ngx_event_add_timer,   100]  [debug] 21359#21359: *1 <ngx_http_upstream_connect,  1826>  event timer add fd:14, expire-time:60 s, timer.key:4111061272
2015/12/31 06:36:20[          ngx_http_finalize_request,  2592]  [debug] 21359#21359: *1 http finalize request rc: -4, "/test2.php?" a:1, c:2
2015/12/31 06:36:20[             ngx_http_close_request,  3899]  [debug] 21359#21359: *1 http request count:2 blk:0
2015/12/31 06:36:20[           ngx_trylock_accept_mutex,   405]  [debug] 21359#21359: accept mutex locked
2015/12/31 06:36:20[           ngx_epoll_process_events,  1713]  [debug] 21359#21359: epoll: fd:13 EPOLLOUT  (ev:0004) d:B24FD0E8
2015/12/31 06:36:20[           ngx_epoll_process_events,  1786]  [debug] 21359#21359: *1 post event AE8FD068
2015/12/31 06:36:20[           ngx_epoll_process_events,  1713]  [debug] 21359#21359: epoll: fd:14 EPOLLOUT  (ev:0004) d:B24FD158
2015/12/31 06:36:20[           ngx_epoll_process_events,  1786]  [debug] 21359#21359: *1 post event AE8FD098
2015/12/31 06:36:20[           ngx_event_process_posted,    65]  [debug] 21359#21359: begin to run befor posted event AE8FD068
2015/12/31 06:36:20[           ngx_event_process_posted,    67]  [debug] 21359#21359: *1 delete posted event AE8FD068
2015/12/31 06:36:20[           ngx_http_request_handler,  2401]  [debug] 21359#21359: *1 http run request(ev->write:1): "/test2.php?"
2015/12/31 06:36:20[ngx_http_upstream_check_broken_connection,  1461]  [debug] 21359#21359: *1 http upstream check client, write event:1, "/test2.php"
2015/12/31 06:36:20[ngx_http_upstream_check_broken_connection,  1584]  [debug] 21359#21359: *1 http upstream recv() size: -1, fd:13 (11: Resource temporarily unavailable)
2015/12/31 06:36:20[           ngx_event_process_posted,    65]  [debug] 21359#21359: begin to run befor posted event AE8FD098
2015/12/31 06:36:20[           ngx_event_process_posted,    67]  [debug] 21359#21359: *1 delete posted event AE8FD098
2015/12/31 06:36:20[          ngx_http_upstream_handler,  1400]  [debug] 21359#21359: *1 http upstream request(ev->write:1): "/test2.php?"
2015/12/31 06:36:20[ngx_http_upstream_send_request_handler,  2420]  [debug] 21359#21359: *1 http upstream send request handler
2015/12/31 06:36:20[     ngx_http_upstream_send_request,  2167]  [debug] 21359#21359: *1 http upstream send request
2015/12/31 06:36:20[ngx_http_upstream_send_request_body,  2305]  [debug] 21359#21359: *1 http upstream send request body
2015/12/31 06:36:20[                   ngx_output_chain,    67][yangya  [debug] 21359#21359: *1 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2015/12/31 06:36:20[                   ngx_output_chain,    90][yangya  [debug] 21359#21359: *1 only one chain buf to output_filter
2015/12/31 06:36:20[                   ngx_chain_writer,   743]  [debug] 21359#21359: *1 chain writer buf fl:0 s:600
2015/12/31 06:36:20[                   ngx_chain_writer,   758]  [debug] 21359#21359: *1 chain writer in: 080F268C
2015/12/31 06:36:20[                         ngx_writev,   199]  [debug] 21359#21359: *1 writev: 600 of 600
2015/12/31 06:36:20[                   ngx_chain_writer,   797]  [debug] 21359#21359: *1 chain writer out: 00000000
2015/12/31 06:36:20[                ngx_event_del_timer,    39]  [debug] 21359#21359: *1 <ngx_http_upstream_send_request,  2258>  event timer del: 14: 4111061272
2015/12/31 06:36:20[     ngx_http_upstream_send_request,  2276]  [debug] 21359#21359: *1 send out chain data to uppeer server OK
2015/12/31 06:36:20[                ngx_event_add_timer,   100]  [debug] 21359#21359: *1 <ngx_http_upstream_send_request,  2285>  event timer add fd:14, expire-time:60 s, timer.key:4111061275
2015/12/31 06:36:20[           ngx_trylock_accept_mutex,   405]  [debug] 21359#21359: accept mutex locked
2015/12/31 06:36:20[           ngx_epoll_process_events,  1713]  [debug] 21359#21359: epoll: fd:14 EPOLLIN EPOLLOUT  (ev:2005) d:B24FD158
2015/12/31 06:36:20[           ngx_epoll_process_events,  1759]  [debug] 21359#21359: *1 post event B06FD098
2015/12/31 06:36:20[           ngx_epoll_process_events,  1786]  [debug] 21359#21359: *1 post event AE8FD098
2015/12/31 06:36:20[           ngx_event_process_posted,    65]  [debug] 21359#21359: begin to run befor posted event B06FD098
2015/12/31 06:36:20[           ngx_event_process_posted,    67]  [debug] 21359#21359: *1 delete posted event B06FD098
2015/12/31 06:36:20[          ngx_http_upstream_handler,  1400]  [debug] 21359#21359: *1 http upstream request(ev->write:0): "/test2.php?"
2015/12/31 06:36:20[   ngx_http_upstream_process_header,  2485]  [debug] 21359#21359: *1 http upstream process header, fd:14, buffer_size:4096
2015/12/31 06:36:20[                      ngx_unix_recv,   204]  [debug] 21359#21359: *1 recv: fd:14 read-size:296 of 3951, ready:1
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 01
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 06
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 00
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 01
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 01
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 0C
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 04
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 00
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3181]  [debug] 21359#21359: *1 http fastcgi record length: 268
2015/12/31 06:36:20[    ngx_http_fastcgi_process_header,  2349]  [debug] 21359#21359: *1 http fastcgi parser: 0
2015/12/31 06:36:20[    ngx_http_fastcgi_process_header,  2457]  [debug] 21359#21359: *1 http fastcgi header: "X-Powered-By: PHP/5.2.13"
2015/12/31 06:36:20[    ngx_http_fastcgi_process_header,  2349]  [debug] 21359#21359: *1 http fastcgi parser: 0
2015/12/31 06:36:20[    ngx_http_fastcgi_process_header,  2457]  [debug] 21359#21359: *1 http fastcgi header: "Content-type: text/html"
2015/12/31 06:36:20[    ngx_http_fastcgi_process_header,  2349]  [debug] 21359#21359: *1 http fastcgi parser: 1
2015/12/31 06:36:20[    ngx_http_fastcgi_process_header,  2473]  [debug] 21359#21359: *1 http fastcgi header done
2015/12/31 06:36:20[               ngx_http_send_header,  3285][yangya  [debug] 21359#21359: *1 ngx http send header
2015/12/31 06:36:20[             ngx_http_header_filter,   677]  [debug] 21359#21359: *1 HTTP/1.1 200 OK
Server: nginx/1.9.2
Date: Wed, 30 Dec 2015 22:36:20 GMT
Content-Type: text/html
Transfer-Encoding: chunked
Connection: keep-alive
X-Powered-By: PHP/5.2.13

2015/12/31 06:36:20[              ngx_http_write_filter,   207]  [debug] 21359#21359: *1 write new buf t:1 f:0 080F2AE8, pos 080F2AE8, size: 180 file: 0, size: 0
2015/12/31 06:36:20[              ngx_http_write_filter,   247]  [debug] 21359#21359: *1 http write filter: l:0 f:0 s:180
2015/12/31 06:36:20[    ngx_http_upstream_send_response,  3190]  [debug] 21359#21359: *1 ngx_http_upstream_send_response, buffering flag:1
2015/12/31 06:36:20[     ngx_http_file_cache_set_header,  1381]  [debug] 21359#21359: *1 http file cache set header
2015/12/31 06:36:20[    ngx_http_upstream_send_response,  3373]  [debug] 21359#21359: *1 http cacheable: 1, r->cache->valid_sec:1451601380, now:1451514980
2015/12/31 06:36:20[    ngx_http_upstream_send_response,  3413][yangya  [debug] 21359#21359: *1 ngx_http_upstream_send_response, p->cacheable:1, tempfile:/var/yyz/cache_xxx/temp, pathfile:/var/yyz/cache_xxx
2015/12/31 06:36:20[ ngx_http_upstream_process_upstream,  4127]  [debug] 21359#21359: *1 http upstream process upstream
2015/12/31 06:36:20[       ngx_event_pipe_read_upstream,   149]  [debug] 21359#21359: *1 pipe read upstream, read ready: 1
2015/12/31 06:36:20[       ngx_event_pipe_read_upstream,   175]  [debug] 21359#21359: *1 pipe preread: 235
2015/12/31 06:36:20[                    ngx_readv_chain,   106]  [debug] 21359#21359: *1 readv: 1, last(iov_len):3655
2015/12/31 06:36:20[       ngx_event_pipe_read_upstream,   349]  [debug] 21359#21359: *1 pipe recv chain: 0, left-size:3655
2015/12/31 06:36:20[       ngx_event_pipe_read_upstream,   500]  [debug] 21359#21359: *1 pipe buf free s:0 t:1 f:0 080F3898, pos 080F3966, size: 235 file: 0, size: 0
2015/12/31 06:36:20[       ngx_event_pipe_read_upstream,   509]  [debug] 21359#21359: *1 pipe length: -1, p->upstream_eof:1, p->upstream_error:0, p->free_raw_bufs:080F27E0, upstream_done:0
2015/12/31 06:36:20[      ngx_http_fastcgi_input_filter,  2798]  [debug] 21359#21359: *1 input buf #0 080F3966
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 01
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 03
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 00
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 01
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 00
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 08
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 00
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3109]  [debug] 21359#21359: *1 http fastcgi record byte: 00
2015/12/31 06:36:20[    ngx_http_fastcgi_process_record,  3181]  [debug] 21359#21359: *1 http fastcgi record length: 8
2015/12/31 06:36:20[      ngx_http_fastcgi_input_filter,  2837][yangya  [debug] 21359#21359: *1 fastcgi input filter upstream_done:1
2015/12/31 06:36:20[      ngx_http_fastcgi_input_filter,  2844]  [debug] 21359#21359: *1 input buf 080F3966 215
2015/12/31 06:36:20[       ngx_event_pipe_read_upstream,   565]  [debug] 21359#21359: *1 pipe write chain
2015/12/31 06:36:20[ngx_event_pipe_write_chain_to_temp_file,   993][ya  [debug] 21359#21359: *1 ngx_event_pipe_write_chain_to_temp_file, p->buf_to_file:080F27E8, p->cacheable:1, tempfile:/var/yyz/cache_xxx/temp
2015/12/31 06:36:20[               ngx_create_temp_file,   196]  [debug] 21359#21359: *1 hashed path: /var/yyz/cache_xxx/temp/1/00/0000000001, persistent:U, access:U
2015/12/31 06:36:20[               ngx_create_temp_file,   201]  [debug] 21359#21359: *1 temp fd:-1
2015/12/31 06:36:20[                    ngx_create_path,   293]  [debug] 21359#21359: *1 temp file: "/var/yyz/cache_xxx/temp/1"
2015/12/31 06:36:20[                    ngx_create_path,   293]  [debug] 21359#21359: *1 temp file: "/var/yyz/cache_xxx/temp/1/00"
2015/12/31 06:36:20[               ngx_create_temp_file,   196]  [debug] 21359#21359: *1 hashed path: /var/yyz/cache_xxx/temp/1/00/0000000001, persistent:U, access:U
2015/12/31 06:36:20[               ngx_create_temp_file,   201]  [debug] 21359#21359: *1 temp fd:15
2015/12/31 06:36:20[                     ngx_write_file,   234]  [debug] 21359#21359: *1 write to filename:/var/yyz/cache_xxx/temp/1/00/0000000001,fd: 15, buf:080F3898, size:421, offset:0
2015/12/31 06:36:20[       ngx_event_pipe_read_upstream,   574][yangya  [debug] 21359#21359: *1 pipe read upstream upstream_done:1
2015/12/31 06:36:20[ ngx_event_pipe_write_to_downstream,   600]  [debug] 21359#21359: *1 pipe write downstream, write ready: 1
2015/12/31 06:36:20[ ngx_event_pipe_write_to_downstream,   620]  [debug] 21359#21359: *1 pipe write downstream flush out
2015/12/31 06:36:20[             ngx_http_output_filter,  3338]  [debug] 21359#21359: *1 http output filter "/test2.php?"
2015/12/31 06:36:20[               ngx_http_copy_filter,   160]  [debug] 21359#21359: *1 http copy filter: "/test2.php?", r->aio:0
2015/12/31 06:36:20[                   ngx_output_chain,    67][yangya  [debug] 21359#21359: *1 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2015/12/31 06:36:20[                  ngx_file_aio_read,   251]  [debug] 21359#21359: *1 aio complete:0 @206:215 /var/yyz/cache_xxx/temp/1/00/0000000001
2015/12/31 06:36:20[               ngx_http_copy_filter,   231]  [debug] 21359#21359: *1 http copy filter: -2 "/test2.php?"
2015/12/31 06:36:20[ ngx_event_pipe_write_to_downstream,   658]  [debug] 21359#21359: *1 pipe write downstream done
2015/12/31 06:36:20[                ngx_event_del_timer,    39]  [debug] 21359#21359: *1 <           ngx_event_pipe,    87>  event timer del: 14: 4111061275
2015/12/31 06:36:20[         ngx_http_file_cache_update,  1508]  [debug] 21359#21359: *1 http file cache update, c->body_start:206
2015/12/31 06:36:20[         ngx_http_file_cache_update,  1521]  [debug] 21359#21359: *1 http file cache rename: "/var/yyz/cache_xxx/temp/1/00/0000000001" to "/var/yyz/cache_xxx/f/27/46492fbf0d9d35d3753c66851e81627f", expire time:1800
2015/12/31 06:36:20[                     ngx_shmtx_lock,   168]  [debug] 21359#21359: shmtx lock
2015/12/31 06:36:20[                   ngx_shmtx_unlock,   249]  [debug] 21359#21359: shmtx unlock
2015/12/31 06:36:20[  ngx_http_upstream_process_request,  4250]  [debug] 21359#21359: *1 http upstream exit: 00000000
2015/12/31 06:36:20[ ngx_http_upstream_finalize_request,  4521]  [debug] 21359#21359: *1 finalize http upstream request rc: 0
2015/12/31 06:36:20[  ngx_http_fastcgi_finalize_request,  3215]  [debug] 21359#21359: *1 finalize http fastcgi request
2015/12/31 06:36:20[ngx_http_upstream_free_round_robin_peer,   887]  [debug] 21359#21359: *1 free rr peer 1 0
2015/12/31 06:36:20[ ngx_http_upstream_finalize_request,  4574]  [debug] 21359#21359: *1 close http upstream connection: 14
2015/12/31 06:36:20[               ngx_close_connection,  1120]  [debug] 21359#21359: *1 delete posted event AE8FD098
2015/12/31 06:36:20[            ngx_reusable_connection,  1177]  [debug] 21359#21359: *1 reusable connection: 0
2015/12/31 06:36:20[               ngx_close_connection,  1139][yangya  [debug] 21359#21359: close socket:14
2015/12/31 06:36:20[ ngx_http_upstream_finalize_request,  4588]  [debug] 21359#21359: *1 http upstream temp fd: 15
2015/12/31 06:36:20[              ngx_http_send_special,  3843][yangya  [debug] 21359#21359: *1 ngx http send special, flags:1
2015/12/31 06:36:20[             ngx_http_output_filter,  3338]  [debug] 21359#21359: *1 http output filter "/test2.php?"
2015/12/31 06:36:20[               ngx_http_copy_filter,   160]  [debug] 21359#21359: *1 http copy filter: "/test2.php?", r->aio:1
2015/12/31 06:36:20[                   ngx_output_chain,    67][yangya  [debug] 21359#21359: *1 ctx->sendfile:0, ctx->aio:1, ctx->directio:0
2015/12/31 06:36:20[               ngx_http_copy_filter,   231]  [debug] 21359#21359: *1 http copy filter: -2 "/test2.php?"
2015/12/31 06:36:20[          ngx_http_finalize_request,  2592]  [debug] 21359#21359: *1 http finalize request rc: -2, "/test2.php?" a:1, c:1
2015/12/31 06:36:20[                ngx_event_add_timer,   100]  [debug] 21359#21359: *1 <ngx_http_set_write_handler,  3013>  event timer add fd:13, expire-time:60 s, timer.key:4111061277
2015/12/31 06:36:20[           ngx_trylock_accept_mutex,   405]  [debug] 21359#21359: accept mutex locked
2015/12/31 06:36:20[           ngx_epoll_process_events,  1713]  [debug] 21359#21359: epoll: fd:10 EPOLLIN  (ev:0001) d:080E2520
2015/12/31 06:36:20[           ngx_epoll_process_events,  1759]  [debug] 21359#21359: post event 080E24E0
2015/12/31 06:36:20[           ngx_event_process_posted,    65]  [debug] 21359#21359: begin to run befor posted event 080E24E0
2015/12/31 06:36:20[           ngx_event_process_posted,    67]  [debug] 21359#21359: delete posted event 080E24E0
2015/12/31 06:36:20[          ngx_epoll_eventfd_handler,  1823]  [debug] 21359#21359: eventfd handler
2015/12/31 06:36:20[          ngx_epoll_eventfd_handler,  1829]  [debug] 21359#21359: aio epoll handler eventfd: 8
2015/12/31 06:36:20[          ngx_epoll_eventfd_handler,  1855]  [debug] 21359#21359: io_getevents: 1
2015/12/31 06:36:20[          ngx_epoll_eventfd_handler,  1865]  [debug] 21359#21359: io_event: 80F2DA4 80F2D64 215 0
2015/12/31 06:36:20[          ngx_epoll_eventfd_handler,  1879]  [debug] 21359#21359: *1 post event 080F2DA4
2015/12/31 06:36:20[           ngx_event_process_posted,    65]  [debug] 21359#21359: begin to run befor posted event 080F2DA4
2015/12/31 06:36:20[           ngx_event_process_posted,    67]  [debug] 21359#21359: *1 delete posted event 080F2DA4
2015/12/31 06:36:20[         ngx_file_aio_event_handler,   329]  [debug] 21359#21359: *1 aio event handler fd:15 /var/yyz/cache_xxx/temp/1/00/0000000001
2015/12/31 06:36:20[           ngx_http_request_handler,  2401]  [debug] 21359#21359: *1 http run request(ev->write:1): "/test2.php?"
2015/12/31 06:36:20[                    ngx_http_writer,  3042]  [debug] 21359#21359: *1 http writer handler: "/test2.php?"
2015/12/31 06:36:20[             ngx_http_output_filter,  3338]  [debug] 21359#21359: *1 http output filter "/test2.php?"
2015/12/31 06:36:20[               ngx_http_copy_filter,   160]  [debug] 21359#21359: *1 http copy filter: "/test2.php?", r->aio:0
2015/12/31 06:36:20[                   ngx_output_chain,    67][yangya  [debug] 21359#21359: *1 ctx->sendfile:0, ctx->aio:0, ctx->directio:0
2015/12/31 06:36:20[                  ngx_file_aio_read,   251]  [debug] 21359#21359: *1 aio complete:1 @206:215 /var/yyz/cache_xxx/temp/1/00/0000000001
2015/12/31 06:36:20[           ngx_http_postpone_filter,   176]  [debug] 21359#21359: *1 http postpone filter "/test2.php?" 080F2E10
2015/12/31 06:36:20[       ngx_http_chunked_body_filter,   212]  [debug] 21359#21359: *1 http chunk: 215
2015/12/31 06:36:20[       ngx_http_chunked_body_filter,   212]  [debug] 21359#21359: *1 http chunk: 0
2015/12/31 06:36:20[       ngx_http_chunked_body_filter,   273]  [debug] 21359#21359: *1 yang test ..........xxxxxxxx ################## lstbuf:1
2015/12/31 06:36:20[              ngx_http_write_filter,   151]  [debug] 21359#21359: *1 write old buf t:1 f:0 080F2AE8, pos 080F2AE8, size: 180 file: 0, size: 0
2015/12/31 06:36:20[              ngx_http_write_filter,   207]  [debug] 21359#21359: *1 write new buf t:1 f:0 080F2E5C, pos 080F2E5C, size: 4 file: 0, size: 0
2015/12/31 06:36:20[              ngx_http_write_filter,   207]  [debug] 21359#21359: *1 write new buf t:1 f:0 081109E0, pos 081109E0, size: 215 file: 0, size: 0
2015/12/31 06:36:20[              ngx_http_write_filter,   207]  [debug] 21359#21359: *1 write new buf t:0 f:0 00000000, pos 080CDCD8, size: 7 file: 0, size: 0
2015/12/31 06:36:20[              ngx_http_write_filter,   247]  [debug] 21359#21359: *1 http write filter: l:1 f:1 s:406
2015/12/31 06:36:20[              ngx_http_write_filter,   377]  [debug] 21359#21359: *1 http write filter limit 0
2015/12/31 06:36:20[                         ngx_writev,   199]  [debug] 21359#21359: *1 writev: 406 of 406
2015/12/31 06:36:20[              ngx_http_write_filter,   383]  [debug] 21359#21359: *1 http write filter 00000000
2015/12/31 06:36:20[               ngx_http_copy_filter,   231]  [debug] 21359#21359: *1 http copy filter: 0 "/test2.php?"
2015/12/31 06:36:20[                    ngx_http_writer,  3108]  [debug] 21359#21359: *1 http writer output filter: 0, "/test2.php?"
2015/12/31 06:36:20[                    ngx_http_writer,  3140]  [debug] 21359#21359: *1 http writer done: "/test2.php?"
2015/12/31 06:36:20[          ngx_http_finalize_request,  2592]  [debug] 21359#21359: *1 http finalize request rc: 0, "/test2.php?" a:1, c:1
2015/12/31 06:36:20[                ngx_event_del_timer,    39]  [debug] 21359#21359: *1 <ngx_http_finalize_request,  2830>  event timer del: 13: 4111061277
2015/12/31 06:36:20[             ngx_http_set_keepalive,  3315]  [debug] 21359#21359: *1 set http keepalive handler
2015/12/31 06:36:20[              ngx_http_free_request,  3955]  [debug] 21359#21359: *1 http close request
2015/12/31 06:36:20[               ngx_http_log_handler,   267]  [debug] 21359#21359: *1 http log handler
2015/12/31 06:36:20[             ngx_http_set_keepalive,  3434]  [debug] 21359#21359: *1 hc free: 00000000 0
2015/12/31 06:36:20[             ngx_http_set_keepalive,  3446]  [debug] 21359#21359: *1 hc busy: 00000000 0
2015/12/31 06:36:20[             ngx_http_set_keepalive,  3492]  [debug] 21359#21359: *1 tcp_nodelay
2015/12/31 06:36:20[            ngx_reusable_connection,  1177]  [debug] 21359#21359: *1 reusable connection: 1
2015/12/31 06:36:20[                ngx_event_add_timer,   100]  [debug] 21359#21359: *1 <   ngx_http_set_keepalive,  3522>  event timer add fd:13, expire-time:75 s, timer.key:4111076279
2015/12/31 06:36:20[             ngx_http_set_keepalive,  3525]  [debug] 21359#21359: *1 post event B06FD068
2015/12/31 06:36:20[           ngx_event_process_posted,    65]  [debug] 21359#21359: begin to run befor posted event B06FD068
2015/12/31 06:36:20[           ngx_event_process_posted,    67]  [debug] 21359#21359: *1 delete posted event B06FD068
2015/12/31 06:36:20[         ngx_http_keepalive_handler,  3545]  [debug] 21359#21359: *1 http keepalive handler
2015/12/31 06:36:20[                      ngx_unix_recv,   185]  [debug] 21359#21359: *1 recv() not ready (11: Resource temporarily unavailable)
2015/12/31 06:36:20[                      ngx_unix_recv,   204]  [debug] 21359#21359: *1 recv: fd:13 read-size:-2 of 1024, ready:0
2015/12/31 06:36:20[           ngx_trylock_accept_mutex,   405]  [debug] 21359#21359: accept mutex locked
2015/12/31 06:36:20[           ngx_epoll_process_events,  1713]  [debug] 21359#21359: epoll: fd:13 EPOLLIN EPOLLOUT  (ev:2005) d:B24FD0E8
2015/12/31 06:36:20[           ngx_epoll_process_events,  1759]  [debug] 21359#21359: *1 post event B06FD068
2015/12/31 06:36:20[           ngx_epoll_process_events,  1786]  [debug] 21359#21359: *1 post event AE8FD068
2015/12/31 06:36:20[           ngx_event_process_posted,    65]  [debug] 21359#21359: begin to run befor posted event B06FD068
2015/12/31 06:36:20[           ngx_event_process_posted,    67]  [debug] 21359#21359: *1 delete posted event B06FD068
2015/12/31 06:36:20[         ngx_http_keepalive_handler,  3545]  [debug] 21359#21359: *1 http keepalive handler
2015/12/31 06:36:20[                      ngx_unix_recv,   204]  [debug] 21359#21359: *1 recv: fd:13 read-size:0 of 1024, ready:0
2015/12/31 06:36:20[         ngx_http_keepalive_handler,  3637]  [info] 21359#21359: *1 client 10.2.13.167 closed keepalive connection
2015/12/31 06:36:20[          ngx_http_close_connection,  4140]  [debug] 21359#21359: *1 close http connection: 13
2015/12/31 06:36:20[                ngx_event_del_timer,    39]  [debug] 21359#21359: *1 <     ngx_close_connection,  1090>  event timer del: 13: 4111076279
2015/12/31 06:36:20[               ngx_close_connection,  1120]  [debug] 21359#21359: *1 delete posted event AE8FD068
2015/12/31 06:36:20[            ngx_reusable_connection,  1177]  [debug] 21359#21359: *1 reusable connection: 0
2015/12/31 06:36:20[               ngx_close_connection,  1139][yangya  [debug] 21359#21359: close socket:13
2015/12/31 06:36:20[           ngx_trylock_accept_mutex,   405]  [debug] 21359#21359: accept mutex locked
2015/12/31 06:36:21[            ngx_event_expire_timers,   133]  [debug] 21361#21361: event timer del: -1: 4111002281
2015/12/31 06:36:21[         ngx_http_file_cache_expire,  1951]  [debug] 21361#21361: http file cache expire
2015/12/31 06:36:21[                     ngx_shmtx_lock,   168]  [debug] 21361#21361: shmtx lock
2015/12/31 06:36:21[                   ngx_shmtx_unlock,   249]  [debug] 21361#21361: shmtx unlock
2015/12/31 06:36:21[                     ngx_shmtx_lock,   168]  [debug] 21361#21361: shmtx lock
2015/12/31 06:36:21[                   ngx_shmtx_unlock,   249]  [debug] 21361#21361: shmtx unlock
2015/12/31 06:36:21[        ngx_http_file_cache_manager,  2150]  [debug] 21361#21361: http file cache size: 1, max_size:262144
2015/12/31 06:36:21[                ngx_event_add_timer,   100]  [debug] 21361#21361: <ngx_cache_manager_process_handler,  1646>  event timer add fd:-1, expire-time:10 s, timer.key:4111012282

*/

