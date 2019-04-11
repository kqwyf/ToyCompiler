#ifndef __PARSER_H__
#define __PARSER_H__

#define MATCH_SOURCE

#include <vector>

using namespace std;

enum LexicalType {
    NONE = 0,
    IDENTIFIER,
    DATATYPE,
    IF,
    ELSE,
    DO,
    WHILE,
    PLUS,
    MINUS,
    MULTIPLY,
    DIVIDE,
    LESS,
    GREATER,
    LESSEQUAL,
    GREATEREQUAL,
    EQUAL,
    NOTEQUAL,
    AND,
    OR,
    NOT,
    ASSIGN,
    LEFTBRACKET,
    RIGHTBRACKET,
    SEMICOLON,
    LEFTBRACE,
    RIGHTBRACE,
    CONSTANT,
    COMMENT
};

enum DataType {
    NONETYPE = 0,
    INT,
    FLOAT,
    BOOL,
    STRUCT
};

union SymbolValue {
    char *stringValue;
    struct {
        bool isFloat;
        union {
            int intValue;
            double floatValue;
        } value;
    } numberValue;
};

struct SymbolTableEntry {
    SymbolValue value;
};

union TokenAttribute {
    int index;
    DataType dataType;
};

struct TokenTableEntry {
    LexicalType type;
    TokenAttribute attr;
#ifdef MATCH_SOURCE
    int start, end;
#endif
};

typedef vector<TokenTableEntry> TokenTable;
typedef vector<SymbolTableEntry> SymbolTable;

int lexicalParse(const char *s, int l, TokenTable &tokenTable, SymbolTable &symbolTable);
void clearTable(TokenTable &tokenTable, SymbolTable &symbolTable);

#endif
