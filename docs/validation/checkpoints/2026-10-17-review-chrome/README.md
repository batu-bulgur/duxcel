# Review selection over title/ribbon chrome

Historical checkpoint. Machine-generated payloads were removed from the current checkout on 18 October 2026; they remain recoverable at Git ref `archive/pre-cleanup-20261018`. Paths and measurements below describe that original run. See the [retention policy](../../README.md).

Implementation **`965efc3`**, accepted on17October2026. The reported screenshot
showed the title bar and ribbon tabs. Those controls were rendered but absent
from the semantic review registry. This was a coverage gap, not a CSS/DPR hit-test
failure.

The actual draw sites now register the brand/document labels, Save, all currently
visible ribbon tabs, Ribbon display and Help. Separate background surfaces and
containers support reports over empty strip areas. Tab bounds use the renderer's
actual scissor and scroll position. Active-tab state is captured; static labels
are reportable without becoming input controls. Disabled controls remain
pickable. Added registration and selection diagnostics are review-only.

## Acceptance

- Expanded native flow **37stages**, including every previous26stage. Optimized
  App **2.432s**, sanitizer App **2.806s**; total gates **4.011s /4.450s**, each
  including the477-check hidden-window probe. Real pointer selection of Home and
  Ribbon display, keyboard selection of the brand, container/background picks,
  and genuinely scrolled/clipped tabs at150% UI scale pass. The test waits for
  requested UI scale to become applied before capturing; it does not shorten
  animation waits or remove older assertions.
- Actual headless Wasm gate **47checks**, **16.173s** total, DPR2 with an80% inset
  canvas. Twelve real pointer selections cover brand, document, disabled Save,
  Home/Insert/Data/Analyze/View, Ribbon display, Help and both blank strips. Each
  enters the real C composer with the exact expected target and cancels back to
  the open filter. The original filter report still reaches the durable queue
  with correct frame/hash/target; report interactions schedule no additional SQL.
  Home selected in the C composer (archived `home-selected.png`).
- Fresh normal/review Wasm and both native shipping/review build boundaries pass
  linked absence after all binaries finished building. Native builds emitted no warnings.

Full source/binary manifests and results (archived `acceptance.json`). All native windows
were hidden before creation; the browser used an owned headless profile and the
shared desktop lock. No user's running app/browser or desktop was controlled.

Earlier failed narrow regressions are excluded: one captured before the newly
requested scale was applied, so it had scale1.0 rather than1.5. The final gate
waits for the actual state and forces genuine tab overflow at400×640. This is a
test setup correction; broad UI parity, physical input and browser sharpness are
not implied by this reporting fix.
