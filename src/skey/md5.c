#include <config.h>
#include <stdlib.h>

#include <string.h>
#include <glib.h>

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
	buf = (char *)g_try_malloc(len+1);
	if (buf == NULL)
		return -1;

	strcpy(buf, seed);
	skey_lowcase(buf);
	strcat(buf, passhrase);
	skey_sevenbit(buf);

        checksum = g_checksum_new (G_CHECKSUM_MD5);
        g_checksum_update (checksum, (const guchar *) buf, len);
	g_free(buf);

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
