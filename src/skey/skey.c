/*
 * skey.c
 *		 Copyright (C) 1995 Kazuhiko Yamamoto
 *	      Kazuhiko Yamamoto <kazu@is.aist-nara.ac.jp>
 */

#include "config.h"
#include "donkey.h"
#include "md.h"
#include <stdlib.h>

private void sevenbit PROTO((char *s));

public int keycrunch(result, seed, passwd)
char *result;   /* 8-byte result */
const char *seed;     /* Seed, any length */
const char *passwd;   /* Password, any length */
{
	char *buf;
	MDX_CTX md;
	UINT4 results[4];
	unsigned int buflen;
	
	buflen = strlen(seed) + strlen(passwd);
	if((buf = malloc(buflen + 1)) == NULL)
		return(RET_ERROR);
	strcpy(buf, seed);
	strcat(buf, passwd);

	/* Crunch the key through MD[45] */
	sevenbit(buf);
	(*mdsw[MD].md_init)((POINTER)&md);
	(*mdsw[MD].md_update)((POINTER)&md, (POINTER)buf, buflen);
	(*mdsw[MD].md_final)((POINTER)results, (POINTER)&md);
	free(buf);

	results[0] ^= results[2];
	results[1] ^= results[3];

	memcpy(result,(char *)results,8);

	return(RET_SUCCESS);
}

private void sevenbit(s)
char *s;
{
	/* make sure there are only 7 bit code in the line*/
	while(*s){
		*s = 0x7f & (*s);
		s++;
	}
}

public void secure_hash(x)
char *x;
{
	MDX_CTX md;
	UINT4 results[4];

	(*mdsw[MD].md_init)((POINTER)&md);
	(*mdsw[MD].md_update)((POINTER)&md,(POINTER)x, 8);
	(*mdsw[MD].md_final)((POINTER)results, (POINTER)&md);
	/* Fold 128 to 64 bits */
	results[0] ^= results[2];
	results[1] ^= results[3];

	/* Only works on byte-addressed little-endian machines!! */
	memcpy(x, (char *)results, 8);
}

/*
 * This code is imported from Bollcore's S/KEY
 */
