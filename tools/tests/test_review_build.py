"""Pure fixtures for production/review artifact checks; no app execution."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock
from test_review_build import (MARKERS, MODULES, REQUIRED_SYMBOLS, TEST_MARKERS,
                               ContractError, check_makefile, check_binaries,
                               inspect_binary, _expand)

class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="duxcel-review-contract-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.shipping, self.review = self.root / "shipping", self.root / "review"
        self.shipping.write_bytes(b"shipping\0--help\0")
        self.review.write_bytes(b"review\0" + b"\0".join(MARKERS))

    def nm(self, arguments, **kwargs):
        data = Path(arguments[-1]).read_bytes()
        names = REQUIRED_SYMBOLS if data.startswith(b"review") else ()
        return subprocess.CompletedProcess(arguments, 0, "\n".join("000001 T _" + name for name in names), "")

    def makefile(self):
        text = "SRC = src/main.c\nAPP_SOURCES = $(SRC)\nAPP_RELEASE_FLAGS = -O2\nAPP_SANITIZE_FLAGS = -O1\nFIXTURE_FLAGS = -O2\n"
        text += "REVIEW_SOURCES = $(APP_SOURCES) " + " ".join("src/dev/review_" + module + ".c" for module in MODULES) + "\n"
        text += "REVIEW_FLAGS = -DDUXCEL_DEV_REVIEW -include src/dev/review_input.h -Ibuild/review\n"
        text += "REVIEW_SANITIZE_FLAGS = -DDUXCEL_DEV_REVIEW -include src/dev/review_input.h -Ibuild/review-sanitize\n"
        text += "INCREMENTAL_OBJECTS = $(APP_RELEASE_OBJECTS) $(APP_SANITIZE_OBJECTS) $(REVIEW_OBJECTS) $(REVIEW_SANITIZE_OBJECTS)\n"
        configurations = (("APP_RELEASE_OBJECTS", "release", "APP_RELEASE_FLAGS", "APP_SOURCES", "build/bin/duxcel"),
                          ("APP_SANITIZE_OBJECTS", "sanitize", "APP_SANITIZE_FLAGS", "APP_SOURCES", "build/bin/duxcel-debug"),
                          ("REVIEW_OBJECTS", "review", "REVIEW_FLAGS", "REVIEW_SOURCES", "build/review/duxcel-review"),
                          ("REVIEW_SANITIZE_OBJECTS", "review-sanitize", "REVIEW_SANITIZE_FLAGS", "REVIEW_SOURCES", "build/review-sanitize/duxcel-review"))
        for name, configuration, flags, sources, target in configurations:
            prefix = "build/obj/" + configuration + "/app/"
            text += name + " = $(addprefix " + prefix + ",$(addsuffix .o,$(basename $(" + sources + "))))\n"
            for suffix in ("c", "m"):
                text += prefix + "%.o: %." + suffix + " " + prefix + "compile.flags\n\t$(call compile_object,$(" + flags + "))\n"
            text += prefix + "compile.flags: force-build-signature\n\t$(call build_signature,$(" + flags + "))\n"
            text += prefix + "link.flags: force-build-signature\n\t$(call build_signature,$(" + name + "))\n"
            text += target + ": $(" + name + ")\n\t$(call link_binary,$(" + name + "),$(LDLIBS))\n"
        return text + "-include $(INCREMENTAL_OBJECTS:.o=.d)\nreview: build/review/duxcel-review\nreview-debug: build/review-sanitize/duxcel-review\n"

    def test_make_configuration_separation_and_leak_rejections(self):
        text = self.makefile()
        self.assertEqual(check_makefile(text)["objects"]["review"], 5)
        failures = [text + "SRC += src/dev/review_capture.c\n",
                    text + "APP_RELEASE_FLAGS += -DDUXCEL_DEV_REVIEW\n",
                    text.replace("REVIEW_FLAGS = -DDUXCEL_DEV_REVIEW", "REVIEW_FLAGS = -DOTHER"),
                    text.replace("REVIEW_OBJECTS = $(addprefix build/obj/review/app/", "REVIEW_OBJECTS = $(addprefix build/obj/release/app/"),
                    text.replace("build/bin/duxcel: $(APP_RELEASE_OBJECTS)", "build/bin/duxcel: $(APP_RELEASE_OBJECTS) $(REVIEW_OBJECTS)"),
                    text.replace("INCREMENTAL_OBJECTS = $(APP_RELEASE_OBJECTS) $(APP_SANITIZE_OBJECTS) $(REVIEW_OBJECTS)", "INCREMENTAL_OBJECTS = $(APP_RELEASE_OBJECTS) $(APP_SANITIZE_OBJECTS)"),
                    text.replace("$(call build_signature,$(REVIEW_FLAGS))", "$(call build_signature,$(APP_RELEASE_FLAGS))")]
        for index, broken in enumerate(failures):
            with self.subTest(case=index), self.assertRaises(ContractError):
                check_makefile(broken)

    def test_complete_artifacts_have_separate_provenance(self):
        with mock.patch.object(subprocess, "run", side_effect=self.nm):
            result = check_binaries(self.shipping, self.review)
        self.assertNotEqual(result["shipping"]["sha256"], result["review"]["sha256"])
        self.assertEqual(result["shipping"]["symbols"], [])

    def test_shipping_feature_strings_and_source_paths_fail(self):
        for marker in MARKERS + (b"src/dev/review_registry.c",):
            with self.subTest(marker=marker), mock.patch.object(subprocess, "run", side_effect=self.nm):
                self.shipping.write_bytes(b"shipping\0" + marker)
                with self.assertRaises(ContractError):
                    check_binaries(self.shipping, self.review)

    def test_isolated_checkpoints_fail_in_both_application_configurations(self):
        for target in (self.shipping, self.review):
            original=target.read_bytes()
            try:
                target.write_bytes(original+b"\0"+TEST_MARKERS[0])
                with mock.patch.object(subprocess,"run",side_effect=self.nm), self.assertRaises(ContractError):
                    check_binaries(self.shipping,self.review)
            finally:
                target.write_bytes(original)

    def test_shipping_symbols_fail_and_review_symbols_are_required(self):
        for shipping in (True, False):
            with self.subTest(shipping=shipping):
                def nm(arguments, **kwargs):
                    is_shipping = Path(arguments[-1]).read_bytes().startswith(b"shipping")
                    names = REQUIRED_SYMBOLS if shipping or not is_shipping else ()
                    if not shipping and not is_shipping:
                        names = REQUIRED_SYMBOLS[:-1]
                    return subprocess.CompletedProcess(arguments, 0, "\n".join("000 T _" + name for name in names), "")
                with mock.patch.object(subprocess, "run", side_effect=nm), self.assertRaises(ContractError):
                    check_binaries(self.shipping, self.review)

    def test_review_marker_missing_invalid_nm_and_same_path_fail(self):
        self.review.write_bytes(b"review\0")
        with mock.patch.object(subprocess, "run", side_effect=self.nm), self.assertRaises(ContractError):
            check_binaries(self.shipping, self.review)
        with mock.patch.object(subprocess, "run", return_value=subprocess.CompletedProcess([], 1, "", "invalid object")), self.assertRaises(ContractError):
            inspect_binary(self.shipping)
        with self.assertRaises(ContractError):
            check_binaries(self.shipping, self.shipping)

    def test_make_expansion_is_bounded_and_never_executes_shell(self):
        self.assertEqual(_expand("$(filter %.c,$(SRC))", {"SRC": "a.c x.o"}), "a.c")
        self.assertEqual(_expand("$(addprefix obj/,$(addsuffix .o,$(basename $(SRC))))", {"SRC": "a.c"}), "obj/a.o")
        self.assertEqual(_expand("$(shell should-not-run)", {}), "")
        with self.assertRaises(ContractError):
            _expand("$(LOOP)", {"LOOP": "$(LOOP)"})
