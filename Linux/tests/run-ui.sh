#!/usr/bin/env bash
# Real GTK, injected backend only. No main.c, elevation, module or benchmark.
set -euo pipefail
ui_test=$1
if [[ -n ${UI_CAPTURE_DIR:-} ]]; then
    mkdir -p -- "$UI_CAPTURE_DIR"
    captures=$(mktemp -d "$UI_CAPTURE_DIR/tuxtimings-ui.XXXXXX")
    echo "Synthetic GTK captures: $captures"
else
    captures=$(mktemp -d /tmp/tuxtimings-ui-captures.XXXXXX)
    trap 'rm -rf -- "$captures"' EXIT
fi
{
    printf 'Synthetic GTK fixtures; no hardware accessed.\n'
    printf 'Revision: '
    git -C "$(dirname "$0")/../.." rev-parse HEAD 2>/dev/null || printf 'archive\n'
    printf 'GTK: '; pkg-config --modversion gtk4
    printf 'Backend: %s; default GDK_SCALE: %s\n' "${GDK_BACKEND:-default}" "${GDK_SCALE:-1}"
} > "$captures/manifest.txt"
scenarios=(ddr4 ddr5 unknown unavailable ratio fgr5 associated temp-zero temp-error
           temp-missing temp-ambiguous io-loss zero lpddr5 configured narrow long)
for scenario in "${scenarios[@]}"; do
    GSK_RENDERER=cairo "$ui_test" "$scenario" "$captures/$scenario.png" >> "$captures/manifest.txt" 2>&1
done
GSK_RENDERER=cairo GDK_SCALE=2 "$ui_test" ddr4 "$captures/hidpi.png" >> "$captures/manifest.txt" 2>&1
printf 'HiDPI: GDK_SCALE=2\n18 synthetic GTK runs passed\n' >> "$captures/manifest.txt"
echo '18 synthetic GTK runs passed'
