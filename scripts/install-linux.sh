#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
prefix=${1:-"${HOME:-$repo_root}/.local"}
bin_dir=$prefix/bin
data_dir=$prefix/share/project-oberon

mkdir -p "$bin_dir" "$data_dir"

make -C "$repo_root" oberon-run
cp "$repo_root/oberon-run" "$bin_dir/project-oberon-run-bin"
cp "$repo_root/RISC5Verilog/prom.mem" "$data_dir/prom.mem"

cat > "$bin_dir/project-oberon-run" <<'EOF'
#!/bin/sh
set -eu

bin_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
data_dir=$(CDPATH= cd -- "$bin_dir/../share/project-oberon" && pwd)
exec "$bin_dir/project-oberon-run-bin" --rom "$data_dir/prom.mem" "$@"
EOF
chmod +x "$bin_dir/project-oberon-run"

if command -v sdl2-config >/dev/null 2>&1; then
  make -C "$repo_root" ui
  cp "$repo_root/oberon-ui" "$bin_dir/project-oberon-ui-bin"
  cat > "$bin_dir/project-oberon-ui" <<'EOF'
#!/bin/sh
set -eu

bin_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
data_dir=$(CDPATH= cd -- "$bin_dir/../share/project-oberon" && pwd)
exec "$bin_dir/project-oberon-ui-bin" --rom "$data_dir/prom.mem" "$@"
EOF
  chmod +x "$bin_dir/project-oberon-ui"
  echo "Installed CLI and SDL UI to $prefix"
else
  rm -f "$bin_dir/project-oberon-ui" "$bin_dir/project-oberon-ui-bin"
  echo "Installed CLI to $prefix (SDL2 not found; UI skipped)"
fi

echo "Add $bin_dir to PATH if it is not already there."
