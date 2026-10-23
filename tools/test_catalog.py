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
    engine_modules: tuple = ()


STANDALONE_GUI = {
    "journal-session": ("build/tests/test_journal_session-gui", "PASS: native checkpoint journal workflow",
                        r"^PASS: native checkpoint journal workflow \(18 stages, [1-9]\d* checks\)$", 18),
    "workbook-lifecycle": ("build/tests/test_workbook_lifecycle-gui", "PASS: native workbook lifecycle workflow",
                           r"^PASS: native workbook lifecycle workflow \(24 stages, [1-9]\d* checks\)$", 24),
    "workbook-history": ("build/tests/test_workbook_history-gui", "PASS: native workbook chronology workflow",
                         r"^PASS: native workbook chronology workflow \(21 stages, [1-9]\d* checks\)$", 21),
    "workbook-identity": ("build/tests/test_workbook_identity-gui", "PASS: native workbook identity workflow",
                          r"^PASS: native workbook identity workflow \(9 stages, [1-9]\d* checks\)$", 9),
    "cell-command": ("build/tests/test_cell_command-native", "PASS: native cell command workflow",
                     r"^PASS: native cell command workflow \(13 stages, [1-9]\d* checks\)$", 13),
}


ENGINE_PROCESS_CHECKS = {
    "workbook_journal": ("tools/test_journal_process.py", "PASS: journal process boundaries",
                         r"^PASS: journal process boundaries \([1-9]\d* checks\)$"),
}


FEATURES = {
    "journal-session": Feature(("cell_command",), "journal-session",
        ("python3 tools/test.py feature --feature workbook-history",
         "python3 tools/test.py native --scenario workspace document --config sanitize",
         "python3 tools/test.py formats --config both"),
        ("src/workbook/journal_session.c", "src/workbook/journal_queue.c", "src/app/app.c: checkpoint admission"),
        ("journal_session", "journal_queue", "workbook_journal")),
    "journal-queue": Feature((), "",
        ("python3 tools/test.py feature --feature workbook-journal",
         "python3 tools/test.py feature --feature workbook-lifecycle"),
        ("src/workbook/journal_queue.c", "src/workbook/journal.c"), ("journal_queue", "workbook_journal")),
    "workbook-journal": Feature((), "",
        ("python3 tools/test.py feature --feature workbook-lifecycle",
         "python3 tools/test.py native --scenario workspace document --config sanitize"),
        ("src/workbook/journal.c", "tests/test_workbook_journal.c"), ("workbook_journal",)),
    "workbook-lifecycle": Feature(("workbook_lifecycle", "workbook_history", "cell_command"), "workbook-lifecycle",
        ("python3 tools/test.py feature --feature workbook-history",
         "python3 tools/test.py native --scenario workspace document --config sanitize",
         "python3 tools/test.py formats --config both"),
        ("src/workbook/lifecycle.c", "src/workbook/history.c", "src/app/app.c: worksheet actions")),
    "workbook-history": Feature(("workbook_history", "cell_command", "workbook_identity"), "workbook-history",
        ("python3 tools/test.py native --scenario workspace document --config sanitize",
         "python3 tools/test.py formats --config both",
         "python3 tools/test_web_app.py --review --review-cell-only"),
        ("src/workbook/history.c", "src/workbook/workbook.c: action order/pruning", "src/app/app.c: history adapter")),
    "workbook-identity": Feature(("workbook_identity", "cell_command", "workbook_codec"), "workbook-identity",
        ("python3 tools/test.py native --scenario document workspace --config sanitize",
         "python3 tools/test.py formats --config both"),
        ("src/workbook/identity.c", "src/workbook/workbook.c: v15 persistence", "src/workbook/cell_command.c"),
        ("workbook_identity_native",)),
    "cell-command": Feature(("cell_command",), "cell-command",
        ("python3 tools/test.py native --scenario document workspace --config sanitize",
         "python3 tools/test_web_app.py --review --review-cell-only"),
        ("src/workbook/cell_command.c", "src/workbook/workbook.h", "src/app/app.c: cell editor adapter")),
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
            "engine_modules": list(feature.engine_modules),
            "engine_process_checks": [ENGINE_PROCESS_CHECKS[module][0] for module in feature.engine_modules if module in ENGINE_PROCESS_CHECKS], "owns": list(feature.owns), "deferred_interactions": list(feature.interactions),
            "milestone": "python3 tools/test.py full --config both",
            "limits": "A focused pass covers this boundary; it does not establish whole-application or browser acceptance."}
