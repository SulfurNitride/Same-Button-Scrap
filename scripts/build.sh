#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE_DIR="${XDG_CACHE_HOME:-${HOME}/.cache}/vcpkg-scrap-with-same-button"
BUILDER_IMAGE="localhost/main-menu-video-player-builder:latest"

mkdir -p "${CACHE_DIR}"

if [[ ! -d "${PROJECT_DIR}/extern/CommonLibF4/lib/commonlib-shared" ]]; then
    echo "CommonLibF4 submodules are missing." >&2
    echo "Run: git -C extern/CommonLibF4 submodule update --init --recursive" >&2
    exit 2
fi

podman run --rm \
    --entrypoint /work/scripts/container-build.sh \
    -v "${PROJECT_DIR}:/work:Z" \
    -v "${CACHE_DIR}:/vcpkg-cache:Z" \
    "${BUILDER_IMAGE}"
