#include <config.h>

#include <ctype.h>

#include <glib.h>

#include "skeyutil.h"

void skey_sevenbit(char *s)
{
	char *p;

	for (p = s; *p; p++)
		*p &= 0x7f;
}

void skey_lowcase(char *s)
{
	char *p;

	for (p = s; *p; p++)
		if (g_ascii_isupper(*p))
			*p = g_ascii_tolower(*p);
}
