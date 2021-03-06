// TODO: add nested struct support
#include <cstdio>
#include <cstring>
#include <string>

using namespace std;

#include "lex.h"
#include "parser.h"
#include "grammar.h"

static const SymbolTableEntryRef NULL_REF = {NULL, -1};

// Parsing context
int SymbolTable::n = 0;
list<SymbolTable*> SymbolTable::tables;
AnalyserStack *stack = NULL;
SymbolTable *symbolTable = NULL;
SymbolTable *SymbolTable::global = NULL;
InstTable *instTable = NULL;
LexicalSymbolTable *nameTable = NULL;

#ifdef DEBUG
void printStack() {
    for(int i = 0; (unsigned long)i < stack->size(); i++)
        fprintf(stderr, "%-3d|", (*stack)[i].stat);
    fprintf(stderr, "\n");
    for(int i = 0; (unsigned long)i < stack->size(); i++)
        fprintf(stderr, "%-3d|", (*stack)[i].sym.type);
    fprintf(stderr, "\n");
}

void printCodeSegment(GrammaSymbol &sym) {
    fprintf(stderr, "Inst table size: %lu\n", instTable->size());
    fprintf(stderr, "Code Segment for symbol %d: init code: %d\n", sym.type, sym.code);
    for(int i = sym.code; i != -1; i = (*instTable)[i].next) {
        fprintf(stderr, " %2d | ", (*instTable)[i].op);
        if((*instTable)[i].arg1.index >= 0)
            fprintf(stderr, " %3d:%-3d | ", (*instTable)[i].arg1.table->number, (*instTable)[i].arg1.index);
        else
            fprintf(stderr, "         | ");
        if((*instTable)[i].arg2.index >= 0)
            fprintf(stderr, " %3d:%-3d | ", (*instTable)[i].arg2.table->number, (*instTable)[i].arg2.index);
        else
            fprintf(stderr, "         | ");
        if((*instTable)[i].result.index < 0)
            fprintf(stderr, "\n");
        else if((*instTable)[i].result.table == NULL)
            fprintf(stderr, " %3d\n", (*instTable)[i].result.index);
        else
            fprintf(stderr, " %3d:%-3d\n", (*instTable)[i].result.table->number, (*instTable)[i].result.index);
    }
    fprintf(stderr, "\n");
}
#endif

void push(int stat, GrammaSymbol sym);
void pop();
void pop(int n);
inline int top(); // returns current symbol type
inline int current(); // returns current state
int link(GrammaSymbol &a, GrammaSymbol &b);
int link(GrammaSymbol &a, int b);
int link(int a, GrammaSymbol &b);
int link(int a, int b);
int sizeOf(TypeInfo *typ, int row, int col);
int sizeOf(SymbolDataType dataType);
void enterTable(SymbolTable *table);
int quitTable();
pair<int, int> evalBoolExp(ExpInfo *exp, int next);
pair<int, int> genBoolJmpCode(ExpInfo *exp);
pair<int, SymbolDataType> genMovsCode(ExpInfo *exp);
bool typeMatch(ExpInfo *lexp, ExpInfo *rexp);
bool typeMatch(SymbolDataType ldataType, ExpInfo *rexp);
bool typeMatch(SymbolTableEntryRef &lref, ExpInfo *rexp);
SymbolDataType typeOf(ExpInfo *exp);

#ifdef PRINT_PRODUCTIONS
int parse(TokenTable &tokenTable, LexicalSymbolTable *lexicalSymbolTable, InstTable *iTable, ProductionSequence &seq) {
#else
int parse(TokenTable &tokenTable, LexicalSymbolTable *lexicalSymbolTable, InstTable *iTable) {
#endif
    if(iTable != NULL) { // semantic analysis mode
        nameTable = lexicalSymbolTable;
        SymbolTable::global = new SymbolTable(NULL, false);
        instTable = new InstTable();
        enterTable(SymbolTable::global);
        for(unsigned long i = 1; i < nameTable->size(); i++) {
            if(!((*nameTable)[i].isString)) {
                LexicalSymbolValue &value = (*nameTable)[i].value;
                if(value.numberValue.isFloat)
                    symbolTable->newSymbol(i, CONSTANT, DT_FLOAT, FLOAT_SIZE);
                else
                    symbolTable->newSymbol(i, CONSTANT, DT_INT, INT_SIZE);
            }
        }
    }
    stack = new AnalyserStack();
    GrammaSymbol endSymbol = GrammaSymbol(/*code=*/-1, /*end=*/-1, /*type=*/END_SYMBOL, /*row=*/1, /*col=*/1);
    push(INIT_STATE, endSymbol);
    int i = 0;
    int returnCode = 0;
    while((unsigned long)i <= tokenTable.size() && !stack->empty()) {
#ifdef DEBUG
        //printStack();
        //fprintf(stderr, "\n");
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
        //fprintf(stderr, "[DEBUG] Action: %c\n", action == '\0' ? '0' : action);
#endif
        if(action == 's') {
            int stat = GOTO[current()][type];
            if(stat < 0) {
#ifdef DEBUG
                fprintf(stderr, "[ERROR] Action is 's' but goto is -1.\n");
#endif
                return -1; // control should never reach here
            } else {
                GrammaSymbol sym = GrammaSymbol(-1, -1, type, entry.row, entry.col);
                sym.row = entry.row;
                sym.col = entry.col;
                if(iTable != NULL) {
                    if(type == IDENTIFIER)
                        sym.attr.id->name = entry.index;
                    else if(type == CONSTANT) {
                        sym.attr.con->name = entry.index;
                        sym.attr.con->dataType = (*nameTable)[entry.index].value.numberValue.isFloat ? DT_FLOAT : DT_INT;
                    }
                }
                push(stat, sym);
                i++;
#ifdef DEBUG
                //fprintf(stderr, "[DEBUG] Shift symbol: %d\n", type);
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
                GrammaSymbol &firstSym = (*stack)[stack->size() - PRO_LENGTH[pro]].sym;
                GrammaSymbol sym = GrammaSymbol(-1, -1, PRO_LEFT[pro], firstSym.row, firstSym.col);
                if(iTable != NULL) { // semantic analysis mode
                    int SAerr = semanticActions[pro](sym); // -2 for compile error
                    if(SAerr == -1) return -1;             // -1 for internal error
                    if(SAerr == -2) returnCode = -2;
                }
                pop(PRO_LENGTH[pro]);
                int stat = GOTO[current()][sym.type];
                push(stat, sym);
#ifdef PRINT_PRODUCTIONS
                seq.push_back(pro);
#endif
            }
        } else if((unsigned long)i == tokenTable.size()) {
            break;
        } else {
            err = true;
            returnCode = -2;
        }
        if(err) {
            printf(GRAMMA_ERROR_MESSAGE[current()], entry.row, entry.col, entry.source);
            // error recovery
            int row = entry.row, col = entry.col;
            while(!stack->empty() && RECOVER_SYMBOL[current()].empty()) {
                row = stack->back().sym.row;
                col = stack->back().sym.col;
                pop();
            }
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
            GrammaSymbol sym = GrammaSymbol(-1, -1, symType, row, col);
            push(GOTO[current()][symType], sym);
        }
    }
    if(ACTION[current()][END_SYMBOL] != 'a') {
        printf("Line %d, Col 1: Uncompleted code.\n", tokenTable.back().row + 1);
    }
    // now the gramma / semantic analysis succeeded
    if(iTable != NULL) {
        iTable->labelTable.resize(instTable->labelTable.size());
        GrammaSymbol program = stack->back().sym;
        for(int i = program.code; i != -1; i = (*instTable)[i].next) {
            int index = iTable->size();
            iTable->push_back((*instTable)[i]);
            if((*instTable)[i].label >= 0)
                iTable->labelTable[(*instTable)[i].label] = index;
        }
    }
    delete stack;
    return returnCode;
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

int link(GrammaSymbol &a, int b) {
    if(a.code == -1) {
        a.code = a.end = b;
    } else {
        (*instTable)[a.end].next = b;
        if(b != -1)
            a.end = b;
    }
    return a.code;
}

int link(int a, GrammaSymbol &b) {
    if(b.code == -1) {
        b.code = b.end = a;
    } else {
        (*instTable)[a].next = b.code;
        if(a != -1)
            b.code = a;
    }
    return a;
}

int link(int a, int b) {
    if(a != -1) {
        (*instTable)[a].next = b;
        return a;
    } else {
        return b;
    }
}

/**
 * Returns: -1 for internal error, -2 for compile error.
 */
int sizeOf(TypeInfo *typ, int row, int col) {
    int size = -1;
    if(typ->dataType == DT_INT)
        size = INT_SIZE;
    else if(typ->dataType == DT_FLOAT)
        size = FLOAT_SIZE;
    else if(typ->dataType == DT_BOOL)
        size = BOOL_SIZE;
    else if(typ->dataType == DT_ARRAY) {
        if(typ->attr.arr->dataType == DT_INT)
            size = INT_SIZE;
        else if(typ->attr.arr->dataType == DT_FLOAT)
            size = FLOAT_SIZE;
        else if(typ->attr.arr->dataType == DT_BOOL)
            size = BOOL_SIZE;
        else {
#ifdef DEBUG
            fprintf(stderr, "[ERROR] Invalid array type.\n");
#endif
            return -1;
        }
        size *= (*nameTable)[typ->attr.arr->lens.front()].value.numberValue.value.intValue;
    } else if(typ->dataType == DT_STRUCT) {
        if(typ->attr.table->busy) {
            printf("Line %d, Col %d: Recursion reference to struct definition.\n", row, col);
            return -2;
        }
        size = typ->attr.table->offset;
    } else {
        size = 0;
    }
    return size;
}

int sizeOf(SymbolDataType dataType) {
    if(dataType == DT_INT)
        return INT_SIZE;
    else if(dataType == DT_FLOAT)
        return FLOAT_SIZE;
    else if(dataType == DT_BOOL)
        return BOOL_SIZE;
    else
        return -1;
}

void enterTable(SymbolTable *table) {
    symbolTable = table;
    symbolTable->busy = true;
}

int quitTable() {
    if(symbolTable->parent == NULL) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Can't quit the global symbol table.");
#endif
        return -1;
    }
    symbolTable->busy = false;
    symbolTable = symbolTable->parent;
    return 0;
}

pair<int, int> evalBoolExp(ExpInfo *exp, int next) {
    SymbolTableEntryRef &ref = exp->ref;
    int label = instTable->newLabel(next);
    int trueCode = instTable->gen(OP_TRU, NULL_REF, NULL_REF, ref);
    int jmpCode = instTable->gen(OP_JMP, NULL_REF, NULL_REF, {NULL, label});
    int falseCode = instTable->gen(OP_FAL, NULL_REF, NULL_REF, ref);
    link(trueCode, jmpCode);
    link(jmpCode, falseCode);
    int trueLabel = instTable->newLabel(trueCode);
    int falseLabel = instTable->newLabel(falseCode);
    instTable->backPatch(exp->trueList, trueLabel);
    instTable->backPatch(exp->falseList, falseLabel);
    return {trueCode, falseCode};
}

pair<int, int> genBoolJmpCode(ExpInfo *exp) {
    int preCode = -1;
    SymbolTableEntryRef tmpRef = exp->ref;
    if((*(exp->ref.table))[exp->ref.index].dataType != DT_BOOL) {
        if(exp->ndim > 0) { // array element
            tmpRef = symbolTable->newTemp(DT_BOOL, BOOL_SIZE);
            preCode = instTable->gen(OP_MOVS, tmpRef, exp->baseRef, exp->ref);
            symbolTable->freeTemp();
        } else if(exp->offset >= 0) { // struct member
            tmpRef = symbolTable->newTemp(DT_BOOL, BOOL_SIZE);
            preCode = instTable->gen(OP_MOVS, tmpRef, exp->ref, {NULL, exp->offset});
            symbolTable->freeTemp();
        } else {
#ifdef DEBUG
            fprintf(stderr, "[ERROR] bool type check failed.\n");
            return {-1, -1};
#endif
        }
    }
    int trueCode = instTable->gen(OP_JNZ, tmpRef, NULL_REF, NULL_REF);
    int falseCode = instTable->gen(OP_JMP, NULL_REF, NULL_REF, NULL_REF);
    link(preCode, trueCode);
    link(trueCode, falseCode);
    exp->trueList.push_back(trueCode);
    exp->falseList.push_back(falseCode);
    if(preCode == -1)
        return {trueCode, falseCode};
    else
        return {preCode, falseCode};
}

pair<int, SymbolDataType> genMovsCode(ExpInfo *exp) {
    int movCode = -1;
    SymbolTableEntryRef ref = exp->ref;
    SymbolDataType realType = typeOf(exp);
    if(exp->ndim > 0) { // array element
        SymbolTableEntryRef tmpRef = symbolTable->newTemp(realType, sizeOf(realType));
        movCode = instTable->gen(OP_MOVS, tmpRef, exp->baseRef, ref);
        symbolTable->freeTemp();
        exp->ref = tmpRef;
        exp->ndim = 0;
        exp->isTemp = true;
    } else if(exp->offset >= 0) { // struct member
        SymbolTableEntryRef tmpRef = symbolTable->newTemp(realType, sizeOf(realType));
        movCode = instTable->gen(OP_MOVS, tmpRef, ref, {NULL, exp->offset});
        symbolTable->freeTemp();
        exp->ref = tmpRef;
        exp->offset = -1;
        exp->isTemp = true;
    }
    return {movCode, realType};
}

bool typeMatch(ExpInfo *lexp, ExpInfo *rexp) {
    SymbolTableEntryRef &lref = lexp->ref;
    SymbolTableEntryRef &rref = rexp->ref;
    SymbolDataType ldataType = typeOf(lexp);
    SymbolDataType rdataType = typeOf(rexp);
    if(ldataType == DT_NONE || rdataType == DT_NONE
            || ldataType == DT_STRUCT_DEF || rdataType == DT_STRUCT_DEF
            || ldataType == DT_BLOCK || rdataType == DT_BLOCK
            || ldataType == DT_ARRAY || rdataType == DT_ARRAY) {
        return false;
    } else if(ldataType == DT_STRUCT) {
        if(rdataType == DT_STRUCT) {
            return (*lref.table)[lref.index].attr.table == (*rref.table)[rref.index].attr.table;
        } else
            return false;
    }
    // now the problem is reduced as the basic-types matching problem
    if(ldataType == DT_BOOL && rdataType != DT_BOOL)
        return false;
    return true;
}

bool typeMatch(SymbolDataType ldataType, ExpInfo *rexp) {
    ExpInfo exp;
    exp.ref = symbolTable->newTemp(ldataType, 0);
    exp.offset = -1;
    exp.ndim = 0;
    bool result = typeMatch(&exp, rexp);
    symbolTable->freeTemp();
    return result;
}

bool typeMatch(SymbolTableEntryRef &lref, ExpInfo *rexp) {
    ExpInfo exp;
    exp.ref = lref;
    exp.offset = -1;
    exp.ndim = 0;
    bool result = typeMatch(&exp, rexp);
    return result;
}

SymbolDataType typeOf(ExpInfo *exp) {
    SymbolTableEntry &entry = (*(exp->ref.table))[exp->ref.index];
    if(entry.dataType == DT_ARRAY) {
        return DT_ARRAY;
    } else if(exp->ndim > 0) {
        SymbolTableEntry &entry = (*(exp->baseRef.table))[exp->baseRef.index];
        if(exp->ndim != entry.attr.arr->ndim)
            return DT_NONE;
        else
            return entry.attr.arr->dataType;
    } else if(exp->offset < 0)
        return entry.dataType;
    SymbolTable *table = entry.attr.table;
    int offset = exp->offset;
    // recursive binary search
    while(true) {
        int l = 0, r = table->size() - 1;
        while(l < r) {
            int mid = (l + r) / 2;
            if((*table)[mid].offset == offset) {
                l = mid;
                break;
            }
            else if((*table)[mid].offset > offset)
                r = mid - 1;
            else if((*table)[mid + 1].offset > offset) { // nested struct / array
                l = r = mid;
                break;
            } else
                l = mid + 1;
        }
        if((*table)[l].offset == offset && (*table)[l].dataType != DT_STRUCT) {
            if((*table)[l].dataType == DT_ARRAY)
                return (*table)[l].attr.arr->dataType;
            else {
                return (*table)[l].dataType;
            }
        } else {
            offset -= (*table)[l].offset;
            table = (*table)[l].attr.table;
        }
    }
}

GrammaSymbol::GrammaSymbol(int code, int end, int type, int row, int col) : code(code),
                                                                            end(end),
                                                                            type(type),
                                                                            row(row),
                                                                            col(col) {
    if(type == EXPRESSION || (EXPRESSION1 <= type && type <= EXPRESSION8))
        this->attr.exp = new ExpInfo();
    else if(type == EXPRESSION_S)
        this->attr.exps = new ExpsInfo();
    else if(type == IDENTIFIER_S)
        this->attr.ids = new IdsInfo();
    else if(type == IDENTIFIER)
        this->attr.id = new IdInfo();
    else if(type == SELECT_BEGIN)
        this->attr.sel_b = new SelBeginInfo();
    else if(type == LOOP_BEGIN)
        this->attr.loop_b = new LoopBeginInfo();
    else if(type == DECLARE_FUNC_BEGIN)
        this->attr.func_b = new FuncBeginInfo();
    else if(type == CONSTANT)
        this->attr.con = new ConstInfo();
    else if(type == TYPE || type == TYPE_BASIC || type == TYPE_ARRAY)
        this->attr.typ = new TypeInfo();
    else if(type == TYPE_STRUCT)
        this->attr.typ_str = new TypeStructInfo();
}

AnalyserStackItem::AnalyserStackItem(int stat, GrammaSymbol sym) : stat(stat), sym(sym) {}

SymbolTable::SymbolTable(SymbolTable *parent, bool isFunc) : number(SymbolTable::n++),
                                                             tempCount(0),
                                                             offset(0),
                                                             busy(false),
                                                             parent(parent) {
    tables.push_back(this);
    if(isFunc)
        this->funcTable = this;
    else if(parent != NULL)
        this->funcTable = parent->funcTable;
    else
        this->funcTable = NULL;
}

SymbolTableEntryRef SymbolTable::newSymbol(int name, int type, SymbolDataType dataType, int size) {
    if(this->tempCount > 0 && name > 0) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Failed to insert symbol because of temp symbols.\n");
#endif
        return NULL_REF;
    }
    SymbolTableEntry entry;
    entry.name = name;
    entry.type = type;
    entry.dataType = dataType;
    entry.offset = this->offset;
    entry.size = size;
    this->offset += size;
    int index = this->size();
    this->nameMap[name] = index;
    this->push_back(entry);
    return (SymbolTableEntryRef){this, index};
}

SymbolTableEntryRef SymbolTable::newTemp(SymbolDataType dataType, int size) {
    this->tempCount++;
    return this->newSymbol(0, IDENTIFIER, dataType, size);
}

void SymbolTable::freeTemp() {
    if(this->tempCount <= 0) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Can't free temp symbol.\n"); // control should never reach here
#endif
        return;
    }
    this->tempCount--;
    this->offset -= this->back().size;
    this->pop_back();
}

SymbolTableEntryRef SymbolTable::findSymbol(int name) {
    SymbolTableEntryRef result;
    if(this->nameMap.find(name) != this->nameMap.end()) {
        result.table = this;
        result.index = nameMap[name];
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

bool SymbolTable::existsSymbol(int name) {
    return this->nameMap.find(name) != this->nameMap.end();
}

int InstTable::gen(OpCode op, const SymbolTableEntryRef &arg1, const SymbolTableEntryRef &arg2, const SymbolTableEntryRef &result) {
    Inst inst;
    inst.index = this->size();
    inst.label = -1;
    inst.next = -1;
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

int InstTable::newLabel(int index) {
    if(index >= 0 && (*this)[index].label >= 0)
        return (*this)[index].label;
    int result = this->labelTable.size();
    this->labelTable.push_back(index);
    if(index >= 0) // label is pre-allocated (when declaring functions) if index < 0
        (*this)[index].label = result;
    return result;
}

/**
 * Fill the pre-allocated label.
 */
void InstTable::fillLabel(int code, int label) {
    this->labelTable[label] = code;
    (*this)[code].label = label;
}

/*****************************
 * Semantic Action Functions *
 *****************************/

// S -> PROGRAM
int SA_0(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] S -> PROGRAM\n");
#endif
    int n = stack->size();
    GrammaSymbol program = (*stack)[n - 1].sym;
    sym.code = program.code;
    sym.end = program.end;
    return 0;
}

// PROGRAM -> DECLARE_S
int SA_1(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] PROGRAM -> DECLARE_S\n");
#endif
    int n = stack->size();
    GrammaSymbol declare_s = (*stack)[n - 1].sym;
    sym.code = declare_s.code;
    sym.end = declare_s.end;
    if(!declare_s.nextList.empty()) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] nextList is not empty.\n");
#endif
        return -1;
    }
    return 0;
}

// STATEMENT_S -> STATEMENT_S STATEMENT
int SA_2(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] STATEMENT_S -> STATEMENT_S STATEMENT\n");
#endif
    int n = stack->size();
    GrammaSymbol statement_s = (*stack)[n - 2].sym;
    GrammaSymbol statement = (*stack)[n - 1].sym;
    sym.code = statement_s.code;
    sym.end = statement_s.end;
    if(statement.code != -1) {
        if(!statement_s.nextList.empty()) {
            int label = instTable->newLabel(statement.code);
            instTable->backPatch(statement_s.nextList, label);
        }
        sym.nextList.splice(sym.nextList.end(), statement.nextList);
        link(sym, statement);
    } else {
        sym.nextList.splice(sym.nextList.end(), statement_s.nextList);
    }
    return 0;
}

// STATEMENT_S -> STATEMENT
int SA_3(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] STATEMENT_S -> STATEMENT\n");
#endif
    int n = stack->size();
    GrammaSymbol statement = (*stack)[n - 1].sym;
    sym.code = statement.code;
    sym.end = statement.end;
    sym.nextList.splice(sym.nextList.end(), statement.nextList);
    return 0;
}

// STATEMENT -> DECLARE_VAR
int SA_4(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] STATEMENT -> DECLARE_VAR\n");
#endif
    sym.code = sym.end = -1;
    return 0;
}

// STATEMENT -> DECLARE_STRUCT
int SA_5(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] STATEMENT -> DECLARE_STRUCT\n");
#endif
    sym.code = sym.end = -1;
    return 0;
}

// STATEMENT -> SELECT
int SA_6(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] STATEMENT -> SELECT\n");
#endif
    int n = stack->size();
    GrammaSymbol select = (*stack)[n - 1].sym;
    sym.code = select.code;
    sym.end = select.end;
    sym.nextList.splice(sym.nextList.end(), select.nextList);
    return 0;
}

// STATEMENT -> LOOP
int SA_7(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] STATEMENT -> LOOP\n");
#endif
    int n = stack->size();
    GrammaSymbol loop = (*stack)[n - 1].sym;
    sym.code = loop.code;
    sym.end = loop.end;
    sym.nextList.splice(sym.nextList.end(), loop.nextList);
    return 0;
}

// STATEMENT -> return EXPRESSION ;
int SA_8(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] STATEMENT -> return EXPRESSION ;\n");
#endif
    int n = stack->size();
    GrammaSymbol expression = (*stack)[n - 2].sym;
    sym.code = expression.code;
    sym.end = expression.end;
    if(expression.attr.exp->isTemp)
        symbolTable->freeTemp();
    SymbolTableEntryRef ref = {symbolTable->funcTable, 0};
    if(!typeMatch(ref, expression.attr.exp)) {
        printf("Line %d, Col %d: Return value doesn't match the return type.\n", expression.row, expression.col);
        return -2;
    }
    int retCode = instTable->gen(OP_RET, NULL_REF, NULL_REF, NULL_REF);
    if((*ref.table)[0].dataType == DT_BOOL && expression.attr.exp->isTemp) {
        pair<int, int> boolCode = evalBoolExp(expression.attr.exp, retCode);
        link(sym, boolCode.first);
        sym.end = boolCode.second;
        if(!expression.nextList.empty()) {
            int boolLabel = instTable->newLabel(boolCode.first);
            instTable->backPatch(expression.nextList, boolLabel);
        }
    }
    int movCode = -1;
    if(expression.attr.exp->offset >= 0)
        movCode = instTable->gen(OP_MOVS, ref, expression.attr.exp->ref, {NULL, expression.attr.exp->offset});
    else
        movCode = instTable->gen(OP_MOV, ref, expression.attr.exp->ref, NULL_REF);
    if(!expression.nextList.empty()) {
        int movLabel = instTable->newLabel(movCode);
        instTable->backPatch(expression.nextList, movLabel);
    }
    link(sym, movCode);
    link(sym, retCode);
    return 0;
}

// STATEMENT -> EXPRESSION ;
int SA_9(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] STATEMENT -> EXPRESSION ;\n");
#endif
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
int SA_10(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] STATEMENT -> STATEMENTS_BEGIN STATEMENT_S }\n");
#endif
    int n = stack->size();
    GrammaSymbol statements_begin = (*stack)[n - 3].sym;
    GrammaSymbol statement_s = (*stack)[n - 2].sym;
    sym.code = statement_s.code;
    sym.end = statement_s.end;
    sym.nextList.splice(sym.nextList.end(), statement_s.nextList);
    return quitTable();
}

// STATEMENT -> { }
int SA_11(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] STATEMENT -> { }\n");
#endif
    sym.code = sym.end = -1;
    return 0;
}

// STATEMENTS_BEGIN -> {
int SA_12(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] STATEMENTS_BEGIN -> {\n");
#endif
    sym.code = sym.end = -1;
    SymbolTableEntryRef ref = symbolTable->newSymbol(0, IDENTIFIER, DT_BLOCK, 0);
    SymbolTable *table = new SymbolTable(symbolTable, false);
    (*ref.table)[ref.index].attr.table = table;
    enterTable(table);
    return 0;
}

// DECLARE_S -> DECLARE_S DECLARE
int SA_13(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE_S -> DECLARE_S DECLARE\n");
#endif
    int n = stack->size();
    GrammaSymbol declare_s = (*stack)[n - 2].sym;
    GrammaSymbol declare = (*stack)[n - 1].sym;
    sym.code = declare_s.code;
    sym.end = declare_s.end;
    link(sym, declare);
    return 0;
}

// DECLARE_S -> DECLARE
int SA_14(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE_S -> DECLARE\n");
#endif
    int n = stack->size();
    GrammaSymbol declare = (*stack)[n - 1].sym;
    sym.code = declare.code;
    sym.end = declare.end;
    return 0;
}

// DECLARE -> DECLARE_VAR
int SA_15(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE -> DECLARE_VAR\n");
#endif
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE -> DECLARE_STRUCT
int SA_16(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE -> DECLARE_STRUCT\n");
#endif
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE -> DECLARE_FUNC
int SA_17(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE -> DECLARE_FUNC\n");
#endif
    int n = stack->size();
    GrammaSymbol declare_func = (*stack)[n - 1].sym;
    sym.code = declare_func.code;
    sym.end = declare_func.end;
    return 0;
}

// DECLARE_VAR -> TYPE IDENTIFIER_S ;
int SA_18(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE_VAR -> TYPE IDENTIFIER_S ;\n");
#endif
    int n = stack->size();
    GrammaSymbol type = (*stack)[n - 3].sym;
    GrammaSymbol identifier_s = (*stack)[n - 2].sym;
    sym.code = sym.end = -1;
    int size = sizeOf(type.attr.typ, type.row, type.col);
    if(size == 0) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Invalid type in variant definition.\n");
#endif
        return -1;
    } else if(size < 0) {
        return size;
    }
    int err = 0;
    list<int> &l = identifier_s.attr.ids->nameList;
    list<int> &rl = identifier_s.attr.ids->rowList;
    list<int> &cl = identifier_s.attr.ids->colList;
    for(list<int>::iterator it = l.begin(), ri = rl.begin(), ci = cl.begin(); it != l.end(); it++, ri++, ci++) {
        if(symbolTable->existsSymbol(*it)) {
            printf("Line %d, Col %d: Identifier has been declared before: %s\n", *ri, *ci, (*nameTable)[*it].value.stringValue);
            err = -2;
            continue;
        }
        SymbolTableEntryRef ref = symbolTable->newSymbol(*it, IDENTIFIER, type.attr.typ->dataType, size);
        if(type.attr.typ->dataType == DT_ARRAY)
            (*ref.table)[ref.index].attr.arr = type.attr.typ->attr.arr;
        else if(type.attr.typ->dataType == DT_STRUCT)
            (*ref.table)[ref.index].attr.table = type.attr.typ->attr.table;
    }
    return err;
}

// DECLARE_VAR_S -> DECLARE_VAR_S DECLARE_VAR
int SA_19(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE_VAR_S -> DECLARE_VAR_S DECLARE_VAR\n");
#endif
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE_VAR_S -> DECLARE_VAR
int SA_20(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE_VAR_S -> DECLARE_VAR\n");
#endif
    sym.code = sym.end = -1;
    return 0;
}

// DECLARE_STRUCT -> DECLARE_STRUCT_BEGIN DECLARE_VAR_S } ;
int SA_21(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE_STRUCT -> DECLARE_STRUCT_BEGIN DECLARE_VAR_S } ;\n");
#endif
    sym.code = sym.end = -1;
    int size = symbolTable->offset;
    quitTable();
    symbolTable->back().offset = size; // the last symbol of the parent table must be the struct definition at this time
    return 0;
}

// DECLARE_STRUCT_BEGIN -> TYPE_STRUCT {
int SA_22(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE_STRUCT_BEGIN -> TYPE_STRUCT {\n");
#endif
    int n = stack->size();
    GrammaSymbol type_struct = (*stack)[n - 2].sym;
    sym.code = sym.end = -1;
    int name = type_struct.attr.typ_str->name;
    if(symbolTable->existsSymbol(name)) {
        printf("Line %d, Col %d: Identifier has been declared before: %s\n", type_struct.row, type_struct.col, (*nameTable)[name].value.stringValue);
        return -2;
    }
    SymbolTableEntryRef ref = symbolTable->newSymbol(name, IDENTIFIER, DT_STRUCT_DEF, 0);
    SymbolTable *table = new SymbolTable(symbolTable, false);
    (*ref.table)[ref.index].attr.table = table;
    enterTable(table);
    return 0;
}

// DECLARE_FUNC -> DECLARE_FUNC_SIGN STATEMENT_S }
int SA_23(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE_FUNC -> DECLARE_FUNC_SIGN STATEMENT_S }\n");
#endif
    int n = stack->size();
    GrammaSymbol statement_s = (*stack)[n - 2].sym;
    sym.code = statement_s.code;
    sym.end = statement_s.end;
    int code = instTable->gen(OP_RET, NULL_REF, NULL_REF, NULL_REF);
    link(sym, code);
    if(!statement_s.nextList.empty()) {
        int label = instTable->newLabel(code);
        instTable->backPatch(statement_s.nextList, label);
    }
    quitTable();
    instTable->fillLabel(sym.code, symbolTable->back().offset); // the back element of the symbol table must be the symbol of this function at this time
    return 0;
}

// DECLARE_FUNC -> DECLARE_FUNC_SIGN }
int SA_24(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE_FUNC -> DECLARE_FUNC_SIGN }\n");
#endif
    int code = instTable->gen(OP_RET, NULL_REF, NULL_REF, NULL_REF);
    sym.code = sym.end = code;
    quitTable();
    instTable->fillLabel(sym.code, symbolTable->back().offset);
    return 0;
}

// DECLARE_FUNC_SIGN -> DECLARE_FUNC_BEGIN PARAMETERS ) {
int SA_25(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE_FUNC_SIGN -> DECLARE_FUNC_BEGIN PARAMETERS ) {\n");
#endif
    int n = stack->size();
    GrammaSymbol parameters = (*stack)[n - 3].sym;
    GrammaSymbol declare_func_begin = (*stack)[n - 4].sym;
    sym.code = sym.end = -1;
    SymbolTableEntryRef ref = declare_func_begin.attr.func_b->ref;
    (*ref.table)[ref.index].attr.func->pCount = parameters.attr.pCount;
    return 0;
}

// DECLARE_FUNC_SIGN -> DECLARE_FUNC_BEGIN ) {
int SA_26(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE_FUNC_SIGN -> DECLARE_FUNC_BEGIN ) {\n");
#endif
    int n = stack->size();
    GrammaSymbol declare_func_begin = (*stack)[n - 3].sym;
    sym.code = sym.end = -1;
    SymbolTableEntryRef ref = declare_func_begin.attr.func_b->ref;
    (*ref.table)[ref.index].attr.func->pCount = 0;
    return 0;
}

// PARAMETERS -> PARAMETERS , TYPE identifier
int SA_27(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] PARAMETERS -> PARAMETERS , TYPE identifier\n");
#endif
    int n = stack->size();
    GrammaSymbol parameters = (*stack)[n - 4].sym;
    GrammaSymbol type = (*stack)[n - 2].sym;
    GrammaSymbol identifier = (*stack)[n - 1].sym;
    sym.code = sym.end = -1;
    int size = sizeOf(type.attr.typ, type.row, type.col);
    if(size == 0) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Invalid type in parameter definition.\n");
#endif
        return -1;
    } else if(size < 0) {
        return size;
    }
    SymbolTableEntryRef ref = symbolTable->newSymbol(identifier.attr.id->name, IDENTIFIER, type.attr.typ->dataType, size);
    if(type.attr.typ->dataType == DT_ARRAY)
        (*ref.table)[ref.index].attr.arr = type.attr.typ->attr.arr;
    else if(type.attr.typ->dataType == DT_STRUCT)
        (*ref.table)[ref.index].attr.table = type.attr.typ->attr.table;
    sym.attr.pCount = parameters.attr.pCount + 1;
    return 0;
}

// PARAMETERS -> TYPE identifier
int SA_28(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] PARAMETERS -> TYPE identifier\n");
#endif
    int n = stack->size();
    GrammaSymbol type = (*stack)[n - 2].sym;
    GrammaSymbol identifier = (*stack)[n - 1].sym;
    sym.code = sym.end = -1;
    sym.attr.pCount = 0;
    int size = sizeOf(type.attr.typ, type.row, type.col);
    if(size == 0) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Invalid type in parameter definition.\n");
#endif
        return -1;
    } else if(size < 0) {
        return size;
    }
    SymbolTableEntryRef ref = symbolTable->newSymbol(identifier.attr.id->name, IDENTIFIER, type.attr.typ->dataType, size);
    if(type.attr.typ->dataType == DT_ARRAY)
        (*ref.table)[ref.index].attr.arr = type.attr.typ->attr.arr;
    else if(type.attr.typ->dataType == DT_STRUCT)
        (*ref.table)[ref.index].attr.table = type.attr.typ->attr.table;
    sym.attr.pCount = 1;
    return 0;
}

// DECLARE_FUNC_BEGIN -> TYPE identifier (
int SA_29(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] DECLARE_FUNC_BEGIN -> TYPE identifier (\n");
#endif
    int n = stack->size();
    GrammaSymbol type = (*stack)[n - 3].sym;
    GrammaSymbol identifier = (*stack)[n - 2].sym;
    sym.code = sym.end = -1;
    if(symbolTable->existsSymbol(identifier.attr.id->name)) {
        printf("Line %d, Col %d: Identifier has been declared before: %s\n", identifier.row, identifier.col, (*nameTable)[identifier.attr.id->name].value.stringValue);
        return -2;
    }
    SymbolTableEntryRef ref = symbolTable->newSymbol(identifier.attr.id->name, IDENTIFIER, DT_BLOCK, 0);
    sym.attr.func_b->ref = ref;
    SymbolTable *table = new SymbolTable(symbolTable, true);
    (*ref.table)[ref.index].attr.func = new FuncInfo();
    (*ref.table)[ref.index].attr.func->table = table;
    (*ref.table)[ref.index].offset = instTable->newLabel(-1); // pre-allocate a label for recursive calls
    enterTable(table);
    int size = sizeOf(type.attr.typ, type.row, type.col);
    if(size == 0) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Invalid return type in function definition.\n");
#endif
        return -1;
    } else if(size < 0) {
        return size;
    }
    ref = table->newSymbol(0, IDENTIFIER, type.attr.typ->dataType, size); // return value symbol
    if(type.attr.typ->dataType == DT_ARRAY)
        (*ref.table)[ref.index].attr.arr = type.attr.typ->attr.arr;
    else if(type.attr.typ->dataType == DT_STRUCT)
        (*ref.table)[ref.index].attr.table = type.attr.typ->attr.table;
    return 0;
}

// TYPE -> TYPE_BASIC
int SA_30(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] TYPE -> TYPE_BASIC\n");
#endif
    int n = stack->size();
    GrammaSymbol type_basic = (*stack)[n - 1].sym;
    sym.code = sym.end = -1;
    sym.attr.typ->dataType = type_basic.attr.typ->dataType;
    return 0;
}

// TYPE -> TYPE_ARRAY
int SA_31(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] TYPE -> TYPE_ARRAY\n");
#endif
    int n = stack->size();
    GrammaSymbol type_array = (*stack)[n - 1].sym;
    sym.code = sym.end = -1;
    sym.attr.typ->dataType = DT_ARRAY;
    ArrayInfo *info = sym.attr.typ->attr.arr = type_array.attr.typ->attr.arr;
    int len = 1;
    for(vector<int>::reverse_iterator it = info->lens.rbegin(); it != info->lens.rend(); it++) {
        len *= (*nameTable)[*it].value.numberValue.value.intValue;
        int name = nameTable->size();
        LexicalSymbolValue value;
        value.numberValue.isFloat = false;
        value.numberValue.value.intValue = len;
        nameTable->push_back({false, value}); // WARNING: bad trick
        *it = name;
        symbolTable->newSymbol(name, CONSTANT, DT_INT, INT_SIZE);
    }
    return 0;
}

// TYPE -> TYPE_STRUCT
int SA_32(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] TYPE -> TYPE_STRUCT\n");
#endif
    int n = stack->size();
    sym.code = sym.end = -1;
    GrammaSymbol type_struct = (*stack)[n - 1].sym;
    sym.attr.typ->dataType = DT_STRUCT;
    int name = type_struct.attr.typ_str->name;
    SymbolTableEntryRef ref = symbolTable->findSymbol(name);
    if(ref.table == NULL) {
        printf("Line %d, Col %d: Undefined struct: %s.\n", type_struct.row, type_struct.col, (*nameTable)[name].value.stringValue);
        sym.attr.typ->attr.table = NULL;
        return -2;
    }
    sym.attr.typ->attr.table = (*ref.table)[ref.index].attr.table;
    return 0;
}

// TYPE_BASIC -> int
int SA_33(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] TYPE_BASIC -> int\n");
#endif
    sym.attr.typ->dataType = DT_INT;
    sym.code = sym.end = -1;
    return 0;
}

// TYPE_BASIC -> float
int SA_34(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] TYPE_BASIC -> float\n");
#endif
    sym.attr.typ->dataType = DT_FLOAT;
    sym.code = sym.end = -1;
    return 0;
}

// TYPE_BASIC -> bool
int SA_35(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] TYPE_BASIC -> bool\n");
#endif
    sym.attr.typ->dataType = DT_BOOL;
    sym.code = sym.end = -1;
    return 0;
}

// TYPE_ARRAY -> TYPE_ARRAY [ constant ]
int SA_36(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] TYPE_ARRAY -> TYPE_ARRAY [ constant ]\n");
#endif
    int n = stack->size();
    GrammaSymbol type_array = (*stack)[n - 4].sym;
    GrammaSymbol constant = (*stack)[n - 2].sym;
    sym.code = sym.end = -1;
    if(constant.attr.con->dataType == DT_FLOAT) {
        printf("Line %d, Col %d: The size of array should be an integer.\n", constant.row, constant.col);
        return -2;
    } else if(constant.attr.con->dataType != DT_INT) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Constant type is not DT_INT or DT_FLOAT.\n");
#endif
        return -1;
    } else if((*nameTable)[constant.attr.con->name].value.numberValue.value.intValue <= 0) {
        printf("Line %d, Col %d: The size of array should be a positive integer.\n", constant.row, constant.col);
        return -2;
    }
    sym.attr.typ->dataType = DT_ARRAY;
    sym.attr.typ->attr.arr = new ArrayInfo();
    sym.attr.typ->attr.arr->dataType = type_array.attr.typ->attr.arr->dataType;
    sym.attr.typ->attr.arr->ndim = type_array.attr.typ->attr.arr->ndim + 1;
    sym.attr.typ->attr.arr->lens = type_array.attr.typ->attr.arr->lens;
    sym.attr.typ->attr.arr->lens.push_back(constant.attr.con->name);
    return 0;
}

// TYPE_ARRAY -> TYPE_BASIC [ constant ]
int SA_37(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] TYPE_ARRAY -> TYPE_BASIC [ constant ]\n");
#endif
    int n = stack->size();
    GrammaSymbol type_basic = (*stack)[n - 4].sym;
    GrammaSymbol constant = (*stack)[n - 2].sym;
    sym.code = sym.end = -1;
    sym.attr.typ->dataType = DT_ARRAY;
    sym.attr.typ->attr.arr = new ArrayInfo();
    sym.attr.typ->attr.arr->dataType = type_basic.attr.typ->dataType;
    sym.attr.typ->attr.arr->ndim = 0;
    if(constant.attr.con->dataType == DT_FLOAT) {
        printf("Line %d, Col %d: The size of array should be an integer.\n", constant.row, constant.col);
        return -2;
    } else if(constant.attr.con->dataType != DT_INT) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] %d\n", constant.attr.con->dataType);
        fprintf(stderr, "[ERROR] Constant type is not DT_INT or DT_FLOAT.\n");
#endif
        return -1;
    } else if((*nameTable)[constant.attr.con->name].value.numberValue.value.intValue <= 0) {
        printf("Line %d, Col %d: The size of array should be a positive integer.\n", constant.row, constant.col);
        return -2;
    }
    sym.attr.typ->attr.arr->ndim = 1;
    sym.attr.typ->attr.arr->lens.push_back(constant.attr.con->name);
    return 0;
}

// TYPE_STRUCT -> struct identifier
int SA_38(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] TYPE_STRUCT -> struct identifier\n");
#endif
    int n = stack->size();
    GrammaSymbol identifier = (*stack)[n - 1].sym;
    sym.code = sym.end = -1;
    sym.attr.typ_str->name = identifier.attr.id->name;
    sym.attr.typ_str->row = identifier.row;
    sym.attr.typ_str->col = identifier.col;
    return 0;
}

// SELECT -> SELECT_BEGIN STATEMENT
int SA_39(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] SELECT -> SELECT_BEGIN STATEMENT\n");
#endif
    int n = stack->size();
    GrammaSymbol select_begin = (*stack)[n - 2].sym;
    GrammaSymbol statement = (*stack)[n - 1].sym;
    int trueLabel = instTable->newLabel(statement.code);
    instTable->backPatch(select_begin.attr.sel_b->trueList, trueLabel);
    sym.nextList.splice(sym.nextList.end(), select_begin.attr.sel_b->falseList);
    sym.nextList.splice(sym.nextList.end(), statement.nextList);
    sym.code = select_begin.code;
    sym.end = select_begin.end;
    link(sym, statement);
    quitTable();
    if(!select_begin.nextList.empty()) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] nextList of SELECT_BEGIN is not empty.\n");
#endif
        return -1;
    }
    return 0;
}

// SELECT -> SELECT_BEGIN STATEMENT SELECT_MID STATEMENT
int SA_40(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] SELECT -> SELECT_BEGIN STATEMENT SELECT_MID STATEMENT\n");
#endif
    int n = stack->size();
    GrammaSymbol select_begin = (*stack)[n - 4].sym;
    GrammaSymbol statement1 = (*stack)[n - 3].sym;
    GrammaSymbol select_mid = (*stack)[n - 2].sym;
    GrammaSymbol statement2 = (*stack)[n - 1].sym;
    int trueLabel = instTable->newLabel(statement1.code);
    int falseLabel = instTable->newLabel(statement2.code);
    instTable->backPatch(select_begin.attr.sel_b->trueList, trueLabel);
    instTable->backPatch(select_begin.attr.sel_b->falseList, falseLabel);
    sym.nextList.splice(sym.nextList.end(), statement1.nextList);
    sym.nextList.splice(sym.nextList.end(), select_mid.nextList);
    sym.nextList.splice(sym.nextList.end(), statement2.nextList);
    sym.code = select_begin.code;
    sym.end = select_begin.end;
    link(sym, statement1);
    link(sym, select_mid);
    link(sym, statement2);
    quitTable();
    if(!select_begin.nextList.empty() || !select_mid.nextList.empty()) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] nextList of SELECT_BEGIN or SELECT_MID is not empty.\n");
#endif
        return -1;
    }
    return 0;
}

// SELECT_BEGIN -> if ( EXPRESSION )
int SA_41(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] SELECT_BEGIN -> if ( EXPRESSION )\n");
#endif
    int n = stack->size();
    GrammaSymbol expression = (*stack)[n - 2].sym;
    sym.code = sym.end = -1;
    SymbolTableEntryRef expRef = expression.attr.exp->ref;
    if(!typeMatch(DT_BOOL, expression.attr.exp)) {
        printf("Line %d, Col %d: The type of if-condition should be a boolean value.\n", expression.row, expression.col);
        return -2;
    }
    sym.code = expression.code;
    sym.end = expression.end;
    if(expression.attr.exp->isTemp && expression.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else {
        if(expression.attr.exp->ndim > 0) // array element
            symbolTable->freeTemp();
        pair<int, int> code = genBoolJmpCode(expression.attr.exp);
        link(sym, code.first);
        sym.end = code.second;
    }
    SymbolTableEntryRef ref = symbolTable->newSymbol(0, IDENTIFIER, DT_BLOCK, 0);
    SymbolTable *table = new SymbolTable(symbolTable, false);
    (*ref.table)[ref.index].attr.table = table;
    enterTable(table);
    sym.attr.sel_b->trueList.splice(sym.attr.sel_b->trueList.end(), expression.attr.exp->trueList);
    sym.attr.sel_b->falseList.splice(sym.attr.sel_b->falseList.end(), expression.attr.exp->falseList);
    return 0;
}

// SELECT_MID -> else
int SA_42(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] SELECT_MID -> else\n");
#endif
    int code = instTable->gen(OP_JMP, NULL_REF, NULL_REF, NULL_REF);
    sym.code = sym.end = code;
    sym.nextList.push_back(code);
    quitTable();
    SymbolTableEntryRef ref = symbolTable->newSymbol(0, IDENTIFIER, DT_BLOCK, 0);
    SymbolTable *table = new SymbolTable(symbolTable, false);
    (*ref.table)[ref.index].attr.table = table;
    enterTable(table);
    return 0;
}

// LOOP -> LOOP_BEGIN STATEMENT
int SA_43(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] LOOP -> LOOP_BEGIN STATEMENT\n");
#endif
    int n = stack->size();
    GrammaSymbol loop_begin = (*stack)[n - 2].sym;
    GrammaSymbol statement = (*stack)[n - 1].sym;
    sym.code = loop_begin.code;
    sym.end = loop_begin.end;
    link(sym, statement);
    int loopLabel = instTable->newLabel(loop_begin.code);
    int bodyLabel = instTable->newLabel(statement.code);
    int code = instTable->gen(OP_JMP, NULL_REF, NULL_REF, {NULL, loopLabel});
    link(sym, code);
    instTable->backPatch(loop_begin.attr.loop_b->trueList, bodyLabel);
    if(!statement.nextList.empty())
        instTable->backPatch(statement.nextList, loopLabel);
    sym.nextList.splice(sym.nextList.end(), loop_begin.attr.loop_b->falseList);
    quitTable();
    if(!loop_begin.nextList.empty()) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] nextList of LOOP_BEGIN is not empty.\n");
#endif
        return -1;
    }
    return 0;
}

// LOOP_BEGIN -> while ( EXPRESSION )
int SA_44(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] LOOP_BEGIN -> while ( EXPRESSION )\n");
#endif
    int n = stack->size();
    GrammaSymbol expression = (*stack)[n - 2].sym;
    sym.code = sym.end = -1;
    SymbolTableEntryRef expRef = expression.attr.exp->ref;
    if(!typeMatch(DT_BOOL, expression.attr.exp)) {
        printf("Line %d, Col %d: The type of while-condition should be a boolean value.\n", expression.row, expression.col);
        return -2;
    }
    sym.code = expression.code;
    sym.end = expression.end;
    if(expression.attr.exp->isTemp && expression.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else {
        if(expression.attr.exp->ndim > 0) // array element
            symbolTable->freeTemp();
        pair<int, int> code = genBoolJmpCode(expression.attr.exp);
        link(sym, code.first);
        sym.end = code.second;
    }
    SymbolTableEntryRef ref = symbolTable->newSymbol(0, IDENTIFIER, DT_BLOCK, 0);
    SymbolTable *table = new SymbolTable(symbolTable, false);
    (*ref.table)[ref.index].attr.table = table;
    enterTable(table);
    sym.attr.loop_b->falseList.splice(sym.attr.loop_b->falseList.end(), expression.attr.exp->falseList);
    sym.attr.loop_b->trueList.splice(sym.attr.loop_b->trueList.end(), expression.attr.exp->trueList);
    return 0;
}

// EXPRESSION_S -> EXPRESSION_S , EXPRESSION
int SA_45(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION_S -> EXPRESSION_S , EXPRESSION\n");
#endif
    int n = stack->size();
    GrammaSymbol expression_s = (*stack)[n - 3].sym;
    GrammaSymbol expression = (*stack)[n - 1].sym;
    sym.code = expression_s.code;
    sym.end = expression_s.end;
    link(sym, expression);
    SymbolTableEntryRef expRef = expression.attr.exp->ref;
    if((*expRef.table)[expRef.index].dataType == DT_ARRAY) { // array element
        if(expression.attr.exp->ndim < (*expRef.table)[expRef.index].attr.arr->ndim) {
            printf("Line %d, Col %d: Too few dimensions for array.\n", expression.row, expression.col);
            return -2;
        } else if(expression.attr.exp->ndim > (*expRef.table)[expRef.index].attr.arr->ndim) {
            printf("Line %d, Col %d: Too many dimensions for array.\n", expression.row, expression.col);
            return -2;
        }
        SymbolDataType dataType = typeOf(expression.attr.exp);
        if(expression.attr.exp->isTemp)
            symbolTable->freeTemp();
        SymbolTableEntryRef tmpRef = symbolTable->newTemp(dataType, sizeOf(dataType));
        int movCode = instTable->gen(OP_MOVS, tmpRef, expression.attr.exp->baseRef, expRef);
        link(sym, movCode);
        expression.attr.exp->ref = expRef = tmpRef;
        expression.attr.exp->isTemp = true;
        expression.attr.exp->ndim = 0;
        expression.attr.exp->offset = -1;
    } else if(expression.attr.exp->offset >= 0) { // struct element
        SymbolDataType dataType = typeOf(expression.attr.exp);
        if(expression.attr.exp->isTemp)
            symbolTable->freeTemp();
        SymbolTableEntryRef tmpRef = symbolTable->newTemp(dataType, sizeOf(dataType));
        int movCode = instTable->gen(OP_MOVS, tmpRef, expRef, {NULL, expression.attr.exp->offset});
        link(sym, movCode);
        expression.attr.exp->ref = expRef = tmpRef;
        expression.attr.exp->isTemp = true;
        expression.attr.exp->ndim = 0;
        expression.attr.exp->offset = -1;
    } else if((*expRef.table)[expRef.index].dataType == DT_BOOL && expression.attr.exp->isTemp) {
        int trueCode = instTable->gen(OP_TRU, NULL_REF, NULL_REF, expression.attr.exp->ref);
        int jmpCode = instTable->gen(OP_JMP, NULL_REF, NULL_REF, NULL_REF);
        int falseCode = instTable->gen(OP_FAL, NULL_REF, NULL_REF, expression.attr.exp->ref);
        link(sym, trueCode);
        link(sym, jmpCode);
        link(sym, falseCode);
        sym.nextList.push_back(jmpCode);
        int trueLabel = instTable->newLabel(trueCode);
        int falseLabel = instTable->newLabel(falseCode);
        instTable->backPatch(expression.attr.exp->trueList, trueLabel);
        instTable->backPatch(expression.attr.exp->falseList, falseLabel);
    }
    if(!expression_s.nextList.empty()) {
        int label = instTable->newLabel(expression.code);
        instTable->backPatch(expression_s.nextList, label);
    }
    sym.nextList.splice(sym.nextList.end(), expression.nextList);
    sym.attr.exps->expList.splice(sym.attr.exps->expList.end(), expression_s.attr.exps->expList);
    sym.attr.exps->expList.push_back(expression.attr.exp);
    sym.attr.exps->rowList = expression_s.attr.exps->rowList;
    sym.attr.exps->colList = expression_s.attr.exps->colList;
    sym.attr.exps->rowList.push_back(expression.row);
    sym.attr.exps->colList.push_back(expression.col);
    return 0;
}

// EXPRESSION_S -> EXPRESSION
int SA_46(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION_S -> EXPRESSION\n");
#endif
    int n = stack->size();
    GrammaSymbol expression = (*stack)[n - 1].sym;
    sym.code = expression.code;
    sym.end = expression.end;
    SymbolTableEntryRef expRef = expression.attr.exp->ref;
    if((*expRef.table)[expRef.index].dataType == DT_ARRAY) { // array element
        if(expression.attr.exp->ndim < (*expRef.table)[expRef.index].attr.arr->ndim) {
            printf("Line %d, Col %d: Too few dimensions for array.\n", expression.row, expression.col);
            return -2;
        } else if(expression.attr.exp->ndim > (*expRef.table)[expRef.index].attr.arr->ndim) {
            printf("Line %d, Col %d: Too many dimensions for array.\n", expression.row, expression.col);
            return -2;
        }
        SymbolDataType dataType = typeOf(expression.attr.exp);
        if(expression.attr.exp->isTemp)
            symbolTable->freeTemp();
        SymbolTableEntryRef tmpRef = symbolTable->newTemp(dataType, sizeOf(dataType));
        int movCode = instTable->gen(OP_MOVS, tmpRef, expression.attr.exp->baseRef, expRef);
        link(sym, movCode);
        expression.attr.exp->ref = expRef = tmpRef;
        expression.attr.exp->isTemp = true;
        expression.attr.exp->ndim = 0;
        expression.attr.exp->offset = -1;
    } else if(expression.attr.exp->offset >= 0) { // struct element
        SymbolDataType dataType = typeOf(expression.attr.exp);
        if(expression.attr.exp->isTemp)
            symbolTable->freeTemp();
        SymbolTableEntryRef tmpRef = symbolTable->newTemp(dataType, sizeOf(dataType));
        int movCode = instTable->gen(OP_MOVS, tmpRef, expRef, {NULL, expression.attr.exp->offset});
        link(sym, movCode);
        expression.attr.exp->ref = expRef = tmpRef;
        expression.attr.exp->isTemp = true;
        expression.attr.exp->ndim = 0;
        expression.attr.exp->offset = -1;
    } else if((*expRef.table)[expRef.index].dataType == DT_BOOL && expression.attr.exp->isTemp) {
        int trueCode = instTable->gen(OP_TRU, NULL_REF, NULL_REF, expression.attr.exp->ref);
        int jmpCode = instTable->gen(OP_JMP, NULL_REF, NULL_REF, NULL_REF);
        int falseCode = instTable->gen(OP_FAL, NULL_REF, NULL_REF, expression.attr.exp->ref);
        link(sym, trueCode);
        link(sym, jmpCode);
        link(sym, falseCode);
        sym.nextList.push_back(jmpCode);
        int trueLabel = instTable->newLabel(trueCode);
        int falseLabel = instTable->newLabel(falseCode);
        instTable->backPatch(expression.attr.exp->trueList, trueLabel);
        instTable->backPatch(expression.attr.exp->falseList, falseLabel);
    }
    sym.nextList.splice(sym.nextList.end(), expression.nextList);
    sym.attr.exps->expList.push_back(expression.attr.exp);
    sym.attr.exps->rowList.push_back(expression.row);
    sym.attr.exps->colList.push_back(expression.col);
    return 0;
}

// EXPRESSION1 -> identifier ( EXPRESSION_S )
int SA_47(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION1 -> identifier ( EXPRESSION_S )\n");
#endif
    int n = stack->size();
    GrammaSymbol identifier = (*stack)[n - 4].sym;
    GrammaSymbol expression_s = (*stack)[n - 2].sym;
    GrammaSymbol rightParen = (*stack)[n - 1].sym;
    sym.code = sym.end = -1;
    SymbolTableEntryRef ref = symbolTable->findSymbol(identifier.attr.id->name);
    if(ref.table == NULL) {
        printf("Line %d, Col %d: Undefined function: %s\n", identifier.row, identifier.col, (*nameTable)[identifier.attr.id->name].value.stringValue);
        return -2;
    } else if((*ref.table)[ref.index].dataType != DT_BLOCK || (*ref.table)[ref.index].name == 0) {
        printf("Line %d, Col %d: Not a function: %s\n", identifier.row, identifier.col, (*nameTable)[identifier.attr.id->name].value.stringValue);
        return -2;
    }
    sym.code = expression_s.code;
    sym.end = expression_s.end;
    for(list<ExpInfo*>::iterator it = expression_s.attr.exps->expList.begin(); it != expression_s.attr.exps->expList.end(); it++) {
        if((*it)->isTemp)
            symbolTable->freeTemp();
    }
    // check the arguments
    int pCount = (*ref.table)[ref.index].attr.func->pCount;
    if(expression_s.attr.exps->expList.size() > (unsigned long)pCount) {
        printf("Line %d, Col %d: Too many arguments.\n", expression_s.attr.exps->rowList[pCount], expression_s.attr.exps->colList[pCount]);
        return -2;
    } else if(expression_s.attr.exps->expList.size() < (unsigned long)pCount) {
        printf("Line %d, Col %d: Too few arguments.\n", rightParen.row, rightParen.col);
        return -2;
    }
    int i = 1;
    for(list<ExpInfo*>::iterator it = expression_s.attr.exps->expList.begin(); it != expression_s.attr.exps->expList.end(); it++) {
        SymbolTableEntryRef pRef = {(*ref.table)[ref.index].attr.func->table, i};
        if(!typeMatch(pRef, *it)) {
            printf("Line %d, Col %d: Invalid argument type.\n", expression_s.attr.exps->rowList[i - 1], expression_s.attr.exps->colList[i - 1]);
            return -2;
        }
        i++;
    }
    for(list<ExpInfo*>::iterator it = expression_s.attr.exps->expList.begin(); it != expression_s.attr.exps->expList.end(); it++) {
        int code = instTable->gen(OP_PAR, NULL_REF, NULL_REF, (*it)->ref);
        link(sym, code);
    }
    SymbolTableEntry &returnValue = (*((*ref.table)[ref.index].attr.func->table))[0];
    sym.attr.exp->ref = symbolTable->newTemp(returnValue.dataType, returnValue.size);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    int code = instTable->gen(OP_CALL, sym.attr.exp->ref, NULL_REF, {NULL, (*ref.table)[ref.index].offset});
    link(sym, code);
    if(!expression_s.nextList.empty()) {
        int codeLabel = instTable->newLabel(code);
        instTable->backPatch(expression_s.nextList, codeLabel);
    }
    if(returnValue.dataType == DT_BOOL) {
        int trueCode = instTable->gen(OP_JNZ, sym.attr.exp->ref, NULL_REF, NULL_REF);
        int falseCode = instTable->gen(OP_JMP, NULL_REF, NULL_REF, NULL_REF);
        link(sym, trueCode);
        link(sym, falseCode);
        sym.attr.exp->trueList.push_back(trueCode);
        sym.attr.exp->falseList.push_back(falseCode);
    }
    return 0;
}

// EXPRESSION1 -> identifier ( )
int SA_48(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION1 -> identifier ( )\n");
#endif
    int n = stack->size();
    GrammaSymbol identifier = (*stack)[n - 3].sym;
    GrammaSymbol rightParen = (*stack)[n - 1].sym;
    sym.code = sym.end = -1;
    SymbolTableEntryRef ref = symbolTable->findSymbol(identifier.attr.id->name);
    if(ref.table == NULL) {
        printf("Line %d, Col %d: Undefined function: %s\n", identifier.row, identifier.col, (*nameTable)[identifier.attr.id->name].value.stringValue);
        return -2;
    } else if((*ref.table)[ref.index].dataType != DT_BLOCK) {
        printf("Line %d, Col %d: %s is not a function.\n", identifier.row, identifier.col, (*nameTable)[identifier.attr.id->name].value.stringValue);
        return -2;
    }
    // check arguments
    if((*ref.table)[ref.index].attr.func->pCount > 0) {
        printf("Line %d, Col %d: Too few arguments.\n", rightParen.row, rightParen.col);
        return -2;
    }
    SymbolTableEntry &returnValue = (*((*ref.table)[ref.index].attr.func->table))[0];
    sym.attr.exp->ref = symbolTable->newTemp(returnValue.dataType, returnValue.size);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    int code = instTable->gen(OP_CALL, sym.attr.exp->ref, NULL_REF, {NULL, (*ref.table)[ref.index].offset});
    sym.code = sym.end = code;
    if(returnValue.dataType == DT_BOOL) {
        int trueCode = instTable->gen(OP_JNZ, sym.attr.exp->ref, NULL_REF, NULL_REF);
        int falseCode = instTable->gen(OP_JMP, NULL_REF, NULL_REF, NULL_REF);
        link(sym, trueCode);
        link(sym, falseCode);
        sym.attr.exp->trueList.push_back(trueCode);
        sym.attr.exp->falseList.push_back(falseCode);
    }
    return 0;
}

// EXPRESSION2 -> EXPRESSION1
int SA_49(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION2 -> EXPRESSION1\n");
#endif
    int n = stack->size();
    GrammaSymbol expression1 = (*stack)[n - 1].sym;
    sym.code = expression1.code;
    sym.end = expression1.end;
    sym.nextList.splice(sym.nextList.end(), expression1.nextList);
    sym.attr.exp = expression1.attr.exp;
    return 0;
}

// EXPRESSION2 -> identifier
int SA_50(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION2 -> identifier\n");
#endif
    int n = stack->size();
    GrammaSymbol identifier = (*stack)[n - 1].sym;
    sym.code = sym.end = -1;
    sym.attr.exp->isTemp = false;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    sym.attr.exp->ref = symbolTable->findSymbol(identifier.attr.id->name);
    if(sym.attr.exp->ref.table == NULL) {
        printf("Line %d, Col %d: Undefined identifier: %s\n", identifier.row, identifier.col, (*nameTable)[identifier.attr.id->name].value.stringValue);
        return -2;
    }
    return 0;
}

// EXPRESSION2 -> constant
int SA_51(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION2 -> constant\n");
#endif
    int n = stack->size();
    GrammaSymbol constant = (*stack)[n - 1].sym;
    sym.code = sym.end = -1;
    sym.attr.exp->isTemp = false;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    sym.attr.exp->ref = symbolTable->findSymbol(constant.attr.con->name);
    if(sym.attr.exp->ref.table == NULL) {
#ifdef DEBUG
        fprintf(stderr, "[ERROR] Undefined constant.\n");
#endif
        return -1;
    }
    return 0;
}

// EXPRESSION2 -> ( EXPRESSION )
int SA_52(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION2 -> ( EXPRESSION )\n");
#endif
    int n = stack->size();
    GrammaSymbol expression = (*stack)[n - 2].sym;
    sym.code = expression.code;
    sym.end = expression.end;
    sym.nextList.splice(sym.nextList.end(), expression.nextList);
    sym.attr.exp = expression.attr.exp;
    return 0;
}

// EXPRESSION2 -> ! EXPRESSION2
int SA_53(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION2 -> ! EXPRESSION2\n");
#endif
    int n = stack->size();
    GrammaSymbol expression2 = (*stack)[n - 1].sym;
    sym.code = expression2.code;
    sym.end = expression2.end;
    SymbolTableEntryRef ref = expression2.attr.exp->ref;
    if(!typeMatch(DT_BOOL, expression2.attr.exp)) {
        printf("Line %d, Col %d: Invalid operand type.\n", expression2.row, expression2.col);
        return -2;
    }
    // now it must be a boolean value
    if(expression2.attr.exp->isTemp && expression2.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else {
        if(expression2.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, int> code = genBoolJmpCode(expression2.attr.exp);
        link(sym, code.first);
        sym.end = code.second;
    }
    sym.attr.exp->ref = symbolTable->newTemp(DT_BOOL, BOOL_SIZE);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    sym.nextList.splice(sym.nextList.end(), expression2.nextList);
    sym.attr.exp->trueList.splice(sym.attr.exp->trueList.end(), expression2.attr.exp->falseList);
    sym.attr.exp->falseList.splice(sym.attr.exp->falseList.end(), expression2.attr.exp->trueList);
    return 0;
}

// EXPRESSION2 -> - EXPRESSION2
int SA_54(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION2 -> - EXPRESSION2\n");
#endif
    int n = stack->size();
    GrammaSymbol negative = (*stack)[n - 2].sym;
    GrammaSymbol expression2 = (*stack)[n - 1].sym;
    sym.code = expression2.code;
    sym.end = expression2.end;
    SymbolTableEntryRef ref = expression2.attr.exp->ref;
    SymbolDataType dataType = (*ref.table)[ref.index].dataType;
    if(!typeMatch(DT_FLOAT, expression2.attr.exp)) {
        printf("Line %d, Col %d: Invalid operation.\n", negative.row, negative.col);
        return -2;
    }
    int size = (*ref.table)[ref.index].size;
    if(expression2.attr.exp->isTemp && expression2.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression2.attr.exp->ndim > 0 || expression2.attr.exp->offset >= 0) {
        if(expression2.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, SymbolDataType> info = genMovsCode(expression2.attr.exp);
        int movsCode = info.first;
        ref = expression2.attr.exp->ref;
        link(sym, movsCode);
    }
    sym.attr.exp->ref = symbolTable->newTemp(dataType, size);
    int code = instTable->gen(OP_NEG, ref, NULL_REF, sym.attr.exp->ref);
    if(!expression2.nextList.empty()) {
        int label = instTable->newLabel(code);
        instTable->backPatch(expression2.nextList, label);
    }
    link(sym, code);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    return 0;
}

// EXPRESSION3 -> EXPRESSION2
int SA_55(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION3 -> EXPRESSION2\n");
#endif
    int n = stack->size();
    GrammaSymbol expression2 = (*stack)[n - 1].sym;
    sym.code = expression2.code;
    sym.end = expression2.end;
    sym.nextList.splice(sym.nextList.end(), expression2.nextList);
    sym.attr.exp = expression2.attr.exp;
    return 0;
}

// EXPRESSION3 -> EXPRESSION3 . identifier
int SA_56(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION3 -> EXPRESSION3 . identifier\n");
#endif
    int n = stack->size();
    GrammaSymbol expression3 = (*stack)[n - 3].sym;
    GrammaSymbol dot = (*stack)[n - 2].sym;
    GrammaSymbol identifier = (*stack)[n - 1].sym;
    sym.code = expression3.code;
    sym.end = expression3.end;
    sym.nextList.splice(sym.nextList.end(), expression3.nextList);
    sym.attr.exp->isTemp = false;
    sym.attr.exp->ndim = 0;
    SymbolTableEntryRef ref = expression3.attr.exp->ref;
    sym.attr.exp->ref = ref;
    if((*ref.table)[ref.index].dataType != DT_STRUCT || expression3.attr.exp->offset >= 0) {
        // TODO: add nested struct support
        printf("Line %d, Col %d: Can't use member operator on non-struct object.\n", dot.row, dot.col);
        return -2;
    }
    SymbolTable *structTable = (*ref.table)[ref.index].attr.table;
    SymbolTableEntryRef idRef = structTable->findSymbol(identifier.attr.id->name);
    if(idRef.table == NULL) {
        printf("Line %d, Col %d: Undefined member in struct.\n", identifier.row, identifier.col);
        return -2;
    }
    sym.attr.exp->offset = (*idRef.table)[idRef.index].offset;
    return 0;
}

// EXPRESSION3 -> EXPRESSION3 [ EXPRESSION ]
int SA_57(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION3 -> EXPRESSION3 [ EXPRESSION ]\n");
#endif
    int n = stack->size();
    GrammaSymbol expression3 = (*stack)[n - 4].sym;
    GrammaSymbol expression = (*stack)[n - 2].sym;
    SymbolTableEntryRef ref = expression.attr.exp->ref;
    sym.code = expression3.code;
    sym.end = expression3.end;
    link(sym, expression);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->offset = -1;
    if(!expression3.nextList.empty()) {
        int label = instTable->newLabel(expression.code);
        instTable->backPatch(expression3.nextList, label);
    }
    if(typeOf(expression.attr.exp) != DT_INT) {
        printf("Line %d, Col %d: The index of array should be an integer.\n", expression.row, expression.col);
        return -2;
    }
    if(expression3.attr.exp->ndim == 0 && typeOf(expression3.attr.exp) != DT_ARRAY) {
        printf("Line %d, Col %d: Not an array.\n", expression3.row, expression3.col);
        return -2;
    }
    int postNum = 0;
    sym.attr.exp->ref = symbolTable->newTemp(DT_INT, INT_SIZE);
    if(expression.attr.exp->isTemp || expression.attr.exp->offset >= 0) {
        if(expression.attr.exp->isTemp)
            postNum++;
        if(expression.attr.exp->ndim > 0 || expression.attr.exp->offset >= 0) {
            pair<int, SymbolDataType> info = genMovsCode(expression.attr.exp);
            int movsCode = info.first;
            ref = expression.attr.exp->ref;
            link(sym, movsCode);
        }
    }
    if(expression3.attr.exp->isTemp) {
        postNum++;
    }
    SymbolTableEntryRef baseRef = expression3.attr.exp->baseRef;
    int code = -1, end = -1;
    if(expression3.attr.exp->ndim == 0) { // base array
        baseRef = expression3.attr.exp->ref;
        sym.attr.exp->ndim = 1;
        code = end = instTable->gen(OP_MOV, sym.attr.exp->ref, ref, NULL_REF);
    } else {
        sym.attr.exp->ndim = expression3.attr.exp->ndim;
        int dimName = (*baseRef.table)[baseRef.index].attr.arr->lens[sym.attr.exp->ndim];
        SymbolTableEntryRef conRef = symbolTable->findSymbol(dimName);
        if(conRef.table == NULL) {
#ifdef DEBUG
            fprintf(stderr, "[ERROR] Array size constant not declared.\n");
#endif
            return -1;
        }
        code = instTable->gen(OP_MUL, expression3.attr.exp->ref, conRef, sym.attr.exp->ref);
        end = instTable->gen(OP_ADD, sym.attr.exp->ref, ref, sym.attr.exp->ref);
        link(code, end);
        sym.attr.exp->ndim++;
    }
    link(sym, code);
    sym.end = end;
    sym.attr.exp->baseRef = baseRef;
    if(postNum) {
        // maintain the temp symbol in the right place
        symbolTable->freeTemp(); // free temp symbol `sym`
        while(postNum--)
            symbolTable->freeTemp(); // free temp symbol `expression3` and `expression` and some else
        SymbolTableEntryRef newRef = symbolTable->newTemp(DT_INT, INT_SIZE);
        int postCode = instTable->gen(OP_MOV, newRef, sym.attr.exp->ref, NULL_REF);
        sym.attr.exp->ref = newRef;
        link(sym, postCode);
    }
    return 0;
}

// EXPRESSION4 -> EXPRESSION3
int SA_58(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION4 -> EXPRESSION3\n");
#endif
    int n = stack->size();
    GrammaSymbol expression3 = (*stack)[n - 1].sym;
    sym.code = expression3.code;
    sym.end = expression3.end;
    sym.nextList.splice(sym.nextList.end(), expression3.nextList);
    sym.attr.exp = expression3.attr.exp;
    return 0;
}

// EXPRESSION4 -> EXPRESSION4 * EXPRESSION3
int SA_59(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION4 -> EXPRESSION4 * EXPRESSION3\n");
#endif
    int n = stack->size();
    GrammaSymbol expression4 = (*stack)[n - 3].sym;
    GrammaSymbol expression3 = (*stack)[n - 1].sym;
    sym.code = expression4.code;
    sym.end = expression4.end;
    link(sym, expression3);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    SymbolTableEntryRef ref4 = expression4.attr.exp->ref;
    SymbolTableEntryRef ref3 = expression3.attr.exp->ref;
    SymbolDataType dataType4 = (*ref4.table)[ref4.index].dataType;
    SymbolDataType dataType3 = (*ref3.table)[ref3.index].dataType;
    if(!typeMatch(DT_FLOAT, expression4.attr.exp)) {
        printf("Line %d, Col %d: Invalid operand types.\n", expression4.row, expression4.col);
        return -2;
    } else if(!typeMatch(DT_FLOAT, expression3.attr.exp)) {
        printf("Line %d, Col %d: Invalid operand types.\n", expression3.row, expression3.col);
        return -2;
    }
    if(expression3.attr.exp->isTemp && expression3.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression3.attr.exp->ndim > 0 || expression3.attr.exp->offset >= 0) {
        if(expression3.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        symbolTable->newTemp(DT_NONE, sizeOf(typeOf(expression4.attr.exp))); // placeholder
        pair<int, SymbolDataType> info = genMovsCode(expression3.attr.exp);
        int movsCode = info.first;
        symbolTable->freeTemp();
        ref3 = expression3.attr.exp->ref;
        dataType3 = info.second;
        link(sym, movsCode);
    }
    if(expression4.attr.exp->isTemp && expression4.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression4.attr.exp->ndim > 0 || expression4.attr.exp->offset >= 0) {
        if(expression4.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, SymbolDataType> info = genMovsCode(expression4.attr.exp);
        int movsCode = info.first;
        ref4 = expression4.attr.exp->ref;
        dataType4 = info.second;
        link(sym, movsCode);
    }
    SymbolDataType dataType = DT_INT;
    int size = INT_SIZE;
    if(dataType4 == DT_FLOAT || dataType3 == DT_FLOAT) {
        dataType = DT_FLOAT;
        size = FLOAT_SIZE;
    }
    sym.attr.exp->ref = symbolTable->newTemp(dataType, size);
    int code = instTable->gen(OP_MUL, ref4, ref3, sym.attr.exp->ref);
    if(!expression4.nextList.empty()) {
        int label3 = instTable->newLabel(expression3.code);
        instTable->backPatch(expression4.nextList, label3);
    }
    if(!expression3.nextList.empty()) {
        int label = instTable->newLabel(code);
        instTable->backPatch(expression3.nextList, label);
    }
    link(sym, code);
    return 0;
}

// EXPRESSION4 -> EXPRESSION4 / EXPRESSION3
int SA_60(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION4 -> EXPRESSION4 / EXPRESSION3\n");
#endif
    int n = stack->size();
    GrammaSymbol expression4 = (*stack)[n - 3].sym;
    GrammaSymbol expression3 = (*stack)[n - 1].sym;
    sym.code = expression4.code;
    sym.end = expression4.end;
    link(sym, expression3);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    SymbolTableEntryRef ref4 = expression4.attr.exp->ref;
    SymbolTableEntryRef ref3 = expression3.attr.exp->ref;
    SymbolDataType dataType4 = (*ref4.table)[ref4.index].dataType;
    SymbolDataType dataType3 = (*ref3.table)[ref3.index].dataType;
    if(!typeMatch(DT_FLOAT, expression4.attr.exp)) {
        printf("Line %d, Col %d: Invalid operand types.\n", expression4.row, expression4.col);
        return -2;
    } else if(!typeMatch(DT_FLOAT, expression3.attr.exp)) {
        printf("Line %d, Col %d: Invalid operand types.\n", expression3.row, expression3.col);
        return -2;
    }
    if(expression3.attr.exp->isTemp && expression3.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression3.attr.exp->ndim > 0 || expression3.attr.exp->offset >= 0) {
        if(expression3.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        symbolTable->newTemp(DT_NONE, sizeOf(typeOf(expression4.attr.exp))); // placeholder
        pair<int, SymbolDataType> info = genMovsCode(expression3.attr.exp);
        int movsCode = info.first;
        symbolTable->freeTemp();
        ref3 = expression3.attr.exp->ref;
        dataType3 = info.second;
        link(sym, movsCode);
    }
    if(expression4.attr.exp->isTemp && expression4.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression4.attr.exp->ndim > 0 || expression4.attr.exp->offset >= 0) {
        if(expression4.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, SymbolDataType> info = genMovsCode(expression4.attr.exp);
        int movsCode = info.first;
        ref4 = expression4.attr.exp->ref;
        dataType4 = info.second;
        link(sym, movsCode);
    }
    SymbolDataType dataType = DT_INT;
    int size = INT_SIZE;
    if(dataType4 == DT_FLOAT || dataType3 == DT_FLOAT) {
        dataType = DT_FLOAT;
        size = FLOAT_SIZE;
    }
    sym.attr.exp->ref = symbolTable->newTemp(dataType, size);
    int code = instTable->gen(OP_DIV, ref4, ref3, sym.attr.exp->ref);
    if(!expression4.nextList.empty()) {
        int label3 = instTable->newLabel(expression3.code);
        instTable->backPatch(expression4.nextList, label3);
    }
    if(!expression3.nextList.empty()) {
        int label = instTable->newLabel(code);
        instTable->backPatch(expression3.nextList, label);
    }
    link(sym, code);
    return 0;
}

// EXPRESSION5 -> EXPRESSION4
int SA_61(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION5 -> EXPRESSION4\n");
#endif
    int n = stack->size();
    GrammaSymbol expression4 = (*stack)[n - 1].sym;
    sym.code = expression4.code;
    sym.end = expression4.end;
    sym.nextList.splice(sym.nextList.end(), expression4.nextList);
    sym.attr.exp = expression4.attr.exp;
    return 0;
}

// EXPRESSION5 -> EXPRESSION5 + EXPRESSION4
int SA_62(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION5 -> EXPRESSION5 + EXPRESSION4\n");
#endif
    int n = stack->size();
    GrammaSymbol expression5 = (*stack)[n - 3].sym;
    GrammaSymbol expression4 = (*stack)[n - 1].sym;
    sym.code = expression5.code;
    sym.end = expression5.end;
    link(sym, expression4);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    SymbolTableEntryRef ref5 = expression5.attr.exp->ref;
    SymbolTableEntryRef ref4 = expression4.attr.exp->ref;
    SymbolDataType dataType5 = (*ref5.table)[ref5.index].dataType;
    SymbolDataType dataType4 = (*ref4.table)[ref4.index].dataType;
    if(!typeMatch(DT_FLOAT, expression5.attr.exp)) {
        printf("Line %d, Col %d: Invalid operand types.\n", expression5.row, expression5.col);
        return -2;
    } else if(!typeMatch(DT_FLOAT, expression4.attr.exp)) {
        printf("Line %d, Col %d: Invalid operand types.\n", expression4.row, expression4.col);
        return -2;
    }
    if(expression4.attr.exp->isTemp && expression4.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression4.attr.exp->ndim > 0 || expression4.attr.exp->offset >= 0) {
        if(expression4.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        symbolTable->newTemp(DT_NONE, sizeOf(typeOf(expression5.attr.exp))); // placeholder
        pair<int, SymbolDataType> info = genMovsCode(expression4.attr.exp);
        int movsCode = info.first;
        symbolTable->freeTemp();
        ref4 = expression4.attr.exp->ref;
        dataType4 = info.second;
        link(sym, movsCode);
    }
    if(expression5.attr.exp->isTemp && expression5.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression5.attr.exp->ndim > 0 || expression5.attr.exp->offset >= 0) {
        if(expression5.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, SymbolDataType> info = genMovsCode(expression5.attr.exp);
        int movsCode = info.first;
        ref5 = expression5.attr.exp->ref;
        dataType5 = info.second;
        link(sym, movsCode);
    }
    SymbolDataType dataType = DT_INT;
    int size = INT_SIZE;
    if(dataType5 == DT_FLOAT || dataType4 == DT_FLOAT) {
        dataType = DT_FLOAT;
        size = FLOAT_SIZE;
    }
    sym.attr.exp->ref = symbolTable->newTemp(dataType, size);
    int code = instTable->gen(OP_ADD, ref5, ref4, sym.attr.exp->ref);
    if(!expression5.nextList.empty()) {
        int label4 = instTable->newLabel(expression4.code);
        instTable->backPatch(expression5.nextList, label4);
    }
    if(!expression4.nextList.empty()) {
        int label = instTable->newLabel(code);
        instTable->backPatch(expression4.nextList, label);
    }
    link(sym, code);
    return 0;
}

// EXPRESSION5 -> EXPRESSION5 - EXPRESSION4
int SA_63(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION5 -> EXPRESSION5 - EXPRESSION4\n");
#endif
    int n = stack->size();
    GrammaSymbol expression5 = (*stack)[n - 3].sym;
    GrammaSymbol expression4 = (*stack)[n - 1].sym;
    sym.code = expression5.code;
    sym.end = expression5.end;
    link(sym, expression4);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    SymbolTableEntryRef ref5 = expression5.attr.exp->ref;
    SymbolTableEntryRef ref4 = expression4.attr.exp->ref;
    SymbolDataType dataType5 = (*ref5.table)[ref5.index].dataType;
    SymbolDataType dataType4 = (*ref4.table)[ref4.index].dataType;
    if(!typeMatch(DT_FLOAT, expression5.attr.exp)) {
        printf("Line %d, Col %d: Invalid operand types.\n", expression5.row, expression5.col);
        return -2;
    } else if(!typeMatch(DT_FLOAT, expression4.attr.exp)) {
        printf("Line %d, Col %d: Invalid operand types.\n", expression4.row, expression4.col);
        return -2;
    }
    if(expression4.attr.exp->isTemp && expression4.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression4.attr.exp->ndim > 0 || expression4.attr.exp->offset >= 0) {
        if(expression4.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        symbolTable->newTemp(DT_NONE, sizeOf(typeOf(expression5.attr.exp))); // placeholder
        pair<int, SymbolDataType> info = genMovsCode(expression4.attr.exp);
        int movsCode = info.first;
        symbolTable->freeTemp();
        ref4 = expression4.attr.exp->ref;
        dataType4 = info.second;
        link(sym, movsCode);
    }
    if(expression5.attr.exp->isTemp && expression5.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression5.attr.exp->ndim > 0 || expression5.attr.exp->offset >= 0) {
        if(expression5.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, SymbolDataType> info = genMovsCode(expression5.attr.exp);
        int movsCode = info.first;
        ref5 = expression5.attr.exp->ref;
        dataType5 = info.second;
        link(sym, movsCode);
    }
    SymbolDataType dataType = DT_INT;
    int size = INT_SIZE;
    if(dataType5 == DT_FLOAT || dataType4 == DT_FLOAT) {
        dataType = DT_FLOAT;
        size = FLOAT_SIZE;
    }
    sym.attr.exp->ref = symbolTable->newTemp(dataType, size);
    int code = instTable->gen(OP_SUB, ref5, ref4, sym.attr.exp->ref);
    if(!expression5.nextList.empty()) {
        int label4 = instTable->newLabel(expression4.code);
        instTable->backPatch(expression5.nextList, label4);
    }
    if(!expression4.nextList.empty()) {
        int label = instTable->newLabel(code);
        instTable->backPatch(expression4.nextList, label);
    }
    link(sym, code);
    return 0;
}

// EXPRESSION6 -> EXPRESSION5
int SA_64(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION6 -> EXPRESSION5\n");
#endif
    int n = stack->size();
    GrammaSymbol expression5 = (*stack)[n - 1].sym;
    sym.code = expression5.code;
    sym.end = expression5.end;
    sym.nextList.splice(sym.nextList.end(), expression5.nextList);
    sym.attr.exp = expression5.attr.exp;
    return 0;
}

// EXPRESSION6 -> EXPRESSION6 == EXPRESSION5
int SA_65(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION6 -> EXPRESSION6 == EXPRESSION5\n");
#endif
    int n = stack->size();
    GrammaSymbol expression6 = (*stack)[n - 3].sym;
    GrammaSymbol expression5 = (*stack)[n - 1].sym;
    sym.code = expression6.code;
    sym.end = expression6.end;
    link(sym, expression5);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    SymbolTableEntryRef ref6 = expression6.attr.exp->ref;
    SymbolTableEntryRef ref5 = expression5.attr.exp->ref;
    if(!typeMatch(expression6.attr.exp, expression5.attr.exp) && !typeMatch(expression5.attr.exp, expression6.attr.exp)) {
        printf("Line %d, Col %d: Can't compare operands of such types.\n", expression6.row, expression6.col);
        return -2;
    }
    bool skip5 = false, skip6 = false;
    if(!expression5.attr.exp->isTemp || expression5.attr.exp->ndim > 0) {
        if(expression5.attr.exp->ndim > 0 || expression5.attr.exp->offset >= 0) {
            skip5 = true; // we free the temp symbol here and copy the value out of the struct / array
                          // so we don't do it later
            symbolTable->newTemp(DT_NONE, sizeOf(typeOf(expression6.attr.exp))); // placeholder
            pair<int, SymbolDataType> info = genMovsCode(expression5.attr.exp);
            int movsCode = info.first;
            symbolTable->freeTemp();
            ref5 = expression5.attr.exp->ref;
            link(sym, movsCode);
        }
    }
    if(!expression6.attr.exp->isTemp || expression6.attr.exp->ndim > 0) {
        if(expression6.attr.exp->ndim > 0 || expression6.attr.exp->offset >= 0) {
            skip6 = true;
            pair<int, SymbolDataType> info = genMovsCode(expression6.attr.exp);
            int movsCode = info.first;
            ref6 = expression6.attr.exp->ref;
            link(sym, movsCode);
        }
    }
    int trueCode = instTable->gen(OP_JE, ref6, ref5, NULL_REF);
    int falseCode = instTable->gen(OP_JMP, NULL_REF, NULL_REF, NULL_REF);
    sym.attr.exp->trueList.push_back(trueCode);
    sym.attr.exp->falseList.push_back(falseCode);
    if(!expression6.nextList.empty()) {
        int label5 = instTable->newLabel(expression5.code);
        instTable->backPatch(expression6.nextList, label5);
    }
    if(!expression5.nextList.empty()) {
        int label = instTable->newLabel(trueCode);
        instTable->backPatch(expression5.nextList, label);
    }
    int code5 = -1, end5 = -1, code6 = -1, end6 = -1, code = -1, end = -1;
    if(!skip5 && expression5.attr.exp->isTemp) {
        symbolTable->freeTemp();
        if((*ref5.table)[ref5.index].dataType == DT_BOOL) {
            pair<int, int> tmpCode = evalBoolExp(expression5.attr.exp, trueCode);
            code5 = tmpCode.first;
            end5 = tmpCode.second;
        }
    }
    if(!skip6 && expression6.attr.exp->isTemp) {
        symbolTable->freeTemp();
        if((*ref6.table)[ref6.index].dataType == DT_BOOL) {
            pair<int, int> tmpCode;
            if(code5 == -1)
                tmpCode = evalBoolExp(expression6.attr.exp, trueCode);
            else
                tmpCode = evalBoolExp(expression6.attr.exp, code5);
            code6 = tmpCode.first;
            end6 = tmpCode.second;
        }
    }
    link(end6, code5);
    sym.attr.exp->ref = symbolTable->newTemp(DT_BOOL, BOOL_SIZE);
    code = (code6 == -1) ? code5 : code6;
    end = (end5 == -1) ? end6 : end5;
    if(code != -1) {
        link(sym, code);
        sym.end = end;
    }
    link(sym, trueCode);
    link(sym, falseCode);
    return 0;
}

// EXPRESSION6 -> EXPRESSION6 != EXPRESSION5
int SA_66(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION6 -> EXPRESSION6 != EXPRESSION5\n");
#endif
    int n = stack->size();
    GrammaSymbol expression6 = (*stack)[n - 3].sym;
    GrammaSymbol expression5 = (*stack)[n - 1].sym;
    sym.code = expression6.code;
    sym.end = expression6.end;
    link(sym, expression5);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    SymbolTableEntryRef ref6 = expression6.attr.exp->ref;
    SymbolTableEntryRef ref5 = expression5.attr.exp->ref;
    if(!typeMatch(expression6.attr.exp, expression5.attr.exp) && !typeMatch(expression5.attr.exp, expression6.attr.exp)) {
        printf("Line %d, Col %d: Can't compare operands of such types.\n", expression6.row, expression6.col);
        return -2;
    }
    bool skip5 = false, skip6 = false;
    if(!expression5.attr.exp->isTemp || expression5.attr.exp->ndim > 0) {
        if(expression5.attr.exp->ndim > 0 || expression5.attr.exp->offset >= 0) {
            skip5 = true; // we free the temp symbol here and copy the value out of the struct / array
                          // so we don't do it later
            symbolTable->newTemp(DT_NONE, sizeOf(typeOf(expression6.attr.exp))); // placeholder
            pair<int, SymbolDataType> info = genMovsCode(expression5.attr.exp);
            int movsCode = info.first;
            symbolTable->freeTemp();
            ref5 = expression5.attr.exp->ref;
            link(sym, movsCode);
        }
    }
    if(!expression6.attr.exp->isTemp || expression6.attr.exp->ndim > 0) {
        if(expression6.attr.exp->ndim > 0 || expression6.attr.exp->offset >= 0) {
            skip6 = true;
            pair<int, SymbolDataType> info = genMovsCode(expression6.attr.exp);
            int movsCode = info.first;
            ref6 = expression6.attr.exp->ref;
            link(sym, movsCode);
        }
    }
    int trueCode = instTable->gen(OP_JNE, ref6, ref5, NULL_REF);
    int falseCode = instTable->gen(OP_JMP, NULL_REF, NULL_REF, NULL_REF);
    sym.attr.exp->trueList.push_back(trueCode);
    sym.attr.exp->falseList.push_back(falseCode);
    if(!expression6.nextList.empty()) {
        int label5 = instTable->newLabel(expression5.code);
        instTable->backPatch(expression6.nextList, label5);
    }
    if(!expression5.nextList.empty()) {
        int label = instTable->newLabel(trueCode);
        instTable->backPatch(expression5.nextList, label);
    }
    int code5 = -1, end5 = -1, code6 = -1, end6 = -1, code = -1, end = -1;
    if(!skip5 && expression5.attr.exp->isTemp) {
        symbolTable->freeTemp();
        if((*ref5.table)[ref5.index].dataType == DT_BOOL) {
            pair<int, int> tmpCode = evalBoolExp(expression5.attr.exp, trueCode);
            code5 = tmpCode.first;
            end5 = tmpCode.second;
        }
    }
    if(!skip6 && expression6.attr.exp->isTemp) {
        symbolTable->freeTemp();
        if((*ref6.table)[ref6.index].dataType == DT_BOOL) {
            pair<int, int> tmpCode;
            if(code5 == -1)
                tmpCode = evalBoolExp(expression6.attr.exp, trueCode);
            else
                tmpCode = evalBoolExp(expression6.attr.exp, code5);
            code6 = tmpCode.first;
            end6 = tmpCode.second;
        }
    }
    link(end6, code5);
    sym.attr.exp->ref = symbolTable->newTemp(DT_BOOL, BOOL_SIZE);
    code = (code6 == -1) ? code5 : code6;
    end = (end5 == -1) ? end6 : end5;
    if(code != -1) {
        link(sym, code);
        sym.end = end;
    }
    link(sym, trueCode);
    link(sym, falseCode);
    return 0;
}

// EXPRESSION6 -> EXPRESSION6 > EXPRESSION5
int SA_67(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION6 -> EXPRESSION6 > EXPRESSION5\n");
#endif
    int n = stack->size();
    GrammaSymbol expression6 = (*stack)[n - 3].sym;
    GrammaSymbol expression5 = (*stack)[n - 1].sym;
    sym.code = expression6.code;
    sym.end = expression6.end;
    link(sym, expression5);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    SymbolTableEntryRef ref6 = expression6.attr.exp->ref;
    SymbolTableEntryRef ref5 = expression5.attr.exp->ref;
    if(!typeMatch(DT_FLOAT, expression6.attr.exp) && !typeMatch(DT_FLOAT, expression5.attr.exp)) {
        printf("Line %d, Col %d: Can't compare operands of such types.\n", expression6.row, expression6.col);
        return -2;
    }
    if(expression5.attr.exp->isTemp && expression5.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression5.attr.exp->ndim > 0 || expression5.attr.exp->offset >= 0) {
        if(expression5.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        symbolTable->newTemp(DT_NONE, sizeOf(typeOf(expression6.attr.exp))); // placeholder
        pair<int, SymbolDataType> info = genMovsCode(expression5.attr.exp);
        int movsCode = info.first;
        symbolTable->freeTemp();
        ref5 = expression5.attr.exp->ref;
        link(sym, movsCode);
    }
    if(expression6.attr.exp->isTemp && expression6.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression6.attr.exp->ndim > 0 || expression6.attr.exp->offset >= 0) {
        if(expression6.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, SymbolDataType> info = genMovsCode(expression6.attr.exp);
        int movsCode = info.first;
        ref6 = expression6.attr.exp->ref;
        link(sym, movsCode);
    }
    sym.attr.exp->ref = symbolTable->newTemp(DT_BOOL, BOOL_SIZE);
    int trueCode = instTable->gen(OP_JG, ref6, ref5, NULL_REF);
    int falseCode = instTable->gen(OP_JMP, NULL_REF, NULL_REF, NULL_REF);
    sym.attr.exp->trueList.push_back(trueCode);
    sym.attr.exp->falseList.push_back(falseCode);
    if(!expression6.nextList.empty()) {
        int label5 = instTable->newLabel(expression5.code);
        instTable->backPatch(expression6.nextList, label5);
    }
    if(!expression5.nextList.empty()) {
        int label = instTable->newLabel(trueCode);
        instTable->backPatch(expression5.nextList, label);
    }
    link(sym, trueCode);
    link(sym, falseCode);
    return 0;
}

// EXPRESSION6 -> EXPRESSION6 >= EXPRESSION5
int SA_68(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION6 -> EXPRESSION6 >= EXPRESSION5\n");
#endif
    int n = stack->size();
    GrammaSymbol expression6 = (*stack)[n - 3].sym;
    GrammaSymbol expression5 = (*stack)[n - 1].sym;
    sym.code = expression6.code;
    sym.end = expression6.end;
    link(sym, expression5);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    SymbolTableEntryRef ref6 = expression6.attr.exp->ref;
    SymbolTableEntryRef ref5 = expression5.attr.exp->ref;
    if(!typeMatch(DT_FLOAT, expression6.attr.exp) && !typeMatch(DT_FLOAT, expression5.attr.exp)) {
        printf("Line %d, Col %d: Can't compare operands of such types.\n", expression6.row, expression6.col);
        return -2;
    }
    if(expression5.attr.exp->isTemp && expression5.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression5.attr.exp->ndim > 0 || expression5.attr.exp->offset >= 0) {
        if(expression5.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        symbolTable->newTemp(DT_NONE, sizeOf(typeOf(expression6.attr.exp))); // placeholder
        pair<int, SymbolDataType> info = genMovsCode(expression5.attr.exp);
        int movsCode = info.first;
        symbolTable->freeTemp();
        ref5 = expression5.attr.exp->ref;
        link(sym, movsCode);
    }
    if(expression6.attr.exp->isTemp && expression6.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression6.attr.exp->ndim > 0 || expression6.attr.exp->offset >= 0) {
        if(expression6.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, SymbolDataType> info = genMovsCode(expression6.attr.exp);
        int movsCode = info.first;
        ref6 = expression6.attr.exp->ref;
        link(sym, movsCode);
    }
    sym.attr.exp->ref = symbolTable->newTemp(DT_BOOL, BOOL_SIZE);
    int trueCode = instTable->gen(OP_JGE, ref6, ref5, NULL_REF);
    int falseCode = instTable->gen(OP_JMP, NULL_REF, NULL_REF, NULL_REF);
    sym.attr.exp->trueList.push_back(trueCode);
    sym.attr.exp->falseList.push_back(falseCode);
    if(!expression6.nextList.empty()) {
        int label5 = instTable->newLabel(expression5.code);
        instTable->backPatch(expression6.nextList, label5);
    }
    if(!expression5.nextList.empty()) {
        int label = instTable->newLabel(trueCode);
        instTable->backPatch(expression5.nextList, label);
    }
    link(sym, trueCode);
    link(sym, falseCode);
    return 0;
}

// EXPRESSION6 -> EXPRESSION6 < EXPRESSION5
int SA_69(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION6 -> EXPRESSION6 < EXPRESSION5\n");
#endif
    int n = stack->size();
    GrammaSymbol expression6 = (*stack)[n - 3].sym;
    GrammaSymbol expression5 = (*stack)[n - 1].sym;
    sym.code = expression6.code;
    sym.end = expression6.end;
    link(sym, expression5);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    SymbolTableEntryRef ref6 = expression6.attr.exp->ref;
    SymbolTableEntryRef ref5 = expression5.attr.exp->ref;
    if(!typeMatch(DT_FLOAT, expression6.attr.exp) && !typeMatch(DT_FLOAT, expression5.attr.exp)) {
        printf("Line %d, Col %d: Can't compare operands of such types.\n", expression6.row, expression6.col);
        return -2;
    }
    if(expression5.attr.exp->isTemp && expression5.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression5.attr.exp->ndim > 0 || expression5.attr.exp->offset >= 0) {
        if(expression5.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        symbolTable->newTemp(DT_NONE, sizeOf(typeOf(expression6.attr.exp))); // placeholder
        pair<int, SymbolDataType> info = genMovsCode(expression5.attr.exp);
        int movsCode = info.first;
        symbolTable->freeTemp();
        ref5 = expression5.attr.exp->ref;
        link(sym, movsCode);
    }
    if(expression6.attr.exp->isTemp && expression6.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression6.attr.exp->ndim > 0 || expression6.attr.exp->offset >= 0) {
        if(expression6.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, SymbolDataType> info = genMovsCode(expression6.attr.exp);
        int movsCode = info.first;
        ref6 = expression6.attr.exp->ref;
        link(sym, movsCode);
    }
    sym.attr.exp->ref = symbolTable->newTemp(DT_BOOL, BOOL_SIZE);
    int trueCode = instTable->gen(OP_JL, ref6, ref5, NULL_REF);
    int falseCode = instTable->gen(OP_JMP, NULL_REF, NULL_REF, NULL_REF);
    sym.attr.exp->trueList.push_back(trueCode);
    sym.attr.exp->falseList.push_back(falseCode);
    if(!expression6.nextList.empty()) {
        int label5 = instTable->newLabel(expression5.code);
        instTable->backPatch(expression6.nextList, label5);
    }
    if(!expression5.nextList.empty()) {
        int label = instTable->newLabel(trueCode);
        instTable->backPatch(expression5.nextList, label);
    }
    link(sym, trueCode);
    link(sym, falseCode);
    return 0;
}

// EXPRESSION6 -> EXPRESSION6 <= EXPRESSION5
int SA_70(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION6 -> EXPRESSION6 <= EXPRESSION5\n");
#endif
    int n = stack->size();
    GrammaSymbol expression6 = (*stack)[n - 3].sym;
    GrammaSymbol expression5 = (*stack)[n - 1].sym;
    sym.code = expression6.code;
    sym.end = expression6.end;
    link(sym, expression5);
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    SymbolTableEntryRef ref6 = expression6.attr.exp->ref;
    SymbolTableEntryRef ref5 = expression5.attr.exp->ref;
    if(!typeMatch(DT_FLOAT, expression6.attr.exp) && !typeMatch(DT_FLOAT, expression5.attr.exp)) {
        printf("Line %d, Col %d: Can't compare operands of such types.\n", expression6.row, expression6.col);
        return -2;
    }
    if(expression5.attr.exp->isTemp && expression5.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression5.attr.exp->ndim > 0 || expression5.attr.exp->offset >= 0) {
        if(expression5.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        symbolTable->newTemp(DT_NONE, sizeOf(typeOf(expression6.attr.exp))); // placeholder
        pair<int, SymbolDataType> info = genMovsCode(expression5.attr.exp);
        int movsCode = info.first;
        symbolTable->freeTemp();
        ref5 = expression5.attr.exp->ref;
        link(sym, movsCode);
    }
    if(expression6.attr.exp->isTemp && expression6.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else if(expression6.attr.exp->ndim > 0 || expression6.attr.exp->offset >= 0) {
        if(expression6.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, SymbolDataType> info = genMovsCode(expression6.attr.exp);
        int movsCode = info.first;
        ref6 = expression6.attr.exp->ref;
        link(sym, movsCode);
    }
    sym.attr.exp->ref = symbolTable->newTemp(DT_BOOL, BOOL_SIZE);
    int trueCode = instTable->gen(OP_JLE, ref6, ref5, NULL_REF);
    int falseCode = instTable->gen(OP_JMP, NULL_REF, NULL_REF, NULL_REF);
    sym.attr.exp->trueList.push_back(trueCode);
    sym.attr.exp->falseList.push_back(falseCode);
    if(!expression6.nextList.empty()) {
        int label5 = instTable->newLabel(expression5.code);
        instTable->backPatch(expression6.nextList, label5);
    }
    if(!expression5.nextList.empty()) {
        int label = instTable->newLabel(trueCode);
        instTable->backPatch(expression5.nextList, label);
    }
    link(sym, trueCode);
    link(sym, falseCode);
    return 0;
}

// EXPRESSION7 -> EXPRESSION6
int SA_71(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION7 -> EXPRESSION6\n");
#endif
    int n = stack->size();
    GrammaSymbol expression6 = (*stack)[n - 1].sym;
    sym.code = expression6.code;
    sym.end = expression6.end;
    sym.nextList.splice(sym.nextList.end(), expression6.nextList);
    sym.attr.exp = expression6.attr.exp;
    return 0;
}

// EXPRESSION7 -> EXPRESSION7 && EXPRESSION6
int SA_72(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION7 -> EXPRESSION7 && EXPRESSION6\n");
#endif
    int n = stack->size();
    GrammaSymbol expression7 = (*stack)[n - 3].sym;
    GrammaSymbol expression6 = (*stack)[n - 1].sym;
    if(expression6.attr.exp->isTemp && expression6.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else {
        if(expression6.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, int> code = genBoolJmpCode(expression6.attr.exp);
        link(expression6, code.first);
        expression6.end = code.second;
    }
    if(expression7.attr.exp->isTemp && expression7.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else {
        if(expression7.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, int> code = genBoolJmpCode(expression7.attr.exp);
        link(expression7, code.first);
        expression7.end = code.second;
    }
    sym.code = expression7.code;
    sym.end = expression7.end;
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    sym.attr.exp->ref = symbolTable->newTemp(DT_BOOL, BOOL_SIZE);
    if(!typeMatch(DT_BOOL, expression7.attr.exp)) {
        printf("Line %d, Col %d: Operands should be boolean values.\n", expression7.row, expression7.col);
        return -2;
    } else if(!typeMatch(DT_BOOL, expression6.attr.exp)) {
        printf("Line %d, Col %d: Operands should be boolean values.\n", expression6.row, expression6.col);
        return -2;
    }
    link(sym, expression6);
    int label6 = instTable->newLabel(expression6.code);
    instTable->backPatch(expression7.attr.exp->trueList, label6);
    sym.attr.exp->trueList.splice(sym.attr.exp->trueList.end(), expression6.attr.exp->trueList);
    sym.attr.exp->falseList.splice(sym.attr.exp->falseList.end(), expression7.attr.exp->falseList);
    sym.attr.exp->falseList.splice(sym.attr.exp->falseList.end(), expression6.attr.exp->falseList);
    return 0;
}

// EXPRESSION7 -> EXPRESSION7 || EXPRESSION6
int SA_73(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION7 -> EXPRESSION7 || EXPRESSION6\n");
#endif
    int n = stack->size();
    GrammaSymbol expression7 = (*stack)[n - 3].sym;
    GrammaSymbol expression6 = (*stack)[n - 1].sym;
    if(expression7.attr.exp->isTemp && expression7.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else {
        if(expression7.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, int> code = genBoolJmpCode(expression7.attr.exp);
        link(expression7, code.first);
        expression7.end = code.second;
    }
    if(expression6.attr.exp->isTemp && expression6.attr.exp->ndim == 0)
        symbolTable->freeTemp();
    else {
        if(expression6.attr.exp->ndim > 0)
            symbolTable->freeTemp();
        pair<int, int> code = genBoolJmpCode(expression6.attr.exp);
        link(expression6, code.first);
        expression6.end = code.second;
    }
    sym.code = expression7.code;
    sym.end = expression7.end;
    sym.attr.exp->isTemp = true;
    sym.attr.exp->ndim = 0;
    sym.attr.exp->offset = -1;
    sym.attr.exp->ref = symbolTable->newTemp(DT_BOOL, BOOL_SIZE);
    if(!typeMatch(DT_BOOL, expression7.attr.exp)) {
        printf("Line %d, Col %d: Operands should be boolean values.\n", expression7.row, expression7.col);
        return -2;
    } else if(!typeMatch(DT_BOOL, expression6.attr.exp)) {
        printf("Line %d, Col %d: Operands should be boolean values.\n", expression6.row, expression6.col);
        return -2;
    }
    link(sym, expression6);
    int label6 = instTable->newLabel(expression6.code);
    instTable->backPatch(expression7.attr.exp->falseList, label6);
    sym.attr.exp->trueList.splice(sym.attr.exp->trueList.end(), expression7.attr.exp->trueList);
    sym.attr.exp->trueList.splice(sym.attr.exp->trueList.end(), expression6.attr.exp->trueList);
    sym.attr.exp->falseList.splice(sym.attr.exp->falseList.end(), expression6.attr.exp->falseList);
    return 0;
}

// EXPRESSION8 -> EXPRESSION7
int SA_74(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION8 -> EXPRESSION7\n");
#endif
    int n = stack->size();
    GrammaSymbol expression7 = (*stack)[n - 1].sym;
    sym.code = expression7.code;
    sym.end = expression7.end;
    sym.nextList.splice(sym.nextList.end(), expression7.nextList);
    sym.attr.exp = expression7.attr.exp;
    return 0;
}

// EXPRESSION8 -> EXPRESSION7 = EXPRESSION8
int SA_75(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION8 -> EXPRESSION7 = EXPRESSION8\n");
#endif
    int n = stack->size();
    GrammaSymbol expression7 = (*stack)[n - 3].sym;
    GrammaSymbol expression8 = (*stack)[n - 1].sym;
    sym.code = expression7.code;
    sym.end = expression7.end;
    sym.attr.exp->isTemp = false;
    sym.attr.exp->ndim = 0;
    link(sym, expression8);
    if(!expression8.nextList.empty()) {
        int label7 = instTable->newLabel(expression7.code);
        instTable->backPatch(expression8.nextList, label7);
    }
    if(!expression7.nextList.empty())
        sym.nextList.splice(sym.nextList.end(), expression7.nextList);
    SymbolTableEntryRef ref7 = expression7.attr.exp->ref;
    SymbolTableEntryRef ref8 = expression8.attr.exp->ref;
    if(expression7.attr.exp->ndim == 0 && expression7.attr.exp->isTemp) {
        printf("Line %d, Col %d: Can't assign a value to an rvalue.\n", expression7.row, expression7.col);
        return -2;
    }
    sym.attr.exp->offset = expression7.attr.exp->offset;
    sym.attr.exp->ref = expression7.attr.exp->ref;
    SymbolDataType dataType7 = (*ref7.table)[ref7.index].dataType;
    SymbolDataType dataType8 = (*ref8.table)[ref8.index].dataType;
    if(!typeMatch(expression7.attr.exp, expression8.attr.exp)) {
        printf("Line %d, Col %d: Can't convert value type between such types.\n", expression8.row, expression8.col);
        return -2;
    }
    if(expression8.attr.exp->isTemp)
        symbolTable->freeTemp();
    bool skip8 = false;
    int code = -1, end = -1;
    if(expression7.attr.exp->offset >= 0 && expression8.attr.exp->offset >= 0) { // both are struct members
        skip8 = true;
        pair<int, SymbolDataType> info = genMovsCode(expression8.attr.exp);
        code = info.first;
        ref8 = expression8.attr.exp->ref;
        dataType8 = info.second;
        end = instTable->gen(OP_MOVT, ref7, ref8, {NULL, expression7.attr.exp->offset});
        link(code, end);
    } else if(expression7.attr.exp->offset >= 0) // struct member
        code = end = instTable->gen(OP_MOVT, ref7, ref8, {NULL, expression7.attr.exp->offset});
    else if(expression8.attr.exp->offset >= 0) // struct member
        code = end = instTable->gen(OP_MOVS, ref7, ref8, {NULL, expression8.attr.exp->offset});
    else if(expression7.attr.exp->ndim > 0 && expression8.attr.exp->ndim > 0) { // both are array element
        skip8 = true;
        pair<int, SymbolDataType> info = genMovsCode(expression8.attr.exp);
        code = info.first;
        ref8 = expression8.attr.exp->ref;
        dataType8 = info.second;
        end = instTable->gen(OP_MOVT, expression7.attr.exp->baseRef, ref8, expression7.attr.exp->ref);
        link(code, end);
        symbolTable->freeTemp();
    } else if(expression7.attr.exp->ndim > 0) { // array element
        code = end = instTable->gen(OP_MOVT, expression7.attr.exp->baseRef, ref8, expression7.attr.exp->ref);
        symbolTable->freeTemp();
    } else if(expression8.attr.exp->ndim > 0) { // array element
        code = end = instTable->gen(OP_MOVS, ref7, expression8.attr.exp->baseRef, expression8.attr.exp->ref);
        symbolTable->freeTemp();
    } else
        code = end = instTable->gen(OP_MOV, ref7, ref8, NULL_REF);
    int code8 = -1, end8 = -1;
    if(!skip8 && dataType8 == DT_BOOL && expression8.attr.exp->isTemp) {
        pair<int, int> tmpCode = evalBoolExp(expression8.attr.exp, code);
        code8 = tmpCode.first;
        end8 = tmpCode.second;
    }
    if(code8 != -1) {
        link(sym, code8);
        sym.end = end8;
    }
    link(sym, code);
    sym.end = end;
    if(dataType7 == DT_BOOL) {
        int trueCode = instTable->gen(OP_JNZ, sym.attr.exp->ref, NULL_REF, NULL_REF);
        int falseCode = instTable->gen(OP_JMP, NULL_REF, NULL_REF, NULL_REF);
        link(sym, trueCode);
        link(sym, falseCode);
        sym.attr.exp->trueList.push_back(trueCode);
        sym.attr.exp->falseList.push_back(falseCode);
    }
    return 0;
}

// EXPRESSION -> EXPRESSION8
int SA_76(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] EXPRESSION -> EXPRESSION8\n");
#endif
    int n = stack->size();
    GrammaSymbol expression8 = (*stack)[n - 1].sym;
    sym.code = expression8.code;
    sym.end = expression8.end;
    sym.nextList.splice(sym.nextList.end(), expression8.nextList);
    sym.attr.exp = expression8.attr.exp;
    return 0;
}

// IDENTIFIER_S -> IDENTIFIER_S , identifier
int SA_77(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] IDENTIFIER_S -> IDENTIFIER_S , identifier\n");
#endif
    int n = stack->size();
    GrammaSymbol identifier_s = (*stack)[n - 3].sym;
    GrammaSymbol identifier = (*stack)[n - 1].sym;
    sym.code = sym.end = -1;
    sym.attr.ids->nameList.splice(sym.attr.ids->nameList.end(), identifier_s.attr.ids->nameList);
    sym.attr.ids->nameList.push_back(identifier.attr.id->name);
    sym.attr.ids->rowList.splice(sym.attr.ids->rowList.end(), identifier_s.attr.ids->rowList);
    sym.attr.ids->colList.splice(sym.attr.ids->colList.end(), identifier_s.attr.ids->colList);
    sym.attr.ids->rowList.push_back(identifier.row);
    sym.attr.ids->colList.push_back(identifier.col);
    return 0;
}

// IDENTIFIER_S -> identifier
int SA_78(GrammaSymbol &sym) {
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] IDENTIFIER_S -> identifier\n");
#endif
    int n = stack->size();
    GrammaSymbol identifier = (*stack)[n - 1].sym;
    sym.code = sym.end = -1;
    sym.attr.ids->nameList.push_back(identifier.attr.id->name);
    sym.attr.ids->rowList.push_back(identifier.row);
    sym.attr.ids->colList.push_back(identifier.col);
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
    SA_78
};

