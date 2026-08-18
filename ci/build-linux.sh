#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JUCE_PATH="${GUITARDSP_JUCE_PATH:-${HOME}/JUCE}"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build-ci}"
JOBS="${JOBS:-2}"

if [[ ! -f "${JUCE_PATH}/CMakeLists.txt" ]]; then
  echo "JUCE not found at: ${JUCE_PATH}" >&2
  echo "Set GUITARDSP_JUCE_PATH=/path/to/JUCE" >&2
  exit 2
fi

cmake -S "${ROOT}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DGUITARDSP_JUCE_PATH="${JUCE_PATH}"

cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

echo
printf 'Build OK: %s\n' "${BUILD_DIR}"
