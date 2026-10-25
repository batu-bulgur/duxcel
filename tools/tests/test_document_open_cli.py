"""CLI inspection must not build or launch a native document test."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import test_open_documents as runner


class DocumentOpenCliTests(unittest.TestCase):
    def test_help_and_unknown_flags_do_not_start_acceptance(self):
        for arguments, expected in ((["--help"], 0), (["--unknown-option"], 2)):
            with self.subTest(arguments=arguments), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                with mock.patch.object(runner, "ROOT", root), contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    with self.assertRaises(SystemExit) as stopped:
                        runner.main(arguments)
                self.assertEqual(stopped.exception.code, expected)
                self.assertEqual(list(root.iterdir()), [])
