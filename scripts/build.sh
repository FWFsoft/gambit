#!/bin/sh
# Shared configure/build path for normal and coverage builds. Run at repo root.
set -eu

absolute_toolchain() {
    # Require a real file and normalize relative paths before entering build/.
    if [ ! -f "$1" ]; then
        echo "Toolchain file not found: $1" >&2
        exit 1
    fi
    directory=$(dirname "$1")
    filename=$(basename "$1")
    printf '%s/%s\n' "$(cd "$directory" && pwd -P)" "$filename"
}

selected=0
toolchain=
if [ "${CMAKE_TOOLCHAIN_FILE+x}" = x ]; then
    selected=1
    toolchain=$CMAKE_TOOLCHAIN_FILE
elif [ -n "${VCPKG_ROOT:-}" ]; then
    selected=1
    toolchain=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
fi
if [ -n "$toolchain" ]; then
    toolchain=$(absolute_toolchain "$toolchain")
fi

if [ -f build/CMakeCache.txt ]; then
    cached=$(sed -n 's/^CMAKE_TOOLCHAIN_FILE:[^=]*=//p' build/CMakeCache.txt)
    if [ -n "$cached" ]; then
        # CMake can cache a path relative to its build directory.
        if [ ! -f "$cached" ] && [ -f "build/$cached" ]; then
            cached=build/$cached
        fi
        cached=$(absolute_toolchain "$cached")
    fi
    if [ "$selected" = 0 ]; then
        # An ordinary incremental build retains its configured toolchain.
        toolchain=$cached
    elif [ "$cached" != "$toolchain" ]; then
        echo 'Toolchain differs from build/CMakeCache.txt; run make clean first (removes build/).' >&2
        exit 1
    fi
fi

# An explicit empty value selects system packages, including when VCPKG_ROOT
# is exported. Never silently reuse/switch an incompatible configured cache.
cmake -S . -B build "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=$toolchain" "$@"
cmake --build build --parallel 8
