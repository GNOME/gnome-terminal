/*
 * md.h
 *		 Copyright (C) 1995 Kazuhiko Yamamoto
 *	      Kazuhiko Yamamoto <kazu@is.aist-nara.ac.jp>
 */

#include "config.h"
#define MD2 0
#define MD4 1
#define MD5 2
/* see mdsw in md.c */

extern int MD;

/*
 * MD
 */

typedef struct {
	UINT1 state[16];    /* state */
	UINT1 checksum[16]; /* checksum */
	unsigned int count; /* number of bytes, modulo 16 */
	UINT1 buffer[16];   /* input buffer */
} MD2_CTX;

typedef struct {
	UINT4 state[4];		/* state (ABCD) */
	UINT4 count[2];		/* number of bits, modulo 2^64 (lsb first) */
	UINT1 buffer[64];	/* input buffer */
} MD4_CTX;

#define MD5_CTX MD4_CTX

typedef union {
	MD2_CTX md2;
	MD4_CTX md4;
} MD_CTX;

struct mdsw {
	void (*md_init)();
	void (*md_update)();
	void (*md_final)(); 	
};


/*
 * MD2
 */

void MD2Init   PROTO((POINTER));
void MD2Update PROTO((POINTER, UINT1 *, unsigned int));
void MD2Final  PROTO((UINT1 [16], POINTER));

/*
 * MD4
 */

void MD4Init   PROTO((POINTER));
void MD4Update PROTO((POINTER, UINT1 *, unsigned int));
void MD4Final  PROTO((UINT1 [16], POINTER));

/*
 * MD5
 */

void MD5Init   PROTO((POINTER));
void MD5Update PROTO((POINTER, UINT1 *, unsigned int));
void MD5Final  PROTO((UINT1 [16], POINTER));

/*
 * MD Switch
 */

extern struct mdsw mdsw[];

void MDEncode PROTO((POINTER, UINT4 *, unsigned int));
void MDDecode PROTO((UINT4 *, POINTER, unsigned int));
