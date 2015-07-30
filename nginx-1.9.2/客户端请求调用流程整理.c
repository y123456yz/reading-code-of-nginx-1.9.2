/*
|   \-- ngx_single_process_cycle
##########################################################################
##########################################################################
|   |   \-- ngx_process_events_and_timers
|   |   |   \-- ngx_event_find_timer
|   |   |   \-- ngx_epoll_process_events  //ngx_process_events
                    epoll_wait() //block here //
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
!---> Event: client send first TCP connect request
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
|   |   |   |   \-- ngx_time_update
|   |   |   |   \-- ngx_event_accept  // accept a connection, add the read event and ngx_http_init_request handler to epoll
                        accept()  //noblock read, get the new client socket
|   |   |   |   |   \-- ngx_get_connection
|   |   |   |   |   \-- ngx_create_pool //create connection pool
|   |   |   |   |   \-- ngx_nonblocking // nonelock client socket  
|   |   |   |   |   \-- ngx_sock_ntop
|   |   |   |   |   \-- ngx_http_init_connection  //ls->handler, set the next read event handler to ngx_http_init_request
|   |   |   |   |   |   \-- ngx_event_add_timer
|   |   |   |   |   |   \-- ngx_handle_read_event  // add the next read event to epoll
|   |   |   |   |   |   |   \-- ngx_epoll_add_event
|   |   |   \-- ngx_event_expire_timers
|   |   |   |   \-- ngx_rbtree_min
##########################################################################
##########################################################################
|   |   \-- ngx_process_events_and_timers
|   |   |   \-- ngx_event_find_timer
|   |   |   \-- ngx_epoll_process_events  //ngx_process_events
                    epoll_wait() //block here //
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
!---> Event: client send first HTTP request
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
|   |   |   |   \-- ngx_time_update
|   |   |   |   \-- ngx_http_init_request  //call ngx_http_process_request_line, till send out the request
|   |   |   |   |   \-- ngx_http_process_request_line
|   |   |   |   |   |   \-- ngx_http_read_request_header
|   |   |   |   |   |   \-- ngx_http_parse_request_line //parse method, URL, version
|   |   |   |   |   |   \-- ngx_http_process_request_headers
|   |   |   |   |   |   |   \-- ngx_http_read_request_header // read all headers
|   |   |   |   |   |   |   \-- ngx_http_parse_header_line // header 1
|   |   |   |   |   |   |   \-- ngx_list_push
|   |   |   |   |   |   |   \-- ngx_pnalloc
|   |   |   |   |   |   |   \-- ngx_hash_find
|   |   |   |   |   |   |   \-- ngx_http_process_user_agent //header 1 handler
|   |   |   |   |   |   |   \-- ngx_http_parse_header_line  // header 2
|   |   |   |   |   |   |   \-- ngx_list_push
|   |   |   |   |   |   |   \-- ngx_pnalloc
|   |   |   |   |   |   |   \-- ngx_hash_find
|   |   |   |   |   |   |   \-- ngx_http_process_host  //header 2 handler
|   |   |   |   |   |   |   \-- ngx_http_parse_header_line  //header 3
|   |   |   |   |   |   |   \-- ngx_list_push
|   |   |   |   |   |   |   \-- ngx_pnalloc
|   |   |   |   |   |   |   \-- ngx_hash_find
|   |   |   |   |   |   |   \-- ngx_http_parse_header_line  // no more header
|   |   |   |   |   |   |   \-- ngx_http_process_request_header
|   |   |   |   |   |   |   |   \-- ngx_http_find_virtual_server
|   |   |   |   |   |   |   \-- ngx_http_process_request
|   |   |   |   |   |   |   |   \-- ngx_event_del_timer
|   |   |   |   |   |   |   |   \-- ngx_http_handler
|   |   |   |   |   |   |   |   |   \-- ngx_http_core_run_phases
|   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_rewrite_phase  //phase checker
|   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_rewrite_handler //phase handler 
|   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_find_config_phase
|   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_find_location
|   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_update_location_config
|   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_rewrite_phase
|   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_rewrite_handler
|   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_post_rewrite_phase
|   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_generic_phase
|   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_limit_req_handler
|   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_generic_phase
|   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_limit_conn_handler
|   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_access_phase
|   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_access_handler
|   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_access_phase
|   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_auth_basic_handler
|   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_post_access_phase
|   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_content_phase
|   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_index_handler
|   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_map_uri_to_path
|   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_set_disable_symlinks
|   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_open_cached_file
|   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_file_info_wrapper
|   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_internal_redirect
|   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_set_exten
|   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_update_location_config
|   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_handler
|   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_run_phases
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_rewrite_phase
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_rewrite_handler
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_find_config_phase
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_find_location
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_update_location_config
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_rewrite_phase
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_rewrite_handler
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_post_rewrite_phase
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_generic_phase
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_limit_req_handler
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_generic_phase
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_limit_conn_handler
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_access_phase
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_access_handler
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_access_phase
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_auth_basic_handler
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_post_access_phase
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_content_phase
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_index_handler
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_content_phase
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_autoindex_handler
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_core_content_phase
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_static_handler
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_map_uri_to_path
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_set_disable_symlinks
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_open_cached_file
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_discard_request_body
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_set_content_type
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_send_header
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_not_modified_header_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_headers_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_userid_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_charset_header_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_destination_charset
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_ssi_header_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_gzip_header_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_range_header_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_list_push
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_chunked_header_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_header_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_create_temp_buf
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_sprintf
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_time
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_write_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_alloc_chain_link
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_output_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_range_body_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_copy_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_output_chain
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_output_chain_as_is
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_charset_body_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_ssi_body_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_postpone_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_gzip_body_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_chunked_body_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_write_filter
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_alloc_chain_link
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_linux_sendfile_chain
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_array_push
																													sendfile() //actually sent out
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_finalize_request
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_post_action
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_finalize_connection
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_close_request
|   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_finalize_request
|   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_finalize_connection
|   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_set_keepalive
|   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_free_request
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_http_log_request
|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_destroy_pool
|   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_event_add_timer  //leeplive timeout
|   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_handle_read_event //add read event
|   |   |   |   |   |   |   |   |   |   |   |   |   |   \-- ngx_reusable_connection
|   |   |   |   |   |   |   |   \-- ngx_http_run_posted_requests
|   |   |   \-- ngx_event_expire_timers
|   |   |   \-- ngx_event_process_posted
|   |   |   |   \-- ngx_http_keepalive_handler
|   |   |   |   |   \-- ngx_palloc
|   |   |   |   |   |   \-- ngx_palloc_large
|   |   |   |   |   |   |   \-- ngx_alloc
|   |   |   |   |   \-- ngx_unix_recv
|   |   |   |   |   \-- ngx_handle_read_event
|   |   |   |   |   \-- ngx_pfree
##########################################################################
##########################################################################
|   |   \-- ngx_process_events_and_timers
|   |   |   \-- ngx_event_find_timer
|   |   |   \-- ngx_epoll_process_events  //ngx_process_events
                    epoll_wait() //block here //
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
!---> Event: timeout for the connection
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
|   |   |   |   \-- ngx_time_update
|   |   |   |   \-- ngx_http_keepalive_handler
|   |   |   |   |   \-- ngx_unix_recv
|   |   |   |   |   \-- ngx_http_close_connection
|   |   |   |   |   |   \-- ngx_close_connection
|   |   |   |   |   |   \-- ngx_destroy_pool
|   |   |   \-- ngx_event_expire_timers
##########################################################################
##########################################################################
|   |   \-- ngx_process_events_and_timers
|   |   |   \-- ngx_event_find_timer
|   |   |   \-- ngx_epoll_process_events  //ngx_process_events
                    epoll_wait() //block here //
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
!---> Event: Client send a new TCP connect request
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
|   |   |   |   \-- ngx_time_update
|   |   |   |   \-- ngx_event_accept
|   |   |   |   |   \-- ngx_get_connection
|   |   |   |   |   \-- ngx_create_pool
|   |   |   |   |   |   \-- ngx_memalign
|   |   |   |   |   \-- ngx_palloc
|   |   |   |   |   \-- ngx_palloc
|   |   |   |   |   \-- ngx_nonblocking
|   |   |   |   |   \-- ngx_atomic_fetch_add
|   |   |   |   |   \-- ngx_pnalloc
|   |   |   |   |   \-- ngx_sock_ntop
|   |   |   |   |   \-- ngx_http_init_connection
|   |   |   |   |   |   \-- ngx_palloc
|   |   |   |   |   |   \-- ngx_event_add_timer
|   |   |   |   |   |   \-- ngx_handle_read_event
|   |   |   |   |   |   |   \-- ngx_epoll_add_event
|   |   |   \-- ngx_event_expire_timers
|   |   |   |   \-- ngx_rbtree_min
##########################################################################
##########################################################################
|   |   \-- ngx_process_events_and_timers
|   |   |   \-- ngx_event_find_timer
|   |   |   |   \-- ngx_rbtree_min
|   |   |   \-- ngx_epoll_process_events  //ngx_process_events
                    epoll_wait() //block here //
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
!---> Event: Client send a new HTTP request
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
|   |   |   |   \-- ngx_time_update
|   |   |   |   \-- ngx_http_init_request
|   |   |   |   |   \-- ngx_http_process_request_line
...

*/

