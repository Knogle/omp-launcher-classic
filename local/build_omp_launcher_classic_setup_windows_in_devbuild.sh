#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
script_path="$repo_root/nsis/omp-launcher-classic-setup.nsi"
icon_path="$repo_root/omp-launcher-classic/assets/samp_icon.ico"
version="working"
commit="$(git -C "$repo_root" rev-parse --short HEAD 2>/dev/null || echo unknown)"
build_root="$repo_root/build-devbuild-win64"
setup_suffix="win64"

dist_dir="$build_root/omp-launcher-classic-dist"
output_dir="$build_root"
output_file="$output_dir/omp-launcher-classic-${version}-${commit}-${setup_suffix}-setup.exe"

bash "$repo_root/local/build_omp_launcher_classic_windows_in_devbuild.sh"

toolbox run --container devbuild bash -lc "
  set -euo pipefail
  makensis \
    -DAPP_NAME='open.mp Classic' \
    -DAPP_VERSION='$version-$commit' \
    -DAPP_PUBLISHER='open.mp Classic' \
    -DDIST_DIR='$dist_dir' \
    -DOUT_FILE='$output_file' \
    -DAPP_ICON='$icon_path' \
    '$script_path'
"

echo "Built setup: $output_file"
