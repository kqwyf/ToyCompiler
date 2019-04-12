#include <cstring>
#include <string>
#include <map>
#include <cstdio>

using namespace std;

#include "parser.h"
#include "comerr.h"

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
const char BLANK_CHAR[] = " \n\t";

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
const SymbolTableEntry templateSymbolEntry {false, {0}};

// lexical analysis context
map<string, int> identifierMap;
map<int, int> intConstantMap;
map<double, int> floatConstantMap;
int row, col;
LexicalError err; // error code when consuming a token.
int skipped; // the number of skipped characters. set when the consumer functions return.
             // negative when the consumer assumes more characters present.

int consumeIDKW(const char *s, TokenTable &tokenTable, SymbolTable &symbolTable);
int consumeOP(const char *s, TokenTable &tokenTable);
int consumeDL(const char *s, TokenTable &tokenTable);
int consumeCS(const char *s, TokenTable &tokenTable, SymbolTable &symbolTable);
int consumeCM(const char *s, TokenTable &tokenTable);

int lexicalParse(const char *s, int l, TokenTable &tokenTable, SymbolTable &symbolTable) {
    identifierMap.clear();
    intConstantMap.clear();
    floatConstantMap.clear();
    clearTable(tokenTable, symbolTable);
    symbolTable.push_back(templateSymbolEntry); // index 0 of the symbol table is not used
    row = col = 1;
    int i = 0;
    while(i < l) {
        // skip blank characters
        while(i < l && strchr(BLANK_CHAR, s[i]) != NULL) {
            if(s[i] == '\n') {
                row++;
                col = 1;
            } else
                col++;
            i++;
        }
        if(i == l) break;
        // judge the type of token by its first character
        bool isComment = false;
        int tokenLength = -1;
        if((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') || s[i] == '_') // identifier or keyword
            tokenLength = consumeIDKW(s + i, tokenTable, symbolTable);
        else if(s[i] == '/' && s[i + 1] == '*') { // comment (must prior to operator)
            tokenLength = consumeCM(s + i, tokenTable);
            isComment = true;
        } else if(strchr(OP_START, s[i]) != NULL       // operator ("==" is judged independently
                || (s[i] == '=' && s[i + 1] == '=')) // to be distinguished with delimiter "=")
            tokenLength = consumeOP(s + i, tokenTable);
        else if(strchr(DL_START, s[i]) != NULL) // delimiter
            tokenLength = consumeDL(s + i, tokenTable);
        else if(s[i] >= '0' && s[i] <= '9') // constant
            tokenLength = consumeCS(s + i, tokenTable, symbolTable);
        else { // error
            tokenLength = 0;
            skipped = 1; // skip the character
            printf(LEXICAL_ERROR_MESSAGE[UNRECOGNIZED_CHARACTER], s[i]);
        }
#ifdef MATCH_SOURCE
        if(tokenLength > 0) { // token consumed successfully
            tokenTable.back().start = i;
            tokenTable.back().end = i + tokenLength;
        }
#endif
        int totLen = tokenLength + (skipped > 0 ? skipped : 0);
        i += totLen;
        if(!isComment) col += totLen; // `row` and `col` is managed by the consumer function
                                      // when consuming comments
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

int consumeIDKW(const char *s, TokenTable &tokenTable, SymbolTable &symbolTable) {
    err = LEXICAL_OK;
    skipped = 0;
    int i;
    for(i = 1; s[i]; i++) {
        if(!((s[i] >= '0' && s[i] <= '9')
                    || (s[i] >= 'a' && s[i] <= 'z')
                    || (s[i] >= 'A' && s[i] <= 'Z')
                    || s[i] == '_')) {
            break;
        }
    }
    char *str = new char[i + 1];
    strncpy(str, s, i);
    str[i] = '\0';
    // judge if it is a non-datatype keyword
    for(int j = 0; j < NON_DATATYPE_KEYWORDS_NUM; j++) {
        if(strcmp(NON_DATATYPE_KEYWORDS[j], str) == 0) {
            tokenTable.push_back(templateTokenEntry);
            tokenTable.back().type = NON_DATATYPE_KEYWORD_CODES[j];
            return i;
        }
    }
    // judge if it is a datatype keyword
    for(int j = 0; j < DATATYPE_KEYWORDS_NUM; j++) {
        if(strcmp(DATATYPE_KEYWORDS[j], str) == 0) {
            tokenTable.push_back(templateTokenEntry);
            tokenTable.back().type = DATATYPE;
            tokenTable.back().attr.dataType = DATATYPE_KEYWORD_CODES[j];
            return i;
        }
    }
    // now it must be an identifier
    tokenTable.push_back(templateTokenEntry);
    tokenTable.back().type = IDENTIFIER;
    string tmp = string(str);
    if(identifierMap[tmp] == 0) {
        identifierMap[tmp] = symbolTable.size();
        symbolTable.push_back(templateSymbolEntry);
    }
    tokenTable.back().attr.index = identifierMap[tmp];
    symbolTable[tokenTable.back().attr.index].isString = true;
    symbolTable[tokenTable.back().attr.index].value.stringValue = str;
    return i;
}

int consumeOP(const char *s, TokenTable &tokenTable) {
    err = LEXICAL_OK;
    skipped = 0;
    LexicalType type;
    int i, l = 1;
    for(i = 0; s[i]; i++) {
        if(s[i] == '+')
            type = PLUS;
        else if(s[i] == '-')
            type = MINUS;
        else if(s[i] == '*')
            type = MULTIPLY;
        else if(s[i] == '/')
            type = DIVIDE;
        else if(s[i] == '<') {
            if(s[i + 1] == '=') {
                type = LESSEQUAL;
                l = 2;
            } else
                type = LESS;
        } else if(s[i] == '>') {
            if(s[i + 1] == '=') {
                type = GREATEREQUAL;
                l = 2;
            } else
                type = GREATER;
        } else if(s[i] == '=' && s[i + 1] == '=') {
            type = EQUAL;
            l = 2;
        } else if(s[i] == '!') {
            if(s[i + 1] == '=') {
                type = NOTEQUAL;
                l = 2;
            } else
                type = NOT;
        } else if(s[i] == '&' && s[i + 1] == '&') {
            type = AND;
            l = 2;
        } else if(s[i] == '|' && s[i + 1] == '|') {
            type = OR;
            l = 2;
        } else {
            if(s[i] == '\n') return 0;
            err = UNRECOGNIZED_OPERATOR;
            printf(LEXICAL_ERROR_MESSAGE[err], row, col + i, s[i]);
            skipped++;
            continue;
        }
        break;
    }
    tokenTable.push_back(templateTokenEntry);
    tokenTable.back().type = type;
    return l;
}

int consumeDL(const char *s, TokenTable &tokenTable) {
    err = LEXICAL_OK;
    skipped = 0;
    LexicalType type;
    int i, l = 1;
    for(i = 0; s[i]; i++) {
        if(s[i] == '=')
            type = ASSIGN;
        else if(s[i] == '(')
            type = LEFTBRACKET;
        else if(s[i] == ')')
            type = RIGHTBRACKET;
        else if(s[i] == ';')
            type = SEMICOLON;
        else if(s[i] == '{')
            type = LEFTBRACE;
        else if(s[i] == '}')
            type = RIGHTBRACE;
        else {
            err = UNKNOWN_ERROR;
            printf(LEXICAL_ERROR_MESSAGE[err], row, col + i);
            skipped++;
            continue;
        }
        break;
    }
    tokenTable.push_back(templateTokenEntry);
    tokenTable.back().type = type;
    return l;
}

int consumeCS(const char *s, TokenTable &tokenTable, SymbolTable &symbolTable) {
    err = LEXICAL_OK;
    skipped = 0;
    int i;
    bool isFloat = false;
    int intValue = 0;
    double floatValue = 0.0;
    double scale = 1;
    intValue = s[0] - '0';
    for(i = 1; s[i]; i++) {
        if(s[i] == '.') { // judge if it is a float number
            isFloat = true;
            floatValue = intValue;
        } else if(s[i] >= '0' && s[i] <= '9') {
            if(isFloat) {
                scale *= 0.1;
                floatValue += (int)(s[i] - '0') * scale;
            } else {
                intValue = intValue * 10 + (int)(s[i] - '0');
            }
        } else {
            break;
        }
    }
    if(isFloat && scale == 1) { // no numbers after the decimal point
        isFloat = false;
    }
    tokenTable.push_back(templateTokenEntry);
    tokenTable.back().type = CONSTANT;
    if(isFloat) {
        if(floatConstantMap[floatValue] == 0) {
            floatConstantMap[floatValue] = symbolTable.size();
            symbolTable.push_back(templateSymbolEntry);
        }
        tokenTable.back().attr.index = floatConstantMap[floatValue];
        symbolTable[tokenTable.back().attr.index].value.numberValue.value.floatValue = floatValue;
    } else {
        if(intConstantMap[intValue] == 0) {
            intConstantMap[intValue] = symbolTable.size();
            symbolTable.push_back(templateSymbolEntry);
        }
        tokenTable.back().attr.index = intConstantMap[intValue];
        symbolTable[tokenTable.back().attr.index].value.numberValue.value.intValue = intValue;
    }
    symbolTable[tokenTable.back().attr.index].isString = false;
    symbolTable[tokenTable.back().attr.index].value.numberValue.isFloat = isFloat;
    return i;
}

// IMPORTANT: `row` and `col` is managed by the consumer function when consuming comments,
// because comment is the only kind of token which can span multiple lines.
int consumeCM(const char *s, TokenTable &tokenTable) {
    err = LEXICAL_OK;
    skipped = 0;
    int i = 0;
    bool matchedFlag = false;
    for(i = 2; s[i]; i++) {
        if(s[i] == '*' && s[i + 1] == '/') {
            matchedFlag = true;
            i = i + 2; // the true length of this comment
            col += 2;
            break;
        } else if(s[i] == '\n') {
            row++;
            col = 1;
        } else
            col++;
    }
    if(!matchedFlag) {
        err = INCOMPLETE_COMMENT;
        printf(LEXICAL_ERROR_MESSAGE[err], row, col);
        skipped = -2;
    }
    tokenTable.push_back(templateTokenEntry);
    tokenTable.back().type = COMMENT;
    return i;
}

