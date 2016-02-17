
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


static ngx_int_t ngx_test_full_name(ngx_str_t *name);


static ngx_atomic_t   temp_number = 0;
ngx_atomic_t         *ngx_temp_number = &temp_number;
ngx_atomic_int_t      ngx_random_number = 123456; // ngx_random_number = (tp->msec << 16) + ngx_pid;


ngx_int_t
ngx_get_full_name(ngx_pool_t *pool, ngx_str_t *prefix, ngx_str_t *name)
{
    size_t      len;
    u_char     *p, *n;
    ngx_int_t   rc;

    rc = ngx_test_full_name(name);

    if (rc == NGX_OK) {
        return rc;
    }

    len = prefix->len;

#if (NGX_WIN32)

    if (rc == 2) {
        len = rc;
    }

#endif

    n = ngx_pnalloc(pool, len + name->len + 1);
    if (n == NULL) {
        return NGX_ERROR;
    }

    p = ngx_cpymem(n, prefix->data, len);
    ngx_cpystrn(p, name->data, name->len + 1);

    name->len += len;
    name->data = n;

    return NGX_OK;
}


static ngx_int_t
ngx_test_full_name(ngx_str_t *name)
{
#if (NGX_WIN32)
    u_char  c0, c1;

    c0 = name->data[0];

    if (name->len < 2) {
        if (c0 == '/') {
            return 2;
        }

        return NGX_DECLINED;
    }

    c1 = name->data[1];

    if (c1 == ':') {
        c0 |= 0x20;

        if ((c0 >= 'a' && c0 <= 'z')) {
            return NGX_OK;
        }

        return NGX_DECLINED;
    }

    if (c1 == '/') {
        return NGX_OK;
    }

    if (c0 == '/') {
        return 2;
    }

    return NGX_DECLINED;

#else

    if (name->data[0] == '/') {
        return NGX_OK;
    }

    return NGX_DECLINED;

#endif
}

/*
如果配置xxx_buffers  XXX_buffer_size指定的空间都用完了，则会把缓存中的数据写入临时文件，然后继续读，读到ngx_event_pipe_write_chain_to_temp_file
后写入临时文件，直到read返回NGX_AGAIN,然后在ngx_event_pipe_write_to_downstream->ngx_output_chain->ngx_output_chain_copy_buf中读取临时文件内容
发送到后端，当数据继续到来，通过epoll read继续循环该流程
*/

/*ngx_http_upstream_init_request->ngx_http_upstream_cache 客户端获取缓存 后端应答回来数据后在ngx_http_upstream_send_response->ngx_http_file_cache_create
中创建临时文件，然后在ngx_event_pipe_write_chain_to_temp_file把读取的后端数据写入临时文件，最后在
ngx_http_upstream_send_response->ngx_http_upstream_process_request->ngx_http_file_cache_update中把临时文件内容rename(相当于mv)到proxy_cache_path指定
的cache目录下面
*/


//创建temp_file临时文件，并把chain链表中的数据写入文件，返回值为写入到文件中的字节数
//如果配置xxx_buffers  XXX_buffer_size指定的空间都用完了，则会把缓存中的数据写入临时文件，然后继续读，读到后写入临时文件，直到read返回NGX_AGAIN
ssize_t
ngx_write_chain_to_temp_file(ngx_temp_file_t *tf, ngx_chain_t *chain)
{
    ngx_int_t  rc;

    if (tf->file.fd == NGX_INVALID_FILE) {
        rc = ngx_create_temp_file(&tf->file, tf->path, tf->pool,
                                  tf->persistent, tf->clean, tf->access); //创建临时文件

        if (rc != NGX_OK) {
            return rc;
        }

        if (tf->log_level) {
            ngx_log_error(tf->log_level, tf->file.log, 0, "%s %V",
                          tf->warn, &tf->file.name);
        }
    }

    //写临时文件的时候更新tf->file->offset  tf->file->sys_offset(也就是ngx_file_t中的成员)  tf->offset(这里是ngx_temp_file_t->offset)在该函数外层更新
    return ngx_write_chain_to_file(&tf->file, chain, tf->offset, tf->pool);
}


/*
配置fastcgi_cache_path /var/yyz/cache_xxx levels=1:2 keys_zone=fcgi:1m inactive=30m max_size=64 use_temp_path=off;打印如下:

2015/12/16 04:25:19[               ngx_create_temp_file,   169]  [debug] 19348#19348: *3 hashed path: /var/yyz/cache_xxx/temp/2/00/0000000002
2015/12/16 04:25:19[               ngx_create_temp_file,   174]  [debug] 19348#19348: *3 temp fd:-1
2015/12/16 04:25:19[                    ngx_create_path,   260]  [debug] 19348#19348: *3 temp file: "/var/yyz/cache_xxx/temp/2"
2015/12/16 04:25:19[                    ngx_create_path,   260]  [debug] 19348#19348: *3 temp file: "/var/yyz/cache_xxx/temp/2/00"
2015/12/16 04:25:19[               ngx_create_temp_file,   169]  [debug] 19348#19348: *3 hashed path: /var/yyz/cache_xxx/temp/2/00/0000000002
2015/12/16 04:25:19[               ngx_create_temp_file,   174]  [debug] 19348#19348: *3 temp fd:15
*/
ngx_int_t
ngx_create_temp_file(ngx_file_t *file, ngx_path_t *path, ngx_pool_t *pool,
    ngx_uint_t persistent, ngx_uint_t clean, ngx_uint_t access)
{
    uint32_t                  n;
    ngx_err_t                 err;
    ngx_pool_cleanup_t       *cln;
    ngx_pool_cleanup_file_t  *clnf;

    file->name.len = path->name.len + 1 + path->len + 10;

    file->name.data = ngx_pnalloc(pool, file->name.len + 1);
    if (file->name.data == NULL) {
        return NGX_ERROR;
    }

#if 0
    for (i = 0; i < file->name.len; i++) {
         file->name.data[i] = 'X';
    }
#endif

    ngx_memcpy(file->name.data, path->name.data, path->name.len);  //把path->name拷贝到file->name

    n = (uint32_t) ngx_next_temp_number(0);

    cln = ngx_pool_cleanup_add(pool, sizeof(ngx_pool_cleanup_file_t));
    if (cln == NULL) {
        return NGX_ERROR;
    }

    for ( ;; ) {
        (void) ngx_sprintf(file->name.data + path->name.len + 1 + path->len,
                           "%010uD%Z", n); //这里保证每次创建的临时文件不一样

        //注意前面已经把path->name拷贝到file->name
        ngx_create_hashed_filename(path, file->name.data, file->name.len);

        ngx_log_debug3(NGX_LOG_DEBUG_CORE, file->log, 0,
                       "hashed path: %s, persistent:%ui, access:%ui", 
                       file->name.data, persistent, access);

        file->fd = ngx_open_tempfile(file->name.data, persistent, access);

        ngx_log_debug1(NGX_LOG_DEBUG_CORE, file->log, 0,
                       "temp fd:%d", file->fd);

        if (file->fd != NGX_INVALID_FILE) { //说明已经存在该文件,如果没有则继续后面的创建，然后从这里退出

            cln->handler = clean ? ngx_pool_delete_file : ngx_pool_cleanup_file;
            clnf = cln->data;

            clnf->fd = file->fd;
            clnf->name = file->name.data;
            clnf->log = pool->log;

            return NGX_OK;
        }

        err = ngx_errno;

        if (err == NGX_EEXIST) { //说明该文件已经存在，则重新获取一个数字
            n = (uint32_t) ngx_next_temp_number(1);
            continue;
        }

        if ((path->level[0] == 0) || (err != NGX_ENOPATH)) {
            ngx_log_error(NGX_LOG_CRIT, file->log, err,
                          ngx_open_tempfile_n " \"%s\" failed",
                          file->name.data);
            return NGX_ERROR;
        }

        if (ngx_create_path(file, path) == NGX_ERROR) { //创建对应的文件
            return NGX_ERROR;
        }
    }
}

/*
ngx_create_hashed_filename 是通过level设置对应的文件夹路径，是根据md5值过来的后面的位数定义的文件夹。
*/
void
ngx_create_hashed_filename(ngx_path_t *path, u_char *file, size_t len)
{
    size_t      i, level;
    ngx_uint_t  n;

    i = path->name.len + 1;

    file[path->name.len + path->len]  = '/';

    /*
     levels=1:2，意思是说使用两级目录，第一级目录名是一个字符，第二级用两个字符。但是nginx最大支持3级目录，即levels=xxx:xxx:xxx。
     那么构成目录名字的字符哪来的呢？假设我们的存储目录为/cache，levels=1:2，那么对于上面的文件 就是这样存储的：
     /cache/0/8d/8ef9229f02c5672c747dc7a324d658d0  注意后面的8d0和cache后面的/0/8d一致
    */

    for (n = 0; n < 3; n++) { //拷贝最后面的MD5值字符串中的最后level[i]个字节到level所处位置
        level = path->level[n];

        if (level == 0) {
            break;
        }

        len -= level;
        file[i - 1] = '/';
        ngx_memcpy(&file[i], &file[len], level);
        i += level + 1;
    }
}

/*
配置fastcgi_cache_path /var/yyz/cache_xxx levels=1:2 keys_zone=fcgi:1m inactive=30m max_size=64 use_temp_path=off;打印如下:

2015/12/16 04:25:19[                    ngx_create_path,   260]  [debug] 19348#19348: *3 temp file: "/var/yyz/cache_xxx/temp/2"
2015/12/16 04:25:19[                    ngx_create_path,   260]  [debug] 19348#19348: *3 temp file: "/var/yyz/cache_xxx/temp/2/00"
*/
ngx_int_t
ngx_create_path(ngx_file_t *file, ngx_path_t *path)
{
    size_t      pos;
    ngx_err_t   err;
    ngx_uint_t  i;

    pos = path->name.len;

    for (i = 0; i < 3; i++) {
        if (path->level[i] == 0) {
            break;
        }

        pos += path->level[i] + 1;

        file->name.data[pos] = '\0';

        ngx_log_debug1(NGX_LOG_DEBUG_CORE, file->log, 0,
                       "temp file: \"%s\"", file->name.data);

        if (ngx_create_dir(file->name.data, 0700) == NGX_FILE_ERROR) {
            err = ngx_errno;
            if (err != NGX_EEXIST) {
                ngx_log_error(NGX_LOG_CRIT, file->log, err,
                              ngx_create_dir_n " \"%s\" failed",
                              file->name.data);
                return NGX_ERROR;
            }
        }

        file->name.data[pos] = '/';
    }

    return NGX_OK;
}


ngx_err_t
ngx_create_full_path(u_char *dir, ngx_uint_t access)
{
    u_char     *p, ch;
    ngx_err_t   err;

    err = 0;

#if (NGX_WIN32)
    p = dir + 3;
#else
    p = dir + 1;
#endif

    for ( /* void */ ; *p; p++) {
        ch = *p;

        if (ch != '/') {
            continue;
        }

        *p = '\0';

        if (ngx_create_dir(dir, access) == NGX_FILE_ERROR) {
            err = ngx_errno;

            switch (err) {
            case NGX_EEXIST:
                err = 0;
            case NGX_EACCES:
                break;

            default:
                return err;
            }
        }

        *p = '/';
    }

    return err;
}

//相当于获取一个随机数
ngx_atomic_uint_t
ngx_next_temp_number(ngx_uint_t collision) //collision置1，则
{
    ngx_atomic_uint_t  n, add;

    add = collision ? ngx_random_number : 1;

    n = ngx_atomic_fetch_add(ngx_temp_number, add);

    return n + add;
}

/*
char*(*set)(ngx_conf_t *cf, ngx_command_t 'vcmd,void *conf)
    关于set回调方法，在处理mytest配置项时已经使用过，其中mytest配置项是
不带参数的。如果处理配置项，我们既可以自己实现一个回调方法来处理配置项（），也可以使用Nginx预设的14个解析配置项方法，这会少
写许多代码，
┏━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名              ┃    行为                                                                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果nginx．conf文件中某个配置项的参数是on或者off（即希望配置项表达打开      ┃
┃                            ┃或者关闭某个功能的意思），而且在Nginx模块的代码中使用ngx_flag_t变量来保       ┃
┃ngx_conf_set_flag_slot      ┃存这个配置项的参数，就可以将set回调方法设为ngx_conf_set_flag_slot。当nginx.   ┃
┃                            ┃conf文件中参数是on时，代码中的ngx_flag_t类型变量将设为1，参数为off时则        ┃
┃                            ┃设为O                                                                         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果配置项后只有1个参数，同时在代码中我们希望用ngx_str_t类型的变量来保      ┃
┃ngx_conf_set_str_slot       ┃                                                                              ┃
┃                            ┃存这个配置项的参数，则可以使用ngx_conf_set_ str slot方法                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果这个配置项会出现多次，每个配置项后面都跟着1个参数，而在程序中我们       ┃
┃                            ┃希望仅用一个ngx_array_t动态数组（           ）来存储所有的参数，且数组中      ┃
┃ngx_conf_set_str_array_slot ┃                                                                              ┃
┃                            ┃的每个参数都以ngx_str_t来存储，那么预设的ngx_conf_set_str_array_slot有法可    ┃
┃                            ┃以帮我们做到                                                                  ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_str_array_slot类似，也是用一个ngx_array_t数组来存储所有同    ┃
┃                            ┃名配置项的参数。只是每个配置项的参数不再只是1个，而必须是两个，且以“配       ┃
┃ngx_conf_set_keyval_slot    ┃                                                                              ┃
┃                            ┃置项名关键字值；”的形式出现在nginx．conf文件中，同时，ngx_conf_set_keyval    ┃
┃                            ┃ slot将把这蜱配置项转化为数组，其中每个元素都存储着key/value键值对            ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_conf_set_num_slot       ┃  配置项后必须携带1个参数，且只能是数字。存储这个参数的变量必须是整型         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间大小，可以是一个数字，这时表示字节数       ┃
┃                            ┃(Byte)。如果数字后跟着k或者K，就表示Kilobyt，IKB=1024B；如果数字后跟          ┃
┃ngx_conf_set_size_slot      ┃                                                                              ┃
┃                            ┃着m或者M，就表示Megabyte，1MB=1024KB。ngx_conf_set_ size slot解析后将         ┃
┃                            ┃把配置项后的参数转化成以字节数为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间上的偏移量。它与设置的参数非常类似，       ┃
┃                            ┃其参数是一个数字时表示Byte，也可以在后面加单位，但与ngx_conf_set_size slot    ┃
┃ngx_conf_set_off_slot       ┃不同的是，数字后面的单位不仅可以是k或者K、m或者M，还可以是g或者G，            ┃
┃                            ┃这时表示Gigabyte，IGB=1024MB。ngx_conf_set_off_slot解析后将把配置项后的       ┃
┃                            ┃参数转化成以字节数为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示时间。这个参数可以在数字后面加单位，如         ┃
┃                            ┃果单位为s或者没有任何单位，那么这个数字表示秒；如果单位为m，则表示分          ┃
┃                            ┃钟，Im=60s：如果单位为h，则表示小时，th=60m：如果单位为d，则表示天，          ┃
┃ngx_conf_set_msec_slot      ┃                                                                              ┃
┃                            ┃ld=24h：如果单位为w，则表示周，lw=7d；如果单位为M，则表示月，1M=30d；         ┃
┃                            ┃如果单位为y，则表示年，ly=365d。ngx_conf_set_msec_slot解析后将把配置项后     ┃
┃                            ┃的参数转化成以毫秒为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_msec slot非常类似，唯一的区别是ngx_conf_set msec—slot解析   ┃
┃ngx_conf_set_sec_slot       ┃后将把配置项后的参数转化成以毫秒为单位的数字，而ngx_conf_set_sec一slot解析    ┃
┃                            ┃后会把配置项后的参数转化成以秒为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带一两个参数，第1个参数是数字，第2个参数表示空间大小。        ┃
┃                            ┃例如，“gzip_buffers 4 8k;”（通常用来表示有多少个ngx_buf_t缓冲区），其中第1  ┃
┃                            ┃个参数不可以携带任何单位，第2个参数不带任何单位时表示Byte，如果以k或          ┃
┃ngx_conf_set_bufs_slot      ┃者K作为单位，则表示Kilobyte，如果以m或者M作为单位，则表示Megabyte。           ┃
┃                            ┃ngx_conf_set- bufs—slot解析后会把配置项后的两个参数转化成ngx_bufs_t结构体下  ┃
┃                            ┃的两个成员。这个配置项对应于Nginx最喜欢用的多缓冲区的解决方案（如接收连       ┃
┃                            ┃接对端发来的TCP流）                                                           ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，其取值范围必须是我们设定好的字符串之一（就像       ┃
┃                            ┃C语言中的枚举一样）。首先，我们要用ngx_conf_enum_t结构定义配置项的取值范      ┃
┃ngx_conf_set_enum_slot      ┃                                                                              ┃
┃                            ┃围，并设定每个值对应的序列号。然后，ngx_conf_set enum slot将会把配置项参      ┃
┃                            ┃数转化为对应的序列号                                                          ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  写ngx_conf_set bitmask slot类似，配置项后必须携带1个参数，其取值范围必      ┃
┃                            ┃须是设定好的字符串之一。首先，我们要用ngx_conf_bitmask_t结构定义配置项的      ┃
┃ngx_conf_set_bitmask_slot   ┃                                                                              ┃
┃                            ┃取值范围，并设定每个值对应的比特位。注意，每个值所对应的比特位都要不同。      ┃
┃                            ┃然后ngx_conf_set_bitmask_ slot将会把配置项参数转化为对应的比特位              ┃
┗━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

┏━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名            ┃    行为                                                                    ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置目录或者文件的读写权限。配置项后可以携带1～3个参数，      ┃
┃                          ┃可以是如下形式：user:rw group:rw all:rw。注意，它的意义与Linux上文件或者目  ┃
┃ngx_conf_set_access_slot  ┃录的权限意义是一致的，但是user/group/all后面的权限只可以设为rw（读／写）或  ┃
┃                          ┃者r（只读），不可以有其他任何形式，如w或者rx等。ngx_conf_set- access slot   ┃
┃                          ┃将会把汶曲参数转化为一个整型                                                ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置路径，配置项后必须携带1个参数，表示1个有意义的路径。      ┃
┃ngx_conf_set_path_slot    ┃                                                                            ┃
┃                          ┃ngx_conf_set_path_slot将会把参数转化为ngx_path_t结构                        ┃
┗━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
*/

/*
ngx_conf_set_path_slot可以携带1～4个参数，其中第1个参数必须是路径，第2～4
个参数必须是整数（大部分情形下可以不使用），可以参见client_body_temp_path
配置项的用法，client_body_temp_path配置项就是用ngx_conf_set_path_slot预设方法来解析
参数的。
    ngx_conf_set_path_slot会把配置项中的路径参数转化为ngx_path_t结构，看一下ngx_path_t的定义。
typedef struct {
     ngx_str_t name;
     size_t len;
      size  t  level [3]
    ngx_path_manager_pt manager;
    ngx_path_loader_pt loader;
    void *data;
     u_char *conf_file;
     ngx_uint_t line;
 } ngx_path_t ;
其中，name成员存储着字符串形式的路径，而level数组就舍存储着第2、第3、第4
个参数（如果存在的话）。这里用ngx_http_mytest_conf_t结构中的ngx_path_t* my_path;来
存储配置项“test_path”后的参数值。
static ngx_command_t  ngx_http_mytest_commands []  =
   {   ngx_string ( " test_path" ) ,
              NGX_HTTP_LOC_CONF I NGX_CONF_TAKE1234,  '
            ngx_conf_set_path_slot ,
               NGX_HTTP_ LOC  CONF_OFFSET,
               offsetof (ngx_http_mytest  conf_t ,   my_path) ,
                  NULL 
    },
    ngx null_Command
  }
   如果nginx.conf中存在配置项test_path /usr/local/nginx/ 1 2 3, my_path指向的ngx_
path_t结构中，name的内容是/usr/local/nginx/，而level[0]为1，level[l]为2，level[2]为3。
如果配置项是test_path /usr/local/nginx/;，那么level数组的3个成员都是O。
*/
char *
ngx_conf_set_path_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = conf;

    ssize_t      level;
    ngx_str_t   *value;
    ngx_uint_t   i, n;
    ngx_path_t  *path, **slot;

    slot = (ngx_path_t **) (p + cmd->offset);

    if (*slot) {
        return "is duplicate";
    }

    path = ngx_pcalloc(cf->pool, sizeof(ngx_path_t));
    if (path == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    path->name = value[1];

    if (path->name.data[path->name.len - 1] == '/') {  //把路径最后面的/去掉
        path->name.len--;
    }

    if (ngx_conf_full_name(cf->cycle, &path->name, 0) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    path->conf_file = cf->conf_file->file.name.data;
    path->line = cf->conf_file->line;

    for (i = 0, n = 2; n < cf->args->nelts; i++, n++) {
        level = ngx_atoi(value[n].data, value[n].len);
        if (level == NGX_ERROR || level == 0) {
            return "invalid value";
        }

        path->level[i] = level;
        path->len += level + 1;
    }

    if (path->len > 10 + i) {
        return "invalid value";
    }

    *slot = path;

    if (ngx_add_path(cf, slot) == NGX_ERROR) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


char *
ngx_conf_merge_path_value(ngx_conf_t *cf, ngx_path_t **path, ngx_path_t *prev,
    ngx_path_init_t *init)
{
    if (*path) {
        return NGX_CONF_OK;
    }

    if (prev) {
        *path = prev;
        return NGX_CONF_OK;
    }

    *path = ngx_pcalloc(cf->pool, sizeof(ngx_path_t));
    if (*path == NULL) {
        return NGX_CONF_ERROR;
    }

    (*path)->name = init->name;

    if (ngx_conf_full_name(cf->cycle, &(*path)->name, 0) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    (*path)->level[0] = init->level[0];
    (*path)->level[1] = init->level[1];
    (*path)->level[2] = init->level[2];

    (*path)->len = init->level[0] + (init->level[0] ? 1 : 0)
                   + init->level[1] + (init->level[1] ? 1 : 0)
                   + init->level[2] + (init->level[2] ? 1 : 0);

    if (ngx_add_path(cf, path) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

/*
char*(*set)(ngx_conf_t *cf, ngx_commandj 'vcmd,void *conf)
    关于set回调方法，在处理mytest配置项时已经使用过，其中mytest配置项是
    不带参数的。如果处理配置项，我们既可以自己实现一个回调方法来处理配置项（），也可以使用Nginx预设的14个解析配置项方法，这会少
写许多代码
┏━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名              ┃    行为                                                                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果nginx．conf文件中某个配置项的参数是on或者off（即希望配置项表达打开      ┃
┃                            ┃或者关闭某个功能的意思），而且在Nginx模块的代码中使用ngx_flag_t变量来保       ┃
┃ngx_conf_set_flag_slot      ┃存这个配置项的参数，就可以将set回调方法设为ngx_conf_set_flag_slot。当nginx.   ┃
┃                            ┃conf文件中参数是on时，代码中的ngx_flag_t类型变量将设为1，参数为off时则        ┃
┃                            ┃设为O                                                                         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果配置项后只有1个参数，同时在代码中我们希望用ngx_str_t类型的变量来保      ┃
┃ngx_conf_set_str_slot       ┃                                                                              ┃
┃                            ┃存这个配置项的参数，则可以使用ngx_conf_set_ str slot方法                      ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  如果这个配置项会出现多次，每个配置项后面都跟着1个参数，而在程序中我们       ┃
┃                            ┃希望仅用一个ngx_array_t动态数组（           ）来存储所有的参数，且数组中      ┃
┃ngx_conf_set_str_array_slot ┃                                                                              ┃
┃                            ┃的每个参数都以ngx_str_t来存储，那么预设的ngx_conf_set_str_array_slot有法可    ┃
┃                            ┃以帮我们做到                                                                  ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_str_array_slot类似，也是用一个ngx_array_t数组来存储所有同    ┃
┃                            ┃名配置项的参数。只是每个配置项的参数不再只是1个，而必须是两个，且以“配       ┃
┃ngx_conf_set_keyval_slot    ┃                                                                              ┃
┃                            ┃置项名关键字值；”的形式出现在nginx．conf文件中，同时，ngx_conf_set_keyval    ┃
┃                            ┃ slot将把这蜱配置项转化为数组，其中每个元素都存储着key/value键值对            ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_conf_set_num_slot       ┃  配置项后必须携带1个参数，且只能是数字。存储这个参数的变量必须是整型         ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间大小，可以是一个数字，这时表示字节数       ┃
┃                            ┃(Byte)。如果数字后跟着k或者K，就表示Kilobyt，IKB=1024B；如果数字后跟          ┃
┃ngx_conf_set_size_slot      ┃                                                                              ┃
┃                            ┃着m或者M，就表示Megabyte，1MB=1024KB。ngx_conf_set_ size slot解析后将         ┃
┃                            ┃把配置项后的参数转化成以字节数为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示空间上的偏移量。它与设置的参数非常类似，       ┃
┃                            ┃其参数是一个数字时表示Byte，也可以在后面加单位，但与ngx_conf_set_size slot    ┃
┃ngx_conf_set_off_slot       ┃不同的是，数字后面的单位不仅可以是k或者K、m或者M，还可以是g或者G，            ┃
┃                            ┃这时表示Gigabyte，IGB=1024MB。ngx_conf_set_off_slot解析后将把配置项后的       ┃
┃                            ┃参数转化成以字节数为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，表示时间。这个参数可以在数字后面加单位，如         ┃
┃                            ┃果单位为s或者没有任何单位，那么这个数字表示秒；如果单位为m，则表示分          ┃
┃                            ┃钟，Im=60s：如果单位为h，则表示小时，th=60m：如果单位为d，则表示天，          ┃
┃ngx_conf_set_msec_slot      ┃                                                                              ┃
┃                            ┃ld=24h：如果单位为w，则表示周，lw=7d；如果单位为M，则表示月，1M=30d；         ┃
┃                            ┃如果单位为y，则表示年，ly=365d。ngx_conf_set_msec_slot解析后将把配置项后     ┃
┃                            ┃的参数转化成以毫秒为单位的数字                                                ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  与ngx_conf_set_msec slot非常类似，唯一的区别是ngx_conf_set msec—slot解析   ┃
┃ngx_conf_set_sec_slot       ┃后将把配置项后的参数转化成以毫秒为单位的数字，而ngx_conf_set_sec一slot解析    ┃
┃                            ┃后会把配置项后的参数转化成以秒为单位的数字                                    ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带一两个参数，第1个参数是数字，第2个参数表示空间大小。        ┃
┃                            ┃例如，“gzip_buffers 4 8k;”（通常用来表示有多少个ngx_buf_t缓冲区），其中第1  ┃
┃                            ┃个参数不可以携带任何单位，第2个参数不带任何单位时表示Byte，如果以k或          ┃
┃ngx_conf_set_bufs_slot      ┃者K作为单位，则表示Kilobyte，如果以m或者M作为单位，则表示Megabyte。           ┃
┃                            ┃ngx_conf_set- bufs—slot解析后会把配置项后的两个参数转化成ngx_bufs_t结构体下  ┃
┃                            ┃的两个成员。这个配置项对应于Nginx最喜欢用的多缓冲区的解决方案（如接收连       ┃
┃                            ┃接对端发来的TCP流）                                                           ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  配置项后必须携带1个参数，其取值范围必须是我们设定好的字符串之一（就像       ┃
┃                            ┃C语言中的枚举一样）。首先，我们要用ngx_conf_enum_t结构定义配置项的取值范      ┃
┃ngx_conf_set_enum_slot      ┃                                                                              ┃
┃                            ┃围，并设定每个值对应的序列号。然后，ngx_conf_set enum slot将会把配置项参      ┃
┃                            ┃数转化为对应的序列号                                                          ┃
┣━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                            ┃  写ngx_conf_set bitmask slot类似，配置项后必须携带1个参数，其取值范围必      ┃
┃                            ┃须是设定好的字符串之一。首先，我们要用ngx_conf_bitmask_t结构定义配置项的      ┃
┃ngx_conf_set_bitmask_slot   ┃                                                                              ┃
┃                            ┃取值范围，并设定每个值对应的比特位。注意，每个值所对应的比特位都要不同。      ┃
┃                            ┃然后ngx_conf_set_bitmask_ slot将会把配置项参数转化为对应的比特位              ┃
┗━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

┏━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    预设方法名            ┃    行为                                                                    ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置目录或者文件的读写权限。配置项后可以携带1～3个参数，      ┃
┃                          ┃可以是如下形式：user:rw group:rw all:rw。注意，它的意义与Linux上文件或者目  ┃
┃ngx_conf_set_access slot  ┃录的权限意义是一致的，但是user/group/all后面的权限只可以设为rw（读／写）或  ┃
┃                          ┃者r（只读），不可以有其他任何形式，如w或者rx等。ngx_conf_set- access slot   ┃
┃                          ┃将会把汶曲参数转化为一个整型                                                ┃
┣━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                          ┃  这个方法用于设置路径，配置项后必须携带1个参数，表示1个有意义的路径。      ┃
┃ngx_conf_set_path_slot    ┃                                                                            ┃
┃                          ┃ngx_conf_set_path_slot将会把参数转化为ngx_path_t结构                        ┃
┗━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
*/

/*
ngx_conf_set_access_slot 用于设置读／写权限，配置项后可以掳带1～3个参数，因此，
在ngx_command_t中的type成员要包含NGX—CONF TAKE123。参数的取值可参见表4-2。
这里用ngx_http_mytest_conf_t结构中的ngx_uint_t my_access;来存储配置项“test_access”
后的参数值，如下所示。
static: ngx_command_t  ngx_http_mytest_commands []  = {
    {   ngx_string ( " test_access " ) ,
              NGX_HTTP_LOC_CONF  I  NGX_CONF_TAKE123 ,
             ngx_conf_set_access_slot ,
                NGX_HTTP_LOC_CONF OFFSET,
                offsetof (ngx_http_mytest  conf_t ,   my_access) ,
                 NULL 
		},
		ngx_null_command
	}
    这样，ngx_conf_set access slot就可以解析读／写权限的配置项了。例如，当nginx
conf中出现配置项test access user:rw group:rw all:r;时，my_access的值将是436。
    注意  在ngx_http_mytest_create loc conf创建结构体时，如果想使用ngx_conf_set_
access slot，那么必须把my_access初始化为NGX CONF UNSET UINT宏，也就是4.2.1
节中的语句mycf->my_access=NGX_CONF_UNSET_UINT;．否则ngx_conf_set_access_slot
解析时会报错。
*/
char *
ngx_conf_set_access_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *confp = conf;

    u_char      *p;
    ngx_str_t   *value;
    ngx_uint_t   i, right, shift, *access;

    access = (ngx_uint_t *) (confp + cmd->offset);

    if (*access != NGX_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elts;

    *access = 0600;

    for (i = 1; i < cf->args->nelts; i++) { //低三位表示all的权限，中间三位为group权限，高三位为user权限

        p = value[i].data;

        if (ngx_strncmp(p, "user:", sizeof("user:") - 1) == 0) {
            shift = 6;
            p += sizeof("user:") - 1;

        } else if (ngx_strncmp(p, "group:", sizeof("group:") - 1) == 0) {
            shift = 3;
            p += sizeof("group:") - 1;

        } else if (ngx_strncmp(p, "all:", sizeof("all:") - 1) == 0) {
            shift = 0;
            p += sizeof("all:") - 1;

        } else {
            goto invalid;
        }

        if (ngx_strcmp(p, "rw") == 0) {
            right = 6;

        } else if (ngx_strcmp(p, "r") == 0) {
            right = 4;
        } else {
            goto invalid;
        }

        *access |= right << shift;
    }

    return NGX_CONF_OK;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid value \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}

/*
检查slot是否已经存在  如果不存在，则添加到cf->cycle->pathes  
检查cache->path是否已经存在 如果不存在，则添加到cf->cycle->pathes  
*/
ngx_int_t
ngx_add_path(ngx_conf_t *cf, ngx_path_t **slot)
{
    ngx_uint_t   i, n;
    ngx_path_t  *path, **p;

    path = *slot;

    p = cf->cycle->paths.elts;
    for (i = 0; i < cf->cycle->paths.nelts; i++) {
        if (p[i]->name.len == path->name.len
            && ngx_strcmp(p[i]->name.data, path->name.data) == 0)
        {
            if (p[i]->data != path->data) { //指向同一块内存的name
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "the same path name \"%V\" "
                                   "used in %s:%ui and",
                                   &p[i]->name, p[i]->conf_file, p[i]->line);
                return NGX_ERROR;
            }

            for (n = 0; n < 3; n++) { 
                if (p[i]->level[n] != path->level[n]) {//name相同，但是level不同，说明有冲突，会以第一个为准
                    if (path->conf_file == NULL) {
                        if (p[i]->conf_file == NULL) {
                            ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                                      "the default path name \"%V\" has "
                                      "the same name as another default path, "
                                      "but the different levels, you need to "
                                      "redefine one of them in http section",
                                      &p[i]->name);
                            return NGX_ERROR;
                        }

                        ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                                      "the path name \"%V\" in %s:%ui has "
                                      "the same name as default path, but "
                                      "the different levels, you need to "
                                      "define default path in http section",
                                      &p[i]->name, p[i]->conf_file, p[i]->line);
                        return NGX_ERROR;
                    }

                    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                      "the same path name \"%V\" in %s:%ui "
                                      "has the different levels than",
                                      &p[i]->name, p[i]->conf_file, p[i]->line);
                    return NGX_ERROR;
                }

                if (p[i]->level[n] == 0) {
                    break;
                }
            }

            *slot = p[i];

            return NGX_OK;
        }
    }

    p = ngx_array_push(&cf->cycle->paths);
    if (p == NULL) {
        return NGX_ERROR;
    }

    *p = path; //把新来的slot添加到cf->cycle->paths

    return NGX_OK;
}


ngx_int_t
ngx_create_paths(ngx_cycle_t *cycle, ngx_uid_t user)
{
    ngx_err_t         err;
    ngx_uint_t        i;
    ngx_path_t      **path;

    path = cycle->paths.elts;
    for (i = 0; i < cycle->paths.nelts; i++) {

        if (ngx_create_dir(path[i]->name.data, 0700) == NGX_FILE_ERROR) {
            err = ngx_errno;
            if (err != NGX_EEXIST) {
                ngx_log_error(NGX_LOG_EMERG, cycle->log, err,
                              ngx_create_dir_n " \"%s\" failed",
                              path[i]->name.data);
                return NGX_ERROR;
            }
        }

        if (user == (ngx_uid_t) NGX_CONF_UNSET_UINT) {
            continue;
        }

#if !(NGX_WIN32)
        {
        ngx_file_info_t   fi;

        if (ngx_file_info((const char *) path[i]->name.data, &fi)
            == NGX_FILE_ERROR)
        {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                          ngx_file_info_n " \"%s\" failed", path[i]->name.data);
            return NGX_ERROR;
        }

        if (fi.st_uid != user) {
            if (chown((const char *) path[i]->name.data, user, -1) == -1) {
                ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                              "chown(\"%s\", %d) failed",
                              path[i]->name.data, user);
                return NGX_ERROR;
            }
        }

        if ((fi.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR))
                                                  != (S_IRUSR|S_IWUSR|S_IXUSR))
        {
            fi.st_mode |= (S_IRUSR|S_IWUSR|S_IXUSR);

            if (chmod((const char *) path[i]->name.data, fi.st_mode) == -1) {
                ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                              "chmod() \"%s\" failed", path[i]->name.data);
                return NGX_ERROR;
            }
        }
        }
#endif
    }

    return NGX_OK;
}


ngx_int_t
ngx_ext_rename_file(ngx_str_t *src, ngx_str_t *to, ngx_ext_rename_file_t *ext)
{
    u_char           *name;
    ngx_err_t         err;
    ngx_copy_file_t   cf;

#if !(NGX_WIN32)

    if (ext->access) {
        if (ngx_change_file_access(src->data, ext->access) == NGX_FILE_ERROR) { //按照ext->access修改文件权限
            ngx_log_error(NGX_LOG_CRIT, ext->log, ngx_errno,
                          ngx_change_file_access_n " \"%s\" failed", src->data);
            err = 0;
            goto failed;
        }
    }

#endif

    if (ext->time != -1) {
        if (ngx_set_file_time(src->data, ext->fd, ext->time) != NGX_OK) {
            ngx_log_error(NGX_LOG_CRIT, ext->log, ngx_errno,
                          ngx_set_file_time_n " \"%s\" failed", src->data);
            err = 0;
            goto failed;
        }
    }

    if (ngx_rename_file(src->data, to->data) != NGX_FILE_ERROR) { //成功直接返回
        return NGX_OK;
    }

    err = ngx_errno;

    if (err == NGX_ENOPATH) { //说明to目录文件不存在，则创建，然后重新rename

        if (!ext->create_path) {
            goto failed;
        }

        err = ngx_create_full_path(to->data, ngx_dir_access(ext->path_access));

        if (err) {
            ngx_log_error(NGX_LOG_CRIT, ext->log, err,
                          ngx_create_dir_n " \"%s\" failed", to->data);
            err = 0;
            goto failed;
        }

        if (ngx_rename_file(src->data, to->data) != NGX_FILE_ERROR) {
            return NGX_OK;//成功直接返回
        }

        err = ngx_errno;
    }

#if (NGX_WIN32)

    if (err == NGX_EEXIST) {
        err = ngx_win32_rename_file(src, to, ext->log);

        if (err == 0) {
            return NGX_OK;
        }
    }

#endif

    if (err == NGX_EXDEV) {

        cf.size = -1;
        cf.buf_size = 0;
        cf.access = ext->access;
        cf.time = ext->time;
        cf.log = ext->log;

        name = ngx_alloc(to->len + 1 + 10 + 1, ext->log);
        if (name == NULL) {
            return NGX_ERROR;
        }

        (void) ngx_sprintf(name, "%*s.%010uD%Z", to->len, to->data,
                           (uint32_t) ngx_next_temp_number(0));

        if (ngx_copy_file(src->data, name, &cf) == NGX_OK) {

            if (ngx_rename_file(name, to->data) != NGX_FILE_ERROR) {
                ngx_free(name);

                if (ngx_delete_file(src->data) == NGX_FILE_ERROR) {
                    ngx_log_error(NGX_LOG_CRIT, ext->log, ngx_errno,
                                  ngx_delete_file_n " \"%s\" failed",
                                  src->data);
                    return NGX_ERROR;
                }

                return NGX_OK;
            }

            ngx_log_error(NGX_LOG_CRIT, ext->log, ngx_errno,
                          ngx_rename_file_n " \"%s\" to \"%s\" failed",
                          name, to->data);

            if (ngx_delete_file(name) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_CRIT, ext->log, ngx_errno,
                              ngx_delete_file_n " \"%s\" failed", name);

            }
        }

        ngx_free(name);

        err = 0;
    }

failed:

    if (ext->delete_file) {
        if (ngx_delete_file(src->data) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_CRIT, ext->log, ngx_errno,
                          ngx_delete_file_n " \"%s\" failed", src->data);
        }
    }

    if (err) {
        ngx_log_error(NGX_LOG_CRIT, ext->log, err,
                      ngx_rename_file_n " \"%s\" to \"%s\" failed",
                      src->data, to->data);
    }

    return NGX_ERROR;
}


ngx_int_t
ngx_copy_file(u_char *from, u_char *to, ngx_copy_file_t *cf)
{
    char             *buf;
    off_t             size;
    size_t            len;
    ssize_t           n;
    ngx_fd_t          fd, nfd;
    ngx_int_t         rc;
    ngx_file_info_t   fi;

    rc = NGX_ERROR;
    buf = NULL;
    nfd = NGX_INVALID_FILE;

    fd = ngx_open_file(from, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);

    if (fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_CRIT, cf->log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", from);
        goto failed;
    }

    if (cf->size != -1) {
        size = cf->size;

    } else {
        if (ngx_fd_info(fd, &fi) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, cf->log, ngx_errno,
                          ngx_fd_info_n " \"%s\" failed", from);

            goto failed;
        }

        size = ngx_file_size(&fi);
    }

    len = cf->buf_size ? cf->buf_size : 65536;

    if ((off_t) len > size) {
        len = (size_t) size;
    }

    buf = ngx_alloc(len, cf->log);
    if (buf == NULL) {
        goto failed;
    }

    nfd = ngx_open_file(to, NGX_FILE_WRONLY, NGX_FILE_CREATE_OR_OPEN,
                        cf->access);

    if (nfd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_CRIT, cf->log, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", to);
        goto failed;
    }

    while (size > 0) {

        if ((off_t) len > size) {
            len = (size_t) size;
        }

        n = ngx_read_fd(fd, buf, len);

        if (n == -1) {
            ngx_log_error(NGX_LOG_ALERT, cf->log, ngx_errno,
                          ngx_read_fd_n " \"%s\" failed", from);
            goto failed;
        }

        if ((size_t) n != len) {
            ngx_log_error(NGX_LOG_ALERT, cf->log, 0,
                          ngx_read_fd_n " has read only %z of %uz from %s",
                          n, size, from);
            goto failed;
        }

        n = ngx_write_fd(nfd, buf, len);

        if (n == -1) {
            ngx_log_error(NGX_LOG_ALERT, cf->log, ngx_errno,
                          ngx_write_fd_n " \"%s\" failed", to);
            goto failed;
        }

        if ((size_t) n != len) {
            ngx_log_error(NGX_LOG_ALERT, cf->log, 0,
                          ngx_write_fd_n " has written only %z of %uz to %s",
                          n, size, to);
            goto failed;
        }

        size -= n;
    }

    if (cf->time != -1) {
        if (ngx_set_file_time(to, nfd, cf->time) != NGX_OK) {
            ngx_log_error(NGX_LOG_ALERT, cf->log, ngx_errno,
                          ngx_set_file_time_n " \"%s\" failed", to);
            goto failed;
        }
    }

    rc = NGX_OK;

failed:

    if (nfd != NGX_INVALID_FILE) {
        if (ngx_close_file(nfd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, cf->log, ngx_errno,
                          ngx_close_file_n " \"%s\" failed", to);
        }
    }

    if (fd != NGX_INVALID_FILE) {
        if (ngx_close_file(fd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, cf->log, ngx_errno,
                          ngx_close_file_n " \"%s\" failed", from);
        }
    }

    if (buf) {
        ngx_free(buf);
    }

    return rc;
}


/*
 * ctx->init_handler() - see ctx->alloc
 * ctx->file_handler() - file handler
 * ctx->pre_tree_handler() - handler is called before entering directory
 * ctx->post_tree_handler() - handler is called after leaving directory
 * ctx->spec_handler() - special (socket, FIFO, etc.) file handler
 *
 * ctx->data - some data structure, it may be the same on all levels, or
 *     reallocated if ctx->alloc is nonzero
 *
 * ctx->alloc - a size of data structure that is allocated at every level
 *     and is initialized by ctx->init_handler()
 *
 * ctx->log - a log
 *
 * on fatal (memory) error handler must return NGX_ABORT to stop walking tree
 */

/*
ngx_walk_tree是递归函数，打开每层路径(dir)直到每个文件(file)，根据其路径和文件名得到key，在缓存的rbtree(红黑树)里面找这个key(部分)，
 如果没有找到的话，就在内存中分配一个映射这个文件的node(但是不会把文件的内容进行缓存)，然后插入到红黑树中和加入队列。  
*/

/*
ctx->file_handler=>  
ngx_http_file_cache_manage_file=>  
ngx_http_file_cache_add_file=>  
ngx_http_file_cache_add  
*/

//使用 ngx_walk_tree 递归遍历缓存目录，并对不同类型的文件根据回调函数做不同的处理。
//ngx_walk_tree这个函数主要是遍历所有的cache目录，然后对于每一个cache文件调用file_handler回调。
ngx_int_t
ngx_walk_tree(ngx_tree_ctx_t *ctx, ngx_str_t *tree)
{
    void       *data, *prev;
    u_char     *p, *name;
    size_t      len;
    ngx_int_t   rc;
    ngx_err_t   err;
    ngx_str_t   file, buf;
    ngx_dir_t   dir;

    ngx_str_null(&buf);

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, ctx->log, 0,
                   "walk tree \"%V\"", tree);

    if (ngx_open_dir(tree, &dir) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_CRIT, ctx->log, ngx_errno,
                      ngx_open_dir_n " \"%s\" failed", tree->data);
        return NGX_ERROR;
    }

    prev = ctx->data;

    if (ctx->alloc) {
        data = ngx_alloc(ctx->alloc, ctx->log);
        if (data == NULL) {
            goto failed;
        }

        if (ctx->init_handler(data, prev) == NGX_ABORT) {
            goto failed;
        }

        ctx->data = data;

    } else {
        data = NULL;
    }

    for ( ;; ) {

        ngx_set_errno(0);

        if (ngx_read_dir(&dir) == NGX_ERROR) {
            err = ngx_errno;

            if (err == NGX_ENOMOREFILES) {
                rc = NGX_OK;

            } else {
                ngx_log_error(NGX_LOG_CRIT, ctx->log, err,
                              ngx_read_dir_n " \"%s\" failed", tree->data);
                rc = NGX_ERROR;
            }

            goto done;
        }

        len = ngx_de_namelen(&dir);
        name = ngx_de_name(&dir);

        ngx_log_debug2(NGX_LOG_DEBUG_CORE, ctx->log, 0,
                      "tree name %uz:\"%s\"", len, name);

        if (len == 1 && name[0] == '.') {
            continue;
        }

        if (len == 2 && name[0] == '.' && name[1] == '.') {
            continue;
        }

        file.len = tree->len + 1 + len;

        if (file.len + NGX_DIR_MASK_LEN > buf.len) {

            if (buf.len) {
                ngx_free(buf.data);
            }

            buf.len = tree->len + 1 + len + NGX_DIR_MASK_LEN;

            buf.data = ngx_alloc(buf.len + 1, ctx->log);
            if (buf.data == NULL) {
                goto failed;
            }
        }

        p = ngx_cpymem(buf.data, tree->data, tree->len);
        *p++ = '/';
        ngx_memcpy(p, name, len + 1);

        file.data = buf.data;

        ngx_log_debug1(NGX_LOG_DEBUG_CORE, ctx->log, 0,
                       "tree path \"%s\"", file.data);

        if (!dir.valid_info) {
            if (ngx_de_info(file.data, &dir) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_CRIT, ctx->log, ngx_errno,
                              ngx_de_info_n " \"%s\" failed", file.data);
                continue;
            }
        }

        if (ngx_de_is_file(&dir)) {

            ngx_log_debug1(NGX_LOG_DEBUG_CORE, ctx->log, 0,
                           "tree file \"%s\"", file.data);

            ctx->size = ngx_de_size(&dir);
            ctx->fs_size = ngx_de_fs_size(&dir);
            ctx->access = ngx_de_access(&dir);
            ctx->mtime = ngx_de_mtime(&dir);

            if (ctx->file_handler(ctx, &file) == NGX_ABORT) {
                goto failed;
            }

        } else if (ngx_de_is_dir(&dir)) {

            ngx_log_debug1(NGX_LOG_DEBUG_CORE, ctx->log, 0,
                           "tree enter dir \"%s\"", file.data);

            ctx->access = ngx_de_access(&dir);
            ctx->mtime = ngx_de_mtime(&dir);

            rc = ctx->pre_tree_handler(ctx, &file);

            if (rc == NGX_ABORT) {
                goto failed;
            }

            if (rc == NGX_DECLINED) {
                ngx_log_debug1(NGX_LOG_DEBUG_CORE, ctx->log, 0,
                               "tree skip dir \"%s\"", file.data);
                continue;
            }

            if (ngx_walk_tree(ctx, &file) == NGX_ABORT) {
                goto failed;
            }

            ctx->access = ngx_de_access(&dir);
            ctx->mtime = ngx_de_mtime(&dir);

            if (ctx->post_tree_handler(ctx, &file) == NGX_ABORT) {
                goto failed;
            }

        } else {

            ngx_log_debug1(NGX_LOG_DEBUG_CORE, ctx->log, 0,
                           "tree special \"%s\"", file.data);

            if (ctx->spec_handler(ctx, &file) == NGX_ABORT) {
                goto failed;
            }
        }
    }

failed:

    rc = NGX_ABORT;

done:

    if (buf.len) {
        ngx_free(buf.data);
    }

    if (data) {
        ngx_free(data);
        ctx->data = prev;
    }

    if (ngx_close_dir(&dir) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_CRIT, ctx->log, ngx_errno,
                      ngx_close_dir_n " \"%s\" failed", tree->data);
    }

    return rc;
}

