#!/usr/bin/env bash
# package.sh — build (optional) and create a distributable zip for NexusMods/mod managers.
# Requires bash 4+ (mapfile), GNU grep (-P), and 7z.
# Auto-build (without --build-dir) requires Linux (uses release-linux preset).
# Git Bash on Windows is supported with --build-dir pointing at an existing build.
#
# Usage: ./scripts/package.sh [--build-dir <dir>] [version]
#   --build-dir  skip cmake build; search <dir> for the DLL instead of build/release-linux
#   version      optional, e.g. 1.0.0 (default: exact git tag or CMakeLists.txt version)

set -euo pipefail

build_dir=""
positional=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            if [[ -z "${2:-}" ]]; then
                echo "Error: --build-dir requires a value"
                exit 1
            fi
            build_dir="$2"; shift 2 ;;
        --*) echo "Error: unknown option: $1"; exit 1 ;;
        *) positional+=("$1"); shift ;;
    esac
done

if [[ ${#positional[@]} -gt 1 ]]; then
    echo "Error: unexpected arguments: ${positional[*]:1}"
    exit 1
fi

if [[ ! -f "CMakeLists.txt" ]]; then
    echo "Error: must be run from the project root (e.g. ./scripts/package.sh)"
    exit 1
fi

if ! command -v 7z &>/dev/null; then
    echo "Error: 7z is not installed"
    exit 1
fi

if [[ ${#positional[@]} -gt 0 ]]; then
    version="${positional[0]}"
elif git_tag=$(git describe --tags --exact-match 2>/dev/null); then
    version="${git_tag#v}"
else
    version=$(grep -oP '^\s+VERSION\s+\K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | head -1 || true)
    if [[ -z "$version" ]]; then
        echo "Error: could not determine version from CMakeLists.txt"
        exit 1
    fi
fi

if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.]+)?(\+[a-zA-Z0-9.]+)?$ ]]; then
    echo "Error: invalid version '${version}' — expected X.Y.Z or X.Y.Z-pre or X.Y.Z+build"
    exit 1
fi

if [[ -z "$build_dir" ]]; then
    echo "Building..."
    cmake --preset release-linux
    cmake --build --preset release-linux
    build_dir="build/release-linux"
fi

project_name=$(grep -oP 'PROPERTIES OUTPUT_NAME \K[\w-]+' CMakeLists.txt || true)
if [[ -z "$project_name" ]]; then
    echo "Error: could not determine DLL name from CMakeLists.txt (OUTPUT_NAME not found)"
    exit 1
fi

mapfile -t dlls < <(find "$build_dir" -maxdepth 2 -type f -name "${project_name}.dll")
if [[ ${#dlls[@]} -ne 1 ]]; then
    echo "Error: expected exactly 1 DLL in ${build_dir}, found ${#dlls[@]}"
    exit 1
fi
dll="${dlls[0]}"

name=$(basename "$dll" .dll)
zip_name="dist/${name}-${version}.zip"

mkdir -p dist

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/SKSE/Plugins"
cp "$dll" "$tmp/SKSE/Plugins/"
cp "HoldFast.ini" "$tmp/SKSE/Plugins/"

# Multi-config generators put the DLL in <binaryDir>/<Config>, one level below the cache.
CMAKE_DIR="${build_dir}"
if [[ ! -f "${CMAKE_DIR}/CMakeCache.txt" ]]; then
    CMAKE_DIR=$(dirname "${build_dir}")
fi
if [[ ! -f "${CMAKE_DIR}/CMakeCache.txt" ]]; then
    echo "Error: no CMakeCache.txt in ${build_dir} or its parent"
    exit 1
fi

VCPKG_INSTALLED=$(grep -oP '^VCPKG_INSTALLED_DIR:PATH=\K.*' "${CMAKE_DIR}/CMakeCache.txt" || true)
VCPKG_TRIPLET=$(grep -oP '^VCPKG_TARGET_TRIPLET:STRING=\K.*' "${CMAKE_DIR}/CMakeCache.txt" || true)
if [[ -z "${VCPKG_INSTALLED}" || -z "${VCPKG_TRIPLET}" ]]; then
    echo "Error: could not read vcpkg install dir or triplet from ${CMAKE_DIR}/CMakeCache.txt"
    exit 1
fi
if command -v cygpath &>/dev/null; then
    VCPKG_INSTALLED=$(cygpath -u "${VCPKG_INSTALLED}")
fi
VCPKG_SHARE="${VCPKG_INSTALLED}/${VCPKG_TRIPLET}/share"

# vcpkg-* ports are host build helpers and are not linked into the DLL.
mapfile -t COPYRIGHTS < <(find "${VCPKG_SHARE}" -mindepth 2 -maxdepth 2 -name copyright ! -path '*/share/vcpkg-*/*' | sort)
if [[ ${#COPYRIGHTS[@]} -eq 0 ]]; then
    echo "Error: no vcpkg copyright files found in ${VCPKG_SHARE}"
    exit 1
fi

if ! SOURCE_URL=$(git remote get-url origin 2>/dev/null); then
    echo "Error: no git remote 'origin'; it is needed for the source URL in the notices"
    exit 1
fi
SOURCE_URL=$(sed -E 's#^(ssh://)?git@github\.com[:/]#https://github.com/#; s#\.git$##' <<< "${SOURCE_URL}")

COMMONLIB_DIR=$(git config -f .gitmodules submodule.lib/commonlibsse-ng.path || true)
COMMONLIB_URL=$(git config -f .gitmodules submodule.lib/commonlibsse-ng.url | sed -E 's#\.git$##' || true)
SKSE_MCP_DIR=$(git config -f .gitmodules submodule.lib/skse-mcp.path || true)
SKSE_MCP_URL=$(git config -f .gitmodules submodule.lib/skse-mcp.url | sed -E 's#\.git$##' || true)
if [[ -z "${COMMONLIB_DIR}" || -z "${COMMONLIB_URL}" || -z "${SKSE_MCP_DIR}" || -z "${SKSE_MCP_URL}" ]]; then
    echo "Error: could not read CommonLibSSE-NG or skse-mcp submodule entries from .gitmodules"
    exit 1
fi
COMMONLIB_LICENSE=$(grep -oP '"license":\s*"\K[^"]+' "${COMMONLIB_DIR}/vcpkg.json" || true)
if [[ -z "${COMMONLIB_LICENSE}" || ! -f "${COMMONLIB_DIR}/EXCEPTIONS.md" || ! -f "${COMMONLIB_DIR}/licenses/README.md" ]]; then
    echo "Error: could not read CommonLibSSE-NG license, EXCEPTIONS.md or licenses/README.md from ${COMMONLIB_DIR}"
    exit 1
fi
mapfile -t COMMONLIB_EXTRA_LICENSES < <(find "${COMMONLIB_DIR}/licenses" -maxdepth 1 -name 'LICENSE-*.txt' | sort)
if [[ ${#COMMONLIB_EXTRA_LICENSES[@]} -eq 0 ]]; then
    echo "Error: no LICENSE-*.txt files found in ${COMMONLIB_DIR}/licenses"
    exit 1
fi

DOCS_DIR="${tmp}/SKSE/Plugins/${name}"
mkdir -p "${DOCS_DIR}"
cp LICENSE "${DOCS_DIR}/LICENSE.txt"

{
    printf '%s is licensed under the GNU General Public License v3.0 or later (see LICENSE.txt).\n' "${name}"
    printf 'Source code: %s\n' "${SOURCE_URL}"
    printf '\nIt includes the following third-party software.\n'
    printf '\n==== CommonLibSSE-NG ====\n\n'
    printf '%s\n' "${COMMONLIB_URL}"
    printf 'Licensed under %s with the additional permissions in EXCEPTIONS.md.\n' "${COMMONLIB_LICENSE}"
    printf 'The files after it are its bundled code and legacy license notices.\n'
    printf '\n---- EXCEPTIONS.md ----\n\n'
    cat "${COMMONLIB_DIR}/EXCEPTIONS.md"
    printf '\n---- licenses/README.md ----\n\n'
    cat "${COMMONLIB_DIR}/licenses/README.md"
    for EXTRA in "${COMMONLIB_EXTRA_LICENSES[@]}"; do
        printf '\n---- %s ----\n\n' "$(basename "${EXTRA}")"
        cat "${EXTRA}"
    done
    for COPYRIGHT in "${COPYRIGHTS[@]}"; do
        printf '\n==== %s ====\n\n' "$(basename "$(dirname "${COPYRIGHT}")")"
        cat "${COPYRIGHT}"
    done
    printf '\n==== skse-mcp ====\n\n%s\n\n' "${SKSE_MCP_URL}"
    cat "${SKSE_MCP_DIR}/LICENSE"
} > "${DOCS_DIR}/THIRD-PARTY-NOTICES.txt"
(cd "$tmp" && 7z a -tzip out.zip SKSE > /dev/null)
mv "$tmp/out.zip" "$zip_name"

echo ""
echo "Package: ${zip_name}"

if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
    echo "zip=${zip_name}" >> "$GITHUB_OUTPUT"
fi
