"""Reviewed feature boundaries, local checks and deferred interaction coverage.

This is an explicit ownership map, not a claim that filename heuristics can
prove all possible dependencies. Register a slice before extending it.
"""
from dataclasses import dataclass


@dataclass(frozen=True)
class Feature:
    modules: tuple
    gui: str
    interactions: tuple
    owns: tuple


FEATURES = {
    "containing-regions": Feature(("cell_regions", "selection_ladder", "selection"), "containing-regions",
        ("python3 tools/test.py affected --feature selection-ladder --modules selection_ladder --config sanitize",
         "python3 tools/test_web_app.py --review --selection-only --source tests/fixtures/null-text.csv"),
        ("src/workbook/cells.c", "src/app/selection.c", "src/grid/selection_ladder.c")),
    "selection-ladder": Feature(("selection_ladder", "selection"), "selection-ladder",
        ("python3 tools/test.py native --scenario statistics clipboard --config sanitize",),
        ("src/app/selection.c", "src/grid/selection_ladder.c")),
    "range-move": Feature((), "range-move",
        ("python3 tools/test.py affected --feature range-replace --modules selection --config sanitize",
         "python3 tools/test.py native --scenario workspace clipboard document --config sanitize"),
        ("src/grid/grid.c", "src/workbook/workbook.c", "src/app/app.c: move adapter")),
    "range-replace": Feature((), "range-replace",
        ("python3 tools/test.py affected --feature range-move --modules selection --config sanitize",
         "python3 tools/test.py native --scenario workspace document --config sanitize"),
        ("src/workbook/workbook.c", "src/app/app.c: replacement adapter")),
    "about": Feature(("build_info",), "about",
        ("python3 tools/test_app_lifecycle.py", "python3 tools/test_open_documents.py"),
        ("src/app/build_info.c", "src/platform/macos_about.m", "tools/build_info.py")),
    "test-infrastructure": Feature((), "",
        ("python3 tools/test.py feature --feature about", "python3 tools/test.py feature --feature containing-regions"),
        ("tools/test.py", "tools/test_catalog.py", "Makefile")),
}


def plan(name):
    feature = FEATURES[name]
    return {"feature": name, "modules": list(feature.modules), "gui": feature.gui or None,
            "tool_tests": ["tools.tests.test_build_info"] if name == "about" else
                          ["tools.tests.test_runner", "tools.tests.test_feature_catalog", "tools.tests.test_web_build"] if name == "test-infrastructure" else [],
            "owns": list(feature.owns), "deferred_interactions": list(feature.interactions),
            "milestone": "python3 tools/test.py full --config both",
            "limits": "A focused pass covers this boundary; it does not establish whole-application or browser acceptance."}
