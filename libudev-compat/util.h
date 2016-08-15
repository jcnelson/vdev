/*
 * util.c (only the parts we need, as well as stuff from systemd/src/ *-util.h and systemd/src/macro.h)
 * 
 * Forked from systemd/src/shared/util.c on March 26, 2015
 * 
 * All modifications to the original source file are Copyright (C) 2015 Jude Nelson <judecn@gmail.com>
 *
 * Original copyright and license text produced below.
 */

/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#ifndef _LIBUDEV_COMPAT_UTIL_H_
#define _LIBUDEV_COMPAT_UTIL_H_

#include <assert.h>
#include <alloca.h>
#include <fcntl.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sched.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/resource.h>
#include <stddef.h>
#include <unistd.h>
#include <locale.h>
#include <mntent.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/syscall.h>	// for gettid()

#include "log.h"

#define _printf_(a,b) __attribute__ ((format (printf, a, b)))
#define _alloc_(...) __attribute__ ((alloc_size(__VA_ARGS__)))
#define _sentinel_ __attribute__ ((sentinel))
#define _unused_ __attribute__ ((unused))
#define _destructor_ __attribute__ ((destructor))
#define _pure_ __attribute__ ((pure))
#define _const_ __attribute__ ((const))
#define _deprecated_ __attribute__ ((deprecated))
#define _packed_ __attribute__ ((packed))
#define _malloc_ __attribute__ ((malloc))
#define _weak_ __attribute__ ((weak))
#define _likely_(x) (__builtin_expect(!!(x),1))
#define _unlikely_(x) (__builtin_expect(!!(x),0))
#define _public_ __attribute__ ((visibility("default")))
#define _hidden_ __attribute__ ((visibility("hidden")))
#define _weakref_(x) __attribute__((weakref(#x)))
#define _alignas_(x) __attribute__((aligned(__alignof(x))))
#define _cleanup_(x) __attribute__((cleanup(x)))

#define memzero(x,l) (memset((x), 0, (l)))

#define streq(a,b) (strcmp((a),(b)) == 0)

bool streq_ptr (const char *a, const char *b);

#define strneq(a, b, n) (strncmp((a), (b), (n)) == 0)

#define new(t, n) ((t*) malloc_multiply(sizeof(t), (n)))

#define new0(t, n) ((t*) calloc((n), sizeof(t)))

#define newa(t, n) ((t*) alloca(sizeof(t)*(n)))

#define newa0(t, n) ((t*) alloca0(sizeof(t)*(n)))

#define newdup(t, p, n) ((t*) memdup_multiply(p, sizeof(t), (n)))

#define malloc0(n) (calloc((n), 1))

#define ELEMENTSOF(x) (sizeof(x)/sizeof((x)[0]))

#define libudev_safe_free( x ) do { if( (x) != NULL ) { free( x ); x = NULL; } } while( 0 )

#define UID_INVALID ((uid_t) -1)
#define GID_INVALID ((gid_t) -1)
#define MODE_INVALID ((mode_t) -1)

// POSIX defines pid_t and gid_t as "signed integer types"
#define UID_FMT "%d"
#define GID_FMT "%d"

// this has been the case for a while...
#define SIZEOF_DEV_T 8

#define ALIGN4(l) (((l) + 3) & ~3)
#define ALIGN8(l) (((l) + 7) & ~7)

// Linux uses LP64
#ifdef __LP64__
#define ALIGN(l) ALIGN8(l)
#else
#define ALIGN(l) ALIGN4(l)
#endif

#ifndef MAX_HANDLE_SZ
#define MAX_HANDLE_SZ	128

struct file_handle
{
  unsigned int handle_bytes;
  int handle_type;
  /* File identifier.  */
  unsigned char f_handle[0];
};
#endif

union file_handle_union
{
  struct file_handle handle;
  char padding[sizeof (struct file_handle) + MAX_HANDLE_SZ];
};
#define FILE_HANDLE_INIT { .handle.handle_bytes = MAX_HANDLE_SZ }

#define FOREACH_LINE(line, f, on_error)                         \
        for (;;)                                                \
                if (!fgets(line, sizeof(line), f)) {            \
                        if (ferror(f)) {                        \
                                on_error;                       \
                        }                                       \
                        break;                                  \
                } else

static inline size_t
ALIGN_TO (size_t l, size_t ali)
{
  return ((l + ali - 1) & ~(ali - 1));
}

size_t
page_size (void)
  _pure_;
#define PAGE_ALIGN(l) ALIGN_TO((l), page_size())

////////// reference counting 

     typedef struct
     {
       volatile unsigned _value;
     } RefCount;

#define REFCNT_GET(r) ((r)._value)
#define REFCNT_INC(r) (__sync_add_and_fetch(&(r)._value, 1))
#define REFCNT_DEC(r) (__sync_sub_and_fetch(&(r)._value, 1))

#define REFCNT_INIT ((RefCount) { ._value = 1 })

////////// time functions 

     typedef uint64_t usec_t;
     typedef uint64_t nsec_t;

#define NSEC_FMT "%" PRIu64
#define USEC_FMT "%" PRIu64

#define USEC_INFINITY ((usec_t) -1)
#define NSEC_INFINITY ((nsec_t) -1)

#define MSEC_PER_SEC  1000ULL
#define USEC_PER_SEC  ((usec_t) 1000000ULL)
#define USEC_PER_MSEC ((usec_t) 1000ULL)
#define NSEC_PER_SEC  ((nsec_t) 1000000000ULL)
#define NSEC_PER_MSEC ((nsec_t) 1000000ULL)
#define NSEC_PER_USEC ((nsec_t) 1000ULL)

     usec_t now (clockid_t clock);
     usec_t timespec_load (const struct timespec *ts);
     struct timespec *timespec_store (struct timespec *ts, usec_t u);

////////// string functions 

     static inline char *startswith (const char *s, const char *prefix)
{
  size_t l;

  l = strlen (prefix);
  if (strncmp (s, prefix, l) == 0)
    return (char *) s + l;

  return NULL;
}

#define strjoina(a, ...)                                                \
        ({                                                              \
                const char *_appendees_[] = { a, __VA_ARGS__ };         \
                char *_d_, *_p_;                                        \
                int _len_ = 0;                                          \
                unsigned _i_;                                           \
                for (_i_ = 0; _i_ < ELEMENTSOF(_appendees_) && _appendees_[_i_]; _i_++) \
                        _len_ += strlen(_appendees_[_i_]);              \
                _p_ = _d_ = alloca(_len_ + 1);                          \
                for (_i_ = 0; _i_ < ELEMENTSOF(_appendees_) && _appendees_[_i_]; _i_++) \
                        _p_ = stpcpy(_p_, _appendees_[_i_]);            \
                *_p_ = 0;                                               \
                _d_;                                                    \
        })

/* Returns the number of chars needed to format variables of the
 * specified type as a decimal string. Adds in extra space for a
 * negative '-' prefix (hence works correctly on signed
 * types). Includes space for the trailing NUL. */
#define DECIMAL_STR_MAX(type)                                           \
        (2+(sizeof(type) <= 1 ? 3 :                                     \
            sizeof(type) <= 2 ? 5 :                                     \
            sizeof(type) <= 4 ? 10 :                                    \
            sizeof(type) <= 8 ? 20 : sizeof(int[-2*(sizeof(type) > 8)])))

char *dirname_malloc (const char *path);
char
hexchar (int x)
  _const_;
     int unhexchar (char c) _const_;

#define NULSTR_FOREACH(i, l)                                    \
        for ((i) = (l); (i) && *(i); (i) = strchr((i), 0)+1)

#define STRV_FOREACH(s, l)                      \
        for ((s) = (l); (s) && *(s); (s)++)

//////////// memory functions

#define _cleanup_(x) __attribute__((cleanup(x)))

#define DEFINE_TRIVIAL_CLEANUP_FUNC(type, func)                 \
        static inline void func##p(type *p) {                   \
                if (*p)                                         \
                        func(*p);                               \
        }                                                       \
        struct __useless_struct_to_allow_trailing_semicolon__

DEFINE_TRIVIAL_CLEANUP_FUNC (FILE *, fclose);
DEFINE_TRIVIAL_CLEANUP_FUNC (FILE *, pclose);
DEFINE_TRIVIAL_CLEANUP_FUNC (DIR *, closedir);
DEFINE_TRIVIAL_CLEANUP_FUNC (FILE *, endmntent);

#define _cleanup_free_ _cleanup_(freep)
#define _cleanup_close_ _cleanup_(closep)
#define _cleanup_umask_ _cleanup_(umaskp)
#define _cleanup_globfree_ _cleanup_(globfree)
#define _cleanup_fclose_ _cleanup_(fclosep)
#define _cleanup_pclose_ _cleanup_(pclosep)
#define _cleanup_closedir_ _cleanup_(closedirp)
#define _cleanup_endmntent_ _cleanup_(endmntentp)
#define _cleanup_close_pair_ _cleanup_(close_pairp)

     _malloc_ _alloc_ (1, 2)
     static inline void *malloc_multiply (size_t a, size_t b)
{
  if (_unlikely_ (b != 0 && a > ((size_t) - 1) / b))
    return NULL;

  return malloc (a * b);
}

_alloc_ (2, 3)
     static inline void *realloc_multiply (void *p, size_t a, size_t b)
{
  if (_unlikely_ (b != 0 && a > ((size_t) - 1) / b))
    return NULL;

  return realloc (p, a * b);
}

////////// sanity checks

#define assert_return(expr, r)                                          \
        do {                                                            \
                if (_unlikely_(!(expr))) {                              \
                        log_error("Assertion '%s' failed", #expr); \
                        return (r);                                     \
                }                                                       \
        } while (false)

#define assert_se(expr)                                                 \
        do {                                                            \
                if (_unlikely_(!(expr)))                                \
                        log_error("Assertion failure: '%s'", #expr); \
        } while (false)

#define DISABLE_WARNING_DECLARATION_AFTER_STATEMENT                     \
        _Pragma("GCC diagnostic push");                                 \
        _Pragma("GCC diagnostic ignored \"-Wdeclaration-after-statement\"")

#define REENABLE_WARNING                                                \
        _Pragma("GCC diagnostic pop")

#if defined(static_assert)
/* static_assert() is sometimes defined in a way that trips up
 * -Wdeclaration-after-statement, hence let's temporarily turn off
 * this warning around it. */
#define assert_cc(expr)                                                 \
        DISABLE_WARNING_DECLARATION_AFTER_STATEMENT;                    \
        static_assert(expr, #expr);                                     \
        REENABLE_WARNING
#else
#define assert_cc(expr)                                                 \
        DISABLE_WARNING_DECLARATION_AFTER_STATEMENT;                    \
        struct CONCATENATE(_assert_struct_, __COUNTER__) {              \
                char x[(expr) ? 0 : -1];                                \
        };                                                              \
        REENABLE_WARNING
#endif

#define assert_not_reached(t)                                           \
        do {                                                            \
                log_error("Not reached: %s", t); \
        } while (false)

////////// type functions 

#define MIN( a, b ) ((a) < (b) ? (a) : (b))
#define MAX( a, b ) ((a) > (b) ? (a) : (b))
#define XCONCATENATE(x, y) x ## y
#define CONCATENATE(x, y) XCONCATENATE(x, y)

#define UNIQ_T(x, uniq) CONCATENATE(__unique_prefix_, CONCATENATE(x, uniq))
#define UNIQ __COUNTER__

#define __container_of(uniq, ptr, type, member)                         \
        __extension__ ({                                                \
                const typeof( ((type*)0)->member ) *UNIQ_T(A, uniq) = (ptr); \
                (type*)( (char *)UNIQ_T(A, uniq) - offsetof(type,member) ); \
        })

#define container_of(ptr, type, member) __container_of(UNIQ, (ptr), type, member)

///////// misc inline functions 

/**
 * Normal qsort requires base to be nonnull. Here were require
 * that only if nmemb > 0.
 */
static inline void
qsort_safe (void *base, size_t nmemb, size_t size,
	    int (*compar) (const void *, const void *))
{
  if (nmemb)
    {
      assert (base);
      qsort (base, nmemb, size, compar);
    }
}

static inline void *
mempset (void *s, int c, size_t n)
{
  memset (s, c, n);
  return (uint8_t *) s + n;
}

static inline unsigned
log2u (unsigned x)
{
  assert (x > 0);

  return sizeof (unsigned) * 8 - __builtin_clz (x) - 1;
}

static inline unsigned
log2u_round_up (unsigned x)
{
  assert (x > 0);

  if (x == 1)
    return 0;

  return log2u (x - 1) + 1;
}

///////// misc functions 
int chmod_and_chown (const char *path, mode_t mode, uid_t uid, gid_t gid);
int is_dir (const char *path, bool follow);
int close_nointr (int fd);
int flush_fd (int fd);
int safe_close (int fd);

int dev_urandom (void *p, size_t n);
void initialize_srand (void);
void random_bytes (void *p, size_t n);

bool is_main_thread (void);

#endif
