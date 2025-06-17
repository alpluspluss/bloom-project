<div align="center">
  <img src="assets/logo.png" alt="Bloom Logo" width="35%">
  <h1>Bloom</h1>
  <p><i>A small, type-safe compiler backend in C++20</i></p>
  <p>
    <img src="https://github.com/alpluspluss/bloom/actions/workflows/build.yml/badge.svg" alt="Build & Unit Tests">
    <img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License">
    <img src="https://img.shields.io/badge/C%2B%2B-20-00599C.svg" alt="C++ Version">
  </p>
</div>

> ![NOTE]
> The project is discontinued in favor of a newer and better version, Arc.

This repository contains the source code for Bloom; a modern, type-safe and moderately
small compiler backend in C++20.

Bloom uses a graph-based intermediate representation to provide a flexible foundation for building optimizing
compilers with customizable passes. It is designed to be easy to use and comprehensible
while being fast and efficient.

## Why is this project discontinued

The project was meant to be production use for my hobby compiler stuffs, but I stopped working
on it due to the [technical debt](#technical-debt) it had, so I hope this becomes educational as a compiler backend.
It contains multiple algorithms for an optimizing compiler such as Dead Code Elimination, Common 
Subexpression Elimination, and Alias Analysis. I hope this repo is good enough for people 
to learn from and for myself.

## Status

The project is discontinued but contains implementations of many compiler optimization 
algorithms.

| Component           | Status     | Notes                                                                      |
|---------------------|------------|----------------------------------------------------------------------------|
| Core IR             | Complete   | Nodes, types, memory                                                       |
| Debug Information   | Complete   | Source mapping, variable tracking                                          |
| Pass Infrastructure | Complete   | Simple scheduling                                                          |
| Optimization Passes | Complete   | DCE, CSE, etc. See the [full list](#available-optimizers) below this table |
| DWARF               | Incomplete | Debug Information and Source Mapping                                       |
| Code Generation     | Incomplete | x86-64, Aarch64, RISC-V                                                    |
| Object File Format  | Incomplete | Mach-O, ELF, PE                                                            |

### Available Optimizers

- Aggressive Dead Code Elimination
- Alias Analysis
- Constant Propagation and Folding
- Common Subexpression Elimination
- Dead Code Elimination
- Dead Store Elimination
- Instruction Combining
- Loop Analysis
- Partial Redundancy Elimination [BROKEN]
- Reassociate
- Scalar Replacement of Aggregrates
- Superword-level Parallelism
- Callgraph Analysis
- Global Dead Code Elimination
- Function inlining
- Function specialization

## Technical Debt

The technical debt is mostly created by the type system which introduces multiple lookups
for non-trivial types such as structs and other user-defined types as well as a single node 
having over 100 bytes. This creates performance issues on a scale and makes certain tasks more 
redundant than it should be (e.g. target lowering). 
Personally, I find this unacceptable as it is an architectural failure and have 
moved on from this project to Arc, which is similar in nature to Bloom but with parallel 
compilation support out of the box without any synchronization and better type system.

## Design Details

Bloom uses a graph-based intermediate representation where computation is represented as
a directed acyclic graph (DAG) of nodes. Each Node represents an operation and maintains
dependencies on other nodes, nodes that depend on this node's result, the static type of the 
value produced, and metadata like linkage and optimization hints.

Type Registry centralizes type creation and ensures type identity. Complex 
types such as structs, functions, arrays are interned to avoid duplication, however
this creates the performance bottleneck mentioned.

Code is organized into **Regions** that form a tree hierarchy which can represent the program
in a structured way that preserves scopes and semantics. This also allows us to directly
compute dominance analysis on the fly without getting too expensive as well as having accurate
debug information mapping. 

Bloom features a two-tier pass infrastructure with automatic dependency tracking
and analysis result caching for both intra-procedural and inter-procedural optimizations.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE.txt) file for details.

## Contributing

This project is no longer actively maintained. Feel free to fork and learn from the implementation.
