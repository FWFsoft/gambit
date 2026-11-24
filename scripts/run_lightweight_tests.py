#!/usr/bin/env python3
"""Standalone, standard-library-only validation; NOT a full game build."""

import argparse
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP_TESTS = (
    ("CameraFog", ("tests/test_camera_fog.cpp", "src/FogOfWar.cpp")),
    ("MusicDirector", ("tests/test_music_director.cpp", "src/MusicDirector.cpp")),
    ("HitSounds", ("tests/test_hit_sounds.cpp", "src/HitSounds.cpp")),
    ("AbilitySounds", ("tests/test_ability_sounds.cpp", "src/AbilitySounds.cpp")),
)


def run_cpp(root, compiler, flags, output):
    command = shlex.split(compiler)
    if not command or shutil.which(command[0]) is None:
        raise RuntimeError(f"required C++ compiler not found: {compiler!r}")
    output.mkdir(parents=True, exist_ok=True)
    # Unique directory prevents stale binaries from being mistaken for passes
    # and allows independent/concurrent runs. Test execution never needs CMake.
    with tempfile.TemporaryDirectory(prefix="run-", dir=output) as tmp:
        for name, sources in CPP_TESTS:
            binary = Path(tmp) / name
            args = command + shlex.split(flags) + [
                "-std=c++17", "-Wall", "-Wextra", "-Werror", "-UNDEBUG",
                "-I", str(root / "include"), "-I", str(root / "tests"),
                *(str(root / source) for source in sources), "-o", str(binary),
            ]
            print(f"BUILD {name}: {shlex.join(args)}", flush=True)
            subprocess.run(args, cwd=root, check=True)
            print(f"RUN {name}", flush=True)
            subprocess.run([str(binary)], cwd=root, check=True)
    return len(CPP_TESTS)


def run_python(root):
    directory = root / "tools" / "maps"
    filename = "test_audit_coverage.py"
    if not (directory / filename).is_file():
        print(f"NOT AVAILABLE (optional): tools/maps/{filename}; no map audit tests ran", flush=True)
        return 0
    # Deliberately do not discover every map test: generator tests can need
    # NumPy/Pillow. This exact suite is the stdlib-only #49 coverage audit.
    sys.path.insert(0, str(directory))
    try:
        suite = unittest.defaultTestLoader.discover(str(directory), pattern=filename)
        result = unittest.TextTestRunner(verbosity=2).run(suite)
    finally:
        sys.path.pop(0)
    if not result.wasSuccessful() or result.skipped or result.testsRun == 0:
        raise RuntimeError("available map audit suite failed, skipped tests, or ran zero tests")
    return result.testsRun


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"),
                        help="compiler command (default: CXX or c++)")
    parser.add_argument("--cxxflags", default=os.environ.get("CXXFLAGS", ""),
                        help="extra flags; warnings/C++17/assertions are enforced afterward")
    args = parser.parse_args(argv)
    try:
        count = run_cpp(ROOT, args.cxx, args.cxxflags, ROOT / "build" / "lightweight")
        python_count = run_python(ROOT)
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"FAIL lightweight validation: {error}", file=sys.stderr)
        return 1
    print(f"PASS lightweight subset: {count} C++ executables, {python_count} map audit tests. "
          "Full native/WASM builds and integration/render tests remain required.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
