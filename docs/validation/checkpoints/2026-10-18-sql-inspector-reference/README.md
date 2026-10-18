# Frozen SQL inspector measurements

This is a measurement provider, not native or Wasm UI acceptance. The unchanged
prototype was opened through its actual Compact ribbon, scale and SQL controls
in an owned headless Chromium session. No React state, SQL text, layout or
prototype files were modified.

`tools/capture_reference_rail.py --sql-only --dpr 2` at runner `7f560b4` passed
1,860 checks with 35 original captures in 45.957 seconds. Original run:
`tests/results/runs/20261018T051457-reference-sql-44683`. Its reference SHA256 is
`5536cadac1673974034817259e5228e6da4a1f8c392786a21796734550b71f69`;
manifest SHA256 is
`11323f994cfa0f0adf7a61cbd6a0cc4898b998ed7b1109595e96aaadcf33644b`.

[provider.json](provider.json) retains original capture identities, six detailed
anatomies, resolved styles and box quads, trusted input and mount-lifetime
observations, and explicitly derived layout rules. Its SHA256 is
`23480f4c61d507615ac92724da46ebf2509d01ce369d983ab8d435e40e983b2a`.

All four mandatory wide/narrow 100/150% cases have zero outer overflow. The
640×640 at 150% case has a 232-point code frame with a 13-point inner scrollbar
lane and a fractional 5.333333492-unit code extent. The exploratory 640×400 at
150% case has a zero-height code frame and outer overflow instead. The code
scroll resets on Steps→SQL; the shared aside retains its outer offset through
that switch and resets it after Collapse unmounts it. Text wrapping and actual
query length remain application-owned; the prototype's SQL is synthetic.
