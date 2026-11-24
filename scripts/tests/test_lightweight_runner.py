"""Runner integration tests using real compiler/subprocess failures, no game deps."""

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "run_lightweight_tests.py"
ROOT = SCRIPT.parents[1]


class LightweightRunnerTest(unittest.TestCase):
    def setUp(self):
        output = ROOT / "build" / "lightweight-runner-tests"
        output.mkdir(parents=True, exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=output)
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / "tests").mkdir()
        self.source = self.root / "tests" / "fixture.cpp"
        self.source.write_text("#include <cassert>\nint main() { assert(true); }\n")

    def run_fixture(self, compiler=None, flags="", python_test=None):
        if python_test is not None:
            directory = self.root / "tools" / "maps"
            directory.mkdir(parents=True)
            (directory / "test_audit_coverage.py").write_text(python_test)
        # Run in a new process, avoiding unittest discovery's module cache.
        code = f"""
import importlib.util, sys
from pathlib import Path
try:
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
except ImportError:
    pass
spec = importlib.util.spec_from_file_location('runner', {str(SCRIPT)!r})
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)
r.ROOT = Path({str(self.root)!r})
r.CPP_TESTS = (('Fixture', ('tests/fixture.cpp',)),)
sys.exit(r.main(['--cxx', {compiler or os.environ.get('CXX', 'c++')!r}, '--cxxflags={flags}']))
"""
        return subprocess.run([sys.executable, "-c", code], cwd=self.root,
                              text=True, capture_output=True)

    def test_pass_and_explicit_absent_audit(self):
        result = self.run_fixture()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("NOT AVAILABLE (optional)", result.stdout)
        self.assertIn("1 C++ executables, 0 map audit tests", result.stdout)

    def test_missing_compiler_fails(self):
        result = self.run_fixture(compiler="gambit-deliberately-nonexistent-cxx-64")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("required C++ compiler not found", result.stderr)

    def test_compile_error_fails(self):
        self.source.write_text("deliberate invalid C++\n")
        self.assertNotEqual(self.run_fixture().returncode, 0)

    def test_assertion_enabled_even_with_release_flags(self):
        self.source.write_text("#include <cassert>\nint main() { assert(false); }\n")
        result = self.run_fixture(flags="-DNDEBUG")
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("PASS lightweight subset", result.stdout)

    def test_available_audit_runs(self):
        result = self.run_fixture(python_test="import unittest\nclass Audit(unittest.TestCase):\n def test_ok(self): self.assertTrue(True)\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("1 map audit tests", result.stdout)

    def test_available_audit_failure_is_fatal(self):
        result = self.run_fixture(python_test="import unittest\nclass Audit(unittest.TestCase):\n def test_bad(self): self.fail('deliberate failure')\n")
        self.assertNotEqual(result.returncode, 0)

    def test_audit_skip_is_not_success(self):
        result = self.run_fixture(python_test="import unittest\n@unittest.skip('deliberate skip')\nclass Audit(unittest.TestCase):\n def test_skip(self): pass\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("skipped tests", result.stderr)

    def test_empty_available_audit_is_not_success(self):
        result = self.run_fixture(python_test="# No tests\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("zero tests", result.stderr)


if __name__ == "__main__":
    unittest.main()
