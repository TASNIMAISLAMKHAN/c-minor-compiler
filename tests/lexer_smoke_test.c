#include <stdio.h>
#include <stdlib.h>

#include "cminor/lexer/lexer.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <source-file>\n", argv[0]);
        return 1;
    }

    FILE *input = fopen(argv[1], "r");

    if (input == NULL) {
        perror("Could not open source file");
        return 1;
    }

    DiagnosticEngine diagnostics;
    diagnostic_engine_init(&diagnostics, argv[1]);

    Lexer *lexer = lexer_create(input, argv[1], &diagnostics);

    if (lexer == NULL) {
        fprintf(stderr, "Could not create lexer.\n");
        fclose(input);
        return 1;
    }

    printf("%-22s %-18s %s\n", "TOKEN", "LEXEME", "VALUE");
    printf("------------------------------------------------------\n");

    int token_errors = 0;

    while (1) {
        Token token = lexer_next_token(lexer);

        printf("%-22s %-18s ",
               token_kind_name(token.kind),
               token.lexeme != NULL ? token.lexeme : "");

        if (token.kind == TOKEN_INTEGER_LITERAL) {
            printf("%d", token.integer_value);
        }

        printf("\n");

        if (token.kind == TOKEN_ERROR) {
            token_errors++;
        }

        int finished = token.kind == TOKEN_EOF;

        free(token.lexeme);

        if (finished) {
            break;
        }
    }

    printf("\n");
    diagnostic_print_stats(&diagnostics);

    lexer_destroy(lexer);
    fclose(input);

    return (diagnostics.stats.error_count == 0 && token_errors == 0)
        ? 0 : 1;
}
