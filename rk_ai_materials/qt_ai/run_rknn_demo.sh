#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 8 ]]; then
  echo "usage: $0 <source_root> <model_path> <label_path> <class_count> <box_threshold> <input_image> <output_dir> <build_dir>" >&2
  exit 64
fi

SOURCE_ROOT=$1
MODEL_PATH=$2
LABEL_PATH=$3
CLASS_COUNT=$4
BOX_THRESHOLD=$5
INPUT_IMAGE=$6
OUTPUT_DIR=$7
BUILD_DIR=$8
TARGET_SOC=${TARGET_SOC:-rk3588}

if [[ ! -d "$SOURCE_ROOT" ]]; then
  echo "ERROR: missing source root: $SOURCE_ROOT" >&2
  exit 2
fi

if [[ ! -f "$MODEL_PATH" ]]; then
  echo "ERROR: missing model file: $MODEL_PATH" >&2
  exit 2
fi

if [[ ! -f "$LABEL_PATH" ]]; then
  echo "ERROR: missing label file: $LABEL_PATH" >&2
  exit 2
fi

if [[ ! "$CLASS_COUNT" =~ ^[1-9][0-9]*$ ]]; then
  echo "ERROR: invalid class count: $CLASS_COUNT" >&2
  exit 2
fi

if [[ ! "$BOX_THRESHOLD" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
  echo "ERROR: invalid box threshold: $BOX_THRESHOLD" >&2
  exit 2
fi

if [[ ! -f "$INPUT_IMAGE" ]]; then
  echo "ERROR: missing input image: $INPUT_IMAGE" >&2
  exit 2
fi

ARCH=$(uname -m)
if [[ "$ARCH" != "aarch64" ]]; then
  echo "ERROR: RKNN demo only runs on RK3588/aarch64, current arch: $ARCH" >&2
  exit 3
fi

PATCH_ROOT="$BUILD_DIR/source"
PATCH_MAIN="$PATCH_ROOT/src/main.cc"
PATCH_POSTPROCESS="$PATCH_ROOT/src/postprocess.cc"
PATCH_HEADER="$PATCH_ROOT/include/postprocess.h"
PATCH_BUILD="$PATCH_ROOT/build_qt_ai"
RUN_INPUT_DIR="$BUILD_DIR/input_image"
LOG_FILE="$BUILD_DIR/last_run.log"
RKNN_EXTRA_LIBS="$PATCH_ROOT/3rdparty/rknpu2/Linux/aarch64:$PATCH_ROOT/rknn_lib"

mkdir -p "$PATCH_ROOT" "$PATCH_BUILD" "$RUN_INPUT_DIR" "$OUTPUT_DIR"
cp -a "$SOURCE_ROOT/." "$PATCH_ROOT/"

escape_sed() {
  printf '%s' "$1" | sed -e 's/[\/&]/\&/g'
}

MODEL_ESC=$(escape_sed "$MODEL_PATH")
LABEL_ESC=$(escape_sed "$LABEL_PATH")
INPUT_ESC=$(escape_sed "$RUN_INPUT_DIR")
OUTPUT_ESC=$(escape_sed "$OUTPUT_DIR")
THRESHOLD_ESC=$(escape_sed "$BOX_THRESHOLD")

sed -i -E "s|const std::string modelPath = ".*";|const std::string modelPath = "$MODEL_ESC";|" "$PATCH_MAIN"
sed -i -E "s|const std::string imageFolder = ".*";|const std::string imageFolder = "$INPUT_ESC";|" "$PATCH_MAIN"
sed -i -E "s|const std::string outputFolder = ".*";|const std::string outputFolder = "$OUTPUT_ESC";|" "$PATCH_MAIN"
sed -i -E "s|#define LABEL_NALE_TXT_PATH ".*".*|#define LABEL_NALE_TXT_PATH "$LABEL_ESC"|" "$PATCH_POSTPROCESS"
sed -i -E "s|#define OBJ_CLASS_NUM [0-9]+|#define OBJ_CLASS_NUM $CLASS_COUNT|" "$PATCH_HEADER"
sed -i -E "s|#define BOX_THRESH [0-9.]+|#define BOX_THRESH $THRESHOLD_ESC|" "$PATCH_HEADER"

find "$RUN_INPUT_DIR" -maxdepth 1 -type f -delete
INPUT_BASENAME=$(basename "$INPUT_IMAGE")
cp -f "$INPUT_IMAGE" "$RUN_INPUT_DIR/$INPUT_BASENAME"

OUTPUT_BASENAME="${INPUT_BASENAME%.*}_out.png"
OUTPUT_IMAGE="$OUTPUT_DIR/$OUTPUT_BASENAME"
rm -f "$OUTPUT_IMAGE"

(
  cd "$PATCH_BUILD"
  cmake -DTARGET_SOC="$TARGET_SOC" .. >/dev/null
  make -j"$(nproc)" >/dev/null
)

export LD_LIBRARY_PATH="$RKNN_EXTRA_LIBS:${LD_LIBRARY_PATH:-}"

set +e
"$PATCH_BUILD/rknn_yolov8_demo" >"$LOG_FILE" 2>&1
STATUS=$?
set -e

cat "$LOG_FILE"

if [[ $STATUS -ne 0 ]]; then
  exit $STATUS
fi

if [[ ! -f "$OUTPUT_IMAGE" ]]; then
  echo "ERROR: result image not generated: $OUTPUT_IMAGE" >&2
  exit 4
fi

echo "RESULT_IMAGE=$OUTPUT_IMAGE"

DETECTION_COUNT=0
while IFS= read -r line; do
  if [[ "$line" == *" @ ("* ]]; then
    echo "DETECTION_LINE=$line"
    DETECTION_COUNT=$((DETECTION_COUNT + 1))
  fi
done < "$LOG_FILE"

echo "DETECTION_COUNT=$DETECTION_COUNT"
