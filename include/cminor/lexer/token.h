#ifndef CMINOR_TOKEN_H
#define CMINOR_TOKEN_H

#include <stdint.h>
#include "cminor/common/source_location.h"

typedef enum {
    TOKEN_EOF,
    TOKEN_ERROR,

    TOKEN_IDENTIFIER,
    TOKEN_INTEGER_LITERAL,

    TOKEN_INT,
    TOKEN_VOID,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_RETURN,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_MODULO,

    TOKEN_ASSIGN,
    TOKEN_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,

    TOKEN_NOT,
    TOKEN_LOGICAL_AND,
    TOKEN_LOGICAL_OR,

    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_SEMICOLON,
    TOKEN_COMMA
} TokenKind;

typedef struct {
    TokenKind kind;
    char *lexeme;
    int32_t integer_value;
    SourceSpan span;
} Token;

const char *token_kind_name(TokenKind kind);
void token_destroy(Token *token);

#endif
