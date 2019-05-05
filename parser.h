#ifndef __PARSER_H__
#define __PARSER_H__

#include <vector>
#include <list>
#include <map>

#include "lex.h"
#include "symbol.h"
#include "opcode.h"

enum SymbolDataType {
    DT_NONE = 0,
    DT_BLOCK,
    DT_ARRAY,
    DT_INT,
    DT_FLOAT,
    DT_BOOL,
    DT_STRUCT,
    DT_STRUCT_DEF
};

const int INT_SIZE = 4;
const int FLOAT_SIZE = 8;
const int BOOL_SIZE = 1;

struct Inst;
class InstTable;
class GrammaSymbol;
class AnalyserStackItem;
class SymbolTable;
struct SymbolTableEntry;
struct SymbolTableEntryRef;
struct ExpInfo;
struct ExpsInfo;
struct IdsInfo;
struct IdInfo;
struct SelBeginInfo;
struct LoopBeginInfo;
struct FuncBeginInfo;
struct FuncInfo;
struct ArrayInfo;
struct ConstInfo;
struct TypeInfo;
struct TypeStructInfo;

// point to a specific entry in a specific symbol table
struct SymbolTableEntryRef {
    SymbolTable *table;
    int index;
};


union ExternalAttribute {
    ExpInfo *exp; // EXPRESSION
    ExpsInfo *exps; // EXPRESSION_S
    IdsInfo *ids; // IDENTIFIER_S
    IdInfo *id; // IDENTIFIER
    SelBeginInfo *sel_b; // SELECT_BEGIN
    LoopBeginInfo *loop_b; // LOOP_BEGIN
    FuncBeginInfo *func_b; // DECLARE_FUNC_BEGIN
    ConstInfo *con; // CONSTANT
    TypeInfo *typ; // TYPE, TYPE_BASIC, TYPE_ARRAY
    TypeStructInfo *typ_str; // TYPE_STRUCT
    int pCount; // PARAMETERS
};

class GrammaSymbol {
    public:
        GrammaSymbol(int code, int end, int type);
        int code; // index of the first instruction in the instruction pool
        int end; // index of the last instruction in the instruction pool
        int type; // symbol type
        list<int> nextList; // indices of instructions which depend on the next inst of this symbol
        ExternalAttribute attr;
};

struct ExpInfo {
    bool isTemp; // if this expression is related to a temp symbol in the symbol table
    SymbolTableEntryRef ref; // the related (maybe temp) symbol in the symbol table
    SymbolTableEntryRef baseRef; // reference to the base symbol of array
    int offset; // offset relative to the symbol base address (in array and struct), -1 for simple identifier
    int ndim; // > 0 only when the expression is an array access expression, 0 for scalar
    list<int> trueList; // indices of instructions which depend on the true label of this symbol
    list<int> falseList; // indices of instructions which depend on the false label of this symbol
};

struct ExpsInfo {
    list<ExpInfo*> expList;
};

struct IdInfo {
    int name;
};

struct IdsInfo {
    list<int> nameList;
};

struct SelBeginInfo {
    list<int> trueList;
    list<int> falseList;
};

struct LoopBeginInfo {
    list<int> trueList;
    list<int> falseList;
};

struct FuncBeginInfo {
    SymbolTableEntryRef ref;
};

struct FuncInfo {
    int pCount; // parameter count
    SymbolTable *table;
};

struct ArrayInfo {
    SymbolDataType dataType; // value in {DT_INT, DT_FLOAT, DT_BOOL}
    int ndim; // number of dimensions of this array
    vector<int> lens; // nameIndices of length of each dimension
};

struct ConstInfo {
    SymbolDataType dataType; // value in {DT_INT, DT_FLOAT}
    int name;
};

struct TypeInfo {
    SymbolDataType dataType;
    union {
        SymbolTable *table;
        ArrayInfo *arr;
    } attr;
};

struct TypeStructInfo {
    int name;
};

class AnalyserStackItem {
    public:
        AnalyserStackItem(int stat, GrammaSymbol sym);
        int stat;
        GrammaSymbol sym;
};

struct SymbolTableEntry {
    int name;
    int type; // symbol type
    SymbolDataType dataType; // data type
    int offset;
    int size;
    union {
        SymbolTable *table;
        ArrayInfo *arr;
        FuncInfo *func;
    } attr;
};

class SymbolTable : public vector<SymbolTableEntry> {
    public:
        SymbolTable(SymbolTable *parent, bool isFunc);
        SymbolTableEntryRef newSymbol(int name, int type, SymbolDataType dataType, int size);
        SymbolTableEntryRef newTemp(SymbolDataType dataType, int size);
        void freeTemp();
        SymbolTableEntryRef findSymbol(int name);
        bool existsSymbol(int name);
        int number;
        int tempCount;
        int offset;
        bool busy;
        SymbolTable *parent;
        SymbolTable *funcTable;
        map<int, int> nameMap;
        static list<SymbolTable*> tables;
        static SymbolTable *global;
        static int n;
};

struct Inst {
    int index; // index in the instruction pool
    int label; // if there is a label before this instruction. positive for label#, -1 for none
    int next; // index of the logically next instruction, -1 for none
    OpCode op;
    SymbolTableEntryRef arg1;
    SymbolTableEntryRef arg2;
    SymbolTableEntryRef result;
};

class InstTable : public vector<Inst> {
    public:
        int gen(OpCode op, const SymbolTableEntryRef &arg1, const SymbolTableEntryRef &arg2, const SymbolTableEntryRef &result);
        void backPatch(list<int> &l, int label);
        int newLabel(int index);
        void fillLabel(int code, int label);
        vector<int> labelTable;
};

typedef vector<AnalyserStackItem> AnalyserStack;

#ifdef PRINT_PRODUCTIONS
typedef vector<int> ProductionSequence;
int parse(TokenTable &tokenTable, LexicalSymbolTable *lexicalSymbolTable, InstTable *iTable, ProductionSequence &seq);
#else
int parse(TokenTable &tokenTable, LexicalSymbolTable *lexicalSymbolTable, InstTable *iTable);
#endif

#endif
