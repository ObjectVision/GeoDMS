#!/usr/bin/env bash
# CreateLinuxSetup.sh
# Creates a distributable package for GeoDMS Linux x64 from the CMake release build.
# Source: build/linux-x64-release/bin  (relative to repo root)
# Output: distr/GeoDms<version>-linux-x64.tar.gz          (tarball)
#         distr/GeoDms<version>-linux-x64.tar.gz.sha256   (checksum)
#         distr/GeoDms<version>-linux-x64.tar.gz.p7s      (CMS/PKCS#7 signature, if cert present)
#         distr/GeoDms<version>-linux-x64.deb             (if dpkg-deb is available)
#
# Usage (from repo root or nsi/ directory):
#   export GeoDmsVersion=19.5.0
#   bash nsi/CreateLinuxSetup.sh
#
# Signing (optional — needed once per machine):
#   Export the Object Vision GlobalSign OV certificate from the Windows
#   certificate store as a .pfx file (include private key), then convert:
#
#     openssl pkcs12 -in ObjectVision.pfx -out ~/.geodms-sign/cert.pem \
#             -clcerts -nokeys
#     openssl pkcs12 -in ObjectVision.pfx -out ~/.geodms-sign/key.pem  \
#             -nocerts -nodes
#     chmod 600 ~/.geodms-sign/key.pem
#
#   Or point to the files explicitly:
#     export GEODMS_SIGN_CERT=/path/to/cert.pem
#     export GEODMS_SIGN_KEY=/path/to/key.pem
#
#   The GlobalSign intermediate CA chain (needed for verification) can be
#   downloaded from GlobalSign's repository and stored as:
#     ~/.geodms-sign/chain.pem
#   or set GEODMS_SIGN_CHAIN=/path/to/chain.pem
#
#   Verification by recipients:
#     openssl cms -verify -binary \
#       -in  GeoDms19.5.0-linux-x64.tar.gz.p7s -inform DER \
#       -content GeoDms19.5.0-linux-x64.tar.gz \
#       -CAfile /etc/ssl/certs/ca-certificates.crt
#     # (GlobalSign root is in the standard system trust store on all major distros)

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

# Signing certificate locations — override via environment variables or
# place converted PEM files in ~/.geodms-sign/
SIGN_DIR="${HOME}/.geodms-sign"
SIGN_CERT="${GEODMS_SIGN_CERT:-${SIGN_DIR}/cert.pem}"
SIGN_KEY="${GEODMS_SIGN_KEY:-${SIGN_DIR}/key.pem}"
SIGN_CHAIN="${GEODMS_SIGN_CHAIN:-${SIGN_DIR}/chain.pem}"

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
[[ -f "${SRC}/profiler.py"   ]] && install -m 644 "${SRC}/profiler.py"   "${DST}/"
[[ -f "${SRC}/regression.py" ]] && install -m 644 "${SRC}/regression.py" "${DST}/"

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
# SHA256 checksum
# ---------------------------------------------------------------------------
SHA256FILE="${TARBALL}.sha256"
echo "Generating SHA256 checksum..."
(cd "${DISTR}" && sha256sum "${PKG_NAME}.tar.gz") > "${SHA256FILE}"
echo "  -> ${SHA256FILE}"
echo "     $(cat "${SHA256FILE}")"

# ---------------------------------------------------------------------------
# CMS/PKCS#7 signature using GlobalSign OV certificate
#
# Signs the SHA256 checksum file (not the tarball directly) — standard
# practice: a signed checksum is smaller to sign and sufficient to prove
# the tarball's integrity and origin.
#
# The resulting .p7s is a DER-encoded CMS detached signature verifiable
# with OpenSSL on any platform. GlobalSign's root CA is in every major
# OS trust store, so no extra CA file is needed on the recipient's side.
# ---------------------------------------------------------------------------
SIG="${SHA256FILE}.p7s"

if [[ -f "${SIGN_CERT}" && -f "${SIGN_KEY}" ]]; then
    echo "Signing checksum with GlobalSign OV certificate..."

    OPENSSL_ARGS=(
        cms -sign -binary -noattr
        -in  "${SHA256FILE}"
        -out "${SIG}"
        -outform DER
        -signer "${SIGN_CERT}"
        -inkey  "${SIGN_KEY}"
    )
    # Include intermediate CA chain if available (improves offline verification)
    if [[ -f "${SIGN_CHAIN}" ]]; then
        OPENSSL_ARGS+=(-certfile "${SIGN_CHAIN}")
    fi

    openssl "${OPENSSL_ARGS[@]}"
    echo "  -> ${SIG}"
    echo ""
    echo "  Recipients verify with:"
    echo "    openssl cms -verify -binary \\"
    echo "      -in  ${PKG_NAME}.tar.gz.sha256.p7s -inform DER \\"
    echo "      -content ${PKG_NAME}.tar.gz.sha256 \\"
    echo "      -CAfile /etc/ssl/certs/ca-certificates.crt"
    echo "    sha256sum -c ${PKG_NAME}.tar.gz.sha256"
else
    echo ""
    echo "  NOTE: Signing skipped — certificate files not found."
    echo "  To sign, export the Object Vision GlobalSign certificate as .pfx"
    echo "  and convert to PEM (see script header for instructions), then"
    echo "  place at:"
    echo "    ${SIGN_CERT}"
    echo "    ${SIGN_KEY}"
fi

# ---------------------------------------------------------------------------
# Create .deb package (if dpkg-deb is available)
# ---------------------------------------------------------------------------
if command -v dpkg-deb &>/dev/null; then
    echo ""
    echo "Creating .deb package..."

    # dpkg-deb requires the DEBIAN control directory to have permissions <=0775.
    # When the staging tree lives on a Windows-mounted filesystem (/mnt/...) the
    # NTFS driver ignores chmod, so we copy the staged tree to a native Linux
    # tmpdir before building.
    DEB_TMP=$(mktemp -d /tmp/geodms-deb-XXXXXX)
    cp -a "${STAGE}/." "${DEB_TMP}/"

    DEBIAN_DIR="${DEB_TMP}/DEBIAN"
    mkdir -p "${DEBIAN_DIR}"
    INSTALLED_SIZE=$(du -sk "${DST}" | cut -f1)

    cat > "${DEBIAN_DIR}/control" <<EOF
Package: geodms
Version: ${GeoDmsVersion}
Architecture: amd64
Maintainer: Object Vision B.V. <info@objectvision.nl>
Installed-Size: ${INSTALLED_SIZE}
Depends: libxcb-xinerama0, libxcb-icccm4, libxcb-image0, libxcb-keysyms1, libxcb-randr0, libxcb-render-util0, libxcb-xkb1, libxkbcommon-x11-0
Description: GeoDMS ${GeoDmsVersion} -- Geographic Data & Model Software
 GeoDMS is a software environment for the specification and calculation
 of geographic data models. This package contains the GUI and runtime.
EOF

    DEB="${DISTR}/${PKG_NAME}.deb"
    dpkg-deb --build "${DEB_TMP}" "${DEB}"
    rm -rf "${DEB_TMP}"
    echo "  -> ${DEB}"
else
    echo "  dpkg-deb not found — skipping .deb creation (tarball only)"
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo ""
echo "Done. Packages in ${DISTR}/"
ls -lh "${DISTR}/${PKG_NAME}"* 2>/dev/null
