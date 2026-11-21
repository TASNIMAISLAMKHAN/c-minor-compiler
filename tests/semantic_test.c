#include <stdio.h>
#include <stdlib.h>

#include "cminor/ast/ast.h"
#include "cminor/common/diagnostic.h"
#include "cminor/parser/parser.h"
#include "cminor/semantic/semantic.h"

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

    AstNode *program = NULL;
    int parse_result = parser_parse(
        input, argv[1], &diagnostics, &program
    );

    fclose(input);

    if (parse_result != 0 || program == NULL) {
        fprintf(stderr, "\nParsing failed.\n");
        diagnostic_print_stats(&diagnostics);
        ast_free(program);
        return EXIT_FAILURE;
    }

    printf("Parsing successful.\n\n");

    SemanticModel model;
    semantic_model_init(&model);

    int semantic_result = semantic_analyze(
        program, &diagnostics, &model
    );

    printf("\n========== SYMBOL TABLE ==========\n");
    semantic_dump_symbols(&model, stdout);

    printf("\n========== SEMANTIC RESULT ==========\n");

    if (semantic_result == 1) {
        printf("Semantic analysis successful.\n");
    } else {
        printf("Semantic analysis failed.\n");
    }

    diagnostic_print_stats(&diagnostics);

    semantic_model_destroy(&model);
    ast_free(program);

    return semantic_result == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
