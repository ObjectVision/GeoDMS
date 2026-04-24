#!/usr/bin/env bash
# CreateLinuxSetup.sh
# Creates a distributable package for GeoDMS Linux x64 from the CMake release build.
# Source: build/linux-x64-release/bin  (relative to repo root)
# Output: distr/GeoDms<version>-linux-x64.tar.gz
#         distr/GeoDms<version>-linux-x64.deb  (if dpkg-deb is available)
#
# Usage (from repo root or nsi/ directory):
#   export GeoDmsVersion=19.5.0
#   bash nsi/CreateLinuxSetup.sh
# Or:
#   GeoDmsVersion=19.5.0 bash nsi/CreateLinuxSetup.sh

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

: "${GeoDmsVersion:?GeoDmsVersion must be set, e.g. export GeoDmsVersion=19.5.0}"

SRC="${REPO_ROOT}/build/linux-x64-release/bin"
DISTR="${REPO_ROOT}/distr"
PKG_NAME="GeoDms${GeoDmsVersion}-linux-x64"
STAGE="${DISTR}/${PKG_NAME}"
INSTALL_PREFIX="/opt/ObjectVision/GeoDms${GeoDmsVersion}"

if [[ ! -d "${SRC}" ]]; then
    echo "ERROR: Source directory not found: ${SRC}"
    echo "       Run 'cmake --build --preset linux-x64-release' first."
    exit 1
fi

mkdir -p "${DISTR}"

# ---------------------------------------------------------------------------
# Stage files
# ---------------------------------------------------------------------------

echo "Staging files to ${STAGE}..."
rm -rf "${STAGE}"
mkdir -p "${STAGE}${INSTALL_PREFIX}"

DST="${STAGE}${INSTALL_PREFIX}"

# Executables
install -m 755 "${SRC}/GeoDmsGuiQt"   "${DST}/"
install -m 755 "${SRC}/GeoDmsRun"     "${DST}/"

# GeoDMS shared libraries
install -m 755 "${SRC}"/libDm*.so     "${DST}/"

# Scripts
install -m 644 "${SRC}/RewriteExpr.lsp" "${DST}/"
install -m 644 "${SRC}/profiler.py"     "${DST}/"
install -m 644 "${SRC}/regression.py"   "${DST}/"

# Geographic data
cp -r "${SRC}/gdaldata"   "${DST}/"
cp -r "${SRC}/proj4data"  "${DST}/"

# DMS library scripts and examples
cp -r "${SRC}/library"    "${DST}/"
cp -r "${SRC}/examples"   "${DST}/"

# Fonts
cp -r "${SRC}/misc"       "${DST}/"

# Qt plugins
for plugin_dir in platforms imageformats xcbglintegrations iconengines tls networkinformation generic styles; do
    if [[ -d "${SRC}/${plugin_dir}" ]]; then
        cp -r "${SRC}/${plugin_dir}" "${DST}/"
    fi
done

# ---------------------------------------------------------------------------
# qt.conf — tell Qt to find plugins relative to the executable
# ---------------------------------------------------------------------------
cat > "${DST}/qt.conf" <<EOF
[Paths]
Prefix = .
EOF

# ---------------------------------------------------------------------------
# Launcher wrapper — sets LD_LIBRARY_PATH so libDm*.so are found
# ---------------------------------------------------------------------------
cat > "${DST}/geodms" <<'EOF'
#!/usr/bin/env bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="${DIR}:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${DIR}"
exec "${DIR}/GeoDmsGuiQt" "$@"
EOF
chmod +x "${DST}/geodms"

# ---------------------------------------------------------------------------
# .desktop file
# ---------------------------------------------------------------------------
APPS_DIR="${STAGE}/usr/share/applications"
mkdir -p "${APPS_DIR}"
cat > "${APPS_DIR}/geodms-${GeoDmsVersion}.desktop" <<EOF
[Desktop Entry]
Name=GeoDMS ${GeoDmsVersion}
Comment=Geographic Data & Model Software
Exec=${INSTALL_PREFIX}/geodms
Icon=${INSTALL_PREFIX}/misc/fonts/dms.ttf
Terminal=false
Type=Application
Categories=Science;Education;
EOF

# ---------------------------------------------------------------------------
# Create tarball
# ---------------------------------------------------------------------------
TARBALL="${DISTR}/${PKG_NAME}.tar.gz"
echo "Creating tarball ${TARBALL}..."
tar -czf "${TARBALL}" -C "${DISTR}" "${PKG_NAME}"
echo "  -> ${TARBALL}"

# ---------------------------------------------------------------------------
# Create .deb package (if dpkg-deb is available)
# ---------------------------------------------------------------------------
if command -v dpkg-deb &>/dev/null; then
    echo "Creating .deb package..."

    # DEBIAN control file
    DEBIAN_DIR="${STAGE}/DEBIAN"
    mkdir -p "${DEBIAN_DIR}"
    INSTALLED_SIZE=$(du -sk "${DST}" | cut -f1)

    cat > "${DEBIAN_DIR}/control" <<EOF
Package: geodms
Version: ${GeoDmsVersion}
Architecture: amd64
Maintainer: Object Vision B.V. <info@objectvision.nl>
Installed-Size: ${INSTALLED_SIZE}
Depends: libxcb-xinerama0, libxcb-icccm4, libxcb-image0, libxcb-keysyms1, libxcb-randr0, libxcb-render-util0, libxcb-xkb1, libxkbcommon-x11-0
Description: GeoDMS ${GeoDmsVersion} — Geographic Data & Model Software
 GeoDMS is a software environment for the specification and calculation
 of geographic data models. This package contains the GUI and runtime.
EOF

    DEB="${DISTR}/${PKG_NAME}.deb"
    dpkg-deb --build "${STAGE}" "${DEB}"
    echo "  -> ${DEB}"
else
    echo "  dpkg-deb not found — skipping .deb creation (tarball only)"
fi

echo ""
echo "Done. Packages in ${DISTR}/"
ls -lh "${DISTR}/${PKG_NAME}"* 2>/dev/null
