#ifndef MD5_H
#define MD5_H

#include <glib.h>

int  MD5Keycrunch(char *result, const char *seed, const char *passhrase);
void MD5SKey(char *x);

#endif /* !MD5_H */
