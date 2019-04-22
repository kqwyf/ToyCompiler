#ifndef __PARSER_H__
#define __PARSER_H__

#define PRINT_PRODUCTIONS

#include <vector>

#include "lex.h"

struct GrammaSymbol {
    int type;
};

struct AnalyserStackItem {
    int stat;
    GrammaSymbol sym;
};

typedef vector<AnalyserStackItem> AnalyserStack;
typedef vector<int> ProductionSequence;

#ifdef PRINT_PRODUCTIONS
int parse(TokenTable &tokenTable, ProductionSequence &seq);
#else
int parse(TokenTable &tokenTable);
#endif

#endif
