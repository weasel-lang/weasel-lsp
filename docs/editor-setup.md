# Editor setup for `.weasel` files

Weasel ships `weasel_lsp_server`, an editor-agnostic LSP. Features:

- **Parse diagnostics** for CCX syntax errors (from the weasel transpiler)
- **Go-to-definition** for `<ComponentName />` uses → `component ComponentName(...)` declaration
- **Completion** of HTML tags, known components, and control-flow keywords (inside CCX regions only)
- **On save**, the corresponding `.cc` file is written next to the `.weasel` (commit both)
- **C++ intelligence** via a clangd subprocess: diagnostics, go-to-def for C++ symbols, hovers, C++ completion outside CCX regions. Results are remapped from the generated `.cc` back to `.weasel` coordinates.

## Build

```
cmake -B build
cmake --build build
```

The binary lands at `build/lsp/weasel_lsp_server`.

## Neovim (nvim-lspconfig)

```lua
-- ~/.config/nvim/init.lua
vim.filetype.add { extension = { weasel = "weasel" } }

local lspconfig_configs = require "lspconfig.configs"
if not lspconfig_configs.weasel then
    lspconfig_configs.weasel = {
        default_config = {
            cmd = { "/abs/path/to/weasel_lsp_server" },
            filetypes = { "weasel" },
            root_dir = require("lspconfig.util").root_pattern("CMakeLists.txt", ".git"),
            settings = {},
        },
    }
end
require("lspconfig").weasel.setup {}
```

Clangd can still be enabled separately for `.cc` / `.cpp` files. The weasel LSP only manages `.weasel` files; the clangd you configure for C++ files is independent of the one weasel spawns as a subprocess.

## Helix

`~/.config/helix/languages.toml`:

```toml
[[language]]
name = "weasel"
scope = "source.weasel"
file-types = ["weasel"]
roots = ["CMakeLists.txt", ".git"]
language-servers = ["weasel-lsp"]

[language-server.weasel-lsp]
command = "/abs/path/to/weasel_lsp_server"
```

## How the server decides what to complete

Completion first asks the compiler `is_position_in_ccx(src, offset)` (see `compiler/include/weasel/compiler/boundary.hpp`).

- **Inside a CCX region**: weasel returns HTML tags + declared components + control-flow keywords. C++ completion from clangd is suppressed for these positions (the column mapping into the generated `.cc` is ambiguous inside CCX).
- **Outside CCX** (plain C++ regions): weasel translates the cursor position into the `.cc` and forwards the request to clangd, then returns clangd's results with the `.weasel` URI.

## clangd subprocess proxy

On `initialize`, the server looks for `clangd` on your `PATH` (override with env var `WEASEL_CLANGD_PATH`) and spawns it pointing at `build/compile_commands.json` (falls back to auto-detection). If clangd isn't available, the server logs a message and falls back to v0.5 behavior (parse diagnostics + component go-to-def + CCX completion).

Generated `.cc` content is pushed to clangd via `didOpen` / `didChange` — clangd never reads stale disk. On `didSave`, weasel writes the `.cc` to disk and notifies clangd.

Clangd `publishDiagnostics` on a `.cc` are remapped to the originating `.weasel` URI. Line offsets within a `cpp_passthrough` region are preserved exactly; diagnostics that land inside a `ccx_region` collapse to the full source line of the CCX entry and the message is prefixed `[in CCX]` (column info inside CCX isn't meaningful in the original source).

To disable the subprocess (useful for tests or if it misbehaves): `export WEASEL_LSP_NO_CLANGD=1`.

## Regenerating `.cc` files

The generated `.cc` should stay in sync with its `.weasel`. The LSP writes the `.cc` on editor save; for batch work (touching the emitter, etc.):

```
cmake --build build --target weasel_regenerate
```

## What's not in v1

- Rename, find-references
- Incremental document sync (server uses full sync for now)
- Precise column remapping inside CCX regions (coarse line-only remap today)
