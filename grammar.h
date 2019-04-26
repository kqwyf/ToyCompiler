/*
 * This file is generated automatically by the LR(1) grammar analyser.
 */

#ifndef __GRAMMAR_H__
#define __GRAMMAR_H__

#include <set>

#include "parser.h"

using namespace std;

const int STATE_N = 294;
const int SYMBOL_N = 71;
const int PRO_N = 79;
const int TERMINAL_N = 35;

const int INIT_STATE = 0;
const int END_SYMBOL = 0;

extern const int GOTO[STATE_N][SYMBOL_N];
extern const char ACTION[STATE_N][TERMINAL_N];
extern const int PRO_LEFT[PRO_N];
extern const int PRO_LENGTH[PRO_N];
extern const set<int> RECOVER_SYMBOL[STATE_N];
extern const set<int> FOLLOW[SYMBOL_N];
extern const char *(PRO[PRO_N]);
extern const char *(GRAMMA_ERROR_MESSAGE[STATE_N]);

extern int (*(semanticActions[PRO_N]))(GrammaSymbol &sym);
inline bool isTerminal(int label);

#endif
 
