#!/bin/bash
set -u
cd /mnt/c/dev/GeoDMS_2026 || exit 1
echo "=== nproc: $(nproc) ==="
echo "=== CMakeCache present: $(ls build/linux-x64-release/CMakeCache.txt 2>/dev/null) ==="
echo "=== Starting build $(date -u +%FT%TZ) ==="
cmake --build --preset linux-x64-release -j $(nproc)
RC=$?
echo "=== Build exit=$RC at $(date -u +%FT%TZ) ==="
ls -la build/linux-x64-release/bin/GeoDmsRun build/linux-x64-release/bin/libDmShv.so build/linux-x64-release/bin/GeoDmsGuiQt 2>&1
exit $RC
