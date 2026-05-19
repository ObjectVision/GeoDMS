#!/bin/bash
set -u
cd /mnt/c/dev/GeoDMS_2026 || exit 1
export GeoDmsVersion=20.0.0
export GeoDmsFlavor=l
# Skip signing -- no token here.
export GEODMS_SIGN_THUMBPRINT=

echo "=== Stage + package $(date -u +%FT%TZ) ==="
bash nsi/CreateLinuxSetup.sh
RC=$?
echo "=== CreateLinuxSetup.sh exit=$RC ==="
[ $RC -ne 0 ] && exit $RC

DEB="distr/GeoDms20.0.0.l-linux-x64.deb"
if [ ! -f "$DEB" ]; then
    echo "no .deb produced -- falling back to direct copy"
    sudo rm -rf /opt/ObjectVision/GeoDms20.0.0.l
    sudo mkdir -p /opt/ObjectVision
    sudo cp -a distr/GeoDms20.0.0.l-linux-x64/opt/ObjectVision/GeoDms20.0.0.l /opt/ObjectVision/
    INSTALL_RC=$?
else
    echo "=== Removing old install ==="
    sudo rm -rf /opt/ObjectVision/GeoDms20.0.0.l
    echo "=== Installing $DEB ==="
    sudo dpkg -i "$DEB" 2>&1
    INSTALL_RC=$?
    if [ $INSTALL_RC -ne 0 ]; then
        echo "dpkg failed -- falling back to direct copy"
        sudo cp -a distr/GeoDms20.0.0.l-linux-x64/opt/ObjectVision/GeoDms20.0.0.l /opt/ObjectVision/
        INSTALL_RC=$?
    fi
fi
echo "=== Install exit=$INSTALL_RC at $(date -u +%FT%TZ) ==="

echo "=== Installed binary timestamps ==="
ls -la /opt/ObjectVision/GeoDms20.0.0.l/GeoDmsRun /opt/ObjectVision/GeoDms20.0.0.l/libDmShv.so /opt/ObjectVision/GeoDms20.0.0.l/GeoDmsGuiQt 2>&1
echo "=== profiler scripts ==="
ls -la /opt/ObjectVision/GeoDms20.0.0.l/profiler/ 2>&1
exit $INSTALL_RC
