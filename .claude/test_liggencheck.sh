#!/bin/bash
set -u
CFG="/mnt/f/SourceData/RegressionTests/prj_snapshots/RSopen_RegressieTest_v2025H2_wLB/cfg/Regression_test.dms"
LOG="/mnt/c/dev/tst/Regression/GeoDMSTestResults/20_0_0_l_x64_SF_S1S2S3_OVSRV10/log/t641_1_LiggenCheck_only.txt"
rm -f "$LOG"
/opt/ObjectVision/GeoDms20.0.0.l/GeoDmsRun /L"$LOG" /S1 /S2 /S3 /SH "$CFG" "WritePrivData/LISA/FSS/LiggenErPuntenBinnenStudyArea"
echo "exit=$?"
echo "--- last 30 lines of log:"
tail -30 "$LOG"
