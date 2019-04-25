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
int link(GrammaSymbol &a, GrammaSymbol &b);

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
                semanticActions[pro](sym);
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

int link(GrammaSymbol &a, GrammaSymbol &b) {
    if(a.code == -1) {
        a.code = b.code;
        a.end = b.end;
    } else {
        (*instTable)[a.end].next = b.code;
        if(b.end != -1)
            a.end = b.end;
    }
    return a.code;
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
    l.clear();
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
void SA_0(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol declare_s = (*stack)[n - 1].sym;
    sym.code = declare_s.code;
    sym.end = declare_s.end;
#ifdef DEBUG
    if(declare_s.nextList.empty())
        fprintf(stderr, "nextList is not empty.\n");
#endif
}

// PROGRAM ->
void SA_1(GrammaSymbol &sym) {
    int n = stack->size();
    sym.code = sym.end = -1;
}

// STATEMENT_S -> STATEMENT_S STATEMENT
void SA_2(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol statement_s = (*stack)[n - 2].sym;
    GrammaSymbol statement = (*stack)[n - 1].sym;
    if(statement.code != -1) {
        if(!statement_s.nextList.empty()) {
            int label = (*instTable)[statement.code].label = labelTable->newLabel(statement.code);
            instTable->backPatch(statement_s.nextList, label);
        }
        link(statement_s, statement);
    }
    // if statement.code is empty, statement.nextList must be empty. so just ignore it
    sym.code = statement_s.code;
    sym.end = statement_s.end;
}

// STATEMENT_S -> STATEMENT
void SA_3(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol statement = (*stack)[n - 1].sym;
    sym.code = statement.code;
    sym.end = statement.end;
    sym.nextList.splice(sym.nextList.end(), statement.nextList);
}

// STATEMENT- > DECLARE
void SA_4(GrammaSymbol &sym) {
}

void SA_5(GrammaSymbol &sym) {
}

void SA_6(GrammaSymbol &sym) {
}

void SA_7(GrammaSymbol &sym) {
}

void SA_8(GrammaSymbol &sym) {
}

void SA_9(GrammaSymbol &sym) {
}

void SA_10(GrammaSymbol &sym) {
}

void SA_11(GrammaSymbol &sym) {
}

void SA_12(GrammaSymbol &sym) {
}

void SA_13(GrammaSymbol &sym) {
}

void SA_14(GrammaSymbol &sym) {
}

void SA_15(GrammaSymbol &sym) {
}

void SA_16(GrammaSymbol &sym) {
}

void SA_17(GrammaSymbol &sym) {
}

void SA_18(GrammaSymbol &sym) {
}

void SA_19(GrammaSymbol &sym) {
}

void SA_20(GrammaSymbol &sym) {
}

void SA_21(GrammaSymbol &sym) {
}

void SA_22(GrammaSymbol &sym) {
}

void SA_23(GrammaSymbol &sym) {
}

void SA_24(GrammaSymbol &sym) {
}

void SA_25(GrammaSymbol &sym) {
}

void SA_26(GrammaSymbol &sym) {
}

void SA_27(GrammaSymbol &sym) {
}

void SA_28(GrammaSymbol &sym) {
}

void SA_29(GrammaSymbol &sym) {
}

void SA_30(GrammaSymbol &sym) {
}

void SA_31(GrammaSymbol &sym) {
}

void SA_32(GrammaSymbol &sym) {
}

void SA_33(GrammaSymbol &sym) {
}

void SA_34(GrammaSymbol &sym) {
}

void SA_35(GrammaSymbol &sym) {
}

void SA_36(GrammaSymbol &sym) {
}

void SA_37(GrammaSymbol &sym) {
}

void SA_38(GrammaSymbol &sym) {
}

void SA_39(GrammaSymbol &sym) {
}

void SA_40(GrammaSymbol &sym) {
}

void SA_41(GrammaSymbol &sym) {
}

void SA_42(GrammaSymbol &sym) {
}

void SA_43(GrammaSymbol &sym) {
}

void SA_44(GrammaSymbol &sym) {
}

void SA_45(GrammaSymbol &sym) {
}

void SA_46(GrammaSymbol &sym) {
}

void SA_47(GrammaSymbol &sym) {
}

void SA_48(GrammaSymbol &sym) {
}

void SA_49(GrammaSymbol &sym) {
}

void SA_50(GrammaSymbol &sym) {
}

void SA_51(GrammaSymbol &sym) {
}

void SA_52(GrammaSymbol &sym) {
}

void SA_53(GrammaSymbol &sym) {
}

void SA_54(GrammaSymbol &sym) {
}

void SA_55(GrammaSymbol &sym) {
}

void SA_56(GrammaSymbol &sym) {
}

void SA_57(GrammaSymbol &sym) {
}

void SA_58(GrammaSymbol &sym) {
}

void SA_59(GrammaSymbol &sym) {
}

void SA_60(GrammaSymbol &sym) {
}

void SA_61(GrammaSymbol &sym) {
}

void SA_62(GrammaSymbol &sym) {
}

void SA_63(GrammaSymbol &sym) {
}

void SA_64(GrammaSymbol &sym) {
}

void SA_65(GrammaSymbol &sym) {
}

void SA_66(GrammaSymbol &sym) {
}

void SA_67(GrammaSymbol &sym) {
}

void SA_68(GrammaSymbol &sym) {
}

void SA_69(GrammaSymbol &sym) {
}

void SA_70(GrammaSymbol &sym) {
}

void SA_71(GrammaSymbol &sym) {
}

void SA_72(GrammaSymbol &sym) {
}

void SA_73(GrammaSymbol &sym) {
}

void SA_74(GrammaSymbol &sym) {
}

void SA_75(GrammaSymbol &sym) {
}

void SA_76(GrammaSymbol &sym) {
}

void SA_77(GrammaSymbol &sym) {
}

void (*(semanticActions[PRO_N]))(GrammaSymbol &sym) = {
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
};

