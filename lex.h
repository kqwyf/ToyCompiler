#ifndef __LEX_H__
#define __LEX_H__

#define MATCH_SOURCE

#include <vector>

using namespace std;

enum LexicalType {
    NONE = 0,
    IDENTIFIER,
    INT,
    FLOAT,
    BOOL,
    STRUCT,
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
    DOT,
    ASSIGN,
    LEFTPAREN,
    RIGHTPAREN,
    LEFTBRACKET,
    RIGHTBRACKET,
    COMMA,
    SEMICOLON,
    LEFTBRACE,
    RIGHTBRACE,
    CONSTANT,
    COMMENT
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
    bool isString;
    SymbolValue value;
};

struct TokenTableEntry {
    LexicalType type;
    int index;
#ifdef MATCH_SOURCE
    int start, end;
#endif
};

typedef vector<TokenTableEntry> TokenTable;
typedef vector<SymbolTableEntry> SymbolTable;

int lexicalAnalyse(const char *s, int l, TokenTable &tokenTable, SymbolTable &symbolTable);

#endif
