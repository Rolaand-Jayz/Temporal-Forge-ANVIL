#!/usr/bin/env bash
set -euo pipefail

ROOT=/mnt/workdrive
FORGE="$ROOT/Temporal-Forge-Player"
RE="$ROOT/RE-of-FSR-4.1.0-Upscaling"

mkdir -p "$ROOT"
cd "$ROOT"

if [[ ! -d "$FORGE/.git" ]]; then
  git clone https://github.com/Rolaand-Jayz/Temporal-Forge-Player.git "$FORGE"
else
  git -C "$FORGE" remote -v
  git -C "$FORGE" fetch --all --prune
fi

if [[ ! -d "$RE/.git" ]]; then
  git clone https://github.com/Rolaand-Jayz/RE-of-FSR-4.1.0-Upscaling.git "$RE"
else
  git -C "$RE" remote -v
  git -C "$RE" fetch --all --prune
fi

echo
echo "Temporal Forge:"
git -C "$FORGE" status --short --branch
git -C "$FORGE" rev-parse HEAD

echo
echo "FSR RE evidence repository:"
git -C "$RE" status --short --branch
git -C "$RE" rev-parse HEAD

cat <<'EOF'

IMPORTANT:
- Temporal-Forge-Player is the mutable working repository for this goal.
- RE-of-FSR-4.1.0-Upscaling is evidence-only unless the maintainer separately authorizes edits.
- Work only under /mnt/workdrive for this campaign.
EOF
