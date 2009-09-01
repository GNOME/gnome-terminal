#include <config.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "skey.h"

typedef struct {
        SKeyAlgorithm algorithm;
	const char *passphrase;
	const char *seed;
	int  count;
	const char *hex;
	const char *btoe;
} TestEntry;

static const TestEntry tests[] = {
	{ MD4, "This is a test.", "TeSt",     0, "D185 4218 EBBB 0B51", "ROME MUG FRED SCAN LIVE LACE"   },
	{ MD4, "This is a test.", "TeSt",     1, "6347 3EF0 1CD0 B444", "CARD SAD MINI RYE COL KIN"      },
	{ MD4, "This is a test.", "TeSt",    99, "C5E6 1277 6E6C 237A", "NOTE OUT IBIS SINK NAVE MODE"   },
	{ MD4, "AbCdEfGhIjK",     "alpha1",   0, "5007 6F47 EB1A DE4E", "AWAY SEN ROOK SALT LICE MAP"    },
	{ MD4, "AbCdEfGhIjK",     "alpha1",   1, "65D2 0D19 49B5 F7AB", "CHEW GRIM WU HANG BUCK SAID"    },
	{ MD4, "AbCdEfGhIjK",     "alpha1",  99, "D150 C82C CE6F 62D1", "ROIL FREE COG HUNK WAIT COCA"   },
	{ MD4, "OTP's are good",  "correct",  0, "849C 79D4 F6F5 5388", "FOOL STEM DONE TOOL BECK NILE"  },
	{ MD4, "OTP's are good",  "correct",  1, "8C09 92FB 2508 47B1", "GIST AMOS MOOT AIDS FOOD SEEM"  },
	{ MD4, "OTP's are good",  "correct", 99, "3F3B F4B4 145F D74B", "TAG SLOW NOV MIN WOOL KENO"     },
	{ MD5, "This is a test.", "TeSt",     0, "9E87 6134 D904 99DD", "INCH SEA ANNE LONG AHEM TOUR"   },
	{ MD5, "This is a test.", "TeSt",     1, "7965 E054 36F5 029F", "EASE OIL FUM CURE AWRY AVIS"    },
	{ MD5, "This is a test.", "TeSt",    99, "50FE 1962 C496 5880", "BAIL TUFT BITS GANG CHEF THY"   },
	{ MD5, "AbCdEfGhIjK",     "alpha1",   0, "8706 6DD9 644B F206", "FULL PEW DOWN ONCE MORT ARC"    },
	{ MD5, "AbCdEfGhIjK",     "alpha1",   1, "7CD3 4C10 40AD D14B", "FACT HOOF AT FIST SITE KENT"    },
	{ MD5, "AbCdEfGhIjK",     "alpha1",  99, "5AA3 7A81 F212 146C", "BODE HOP JAKE STOW JUT RAP"     },
	{ MD5, "OTP's are good",  "correct",  0, "F205 7539 43DE 4CF9", "ULAN NEW ARMY FUSE SUIT EYED"   },
	{ MD5, "OTP's are good",  "correct",  1, "DDCD AC95 6F23 4937", "SKIM CULT LOB SLAM POE HOWL"    },
	{ MD5, "OTP's are good",  "correct", 99, "B203 E28F A525 BE47", "LONG IVY JULY AJAR BOND LEE"    },
	{ SHA1, "This is a test.", "TeSt",     0, "BB9E 6AE1 979D 8FF4", "MILT VARY MAST OK SEES WENT"   },
	{ SHA1, "This is a test.", "TeSt",     1, "63D9 3663 9734 385B", "CART OTTO HIVE ODE VAT NUT"    },
	{ SHA1, "This is a test.", "TeSt",    99, "87FE C776 8B73 CCF9", "GAFF WAIT SKID GIG SKY EYED"   },
	{ SHA1, "AbCdEfGhIjK",     "alpha1",   0, "AD85 F658 EBE3 83C9", "LEST OR HEEL SCOT ROB SUIT"    },
	{ SHA1, "AbCdEfGhIjK",     "alpha1",   1, "D07C E229 B5CF 119B", "RITE TAKE GELD COST TUNE RECK" },
	{ SHA1, "AbCdEfGhIjK",     "alpha1",  99, "27BC 7103 5AAF 3DC6", "MAY STAR TIN LYON VEDA STAN"   },
	{ SHA1, "OTP's are good",  "correct",  0, "D51F 3E99 BF8E 6F0B", "RUST WELT KICK FELL TAIL FRAU" },
	{ SHA1, "OTP's are good",  "correct",  1, "82AE B52D 9437 74E4", "FLIT DOSE ALSO MEW DRUM DEFY"  },
	{ SHA1, "OTP's are good",  "correct", 99, "4F29 6A74 FE15 67EC", "AURA ALOE HURL WING BERG WAIT" },

        { SHA1, "Passphrase",      "IiIi",   100, "27F4 01CC 0AC8 5112", "MEG JACK DIET GAD FORK GARY"   }
};

static const char *algos[] = {
  "MD4",
  "MD5",
  "SHA1"
};

static void
skey_test (gconstpointer data)
{
        const TestEntry *test = (const TestEntry *) data;
        char *key;

        key = skey (test->algorithm,
                    test->count,
                    test->seed,
                    test->passphrase);
        g_assert (key != NULL);
        g_assert (strcmp (key, test->btoe) == 0);
        free (key);
}

int main(int argc, char *argv[])
{
        guint i;

        if (!setlocale (LC_ALL, ""))
                 g_error ("Locale not supported by C library!\n");

        g_test_init (&argc, &argv, NULL);
        g_test_bug_base ("http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-terminal");

        for (i = 0; i < G_N_ELEMENTS (tests); ++i) {
                const TestEntry *test = &tests[i];
                char *name;

                name = g_strdup_printf ("/%s/%s/%s/%d/%s/%s",
                                        algos[test->algorithm],
                                        test->passphrase,
                                        test->seed,
                                        test->count,
                                        test->hex,
                                        test->btoe);
                g_test_add_data_func (name, test, skey_test);
                g_free (name);
        }

        return g_test_run ();
}
