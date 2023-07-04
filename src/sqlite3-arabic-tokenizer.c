#include <stdio.h>
#include <string.h>
#include "sqlite3ext.h"
#include <assert.h>
#include "sqlite3-arabic-tokenizer.h"

SQLITE_EXTENSION_INIT1;
#if defined(_WIN32)
#define _USE_MATH_DEFINES
#endif /* _WIN32 */


typedef unsigned char utf8_t;

struct TextInfo {
    char *modifiedText;
    int length;
};

struct WordInfo {
    char **words;
    int total;
    int **actualStartIndexes;
    int **actualEndIndexes;
};


#define isunicode(c) (((c)&0xc0)==0xc0)

static int arabic_unicode[66] = {
        1548, // arabic comma
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
        1648, // 59

        1571, 1573, 1570, 1649, // 63 alif replace
        1610,
        1569,
        1607, // 66
};

int charsToRemove(const char *c) {
    return (*c == ' '
            || *c == '?'
            || *c == '.'
            || *c == '#'
            || *c == '!'
            || *c == '*'
            || *c == '%'
            || *c == '^'
            || *c == '~'
            || *c == '`'
            || *c == '"'
            || *c == '>'
            || *c == '<'
            || *c == ';'
            || *c == ')'
            || *c == '('
            || *c == '+'
            || *c == '-'
            || *c == '='
            || *c == '$'
            || *c == '/'
            || *c == '\\'
            || *c == '|'
            || *c == ','
            || *c == ':'
            || *c == '{'
            || *c == '}'
            || *c == '['
            || *c == ']'
            || *c == '_'

    );
}

void removeChars(char *word, int l) {
    int i, j;
    int len = l;
    for (i = 0; i < len; i++) {
        if (charsToRemove(&word[i])) {
            for (j = i; j < len; j++) {
                word[j] = word[j + 1];
            }
            len--;
            i--;
        }
    }
}

char *aliff = "ا";
char *r1 = "ى";
char *r2 = "ئ";
char *r3 = "ة";

int unicode_diacritic(int code) {

    int found = -1;
    for (int i = 0; i < 66; i++) {
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

struct TextInfo *remove_diacritic(const char *text, int debug) {
    if (debug) printf("\nremove_diacritic START %s\n", text);
    struct TextInfo *info = (struct TextInfo *) sqlite3_malloc(sizeof(struct TextInfo));

    int total = 0;
    for (; text[total] != '\0'; total++);
    if (debug) printf("\nTOTAL LENGTH %d\n", total);

    int l;
    char *replaced = (char *) sqlite3_malloc(total + 5);
    int j = 0;
    for (int i = 0; text[i] != '\0';) {
        if (!isunicode(text[i])) {
            *(replaced + j++) = text[i];
            i++;
        } else {
            l = 0;
            int z = utf8_decode(&text[i], &l);
            i += l;
            int index = unicode_diacritic(z);
            if (index == -1) {
                *(replaced + j++) = text[i - 2];
                *(replaced + j++) = text[i - 1];
            } else if (index >= 59 && index <= 62) {
                *(replaced + j++) = aliff[0];
                *(replaced + j++) = aliff[1];
            } else if (index >= 63 && index <= 65) {
                if (index == 63) {
                    *(replaced + j++) = r1[0];
                    *(replaced + j++) = r1[1];
                } else if (index == 64) {
                    *(replaced + j++) = r2[0];
                    *(replaced + j++) = r2[1];
                } else if (index == 65) {
                    *(replaced + j++) = r3[0];
                    *(replaced + j++) = r3[1];
                }
            }
        }
    }

    replaced[j] = '\0';
    if (debug) printf("\n\nLENGTH: %d\n\n", j);
    info->modifiedText = replaced;
    info->length = j;
    if (debug) printf("\nremove_diacritic END %s\n", replaced);
    return info;
}


struct WordInfo *splitInWordWithLength(const char *text, int length, int debug) {

    if (debug == 1) printf("\nsplitInWordWithLength %d\n", length);
    int totalWords = 0;
    int totalChar = 0;
    for (; totalChar < length; totalChar++) {
        if (text[totalChar] == ' ') {
            totalWords += 1;
        }
    }
    totalWords += 1;
    totalChar = length;

    struct WordInfo *info = (struct WordInfo *) sqlite3_malloc(sizeof(struct WordInfo));
    char **words = sqlite3_malloc(totalWords * sizeof(char *));
    int **actualStartIndexes = sqlite3_malloc(totalWords * sizeof(int *));
    int **actualEndIndexes = sqlite3_malloc(totalWords * sizeof(int *));
    int wordIndex = -1;
    int i = 0;
    int j = -1;
    int *startIndex = (int *) sqlite3_malloc(sizeof(int));
    int *endIndex = (int *) sqlite3_malloc(sizeof(int));
    char *word = (char *) sqlite3_malloc(totalChar + 5);
    *startIndex = i;
    for (; i < totalChar; i++) {
        if (debug) printf("\n%c\n", text[i]);
        if (text[i] == ' ') {
            if (j >= 0) {
                word[++j] = '\0';
                removeChars(word, j);
                words[++wordIndex] = word;
                *endIndex = i; // i > 0 ? i - 1 : i;
                actualStartIndexes[wordIndex] = startIndex;
                actualEndIndexes[wordIndex] = endIndex;
                j = -1;
                word = (char *) sqlite3_malloc(totalChar + 5);
                startIndex = (int *) sqlite3_malloc(sizeof(int));
                endIndex = (int *) sqlite3_malloc(sizeof(int));
                *startIndex = i; // i + 1;
            }
        } else {
            word[++j] = text[i];
        }
    }

    // for last word
    if (j >= 0) {
        word[++j] = '\0';
        words[++wordIndex] = word;
        *endIndex = i; // i > 0 ? i - 1 : i;
        actualStartIndexes[wordIndex] = startIndex;
        actualEndIndexes[wordIndex] = endIndex;
    }

    info->words = words;
    info->total = wordIndex + 1;
    info->actualStartIndexes = actualStartIndexes;
    info->actualEndIndexes = actualEndIndexes;
    return info;
}

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

static int v = 0;

static int fts5ArabicTokenizerCreate(
        void *pCtx,
        const char **azArg,
        int nArg,
        Fts5Tokenizer **ppOut
) {
    *ppOut = (Fts5Tokenizer * ) & v;
    return SQLITE_OK;
}

static void fts5ArabicTokenizerDelete(Fts5Tokenizer *p) {
    assert(p == (Fts5Tokenizer * ) & v);
}

static int fts5ArabicTokenizerTokenize(
        Fts5Tokenizer *pTokenizer,
        void *pCtx,
        int flags,
        const char *pText, int nText,
        int (*xToken)(void *, int, const char *, int, int, int)
) {

    struct WordInfo *wInfo = splitInWordWithLength(pText, nText, 0);
    for (int i = 0; i < wInfo->total; i++) {
        struct TextInfo *info = remove_diacritic(wInfo->words[i], 0);
        int rc = xToken(pCtx, 0, info->modifiedText, info->length, *wInfo->actualStartIndexes[i],
                        *wInfo->actualEndIndexes[i]);
        sqlite3_free(wInfo->words[i]);
        sqlite3_free(wInfo->actualStartIndexes[i]);
        sqlite3_free(wInfo->actualEndIndexes[i]);
        if (rc) return rc;
    }

    sqlite3_free(wInfo->words);
    sqlite3_free(wInfo->actualStartIndexes);
    sqlite3_free(wInfo->actualEndIndexes);
    sqlite3_free(wInfo);

    return SQLITE_OK;
}


#ifdef _WIN32
__declspec(dllexport)
#endif

int sqlite3_sqlitearabictokenizer_init(sqlite3 *db, char **error, const sqlite3_api_routines *api) {
    fts5_api *ftsApi;

    fts5_tokenizer tokenizer = {fts5ArabicTokenizerCreate, fts5ArabicTokenizerDelete, fts5ArabicTokenizerTokenize};

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