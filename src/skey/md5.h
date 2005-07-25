#ifndef MD5_H
#define MD5_H

#include <glib.h>

typedef struct {
	guint32 buf[4];
	guint32 bits[2];
	unsigned char in[64];
} MD5_CTX;

void MD5Transform(guint32 buf[4], guint32 const in[16]);
void MD5Init(MD5_CTX *context);
void MD5Update(MD5_CTX *context, unsigned char const *buf, unsigned len);
void MD5Final(unsigned char *digest, MD5_CTX *context);
int  MD5Keycrunch(char *result, const char *seed, const char *passhrase);
void MD5SKey(char *x);


#endif /* !MD5_H */
