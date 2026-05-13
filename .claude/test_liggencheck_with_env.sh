#!/bin/bash
# Test only the LiggenErPuntenBinnenStudyArea integrity check item, with full env.
set -u

# Env vars exactly as t641_1 was run.
export GEODMS_OVERRIDABLE_RegressionTestsSourceDataDir=/mnt/f/SourceData/RegressionTests
export RegressionTestsAltSourceDataDir=/mnt/f/SourceData
export GEODMS_DIRECTORIES_LOCALDATADIR=/mnt/c/LocalData
export GEODMS_directories_SourceDataDir=/mnt/f/SourceData
export LocalDataDirRegression=/mnt/c/LocalData/regression
export tmpFileDir=/mnt/c/LocalData/regression/log
export GEODMS_Overridable_RSo_DataDir=/mnt/f/SourceData/RSOpen
export GEODMS_Overridable_RSo_PrivDataDir=/mnt/f/SourceData/RSOpen_Priv
export GEODMS_DIRECTORIES_LOCALDATAPROJDIR=/mnt/c/LocalData/regression/RSopen_RegressieTest_v2025
export UseFence=TRUE
export AlleenEindjaar=FALSE
export VariantDataOntkoppeld=FALSE

CFG="/mnt/f/SourceData/RegressionTests/prj_snapshots/RSopen_RegressieTest_v2025H2_wLB/cfg/Regression_test.dms"
LOG="/mnt/c/dev/tst/Regression/GeoDMSTestResults/20_0_0_l_x64_SF_S1S2S3_OVSRV10/log/t641_1_LiggenCheck_only_v2.txt"
rm -f "$LOG"

echo "=== launching GeoDmsRun on the IntegrityCheck item only ==="
/opt/ObjectVision/GeoDms20.0.0.l/GeoDmsRun \
    /L"$LOG" /S1 /S2 /S3 /SH \
    "$CFG" \
    "WritePrivData/LISA/FSS/LiggenErPuntenBinnenStudyArea"
RC=$?
echo "=== exit=$RC ==="
echo
echo "=== ErrorLevel lines, last 5 lines of log ==="
grep -E 'ErrorLevel|LiggenErPunten|Highest CommitCharge|@@@@@' "$LOG"
