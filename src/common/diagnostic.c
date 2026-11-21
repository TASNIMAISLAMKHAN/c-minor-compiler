#include <stdio.h>
#include <stdarg.h>

#include "cminor/common/diagnostic.h"

void diagnostic_engine_init(
    DiagnosticEngine *engine,
    const char *source_name
) {
    if (engine == NULL) {
        return;
    }

    engine->source_name = source_name != NULL ? source_name : "<input>";
    engine->stats.error_count = 0;
    engine->stats.warning_count = 0;
}

void diagnostic_report(
    DiagnosticEngine *engine,
    DiagnosticLevel level,
    SourceSpan span,
    const char *format,
    ...
) {
    if (engine == NULL || format == NULL) {
        return;
    }

    const char *level_name = "note";

    switch (level) {
        case DIAGNOSTIC_NOTE:
            level_name = "note";
            break;

        case DIAGNOSTIC_WARNING:
            level_name = "warning";
            engine->stats.warning_count++;
            break;

        case DIAGNOSTIC_ERROR:
            level_name = "error";
            engine->stats.error_count++;
            break;
    }

    fprintf(
        stderr,
        "%s:%zu:%zu: %s: ",
        engine->source_name,
        span.start.line,
        span.start.column,
        level_name
    );

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fputc('\n', stderr);
}

void diagnostic_print_stats(const DiagnosticEngine *engine) {
    if (engine == NULL) {
        return;
    }

    fprintf(
        stderr,
        "%u error(s), %u warning(s)\n",
        engine->stats.error_count,
        engine->stats.warning_count
    );
}
