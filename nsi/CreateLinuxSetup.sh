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
# Signing (optional — requires the Object Vision SafeNet hardware token):
#   The private key never leaves the token; signing is done via PowerShell's
#   .NET SignedCms API which drives the token through Windows CNG/CSP.
#   This script must be run from WSL2 on the Windows machine that has the
#   token plugged in.  You will be prompted for the token PIN.
#
#   The signing certificate thumbprint is hard-coded as GEODMS_SIGN_THUMBPRINT
#   (default: E6E0FE67472C3A0DB879E19F8C797DB61645D9DE).  Override:
#     export GEODMS_SIGN_THUMBPRINT=<other-thumbprint>
#   Disable signing entirely:
#     export GEODMS_SIGN_THUMBPRINT=
#
#   Verification by recipients:
#     openssl cms -verify -binary -purpose any \
#       -in  GeoDms19.5.0-linux-x64.tar.gz.sha256.p7s -inform DER \
#       -content GeoDms19.5.0-linux-x64.tar.gz.sha256 \
#       -CAfile GlobalSign-CodeSigning-Root-R45.pem
#     sha256sum -c GeoDms19.5.0-linux-x64.tar.gz.sha256
#   (GlobalSign-CodeSigning-Root-R45.pem is produced alongside the tarball)

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

# Signing — Object Vision GlobalSign EV Code Signing certificate
# The private key lives on a SafeNet hardware token (non-exportable).
# Signing is performed via PowerShell's .NET SignedCms API, which drives the
# token through the Windows CNG/CSP layer — no key export required.
#
# Override the certificate thumbprint via GEODMS_SIGN_THUMBPRINT, or set it
# to the empty string to skip signing.
SIGN_THUMBPRINT="${GEODMS_SIGN_THUMBPRINT:-E6E0FE67472C3A0DB879E19F8C797DB61645D9DE}"

# GlobalSign Code Signing Root R45 — not in all distros' default trust stores,
# so we download it once and ship it alongside the tarball.
GS_ROOT_URL="http://secure.globalsign.com/cacert/codesigningrootr45.crt"
GS_ROOT_PEM="${DISTR}/GlobalSign-CodeSigning-Root-R45.pem"

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
# Signing is done via powershell.exe (accessible from WSL2) using the .NET
# SignedCms API, which drives the SafeNet hardware token through Windows CNG
# without exporting the private key.  The resulting .p7s is a DER-encoded
# CMS detached signature.
#
# The GlobalSign Code Signing Root R45 is not in all distros' default CA
# bundles, so we also fetch it once and ship it alongside the tarball.
# Recipients verify with:
#   openssl cms -verify -binary -purpose any \
#     -in  <pkg>.tar.gz.sha256.p7s -inform DER \
#     -content <pkg>.tar.gz.sha256 \
#     -CAfile GlobalSign-CodeSigning-Root-R45.pem
#   sha256sum -c <pkg>.tar.gz.sha256
# ---------------------------------------------------------------------------
SIG="${SHA256FILE}.p7s"

if [[ -n "${SIGN_THUMBPRINT}" ]] && command -v powershell.exe &>/dev/null; then
    echo "Signing checksum with GlobalSign EV certificate (token: ${SIGN_THUMBPRINT})..."

    # Windows path for the SHA256 file (needed by PowerShell)
    SHA256_WIN=$(wslpath -w "${SHA256FILE}")
    SIG_WIN=$(wslpath -w "${SIG}")

    powershell.exe -NoProfile -Command "
Add-Type -AssemblyName 'System.Security'
\$cert = Get-Item ('Cert:\CurrentUser\My\\${SIGN_THUMBPRINT}')
\$bytes  = [System.IO.File]::ReadAllBytes('${SHA256_WIN}')
\$ci     = New-Object System.Security.Cryptography.Pkcs.ContentInfo(,\$bytes)
\$cms    = New-Object System.Security.Cryptography.Pkcs.SignedCms(\$ci, \$true)
\$signer = New-Object System.Security.Cryptography.Pkcs.CmsSigner(\$cert)
\$signer.IncludeOption = [System.Security.Cryptography.X509Certificates.X509IncludeOption]::ExcludeRoot
\$cms.ComputeSignature(\$signer)
[System.IO.File]::WriteAllBytes('${SIG_WIN}', \$cms.Encode())
Write-Host ('  -> ${SIG_WIN} (' + (Get-Item '${SIG_WIN}').Length + ' bytes)')
"

    # Download GlobalSign Code Signing Root R45 if not already present
    if [[ ! -f "${GS_ROOT_PEM}" ]]; then
        echo "Fetching GlobalSign Code Signing Root R45..."
        if command -v curl &>/dev/null; then
            curl -fsSL "${GS_ROOT_URL}" | openssl x509 -inform DER -out "${GS_ROOT_PEM}"
        elif command -v wget &>/dev/null; then
            wget -qO- "${GS_ROOT_URL}" | openssl x509 -inform DER -out "${GS_ROOT_PEM}"
        else
            echo "  WARNING: curl/wget not found — root cert not downloaded."
        fi
        [[ -f "${GS_ROOT_PEM}" ]] && echo "  -> ${GS_ROOT_PEM}"
    fi

    echo ""
    echo "  Recipients verify with:"
    echo "    openssl cms -verify -binary -purpose any \\"
    echo "      -in  ${PKG_NAME}.tar.gz.sha256.p7s -inform DER \\"
    echo "      -content ${PKG_NAME}.tar.gz.sha256 \\"
    echo "      -CAfile GlobalSign-CodeSigning-Root-R45.pem"
    echo "    sha256sum -c ${PKG_NAME}.tar.gz.sha256"
else
    echo ""
    echo "  NOTE: Signing skipped."
    if [[ -z "${SIGN_THUMBPRINT}" ]]; then
        echo "  Set GEODMS_SIGN_THUMBPRINT to the certificate thumbprint to enable signing."
    else
        echo "  powershell.exe not found — run this script from WSL2 on a Windows host."
    fi
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
