#include <cstdio>
#include <cstring>
#include <string>

using namespace std;

#include "lex.h"
#include "parser.h"
#include "grammar.h"

// Parsing context
int row = -1, col = -1;
AnalyserStack *stack = NULL;
SymbolTable *symbolTable = NULL;
InstTable *instTable = NULL;
LabelTable *labelTable = NULL;
LexicalSymbolTable *nameTable = NULL;

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
int sizeOf(TypeInfo *typ, bool isDef);
void enterTable(SymbolTable *table);
int quitTable();

#ifdef PRINT_PRODUCTIONS
int parse(TokenTable &tokenTable, LexicalSymbolTable *lexicalSymbolTable, ProductionSequence &seq) {
#else
int parse(TokenTable &tokenTable, LexicalSymbolTable *lexicalSymbolTable) {
#endif
    nameTable = lexicalSymbolTable;
    SymbolTable::global = new SymbolTable(NULL);
    instTable = new InstTable();
    labelTable = new LabelTable();
    stack = new AnalyserStack();
    enterTable(SymbolTable::global);
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
        row = entry.row;
        col = entry.col;
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
                if(type == IDENTIFIER || type == CONSTANT)
                    sym.attr.id->nameIndex = entry.index;
                push(stat, sym);
                i++;
#ifdef DEBUG
                fprintf(stderr, "[DEBUG] Shift symbol: %d\n", type);
#endif
            }
        } else if(action == 'r') {
            int pro = GOTO[current()][type];
            if(pro < 0) {
#ifdef DEBUG
                fprintf(stderr, "[ERROR] Action is 'r' but goto is -1.\n");
#endif
                return -1; // control should never reach here
            } else {
                if(stack->size() <= (unsigned long)PRO_LENGTH[pro]) {
#ifdef DEBUG
                    fprintf(stderr, "[ERROR] Gramma analysis error.\n");
#endif
                    return -1;
                }
                GrammaSymbol sym = GrammaSymbol(-1, -1, PRO_LEFT[pro]);
                int SAerr = semanticActions[pro](sym); // -1 for internal error
                if(SAerr == -1) return -1;             // -2 for compile error
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
            printf(GRAMMA_ERROR_MESSAGE[current()], row, col, entry.source);
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
            // TODO: call the semantic action function with proper stack
            GrammaSymbol sym = GrammaSymbol(-1, -1, symType);
            push(GOTO[current()][symType], sym);
        }
    }
    if(ACTION[current()][END_SYMBOL] != 'a') {
        printf("Line %d, Col 1: Uncompleted code.\n", tokenTable.back().row + 1);
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

/**
 * Returns: -1 for internal error, -2 for compile error.
 */
int sizeOf(TypeInfo *typ) {
    int size = -1;
    if(typ->dataType == DT_INT)
        size = INT_SIZE;
    else if(typ->dataType == DT_FLOAT)
        size = FLOAT_SIZE;
    else if(typ->dataType == DT_BOOL)
        size = BOOL_SIZE;
    else if(typ->dataType == DT_ARRAY) {
        list<int> *sizeList = &(typ->attr.arr->sizes);
        if(typ->attr.arr->type == DT_INT)
            size = INT_SIZE;
        else if(typ->attr.arr->type == DT_FLOAT)
            size = FLOAT_SIZE;
        else if(typ->attr.arr->type == DT_BOOL)
            size = BOOL_SIZE;
        else {
#ifdef DEBUG
            fprintf(stderr, "[ERROR] Invalid array type.\n");
#endif
            return -1;
        }
        for(list<int>::iterator it = sizeList->begin(); it != sizeList->end(); it++)
            size *= *it;
    } else if(typ->dataType == DT_STRUCT) {
        if(typ->attr.table->busy) {
            printf("Line %d, Col %d: Recursion reference to struct definition.\n", row, col);
            return -2;
        }
        size = typ->attr.table->offset;
    } else {
        return 0;
    }
}

void enterTable(SymbolTable *table) {
    symbolTable = table;
    symbolTable->busy = true;
}

int quitTable() {
    if(symbolTable->parent == NULL) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Can't quit the current symbol table (global).");
#endif
        return -1;
    }
    symbolTable->busy = false;
    symbolTable = symbolTable->parent;
    return 0;
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
                                                offset(0),
                                                busy(false),
                                                parent(parent) {
    // this->tempTable = new TempSymbolTable(this); // TODO: check if temp table is necessary
}

SymbolTableEntryRef SymbolTable::newSymbol(const char *name, int type, SymbolDataType dataType, int size) {
    SymbolTableEntry entry;
    if(name != NULL) {
        int nameLen = strlen(name);
        entry.name.strValue = new char[nameLen + 1];
        strcpy(entry.name.strValue, name);
    } else {
        entry.name.strValue = NULL;
    }
    entry.type = type;
    entry.dataType = dataType;
    entry.offset = this->offset;
    this->offset += size;
    int index = this->size();
    this->push_back(entry);
    return (SymbolTableEntryRef){this, index};
}

SymbolTableEntryRef SymbolTable::newSymbol(int value) {
    SymbolTableEntry entry;
    entry.name.intValue = value;
    entry.type = CONSTANT;
    entry.dataType = DT_INT;
    entry.offset = this->offset;
    this->offset += INT_SIZE;
    int index = this->size();
    this->push_back(entry);
    return (SymbolTableEntryRef){this, index};
}

SymbolTableEntryRef SymbolTable::newSymbol(double value) {
    SymbolTableEntry entry;
    entry.name.floatValue = value;
    entry.type = CONSTANT;
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

SymbolTableEntryRef SymbolTable::findSymbol(const char *name) {
    SymbolTableEntryRef result;
    string str = string(name);
    if(this->stringMap.find(str) != this->stringMap.end()) {
        result.table = this;
        result.index = stringMap[str];
        return result;
    }
    if(this->parent != NULL) {
        return this->parent->findSymbol(name);
    } else {
        result.table = NULL;
        result.index = -1;
        return result;
    }
}

SymbolTableEntryRef SymbolTable::findSymbol(int value) {
    SymbolTableEntryRef result;
    if(SymbolTable::intMap.find(value) != SymbolTable::intMap.end()) {
        result.table = SymbolTable::global;
        result.index = SymbolTable::intMap[value];
    } else {
        result.table = NULL;
        result.index = -1;
    }
    return result;
}

SymbolTableEntryRef SymbolTable::findSymbol(double value) {
    SymbolTableEntryRef result;
    if(SymbolTable::floatMap.find(value) != SymbolTable::floatMap.end()) {
        result.table = SymbolTable::global;
        result.index = SymbolTable::floatMap[value];
    } else {
        result.table = NULL;
        result.index = -1;
    }
    return result;
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
int SA_0(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol declare_s = (*stack)[n - 1].sym;
    sym.code = declare_s.code;
    sym.end = declare_s.end;
    if(declare_s.nextList.empty()) {
#ifdef DEBUG
        fprintf(stderr, "nextList is not empty.\n");
#endif
        return -1;
    }
    return 0;
}

// STATEMENT_S -> STATEMENT_S STATEMENT
int SA_1(GrammaSymbol &sym) {
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
    return 0;
}

// STATEMENT_S -> STATEMENT
int SA_2(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol statement = (*stack)[n - 1].sym;
    sym.code = statement.code;
    sym.end = statement.end;
    sym.nextList.splice(sym.nextList.end(), statement.nextList);
    return 0;
}

// STATEMENT -> DECLARE_VAR
int SA_3(GrammaSymbol &sym) {
    sym.code = sym.end = -1;
    return 0;
}

// STATEMENT -> DECLARE_STRUCT
int SA_4(GrammaSymbol &sym) {
    sym.code = sym.end = -1;
    return 0;
}

// STATEMENT -> SELECT
int SA_5(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol select = (*stack)[n - 1].sym;
    sym.code = select.code;
    sym.end = select.end;
    sym.nextList.splice(sym.nextList.end(), select.nextList);
    return 0;
}

// STATEMENT -> LOOP
int SA_6(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol loop = (*stack)[n - 1].sym;
    sym.code = loop.code;
    sym.end = loop.end;
    sym.nextList.splice(sym.nextList.end(), loop.nextList);
    return 0;
}

// STATEMENT -> EXPRESSION ;
int SA_7(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol expression = (*stack)[n - 2].sym;
    sym.code = expression.code;
    sym.end = expression.end;
    sym.nextList.splice(sym.nextList.end(), expression.nextList);
    sym.nextList.splice(sym.nextList.end(), expression.attr.exp->trueList);
    sym.nextList.splice(sym.nextList.end(), expression.attr.exp->falseList);
    if(expression.attr.exp->isTemp)
        symbolTable->freeTemp();
    return 0;
}

// STATEMENT -> STATEMENTS_BEGIN STATEMENT_S }
int SA_8(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol statements_begin = (*stack)[n - 3].sym;
    GrammaSymbol statement_s = (*stack)[n - 2].sym;
    sym.code = statement_s.code;
    sym.end = statement_s.end;
    sym.nextList.splice(sym.nextList.end(), statement_s.nextList);
    return quitTable();
}

// STATEMENT -> { }
int SA_9(GrammaSymbol &sym) {
    sym.code = sym.end = -1;
    return 0;
}

// STATEMENTS_BEGIN -> {
int SA_10(GrammaSymbol &sym) {
    SymbolTableEntryRef ref = symbolTable->newSymbol(NULL, IDENTIFIER, DT_BLOCK, 0);
    SymbolTable *table = new SymbolTable(symbolTable);
    (*ref.table)[ref.index].attr.table = table;
    enterTable(table);
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE_S -> DECLARE_S DECLARE
int SA_11(GrammaSymbol &sym) {
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE_S -> DECLARE
int SA_12(GrammaSymbol &sym) {
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE -> DECLARE_VAR
int SA_13(GrammaSymbol &sym) {
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE -> DECLARE_STRUCT
int SA_14(GrammaSymbol &sym) {
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE -> DECLARE_FUNC
int SA_15(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol declare_func = (*stack)[n - 1].sym;
    sym.code = declare_func.code;
    sym.end = declare_func.end;
    return 0;
}

// DECLARE_VAR -> TYPE IDENTIFIER_S ;
int SA_16(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol type = (*stack)[n - 3].sym;
    GrammaSymbol identifier_s = (*stack)[n - 2].sym;
    int size = sizeOf(type.attr.typ);
    if(size == 0) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Invalid type in variant definition.\n");
#endif
        return -1;
    } else if(size < 0) {
        return size;
    }
    list<int> *l = &(identifier_s.attr.ids->nameIndexList);
    for(list<int>::iterator it = l->begin(); it != l->end(); it++) {
        SymbolTableEntryRef ref = symbolTable->newSymbol((*nameTable)[*it].value.stringValue, IDENTIFIER, type.attr.typ->dataType, size);
        if(type.attr.typ->dataType == DT_ARRAY)
            (*ref.table)[ref.index].attr.arr = type.attr.typ->attr.arr;
        else if(type.attr.typ->dataType == DT_STRUCT)
            (*ref.table)[ref.index].attr.table = type.attr.typ->attr.table;
    }
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE_VAR_S -> DECLARE_VAR_S DECLARE_VAR
int SA_17(GrammaSymbol &sym) {
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE_VAR_S -> DECLARE_VAR
int SA_18(GrammaSymbol &sym) {
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE_STRUCT -> DECLARE_STRUCT_BEGIN DECLARE_VAR_S } ;
int SA_19(GrammaSymbol &sym) {
    sym.code = sym.end = -1;
    quitTable();
    return 0;
}

// DECLARE_STRUCT_BEGIN -> TYPE_STRUCT {
int SA_20(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol type_struct = (*stack)[n - 2].sym;
    SymbolTableEntryRef ref = symbolTable->newSymbol((*nameTable)[type_struct.attr.typ_str->nameIndex].value.stringValue, IDENTIFIER, DT_STRUCT_DEF, 0);
    SymbolTable *table = new SymbolTable(symbolTable);
    (*ref.table)[ref.index].attr.table = table;
    enterTable(table);
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE_FUNC -> DECLARE_FUNC_BEGIN DECLARE_FUNC_MID STATEMENT_S }
int SA_21(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol declare_func_mid = (*stack)[n - 3].sym;
    GrammaSymbol statement_s = (*stack)[n - 2].sym;
    sym.code = statement_s.code;
    sym.end = statement_s.end;
    quitTable();
    int label = labelTable->newLabel(sym.code);
    symbolTable->back().offset = label; // the back element of the symbol table must be the symbol of this function at this time
    symbolTable->back().attr.func->pCount = declare_func_mid.attr.pCount;
    return 0;
}

// DECLARE_FUNC -> DECLARE_FUNC_BEGIN DECLARE_FUNC_MID }
int SA_22(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol declare_func_mid = (*stack)[n - 2].sym;
    sym.code = sym.end = -1; // TODO: add return statement automatically and set the offset as the label
    quitTable();
    // TODO: symbolTable->back().offset = label;
    symbolTable->back().attr.func->pCount = declare_func_mid.attr.pCount;
    return 0;
}

// DECLARE_FUNC_BEGIN -> TYPE identifier (
int SA_23(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol type = (*stack)[n - 3].sym;
    GrammaSymbol identifier = (*stack)[n - 2].sym;
    SymbolTableEntryRef ref = symbolTable->newSymbol((*nameTable)[identifier.attr.id->nameIndex].value.stringValue, IDENTIFIER, DT_BLOCK, 0);
    SymbolTable *table = new SymbolTable(symbolTable);
    (*ref.table)[ref.index].attr.func->table = table;
    enterTable(table);
    int size = sizeOf(type.attr.typ);
    if(size == 0) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Invalid return type in function definition.\n");
#endif
        return -1;
    } else if(size < 0) {
        return size;
    }
    ref = table->newSymbol(NULL, IDENTIFIER, type.attr.typ->dataType, size); // return value symbol
    if(type.attr.typ->dataType == DT_ARRAY)
        (*ref.table)[ref.index].attr.arr = type.attr.typ->attr.arr;
    else if(type.attr.typ->dataType == DT_STRUCT)
        (*ref.table)[ref.index].attr.table = type.attr.typ->attr.table;
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE_FUNC_MID -> PARAMETERS ) {
int SA_24(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol parameters = (*stack)[n - 3].sym;
    sym.attr.pCount = parameters.attr.pCount;
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE_FUNC_MID -> ) {
int SA_25(GrammaSymbol &sym) {
    sym.attr.pCount = 0;
    sym.code = sym.end = -1;
    return 0;
}

// PARAMETERS -> PARAMETERS , TYPE identifier
int SA_26(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol parameters = (*stack)[n - 4].sym;
    GrammaSymbol type = (*stack)[n - 2].sym;
    GrammaSymbol identifier = (*stack)[n - 1].sym;
    int size = sizeOf(type.attr.typ);
    if(size == 0) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Invalid type in parameter definition.\n");
#endif
        return -1;
    } else if(size < 0) {
        return size;
    }
    SymbolTableEntryRef ref = symbolTable->newSymbol((*nameTable)[identifier.attr.id->nameIndex].value.stringValue, IDENTIFIER, DT_BLOCK, 0);
    if(type.attr.typ->dataType == DT_ARRAY)
        (*ref.table)[ref.index].attr.arr = type.attr.typ->attr.arr;
    else if(type.attr.typ->dataType == DT_STRUCT)
        (*ref.table)[ref.index].attr.table = type.attr.typ->attr.table;
    sym.attr.pCount = parameters.attr.pCount + 1;
    sym.code = sym.end = -1;
    return 0;
}

// PARAMETERS -> TYPE identifier
int SA_27(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol type = (*stack)[n - 2].sym;
    GrammaSymbol identifier = (*stack)[n - 1].sym;
    int size = sizeOf(type.attr.typ);
    if(size == 0) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Invalid type in parameter definition.\n");
#endif
        return -1;
    } else if(size < 0) {
        return size;
    }
    SymbolTableEntryRef ref = symbolTable->newSymbol((*nameTable)[identifier.attr.id->nameIndex].value.stringValue, IDENTIFIER, DT_BLOCK, 0);
    if(type.attr.typ->dataType == DT_ARRAY)
        (*ref.table)[ref.index].attr.arr = type.attr.typ->attr.arr;
    else if(type.attr.typ->dataType == DT_STRUCT)
        (*ref.table)[ref.index].attr.table = type.attr.typ->attr.table;
    sym.attr.pCount = 1;
    sym.code = sym.end = -1;
    return 0;
}

// TYPE -> TYPE_BASIC
int SA_28(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol type_basic = (*stack)[n - 1].sym;
    sym.attr.typ->dataType = type_basic.attr.typ->dataType;
    sym.code = sym.end = -1;
    return 0;
}

// TYPE -> TYPE_ARRAY
int SA_29(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol type_array = (*stack)[n - 1].sym;
    sym.attr.typ->dataType = type_array.attr.typ->dataType;
    sym.attr.typ->attr.arr = type_array.attr.typ->attr.arr;
    sym.code = sym.end = -1;
    return 0;
}

// TYPE -> TYPE_STRUCT
int SA_30(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol type_struct = (*stack)[n - 1].sym;
    const char *name = (*nameTable)[type_struct.attr.typ_str->nameIndex].value.stringValue;
    SymbolTableEntryRef ref = symbolTable->findSymbol(name);
    if(ref.table == NULL) {
        printf("Line %d, Col %d: Undefined struct: %s.\n", row, col, name);
        return -2;
    }
    sym.attr.typ->dataType = DT_STRUCT;
    sym.attr.typ->attr.table = (*ref.table)[ref.index].attr.table;
    sym.code = sym.end = -1;
    return 0;
}

// TYPE_BASIC -> int
int SA_31(GrammaSymbol &sym) {
    sym.attr.typ->dataType = DT_INT;
    sym.code = sym.end = -1;
    return 0;
}

// TYPE_BASIC -> float
int SA_32(GrammaSymbol &sym) {
    sym.attr.typ->dataType = DT_FLOAT;
    sym.code = sym.end = -1;
    return 0;
}

// TYPE_BASIC -> bool
int SA_33(GrammaSymbol &sym) {
    sym.attr.typ->dataType = DT_BOOL;
    sym.code = sym.end = -1;
    return 0;
}

// TYPE_ARRAY -> TYPE_ARRAY [ constant ]
int SA_34(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol type_array = (*stack)[n - 4].sym;
    GrammaSymbol constant = (*stack)[n - 2].sym;
    if(constant.attr.con->dataType == DT_FLOAT) {
        printf("Line %d, Col %d: The size of array should be an integer.\n", row, col);
        return -2;
    } else if(constant.attr.con->dataType != DT_INT) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Constant type is not DT_INT or DT_FLOAT.\n");
#endif
        return -1;
    } else if((*nameTable)[constant.attr.con->nameIndex].value.numberValue.value.intValue <= 0) {
        printf("Line %d, Col %d: The size of array should be a positive integer.\n", row, col);
        return -2;
    }
    sym.attr.typ->attr.arr->ndim = type_array.attr.typ->attr.arr->ndim + 1;
    sym.attr.typ->attr.arr->sizes.splice(sym.attr.typ->attr.arr->sizes.end(), type_array.attr.typ->attr.arr->sizes);
    sym.attr.typ->attr.arr->sizes.push_back((*nameTable)[constant.attr.con->nameIndex].value.numberValue.value.intValue);
    sym.code = sym.end = -1;
    return 0;
}

// TYPE_ARRAY -> TYPE_BASIC [ constant ]
int SA_35(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol type_basic = (*stack)[n - 4].sym;
    GrammaSymbol constant = (*stack)[n - 2].sym;
    if(constant.attr.con->dataType == DT_FLOAT) {
        printf("Line %d, Col %d: The size of array should be an integer.\n", row, col);
        return -2;
    } else if(constant.attr.con->dataType != DT_INT) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Constant type is not DT_INT or DT_FLOAT.\n");
#endif
        return -1;
    } else if((*nameTable)[constant.attr.con->nameIndex].value.numberValue.value.intValue <= 0) {
        printf("Line %d, Col %d: The size of array should be a positive integer.\n", row, col);
        return -2;
    }
    sym.attr.typ->attr.arr = new ArrayInfo();
    sym.attr.typ->attr.arr->ndim = 1;
    sym.attr.typ->attr.arr->dataType = type_basic.attr.typ->dataType;
    sym.attr.typ->attr.arr->sizes.push_back((*nameTable)[constant.attr.con->nameIndex].value.numberValue.value.intValue);
    sym.code = sym.end = -1;
    return 0;
}

// TYPE_STRUCT -> struct identifier
int SA_36(GrammaSymbol &sym) {
    int n = stack->size();
    GrammaSymbol identifier = (*stack)[n - 1].sym;
    sym.attr.typ_str->nameIndex = identifier.attr.id->nameIndex;
    sym.code = sym.end = -1;
    return 0;
}

int SA_37(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_38(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_39(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_40(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_41(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_42(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_43(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_44(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_45(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_46(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_47(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_48(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_49(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_50(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_51(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_52(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_53(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_54(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_55(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_56(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_57(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_58(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_59(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_60(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_61(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_62(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_63(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_64(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_65(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_66(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_67(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_68(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_69(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_70(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_71(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_72(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_73(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_74(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_75(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_76(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_77(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int SA_78(GrammaSymbol &sym) {
    int n = stack->size();
    return 0;
}

int (*(semanticActions[PRO_N]))(GrammaSymbol &sym) = {
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
    SA_78,
};

