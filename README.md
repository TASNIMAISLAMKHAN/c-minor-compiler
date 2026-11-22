# C Minor Compiler

A compiler for the C Minor programming language, developed using C, Flex, and Bison. It implements a complete compilation pipeline with a custom Student Intermediate Representation (IR) and an LLVM-based x86-64 backend.

## 1. Project Overview

The compiler translates C Minor source code into native Linux x86-64 assembly and executable programs.

### Compilation Pipeline

1. Lexical Analysis using Flex.
2. Syntax Analysis using Bison.
3. Abstract Syntax Tree (AST) construction.
4. Semantic analysis, type checking, and symbol table management.
5. Custom Student IR generation.
6. LLVM IR generation.
7. x86-64 assembly generation using LLVM `llc`.
8. Assembly and linking using GCC and GNU Binutils.

## 2. Supported Language Features

C Minor is a subset of C, not a complete ISO C compiler.

Current supported features:

- Integer and void function return types.
- Local integer variables and function parameters.
- Arithmetic operations and comparisons.
- Logical operators with short-circuit evaluation.
- Variable declarations and assignments.
- `if` and `else` statements.
- `while` and `for` loops.
- `break` and `continue`.
- Function declarations and definitions.
- Direct function calls and recursion.
- Semantic analysis and symbol table generation.
- Student IR and LLVM IR generation.

## 3. Technologies

- C (C11)
- Flex
- GNU Bison
- GCC and GNU Make
- LLVM (`llc`)
- GNU Binutils
- Ubuntu on WSL2
- Visual Studio Code

## 4. Requirements

Install the required tools on Ubuntu or WSL2:

    sudo apt update
    sudo apt install build-essential flex bison llvm

Verify the installation:

    gcc --version
    make --version
    flex --version
    bison --version
    llc --version

Target platform: Linux x86-64 (System V AMD64 ABI).

## 5. Build the Compiler

Clone the repository and enter the project directory:

    git clone <repository-url>
    cd compiler-construction

Replace `<repository-url>` with the actual repository URL. If you already have the repository, skip the clone command and run the remaining commands from its directory.

Build the compiler:

    make

The compiler executable will be generated at `build/bin/cminor`.

## 6. Run the Compiler

Display help:

    ./build/bin/cminor --help

Display capabilities:

    ./build/bin/cminor --capabilities

Check a source file:

    ./build/bin/cminor --check examples/semantic_valid.cm

Dump tokens:

    ./build/bin/cminor --dump-tokens examples/lexer_smoke.cm

Dump AST:

    ./build/bin/cminor --dump-ast examples/semantic_valid.cm

Dump symbol table:

    ./build/bin/cminor --dump-symbols examples/semantic_valid.cm

Dump Student IR:

    ./build/bin/cminor --dump-ir examples/semantic_valid.cm

Generate LLVM IR:

    ./build/bin/cminor --emit-backend-ir examples/semantic_valid.cm -o build/output.ll

Compile to assembly:

    ./build/bin/cminor examples/semantic_valid.cm -o build/output.s

Assemble and link:

    gcc build/output.s -o build/program

Execute:

    ./build/program

The `semantic_valid.cm` example is expected to return exit status `5`.

## 7. Run Tests

Build and run all automated tests:

    make test

The test suite includes lexer, parser, semantic analysis, Student IR, and LLVM backend tests. The backend test also verifies LLVM object-file generation.

Remove generated build files:

    make clean

## 8. Project Structure

    compiler-construction/
    ├── Makefile
    ├── README.md
    ├── include/cminor/
    │   ├── ast/
    │   ├── backend/
    │   ├── common/
    │   ├── ir/
    │   ├── lexer/
    │   ├── parser/
    │   └── semantic/
    ├── src/
    │   ├── ast/
    │   ├── backend/
    │   ├── common/
    │   ├── ir/
    │   ├── lexer/
    │   ├── parser/
    │   ├── semantic/
    │   └── main.c
    ├── tests/
    ├── examples/
    ├── docs/
    └── build/

Generated files and compiled binaries are stored in `build/`.

## 9. Development Status

- [x] Project structure
- [x] Lexical analyzer
- [x] Syntax analyzer and AST
- [x] Semantic analysis and symbol table
- [x] Student Intermediate Representation
- [x] LLVM IR translation
- [x] x86-64 assembly generation
- [x] Compiler driver and command-line interface
- [x] Makefile build system
- [x] Automated tests
- [x] Project documentation

---

Developed as part of a Compiler Construction course project.

**Author:** TASNIMAISLAMKHAN
