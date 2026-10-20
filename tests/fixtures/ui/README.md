# UI expected-value contracts

These are test inputs for real C UI behavior, never executable mock/design code.
Each JSON has a schema, version and a finite set of accepted states:

| File | Consumer / expected values |
| --- | --- |
| `rail-measurements.json` | Expanded panel/control bounds at four viewport/scale combinations |
| `steps-scroll.json` | Seven Steps panel/header/list states including inner/outer scrolling |
| `sql-inspector.json` | Six SQL inspector states, bounds, wrapping and fractional scroll limits |

`web/tests/support/ui_contracts.mjs` checks the inputs and prepares expected values;
`web/tests/driver.mjs` compares actual C state, controls and pixels against them.
The contract test preserves the accepted geometry/scroll/color fingerprints.
Intentional UI changes require deliberate expectation changes and real behavior
verification, not blindly preserving old pixels.

Old run commands, source manifests, screenshot provenance, exploratory states
and historical pass reports have been removed. Original material remains in Git.
These inputs have no dependency on old sessions or deleted mock source files.
