# Shared-C browser platform

Duxcel compiles the same C99/raylib App to Wasm. Top-level MJS modules are browser
platform/engine/review adapters, not a second spreadsheet UI. C owns layout,
controls, interactions and analytical definitions. Official asynchronous
DuckDB-Wasm Worker replies cross the bounded typed bridge into C-owned Pages.

| Location | Role |
| --- | --- |
| `host.mjs`, `browser_platform.mjs`, `canvas_*.mjs` | Startup, browser-scheduled frames, input and DPR/backing dimensions |
| `official_worker.mjs`, `engine_host.mjs`, `page_wire.mjs`, `codec_stream_transport.mjs` | Official engine/typed transport and persistence components |
| `clipboard.mjs`, `download.mjs` | Browser permission/lifetime boundaries |
| `host_review.mjs`, `review_*.mjs` | Development-only C review transport, excluded from production packages |
| `tests/*.test.mjs` | Node adapter/unit/contract tests |
| `tests/driver.mjs`, `tests/export_driver.mjs`, `tests/cdp*.mjs` | Owned hidden-browser integration drivers |
| `tests/support/` | Test-only probes/hosts and expected-value loaders; not production payload |
| `package*.json`, `toolchain.lock.json` | Existing pinned dependencies/toolchain; no floating upgrades |

```sh
npm ci --prefix web --ignore-scripts --no-audit --no-fund
python3 tools/web_toolchain.py bootstrap
python3 tools/build_web_raylib.py
make web
npm test --prefix web
```

The SDK/raylib caches live in `.cache/web/`; Wasm packages/objects live in
`build/web/`. After relocating an older installed SDK, run
`python3 tools/web_toolchain.py --offline bootstrap` to refresh absolute compiler
configuration. Existing pins/versions are retained. Native `make` needs no browser
SDK and starts no browser/server. Downloads occur only on explicit setup commands.

Browser tests use their own hidden Chromium process/profile, ephemeral origin
and shared desktop lock. They do not operate the user's browser. Test source
imports remain valid for Node; the test asset adapter rebases them into the flat
isolated Wasm package when served. Production packaging rejects test modules.

CSV/Parquet import, PIVOT and bounded operations use the real Worker. Complete
browser workbook Open/Save, recovery and full physical-input/browser coverage
remain unfinished; see `docs/STATUS.md` and `docs/ROADMAP.md`.
