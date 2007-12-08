#ifndef _SHA1_H
#define _SHA1_H

#include <glib.h>
  
int  SHA1Keycrunch(char *result, const char *seed, const char *passphrase);
void SHA1SKey(char *x);

#endif /* _SHA1_H */
