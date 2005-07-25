#include <ctype.h>

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
		if (isupper(*p))
			*p = tolower(*p);
}
