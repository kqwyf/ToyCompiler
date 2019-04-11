#include <cstring>
#include <string>
#include <map>

using namespace std;

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

const TokenTableEntry templateTokenEntry = {
    NONE, // type
    {0} // attr
#ifdef MATCH_SOURCE
    ,0, // start
    0 // end
#endif
};
const SymbolTableEntry templateSymbolEntry {0};

map<string, int> identifierMap;
map<int, int> intConstantMap;
map<double, int> floatConstantMap;

int consumeIDKW(const char *s, TokenTable &tokenTable, SymbolTable symbolTable);
int consumeOP(const char *s, TokenTable &tokenTable);
int consumeDL(const char *s, TokenTable &tokenTable);
int consumeCS(const char *s, TokenTable &tokenTable, SymbolTable symbolTable);
int consumeCM(const char *s, TokenTable &tokenTable);

int lexicalParse(const char *s, int l, TokenTable &tokenTable, SymbolTable &symbolTable) {
    identifierMap.clear();
    intConstantMap.clear();
    floatConstantMap.clear();
    clearTable(tokenTable, symbolTable);
    int i = 0;
    while(i < l) {
        // skip blank characters
        while(i < l && strchr(BLANK_CHAR, s[i]) != NULL) i++;
        if(i == l) break;
        // judge the type of token by its first character
        int tokenLength = -1;
        if((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') || s[i] == '_') // identifier or keyword
            tokenLength = consumeIDKW(s + i, tokenTable, symbolTable);
        else if(s[i] == '/' && s[i + 1] == '*') // comment (must prior to operator)
            tokenLength = consumeCM(s + i, tokenTable);
        else if(strchr(OP_START, s[i]) != NULL       // operator ("==" is judged independently
                || (s[i] == '=' && s[i + 1] == '=')) // to be distinguished with delimiter "=")
            tokenLength = consumeOP(s + i, tokenTable);
        else if(strchr(DL_START, s[i]) != NULL) // delimiter
            tokenLength = consumeDL(s + i, tokenTable);
        else if(s[i] >= '0' && s[i] <= '9') // constant
            tokenLength = consumeCS(s + i, tokenTable, symbolTable);
        else // error
            return -1;
        if(tokenLength <= 0) return -1;
#ifdef MATCH_SOURCE
        tokenTable.back().start = i;
        tokenTable.back().end = i + tokenLength;
#endif
        i += tokenLength;
    }
    return 0;
}

void clearTable(TokenTable &tokenTable, SymbolTable &symbolTable) {
    for(TokenTable::iterator it = tokenTable.begin(); it != tokenTable.end(); it++)
        if(it->type == IDENTIFIER || it->type == COMMENT)
            if(symbolTable[it->attr.index].value.stringValue != NULL) {
                delete symbolTable[it->attr.index].value.stringValue;
                symbolTable[it->attr.index].value.stringValue = NULL;
            }
    tokenTable.clear();
    symbolTable.clear();
}

int consumeIDKW(const char *s, TokenTable &tokenTable, SymbolTable symbolTable) {
    identifierMap.clear();
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
            tokenTable.push_back(templateTokenEntry);
            tokenTable.back().type = NON_DATATYPE_KEYWORD_CODES[j];
            return i;
        }
    }
    // judge if it is a datatype keyword
    for(int j = 0; j < DATATYPE_KEYWORDS_NUM; j++) {
        if(strncmp(DATATYPE_KEYWORDS[j], s, i) == 0) {
            tokenTable.push_back(templateTokenEntry);
            tokenTable.back().type = DATATYPE;
            tokenTable.back().attr.dataType = DATATYPE_KEYWORD_CODES[j];
            return i;
        }
    }
    // now it must be an identifier
    tokenTable.push_back(templateTokenEntry);
    tokenTable.back().type = IDENTIFIER;
    char *str = new char[i + 1];
    strncpy(str, s, i);
    str[i] = '\0';
    string tmp = string(str);
    if(identifierMap[tmp] == 0) {
        tokenTable.back().attr.index = identifierMap[tmp] = symbolTable.size() + 1;
        symbolTable.push_back(templateSymbolEntry);
    }
    symbolTable[tokenTable.back().attr.index].value.stringValue = str;
    return i;
}

int consumeOP(const char *s, TokenTable &tokenTable) {
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
    tokenTable.push_back(templateTokenEntry);
    tokenTable.back().type = type;
    return l;
}

int consumeDL(const char *s, TokenTable &tokenTable) {
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
    tokenTable.push_back(templateTokenEntry);
    tokenTable.back().type = type;
    return l;
}

int consumeCS(const char *s, TokenTable &tokenTable, SymbolTable &symbolTable) {
    int i;
    bool isFloat = false;
    int intValue = 0;
    double floatValue = 0.0;
    double scale = 1;
    for(i = 1; s[i]; i++) {
        if(s[i] == '.') { // judge if it is a float number
            isFloat = true;
            floatValue = intValue;
        } else if(s[i] >= '0' && s[i] <= '9') {
            if(isFloat) {
                intValue = intValue * 10 + (int)s[i];
            } else {
                scale *= 0.1;
                floatValue += (int)s[i] * scale;
            }
        } else {
            break;
        }
    }
    if(isFloat && scale == 1) // no numbers after the decimal point
        return -1;
    tokenTable.push_back(templateTokenEntry);
    tokenTable.back().type = CONSTANT;
    symbolTable[tokenTable.back().attr.index].value.numberValue.isFloat = isFloat;
    if(isFloat) {
        if(floatConstantMap[floatValue] == 0) {
            tokenTable.back().attr.index = floatConstantMap[floatValue] = symbolTable.size() + 1;
            symbolTable.push_back(templateSymbolEntry);
        }
        symbolTable[tokenTable.back().attr.index].value.numberValue.value.floatValue = floatValue;
    } else {
        if(intConstantMap[intValue] == 0) {
            tokenTable.back().attr.index = intConstantMap[intValue] = symbolTable.size() + 1;
            symbolTable.push_back(templateSymbolEntry);
        }
        symbolTable[tokenTable.back().attr.index].value.numberValue.value.intValue = intValue;
    }
    return i;
}

int consumeCM(const char *s, TokenTable &tokenTable) {
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
    tokenTable.push_back(templateTokenEntry);
    tokenTable.back().type = COMMENT;
    return i;
}

