
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_ATOMIC_H_INCLUDED_
#define _NGX_ATOMIC_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

/*
能够执行原子操作的原子变量只有整型，包括无符号整型ngx_atomic_ uint_t和有符号整型ngx_atomic_t，这两种类型都使用了volatile关键字
告诉C编译器不要做优化

想要使用原子操作来修改、获取整型变量，自然不能使用加减号，而要使用Nginx提供的两个方法：ngx_atomic_cmp_set和ngx_atomic_fetch_add。
这两个方法都可以用来修改原子变量的值，而ngx_atomic_cmp_set方法同时还可以比较原子变量的值
*/

#if (NGX_HAVE_LIBATOMIC)

#define AO_REQUIRE_CAS
#include <atomic_ops.h>

#define NGX_HAVE_ATOMIC_OPS  1

typedef long                        ngx_atomic_int_t;

/*
Most operations operate on values of type AO_T, which are unsigned integers
whose size matches that of pointers on the given architecture.
*/
typedef AO_t                        ngx_atomic_uint_t;
typedef volatile ngx_atomic_uint_t  ngx_atomic_t;

#if (NGX_PTR_SIZE == 8)
#define NGX_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)
#else
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)
#endif

#define ngx_atomic_cmp_set(lock, old, new)                                    \
    AO_compare_and_swap(lock, old, new)
#define ngx_atomic_fetch_add(value, add)                                      \
    AO_fetch_and_add(value, add)
#define ngx_memory_barrier()        AO_nop()
#define ngx_cpu_pause()


#elif (NGX_DARWIN_ATOMIC)

/*
 * use Darwin 8 atomic(3) and barrier(3) operations
 * optimized at run-time for UP and SMP
 */

#include <libkern/OSAtomic.h>

/* "bool" conflicts with perl's CORE/handy.h */
#if 0
#undef bool
#endif


#define NGX_HAVE_ATOMIC_OPS  1

#if (NGX_PTR_SIZE == 8)

typedef int64_t                     ngx_atomic_int_t;
typedef uint64_t                    ngx_atomic_uint_t;
#define NGX_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)

#define ngx_atomic_cmp_set(lock, old, new)                                    \
    OSAtomicCompareAndSwap64Barrier(old, new, (int64_t *) lock)

#define ngx_atomic_fetch_add(value, add)                                      \
    (OSAtomicAdd64(add, (int64_t *) value) - add)

#else

typedef int32_t                     ngx_atomic_int_t;
typedef uint32_t                    ngx_atomic_uint_t;
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)

#define ngx_atomic_cmp_set(lock, old, new)                                    \
    OSAtomicCompareAndSwap32Barrier(old, new, (int32_t *) lock)

#define ngx_atomic_fetch_add(value, add)                                      \
    (OSAtomicAdd32(add, (int32_t *) value) - add)

#endif

#define ngx_memory_barrier()        OSMemoryBarrier()

#define ngx_cpu_pause()

typedef volatile ngx_atomic_uint_t  ngx_atomic_t;


#elif (NGX_HAVE_GCC_ATOMIC)

/* GCC 4.1 builtin atomic operations */

#define NGX_HAVE_ATOMIC_OPS  1  //走这里，表示系统支持原子操作

typedef long                        ngx_atomic_int_t;
typedef unsigned long               ngx_atomic_uint_t;

#if (NGX_PTR_SIZE == 8)
#define NGX_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)
#else
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)
#endif

typedef volatile ngx_atomic_uint_t  ngx_atomic_t;


#define ngx_atomic_cmp_set(lock, old, set)                                    \
    __sync_bool_compare_and_swap(lock, old, set)

#define ngx_atomic_fetch_add(value, add)                                      \
    __sync_fetch_and_add(value, add)

#define ngx_memory_barrier()        __sync_synchronize()

#if ( __i386__ || __i386 || __amd64__ || __amd64 )
#define ngx_cpu_pause()             __asm__ ("pause")
#else
#define ngx_cpu_pause()
#endif


#elif ( __i386__ || __i386 )

typedef int32_t                     ngx_atomic_int_t;
typedef uint32_t                    ngx_atomic_uint_t;
typedef volatile ngx_atomic_uint_t  ngx_atomic_t;
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)


#if ( __SUNPRO_C )

#define NGX_HAVE_ATOMIC_OPS  1

ngx_atomic_uint_t
ngx_atomic_cmp_set(ngx_atomic_t *lock, ngx_atomic_uint_t old,
    ngx_atomic_uint_t set);

ngx_atomic_int_t
ngx_atomic_fetch_add(ngx_atomic_t *value, ngx_atomic_int_t add);

/*
 * Sun Studio 12 exits with segmentation fault on '__asm ("pause")',
 * so ngx_cpu_pause is declared in src/os/unix/ngx_sunpro_x86.il
 */

void
ngx_cpu_pause(void);

/* the code in src/os/unix/ngx_sunpro_x86.il */

#define ngx_memory_barrier()        __asm (".volatile"); __asm (".nonvolatile")


#else /* ( __GNUC__ || __INTEL_COMPILER ) */

#define NGX_HAVE_ATOMIC_OPS  1

#include "ngx_gcc_atomic_x86.h"

#endif


#elif ( __amd64__ || __amd64 )

typedef int64_t                     ngx_atomic_int_t;
typedef uint64_t                    ngx_atomic_uint_t;
typedef volatile ngx_atomic_uint_t  ngx_atomic_t;
#define NGX_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)


#if ( __SUNPRO_C )

#define NGX_HAVE_ATOMIC_OPS  1

ngx_atomic_uint_t
ngx_atomic_cmp_set(ngx_atomic_t *lock, ngx_atomic_uint_t old,
    ngx_atomic_uint_t set);

ngx_atomic_int_t
ngx_atomic_fetch_add(ngx_atomic_t *value, ngx_atomic_int_t add);

/*
 * Sun Studio 12 exits with segmentation fault on '__asm ("pause")',
 * so ngx_cpu_pause is declared in src/os/unix/ngx_sunpro_amd64.il
 */

void
ngx_cpu_pause(void);

/* the code in src/os/unix/ngx_sunpro_amd64.il */

#define ngx_memory_barrier()        __asm (".volatile"); __asm (".nonvolatile")


#else /* ( __GNUC__ || __INTEL_COMPILER ) */

#define NGX_HAVE_ATOMIC_OPS  1

#include "ngx_gcc_atomic_amd64.h"

#endif


#elif ( __sparc__ || __sparc || __sparcv9 )

#if (NGX_PTR_SIZE == 8)

typedef int64_t                     ngx_atomic_int_t;
typedef uint64_t                    ngx_atomic_uint_t;
#define NGX_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)

#else

typedef int32_t                     ngx_atomic_int_t;
typedef uint32_t                    ngx_atomic_uint_t;
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)

#endif

typedef volatile ngx_atomic_uint_t  ngx_atomic_t;


#if ( __SUNPRO_C )

#define NGX_HAVE_ATOMIC_OPS  1

#include "ngx_sunpro_atomic_sparc64.h"


#else /* ( __GNUC__ || __INTEL_COMPILER ) */

#define NGX_HAVE_ATOMIC_OPS  1

#include "ngx_gcc_atomic_sparc64.h"

#endif


#elif ( __powerpc__ || __POWERPC__ )

#define NGX_HAVE_ATOMIC_OPS  1

#if (NGX_PTR_SIZE == 8)

typedef int64_t                     ngx_atomic_int_t;
typedef uint64_t                    ngx_atomic_uint_t;
#define NGX_ATOMIC_T_LEN            (sizeof("-9223372036854775808") - 1)

#else

typedef int32_t                     ngx_atomic_int_t;
typedef uint32_t                    ngx_atomic_uint_t;
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)

#endif

typedef volatile ngx_atomic_uint_t  ngx_atomic_t;


#include "ngx_gcc_atomic_ppc.h"

#endif


#if !(NGX_HAVE_ATOMIC_OPS)

#define NGX_HAVE_ATOMIC_OPS  0

typedef int32_t                     ngx_atomic_int_t;
typedef uint32_t                    ngx_atomic_uint_t;
/*
所谓原子操作，就是该操作绝不会在执行完毕前被任何其他任务或事件打断，也就说，它的最小的执行单位，不可能有比它更小的执行单位，因此
这里的原子实际是使用了物理学里的物质微粒的概念。

　　原子操作需要硬件的支持，因此是架构相关的，其API和原子类型的定义都定义在内核源码树的include/asm/atomic.h文件中，它们都使用
汇编语言实现，因为C语言并不能实现这样的操作。
　　原子操作主要用于实现资源计数，很多引用计数(refcnt)就是通过原子操作实现的。原子类型定义如下：
typedef struct
 {
 volatile int counter;
 } atomic_t; 

　　volatile修饰字段告诉gcc不要对该类型的数据做优化处理，对它的访问都是对内存的访问，而不是对寄存器的访问。
*/
typedef volatile ngx_atomic_uint_t  ngx_atomic_t;
#define NGX_ATOMIC_T_LEN            (sizeof("-2147483648") - 1)

/*
ngx_atomic_cmp_set万法会将old参数与原子变量lock的值做比较，如果它们相等，则把lock设为参数set，同时方法返回1；如果它们不相等，则不做任何修改，返回0
*/
static ngx_inline ngx_atomic_uint_t
ngx_atomic_cmp_set(ngx_atomic_t *lock, ngx_atomic_uint_t old,
     ngx_atomic_uint_t set)
{
     if (*lock == old) {
         *lock = set;
         return 1;
     }

     return 0;
}

/*
原子变量value的值加上参数add，同时返回之前value的值。
*/
static ngx_inline ngx_atomic_int_t
ngx_atomic_fetch_add(ngx_atomic_t *value, ngx_atomic_int_t add)
{
     ngx_atomic_int_t  old;
     old = *value;
     *value += add;

     return old;
}

#define ngx_memory_barrier()
#define ngx_cpu_pause()

#endif


void ngx_spinlock(ngx_atomic_t *lock, ngx_atomic_int_t value, ngx_uint_t spin);

#define ngx_trylock(lock)  (*(lock) == 0 && ngx_atomic_cmp_set(lock, 0, 1))
#define ngx_unlock(lock)    *(lock) = 0


#endif /* _NGX_ATOMIC_H_INCLUDED_ */
