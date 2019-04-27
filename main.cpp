#include <cstdio>
#include <cstring>

#include "lex.h"
#include "parser.h"
#include "grammar.h"

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
    "INT",
    "FLOAT",
    "BOOL",
    "STRUCT",
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
    "DOT",
    "ASSIGN",
    "LEFTPAREN",
    "RIGHTPAREN",
    "LEFTBRACKET",
    "RIGHTBRACKET",
    "COMMA",
    "SEMICOLON",
    "LEFTBRACE",
    "RIGHTBRACE",
    "CONSTANT",
    "COMMENT"
};

const char *(DATATYPE_STRING[]) = {
    "NONE",
    "BLOCK",
    "ARRAY",
    "INT",
    "FLOAT",
    "BOOL",
    "STRUCT",
    "STRUCT_DEF"
};

const char *(OPCODE_STRING[]) = {
    "NONE",
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "NEG",
    "MOV",
    "TRU",
    "FAL",
    "JMP",
    "JZ",
    "JNZ",
    "JL",
    "JG",
    "JLE",
    "JGE",
    "JE",
    "JNE",
    "PAR",
    "CALL",
    "RET",
    "MOVS",
    "MOVT"
};

void showTable(SymbolTable *table, LexicalSymbolTable *nameTable);

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
        printf("No compiling mode selected. Default to semantic.\n");
        mode = SEMANTIC;
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
    
    if(mode == NONE_MODE) {
        delete buffer;
        return 0;
    }

    // lexical analysis
    TokenTable *tokenTable = new TokenTable();
    LexicalSymbolTable *symbolTable = new LexicalSymbolTable();
    int err = lexicalAnalyse(buffer, length, *tokenTable, *symbolTable);
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
            printf("< %-12s, %-6d >\n", lexicalTypeString[it->type], it->index);
        else
            printf("< %-12s,        >\n", lexicalTypeString[it->type]);
    }
    printf("\nSymbol table:\n");
    for(unsigned long i = 1; i < symbolTable->size(); i++) {
        if((*symbolTable)[i].isString)
            printf("%-4lu  %s\n", i, (*symbolTable)[i].value.stringValue);
        else if((*symbolTable)[i].value.numberValue.isFloat)
            printf("%-4lu  %f\n", i, (*symbolTable)[i].value.numberValue.value.floatValue);
        else
            printf("%-4lu  %d\n", i, (*symbolTable)[i].value.numberValue.value.intValue);
    }
    delete buffer;

    if(mode == LEXICAL) {
        delete tokenTable;
        delete symbolTable;
        return 0;
    }

    // gramma and semantic analysis
    InstTable *instTable = NULL;
#ifdef PRINT_PRODUCTIONS
    ProductionSequence *productionSequence = new ProductionSequence();
    if(mode == SEMANTIC)
        instTable = new InstTable();
    err = parse(*tokenTable, symbolTable, instTable, *productionSequence);
    if(err) putchar('\n');
    printf("\nProduction sequence:\n");
    for(ProductionSequence::iterator it = productionSequence->begin(); it != productionSequence->end(); it++)
        printf("%s\n", PRO[*it]);
#else
    if(mode == SEMANTIC)
        instTable = new InstTable();
    err = parse(*tokenTable, symbolTable, instTable);
#endif
    if(mode == GRAMMA) {
        delete tokenTable;
        delete symbolTable;
#ifdef PRINT_PRODUCTIONS
        delete productionSequence;
#endif
        return 0;
    }
    if(err) putchar('\n');
    printf("\nSemantic Symbol Tables:\n\n");
    for(list<SymbolTable*>::iterator it = SymbolTable::tables.begin(); it != SymbolTable::tables.end(); it++)
        showTable(*it, symbolTable);
    printf("\nInstruction sequence:\n");
    for(unsigned long i = 0; i < instTable->size(); i++) {
        if((*instTable)[i].label >= 0)
            printf(".L%-4d ", (*instTable)[i].label);
        else
            printf("       ");
        printf("(%4s, ", OPCODE_STRING[(*instTable)[i].op]);
        if((*instTable)[i].arg1.index == -1)
            printf("       , ");
        else
            printf("%3d:%-3d, ", (*instTable)[i].arg1.table->number, (*instTable)[i].arg1.index);
        if((*instTable)[i].arg2.index == -1)
            printf("       , ");
        else
            printf("%3d:%-3d, ", (*instTable)[i].arg2.table->number, (*instTable)[i].arg2.index);
        if((*instTable)[i].result.index == -1)
            printf("       )\n");
        else if((*instTable)[i].result.table == NULL) {
            if((*instTable)[i].op == OP_MOVS || (*instTable)[i].op == OP_MOVT)
                printf("%-4d   )\n", (*instTable)[i].result.index);
            else
                printf(".L%-4d )\n", (*instTable)[i].result.index);
        }
        else
            printf("%3d:%-3d)\n", (*instTable)[i].result.table->number, (*instTable)[i].result.index);
    }
}

void showTable(SymbolTable *table, LexicalSymbolTable *nameTable) {
    if(table->number == 0)
        printf("Global Symbol Table (Table 0):\n");
    else
        printf("Table %d:\n", table->number);
    int index = 0;
    printf("  #   |   DataType   |     Name     |  Offset  |    Attr\n");
    printf("------+--------------+--------------+----------+------------\n");
    for(SymbolTable::iterator it = table->begin(); it != table->end(); it++) {
        printf("%5d | ", index++);
        printf("%-12s | ", DATATYPE_STRING[it->dataType]);
        LexicalSymbolTableEntry &nameEntry = (*nameTable)[it->name];
        if(it->name == 0)
            printf("(anonymous)  | ");
        else if(nameEntry.isString)
            printf("%-12s | ", nameEntry.value.stringValue);
        else if(nameEntry.value.numberValue.isFloat)
            printf("%-12f | ", nameEntry.value.numberValue.value.floatValue);
        else
            printf("%-12d | ", nameEntry.value.numberValue.value.intValue);
        if(it->dataType == DT_BLOCK && it->name != 0)
            printf(".L%-6d | ", it->offset);
        else if(it->dataType == DT_BLOCK)
            printf("         | ");
        else if(it->dataType == DT_STRUCT_DEF)
            printf("[%-6d] | ", it->offset);
        else
            printf("%-8d | ", it->offset);
        if(it->dataType == DT_ARRAY) {
            if(it->attr.arr->dataType == DT_INT)
                printf("int");
            else if(it->attr.arr->dataType == DT_FLOAT)
                printf("float");
            else if(it->attr.arr->dataType == DT_BOOL)
                printf("bool");
            for(list<int>::iterator i = it->attr.arr->lens.begin(); i != it->attr.arr->lens.end(); i++)
                printf("[%d]", *i);
        } else if(it->dataType == DT_BLOCK && it->name != 0) {
            printf("Table %d, Params#: %d", it->attr.func->table->number, it->attr.func->pCount);
        } else if(it->dataType == DT_STRUCT || it->dataType == DT_STRUCT_DEF) {
            printf("Table %d", it->attr.table->number);
        }
        printf("\n");
    }
    printf("\n");
}

