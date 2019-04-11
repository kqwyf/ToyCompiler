#include <cstring>

#include "parser.h"

/* Abbreviations:
 *
 * ID: identifier
 * KW: keyword
 * OP: operator
 * DL: delimiter
 * CS: constant
 * CM: comment
 */

const char *OP_START = "+-*/<>!&|"; // IMPORTANT: "==" should be judged independently,
                                    // in order not to be confused with delimiter "="
const char *DL_START = "=();{}";
const char BLANK_CHAR[] = {' ', '\n', '\t'};

// one-to-one correspondent non-datatype keywords and their codes
const int NON_DATATYPE_KEYWORDS_NUM = 4;
const char *(NON_DATATYPE_KEYWORDS[NON_DATATYPE_KEYWORDS_NUM]) = {"if", "else", "do", "while"};
const LexicalType NON_DATATYPE_KEYWORD_CODES[NON_DATATYPE_KEYWORDS_NUM] = {IF, ELSE, DO, WHILE};

// one-to-one correspondent datatype keywords and their codes
const int DATATYPE_KEYWORDS_NUM = 4;
const char *(DATATYPE_KEYWORDS[DATATYPE_KEYWORDS_NUM]) = {"int", "float", "bool", "struct"};
const DataType DATATYPE_KEYWORD_CODES[DATATYPE_KEYWORDS_NUM] = {INT, FLOAT, BOOL, STRUCT};

const SymbolTableEntry templateEntry = {
    NONE, // type
    {0}, // attr
#ifdef MATCH_SOURCE
    0, // start
    0 // end
#endif
};

int consumeIDKW(const char *s, SymbolTable &table);
int consumeOP(const char *s, SymbolTable &table);
int consumeDL(const char *s, SymbolTable &table);
int consumeCS(const char *s, SymbolTable &table);
int consumeCM(const char *s, SymbolTable &table);

int lexicalParse(const char *s, int l, SymbolTable &table) {
    int i = 0;
    while(i < l) {
        // skip blank characters
        while(i < l && strchr(BLANK_CHAR, s[i]) != NULL) i++;
        if(i == l) break;
        // judge the type of token by its first character
        int tokenLength = -1;
        if((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') || s[i] == '_') // identifier or keyword
            tokenLength = consumeIDKW(s + i, table);
        else if(s[i] == '/' && s[i + 1] == '*') // comment (must prior to operator)
            tokenLength = consumeCM(s + i, table);
        else if(strchr(OP_START, s[i]) != NULL       // operator ("==" is judged independently
                || (s[i] == '=' && s[i + 1] == '=')) // to be distinguished with delimiter "=")
            tokenLength = consumeOP(s + i, table);
        else if(strchr(DL_START, s[i]) != NULL) // delimiter
            tokenLength = consumeDL(s + i, table);
        else if(s[i] >= '0' && s[i] <= '9') // constant
            tokenLength = consumeCS(s + i, table);
        else // error
            return -1;
        if(tokenLength <= 0) return -1;
#ifdef MATCH_SOURCE
        table.back().start = i;
        table.back().end = i + tokenLength;
#endif
        i += tokenLength;
    }
    return 0;
}

void clearTable(SymbolTable &table) {
    for(SymbolTable::iterator it = table.begin(); it != table.end(); it++)
        if(it->type == IDENTIFIER || it->type == COMMENT)
            delete it->attr.stringValue;
    table.clear();
}

int consumeIDKW(const char *s, SymbolTable &table) {
    int i;
    for(i = 1; s[i]; i++) {
        if(!((s[i] >= '0' && s[i] <= '9')
                    || (s[i] >= 'a' && s[i] <= 'z')
                    || (s[i] >= 'A' && s[i] <= 'Z')
                    || s[i] == '_')) {
            break;
        }
    }
    // judge if it is a non-datatype keyword
    for(int j = 0; j < NON_DATATYPE_KEYWORDS_NUM; j++) {
        if(strncmp(NON_DATATYPE_KEYWORDS[j], s, i) == 0) {
            table.push_back(templateEntry);
            table.back().type = NON_DATATYPE_KEYWORD_CODES[j];
            return i;
        }
    }
    // judge if it is a datatype keyword
    for(int j = 0; j < DATATYPE_KEYWORDS_NUM; j++) {
        if(strncmp(DATATYPE_KEYWORDS[j], s, i) == 0) {
            table.push_back(templateEntry);
            table.back().type = DATATYPE;
            table.back().attr.dataType = DATATYPE_KEYWORD_CODES[j];
            return i;
        }
    }
    // now it must be an identifier
    table.push_back(templateEntry);
    table.back().type = IDENTIFIER;
    char *str = new char[i + 1];
    strncpy(str, s, i);
    str[i] = '\0';
    table.back().attr.stringValue = str;
    return i;
}

int consumeOP(const char *s, SymbolTable &table) {
    LexicalType type;
    int l = 1;
    if(s[0] == '+')
        type = PLUS;
    else if(s[0] == '-')
        type = MINUS;
    else if(s[0] == '*')
        type = MULTIPLY;
    else if(s[0] == '/')
        type = DIVIDE;
    else if(s[0] == '<') {
        if(s[1] == '=') {
            type = LESSEQUAL;
            l = 2;
        } else
            type = LESS;
    } else if(s[0] == '>') {
        if(s[1] == '=') {
            type = GREATEREQUAL;
            l = 2;
        } else
            type = GREATER;
    } else if(s[0] == '=' && s[1] == '=') {
        type = EQUAL;
        l = 2;
    } else if(s[0] == '!') {
        if(s[1] == '=') {
            type = NOTEQUAL;
            l = 2;
        } else
            type = NOT;
    } else if(s[0] == '&' && s[1] == '&') {
        type = AND;
        l = 2;
    } else if(s[0] == '|' && s[1] == '|') {
        type = OR;
        l = 2;
    } else
        return -1;
    table.push_back(templateEntry);
    table.back().type = type;
    return l;
}

int consumeDL(const char *s, SymbolTable &table) {
    LexicalType type;
    int l = 1;
    if(s[0] == '=')
        type = ASSIGN;
    else if(s[0] == '(')
        type = LEFTBRACKET;
    else if(s[0] == ')')
        type = RIGHTBRACKET;
    else if(s[0] == ';')
        type = SEMICOLON;
    else if(s[0] == '{')
        type = LEFTBRACE;
    else if(s[0] == '}')
        type = RIGHTBRACE;
    else
        return -1;
    table.push_back(templateEntry);
    table.back().type = type;
    return l;
}

int consumeCS(const char *s, SymbolTable &table) {
    int i;
    LexicalType type = INT_CONSTANT;
    int intValue = 0;
    double floatValue = 0.0;
    double scale = 1;
    for(i = 1; s[i]; i++) {
        if(s[i] == '.') { // judge if it is a float number
            type = FLOAT_CONSTANT;
            floatValue = intValue;
        } else if(s[i] >= '0' && s[i] <= '9') {
            if(type == INT_CONSTANT) {
                intValue = intValue * 10 + (int)s[i];
            } else {
                scale *= 0.1;
                floatValue += (int)s[i] * scale;
            }
        } else {
            break;
        }
    }
    if(type == FLOAT_CONSTANT && scale == 1) // no numbers after the decimal point
        return -1;
    table.push_back(templateEntry);
    table.back().type = type;
    if(type == INT_CONSTANT)
        table.back().attr.intValue = intValue;
    else
        table.back().attr.floatValue = floatValue;
    return i;
}

int consumeCM(const char *s, SymbolTable &table) {
    int i = 0;
    bool matchedFlag = false;
    for(i = 2; s[i]; i++) {
        if(s[i] == '*' && s[i + 1] == '/') {
            matchedFlag = true;
            i = i + 2; // the true length of this comment
            break;
        }
    }
    if(!matchedFlag)
        return -1;
    table.push_back(templateEntry);
    table.back().type = COMMENT;
    char *str = new char[i + 1];
    strncpy(str, s, i);
    str[i] = '\0';
    table.back().attr.stringValue = str;
    return i;
}

