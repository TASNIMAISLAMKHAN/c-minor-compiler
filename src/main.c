#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "cminor/ast/ast.h"
#include "cminor/common/diagnostic.h"
#include "cminor/lexer/lexer.h"
#include "cminor/parser/parser.h"
#include "cminor/semantic/semantic.h"
#include "cminor/ir/ir.h"
#include "cminor/backend/llvm_backend.h"

typedef enum {
    MODE_COMPILE,
    MODE_CHECK,
    MODE_TOKENS,
    MODE_AST,
    MODE_SYMBOLS,
    MODE_IR,
    MODE_LLVM,
    MODE_CAPABILITIES
} CompilerMode;

static void print_usage(const char *program)
{
    printf("C Minor Compiler\n\n");
    printf("Usage: %s [option] <source.cm> [-o output]\n\n", program);
    printf("Options:\n");
    printf("  --check              Validate the complete compilation pipeline\n");
    printf("  --dump-tokens        Display lexical tokens\n");
    printf("  --dump-ast           Display the abstract syntax tree\n");
    printf("  --dump-symbols       Display semantic symbol tables\n");
    printf("  --dump-ir            Display Student IR\n");
    printf("  --emit-backend-ir    Emit LLVM IR\n");
    printf("  --capabilities       Display compiler capabilities\n");
    printf("  -o <file>            Specify output file\n");
    printf("  --help               Display this help message\n");
}

static void print_capabilities(void)
{
    printf("C Minor Compiler Capabilities\n");
    printf("=============================\n");
    printf("Language: C Minor\n");
    printf("Implementation: C\n");
    printf("Lexer: Flex\n");
    printf("Parser: Bison\n");
    printf("Intermediate Representation: Student IR\n");
    printf("Backend: LLVM IR\n");
    printf("Native code generator: LLVM llc\n");
    printf("Target: x86-64 Linux\n\n");

    printf("Supported features:\n");
    printf("  - int and void function return types\n");
    printf("  - Local integer variables and parameters\n");
    printf("  - Arithmetic and comparison operators\n");
    printf("  - Logical operators with short-circuit evaluation\n");
    printf("  - if / else, while, and for statements\n");
    printf("  - break and continue\n");
    printf("  - Function declarations and definitions\n");
    printf("  - Direct function calls and recursion\n");
    printf("  - Semantic analysis and symbol tables\n");
    printf("  - Student IR generation\n");
    printf("  - LLVM IR and x86-64 assembly generation\n");
}

static int dump_tokens(const char *source_name)
{
    FILE *input = fopen(source_name, "r");

    if (input == NULL) {
        perror(source_name);
        return EXIT_FAILURE;
    }

    DiagnosticEngine diagnostics;
    diagnostic_engine_init(&diagnostics, source_name);

    Lexer *lexer = lexer_create(input, source_name, &diagnostics);

    if (lexer == NULL) {
        fclose(input);
        fprintf(stderr, "Error: Could not initialize lexer.\n");
        return EXIT_FAILURE;
    }

    printf("========== TOKENS ==========\n");

    int failed = 0;

    while (1) {
        Token token = lexer_next_token(lexer);

        printf("%zu:%zu  %-20s",
               token.span.start.line,
               token.span.start.column,
               token_kind_name(token.kind));

        if (token.lexeme != NULL) {
            printf(" %s", token.lexeme);
        }

        if (token.kind == TOKEN_INTEGER_LITERAL) {
            printf(" (value: %d)", token.integer_value);
        }

        printf("\n");

        TokenKind kind = token.kind;
        token_destroy(&token);

        if (kind == TOKEN_ERROR) {
            failed = 1;
            break;
        }

        if (kind == TOKEN_EOF) {
            break;
        }
    }

    lexer_destroy(lexer);
    fclose(input);

    diagnostic_print_stats(&diagnostics);

    if (diagnostics.stats.error_count > 0) {
        failed = 1;
    }

    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

static int run_llc(const char *llvm_file, const char *assembly_file)
{
    pid_t child = fork();

    if (child < 0) {
        perror("fork");
        return 0;
    }

    if (child == 0) {
        char *const arguments[] = {
            "llc",
            (char *)llvm_file,
            "-o",
            (char *)assembly_file,
            NULL
        };

        execvp("llc", arguments);
        perror("llc");
        _exit(127);
    }

    int status = 0;

    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return 0;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int normalize_assembly(const char *assembly_file)
{
    FILE *input = fopen(assembly_file, "r");

    if (input == NULL) {
        perror(assembly_file);
        return 0;
    }

    size_t temp_length = strlen(assembly_file) + 5;
    char *temp_name = malloc(temp_length);

    if (temp_name == NULL) {
        fclose(input);
        return 0;
    }

    snprintf(temp_name, temp_length, "%s.tmp", assembly_file);

    FILE *output = fopen(temp_name, "w");

    if (output == NULL) {
        perror(temp_name);
        fclose(input);
        free(temp_name);
        return 0;
    }

    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;
    int success = 1;

    while ((length = getline(&line, &capacity, input)) != -1) {
        char *content = line;

        while (*content == ' ' || *content == '\t') {
            content++;
        }

        if (strncmp(content, ".prefalign", 10) == 0) {
            if (fputs("        .p2align 4\n", output) == EOF) {
                success = 0;
                break;
            }
        } else if (fwrite(line, 1, (size_t)length, output) !=
                   (size_t)length) {
            success = 0;
            break;
        }
    }

    if (ferror(input) || fclose(input) != 0) {
        success = 0;
    }

    free(line);

    if (fclose(output) != 0) {
        success = 0;
    }

    if (success && rename(temp_name, assembly_file) != 0) {
        perror("rename");
        success = 0;
    }

    if (!success) {
        fprintf(stderr, "Error: Could not normalize assembly output.\n");
        unlink(temp_name);
    }

    free(temp_name);
    return success;
}

static char *default_output_name(const char *source_name)
{
    const char *dot = strrchr(source_name, '.');
    size_t base_length = strlen(source_name);

    if (dot != NULL) {
        base_length = (size_t)(dot - source_name);
    }

    char *result = malloc(base_length + 3);

    if (result == NULL) {
        return NULL;
    }

    memcpy(result, source_name, base_length);
    memcpy(result + base_length, ".s", 3);

    return result;
}

int main(int argc, char **argv)
{
    CompilerMode mode = MODE_COMPILE;
    const char *source_name = NULL;
    const char *output_name = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 ||
            strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--check") == 0) {
            mode = MODE_CHECK;
        } else if (strcmp(argv[i], "--dump-tokens") == 0) {
            mode = MODE_TOKENS;
        } else if (strcmp(argv[i], "--dump-ast") == 0) {
            mode = MODE_AST;
        } else if (strcmp(argv[i], "--dump-symbols") == 0) {
            mode = MODE_SYMBOLS;
        } else if (strcmp(argv[i], "--dump-ir") == 0) {
            mode = MODE_IR;
        } else if (strcmp(argv[i], "--emit-backend-ir") == 0) {
            mode = MODE_LLVM;
        } else if (strcmp(argv[i], "--capabilities") == 0) {
            mode = MODE_CAPABILITIES;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -o requires an output filename.\n");
                return EXIT_FAILURE;
            }
            output_name = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option '%s'.\n", argv[i]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        } else if (source_name == NULL) {
            source_name = argv[i];
        } else {
            fprintf(stderr, "Error: Multiple source files are not supported.\n");
            return EXIT_FAILURE;
        }
    }

    if (mode == MODE_CAPABILITIES) {
        print_capabilities();
        return EXIT_SUCCESS;
    }

    if (source_name == NULL) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (mode == MODE_TOKENS) {
        return dump_tokens(source_name);
    }

    if (mode == MODE_AST && output_name != NULL) {
        fprintf(stderr, "Error: -o is not used with --dump-ast.\n");
        return EXIT_FAILURE;
    }

    if (mode == MODE_SYMBOLS && output_name != NULL) {
        fprintf(stderr, "Error: -o is not used with --dump-symbols.\n");
        return EXIT_FAILURE;
    }

    if (mode == MODE_IR && output_name != NULL) {
        fprintf(stderr, "Error: -o is not used with --dump-ir.\n");
        return EXIT_FAILURE;
    }

    FILE *input = fopen(source_name, "r");

    if (input == NULL) {
        perror(source_name);
        return EXIT_FAILURE;
    }

    DiagnosticEngine diagnostics;
    diagnostic_engine_init(&diagnostics, source_name);

    AstNode *ast = NULL;

    printf("[1/4] Parsing source...\n");

    int parse_result = parser_parse(
        input,
        source_name,
        &diagnostics,
        &ast
    );

    fclose(input);

    if (parse_result != 0 || ast == NULL) {
        fprintf(stderr, "Error: Parsing failed.\n");
        diagnostic_print_stats(&diagnostics);
        ast_free(ast);
        return EXIT_FAILURE;
    }

    printf("[OK] Parsing successful.\n");

    if (mode == MODE_AST) {
        printf("\n========== ABSTRACT SYNTAX TREE ==========\n");
        ast_dump(ast, stdout);
        ast_free(ast);
        return EXIT_SUCCESS;
    }

    printf("[2/4] Performing semantic analysis...\n");

    SemanticModel semantic_model = {0};

    int semantic_result = semantic_analyze(
        ast,
        &diagnostics,
        &semantic_model
    );

    if (semantic_result != 1) {
        fprintf(stderr, "Error: Semantic analysis failed.\n");
        diagnostic_print_stats(&diagnostics);
        semantic_model_destroy(&semantic_model);
        ast_free(ast);
        return EXIT_FAILURE;
    }

    printf("[OK] Semantic analysis successful.\n");

    if (mode == MODE_SYMBOLS) {
        printf("\n========== SYMBOL TABLE ==========\n");
        semantic_dump_symbols(&semantic_model, stdout);

        semantic_model_destroy(&semantic_model);
        ast_free(ast);
        return EXIT_SUCCESS;
    }

    printf("[3/4] Generating Student IR...\n");

    IRProgram ir = {0};

    if (!ir_generate(ast, &ir)) {
        fprintf(stderr, "Error: Student IR generation failed.\n");

        ir_program_destroy(&ir);
        semantic_model_destroy(&semantic_model);
        ast_free(ast);

        return EXIT_FAILURE;
    }

    printf("[OK] Student IR generation successful.\n");

    if (mode == MODE_IR) {
        printf("\n========== STUDENT IR ==========\n");
        ir_dump(&ir, stdout);

        ir_program_destroy(&ir);
        semantic_model_destroy(&semantic_model);
        ast_free(ast);
        return EXIT_SUCCESS;
    }

    printf("[4/4] Generating LLVM IR...\n");

    FILE *llvm_output = NULL;
    char temporary_name[] = "/tmp/cminor-llvm-XXXXXX";
    int temporary_fd = -1;
    int use_temporary = (mode == MODE_COMPILE || mode == MODE_CHECK);

    if (use_temporary) {
        temporary_fd = mkstemp(temporary_name);

        if (temporary_fd < 0) {
            perror("mkstemp");
            goto cleanup_failure;
        }

        llvm_output = fdopen(temporary_fd, "w");

        if (llvm_output == NULL) {
            perror("fdopen");
            close(temporary_fd);
            unlink(temporary_name);
            goto cleanup_failure;
        }
    } else if (output_name != NULL) {
        llvm_output = fopen(output_name, "w");

        if (llvm_output == NULL) {
            perror(output_name);
            goto cleanup_failure;
        }
    } else {
        llvm_output = stdout;
    }

    int backend_result = llvm_backend_emit(
        &ir,
        &semantic_model,
        llvm_output
    );

    if (llvm_output != stdout) {
        if (fclose(llvm_output) != 0) {
            perror("LLVM output");
            if (use_temporary) {
                unlink(temporary_name);
            }
            goto cleanup_failure;
        }
    } else {
        fflush(stdout);
    }

    if (!backend_result) {
        fprintf(stderr, "Error: LLVM IR generation failed.\n");

        if (use_temporary) {
            unlink(temporary_name);
        }

        goto cleanup_failure;
    }

    printf("[OK] LLVM IR generation successful.\n");

    if (mode == MODE_LLVM) {
        if (output_name != NULL) {
            printf("Output: %s\n", output_name);
        }

        ir_program_destroy(&ir);
        semantic_model_destroy(&semantic_model);
        ast_free(ast);
        return EXIT_SUCCESS;
    }

    if (mode == MODE_CHECK) {
        printf("\nCompilation check successful.\n");

        unlink(temporary_name);
        ir_program_destroy(&ir);
        semantic_model_destroy(&semantic_model);
        ast_free(ast);
        return EXIT_SUCCESS;
    }

    char *generated_name = NULL;
    const char *assembly_name = output_name;

    if (assembly_name == NULL) {
        generated_name = default_output_name(source_name);
        assembly_name = generated_name;
    }

    if (assembly_name == NULL) {
        fprintf(stderr, "Error: Could not create output filename.\n");
        unlink(temporary_name);
        goto cleanup_failure;
    }

    printf("Generating x86-64 assembly...\n");

    if (!run_llc(temporary_name, assembly_name)) {
        fprintf(stderr, "Error: LLVM llc failed.\n");
        unlink(temporary_name);
        free(generated_name);
        goto cleanup_failure;
    }

    if (!normalize_assembly(assembly_name)) {
        unlink(temporary_name);
        free(generated_name);
        goto cleanup_failure;
    }

    unlink(temporary_name);

    printf("[OK] Assembly generation successful.\n");
    printf("Output: %s\n", assembly_name);

    free(generated_name);
    ir_program_destroy(&ir);
    semantic_model_destroy(&semantic_model);
    ast_free(ast);

    return EXIT_SUCCESS;

cleanup_failure:
    ir_program_destroy(&ir);
    semantic_model_destroy(&semantic_model);
    ast_free(ast);
    return EXIT_FAILURE;
}
