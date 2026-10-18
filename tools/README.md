# Development tools

These programs are not linked into the native application. Ordinary `make` builds the native C app; tests, browser builds, artwork generation and review services are separate commands.

| Files | Purpose / why retained |
| --- | --- |
| `create_design_workbook.c` | Native deterministic CSV/Parquet/workbook fixture and actual production PIVOT assertions. Historical filenames remain for test compatibility; no mock dependency. |
| `test.py`, `test_runner_test.py` | Process/fixture/desktop ownership, bounded focused/full tiers, sanitizer/completion/binary guards and cleanup; contract tests cover refusals. Candidate for a parity-tested C runner. |
| `test_build.py`, `test_review_build.py` | Incremental/atomic build and production-review exclusion contracts. |
| `native_review.py`, `review_queue.py`, `review_service.py`, `review_worker.py` | External report validation, durable queue, leases/recovery and optional agent dispatch. Services are not started by `make` or the product. |
| `web_toolchain.py`, `build_web*.py`, `web_object_cache.py`, `prepare_web_ci.py` | Isolated pinned SDK/headers/raylib, Wasm builds and incremental object cache. |
| `package_web_pages.py`, `test_web_pages.py` | Stage only production browser payload and check real browser import/filter at a deployment subpath. |
| `test_web_app.py`, `test_web_app.mjs`, `test_web_cdp.mjs`, `cdp_keyboard.mjs` | Own hidden Chromium lifecycle and drive the real C canvas/official Worker. The large MJS driver needs modularization; it is not another spreadsheet UI. |
| `generate_icons.py`, `render_ui_icons.m`, `check_macos_icon.m` | Offline checked-in icon derivatives and macOS artwork verification. Explicit generation uses Python standard-library tooling and macOS artwork APIs; native runtime does not. |
| `project_inventory.py` | Read-only classification/fingerprinting; output is ignored, never a tracked per-build manifest. |
| Other `*_test.py`, `*.test.mjs`, `test_*.py` | Tests of adapters, file/queue/process contracts or actual platform integration. |

Most product/engine/grid/codec assertions are already C. C99 alone does not supply portable processes, HTTP, filesystem locks, DOM or IndexedDB; moving orchestration requires explicit platform glue and equivalent failure tests. Keep the browser API boundary small and product semantics in C. Do not hide JS inside C strings to manufacture a language percentage.
