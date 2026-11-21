#include <stdlib.h>

#include "parser.tab.h"
#include "cminor/parser/parser.h"
#include "cminor/parser/parser_context.h"

#define RETURN_TOKEN(kind, parser_token) \
    case kind:                           \
        free(token.lexeme);              \
        return parser_token

int cmyylex(ParserContext *ctx) {
    Token token = lexer_next_token(ctx->lexer);

    cmyylloc.first_line = (int)token.span.start.line;
    cmyylloc.first_column = (int)token.span.start.column;
    cmyylloc.last_line = (int)token.span.end.line;
    cmyylloc.last_column = (int)token.span.end.column;

    switch (token.kind) {
        case TOKEN_EOF:
            free(token.lexeme);
            return 0;

        case TOKEN_IDENTIFIER:
            cmyylval.text = token.lexeme;
            return T_IDENTIFIER;

        case TOKEN_INTEGER_LITERAL:
            cmyylval.integer = token.integer_value;
            free(token.lexeme);
            return T_INTEGER_LITERAL;

        RETURN_TOKEN(TOKEN_INT, T_INT);
        RETURN_TOKEN(TOKEN_VOID, T_VOID);
        RETURN_TOKEN(TOKEN_IF, T_IF);
        RETURN_TOKEN(TOKEN_ELSE, T_ELSE);
        RETURN_TOKEN(TOKEN_WHILE, T_WHILE);
        RETURN_TOKEN(TOKEN_FOR, T_FOR);
        RETURN_TOKEN(TOKEN_BREAK, T_BREAK);
        RETURN_TOKEN(TOKEN_CONTINUE, T_CONTINUE);
        RETURN_TOKEN(TOKEN_RETURN, T_RETURN);

        RETURN_TOKEN(TOKEN_PLUS, T_PLUS);
        RETURN_TOKEN(TOKEN_MINUS, T_MINUS);
        RETURN_TOKEN(TOKEN_MULTIPLY, T_MULTIPLY);
        RETURN_TOKEN(TOKEN_DIVIDE, T_DIVIDE);
        RETURN_TOKEN(TOKEN_MODULO, T_MODULO);

        RETURN_TOKEN(TOKEN_ASSIGN, T_ASSIGN);
        RETURN_TOKEN(TOKEN_EQUAL, T_EQUAL);
        RETURN_TOKEN(TOKEN_NOT_EQUAL, T_NOT_EQUAL);
        RETURN_TOKEN(TOKEN_LESS, T_LESS);
        RETURN_TOKEN(TOKEN_GREATER, T_GREATER);
        RETURN_TOKEN(TOKEN_LESS_EQUAL, T_LESS_EQUAL);
        RETURN_TOKEN(TOKEN_GREATER_EQUAL, T_GREATER_EQUAL);

        RETURN_TOKEN(TOKEN_NOT, T_NOT);
        RETURN_TOKEN(TOKEN_LOGICAL_AND, T_LOGICAL_AND);
        RETURN_TOKEN(TOKEN_LOGICAL_OR, T_LOGICAL_OR);

        RETURN_TOKEN(TOKEN_LEFT_PAREN, T_LEFT_PAREN);
        RETURN_TOKEN(TOKEN_RIGHT_PAREN, T_RIGHT_PAREN);
        RETURN_TOKEN(TOKEN_LEFT_BRACE, T_LEFT_BRACE);
        RETURN_TOKEN(TOKEN_RIGHT_BRACE, T_RIGHT_BRACE);
        RETURN_TOKEN(TOKEN_SEMICOLON, T_SEMICOLON);
        RETURN_TOKEN(TOKEN_COMMA, T_COMMA);

        case TOKEN_ERROR:
        default:
            free(token.lexeme);
            return CMYYUNDEF;
    }
}

#undef RETURN_TOKEN

int parser_parse(
    FILE *input,
    const char *source_name,
    DiagnosticEngine *diagnostics,
    AstNode **program
) {
    if (input == NULL || diagnostics == NULL || program == NULL) {
        return 1;
    }

    *program = NULL;

    ParserContext context = {0};
    context.diagnostics = diagnostics;

    context.lexer = lexer_create(
        input,
        source_name,
        diagnostics
    );

    if (context.lexer == NULL) {
        return 1;
    }

    int result = cmyyparse(&context);

    lexer_destroy(context.lexer);

    if (result == 0 && context.syntax_errors == 0) {
        *program = context.program;
        return 0;
    }

    ast_free(context.program);
    return 1;
}
