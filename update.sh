#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
case ${1:-} in
    --help|-h)
        cat <<'EOF'
Usage: ./update.sh [--keep-untracked]
Fast-forward the current branch from its configured upstream.
Refuses detached HEAD, missing upstream, divergence, tracked edits and untracked
files (including ignored files). Branches and local files are never reset,
switched, stashed or deleted.
--keep-untracked is accepted for compatibility; preservation is always enabled.
Build and install separately after reviewing the update.
EOF
        exit 0 ;;
    --keep-untracked) shift ;;
esac
[[ $# == 0 ]] || { echo "Unknown option; use --help." >&2; exit 2; }
command -v git >/dev/null || { echo "git is not installed." >&2; exit 1; }
git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1 || {
    echo "This is not a git clone; obtain a source archive instead." >&2; exit 1;
}
branch=$(git -C "$ROOT_DIR" symbolic-ref --quiet --short HEAD) || {
    echo "Detached HEAD: choose a branch explicitly before updating." >&2; exit 1;
}
if [[ -n $(git -C "$ROOT_DIR" status --porcelain --untracked-files=all) ||
      -n $(git -C "$ROOT_DIR" ls-files --others) ]]; then
    echo "Local changes or untracked files: preserve them before updating." >&2
    exit 1
fi
upstream=$(git -C "$ROOT_DIR" rev-parse --abbrev-ref --symbolic-full-name '@{upstream}') || {
    echo "No upstream for $branch; configure it explicitly." >&2; exit 1;
}
remote=$(git -C "$ROOT_DIR" config --get "branch.$branch.remote")
merge_ref=$(git -C "$ROOT_DIR" config --get "branch.$branch.merge")
printf '==> Updating %s from %s (fast-forward only)...\n' "$branch" "$upstream"
git -C "$ROOT_DIR" fetch -- "$remote" "$merge_ref"
git -C "$ROOT_DIR" merge --ff-only -- "$upstream"
echo '==> Update complete. Review, build and install separately.'
