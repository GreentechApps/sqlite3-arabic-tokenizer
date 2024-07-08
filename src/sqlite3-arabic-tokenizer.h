//
// Created by Shahriar Nasim Nafi on 18/11/22.
//

#ifndef SQLITE3_ARABIC_TOKENIZER_SQLITE3_ARABIC_TOKENIZER_H
#define SQLITE3_ARABIC_TOKENIZER_SQLITE3_ARABIC_TOKENIZER_H

#include "sqlite3ext.h"

#ifdef _WIN32
    #ifdef SQLITE3_ARABIC_TOKENIZER_EXPORTS
        #define SQLITE3_ARABIC_TOKENIZER_API __declspec(dllexport)
    #else
        #define SQLITE3_ARABIC_TOKENIZER_API __declspec(dllimport)
    #endif
#else
    #define SQLITE3_ARABIC_TOKENIZER_API
#endif

SQLITE3_ARABIC_TOKENIZER_API int sqlite3_sqlitearabictokenizer_init(sqlite3 *db, char **error, const sqlite3_api_routines *api);

#endif //SQLITE3_ARABIC_TOKENIZER_SQLITE3_ARABIC_TOKENIZER_H