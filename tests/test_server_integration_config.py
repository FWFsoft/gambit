#!/usr/bin/env python3
"""Registration regression, not a game/server integration test.

Run: python3 tests/test_server_integration_config.py -v
Uses the production CMake module in a dependency-free fixture. Real configure /
CTest checks are skipped explicitly when CMake/CTest or a POSIX shell is absent.
"""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "cmake" / "ServerIntegrationTests.cmake"
OPTION = "GAMBIT_ENABLE_SERVER_INTEGRATION_TESTS"


class RegistrationContract(unittest.TestCase):
    def test_production_wiring_and_default(self):
        source = (ROOT / "CMakeLists.txt").read_text()
        module = MODULE.read_text()
        self.assertIn("include(cmake/ServerIntegrationTests.cmake)", source)
        self.assertNotIn("add_test(NAME HeadlessMovement", source)
        self.assertRegex(module, rf"option\({OPTION}\s+\"[^\"]+\"\s+OFF\)")
        self.assertIn(f"if({OPTION})", module)
        self.assertNotIn("DISABLED", module)

    @unittest.skipUnless(shutil.which("cmake") and shutil.which("ctest")
                         and os.name == "posix",
                         "requires CMake, CTest and POSIX shell; no game dependencies")
    def test_default_on_and_back_off(self):
        with tempfile.TemporaryDirectory(prefix="gambit-ctest-") as tmp:
            # Resolved: macOS temp dirs sit behind /var -> /private/var, and
            # `pwd` in the stub reports the real path.
            root = Path(tmp).resolve()
            source, build = root / "source", root / "build"
            source.mkdir()
            marker = source / "executed.txt"
            stub = source / "headless-stub"
            stub.write_text('#!/bin/sh\npwd > executed.txt\n')
            stub.chmod(0o755)
            # Include the real registration module, not a copied condition.
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.16)\n'
                'project(RegistrationFixture NONE)\n'
                'enable_testing()\n'
                'add_executable(test_headless_movement IMPORTED)\n'
                f'set_target_properties(test_headless_movement PROPERTIES '
                f'IMPORTED_LOCATION "{stub.as_posix()}")\n'
                f'include("{MODULE.as_posix()}")\n'
            )

            def run(*args, cwd=None):
                return subprocess.run(args, cwd=cwd, check=True, text=True,
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE).stdout

            def configure(value=None):
                args = ["cmake", "-S", str(source), "-B", str(build)]
                if value is not None:
                    args.append(f"-D{OPTION}={value}")
                run(*args)
                return json.loads(run("ctest", "--show-only=json-v1",
                                      cwd=build))["tests"]

            self.assertEqual(configure(), [])  # actual default, no override
            run("ctest", "--output-on-failure", cwd=build)
            self.assertFalse(marker.exists())
            tests = configure("ON")
            self.assertEqual([test["name"] for test in tests], ["HeadlessMovement"])
            properties = {p["name"]: p["value"] for p in tests[0]["properties"]}
            self.assertFalse(properties.get("DISABLED", False))
            self.assertEqual(set(properties["LABELS"]), {"integration", "manual"})
            self.assertEqual(Path(properties["WORKING_DIRECTORY"]), source)
            run("ctest", "-L", "integration", "--output-on-failure", cwd=build)
            self.assertEqual(Path(marker.read_text().strip()), source)
            marker.unlink()
            self.assertEqual(configure("OFF"), [])  # no stale ON registration
            run("ctest", "--output-on-failure", cwd=build)
            self.assertFalse(marker.exists())


if __name__ == "__main__":
    unittest.main()
