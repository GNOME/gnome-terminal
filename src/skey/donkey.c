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



/*
 * global variables
 */

int MD = DefaultMD; 

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
