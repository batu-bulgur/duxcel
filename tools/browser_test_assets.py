"""Serve/package test modules at the flat Wasm artifact boundary.

Sources keep correct relative imports for direct Node tests. The browser test
origin and isolated C drivers serve the same modules beside the runtime assets.
Only static relative module specifiers are rebased; no product logic is added.
"""
import re


def flat_test_module(path):
    source = path.read_text()
    def rebase(match):
        prefix, quote, name = match.groups()
        return prefix + quote + './' + name.rsplit('/', 1)[-1] + quote
    return re.sub(r"(\bfrom\s*|\bimport\s*\(\s*)(['\"])(\.{1,2}/[^'\"]+\.mjs)\2", rebase, source)
