/*
 * donkey.h
 */

/* should be included after config.h */

char *btoe PROTO((char *md));
int passphrase PROTO((char *prompt, char *phrase, int size, int rej));
void secure_hash PROTO((char *x));
int keycrunch PROTO((char *result, const char *seed, const char *passwd));
