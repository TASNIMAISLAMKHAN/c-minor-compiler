#include <stdio.h>
#include <stdlib.h>

#include "cminor/ast/ast.h"
#include "cminor/common/diagnostic.h"
#include "cminor/parser/parser.h"
#include "cminor/semantic/semantic.h"
#include "cminor/ir/ir.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <source.cm>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *input = fopen(argv[1], "r");

    if (input == NULL) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    DiagnosticEngine diagnostics;
    diagnostic_engine_init(&diagnostics, argv[1]);

    AstNode *ast = NULL;

    int parse_result = parser_parse(
        input,
        argv[1],
        &diagnostics,
        &ast
    );

    fclose(input);

    if (parse_result != 0 || ast == NULL) {
        fprintf(stderr, "Parsing failed.\n");
        ast_free(ast);
        return EXIT_FAILURE;
    }

    printf("Parsing successful.\n");

    SemanticModel semantic_model;
    semantic_model_init(&semantic_model);

    int semantic_result = semantic_analyze(
        ast,
        &diagnostics,
        &semantic_model
    );

    if (semantic_result != 1) {
        fprintf(stderr, "Semantic analysis failed.\n");
        diagnostic_print_stats(&diagnostics);
        semantic_model_destroy(&semantic_model);
        ast_free(ast);
        return EXIT_FAILURE;
    }

    printf("Semantic analysis successful.\n");

    IRProgram ir;
    ir_program_init(&ir);

    if (!ir_generate(ast, &ir)) {
        fprintf(stderr, "Student IR generation failed.\n");

        ir_program_destroy(&ir);
        semantic_model_destroy(&semantic_model);
        ast_free(ast);

        return EXIT_FAILURE;
    }

    printf("Student IR generation successful.\n\n");
    ir_dump(&ir, stdout);

    ir_program_destroy(&ir);
    semantic_model_destroy(&semantic_model);
    ast_free(ast);

    return EXIT_SUCCESS;
}
