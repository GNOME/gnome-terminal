typedef enum {
  MD4,
  MD5,
  SHA1
} SKeyAlgorithm;

#define SKEY_SIZE	8

char *skey(SKeyAlgorithm algorithm, int seq, const char *seed, const char *passhrase);

