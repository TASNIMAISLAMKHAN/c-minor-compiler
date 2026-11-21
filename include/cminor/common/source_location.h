#ifndef CMINOR_SOURCE_LOCATION_H
#define CMINOR_SOURCE_LOCATION_H

#include <stddef.h>

/*
 * Represents a single position in the source file.
 *
 * line and column are 1-based.
 * offset is a 0-based byte position.
 */
typedef struct {
    size_t line;
    size_t column;
    size_t offset;
} SourceLocation;

/*
 * Represents a range of source code.
 * start is the first position.
 * end is the position immediately after the range.
 */
typedef struct {
    SourceLocation start;
    SourceLocation end;
} SourceSpan;

#endif
