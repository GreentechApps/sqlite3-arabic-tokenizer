#include <stdio.h>
#include <string.h>
#include <sqlite3ext.h>

#define MIN_TOKEN_LEN (3)
#define MAX_TOKEN_LEN (64)

SQLITE_EXTENSION_INIT1;
#if defined(_WIN32)
#define _USE_MATH_DEFINES
#endif /* _WIN32 */


typedef unsigned char utf8_t;


#define isunicode(c) (((c)&0xc0)==0xc0)

static int arabic_unicode[65] = {
        1552,
        1553,
        1554,
        1555,
        1556,
        1557,
        1558,
        1559,
        1560,
        1561,
        1562,
        1750,
        1751,
        1752,
        1753,
        1754,
        1755,
        1756,
        1757,
        1758,
        1759,
        1760,
        1761,
        1762,
        1763,
        1764,
        1765,
        1766,
        1767,
        1768,
        1769,
        1770,
        1771,
        1772,
        1773,
        1600,
        1611,
        1612,
        1613,
        1614,
        1615,
        1616,
        1617,
        1618,
        1619,
        1620,
        1621,
        1622,
        1623,
        1624,
        1625,
        1626,
        1627,
        1628,
        1629,
        1630,
        1631,
        1648, // 58

        1571, 1573, 1570, 1649, // 62 alif replace
        1610,
        1569,
        1607, // 65
};

char *aliff = "ا";
char *r1 = "ى";
char *r2 = "ئ";
char *r3 = "ة";

int unicode_diacritic(int code) {

    int found = 0;
    for (int i = 0; i < 64; i++) {
        if (arabic_unicode[i] == code) {
            found = i;
            break;
        }
    }

    return found;
}

int utf8_decode(const char *str, int *i) {
    const utf8_t *s = (const utf8_t *) str; // Use unsigned chars
    int u = *s, l = 1;
    if (isunicode(u)) {
        int a = (u & 0x20) ? ((u & 0x10) ? ((u & 0x08) ? ((u & 0x04) ? 6 : 5) : 4) : 3) : 2;
        if (a < 6 || !(u & 0x02)) {
            int b, p = 0;
            u = ((u << (a + 1)) & 0xff) >> (a + 1);
            for (b = 1; b < a; ++b)
                u = (u << 6) | (s[l++] & 0x3f);
        }
    }
    if (i) *i += l;
    return u;
}

char *remove_diacritic(char *text, int debug, int escaped_space) {
    int l;
    int turn = 0;
    for (int i = 0; i < 2147483647 && text[i] != '\0';) {
        if (!isunicode(text[i])) {
            if (escaped_space == 1) {
                break;
            } else {
                if (text[i] != ' ') break;
                i++;
            }

        } else {
            l = 0;
            int z = utf8_decode(&text[i], &l);
            if (debug == 1) printf("Unicode value at %d %d is U+%04X and it\'s %d bytes.\n", z, i, z, l);
            i += l;
            turn += l;
        }
    }

    char *replaced = (char *) sqlite3_malloc(turn);
    int j = 0;
    for (int i = 0; i <= turn && text[i] != '\0';) {
        if (!isunicode(text[i])) {
            *(replaced + j++) = text[i];
            i++;
        } else {
            l = 0;
            int z = utf8_decode(&text[i], &l);
            i += l;
            int index = unicode_diacritic(z);
            if (index == 0) {
                *(replaced + j++) = text[i - 2];
                *(replaced + j++) = text[i - 1];
            } else if (index >= 58 && index <= 61) {
                *(replaced + j++) = aliff[0];
                *(replaced + j++) = aliff[1];
            } else if (index >= 62 && index <= 64) {
                if (index == 62) {
                    *(replaced + j++) = r1[0];
                    *(replaced + j++) = r1[1];
                } else if (index == 63) {
                    *(replaced + j++) = r2[0];
                    *(replaced + j++) = r2[1];
                } else if (index == 64) {
                    *(replaced + j++) = r3[0];
                    *(replaced + j++) = r3[1];
                }
            }

        }
    }
    replaced[j] = '\0';
    return replaced;
}


struct SnowTokenizer {
    void *pCtx;

    fts5_tokenizer nextTokenizerModule;
    Fts5Tokenizer *nextTokenizerInstance;

    int (*xToken)(void *, int, const char *, int, int, int);
};


static fts5_api *fts5_api_from_db(sqlite3 *db) {
    fts5_api *pRet = 0;
    sqlite3_stmt *pStmt = 0;

    int version = sqlite3_libversion_number();
    if (version >= 3020000) {  // current api
        if (SQLITE_OK == sqlite3_prepare(db, "SELECT fts5(?1)", -1, &pStmt, 0)) {
            sqlite3_bind_pointer(pStmt, 1, (void *) &pRet, "fts5_api_ptr", NULL);
            sqlite3_step(pStmt);
        }
        sqlite3_finalize(pStmt);
    } else {  // before 3.20
        int rc = sqlite3_prepare(db, "SELECT fts5()", -1, &pStmt, 0);
        if (rc == SQLITE_OK) {
            if (SQLITE_ROW == sqlite3_step(pStmt) && sizeof(fts5_api * ) == sqlite3_column_bytes(pStmt, 0)) {
                memcpy(&pRet, sqlite3_column_blob(pStmt, 0), sizeof(fts5_api * ));
            }
            sqlite3_finalize(pStmt);
        }
    }
    return pRet;
}

static void ftsSnowballDelete(Fts5Tokenizer *pTok) {
    if (pTok) {
        struct SnowTokenizer *p = (struct SnowTokenizer *) pTok;

        if (p->nextTokenizerInstance) {
            p->nextTokenizerModule.xDelete(p->nextTokenizerInstance);
        }

        sqlite3_free(p);
    }
}

static int ftsSnowballCreate(void *pCtx, const char **azArg, int nArg, Fts5Tokenizer **ppOut) {
    struct SnowTokenizer *result;
    fts5_api *pApi = (fts5_api *) pCtx;
    void *pUserdata = 0;
    int rc = SQLITE_OK;
    int nextArg;
    const char *zBase = "unicode61";

    result = (struct SnowTokenizer *) sqlite3_malloc(sizeof(struct SnowTokenizer));

    if (result) {
        memset(result, 0, sizeof(struct SnowTokenizer));
        rc = SQLITE_OK;
    } else {
        rc = SQLITE_ERROR;
    }

    if (rc == SQLITE_OK) {
        if (nArg > nextArg) {
            zBase = azArg[nextArg];
        }
        rc = pApi->xFindTokenizer(pApi, zBase, &pUserdata, &result->nextTokenizerModule);
    }

    if (rc == SQLITE_OK) {
        int nArg2 = (nArg > nextArg + 1 ? nArg - nextArg - 1 : 0);
        const char **azArg2 = (nArg2 ? &azArg[nextArg + 1] : 0);
        rc = result->nextTokenizerModule.xCreate(pUserdata, azArg2, nArg2, &result->nextTokenizerInstance);
    }

    if (rc != SQLITE_OK) {
        ftsSnowballDelete((Fts5Tokenizer *) result);
        result = NULL;
    }

    *ppOut = (Fts5Tokenizer *) result;
    return rc;
}
// biism rah r
static int fts5SnowballCb(void *pCtx, int tflags, const char *pToken, int nToken, int iStart, int iEnd) {
    struct SnowTokenizer *p = (struct SnowTokenizer *) pCtx;

//    char *str = (char *) sqlite3_malloc(iEnd - iStart + 2);
//    int len = iEnd - iStart;
//    int i = 0;
//    for (; i <= len; i++) {
//        *(str + i) = *(pToken + iStart + i);
//    }
    // char *result = remove_diacritic(pToken, 0, 1);
    printf("CHECK %s", pToken);

    return p->xToken(p->pCtx, tflags, pToken, nToken, iStart, iEnd);

}

static int ftsSnowballTokenize(Fts5Tokenizer *pTokenizer, void *pCtx, int flags, const char *pText, int nText,
                               int (*xToken)(void *, int, const char *, int nToken, int iStart, int iEnd)) {
    struct SnowTokenizer *p = (struct SnowTokenizer *) pTokenizer;
    p->xToken = xToken;
    p->pCtx = pCtx;

    return p->nextTokenizerModule.xTokenize(p->nextTokenizerInstance, (void *) p, flags, pText, nText, fts5SnowballCb);
}

#ifdef _WIN32
__declspec(dllexport)
#endif

int sqlite3_arabicftstokenizer_init(sqlite3 *db, char **error, const sqlite3_api_routines *api) {
    fts5_api *ftsApi;

    fts5_tokenizer tokenizer = {ftsSnowballCreate, ftsSnowballDelete, ftsSnowballTokenize};

    SQLITE_EXTENSION_INIT2(api);

    ftsApi = fts5_api_from_db(db);

    if (ftsApi) {
        ftsApi->xCreateTokenizer(ftsApi, "arabic_tokenizer", (void *) ftsApi, &tokenizer, NULL);
        return SQLITE_OK;
    } else {
        *error = sqlite3_mprintf("Can't find fts5 extension");
        return SQLITE_ERROR;
    }
}

// gcc -fPIC -dynamiclib arabic_fts5_tokenizer.c -o dist/x86/arabic_fts5_tokenizer.dylib -target x86_64-apple-macos10.12

// gcc -fPIC -dynamiclib arabic_fts5_tokenizer.c -o dist/x86/arabic_fts5_tokenizer.dylib -target x86_64-apple-macos10.12

// CREATE VIRTUAL TABLE hello USING fts5(text, tokenize='arabic_tokenizer');

// .open /Users/snnafi/Downloads/quran.db // 1st

// select load_extension('/Users/snnafi/Desktop/CLionProjects/General/arabic-remove-diacrities/dist/x86/arabic_fts5_tokenizer');

// INSERT INTO hello (text) VALUES('ثُمَّ أَنتُمْ هَٰٓؤُلَآءِ تَقْتُلُونَ أَنفُسَكُمْ وَتُخْرِجُونَ فَرِيقًا مِّنكُم مِّن دِيَٰرِهِمْ تَظَٰهَرُونَ عَلَيْهِم بِٱلْإِثْمِ وَٱلْعُدْوَٰنِ وَإِن يَأْتُوكُمْ أُسَٰرَىٰ تُفَٰدُوهُمْ وَهُوَ مُحَرَّمٌ عَلَيْكُمْ إِخْرَاجُهُمْۚ أَفَتُؤْمِنُونَ بِبَعْضِ ٱلْكِتَٰبِ وَتَكْفُرُونَ بِبَعْضٍۚ فَمَا جَزَآءُ مَن يَفْعَلُ ذَٰلِكَ مِنكُمْ إِلَّا خِزْىٌ فِى ٱلْحَيَوٰةِ ٱلدُّنْيَاۖ وَيَوْمَ ٱلْقِيَٰمَةِ يُرَدُّونَ إِلَىٰٓ أَشَدِّ ٱلْعَذَابِۗ وَمَا ٱللَّهُ بِغَٰفِلٍ عَمَّا تَعْمَلُون');

// select * from verses  where text MATCH 'ثُمَّ';

// select * from heyy1 where text MATCH 'ثم';

// CREATE VIRTUAL TABLE IF NOT EXISTS heyy1 USING fts5 (sura,ayah,text,primary, tokenize='arabic_tokenizer');

// insert into heyy1 values (1, 1, 'بِسْمِ ٱللَّهِ ٱلرَّحْمَٰنِ ٱلرَّحِيمِ ', NULL);