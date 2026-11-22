#include "cminor/common/source_limits.h"

#include <stdio.h>
#include <stddef.h>

#include "cminor/common/diagnostic.h"
#include "cminor/lexer/lexer.h"

#define CMINOR_MAX_SOURCE_BYTES 32768
#define CMINOR_MAX_LINE_LENGTH 2048
#define CMINOR_MAX_TOKENS 4000
#define CMINOR_MAX_BLOCK_DEPTH 16
#define CMINOR_MAX_EXPRESSION_DEPTH 16

int source_validate_limits(const char *source_name)
{
    FILE *input = fopen(source_name, "rb");

    if (input == NULL) {
        perror(source_name);
        return 0;
    }

    size_t byte_count = 0;
    size_t line_length = 0;
    int character;

    while ((character = fgetc(input)) != EOF) {
        byte_count++;

        if (byte_count > CMINOR_MAX_SOURCE_BYTES) {
            fprintf(stderr,
                    "Error: Source exceeds the 32 KiB (32768-byte) limit.\n");
            fclose(input);
            return 0;
        }

        if (character == '\n') {
            line_length = 0;
        } else if (character != '\r') {
            line_length++;

            if (line_length > CMINOR_MAX_LINE_LENGTH) {
                fprintf(stderr,
                        "Error: Source line exceeds 2048 characters.\n");
                fclose(input);
                return 0;
            }
        }
    }

    if (ferror(input)) {
        perror(source_name);
        fclose(input);
        return 0;
    }

    rewind(input);

    DiagnosticEngine diagnostics;
    diagnostic_engine_init(&diagnostics, source_name);

    Lexer *lexer = lexer_create(input, source_name, &diagnostics);

    if (lexer == NULL) {
        fprintf(stderr, "Error: Could not initialize source validator.\n");
        fclose(input);
        return 0;
    }

    size_t token_count = 0;
    size_t block_depth = 0;
    size_t expression_depth = 0;
    int failed = 0;

    while (1) {
        Token token = lexer_next_token(lexer);

        if (token.kind == TOKEN_ERROR) {
            token_destroy(&token);
            failed = 1;
            break;
        }

        if (token.kind == TOKEN_EOF) {
            token_destroy(&token);
            break;
        }

        token_count++;

        if (token_count > CMINOR_MAX_TOKENS) {
            fprintf(stderr,
                    "Error: Source exceeds the 4000-token limit.\n");
            token_destroy(&token);
            failed = 1;
            break;
        }

        if (token.kind == TOKEN_LEFT_BRACE) {
            block_depth++;

            if (block_depth > CMINOR_MAX_BLOCK_DEPTH) {
                fprintf(stderr,
                        "Error at line %zu, column %zu: "
                        "Block nesting exceeds 16 levels.\n",
                        token.span.start.line,
                        token.span.start.column);
                token_destroy(&token);
                failed = 1;
                break;
            }
        } else if (token.kind == TOKEN_RIGHT_BRACE) {
            if (block_depth > 0) {
                block_depth--;
            }
        } else if (token.kind == TOKEN_LEFT_PAREN) {
            expression_depth++;

            if (expression_depth > CMINOR_MAX_EXPRESSION_DEPTH) {
                fprintf(stderr,
                        "Error at line %zu, column %zu: "
                        "Expression parenthesis nesting exceeds 16 levels.\n",
                        token.span.start.line,
                        token.span.start.column);
                token_destroy(&token);
                failed = 1;
                break;
            }
        } else if (token.kind == TOKEN_RIGHT_PAREN) {
            if (expression_depth > 0) {
                expression_depth--;
            }
        }

        token_destroy(&token);
    }

    lexer_destroy(lexer);
    fclose(input);

    return !failed;
}
