# Development automation

Tools orchestrate processes, filesystem/network work, build artifacts and review
reports. Use Python standard-library code for new orchestration. Do not implement
spreadsheet calculations or duplicate the C product model here. Tool tests live
under `tests/` in this subsystem; no session-specific scripts or payloads remain
beside the reusable tools.

| Files | Responsibility / interface |
| --- | --- |
| `workspace.py` | Output/cache/input retention, build-local operation locks, OS-temp cleanup gate and source/binary fingerprints |
| `create_workbook.py` | Python synthetic data/Parquet/fixed workbook generation via the existing DuckDB CLI; no C generator |
| `test.py` | Focused/native/full test orchestration; process exits/completion, sanitizer diagnostics and fixture ordering |
| `test_app_lifecycle.py`, `test_native_review.py` | Hidden native workflow drivers through compiled C binaries and bounded reports |
| `test_open_documents.py` | Real hidden LaunchServices cold/warm workbook Open in a private bundle; test-only native-alert rejection with negative control, exact framebuffer comparison, source-file retention and owned-process/registration cleanup |
| `test_review_build.py`, `test_web_review_build.py` | Actual build/artifact isolation checks; self-test cases live in `tools/tests/` |
| `build_web*.py`, `web_toolchain.py`, `web_object_cache.py`, `web_raylib_adapter.py`, `prepare_web_ci.py` | Pinned compiler/raylib, shared-C Wasm builds and object caching |
| `package_web*.py`, `test_web_pages.py` | Stage/verify production browser packages and deployment paths |
| `test_web_app.py`, `test_web_export.py`, `browser_test_assets.py` | Own hidden browser/server lifetime and serve the test modules from `web/tests/` |
| `native_review.py`, `review_queue.py`, `review_service.py`, `review_worker.py` | External development-only reports, durable queue, leases and optional agent dispatch |
| `download_test_data.py` | Explicit public-data download; optional CLI conversion generates derivatives in build, originals survive clean |
| `generate_icons.py`, `render_ui_icons.m`, `check_macos_icon.m` | Offline artwork derivatives and native AppKit artwork integration; Objective-C is the macOS API boundary |
| `project_inventory.py` | Read-only local file classification/fingerprints; output stays ignored |

## Language and runtime boundary

- Native product invariants and assertions linking product APIs are C99 under
  `src/` or `tests/`. Synthetic fixture generation is Python; the native fixture
  test is `tests/test_fixture.c`.
- Python launches those programs and consumes exits/logs/versioned reports. It
  does not load a second model or call Python from the shipping native app.
- Browser API/Worker glue is JavaScript under `web/`. Browser tests are under
  `web/tests/`; no JavaScript spreadsheet UI or Node source belongs in tools.
- Objective-C is used when a native macOS API is needed.
- `__pycache__/` is ordinary ignored Python bytecode. It is regenerated, never
  source or a report, and never a reason to preserve an obsolete script.

Do not rewrite a program into another language only to change a GitHub percentage.
An exception to these roles needs a concrete API/platform/performance reason,
a named owner and a testable interface. Preserve meaningful existing coverage.

## Test commands

`python3 -m unittest discover -s tools/tests -t .` runs the tool contracts.
`make test-build` separately executes the real miniature Make/build graph.
`make test-fixture` runs the real C/DuckDB helper in both configurations.
`npm test --prefix web` runs browser adapter/contract unit tests.

The one test-package bootstrap mirrors standalone CLI imports; no product code
imports it. Normal `make` builds the native app and starts no service or agent.

`test_journal_process.py --binary build/tests/test_workbook_journal` owns the
native journal process-kill boundary checks. It starts only its private C helper,
waits for explicit pre/post-COMMIT barriers, kills that process and reopens its
own temporary databases to assert exact old/new records and idempotent retries.
The `workbook-journal` feature gate runs it serially for each already-built
optimized/sanitizer engine binary and verifies binary fingerprints. It never
starts or addresses a desktop application.
