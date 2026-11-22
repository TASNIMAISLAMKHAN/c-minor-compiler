#include <stdio.h>
#include <stdlib.h>

#include "cminor/ast/ast.h"
#include "cminor/common/diagnostic.h"
#include "cminor/parser/parser.h"
#include "cminor/semantic/semantic.h"
#include "cminor/ir/ir.h"
#include "cminor/backend/llvm_backend.h"

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr,
                "Usage: %s <source.cm> <output.ll>\n",
                argv[0]);
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
        diagnostic_print_stats(&diagnostics);
        ast_free(ast);
        return EXIT_FAILURE;
    }

    printf("[OK] Parsing successful.\n");

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

    printf("[OK] Semantic analysis successful.\n");

    IRProgram ir;
    ir_program_init(&ir);

    if (!ir_generate(ast, &ir)) {
        fprintf(stderr, "Student IR generation failed.\n");

        ir_program_destroy(&ir);
        semantic_model_destroy(&semantic_model);
        ast_free(ast);

        return EXIT_FAILURE;
    }

    printf("[OK] Student IR generation successful.\n");

    FILE *output = fopen(argv[2], "w");

    if (output == NULL) {
        perror(argv[2]);

        ir_program_destroy(&ir);
        semantic_model_destroy(&semantic_model);
        ast_free(ast);

        return EXIT_FAILURE;
    }

    int backend_result = llvm_backend_emit(
        &ir,
        &semantic_model,
        output
    );

    fclose(output);

    if (!backend_result) {
        fprintf(stderr, "LLVM backend generation failed.\n");

        ir_program_destroy(&ir);
        semantic_model_destroy(&semantic_model);
        ast_free(ast);

        return EXIT_FAILURE;
    }

    printf("[OK] LLVM IR generation successful.\n");
    printf("Output: %s\n", argv[2]);

    ir_program_destroy(&ir);
    semantic_model_destroy(&semantic_model);
    ast_free(ast);

    return EXIT_SUCCESS;
}
