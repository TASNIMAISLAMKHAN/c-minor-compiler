#ifndef CMINOR_PARSER_H
#define CMINOR_PARSER_H

#include <stdio.h>

#include "cminor/ast/ast.h"
#include "cminor/common/diagnostic.h"

int parser_parse(
    FILE *input,
    const char *source_name,
    DiagnosticEngine *diagnostics,
    AstNode **program
);

#endif
