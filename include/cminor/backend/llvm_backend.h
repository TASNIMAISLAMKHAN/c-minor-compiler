#ifndef CMINOR_LLVM_BACKEND_H
#define CMINOR_LLVM_BACKEND_H

#include <stdio.h>

#include "cminor/ir/ir.h"
#include "cminor/semantic/semantic.h"

/*
 * Translate Student IR into textual LLVM IR.
 *
 * Returns 1 on success, 0 on failure.
 */
int llvm_backend_emit(
    const IRProgram *program,
    const SemanticModel *semantic_model,
    FILE *output
);

#endif
