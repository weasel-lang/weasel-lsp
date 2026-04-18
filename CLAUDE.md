# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test

```bash
# Configure (from repo root)
cmake -B build

# Build
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run example
./build/examples/task_list_example
```

CMake options (both ON by default): `-DWEASEL_BUILD_TESTS=OFF`, `-DWEASEL_BUILD_EXAMPLES=OFF`

## What This Is

Weasel is a **C++20 header-mostly library for type-safe, composable HTML generation**. The name "weasel-lang" reflects a DSL-like API for building HTML trees in C++, not a programming language.

## Architecture

The library is built around a `std::variant`-based node tree:

```
node = std::variant<std::monostate, text_node, raw_node, element_node, fragment_node>
```

**Core headers** (`include/weasel/`):
- `node.hpp` — defines the five node types and the `node` variant alias
- `factory.hpp` — builder functions: `text()`, `raw()`, `tag()`, `fragment()`
- `renderer.hpp` / `src/renderer.cpp` — converts node trees to HTML strings via `std::visit` visitor pattern
- `context.hpp` — thread-local context provider template (generic, for passing state through component trees without explicit arguments)
- `weasel.hpp` — convenience umbrella include

**Rendering**: The renderer recurses through the node tree using `std::visit`. `text_node` content is HTML-escaped; `raw_node` is passed verbatim; `element_node` renders tag, attributes, and children (void elements get no closing tag); `fragment_node` renders children with no wrapper.

**Component pattern**: See `examples/task_list.cpp` — components are plain C++ functions returning `node`, accepting props structs. No framework magic.

## Tests

Tests use [Doctest](https://github.com/doctest/doctest) (auto-fetched by CMake):
- `tests/test_node.cpp` — node construction and variant behavior
- `tests/test_factory.cpp` — factory function output
- `tests/test_renderer.cpp` — HTML output correctness (escaping, void elements, nesting)
- `tests/test_context.cpp` — thread-local context behavior

To run a single test binary directly: `./build/tests/weasel_tests`
