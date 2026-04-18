# Editor setup for `.weasel` files

Weasel ships `weasel_lsp_server`, an editor-agnostic LSP. v0.5 provides:

- **Parse diagnostics** for CCX syntax errors
- **Go-to-definition** for `<ComponentName />` uses → `component ComponentName(...)` declaration
- **Completion** of HTML tags, known components, and control-flow keywords (inside CCX regions only)
- **On save**, the corresponding `.cc` file is written next to the `.weasel` (commit both)

C++ intelligence on the generated `.cc` (diagnostics, go-to-def across translation units, hovers) comes from your existing **clangd** setup — v0.5 does not proxy to clangd; just point clangd at the same project and it will pick up `.cc` files via `compile_commands.json`.

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

Clangd should be enabled separately for C++ files so it indexes the generated `.cc` alongside your `.weasel` edits.

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

Completion first asks the compiler `is_position_in_ccx(src, offset)` (see `compiler/include/weasel/compiler/boundary.hpp`). Inside a CCX region, you get HTML tags + declared components + control-flow keywords. Outside CCX, v0.5 returns nothing — rely on clangd for C++ completion.

## Regenerating `.cc` files

The generated `.cc` should stay in sync with its `.weasel`. The LSP writes the `.cc` on editor save; for batch work (touching the emitter, etc.):

```
cmake --build build --target weasel_regenerate
```

## What's not in v0.5

- Diagnostics / go-to-def / hover / completion for the C++ parts of `.weasel` (coming in v1 via a clangd subprocess proxy)
- Rename, find-references
- Incremental sync (server uses full sync for now)

See the implementation plan in the repo for what v1 adds.
