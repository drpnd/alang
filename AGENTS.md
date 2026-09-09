# AGENTS.md for alang compilers

## Overview

This repository implements a self-hosting compiler of a data flow processing language, named `alang`.
It consists of two compilers; one is a bootstrap compiler written in C to compile the first stage with minimal fundamental features of alang, and the other is a self-hosting compiler implementing full features written in alang.


## Language design

The syntax and grammar in the Backus–Naur form (BNF) of alang are defined in `SYNTAX.md`.


## Implementation guidelines

THe directory structure is as follows:
- `bootstrap`: Bootstrap minimal compiler source codes written in C.
  - `bootstrap/arch`: ISA-specific code files.
- `src`: Self-hosting compiler unsig the bootstrap compiler, written in alang.
  - `src/compiler`: Compiler
  - `src/linker`

The supported architectures are the following:
- [aarch64](https://en.wikipedia.org/wiki/AArch64)
- [x86-64](https://en.wikipedia.org/wiki/X86-64)

The supported executable, object, and shared libraly binary formats are the followings:
- [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)
- [Mach-O](https://en.wikipedia.org/wiki/Mach-O)

At this stage, we implement the bootstrap compiler in C in the `bootstrap` directory.

The implementation guidelines for the bootstrap compiler are following:
- Use flex (`$(LEX)` for `Makefile`) as a lexical analyzer.
- Use yacc (`$(YACC)` for `Makefile`) as a compiler-compiler.

Then, follow the following instructions for the implementation:
- Create a `Makefile` for building and testing.
- 


## Build and test commands


## Code style guidelines

- C language based on GNU coding standards.
- alang's coding style to be defined later.

## Testing instructions

Tests to be defined later.

## Boundaries

- Ues the `tmp` directory as a working directory when creating temporary files.

