#!/bin/zsh
# bake-item-icon.command — render a 3D item icon THROUGH the real WickedEngine loop and
# write it to a PNG (WICKED-UI-03D in-loop icon bake). This is the engine-first bake: a
# RenderPath3D driven by wi::Application::Run() — exactly how the WE Editor renders its
# 3D asset previews (see Editor/CameraComponentWindow.cpp CameraPreview). The production
# IIconBakeSink will run this same render inside the live client.
#
# A window opens briefly, renders the item, writes the PNG, and quits on its own.
#
# Usage:  double-click (bakes the cube to ~/dmo-item-icon.png), OR from a terminal:
#   ./bake-item-icon.command                                   # cube -> ~/dmo-item-icon.png
#   ./bake-item-icon.command out.png                           # cube -> out.png
#   ./bake-item-icon.command out.png ../Content/models/DamagedHelmet.glb   # a real model

set -e
SCRIPT_DIR="${0:A:h}"
BUILD_DIR="${SCRIPT_DIR}/../../build-dmo-validation"
RUN_DIR="${BUILD_DIR}/Samples/DmoClientMac"

OUT="${1:-$HOME/dmo-item-icon.png}"
MODEL="${2:-builtin:cube}"

echo "==> Building DmoClientMac (incremental)…"
cmake --build "${BUILD_DIR}" --target DmoClientMac -j

echo "==> Baking item icon through the real engine loop…"
echo "    model  = ${MODEL}"
echo "    output = ${OUT}"
cd "${RUN_DIR}"                            # CWD must be the run dir (shader + Content paths are relative)
DMO_ITEM_CAPTURE="${OUT}" DMO_ITEM_BAKE_MODEL="${MODEL}" ./DmoClientMac

echo "==> Done. Wrote: ${OUT}"
