/*
 * SHA-1 in C
 * By Steve Reid <steve@edmweb.com>
 * 100% Public Domain
 */

#ifndef _SHA1_H
#define _SHA1_H

#include <glib.h>

typedef struct {
    guint32 state[5];
    guint32 count[2];  
    unsigned char buffer[64];
} SHA1_CTX;
  
void SHA1Transform (guint32 state[5], const unsigned char buffer[64]);
void SHA1Init (SHA1_CTX *context);
void SHA1Update (SHA1_CTX *context, const unsigned char *data, guint len);
void SHA1Final (unsigned char digest[20], SHA1_CTX *context);
int  SHA1Keycrunch(char *result, const char *seed, const char *passphrase);
void SHA1SKey(char *x);

#define SHA1_DIGESTSIZE       20
#define SHA1_BLOCKSIZE        64
#define HTONDIGEST(x) { \
      x[0] = htonl(x[0]); \
      x[1] = htonl(x[1]); \
      x[2] = htonl(x[2]); \
      x[3] = htonl(x[3]); \
      x[4] = htonl(x[4]); }

#define NTOHDIGEST(x) { \
      x[0] = ntohl(x[0]); \
      x[1] = ntohl(x[1]); \
      x[2] = ntohl(x[2]); \
      x[3] = ntohl(x[3]); \
      x[4] = ntohl(x[4]); }

#endif /* _SHA1_H */
