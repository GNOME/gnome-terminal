#ifndef __SKEY_H__
#define __SKEY_H__

#define MD2 0
#define MD4 1
#define MD5 2

char *skey (int         md,
	    int         seq,
	    const char *seed,
	    const char *password);

#endif
