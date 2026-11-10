"""The Python SDK must declare every third-party module it imports at load time.

`import eni` runs eni/__init__.py, which imports eni/core.py. A third-party
module imported at the top level of any SDK module has to be in
`install_requires` in sdk/python/setup.py, or a plain `pip install` gives a
package that cannot be imported. Imports inside functions are optional
features and may live in `extras_require` instead.
"""

import ast
import re
import sys
from pathlib import Path

SDK = Path(__file__).resolve().parents[2] / "sdk" / "python"


def _install_requires():
    tree = ast.parse((SDK / "setup.py").read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, ast.Call) and getattr(node.func, "id", None) == "setup":
            for kw in node.keywords:
                if kw.arg == "install_requires":
                    reqs = ast.literal_eval(kw.value)
                    return {re.split(r"[<>=!~\[; ]", r, maxsplit=1)[0].lower() for r in reqs}
    raise AssertionError("setup() call with install_requires not found in sdk/python/setup.py")


def _top_level_imports(path):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    names = set()
    for node in tree.body:  # module level only
        if isinstance(node, ast.Import):
            names.update(alias.name.split(".")[0] for alias in node.names)
        elif isinstance(node, ast.ImportFrom) and node.level == 0 and node.module:
            names.add(node.module.split(".")[0])
    return names


def test_top_level_third_party_imports_are_install_requires():
    stdlib = set(getattr(sys, "stdlib_module_names", ())) or {
        "ctypes", "os", "platform", "time", "pathlib", "asyncio", "sys", "json",
    }
    required = _install_requires()
    missing = {}
    for path in sorted((SDK / "eni").glob("*.py")):
        for name in _top_level_imports(path) - stdlib - {"eni"}:
            if name.lower() not in required:
                missing.setdefault(name, []).append(path.name)
    assert not missing, (
        f"imported at module level but not in install_requires: {missing}"
    )
