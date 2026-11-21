#ifndef CMINOR_LEXER_SCANNER_H
#define CMINOR_LEXER_SCANNER_H

#include <stdio.h>
#include "cminor/lexer/lexer_internal.h"

typedef void *yyscan_t;

int cm_lex_init_extra(LexerContext *context, yyscan_t *scanner);
int cm_lex_destroy(yyscan_t scanner);
void cm_set_in(FILE *input, yyscan_t scanner);
int cm_lex(yyscan_t scanner);

#endif
