#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 || ( "$1" != ubuntu && "$1" != windows ) ]]; then
  echo 'usage: stage-m5-tester.sh ubuntu|windows BUILD_DIR EMPTY_OUTPUT_DIR' >&2
  exit 2
fi

platform=$1
build_dir=$2
output_dir=$3
suffix=
if [[ $platform == windows ]]; then suffix=.exe; fi

desktop="$build_dir/ksp_desktop$suffix"
worker="$build_dir/ksp_worker$suffix"
guide=docs/TESTING.md

for required in "$desktop" "$worker" "$guide"; do
  if [[ ! -f $required || ! -s $required ]]; then
    echo "tester bundle input missing or empty: $required" >&2
    exit 1
  fi
done
command -v ldd >/dev/null || { echo 'ldd is required for dependency inspection' >&2; exit 1; }
commit=$(git rev-parse --verify HEAD)
if [[ ! $commit =~ ^[0-9a-f]{40}$ ]]; then
  echo 'source commit identifier unavailable' >&2
  exit 1
fi

# Scan before publishing. The record describes the CI build host, not a tester's machine.
dependency_record=$(mktemp)
trap 'rm -f "$dependency_record"' EXIT
for executable in "$desktop" "$worker"; do
  printf 'Executable: %s\n' "$(basename "$executable")" >> "$dependency_record"
  if ! ldd "$executable" >> "$dependency_record" 2>&1; then
    echo "dynamic dependency scan failed: $executable" >&2
    exit 1
  fi
  printf '\n' >> "$dependency_record"
done
if grep -Eiq 'not found|cannot find|could not find|no such file|error while loading' "$dependency_record"; then
  echo 'unresolved dynamic dependency in build environment' >&2
  cat "$dependency_record" >&2
  exit 1
fi

mkdir -p "$output_dir"
if [[ -n $(find "$output_dir" -mindepth 1 -maxdepth 1 -print -quit) ]]; then
  echo "tester output directory must be empty: $output_dir" >&2
  exit 1
fi
cp "$desktop" "$worker" "$output_dir/"
cp "$guide" "$output_dir/TESTING.md"
printf '%s\n' "$commit" > "$output_dir/source-commit.txt"
cp "$dependency_record" "$output_dir/runtime-dependencies.txt"

for staged in "$output_dir/ksp_desktop$suffix" "$output_dir/ksp_worker$suffix" \
  "$output_dir/TESTING.md" "$output_dir/source-commit.txt" \
  "$output_dir/runtime-dependencies.txt"; do
  if [[ ! -s $staged ]]; then
    echo "tester bundle output missing or empty: $staged" >&2
    exit 1
  fi
done
echo "staged $platform development-test bundle: $output_dir"
