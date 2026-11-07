#!/bin/sh
set -eu

bundle_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$bundle_dir/oberon-run" --rom "$bundle_dir/prom.mem" "$@"
