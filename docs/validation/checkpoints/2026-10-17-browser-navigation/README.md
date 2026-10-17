# Focused browser navigation ownership

17 October 2026. Native C/raylib production acquires no web runtime or dependency. `8a3cb13` improves CDP diagnostics; `44889cb` adds the thin shared host policy; `f4c3121` wires normal/review loaders, packaging and actual event assertions. C owns spreadsheet input/rendering throughout.

## Failure and inference

The earlier manual edit stall was intermittent for an identical Wasm artifact. The new diagnostics distinguish keyDown/frame-wait/keyUp/frame-wait, retain pending request identity/time, reject WebSocket disconnection promptly and release owned held keys during teardown. Eight mocked transport tests cover timeouts, late replies, disconnect/send errors and cleanup.

The unguarded Worker-probe run [failed in7.489s](crash/summary.json), during **ArrowUp frames_after_keyDown**. The WebSocket disconnected; visibility, debugger and screenshot probes failed explicitly rather than manufacturing a C stack. A temporally matching macOS report records Chromium150.0.7871.46, main PID69787 with Node parent69786, a stack-exhaustion SIGSEGV and `NSMenu`/`NSView performKeyEquivalent` frames. [Selected OS fields](crash/os-crash-summary.json) retain the full-report digest without unrelated host diagnostics. That run predates direct spawn-PID recording; newer runs write `browser-process.json` before input and retain exit/held-key cleanup state.

Installed Emscripten6.0.12 GLFW prevents default only for Backspace/Tab, leaving arrow defaults active. Chromium documents that unhandled arrows may be redispatched through Cocoa key-equivalent handling ([official keyboard handling](https://www.chromium.org/developers/os-x-keyboard-handling/)). This supports the redispatch hypothesis for this stack; it does not establish the cause of every historical timeout.

A deliberate old-artifact [negative control](negative-control/summary.json) reached C selection and failed the new assertion in13.550s: [ArrowUp and Shift+Right/Down all had `default_prevented:false`](negative-control/navigation.json). That failure is expected evidence, not a passing regression.

## Integrated policy and acceptance

`web/canvas_input.mjs` installs one disposable canvas bubble listener. Only the focused canvas's unmodified or Shift-only arrows, Home/End and PageUp/PageDown call `preventDefault`. It never stops propagation: GLFW's window-capture listener still queues the original event for C. Printable keys/Space, other DOM inputs, composition and Ctrl/Alt/Meta chords are untouched. Seven pure tests cover these boundaries, repeat installation and disposal. This is platform event ownership, not JavaScript spreadsheet behavior.

Guarded normal artifact `build/web/app/2f89ecdd14612ee1c24b` has Wasm SHA-256 **`3d707332e4fd1f2263f9ed8ba211f0227388a919c6d08795adad96941d8e0a46`, identical to the unguarded control**. Its2.401s rebuild reused every C object. Review artifact `build/web/review/b0aaa2cf05fcc0255dc2` has Wasm `f89e37dba3d46234705aa11d928454ff3d39177d137d5254060a9e9f3d066de6`; the14.128s build also includes the earlier native select changes. Exact inputs/hashes are retained in both build records and run summaries.

| Real hidden browser gate | Checks | Total elapsed | Evidence |
| --- | ---: | ---: | --- |
| Manual exact-text entry/navigation + official Worker probe |12|8.821s|[summary](manual-worker/summary.json), [claimed events](manual-worker/navigation.json) |
| Same manual C workflow without probe |11|7.406s|[summary](manual/summary.json), [claimed events](manual/navigation.json), [selection](manual/selection.png) |
| CSV sort/filter + actual C report export, DPR2/inset CSS |35|13.104s|[summary](review/summary.json), [reader](review/reader.json), [frame](review/report-frame.png) |

Manual captures show text `9007199254740993` retained in A1, returning to A1 after Enter/ArrowUp and extending to A1:B2 with Shift. The page records all three navigation events as prevented while C continues moving/selecting. These were separate owned headless profiles; no user's browser, app, Space or OS input was controlled. All three children were alive before cleanup, exited0 and retained no held keys.

All **28 pure transport/input/report-host groups** passed together in121.254ms. The packaged/linked build contract passed, including identical normal/review policy bytes, active loader imports, review-only C exports, actual shipping absence and native binaries. This does not alter native acceptance or claim the whole browser matrix stable.

Remaining W5 limits: Ctrl/Alt/Meta navigation/shortcuts, IME delivery, clipboard, full DPI/backing/context loss and sustained/browser-matrix crash stability. DPR2 still uses a1× backing framebuffer. This closes the reproduced navigation-default leak, not every historical stall or browser-product task. Original frame waits/timeouts remain; failed application input is not retried.
