#!/usr/bin/env bash
set -euo pipefail

find include src tests \( -iname '*.h' -o -iname '*.hpp' -o -iname '*.cpp' \) -print0 \
  | xargs -0 -r clang-format -i
