"""Contract tests for the standalone developer CLIs.

Run with python3 -m unittest discover -s tools/tests -t . . The one test-only
bootstrap mirrors direct script invocation so CLI sibling imports retain their
outside-working-directory behavior. Product/runtime modules never load this.
"""
import sys
from pathlib import Path
TOOLS = Path(__file__).resolve().parents[1]
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
