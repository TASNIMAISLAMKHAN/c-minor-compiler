#include <stdio.h>
#include <stdlib.h>

#include "cminor/ast/ast.h"
#include "cminor/common/diagnostic.h"
#include "cminor/parser/parser.h"

int main(int argc, char **argv) {
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

    int result = parser_parse(
        input,
        argv[1],
        &diagnostics,
        &program
    );

    fclose(input);

    if (result == 0) {
        printf("\n=== ABSTRACT SYNTAX TREE ===\n");
        ast_dump(program, stdout);
        printf("\nParsing successful.\n");
    } else {
        printf("\nParsing failed.\n");
    }

    diagnostic_print_stats(&diagnostics);
    ast_free(program);

    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
