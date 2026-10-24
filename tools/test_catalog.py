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
    fixtures: tuple = ()


STANDALONE_GUI = {
    "recovery-menu": ("build/tests/test_recovery_process-debug", "PASS: native recovery choice workflow",
                       r"^PASS: native recovery choice workflow \(19 stages, [1-9]\d* checks\)$", 19),
    "recovery": ("build/tests/test_recovery-gui", "PASS: native recovery choice workflow",
                  r"^PASS: native recovery choice workflow \(19 stages, [1-9]\d* checks\)$", 19),
    "workbook-save": ("build/tests/test_workbook_save-gui", "PASS: native controlled Save workflow",
                      r"^PASS: native controlled Save workflow \(18 stages, [1-9]\d* checks\)$", 18),
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


@dataclass(frozen=True)
class EngineProcessCheck:
    script: str
    marker: str
    completion: str
    support: tuple = ()
    arguments: tuple = ()
    features: tuple = ()
    primary: bool = False

    def selected(self, feature):
        return feature is None or not self.features or feature in self.features


JOURNAL_FAULT_LIBRARY = "build/tests/libjournal_fault.dylib"
ENGINE_PROCESS_CHECKS = {
    "recovery_process": EngineProcessCheck("tools/test_recovery_process.py", "PASS: native recovery crash and ownership",
        r"^PASS: native recovery crash and ownership \(3 processes\)$", primary=True),
    "journal_performance": EngineProcessCheck("tools/benchmark_journal.py", "PASS: journal workload provenance and measurements",
        r"^PASS: journal workload provenance and measurements$", arguments=("--input", "build/tests/generated/journal-paste.tsv"), primary=True),
    "workbook_journal": EngineProcessCheck("tools/test_journal_process.py", "PASS: journal process boundaries",
        r"^PASS: journal process boundaries \(10 cases, 72 checks\)$", (JOURNAL_FAULT_LIBRARY,), ("--fault-library", JOURNAL_FAULT_LIBRARY), ("workbook-journal", "journal-faults")),
    "journal_queue": EngineProcessCheck("tools/test_journal_faults.py", "PASS: journal queue filesystem failures",
        r"^PASS: journal queue filesystem failures \(12 cases, 28 checks\)$", (JOURNAL_FAULT_LIBRARY,), ("--fault-library", JOURNAL_FAULT_LIBRARY, "--queue"), ("journal-queue", "journal-faults")),
}


FEATURES = {
    "archive-write": Feature((), "workbook-save",
        ("python3 tools/test.py feature --feature workbook-archive", "python3 tools/test.py feature --feature recovery-disposition"),
        ("src/workbook/archive_write.c", "src/workbook/archive_write.h", "src/workbook/save_native.h", "src/workbook/workbook.c: typed native Save transaction adapter", "tests/test_archive_write.c"),
        ("archive_write", "workbook_save", "worker_save", "archive_read")),
    "archive-read": Feature((), "",
        ("python3 tools/test.py feature --feature workbook-archive", "python3 tools/test.py feature --feature save-ui", "python3 tools/test.py feature --feature recovery-disposition"),
        ("src/workbook/archive_read.c", "src/workbook/archive_read.h", "src/workbook/workbook.c: borrowed v16 head reader", "tests/test_archive_read.c"),
        ("archive_read",)),
    "version-closure": Feature((), "",
        ("python3 tools/test.py feature --feature workbook-archive", "python3 tools/test.py feature --feature save-ui"),
        ("src/workbook/version_restore.c", "src/workbook/version_restore.h", "tests/test_version_restore.c"),
        ("version_restore", "core_restore", "version_read")),
    "core-restore": Feature((), "",
        ("python3 tools/test.py feature --feature workbook-archive", "python3 tools/test.py feature --feature save-ui"),
        ("src/workbook/core_payload.c: streamed restore", "src/workbook/save.h", "src/workbook/workbook.c: strict leaf reader", "tests/test_core_restore.c"),
        ("core_restore", "core_payload", "workbook_payload")),
    "version-metadata": Feature((), "",
        ("python3 tools/test.py feature --feature workbook-archive", "python3 tools/test.py feature --feature save-ui"),
        ("src/workbook/version_read.c", "src/workbook/version_read.h", "src/workbook/version_codec.c", "src/workbook/version_sql.c", "tests/test_version_read.c"),
        ("version_read", "version_stage")),
    "version-stage": Feature(("version_index", "version_chunks"), "",
        ("python3 tools/test.py feature --feature save-ui", "python3 tools/test.py feature --feature workbook-archive"),
        ("src/workbook/version_stage.c", "src/workbook/version_stage.h", "tests/test_version_stage.c"),
        ("version_stage",)),
    "version-chunks": Feature(("version_chunks",), "",
        ("python3 tools/test.py feature --feature workbook-archive",),
        ("src/workbook/version_chunks.c", "src/workbook/version_chunks.h", "tests/test_version_chunks.c")),
    "core-compare": Feature((), "",
        ("python3 tools/test.py feature --feature save-ui", "python3 tools/test.py feature --feature workbook-archive"),
        ("src/workbook/core_compare.c", "src/workbook/core_compare.h", "tests/test_core_compare.c"),
        ("core_compare",)),
    "core-payload": Feature((), "",
        ("python3 tools/test.py feature --feature save-ui", "python3 tools/test.py feature --feature workbook-archive", "python3 tools/test.py feature --feature recovery-disposition"),
        ("src/workbook/core_payload.c", "src/workbook/core_payload.h", "tests/test_core_payload.c"),
        ("core_payload", "file_lease")),
    "payload-paths": Feature((), "workbook-save",
        ("python3 tools/test.py feature --feature workbook-archive", "python3 tools/test.py feature --feature recovery-disposition"),
        ("src/workbook/workbook.c: relocatable core payload", "src/workbook/save.h", "tests/test_workbook_payload.c"),
        ("workbook_payload", "workbook_save", "worker_save")),
    "version-index": Feature(("version_index",), "",
        ("python3 tools/test.py feature --feature save-ui", "python3 tools/test.py feature --feature recovery-disposition", "python3 tools/test.py feature --feature workbook-archive"),
        ("src/workbook/version_index.c", "src/workbook/version_index.h", "tests/test_version_index.c")),
    "stats-budget": Feature(("statsview_input",), "stats-budget",
        ("python3 tools/test.py native --scenario layout --config sanitize",),
        ("src/app/app.c: Statistics to format-budget interaction",)),
    "foundation-layout-end": Feature((), "foundation-layout-end",
        ("python3 tools/test.py native --scenario layout --config sanitize",),
        ("src/app/app.c: inspector and manual range milestone workflow",)),
    "foundation-sources": Feature((), "foundation-sources",
        ("python3 tools/test.py native --scenario layout --config sanitize",),
        ("src/app/app.c: source-format and range milestone workflow",)),
    "source-layout": Feature(("cellreadout",), "source-layout",
        ("python3 tools/test.py native --scenario layout --config sanitize",),
        ("src/app/app.c: source menu to canvas layout and value-strip isolation",)),
    "foundation-final": Feature((), "foundation-final",
        ("python3 tools/test.py native --scenario layout --config sanitize",),
        ("src/app/app.c: remaining shared actions/source-format/range milestone workflow",)),
    "foundation-remainder": Feature((), "foundation-remainder",
        ("python3 tools/test.py native --scenario layout --config sanitize",),
        ("src/app/app.c: foundation milestone remaining layout workflow",)),
    "foundation-layout": Feature((), "foundation-layout",
        ("python3 tools/test.py native --scenario layout --config sanitize",),
        ("src/app/app.c: foundation milestone layout continuation",)),
    "inspector-basics": Feature(("inspector", "stepview"), "inspector-basics",
        ("python3 tools/test.py affected --feature inspector-scroll --modules inspector --config sanitize", "python3 tools/test.py native --scenario layout --config sanitize"),
        ("src/app/app.c: Steps/SQL/preview workflow and visible row hit geometry",)),
    "format-controls": Feature(("format",), "format-controls",
        ("python3 tools/test.py affected --feature source-format --modules source_scope format --config sanitize", "python3 tools/test.py native --scenario layout --config sanitize"),
        ("src/app/app.c: generic formatting controls and manual/source test ownership",)),
    "workbook-schema": Feature(("workbook_schema",), "workbook-save",
        ("python3 tools/test.py feature --feature workbook-archive", "python3 tools/test.py feature --feature recovery-ui", "python3 tools/test.py feature --feature workbook-identity"),
        ("src/workbook/schema.c", "src/workbook/schema.h", "src/workbook/workbook.c: bounded catalog proof")),
    "workbook-archive": Feature((), "",
        ("python3 tools/test.py feature --feature workbook-identity", "python3 tools/test.py feature --feature recovery-ui"),
        ("tests/test_workbook.c: source/guarded archive upgrades", "Makefile: per-file workbook engine test"), ("core", "workbook")),
    "recovery-menu": Feature(("keytips",), "recovery-menu",
        ("python3 tools/test.py feature --feature recovery-ui", "python3 tools/test_web_app.py --review --review-controls-only --source tests/fixtures/transactions.csv"),
        ("src/ui/keytips.c: File command registry", "src/app/app.c: File menu paint", "src/dev/review_app.inc: File target metadata")),
    "recovery-crash": Feature((), "",
        ("python3 tools/test.py feature --feature recovery-ui", "python3 tools/test.py full --config both"),
        ("tools/test_recovery_process.py", "tests/support/recovery_app.inc: owned process roles"), ("recovery_process",)),
    "recovery-ui": Feature(("keytips",), "recovery",
        ("python3 tools/test.py feature --feature recovery-disposition", "python3 tools/test.py feature --feature recovery-worker", "python3 tools/test.py native --scenario document --config sanitize"),
        ("src/app/recovery.inc", "src/ui/recoveryview.c", "src/app/app.c: recovery document adoption", "src/app/source_format.inc: recovery ownership")),
    "recovery-disposition": Feature((), "journal-session",
        ("python3 tools/test.py feature --feature recovery-worker", "python3 tools/test.py feature --feature recovery-catalog"),
        ("src/workbook/recovery.c: explicit retirement", "src/workbook/journal_session.c: inherited lineage", "src/platform/platform.h: noncreating recovery location"), ("journal_session", "workbook_recovery")),
    "recovery-worker": Feature((), "",
        ("python3 tools/test.py feature --feature recovery-catalog", "python3 tools/test.py feature --feature recovery-candidate"),
        ("src/workbook/recovery_worker.c", "src/workbook/recovery_worker.h"), ("recovery_worker",)),
    "recovery-catalog": Feature((), "",
        ("python3 tools/test.py feature --feature recovery-owner", "python3 tools/test.py feature --feature recovery-candidate"),
        ("src/platform/directory.c", "src/workbook/recovery_catalog.c", "src/workbook/recovery_catalog.h"), ("directory_cursor", "recovery_catalog")),
    "recovery-candidate": Feature((), "",
        ("python3 tools/test.py feature --feature recovery-owner", "python3 tools/test.py feature --feature workbook-journal"),
        ("src/workbook/recovery.c", "src/workbook/recovery.h", "src/workbook/journal.c: metadata discovery"), ("workbook_recovery",)),
    "recovery-owner": Feature((), "journal-session",
        ("python3 tools/test.py feature --feature journal-session", "python3 tools/test.py feature --feature journal-faults"),
        ("src/platform/file_lease.c", "src/platform/file_lease.h", "src/workbook/journal_session.c: generation ownership"), ("file_lease", "journal_session")),
    "save-ui": Feature((), "workbook-save",
        ("python3 tools/test.py feature --feature workbook-save", "python3 tools/test.py feature --feature journal-session"),
        ("src/app/app.c: Save progress and completion", "tests/support/workbook_save_app.inc")),
    "save-worker": Feature((), "journal-session",
        ("python3 tools/test.py feature --feature workbook-save", "python3 tools/test.py native --scenario background document --config sanitize"),
        ("src/data/worker.c: ordered native Save lane", "src/data/worker.h"), ("worker_save",)),
    "workbook-save": Feature((), "workbook-save",
        ("python3 tools/test.py feature --feature journal-session", "python3 tools/test.py native --scenario document --config sanitize", "python3 tools/test.py formats --config both"),
        ("src/workbook/save.h", "src/workbook/save_fence.c", "src/workbook/workbook.c: controlled transactional Save", "src/data/worker.c: Save receipt", "src/app/app.c: progress UI"), ("workbook_save", "worker_save")),
    "journal-performance": Feature((), "",
        ("python3 tools/test.py feature --feature journal-session", "python3 tools/test.py feature --feature journal-faults"),
        ("src/workbook/journal_session.c: measured admission", "tests/test_journal_performance.c", "tools/benchmark_journal.py"),
        ("journal_performance",), ("journal-paste",)),
    "journal-faults": Feature((), "",
        ("python3 tools/test.py feature --feature journal-session",
         "python3 tools/test.py native --scenario workspace document --config sanitize"),
        ("src/workbook/journal.c", "src/workbook/journal_queue.c", "tests/support/journal_fault.c"),
        ("workbook_journal", "journal_queue")),
    "journal-session": Feature(("cell_command",), "journal-session",
        ("python3 tools/test.py feature --feature journal-faults",
         "python3 tools/test.py feature --feature workbook-history",
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
            "engine_modules": list(feature.engine_modules), "fixtures": list(feature.fixtures),
            "process_support_targets": sorted({target for module in feature.engine_modules if module in ENGINE_PROCESS_CHECKS and ENGINE_PROCESS_CHECKS[module].selected(name) for target in ENGINE_PROCESS_CHECKS[module].support}),
            "engine_process_checks": [ENGINE_PROCESS_CHECKS[module].script for module in feature.engine_modules if module in ENGINE_PROCESS_CHECKS and ENGINE_PROCESS_CHECKS[module].selected(name)], "owns": list(feature.owns), "deferred_interactions": list(feature.interactions),
            "milestone": "python3 tools/test.py full --config both",
            "limits": "A focused pass covers this boundary; it does not establish whole-application or browser acceptance."}


FEATURE_FIXTURES = {"journal-paste": (("tools/create_workbook.py", "--journal-paste-only"), "PASS: journal paste fixture:", "build/tests/generated/journal-paste.tsv")}
