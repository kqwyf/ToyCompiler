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

union LexicalSymbolValue {
    char *stringValue;
    struct {
        bool isFloat;
        union {
            int intValue;
            double floatValue;
        } value;
    } numberValue;
};

struct LexicalSymbolTableEntry {
    bool isString;
    LexicalSymbolValue value;
};

struct TokenTableEntry {
    LexicalType type;
    int index;
    int row, col;
    char *source;
#ifdef MATCH_SOURCE
    int start, end;
#endif
};

typedef vector<TokenTableEntry> TokenTable;
typedef vector<LexicalSymbolTableEntry> LexicalSymbolTable;

int lexicalAnalyse(const char *s, int l, TokenTable &tokenTable, LexicalSymbolTable &symbolTable);

#endif
