/*
 * donkey.h
 */

/* should be included after config.h */

public char *btoe PROTO((char *md));
public int passphrase PROTO((char *prompt, char *phrase, int size, int rej));
public void secure_hash PROTO((char *x));
public int keycrunch PROTO((char *result, const char *seed, const char *passwd));
