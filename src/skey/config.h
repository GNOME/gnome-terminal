/*
 * config.h
 *		 Copyright (C) 1995 Kazuhiko Yamamoto
 *	      Kazuhiko Yamamoto <kazu@is.aist-nara.ac.jp>
 */

/*
 * PROTOTYPE
 *      e.g. void test PROTO((a, b)) {};
 */

#ifdef __STDC__
#  define PROTO(list) list
#else
#  define PROTO(list) ()
#endif

#define private static
#define public  extern  


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#ifndef EXIT_SUCCESS
#  define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
#  define EXIT_FAILURE 1
#endif

#define RET_SUCCESS  1
#define RET_ERROR   -1

#if STDC_HEADERS
#  include <string.h>
#  define memsetzero(ptr, size) memset((ptr), 0, (size)) 
#else
#  ifndef HAVE_STRCHR
#    define strchr index
#    define strrchr rindex
#  endif
   char *strchr (), *strrchr ();
#  ifdef HAVE_MEMCPY
#    define memsetzero(ptr, size) memset((ptr), 0, (size))
#  else
#    define memcpy(d, s, n) bcopy ((s), (d), (n))
#    define memmove(d, s, n) bcopy ((s), (d), (n))
#    define memsetzero(ptr, size) bzero((ptr), (size))
#  endif
#endif

#include <signal.h>
typedef RETSIGTYPE (*sigtype)();

#ifdef HAVE_TERMIOS_H
#  include <termios.h>
#else
#  ifdef HAVE_TERMIO_H
#    include <termio.h>
#  else
#    ifdef HAVE_SYS_IOCTL_H
#      include <sys/ioctl.h>
#    endif
#    include <sgtty.h>
#  endif
#endif

#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif
#include <time.h>
#include <pwd.h>

#if SIZEOF_LONG == 1
typedef signed long INT1;
typedef unsigned long UINT1;
#elif SIZEOF_INT == 1
typedef signed int INT1;
typedef unsigned int UINT1;
#elif SIZEOF_SHORT == 1
typedef signed short INT1;
typedef unsigned short UINT1;
#elif SIZEOF_CHAR == 1
typedef signed char INT1;
typedef unsigned char UINT1;
#endif

#if SIZEOF_LONG == 2
typedef signed long INT2;
typedef unsigned long UINT2;
#elif SIZEOF_INT == 2
typedef signed int INT2;
typedef unsigned int UINT2;
#elif SIZEOF_SHORT == 2
typedef signed short INT2;
typedef unsigned short UINT2;
#elif SIZEOF_CHAR == 2
typedef signed char INT2;
typedef unsigned char UINT2;
#endif

#if SIZEOF_LONG == 4
typedef signed long INT4;
typedef unsigned long UINT4;
#elif SIZEOF_INT == 4
typedef signed int INT4;
typedef unsigned int UINT4;
#elif SIZEOF_SHORT == 4
typedef signed short INT4;
typedef unsigned short UINT4;
#elif SIZEOF_CHAR == 4
typedef signed char INT4;
typedef unsigned char UINT4;
#endif

#if SIZEOF_LONG == 8
typedef signed long INT8;
typedef unsigned long UINT8;
#elif SIZEOF_INT == 8
typedef signed int INT8;
typedef unsigned int UINT8;
#elif SIZEOF_SHORT == 8
typedef signed short INT8;
typedef unsigned short UINT8;
#elif SIZEOF_CHAR == 8
typedef signed char INT8;
typedef unsigned char UINT8;
#endif

typedef UINT1 * POINTER;

#define SUCCESS 0
#define ERROR 1

#define FILESEP    '/'
#define FILESEPLEN 1

#define MDXFinal        MD4Final
#define MDXInit         MD4Init
#define MDXUpdate       MD4Update
#define MDX_CTX         MD4_CTX

#define SEED_LEN 17
#define DEFAULT_SEQ 99		
#define SEED_HOST_LEN 2

#ifndef DefaultMD
#define DefaultMD MD4
#endif

#define PASS_PHRASE_MIN_LEN	8
#define PASS_PHRASE_MAX_LEN	256

