#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "md4.h"
#include "md5.h"
#include "sha1.h"
#include "skey.h"
#include "btoe.h"


struct skey_hash {
	int (*Keycrunch) (char *, const char *, const char *);
	void (*Skey) (char *);
};
static struct skey_hash hash_table[] = {
	{ MD4Keycrunch,  MD4SKey },
	{ MD5Keycrunch,  MD5SKey },
	{ SHA1Keycrunch, SHA1SKey }
};


char *skey(int algorithm, int seq, const char *seed, const char *passphrase)
{
	char key[SKEY_SIZE];
	int i;
	g_assert(algorithm >= MD4);
	g_assert(algorithm <= SHA1);
	if (hash_table[algorithm].Keycrunch(key, seed, passphrase) == -1)
		return NULL;

	for (i = 0; i < seq; i++)
		hash_table[algorithm].Skey(key);

	return strdup(btoe((unsigned char *)key));
}

#ifdef SKEY_MAIN

struct test_entry {
	const char *passphrase;
	const char *seed;
	int  count;
	const char *hex;
	const char *btoe;
};

static struct test_entry md4_test_vector[] = {
	{"This is a test.", "TeSt",     0, "D185 4218 EBBB 0B51", "ROME MUG FRED SCAN LIVE LACE"},
	{"This is a test.", "TeSt",     1, "6347 3EF0 1CD0 B444", "CARD SAD MINI RYE COL KIN"},
	{"This is a test.", "TeSt",    99, "C5E6 1277 6E6C 237A", "NOTE OUT IBIS SINK NAVE MODE"},
	{"AbCdEfGhIjK",     "alpha1",   0, "5007 6F47 EB1A DE4E", "AWAY SEN ROOK SALT LICE MAP"},
	{"AbCdEfGhIjK",     "alpha1",   1, "65D2 0D19 49B5 F7AB", "CHEW GRIM WU HANG BUCK SAID"},
	{"AbCdEfGhIjK",     "alpha1",  99, "D150 C82C CE6F 62D1", "ROIL FREE COG HUNK WAIT COCA"},
	{"OTP's are good",  "correct",  0, "849C 79D4 F6F5 5388", "FOOL STEM DONE TOOL BECK NILE"},
	{"OTP's are good",  "correct",  1, "8C09 92FB 2508 47B1", "GIST AMOS MOOT AIDS FOOD SEEM"},
	{"OTP's are good",  "correct", 99, "3F3B F4B4 145F D74B", "TAG SLOW NOV MIN WOOL KENO"}
};

static struct test_entry md5_test_vector[] = {
	{"This is a test.", "TeSt",     0, "9E87 6134 D904 99DD", "INCH SEA ANNE LONG AHEM TOUR"},
	{"This is a test.", "TeSt",     1, "7965 E054 36F5 029F", "EASE OIL FUM CURE AWRY AVIS"},
	{"This is a test.", "TeSt",    99, "50FE 1962 C496 5880", "BAIL TUFT BITS GANG CHEF THY"},
	{"AbCdEfGhIjK",     "alpha1",   0, "8706 6DD9 644B F206", "FULL PEW DOWN ONCE MORT ARC"},
	{"AbCdEfGhIjK",     "alpha1",   1, "7CD3 4C10 40AD D14B", "FACT HOOF AT FIST SITE KENT"},
	{"AbCdEfGhIjK",     "alpha1",  99, "5AA3 7A81 F212 146C", "BODE HOP JAKE STOW JUT RAP"},
	{"OTP's are good",  "correct",  0, "F205 7539 43DE 4CF9", "ULAN NEW ARMY FUSE SUIT EYED"},
	{"OTP's are good",  "correct",  1, "DDCD AC95 6F23 4937", "SKIM CULT LOB SLAM POE HOWL"},
	{"OTP's are good",  "correct", 99, "B203 E28F A525 BE47", "LONG IVY JULY AJAR BOND LEE"}
};

static struct test_entry sha1_test_vector[] = {
	{"This is a test.", "TeSt",     0, "BB9E 6AE1 979D 8FF4", "MILT VARY MAST OK SEES WENT"},
	{"This is a test.", "TeSt",     1, "63D9 3663 9734 385B", "CART OTTO HIVE ODE VAT NUT"},
	{"This is a test.", "TeSt",    99, "87FE C776 8B73 CCF9", "GAFF WAIT SKID GIG SKY EYED"},
	{"AbCdEfGhIjK",     "alpha1",   0, "AD85 F658 EBE3 83C9", "LEST OR HEEL SCOT ROB SUIT"},
	{"AbCdEfGhIjK",     "alpha1",   1, "D07C E229 B5CF 119B", "RITE TAKE GELD COST TUNE RECK"},
	{"AbCdEfGhIjK",     "alpha1",  99, "27BC 7103 5AAF 3DC6", "MAY STAR TIN LYON VEDA STAN"},
	{"OTP's are good",  "correct",  0, "D51F 3E99 BF8E 6F0B", "RUST WELT KICK FELL TAIL FRAU"},
	{"OTP's are good",  "correct",  1, "82AE B52D 9437 74E4", "FLIT DOSE ALSO MEW DRUM DEFY"},
	{"OTP's are good",  "correct", 99, "4F29 6A74 FE15 67EC", "AURA ALOE HURL WING BERG WAIT"}
};

static int skey_test(void)
{
	int i, n;
	int failed = 0;
	char *key;

	printf("Testing of S/Key otp using Appendix C of rfc 2289\n");
	printf("* MD4:\n");
	n = sizeof(md4_test_vector) / sizeof(struct test_entry);
	for (i = 0; i < n; i++) {
		key = skey(MD4, md4_test_vector[i].count,
			md4_test_vector[i].seed,
			md4_test_vector[i].passphrase);
		if (key == NULL) {
			printf("Error calculating key\n");
			continue;
		}
		printf("%15s %8s %02d %32s -> %32s",
			md4_test_vector[i].passphrase,
			md4_test_vector[i].seed,
			md4_test_vector[i].count,
			md4_test_vector[i].btoe,
			key);
		if (strcmp(key, md4_test_vector[i].btoe) != 0) {
			printf(" - WRONG\n");
			failed++;
		} else
			printf(" - OK\n");
		free(key);
	}

	printf("\n* MD5:\n");
	n = sizeof(md5_test_vector) / sizeof(struct test_entry);
	for (i = 0; i < n; i++) {
		key = skey(MD5, md5_test_vector[i].count,
			md5_test_vector[i].seed,
			md5_test_vector[i].passphrase);
		if (key == NULL) {
			printf("Error calculating key\n");
			continue;
		}
		printf("%15s %8s %02d %32s -> %32s",
			md5_test_vector[i].passphrase,
			md5_test_vector[i].seed,
			md5_test_vector[i].count,
			md5_test_vector[i].btoe,
			key);
		if (strcmp(key, md5_test_vector[i].btoe) != 0) {
			printf(" - WRONG\n");
			failed++;
		} else
			printf(" - OK\n");
		free(key);
	}

	printf("\n* SHA1:\n");
	n = sizeof(sha1_test_vector) / sizeof(struct test_entry);
	for (i = 0; i < n; i++) {
		key = skey(SHA1, sha1_test_vector[i].count,
			sha1_test_vector[i].seed,
			sha1_test_vector[i].passphrase);
		if (key == NULL) {
			printf("Error calculating key\n");
			continue;
		}
		printf("%15s %8s %02d %32s -> %32s",
			sha1_test_vector[i].passphrase,
			sha1_test_vector[i].seed,
			sha1_test_vector[i].count,
			sha1_test_vector[i].btoe,
			key);
		if (strcmp(key, sha1_test_vector[i].btoe) != 0) {
			printf(" - WRONG\n");
			failed++;
		} else
			printf(" - OK\n");
		free(key);
	}

	printf("\nTotal test failures: %d\n", failed);
	return failed;
}

int main(int argc, char *argv[])
{
	char *key;
	int c;

	if (argc != 4) {
		printf("No arguments specified, will do automatic testing\n");
		printf("If you don't want this:\n");
		printf("Use %s <password> <count> <seed>\n", argv[0]);
		return skey_test();
	}

	c = atoi(argv[2]);
	
	key = skey(MD4, c, argv[3], argv[1]);
	if (key != NULL) {
		printf("MD4 %s\n", btoe((unsigned char *)key));
		free(key);
	}
	
	key = skey(MD5, c, argv[3], argv[1]);
	if (key != NULL) {
		printf("MD5  %s\n", btoe((unsigned char *)key));
		free(key);
	}

	key = skey(SHA1,c, argv[3], argv[1]);
	if (key != NULL) {
		printf("SHA1 %s\n", btoe((unsigned char *)key));
		free(key);
	}

	return 0;
}

#endif
