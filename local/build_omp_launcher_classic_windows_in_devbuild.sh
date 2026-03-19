#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
source_dir="$repo_root/omp-launcher-classic"
inject_helper_manifest="$repo_root/inject_helper/Cargo.toml"
notice_file="$repo_root/THIRD_PARTY_NOTICES.md"
optional_payloads=(
  "$repo_root/build-devbuild/client/samp.dll"
  "$repo_root/client/samp.dll"
  "$repo_root/samp.dll"
)

build_root="$repo_root/build-devbuild-win64"
rust_target="x86_64-pc-windows-gnu"
mingw_prefix="/usr/x86_64-w64-mingw32/sys-root/mingw"
runtime_dlls=(
  Qt6Core.dll
  Qt6Gui.dll
  Qt6Network.dll
  Qt6Svg.dll
  Qt6Widgets.dll
  libgcc_s_seh-1.dll
  libstdc++-6.dll
  libwinpthread-1.dll
  icudata76.dll
  icui18n76.dll
  icuuc76.dll
  libpcre2-16-0.dll
  libpcre2-8-0.dll
  zlib1.dll
  libfontconfig-1.dll
  libfreetype-6.dll
  libharfbuzz-0.dll
  libpng16-16.dll
  libbz2-1.dll
  libexpat-1.dll
  libglib-2.0-0.dll
  iconv.dll
  libintl-8.dll
  libcrypto-3-x64.dll
  libssl-3-x64.dll
)

build_dir="$build_root/omp-launcher-classic"
dist_dir="$build_root/omp-launcher-classic-dist"
inject_helper_target_dir="$build_root/inject-helper-target"
inject_helper_exe="$inject_helper_target_dir/$rust_target/release/omp-launcher-classic-inject-helper.exe"
toolchain_file="$mingw_prefix/lib/cmake/Qt6/qt.toolchain.cmake"
mingw_bin="$mingw_prefix/bin"
qt_plugin_root="$mingw_prefix/lib/qt6/plugins"

toolbox run --container devbuild bash -lc "
  set -euo pipefail
  rm -rf '$build_dir' '$dist_dir'
  mkdir -p \
    '$build_dir/platforms' \
    '$build_dir/iconengines' \
    '$build_dir/imageformats' \
    '$build_dir/tls' \
    '$dist_dir/platforms' \
    '$dist_dir/iconengines' \
    '$dist_dir/imageformats' \
    '$dist_dir/tls'

  cmake -G Ninja \
    -S '$source_dir' \
    -B '$build_dir' \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE='$toolchain_file'

  cmake --build '$build_dir' -j\$(nproc)

  cargo build \
    --release \
    --target '$rust_target' \
    --manifest-path '$inject_helper_manifest' \
    --target-dir '$inject_helper_target_dir'

  cp '$build_dir/omp-launcher-classic.exe' '$dist_dir/'
  cp '$inject_helper_exe' '$build_dir/'
  cp '$inject_helper_exe' '$dist_dir/'

  for dll in \
    ${runtime_dlls[*]}
  do
    cp '$mingw_bin/'\"\$dll\" '$build_dir/'
    cp '$mingw_bin/'\"\$dll\" '$dist_dir/'
  done

  cp '$qt_plugin_root/platforms/qwindows.dll' '$build_dir/platforms/'
  cp '$qt_plugin_root/platforms/qwindows.dll' '$dist_dir/platforms/'
  cp '$qt_plugin_root/iconengines/qsvgicon.dll' '$build_dir/iconengines/'
  cp '$qt_plugin_root/iconengines/qsvgicon.dll' '$dist_dir/iconengines/'
  cp '$qt_plugin_root/imageformats/qsvg.dll' '$build_dir/imageformats/'
  cp '$qt_plugin_root/imageformats/qsvg.dll' '$dist_dir/imageformats/'
  cp '$qt_plugin_root/tls/qcertonlybackend.dll' '$build_dir/tls/'
  cp '$qt_plugin_root/tls/qcertonlybackend.dll' '$dist_dir/tls/'
  cp '$qt_plugin_root/tls/qopensslbackend.dll' '$build_dir/tls/'
  cp '$qt_plugin_root/tls/qopensslbackend.dll' '$dist_dir/tls/'
  cp '$qt_plugin_root/tls/qschannelbackend.dll' '$build_dir/tls/'
  cp '$qt_plugin_root/tls/qschannelbackend.dll' '$dist_dir/tls/'
"

for payload in "${optional_payloads[@]}"; do
  if [[ -f "$payload" ]]; then
    cp "$payload" "$build_dir/"
    cp "$payload" "$dist_dir/"
  fi
done

cp "$notice_file" "$build_dir/"
cp "$notice_file" "$dist_dir/"

echo "Built exe: $build_dir/omp-launcher-classic.exe"
echo "Deployed folder: $dist_dir"
