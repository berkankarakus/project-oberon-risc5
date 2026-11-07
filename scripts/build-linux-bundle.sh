#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_dir=${1:-"$repo_root/dist/project-oberon-linux"}

mkdir -p "$output_dir"
make -C "$repo_root" oberon-run
cp "$repo_root/oberon-run" "$output_dir/oberon-run"
cp "$repo_root/RISC5Verilog/prom.mem" "$output_dir/prom.mem"
cp "$repo_root/scripts/run-linux-bundle.sh" "$output_dir/run.sh"
cp "$repo_root/emulator/README.md" "$output_dir/README.md"
chmod +x "$output_dir/run.sh"

printf 'Linux bundle ready: %s\n' "$output_dir"
