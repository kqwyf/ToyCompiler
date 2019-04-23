#include <cstdio>
#include <cstring>

using namespace std;

#include "lex.h"
#include "parser.h"
#include "grammar.h"

// Parsing context
AnalyserStack *stack = NULL;
SymbolTable *symbolTable = NULL;
InstTable *instTable = NULL;
LabelTable *labelTable = NULL;

#ifdef DEBUG
void printStack() {
    for(int i = 0; (unsigned long)i < stack->size(); i++)
        fprintf(stderr, "%-3d|", (*stack)[i].stat);
    fputchar(stderr, '\n');
    for(int i = 0; (unsigned long)i < stack->size(); i++)
        fprintf(stderr, "%-3d|", (*stack)[i].sym.type);
    fputchar(stderr, '\n');
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
    GrammaSymbol endSymbol = GrammaSymbol(/*code=*/-1, /*end=*/-1, /*type=*/END_SYMBOL);
    push(INIT_STATE, endSymbol);
    int i = 0;
    while((unsigned long)i <= tokenTable.size() && !stack->empty()) {
#ifdef DEBUG
        printStack();
        fputchar(stderr, '\n');
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
        fprintf(stderr, "[DEBUG] Action: %c\n", action == '\0' ? '0' : action);
#endif
        if(action == 's') {
            int stat = GOTO[current()][type];
            if(stat < 0) {
#ifdef DEBUG
                fprintf(stderr, "[ERROR] Action is 's' but goto is -1.\n");
#endif
                return -1; // control should never reach here
            } else {
                GrammaSymbol sym = GrammaSymbol(-1, -1, type);
                push(stat, sym);
                i++;
#ifdef DEBUG
                fprintf(stderr, "[DEBUG] Shift symbol: %d\n", type);
#endif
            }
        } else if(action == 'r') {
            int pro = GOTO[current()][type];
            if(pro < 0) {
                return -1; // control should never reach here
            } else {
                pop(PRO_LENGTH[pro]);
                GrammaSymbol sym = GrammaSymbol(-1, -1, PRO_LEFT[pro]);
                int stat = GOTO[current()][sym.type];
                push(stat, sym);
#ifdef DEBUG
                fprintf(stderr, "[DEBUG] Reduce: %d\n", pro);
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
            printf(GRAMMA_ERROR_MESSAGE[current()], entry.row, entry.col, entry.source);
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
            GrammaSymbol sym = GrammaSymbol(-1, -1, symType);
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
    stack->push_back(AnalyserStackItem(stat, sym));
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

GrammaSymbol::GrammaSymbol(int code, int end, int type) : code(code),
                                                          end(end),
                                                          type(type) {
    if(type == EXPRESSION || (EXPRESSION1 <= type && type <= EXPRESSION8))
        this->attr.exp = new ExpInfo();
    else if(type == EXPRESSION_S)
        this->attr.exps = new ExpsInfo();
    else if(type == IDENTIFIER_S)
        this->attr.ids = new IdsInfo();
    else if(type == SELECT_BEGIN)
        this->attr.sel_b = new SelBeginInfo();
    else if(type == LOOP_BEGIN)
        this->attr.loop_b = new LoopBeginInfo();
    else if(type == TYPE_ARRAY)
        this->attr.arr = new ArrayInfo();
    else if(type == CONSTANT)
        this->attr.con = new ConstInfo();
}

AnalyserStackItem::AnalyserStackItem(int stat, GrammaSymbol sym) : stat(stat), sym(sym) {}

SymbolTable::SymbolTable(SymbolTable *parent) : tempCount(0),
                                                parent(parent) {
    // this->tempTable = new TempSymbolTable(this); // TODO: check if temp table is necessary
}

SymbolTableEntryRef SymbolTable::newSymbol(const char *name, int type, SymbolDataType dataType, int size) {
    int nameLen = strlen(name);
    SymbolTableEntry entry;
    entry.name.strValue = new char[nameLen + 1];
    strcpy(entry.name.strValue, name);
    entry.type = type;
    entry.dataType = dataType;
    entry.offset = this->offset;
    this->offset += size;
    int index = this->size();
    this->push_back(entry);
    return (SymbolTableEntryRef){this, index};
}

SymbolTableEntryRef SymbolTable::newSymbol(int value, int type) {
    SymbolTableEntry entry;
    entry.name.intValue = value;
    entry.type = type;
    entry.dataType = DT_INT;
    entry.offset = this->offset;
    this->offset += INT_SIZE;
    int index = this->size();
    this->push_back(entry);
    return (SymbolTableEntryRef){this, index};
}

SymbolTableEntryRef SymbolTable::newSymbol(double value, int type) {
    SymbolTableEntry entry;
    entry.name.floatValue = value;
    entry.type = type;
    entry.dataType = DT_FLOAT;
    entry.offset = this->offset;
    this->offset += FLOAT_SIZE;
    int index = this->size();
    this->push_back(entry);
    return (SymbolTableEntryRef){this, index};
}

SymbolTableEntryRef SymbolTable::newTemp(SymbolTableEntry &entry, int size) {
    int oldOffset = entry.offset;
    this->tempCount++;
    entry.offset = this->offset;
    this->offset += size;
    int index = this->size();
    this->push_back(entry);
    entry.offset = oldOffset;
    return (SymbolTableEntryRef){this, index};
}

void SymbolTable::freeTemp() {
    if(this->tempCount <= 0) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Can't free temp symbol.\n"); // control should never reach here
#endif
        return;
    }
    this->tempCount--;
    this->pop_back();
}

int InstTable::gen(OpCode op, const SymbolTableEntryRef &arg1, const SymbolTableEntryRef &arg2, const SymbolTableEntryRef &result) {
    Inst inst;
    inst.index = this->size();
    inst.label = -1;
    inst.op = op;
    inst.arg1 = arg1;
    inst.arg2 = arg2;
    inst.result = result;
    this->push_back(inst);
    return inst.index;
}

void InstTable::backPatch(list<int> &l, int label) {
    for(list<int>::iterator it = l.begin(); it != l.end(); it++) {
        (*this)[*it].result.table = NULL;
        (*this)[*it].result.index = label;
    }
}

int LabelTable::newLabel(int index) {
    int result = this->size();
    this->push_back(index);
    return result;
}

