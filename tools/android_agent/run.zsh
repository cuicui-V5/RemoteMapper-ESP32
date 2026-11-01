set -euo pipefail
source_dir="${0:A:h}"
task_root="${source_dir:h:h}"
export TMPDIR="$task_root/work/runtime"
export HF_HOME="$task_root/work/hf-cache"
export TIKTOKEN_CACHE_DIR="$task_root/work/tiktoken-cache"
export PYTHONPYCACHEPREFIX="$task_root/work/pycache"
export ADBUTILS_ADB_PATH="$task_root/work/deps/platform-tools/adb"
exec "$task_root/work/venv/bin/python" "$source_dir/launcher.py" \
  --state-dir "$task_root/work/android-state" \
  --work-dir "$task_root/work/live-agent" \
  --asr-model "$task_root/work/models/whisper-large-v3-turbo" \
  --allow-package com.android.settings "$@"
