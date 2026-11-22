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
#include "cminor/common/source_limits.h"
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
    MODE_CAPABILITIES,
    MODE_VERSION
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
    printf("  --backend=llvm       Select the LLVM backend\n");
    printf("  --version            Display compiler version\n");
    printf("  -o <file>            Specify output file\n");
    printf("  --help               Display this help message\n");
}

static void print_version(void)
{
    printf("C Minor Compiler\n");
    printf("C Minor Language Profile: 1.0\n");
    printf("Selected Backend: LLVM\n");
}

static void print_capabilities(void)
{
    printf("C Minor Compiler Capabilities\n");
    printf("=============================\n\n");

    printf("Language:\n");
    printf("  Name: C Minor\n");
    printf("  Language Profile: 1.0\n");
    printf("  Implementation Language: C\n\n");

    printf("Compiler Tools:\n");
    printf("  Compiler: GCC %s\n", __VERSION__);
    printf("  Lexer Generator: Flex 2.6.4\n");
    printf("  Parser Generator: GNU Bison 3.8.2\n\n");

    printf("Compilation Pipeline:\n");
    printf("  Lexer: Flex-generated lexer\n");
    printf("  Parser: Bison-generated parser\n");
    printf("  Intermediate Representation: Student IR\n");
    printf("  Backend Intermediate Representation: LLVM IR\n");
    printf("  Native Code Generator: LLVM llc 23.1.3\n\n");

    printf("Target Platform:\n");
    printf("  Operating System: Linux\n");
    printf("  Architecture: x86-64\n");
    printf("  Target Triple: x86_64-pc-linux-gnu\n");
    printf("  ABI: System V AMD64\n");
    printf("  Object Format: ELF\n\n");

    printf("Supported Backend:\n");
    printf("  - LLVM 23.1.3\n\n");

    printf("Accepted Source Input:\n");
    printf("  - Accepted extensions: .cm and .c\n");
    printf("  - Filename extension is validated\n");
    printf("  - Preprocessing: Not implemented\n\n");

    printf("Supported Language Features:\n");
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
    printf("  - LLVM IR and x86-64 assembly generation\n\n");

    printf("Resource Limits:\n");
    printf("  - Maximum source size: 32 KiB (32768 bytes)\n");
    printf("  - Maximum tokens: 4000\n");
    printf("  - Maximum source line length: 2048 characters\n");
    printf("  - Maximum block nesting depth: 16\n");
    printf("  - Maximum expression parenthesis depth: 16\n");
    printf("  - Maximum user functions: 32\n");
    printf("  - Maximum parameters per function: 6\n");
    printf("  - Maximum local declarations per block: 64\n\n");

    printf("Backend Selection:\n");
    printf("  - Default backend: LLVM\n");
    printf("  - Supported option: --backend=llvm\n");
    printf("  - Unsupported option: --backend=qbe\n");
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
            "-O=0",
            "-mtriple=x86_64-pc-linux-gnu",
            "-filetype=asm",
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

static int has_supported_extension(const char *filename)
{
    const char *extension = strrchr(filename, '.');

    if (extension == NULL) {
        return 0;
    }

    return strcmp(extension, ".cm") == 0 ||
           strcmp(extension, ".c") == 0;
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
    int mode_option_seen = 0;

    for (int i = 1; i < argc; i++) {
        int is_mode_option =
            strcmp(argv[i], "--version") == 0 ||
            strcmp(argv[i], "--check") == 0 ||
            strcmp(argv[i], "--dump-tokens") == 0 ||
            strcmp(argv[i], "--dump-ast") == 0 ||
            strcmp(argv[i], "--dump-symbols") == 0 ||
            strcmp(argv[i], "--dump-ir") == 0 ||
            strcmp(argv[i], "--emit-backend-ir") == 0 ||
            strcmp(argv[i], "--capabilities") == 0;

        if (is_mode_option) {
            if (mode_option_seen) {
                fprintf(stderr,
                        "Error: Compilation mode options are mutually exclusive. "
                        "Specify only one mode option.\n");
                return 2;
            }

            mode_option_seen = 1;
        }

        if (strcmp(argv[i], "--version") == 0) {
            mode = MODE_VERSION;
        } else if (strcmp(argv[i], "--backend=llvm") == 0) {
            /* LLVM is the only implemented backend. */
        } else if (strcmp(argv[i], "--backend=qbe") == 0) {
            fprintf(stderr, "Error: QBE backend is not implemented. Supported backend: llvm.\n");
            return EXIT_FAILURE;
        } else if (strcmp(argv[i], "--help") == 0 ||
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
                return 2;
            }
            output_name = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option '%s'.\n", argv[i]);
            print_usage(argv[0]);
            return 2;
        } else if (source_name == NULL) {
            source_name = argv[i];
        } else {
            fprintf(stderr, "Error: Multiple source files are not supported.\n");
            return 2;
        }
    }

    if ((mode == MODE_VERSION || mode == MODE_CAPABILITIES) &&
        (source_name != NULL || output_name != NULL)) {
        fprintf(stderr, "Error: This option does not accept source or output files.\n");
        return 2;
    }

    if (mode == MODE_VERSION) {
        print_version();
        return EXIT_SUCCESS;
    }

    if (mode == MODE_CAPABILITIES) {
        print_capabilities();
        return EXIT_SUCCESS;
    }

    if (source_name == NULL) {
        print_usage(argv[0]);
        return 2;
    }

    if (output_name != NULL && mode != MODE_COMPILE && mode != MODE_LLVM) {
        fprintf(stderr, "Error: -o is supported only in compile and backend-IR modes.\n");
        return 2;
    }

    if (!has_supported_extension(source_name)) {
        fprintf(stderr, "Error: Source filename must end in .cm or .c: %s\n",
                source_name);
        return EXIT_FAILURE;
    }

    if (!source_validate_limits(source_name)) {
        return EXIT_FAILURE;
    }

    if (mode == MODE_TOKENS) {
        return dump_tokens(source_name);
    }

    FILE *input = fopen(source_name, "r");

    if (input == NULL) {
        perror(source_name);
        return EXIT_FAILURE;
    }

    DiagnosticEngine diagnostics;
    diagnostic_engine_init(&diagnostics, source_name);

    AstNode *ast = NULL;

    fprintf(stderr, "[1/4] Parsing source...\n");

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

    fprintf(stderr, "[OK] Parsing successful.\n");

    if (mode == MODE_AST) {
        printf("\n========== ABSTRACT SYNTAX TREE ==========\n");
        ast_dump(ast, stdout);
        ast_free(ast);
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "[2/4] Performing semantic analysis...\n");

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

    fprintf(stderr, "[OK] Semantic analysis successful.\n");

    if (mode == MODE_SYMBOLS) {
        printf("\n========== SYMBOL TABLE ==========\n");
        semantic_dump_symbols(&semantic_model, stdout);

        semantic_model_destroy(&semantic_model);
        ast_free(ast);
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "[3/4] Generating Student IR...\n");

    IRProgram ir = {0};

    if (!ir_generate(ast, &ir)) {
        fprintf(stderr, "Error: Student IR generation failed.\n");

        ir_program_destroy(&ir);
        semantic_model_destroy(&semantic_model);
        ast_free(ast);

        return EXIT_FAILURE;
    }

    fprintf(stderr, "[OK] Student IR generation successful.\n");

    if (mode == MODE_IR) {
        printf("\n========== STUDENT IR ==========\n");
        ir_dump(&ir, stdout);

        ir_program_destroy(&ir);
        semantic_model_destroy(&semantic_model);
        ast_free(ast);
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "[4/4] Generating LLVM IR...\n");

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

    fprintf(stderr, "[OK] LLVM IR generation successful.\n");

    if (mode == MODE_LLVM) {
        if (output_name != NULL) {
            fprintf(stderr, "Output: %s\n", output_name);
        }

        ir_program_destroy(&ir);
        semantic_model_destroy(&semantic_model);
        ast_free(ast);
        return EXIT_SUCCESS;
    }

    if (mode == MODE_CHECK) {
        fprintf(stderr, "\nCompilation check successful.\n");

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

    fprintf(stderr, "Generating x86-64 assembly...\n");

    size_t assembly_temp_length =
        strlen(assembly_name) + sizeof(".tmpXXXXXX");

    char *assembly_temp = malloc(assembly_temp_length);

    if (assembly_temp == NULL) {
        fprintf(stderr, "Error: Could not allocate temporary output name.\n");
        unlink(temporary_name);
        free(generated_name);
        goto cleanup_failure;
    }

    snprintf(assembly_temp, assembly_temp_length,
             "%s.tmpXXXXXX", assembly_name);

    int assembly_fd = mkstemp(assembly_temp);

    if (assembly_fd < 0) {
        perror("mkstemp");
        free(assembly_temp);
        unlink(temporary_name);
        free(generated_name);
        goto cleanup_failure;
    }

    if (close(assembly_fd) != 0) {
        perror("close");
        unlink(assembly_temp);
        free(assembly_temp);
        unlink(temporary_name);
        free(generated_name);
        goto cleanup_failure;
    }

    if (!run_llc(temporary_name, assembly_temp)) {
        fprintf(stderr, "Error: LLVM llc failed.\n");
        unlink(assembly_temp);
        free(assembly_temp);
        unlink(temporary_name);
        free(generated_name);
        goto cleanup_failure;
    }

    if (!normalize_assembly(assembly_temp)) {
        unlink(assembly_temp);
        free(assembly_temp);
        unlink(temporary_name);
        free(generated_name);
        goto cleanup_failure;
    }

    if (rename(assembly_temp, assembly_name) != 0) {
        perror("rename");
        unlink(assembly_temp);
        free(assembly_temp);
        unlink(temporary_name);
        free(generated_name);
        goto cleanup_failure;
    }

    free(assembly_temp);
    unlink(temporary_name);

    fprintf(stderr, "[OK] Assembly generation successful.\n");
    fprintf(stderr, "Output: %s\n", assembly_name);

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
