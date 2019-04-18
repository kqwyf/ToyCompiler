#include <cstdio>

using namespace std;

#include "lex.h"
#include "parser.h"
#include "grammar.h"

const GrammaSymbol templateGrammaSymbol = {
    -1 // label
};
const AnalyserStackItem templateStackItem = {
    -1, // stat
    templateGrammaSymbol // sym
};

AnalyserStack *stack = NULL;

///////////////////////////////////
void printStack() {
    for(int i = 0; (unsigned long)i < stack->size(); i++)
        printf("%-3d|", (*stack)[i].stat);
    putchar('\n');
    for(int i = 0; (unsigned long)i < stack->size(); i++)
        printf("%-3d|", (*stack)[i].sym.label);
    putchar('\n');
}
///////////////////////////////////

void push(int stat, GrammaSymbol sym);
void pop();
void pop(int n);
inline int top(); // returns current symbol label
inline int current(); // returns current state

#ifdef PRINT_PRODUCTIONS
int parse(TokenTable &tokenTable, ProductionSequence &seq) {
#else
int parse(TokenTable &tokenTable) {
#endif
    stack = new AnalyserStack();
    GrammaSymbol endSymbol = templateGrammaSymbol;
    endSymbol.label = END_SYMBOL;
    push(INIT_STATE, endSymbol);
    int i = 0;
    while((unsigned long)i <= tokenTable.size() && !stack->empty()) {
        //printStack(); ////////////////////////////////
        //putchar('\n'); ///////////////////////////////
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
        //printf("[DEBUG] key : %d, %d\n", current(), type); ///////////////////////////////
        char action = ACTION[current()][type];
        //printf("[DEBUG] Action: %c\n", action == '\0' ? '0' : action); //////////////////////////////////
        if(action == 's') {
            int stat = GOTO[current()][type];
            if(stat < 0) {
                return -1; // control should never reach here
            } else {
                GrammaSymbol sym = templateGrammaSymbol;
                sym.label = type;
                push(stat, sym);
                i++;
            }
        } else if(action == 'r') {
            int pro = GOTO[current()][type];
            if(pro < 0) {
                return -1; // control should never reach here
            } else {
                pop(PRO_LENGTH[pro]);
                GrammaSymbol sym = templateGrammaSymbol;
                sym.label = PRO_LEFT[pro];
                int stat = GOTO[current()][sym.label];
                push(stat, sym);
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
            //printf("[DEBUG] Error occured.\n"); ///////////////////////////////////
            printf(GRAMMA_ERROR_MESSAGE[current()], tokenTable[i].row, tokenTable[i].col, "TODO"); // TODO
            // error recovery
            while(!stack->empty() && RECOVER_SYMBOL[current()].empty())
                pop();
            //printStack(); ///////////////////////////////////
            if(stack->empty())
                break;
            int label = -1;
            for(int stat = current(); (unsigned long)i < tokenTable.size(); i++) {
                for(set<int>::iterator it = RECOVER_SYMBOL[stat].begin(); it != RECOVER_SYMBOL[stat].end(); it++) {
                    if(FOLLOW[*it].find(tokenTable[i].type) != FOLLOW[*it].end()) {
                        label = *it;
                        break;
                    }
                }
                if(label != -1)
                    break;
            }
            if(label == -1)
                break;
            GrammaSymbol sym = templateGrammaSymbol;
            sym.label = label;
            push(GOTO[current()][label], sym);
            printf("[DEBUG] Error handling finished.\n");
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
    return stack->back().sym.label;
}

