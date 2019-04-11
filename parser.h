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
    INT_CONSTANT,
    FLOAT_CONSTANT,
    COMMENT
};

enum DataType {
    NONETYPE = 0,
    INT,
    FLOAT,
    BOOL,
    STRUCT
};

union LexicalAttribute {
    char *stringValue;
    DataType dataType;
    int intValue;
    double floatValue;
};

struct SymbolTableEntry {
    LexicalType type;
    LexicalAttribute attr;
#ifdef MATCH_SOURCE
    int start, end;
#endif
};

typedef vector<SymbolTableEntry> SymbolTable;

int lexicalParse(const char *s, SymbolTable &table);
void clearTable(SymbolTable &table);

#endif
