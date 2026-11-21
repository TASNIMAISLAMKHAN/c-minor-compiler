
%option reentrant
%option noyywrap
%option never-interactive
%option noinput
%option nounput
%option prefix="cm_"
%option extra-type="LexerContext *"
%option nodefault
%option 8bit

%x COMMENT

%{
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "cminor/lexer/lexer_internal.h"

#define YY_USER_ACTION \
    lexer_context_advance(yyextra, yytext, (size_t)yyleng);

#define EMIT(kind, value) do { \
    lexer_context_set_token(yyextra, kind, yytext, \
                            (size_t)yyleng, value); \
    return 1; \
} while (0)

#define ERROR_TOKEN(message) do { \
    diagnostic_report(yyextra->diagnostics, DIAGNOSTIC_ERROR, \
        (SourceSpan){yyextra->token_start, yyextra->token_end}, \
        "%s", message); \
    lexer_context_set_token(yyextra, TOKEN_ERROR, yytext, \
                            (size_t)yyleng, 0); \
    return 1; \
} while (0)
%}

DIGIT       [0-9]
LETTER      [A-Za-z_]
IDENT       {LETTER}[A-Za-z0-9_]*
INTEGER     {DIGIT}+

%%

    /* Preprocessor directives are not supported. */
^[ \t]*#[^\r\n]* {
    ERROR_TOKEN("preprocessor directives are not supported");
}

    /* C line splicing is not supported. */
\\(\r\n|\r|\n) {
    ERROR_TOKEN("line splicing is not supported");
}

    /* Whitespace */
[ \t\f\v\r\n]+ {
    /* Position tracking is handled by YY_USER_ACTION. */
}

    /* Single-line comments */
"//"[^\r\n]* {
    /* Ignore single-line comments. */
}

    /* Begin block comment */
"/*" {
    yyextra->comment_start = yyextra->token_start;
    BEGIN(COMMENT);
}

<COMMENT>"*/" {
    BEGIN(INITIAL);
}

<COMMENT>[^*\r\n]+ {
    /* Comment content. */
}

<COMMENT>"*" {
    /* Asterisk inside comment. */
}

<COMMENT>(\r\n|\r|\n) {
    /* Newlines inside comments. */
}

<COMMENT><<EOF>> {
    SourceSpan span = {
        yyextra->comment_start,
        {yyextra->line, yyextra->column, yyextra->offset}
    };

    diagnostic_report(yyextra->diagnostics, DIAGNOSTIC_ERROR,
                      span, "unterminated block comment");

    lexer_context_set_token(yyextra, TOKEN_ERROR, "/*", 2, 0);
    BEGIN(INITIAL);
    return 1;
}

    /* Keywords */
"int"       { EMIT(TOKEN_INT, 0); }
"void"      { EMIT(TOKEN_VOID, 0); }
"if"        { EMIT(TOKEN_IF, 0); }
"else"      { EMIT(TOKEN_ELSE, 0); }
"while"     { EMIT(TOKEN_WHILE, 0); }
"for"       { EMIT(TOKEN_FOR, 0); }
"break"     { EMIT(TOKEN_BREAK, 0); }
"continue"  { EMIT(TOKEN_CONTINUE, 0); }
"return"    { EMIT(TOKEN_RETURN, 0); }

    /* Integer literals */
{INTEGER} {
    char *end = NULL;
    unsigned long long value = strtoull(yytext, &end, 10);

    if (end == yytext || *end != '\0' ||
        value > INT32_MAX || value == ULLONG_MAX) {
        ERROR_TOKEN("integer literal is outside the supported range");
    }

    EMIT(TOKEN_INTEGER_LITERAL, (int32_t)value);
}

    /* Identifiers */
{IDENT} {
    EMIT(TOKEN_IDENTIFIER, 0);
}

    /* Unsupported operators */
"++"|"--"|"+="|"-="|"*="|"/="|"%="|"&="|"|="|"^="|"<<"|">>"|"->"|"..." {
    ERROR_TOKEN("unsupported operator");
}

    /* Supported multi-character operators */
"==" { EMIT(TOKEN_EQUAL, 0); }
"!=" { EMIT(TOKEN_NOT_EQUAL, 0); }
"<=" { EMIT(TOKEN_LESS_EQUAL, 0); }
">=" { EMIT(TOKEN_GREATER_EQUAL, 0); }
"&&" { EMIT(TOKEN_LOGICAL_AND, 0); }
"||" { EMIT(TOKEN_LOGICAL_OR, 0); }

    /* Supported single-character operators */
"+"  { EMIT(TOKEN_PLUS, 0); }
"-"  { EMIT(TOKEN_MINUS, 0); }
"*"  { EMIT(TOKEN_MULTIPLY, 0); }
"/"  { EMIT(TOKEN_DIVIDE, 0); }
"%"  { EMIT(TOKEN_MODULO, 0); }
"="  { EMIT(TOKEN_ASSIGN, 0); }
"<"  { EMIT(TOKEN_LESS, 0); }
">"  { EMIT(TOKEN_GREATER, 0); }
"!"  { EMIT(TOKEN_NOT, 0); }

    /* Delimiters */
"("  { EMIT(TOKEN_LEFT_PAREN, 0); }
")"  { EMIT(TOKEN_RIGHT_PAREN, 0); }
"{"  { EMIT(TOKEN_LEFT_BRACE, 0); }
"}"  { EMIT(TOKEN_RIGHT_BRACE, 0); }
";"  { EMIT(TOKEN_SEMICOLON, 0); }
","  { EMIT(TOKEN_COMMA, 0); }

    /* Unsupported quoted literals */
\"([^\"\\\r\n]|\\.)*\" {
    ERROR_TOKEN("string literals are not supported");
}

\'([^\'\\\r\n]|\\.)*\' {
    ERROR_TOKEN("character literals are not supported");
}

    /* Unterminated quoted literals */
\"([^\"\\\r\n]|\\.)* {
    ERROR_TOKEN("unterminated string literal");
}

\'([^\'\\\r\n]|\\.)* {
    ERROR_TOKEN("unterminated character literal");
}

    /* Reject non-ASCII input */
[\x80-\xFF] {
    ERROR_TOKEN("non-ASCII characters are not supported");
}

    /* Reject other unsupported characters */
. {
    ERROR_TOKEN("invalid or unsupported character");
}

%%

