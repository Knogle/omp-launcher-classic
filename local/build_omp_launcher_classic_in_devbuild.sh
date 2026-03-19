#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
source_dir="$repo_root/omp-launcher-classic"
build_dir="$repo_root/build-devbuild/omp-launcher-classic"
notice_file="$repo_root/THIRD_PARTY_NOTICES.md"

mkdir -p "$build_dir"

toolbox run --container devbuild bash -lc "cmake -S '$source_dir' -B '$build_dir' -DCMAKE_BUILD_TYPE=Release"
toolbox run --container devbuild bash -lc "cmake --build '$build_dir' -j\$(nproc)"

cp "$notice_file" "$build_dir/"

echo "Built: $build_dir/omp-launcher-classic"
