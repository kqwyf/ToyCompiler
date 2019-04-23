#ifndef __LEX_H__
#define __LEX_H__

#define MATCH_SOURCE

#include <vector>

#include "symbol.h"

using namespace std;

union LexicalSymbolValue {
    char *stringValue;
    struct {
        bool isFloat;
        union {
            int intValue;
            double floatValue;
        } value;
    } numberValue;
};

struct LexicalSymbolTableEntry {
    bool isString;
    LexicalSymbolValue value;
};

struct TokenTableEntry {
    SymbolType type;
    int index;
    int row, col;
    char *source;
#ifdef MATCH_SOURCE
    int start, end;
#endif
};

typedef vector<TokenTableEntry> TokenTable;
typedef vector<LexicalSymbolTableEntry> LexicalSymbolTable;

int lexicalAnalyse(const char *s, int l, TokenTable &tokenTable, LexicalSymbolTable &symbolTable);

#endif
