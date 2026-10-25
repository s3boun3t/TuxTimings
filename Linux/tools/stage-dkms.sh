#!/usr/bin/env bash
# Copy module sources only; no DKMS, installation, or kernel interaction.
set -euo pipefail
linux_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ${1:-} == --module && $# == 3 ]]; then
    source_dir=$2
    destination=$3
    mkdir -p -- "$destination"
    install -m644 -- "$source_dir/Makefile" "$source_dir/dkms.conf" \
        "$source_dir"/*.c "$source_dir"/*.h "$destination/"
elif [[ $# == 1 && -n $1 ]]; then
    for module in aod-voltages tuxbench; do
        version=$(sed -n 's/^PACKAGE_VERSION="\([0-9.]*\)"$/\1/p' "$linux_dir/src/$module/dkms.conf")
        [[ -n $version ]] || { echo "Missing module version: $module" >&2; exit 1; }
        bash "$0" --module "$linux_dir/src/$module" "$1/$module-$version"
    done
else
    echo "Usage: $0 DESTINATION | --module SOURCE DESTINATION" >&2
    exit 2
fi
