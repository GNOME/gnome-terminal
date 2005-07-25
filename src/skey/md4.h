#ifndef MD4_H
#define MD4_H

#include <glib.h>
 
typedef struct {
	guint32 buf[4];
	guint32 bits[2];
	unsigned char in[64];
} MD4_CTX;

void MD4Transform(guint32 buf[4], guint32 const in[16]);
void MD4Init(MD4_CTX *context);
void MD4Update(MD4_CTX *context, unsigned char const *buf, unsigned len);
void MD4Final(unsigned char *digest, MD4_CTX *context);
int  MD4Keycrunch(char *result, const char *seed, const char *passphrase);
void MD4SKey(char *x);

#endif /* !MD4_H */
