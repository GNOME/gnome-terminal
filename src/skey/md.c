/*
 * md.c
 *		 Copyright (C) 1995 Kazuhiko Yamamoto
 *	      Kazuhiko Yamamoto <kazu@is.aist-nara.ac.jp>
 */

#include "config.h"
#include "md.h"

struct mdsw mdsw[] = {
    {MD2Init, MD2Update, MD2Final},
    {MD4Init, MD4Update, MD4Final},
    {MD5Init, MD5Update, MD5Final},
};

/*
 * Encodes input (UINT4) into output (unsigned char). Assumes len is
 * a multiple of 4.
 */
void MDEncode (output, input, len)
POINTER output;
UINT4 *input;
unsigned int len;
{
	unsigned int i, j;
	
	for (i = 0, j = 0; j < len; i++, j += 4) {
		output[j] = (unsigned char)(input[i] & 0xff);
		output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
		output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
		output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
	}
}

/*
 * Decodes input (unsigned char) into output (UINT4). Assumes len is
 * a multiple of 4.
 */
void MDDecode (output, input, len)
UINT4 *output;
POINTER input;
unsigned int len;
{
	unsigned int i, j;
	
	for (i = 0, j = 0; j < len; i++, j += 4)
		output[i] = ((UINT4)input[j]) | (((UINT4)input[j+1]) << 8) |
			(((UINT4)input[j+2]) << 16) | (((UINT4)input[j+3]) << 24);
}


/*
 * Copyright (C) 1990-2, RSA Data Security, Inc. All rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD4 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD4 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */
