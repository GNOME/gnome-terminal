/*
 * config.h
 *		 Copyright (C) 1995 Kazuhiko Yamamoto
 *	      Kazuhiko Yamamoto <kazu@is.aist-nara.ac.jp>
 */

/*
 * PROTOTYPE
 *      e.g. void test PROTO((a, b)) {};
 */

/* Modified to use glib typedefs instead of custom code.
 * 11/3/2002  -  Jonathan Blandford <jrb@gnome.org>
 */

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define PROTO(list) list
#define private static
#define public  extern  

#define UINT1 guint8
#define INT1 gint8
#define UINT2 guint16
#define INT2 gint16
#define UINT4 guint32
#define INT4 gint32


#ifndef EXIT_SUCCESS
#  define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
#  define EXIT_FAILURE 1
#endif

#define RET_SUCCESS  1
#define RET_ERROR   -1


#define POINTER UINT1 *


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
