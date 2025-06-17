# Getting Started with Bloom

Welcome to Bloom, a modern, type-safe compiler backend written in C++20.
This guide will help you set up Bloom and build it from source.

## Prerequisites

Before you start using Bloom, please review the requirements and dependencies below.
This may save you some trouble by knowing ahead of time what hardware and software you will need.

### Hardware

| OS    | Architecture | Status    |
|-------|--------------|-----------|
| Linux | x86_64       | Supported |
| Linux | Aarch64      | Supported |
| macOS | Aarch64      | Supported |

### Software

| Component    | Version / Notes                       |
|--------------|---------------------------------------|
| CMake        | >= 3.30+                              |
| C++ Compiler | C++20 compatible                      |
| Ninja        | Optional, for faster builds           |
| GoogleTest   | Optional, for build with unit tests   |
| GoogleBench  | Optional, for build with benchmarking |

## Building from Source

#### Clone the Repository

```bash
git clone https://github.com/alpluspluss/bloom.git
cd bloom
```

#### Configure

```shell
mkdir build
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBLM_BUILD_EXAMPLES=OFF \
  -DBLM_BUILD_TESTS=OFF \
  -DBLM_BUILD_BENCHMARKS=OFF
```

#### Build

```bash
cmake --build build
```

## Build Options

Bloom supports several build options that can be configured when running CMake.

| Option                 | Description      | Default |
|------------------------|------------------|---------|
| `BLM_BUILD_BENCHMARKS` | Build benchmarks | `OFF`   |
| `BLM_BUILD_EXAMPLES`   | Build examples   | `OFF`   |
| `BLM_BUILD_TESTS`      | Build tests      | `ON`    |

For example, to build with examples enabled:

```bash
cmake -B build -DBLM_BUILD_EXAMPLES=ON
```

### Faster Builds with Ninja

For faster builds, it's recommended to use a generator that supports parallel builds,
such as Ninja. You can install Ninja via your package manager or through the
[official website](https://ninja-build.org/).

To configure Bloom with Ninja, simply run:

```shell
cmake -B build -G Ninja
ninja -C build
```

## Quick Start

This guide serves as a starting point for new users to get a quick overview of how to get started with Bloom.
The following sections provide more detail about the various components of Bloom and how to use them.

### Directory Structure

Bloom is organized into several directories, each serving a specific purpose.

#### `/examples`

This contains small projects that demonstrate how to use Bloom in various scenarios, including
lowering, custom optimization passes, code generation, and more.

Currently, this directory is empty but will be filled with examples in the future as the project
grows.

#### `/include`

This directory contains the public headers for Bloom. You can include these headers in your
project to use Bloom's functionality.

##### `/include/bloom/core`

The core directory contains fundamental components that form the backbone of Bloom:

- `context.hpp`: The central context for memory management and resource tracking
- `node.hpp`: Base IR node structure and definitions
- `types.hpp`: Unified type system for representing various data types
- `typed_data.hpp`: Type-safe union for handling diverse data values
- `db_info.hpp`: Debug information structures for source mapping
- `module.hpp`: Top-level container for compilation units
- `region.hpp`: Hierarchical structure for organizing code blocks
- `type_registry.hpp`: Type management, deduplication, and lookup system

##### `/include/bloom/ir`

The IR directory contains components for building and manipulating the intermediate representation.

- `builder.h`: Fluent interface for constructing IR nodes and graphs

##### `/include/bloom/solvers`

The solvers directory contains optimization and analysis frameworks.

- `analysis_pass.hpp`: Base class for analysis passes
- `pass.h`: Base classes for analysis and transformation passes
- `pass_context.hpp`: Context for passes, including data flow and analysis results
- `pass_manager.h`: Orchestration of passes with dependency handling
- `transform_pass.h`: Base class for transformation passes

##### `/include/bloom/support`

The support directory contains utility components.

- `allocator.hpp`: Custom pool-based memory allocation
- `string_table.hpp`: Efficient string interning system
- `conv.hpp`: Type conversion utilities

##### `/include/bloom/transform`

The transform directory contains standard passes for IR transformations.

- `cse.hpp`: Common Subexpression Elimination
- `dce.hpp`: Dead Code Elimination
- `alias_analysis.hpp`: Alias Analysis
- `pre.hpp`: Preliminary Analysis

#### `/lib`

The lib directory contains the implementation of the components declared in the include directory:

- `/lib/core`: Implementation of core components
- `/lib/ir`: Implementation of IR components
- `/lib/solvers`: Implementation of optimization passes
- `/lib/support`: Implementation of utility components

#### `/tests`

The tests directory contains unit tests for Bloom components:

- `/tests/core`: Tests for core components
- `/tests/ir`: Tests for IR components
- `/tests/solvers`: Tests for optimization passes
- `/tests/support`: Tests for utility components

## Links

Keep in mind that this is just the beginning. There are so many cool things
to explore and learn about Bloom. Here are some useful links for more information:

- [Bloom Homepage](https://alpluspluss.github.io/bloom/)
- [Examples](https://github.com/alpluspluss/bloom/tree/main/examples)
- [Contributing](https://github.com/alpluspluss/bloom/tree/main/CONTRIBUTING.md)