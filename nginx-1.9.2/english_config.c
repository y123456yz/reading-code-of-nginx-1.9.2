/*

Example Configuration

user www www;
worker_processes 2;

error_log /var/log/nginx-error.log info;

events {
    use kqueue;
    worker_connections 2048;
}

...

Directives
Syntax:  accept_mutex on | off;
 
Default:  accept_mutex on; 
Context:  events
 

If accept_mutex is enabled, worker processes will accept new connections by turn. Otherwise, all worker processes will be notified about new connections, and if volume of new connections is low, some of the worker processes may just waste system resources. 

Syntax:  accept_mutex_delay time;
 
Default:  accept_mutex_delay 500ms; 
Context:  events
 

If accept_mutex is enabled, specifies the maximum time during which a worker process will try to restart accepting new connections if another worker process is currently accepting new connections. 

Syntax:  daemon on | off;
 
Default:  daemon on; 
Context:  main
 

Determines whether nginx should become a daemon. Mainly used during development. 

Syntax:  debug_connection address | CIDR | unix:;
 
Default:  ¡ª  
Context:  events
 

Enables debugging log for selected client connections. Other connections will use logging level set by the error_log directive. Debugged connections are specified by IPv4 or IPv6 (1.3.0, 1.2.1) address or network. A connection may also be specified using a hostname. For connections using UNIX-domain sockets (1.3.0, 1.2.1), debugging log is enabled by the ¡°unix:¡± parameter. 

events {
    debug_connection 127.0.0.1;
    debug_connection localhost;
    debug_connection 192.0.2.0/24;
    debug_connection ::1;
    debug_connection 2001:0db8::/32;
    debug_connection unix:;
    ...
}

For this directive to work, nginx needs to be built with --with-debug, see ¡°A debugging log¡±. 

Syntax:  debug_points abort | stop;
 
Default:  ¡ª  
Context:  main
 

This directive is used for debugging. 

When internal error is detected, e.g. the leak of sockets on restart of working processes, enabling debug_points leads to a core file creation (abort) or to stopping of a process (stop) for further analysis using a system debugger. 

Syntax:  error_log file | stderr | syslog:server=address[,parameter=value] | memory:size [debug | info | notice | warn | error | crit | alert | emerg];
 
Default:  error_log logs/error.log error; 
Context:  main, http, mail, stream, server, location
 

Configures logging. Several logs can be specified on the same level (1.5.2). 

The first parameter defines a file that will store the log. The special value stderr selects the standard error file. Logging to syslog can be configured by specifying the ¡°syslog:¡± prefix. Logging to a cyclic memory buffer can be configured by specifying the ¡°memory:¡± prefix and buffer size, and is generally used for debugging (1.7.11). 

The second parameter determines the level of logging. Log levels above are listed in the order of increasing severity. Setting a certain log level will cause all messages of the specified and more severe log levels to be logged. For example, the default level error will cause error, crit, alert, and emerg messages to be logged. If this parameter is omitted then error is used. 

For debug logging to work, nginx needs to be built with --with-debug, see ¡°A debugging log¡±. 

The directive can be specified on the stream level starting from version 1.7.11. 

The directive can be specified on the mail level starting from version 1.9.0. 

Syntax:  env variable[=value];
 
Default:  env TZ; 
Context:  main
 

By default, nginx removes all environment variables inherited from its parent process except the TZ variable. This directive allows preserving some of the inherited variables, changing their values, or creating new environment variables. These variables are then: 

?inherited during a live upgrade of an executable file; 
?used by the ngx_http_perl_module module; 
?used by worker processes. One should bear in mind that controlling system libraries in this way is not always possible as it is common for libraries to check variables only during initialization, well before they can be set using this directive. An exception from this is an above mentioned live upgrade of an executable file. 

The TZ variable is always inherited and available to the ngx_http_perl_module module, unless it is configured explicitly. 

Usage example: 

env MALLOC_OPTIONS;
env PERL5LIB=/data/site/modules;
env OPENSSL_ALLOW_PROXY_CERTS=1;


The NGINX environment variable is used internally by nginx and should not be set directly by the user. 

Syntax:  events { ... }
 
Default:  ¡ª  
Context:  main
 

Provides the configuration file context in which the directives that affect connection processing are specified. 

Syntax:  include file | mask;
 
Default:  ¡ª  
Context:  any
 

Includes another file, or files matching the specified mask, into configuration. Included files should consist of syntactically correct directives and blocks. 

Usage example: 

include mime.types;
include vhosts/*.conf;

Syntax:  lock_file file;
 
Default:  lock_file logs/nginx.lock; 
Context:  main
 

nginx uses the locking mechanism to implement accept_mutex and serialize access to shared memory. On most systems the locks are implemented using atomic operations, and this directive is ignored. On other systems the ¡°lock file¡± mechanism is used. This directive specifies a prefix for the names of lock files. 

Syntax:  master_process on | off;
 
Default:  master_process on; 
Context:  main
 

Determines whether worker processes are started. This directive is intended for nginx developers. 

Syntax:  multi_accept on | off;
 
Default:  multi_accept off; 
Context:  events
 

If multi_accept is disabled, a worker process will accept one new connection at a time. Otherwise, a worker process will accept all new connections at a time. 

The directive is ignored if kqueue connection processing method is used, because it reports the number of new connections waiting to be accepted. 

Syntax:  pcre_jit on | off;
 
Default:  pcre_jit off; 
Context:  main
 

This directive appeared in version 1.1.12. 

Enables or disables the use of ¡°just-in-time compilation¡± (PCRE JIT) for the regular expressions known by the time of configuration parsing. 

PCRE JIT can speed up processing of regular expressions significantly. 

The JIT is available in PCRE libraries starting from version 8.20 built with the --enable-jit configuration parameter. When the PCRE library is built with nginx (--with-pcre=), the JIT support is enabled via the --with-pcre-jit configuration parameter. 

Syntax:  pid file;
 
Default:  pid nginx.pid; 
Context:  main
 

Defines a file that will store the process ID of the main process. 

Syntax:  ssl_engine device;
 
Default:  ¡ª  
Context:  main
 

Defines the name of the hardware SSL accelerator. 

Syntax:  thread_pool name threads=number [max_queue=number];
 
Default:  thread_pool default threads=32 max_queue=65536; 
Context:  main
 

This directive appeared in version 1.7.11. 

Defines named thread pools used for multi-threaded reading and sending of files without blocking worker processes. 

The threads parameter defines the number of threads in the pool. 

In the event that all threads in the pool are busy, a new task will wait in the queue. The max_queue parameter limits the number of tasks allowed to be waiting in the queue. By default, up to 65536 tasks can wait in the queue. When the queue overflows, the task is completed with an error. 

Syntax:  timer_resolution interval;
 
Default:  ¡ª  
Context:  main
 

Reduces timer resolution in worker processes, thus reducing the number of gettimeofday() system calls made. By default, gettimeofday() is called each time a kernel event is received. With reduced resolution, gettimeofday() is only called once per specified interval. 

Example: 

timer_resolution 100ms;

Internal implementation of the interval depends on the method used: 

?the EVFILT_TIMER filter if kqueue is used; 
?timer_create() if eventport is used; 
?setitimer() otherwise. 

Syntax:  use method;
 
Default:  ¡ª  
Context:  events
 

Specifies the connection processing method to use. There is normally no need to specify it explicitly, because nginx will by default use the most efficient method. 

Syntax:  user user [group];
 
Default:  user nobody nobody; 
Context:  main
 

Defines user and group credentials used by worker processes. If group is omitted, a group whose name equals that of user is used. 

Syntax:  worker_aio_requests number;
 
Default:  worker_aio_requests 32; 
Context:  events
 

This directive appeared in versions 1.1.4 and 1.0.7. 

When using aio with the epoll connection processing method, sets the maximum number of outstanding asynchronous I/O operations for a single worker process. 

Syntax:  worker_connections number;
 
Default:  worker_connections 512; 
Context:  events
 

Sets the maximum number of simultaneous connections that can be opened by a worker process. 

It should be kept in mind that this number includes all connections (e.g. connections with proxied servers, among others), not only connections with clients. Another consideration is that the actual number of simultaneous connections cannot exceed the current limit on the maximum number of open files, which can be changed by worker_rlimit_nofile. 

Syntax:  worker_cpu_affinity cpumask ...;
 
Default:  ¡ª  
Context:  main
 

Binds worker processes to the sets of CPUs. Each CPU set is represented by a bitmask of allowed CPUs. There should be a separate set defined for each of the worker processes. By default, worker processes are not bound to any specific CPUs. 

For example, 

worker_processes    4;
worker_cpu_affinity 0001 0010 0100 1000;
binds each worker process to a separate CPU, while 

worker_processes    2;
worker_cpu_affinity 0101 1010;
binds the first worker process to CPU0/CPU2, and the second worker process to CPU1/CPU3. The second example is suitable for hyper-threading. 


The directive is only available on FreeBSD and Linux. 

Syntax:  worker_priority number;
 
Default:  worker_priority 0; 
Context:  main
 

Defines the scheduling priority for worker processes like it is done by the nice command: a negative number means higher priority. Allowed range normally varies from -20 to 20. 

Example: 

worker_priority -10;

Syntax:  worker_processes number | auto;
 
Default:  worker_processes 1; 
Context:  main
 

Defines the number of worker processes. 

The optimal value depends on many factors including (but not limited to) the number of CPU cores, the number of hard disk drives that store data, and load pattern. When one is in doubt, setting it to the number of available CPU cores would be a good start (the value ¡°auto¡± will try to autodetect it). 

The auto parameter is supported starting from versions 1.3.8 and 1.2.5. 

Syntax:  worker_rlimit_core size;
 
Default:  ¡ª  
Context:  main
 

Changes the limit on the largest size of a core file (RLIMIT_CORE) for worker processes. Used to increase the limit without restarting the main process. 

Syntax:  worker_rlimit_nofile number;
 
Default:  ¡ª  
Context:  main
 

Changes the limit on the maximum number of open files (RLIMIT_NOFILE) for worker processes. Used to increase the limit without restarting the main process. 

Syntax:  working_directory directory;
 
Default:  ¡ª  
Context:  main
 

Defines the current working directory for a worker process. It is primarily used when writing a core-file, in which case a worker process should have write permission for the specified directory. 




Module ngx_http_rewrite_module
Directives
     break
     if
     return
     rewrite
     rewrite_log
     set
     uninitialized_variable_warn
Internal Implementation
 
The ngx_http_rewrite_module module is used to change request URI using regular expressions, return redirects, and conditionally select configurations. 

The ngx_http_rewrite_module module directives are processed in the following order: 

?the directives of this module specified on the server level are executed sequentially; 
?repeatedly: 
?a location is searched based on a request URI; 
?the directives of this module specified inside the found location are executed sequentially; 
?the loop is repeated if a request URI was rewritten, but not more than 10 times. 

Directives
Syntax:  break;
 
Default:  ¡ª  
Context:  server, location, if
 

Stops processing the current set of ngx_http_rewrite_module directives. 

If a directive is specified inside the location, further processing of the request continues in this location. 

Example: 

if ($slow) {
    limit_rate 10k;
    break;
}

Syntax:  if (condition) { ... }
 
Default:  ¡ª  
Context:  server, location
 

The specified condition is evaluated. If true, this module directives specified inside the braces are executed, and the request is assigned the configuration inside the if directive. Configurations inside the if directives are inherited from the previous configuration level. 

A condition may be any of the following: 

?a variable name; false if the value of a variable is an empty string or ¡°0¡±; 
Before version 1.0.1, any string starting with ¡°0¡± was considered a false value. 
?comparison of a variable with a string using the ¡°=¡± and ¡°!=¡± operators; 
?matching of a variable against a regular expression using the ¡°~¡± (for case-sensitive matching) and ¡°~*¡± (for case-insensitive matching) operators. Regular expressions can contain captures that are made available for later reuse in the $1..$9 variables. Negative operators ¡°!~¡± and ¡°!~*¡± are also available. If a regular expression includes the ¡°}¡± or ¡°;¡± characters, the whole expressions should be enclosed in single or double quotes. 
?checking of a file existence with the ¡°-f¡± and ¡°!-f¡± operators; 
?checking of a directory existence with the ¡°-d¡± and ¡°!-d¡± operators; 
?checking of a file, directory, or symbolic link existence with the ¡°-e¡± and ¡°!-e¡± operators; 
?checking for an executable file with the ¡°-x¡± and ¡°!-x¡± operators. 

Examples: 

if ($http_user_agent ~ MSIE) {
    rewrite ^(.*)$ /msie/$1 break;
}

if ($http_cookie ~* "id=([^;]+)(?:;|$)") {
    set $id $1;
}

if ($request_method = POST) {
    return 405;
}

if ($slow) {
    limit_rate 10k;
}

if ($invalid_referer) {
    return 403;
}

A value of the $invalid_referer embedded variable is set by the valid_referers directive. 

Syntax:  return code [text];
return code URL;
return URL;
 
Default:  ¡ª  
Context:  server, location, if
 

Stops processing and returns the specified code to a client. The non-standard code 444 closes a connection without sending a response header. 

Starting from version 0.8.42, it is possible to specify either a redirect URL (for codes 301, 302, 303, and 307), or the response body text (for other codes). A response body text and redirect URL can contain variables. As a special case, a redirect URL can be specified as a URI local to this server, in which case the full redirect URL is formed according to the request scheme ($scheme) and the server_name_in_redirect and port_in_redirect directives. 

In addition, a URL for temporary redirect with the code 302 can be specified as the sole parameter. Such a parameter should start with the ¡°http://¡±, ¡°https://¡±, or ¡°$scheme¡± string. A URL can contain variables. 


Only the following codes could be returned before version 0.7.51: 204, 400, 402 ¡ª 406, 408, 410, 411, 413, 416, and 500 ¡ª 504. 

The code 307 was not treated as a redirect until versions 1.1.16 and 1.0.13. 

See also the error_page directive. 

Syntax:  rewrite regex replacement [flag];
 
Default:  ¡ª  
Context:  server, location, if
 

If the specified regular expression matches a request URI, URI is changed as specified in the replacement string. The rewrite directives are executed sequentially in order of their appearance in the configuration file. It is possible to terminate further processing of the directives using flags. If a replacement string starts with ¡°http://¡± or ¡°https://¡±, the processing stops and the redirect is returned to a client. 

An optional flag parameter can be one of: 

last
stops processing the current set of ngx_http_rewrite_module directives and starts a search for a new location matching the changed URI; 
break
stops processing the current set of ngx_http_rewrite_module directives as with the break directive; 
redirect
returns a temporary redirect with the 302 code; used if a replacement string does not start with ¡°http://¡± or ¡°https://¡±; 
permanent
returns a permanent redirect with the 301 code. 
The full redirect URL is formed according to the request scheme ($scheme) and the server_name_in_redirect and port_in_redirect directives. 

Example: 

server {
    ...
    rewrite ^(/download/.*)/media/(.*)\..*$ $1/mp3/$2.mp3 last;
    rewrite ^(/download/.*)/audio/(.*)\..*$ $1/mp3/$2.ra  last;
    return  403;
    ...
}

But if these directives are put inside the ¡°/download/¡± location, the last flag should be replaced by break, or otherwise nginx will make 10 cycles and return the 500 error: 

location /download/ {
    rewrite ^(/download/.*)/media/(.*)\..*$ $1/mp3/$2.mp3 break;
    rewrite ^(/download/.*)/audio/(.*)\..*$ $1/mp3/$2.ra  break;
    return  403;
}

If a replacement string includes the new request arguments, the previous request arguments are appended after them. If this is undesired, putting a question mark at the end of a replacement string avoids having them appended, for example: 

rewrite ^/users/(.*)$ /show?user=$1? last;

If a regular expression includes the ¡°}¡± or ¡°;¡± characters, the whole expressions should be enclosed in single or double quotes. 

Syntax:  rewrite_log on | off;
 
Default:  rewrite_log off; 
Context:  http, server, location, if
 

Enables or disables logging of ngx_http_rewrite_module module directives processing results into the error_log at the notice level. 

Syntax:  set $variable value;
 
Default:  ¡ª  
Context:  server, location, if
 

Sets a value for the specified variable. The value can contain text, variables, and their combination. 

Syntax:  uninitialized_variable_warn on | off;
 
Default:  uninitialized_variable_warn on; 
Context:  http, server, location, if
 

Controls whether warnings about uninitialized variables are logged. 

Internal Implementation
The ngx_http_rewrite_module module directives are compiled at the configuration stage into internal instructions that are interpreted during request processing. An interpreter is a simple virtual stack machine. 

For example, the directives 

location /download/ {
    if ($forbidden) {
        return 403;
    }

    if ($slow) {
        limit_rate 10k;
    }

    rewrite ^/(download/.*)/media/(.*)\..*$ /$1/mp3/$2.mp3 break;
}
will be translated into these instructions: 

variable $forbidden
check against zero
    return 403
    end of code
variable $slow
check against zero
match of regular expression
copy "/"
copy $1
copy "/mp3/"
copy $2
copy ".mp3"
end of regular expression
end of code

Note that there are no instructions for the limit_rate directive above as it is unrelated to the ngx_http_rewrite_module module. A separate configuration is created for the if block. If the condition holds true, a request is assigned this configuration where limit_rate equals to 10k. 

The directive 

rewrite ^/(download/.*)/media/(.*)\..*$ /$1/mp3/$2.mp3 break;
can be made smaller by one instruction if the first slash in the regular expression is put inside the parentheses: 

rewrite ^(/download/.*)/media/(.*)\..*$ $1/mp3/$2.mp3 break;
The corresponding instructions will then look like this: 

match of regular expression
copy $1
copy "/mp3/"
copy $2
copy ".mp3"
end of regular expression
end of code






Core functionality
Example Configuration
Directives
     accept_mutex
     accept_mutex_delay
     daemon
     debug_connection
     debug_points
     error_log
     env
     events
     include
     lock_file
     master_process
     multi_accept
     pcre_jit
     pid
     ssl_engine
     thread_pool
     timer_resolution
     use
     user
     worker_aio_requests
     worker_connections
     worker_cpu_affinity
     worker_priority
     worker_processes
     worker_rlimit_core
     worker_rlimit_nofile
     working_directory
 
Example Configuration

user www www;
worker_processes 2;

error_log /var/log/nginx-error.log info;

events {
    use kqueue;
    worker_connections 2048;
}

...

Directives
Syntax:  accept_mutex on | off;
 
Default:  accept_mutex on; 
Context:  events
 

If accept_mutex is enabled, worker processes will accept new connections by turn. Otherwise, all worker processes will be notified about new connections, and if volume of new connections is low, some of the worker processes may just waste system resources. 

Syntax:  accept_mutex_delay time;
 
Default:  accept_mutex_delay 500ms; 
Context:  events
 

If accept_mutex is enabled, specifies the maximum time during which a worker process will try to restart accepting new connections if another worker process is currently accepting new connections. 

Syntax:  daemon on | off;
 
Default:  daemon on; 
Context:  main
 

Determines whether nginx should become a daemon. Mainly used during development. 

Syntax:  debug_connection address | CIDR | unix:;
 
Default:  ¡ª  
Context:  events
 

Enables debugging log for selected client connections. Other connections will use logging level set by the error_log directive. Debugged connections are specified by IPv4 or IPv6 (1.3.0, 1.2.1) address or network. A connection may also be specified using a hostname. For connections using UNIX-domain sockets (1.3.0, 1.2.1), debugging log is enabled by the ¡°unix:¡± parameter. 

events {
    debug_connection 127.0.0.1;
    debug_connection localhost;
    debug_connection 192.0.2.0/24;
    debug_connection ::1;
    debug_connection 2001:0db8::/32;
    debug_connection unix:;
    ...
}

For this directive to work, nginx needs to be built with --with-debug, see ¡°A debugging log¡±. 

Syntax:  debug_points abort | stop;
 
Default:  ¡ª  
Context:  main
 

This directive is used for debugging. 

When internal error is detected, e.g. the leak of sockets on restart of working processes, enabling debug_points leads to a core file creation (abort) or to stopping of a process (stop) for further analysis using a system debugger. 

Syntax:  error_log file | stderr | syslog:server=address[,parameter=value] | memory:size [debug | info | notice | warn | error | crit | alert | emerg];
 
Default:  error_log logs/error.log error; 
Context:  main, http, mail, stream, server, location
 

Configures logging. Several logs can be specified on the same level (1.5.2). 

The first parameter defines a file that will store the log. The special value stderr selects the standard error file. Logging to syslog can be configured by specifying the ¡°syslog:¡± prefix. Logging to a cyclic memory buffer can be configured by specifying the ¡°memory:¡± prefix and buffer size, and is generally used for debugging (1.7.11). 

The second parameter determines the level of logging. Log levels above are listed in the order of increasing severity. Setting a certain log level will cause all messages of the specified and more severe log levels to be logged. For example, the default level error will cause error, crit, alert, and emerg messages to be logged. If this parameter is omitted then error is used. 

For debug logging to work, nginx needs to be built with --with-debug, see ¡°A debugging log¡±. 

The directive can be specified on the stream level starting from version 1.7.11. 

The directive can be specified on the mail level starting from version 1.9.0. 

Syntax:  env variable[=value];
 
Default:  env TZ; 
Context:  main
 

By default, nginx removes all environment variables inherited from its parent process except the TZ variable. This directive allows preserving some of the inherited variables, changing their values, or creating new environment variables. These variables are then: 

?inherited during a live upgrade of an executable file; 
?used by the ngx_http_perl_module module; 
?used by worker processes. One should bear in mind that controlling system libraries in this way is not always possible as it is common for libraries to check variables only during initialization, well before they can be set using this directive. An exception from this is an above mentioned live upgrade of an executable file. 

The TZ variable is always inherited and available to the ngx_http_perl_module module, unless it is configured explicitly. 

Usage example: 

env MALLOC_OPTIONS;
env PERL5LIB=/data/site/modules;
env OPENSSL_ALLOW_PROXY_CERTS=1;


The NGINX environment variable is used internally by nginx and should not be set directly by the user. 

Syntax:  events { ... }
 
Default:  ¡ª  
Context:  main
 

Provides the configuration file context in which the directives that affect connection processing are specified. 

Syntax:  include file | mask;
 
Default:  ¡ª  
Context:  any
 

Includes another file, or files matching the specified mask, into configuration. Included files should consist of syntactically correct directives and blocks. 

Usage example: 

include mime.types;
include vhosts/*.conf;

Syntax:  lock_file file;
 
Default:  lock_file logs/nginx.lock; 
Context:  main
 

nginx uses the locking mechanism to implement accept_mutex and serialize access to shared memory. On most systems the locks are implemented using atomic operations, and this directive is ignored. On other systems the ¡°lock file¡± mechanism is used. This directive specifies a prefix for the names of lock files. 

Syntax:  master_process on | off;
 
Default:  master_process on; 
Context:  main
 

Determines whether worker processes are started. This directive is intended for nginx developers. 

Syntax:  multi_accept on | off;
 
Default:  multi_accept off; 
Context:  events
 

If multi_accept is disabled, a worker process will accept one new connection at a time. Otherwise, a worker process will accept all new connections at a time. 

The directive is ignored if kqueue connection processing method is used, because it reports the number of new connections waiting to be accepted. 

Syntax:  pcre_jit on | off;
 
Default:  pcre_jit off; 
Context:  main
 

This directive appeared in version 1.1.12. 

Enables or disables the use of ¡°just-in-time compilation¡± (PCRE JIT) for the regular expressions known by the time of configuration parsing. 

PCRE JIT can speed up processing of regular expressions significantly. 

The JIT is available in PCRE libraries starting from version 8.20 built with the --enable-jit configuration parameter. When the PCRE library is built with nginx (--with-pcre=), the JIT support is enabled via the --with-pcre-jit configuration parameter. 

Syntax:  pid file;
 
Default:  pid nginx.pid; 
Context:  main
 

Defines a file that will store the process ID of the main process. 

Syntax:  ssl_engine device;
 
Default:  ¡ª  
Context:  main
 

Defines the name of the hardware SSL accelerator. 

Syntax:  thread_pool name threads=number [max_queue=number];
 
Default:  thread_pool default threads=32 max_queue=65536; 
Context:  main
 

This directive appeared in version 1.7.11. 

Defines named thread pools used for multi-threaded reading and sending of files without blocking worker processes. 

The threads parameter defines the number of threads in the pool. 

In the event that all threads in the pool are busy, a new task will wait in the queue. The max_queue parameter limits the number of tasks allowed to be waiting in the queue. By default, up to 65536 tasks can wait in the queue. When the queue overflows, the task is completed with an error. 

Syntax:  timer_resolution interval;
 
Default:  ¡ª  
Context:  main
 

Reduces timer resolution in worker processes, thus reducing the number of gettimeofday() system calls made. By default, gettimeofday() is called each time a kernel event is received. With reduced resolution, gettimeofday() is only called once per specified interval. 

Example: 

timer_resolution 100ms;

Internal implementation of the interval depends on the method used: 

?the EVFILT_TIMER filter if kqueue is used; 
?timer_create() if eventport is used; 
?setitimer() otherwise. 

Syntax:  use method;
 
Default:  ¡ª  
Context:  events
 

Specifies the connection processing method to use. There is normally no need to specify it explicitly, because nginx will by default use the most efficient method. 

Syntax:  user user [group];
 
Default:  user nobody nobody; 
Context:  main
 

Defines user and group credentials used by worker processes. If group is omitted, a group whose name equals that of user is used. 

Syntax:  worker_aio_requests number;
 
Default:  worker_aio_requests 32; 
Context:  events
 

This directive appeared in versions 1.1.4 and 1.0.7. 

When using aio with the epoll connection processing method, sets the maximum number of outstanding asynchronous I/O operations for a single worker process. 

Syntax:  worker_connections number;
 
Default:  worker_connections 512; 
Context:  events
 

Sets the maximum number of simultaneous connections that can be opened by a worker process. 

It should be kept in mind that this number includes all connections (e.g. connections with proxied servers, among others), not only connections with clients. Another consideration is that the actual number of simultaneous connections cannot exceed the current limit on the maximum number of open files, which can be changed by worker_rlimit_nofile. 

Syntax:  worker_cpu_affinity cpumask ...;
 
Default:  ¡ª  
Context:  main
 

Binds worker processes to the sets of CPUs. Each CPU set is represented by a bitmask of allowed CPUs. There should be a separate set defined for each of the worker processes. By default, worker processes are not bound to any specific CPUs. 

For example, 

worker_processes    4;
worker_cpu_affinity 0001 0010 0100 1000;
binds each worker process to a separate CPU, while 

worker_processes    2;
worker_cpu_affinity 0101 1010;
binds the first worker process to CPU0/CPU2, and the second worker process to CPU1/CPU3. The second example is suitable for hyper-threading. 


The directive is only available on FreeBSD and Linux. 

Syntax:  worker_priority number;
 
Default:  worker_priority 0; 
Context:  main
 

Defines the scheduling priority for worker processes like it is done by the nice command: a negative number means higher priority. Allowed range normally varies from -20 to 20. 

Example: 

worker_priority -10;

Syntax:  worker_processes number | auto;
 
Default:  worker_processes 1; 
Context:  main
 

Defines the number of worker processes. 

The optimal value depends on many factors including (but not limited to) the number of CPU cores, the number of hard disk drives that store data, and load pattern. When one is in doubt, setting it to the number of available CPU cores would be a good start (the value ¡°auto¡± will try to autodetect it). 

The auto parameter is supported starting from versions 1.3.8 and 1.2.5. 

Syntax:  worker_rlimit_core size;
 
Default:  ¡ª  
Context:  main
 

Changes the limit on the largest size of a core file (RLIMIT_CORE) for worker processes. Used to increase the limit without restarting the main process. 

Syntax:  worker_rlimit_nofile number;
 
Default:  ¡ª  
Context:  main
 

Changes the limit on the maximum number of open files (RLIMIT_NOFILE) for worker processes. Used to increase the limit without restarting the main process. 

Syntax:  working_directory directory;
 
Default:  ¡ª  
Context:  main
 

Defines the current working directory for a worker process. It is primarily used when writing a core-file, in which case a worker process should have write permission for the specified directory. 




Module ngx_http_core_module
Directives
     aio
     alias
     chunked_transfer_encoding
     client_body_buffer_size
     client_body_in_file_only
     client_body_in_single_buffer
     client_body_temp_path
     client_body_timeout
     client_header_buffer_size
     client_header_timeout
     client_max_body_size
     connection_pool_size
     default_type
     directio
     directio_alignment
     disable_symlinks
     error_page
     etag
     http
     if_modified_since
     ignore_invalid_headers
     internal
     keepalive_disable
     keepalive_requests
     keepalive_timeout
     large_client_header_buffers
     limit_except
     limit_rate
     limit_rate_after
     lingering_close
     lingering_time
     lingering_timeout
     listen
     location
     log_not_found
     log_subrequest
     max_ranges
     merge_slashes
     msie_padding
     msie_refresh
     open_file_cache
     open_file_cache_errors
     open_file_cache_min_uses
     open_file_cache_valid
     output_buffers
     port_in_redirect
     postpone_output
     read_ahead
     recursive_error_pages
     request_pool_size
     reset_timedout_connection
     resolver
     resolver_timeout
     root
     satisfy
     send_lowat
     send_timeout
     sendfile
     sendfile_max_chunk
     server
     server_name
     server_name_in_redirect
     server_names_hash_bucket_size
     server_names_hash_max_size
     server_tokens
     tcp_nodelay
     tcp_nopush
     try_files
     types
     types_hash_bucket_size
     types_hash_max_size
     underscores_in_headers
     variables_hash_bucket_size
     variables_hash_max_size
Embedded Variables
 
Directives
Syntax:  aio on | off | threads[=pool];
 
Default:  aio off; 
Context:  http, server, location
 

This directive appeared in version 0.8.11. 

Enables or disables the use of asynchronous file I/O (AIO) on FreeBSD and Linux: 

location /video/ {
    aio            on;
    output_buffers 1 64k;
}

On FreeBSD, AIO can be used starting from FreeBSD 4.3. AIO can either be linked statically into a kernel: 

options VFS_AIO
or loaded dynamically as a kernel loadable module: 

kldload aio

On Linux, AIO can be used starting from kernel version 2.6.22. Also, it is necessary to enable directio, or otherwise reading will be blocking: 

location /video/ {
    aio            on;
    directio       512;
    output_buffers 1 128k;
}

On Linux, directio can only be used for reading blocks that are aligned on 512-byte boundaries (or 4K for XFS). File¡¯s unaligned end is read in blocking mode. The same holds true for byte range requests and for FLV requests not from the beginning of a file: reading of unaligned data at the beginning and end of a file will be blocking. 

When both AIO and sendfile are enabled on Linux, AIO is used for files that are larger than or equal to the size specified in the directio directive, while sendfile is used for files of smaller sizes or when directio is disabled. 

location /video/ {
    sendfile       on;
    aio            on;
    directio       8m;
}

Finally, files can be read and sent using multi-threading (1.7.11), without blocking a worker process: 

location /video/ {
    sendfile       on;
    aio            threads;
}
Read and send file operations are offloaded to threads of the specified pool. If the pool name is omitted, the pool with the name ¡°default¡± is used. The pool name can also be set with variables: 

aio threads=pool$disk;
By default, multi-threading is disabled, it should be enabled with the --with-threads configuration parameter. Currently, multi-threading is compatible only with the epoll, kqueue, and eventport methods. Multi-threaded sending of files is only supported on Linux. 

See also the sendfile directive. 

Syntax:  alias path;
 
Default:  ¡ª  
Context:  location
 

Defines a replacement for the specified location. For example, with the following configuration 

location /i/ {
    alias /data/w3/images/;
}
on request of ¡°/i/top.gif¡±, the file /data/w3/images/top.gif will be sent. 

The path value can contain variables, except $document_root and $realpath_root. 

If alias is used inside a location defined with a regular expression then such regular expression should contain captures and alias should refer to these captures (0.7.40), for example: 

location ~ ^/users/(.+\.(?:gif|jpe?g|png))$ {
    alias /data/w3/images/$1;
}

When location matches the last part of the directive¡¯s value: 

location /images/ {
    alias /data/w3/images/;
}
it is better to use the root directive instead: 

location /images/ {
    root /data/w3;
}

Syntax:  chunked_transfer_encoding on | off;
 
Default:  chunked_transfer_encoding on; 
Context:  http, server, location
 

Allows disabling chunked transfer encoding in HTTP/1.1. It may come in handy when using a software failing to support chunked encoding despite the standard¡¯s requirement. 

Syntax:  client_body_buffer_size size;
 
Default:  client_body_buffer_size 8k|16k; 
Context:  http, server, location
 

Sets buffer size for reading client request body. In case the request body is larger than the buffer, the whole body or only its part is written to a temporary file. By default, buffer size is equal to two memory pages. This is 8K on x86, other 32-bit platforms, and x86-64. It is usually 16K on other 64-bit platforms. 

Syntax:  client_body_in_file_only on | clean | off;
 
Default:  client_body_in_file_only off; 
Context:  http, server, location
 

Determines whether nginx should save the entire client request body into a file. This directive can be used during debugging, or when using the $request_body_file variable, or the $r->request_body_file method of the module ngx_http_perl_module. 

When set to the value on, temporary files are not removed after request processing. 

The value clean will cause the temporary files left after request processing to be removed. 

Syntax:  client_body_in_single_buffer on | off;
 
Default:  client_body_in_single_buffer off; 
Context:  http, server, location
 

Determines whether nginx should save the entire client request body in a single buffer. The directive is recommended when using the $request_body variable, to save the number of copy operations involved. 

Syntax:  client_body_temp_path path [level1 [level2 [level3]]];
 
Default:  client_body_temp_path client_body_temp; 
Context:  http, server, location
 

Defines a directory for storing temporary files holding client request bodies. Up to three-level subdirectory hierarchy can be used under the specified directory. For example, in the following configuration 

client_body_temp_path /spool/nginx/client_temp 1 2;
a path to a temporary file might look like this: 

/spool/nginx/client_temp/7/45/00000123457

Syntax:  client_body_timeout time;
 
Default:  client_body_timeout 60s; 
Context:  http, server, location
 

Defines a timeout for reading client request body. The timeout is set only for a period between two successive read operations, not for the transmission of the whole request body. If a client does not transmit anything within this time, the 408 (Request Time-out) error is returned to the client. 

Syntax:  client_header_buffer_size size;
 
Default:  client_header_buffer_size 1k; 
Context:  http, server
 

Sets buffer size for reading client request header. For most requests, a buffer of 1K bytes is enough. However, if a request includes long cookies, or comes from a WAP client, it may not fit into 1K. If a request line or a request header field does not fit into this buffer then larger buffers, configured by the large_client_header_buffers directive, are allocated. 

Syntax:  client_header_timeout time;
 
Default:  client_header_timeout 60s; 
Context:  http, server
 

Defines a timeout for reading client request header. If a client does not transmit the entire header within this time, the 408 (Request Time-out) error is returned to the client. 

Syntax:  client_max_body_size size;
 
Default:  client_max_body_size 1m; 
Context:  http, server, location
 

Sets the maximum allowed size of the client request body, specified in the ¡°Content-Length¡± request header field. If the size in a request exceeds the configured value, the 413 (Request Entity Too Large) error is returned to the client. Please be aware that browsers cannot correctly display this error. Setting size to 0 disables checking of client request body size. 

Syntax:  connection_pool_size size;
 
Default:  connection_pool_size 256; 
Context:  http, server
 

Allows accurate tuning of per-connection memory allocations. This directive has minimal impact on performance and should not generally be used. 

Syntax:  default_type mime-type;
 
Default:  default_type text/plain; 
Context:  http, server, location
 

Defines the default MIME type of a response. Mapping of file name extensions to MIME types can be set with the types directive. 

Syntax:  directio size | off;
 
Default:  directio off; 
Context:  http, server, location
 

This directive appeared in version 0.7.7. 

Enables the use of the O_DIRECT flag (FreeBSD, Linux), the F_NOCACHE flag (Mac OS X), or the directio() function (Solaris), when reading files that are larger than or equal to the specified size. The directive automatically disables (0.7.15) the use of sendfile for a given request. It can be useful for serving large files: 

directio 4m;
or when using aio on Linux. 

Syntax:  directio_alignment size;
 
Default:  directio_alignment 512; 
Context:  http, server, location
 

This directive appeared in version 0.8.11. 

Sets the alignment for directio. In most cases, a 512-byte alignment is enough. However, when using XFS under Linux, it needs to be increased to 4K. 

Syntax:  disable_symlinks off;
disable_symlinks on | if_not_owner [from=part];
 
Default:  disable_symlinks off; 
Context:  http, server, location
 

This directive appeared in version 1.1.15. 

Determines how symbolic links should be treated when opening files: 

off
Symbolic links in the pathname are allowed and not checked. This is the default behavior. 
on
If any component of the pathname is a symbolic link, access to a file is denied. 
if_not_owner
Access to a file is denied if any component of the pathname is a symbolic link, and the link and object that the link points to have different owners. 
from=part
When checking symbolic links (parameters on and if_not_owner), all components of the pathname are normally checked. Checking of symbolic links in the initial part of the pathname may be avoided by specifying additionally the from=part parameter. In this case, symbolic links are checked only from the pathname component that follows the specified initial part. If the value is not an initial part of the pathname checked, the whole pathname is checked as if this parameter was not specified at all. If the value matches the whole file name, symbolic links are not checked. The parameter value can contain variables. 

Example: 

disable_symlinks on from=$document_root;

This directive is only available on systems that have the openat() and fstatat() interfaces. Such systems include modern versions of FreeBSD, Linux, and Solaris. 

Parameters on and if_not_owner add a processing overhead. 

On systems that do not support opening of directories only for search, to use these parameters it is required that worker processes have read permissions for all directories being checked. 


The ngx_http_autoindex_module, ngx_http_random_index_module, and ngx_http_dav_module modules currently ignore this directive. 

Syntax:  error_page code ... [=[response]] uri;
 
Default:  ¡ª  
Context:  http, server, location, if in location
 

Defines the URI that will be shown for the specified errors. error_page directives are inherited from the previous level only if there are no error_page directives defined on the current level. A uri value can contain variables. 

Example: 

error_page 404             /404.html;
error_page 500 502 503 504 /50x.html;

Furthermore, it is possible to change the response code to another using the ¡°=response¡± syntax, for example: 

error_page 404 =200 /empty.gif;

If an error response is processed by a proxied server or a FastCGI/uwsgi/SCGI server, and the server may return different response codes (e.g., 200, 302, 401 or 404), it is possible to respond with the code it returns: 

error_page 404 = /404.php;

It is also possible to use redirects for error processing: 

error_page 403      http://example.com/forbidden.html;
error_page 404 =301 http://example.com/notfound.html;
In this case, by default, the response code 302 is returned to the client. It can only be changed to one of the redirect status codes (301, 302, 303, and 307). 

If there is no need to change URI during internal redirection it is possible to pass error processing into a named location: 

location / {
    error_page 404 = @fallback;
}

location @fallback {
    proxy_pass http://backend;
}


If uri processing leads to an error, the status code of the last occurred error is returned to the client. 

Syntax:  etag on | off;
 
Default:  etag on; 
Context:  http, server, location
 

This directive appeared in version 1.3.3. 

Enables or disables automatic generation of the ¡°ETag¡± response header field for static resources. 

Syntax:  http { ... }
 
Default:  ¡ª  
Context:  main
 

Provides the configuration file context in which the HTTP server directives are specified. 

Syntax:  if_modified_since off | exact | before;
 
Default:  if_modified_since exact; 
Context:  http, server, location
 

This directive appeared in version 0.7.24. 

Specifies how to compare modification time of a response with the time in the ¡°If-Modified-Since¡± request header field: 

off
the ¡°If-Modified-Since¡± request header field is ignored (0.7.34); 
exact
exact match; 
before
modification time of a response is less than or equal to the time in the ¡°If-Modified-Since¡± request header field. 

Syntax:  ignore_invalid_headers on | off;
 
Default:  ignore_invalid_headers on; 
Context:  http, server
 

Controls whether header fields with invalid names should be ignored. Valid names are composed of English letters, digits, hyphens, and possibly underscores (as controlled by the underscores_in_headers directive). 

If the directive is specified on the server level, its value is only used if a server is a default one. The value specified also applies to all virtual servers listening on the same address and port. 

Syntax:  internal;
 
Default:  ¡ª  
Context:  location
 

Specifies that a given location can only be used for internal requests. For external requests, the client error 404 (Not Found) is returned. Internal requests are the following: 

?requests redirected by the error_page, index, random_index, and try_files directives; 
?requests redirected by the ¡°X-Accel-Redirect¡± response header field from an upstream server; 
?subrequests formed by the ¡°include virtual¡± command of the ngx_http_ssi_module module and by the ngx_http_addition_module module directives; 
?requests changed by the rewrite directive. 

Example: 

error_page 404 /404.html;

location /404.html {
    internal;
}

There is a limit of 10 internal redirects per request to prevent request processing cycles that can occur in incorrect configurations. If this limit is reached, the error 500 (Internal Server Error) is returned. In such cases, the ¡°rewrite or internal redirection cycle¡± message can be seen in the error log. 

Syntax:  keepalive_disable none | browser ...;
 
Default:  keepalive_disable msie6; 
Context:  http, server, location
 

Disables keep-alive connections with misbehaving browsers. The browser parameters specify which browsers will be affected. The value msie6 disables keep-alive connections with old versions of MSIE, once a POST request is received. The value safari disables keep-alive connections with Safari and Safari-like browsers on Mac OS X and Mac OS X-like operating systems. The value none enables keep-alive connections with all browsers. 

Prior to version 1.1.18, the value safari matched all Safari and Safari-like browsers on all operating systems, and keep-alive connections with them were disabled by default. 

Syntax:  keepalive_requests number;
 
Default:  keepalive_requests 100; 
Context:  http, server, location
 

This directive appeared in version 0.8.0. 

Sets the maximum number of requests that can be served through one keep-alive connection. After the maximum number of requests are made, the connection is closed. 

Syntax:  keepalive_timeout timeout [header_timeout];
 
Default:  keepalive_timeout 75s; 
Context:  http, server, location
 

The first parameter sets a timeout during which a keep-alive client connection will stay open on the server side. The zero value disables keep-alive client connections. The optional second parameter sets a value in the ¡°Keep-Alive: timeout=time¡± response header field. Two parameters may differ. 

The ¡°Keep-Alive: timeout=time¡± header field is recognized by Mozilla and Konqueror. MSIE closes keep-alive connections by itself in about 60 seconds. 

Syntax:  large_client_header_buffers number size;
 
Default:  large_client_header_buffers 4 8k; 
Context:  http, server
 

Sets the maximum number and size of buffers used for reading large client request header. A request line cannot exceed the size of one buffer, or the 414 (Request-URI Too Large) error is returned to the client. A request header field cannot exceed the size of one buffer as well, or the 400 (Bad Request) error is returned to the client. Buffers are allocated only on demand. By default, the buffer size is equal to 8K bytes. If after the end of request processing a connection is transitioned into the keep-alive state, these buffers are released. 

Syntax:  limit_except method ... { ... }
 
Default:  ¡ª  
Context:  location
 

Limits allowed HTTP methods inside a location. The method parameter can be one of the following: GET, HEAD, POST, PUT, DELETE, MKCOL, COPY, MOVE, OPTIONS, PROPFIND, PROPPATCH, LOCK, UNLOCK, or PATCH. Allowing the GET method makes the HEAD method also allowed. Access to other methods can be limited using the ngx_http_access_module and ngx_http_auth_basic_module modules directives: 

limit_except GET {
    allow 192.168.1.0/32;
    deny  all;
}
Please note that this will limit access to all methods except GET and HEAD. 

Syntax:  limit_rate rate;
 
Default:  limit_rate 0; 
Context:  http, server, location, if in location
 

Limits the rate of response transmission to a client. The rate is specified in bytes per second. The zero value disables rate limiting. The limit is set per a request, and so if a client simultaneously opens two connections, the overall rate will be twice as much as the specified limit. 

Rate limit can also be set in the $limit_rate variable. It may be useful in cases where rate should be limited depending on a certain condition: 

server {

    if ($slow) {
        set $limit_rate 4k;
    }

    ...
}

Rate limit can also be set in the ¡°X-Accel-Limit-Rate¡± header field of a proxied server response. This capability can be disabled using the proxy_ignore_headers, fastcgi_ignore_headers, uwsgi_ignore_headers, and scgi_ignore_headers directives. 

Syntax:  limit_rate_after size;
 
Default:  limit_rate_after 0; 
Context:  http, server, location, if in location
 

This directive appeared in version 0.8.0. 

Sets the initial amount after which the further transmission of a response to a client will be rate limited. 

Example: 

location /flv/ {
    flv;
    limit_rate_after 500k;
    limit_rate       50k;
}

Syntax:  lingering_close off | on | always;
 
Default:  lingering_close on; 
Context:  http, server, location
 

This directive appeared in versions 1.1.0 and 1.0.6. 

Controls how nginx closes client connections. 

The default value ¡°on¡± instructs nginx to wait for and process additional data from a client before fully closing a connection, but only if heuristics suggests that a client may be sending more data. 

The value ¡°always¡± will cause nginx to unconditionally wait for and process additional client data. 

The value ¡°off¡± tells nginx to never wait for more data and close the connection immediately. This behavior breaks the protocol and should not be used under normal circumstances. 

Syntax:  lingering_time time;
 
Default:  lingering_time 30s; 
Context:  http, server, location
 

When lingering_close is in effect, this directive specifies the maximum time during which nginx will process (read and ignore) additional data coming from a client. After that, the connection will be closed, even if there will be more data. 

Syntax:  lingering_timeout time;
 
Default:  lingering_timeout 5s; 
Context:  http, server, location
 

When lingering_close is in effect, this directive specifies the maximum waiting time for more client data to arrive. If data are not received during this time, the connection is closed. Otherwise, the data are read and ignored, and nginx starts waiting for more data again. The ¡°wait-read-ignore¡± cycle is repeated, but no longer than specified by the lingering_time directive. 

Syntax:  listen address[:port] [default_server] [ssl] [spdy] [proxy_protocol] [setfib=number] [fastopen=number] [backlog=number] [rcvbuf=size] [sndbuf=size] [accept_filter=filter] [deferred] [bind] [ipv6only=on|off] [reuseport] [so_keepalive=on|off|[keepidle]:[keepintvl]:[keepcnt]];
listen port [default_server] [ssl] [spdy] [proxy_protocol] [setfib=number] [fastopen=number] [backlog=number] [rcvbuf=size] [sndbuf=size] [accept_filter=filter] [deferred] [bind] [ipv6only=on|off] [reuseport] [so_keepalive=on|off|[keepidle]:[keepintvl]:[keepcnt]];
listen unix:path [default_server] [ssl] [spdy] [proxy_protocol] [backlog=number] [rcvbuf=size] [sndbuf=size] [accept_filter=filter] [deferred] [bind] [so_keepalive=on|off|[keepidle]:[keepintvl]:[keepcnt]];
 
Default:  listen *:80 | *:8000; 
Context:  server
 

Sets the address and port for IP, or the path for a UNIX-domain socket on which the server will accept requests. Both address and port, or only address or only port can be specified. An address may also be a hostname, for example: 

listen 127.0.0.1:8000;
listen 127.0.0.1;
listen 8000;
listen *:8000;
listen localhost:8000;
IPv6 addresses (0.7.36) are specified in square brackets: 

listen [::]:8000;
listen [::1];
UNIX-domain sockets (0.8.21) are specified with the ¡°unix:¡± prefix: 

listen unix:/var/run/nginx.sock;

If only address is given, the port 80 is used. 

If the directive is not present then either *:80 is used if nginx runs with the superuser privileges, or *:8000 otherwise. 

The default_server parameter, if present, will cause the server to become the default server for the specified address:port pair. If none of the directives have the default_server parameter then the first server with the address:port pair will be the default server for this pair. 

In versions prior to 0.8.21 this parameter is named simply default. 

The ssl parameter (0.7.14) allows specifying that all connections accepted on this port should work in SSL mode. This allows for a more compact configuration for the server that handles both HTTP and HTTPS requests. 

The spdy parameter (1.3.15) allows accepting SPDY connections on this port. Normally, for this to work the ssl parameter should be specified as well, but nginx can also be configured to accept SPDY connections without SSL. 

The proxy_protocol parameter (1.5.12) allows specifying that all connections accepted on this port should use the PROXY protocol. 

The listen directive can have several additional parameters specific to socket-related system calls. These parameters can be specified in any listen directive, but only once for a given address:port pair. 

In versions prior to 0.8.21, they could only be specified in the listen directive together with the default parameter. 

setfib=number 
this parameter (0.8.44) sets the associated routing table, FIB (the SO_SETFIB option) for the listening socket. This currently works only on FreeBSD. 
fastopen=number 
enables ¡°TCP Fast Open¡± for the listening socket (1.5.8) and limits the maximum length for the queue of connections that have not yet completed the three-way handshake. 
Do not enable this feature unless the server can handle receiving the same SYN packet with data more than once. 
backlog=number 
sets the backlog parameter in the listen() call that limits the maximum length for the queue of pending connections. By default, backlog is set to -1 on FreeBSD, DragonFly BSD, and Mac OS X, and to 511 on other platforms. 
rcvbuf=size 
sets the receive buffer size (the SO_RCVBUF option) for the listening socket. 
sndbuf=size 
sets the send buffer size (the SO_SNDBUF option) for the listening socket. 
accept_filter=filter 
sets the name of accept filter (the SO_ACCEPTFILTER option) for the listening socket that filters incoming connections before passing them to accept(). This works only on FreeBSD and NetBSD 5.0+. Possible values are dataready and httpready. 
deferred 
instructs to use a deferred accept() (the TCP_DEFER_ACCEPT socket option) on Linux. 
bind 
instructs to make a separate bind() call for a given address:port pair. This is useful because if there are several listen directives with the same port but different addresses, and one of the listen directives listens on all addresses for the given port (*:port), nginx will bind() only to *:port. It should be noted that the getsockname() system call will be made in this case to determine the address that accepted the connection. If the setfib, backlog, rcvbuf, sndbuf, accept_filter, deferred, ipv6only, or so_keepalive parameters are used then for a given address:port pair a separate bind() call will always be made. 
ipv6only=on|off 
this parameter (0.7.42) determines (via the IPV6_V6ONLY socket option) whether an IPv6 socket listening on a wildcard address [::] will accept only IPv6 connections or both IPv6 and IPv4 connections. This parameter is turned on by default. It can only be set once on start. 
Prior to version 1.3.4, if this parameter was omitted then the operating system¡¯s settings were in effect for the socket. 
reuseport 
this parameter (1.9.1) instructs to create an individual listening socket for each worker process (using the SO_REUSEPORT socket option), allowing a kernel to distribute incoming connections between worker processes. This currently works only on Linux 3.9+ and DragonFly BSD. 
Inappropriate use of this option may have its security implications. 
so_keepalive=on|off|[keepidle]:[keepintvl]:[keepcnt] 
this parameter (1.1.11) configures the ¡°TCP keepalive¡± behavior for the listening socket. If this parameter is omitted then the operating system¡¯s settings will be in effect for the socket. If it is set to the value ¡°on¡±, the SO_KEEPALIVE option is turned on for the socket. If it is set to the value ¡°off¡±, the SO_KEEPALIVE option is turned off for the socket. Some operating systems support setting of TCP keepalive parameters on a per-socket basis using the TCP_KEEPIDLE, TCP_KEEPINTVL, and TCP_KEEPCNT socket options. On such systems (currently, Linux 2.4+, NetBSD 5+, and FreeBSD 9.0-STABLE), they can be configured using the keepidle, keepintvl, and keepcnt parameters. One or two parameters may be omitted, in which case the system default setting for the corresponding socket option will be in effect. For example, 
so_keepalive=30m::10will set the idle timeout (TCP_KEEPIDLE) to 30 minutes, leave the probe interval (TCP_KEEPINTVL) at its system default, and set the probes count (TCP_KEEPCNT) to 10 probes. 

Example: 

listen 127.0.0.1 default_server accept_filter=dataready backlog=1024;

Syntax:  location [ = | ~ | ~* | ^~ ] uri { ... }
location @name { ... }
 
Default:  ¡ª  
Context:  server, location
 

Sets configuration depending on a request URI. 

The matching is performed against a normalized URI, after decoding the text encoded in the ¡°%XX¡± form, resolving references to relative path components ¡°.¡± and ¡°..¡±, and possible compression of two or more adjacent slashes into a single slash. 

A location can either be defined by a prefix string, or by a regular expression. Regular expressions are specified with the preceding ¡°~*¡± modifier (for case-insensitive matching), or the ¡°~¡± modifier (for case-sensitive matching). To find location matching a given request, nginx first checks locations defined using the prefix strings (prefix locations). Among them, the location with the longest matching prefix is selected and remembered. Then regular expressions are checked, in the order of their appearance in the configuration file. The search of regular expressions terminates on the first match, and the corresponding configuration is used. If no match with a regular expression is found then the configuration of the prefix location remembered earlier is used. 

location blocks can be nested, with some exceptions mentioned below. 

For case-insensitive operating systems such as Mac OS X and Cygwin, matching with prefix strings ignores a case (0.7.7). However, comparison is limited to one-byte locales. 

Regular expressions can contain captures (0.7.40) that can later be used in other directives. 

If the longest matching prefix location has the ¡°^~¡± modifier then regular expressions are not checked. 

Also, using the ¡°=¡± modifier it is possible to define an exact match of URI and location. If an exact match is found, the search terminates. For example, if a ¡°/¡± request happens frequently, defining ¡°location = /¡± will speed up the processing of these requests, as search terminates right after the first comparison. Such a location cannot obviously contain nested locations. 


In versions from 0.7.1 to 0.8.41, if a request matched the prefix location without the ¡°=¡± and ¡°^~¡± modifiers, the search also terminated and regular expressions were not checked. 

Let¡¯s illustrate the above by an example: 

location = / {
    [ configuration A ]
}

location / {
    [ configuration B ]
}

location /documents/ {
    [ configuration C ]
}

location ^~ /images/ {
    [ configuration D ]
}

location ~* \.(gif|jpg|jpeg)$ {
    [ configuration E ]
}
The ¡°/¡± request will match configuration A, the ¡°/index.html¡± request will match configuration B, the ¡°/documents/document.html¡± request will match configuration C, the ¡°/images/1.gif¡± request will match configuration D, and the ¡°/documents/1.jpg¡± request will match configuration E. 

The ¡°@¡± prefix defines a named location. Such a location is not used for a regular request processing, but instead used for request redirection. They cannot be nested, and cannot contain nested locations. 

If a location is defined by a prefix string that ends with the slash character, and requests are processed by one of proxy_pass, fastcgi_pass, uwsgi_pass, scgi_pass, or memcached_pass, then the special processing is performed. In response to a request with URI equal to this string, but without the trailing slash, a permanent redirect with the code 301 will be returned to the requested URI with the slash appended. If this is not desired, an exact match of the URI and location could be defined like this: 

location /user/ {
    proxy_pass http://user.example.com;
}

location = /user {
    proxy_pass http://login.example.com;
}

Syntax:  log_not_found on | off;
 
Default:  log_not_found on; 
Context:  http, server, location
 

Enables or disables logging of errors about not found files into error_log. 

Syntax:  log_subrequest on | off;
 
Default:  log_subrequest off; 
Context:  http, server, location
 

Enables or disables logging of subrequests into access_log. 

Syntax:  max_ranges number;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 1.1.2. 

Limits the maximum allowed number of ranges in byte-range requests. Requests that exceed the limit are processed as if there were no byte ranges specified. By default, the number of ranges is not limited. The zero value disables the byte-range support completely. 

Syntax:  merge_slashes on | off;
 
Default:  merge_slashes on; 
Context:  http, server
 

Enables or disables compression of two or more adjacent slashes in a URI into a single slash. 

Note that compression is essential for the correct matching of prefix string and regular expression locations. Without it, the ¡°//scripts/one.php¡± request would not match 

location /scripts/ {
    ...
}
and might be processed as a static file. So it gets converted to ¡°/scripts/one.php¡±. 

Turning the compression off can become necessary if a URI contains base64-encoded names, since base64 uses the ¡°/¡± character internally. However, for security considerations, it is better to avoid turning the compression off. 

If the directive is specified on the server level, its value is only used if a server is a default one. The value specified also applies to all virtual servers listening on the same address and port. 

Syntax:  msie_padding on | off;
 
Default:  msie_padding on; 
Context:  http, server, location
 

Enables or disables adding comments to responses for MSIE clients with status greater than 400 to increase the response size to 512 bytes. 

Syntax:  msie_refresh on | off;
 
Default:  msie_refresh off; 
Context:  http, server, location
 

Enables or disables issuing refreshes instead of redirects for MSIE clients. 

Syntax:  open_file_cache off;
open_file_cache max=N [inactive=time];
 
Default:  open_file_cache off; 
Context:  http, server, location
 

Configures a cache that can store: 

?open file descriptors, their sizes and modification times; 
?information on existence of directories; 
?file lookup errors, such as ¡°file not found¡±, ¡°no read permission¡±, and so on. 
Caching of errors should be enabled separately by the open_file_cache_errors directive. 

The directive has the following parameters: 

max 
sets the maximum number of elements in the cache; on cache overflow the least recently used (LRU) elements are removed; 
inactive 
defines a time after which an element is removed from the cache if it has not been accessed during this time; by default, it is 60 seconds; 
off 
disables the cache. 

Example: 

open_file_cache          max=1000 inactive=20s;
open_file_cache_valid    30s;
open_file_cache_min_uses 2;
open_file_cache_errors   on;

Syntax:  open_file_cache_errors on | off;
 
Default:  open_file_cache_errors off; 
Context:  http, server, location
 

Enables or disables caching of file lookup errors by open_file_cache. 

Syntax:  open_file_cache_min_uses number;
 
Default:  open_file_cache_min_uses 1; 
Context:  http, server, location
 

Sets the minimum number of file accesses during the period configured by the inactive parameter of the open_file_cache directive, required for a file descriptor to remain open in the cache. 

Syntax:  open_file_cache_valid time;
 
Default:  open_file_cache_valid 60s; 
Context:  http, server, location
 

Sets a time after which open_file_cache elements should be validated. 

Syntax:  output_buffers number size;
 
Default:  output_buffers 1 32k; 
Context:  http, server, location
 

Sets the number and size of the buffers used for reading a response from a disk. 

Syntax:  port_in_redirect on | off;
 
Default:  port_in_redirect on; 
Context:  http, server, location
 

Enables or disables specifying the port in redirects issued by nginx. 

The use of the primary server name in redirects is controlled by the server_name_in_redirect directive. 

Syntax:  postpone_output size;
 
Default:  postpone_output 1460; 
Context:  http, server, location
 

If possible, the transmission of client data will be postponed until nginx has at least size bytes of data to send. The zero value disables postponing data transmission. 

Syntax:  read_ahead size;
 
Default:  read_ahead 0; 
Context:  http, server, location
 

Sets the amount of pre-reading for the kernel when working with file. 

On Linux, the posix_fadvise(0, 0, 0, POSIX_FADV_SEQUENTIAL) system call is used, and so the size parameter is ignored. 

On FreeBSD, the fcntl(O_READAHEAD, size) system call, supported since FreeBSD 9.0-CURRENT, is used. FreeBSD 7 has to be patched. 

Syntax:  recursive_error_pages on | off;
 
Default:  recursive_error_pages off; 
Context:  http, server, location
 

Enables or disables doing several redirects using the error_page directive. The number of such redirects is limited. 

Syntax:  request_pool_size size;
 
Default:  request_pool_size 4k; 
Context:  http, server
 

Allows accurate tuning of per-request memory allocations. This directive has minimal impact on performance and should not generally be used. 

Syntax:  reset_timedout_connection on | off;
 
Default:  reset_timedout_connection off; 
Context:  http, server, location
 

Enables or disables resetting timed out connections. The reset is performed as follows. Before closing a socket, 
the SO_LINGER option is set on it with a timeout value of 0. When the socket is closed, TCP RST is sent to the client, 
and all memory occupied by this socket is released. This helps avoid keeping an already closed socket with filled buffers 
in a FIN_WAIT1 state for a long time. 

It should be noted that timed out keep-alive connections are closed normally. 

Syntax:  resolver address ... [valid=time] [ipv6=on|off];
 
Default:  ¡ª  
Context:  http, server, location
 

Configures name servers used to resolve names of upstream servers into addresses, for example: 

resolver 127.0.0.1 [::1]:5353;
An address can be specified as a domain name or IP address, and an optional port (1.3.1, 1.2.2). If port is not specified, the port 53 is used. Name servers are queried in a round-robin fashion. 

Before version 1.1.7, only a single name server could be configured. Specifying name servers using IPv6 addresses is supported starting from versions 1.3.1 and 1.2.2. 
By default, nginx will look up both IPv4 and IPv6 addresses while resolving. If looking up of IPv6 addresses is not desired, the ipv6=off parameter can be specified. 

Resolving of names into IPv6 addresses is supported starting from version 1.5.8. 
By default, nginx caches answers using the TTL value of a response. An optional valid parameter allows overriding it: 

resolver 127.0.0.1 [::1]:5353 valid=30s;

Before version 1.1.9, tuning of caching time was not possible, and nginx always cached answers for the duration of 5 minutes. 

Syntax:  resolver_timeout time;
 
Default:  resolver_timeout 30s; 
Context:  http, server, location
 

Sets a timeout for name resolution, for example: 

resolver_timeout 5s;

Syntax:  root path;
 
Default:  root html; 
Context:  http, server, location, if in location
 

Sets the root directory for requests. For example, with the following configuration 

location /i/ {
    root /data/w3;
}
The /data/w3/i/top.gif file will be sent in response to the ¡°/i/top.gif¡± request. 

The path value can contain variables, except $document_root and $realpath_root. 

A path to the file is constructed by merely adding a URI to the value of the root directive. If a URI has to be modified, the alias directive should be used. 

Syntax:  satisfy all | any;
 
Default:  satisfy all; 
Context:  http, server, location
 

Allows access if all (all) or at least one (any) of the ngx_http_access_module, ngx_http_auth_basic_module or ngx_http_auth_request_module modules allow access. 

Example: 

location / {
    satisfy any;

    allow 192.168.1.0/32;
    deny  all;

    auth_basic           "closed site";
    auth_basic_user_file conf/htpasswd;
}

Syntax:  send_lowat size;
 
Default:  send_lowat 0; 
Context:  http, server, location
 
If the directive is set to a non-zero value, nginx will try to minimize the number of send operations on client sockets by 
using either NOTE_LOWAT flag of the kqueue method or the SO_SNDLOWAT socket option. In both cases the specified size is used. 

This directive is ignored on Linux, Solaris, and Windows. 

Syntax:  send_timeout time;
 
Default:  send_timeout 60s; 
Context:  http, server, location
 

Sets a timeout for transmitting a response to the client. The timeout is set only between two successive write operations, not for the transmission of the whole response. If the client does not receive anything within this time, the connection is closed. 

Syntax:  sendfile on | off;
 
Default:  sendfile off; 
Context:  http, server, location, if in location
 

Enables or disables the use of sendfile(). 

Starting from nginx 0.8.12 and FreeBSD 5.2.1, aio can be used to pre-load data for sendfile(): 

location /video/ {
    sendfile       on;
    tcp_nopush     on;
    aio            on;
}
In this configuration, sendfile() is called with the SF_NODISKIO flag which causes it not to block on disk I/O, but, instead, report back that the data are not in memory. nginx then initiates an asynchronous data load by reading one byte. On the first read, the FreeBSD kernel loads the first 128K bytes of a file into memory, although next reads will only load data in 16K chunks. This can be changed using the read_ahead directive. 

Before version 1.7.11, pre-loading could be enabled with aio sendfile;. 

Syntax:  sendfile_max_chunk size;
 
Default:  sendfile_max_chunk 0; 
Context:  http, server, location
 

When set to a non-zero value, limits the amount of data that can be transferred in a single sendfile() call. Without the limit, one fast connection may seize the worker process entirely. 

Syntax:  server { ... }
 
Default:  ¡ª  
Context:  http
 

Sets configuration for a virtual server. There is no clear separation between IP-based (based on the IP address) and name-based (based on the ¡°Host¡± request header field) virtual servers. Instead, the listen directives describe all addresses and ports that should accept connections for the server, and the server_name directive lists all server names. Example configurations are provided in the ¡°How nginx processes a request¡± document. 

Syntax:  server_name name ...;
 
Default:  server_name ""; 
Context:  server
 

Sets names of a virtual server, for example: 

server {
    server_name example.com www.example.com;
}

The first name becomes the primary server name. 

Server names can include an asterisk (¡°*¡±) replacing the first or last part of a name: 

server {
    server_name example.com *.example.com www.example.*;
}
Such names are called wildcard names. 

The first two of the names mentioned above can be combined in one: 

server {
    server_name .example.com;
}

It is also possible to use regular expressions in server names, preceding the name with a tilde (¡°~¡±): 

server {
    server_name www.example.com ~^www\d+\.example\.com$;
}

Regular expressions can contain captures (0.7.40) that can later be used in other directives: 

server {
    server_name ~^(www\.)?(.+)$;

    location / {
        root /sites/$2;
    }
}

server {
    server_name _;

    location / {
        root /sites/default;
    }
}

Named captures in regular expressions create variables (0.8.25) that can later be used in other directives: 

server {
    server_name ~^(www\.)?(?<domain>.+)$;

    location / {
        root /sites/$domain;
    }
}

server {
    server_name _;

    location / {
        root /sites/default;
    }
}

If the directive¡¯s parameter is set to ¡°$hostname¡± (0.9.4), the machine¡¯s hostname is inserted. 

It is also possible to specify an empty server name (0.7.11): 

server {
    server_name www.example.com "";
}
It allows this server to process requests without the ¡°Host¡± header field ¡ª instead of the default server ¡ª for the given address:port pair. This is the default setting. 

Before 0.8.48, the machine¡¯s hostname was used by default. 

During searching for a virtual server by name, if the name matches more than one of the specified variants, (e.g. both a wildcard name and regular expression match), the first matching variant will be chosen, in the following order of priority: 

1.the exact name 
2.the longest wildcard name starting with an asterisk, e.g. ¡°*.example.com¡± 
3.the longest wildcard name ending with an asterisk, e.g. ¡°mail.*¡± 
4.the first matching regular expression (in order of appearance in the configuration file) 

Detailed description of server names is provided in a separate Server names document. 

Syntax:  server_name_in_redirect on | off;
 
Default:  server_name_in_redirect off; 
Context:  http, server, location
 

Enables or disables the use of the primary server name, specified by the server_name directive, in redirects issued by nginx. When the use of the primary server name is disabled, the name from the ¡°Host¡± request header field is used. If this field is not present, the IP address of the server is used. 

The use of a port in redirects is controlled by the port_in_redirect directive. 

Syntax:  server_names_hash_bucket_size size;
 
Default:  server_names_hash_bucket_size 32|64|128; 
Context:  http
 

Sets the bucket size for the server names hash tables. The default value depends on the size of the processor¡¯s cache line. The details of setting up hash tables are provided in a separate document. 

Syntax:  server_names_hash_max_size size;
 
Default:  server_names_hash_max_size 512; 
Context:  http
 

Sets the maximum size of the server names hash tables. The details of setting up hash tables are provided in a separate document. 

Syntax:  server_tokens on | off;
 
Default:  server_tokens on; 
Context:  http, server, location
 

Enables or disables emitting nginx version in error messages and in the ¡°Server¡± response header field. 

Syntax:  tcp_nodelay on | off;
 
Default:  tcp_nodelay on; 
Context:  http, server, location
 

Enables or disables the use of the TCP_NODELAY option. The option is enabled only when a connection is transitioned into the keep-alive state. 

Syntax:  tcp_nopush on | off;
 
Default:  tcp_nopush off; 
Context:  http, server, location
 

Enables or disables the use of the TCP_NOPUSH socket option on FreeBSD or the TCP_CORK socket option on Linux. The options are enabled only when sendfile is used. Enabling the option allows 

?sending the response header and the beginning of a file in one packet, on Linux and FreeBSD 4.*; 
?sending a file in full packets. 

Syntax:  try_files file ... uri;
try_files file ... =code;
 
Default:  ¡ª  
Context:  server, location
 

Checks the existence of files in the specified order and uses the first found file for request processing; the processing is performed in the current context. The path to a file is constructed from the file parameter according to the root and alias directives. It is possible to check directory¡¯s existence by specifying a slash at the end of a name, e.g. ¡°$uri/¡±. If none of the files were found, an internal redirect to the uri specified in the last parameter is made. For example: 

location /images/ {
    try_files $uri /images/default.gif;
}

location = /images/default.gif {
    expires 30s;
}
The last parameter can also point to a named location, as shown in examples below. Starting from version 0.7.51, the last parameter can also be a code: 

location / {
    try_files $uri $uri/index.html $uri.html =404;
}

Example in proxying Mongrel: 

location / {
    try_files /system/maintenance.html
              $uri $uri/index.html $uri.html
              @mongrel;
}

location @mongrel {
    proxy_pass http://mongrel;
}

Example for Drupal/FastCGI: 

location / {
    try_files $uri $uri/ @drupal;
}

location ~ \.php$ {
    try_files $uri @drupal;

    fastcgi_pass ...;

    fastcgi_param SCRIPT_FILENAME /path/to$fastcgi_script_name;
    fastcgi_param SCRIPT_NAME     $fastcgi_script_name;
    fastcgi_param QUERY_STRING    $args;

    ... other fastcgi_param's
}

location @drupal {
    fastcgi_pass ...;

    fastcgi_param SCRIPT_FILENAME /path/to/index.php;
    fastcgi_param SCRIPT_NAME     /index.php;
    fastcgi_param QUERY_STRING    q=$uri&$args;

    ... other fastcgi_param's
}
In the following example, 

location / {
    try_files $uri $uri/ @drupal;
}
the try_files directive is equivalent to 

location / {
    error_page 404 = @drupal;
    log_not_found off;
}
And here, 

location ~ \.php$ {
    try_files $uri @drupal;

    fastcgi_pass ...;

    fastcgi_param SCRIPT_FILENAME /path/to$fastcgi_script_name;

    ...
}
try_files checks the existence of the PHP file before passing the request to the FastCGI server. 

Example for Wordpress and Joomla: 

location / {
    try_files $uri $uri/ @wordpress;
}

location ~ \.php$ {
    try_files $uri @wordpress;

    fastcgi_pass ...;

    fastcgi_param SCRIPT_FILENAME /path/to$fastcgi_script_name;
    ... other fastcgi_param's
}

location @wordpress {
    fastcgi_pass ...;

    fastcgi_param SCRIPT_FILENAME /path/to/index.php;
    ... other fastcgi_param's
}

Syntax:  types { ... }
 
Default:  types {
    text/html  html;
    image/gif  gif;
    image/jpeg jpg;
} 
Context:  http, server, location
 

Maps file name extensions to MIME types of responses. Extensions are case-insensitive. Several extensions can be mapped to one type, for example: 

types {
    application/octet-stream bin exe dll;
    application/octet-stream deb;
    application/octet-stream dmg;
}

A sufficiently full mapping table is distributed with nginx in the conf/mime.types file. 

To make a particular location emit the ¡°application/octet-stream¡± MIME type for all requests, the following configuration can be used: 

location /download/ {
    types        { }
    default_type application/octet-stream;
}

Syntax:  types_hash_bucket_size size;
 
Default:  types_hash_bucket_size 64; 
Context:  http, server, location
 

Sets the bucket size for the types hash tables. The details of setting up hash tables are provided in a separate document. 

Prior to version 1.5.13, the default value depended on the size of the processor¡¯s cache line. 

Syntax:  types_hash_max_size size;
 
Default:  types_hash_max_size 1024; 
Context:  http, server, location
 

Sets the maximum size of the types hash tables. The details of setting up hash tables are provided in a separate document. 

Syntax:  underscores_in_headers on | off;
 
Default:  underscores_in_headers off; 
Context:  http, server
 

Enables or disables the use of underscores in client request header fields. When the use of underscores is disabled, request header fields whose names contain underscores are marked as invalid and become subject to the ignore_invalid_headers directive. 

If the directive is specified on the server level, its value is only used if a server is a default one. The value specified also applies to all virtual servers listening on the same address and port. 

Syntax:  variables_hash_bucket_size size;
 
Default:  variables_hash_bucket_size 64; 
Context:  http
 

Sets the bucket size for the variables hash table. The details of setting up hash tables are provided in a separate document. 

Syntax:  variables_hash_max_size size;
 
Default:  variables_hash_max_size 1024; 
Context:  http
 

Sets the maximum size of the variables hash table. The details of setting up hash tables are provided in a separate document. 

Prior to version 1.5.13, the default value was 512. 

Embedded Variables
The ngx_http_core_module module supports embedded variables with names matching the Apache Server variables. First of all, these are variables representing client request header fields, such as $http_user_agent, $http_cookie, and so on. Also there are other variables: 

$arg_name
argument name in the request line 
$args
arguments in the request line 
$binary_remote_addr
client address in a binary form, value¡¯s length is always 4 bytes 
$body_bytes_sent
number of bytes sent to a client, not counting the response header; this variable is compatible with the ¡°%B¡± parameter of the mod_log_config Apache module 
$bytes_sent
number of bytes sent to a client (1.3.8, 1.2.5) 
$connection
connection serial number (1.3.8, 1.2.5) 
$connection_requests
current number of requests made through a connection (1.3.8, 1.2.5) 
$content_length
¡°Content-Length¡± request header field 
$content_type
¡°Content-Type¡± request header field 
$cookie_name
the name cookie 
$document_root
root or alias directive¡¯s value for the current request 
$document_uri
same as $uri 
$host
in this order of precedence: host name from the request line, or host name from the ¡°Host¡± request header field, or the server name matching a request 
$hostname
host name 
$http_name
arbitrary request header field; the last part of a variable name is the field name converted to lower case with dashes replaced by underscores 
$https
¡°on¡± if connection operates in SSL mode, or an empty string otherwise 
$is_args
¡°?¡± if a request line has arguments, or an empty string otherwise 
$limit_rate
setting this variable enables response rate limiting; see limit_rate 
$msec
current time in seconds with the milliseconds resolution (1.3.9, 1.2.6) 
$nginx_version
nginx version 
$pid
PID of the worker process 
$pipe
¡°p¡± if request was pipelined, ¡°.¡± otherwise (1.3.12, 1.2.7) 
$proxy_protocol_addr
client address from the PROXY protocol header, or an empty string otherwise (1.5.12) 
The PROXY protocol must be previously enabled by setting the proxy_protocol parameter in the listen directive. 

$query_string
same as $args 
$realpath_root
an absolute pathname corresponding to the root or alias directive¡¯s value for the current request, with all symbolic links resolved to real paths 
$remote_addr
client address 
$remote_port
client port 
$remote_user
user name supplied with the Basic authentication 
$request
full original request line 
$request_body
request body 
The variable¡¯s value is made available in locations processed by the proxy_pass, fastcgi_pass, uwsgi_pass, and scgi_pass directives. 

$request_body_file
name of a temporary file with the request body 
At the end of processing, the file needs to be removed. To always write the request body to a file, client_body_in_file_only needs to be enabled. When the name of a temporary file is passed in a proxied request or in a request to a FastCGI/uwsgi/SCGI server, passing the request body should be disabled by the proxy_pass_request_body off, fastcgi_pass_request_body off, uwsgi_pass_request_body off, or scgi_pass_request_body off directives, respectively. 

$request_completion
¡°OK¡± if a request has completed, or an empty string otherwise 
$request_filename
file path for the current request, based on the root or alias directives, and the request URI 
$request_length
request length (including request line, header, and request body) (1.3.12, 1.2.7) 
$request_method
request method, usually ¡°GET¡± or ¡°POST¡± 
$request_time
request processing time in seconds with a milliseconds resolution (1.3.9, 1.2.6); time elapsed since the first bytes were read from the client 
$request_uri
full original request URI (with arguments) 
$scheme
request scheme, ¡°http¡± or ¡°https¡± 
$sent_http_name
arbitrary response header field; the last part of a variable name is the field name converted to lower case with dashes replaced by underscores 
$server_addr
an address of the server which accepted a request 
Computing a value of this variable usually requires one system call. To avoid a system call, the listen directives must specify addresses and use the bind parameter. 

$server_name
name of the server which accepted a request 
$server_port
port of the server which accepted a request 
$server_protocol
request protocol, usually ¡°HTTP/1.0¡± or ¡°HTTP/1.1¡± 
$status
response status (1.3.2, 1.2.2) 
$tcpinfo_rtt, $tcpinfo_rttvar, $tcpinfo_snd_cwnd, $tcpinfo_rcv_space 
information about the client TCP connection; available on systems that support the TCP_INFO socket option 
$time_iso8601
local time in the ISO 8601 standard format (1.3.12, 1.2.7) 
$time_local
local time in the Common Log Format (1.3.12, 1.2.7) 
$uri
current URI in request, normalized 
The value of $uri may change during request processing, e.g. when doing internal redirects, or when using index files. 


Module ngx_http_upstream_module
Example Configuration
Directives
     upstream
     server
     zone
     hash
     ip_hash
     keepalive
     least_conn
     least_time
     health_check
     match
     queue
     sticky
     sticky_cookie_insert
Embedded Variables
 
The ngx_http_upstream_module module is used to define groups of servers that can be referenced by the proxy_pass, fastcgi_pass, uwsgi_pass, scgi_pass, and memcached_pass directives. 

Example Configuration

upstream backend {
    server backend1.example.com       weight=5;
    server backend2.example.com:8080;
    server unix:/tmp/backend3;

    server backup1.example.com:8080   backup;
    server backup2.example.com:8080   backup;
}

server {
    location / {
        proxy_pass http://backend;
    }
}

Dynamically configurable group, available as part of our commercial subscription: 

resolver 10.0.0.1;

upstream dynamic {
    zone upstream_dynamic 64k;

    server backend1.example.com      weight=5;
    server backend2.example.com:8080 fail_timeout=5s slow_start=30s;
    server 192.0.2.1                 max_fails=3;
    server backend3.example.com      resolve;

    server backup1.example.com:8080  backup;
    server backup2.example.com:8080  backup;
}

server {
    location / {
        proxy_pass http://dynamic;
        health_check;
    }
}

Directives
Syntax:  upstream name { ... }
 
Default:  ¡ª  
Context:  http
 

Defines a group of servers. Servers can listen on different ports. In addition, servers listening on TCP and UNIX-domain sockets can be mixed. 

Example: 

upstream backend {
    server backend1.example.com weight=5;
    server 127.0.0.1:8080       max_fails=3 fail_timeout=30s;
    server unix:/tmp/backend3;

    server backup1.example.com  backup;
}

By default, requests are distributed between the servers using a weighted round-robin balancing method. In the above example, each 7 requests will be distributed as follows: 5 requests go to backend1.example.com and one request to each of the second and third servers. If an error occurs during communication with a server, the request will be passed to the next server, and so on until all of the functioning servers will be tried. If a successful response could not be obtained from any of the servers, the client will receive the result of the communication with the last server. 

Syntax:  server address [parameters];
 
Default:  ¡ª  
Context:  upstream
 

Defines the address and other parameters of a server. The address can be specified as a domain name or IP address, with an optional port, or as a UNIX-domain socket path specified after the ¡°unix:¡± prefix. If a port is not specified, the port 80 is used. A domain name that resolves to several IP addresses defines multiple servers at once. 

The following parameters can be defined: 

weight=number 
sets the weight of the server, by default, 1. 
max_fails=number 
sets the number of unsuccessful attempts to communicate with the server that should happen in the duration set by the fail_timeout parameter to consider the server unavailable for a duration also set by the fail_timeout parameter. By default, the number of unsuccessful attempts is set to 1. The zero value disables the accounting of attempts. What is considered an unsuccessful attempt is defined by the proxy_next_upstream, fastcgi_next_upstream, uwsgi_next_upstream, scgi_next_upstream, and memcached_next_upstream directives. 
fail_timeout=time 
sets 
?the time during which the specified number of unsuccessful attempts to communicate with the server should happen to consider the server unavailable; 
?and the period of time the server will be considered unavailable. 
By default, the parameter is set to 10 seconds. 
backup 
marks the server as a backup server. It will be passed requests when the primary servers are unavailable. 
down 
marks the server as permanently unavailable. 

Additionally, the following parameters are available as part of our commercial subscription: 

max_conns=number 
limits the maximum number of simultaneous active connections to the proxied server (1.5.9). Default value is zero, meaning there is no limit. 
When keepalive connections and multiple workers are enabled, the total number of connections to the proxied server may exceed the max_conns value. 
resolve 
monitors changes of the IP addresses that correspond to a domain name of the server, and automatically modifies the upstream configuration without the need of restarting nginx (1.5.12). 
In order for this parameter to work, the resolver directive must be specified in the http block. Example: 

http {
    resolver 10.0.0.1;

    upstream u {
        zone ...;
        ...
        server example.com resolve;
    }
}

route=string 
sets the server route name. 
slow_start=time 
sets the time during which the server will recover its weight from zero to a nominal value, when unhealthy server becomes healthy, or when the server becomes available after a period of time it was considered unavailable. Default value is zero, i.e. slow start is disabled. 


If there is only a single server in a group, max_fails, fail_timeout and slow_start parameters are ignored, and such a server will never be considered unavailable. 

Syntax:  zone name [size];
 
Default:  ¡ª  
Context:  upstream
 

This directive appeared in version 1.9.0. 

Defines the name and size of the shared memory zone that keeps the group¡¯s configuration and run-time state that are shared between worker processes. Several groups may share the same zone. In this case, it is enough to specify the size only once. 

Additionally, as part of our commercial subscription, such groups allow changing the group membership or modifying the settings of a particular server without the need of restarting nginx. The configuration is accessible via a special location handled by upstream_conf. 

Syntax:  hash key [consistent];
 
Default:  ¡ª  
Context:  upstream
 

This directive appeared in version 1.7.2. 

Specifies a load balancing method for a server group where the client-server mapping is based on the hashed key value. The key can contain text, variables, and their combinations. Note that adding or removing a server from the group may result in remapping most of the keys to different servers. The method is compatible with the Cache::Memcached Perl library. 

If the consistent parameter is specified the ketama consistent hashing method will be used instead. The method ensures that only a few keys will be remapped to different servers when a server is added to or removed from the group. This helps to achieve a higher cache hit ratio for caching servers. The method is compatible with the Cache::Memcached::Fast Perl library with the ketama_points parameter set to 160. 

Syntax:  ip_hash;
 
Default:  ¡ª  
Context:  upstream
 

Specifies that a group should use a load balancing method where requests are distributed between servers based on client IP addresses. The first three octets of the client IPv4 address, or the entire IPv6 address, are used as a hashing key. The method ensures that requests from the same client will always be passed to the same server except when this server is unavailable. In the latter case client requests will be passed to another server. Most probably, it will always be the same server as well. 

IPv6 addresses are supported starting from versions 1.3.2 and 1.2.2. 

If one of the servers needs to be temporarily removed, it should be marked with the down parameter in order to preserve the current hashing of client IP addresses. 

Example: 

upstream backend {
    ip_hash;

    server backend1.example.com;
    server backend2.example.com;
    server backend3.example.com down;
    server backend4.example.com;
}


Until versions 1.3.1 and 1.2.2, it was not possible to specify a weight for servers using the ip_hash load balancing method. 

Syntax:  keepalive connections;
 
Default:  ¡ª  
Context:  upstream
 

This directive appeared in version 1.1.4. 

Activates the cache for connections to upstream servers. 

The connections parameter sets the maximum number of idle keepalive connections to upstream servers that are preserved in the cache of each worker process. When this number is exceeded, the least recently used connections are closed. 

It should be particularly noted that the keepalive directive does not limit the total number of connections to upstream servers that an nginx worker process can open. The connections parameter should be set to a number small enough to let upstream servers process new incoming connections as well. 

Example configuration of memcached upstream with keepalive connections: 

upstream memcached_backend {
    server 127.0.0.1:11211;
    server 10.0.0.2:11211;

    keepalive 32;
}

server {
    ...

    location /memcached/ {
        set $memcached_key $uri;
        memcached_pass memcached_backend;
    }

}

For HTTP, the proxy_http_version directive should be set to ¡°1.1¡± and the ¡°Connection¡± header field should be cleared: 

upstream http_backend {
    server 127.0.0.1:8080;

    keepalive 16;
}

server {
    ...

    location /http/ {
        proxy_pass http://http_backend;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        ...
    }
}


Alternatively, HTTP/1.0 persistent connections can be used by passing the ¡°Connection: Keep-Alive¡± header field to an upstream server, though this method is not recommended. 

For FastCGI servers, it is required to set fastcgi_keep_conn for keepalive connections to work: 

upstream fastcgi_backend {
    server 127.0.0.1:9000;

    keepalive 8;
}

server {
    ...

    location /fastcgi/ {
        fastcgi_pass fastcgi_backend;
        fastcgi_keep_conn on;
        ...
    }
}


When using load balancer methods other than the default round-robin method, it is necessary to activate them before the keepalive directive. 

SCGI and uwsgi protocols do not have a notion of keepalive connections. 

Syntax:  least_conn;
 
Default:  ¡ª  
Context:  upstream
 

This directive appeared in versions 1.3.1 and 1.2.2. 

Specifies that a group should use a load balancing method where a request is passed to the server with the least number of active connections, taking into account weights of servers. If there are several such servers, they are tried in turn using a weighted round-robin balancing method. 

Syntax:  least_time header | last_byte;
 
Default:  ¡ª  
Context:  upstream
 

This directive appeared in version 1.7.10. 

Specifies that a group should use a load balancing method where a request is passed to the server with the least average response time and least number of active connections, taking into account weights of servers. If there are several such servers, they are tried in turn using a weighted round-robin balancing method. 

If the header parameter is specified, time to receive the response header is used. If the last_byte parameter is specified, time to receive the full response is used. 


This directive is available as part of our commercial subscription. 

Syntax:  health_check [parameters];
 
Default:  ¡ª  
Context:  location
 

Enables periodic health checks of the servers in a group referenced in the surrounding location. 

The following optional parameters are supported: 

interval=time 
sets the interval between two consecutive health checks, by default, 5 seconds; 
fails=number 
sets the number of consecutive failed health checks of a particular server after which this server will be considered unhealthy, by default, 1; 
passes=number 
sets the number of consecutive passed health checks of a particular server after which the server will be considered healthy, by default, 1; 
uri=uri 
defines the URI used in health check requests, by default, ¡°/¡±; 
match=name 
specifies the match block configuring the tests that a response should pass in order for a health check to pass; by default, the response should have status code 2xx or 3xx. 

For example, 

location / {
    proxy_pass http://backend;
    health_check;
}
will send ¡°/¡± requests to each server in the backend group every five seconds. If any communication error or timeout occurs, or a proxied server responds with the status code other than 2xx or 3xx, the health check will fail, and the server will be considered unhealthy. Client requests are not passed to unhealthy servers. 

Health checks can be configured to test the status code of a response, presence of certain header fields and their values, and the body contents. Tests are configured separately using the match directive and referenced in the match parameter. For example: 

http {
    server {
    ...
        location / {
            proxy_pass http://backend;
            health_check match=welcome;
        }
    }

    match welcome {
        status 200;
        header Content-Type = text/html;
        body ~ "Welcome to nginx!";
    }
}
This configuration tells that for a health check to pass, the response to a health check request should succeed, have status 200, content type ¡°text/html¡±, and contain ¡°Welcome to nginx!¡± in the body. 

The server group must reside in the shared memory. 

If several health checks are defined for the same group of servers, a single failure of any check will make the corresponding server be considered unhealthy. 


Please note that most of the variables will have empty values when used with health checks. 


This directive is available as part of our commercial subscription. 

Syntax:  match name { ... }
 
Default:  ¡ª  
Context:  http
 

Defines the named test set used to verify responses to health check requests. 

The following items can be tested in a response: 

status 200;
status is 200
status ! 500;
status is not 500
status 200 204;
status is 200 or 204
status ! 301 302;
status is neither 301 nor 302
status 200-399;
status is in the range from 200 to 399
status ! 400-599;
status is not in the range from 400 to 599
status 301-303 307;
status is either 301, 302, 303, or 307

header Content-Type = text/html;
header contains ¡°Content-Type¡± with value text/html 
header Content-Type != text/html;
header contains ¡°Content-Type¡± with value other than text/html 
header Connection ~ close;
header contains ¡°Connection¡± with value matching regular expression close 
header Connection !~ close;
header contains ¡°Connection¡± with value not matching regular expression close 
header Host;
header contains ¡°Host¡±
header ! X-Accel-Redirect;
header lacks ¡°X-Accel-Redirect¡±

body ~ "Welcome to nginx!";
body matches regular expression ¡°Welcome to nginx!¡± 
body !~ "Welcome to nginx!";
body does not match regular expression ¡°Welcome to nginx!¡± 

If several tests are specified, the response matches only if it matches all tests. 

Only the first 256k of the response body are examined. 

Examples: 

# status is 200, content type is "text/html",
# and body contains "Welcome to nginx!"
match welcome {
    status 200;
    header Content-Type = text/html;
    body ~ "Welcome to nginx!";
}

# status is not one of 301, 302, 303, or 307, and header does not have "Refresh:"
match not_redirect {
    status ! 301-303 307;
    header ! Refresh;
}

# status ok and not in maintenance mode
match server_ok {
    status 200-399;
    body !~ "maintenance mode";
}


This directive is available as part of our commercial subscription. 

Syntax:  queue number [timeout=time];
 
Default:  ¡ª  
Context:  upstream
 

This directive appeared in version 1.5.12. 

If an upstream server cannot be selected immediately while processing a request, and there are the servers in the group that have reached the max_conns limit, the request will be placed into the queue. The directive specifies the maximum number of requests that can be in the queue at the same time. If the queue is filled up, or the server to pass the request to cannot be selected within the time period specified in the timeout parameter, an error will be returned to the client. 

The default value of the timeout parameter is 60 seconds. 


This directive is available as part of our commercial subscription. 

Syntax:  sticky cookie name [expires=time] [domain=domain] [httponly] [secure] [path=path];
sticky route $variable ...;
sticky learn create=$variable lookup=$variable zone=name:size [timeout=time];
 
Default:  ¡ª  
Context:  upstream
 

This directive appeared in version 1.5.7. 

Enables session affinity, which causes requests from the same client to be passed to the same server in a group of servers. Three methods are available: 

cookie
When the cookie method is used, information about the designated server is passed in an HTTP cookie generated by nginx: 

upstream backend {
    server backend1.example.com;
    server backend2.example.com;

    sticky cookie srv_id expires=1h domain=.example.com path=/;
}

A request that comes from a client not yet bound to a particular server is passed to the server selected by the configured balancing method. Further requests with this cookie will be passed to the designated server. If the designated server cannot process a request, the new server is selected as if the client has not been bound yet. 

The first parameter sets the name of the cookie to be set or inspected. Additional parameters may be as follows: 

expires=time
Sets the time for which a browser should keep the cookie. The special value max will cause the cookie to expire on ¡°31 Dec 2037 23:55:55 GMT¡±. If the parameter is not specified, it will cause the cookie to expire at the end of a browser session. 
domain=domain
Defines the domain for which the cookie is set. 
httponly
Adds the HttpOnly attribute to the cookie (1.7.11). 
secure
Adds the Secure attribute to the cookie (1.7.11). 
path=path
Defines the path for which the cookie is set. 
If any parameters are omitted, the corresponding cookie fields are not set. 

route
When the route method is used, proxied server assigns client a route on receipt of the first request. All subsequent requests from this client will carry routing information in a cookie or URI. This information is compared with the ¡°route¡± parameter of the server directive to identify the server to which the request should be proxied. If the designated server cannot process a request, the new server is selected by the configured balancing method as if there is no routing information in the request. 

The parameters of the route method specify variables that may contain routing information. The first non-empty variable is used to find the matching server. 

Example: 

map $cookie_jsessionid $route_cookie {
    ~.+\.(?P<route>\w+)$ $route;
}

map $request_uri $route_uri {
    ~jsessionid=.+\.(?P<route>\w+)$ $route;
}

upstream backend {
    server backend1.example.com route=a;
    server backend2.example.com route=b;

    sticky route $route_cookie $route_uri;
}
Here, the route is taken from the ¡°JSESSIONID¡± cookie if present in a request. Otherwise, the route from the URI is used. 

learn
When the learn method (1.7.1) is used, nginx analyzes upstream server responses and learns server-initiated sessions usually passed in an HTTP cookie. 

upstream backend {
   server backend1.example.com:8080;
   server backend2.example.com:8081;

   sticky learn
          create=$upstream_cookie_examplecookie
          lookup=$cookie_examplecookie
          zone=client_sessions:1m;
}
In the example, the upstream server creates a session by setting the cookie ¡°EXAMPLECOOKIE¡± in the response. Further requests with this cookie will be passed to the same server. If the server cannot process the request, the new server is selected as if the client has not been bound yet. 

The parameters create and lookup specify variables that indicate how new sessions are created and existing sessions are searched, respectively. Both parameters may be specified more than once, in which case the first non-empty variable is used. 

Sessions are stored in a shared memory zone, whose name and size are configured by the zone parameter. One megabyte zone can store about 8000 sessions on the 64-bit platform. The sessions that are not accessed during the time specified by the timeout parameter get removed from the zone. By default, timeout is set to 10 minutes. 



This directive is available as part of our commercial subscription. 

Syntax:  sticky_cookie_insert name [expires=time] [domain=domain] [path=path];
 
Default:  ¡ª  
Context:  upstream
 

This directive is obsolete since version 1.5.7. An equivalent sticky directive with a new syntax should be used instead: 

sticky cookie name [expires=time] [domain=domain] [path=path]; 

Embedded Variables
The ngx_http_upstream_module module supports the following embedded variables: 

$upstream_addr
keeps the IP address and port, or the path to the UNIX-domain socket of the upstream server. If several servers were contacted during request processing, their addresses are separated by commas, e.g. ¡°192.168.1.1:80, 192.168.1.2:80, unix:/tmp/sock¡±. If an internal redirect from one server group to another happens, initiated by ¡°X-Accel-Redirect¡± or error_page, then the server addresses from different groups are separated by colons, e.g. ¡°192.168.1.1:80, 192.168.1.2:80, unix:/tmp/sock : 192.168.10.1:80, 192.168.10.2:80¡±. 
$upstream_cache_status 
keeps the status of accessing a response cache (0.8.3). The status can be either ¡°MISS¡±, ¡°BYPASS¡±, ¡°EXPIRED¡±, ¡°STALE¡±, ¡°UPDATING¡±, ¡°REVALIDATED¡±, or ¡°HIT¡±. 
$upstream_connect_time 
keeps time spent on establishing a connection with the upstream server (1.9.1); the time is kept in seconds with millisecond resolution. In case of SSL, includes time spent on handshake. Times of several connections are separated by commas and colons like addresses in the $upstream_addr variable. 
$upstream_cookie_name 
cookie with the specified name sent by the upstream server in the ¡°Set-Cookie¡± response header field (1.7.1). Only the cookies from the response of the last server are saved. 
$upstream_header_time 
keeps time spent on receiving the response header from the upstream server (1.7.10); the time is kept in seconds with millisecond resolution. Times of several responses are separated by commas and colons like addresses in the $upstream_addr variable. 
$upstream_http_name
keep server response header fields. For example, the ¡°Server¡± response header field is available through the $upstream_http_server variable. The rules of converting header field names to variable names are the same as for the variables that start with the ¡°$http_¡± prefix. Only the header fields from the response of the last server are saved. 
$upstream_response_length 
keeps the length of the response obtained from the upstream server (0.7.27); the length is kept in bytes. Lengths of several responses are separated by commas and colons like addresses in the $upstream_addr variable. 
$upstream_response_time 
keeps time spent on receiving the response from the upstream server; the time is kept in seconds with millisecond resolution. Times of several responses are separated by commas and colons like addresses in the $upstream_addr variable. 
$upstream_status
keeps status code of the response obtained from the upstream server. Status codes of several responses are separated by commas and colons like addresses in the $upstream_addr variable. 





Module ngx_http_proxy_module
Example Configuration
Directives
     proxy_bind
     proxy_buffer_size
     proxy_buffering
     proxy_buffers
     proxy_busy_buffers_size
     proxy_cache
     proxy_cache_bypass
     proxy_cache_key
     proxy_cache_lock
     proxy_cache_lock_age
     proxy_cache_lock_timeout
     proxy_cache_methods
     proxy_cache_min_uses
     proxy_cache_path
     proxy_cache_purge
     proxy_cache_revalidate
     proxy_cache_use_stale
     proxy_cache_valid
     proxy_connect_timeout
     proxy_cookie_domain
     proxy_cookie_path
     proxy_force_ranges
     proxy_headers_hash_bucket_size
     proxy_headers_hash_max_size
     proxy_hide_header
     proxy_http_version
     proxy_ignore_client_abort
     proxy_ignore_headers
     proxy_intercept_errors
     proxy_limit_rate
     proxy_max_temp_file_size
     proxy_method
     proxy_next_upstream
     proxy_next_upstream_timeout
     proxy_next_upstream_tries
     proxy_no_cache
     proxy_pass
     proxy_pass_header
     proxy_pass_request_body
     proxy_pass_request_headers
     proxy_read_timeout
     proxy_redirect
     proxy_request_buffering
     proxy_send_lowat
     proxy_send_timeout
     proxy_set_body
     proxy_set_header
     proxy_ssl_certificate
     proxy_ssl_certificate_key
     proxy_ssl_ciphers
     proxy_ssl_crl
     proxy_ssl_name
     proxy_ssl_password_file
     proxy_ssl_server_name
     proxy_ssl_session_reuse
     proxy_ssl_protocols
     proxy_ssl_trusted_certificate
     proxy_ssl_verify
     proxy_ssl_verify_depth
     proxy_store
     proxy_store_access
     proxy_temp_file_write_size
     proxy_temp_path
Embedded Variables
 
The ngx_http_proxy_module module allows passing requests to another server. 

Example Configuration

location / {
    proxy_pass       http://localhost:8000;
    proxy_set_header Host      $host;
    proxy_set_header X-Real-IP $remote_addr;
}

Directives
Syntax:  proxy_bind address | off;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 0.8.22. 

Makes outgoing connections to a proxied server originate from the specified local IP address. Parameter value can contain variables (1.3.12). The special value off (1.3.12) cancels the effect of the proxy_bind directive inherited from the previous configuration level, which allows the system to auto-assign the local IP address. 

Syntax:  proxy_buffer_size size;
 
Default:  proxy_buffer_size 4k|8k; 
Context:  http, server, location
 

Sets the size of the buffer used for reading the first part of the response received from the proxied server. This part usually contains a small response header. By default, the buffer size is equal to one memory page. This is either 4K or 8K, depending on a platform. It can be made smaller, however. 

Syntax:  proxy_buffering on | off;
 
Default:  proxy_buffering on; 
Context:  http, server, location
 

Enables or disables buffering of responses from the proxied server. 

When buffering is enabled, nginx receives a response from the proxied server as soon as possible, saving it into the buffers set by the proxy_buffer_size and proxy_buffers directives. If the whole response does not fit into memory, a part of it can be saved to a temporary file on the disk. Writing to temporary files is controlled by the proxy_max_temp_file_size and proxy_temp_file_write_size directives. 

When buffering is disabled, the response is passed to a client synchronously, immediately as it is received. nginx will not try to read the whole response from the proxied server. The maximum size of the data that nginx can receive from the server at a time is set by the proxy_buffer_size directive. 

Buffering can also be enabled or disabled by passing ¡°yes¡± or ¡°no¡± in the ¡°X-Accel-Buffering¡± response header field. This capability can be disabled using the proxy_ignore_headers directive. 

Syntax:  proxy_buffers number size;
 
Default:  proxy_buffers 8 4k|8k; 
Context:  http, server, location
 

Sets the number and size of the buffers used for reading a response from the proxied server, for a single connection. By default, the buffer size is equal to one memory page. This is either 4K or 8K, depending on a platform. 

Syntax:  proxy_busy_buffers_size size;
 
Default:  proxy_busy_buffers_size 8k|16k; 
Context:  http, server, location
 

When buffering of responses from the proxied server is enabled, limits the total size of buffers that can be busy sending a response to the client while the response is not yet fully read. In the meantime, the rest of the buffers can be used for reading the response and, if needed, buffering part of the response to a temporary file. By default, size is limited by the size of two buffers set by the proxy_buffer_size and proxy_buffers directives. 

Syntax:  proxy_cache zone | off;
 
Default:  proxy_cache off; 
Context:  http, server, location
 

Defines a shared memory zone used for caching. The same zone can be used in several places. Parameter value can contain variables (1.7.9). The off parameter disables caching inherited from the previous configuration level. 

Syntax:  proxy_cache_bypass string ...;
 
Default:  ¡ª  
Context:  http, server, location
 

Defines conditions under which the response will not be taken from a cache. If at least one value of the string parameters is not empty and is not equal to ¡°0¡± then the response will not be taken from the cache: 

proxy_cache_bypass $cookie_nocache $arg_nocache$arg_comment;
proxy_cache_bypass $http_pragma    $http_authorization;
Can be used along with the proxy_no_cache directive. 

Syntax:  proxy_cache_key string;
 
Default:  proxy_cache_key $scheme$proxy_host$request_uri; 
Context:  http, server, location
 

Defines a key for caching, for example 

proxy_cache_key "$host$request_uri $cookie_user";
By default, the directive¡¯s value is close to the string 

proxy_cache_key $scheme$proxy_host$uri$is_args$args;

Syntax:  proxy_cache_lock on | off;
 
Default:  proxy_cache_lock off; 
Context:  http, server, location
 

This directive appeared in version 1.1.12. 

When enabled, only one request at a time will be allowed to populate a new cache element identified according to the proxy_cache_key directive by passing a request to a proxied server. Other requests of the same cache element will either wait for a response to appear in the cache or the cache lock for this element to be released, up to the time set by the proxy_cache_lock_timeout directive. 

Syntax:  proxy_cache_lock_age time;
 
Default:  proxy_cache_lock_age 5s; 
Context:  http, server, location
 

This directive appeared in version 1.7.8. 

If the last request passed to the proxied server for populating a new cache element has not completed for the specified time, one more request may be passed to the proxied server. 

Syntax:  proxy_cache_lock_timeout time;
 
Default:  proxy_cache_lock_timeout 5s; 
Context:  http, server, location
 

This directive appeared in version 1.1.12. 

Sets a timeout for proxy_cache_lock. When the time expires, the request will be passed to the proxied server, however, the response will not be cached. 

Before 1.7.8, the response could be cached. 

Syntax:  proxy_cache_methods GET | HEAD | POST ...;
 
Default:  proxy_cache_methods GET HEAD; 
Context:  http, server, location
 

This directive appeared in version 0.7.59. 

If the client request method is listed in this directive then the response will be cached. ¡°GET¡± and ¡°HEAD¡± methods are always added to the list, though it is recommended to specify them explicitly. See also the proxy_no_cache directive. 

Syntax:  proxy_cache_min_uses number;
 
Default:  proxy_cache_min_uses 1; 
Context:  http, server, location
 

Sets the number of requests after which the response will be cached. 

Syntax:  proxy_cache_path path [levels=levels] [use_temp_path=on|off] keys_zone=name:size [inactive=time] [max_size=size] [loader_files=number] [loader_sleep=time] [loader_threshold=time];
 
Default:  ¡ª  
Context:  http
 

Sets the path and other parameters of a cache. Cache data are stored in files. The file name in a cache is a result of applying the MD5 function to the cache key. The levels parameter defines hierarchy levels of a cache. For example, in the following configuration 

proxy_cache_path /data/nginx/cache levels=1:2 keys_zone=one:10m;
file names in a cache will look like this: 

/data/nginx/cache/c/29/b7f54b2df7773722d382f4809d65029c

A cached response is first written to a temporary file, and then the file is renamed. Starting from version 0.8.9, temporary files and the cache can be put on different file systems. However, be aware that in this case a file is copied across two file systems instead of the cheap renaming operation. It is thus recommended that for any given location both cache and a directory holding temporary files are put on the same file system. The directory for temporary files is set based on the use_temp_path parameter (1.7.10). If this parameter is omitted or set to the value on, the directory set by the proxy_temp_path directive for the given location will be used. If the value is set to off, temporary files will be put directly in the cache directory. 

In addition, all active keys and information about data are stored in a shared memory zone, whose name and size are configured by the keys_zone parameter. One megabyte zone can store about 8 thousand keys. 

Cached data that are not accessed during the time specified by the inactive parameter get removed from the cache regardless of their freshness. By default, inactive is set to 10 minutes. 

The special ¡°cache manager¡± process monitors the maximum cache size set by the max_size parameter. When this size is exceeded, it removes the least recently used data. 

A minute after the start the special ¡°cache loader¡± process is activated. It loads information about previously cached data stored on file system into a cache zone. The loading is done in iterations. During one iteration no more than loader_files items are loaded (by default, 100). Besides, the duration of one iteration is limited by the loader_threshold parameter (by default, 200 milliseconds). Between iterations, a pause configured by the loader_sleep parameter (by default, 50 milliseconds) is made. 

Syntax:  proxy_cache_purge string ...;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 1.5.7. 

Defines conditions under which the request will be considered a cache purge request. If at least one value of the string parameters is not empty and is not equal to ¡°0¡± then the cache entry with a corresponding cache key is removed. The result of successful operation is indicated by returning the 204 (No Content) response. 

If the cache key of a purge request ends with an asterisk (¡°*¡±), all cache entries matching the wildcard key will be removed from the cache. 

Example configuration: 

proxy_cache_path /data/nginx/cache keys_zone=cache_zone:10m;

map $request_method $purge_method {
    PURGE   1;
    default 0;
}

server {
    ...
    location / {
        proxy_pass http://backend;
        proxy_cache cache_zone;
        proxy_cache_key $uri;
        proxy_cache_purge $purge_method;
    }
}

This functionality is available as part of our commercial subscription. 

Syntax:  proxy_cache_revalidate on | off;
 
Default:  proxy_cache_revalidate off; 
Context:  http, server, location
 

This directive appeared in version 1.5.7. 

Enables revalidation of expired cache items using conditional requests with the ¡°If-Modified-Since¡± and ¡°If-None-Match¡± header fields. 

Syntax:  proxy_cache_use_stale error | timeout | invalid_header | updating | http_500 | http_502 | http_503 | http_504 | http_403 | http_404 | off ...;
 
Default:  proxy_cache_use_stale off; 
Context:  http, server, location
 

Determines in which cases a stale cached response can be used when an error occurs during communication with the proxied server. The directive¡¯s parameters match the parameters of the proxy_next_upstream directive. 

The error parameter also permits using a stale cached response if a proxied server to process a request cannot be selected. 

Additionally, the updating parameter permits using a stale cached response if it is currently being updated. This allows minimizing the number of accesses to proxied servers when updating cached data. 

To minimize the number of accesses to proxied servers when populating a new cache element, the proxy_cache_lock directive can be used. 

Syntax:  proxy_cache_valid [code ...] time;
 
Default:  ¡ª  
Context:  http, server, location
 

Sets caching time for different response codes. For example, the following directives 

proxy_cache_valid 200 302 10m;
proxy_cache_valid 404      1m;
set 10 minutes of caching for responses with codes 200 and 302 and 1 minute for responses with code 404. 

If only caching time is specified 

proxy_cache_valid 5m;
then only 200, 301, and 302 responses are cached. 

In addition, the any parameter can be specified to cache any responses: 

proxy_cache_valid 200 302 10m;
proxy_cache_valid 301      1h;
proxy_cache_valid any      1m;

Parameters of caching can also be set directly in the response header. This has higher priority than setting of caching time using the directive. 

?The ¡°X-Accel-Expires¡± header field sets caching time of a response in seconds. The zero value disables caching for a response. If the value starts with the @ prefix, it sets an absolute time in seconds since Epoch, up to which the response may be cached. 
?If the header does not include the ¡°X-Accel-Expires¡± field, parameters of caching may be set in the header fields ¡°Expires¡± or ¡°Cache-Control¡±. 
?If the header includes the ¡°Set-Cookie¡± field, such a response will not be cached. 
?If the header includes the ¡°Vary¡± field with the special value ¡°*¡±, such a response will not be cached (1.7.7). If the header includes the ¡°Vary¡± field with another value, such a response will be cached taking into account the corresponding request header fields (1.7.7). 
Processing of one or more of these response header fields can be disabled using the proxy_ignore_headers directive. 

Syntax:  proxy_connect_timeout time;
 
Default:  proxy_connect_timeout 60s; 
Context:  http, server, location
 

Defines a timeout for establishing a connection with a proxied server. It should be noted that this timeout cannot usually exceed 75 seconds. 

Syntax:  proxy_cookie_domain off;
proxy_cookie_domain domain replacement;
 
Default:  proxy_cookie_domain off; 
Context:  http, server, location
 

This directive appeared in version 1.1.15. 

Sets a text that should be changed in the domain attribute of the ¡°Set-Cookie¡± header fields of a proxied server response. Suppose a proxied server returned the ¡°Set-Cookie¡± header field with the attribute ¡°domain=localhost¡±. The directive 

proxy_cookie_domain localhost example.org;
will rewrite this attribute to ¡°domain=example.org¡±. 

A dot at the beginning of the domain and replacement strings and the domain attribute is ignored. Matching is case-insensitive. 

The domain and replacement strings can contain variables: 

proxy_cookie_domain www.$host $host;

The directive can also be specified using regular expressions. In this case, domain should start from the ¡°~¡± symbol. A regular expression can contain named and positional captures, and replacement can reference them: 

proxy_cookie_domain ~\.(?P<sl_domain>[-0-9a-z]+\.[a-z]+)$ $sl_domain;

There could be several proxy_cookie_domain directives: 

proxy_cookie_domain localhost example.org;
proxy_cookie_domain ~\.([a-z]+\.[a-z]+)$ $1;

The off parameter cancels the effect of all proxy_cookie_domain directives on the current level: 

proxy_cookie_domain off;
proxy_cookie_domain localhost example.org;
proxy_cookie_domain www.example.org example.org;

Syntax:  proxy_cookie_path off;
proxy_cookie_path path replacement;
 
Default:  proxy_cookie_path off; 
Context:  http, server, location
 

This directive appeared in version 1.1.15. 

Sets a text that should be changed in the path attribute of the ¡°Set-Cookie¡± header fields of a proxied server response. Suppose a proxied server returned the ¡°Set-Cookie¡± header field with the attribute ¡°path=/two/some/uri/¡±. The directive 

proxy_cookie_path /two/ /;
will rewrite this attribute to ¡°path=/some/uri/¡±. 

The path and replacement strings can contain variables: 

proxy_cookie_path $uri /some$uri;

The directive can also be specified using regular expressions. In this case, path should either start from the ¡°~¡± symbol for a case-sensitive matching, or from the ¡°~*¡± symbols for case-insensitive matching. The regular expression can contain named and positional captures, and replacement can reference them: 

proxy_cookie_path ~*^/user/([^/]+) /u/$1;

There could be several proxy_cookie_path directives: 

proxy_cookie_path /one/ /;
proxy_cookie_path / /two/;

The off parameter cancels the effect of all proxy_cookie_path directives on the current level: 

proxy_cookie_path off;
proxy_cookie_path /two/ /;
proxy_cookie_path ~*^/user/([^/]+) /u/$1;

Syntax:  proxy_force_ranges on | off;
 
Default:  proxy_force_ranges off; 
Context:  http, server, location
 

This directive appeared in version 1.7.7. 

Enables byte-range support for both cached and uncached responses from the proxied server regardless of the ¡°Accept-Ranges¡± field in these responses. 

Syntax:  proxy_headers_hash_bucket_size size;
 
Default:  proxy_headers_hash_bucket_size 64; 
Context:  http, server, location
 

Sets the bucket size for hash tables used by the proxy_hide_header and proxy_set_header directives. The details of setting up hash tables are provided in a separate document. 

Syntax:  proxy_headers_hash_max_size size;
 
Default:  proxy_headers_hash_max_size 512; 
Context:  http, server, location
 

Sets the maximum size of hash tables used by the proxy_hide_header and proxy_set_header directives. The details of setting up hash tables are provided in a separate document. 

Syntax:  proxy_hide_header field;
 
Default:  ¡ª  
Context:  http, server, location
 

By default, nginx does not pass the header fields ¡°Date¡±, ¡°Server¡±, ¡°X-Pad¡±, and ¡°X-Accel-...¡± from the response of a proxied server to a client. The proxy_hide_header directive sets additional fields that will not be passed. If, on the contrary, the passing of fields needs to be permitted, the proxy_pass_header directive can be used. 

Syntax:  proxy_http_version 1.0 | 1.1;
 
Default:  proxy_http_version 1.0; 
Context:  http, server, location
 

This directive appeared in version 1.1.4. 

Sets the HTTP protocol version for proxying. By default, version 1.0 is used. Version 1.1 is recommended for use with keepalive connections. 

Syntax:  proxy_ignore_client_abort on | off;
 
Default:  proxy_ignore_client_abort off; 
Context:  http, server, location
 

Determines whether the connection with a proxied server should be closed when a client closes the connection without waiting for a response. 

Syntax:  proxy_ignore_headers field ...;
 
Default:  ¡ª  
Context:  http, server, location
 

Disables processing of certain response header fields from the proxied server. The following fields can be ignored: ¡°X-Accel-Redirect¡±, ¡°X-Accel-Expires¡±, ¡°X-Accel-Limit-Rate¡± (1.1.6), ¡°X-Accel-Buffering¡± (1.1.6), ¡°X-Accel-Charset¡± (1.1.6), ¡°Expires¡±, ¡°Cache-Control¡±, ¡°Set-Cookie¡± (0.8.44), and ¡°Vary¡± (1.7.7). 

If not disabled, processing of these header fields has the following effect: 

?¡°X-Accel-Expires¡±, ¡°Expires¡±, ¡°Cache-Control¡±, ¡°Set-Cookie¡±, and ¡°Vary¡± set the parameters of response caching; 
?¡°X-Accel-Redirect¡± performs an internal redirect to the specified URI; 
?¡°X-Accel-Limit-Rate¡± sets the rate limit for transmission of a response to a client; 
?¡°X-Accel-Buffering¡± enables or disables buffering of a response; 
?¡°X-Accel-Charset¡± sets the desired charset of a response. 

Syntax:  proxy_intercept_errors on | off;
 
Default:  proxy_intercept_errors off; 
Context:  http, server, location
 

Determines whether proxied responses with codes greater than or equal to 300 should be passed to a client or be redirected to nginx for processing with the error_page directive. 

Syntax:  proxy_limit_rate rate;
 
Default:  proxy_limit_rate 0; 
Context:  http, server, location
 

This directive appeared in version 1.7.7. 

Limits the speed of reading the response from the proxied server. The rate is specified in bytes per second. The zero value disables rate limiting. The limit is set per a request, and so if nginx simultaneously opens two connections to the proxied server, the overall rate will be twice as much as the specified limit. The limitation works only if buffering of responses from the proxied server is enabled. 

Syntax:  proxy_max_temp_file_size size;
 
Default:  proxy_max_temp_file_size 1024m; 
Context:  http, server, location
 

When buffering of responses from the proxied server is enabled, and the whole response does not fit into the buffers set by the proxy_buffer_size and proxy_buffers directives, a part of the response can be saved to a temporary file. This directive sets the maximum size of the temporary file. The size of data written to the temporary file at a time is set by the proxy_temp_file_write_size directive. 

The zero value disables buffering of responses to temporary files. 


This restriction does not apply to responses that will be cached or stored on disk. 

Syntax:  proxy_method method;
 
Default:  ¡ª  
Context:  http, server, location
 

Specifies the HTTP method to use in requests forwarded to the proxied server instead of the method from the client request. 

Syntax:  proxy_next_upstream error | timeout | invalid_header | http_500 | http_502 | http_503 | http_504 | http_403 | http_404 | off ...;
 
Default:  proxy_next_upstream error timeout; 
Context:  http, server, location
 

Specifies in which cases a request should be passed to the next server: 

error
an error occurred while establishing a connection with the server, passing a request to it, or reading the response header;
timeout
a timeout has occurred while establishing a connection with the server, passing a request to it, or reading the response header;
invalid_header
a server returned an empty or invalid response;
http_500
a server returned a response with the code 500;
http_502
a server returned a response with the code 502;
http_503
a server returned a response with the code 503;
http_504
a server returned a response with the code 504;
http_403
a server returned a response with the code 403;
http_404
a server returned a response with the code 404;
off
disables passing a request to the next server.

One should bear in mind that passing a request to the next server is only possible if nothing has been sent to a client yet. That is, if an error or timeout occurs in the middle of the transferring of a response, fixing this is impossible. 

The directive also defines what is considered an unsuccessful attempt of communication with a server. The cases of error, timeout and invalid_header are always considered unsuccessful attempts, even if they are not specified in the directive. The cases of http_500, http_502, http_503 and http_504 are considered unsuccessful attempts only if they are specified in the directive. The cases of http_403 and http_404 are never considered unsuccessful attempts. 

Passing a request to the next server can be limited by the number of tries and by time. 

Syntax:  proxy_next_upstream_timeout time;
 
Default:  proxy_next_upstream_timeout 0; 
Context:  http, server, location
 

This directive appeared in version 1.7.5. 

Limits the time allowed to pass a request to the next server. The 0 value turns off this limitation. 

Syntax:  proxy_next_upstream_tries number;
 
Default:  proxy_next_upstream_tries 0; 
Context:  http, server, location
 

This directive appeared in version 1.7.5. 

Limits the number of possible tries for passing a request to the next server. The 0 value turns off this limitation. 

Syntax:  proxy_no_cache string ...;
 
Default:  ¡ª  
Context:  http, server, location
 

Defines conditions under which the response will not be saved to a cache. If at least one value of the string parameters is not empty and is not equal to ¡°0¡± then the response will not be saved: 

proxy_no_cache $cookie_nocache $arg_nocache$arg_comment;
proxy_no_cache $http_pragma    $http_authorization;
Can be used along with the proxy_cache_bypass directive. 

Syntax:  proxy_pass URL;
 
Default:  ¡ª  
Context:  location, if in location, limit_except
 

Sets the protocol and address of a proxied server and an optional URI to which a location should be mapped. As a protocol, ¡°http¡± or ¡°https¡± can be specified. The address can be specified as a domain name or IP address, and an optional port: 

proxy_pass http://localhost:8000/uri/;
or as a UNIX-domain socket path specified after the word ¡°unix¡± and enclosed in colons: 

proxy_pass http://unix:/tmp/backend.socket:/uri/;

If a domain name resolves to several addresses, all of them will be used in a round-robin fashion. In addition, an address can be specified as a server group. 

A request URI is passed to the server as follows: 

?If the proxy_pass directive is specified with a URI, then when a request is passed to the server, the part of a normalized request URI matching the location is replaced by a URI specified in the directive: 
location /name/ {
    proxy_pass http://127.0.0.1/remote/;
}
?If proxy_pass is specified without a URI, the request URI is passed to the server in the same form as sent by a client when the original request is processed, or the full normalized request URI is passed when processing the changed URI: 
location /some/path/ {
    proxy_pass http://127.0.0.1;
}
Before version 1.1.12, if proxy_pass is specified without a URI, the original request URI might be passed instead of the changed URI in some cases. 

In some cases, the part of a request URI to be replaced cannot be determined: 

?When location is specified using a regular expression. 
In this case, the directive should be specified without a URI. 

?When the URI is changed inside a proxied location using the rewrite directive, and this same configuration will be used to process a request (break): 
location /name/ {
    rewrite    /name/([^/]+) /users?name=$1 break;
    proxy_pass http://127.0.0.1;
}
In this case, the URI specified in the directive is ignored and the full changed request URI is passed to the server. 


A server name, its port and the passed URI can also be specified using variables: 

proxy_pass http://$host$uri;
or even like this: 

proxy_pass $request;

In this case, the server name is searched among the described server groups, and, if not found, is determined using a resolver. 

WebSocket proxying requires special configuration and is supported since version 1.3.13. 

Syntax:  proxy_pass_header field;
 
Default:  ¡ª  
Context:  http, server, location
 

Permits passing otherwise disabled header fields from a proxied server to a client. 

Syntax:  proxy_pass_request_body on | off;
 
Default:  proxy_pass_request_body on; 
Context:  http, server, location
 

Indicates whether the original request body is passed to the proxied server. 

location /x-accel-redirect-here/ {
    proxy_method GET;
    proxy_pass_request_body off;
    proxy_set_header Content-Length "";

    proxy_pass ...
}
See also the proxy_set_header and proxy_pass_request_headers directives. 

Syntax:  proxy_pass_request_headers on | off;
 
Default:  proxy_pass_request_headers on; 
Context:  http, server, location
 

Indicates whether the header fields of the original request are passed to the proxied server. 

location /x-accel-redirect-here/ {
    proxy_method GET;
    proxy_pass_request_headers off;
    proxy_pass_request_body off;

    proxy_pass ...
}
See also the proxy_set_header and proxy_pass_request_body directives. 

Syntax:  proxy_read_timeout time;
 
Default:  proxy_read_timeout 60s; 
Context:  http, server, location
 

Defines a timeout for reading a response from the proxied server. The timeout is set only between two successive read operations, not for the transmission of the whole response. If the proxied server does not transmit anything within this time, the connection is closed. 

Syntax:  proxy_redirect default;
proxy_redirect off;
proxy_redirect redirect replacement;
 
Default:  proxy_redirect default; 
Context:  http, server, location
 

Sets the text that should be changed in the ¡°Location¡± and ¡°Refresh¡± header fields of a proxied server response. Suppose a proxied server returned the header field ¡°Location: http://localhost:8000/two/some/uri/¡±. The directive 

proxy_redirect http://localhost:8000/two/ http://frontend/one/;
will rewrite this string to ¡°Location: http://frontend/one/some/uri/¡±. 

A server name may be omitted in the replacement string: 

proxy_redirect http://localhost:8000/two/ /;
then the primary server¡¯s name and port, if different from 80, will be inserted. 

The default replacement specified by the default parameter uses the parameters of the location and proxy_pass directives. Hence, the two configurations below are equivalent: 

location /one/ {
    proxy_pass     http://upstream:port/two/;
    proxy_redirect default;

location /one/ {
    proxy_pass     http://upstream:port/two/;
    proxy_redirect http://upstream:port/two/ /one/;
The default parameter is not permitted if proxy_pass is specified using variables. 

A replacement string can contain variables: 

proxy_redirect http://localhost:8000/ http://$host:$server_port/;

A redirect can also contain (1.1.11) variables: 

proxy_redirect http://$proxy_host:8000/ /;

The directive can be specified (1.1.11) using regular expressions. In this case, redirect should either start with the ¡°~¡± symbol for a case-sensitive matching, or with the ¡°~*¡± symbols for case-insensitive matching. The regular expression can contain named and positional captures, and replacement can reference them: 

proxy_redirect ~^(http://[^:]+):\d+(/.+)$ $1$2;
proxy_redirect ~* /user/([^/]+)/(.+)$      http://$1.example.com/$2;

There could be several proxy_redirect directives: 

proxy_redirect default;
proxy_redirect http://localhost:8000/  /;
proxy_redirect http://www.example.com/ /;

The off parameter cancels the effect of all proxy_redirect directives on the current level: 

proxy_redirect off;
proxy_redirect default;
proxy_redirect http://localhost:8000/  /;
proxy_redirect http://www.example.com/ /;

Using this directive, it is also possible to add host names to relative redirects issued by a proxied server: 

proxy_redirect / /;

Syntax:  proxy_request_buffering on | off;
 
Default:  proxy_request_buffering on; 
Context:  http, server, location
 

This directive appeared in version 1.7.11. 

Enables or disables buffering of a client request body. 

When buffering is enabled, the entire request body is read from the client before sending the request to a proxied server. 

When buffering is disabled, the request body is sent to the proxied server immediately as it is received. In this case, the request cannot be passed to the next server if nginx already started sending the request body. 

When HTTP/1.1 chunked transfer encoding is used to send the original request body, the request body will be buffered regardless of the directive value unless HTTP/1.1 is enabled for proxying. 

Syntax:  proxy_send_lowat size;
 
Default:  proxy_send_lowat 0; 
Context:  http, server, location
 

If the directive is set to a non-zero value, nginx will try to minimize the number of send operations on outgoing connections to a proxied server by using either NOTE_LOWAT flag of the kqueue method, or the SO_SNDLOWAT socket option, with the specified size. 

This directive is ignored on Linux, Solaris, and Windows. 

Syntax:  proxy_send_timeout time;
 
Default:  proxy_send_timeout 60s; 
Context:  http, server, location
 

Sets a timeout for transmitting a request to the proxied server. The timeout is set only between two successive write operations, not for the transmission of the whole request. If the proxied server does not receive anything within this time, the connection is closed. 

Syntax:  proxy_set_body value;
 
Default:  ¡ª  
Context:  http, server, location
 

Allows redefining the request body passed to the proxied server. The value can contain text, variables, and their combination. 

Syntax:  proxy_set_header field value;
 
Default:  proxy_set_header Host $proxy_host;proxy_set_header Connection close; 
Context:  http, server, location
 

Allows redefining or appending fields to the request header passed to the proxied server. The value can contain text, variables, and their combinations. These directives are inherited from the previous level if and only if there are no proxy_set_header directives defined on the current level. By default, only two fields are redefined: 

proxy_set_header Host       $proxy_host;
proxy_set_header Connection close;

An unchanged ¡°Host¡± request header field can be passed like this: 

proxy_set_header Host       $http_host;

However, if this field is not present in a client request header then nothing will be passed. In such a case it is better to use the $host variable - its value equals the server name in the ¡°Host¡± request header field or the primary server name if this field is not present: 

proxy_set_header Host       $host;

In addition, the server name can be passed together with the port of the proxied server: 

proxy_set_header Host       $host:$proxy_port;

If the value of a header field is an empty string then this field will not be passed to a proxied server: 

proxy_set_header Accept-Encoding "";

Syntax:  proxy_ssl_certificate file;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 1.7.8. 

Specifies a file with the certificate in the PEM format used for authentication to a proxied HTTPS server. 

Syntax:  proxy_ssl_certificate_key file;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 1.7.8. 

Specifies a file with the secret key in the PEM format used for authentication to a proxied HTTPS server. 

The value engine:name:id can be specified instead of the file (1.7.9), which loads a secret key with a specified id from the OpenSSL engine name. 

Syntax:  proxy_ssl_ciphers ciphers;
 
Default:  proxy_ssl_ciphers DEFAULT; 
Context:  http, server, location
 

This directive appeared in version 1.5.6. 

Specifies the enabled ciphers for requests to a proxied HTTPS server. The ciphers are specified in the format understood by the OpenSSL library. 

The full list can be viewed using the ¡°openssl ciphers¡± command. 

Syntax:  proxy_ssl_crl file;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 1.7.0. 

Specifies a file with revoked certificates (CRL) in the PEM format used to verify the certificate of the proxied HTTPS server. 

Syntax:  proxy_ssl_name name;
 
Default:  proxy_ssl_name $proxy_host; 
Context:  http, server, location
 

This directive appeared in version 1.7.0. 

Allows to override the server name used to verify the certificate of the proxied HTTPS server and to be passed through SNI when establishing a connection with the proxied HTTPS server. 

By default, the host part of the proxy_pass URL is used. 

Syntax:  proxy_ssl_password_file file;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 1.7.8. 

Specifies a file with passphrases for secret keys where each passphrase is specified on a separate line. Passphrases are tried in turn when loading the key. 

Syntax:  proxy_ssl_server_name on | off;
 
Default:  proxy_ssl_server_name off; 
Context:  http, server, location
 

This directive appeared in version 1.7.0. 

Enables or disables passing of the server name through TLS Server Name Indication extension (SNI, RFC 6066) when establishing a connection with the proxied HTTPS server. 

Syntax:  proxy_ssl_session_reuse on | off;
 
Default:  proxy_ssl_session_reuse on; 
Context:  http, server, location
 

Determines whether SSL sessions can be reused when working with the proxied server. If the errors ¡°SSL3_GET_FINISHED:digest check failed¡± appear in the logs, try disabling session reuse. 

Syntax:  proxy_ssl_protocols [SSLv2] [SSLv3] [TLSv1] [TLSv1.1] [TLSv1.2];
 
Default:  proxy_ssl_protocols TLSv1 TLSv1.1 TLSv1.2; 
Context:  http, server, location
 

This directive appeared in version 1.5.6. 

Enables the specified protocols for requests to a proxied HTTPS server. 

Syntax:  proxy_ssl_trusted_certificate file;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 1.7.0. 

Specifies a file with trusted CA certificates in the PEM format used to verify the certificate of the proxied HTTPS server. 

Syntax:  proxy_ssl_verify on | off;
 
Default:  proxy_ssl_verify off; 
Context:  http, server, location
 

This directive appeared in version 1.7.0. 

Enables or disables verification of the proxied HTTPS server certificate. 

Syntax:  proxy_ssl_verify_depth number;
 
Default:  proxy_ssl_verify_depth 1; 
Context:  http, server, location
 

This directive appeared in version 1.7.0. 

Sets the verification depth in the proxied HTTPS server certificates chain. 

Syntax:  proxy_store on | off | string;
 
Default:  proxy_store off; 
Context:  http, server, location
 

Enables saving of files to a disk. The on parameter saves files with paths corresponding to the directives alias or root. The off parameter disables saving of files. In addition, the file name can be set explicitly using the string with variables: 

proxy_store /data/www$original_uri;

The modification time of files is set according to the received ¡°Last-Modified¡± response header field. The response is first written to a temporary file, and then the file is renamed. Starting from version 0.8.9, temporary files and the persistent store can be put on different file systems. However, be aware that in this case a file is copied across two file systems instead of the cheap renaming operation. It is thus recommended that for any given location both saved files and a directory holding temporary files, set by the proxy_temp_path directive, are put on the same file system. 

This directive can be used to create local copies of static unchangeable files, e.g.: 

location /images/ {
    root               /data/www;
    error_page         404 = /fetch$uri;
}

location /fetch/ {
    internal;

    proxy_pass         http://backend/;
    proxy_store        on;
    proxy_store_access user:rw group:rw all:r;
    proxy_temp_path    /data/temp;

    alias              /data/www/;
}

or like this: 

location /images/ {
    root               /data/www;
    error_page         404 = @fetch;
}

location @fetch {
    internal;

    proxy_pass         http://backend;
    proxy_store        on;
    proxy_store_access user:rw group:rw all:r;
    proxy_temp_path    /data/temp;

    root               /data/www;
}

Syntax:  proxy_store_access users:permissions ...;
 
Default:  proxy_store_access user:rw; 
Context:  http, server, location
 

Sets access permissions for newly created files and directories, e.g.: 

proxy_store_access user:rw group:rw all:r;

If any group or all access permissions are specified then user permissions may be omitted: 

proxy_store_access group:rw all:r;

Syntax:  proxy_temp_file_write_size size;
 
Default:  proxy_temp_file_write_size 8k|16k; 
Context:  http, server, location
 

Limits the size of data written to a temporary file at a time, when buffering of responses from the proxied server to temporary files is enabled. By default, size is limited by two buffers set by the proxy_buffer_size and proxy_buffers directives. The maximum size of a temporary file is set by the proxy_max_temp_file_size directive. 

Syntax:  proxy_temp_path path [level1 [level2 [level3]]];
 
Default:  proxy_temp_path proxy_temp; 
Context:  http, server, location
 

Defines a directory for storing temporary files with data received from proxied servers. Up to three-level subdirectory hierarchy can be used underneath the specified directory. For example, in the following configuration 

proxy_temp_path /spool/nginx/proxy_temp 1 2;
a temporary file might look like this: 

/spool/nginx/proxy_temp/7/45/00000123457

See also the use_temp_path parameter of the proxy_cache_path directive. 

Embedded Variables
The ngx_http_proxy_module module supports embedded variables that can be used to compose headers using the proxy_set_header directive: 

$proxy_host
name and port of a proxied server as specified in the proxy_pass directive;
$proxy_port
port of a proxied server as specified in the proxy_pass directive, or the protocol¡¯s default port;
$proxy_add_x_forwarded_for
the ¡°X-Forwarded-For¡± client request header field with the $remote_addr variable appended to it, separated by a comma. If the ¡°X-Forwarded-For¡± field is not present in the client request header, the $proxy_add_x_forwarded_for variable is equal to the $remote_addr variable.




Module ngx_http_access_module
Example Configuration
Directives
     allow
     deny
 
The ngx_http_access_module module allows limiting access to certain client addresses. 

Access can also be limited by password or by the result of subrequest. Simultaneous limitation of access by address and by password is controlled by the satisfy directive. 

Example Configuration

location / {
    deny  192.168.1.1;
    allow 192.168.1.0/24;
    allow 10.1.1.0/16;
    allow 2001:0db8::/32;
    deny  all;
}

The rules are checked in sequence until the first match is found. In this example, access is allowed only for IPv4 networks 10.1.1.0/16 and 192.168.1.0/24 excluding the address 192.168.1.1, and for IPv6 network 2001:0db8::/32. In case of a lot of rules, the use of the ngx_http_geo_module module variables is preferable. 

Directives
Syntax:  allow address | CIDR | unix: | all;
 
Default:  ¡ª  
Context:  http, server, location, limit_except
 

Allows access for the specified network or address. If the special value unix: is specified (1.5.1), allows access for all UNIX-domain sockets. 

Syntax:  deny address | CIDR | unix: | all;
 
Default:  ¡ª  
Context:  http, server, location, limit_except
 

Denies access for the specified network or address. If the special value unix: is specified (1.5.1), denies access for all UNIX-domain sockets. 





news
about
download
security
documentation
faq
books
support
donation

trac
wiki
twitter
blog
Module ngx_http_proxy_module
Example Configuration
Directives
     proxy_bind
     proxy_buffer_size
     proxy_buffering
     proxy_buffers
     proxy_busy_buffers_size
     proxy_cache
     proxy_cache_bypass
     proxy_cache_key
     proxy_cache_lock
     proxy_cache_lock_age
     proxy_cache_lock_timeout
     proxy_cache_methods
     proxy_cache_min_uses
     proxy_cache_path
     proxy_cache_purge
     proxy_cache_revalidate
     proxy_cache_use_stale
     proxy_cache_valid
     proxy_connect_timeout
     proxy_cookie_domain
     proxy_cookie_path
     proxy_force_ranges
     proxy_headers_hash_bucket_size
     proxy_headers_hash_max_size
     proxy_hide_header
     proxy_http_version
     proxy_ignore_client_abort
     proxy_ignore_headers
     proxy_intercept_errors
     proxy_limit_rate
     proxy_max_temp_file_size
     proxy_method
     proxy_next_upstream
     proxy_next_upstream_timeout
     proxy_next_upstream_tries
     proxy_no_cache
     proxy_pass
     proxy_pass_header
     proxy_pass_request_body
     proxy_pass_request_headers
     proxy_read_timeout
     proxy_redirect
     proxy_request_buffering
     proxy_send_lowat
     proxy_send_timeout
     proxy_set_body
     proxy_set_header
     proxy_ssl_certificate
     proxy_ssl_certificate_key
     proxy_ssl_ciphers
     proxy_ssl_crl
     proxy_ssl_name
     proxy_ssl_password_file
     proxy_ssl_server_name
     proxy_ssl_session_reuse
     proxy_ssl_protocols
     proxy_ssl_trusted_certificate
     proxy_ssl_verify
     proxy_ssl_verify_depth
     proxy_store
     proxy_store_access
     proxy_temp_file_write_size
     proxy_temp_path
Embedded Variables
 
The ngx_http_proxy_module module allows passing requests to another server. 

Example Configuration

location / {
    proxy_pass       http://localhost:8000;
    proxy_set_header Host      $host;
    proxy_set_header X-Real-IP $remote_addr;
}

Directives
Syntax:  proxy_bind address | off;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 0.8.22. 

Makes outgoing connections to a proxied server originate from the specified local IP address. Parameter value can contain variables (1.3.12). The special value off (1.3.12) cancels the effect of the proxy_bind directive inherited from the previous configuration level, which allows the system to auto-assign the local IP address. 

Syntax:  proxy_buffer_size size;
 
Default:  proxy_buffer_size 4k|8k; 
Context:  http, server, location
 

Sets the size of the buffer used for reading the first part of the response received from the proxied server. This part usually contains a small response header. By default, the buffer size is equal to one memory page. This is either 4K or 8K, depending on a platform. It can be made smaller, however. 

Syntax:  proxy_buffering on | off;
 
Default:  proxy_buffering on; 
Context:  http, server, location
 

Enables or disables buffering of responses from the proxied server. 

When buffering is enabled, nginx receives a response from the proxied server as soon as possible, saving it into the buffers set by the proxy_buffer_size and proxy_buffers directives. If the whole response does not fit into memory, a part of it can be saved to a temporary file on the disk. Writing to temporary files is controlled by the proxy_max_temp_file_size and proxy_temp_file_write_size directives. 

When buffering is disabled, the response is passed to a client synchronously, immediately as it is received. nginx will not try to read the whole response from the proxied server. The maximum size of the data that nginx can receive from the server at a time is set by the proxy_buffer_size directive. 

Buffering can also be enabled or disabled by passing ¡°yes¡± or ¡°no¡± in the ¡°X-Accel-Buffering¡± response header field. This capability can be disabled using the proxy_ignore_headers directive. 

Syntax:  proxy_buffers number size;
 
Default:  proxy_buffers 8 4k|8k; 
Context:  http, server, location
 

Sets the number and size of the buffers used for reading a response from the proxied server, for a single connection. By default, the buffer size is equal to one memory page. This is either 4K or 8K, depending on a platform. 

Syntax:  proxy_busy_buffers_size size;
 
Default:  proxy_busy_buffers_size 8k|16k; 
Context:  http, server, location
 

When buffering of responses from the proxied server is enabled, limits the total size of buffers that can be busy sending a response to the client while the response is not yet fully read. In the meantime, the rest of the buffers can be used for reading the response and, if needed, buffering part of the response to a temporary file. By default, size is limited by the size of two buffers set by the proxy_buffer_size and proxy_buffers directives. 

Syntax:  proxy_cache zone | off;
 
Default:  proxy_cache off; 
Context:  http, server, location
 

Defines a shared memory zone used for caching. The same zone can be used in several places. Parameter value can contain variables (1.7.9). The off parameter disables caching inherited from the previous configuration level. 

Syntax:  proxy_cache_bypass string ...;
 
Default:  ¡ª  
Context:  http, server, location
 

Defines conditions under which the response will not be taken from a cache. If at least one value of the string parameters is not empty and is not equal to ¡°0¡± then the response will not be taken from the cache: 

proxy_cache_bypass $cookie_nocache $arg_nocache$arg_comment;
proxy_cache_bypass $http_pragma    $http_authorization;
Can be used along with the proxy_no_cache directive. 

Syntax:  proxy_cache_key string;
 
Default:  proxy_cache_key $scheme$proxy_host$request_uri; 
Context:  http, server, location
 

Defines a key for caching, for example 

proxy_cache_key "$host$request_uri $cookie_user";
By default, the directive¡¯s value is close to the string 

proxy_cache_key $scheme$proxy_host$uri$is_args$args;

Syntax:  proxy_cache_lock on | off;
 
Default:  proxy_cache_lock off; 
Context:  http, server, location
 

This directive appeared in version 1.1.12. 

When enabled, only one request at a time will be allowed to populate a new cache element identified according to the proxy_cache_key directive by passing a request to a proxied server. Other requests of the same cache element will either wait for a response to appear in the cache or the cache lock for this element to be released, up to the time set by the proxy_cache_lock_timeout directive. 

Syntax:  proxy_cache_lock_age time;
 
Default:  proxy_cache_lock_age 5s; 
Context:  http, server, location
 

This directive appeared in version 1.7.8. 

If the last request passed to the proxied server for populating a new cache element has not completed for the specified time, one more request may be passed to the proxied server. 

Syntax:  proxy_cache_lock_timeout time;
 
Default:  proxy_cache_lock_timeout 5s; 
Context:  http, server, location
 

This directive appeared in version 1.1.12. 

Sets a timeout for proxy_cache_lock. When the time expires, the request will be passed to the proxied server, however, the response will not be cached. 

Before 1.7.8, the response could be cached. 

Syntax:  proxy_cache_methods GET | HEAD | POST ...;
 
Default:  proxy_cache_methods GET HEAD; 
Context:  http, server, location
 

This directive appeared in version 0.7.59. 

If the client request method is listed in this directive then the response will be cached. ¡°GET¡± and ¡°HEAD¡± methods are always added to the list, though it is recommended to specify them explicitly. See also the proxy_no_cache directive. 

Syntax:  proxy_cache_min_uses number;
 
Default:  proxy_cache_min_uses 1; 
Context:  http, server, location
 

Sets the number of requests after which the response will be cached. 

Syntax:  proxy_cache_path path [levels=levels] [use_temp_path=on|off] keys_zone=name:size [inactive=time] [max_size=size] [loader_files=number] [loader_sleep=time] [loader_threshold=time];
 
Default:  ¡ª  
Context:  http
 

Sets the path and other parameters of a cache. Cache data are stored in files. The file name in a cache is a result of applying the MD5 function to the cache key. The levels parameter defines hierarchy levels of a cache. For example, in the following configuration 

proxy_cache_path /data/nginx/cache levels=1:2 keys_zone=one:10m;
file names in a cache will look like this: 

/data/nginx/cache/c/29/b7f54b2df7773722d382f4809d65029c

A cached response is first written to a temporary file, and then the file is renamed. Starting from version 0.8.9, temporary files and the cache can be put on different file systems. However, be aware that in this case a file is copied across two file systems instead of the cheap renaming operation. It is thus recommended that for any given location both cache and a directory holding temporary files are put on the same file system. The directory for temporary files is set based on the use_temp_path parameter (1.7.10). If this parameter is omitted or set to the value on, the directory set by the proxy_temp_path directive for the given location will be used. If the value is set to off, temporary files will be put directly in the cache directory. 

In addition, all active keys and information about data are stored in a shared memory zone, whose name and size are configured by the keys_zone parameter. One megabyte zone can store about 8 thousand keys. 

Cached data that are not accessed during the time specified by the inactive parameter get removed from the cache regardless of their freshness. By default, inactive is set to 10 minutes. 

The special ¡°cache manager¡± process monitors the maximum cache size set by the max_size parameter. When this size is exceeded, it removes the least recently used data. 

A minute after the start the special ¡°cache loader¡± process is activated. It loads information about previously cached data stored on file system into a cache zone. The loading is done in iterations. During one iteration no more than loader_files items are loaded (by default, 100). Besides, the duration of one iteration is limited by the loader_threshold parameter (by default, 200 milliseconds). Between iterations, a pause configured by the loader_sleep parameter (by default, 50 milliseconds) is made. 

Syntax:  proxy_cache_purge string ...;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 1.5.7. 

Defines conditions under which the request will be considered a cache purge request. If at least one value of the string parameters is not empty and is not equal to ¡°0¡± then the cache entry with a corresponding cache key is removed. The result of successful operation is indicated by returning the 204 (No Content) response. 

If the cache key of a purge request ends with an asterisk (¡°*¡±), all cache entries matching the wildcard key will be removed from the cache. 

Example configuration: 

proxy_cache_path /data/nginx/cache keys_zone=cache_zone:10m;

map $request_method $purge_method {
    PURGE   1;
    default 0;
}

server {
    ...
    location / {
        proxy_pass http://backend;
        proxy_cache cache_zone;
        proxy_cache_key $uri;
        proxy_cache_purge $purge_method;
    }
}

This functionality is available as part of our commercial subscription. 

Syntax:  proxy_cache_revalidate on | off;
 
Default:  proxy_cache_revalidate off; 
Context:  http, server, location
 

This directive appeared in version 1.5.7. 

Enables revalidation of expired cache items using conditional requests with the ¡°If-Modified-Since¡± and ¡°If-None-Match¡± header fields. 

Syntax:  proxy_cache_use_stale error | timeout | invalid_header | updating | http_500 | http_502 | http_503 | http_504 | http_403 | http_404 | off ...;
 
Default:  proxy_cache_use_stale off; 
Context:  http, server, location
 

Determines in which cases a stale cached response can be used when an error occurs during communication with the proxied server. The directive¡¯s parameters match the parameters of the proxy_next_upstream directive. 

The error parameter also permits using a stale cached response if a proxied server to process a request cannot be selected. 

Additionally, the updating parameter permits using a stale cached response if it is currently being updated. This allows minimizing the number of accesses to proxied servers when updating cached data. 

To minimize the number of accesses to proxied servers when populating a new cache element, the proxy_cache_lock directive can be used. 

Syntax:  proxy_cache_valid [code ...] time;
 
Default:  ¡ª  
Context:  http, server, location
 

Sets caching time for different response codes. For example, the following directives 

proxy_cache_valid 200 302 10m;
proxy_cache_valid 404      1m;
set 10 minutes of caching for responses with codes 200 and 302 and 1 minute for responses with code 404. 

If only caching time is specified 

proxy_cache_valid 5m;
then only 200, 301, and 302 responses are cached. 

In addition, the any parameter can be specified to cache any responses: 

proxy_cache_valid 200 302 10m;
proxy_cache_valid 301      1h;
proxy_cache_valid any      1m;

Parameters of caching can also be set directly in the response header. This has higher priority than setting of caching time using the directive. 

?The ¡°X-Accel-Expires¡± header field sets caching time of a response in seconds. The zero value disables caching for a response. If the value starts with the @ prefix, it sets an absolute time in seconds since Epoch, up to which the response may be cached. 
?If the header does not include the ¡°X-Accel-Expires¡± field, parameters of caching may be set in the header fields ¡°Expires¡± or ¡°Cache-Control¡±. 
?If the header includes the ¡°Set-Cookie¡± field, such a response will not be cached. 
?If the header includes the ¡°Vary¡± field with the special value ¡°*¡±, such a response will not be cached (1.7.7). If the header includes the ¡°Vary¡± field with another value, such a response will be cached taking into account the corresponding request header fields (1.7.7). 
Processing of one or more of these response header fields can be disabled using the proxy_ignore_headers directive. 

Syntax:  proxy_connect_timeout time;
 
Default:  proxy_connect_timeout 60s; 
Context:  http, server, location
 

Defines a timeout for establishing a connection with a proxied server. It should be noted that this timeout cannot usually exceed 75 seconds. 

Syntax:  proxy_cookie_domain off;
proxy_cookie_domain domain replacement;
 
Default:  proxy_cookie_domain off; 
Context:  http, server, location
 

This directive appeared in version 1.1.15. 

Sets a text that should be changed in the domain attribute of the ¡°Set-Cookie¡± header fields of a proxied server response. Suppose a proxied server returned the ¡°Set-Cookie¡± header field with the attribute ¡°domain=localhost¡±. The directive 

proxy_cookie_domain localhost example.org;
will rewrite this attribute to ¡°domain=example.org¡±. 

A dot at the beginning of the domain and replacement strings and the domain attribute is ignored. Matching is case-insensitive. 

The domain and replacement strings can contain variables: 

proxy_cookie_domain www.$host $host;

The directive can also be specified using regular expressions. In this case, domain should start from the ¡°~¡± symbol. A regular expression can contain named and positional captures, and replacement can reference them: 

proxy_cookie_domain ~\.(?P<sl_domain>[-0-9a-z]+\.[a-z]+)$ $sl_domain;

There could be several proxy_cookie_domain directives: 

proxy_cookie_domain localhost example.org;
proxy_cookie_domain ~\.([a-z]+\.[a-z]+)$ $1;

The off parameter cancels the effect of all proxy_cookie_domain directives on the current level: 

proxy_cookie_domain off;
proxy_cookie_domain localhost example.org;
proxy_cookie_domain www.example.org example.org;

Syntax:  proxy_cookie_path off;
proxy_cookie_path path replacement;
 
Default:  proxy_cookie_path off; 
Context:  http, server, location
 

This directive appeared in version 1.1.15. 

Sets a text that should be changed in the path attribute of the ¡°Set-Cookie¡± header fields of a proxied server response. Suppose a proxied server returned the ¡°Set-Cookie¡± header field with the attribute ¡°path=/two/some/uri/¡±. The directive 

proxy_cookie_path /two/ /;
will rewrite this attribute to ¡°path=/some/uri/¡±. 

The path and replacement strings can contain variables: 

proxy_cookie_path $uri /some$uri;

The directive can also be specified using regular expressions. In this case, path should either start from the ¡°~¡± symbol for a case-sensitive matching, or from the ¡°~*¡± symbols for case-insensitive matching. The regular expression can contain named and positional captures, and replacement can reference them: 

proxy_cookie_path ~*^/user/([^/]+) /u/$1;

There could be several proxy_cookie_path directives: 

proxy_cookie_path /one/ /;
proxy_cookie_path / /two/;

The off parameter cancels the effect of all proxy_cookie_path directives on the current level: 

proxy_cookie_path off;
proxy_cookie_path /two/ /;
proxy_cookie_path ~*^/user/([^/]+) /u/$1;

Syntax:  proxy_force_ranges on | off;
 
Default:  proxy_force_ranges off; 
Context:  http, server, location
 

This directive appeared in version 1.7.7. 

Enables byte-range support for both cached and uncached responses from the proxied server regardless of the ¡°Accept-Ranges¡± field in these responses. 

Syntax:  proxy_headers_hash_bucket_size size;
 
Default:  proxy_headers_hash_bucket_size 64; 
Context:  http, server, location
 

Sets the bucket size for hash tables used by the proxy_hide_header and proxy_set_header directives. The details of setting up hash tables are provided in a separate document. 

Syntax:  proxy_headers_hash_max_size size;
 
Default:  proxy_headers_hash_max_size 512; 
Context:  http, server, location
 

Sets the maximum size of hash tables used by the proxy_hide_header and proxy_set_header directives. The details of setting up hash tables are provided in a separate document. 

Syntax:  proxy_hide_header field;
 
Default:  ¡ª  
Context:  http, server, location
 

By default, nginx does not pass the header fields ¡°Date¡±, ¡°Server¡±, ¡°X-Pad¡±, and ¡°X-Accel-...¡± from the response of a proxied server to a client. The proxy_hide_header directive sets additional fields that will not be passed. If, on the contrary, the passing of fields needs to be permitted, the proxy_pass_header directive can be used. 

Syntax:  proxy_http_version 1.0 | 1.1;
 
Default:  proxy_http_version 1.0; 
Context:  http, server, location
 

This directive appeared in version 1.1.4. 

Sets the HTTP protocol version for proxying. By default, version 1.0 is used. Version 1.1 is recommended for use with keepalive connections. 

Syntax:  proxy_ignore_client_abort on | off;
 
Default:  proxy_ignore_client_abort off; 
Context:  http, server, location
 

Determines whether the connection with a proxied server should be closed when a client closes the connection without waiting for a response. 

Syntax:  proxy_ignore_headers field ...;
 
Default:  ¡ª  
Context:  http, server, location
 

Disables processing of certain response header fields from the proxied server. The following fields can be ignored: ¡°X-Accel-Redirect¡±, ¡°X-Accel-Expires¡±, ¡°X-Accel-Limit-Rate¡± (1.1.6), ¡°X-Accel-Buffering¡± (1.1.6), ¡°X-Accel-Charset¡± (1.1.6), ¡°Expires¡±, ¡°Cache-Control¡±, ¡°Set-Cookie¡± (0.8.44), and ¡°Vary¡± (1.7.7). 

If not disabled, processing of these header fields has the following effect: 

?¡°X-Accel-Expires¡±, ¡°Expires¡±, ¡°Cache-Control¡±, ¡°Set-Cookie¡±, and ¡°Vary¡± set the parameters of response caching; 
?¡°X-Accel-Redirect¡± performs an internal redirect to the specified URI; 
?¡°X-Accel-Limit-Rate¡± sets the rate limit for transmission of a response to a client; 
?¡°X-Accel-Buffering¡± enables or disables buffering of a response; 
?¡°X-Accel-Charset¡± sets the desired charset of a response. 

Syntax:  proxy_intercept_errors on | off;
 
Default:  proxy_intercept_errors off; 
Context:  http, server, location
 

Determines whether proxied responses with codes greater than or equal to 300 should be passed to a client or be redirected to nginx for processing with the error_page directive. 

Syntax:  proxy_limit_rate rate;
 
Default:  proxy_limit_rate 0; 
Context:  http, server, location
 

This directive appeared in version 1.7.7. 

Limits the speed of reading the response from the proxied server. The rate is specified in bytes per second. The zero value disables rate limiting. The limit is set per a request, and so if nginx simultaneously opens two connections to the proxied server, the overall rate will be twice as much as the specified limit. The limitation works only if buffering of responses from the proxied server is enabled. 

Syntax:  proxy_max_temp_file_size size;
 
Default:  proxy_max_temp_file_size 1024m; 
Context:  http, server, location
 

When buffering of responses from the proxied server is enabled, and the whole response does not fit into the buffers set by the proxy_buffer_size and proxy_buffers directives, a part of the response can be saved to a temporary file. This directive sets the maximum size of the temporary file. The size of data written to the temporary file at a time is set by the proxy_temp_file_write_size directive. 

The zero value disables buffering of responses to temporary files. 


This restriction does not apply to responses that will be cached or stored on disk. 

Syntax:  proxy_method method;
 
Default:  ¡ª  
Context:  http, server, location
 

Specifies the HTTP method to use in requests forwarded to the proxied server instead of the method from the client request. 

Syntax:  proxy_next_upstream error | timeout | invalid_header | http_500 | http_502 | http_503 | http_504 | http_403 | http_404 | off ...;
 
Default:  proxy_next_upstream error timeout; 
Context:  http, server, location
 

Specifies in which cases a request should be passed to the next server: 

error
an error occurred while establishing a connection with the server, passing a request to it, or reading the response header;
timeout
a timeout has occurred while establishing a connection with the server, passing a request to it, or reading the response header;
invalid_header
a server returned an empty or invalid response;
http_500
a server returned a response with the code 500;
http_502
a server returned a response with the code 502;
http_503
a server returned a response with the code 503;
http_504
a server returned a response with the code 504;
http_403
a server returned a response with the code 403;
http_404
a server returned a response with the code 404;
off
disables passing a request to the next server.

One should bear in mind that passing a request to the next server is only possible if nothing has been sent to a client yet. That is, if an error or timeout occurs in the middle of the transferring of a response, fixing this is impossible. 

The directive also defines what is considered an unsuccessful attempt of communication with a server. The cases of error, timeout and invalid_header are always considered unsuccessful attempts, even if they are not specified in the directive. The cases of http_500, http_502, http_503 and http_504 are considered unsuccessful attempts only if they are specified in the directive. The cases of http_403 and http_404 are never considered unsuccessful attempts. 

Passing a request to the next server can be limited by the number of tries and by time. 

Syntax:  proxy_next_upstream_timeout time;
 
Default:  proxy_next_upstream_timeout 0; 
Context:  http, server, location
 

This directive appeared in version 1.7.5. 

Limits the time allowed to pass a request to the next server. The 0 value turns off this limitation. 

Syntax:  proxy_next_upstream_tries number;
 
Default:  proxy_next_upstream_tries 0; 
Context:  http, server, location
 

This directive appeared in version 1.7.5. 

Limits the number of possible tries for passing a request to the next server. The 0 value turns off this limitation. 

Syntax:  proxy_no_cache string ...;
 
Default:  ¡ª  
Context:  http, server, location
 

Defines conditions under which the response will not be saved to a cache. If at least one value of the string parameters is not empty and is not equal to ¡°0¡± then the response will not be saved: 

proxy_no_cache $cookie_nocache $arg_nocache$arg_comment;
proxy_no_cache $http_pragma    $http_authorization;
Can be used along with the proxy_cache_bypass directive. 

Syntax:  proxy_pass URL;
 
Default:  ¡ª  
Context:  location, if in location, limit_except
 

Sets the protocol and address of a proxied server and an optional URI to which a location should be mapped. As a protocol, ¡°http¡± or ¡°https¡± can be specified. The address can be specified as a domain name or IP address, and an optional port: 

proxy_pass http://localhost:8000/uri/;
or as a UNIX-domain socket path specified after the word ¡°unix¡± and enclosed in colons: 

proxy_pass http://unix:/tmp/backend.socket:/uri/;

If a domain name resolves to several addresses, all of them will be used in a round-robin fashion. In addition, an address can be specified as a server group. 

A request URI is passed to the server as follows: 

?If the proxy_pass directive is specified with a URI, then when a request is passed to the server, the part of a normalized request URI matching the location is replaced by a URI specified in the directive: 
location /name/ {
    proxy_pass http://127.0.0.1/remote/;
}
?If proxy_pass is specified without a URI, the request URI is passed to the server in the same form as sent by a client when the original request is processed, or the full normalized request URI is passed when processing the changed URI: 
location /some/path/ {
    proxy_pass http://127.0.0.1;
}
Before version 1.1.12, if proxy_pass is specified without a URI, the original request URI might be passed instead of the changed URI in some cases. 

In some cases, the part of a request URI to be replaced cannot be determined: 

?When location is specified using a regular expression. 
In this case, the directive should be specified without a URI. 

?When the URI is changed inside a proxied location using the rewrite directive, and this same configuration will be used to process a request (break): 
location /name/ {
    rewrite    /name/([^/]+) /users?name=$1 break;
    proxy_pass http://127.0.0.1;
}
In this case, the URI specified in the directive is ignored and the full changed request URI is passed to the server. 


A server name, its port and the passed URI can also be specified using variables: 

proxy_pass http://$host$uri;
or even like this: 

proxy_pass $request;

In this case, the server name is searched among the described server groups, and, if not found, is determined using a resolver. 

WebSocket proxying requires special configuration and is supported since version 1.3.13. 

Syntax:  proxy_pass_header field;
 
Default:  ¡ª  
Context:  http, server, location
 

Permits passing otherwise disabled header fields from a proxied server to a client. 

Syntax:  proxy_pass_request_body on | off;
 
Default:  proxy_pass_request_body on; 
Context:  http, server, location
 

Indicates whether the original request body is passed to the proxied server. 

location /x-accel-redirect-here/ {
    proxy_method GET;
    proxy_pass_request_body off;
    proxy_set_header Content-Length "";

    proxy_pass ...
}
See also the proxy_set_header and proxy_pass_request_headers directives. 

Syntax:  proxy_pass_request_headers on | off;
 
Default:  proxy_pass_request_headers on; 
Context:  http, server, location
 

Indicates whether the header fields of the original request are passed to the proxied server. 

location /x-accel-redirect-here/ {
    proxy_method GET;
    proxy_pass_request_headers off;
    proxy_pass_request_body off;

    proxy_pass ...
}
See also the proxy_set_header and proxy_pass_request_body directives. 

Syntax:  proxy_read_timeout time;
 
Default:  proxy_read_timeout 60s; 
Context:  http, server, location
 

Defines a timeout for reading a response from the proxied server. The timeout is set only between two successive read operations, not for the transmission of the whole response. If the proxied server does not transmit anything within this time, the connection is closed. 

Syntax:  proxy_redirect default;
proxy_redirect off;
proxy_redirect redirect replacement;
 
Default:  proxy_redirect default; 
Context:  http, server, location
 

Sets the text that should be changed in the ¡°Location¡± and ¡°Refresh¡± header fields of a proxied server response. Suppose a proxied server returned the header field ¡°Location: http://localhost:8000/two/some/uri/¡±. The directive 

proxy_redirect http://localhost:8000/two/ http://frontend/one/;
will rewrite this string to ¡°Location: http://frontend/one/some/uri/¡±. 

A server name may be omitted in the replacement string: 

proxy_redirect http://localhost:8000/two/ /;
then the primary server¡¯s name and port, if different from 80, will be inserted. 

The default replacement specified by the default parameter uses the parameters of the location and proxy_pass directives. Hence, the two configurations below are equivalent: 

location /one/ {
    proxy_pass     http://upstream:port/two/;
    proxy_redirect default;

location /one/ {
    proxy_pass     http://upstream:port/two/;
    proxy_redirect http://upstream:port/two/ /one/;
The default parameter is not permitted if proxy_pass is specified using variables. 

A replacement string can contain variables: 

proxy_redirect http://localhost:8000/ http://$host:$server_port/;

A redirect can also contain (1.1.11) variables: 

proxy_redirect http://$proxy_host:8000/ /;

The directive can be specified (1.1.11) using regular expressions. In this case, redirect should either start with the ¡°~¡± symbol for a case-sensitive matching, or with the ¡°~*¡± symbols for case-insensitive matching. The regular expression can contain named and positional captures, and replacement can reference them: 

proxy_redirect ~^(http://[^:]+):\d+(/.+)$ $1$2;
proxy_redirect ~* /user/([^/]+)/(.+)$      http://$1.example.com/$2;

There could be several proxy_redirect directives: 

proxy_redirect default;
proxy_redirect http://localhost:8000/  /;
proxy_redirect http://www.example.com/ /;

The off parameter cancels the effect of all proxy_redirect directives on the current level: 

proxy_redirect off;
proxy_redirect default;
proxy_redirect http://localhost:8000/  /;
proxy_redirect http://www.example.com/ /;

Using this directive, it is also possible to add host names to relative redirects issued by a proxied server: 

proxy_redirect / /;

Syntax:  proxy_request_buffering on | off;
 
Default:  proxy_request_buffering on; 
Context:  http, server, location
 

This directive appeared in version 1.7.11. 

Enables or disables buffering of a client request body. 

When buffering is enabled, the entire request body is read from the client before sending the request to a proxied server. 

When buffering is disabled, the request body is sent to the proxied server immediately as it is received. In this case, the request cannot be passed to the next server if nginx already started sending the request body. 

When HTTP/1.1 chunked transfer encoding is used to send the original request body, the request body will be buffered regardless of the directive value unless HTTP/1.1 is enabled for proxying. 

Syntax:  proxy_send_lowat size;
 
Default:  proxy_send_lowat 0; 
Context:  http, server, location
 

If the directive is set to a non-zero value, nginx will try to minimize the number of send operations on outgoing connections to a proxied server by using either NOTE_LOWAT flag of the kqueue method, or the SO_SNDLOWAT socket option, with the specified size. 

This directive is ignored on Linux, Solaris, and Windows. 

Syntax:  proxy_send_timeout time;
 
Default:  proxy_send_timeout 60s; 
Context:  http, server, location
 

Sets a timeout for transmitting a request to the proxied server. The timeout is set only between two successive write operations, not for the transmission of the whole request. If the proxied server does not receive anything within this time, the connection is closed. 

Syntax:  proxy_set_body value;
 
Default:  ¡ª  
Context:  http, server, location
 

Allows redefining the request body passed to the proxied server. The value can contain text, variables, and their combination. 

Syntax:  proxy_set_header field value;
 
Default:  proxy_set_header Host $proxy_host;proxy_set_header Connection close; 
Context:  http, server, location
 

Allows redefining or appending fields to the request header passed to the proxied server. The value can contain text, variables, and their combinations. These directives are inherited from the previous level if and only if there are no proxy_set_header directives defined on the current level. By default, only two fields are redefined: 

proxy_set_header Host       $proxy_host;
proxy_set_header Connection close;

An unchanged ¡°Host¡± request header field can be passed like this: 

proxy_set_header Host       $http_host;

However, if this field is not present in a client request header then nothing will be passed. In such a case it is better to use the $host variable - its value equals the server name in the ¡°Host¡± request header field or the primary server name if this field is not present: 

proxy_set_header Host       $host;

In addition, the server name can be passed together with the port of the proxied server: 

proxy_set_header Host       $host:$proxy_port;

If the value of a header field is an empty string then this field will not be passed to a proxied server: 

proxy_set_header Accept-Encoding "";

Syntax:  proxy_ssl_certificate file;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 1.7.8. 

Specifies a file with the certificate in the PEM format used for authentication to a proxied HTTPS server. 

Syntax:  proxy_ssl_certificate_key file;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 1.7.8. 

Specifies a file with the secret key in the PEM format used for authentication to a proxied HTTPS server. 

The value engine:name:id can be specified instead of the file (1.7.9), which loads a secret key with a specified id from the OpenSSL engine name. 

Syntax:  proxy_ssl_ciphers ciphers;
 
Default:  proxy_ssl_ciphers DEFAULT; 
Context:  http, server, location
 

This directive appeared in version 1.5.6. 

Specifies the enabled ciphers for requests to a proxied HTTPS server. The ciphers are specified in the format understood by the OpenSSL library. 

The full list can be viewed using the ¡°openssl ciphers¡± command. 

Syntax:  proxy_ssl_crl file;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 1.7.0. 

Specifies a file with revoked certificates (CRL) in the PEM format used to verify the certificate of the proxied HTTPS server. 

Syntax:  proxy_ssl_name name;
 
Default:  proxy_ssl_name $proxy_host; 
Context:  http, server, location
 

This directive appeared in version 1.7.0. 

Allows to override the server name used to verify the certificate of the proxied HTTPS server and to be passed through SNI when establishing a connection with the proxied HTTPS server. 

By default, the host part of the proxy_pass URL is used. 

Syntax:  proxy_ssl_password_file file;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 1.7.8. 

Specifies a file with passphrases for secret keys where each passphrase is specified on a separate line. Passphrases are tried in turn when loading the key. 

Syntax:  proxy_ssl_server_name on | off;
 
Default:  proxy_ssl_server_name off; 
Context:  http, server, location
 

This directive appeared in version 1.7.0. 

Enables or disables passing of the server name through TLS Server Name Indication extension (SNI, RFC 6066) when establishing a connection with the proxied HTTPS server. 

Syntax:  proxy_ssl_session_reuse on | off;
 
Default:  proxy_ssl_session_reuse on; 
Context:  http, server, location
 

Determines whether SSL sessions can be reused when working with the proxied server. If the errors ¡°SSL3_GET_FINISHED:digest check failed¡± appear in the logs, try disabling session reuse. 

Syntax:  proxy_ssl_protocols [SSLv2] [SSLv3] [TLSv1] [TLSv1.1] [TLSv1.2];
 
Default:  proxy_ssl_protocols TLSv1 TLSv1.1 TLSv1.2; 
Context:  http, server, location
 

This directive appeared in version 1.5.6. 

Enables the specified protocols for requests to a proxied HTTPS server. 

Syntax:  proxy_ssl_trusted_certificate file;
 
Default:  ¡ª  
Context:  http, server, location
 

This directive appeared in version 1.7.0. 

Specifies a file with trusted CA certificates in the PEM format used to verify the certificate of the proxied HTTPS server. 

Syntax:  proxy_ssl_verify on | off;
 
Default:  proxy_ssl_verify off; 
Context:  http, server, location
 

This directive appeared in version 1.7.0. 

Enables or disables verification of the proxied HTTPS server certificate. 

Syntax:  proxy_ssl_verify_depth number;
 
Default:  proxy_ssl_verify_depth 1; 
Context:  http, server, location
 

This directive appeared in version 1.7.0. 

Sets the verification depth in the proxied HTTPS server certificates chain. 

Syntax:  proxy_store on | off | string;
 
Default:  proxy_store off; 
Context:  http, server, location
 

Enables saving of files to a disk. The on parameter saves files with paths corresponding to the directives alias or root. The off parameter disables saving of files. In addition, the file name can be set explicitly using the string with variables: 

proxy_store /data/www$original_uri;

The modification time of files is set according to the received ¡°Last-Modified¡± response header field. The response is first written to a temporary file, and then the file is renamed. Starting from version 0.8.9, temporary files and the persistent store can be put on different file systems. However, be aware that in this case a file is copied across two file systems instead of the cheap renaming operation. It is thus recommended that for any given location both saved files and a directory holding temporary files, set by the proxy_temp_path directive, are put on the same file system. 

This directive can be used to create local copies of static unchangeable files, e.g.: 

location /images/ {
    root               /data/www;
    error_page         404 = /fetch$uri;
}

location /fetch/ {
    internal;

    proxy_pass         http://backend/;
    proxy_store        on;
    proxy_store_access user:rw group:rw all:r;
    proxy_temp_path    /data/temp;

    alias              /data/www/;
}

or like this: 

location /images/ {
    root               /data/www;
    error_page         404 = @fetch;
}

location @fetch {
    internal;

    proxy_pass         http://backend;
    proxy_store        on;
    proxy_store_access user:rw group:rw all:r;
    proxy_temp_path    /data/temp;

    root               /data/www;
}

Syntax:  proxy_store_access users:permissions ...;
 
Default:  proxy_store_access user:rw; 
Context:  http, server, location
 

Sets access permissions for newly created files and directories, e.g.: 

proxy_store_access user:rw group:rw all:r;

If any group or all access permissions are specified then user permissions may be omitted: 

proxy_store_access group:rw all:r;

Syntax:  proxy_temp_file_write_size size;
 
Default:  proxy_temp_file_write_size 8k|16k; 
Context:  http, server, location
 

Limits the size of data written to a temporary file at a time, when buffering of responses from the proxied server to temporary files is enabled. By default, size is limited by two buffers set by the proxy_buffer_size and proxy_buffers directives. The maximum size of a temporary file is set by the proxy_max_temp_file_size directive. 

Syntax:  proxy_temp_path path [level1 [level2 [level3]]];
 
Default:  proxy_temp_path proxy_temp; 
Context:  http, server, location
 

Defines a directory for storing temporary files with data received from proxied servers. Up to three-level subdirectory hierarchy can be used underneath the specified directory. For example, in the following configuration 

proxy_temp_path /spool/nginx/proxy_temp 1 2;
a temporary file might look like this: 

/spool/nginx/proxy_temp/7/45/00000123457

See also the use_temp_path parameter of the proxy_cache_path directive. 

Embedded Variables
The ngx_http_proxy_module module supports embedded variables that can be used to compose headers using the proxy_set_header directive: 

$proxy_host
name and port of a proxied server as specified in the proxy_pass directive;
$proxy_port
port of a proxied server as specified in the proxy_pass directive, or the protocol¡¯s default port;
$proxy_add_x_forwarded_for
the ¡°X-Forwarded-For¡± client request header field with the $remote_addr variable appended to it, separated by a comma. If the ¡°X-Forwarded-For¡± field is not present in the client request header, the $proxy_add_x_forwarded_for variable is equal to the $remote_addr variable.

*/
