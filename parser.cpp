#include <cstdio>

using namespace std;

#include "lex.h"
#include "parser.h"
#include "grammar.h"

const GrammaSymbol templateGrammaSymbol = {
    -1 // type
};
const AnalyserStackItem templateStackItem = {
    -1, // stat
    templateGrammaSymbol // sym
};

AnalyserStack *stack = NULL;

#ifdef DEBUG
void printStack() {
    for(int i = 0; (unsigned long)i < stack->size(); i++)
        printf("%-3d|", (*stack)[i].stat);
    putchar('\n');
    for(int i = 0; (unsigned long)i < stack->size(); i++)
        printf("%-3d|", (*stack)[i].sym.type);
    putchar('\n');
}
#endif

void push(int stat, GrammaSymbol sym);
void pop();
void pop(int n);
inline int top(); // returns current symbol type
inline int current(); // returns current state

#ifdef PRINT_PRODUCTIONS
int parse(TokenTable &tokenTable, ProductionSequence &seq) {
#else
int parse(TokenTable &tokenTable) {
#endif
    stack = new AnalyserStack();
    GrammaSymbol endSymbol = templateGrammaSymbol;
    endSymbol.type = END_SYMBOL;
    push(INIT_STATE, endSymbol);
    int i = 0;
    while((unsigned long)i <= tokenTable.size() && !stack->empty()) {
#ifdef DEBUG
        printStack();
        putchar('\n');
#endif
        TokenTableEntry entry; 
        int type = NONE; // when it reaches the end of the token table, there is always an end symbol
        if(((unsigned long)i < tokenTable.size())) {
            entry = tokenTable[i];
            type = entry.type;
        }
        if(type == COMMENT) {
            i++;
            continue;
        }
        bool err = false;
        char action = ACTION[current()][type];
#ifdef DEBUG
        printf("[DEBUG] Action: %c\n", action == '\0' ? '0' : action);
#endif
        if(action == 's') {
            int stat = GOTO[current()][type];
            if(stat < 0) {
                return -1; // control should never reach here
            } else {
                GrammaSymbol sym = templateGrammaSymbol;
                sym.type = type;
                push(stat, sym);
                i++;
#ifdef DEBUG
                printf("[DEBUG] Shift symbol: %d\n", type);
#endif
            }
        } else if(action == 'r') {
            int pro = GOTO[current()][type];
            if(pro < 0) {
                return -1; // control should never reach here
            } else {
                pop(PRO_LENGTH[pro]);
                GrammaSymbol sym = templateGrammaSymbol;
                sym.type = PRO_LEFT[pro];
                int stat = GOTO[current()][sym.type];
                push(stat, sym);
#ifdef DEBUG
                printf("[DEBUG] Reduce: %d\n", pro);
#endif
#ifdef PRINT_PRODUCTIONS
                seq.push_back(pro);
#endif
            }
        } else if((unsigned long)i == tokenTable.size()) {
            break;
        } else {
            err = true;
        }
        if(err) {
            printf(GRAMMA_ERROR_MESSAGE[current()], entry.row, entry.col, entry.source); // TODO
            // error recovery
            while(!stack->empty() && RECOVER_SYMBOL[current()].empty())
                pop();
            if(stack->empty())
                break;
            int symType = -1;
            for(int stat = current(); (unsigned long)i < tokenTable.size(); i++) {
                for(set<int>::iterator it = RECOVER_SYMBOL[stat].begin(); it != RECOVER_SYMBOL[stat].end(); it++) {
                    if(GOTO[GOTO[stat][*it]][tokenTable[i].type] != -1) {
                        symType = *it;
                        break;
                    }
                }
                if(symType != -1)
                    break;
            }
            if(symType == -1)
                break;
            GrammaSymbol sym = templateGrammaSymbol;
            sym.type = symType;
            push(GOTO[current()][symType], sym);
        }
    }
    if(ACTION[current()][END_SYMBOL] != 'a') {
        printf("Line %d, Col 1: Uncompleted code.\n", tokenTable.back().row + 1);
        // TODO: error handling
    }
    delete stack;
    return 0;
}

void push(int stat, GrammaSymbol sym) {
    stack->push_back(templateStackItem);
    stack->back().stat = stat;
    stack->back().sym = sym;
}

void pop() {
    stack->pop_back();
}

void pop(int n) {
    while(n--)
        stack->pop_back();
}

int current() {
    return stack->back().stat;
}

int top() {
    return stack->back().sym.type;
}

