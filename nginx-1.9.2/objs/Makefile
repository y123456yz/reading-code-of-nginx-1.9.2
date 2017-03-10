
CC =	cc
CFLAGS = -I/usr/local/include/luajit-2.0  -pipe  -O -W -Wall -Wpointer-arith -Wno-unused-parameter -Werror -g  -DNDK_SET_VAR
CPP =	cc -E
LINK =	$(CC)


ALL_INCS = -I src/core \
	-I src/event \
	-I src/event/modules \
	-I src/os/unix \
	-I /usr/local/include/luajit-2.0 \
	-I ./src/lua-nginx-module-master//src/api \
	-I /usr/local/openssl/../openssl/include \
	-I objs \
	-I src/http \
	-I src/http/modules \
	-I src/http/v2 \
	-I src/mail \
	-I src/stream


CORE_DEPS = src/core/nginx.h \
	src/core/ngx_config.h \
	src/core/ngx_core.h \
	src/core/ngx_log.h \
	src/core/ngx_palloc.h \
	src/core/ngx_array.h \
	src/core/ngx_list.h \
	src/core/ngx_hash.h \
	src/core/ngx_buf.h \
	src/core/ngx_queue.h \
	src/core/ngx_string.h \
	src/core/ngx_parse.h \
	src/core/ngx_parse_time.h \
	src/core/ngx_inet.h \
	src/core/ngx_file.h \
	src/core/ngx_crc.h \
	src/core/ngx_crc32.h \
	src/core/ngx_murmurhash.h \
	src/core/ngx_md5.h \
	src/core/ngx_sha1.h \
	src/core/ngx_rbtree.h \
	src/core/ngx_radix_tree.h \
	src/core/ngx_rwlock.h \
	src/core/ngx_slab.h \
	src/core/ngx_times.h \
	src/core/ngx_shmtx.h \
	src/core/ngx_connection.h \
	src/core/ngx_cycle.h \
	src/core/ngx_conf_file.h \
	src/core/ngx_resolver.h \
	src/core/ngx_open_file_cache.h \
	src/core/ngx_crypt.h \
	src/core/ngx_proxy_protocol.h \
	src/core/ngx_syslog.h \
	src/event/ngx_event.h \
	src/event/ngx_event_timer.h \
	src/event/ngx_event_posted.h \
	src/event/ngx_event_connect.h \
	src/event/ngx_event_pipe.h \
	src/os/unix/ngx_time.h \
	src/os/unix/ngx_errno.h \
	src/os/unix/ngx_alloc.h \
	src/os/unix/ngx_files.h \
	src/os/unix/ngx_channel.h \
	src/os/unix/ngx_shmem.h \
	src/os/unix/ngx_process.h \
	src/os/unix/ngx_setaffinity.h \
	src/os/unix/ngx_setproctitle.h \
	src/os/unix/ngx_atomic.h \
	src/os/unix/ngx_gcc_atomic_x86.h \
	src/os/unix/ngx_thread.h \
	src/os/unix/ngx_socket.h \
	src/os/unix/ngx_os.h \
	src/os/unix/ngx_user.h \
	src/os/unix/ngx_process_cycle.h \
	src/os/unix/ngx_linux_config.h \
	src/os/unix/ngx_linux.h \
	src/core/ngx_thread_pool.h \
	src/event/ngx_event_openssl.h \
	src/core/ngx_regex.h \
	/usr/local/openssl/../openssl/include/openssl/ssl.h \
	objs/ngx_auto_config.h


CORE_INCS = -I src/core \
	-I src/event \
	-I src/event/modules \
	-I src/os/unix \
	-I /usr/local/include/luajit-2.0 \
	-I ./src/lua-nginx-module-master//src/api \
	-I /usr/local/openssl/../openssl/include \
	-I objs


HTTP_DEPS = src/http/ngx_http.h \
	src/http/ngx_http_request.h \
	src/http/ngx_http_config.h \
	src/http/ngx_http_core_module.h \
	src/http/ngx_http_cache.h \
	src/http/ngx_http_variables.h \
	src/http/ngx_http_script.h \
	src/http/ngx_http_upstream.h \
	src/http/ngx_http_upstream_round_robin.h \
	src/http/modules/ngx_http_ssi_filter_module.h \
	src/http/v2/ngx_http_v2.h \
	src/http/v2/ngx_http_v2_module.h \
	src/http/modules/ngx_http_ssl_module.h


HTTP_INCS = -I src/http \
	-I src/http/modules \
	-I src/http/v2


ADDON_DEPS = $(CORE_DEPS)  /root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_module.h /root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ddebug.h /root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_handler.h /root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_util.h /root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_sleep.h /root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_filter.h /root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_var.h /root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_location.h /root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_echo.h /root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_request_info.h /root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_subrequest.h /root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_foreach.h ./src/redis2-nginx-module-master//src/ddebug.h ./src/redis2-nginx-module-master//src/ngx_http_redis2_module.h ./src/redis2-nginx-module-master//src/ngx_http_redis2_handler.h ./src/redis2-nginx-module-master//src/ngx_http_redis2_reply.h ./src/redis2-nginx-module-master//src/ngx_http_redis2_util.h                 ./src/lua-nginx-module-master//src/ddebug.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_script.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_log.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_subrequest.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_ndk.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_control.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_time.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_string.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_misc.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_variable.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_output.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_headers.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_uri.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_req_body.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_args.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_ctx.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_regex.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_common.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_directive.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_headers_out.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_headers_in.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_consts.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_exception.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_util.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_cache.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_contentby.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_rewriteby.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_accessby.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_setby.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_capturefilter.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_clfactory.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_pcrefix.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_headerfilterby.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_shdict.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_socket_tcp.h                 ./src/lua-nginx-module-master//src/api/ngx_http_lua_api.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_logby.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_sleep.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_semaphore.h                ./src/lua-nginx-module-master//src/ngx_http_lua_coroutine.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_bodyfilterby.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_initby.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_initworkerby.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_socket_udp.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_req_method.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_phase.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_probe.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_uthread.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_timer.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_config.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_worker.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_ssl_certby.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_lex.h                 ./src/lua-nginx-module-master//src/ngx_http_lua_balancer.h                  $(HTTP_DEPS)


objs/nginx:	objs/src/core/nginx.o \
	objs/src/core/ngx_log.o \
	objs/src/core/ngx_palloc.o \
	objs/src/core/ngx_array.o \
	objs/src/core/ngx_list.o \
	objs/src/core/ngx_hash.o \
	objs/src/core/ngx_buf.o \
	objs/src/core/ngx_queue.o \
	objs/src/core/ngx_output_chain.o \
	objs/src/core/ngx_string.o \
	objs/src/core/ngx_parse.o \
	objs/src/core/ngx_parse_time.o \
	objs/src/core/ngx_inet.o \
	objs/src/core/ngx_file.o \
	objs/src/core/ngx_crc32.o \
	objs/src/core/ngx_murmurhash.o \
	objs/src/core/ngx_md5.o \
	objs/src/core/ngx_rbtree.o \
	objs/src/core/ngx_radix_tree.o \
	objs/src/core/ngx_slab.o \
	objs/src/core/ngx_times.o \
	objs/src/core/ngx_shmtx.o \
	objs/src/core/ngx_connection.o \
	objs/src/core/ngx_cycle.o \
	objs/src/core/ngx_spinlock.o \
	objs/src/core/ngx_rwlock.o \
	objs/src/core/ngx_cpuinfo.o \
	objs/src/core/ngx_conf_file.o \
	objs/src/core/ngx_resolver.o \
	objs/src/core/ngx_open_file_cache.o \
	objs/src/core/ngx_crypt.o \
	objs/src/core/ngx_proxy_protocol.o \
	objs/src/core/ngx_syslog.o \
	objs/src/event/ngx_event.o \
	objs/src/event/ngx_event_timer.o \
	objs/src/event/ngx_event_posted.o \
	objs/src/event/ngx_event_accept.o \
	objs/src/event/ngx_event_connect.o \
	objs/src/event/ngx_event_pipe.o \
	objs/src/os/unix/ngx_time.o \
	objs/src/os/unix/ngx_errno.o \
	objs/src/os/unix/ngx_alloc.o \
	objs/src/os/unix/ngx_files.o \
	objs/src/os/unix/ngx_socket.o \
	objs/src/os/unix/ngx_recv.o \
	objs/src/os/unix/ngx_readv_chain.o \
	objs/src/os/unix/ngx_udp_recv.o \
	objs/src/os/unix/ngx_send.o \
	objs/src/os/unix/ngx_writev_chain.o \
	objs/src/os/unix/ngx_channel.o \
	objs/src/os/unix/ngx_shmem.o \
	objs/src/os/unix/ngx_process.o \
	objs/src/os/unix/ngx_daemon.o \
	objs/src/os/unix/ngx_setaffinity.o \
	objs/src/os/unix/ngx_setproctitle.o \
	objs/src/os/unix/ngx_posix_init.o \
	objs/src/os/unix/ngx_user.o \
	objs/src/os/unix/ngx_process_cycle.o \
	objs/src/os/unix/ngx_linux_init.o \
	objs/src/event/modules/ngx_epoll_module.o \
	objs/src/os/unix/ngx_linux_sendfile_chain.o \
	objs/src/os/unix/ngx_linux_aio_read.o \
	objs/src/core/ngx_thread_pool.o \
	objs/src/os/unix/ngx_thread_cond.o \
	objs/src/os/unix/ngx_thread_mutex.o \
	objs/src/os/unix/ngx_thread_id.o \
	objs/src/event/ngx_event_openssl.o \
	objs/src/event/ngx_event_openssl_stapling.o \
	objs/src/core/ngx_regex.o \
	objs/src/http/ngx_http.o \
	objs/src/http/ngx_http_core_module.o \
	objs/src/http/ngx_http_special_response.o \
	objs/src/http/ngx_http_request.o \
	objs/src/http/ngx_http_parse.o \
	objs/src/http/ngx_http_header_filter_module.o \
	objs/src/http/ngx_http_write_filter_module.o \
	objs/src/http/ngx_http_copy_filter_module.o \
	objs/src/http/modules/ngx_http_log_module.o \
	objs/src/http/ngx_http_request_body.o \
	objs/src/http/ngx_http_variables.o \
	objs/src/http/ngx_http_script.o \
	objs/src/http/ngx_http_upstream.o \
	objs/src/http/ngx_http_upstream_round_robin.o \
	objs/src/http/modules/ngx_http_static_module.o \
	objs/src/http/modules/ngx_http_index_module.o \
	objs/src/http/modules/ngx_http_chunked_filter_module.o \
	objs/src/http/modules/ngx_http_range_filter_module.o \
	objs/src/http/modules/ngx_http_headers_filter_module.o \
	objs/src/http/modules/ngx_http_not_modified_filter_module.o \
	objs/src/http/ngx_http_file_cache.o \
	objs/src/http/modules/ngx_http_gzip_filter_module.o \
	objs/src/http/ngx_http_postpone_filter_module.o \
	objs/src/http/modules/ngx_http_ssi_filter_module.o \
	objs/src/http/modules/ngx_http_charset_filter_module.o \
	objs/src/http/modules/ngx_http_userid_filter_module.o \
	objs/src/http/v2/ngx_http_v2.o \
	objs/src/http/v2/ngx_http_v2_table.o \
	objs/src/http/v2/ngx_http_v2_huff_decode.o \
	objs/src/http/v2/ngx_http_v2_huff_encode.o \
	objs/src/http/v2/ngx_http_v2_module.o \
	objs/src/http/v2/ngx_http_v2_filter_module.o \
	objs/src/http/modules/ngx_http_autoindex_module.o \
	objs/src/http/modules/ngx_http_auth_basic_module.o \
	objs/src/http/modules/ngx_http_access_module.o \
	objs/src/http/modules/ngx_http_limit_conn_module.o \
	objs/src/http/modules/ngx_http_limit_req_module.o \
	objs/src/http/modules/ngx_http_geo_module.o \
	objs/src/http/modules/ngx_http_map_module.o \
	objs/src/http/modules/ngx_http_split_clients_module.o \
	objs/src/http/modules/ngx_http_referer_module.o \
	objs/src/http/modules/ngx_http_rewrite_module.o \
	objs/src/http/modules/ngx_http_ssl_module.o \
	objs/src/http/modules/ngx_http_proxy_module.o \
	objs/src/http/modules/ngx_http_fastcgi_module.o \
	objs/src/http/modules/ngx_http_uwsgi_module.o \
	objs/src/http/modules/ngx_http_scgi_module.o \
	objs/src/http/modules/ngx_http_memcached_module.o \
	objs/src/http/modules/ngx_http_empty_gif_module.o \
	objs/src/http/modules/ngx_http_browser_module.o \
	objs/src/http/modules/ngx_http_secure_link_module.o \
	objs/src/http/modules/ngx_http_upstream_hash_module.o \
	objs/src/http/modules/ngx_http_upstream_ip_hash_module.o \
	objs/src/http/modules/ngx_http_upstream_least_conn_module.o \
	objs/src/http/modules/ngx_http_upstream_keepalive_module.o \
	objs/src/http/modules/ngx_http_upstream_zone_module.o \
	objs/addon/mytest_config/mytest_config.o \
	objs/addon/my_test_module/ngx_http_mytest_module.o \
	objs/addon/mytest_subrequest/ngx_http_subrequest_mytest_module.o \
	objs/addon/mytest_upstream/mytest_upstream.o \
	objs/addon/ngx_http_myfilter_module/ngx_http_myfilter_module.o \
	objs/addon/sendfile_test/sendfile_test.o \
	objs/addon/src/ngx_http_echo_module.o \
	objs/addon/src/ngx_http_echo_util.o \
	objs/addon/src/ngx_http_echo_timer.o \
	objs/addon/src/ngx_http_echo_var.o \
	objs/addon/src/ngx_http_echo_handler.o \
	objs/addon/src/ngx_http_echo_filter.o \
	objs/addon/src/ngx_http_echo_sleep.o \
	objs/addon/src/ngx_http_echo_location.o \
	objs/addon/src/ngx_http_echo_echo.o \
	objs/addon/src/ngx_http_echo_request_info.o \
	objs/addon/src/ngx_http_echo_subrequest.o \
	objs/addon/src/ngx_http_echo_foreach.o \
	objs/addon/nginx-requestkey-module-master/ngx_http_accesskey_module.o \
	objs/addon/src/ngx_http_redis2_module.o \
	objs/addon/src/ngx_http_redis2_handler.o \
	objs/addon/src/ngx_http_redis2_reply.o \
	objs/addon/src/ngx_http_redis2_util.o \
	objs/addon/src/ngx_http_lua_script.o \
	objs/addon/src/ngx_http_lua_log.o \
	objs/addon/src/ngx_http_lua_subrequest.o \
	objs/addon/src/ngx_http_lua_ndk.o \
	objs/addon/src/ngx_http_lua_control.o \
	objs/addon/src/ngx_http_lua_time.o \
	objs/addon/src/ngx_http_lua_misc.o \
	objs/addon/src/ngx_http_lua_variable.o \
	objs/addon/src/ngx_http_lua_string.o \
	objs/addon/src/ngx_http_lua_output.o \
	objs/addon/src/ngx_http_lua_headers.o \
	objs/addon/src/ngx_http_lua_req_body.o \
	objs/addon/src/ngx_http_lua_uri.o \
	objs/addon/src/ngx_http_lua_args.o \
	objs/addon/src/ngx_http_lua_ctx.o \
	objs/addon/src/ngx_http_lua_regex.o \
	objs/addon/src/ngx_http_lua_module.o \
	objs/addon/src/ngx_http_lua_headers_out.o \
	objs/addon/src/ngx_http_lua_headers_in.o \
	objs/addon/src/ngx_http_lua_directive.o \
	objs/addon/src/ngx_http_lua_consts.o \
	objs/addon/src/ngx_http_lua_exception.o \
	objs/addon/src/ngx_http_lua_util.o \
	objs/addon/src/ngx_http_lua_cache.o \
	objs/addon/src/ngx_http_lua_contentby.o \
	objs/addon/src/ngx_http_lua_rewriteby.o \
	objs/addon/src/ngx_http_lua_accessby.o \
	objs/addon/src/ngx_http_lua_setby.o \
	objs/addon/src/ngx_http_lua_capturefilter.o \
	objs/addon/src/ngx_http_lua_clfactory.o \
	objs/addon/src/ngx_http_lua_pcrefix.o \
	objs/addon/src/ngx_http_lua_headerfilterby.o \
	objs/addon/src/ngx_http_lua_shdict.o \
	objs/addon/src/ngx_http_lua_socket_tcp.o \
	objs/addon/src/ngx_http_lua_api.o \
	objs/addon/src/ngx_http_lua_logby.o \
	objs/addon/src/ngx_http_lua_sleep.o \
	objs/addon/src/ngx_http_lua_semaphore.o \
	objs/addon/src/ngx_http_lua_coroutine.o \
	objs/addon/src/ngx_http_lua_bodyfilterby.o \
	objs/addon/src/ngx_http_lua_initby.o \
	objs/addon/src/ngx_http_lua_initworkerby.o \
	objs/addon/src/ngx_http_lua_socket_udp.o \
	objs/addon/src/ngx_http_lua_req_method.o \
	objs/addon/src/ngx_http_lua_phase.o \
	objs/addon/src/ngx_http_lua_uthread.o \
	objs/addon/src/ngx_http_lua_timer.o \
	objs/addon/src/ngx_http_lua_config.o \
	objs/addon/src/ngx_http_lua_worker.o \
	objs/addon/src/ngx_http_lua_ssl_certby.o \
	objs/addon/src/ngx_http_lua_ssl_ocsp.o \
	objs/addon/src/ngx_http_lua_lex.o \
	objs/addon/src/ngx_http_lua_balancer.o \
	objs/ngx_modules.o

	$(LINK) -o objs/nginx \
	objs/src/core/nginx.o \
	objs/src/core/ngx_log.o \
	objs/src/core/ngx_palloc.o \
	objs/src/core/ngx_array.o \
	objs/src/core/ngx_list.o \
	objs/src/core/ngx_hash.o \
	objs/src/core/ngx_buf.o \
	objs/src/core/ngx_queue.o \
	objs/src/core/ngx_output_chain.o \
	objs/src/core/ngx_string.o \
	objs/src/core/ngx_parse.o \
	objs/src/core/ngx_parse_time.o \
	objs/src/core/ngx_inet.o \
	objs/src/core/ngx_file.o \
	objs/src/core/ngx_crc32.o \
	objs/src/core/ngx_murmurhash.o \
	objs/src/core/ngx_md5.o \
	objs/src/core/ngx_rbtree.o \
	objs/src/core/ngx_radix_tree.o \
	objs/src/core/ngx_slab.o \
	objs/src/core/ngx_times.o \
	objs/src/core/ngx_shmtx.o \
	objs/src/core/ngx_connection.o \
	objs/src/core/ngx_cycle.o \
	objs/src/core/ngx_spinlock.o \
	objs/src/core/ngx_rwlock.o \
	objs/src/core/ngx_cpuinfo.o \
	objs/src/core/ngx_conf_file.o \
	objs/src/core/ngx_resolver.o \
	objs/src/core/ngx_open_file_cache.o \
	objs/src/core/ngx_crypt.o \
	objs/src/core/ngx_proxy_protocol.o \
	objs/src/core/ngx_syslog.o \
	objs/src/event/ngx_event.o \
	objs/src/event/ngx_event_timer.o \
	objs/src/event/ngx_event_posted.o \
	objs/src/event/ngx_event_accept.o \
	objs/src/event/ngx_event_connect.o \
	objs/src/event/ngx_event_pipe.o \
	objs/src/os/unix/ngx_time.o \
	objs/src/os/unix/ngx_errno.o \
	objs/src/os/unix/ngx_alloc.o \
	objs/src/os/unix/ngx_files.o \
	objs/src/os/unix/ngx_socket.o \
	objs/src/os/unix/ngx_recv.o \
	objs/src/os/unix/ngx_readv_chain.o \
	objs/src/os/unix/ngx_udp_recv.o \
	objs/src/os/unix/ngx_send.o \
	objs/src/os/unix/ngx_writev_chain.o \
	objs/src/os/unix/ngx_channel.o \
	objs/src/os/unix/ngx_shmem.o \
	objs/src/os/unix/ngx_process.o \
	objs/src/os/unix/ngx_daemon.o \
	objs/src/os/unix/ngx_setaffinity.o \
	objs/src/os/unix/ngx_setproctitle.o \
	objs/src/os/unix/ngx_posix_init.o \
	objs/src/os/unix/ngx_user.o \
	objs/src/os/unix/ngx_process_cycle.o \
	objs/src/os/unix/ngx_linux_init.o \
	objs/src/event/modules/ngx_epoll_module.o \
	objs/src/os/unix/ngx_linux_sendfile_chain.o \
	objs/src/os/unix/ngx_linux_aio_read.o \
	objs/src/core/ngx_thread_pool.o \
	objs/src/os/unix/ngx_thread_cond.o \
	objs/src/os/unix/ngx_thread_mutex.o \
	objs/src/os/unix/ngx_thread_id.o \
	objs/src/event/ngx_event_openssl.o \
	objs/src/event/ngx_event_openssl_stapling.o \
	objs/src/core/ngx_regex.o \
	objs/src/http/ngx_http.o \
	objs/src/http/ngx_http_core_module.o \
	objs/src/http/ngx_http_special_response.o \
	objs/src/http/ngx_http_request.o \
	objs/src/http/ngx_http_parse.o \
	objs/src/http/ngx_http_header_filter_module.o \
	objs/src/http/ngx_http_write_filter_module.o \
	objs/src/http/ngx_http_copy_filter_module.o \
	objs/src/http/modules/ngx_http_log_module.o \
	objs/src/http/ngx_http_request_body.o \
	objs/src/http/ngx_http_variables.o \
	objs/src/http/ngx_http_script.o \
	objs/src/http/ngx_http_upstream.o \
	objs/src/http/ngx_http_upstream_round_robin.o \
	objs/src/http/modules/ngx_http_static_module.o \
	objs/src/http/modules/ngx_http_index_module.o \
	objs/src/http/modules/ngx_http_chunked_filter_module.o \
	objs/src/http/modules/ngx_http_range_filter_module.o \
	objs/src/http/modules/ngx_http_headers_filter_module.o \
	objs/src/http/modules/ngx_http_not_modified_filter_module.o \
	objs/src/http/ngx_http_file_cache.o \
	objs/src/http/modules/ngx_http_gzip_filter_module.o \
	objs/src/http/ngx_http_postpone_filter_module.o \
	objs/src/http/modules/ngx_http_ssi_filter_module.o \
	objs/src/http/modules/ngx_http_charset_filter_module.o \
	objs/src/http/modules/ngx_http_userid_filter_module.o \
	objs/src/http/v2/ngx_http_v2.o \
	objs/src/http/v2/ngx_http_v2_table.o \
	objs/src/http/v2/ngx_http_v2_huff_decode.o \
	objs/src/http/v2/ngx_http_v2_huff_encode.o \
	objs/src/http/v2/ngx_http_v2_module.o \
	objs/src/http/v2/ngx_http_v2_filter_module.o \
	objs/src/http/modules/ngx_http_autoindex_module.o \
	objs/src/http/modules/ngx_http_auth_basic_module.o \
	objs/src/http/modules/ngx_http_access_module.o \
	objs/src/http/modules/ngx_http_limit_conn_module.o \
	objs/src/http/modules/ngx_http_limit_req_module.o \
	objs/src/http/modules/ngx_http_geo_module.o \
	objs/src/http/modules/ngx_http_map_module.o \
	objs/src/http/modules/ngx_http_split_clients_module.o \
	objs/src/http/modules/ngx_http_referer_module.o \
	objs/src/http/modules/ngx_http_rewrite_module.o \
	objs/src/http/modules/ngx_http_ssl_module.o \
	objs/src/http/modules/ngx_http_proxy_module.o \
	objs/src/http/modules/ngx_http_fastcgi_module.o \
	objs/src/http/modules/ngx_http_uwsgi_module.o \
	objs/src/http/modules/ngx_http_scgi_module.o \
	objs/src/http/modules/ngx_http_memcached_module.o \
	objs/src/http/modules/ngx_http_empty_gif_module.o \
	objs/src/http/modules/ngx_http_browser_module.o \
	objs/src/http/modules/ngx_http_secure_link_module.o \
	objs/src/http/modules/ngx_http_upstream_hash_module.o \
	objs/src/http/modules/ngx_http_upstream_ip_hash_module.o \
	objs/src/http/modules/ngx_http_upstream_least_conn_module.o \
	objs/src/http/modules/ngx_http_upstream_keepalive_module.o \
	objs/src/http/modules/ngx_http_upstream_zone_module.o \
	objs/addon/mytest_config/mytest_config.o \
	objs/addon/my_test_module/ngx_http_mytest_module.o \
	objs/addon/mytest_subrequest/ngx_http_subrequest_mytest_module.o \
	objs/addon/mytest_upstream/mytest_upstream.o \
	objs/addon/ngx_http_myfilter_module/ngx_http_myfilter_module.o \
	objs/addon/sendfile_test/sendfile_test.o \
	objs/addon/src/ngx_http_echo_module.o \
	objs/addon/src/ngx_http_echo_util.o \
	objs/addon/src/ngx_http_echo_timer.o \
	objs/addon/src/ngx_http_echo_var.o \
	objs/addon/src/ngx_http_echo_handler.o \
	objs/addon/src/ngx_http_echo_filter.o \
	objs/addon/src/ngx_http_echo_sleep.o \
	objs/addon/src/ngx_http_echo_location.o \
	objs/addon/src/ngx_http_echo_echo.o \
	objs/addon/src/ngx_http_echo_request_info.o \
	objs/addon/src/ngx_http_echo_subrequest.o \
	objs/addon/src/ngx_http_echo_foreach.o \
	objs/addon/nginx-requestkey-module-master/ngx_http_accesskey_module.o \
	objs/addon/src/ngx_http_redis2_module.o \
	objs/addon/src/ngx_http_redis2_handler.o \
	objs/addon/src/ngx_http_redis2_reply.o \
	objs/addon/src/ngx_http_redis2_util.o \
	objs/addon/src/ngx_http_lua_script.o \
	objs/addon/src/ngx_http_lua_log.o \
	objs/addon/src/ngx_http_lua_subrequest.o \
	objs/addon/src/ngx_http_lua_ndk.o \
	objs/addon/src/ngx_http_lua_control.o \
	objs/addon/src/ngx_http_lua_time.o \
	objs/addon/src/ngx_http_lua_misc.o \
	objs/addon/src/ngx_http_lua_variable.o \
	objs/addon/src/ngx_http_lua_string.o \
	objs/addon/src/ngx_http_lua_output.o \
	objs/addon/src/ngx_http_lua_headers.o \
	objs/addon/src/ngx_http_lua_req_body.o \
	objs/addon/src/ngx_http_lua_uri.o \
	objs/addon/src/ngx_http_lua_args.o \
	objs/addon/src/ngx_http_lua_ctx.o \
	objs/addon/src/ngx_http_lua_regex.o \
	objs/addon/src/ngx_http_lua_module.o \
	objs/addon/src/ngx_http_lua_headers_out.o \
	objs/addon/src/ngx_http_lua_headers_in.o \
	objs/addon/src/ngx_http_lua_directive.o \
	objs/addon/src/ngx_http_lua_consts.o \
	objs/addon/src/ngx_http_lua_exception.o \
	objs/addon/src/ngx_http_lua_util.o \
	objs/addon/src/ngx_http_lua_cache.o \
	objs/addon/src/ngx_http_lua_contentby.o \
	objs/addon/src/ngx_http_lua_rewriteby.o \
	objs/addon/src/ngx_http_lua_accessby.o \
	objs/addon/src/ngx_http_lua_setby.o \
	objs/addon/src/ngx_http_lua_capturefilter.o \
	objs/addon/src/ngx_http_lua_clfactory.o \
	objs/addon/src/ngx_http_lua_pcrefix.o \
	objs/addon/src/ngx_http_lua_headerfilterby.o \
	objs/addon/src/ngx_http_lua_shdict.o \
	objs/addon/src/ngx_http_lua_socket_tcp.o \
	objs/addon/src/ngx_http_lua_api.o \
	objs/addon/src/ngx_http_lua_logby.o \
	objs/addon/src/ngx_http_lua_sleep.o \
	objs/addon/src/ngx_http_lua_semaphore.o \
	objs/addon/src/ngx_http_lua_coroutine.o \
	objs/addon/src/ngx_http_lua_bodyfilterby.o \
	objs/addon/src/ngx_http_lua_initby.o \
	objs/addon/src/ngx_http_lua_initworkerby.o \
	objs/addon/src/ngx_http_lua_socket_udp.o \
	objs/addon/src/ngx_http_lua_req_method.o \
	objs/addon/src/ngx_http_lua_phase.o \
	objs/addon/src/ngx_http_lua_uthread.o \
	objs/addon/src/ngx_http_lua_timer.o \
	objs/addon/src/ngx_http_lua_config.o \
	objs/addon/src/ngx_http_lua_worker.o \
	objs/addon/src/ngx_http_lua_ssl_certby.o \
	objs/addon/src/ngx_http_lua_ssl_ocsp.o \
	objs/addon/src/ngx_http_lua_lex.o \
	objs/addon/src/ngx_http_lua_balancer.o \
	objs/ngx_modules.o \
	-L/usr/local/lib -Wl,-E -lpthread -lpthread -lcrypt -L/usr/local/lib -lluajit-5.1 -lm -ldl -lpcre /usr/local/openssl/../openssl/lib/libssl.a /usr/local/openssl/../openssl/lib/libcrypto.a -ldl -lz
	


objs/ngx_modules.o:	$(CORE_DEPS) \
	objs/ngx_modules.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/ngx_modules.o \
		objs/ngx_modules.c


objs/src/core/nginx.o:	$(CORE_DEPS) \
	src/core/nginx.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/nginx.o \
		src/core/nginx.c


objs/src/core/ngx_log.o:	$(CORE_DEPS) \
	src/core/ngx_log.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_log.o \
		src/core/ngx_log.c


objs/src/core/ngx_palloc.o:	$(CORE_DEPS) \
	src/core/ngx_palloc.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_palloc.o \
		src/core/ngx_palloc.c


objs/src/core/ngx_array.o:	$(CORE_DEPS) \
	src/core/ngx_array.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_array.o \
		src/core/ngx_array.c


objs/src/core/ngx_list.o:	$(CORE_DEPS) \
	src/core/ngx_list.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_list.o \
		src/core/ngx_list.c


objs/src/core/ngx_hash.o:	$(CORE_DEPS) \
	src/core/ngx_hash.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_hash.o \
		src/core/ngx_hash.c


objs/src/core/ngx_buf.o:	$(CORE_DEPS) \
	src/core/ngx_buf.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_buf.o \
		src/core/ngx_buf.c


objs/src/core/ngx_queue.o:	$(CORE_DEPS) \
	src/core/ngx_queue.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_queue.o \
		src/core/ngx_queue.c


objs/src/core/ngx_output_chain.o:	$(CORE_DEPS) \
	src/core/ngx_output_chain.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_output_chain.o \
		src/core/ngx_output_chain.c


objs/src/core/ngx_string.o:	$(CORE_DEPS) \
	src/core/ngx_string.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_string.o \
		src/core/ngx_string.c


objs/src/core/ngx_parse.o:	$(CORE_DEPS) \
	src/core/ngx_parse.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_parse.o \
		src/core/ngx_parse.c


objs/src/core/ngx_parse_time.o:	$(CORE_DEPS) \
	src/core/ngx_parse_time.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_parse_time.o \
		src/core/ngx_parse_time.c


objs/src/core/ngx_inet.o:	$(CORE_DEPS) \
	src/core/ngx_inet.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_inet.o \
		src/core/ngx_inet.c


objs/src/core/ngx_file.o:	$(CORE_DEPS) \
	src/core/ngx_file.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_file.o \
		src/core/ngx_file.c


objs/src/core/ngx_crc32.o:	$(CORE_DEPS) \
	src/core/ngx_crc32.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_crc32.o \
		src/core/ngx_crc32.c


objs/src/core/ngx_murmurhash.o:	$(CORE_DEPS) \
	src/core/ngx_murmurhash.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_murmurhash.o \
		src/core/ngx_murmurhash.c


objs/src/core/ngx_md5.o:	$(CORE_DEPS) \
	src/core/ngx_md5.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_md5.o \
		src/core/ngx_md5.c


objs/src/core/ngx_rbtree.o:	$(CORE_DEPS) \
	src/core/ngx_rbtree.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_rbtree.o \
		src/core/ngx_rbtree.c


objs/src/core/ngx_radix_tree.o:	$(CORE_DEPS) \
	src/core/ngx_radix_tree.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_radix_tree.o \
		src/core/ngx_radix_tree.c


objs/src/core/ngx_slab.o:	$(CORE_DEPS) \
	src/core/ngx_slab.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_slab.o \
		src/core/ngx_slab.c


objs/src/core/ngx_times.o:	$(CORE_DEPS) \
	src/core/ngx_times.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_times.o \
		src/core/ngx_times.c


objs/src/core/ngx_shmtx.o:	$(CORE_DEPS) \
	src/core/ngx_shmtx.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_shmtx.o \
		src/core/ngx_shmtx.c


objs/src/core/ngx_connection.o:	$(CORE_DEPS) \
	src/core/ngx_connection.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_connection.o \
		src/core/ngx_connection.c


objs/src/core/ngx_cycle.o:	$(CORE_DEPS) \
	src/core/ngx_cycle.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_cycle.o \
		src/core/ngx_cycle.c


objs/src/core/ngx_spinlock.o:	$(CORE_DEPS) \
	src/core/ngx_spinlock.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_spinlock.o \
		src/core/ngx_spinlock.c


objs/src/core/ngx_rwlock.o:	$(CORE_DEPS) \
	src/core/ngx_rwlock.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_rwlock.o \
		src/core/ngx_rwlock.c


objs/src/core/ngx_cpuinfo.o:	$(CORE_DEPS) \
	src/core/ngx_cpuinfo.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_cpuinfo.o \
		src/core/ngx_cpuinfo.c


objs/src/core/ngx_conf_file.o:	$(CORE_DEPS) \
	src/core/ngx_conf_file.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_conf_file.o \
		src/core/ngx_conf_file.c


objs/src/core/ngx_resolver.o:	$(CORE_DEPS) \
	src/core/ngx_resolver.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_resolver.o \
		src/core/ngx_resolver.c


objs/src/core/ngx_open_file_cache.o:	$(CORE_DEPS) \
	src/core/ngx_open_file_cache.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_open_file_cache.o \
		src/core/ngx_open_file_cache.c


objs/src/core/ngx_crypt.o:	$(CORE_DEPS) \
	src/core/ngx_crypt.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_crypt.o \
		src/core/ngx_crypt.c


objs/src/core/ngx_proxy_protocol.o:	$(CORE_DEPS) \
	src/core/ngx_proxy_protocol.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_proxy_protocol.o \
		src/core/ngx_proxy_protocol.c


objs/src/core/ngx_syslog.o:	$(CORE_DEPS) \
	src/core/ngx_syslog.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_syslog.o \
		src/core/ngx_syslog.c


objs/src/event/ngx_event.o:	$(CORE_DEPS) \
	src/event/ngx_event.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/ngx_event.o \
		src/event/ngx_event.c


objs/src/event/ngx_event_timer.o:	$(CORE_DEPS) \
	src/event/ngx_event_timer.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/ngx_event_timer.o \
		src/event/ngx_event_timer.c


objs/src/event/ngx_event_posted.o:	$(CORE_DEPS) \
	src/event/ngx_event_posted.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/ngx_event_posted.o \
		src/event/ngx_event_posted.c


objs/src/event/ngx_event_accept.o:	$(CORE_DEPS) \
	src/event/ngx_event_accept.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/ngx_event_accept.o \
		src/event/ngx_event_accept.c


objs/src/event/ngx_event_connect.o:	$(CORE_DEPS) \
	src/event/ngx_event_connect.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/ngx_event_connect.o \
		src/event/ngx_event_connect.c


objs/src/event/ngx_event_pipe.o:	$(CORE_DEPS) \
	src/event/ngx_event_pipe.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/ngx_event_pipe.o \
		src/event/ngx_event_pipe.c


objs/src/os/unix/ngx_time.o:	$(CORE_DEPS) \
	src/os/unix/ngx_time.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_time.o \
		src/os/unix/ngx_time.c


objs/src/os/unix/ngx_errno.o:	$(CORE_DEPS) \
	src/os/unix/ngx_errno.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_errno.o \
		src/os/unix/ngx_errno.c


objs/src/os/unix/ngx_alloc.o:	$(CORE_DEPS) \
	src/os/unix/ngx_alloc.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_alloc.o \
		src/os/unix/ngx_alloc.c


objs/src/os/unix/ngx_files.o:	$(CORE_DEPS) \
	src/os/unix/ngx_files.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_files.o \
		src/os/unix/ngx_files.c


objs/src/os/unix/ngx_socket.o:	$(CORE_DEPS) \
	src/os/unix/ngx_socket.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_socket.o \
		src/os/unix/ngx_socket.c


objs/src/os/unix/ngx_recv.o:	$(CORE_DEPS) \
	src/os/unix/ngx_recv.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_recv.o \
		src/os/unix/ngx_recv.c


objs/src/os/unix/ngx_readv_chain.o:	$(CORE_DEPS) \
	src/os/unix/ngx_readv_chain.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_readv_chain.o \
		src/os/unix/ngx_readv_chain.c


objs/src/os/unix/ngx_udp_recv.o:	$(CORE_DEPS) \
	src/os/unix/ngx_udp_recv.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_udp_recv.o \
		src/os/unix/ngx_udp_recv.c


objs/src/os/unix/ngx_send.o:	$(CORE_DEPS) \
	src/os/unix/ngx_send.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_send.o \
		src/os/unix/ngx_send.c


objs/src/os/unix/ngx_writev_chain.o:	$(CORE_DEPS) \
	src/os/unix/ngx_writev_chain.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_writev_chain.o \
		src/os/unix/ngx_writev_chain.c


objs/src/os/unix/ngx_channel.o:	$(CORE_DEPS) \
	src/os/unix/ngx_channel.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_channel.o \
		src/os/unix/ngx_channel.c


objs/src/os/unix/ngx_shmem.o:	$(CORE_DEPS) \
	src/os/unix/ngx_shmem.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_shmem.o \
		src/os/unix/ngx_shmem.c


objs/src/os/unix/ngx_process.o:	$(CORE_DEPS) \
	src/os/unix/ngx_process.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_process.o \
		src/os/unix/ngx_process.c


objs/src/os/unix/ngx_daemon.o:	$(CORE_DEPS) \
	src/os/unix/ngx_daemon.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_daemon.o \
		src/os/unix/ngx_daemon.c


objs/src/os/unix/ngx_setaffinity.o:	$(CORE_DEPS) \
	src/os/unix/ngx_setaffinity.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_setaffinity.o \
		src/os/unix/ngx_setaffinity.c


objs/src/os/unix/ngx_setproctitle.o:	$(CORE_DEPS) \
	src/os/unix/ngx_setproctitle.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_setproctitle.o \
		src/os/unix/ngx_setproctitle.c


objs/src/os/unix/ngx_posix_init.o:	$(CORE_DEPS) \
	src/os/unix/ngx_posix_init.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_posix_init.o \
		src/os/unix/ngx_posix_init.c


objs/src/os/unix/ngx_user.o:	$(CORE_DEPS) \
	src/os/unix/ngx_user.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_user.o \
		src/os/unix/ngx_user.c


objs/src/os/unix/ngx_process_cycle.o:	$(CORE_DEPS) \
	src/os/unix/ngx_process_cycle.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_process_cycle.o \
		src/os/unix/ngx_process_cycle.c


objs/src/os/unix/ngx_linux_init.o:	$(CORE_DEPS) \
	src/os/unix/ngx_linux_init.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_linux_init.o \
		src/os/unix/ngx_linux_init.c


objs/src/event/modules/ngx_epoll_module.o:	$(CORE_DEPS) \
	src/event/modules/ngx_epoll_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/modules/ngx_epoll_module.o \
		src/event/modules/ngx_epoll_module.c


objs/src/os/unix/ngx_linux_sendfile_chain.o:	$(CORE_DEPS) \
	src/os/unix/ngx_linux_sendfile_chain.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_linux_sendfile_chain.o \
		src/os/unix/ngx_linux_sendfile_chain.c


objs/src/os/unix/ngx_linux_aio_read.o:	$(CORE_DEPS) \
	src/os/unix/ngx_linux_aio_read.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_linux_aio_read.o \
		src/os/unix/ngx_linux_aio_read.c


objs/src/core/ngx_thread_pool.o:	$(CORE_DEPS) \
	src/core/ngx_thread_pool.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_thread_pool.o \
		src/core/ngx_thread_pool.c


objs/src/os/unix/ngx_thread_cond.o:	$(CORE_DEPS) \
	src/os/unix/ngx_thread_cond.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_thread_cond.o \
		src/os/unix/ngx_thread_cond.c


objs/src/os/unix/ngx_thread_mutex.o:	$(CORE_DEPS) \
	src/os/unix/ngx_thread_mutex.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_thread_mutex.o \
		src/os/unix/ngx_thread_mutex.c


objs/src/os/unix/ngx_thread_id.o:	$(CORE_DEPS) \
	src/os/unix/ngx_thread_id.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/ngx_thread_id.o \
		src/os/unix/ngx_thread_id.c


objs/src/event/ngx_event_openssl.o:	$(CORE_DEPS) \
	src/event/ngx_event_openssl.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/ngx_event_openssl.o \
		src/event/ngx_event_openssl.c


objs/src/event/ngx_event_openssl_stapling.o:	$(CORE_DEPS) \
	src/event/ngx_event_openssl_stapling.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/ngx_event_openssl_stapling.o \
		src/event/ngx_event_openssl_stapling.c


objs/src/core/ngx_regex.o:	$(CORE_DEPS) \
	src/core/ngx_regex.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/ngx_regex.o \
		src/core/ngx_regex.c


objs/src/http/ngx_http.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http.o \
		src/http/ngx_http.c


objs/src/http/ngx_http_core_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_core_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_core_module.o \
		src/http/ngx_http_core_module.c


objs/src/http/ngx_http_special_response.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_special_response.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_special_response.o \
		src/http/ngx_http_special_response.c


objs/src/http/ngx_http_request.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_request.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_request.o \
		src/http/ngx_http_request.c


objs/src/http/ngx_http_parse.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_parse.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_parse.o \
		src/http/ngx_http_parse.c


objs/src/http/ngx_http_header_filter_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_header_filter_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_header_filter_module.o \
		src/http/ngx_http_header_filter_module.c


objs/src/http/ngx_http_write_filter_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_write_filter_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_write_filter_module.o \
		src/http/ngx_http_write_filter_module.c


objs/src/http/ngx_http_copy_filter_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_copy_filter_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_copy_filter_module.o \
		src/http/ngx_http_copy_filter_module.c


objs/src/http/modules/ngx_http_log_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_log_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_log_module.o \
		src/http/modules/ngx_http_log_module.c


objs/src/http/ngx_http_request_body.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_request_body.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_request_body.o \
		src/http/ngx_http_request_body.c


objs/src/http/ngx_http_variables.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_variables.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_variables.o \
		src/http/ngx_http_variables.c


objs/src/http/ngx_http_script.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_script.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_script.o \
		src/http/ngx_http_script.c


objs/src/http/ngx_http_upstream.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_upstream.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_upstream.o \
		src/http/ngx_http_upstream.c


objs/src/http/ngx_http_upstream_round_robin.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_upstream_round_robin.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_upstream_round_robin.o \
		src/http/ngx_http_upstream_round_robin.c


objs/src/http/modules/ngx_http_static_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_static_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_static_module.o \
		src/http/modules/ngx_http_static_module.c


objs/src/http/modules/ngx_http_index_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_index_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_index_module.o \
		src/http/modules/ngx_http_index_module.c


objs/src/http/modules/ngx_http_chunked_filter_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_chunked_filter_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_chunked_filter_module.o \
		src/http/modules/ngx_http_chunked_filter_module.c


objs/src/http/modules/ngx_http_range_filter_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_range_filter_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_range_filter_module.o \
		src/http/modules/ngx_http_range_filter_module.c


objs/src/http/modules/ngx_http_headers_filter_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_headers_filter_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_headers_filter_module.o \
		src/http/modules/ngx_http_headers_filter_module.c


objs/src/http/modules/ngx_http_not_modified_filter_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_not_modified_filter_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_not_modified_filter_module.o \
		src/http/modules/ngx_http_not_modified_filter_module.c


objs/src/http/ngx_http_file_cache.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_file_cache.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_file_cache.o \
		src/http/ngx_http_file_cache.c


objs/src/http/modules/ngx_http_gzip_filter_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_gzip_filter_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_gzip_filter_module.o \
		src/http/modules/ngx_http_gzip_filter_module.c


objs/src/http/ngx_http_postpone_filter_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/ngx_http_postpone_filter_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/ngx_http_postpone_filter_module.o \
		src/http/ngx_http_postpone_filter_module.c


objs/src/http/modules/ngx_http_ssi_filter_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_ssi_filter_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_ssi_filter_module.o \
		src/http/modules/ngx_http_ssi_filter_module.c


objs/src/http/modules/ngx_http_charset_filter_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_charset_filter_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_charset_filter_module.o \
		src/http/modules/ngx_http_charset_filter_module.c


objs/src/http/modules/ngx_http_userid_filter_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_userid_filter_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_userid_filter_module.o \
		src/http/modules/ngx_http_userid_filter_module.c


objs/src/http/v2/ngx_http_v2.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/v2/ngx_http_v2.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/v2/ngx_http_v2.o \
		src/http/v2/ngx_http_v2.c


objs/src/http/v2/ngx_http_v2_table.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/v2/ngx_http_v2_table.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/v2/ngx_http_v2_table.o \
		src/http/v2/ngx_http_v2_table.c


objs/src/http/v2/ngx_http_v2_huff_decode.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/v2/ngx_http_v2_huff_decode.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/v2/ngx_http_v2_huff_decode.o \
		src/http/v2/ngx_http_v2_huff_decode.c


objs/src/http/v2/ngx_http_v2_huff_encode.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/v2/ngx_http_v2_huff_encode.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/v2/ngx_http_v2_huff_encode.o \
		src/http/v2/ngx_http_v2_huff_encode.c


objs/src/http/v2/ngx_http_v2_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/v2/ngx_http_v2_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/v2/ngx_http_v2_module.o \
		src/http/v2/ngx_http_v2_module.c


objs/src/http/v2/ngx_http_v2_filter_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/v2/ngx_http_v2_filter_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/v2/ngx_http_v2_filter_module.o \
		src/http/v2/ngx_http_v2_filter_module.c


objs/src/http/modules/ngx_http_autoindex_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_autoindex_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_autoindex_module.o \
		src/http/modules/ngx_http_autoindex_module.c


objs/src/http/modules/ngx_http_auth_basic_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_auth_basic_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_auth_basic_module.o \
		src/http/modules/ngx_http_auth_basic_module.c


objs/src/http/modules/ngx_http_access_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_access_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_access_module.o \
		src/http/modules/ngx_http_access_module.c


objs/src/http/modules/ngx_http_limit_conn_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_limit_conn_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_limit_conn_module.o \
		src/http/modules/ngx_http_limit_conn_module.c


objs/src/http/modules/ngx_http_limit_req_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_limit_req_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_limit_req_module.o \
		src/http/modules/ngx_http_limit_req_module.c


objs/src/http/modules/ngx_http_geo_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_geo_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_geo_module.o \
		src/http/modules/ngx_http_geo_module.c


objs/src/http/modules/ngx_http_map_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_map_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_map_module.o \
		src/http/modules/ngx_http_map_module.c


objs/src/http/modules/ngx_http_split_clients_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_split_clients_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_split_clients_module.o \
		src/http/modules/ngx_http_split_clients_module.c


objs/src/http/modules/ngx_http_referer_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_referer_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_referer_module.o \
		src/http/modules/ngx_http_referer_module.c


objs/src/http/modules/ngx_http_rewrite_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_rewrite_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_rewrite_module.o \
		src/http/modules/ngx_http_rewrite_module.c


objs/src/http/modules/ngx_http_ssl_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_ssl_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_ssl_module.o \
		src/http/modules/ngx_http_ssl_module.c


objs/src/http/modules/ngx_http_proxy_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_proxy_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_proxy_module.o \
		src/http/modules/ngx_http_proxy_module.c


objs/src/http/modules/ngx_http_fastcgi_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_fastcgi_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_fastcgi_module.o \
		src/http/modules/ngx_http_fastcgi_module.c


objs/src/http/modules/ngx_http_uwsgi_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_uwsgi_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_uwsgi_module.o \
		src/http/modules/ngx_http_uwsgi_module.c


objs/src/http/modules/ngx_http_scgi_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_scgi_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_scgi_module.o \
		src/http/modules/ngx_http_scgi_module.c


objs/src/http/modules/ngx_http_memcached_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_memcached_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_memcached_module.o \
		src/http/modules/ngx_http_memcached_module.c


objs/src/http/modules/ngx_http_empty_gif_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_empty_gif_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_empty_gif_module.o \
		src/http/modules/ngx_http_empty_gif_module.c


objs/src/http/modules/ngx_http_browser_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_browser_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_browser_module.o \
		src/http/modules/ngx_http_browser_module.c


objs/src/http/modules/ngx_http_secure_link_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_secure_link_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_secure_link_module.o \
		src/http/modules/ngx_http_secure_link_module.c


objs/src/http/modules/ngx_http_upstream_hash_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_upstream_hash_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_upstream_hash_module.o \
		src/http/modules/ngx_http_upstream_hash_module.c


objs/src/http/modules/ngx_http_upstream_ip_hash_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_upstream_ip_hash_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_upstream_ip_hash_module.o \
		src/http/modules/ngx_http_upstream_ip_hash_module.c


objs/src/http/modules/ngx_http_upstream_least_conn_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_upstream_least_conn_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_upstream_least_conn_module.o \
		src/http/modules/ngx_http_upstream_least_conn_module.c


objs/src/http/modules/ngx_http_upstream_keepalive_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_upstream_keepalive_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_upstream_keepalive_module.o \
		src/http/modules/ngx_http_upstream_keepalive_module.c


objs/src/http/modules/ngx_http_upstream_zone_module.o:	$(CORE_DEPS) $(HTTP_DEPS) \
	src/http/modules/ngx_http_upstream_zone_module.c
	$(CC) -c -g $(CFLAGS) $(CORE_INCS) $(HTTP_INCS) \
		-o objs/src/http/modules/ngx_http_upstream_zone_module.o \
		src/http/modules/ngx_http_upstream_zone_module.c


objs/addon/mytest_config/mytest_config.o:	$(ADDON_DEPS) \
	./src/mytest_config/mytest_config.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/mytest_config/mytest_config.o \
		./src/mytest_config/mytest_config.c


objs/addon/my_test_module/ngx_http_mytest_module.o:	$(ADDON_DEPS) \
	./src/my_test_module/ngx_http_mytest_module.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/my_test_module/ngx_http_mytest_module.o \
		./src/my_test_module/ngx_http_mytest_module.c


objs/addon/mytest_subrequest/ngx_http_subrequest_mytest_module.o:	$(ADDON_DEPS) \
	./src/mytest_subrequest/ngx_http_subrequest_mytest_module.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/mytest_subrequest/ngx_http_subrequest_mytest_module.o \
		./src/mytest_subrequest/ngx_http_subrequest_mytest_module.c


objs/addon/mytest_upstream/mytest_upstream.o:	$(ADDON_DEPS) \
	./src/mytest_upstream/mytest_upstream.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/mytest_upstream/mytest_upstream.o \
		./src/mytest_upstream/mytest_upstream.c


objs/addon/ngx_http_myfilter_module/ngx_http_myfilter_module.o:	$(ADDON_DEPS) \
	./src/ngx_http_myfilter_module/ngx_http_myfilter_module.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/ngx_http_myfilter_module/ngx_http_myfilter_module.o \
		./src/ngx_http_myfilter_module/ngx_http_myfilter_module.c


objs/addon/sendfile_test/sendfile_test.o:	$(ADDON_DEPS) \
	./src/sendfile_test/sendfile_test.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/sendfile_test/sendfile_test.o \
		./src/sendfile_test/sendfile_test.c


objs/addon/src/ngx_http_echo_module.o:	$(ADDON_DEPS) \
	/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_module.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_echo_module.o \
		/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_module.c


objs/addon/src/ngx_http_echo_util.o:	$(ADDON_DEPS) \
	/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_util.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_echo_util.o \
		/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_util.c


objs/addon/src/ngx_http_echo_timer.o:	$(ADDON_DEPS) \
	/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_timer.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_echo_timer.o \
		/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_timer.c


objs/addon/src/ngx_http_echo_var.o:	$(ADDON_DEPS) \
	/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_var.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_echo_var.o \
		/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_var.c


objs/addon/src/ngx_http_echo_handler.o:	$(ADDON_DEPS) \
	/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_handler.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_echo_handler.o \
		/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_handler.c


objs/addon/src/ngx_http_echo_filter.o:	$(ADDON_DEPS) \
	/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_filter.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_echo_filter.o \
		/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_filter.c


objs/addon/src/ngx_http_echo_sleep.o:	$(ADDON_DEPS) \
	/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_sleep.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_echo_sleep.o \
		/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_sleep.c


objs/addon/src/ngx_http_echo_location.o:	$(ADDON_DEPS) \
	/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_location.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_echo_location.o \
		/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_location.c


objs/addon/src/ngx_http_echo_echo.o:	$(ADDON_DEPS) \
	/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_echo.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_echo_echo.o \
		/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_echo.c


objs/addon/src/ngx_http_echo_request_info.o:	$(ADDON_DEPS) \
	/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_request_info.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_echo_request_info.o \
		/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_request_info.c


objs/addon/src/ngx_http_echo_subrequest.o:	$(ADDON_DEPS) \
	/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_subrequest.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_echo_subrequest.o \
		/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_subrequest.c


objs/addon/src/ngx_http_echo_foreach.o:	$(ADDON_DEPS) \
	/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_foreach.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_echo_foreach.o \
		/root/yangyazhou/reading-code-of-nginx-1.9.2/nginx-1.9.2/src/echo-nginx-module-master//src/ngx_http_echo_foreach.c


objs/addon/nginx-requestkey-module-master/ngx_http_accesskey_module.o:	$(ADDON_DEPS) \
	./src/nginx-requestkey-module-master//ngx_http_accesskey_module.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/nginx-requestkey-module-master/ngx_http_accesskey_module.o \
		./src/nginx-requestkey-module-master//ngx_http_accesskey_module.c


objs/addon/src/ngx_http_redis2_module.o:	$(ADDON_DEPS) \
	./src/redis2-nginx-module-master//src/ngx_http_redis2_module.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_redis2_module.o \
		./src/redis2-nginx-module-master//src/ngx_http_redis2_module.c


objs/addon/src/ngx_http_redis2_handler.o:	$(ADDON_DEPS) \
	./src/redis2-nginx-module-master//src/ngx_http_redis2_handler.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_redis2_handler.o \
		./src/redis2-nginx-module-master//src/ngx_http_redis2_handler.c


objs/addon/src/ngx_http_redis2_reply.o:	$(ADDON_DEPS) \
	./src/redis2-nginx-module-master//src/ngx_http_redis2_reply.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_redis2_reply.o \
		./src/redis2-nginx-module-master//src/ngx_http_redis2_reply.c


objs/addon/src/ngx_http_redis2_util.o:	$(ADDON_DEPS) \
	./src/redis2-nginx-module-master//src/ngx_http_redis2_util.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_redis2_util.o \
		./src/redis2-nginx-module-master//src/ngx_http_redis2_util.c


objs/addon/src/ngx_http_lua_script.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_script.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_script.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_script.c


objs/addon/src/ngx_http_lua_log.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_log.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_log.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_log.c


objs/addon/src/ngx_http_lua_subrequest.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_subrequest.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_subrequest.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_subrequest.c


objs/addon/src/ngx_http_lua_ndk.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_ndk.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_ndk.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_ndk.c


objs/addon/src/ngx_http_lua_control.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_control.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_control.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_control.c


objs/addon/src/ngx_http_lua_time.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_time.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_time.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_time.c


objs/addon/src/ngx_http_lua_misc.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_misc.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_misc.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_misc.c


objs/addon/src/ngx_http_lua_variable.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_variable.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_variable.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_variable.c


objs/addon/src/ngx_http_lua_string.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_string.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_string.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_string.c


objs/addon/src/ngx_http_lua_output.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_output.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_output.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_output.c


objs/addon/src/ngx_http_lua_headers.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_headers.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_headers.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_headers.c


objs/addon/src/ngx_http_lua_req_body.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_req_body.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_req_body.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_req_body.c


objs/addon/src/ngx_http_lua_uri.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_uri.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_uri.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_uri.c


objs/addon/src/ngx_http_lua_args.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_args.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_args.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_args.c


objs/addon/src/ngx_http_lua_ctx.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_ctx.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_ctx.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_ctx.c


objs/addon/src/ngx_http_lua_regex.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_regex.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_regex.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_regex.c


objs/addon/src/ngx_http_lua_module.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_module.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_module.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_module.c


objs/addon/src/ngx_http_lua_headers_out.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_headers_out.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_headers_out.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_headers_out.c


objs/addon/src/ngx_http_lua_headers_in.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_headers_in.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_headers_in.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_headers_in.c


objs/addon/src/ngx_http_lua_directive.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_directive.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_directive.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_directive.c


objs/addon/src/ngx_http_lua_consts.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_consts.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_consts.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_consts.c


objs/addon/src/ngx_http_lua_exception.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_exception.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_exception.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_exception.c


objs/addon/src/ngx_http_lua_util.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_util.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_util.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_util.c


objs/addon/src/ngx_http_lua_cache.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_cache.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_cache.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_cache.c


objs/addon/src/ngx_http_lua_contentby.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_contentby.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_contentby.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_contentby.c


objs/addon/src/ngx_http_lua_rewriteby.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_rewriteby.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_rewriteby.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_rewriteby.c


objs/addon/src/ngx_http_lua_accessby.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_accessby.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_accessby.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_accessby.c


objs/addon/src/ngx_http_lua_setby.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_setby.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_setby.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_setby.c


objs/addon/src/ngx_http_lua_capturefilter.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_capturefilter.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_capturefilter.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_capturefilter.c


objs/addon/src/ngx_http_lua_clfactory.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_clfactory.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_clfactory.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_clfactory.c


objs/addon/src/ngx_http_lua_pcrefix.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_pcrefix.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_pcrefix.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_pcrefix.c


objs/addon/src/ngx_http_lua_headerfilterby.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_headerfilterby.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_headerfilterby.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_headerfilterby.c


objs/addon/src/ngx_http_lua_shdict.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_shdict.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_shdict.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_shdict.c


objs/addon/src/ngx_http_lua_socket_tcp.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_socket_tcp.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_socket_tcp.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_socket_tcp.c


objs/addon/src/ngx_http_lua_api.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_api.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_api.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_api.c


objs/addon/src/ngx_http_lua_logby.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_logby.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_logby.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_logby.c


objs/addon/src/ngx_http_lua_sleep.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_sleep.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_sleep.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_sleep.c


objs/addon/src/ngx_http_lua_semaphore.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_semaphore.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_semaphore.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_semaphore.c


objs/addon/src/ngx_http_lua_coroutine.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_coroutine.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_coroutine.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_coroutine.c


objs/addon/src/ngx_http_lua_bodyfilterby.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_bodyfilterby.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_bodyfilterby.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_bodyfilterby.c


objs/addon/src/ngx_http_lua_initby.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_initby.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_initby.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_initby.c


objs/addon/src/ngx_http_lua_initworkerby.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_initworkerby.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_initworkerby.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_initworkerby.c


objs/addon/src/ngx_http_lua_socket_udp.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_socket_udp.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_socket_udp.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_socket_udp.c


objs/addon/src/ngx_http_lua_req_method.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_req_method.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_req_method.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_req_method.c


objs/addon/src/ngx_http_lua_phase.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_phase.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_phase.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_phase.c


objs/addon/src/ngx_http_lua_uthread.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_uthread.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_uthread.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_uthread.c


objs/addon/src/ngx_http_lua_timer.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_timer.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_timer.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_timer.c


objs/addon/src/ngx_http_lua_config.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_config.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_config.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_config.c


objs/addon/src/ngx_http_lua_worker.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_worker.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_worker.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_worker.c


objs/addon/src/ngx_http_lua_ssl_certby.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_ssl_certby.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_ssl_certby.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_ssl_certby.c


objs/addon/src/ngx_http_lua_ssl_ocsp.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_ssl_ocsp.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_ssl_ocsp.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_ssl_ocsp.c


objs/addon/src/ngx_http_lua_lex.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_lex.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_lex.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_lex.c


objs/addon/src/ngx_http_lua_balancer.o:	$(ADDON_DEPS) \
	./src/lua-nginx-module-master//src/ngx_http_lua_balancer.c
	$(CC) -c -g $(CFLAGS)  $(ALL_INCS) \
		-o objs/addon/src/ngx_http_lua_balancer.o \
		./src/lua-nginx-module-master//src/ngx_http_lua_balancer.c


/usr/local/openssl/.openssl/include/openssl/ssl.h:	objs/Makefile
	cd /usr/local/openssl \
	&& if [ -f Makefile ]; then $(MAKE) clean; fi \
	&& ./config --prefix=/usr/local/openssl/.openssl no-shared  \
	&& $(MAKE) \
	&& $(MAKE) install LIBDIR=lib


manpage:	objs/nginx.8

objs/nginx.8:	man/nginx.8 objs/ngx_auto_config.h
	sed -e "s|%%PREFIX%%|/usr/local/nginx|" \
		-e "s|%%PID_PATH%%|/usr/local/nginx/logs/nginx.pid|" \
		-e "s|%%CONF_PATH%%|/usr/local/nginx/conf/nginx.conf|" \
		-e "s|%%ERROR_LOG_PATH%%|/usr/local/nginx/logs/error.log|" \
		< man/nginx.8 > $@

install:	objs/nginx 		
	test -d '$(DESTDIR)/usr/local/nginx' || mkdir -p '$(DESTDIR)/usr/local/nginx'

	test -d '$(DESTDIR)/usr/local/nginx/sbin' 		|| mkdir -p '$(DESTDIR)/usr/local/nginx/sbin'
	test ! -f '$(DESTDIR)/usr/local/nginx/sbin/nginx' 		|| mv '$(DESTDIR)/usr/local/nginx/sbin/nginx' 			'$(DESTDIR)/usr/local/nginx/sbin/nginx.old'
	cp objs/nginx '$(DESTDIR)/usr/local/nginx/sbin/nginx'

	test -d '$(DESTDIR)/usr/local/nginx/conf' 		|| mkdir -p '$(DESTDIR)/usr/local/nginx/conf'

	cp conf/koi-win '$(DESTDIR)/usr/local/nginx/conf'
	cp conf/koi-utf '$(DESTDIR)/usr/local/nginx/conf'
	cp conf/win-utf '$(DESTDIR)/usr/local/nginx/conf'

	test -f '$(DESTDIR)/usr/local/nginx/conf/mime.types' 		|| cp conf/mime.types '$(DESTDIR)/usr/local/nginx/conf'
	cp conf/mime.types '$(DESTDIR)/usr/local/nginx/conf/mime.types.default'

	test -f '$(DESTDIR)/usr/local/nginx/conf/fastcgi_params' 		|| cp conf/fastcgi_params '$(DESTDIR)/usr/local/nginx/conf'
	cp conf/fastcgi_params 		'$(DESTDIR)/usr/local/nginx/conf/fastcgi_params.default'

	test -f '$(DESTDIR)/usr/local/nginx/conf/fastcgi.conf' 		|| cp conf/fastcgi.conf '$(DESTDIR)/usr/local/nginx/conf'
	cp conf/fastcgi.conf '$(DESTDIR)/usr/local/nginx/conf/fastcgi.conf.default'

	test -f '$(DESTDIR)/usr/local/nginx/conf/uwsgi_params' 		|| cp conf/uwsgi_params '$(DESTDIR)/usr/local/nginx/conf'
	cp conf/uwsgi_params 		'$(DESTDIR)/usr/local/nginx/conf/uwsgi_params.default'

	test -f '$(DESTDIR)/usr/local/nginx/conf/scgi_params' 		|| cp conf/scgi_params '$(DESTDIR)/usr/local/nginx/conf'
	cp conf/scgi_params 		'$(DESTDIR)/usr/local/nginx/conf/scgi_params.default'

	test -f '$(DESTDIR)/usr/local/nginx/conf/nginx.conf' 		|| cp conf/nginx.conf '$(DESTDIR)/usr/local/nginx/conf/nginx.conf'
	cp conf/nginx.conf '$(DESTDIR)/usr/local/nginx/conf/nginx.conf.default'

	test -d '$(DESTDIR)/usr/local/nginx/logs' 		|| mkdir -p '$(DESTDIR)/usr/local/nginx/logs'

	test -d '$(DESTDIR)/usr/local/nginx/logs' || 		mkdir -p '$(DESTDIR)/usr/local/nginx/logs'

	test -d '$(DESTDIR)/usr/local/nginx/html' 		|| cp -R html '$(DESTDIR)/usr/local/nginx'

	test -d '$(DESTDIR)/usr/local/nginx/logs' || 		mkdir -p '$(DESTDIR)/usr/local/nginx/logs'
