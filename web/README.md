# Shared C99 browser proof

This directory pins the tools for compiling Duxcel's existing C99/raylib UI.
The bootstrap installs a compiler and raylib source. The shared App now runs
through a browser-scheduled tick, with owned hidden Chromium interaction evidence.
The official DuckDB-Wasm source-query bridge and asynchronous CSV/Parquet import are now running. No alternate JavaScript UI or
custom combined DuckDB build is introduced.

From the checkout root:

```sh
python3 tools/web_toolchain.py bootstrap
python3 tools/web_toolchain.py status
python3 tools/web_toolchain.py run emcc --version
python3 tools/web_toolchain_test.py
```

The default install is `build/web/toolchain`, already covered by the project's
ignored `build/` tree. `--root build/web/another-toolchain` selects another
isolated configuration in the same checkout. External roots and directory
symlink redirection are rejected. Each worktree has its own writable toolchain.

An existing Python ≥3.10 and Node ≥18.3 are required. The bootstrap searches the
current Python and existing versioned Python executables, and uses the current
Node. Select explicit existing executables when needed:

```sh
python3 tools/web_toolchain.py --python /opt/homebrew/bin/python3.13 bootstrap
python3 tools/web_toolchain.py --offline --python /opt/homebrew/bin/python3.13 bootstrap
```

Only `bootstrap` downloads. `--offline bootstrap` requires the pinned archives
and an already installed compiler; it never asks emsdk to retrieve missing
components. `status` checks locally without network. `run` wraps the installed
`emcc`, `em++`, `emar`, `emranlib`, `emmake` or `emcmake`; arguments after the tool
are passed directly, with optional `--`. Later compiler port/cache preparation
may require downloads, so `--offline run` is rejected.

The subprocess environment sets `EM_CONFIG`, `EM_CACHE`, `EMSDK`,
`EMSDK_PYTHON` and `EMSDK_NODE` to the local configuration and existing runtimes.
It preserves the caller's PATH and HOME, removes inherited Emscripten overrides,
and caps `EMCC_CORES`, `EMSDK_NUM_CORES` and `BINARYEN_CORES` at one. It never
sources `emsdk_env.sh`, activates a global SDK, edits shell startup files or
installs Node/Python/npm packages. Compiler configuration and successful install
provenance are stored in `.emscripten` and `state.json` under the install root.

The frozen emsdk driver installs only its `releases-<release_hash>-64bit` tool
from a preverified archive. Source archives are safely staged before publication;
checksums are verified on download and before cached reuse. Failed downloads
remove their owned temporary file. An interrupted bootstrap may leave
`.bootstrap-lock`; inspect that no bootstrap owns the directory before removing
that lock. Changed source pins require a new isolated root rather than replacing
an existing source tree.

## Pins and primary evidence

[toolchain.lock.json](toolchain.lock.json) records immutable source commits,
archive URLs and measured SHA256 checksums, plus exact browser package versions
and registry SHA512 integrity. These are explicit pins, not floating `latest`
lookups during bootstrap.

- [Emscripten emsdk 6.0.12](https://github.com/emscripten-core/emsdk/tree/6.0.12):
  driver commit `35ff8a6d150541276abbc6bae512ca90bcfbe220`, release build
  `6e9f6885cb17a3d6c8fd488a5cff730f50aba61d`. The frozen driver's
  [release map](https://github.com/emscripten-core/emsdk/blob/35ff8a6d150541276abbc6bae512ca90bcfbe220/emscripten-releases-tags.json)
  and [manifest](https://github.com/emscripten-core/emsdk/blob/35ff8a6d150541276abbc6bae512ca90bcfbe220/emsdk_manifest.json)
  identify the official binary artifact. This lock currently includes macOS
  arm64 only. Other hosts fail clearly; their installation is unvalidated.
- [raylib 6.0](https://github.com/raysan5/raylib/releases/tag/6.0): commit
  `dbc56a87da87d973a9c5baa4e7438a9d20121d28`, matching the current native
  6.0.0 API. The new 6.2 release is not an implicit renderer upgrade.
  [Official web build guidance](https://github.com/raysan5/raylib/wiki/Working-for-Web-%28HTML5%29)
  is reference for subsequent integration; this bootstrap does not build raylib.
- [Official DuckDB-Wasm 1.32.0 registry record](https://registry.npmjs.org/@duckdb/duckdb-wasm/1.32.0)
  and [Arrow 17.0.0 record](https://registry.npmjs.org/apache-arrow/17.0.0):
  Arrow satisfies the package's `^17.0.0` dependency. When checked on
  17 October 2026, npm's DuckDB-Wasm `latest` tag was prerelease-named
  `1.33.1-dev57.0`; stable published 1.32.0 is pinned deliberately. The intended
  bridge uses the [official asynchronous Worker API](https://duckdb.org/docs/stable/clients/wasm/instantiation.html),
  initially its single threaded bundles. Package pins do not establish the
  embedded engine version; the Worker checkpoint obtains that from the running engine.

Browser packages are declared for the later bridge and are not downloaded or
installed by this bootstrap. Their full transitive packaging lock belongs with
that integration. No browser computation, exact-value transport or engine/UI
parity is claimed by toolchain setup.

## Local setup evidence — 17 October 2026

Validated on this Mac, arm64, in
`/Users/h2o/.codex/worktrees/grid-active-selection/duxcel/build/web/toolchain`:

| Item | Result |
| --- | --- |
| Driver archive | 177,386 bytes downloaded in 0.777 s |
| raylib source archive | 52,562,578 bytes downloaded in 6.045 s |
| Official SDK binary archive | 280,157,468 bytes downloaded in 51.565 s |
| Existing runtimes used | Python 3.13.15; Node 26.7.0 |
| Actual compiler version | emcc 6.0.12, Emscripten commit `5488e0871d46d19a3209988190fa6a653126256f` |
| Entire local footprint, retaining archives | 1,974,228 KiB, about 1.88 GiB (`du -sk`) |
| Cached offline bootstrap | 0.612 s; 32,849,920-byte child peak RSS; zero downloads |

The three archives were retrieved from the lock's official URLs and checksummed
before running bootstrap. Consequently the subsequent install correctly reports
`downloaded_bytes: 0`: it reused that cache. The frozen installer logged
`already downloaded, skipping`, unpacked its compiler-only tool and completed.
`python3 tools/web_toolchain.py run emcc --version` succeeded, as did a later
`--offline bootstrap`; all six wrapped tool scripts exist. These are setup
measurements. Initial unpack time/peak RSS, compilation and application startup
were not measured in this lane.

Offline fixture tests cover pins, private paths, checksum/network errors, partial
cleanup, safe extraction, runtime/version rejection, environment isolation,
compiler-only install, failed/offline install and independent local status.
The installed ignored cache can be moved intact into another checkout's
`build/web/toolchain` while unused; run offline bootstrap there to rewrite its
absolute configuration before use. The SDK/cache is not a committed artifact.

## Main checkout integration

The owned cache has been relocated into `/Users/h2o/dev/duxcel/build/web/toolchain`,
and offline bootstrap/status passed after rewriting its absolute configuration.
The same C99 compiler was also used for an actual Wasm/Node smoke program.

```sh
python3 tools/build_web_raylib.py
```

This builds the pinned WebGL ES2 dependency in a private copied source directory
under `build/web/raylib/`, using one compiler process and the local SDK. The final
archive is published atomically only after all object formats are verified as
Wasm. It preserves the SDK's pinned source tree and native application outputs.
Emscripten's `EM_ASM` macros require GNU mode, so this third-party adapter uses
raylib's upstream GNU99 setting. Duxcel code continues to use C99. Existing
upstream audio warnings are retained in `build/web/raylib/build.txt`.

[Compiler/dependency evidence](../docs/validation/checkpoints/2026-10-17-web-toolchain/)
records timing, sizes and hashes. The shared App renderer is now implemented; the official
DuckDB-Wasm Worker source bridge and file import are implemented.

## Shared App renderer

```sh
make web
python3 tools/test_web_app.py --no-build
```

`make web` publishes immutable, hashed outputs under `build/web/app/<signature>/`
and records the current directory in `build/web/app/latest.json`. Serve that
directory with a local HTTP server; opening HTML via `file:` is unsupported.
The test command acquires the shared desktop lock, serves the exact artifact on
an ephemeral loopback port, and starts installed Chromium in headless mode with
a fresh temporary profile. It never uses a user's browser/profile or activates
a window. Set `DUXCEL_CHROMIUM` to an existing compatible executable if needed.

This checkpoint contains the real C UI, sparse manual cells and official asynchronous
DuckDB-Wasm source pages/count/SQL validation/preview, paged available values and date hierarchy. Open imports CSV/TSV/Parquet
through the browser file chooser (Ctrl+O in this proof). Save/relink, workbook
persistence and further analytical operations remain explicitly unavailable. Native production still uses native DuckDB and Cocoa.
Current evidence is DPR 1; Retina/clipboard/IME/physical input and full browser
parity are separate gates. See the [renderer checkpoint](../docs/validation/checkpoints/2026-10-17-shared-browser/).

The local browser packages are installed with `npm ci --ignore-scripts --no-audit --no-fund`
in `web/`, using the committed lock. The packager copies only same-origin runtime
assets/licenses and records hashes; the accepted engine is the official single-threaded
EH bundle, requiring WebAssembly exception handling. Tested Chromium 150 reports
engine v1.4.3 from package 1.32.0. The MVP bundle's SQL error behavior failed acceptance
and is not packaged. Native distribution is unaffected.

Run `make test-browser-core` for the fast release/sanitizer/transport gate.
`python3 tools/build_web.py --worker-driver` builds a separate no-window C acceptance
artifact; `python3 tools/test_web_app.py --no-build --worker-driver` exercises the
full real-engine bridge. Source UI gates and current limits are in
[TESTING.md](../docs/TESTING.md) and the [latest filter/active-query checkpoint](../docs/validation/checkpoints/2026-10-17-browser-filters/).

`make web-review` produces a separate development artifact under `build/web/review/`.
It reuses C capture/registry/picking/composition and prepares a single report archive
download through the review-only browser adapter. Command/Ctrl+Shift+. starts it.
No review host, registry or callback export is packaged with the normal target.
`tools/test_web_review_build.py` verifies the actual compiled boundary; see
[browser reporting acceptance](../docs/validation/checkpoints/2026-10-17-browser-review/).
