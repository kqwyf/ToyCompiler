#ifndef __OPCODE_H__
#define __OPCODE_H__

enum OpCode {
    OP_NONE = 0,
    OP_ADD,  // *result = *arg1 + *arg2
    OP_SUB,  // *result = *arg1 - *arg2
    OP_MUL,  // *result = *arg1 * *arg2
    OP_DIV,  // *result = *arg1 / *arg2
    OP_NEG,  // *result = -*arg1
    OP_MOV,  // *arg1 = *arg2
    OP_TRU,  // *result = 1
    OP_FAL,  // *result = 0
    OP_JMP,  // goto result
    OP_JZ,   // if(*arg1 == 0) goto result
    OP_JNZ,  // if(*arg1 != 0) goto result
    OP_JL,   // if(*arg1 < *arg2) goto result
    OP_JG,   // if(*arg1 > *arg2) goto result
    OP_JLE,  // if(*arg1 <= *arg2) goto result
    OP_JGE,  // if(*arg1 >= *arg2) goto result
    OP_JE,   // if(*arg1 == *arg2) goto result
    OP_JNE,  // if(*arg1 != *arg2) goto result
    OP_PAR,  // param result
    OP_CALL, // *arg1 = result()
    OP_RET,  // return symbolTable[0]
    OP_MOVS, // *arg1 = *(arg2 + *result), when result.table is NULL, *arg1 = *(arg2 + result.index)
    OP_MOVT  // *(arg1 + *result) = *arg2, when result.table is NULL, *(arg1 + result.index) = *arg2
};

#endif
