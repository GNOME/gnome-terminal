/*
 *			donkey  --- Don't Key
 *	       An alternative for S/KEY's key program.
 *
 *		 Copyright (C) 1995 Kazuhiko Yamamoto
 *
 * This software conforms GNU GENERAL PUBLIC LICENSE Version 2.
 *
 * Author:  Kazuhiko Yamamoto <kazu@is.aist-nara.ac.jp>
 * Created: April   25, 1995
 * Revised: October  4, 1995
 *
 */

#include "config.h"
#include "donkey.h"
#include "md.h"

/*
 * declaration of local functions
 */

private void usage PROTO((char *progname));
private void help PROTO((char *progname));
private void version PROTO((char *progname));
private int btoa8 PROTO((char *out, char *in));


/*
 * global variables
 */

int MD = DefaultMD; 


private int btoa8(out, in)
char *out, *in;
{
	int i;
	
	if(in == NULL || out == NULL)
		return -1;
	
	for(i=0; i < 8; i++){
		sprintf(out, "%02x", *in++ & 0xff);
		out += 2;
	}
	
	return 0;
}

/*
 * main
 */

char *
skey (int         md,
      int         seq,
      const char *seed,
      const char *password)
{
	int i = 1;
	char key[8];
	/*
	 * default option values
	 */

	MD = md;

	if (keycrunch(key, seed, password) == RET_ERROR) {
		return NULL;
	}

	for (i = 0; i < seq; i++) secure_hash(key);
	return (strdup (btoe(key)));

}
