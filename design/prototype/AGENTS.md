# Prototype Instructions

Run the local server yourself and open the preview in the browser available to this environment. Do not give the user server-start instructions when you can run it.

Before making substantial visual changes, use the Product Design plugin's `get-context` skill when the visual source is unclear or no longer matches the current goal. When the user gives durable prototype-specific design feedback, preferences, or decisions, record them in `AGENTS.md`.

When implementing from a selected generated mock, treat that image as the source of truth for layout, component anatomy, density, spacing, color, typography, visible content, and hierarchy.

Build app UI in `src/`. Keep `.openai/hosting.json`, `worker/index.js`, `scripts/prepare-sites-build.mjs`, and `tests/sites-worker.test.mjs` intact so the same local prototype can be handed to Sites. Before a Sites handoff, run `npm run build` and `npm run test:sites`; the build must leave `dist/client/index.html`, `dist/server/index.js`, and `dist/.openai/hosting.json`.

## Durable Duxcel decisions — 16 October 2026

- This is a disposable UI experiment; production stays C99 + raylib + DuckDB. Do not modify native code as part of design iteration.
- The latest human scope prioritizes motion, pane relationships, drag/drop, typography, icon proportions, density, resizing and viewports. Calculation/query fidelity is not a gate. Use plausible synthetic data and honest sample counts.
- Use real locally bundled IBM Plex Sans, including real italic cuts. Generated image lettering is not font evidence. Inter is superseded.
- Follow the revised analysis/data-inspection compositions alongside the human's explicit density and interaction decisions. Exact 22-unit status height, actual fonts, real controls, and honest sample labels override generated-image inconsistencies.
- Keep Workbook navigation vertical. Make Fields independently useful for source navigation. Use two stacked horizontal Rows/Columns shelves and contextual Measures/Marks.
- Transformations belongs in a right pane. Collapse retains an entry; dismissal removes its rail completely. Never leave a permanent empty rail.
- Full, compact, icons-only and focus modes should preserve context. Interface scale and canvas zoom are separate.
- Document animation timing and responsive behavior, and distinguish browser checks from native/engine validation. Final implementation handoff waits for design approval.
- WebAssembly distribution is future context; do not introduce DuckDB-WASM or another engine into this UI study without a new request.

## Responsive and frozen-pane refinement — 16 October 2026

- A selected scrolling cell must paint and hit-test below frozen cells and row headers. Grid header, frozen surfaces, and active-cell visuals have explicit separate layers; selection never promotes a scrolling cell above a frozen surface.
- Ribbon groups adapt from full controls to named group buttons, then named overflow in priority order. Measure real rendered widths in logical units; avoid a clipped horizontal command strip. Preserve explicit Full/Compact/Icons user mode while adapting within it.
- Expanded groups retain a small disclosure control so all commands remain available in every mode. Overflow keeps group identity. Keep display/help and Save reachable; scroll only the tab names when necessary.
- Use consistent 16-unit ordinary icons, 20-unit full-ribbon analysis icons, 11-unit disclosure arrows, and aligned button targets. Icons mode is now 32 units tall so targets fit cleanly.
- Below 700 logical workspace units, Workbook/Fields/Marks use an explicitly opened left overlay. Preserve section visibility preferences and restore the dock when the viewport widens. A persistent workspace button recovers it.
- Fit status items by their measured full text widths; never force one item into insufficient space. Hidden values remain in statistics customization.

- Preserve configured frozen columns across viewport changes. When fewer than 72 logical units remain for scrolling data, show an explicit Unfreeze action rather than silently changing the configuration.
