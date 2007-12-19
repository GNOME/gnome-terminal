/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

#include <config.h>

#include <glib/gchecksum.h>

#include "skey.h"
#include "skeyutil.h"
#include "md5.h"

int MD5Keycrunch(char *result, const char *seed, const char *passhrase)
{
	char *buf;
	gsize len;
        GChecksum *checksum;
        guint8 digest[16];
        gsize digest_len = sizeof (digest);
	guint32 *results;

	len = strlen(seed) + strlen(passhrase);
	buf = (char *)malloc(len+1);
	if (buf == NULL)
		return -1;

	strcpy(buf, seed);
	skey_lowcase(buf);
	strcat(buf, passhrase);
	skey_sevenbit(buf);

        checksum = g_checksum_new (G_CHECKSUM_MD5);
        g_checksum_update (checksum, (const guchar *) buf, len);
	free(buf);

        g_checksum_get_digest (checksum, digest, &digest_len);
        g_assert (digest_len == 16);

        results = (guint32 *) digest;
	results[0] ^= results[2];
	results[1] ^= results[3];

	memcpy((void *)result, (void *)results, SKEY_SIZE);

        g_checksum_free (checksum);

	return 0;
}

void MD5SKey(char *x)
{
        GChecksum *checksum;
        guint8 digest[16];
        gsize digest_len = sizeof (digest);
	guint32 *results;

        checksum = g_checksum_new (G_CHECKSUM_MD5);
        g_checksum_update (checksum, (const guchar *) x, SKEY_SIZE);
        g_checksum_get_digest (checksum, digest, &digest_len);
        g_assert (digest_len == 16);

        results = (guint32 *) digest;
	results[0] ^= results[2];
	results[1] ^= results[3];

	memcpy((void *)x, (void *)results, SKEY_SIZE);

        g_checksum_free (checksum);
}
