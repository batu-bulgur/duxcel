# Native rendering inputs

`shadow_reference.h` and `shadow_corner_reference.h` contain expected numeric
pixel data consumed by `tests/test_shadowview.c`. They are test oracles, not
runtime artwork or generated run output. Their original capture provenance is
recoverable from Git; old screenshots/session reports are not required to run
the native tests. The opt-in corner diagnostic remains an open rendering task;
retaining an oracle does not claim that diagnostic passes.
