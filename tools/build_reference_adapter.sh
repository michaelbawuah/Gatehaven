#!/usr/bin/env bash
set -euo pipefail
if [[ $# != 2 ]]; then
  echo "Usage: bash tools/build_reference_adapter.sh REFERENCE_CHECKOUT OUTPUT" >&2
  exit 2
fi
source_root=$(cd -- "$1" && pwd)
expected=19c00d0bd794c3dd558d501939e55f186f58c9e6
if [[ $(git -C "$source_root" rev-parse HEAD) != "$expected" ]]; then
  echo "Reference checkout is not the pinned compatibility target" >&2
  exit 1
fi
if [[ -n $(git -C "$source_root" status --porcelain --untracked-files=no) ]]; then
  echo "Reference checkout has modified tracked files" >&2
  exit 1
fi
script_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
read -r -a sdl_flags <<< "$(pkg-config --cflags sdl2)"
"${CXX:-c++}" -std=c++17 -O3 -DNDEBUG -pthread -include cstring -include stdexcept   "${sdl_flags[@]}" -I "$source_root/CircuitSandbox"   "$script_root/reference_adapter.cpp" "$source_root/CircuitSandbox/simulator.cpp" -o "$2"
