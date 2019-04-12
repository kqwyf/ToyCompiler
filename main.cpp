#include <cstdio>
#include <cstring>

#include "parser.h"

const char *usage = 
"Usage:\n\
%s [-l|-g|-s] source.src [target.txt]\n\
\n\
Options:\n\
    -l: Process lexical analysis. Output the token sequence and the\n\
        symbol table.\n\
    -g: Process gramma analysis. Output the LR analysis table and the\n\
        production sequence.\n\
    -s: Process semantic analysis. Output the symbol table and the\n\
        4-element expression sequence.\n\
";

enum CompileMode {
    NONE_MODE,
    LEXICAL,
    GRAMMA,
    SEMANTIC
};

const char *(modeString[]) = {"none", "lexical", "gramma", "semantic"};

const char *(lexicalTypeString[]) = {
    "NONE",
    "IDENTIFIER",
    "DATATYPE",
    "IF",
    "ELSE",
    "DO",
    "WHILE",
    "PLUS",
    "MINUS",
    "MULTIPLY",
    "DIVIDE",
    "LESS",
    "GREATER",
    "LESSEQUAL",
    "GREATEREQUAL",
    "EQUAL",
    "NOTEQUAL",
    "AND",
    "OR",
    "NOT",
    "ASSIGN",
    "LEFTBRACKET",
    "RIGHTBRACKET",
    "SEMICOLON",
    "LEFTBRACE",
    "RIGHTBRACE",
    "CONSTANT",
    "COMMENT"
};

const char *(dataTypeString[]) = {
    "NONE",
    "INT",
    "FLOAT",
    "BOOL",
    "STRUCT"
};

int main(int argc, char **argv) {
    if(argc == 1) {
        printf(usage, argv[0]);
        return 0;
    }
    CompileMode mode = NONE_MODE;
    bool tooManyOptionsFlag = false;
    bool tooManySourcesFlag = false;
    bool tooManyTargetsFlag = false;
    bool outputTempFlag = false;
    char *sourceFile = NULL;
    char *targetFile = NULL;
    for(int i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
            if(strcmp("-l", argv[i]) == 0) {
                if(mode != NONE_MODE && !tooManyOptionsFlag) {
                    printf("Too many options. Use mode: %s\n", modeString[mode]);
                    tooManyOptionsFlag = true;
                    continue;
                }
                mode = LEXICAL;
            } else if(strcmp("-g", argv[i]) == 0) {
                if(mode != NONE_MODE && !tooManyOptionsFlag) {
                    printf("Too many options. Use mode: %s\n", modeString[mode]);
                    tooManyOptionsFlag = true;
                    continue;
                }
                mode = GRAMMA;
            } else if(strcmp("-s", argv[i]) == 0) {
                if(mode != NONE_MODE && !tooManyOptionsFlag) {
                    printf("Too many options. Use mode: %s\n", modeString[mode]);
                    tooManyOptionsFlag = true;
                    continue;
                }
                mode = SEMANTIC;
            } else if(strcmp("-o", argv[i]) == 0) {
                outputTempFlag = true;
            } else {
                printf("Unrecognized option: %s\n", argv[i]);
            }
        } else {
            if(outputTempFlag) { // set the target file
                if(targetFile == NULL) {
                    targetFile = argv[i];
                } else {
                    if(!tooManyTargetsFlag)
                        printf("Too many target files. Use file: %s\n", targetFile);
                    tooManyTargetsFlag = true;
                }
                outputTempFlag = false;
            } else { // set the source file
                if(sourceFile == NULL) {
                    sourceFile = argv[i];
                } else {
                    if(!tooManySourcesFlag)
                        printf("Too many source files. Use file: %s\n", sourceFile);
                    tooManySourcesFlag = true;
                }
            }
        }
    }
    if(sourceFile == NULL) {
        printf("No source file provided.\n");
        return 0;
    }
    if(mode == NONE_MODE) {
        printf("No compiling mode selected. Default to lexical.\n");
        mode = LEXICAL;
    }
    FILE *ft = NULL;
    if(targetFile != NULL) {
        ft = freopen(targetFile, "w", stdout);
        if(ft == NULL) {
            fprintf(stderr, "Error occured when opening the target file.\n");
        }
    }
    
    FILE *fs = fopen(sourceFile, "r");
    char *buffer;
    if(fs == NULL) {
        fprintf(stderr, "Error occured when opening the source file.\n");
        return 1;
    }
    fseek(fs, 0, SEEK_END);
    long length = ftell(fs);
    fseek(fs, 0, SEEK_SET);
    buffer = new char[length + 1];
    long len = fread(buffer, sizeof(char), length, fs);
    if(len != length) {
        fprintf(stderr, "Error occered when reading file.\n");
        return 1;
    }
    buffer[length] = '\0';
    fclose(fs);

    if(mode == LEXICAL) {
        TokenTable *tokenTable = new TokenTable();
        SymbolTable *symbolTable = new SymbolTable();
        int err = lexicalParse(buffer, length, *tokenTable, *symbolTable);
        if(err) putchar('\n');
        printf("Token sequence:\n");
        for(TokenTable::iterator it = tokenTable->begin(); it != tokenTable->end(); it++) {
#ifdef MATCH_SOURCE
            if(it->type == COMMENT) {
                printf("/* ... */     ");
            } else {
                for(int i = it->start; i < it->end; i++)
                    putchar(buffer[i]);
                for(int i = 0; i < 14 - (it->end - it->start); i++)
                    putchar(' ');
            }
            printf("  ");
#endif
            if(it->type == IDENTIFIER || it->type == CONSTANT)
                printf("< %-12s, %-6d >\n", lexicalTypeString[it->type], it->attr.index);
            else if(it->type == DATATYPE)
                printf("< %-12s, %-6s >\n", lexicalTypeString[it->type], dataTypeString[it->attr.dataType]);
            else
                printf("< %-12s,        >\n", lexicalTypeString[it->type]);
        }
        printf("\nSymbol table:\n");
        for(unsigned long i = 1; i != symbolTable->size(); i++) {
            if((*symbolTable)[i].isString)
                printf("%-4lu  %s\n", i, (*symbolTable)[i].value.stringValue);
            else if((*symbolTable)[i].value.numberValue.isFloat)
                printf("%-4lu  %f\n", i, (*symbolTable)[i].value.numberValue.value.floatValue);
            else
                printf("%-4lu  %d\n", i, (*symbolTable)[i].value.numberValue.value.intValue);
        }
    }
}
