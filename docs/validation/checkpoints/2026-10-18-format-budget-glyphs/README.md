# Native budget retry and replacement-glyph acceptance

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

This checkpoint closes two bounded defects: an eligible native primary legacy baseline remaining blocked after budget relief, and unsupported characters rendering as literal question marks. It also retains the later browser pane-key acceptance separately. Product UI/calculations remain C99/raylib and DuckDB. No fonts, dependencies or second JavaScript UI were added.

| Gate | Tested source | Result | Seconds |
| --- | --- | --- | ---: |
| Native primary baseline budget retry | `5e9be01` |15 stages `867,882)`, optimized |10.368 App /12.464 total |
| Same budget retry | `5e9be01` |15 stages, O1 ASan/UBSan |25.653 /27.485 |
| Adjacent native legacy baseline | `5e9be01` |22 stages `[824,846)`, O1 ASan/UBSan |15.540 /17.305 |
| Hidden native review | `5e9be01` |400 stages per optimized/O1 configuration |18.301 /21.674 App |
| Integrated headless fonts | `7bed3bc` |602 metrics +1,715 actual-UI checks per optimized/O1 configuration |10.528 total, including8.012 build |
| Actual hidden GPU | `cca8081` |303 new glyph +110 preserved paint checks;477 background checks |2.186 GPU /4.221 total |
| Browser filename/history | Product `5e9be01`, runner `fc28f02` |1,035 checks /20 stages, transformed DPR2 |43.719 driver /46.249 browser /46.471 total |
| Private budget positive | `76cd486` |Same15-stage case, optimized |9.501 App /17.796 total |
| Controlled budget negative | `d997dd7` |Fails at870 after real Cell relief |3.569 /6.972 |
| Browser pane keys | Product `fe6f179`, tools `4f05e93` integrated there |540 checks, transformed DPR2 |42.057 driver /44.352 browser /44.483 total |
| Natural pane-key product negative | Product `5e9be01`, runner `4f05e93` |Fields Control+Z swallowed after282 checks |14.446 driver /17.155 total |
| Later native integration regression | `fe6f179` |400 stages per optimized/O1 configuration |17.709 /20.099 App |
| Signed local bundle | `fe6f179` |Source/signature verified, no launch |0.764 |

The budget/filename acceptance and corresponding normal/review Wasm manifests are scoped to `5e9be01`. The GPU follow-up at `cca8081` changes the paint fixture's clipped reference; it does not change production cropping. Pane-key `fe6f179` is a later product revision with separate four-configuration native builds, normal/review Wasm manifests, native review and production-exclusion evidence. Earlier official Worker192, source-format activation and canceled-active reservation proofs retain their existing source scopes. The maintained catalog is882 layout /1,315 App stages; this is not a fresh full-inventory pass.

## Budget denial and retry permission

The automatic compatibility baseline records typed quota denials separately from terminal failures. Only `SHEET`, `CURRENT`, `RETAINED` and `APP` admission statuses grant deferral. An unchanged document/sheet/source observation with unchanged Cell/Format revisions does not retry. A genuine revision change consumes one new permission, rechecks eligibility and admission, and then performs normal verification. Terminal exhausted revisions remain terminal even after an unrelated edit.

The actual15-stage App fixture forces **`FORMAT_BUDGET_APP`**, with491,520 current bytes and16,220,160 retained bytes. The denied attempt performs one Verify, no Collect or SQL, retains its source/pages/history, and cannot overwrite the original unverified workbook. A normal Cell edit releases Format Redo history; a separate normal Format edit releases that history through its own revision. Both paths then verify and publish the eligible primary baseline once. Undo/Redo, original finite styles, retained manual values and real Save/Open are checked, with no idle resubmissions. The final exhausted-format-revision case stays unverified and preserves destination bytes. [Optimized log (archived `runs/budget-release/layout-legacy-format-budget-retry.txt`), O1 log (archived `runs/budget-sanitize/layout-legacy-format-budget-retry.txt`).

The Cell relief (archived `images/budget-cell-retry.png`) and Format relief (archived `images/budget-format-retry.png`) frames are unchanged captures from the accepted O1 run `20261018T034606-affected-14860`; the optimized run used the same named output paths earlier. They are not relabeled as optimized captures.

The negative control patch (archived `controls/terminal-memo.patch`) deliberately restores the earlier terminal-observation latch on the positive implementation. It fails the actual retry requirement at870 after Cell revision/history relief. This is a **controlled regression**, not a natural run of an old released binary. Its summary (archived `runs/controlled-budget-red/summary.json`) retains the exact source/binary hashes.

Other typed quota categories are implemented but not separately forced by these15 App stages. Canceled-but-active Worker reservation retirement was not re-tested here. Accounting remains explicit category/reservation accounting, not allocator/RSS or a hard peak-memory claim. Universal legacy migration, mixed/Table/PIVOT history mapping, new RESULT commands and browser source identity remain open.

## Honest missing-glyph rendering

All five bundled Plex faces contain a genuine U+FFFD glyph. None contains `表` or `🦆`; JetBrains Mono does not supply them either. The existing primary atlas request adds only U+FFFD. Atlas validation checks the actual returned codepoint before publishing the family, because raylib's `GetGlyphIndex` otherwise silently returns `?`. Initial/custom-font failure remains a load failure; failed reloads preserve the complete previous family and NULL-mark font. No range expansion or dynamic per-frame rasterization/cache was introduced.

The headless checks link actual `ui.c` and real bundled CPU rasterization, with bounded texture/draw/window stubs. They verify all six failed temporary-face positions, retained-family retry, matching measurement/drawing for unsupported BMP/supplementary scalars, literal `?`, clipping and unchanged UTF-8. Those checks alone do not prove GPU output.

The separate real GPU gate uses six primary atlases at80/100/150% on the observed2× backing surface. Exact translated RGBA comparisons require Arabic/CJK/emoji unsupported scalars to match explicit U+FFFD and differ from literal `?`. Identically clipped unsupported/U+FFFD pairs also match exactly, retain nonzero ink and leave both outside backgrounds white. All18 face/scale records have zero glyph/clip differences and zero outside ink; replacement ink covers204–795 pixels per tile and differs from question-mark tiles at225–882 pixels. Atlas identities and retained paint allocation/bytes stay unchanged during draws; input UTF-8 is not rewritten. GPU manifest (archived `gpu/manifest.json`), measured log (archived `gpu/gpu.log`), 80% (archived `images/native-glyph-080.png`), 100% (archived `images/native-glyph-100.png`), 150% (archived `images/native-glyph-150.png`).

An earlier transient `fc28f02` fixture compared a CPU-cropped quad with an uncut reference. It observed nine inside-edge pixels at scale80%/face0, localx13 and y21/22/24–28/35/36, with RGB differences of at most1, alpha255 and no outside ink. The original log/PNG were overwritten, so no retained artifact/hash is claimed. CPU cropping recomputes quad/UV endpoints; the corrected fixture compares equally clipped primitives with **exact RGBA, no tolerance and no production cropping change**. This diagnostic is neither a natural missing-glyph product failure nor acceptance evidence.

The current renamed Table frame (archived `images/renamed-table.png`) visibly uses replacement marks while retaining the exact original UTF-8 filename in captured state. The real browser flow retains distinct five/three-row imports, rename/filter/query/import history, owned previews and zero cached-revisit jobs. Real CJK/emoji glyphs, broader script loading/shaping, physical DPI/IME and full international visual parity remain unfinished.

## Browser pane-key boundary

The later actual browser gate sends trusted Control/Meta history chords through real read-only Steps, Fields, Workbook and ribbon focus, with eight history variants. It retains exactly one sparse-cell action and zero history-only jobs. Fields search and the owned F2 draft keep local Undo/Redo. Inline SQL Copy uses only the pre-loader memory writer, with no OS clipboard fallback. Genuine F5 refresh and Shift+F11 new-sheet actions remain separate from the zero-job windows; primary+Alt+F toggles Fields for a selected empty Pivot without activating its Filters shelf. Browser-reserved O/S/W/N, physical input, native AppKit delivery, `.dxls` persistence and saved reports are excluded.

The natural old-product frame (archived `images/pane-keys-natural-red.png`) fails Fields Control+Z after Steps succeeds; it is separate from the controlled budget regression and transient clip comparison. The accepted empty-Pivot Fields frame (archived `images/pane-keys-pivot-fields.png`) belongs to the later540-check flow. Broad native review400 at `fe6f179` separately passes optimized/O1 in17.709/20.099s, with native/Wasm shipping exclusion. That is regression acceptance, not a new focused native pane-key inventory or physical AppKit delivery claim.

## Reproduction and retained evidence

```sh
python3 tools/test.py fast --modules fontmetrics ui_font_fallback --config both
python3 tools/test.py affected --feature legacy-format-budget-retry --modules fontmetrics --config release
python3 tools/test.py affected --feature legacy-format-budget-retry --modules fontmetrics --config sanitize
python3 tools/test_web_app.py --no-build --review --source tests/fixtures/transactions.csv --source-labels-only --dpr 2 --canvas-transform
python3 tools/test_web_app.py --no-build --review --source tests/fixtures/transactions.csv --pane-shortcuts-only --dpr 2 --canvas-transform
```

These commands preserve existing waits, exact completion ranges and private/headless desktop ownership. The GPU probe remains in the existing full tier; the focused run was executed by the single desktop owner under the shared lock, using the unchanged commands recorded in its manifest. No runtime/compiler was launched while curating this record.

`measurements.json` records source scopes, original paths/hashes, all18 GPU measurements, font asset hashes and controlled/transient failure distinctions. `browser-measurements.json` and `pane-key-measurements.json` retain selected actual state values and original hashes, omitting input streams. Native summaries/logs, build manifests and eight selected PNGs are copied unchanged. Generated databases, native report bundles, mutable fixtures and large input/event streams remain outside this checkpoint. `files.sha256.json` hashes retained files except itself.

The local signed bundle was refreshed to `fe6f179` with source/signature verification in0.764s and no launch; package.json (archived `package.json`) retains unsigned/signed binary hashes. The single owned8767 service serves the accepted review artifact `414e3ce0d0d6b8bf21fd` from that source. service.json (archived `service.json`) records the existing project/outbox, retained report IDs and zero reports before/after refresh. No user tab was reloaded and no native app was launched. This proves accepted artifact serving and queue retention, not a new end-to-end report-delivery run.
