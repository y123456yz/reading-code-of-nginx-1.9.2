
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_FILES_H_INCLUDED_
#define _NGX_FILES_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef int                      ngx_fd_t;


typedef struct stat              ngx_file_info_t;//文件大小等资源信息，实际就是Linux系统定义的stat结构
typedef ino_t                    ngx_file_uniq_t;


typedef struct {
    u_char                      *name;
    size_t                       size;
    void                        *addr;
    ngx_fd_t                     fd;
    ngx_log_t                   *log;
} ngx_file_mapping_t;


typedef struct {
    DIR                         *dir;
    struct dirent               *de;
    struct stat                  info;

    unsigned                     type:8;
    unsigned                     valid_info:1;
} ngx_dir_t;


typedef struct {
    size_t                       n;
    glob_t                       pglob;
    u_char                      *pattern;
    ngx_log_t                   *log;
    ngx_uint_t                   test;
} ngx_glob_t;


#define NGX_INVALID_FILE         -1
#define NGX_FILE_ERROR           -1



#ifdef __CYGWIN__

#ifndef NGX_HAVE_CASELESS_FILESYSTEM
#define NGX_HAVE_CASELESS_FILESYSTEM  1
#endif

/*
实际上，ngx_open_file与open方法的区别不大，ngx_open_file返回的是Linux系统的文件句柄。对于打开文件的标志位，Nginx也定义了以下几个宏来加以封装。
#define NGX_FILE_RDONLY O_RDONLY
#define NGX_FILE_WRONLY O_WRONLY
#define NGX_FILE_RDWR O_RDWR
#define NGX_FILE_CREATE_OR_OPEN O_CREAT
#define NGX_FILE_OPEN 0
#define NGX_FILE_TRUNCATE O_CREAT|O_TRUNC
#define NGX_FILE_APPEND O_WRONLY|O_APPEND
#define NGX_FILE_NONBLOCK O_NONBLOCK

#define NGX_FILE_DEFAULT_ACCESS 0644
#define NGX_FILE_OWNER_ACCESS 0600
*/
#define ngx_open_file(name, mode, create, access)                            \
    open((const char *) name, mode|create|O_BINARY, access)

#else

#define ngx_open_file(name, mode, create, access)                            \
    open((const char *) name, mode|create, access)

#endif

#define ngx_open_file_n          "open()"

#define NGX_FILE_RDONLY          O_RDONLY
#define NGX_FILE_WRONLY          O_WRONLY
#define NGX_FILE_RDWR            O_RDWR
#define NGX_FILE_CREATE_OR_OPEN  O_CREAT
#define NGX_FILE_OPEN            0
#define NGX_FILE_TRUNCATE        (O_CREAT|O_TRUNC)
#define NGX_FILE_APPEND          (O_WRONLY|O_APPEND)
#define NGX_FILE_NONBLOCK        O_NONBLOCK

#if (NGX_HAVE_OPENAT)
#define NGX_FILE_NOFOLLOW        O_NOFOLLOW

#if defined(O_DIRECTORY)
#define NGX_FILE_DIRECTORY       O_DIRECTORY
#else
#define NGX_FILE_DIRECTORY       0
#endif

#if defined(O_SEARCH)
#define NGX_FILE_SEARCH          (O_SEARCH|NGX_FILE_DIRECTORY)

#elif defined(O_EXEC)
#define NGX_FILE_SEARCH          (O_EXEC|NGX_FILE_DIRECTORY)

#elif (NGX_HAVE_O_PATH)
#define NGX_FILE_SEARCH          (O_PATH|O_RDONLY|NGX_FILE_DIRECTORY)

#else
#define NGX_FILE_SEARCH          (O_RDONLY|NGX_FILE_DIRECTORY)
#endif

#endif /* NGX_HAVE_OPENAT */

/*
    -rw-rw-r--
　　一共有10位数
　　其中： 最前面那个 - 代表的是类型
　　中间那三个 rw- 代表的是所有者（user）
　　然后那三个 rw- 代表的是组群（group）
　　最后那三个 r-- 代表的是其他人（other）
*/
#define NGX_FILE_DEFAULT_ACCESS  0644 //-rw-r--r--  所有者有读写权限 组群有读写权限，其他人只有读权限
#define NGX_FILE_OWNER_ACCESS    0600 //-rw-------


#define ngx_close_file           close
#define ngx_close_file_n         "close()"

//unlink 只能删除文件，不能删除目录
#define ngx_delete_file(name)    unlink((const char *) name)
#define ngx_delete_file_n        "unlink()"


ngx_fd_t ngx_open_tempfile(u_char *name, ngx_uint_t persistent,
    ngx_uint_t access);
#define ngx_open_tempfile_n      "open()"


ssize_t ngx_read_file(ngx_file_t *file, u_char *buf, size_t size, off_t offset);
#if (NGX_HAVE_PREAD)
#define ngx_read_file_n          "pread()"
#else
#define ngx_read_file_n          "read()"
#endif

ssize_t ngx_write_file(ngx_file_t *file, u_char *buf, size_t size,
    off_t offset);

ssize_t ngx_write_chain_to_file(ngx_file_t *file, ngx_chain_t *ce,
    off_t offset, ngx_pool_t *pool);


#define ngx_read_fd              read
#define ngx_read_fd_n            "read()"

/*
 * we use inlined function instead of simple #define
 * because glibc 2.3 sets warn_unused_result attribute for write()
 * and in this case gcc 4.3 ignores (void) cast
 */
static ngx_inline ssize_t
ngx_write_fd(ngx_fd_t fd, void *buf, size_t n)
{
    return write(fd, buf, n);
}

#define ngx_write_fd_n           "write()"


#define ngx_write_console        ngx_write_fd


#define ngx_linefeed(p)          *p++ = LF;
#define NGX_LINEFEED_SIZE        1
#define NGX_LINEFEED             "\x0a"

/* rename函数功能是给一个文件重命名，用该函数可以实现文件移动功能，把一个文件的完整路径的盘符改一下就实现了这个文件的移动 */
//rename和mv命令差不多，只是rename支持批量修改  也就是说，mv也能用于改名，但不能实现批量处理（改名时，不支持*等符号的），而rename可以。
//rename后文件的fd和stat信息不变
#define ngx_rename_file(o, n)    rename((const char *) o, (const char *) n)
#define ngx_rename_file_n        "rename()"


#define ngx_change_file_access(n, a) chmod((const char *) n, a)
#define ngx_change_file_access_n "chmod()"


ngx_int_t ngx_set_file_time(u_char *name, ngx_fd_t fd, time_t s);
#define ngx_set_file_time_n      "utimes()"

/*
stat系统调用系列包括了fstat、stat和lstat，它们都是用来返回“相关文件状态信息”的，三者的不同之处在于设定源文件的方式不同。

我们已经学习完了struct stat和各种st_mode相关宏，现在就可以拿它们和stat系统调用相互配合工作了！

int fstat(int filedes, struct stat *buf);
int stat(const char *path, struct stat *buf);
int lstat(const char *path, struct stat *buf);
聪明人一眼就能看出来fstat的第一个参数是和另外两个不一样的，对！fstat区别于另外两个系统调用的地方在于，fstat系统调用接受的是 一个“文件描述符”，
而另外两个则直接接受“文件全路径”。文件描述符是需要我们用open系统调用后才能得到的，而文件全路经直接写就可以了。

stat和lstat的区别：当文件是一个符号链接时，lstat返回的是该符号链接本身的信息；而stat返回的是该链接指向的文件的信息。（似乎有些晕吧，这
样记，lstat比stat多了一个l，因此它是有本事处理符号链接文件的，因此当遇到符号链接文件时，lstat当然不会放过。而 stat系统调用没有这个本事，
它只能对符号链接文件睁一只眼闭一只眼，直接去处理链接所指文件喽） 
*/

#define ngx_file_info(file, sb)  stat((const char *) file, sb)
#define ngx_file_info_n          "stat()"

//fstat()用来将参数fildes所指的文件状态，复制到参数buf所指的
#define ngx_fd_info(fd, sb)      fstat(fd, sb) //返回“相关文件状态信息”的
#define ngx_fd_info_n            "fstat()"

#define ngx_link_info(file, sb)  lstat((const char *) file, sb)
#define ngx_link_info_n          "lstat()"

/*
struct stat  
10.{  
11.  
12.    dev_t       st_dev;     / * ID of device containing file -文件所在设备的ID* /  
13.  
14.    ino_t       st_ino;     / * inode number -inode节点号* /  
15.  
16.    mode_t      st_mode;    / * protection -保护模式?* /  
17.  
18.    nlink_t     st_nlink;   / * number of hard links -链向此文件的连接数(硬连接)* /  
19.  
20.    uid_t       st_uid;     / * user ID of owner -user id* /  
21.  
22.    gid_t       st_gid;     / * group ID of owner - group id* /  
23.  
24.    dev_t       st_rdev;    / * device ID (if special file) -设备号，针对设备文件* /  
25.  
26.    off_t       st_size;    / * total size, in bytes -文件大小，字节为单位* /  
27.  
28.    blksize_t   st_blksize; / * blocksize for filesystem I/O -系统块的大小* /  
29.  
30.    blkcnt_t    st_blocks;  / * number of blocks allocated -文件所占块数* /  
31.  
32.    time_t      st_atime;   / * time of last access -最近存取时间* /  
33.  
34.    time_t      st_mtime;   / * time of last modification -最近修改时间* /  
35.  
36.    time_t      st_ctime;   / * time of last status change - * /  
37.  
38.};  
*/
#define ngx_is_dir(sb)           (S_ISDIR((sb)->st_mode))
#define ngx_is_file(sb)          (S_ISREG((sb)->st_mode))
#define ngx_is_link(sb)          (S_ISLNK((sb)->st_mode))
#define ngx_is_exec(sb)          (((sb)->st_mode & S_IXUSR) == S_IXUSR)
#define ngx_file_access(sb)      ((sb)->st_mode & 0777)
#define ngx_file_size(sb)        (sb)->st_size
#define ngx_file_fs_size(sb)     ngx_max((sb)->st_size, (sb)->st_blocks * 512)
#define ngx_file_mtime(sb)       (sb)->st_mtime
/* inode number -inode节点号*/  
//同一个设备中的每个文件，这个值都是不同的
#define ngx_file_uniq(sb)        (sb)->st_ino  


ngx_int_t ngx_create_file_mapping(ngx_file_mapping_t *fm);
void ngx_close_file_mapping(ngx_file_mapping_t *fm);


#define ngx_realpath(p, r)       (u_char *) realpath((char *) p, (char *) r)
#define ngx_realpath_n           "realpath()"

//getcwd()会将当前工作目录的绝对路径复制到参数buf所指的内存空间中,参数size为buf的空间大小
#define ngx_getcwd(buf, size)    (getcwd((char *) buf, size) != NULL)
#define ngx_getcwd_n             "getcwd()"
#define ngx_path_separator(c)    ((c) == '/')


#if defined(PATH_MAX)

#define NGX_HAVE_MAX_PATH        1
#define NGX_MAX_PATH             PATH_MAX

#else

#define NGX_MAX_PATH             4096

#endif


#define NGX_DIR_MASK_LEN         0


ngx_int_t ngx_open_dir(ngx_str_t *name, ngx_dir_t *dir);
#define ngx_open_dir_n           "opendir()"


#define ngx_close_dir(d)         closedir((d)->dir)
#define ngx_close_dir_n          "closedir()"


ngx_int_t ngx_read_dir(ngx_dir_t *dir);
#define ngx_read_dir_n           "readdir()"


#define ngx_create_dir(name, access) mkdir((const char *) name, access)
#define ngx_create_dir_n         "mkdir()"


#define ngx_delete_dir(name)     rmdir((const char *) name)
#define ngx_delete_dir_n         "rmdir()"


#define ngx_dir_access(a)        (a | (a & 0444) >> 2)


#define ngx_de_name(dir)         ((u_char *) (dir)->de->d_name)
#if (NGX_HAVE_D_NAMLEN)
#define ngx_de_namelen(dir)      (dir)->de->d_namlen
#else
#define ngx_de_namelen(dir)      ngx_strlen((dir)->de->d_name)
#endif

static ngx_inline ngx_int_t
ngx_de_info(u_char *name, ngx_dir_t *dir)
{
    dir->type = 0;
    return stat((const char *) name, &dir->info);
}

#define ngx_de_info_n            "stat()"
#define ngx_de_link_info(name, dir)  lstat((const char *) name, &(dir)->info)
#define ngx_de_link_info_n       "lstat()"

#if (NGX_HAVE_D_TYPE)

/*
 * some file systems (e.g. XFS on Linux and CD9660 on FreeBSD)
 * do not set dirent.d_type
 */

#define ngx_de_is_dir(dir)                                                   \
    (((dir)->type) ? ((dir)->type == DT_DIR) : (S_ISDIR((dir)->info.st_mode)))
#define ngx_de_is_file(dir)                                                  \
    (((dir)->type) ? ((dir)->type == DT_REG) : (S_ISREG((dir)->info.st_mode)))
#define ngx_de_is_link(dir)                                                  \
    (((dir)->type) ? ((dir)->type == DT_LNK) : (S_ISLNK((dir)->info.st_mode)))

#else

#define ngx_de_is_dir(dir)       (S_ISDIR((dir)->info.st_mode))
#define ngx_de_is_file(dir)      (S_ISREG((dir)->info.st_mode))
#define ngx_de_is_link(dir)      (S_ISLNK((dir)->info.st_mode))

#endif

#define ngx_de_access(dir)       (((dir)->info.st_mode) & 0777)
#define ngx_de_size(dir)         (dir)->info.st_size
#define ngx_de_fs_size(dir)                                                  \
    ngx_max((dir)->info.st_size, (dir)->info.st_blocks * 512)
#define ngx_de_mtime(dir)        (dir)->info.st_mtime


ngx_int_t ngx_open_glob(ngx_glob_t *gl);
#define ngx_open_glob_n          "glob()"
ngx_int_t ngx_read_glob(ngx_glob_t *gl, ngx_str_t *name);
void ngx_close_glob(ngx_glob_t *gl);


ngx_err_t ngx_trylock_fd(ngx_fd_t fd);
ngx_err_t ngx_lock_fd(ngx_fd_t fd);
ngx_err_t ngx_unlock_fd(ngx_fd_t fd);

#define ngx_trylock_fd_n         "fcntl(F_SETLK, F_WRLCK)"
#define ngx_lock_fd_n            "fcntl(F_SETLKW, F_WRLCK)"
#define ngx_unlock_fd_n          "fcntl(F_SETLK, F_UNLCK)"


#if (NGX_HAVE_F_READAHEAD)

#define NGX_HAVE_READ_AHEAD      1

#define ngx_read_ahead(fd, n)    fcntl(fd, F_READAHEAD, (int) n)
#define ngx_read_ahead_n         "fcntl(fd, F_READAHEAD)"

#elif (NGX_HAVE_POSIX_FADVISE)

#define NGX_HAVE_READ_AHEAD      1

ngx_int_t ngx_read_ahead(ngx_fd_t fd, size_t n);
#define ngx_read_ahead_n         "posix_fadvise(POSIX_FADV_SEQUENTIAL)"

#else

#define ngx_read_ahead(fd, n)    0
#define ngx_read_ahead_n         "ngx_read_ahead_n"

#endif


#if (NGX_HAVE_O_DIRECT)

ngx_int_t ngx_directio_on(ngx_fd_t fd);
#define ngx_directio_on_n        "fcntl(O_DIRECT)"

ngx_int_t ngx_directio_off(ngx_fd_t fd);
#define ngx_directio_off_n       "fcntl(!O_DIRECT)"

#elif (NGX_HAVE_F_NOCACHE)

#define ngx_directio_on(fd)      fcntl(fd, F_NOCACHE, 1)
#define ngx_directio_on_n        "fcntl(F_NOCACHE, 1)"

#elif (NGX_HAVE_DIRECTIO)

#define ngx_directio_on(fd)      directio(fd, DIRECTIO_ON)
#define ngx_directio_on_n        "directio(DIRECTIO_ON)"

#else

#define ngx_directio_on(fd)      0
#define ngx_directio_on_n        "ngx_directio_on_n"

#endif

size_t ngx_fs_bsize(u_char *name);


#if (NGX_HAVE_OPENAT)

#define ngx_openat_file(fd, name, mode, create, access)                      \
    openat(fd, (const char *) name, mode|create, access)

#define ngx_openat_file_n        "openat()"

#define ngx_file_at_info(fd, name, sb, flag)                                 \
    fstatat(fd, (const char *) name, sb, flag)

#define ngx_file_at_info_n       "fstatat()"

#define NGX_AT_FDCWD             (ngx_fd_t) AT_FDCWD

#endif


#define ngx_stdout               STDOUT_FILENO
#define ngx_stderr               STDERR_FILENO
#define ngx_set_stderr(fd)       dup2(fd, STDERR_FILENO) //dup2和dup都可用来复制一个现存的 文件描述符，使两个文件描述符指向同一个file 结构体。
#define ngx_set_stderr_n         "dup2(STDERR_FILENO)"


#if (NGX_HAVE_FILE_AIO)

ngx_int_t ngx_file_aio_init(ngx_file_t *file, ngx_pool_t *pool);
ssize_t ngx_file_aio_read(ngx_file_t *file, u_char *buf, size_t size,
    off_t offset, ngx_pool_t *pool);

extern ngx_uint_t  ngx_file_aio;

#endif

#if (NGX_THREADS)
ssize_t ngx_thread_read(ngx_thread_task_t **taskp, ngx_file_t *file,
    u_char *buf, size_t size, off_t offset, ngx_pool_t *pool);
#endif


#endif /* _NGX_FILES_H_INCLUDED_ */
