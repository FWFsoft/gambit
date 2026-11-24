"""Exercise real GNUmakefile recipes without CMake or C++ dependencies."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
MAKE = shutil.which("make")


@unittest.skipUnless(MAKE, "GNU make is required")
class PortableBuildTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="gambit build test ")
        self.addCleanup(self.temp.cleanup)
        # Resolved: macOS temp dirs sit behind the /var -> /private/var link,
        # and the build script resolves symlinks.
        self.root = Path(self.temp.name).resolve()
        (self.root / "scripts").mkdir()
        (self.root / "build").mkdir()
        shutil.copy(ROOT / "GNUmakefile", self.root)
        shutil.copy(ROOT / "scripts/build.sh", self.root / "scripts")
        self.bin = self.root / "stub bin"
        self.bin.mkdir()
        self.log = self.root / "commands.jsonl"
        # env, not sys.executable: a shebang cannot hold a path with spaces
        # (uv's Python lives under "Application Support" on macOS).
        stub = "#!/usr/bin/env python3\n" + '''import json, os, pathlib, sys
name = pathlib.Path(sys.argv[0]).name
with open(os.environ["COMMAND_LOG"], "a") as log:
    log.write(json.dumps([name, *sys.argv[1:]]) + "\\n")
if name == "cmake":
    stage = "BUILD" if "--build" in sys.argv else "CONFIGURE"
else:
    stage = name.upper()
sys.exit(int(os.environ.get("FAIL_" + stage, "0")))
'''
        for name in ("cmake", "ctest", "lcov", "genhtml"):
            path = self.bin / name
            path.write_text(stub)
            path.chmod(0o755)
        self.env = os.environ.copy()
        for name in ("CMAKE_TOOLCHAIN_FILE", "VCPKG_ROOT", "MAKEFLAGS", "MFLAGS"):
            self.env.pop(name, None)
        self.env.update(PATH=str(self.bin) + os.pathsep + self.env["PATH"],
                        COMMAND_LOG=str(self.log))

    def toolchain(self, name="custom toolchain.cmake"):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
        return path

    def run_make(self, target="build", variables=(), **env):
        return subprocess.run([MAKE, "--no-print-directory", target, *variables],
                              cwd=self.root, env={**self.env, **env},
                              capture_output=True, text=True)

    def calls(self):
        return [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []

    def cache(self, toolchain=None):
        (self.root / "build/CMakeCache.txt").write_text(
            "# existing CMake cache\n" if toolchain is None else
            f"CMAKE_TOOLCHAIN_FILE:FILEPATH={toolchain}\n")

    def assert_configured(self, result, toolchain="", coverage=False):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        calls = self.calls()
        expected = ["cmake", "-S", ".", "-B", "build",
                    f"-DCMAKE_TOOLCHAIN_FILE:FILEPATH={toolchain}"]
        if coverage:
            expected.append("-DENABLE_COVERAGE=ON")
        self.assertEqual(calls[0], expected)
        self.assertEqual(calls[1], ["cmake", "--build", "build", "--parallel", "8"])
        if coverage:
            self.assertEqual([call[0] for call in calls[2:]],
                             ["ctest", "lcov", "lcov", "genhtml"])

    def test_system_fallback(self):
        self.assert_configured(self.run_make())

    def test_system_coverage(self):
        self.assert_configured(self.run_make("test-coverage"), coverage=True)

    def test_vcpkg_root_with_spaces_in_build(self):
        path = self.toolchain("vcpkg root/scripts/buildsystems/vcpkg.cmake")
        self.assert_configured(self.run_make(VCPKG_ROOT=str(path.parents[2])), path)

    def test_explicit_override_in_coverage(self):
        path = self.toolchain()
        self.assert_configured(self.run_make("test-coverage", CMAKE_TOOLCHAIN_FILE=str(path),
                                             VCPKG_ROOT="/not/used"), path, coverage=True)

    def test_vcpkg_root_with_spaces_in_coverage(self):
        path = self.toolchain("vcpkg root/scripts/buildsystems/vcpkg.cmake")
        self.assert_configured(self.run_make("test-coverage", VCPKG_ROOT=str(path.parents[2])),
                               path, coverage=True)

    def test_explicit_override_beats_vcpkg(self):
        path = self.toolchain()
        self.assert_configured(self.run_make(variables=(f"CMAKE_TOOLCHAIN_FILE={path}",),
                                             VCPKG_ROOT="/not/used"), path)

    def test_explicit_empty_beats_vcpkg(self):
        self.assert_configured(self.run_make(variables=("CMAKE_TOOLCHAIN_FILE=",),
                                             VCPKG_ROOT="/not/used"))

    def test_relative_environment_override(self):
        path = self.toolchain()
        self.assert_configured(self.run_make(CMAKE_TOOLCHAIN_FILE=path.name), path)

    def test_keep_cached_toolchain_when_unconfigured(self):
        path = self.toolchain()
        self.cache(path)
        self.assert_configured(self.run_make(), path)

    def test_same_cached_toolchain_allowed_for_coverage(self):
        path = self.toolchain()
        self.cache(path)
        self.assert_configured(self.run_make("test-coverage", CMAKE_TOOLCHAIN_FILE=str(path)),
                               path, coverage=True)

    def test_cached_system_provider_retained(self):
        self.cache()
        self.assert_configured(self.run_make())

    def test_cached_vcpkg_switch_rejected_in_coverage(self):
        self.cache(self.toolchain())
        path = self.toolchain("new root/scripts/buildsystems/vcpkg.cmake")
        result = self.run_make("test-coverage", VCPKG_ROOT=str(path.parents[2]))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("make clean", result.stderr)
        self.assertEqual(self.calls(), [])

    def test_no_cached_toolchain_to_requested_toolchain_rejected(self):
        path = self.toolchain()
        self.cache()
        result = self.run_make(CMAKE_TOOLCHAIN_FILE=str(path))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("make clean", result.stderr)
        self.assertEqual(self.calls(), [])

    def test_cached_toolchain_switch_and_system_rejected(self):
        path = self.toolchain()
        other = self.toolchain("other.cmake")
        self.cache(path)
        for selected in ("", str(other)):
            with self.subTest(selected=selected):
                result = self.run_make(CMAKE_TOOLCHAIN_FILE=selected)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("make clean", result.stderr)
                self.assertEqual(self.calls(), [])

    def test_missing_explicit_or_vcpkg_path_rejected(self):
        for env in ({"CMAKE_TOOLCHAIN_FILE": "/missing/file"},
                    {"VCPKG_ROOT": "/missing/root"}):
            with self.subTest(env=env):
                result = self.run_make(**env)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Toolchain file not found", result.stderr)
                self.assertEqual(self.calls(), [])

    def test_configure_and_build_failures_stop_coverage(self):
        for stage, count in (("CONFIGURE", 1), ("BUILD", 2)):
            with self.subTest(stage=stage):
                self.log.unlink(missing_ok=True)
                result = self.run_make("test-coverage", **{"FAIL_" + stage: "17"})
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Error 17", result.stderr)
                self.assertEqual(len(self.calls()), count)

    def test_ctest_failure_stops_report(self):
        result = self.run_make("test-coverage", FAIL_CTEST="23")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Error 23", result.stderr)
        self.assertEqual([call[0] for call in self.calls()], ["cmake", "cmake", "ctest"])


if __name__ == "__main__":
    unittest.main()
