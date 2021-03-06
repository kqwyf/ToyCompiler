/*
 * This file is generated automatically by the LR(1) grammar analyser.
 */

#ifndef __PRODUCTIONS_H__
#define __PRODUCTIONS_H__

static const int PRO_N = 78;

const char *(PRO[PRO_N]) = {
    "S -> PROGRAM",
    "PROGRAM -> DECLARE_S",
    "STATEMENT_S -> STATEMENT_S STATEMENT",
    "STATEMENT_S -> STATEMENT",
    "STATEMENT -> DECLARE",
    "STATEMENT -> SELECT",
    "STATEMENT -> LOOP",
    "STATEMENT -> EXPRESSION ;",
    "STATEMENT -> STATEMENTS_BEGIN STATEMENT_S }",
    "STATEMENT -> { }",
    "STATEMENTS_BEGIN -> {",
    "DECLARE_S -> DECLARE_S DECLARE",
    "DECLARE_S -> DECLARE",
    "DECLARE -> DECLARE_VAR",
    "DECLARE -> DECLARE_STRUCT",
    "DECLARE -> DECLARE_FUNC",
    "DECLARE_VAR_S -> DECLARE_VAR_S",
    "DECLARE_VAR_S -> DECLARE_VAR",
    "DECLARE_VAR -> TYPE IDENTIFIER_S ;",
    "DECLARE_STRUCT -> DECLARE_STRUCT_BEGIN DECLARE_S } ;",
    "DECLARE_STRUCT_BEGIN -> TYPE_STRUCT {",
    "DECLARE_FUNC -> DECLARE_FUNC_BEGIN DECLARE_FUNC_MID STATEMENT_S }",
    "DECLARE_FUNC -> DECLARE_FUNC_BEGIN DECLARE_FUNC_MID }",
    "DECLARE_FUNC_BEGIN -> TYPE identifier (",
    "DECLARE_FUNC_MID -> PARAMETERS ) {",
    "DECLARE_FUNC_MID -> ) {",
    "PARAMETERS -> PARAMETERS , TYPE identifier",
    "PARAMETERS -> TYPE identifier",
    "TYPE -> TYPE_BASIC",
    "TYPE -> TYPE_ARRAY",
    "TYPE -> TYPE_STRUCT",
    "TYPE_BASIC -> int",
    "TYPE_BASIC -> float",
    "TYPE_BASIC -> bool",
    "TYPE_ARRAY -> TYPE_ARRAY [ constant ]",
    "TYPE_ARRAY -> TYPE_BASIC [ constant ]",
    "TYPE_STRUCT -> struct identifier",
    "SELECT -> SELECT_BEGIN STATEMENT",
    "SELECT -> SELECT_BEGIN STATEMENT SELECT_MID STATEMENT",
    "SELECT_BEGIN -> if ( EXPRESSION )",
    "SELECT_MID -> else",
    "LOOP -> LOOP_BEGIN STATEMENT",
    "LOOP_BEGIN -> while ( EXPRESSION )",
    "EXPRESSION_S -> EXPRESSION_S , EXPRESSION",
    "EXPRESSION_S -> EXPRESSION",
    "EXPRESSION1 -> identifier ( EXPRESSION_S )",
    "EXPRESSION1 -> identifier ( )",
    "EXPRESSION2 -> EXPRESSION1",
    "EXPRESSION2 -> identifier",
    "EXPRESSION2 -> constant",
    "EXPRESSION2 -> ( EXPRESSION )",
    "EXPRESSION2 -> ! EXPRESSION2",
    "EXPRESSION2 -> - EXPRESSION2",
    "EXPRESSION3 -> EXPRESSION2",
    "EXPRESSION3 -> EXPRESSION3 . identifier",
    "EXPRESSION3 -> EXPRESSION3 [ EXPRESSION ]",
    "EXPRESSION4 -> EXPRESSION3",
    "EXPRESSION4 -> EXPRESSION4 * EXPRESSION3",
    "EXPRESSION4 -> EXPRESSION4 / EXPRESSION3",
    "EXPRESSION5 -> EXPRESSION4",
    "EXPRESSION5 -> EXPRESSION5 + EXPRESSION4",
    "EXPRESSION5 -> EXPRESSION5 - EXPRESSION4",
    "EXPRESSION6 -> EXPRESSION5",
    "EXPRESSION6 -> EXPRESSION6 OPERATOR_REL EXPRESSION5",
    "EXPRESSION7 -> EXPRESSION6",
    "EXPRESSION7 -> EXPRESSION7 && EXPRESSION6",
    "EXPRESSION7 -> EXPRESSION7 || EXPRESSION6",
    "EXPRESSION8 -> EXPRESSION7",
    "EXPRESSION8 -> EXPRESSION8 = EXPRESSION7",
    "EXPRESSION -> EXPRESSION8",
    "IDENTIFIER_S -> IDENTIFIER_S , identifier",
    "IDENTIFIER_S -> identifier",
    "OPERATOR_REL -> ==",
    "OPERATOR_REL -> !=",
    "OPERATOR_REL -> >",
    "OPERATOR_REL -> >=",
    "OPERATOR_REL -> <",
    "OPERATOR_REL -> <="
};

#endif

