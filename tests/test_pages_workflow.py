"""Focused CI contract checks, using only Python's standard library.

This inspects the workflow's current indentation/inline-list convention, not
arbitrary YAML or the Actions expression language. Unsupported formatting fails
closed; use actionlint and a real Actions run for full workflow validation.
Run from any directory: python3 /path/to/repo/tests/test_pages_workflow.py
"""

from pathlib import Path
import re
import unittest


WORKFLOW = Path(__file__).resolve().parents[1] / ".github/workflows/build.yml"
MAIN_PUSH = "github.ref == 'refs/heads/main' && github.event_name == 'push'"


def block(text, heading, indent):
    """Extract one uniquely named mapping block at the specified indentation."""
    pattern = rf"(?m)^{' ' * indent}{re.escape(heading)}:\s*$"
    matches = list(re.finditer(pattern, text))
    if len(matches) != 1:
        raise AssertionError(f"expected one {heading} block")
    start = matches[0].end()
    end = re.search(rf"(?m)^\S|^ {{1,{indent}}}\S" if indent else r"(?m)^\S",
                    text[start:])
    return text[start:start + end.start()] if end else text[start:]


def field(text, name, indent):
    values = re.findall(rf"(?m)^{' ' * indent}{re.escape(name)}: (.+)$", text)
    if len(values) != 1:
        raise AssertionError(f"expected one {name} field")
    return values[0].strip()


def check_contract(text):
    """Check the dependency graph and guards without evaluating Actions code."""
    jobs = block(text, "jobs", 0)
    native = block(jobs, "native-build", 2)
    wasm = block(jobs, "wasm-build", 2)
    deploy = block(jobs, "deploy-pages", 2)
    needs = field(deploy, "needs", 4)
    if not (needs.startswith("[") and needs.endswith("]")):
        raise AssertionError("deploy needs must be an inline list")
    dependencies = [value.strip() for value in needs[1:-1].split(",")]
    assert sorted(dependencies) == ["native-build", "wasm-build"], dependencies
    assert field(deploy, "if", 4) == MAIN_PUSH
    for build in (native, wasm):
        # Builds must still run for PRs; neither failures nor skips may be hidden.
        assert not re.search(r"(?m)^    if:", build)
        assert "continue-on-error:" not in build
    assert "ctest --output-on-failure" in native
    assert "run: python3 tests/test_pages_workflow.py" in native
    assert "uses: actions/deploy-pages@v4" in deploy
    guards = re.findall(r"(?m)^      if: (.+)$", wasm)
    assert guards == [MAIN_PUSH, MAIN_PUSH], guards
    assert "uses: actions/upload-pages-artifact@v3" in wasm
    assert "        path: _site" in wasm
    triggers = block(text, "on", 0)
    assert re.search(r"(?m)^  pull_request:\s*$", triggers)
    assert "  push:\n    branches:\n      - main\n" in triggers
    permissions = block(text, "permissions", 0)
    assert permissions.strip() == "contents: read\n  pages: write\n  id-token: write"


class PagesWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflow = WORKFLOW.read_text()

    def test_workflow_contract(self):
        check_contract(self.workflow)

    def test_dependency_order_is_not_significant(self):
        check_contract(self.workflow.replace(
            "needs: [native-build, wasm-build]",
            "needs: [wasm-build, native-build]"))

    def test_rejects_gate_regressions(self):
        mutations = {
            "original_wasm_only": ("needs: [native-build, wasm-build]", "needs: wasm-build"),
            "missing_native": ("needs: [native-build, wasm-build]", "needs: [wasm-build]"),
            "missing_wasm": ("needs: [native-build, wasm-build]", "needs: [native-build]"),
            "unknown_job": ("needs: [native-build, wasm-build]", "needs: [native-tests, wasm-build]"),
            "bypass_success": (f"    if: {MAIN_PUSH}", f"    if: always() && ({MAIN_PUSH})"),
            "no_event_guard": (MAIN_PUSH, "github.ref == 'refs/heads/main'"),
            "no_branch_guard": (MAIN_PUSH, "github.event_name == 'push'"),
            "conditional_native": ("  native-build:\n", "  native-build:\n    if: false\n"),
            "conditional_wasm": ("  wasm-build:\n", "  wasm-build:\n    if: false\n"),
            "ignored_native_failure": ("  native-build:\n", "  native-build:\n    continue-on-error: true\n"),
            "ignored_test_failure": ("    - name: Run tests\n", "    - name: Run tests\n      continue-on-error: true\n"),
            "no_native_tests": ("ctest --output-on-failure", "echo skipped"),
            "no_pr_builds": ("  pull_request:\n", ""),
        }
        for name, (before, after) in mutations.items():
            with self.subTest(name=name):
                self.assertIn(before, self.workflow)
                with self.assertRaises(AssertionError):
                    check_contract(self.workflow.replace(before, after))


if __name__ == "__main__":
    unittest.main(verbosity=2)
