# UI regression inputs

These four immutable JSON inputs retain accepted panel geometry, scrolling and paint measurements used by the real C/Wasm tests. They contain no executable UI and are not a design specification. The former mock has been retired; current native behavior and written requirements define the product. Change these baselines deliberately when an intended native behavior changes.

Original captures and sources remain in Git at `archive/pre-layout-cleanup-20261018`. Image paths and source hashes inside these files are historical provenance; tests do not reopen those sources or require those images. The test driver validates these files themselves and actual application behavior.
