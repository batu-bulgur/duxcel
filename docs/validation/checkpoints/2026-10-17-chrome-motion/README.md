# Unified chrome and canvas motion — 17 October 2026

Native code through `3cc8585`, focused browser input policy through `75f104d`, and browser harness through `91f480f`. Each copied manifest retains its actual source/binary identity. Later test-only commits do not change the native program. This is focused acceptance, not a full regression or final exact-prototype sign-off.

## Implemented behavior

- macOS uses full-size transparent title content, with native title text hidden. Its native traffic lights, cached logo, Duxcel name, document title and Save occupy one visual strip. The C title starts after the actual converted traffic-light extent (81 logical points here), rather than an assumed Retina multiplier. Pointer coordinates now convert from the actual renderer view. Native window dragging excludes Save and modal/review input; double-click follows the system Zoom/Minimize/None preference.
- Native controls retain stable AppKit size/position at independent C interface scales. An attempted manual reposition was removed: AppKit relayout restores those frames, which would produce jitter. The hidden test verifies their stable geometry, title-content extent, resize, framebuffer pixels and nonactivation at 80/100/150% and Focus. GL captures exclude AppKit-drawn traffic lights; native-view geometry, not an invented GL rendering, establishes their placement.
- Ribbon display is a compact chevron at the ribbon's lower-right corner. Full mode uses the footer; Compact/Icons reserve another 32 logical units, keeping history/More and commands separate. Menus anchor to the actual control. Tabs gain the space formerly occupied by the top-row label.
- Keytips use light fill, blue-gray outline, navy text, bounded shadows and a shared 140ms transition in/out. Reduced motion remains immediate. Escape now dismisses a bare-Alt hint state even when the grid, rather than the ribbon, owns keyboard focus.
- Elastic pan preserves inward velocity under tiny momentum tails. Resistance composes independently of event packet size, uses CSS-sized limits through zoom, and integrates elapsed time rather than losing long frames. Opposite movement consumes the exposed distance before moving the sheet. Precise horizontal pan grows the virtual columns only when the grid owns the pointer.
- Exposed canvas backing is RGB 201/211/220; the displaced row1/A boundaries use RGB 143/162/179, including the column header and row gutter. Canonical position and cell selection remain unchanged during return. There are no new UI dependencies or idle database queries.

## Focused acceptance

| Gate | Result | Elapsed |
| --- | --- | --- |
| Pure grid/ribbon, release and ASan/UBSan | 22,000 checks each, including1,186 focused overscroll checks | 1.277s total |
| Hidden titlebar platform/GPU helper, ASan/UBSan | 1,937 checks; 80/100/150%, Focus, hidden resize, real before/after-swap pixels | 3.488s |
| Hidden native active-cell, ASan/UBSan | 41 application stages plus477 background checks | 23.002s total |
| Hidden native reporting, ASan/UBSan | 37 stages, actual frame bundles/reader, title/ribbon/disabled/narrow targets;477 background checks | 2.966s App;5.035s overall |
| Adjacent narrow-ribbon profile | 18 stages with Compact/Icons/Focus and477 background checks; same C ribbon implementation, before the final edge-color change | 12.908s total |
| Current optimized native App | 6 basic stages plus477 background checks | 4.839s total |
| Owned headless shared-C browser, DPR2 and inset CSS canvas | 116 checks: actual GL spring/edges/hint fades, guarded DOM keytips, precise wheel/DPR/zoom, real CSV filters/date hierarchy,12 review picks | 27.356s total |
| Shipping Wasm App |Normal manual navigation/input/render gate | 7.978s total |
| Focused browser-input pure tests | 8 groups, including bare Alt, composition/AltGraph/OS chords and focus/lifecycle boundaries | 0.085s |
| Shipping isolation |Current linked native and Wasm review absence passed |See manifests |

The browser sampled a 49.5-logical-point pull on both axes. It decreased monotonically through 44,34.5,25.5,17.5 and 12 points, fell to 0.5 by 167ms and to the framebuffer baseline by 183ms; it stayed there through 367ms. The measured boundaries were exactly RGB 143/162/179 against white paper and the darker backing. Peak PNG encoding is a separate pull after the timing sampler, so PNG readback does not inflate the measured return. This is a captured synthetic gesture, not a physical trackpad latency benchmark.

Native [base capture](native-base.png) includes a real query in flight. Browser [light keytips](keytips-light.png), [stretched canvas](overscroll-stretched.png) and [ribbon menu](ribbon-display.png) use the actual shared C renderer. The complete state-by-state UI port remains open.

## Browser Alt limitation

Two trusted CDP Alt sequences crashed the installed Chromium 150.0.7871.46 on macOS 26.6.2 with SIGSEGV in AppKit menu/key-equivalent routing. The focused canvas's new bare-Alt default guard did **not** eliminate that native headless-browser crash. Failed summaries, driver errors and owned-process cleanup remain here; this failure is not relabeled a pass.

The accepted hint checks therefore use explicit DOM keyboard events through the real focused canvas, GLFW and C controller, with actual GPU fade samples and default-prevention assertions. Other existing keyboard/pointer/wheel gates retain their original delivery. Physical/trusted Alt, physical trackpad momentum/pinch, real window dragging/double-click/fullscreen/Spaces and the broader browser matrix remain unaccepted. No user browser tab or visible native application was controlled.

## Artifacts and delivery

`build/Duxcel.app` is rebuilt and its ad-hoc signature verifies. All 25 file-backed Mach-O code/data sections match the tested optimized `./duxcel`; signatures legitimately change the whole-file hash. Bundle signing is not an additional bundle-launch test. [Exact hashes](titlebar-bundle.json).

The single existing Wasm review origin remains `http://127.0.0.1:8767/`, now serving the accepted review artifact with automatic external queue pickup. HTTP Wasm bytes were hashed against its build manifest. The user's existing tab was neither opened nor reloaded. [Served artifact](titlebar-served.json). Mock/Agentation services remain stopped.
