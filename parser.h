#ifndef __PARSER_H__
#define __PARSER_H__

#define PRINT_PRODUCTIONS

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
class LabelTable;
class GrammaSymbol;
class AnalyserStackItem;
struct TempSymbolTable;
class SymbolTable;
struct SymbolTableEntry;
struct SymbolTableEntryRef;
struct ExpInfo;
struct ExpsInfo;
struct IdsInfo;
struct IdInfo;
struct SelBeginInfo;
struct LoopBeginInfo;
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
    ConstInfo *con; // CONSTANT
    TypeInfo *typ; // TYPE, TYPE_BASIC, TYPE_ARRAY, TYPE_STRUCT
    TypeStructInfo *typ_str; // TYPE_STRUCT
    int pCount; // PARAMETERS, DECLARE_FUNC_MID
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
    list<int> trueList; // indices of instructions which depend on the true label of this symbol
    list<int> falseList; // indices of instructions which depend on the false label of this symbol
};

struct ExpsInfo {
    list<ExpInfo> expList;
    list<int> nextList;
};

struct IdInfo {
    int nameIndex;
};

struct IdsInfo {
    list<int> nameIndexList;
};

struct SelBeginInfo {
    list<int> trueList;
    list<int> falseList;
};

struct LoopBeginInfo {
    list<int> trueList;
    list<int> falseList;
};

struct FuncInfo {
    int pCount; // parameter count
    SymbolTable *table;
};

struct ArrayInfo {
    SymbolDataType dataType; // value in {DT_INT, DT_FLOAT, DT_BOOL}
    int ndim; // number of dimensions of this array
    list<int> sizes; // size of each dimension
};

struct ConstInfo {
    SymbolDataType dataType; // value in {DT_INT, DT_FLOAT}
    int nameIndex;
};

struct TypeInfo {
    SymbolDataType dataType;
    union {
        SymbolTable *table;
        ArrayInfo *arr;
    } attr;
};

struct TypeStructInfo {
    int nameIndex;
};

class AnalyserStackItem {
    public:
        AnalyserStackItem(int stat, GrammaSymbol sym);
        int stat;
        GrammaSymbol sym;
};

struct SymbolTableEntry {
    union {
        char *strValue;
        int intValue;
        double floatValue;
    } name;
    int type; // symbol type
    SymbolDataType dataType; // data type
    int offset;
    union {
        SymbolTable *table;
        ArrayInfo *arr;
        FuncInfo *func;
    } attr;
};

class SymbolTable : public vector<SymbolTableEntry> {
    public:
        SymbolTable(SymbolTable *parent);
        SymbolTableEntryRef newSymbol(const char *name, int type, SymbolDataType dataType, int size);
        SymbolTableEntryRef newSymbol(int value);
        SymbolTableEntryRef newSymbol(double value);
        SymbolTableEntryRef newTemp(SymbolTableEntry &entry, int size);
        void freeTemp();
        SymbolTableEntryRef findSymbol(const char *name);
        SymbolTableEntryRef findSymbol(int value);
        SymbolTableEntryRef findSymbol(double value);
        int tempCount;
        int offset;
        bool busy;
        SymbolTable *parent;
        TempSymbolTable *tempTable;
        map<string, int> stringMap;
        static SymbolTable *global;
        static map<int, int> intMap;
        static map<double, int> floatMap;
};

struct TempSymbolTable : public vector<int> {
    public:
        TempSymbolTable(SymbolTable *symbolTable);
        SymbolTable *symbolTable;
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
        InstTable();
        int gen(OpCode op, const SymbolTableEntryRef &arg1, const SymbolTableEntryRef &arg2, const SymbolTableEntryRef &result);
        void backPatch(list<int> &l, int label);
};

class LabelTable : public vector<int> {
    public:
        LabelTable();
        int newLabel(int index);
};

typedef vector<AnalyserStackItem> AnalyserStack;
typedef vector<int> ProductionSequence;

#ifdef PRINT_PRODUCTIONS
int parse(TokenTable &tokenTable, LexicalSymbolTable *lexicalSymbolTable, ProductionSequence &seq);
#else
int parse(TokenTable &tokenTable, LexicalSymbolTable *lexicalSymbolTable);
#endif

#endif
