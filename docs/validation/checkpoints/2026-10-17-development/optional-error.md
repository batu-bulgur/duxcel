# Optional-error component verification

Source commit: `981a8fc74b2dd7894566e07fff0c11c65efe2873`, integrated unchanged as `90fc7a6`.

`objects_add_chart` now accepts an omitted error sink safely for invalid dimensions and pivot definitions. Thirteen assertions verify diagnostics and unchanged object/history state.

| Command | Result | Duration |
| --- | --- | --- |
| `make tests/test_workbook` then `./tests/test_workbook` | 1,683 assertions, exit 0 | 7.352s execution |
| `make tests/test_workbook-debug` then `ASAN_OPTIONS=halt_on_error=1 ./tests/test_workbook-debug` | 1,683 assertions, exit 0; no sanitizer diagnostics | 37.867s execution |

The isolated worktree created `tests/generated/workspace-old` and `workspace-new` before execution. Both builds were warning-free. This record is transcribed from the agent tool outputs; raw logs were not saved by those initial commands. Subsequent workflow runs retain raw per-test logs and JSON manifests automatically. No full native acceptance is implied.
