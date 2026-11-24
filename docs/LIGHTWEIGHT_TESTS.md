# Lightweight validation (subset only)

From the repository root:

```sh
make test-lightweight
# Or, without Make (also works from another working directory):
python3 /path/to/gambit/scripts/run_lightweight_tests.py
```

Requires an existing Python 3.8+ and GCC/Clang-compatible C++17 compiler
(`c++` by default). No install, root CMake configuration, SDL/OpenGL, ENet,
spdlog, audio device, server or browser is needed. No dependencies are fetched.
Missing compiler, build failure, test failure or launch error returns nonzero.

## Exact coverage

The runner compiles and executes these existing test/source pairs:

| Executable | Test | Implementation |
| --- | --- | --- |
| CameraFog | `tests/test_camera_fog.cpp` | `src/FogOfWar.cpp` (camera is header-only) |
| MusicDirector | `tests/test_music_director.cpp` | `src/MusicDirector.cpp` |
| HitSounds | `tests/test_hit_sounds.cpp` | `src/HitSounds.cpp` |
| AbilitySounds | `tests/test_ability_sounds.cpp` | `src/AbilitySounds.cpp` |

Each run builds fresh binaries in a unique temporary directory beneath
`build/lightweight/` and removes it afterward, including on test failure.
It does not reuse stale CMake executables or reconfigure the full game.
Compilation enforces `-std=c++17 -Wall -Wextra -Werror -UNDEBUG` after extra
flags so test assertions stay enabled even with `CXXFLAGS=-DNDEBUG`.

Compiler overrides (shell-split commands, never executed through a shell):

```sh
CXX=clang++ make test-lightweight
CXXFLAGS='-O2 -DNDEBUG' make test-lightweight
python3 scripts/run_lightweight_tests.py --cxx 'ccache clang++' --cxxflags=-O2
```

Only `CXX` and `CXXFLAGS` are consulted; root CMake/vcpkg options and other
Make flags do not configure these standalone builds. GCC/Clang flags are not
portable to MSVC; this is not a replacement Windows build configuration.

## Optional integrated map audit tests

If `tools/maps/test_audit_coverage.py` exists (the independent #49 audit),
its standard-library unittest suite is included. Missing file prints
`NOT AVAILABLE (optional)` and the final summary reports zero map tests.
It is not bundled or copied from another patch. Once present, import errors,
failed tests, **skipped tests**, or zero discovered tests make the command
fail. A skip cannot silently turn into a success.

Other map tests are deliberately not auto-discovered: generator suites can
need NumPy/Pillow and are outside this dependency-free subset. Running the
optional audit tests is not the same as auditing or regenerating the real
planet; execute `tools/maps/audit_coverage.py` separately when available.

## Validate the runner itself

```sh
python3 -m unittest discover -s scripts/tests -p 'test_lightweight_runner.py' -v
```

These use small isolated fixtures and real compiler/subprocess execution:
success/absent optional suite, missing compiler, invalid C++, assertion failure
with `-DNDEBUG`, available map suite, failed map test, skipped map test, and
empty suite. No game files are modified; temporary fixtures live under
`build/lightweight-runner-tests/`.

## What success does not mean

Success covers only the named logic test executables and optional audit unit
tests. It does **not** validate the full application, networking, renderer,
audio playback, generated assets, real-map reachability, or WASM/browser.
Full native/WASM builds, CTest/integration runs and rendered/playtest checks
remain authoritative for their respective paths. Continue to use the existing
`make build`, `make test` and documented native/WASM workflows on configured
hosts; none of those commands is changed by this target.
