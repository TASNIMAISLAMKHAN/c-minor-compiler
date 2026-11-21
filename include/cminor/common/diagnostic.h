#ifndef CMINOR_DIAGNOSTIC_H
#define CMINOR_DIAGNOSTIC_H

#include <stdarg.h>
#include "cminor/common/source_location.h"

typedef enum {
    DIAGNOSTIC_NOTE,
    DIAGNOSTIC_WARNING,
    DIAGNOSTIC_ERROR
} DiagnosticLevel;

typedef struct {
    unsigned int error_count;
    unsigned int warning_count;
} DiagnosticStats;

typedef struct {
    const char *source_name;
    DiagnosticStats stats;
} DiagnosticEngine;

void diagnostic_engine_init(DiagnosticEngine *engine,
                            const char *source_name);

void diagnostic_report(DiagnosticEngine *engine,
                       DiagnosticLevel level,
                       SourceSpan span,
                       const char *format, ...);

void diagnostic_print_stats(const DiagnosticEngine *engine);

#endif
