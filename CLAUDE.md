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

```bash
# Run the compiler (transpile a .weasel file to .cc)
./build/compiler/weasel_compiler input.weasel -o output.cc
```

## What This Is

Weasel is a **C++20 header-mostly library for type-safe, composable HTML generation**, plus a **transpiler** (`compiler/`) that converts CCX (C++ Components eXtended) — a JSX-like template syntax embedded in C++ — into pure C++ using the Weasel runtime. The name "weasel-lang" reflects a DSL-like API for building HTML trees in C++, not a programming language.

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

## Compiler / Transpiler (`compiler/`)

The compiler transpiles `.weasel` source files (C++ with embedded CCX markup) into plain C++ that uses the Weasel runtime.

**Pipeline**:
```
.weasel source → load_source() → transpile() → .cc output
                                     ├─ collect_components()  (find component declarations)
                                     ├─ driver::step()        (char-by-char: pass-through C++ or detect CCX)
                                     ├─ parse_element()       (recursive-descent CCX → ccx_node AST)
                                     └─ emit()                (ccx_node AST → C++ code)
```

**CCX syntax** — JSX-like markup inside C++:
- HTML elements: `<div class="foo">{expr}</div>`, self-closing: `<br />`
- Components (capitalized or declared with `component`): `<MyCard title={x} />`
- Control flow inside `{}`: `{if (cond) { <p>yes</p> }}`, `{for (...) { <li>{x}</li> }}`, `{while (...) { ... }}`
- `component Name(props)` declarations are rewritten to `weasel::node Name(props)`

**Compiler headers** (`compiler/include/weasel/compiler/`):
- `scanner.hpp` — stateful text scanner: position tracking, identifier/literal/comment readers
- `source.hpp` — `source_buffer`: file content + line-start offsets; `load_source()` / `make_source()`
- `ccx_parser.hpp` — `ccx_node` AST + `parse_element()` entry point
- `emitter.hpp` — `emit(ccx_node, ostream)`: AST → C++ code generation
- `transpiler.hpp` — `transpile(src, out, options)`: top-level orchestration; `collect_components()`

**Emitted code patterns**:
- HTML tag: `weasel::tag("div", {{"class","foo"}}, {child1, child2})`
- Component: `MyCard({.title = x})`
- Text: `weasel::text("hello")`
- Expression child: `weasel::text(expr)`
- If/for/while: wrapped in immediately-invoked lambdas returning `weasel::node` (for loops use `node_list` + `weasel::fragment`)

**Line preservation**: emitter pads output with newlines so generated `.cc` line numbers match the `.weasel` source.

## LSP Server (`lsp/`)

`build/lsp/weasel_lsp_server` — editor-agnostic LSP over stdio. v0.5 features:
- Parse diagnostics for CCX errors (surfaces `parse_error` from the compiler as LSP `Diagnostic`)
- Go-to-definition for `<ComponentName />` → `component ComponentName(...)` declaration (uses `collect_component_infos`)
- Completion of HTML tags, known components, and control-flow keywords inside CCX regions (uses `is_position_in_ccx`)
- On `didSave`, writes the `.cc` next to the `.weasel` — commit both, templ-style

C++ intelligence on the generated `.cc` is not proxied in v0.5; users rely on their own clangd setup. v1 will proxy to clangd via a subprocess.

**Layout** (`lsp/include/weasel/lsp/`):
- `jsonrpc.hpp` — Content-Length framing, JSON message read/write
- `document_store.hpp` — per-URI cache: text, components, CCX spans, parse diagnostics
- `server.hpp` — LSP method dispatch
- `features.hpp` — `build_completion`, `build_definition`, `build_diagnostics_payload`

Disable with `-DWEASEL_BUILD_LSP=OFF`. See `docs/editor-setup.md` for neovim/helix config.

## Tests

Tests use [Doctest](https://github.com/doctest/doctest) (auto-fetched by CMake):
- `tests/test_node.cpp` — node construction and variant behavior
- `tests/test_factory.cpp` — factory function output
- `tests/test_renderer.cpp` — HTML output correctness (escaping, void elements, nesting)
- `tests/test_context.cpp` — thread-local context behavior
- `tests/compiler/test_position.cpp`, `test_component_info.cpp`, `test_boundary.cpp`, `test_diagnostic.cpp` — compiler library internals used by the LSP
- `tests/lsp/test_lsp_server.cpp` — end-to-end LSP over stringstreams (initialize, didOpen, diagnostics, definition, completion)

To run a single test binary directly: `./build/tests/weasel_tests`
