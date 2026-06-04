#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP="${ROOT_DIR}/build/packetlab"

if [[ ! -x "${APP}" ]]; then
  echo "packetlab binary not found. Run ./scripts/build.sh first."
  exit 1
fi

exec "${APP}"
