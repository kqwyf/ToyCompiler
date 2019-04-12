#ifndef __COMERR_H__
#define __COMERR_H__

enum LexicalError {
    LEXICAL_OK = 0,
    UNKNOWN_ERROR,
    UNRECOGNIZED_CHARACTER,
    UNRECOGNIZED_OPERATOR,
    INCOMPLETE_COMMENT
};

const char *(LEXICAL_ERROR_MESSAGE[]) = {
    "Line %d, Col %d: No error.",
    "Line %d, Col %d: Unknown error."
    "Line %d, Col %d: Unrecognized character: %c. Skipped.",
    "Line %d, Col %d: Unrecognized operator: %c. Skipped.",
    "Line %d, Col %d: Incomplete comment. Enclosed automatically."
};

#endif
