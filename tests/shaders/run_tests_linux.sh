#!/usr/bin/env bash
set -euo pipefail

# Helper script to copy Proton/vkd3d-proton DLLs and run Skyrim shader unit tests on Linux

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../../build-mingw/tests/shaders"

if [ ! -d "${BUILD_DIR}" ]; then
    echo "Error: Build directory not found: ${BUILD_DIR}"
    echo "Please build the project first:"
    echo "  cmake -B build-mingw -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DBUILD_PLUGIN=OFF -DBUILD_SHADER_TESTS=ON"
    echo "  cmake --build build-mingw --target shader_tests"
    exit 1
fi

# Try to find vkd3d-proton DLLs if they are not already in the build folder
if [ ! -f "${BUILD_DIR}/d3d12.dll" ] || [ ! -f "${BUILD_DIR}/d3d12core.dll" ]; then
    echo "vkd3d-proton DLLs not found in build directory. Searching for them..."
    
    # Common locations on Linux (Proton CachyOS, standard Steam, local Steam, etc.)
    CANDIDATES=(
        "/usr/share/steam/compatibilitytools.d/proton-cachyos/files/lib/wine/vkd3d-proton/x86_64-windows"
        "${HOME}/.local/share/Steam/steamapps/common/Proton - Experimental/files/lib64/wine/vkd3d-proton"
        "${HOME}/.steam/steam/steamapps/common/Proton - Experimental/files/lib64/wine/vkd3d-proton"
        "${HOME}/.steam/root/compatibilitytools.d/proton-cachyos/files/lib/wine/vkd3d-proton/x86_64-windows"
        "/usr/lib/wine/x86_64-windows"
    )
    
    FOUND_DIR=""
    # Check if user specified a directory via environment variable
    if [ -n "${PROTON_VKD3D_DIR:-}" ]; then
        if [ -d "${PROTON_VKD3D_DIR}" ] && [ -f "${PROTON_VKD3D_DIR}/d3d12.dll" ]; then
            FOUND_DIR="${PROTON_VKD3D_DIR}"
        fi
    fi
    
    if [ -z "${FOUND_DIR}" ]; then
        for DIR in "${CANDIDATES[@]}"; do
            if [ -d "${DIR}" ] && [ -f "${DIR}/d3d12.dll" ] && [ -f "${DIR}/d3d12core.dll" ]; then
                FOUND_DIR="${DIR}"
                break
            fi
        done
    fi
    
    if [ -z "${FOUND_DIR}" ]; then
        echo "Error: Could not locate vkd3d-proton DLLs (d3d12.dll and d3d12core.dll)."
        echo "Please specify their directory by setting the PROTON_VKD3D_DIR environment variable."
        echo "Example:"
        echo "  export PROTON_VKD3D_DIR=\"/path/to/vkd3d-proton/x86_64-windows\""
        echo "  ./run_tests_linux.sh"
        exit 1
    fi
    
    echo "Found vkd3d-proton DLLs at: ${FOUND_DIR}"
    echo "Copying to build output directory..."
    cp "${FOUND_DIR}/d3d12.dll" "${BUILD_DIR}/"
    cp "${FOUND_DIR}/d3d12core.dll" "${BUILD_DIR}/"
    cp "${FOUND_DIR}/d3d12core.dll" "${BUILD_DIR}/D3D12/D3D12Core.dll"
    cp "${FOUND_DIR}/d3d12core.dll" "${BUILD_DIR}/D3D12/d3d12core.dll"
fi

# Ensure lowercase symlinks and copies are present for Linux case-sensitivity
cd "${BUILD_DIR}"
ln -sf D3D12 d3d12

echo "Running shader unit tests via Wine..."
WINEDLLOVERRIDES="d3d12,d3d12core=n" wine ./shader_tests.exe "$@"
