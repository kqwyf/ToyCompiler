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
                GrammaSymbol sym = GrammaSymbol(-1, -1, PRO_LEFT[pro]);
                semanticActions[pro](*stack, sym, PRO_LENGTH[pro]);
                pop(PRO_LENGTH[pro]);
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

/*****************************
 * Semantic Action Functions *
 *****************************/

// PROGRAM -> DELARE_S
void SA_0(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_1(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_2(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_3(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_4(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_5(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_6(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_7(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_8(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_9(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_10(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_11(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_12(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_13(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_14(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_15(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_16(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_17(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_18(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_19(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_20(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_21(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_22(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_23(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_24(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_25(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_26(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_27(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_28(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_29(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_30(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_31(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_32(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_33(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_34(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_35(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_36(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_37(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_38(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_39(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_40(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_41(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_42(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_43(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_44(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_45(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_46(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_47(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_48(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_49(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_50(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_51(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_52(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_53(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_54(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_55(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_56(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_57(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_58(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_59(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_60(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_61(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_62(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_63(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_64(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_65(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_66(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_67(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_68(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_69(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_70(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_71(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_72(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_73(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_74(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_75(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_76(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void SA_77(AnalyserStack &stack, GrammaSymbol &sym, int proLen) {
}

void (*(semanticActions[PRO_N]))(AnalyserStack &stack, GrammaSymbol &sym, int proLen) = {
    SA_0,
    SA_1,
    SA_2,
    SA_3,
    SA_4,
    SA_5,
    SA_6,
    SA_7,
    SA_8,
    SA_9,
    SA_10,
    SA_11,
    SA_12,
    SA_13,
    SA_14,
    SA_15,
    SA_16,
    SA_17,
    SA_18,
    SA_19,
    SA_20,
    SA_21,
    SA_22,
    SA_23,
    SA_24,
    SA_25,
    SA_26,
    SA_27,
    SA_28,
    SA_29,
    SA_30,
    SA_31,
    SA_32,
    SA_33,
    SA_34,
    SA_35,
    SA_36,
    SA_37,
    SA_38,
    SA_39,
    SA_40,
    SA_41,
    SA_42,
    SA_43,
    SA_44,
    SA_45,
    SA_46,
    SA_47,
    SA_48,
    SA_49,
    SA_50,
    SA_51,
    SA_52,
    SA_53,
    SA_54,
    SA_55,
    SA_56,
    SA_57,
    SA_58,
    SA_59,
    SA_60,
    SA_61,
    SA_62,
    SA_63,
    SA_64,
    SA_65,
    SA_66,
    SA_67,
    SA_68,
    SA_69,
    SA_70,
    SA_71,
    SA_72,
    SA_73,
    SA_74,
    SA_75,
    SA_76,
    SA_77,
};

