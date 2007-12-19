/*
 * SHA-1 in C
 * By Steve Reid <steve@edmweb.com>
 * 100% Public Domain
 *
 * Test Vectors (from FIPS PUB 180-1)
 * "abc"
 *   A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
 * "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
 *   84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
 * A million repetitions of "a"
 *   34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
 */

#include <config.h>

#include <glib/gchecksum.h>

#include "skey.h"
#include "skeyutil.h"
#include "sha1.h"


#define SHA1_DIGESTSIZE       20
#define SHA1_BLOCKSIZE        64

#define HTONDIGEST(x) { \
      x[0] = htonl(x[0]); \
      x[1] = htonl(x[1]); \
      x[2] = htonl(x[2]); \
      x[3] = htonl(x[3]); \
      x[4] = htonl(x[4]); }

#ifdef WORDS_BIGENDIAN
/*
 * Note: this code is harmless on little-endian machines.
 */
static void byteReverse(unsigned char *buf, unsigned longs)
{
    guint32 t;
    do {
	t = (guint32) ((unsigned) buf[3] << 8 | buf[2]) << 16 |
	    ((unsigned) buf[1] << 8 | buf[0]);
	*(guint32 *) buf = t;
	buf += 4;
    } while (--longs);
}
#endif


int SHA1Keycrunch(char *result, const char *seed, const char *passphrase)
{
	char *buf;
        gsize len;
        GChecksum *checksum;
        guint8 digest[20];
        gsize digest_len = sizeof (digest);
	guint32 *results;

	len = strlen(seed) + strlen(passphrase);
	if ((buf = (char *)malloc(len+1)) == NULL)
		return -1;

	strcpy(buf, seed);
	skey_lowcase(buf);
	strcat(buf, passphrase);
	skey_sevenbit(buf);

        checksum = g_checksum_new (G_CHECKSUM_SHA1);
        g_checksum_update (checksum, (const guchar *) buf, len);
	free(buf);

        g_checksum_get_digest (checksum, digest, &digest_len);
        g_assert (digest_len == 20);

        results = (guint32 *) digest;

#ifndef WORDS_BIGENDIAN
	HTONDIGEST(results);
#else
	byteReverse((unsigned char *)digest, 5);
#endif

        results = (guint32 *) digest;
        results[0] ^= results[2];
	results[1] ^= results[3];
	results[0] ^= results[4];

	memcpy((void *)result, (void *)results, SKEY_SIZE);

        g_checksum_free (checksum);

	return 0;
}

void SHA1SKey(char *x)
{
        GChecksum *checksum;
        guint8 digest[20];
        gsize digest_len = sizeof (digest);
	guint32 *results;

        checksum = g_checksum_new (G_CHECKSUM_SHA1);
        g_checksum_update (checksum, (const guchar *) x, SKEY_SIZE);
        g_checksum_get_digest (checksum, digest, &digest_len);
        g_assert (digest_len == 20);

        results = (guint32 *) digest;
#ifndef WORDS_BIGENDIAN
	HTONDIGEST(results);
#else
	byteReverse((unsigned char *)digest, 5);
#endif

	results[0] ^= results[2];
	results[1] ^= results[3];
	results[0] ^= results[4];

	memcpy((void *)x, (void *)results, SKEY_SIZE);

        g_checksum_free (checksum);
}
