#!/bin/zsh
# run-3d-probe.command — double-clickable shortcut to run the WickedEngine 3D render
# sanity check on this Mac (WICKED-UI-03D icon-bake render-bug diagnosis).
#
# It builds DmoClientMac, then launches it in DMO_3D_PROBE mode: a windowed
# RenderPath3D showing a lit cube (or a model you pass as $1), driven by the real
# wi::Application::Run() frame loop — the same loop the working 2D UI uses.
#
#   • A shaded cube appears  -> the engine's 3D path renders fine on Metal; the
#                               icon-bake bug is in our headless hand-rolled pump.
#   • The window is black    -> 3D-on-Metal is broken engine-wide (bigger fix).
#
# Usage:  double-click this file, OR from a terminal:
#   ./run-3d-probe.command                                  # lit cube
#   ./run-3d-probe.command ../Content/models/DamagedHelmet.glb   # a real model
#
# Close the window (or Cmd-Q) to quit.

set -e

# Resolve the engine root from this script's own location (works when double-clicked).
SCRIPT_DIR="${0:A:h}"
ENGINE_DIR="${SCRIPT_DIR}/../.."          # external/WickedEngine
BUILD_DIR="${ENGINE_DIR}/build-dmo-validation"
RUN_DIR="${BUILD_DIR}/Samples/DmoClientMac"

echo "==> Building DmoClientMac (incremental)…"
cmake --build "${BUILD_DIR}" --target DmoClientMac -j

echo "==> Launching 3D probe…"
cd "${RUN_DIR}"                            # CWD must be the run dir (shader + Content paths are relative)

MODEL_ARG="${1:-builtin:cube}"
echo "    model = ${MODEL_ARG}"
DMO_3D_PROBE=1 DMO_ITEM_BAKE_MODEL="${MODEL_ARG}" ./DmoClientMac

echo "==> Probe window closed."
