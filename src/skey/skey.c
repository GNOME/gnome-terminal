#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "md4.h"
#include "md5.h"
#include "sha1.h"
#include "skey.h"
#include "btoe.h"

struct skey_hash {
	int (*Keycrunch) (char *, const char *, const char *);
	void (*Skey) (char *);
};
static struct skey_hash hash_table[] = {
	{ MD4Keycrunch,  MD4SKey },
	{ MD5Keycrunch,  MD5SKey },
	{ SHA1Keycrunch, SHA1SKey }
};


char *skey(SKeyAlgorithm algorithm, int seq, const char *seed, const char *passphrase)
{
	char key[SKEY_SIZE];
	int i;
        g_assert (algorithm < G_N_ELEMENTS (hash_table));
	if (hash_table[algorithm].Keycrunch(key, seed, passphrase) == -1)
		return NULL;

	for (i = 0; i < seq; i++)
		hash_table[algorithm].Skey(key);

	return strdup(btoe((unsigned char *)key));
}
