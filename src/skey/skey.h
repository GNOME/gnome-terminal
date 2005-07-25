#define MD4  0
#define MD5  1
#define SHA1 2

#define SKEY_SIZE	8

char *skey(int algorithm, int seq, const char *seed, const char *passhrase);

