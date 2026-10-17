# Web compiler and dependency preparation — 17 October 2026

This is W1 preparation, not a running Duxcel browser UI or DuckDB-Wasm bridge.

- The pinned Emscripten 6.0.12 installation was relocated from the owned worktree into `build/web/toolchain`; offline bootstrap rewrote its private absolute configuration and status succeeded without downloads. Existing Python 3.13.15 and Node 26.7.0 were reused. See `bootstrap.json` and `status.json`, with 11 fixture checks in `toolchain-tests.txt`.
- `compiler-smoke.c/.json` records an actual **C99** compile and Node execution, printing `C99-Wasm 9007199254740993`. The cold compile took **54.989 s**, including one-time generation of the private SDK sysroot/libraries. This proves compiler/runtime operation, not typed database transport or analytical semantics.
- `raylib-web.json` records the pinned raylib 6.0 WebGL ES2 static dependency build: **7.039 s**, six verified Wasm objects, **1,358,764 bytes**, SHA-256 `d6034610ffa1b129a9f4e3a5d5606bede8f1ba1a8ab32dfc12f77310c1cc4fe3`. Native Homebrew objects were not reused. The library and generated compiler products stay under ignored `build/web/`.
- A strict `-std=c99` dependency attempt failed because Emscripten's `EM_ASM` header explicitly requires GNU mode. The final dependency build uses upstream raylib's GNU99 setting. Duxcel's C code remains C99; no JavaScript spreadsheet UI was added. No upstream sources were modified. The log retains 11 existing audio dependency warnings (deprecated Emscripten version macros and a Vorbis pointer comparison); these were not hidden or treated as renderer acceptance.

The shared App still needs a persistent browser-scheduled frame callback and portable result/workbook/worker ownership seams. Browser rendering, real source import, DuckDB-Wasm execution, file persistence, exact-value bridge and browser review acceptance remain pending. Native production targets and their dependencies are unchanged by this preparation.
