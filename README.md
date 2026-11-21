# C Minor Compiler

A compiler for the C Minor programming language, developed using C, Flex, and Bison, with a custom intermediate representation (Student IR) and an LLVM-based x86-64 backend.

## Project Overview

This project aims to implement a complete compiler pipeline, from source-code analysis to native executable generation.

## Compiler Architecture

The planned compilation pipeline is:

1. **Lexical Analysis** — Tokenization using Flex.
2. **Syntax Analysis** — Parsing using Bison.
3. **Abstract Syntax Tree (AST)** — Construction of the syntax tree.
4. **Semantic Analysis** — Type checking, scope management, and symbol table implementation.
5. **Student IR** — Development of a custom intermediate representation.
6. **LLVM IR Generation** — Translation from Student IR to LLVM IR.
7. **Machine Code Generation** — x86-64 assembly generation using LLVM.
8. **Assembly and Linking** — Executable generation using GNU Binutils and GCC.

## Technologies

- C
- Flex
- Bison
- GCC
- LLVM 23.1.3
- GNU Binutils
- Ubuntu on WSL2
- Visual Studio Code

## Development Status

- [x] Development environment setup
- [x] Backend toolchain verification
- [ ] Project structure
- [ ] Lexical analyzer
- [ ] Syntax analyzer and AST
- [ ] Semantic analysis and symbol table
- [ ] Student IR
- [ ] LLVM IR translation
- [ ] x86-64 code generation
- [ ] Compiler driver
- [ ] Testing and documentation

## Platform

Target platform: Linux x86-64 (System V AMD64 ABI).

## Author

TASNIMAISLAMKHAN

---

This project is being developed as part of a Compiler Construction course project.
