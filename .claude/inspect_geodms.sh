#!/usr/bin/env bash
PID=$(ps -eo pid,comm | awk '/GeoDmsRun/ {print $1; exit}')
if [ -z "$PID" ]; then
    echo "GeoDmsRun not running"
    exit 0
fi
echo "PID=$PID comm=$(cat /proc/$PID/comm 2>/dev/null)"
echo "--- per-thread state/wchan (top 20 unique):"
for t in /proc/$PID/task/*; do
    tid=$(basename $t)
    state=$(awk '/^State:/ {print $2; exit}' $t/status 2>/dev/null)
    wchan=$(cat $t/wchan 2>/dev/null)
    echo "state=$state wchan=$wchan"
done | sort | uniq -c | sort -rn | head -20
echo
echo "--- gdb backtrace (main thread + 3 others):"
sudo gdb -batch -nx -p $PID \
    -ex 'set pagination off' \
    -ex 'thread apply 1-4 bt 20' 2>&1 | tail -100
