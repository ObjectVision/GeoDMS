#!/usr/bin/env bash
# CreateLinuxSetup.sh
# Creates the distributable packages for GeoDMS Linux x64 from the CMake release build.
# Source: build/linux-x64-release/bin  (relative to repo root)
# Output: distr/GeoDms<version>.<flavor>-linux-x64.tar.gz          (tarball)
#         distr/GeoDms<version>.<flavor>-linux-x64.tar.gz.sha256   (checksum)
#         distr/GeoDms<version>.<flavor>-linux-x64.tar.gz.sha256.p7s (CMS/PKCS#7 signature, if cert present)
#         distr/GeoDms<version>.<flavor>-linux-x64.deb             (if dpkg-deb is available)
#
# The .deb and the .tar.gz are assembled from ONE staged tree in the same run
# and carry the identical install payload under /opt/ObjectVision/GeoDms<ver>.<flavor>/;
# a verification step diffs the two file lists and asserts the critical runtime
# files are present, failing the build otherwise. The only intended difference:
# the tarball additionally contains <pkg>/VERIFY.md (signature-verification
# instructions -- meaningful next to the tarball, pointless inside the .deb,
# which used to install it as /VERIFY.md at the filesystem root).
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
#   Verification by recipients (root CA must be fetched from GlobalSign directly,
#   NOT from the release page — see nsi/VERIFY-LINUX.md for the fingerprint):
#     curl -fsSL http://secure.globalsign.com/cacert/codesigningrootr45.crt \
#       | openssl x509 -inform DER -out GlobalSign-CodeSigning-Root-R45.pem
#     openssl cms -verify -binary -purpose any \
#       -in  GeoDms19.5.0-linux-x64.tar.gz.sha256.p7s -inform DER \
#       -content GeoDms19.5.0-linux-x64.tar.gz.sha256 \
#       -CAfile GlobalSign-CodeSigning-Root-R45.pem
#     sha256sum -c GeoDms19.5.0-linux-x64.tar.gz.sha256

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

: "${GeoDmsVersion:?GeoDmsVersion must be set, e.g. export GeoDmsVersion=20.0.0}"

# Flavor suffix — "l" for linux, so the produced package and install path
# co-exist with the Windows msbuild (m) and cmake (c) outputs of the same
# version. Override via GeoDmsFlavor in the calling environment if needed.
GeoDmsFlavor="${GeoDmsFlavor:-l}"

SRC="${REPO_ROOT}/build/linux-x64-release/bin"
DISTR="${REPO_ROOT}/distr"
PKG_NAME="GeoDms${GeoDmsVersion}.${GeoDmsFlavor}-linux-x64"
INSTALL_PREFIX="/opt/ObjectVision/GeoDms${GeoDmsVersion}.${GeoDmsFlavor}"

# Signing — Object Vision GlobalSign EV Code Signing certificate
# The private key lives on a SafeNet hardware token (non-exportable).
# Signing is performed via PowerShell's .NET SignedCms API, which drives the
# token through the Windows CNG/CSP layer — no key export required.
#
# Override the certificate thumbprint via GEODMS_SIGN_THUMBPRINT, or set it
# to the empty string to skip signing. Note the colon-less ${VAR-default}:
# with ${VAR:-default} an exported-but-EMPTY value would fall back to the
# default thumbprint too, making the documented opt-out drive the SafeNet
# token (and pop its PIN dialog) anyway.
SIGN_THUMBPRINT="${GEODMS_SIGN_THUMBPRINT-E6E0FE67472C3A0DB879E19F8C797DB61645D9DE}"

# NOTE: the GlobalSign Code Signing Root R45 is NOT distributed with the release.
# Recipients must fetch it independently from GlobalSign to prevent a compromised
# release page from substituting both the signature and the CA file.
# See nsi/VERIFY-LINUX.md for the expected fingerprint and fetch instructions.

if [[ ! -d "${SRC}" ]]; then
    echo "ERROR: Source directory not found: ${SRC}"
    echo "       Run 'cmake --build --preset linux-x64-release' first."
    exit 1
fi

mkdir -p "${DISTR}"

# ---------------------------------------------------------------------------
# Single authoritative staging tree — on the WSL-NATIVE filesystem.
#
# Both the .tar.gz and the .deb are assembled from THIS one tree, in this one
# run, and verified against each other below. Rationale (the 20.7.0 profiler/
# incident): the tree used to be staged under distr/ on the Windows drvfs
# mount (/mnt/c) and the .deb was then built from a SECOND, later copy of it
# (cp -a into /tmp — needed because chmod does not work on drvfs). Under
# heavy IO load the 9p/drvfs layer can return truncated directory listings
# WITHOUT reporting an error, so that second copy silently dropped the whole
# profiler/ dir from the .deb while the tarball — created minutes earlier,
# before the load — was complete. Every .l regression test then failed with
# return_code 127 because <install>/profiler/run_with_sampler.sh was missing.
# Staging natively removes the second lossy hop (dpkg-deb builds directly
# from the stage), and the verify step fails the build if the two payloads
# ever diverge or a critical file is missing.
# ---------------------------------------------------------------------------
PKG_TMP=$(mktemp -d /tmp/geodms-pkg-XXXXXX)
trap 'rm -rf "${PKG_TMP}"' EXIT
STAGE="${PKG_TMP}/${PKG_NAME}"
DST="${STAGE}${INSTALL_PREFIX}"

# Older script versions staged under distr/<pkg-name>/ on the Windows mount.
# Remove any leftover so a stale tree cannot be mistaken for the content of
# the current packages (the tarball itself is the inspectable stage now).
rm -rf "${DISTR:?}/${PKG_NAME}"

echo "Staging files to ${STAGE}..."
mkdir -p "${DST}"

# Executables
install -m 755 "${SRC}/GeoDmsGuiQt"   "${DST}/"
install -m 755 "${SRC}/GeoDmsRun"     "${DST}/"

# GeoDMS shared libraries
install -m 755 "${SRC}"/libDm*.so     "${DST}/"

# Bundle the Qt6 runtime + its less-ubiquitous dependencies so the install is
# self-contained and runs on machines without a (matching) system Qt6 -- e.g. the
# headless OVSRV05 test server (#1137). The Qt *plugins* (platforms/, imageformats/,
# ...) are staged further below; here we add the core libraries they and the
# executable link against. Resolved via ldd from THIS build so the bundled libs
# always match the bundled plugins. Base libs (glibc, libstdc++, glib, freetype,
# png, zstd) are intentionally left to the host -- present on any desktop/server
# distro, and bundling them risks ABI conflicts with the host's own software.
echo "Bundling Qt6 runtime libraries..."
QT_BUNDLE_RE='/(libQt6[A-Za-z]+|libicudata|libicui18n|libicuuc|libdouble-conversion|libpcre2-16|libmd4c|libb2|libgraphite2|libharfbuzz)\.so'
_qt_libs=$( { ldd "${SRC}/GeoDmsGuiQt" || true
              for _p in "${SRC}"/platforms/*.so; do [[ -e "$_p" ]] && { ldd "$_p" || true; }; done
            } 2>/dev/null | awk '/=> \// {print $3}' | { grep -E "${QT_BUNDLE_RE}" || true; } | sort -u )
for _lib in ${_qt_libs}; do
    if [[ -f "${_lib}" ]]; then
        cp -Lf "${_lib}" "${DST}/"        # -L: copy the concrete file under its soname
        echo "  + $(basename "${_lib}")"
    fi
done

# Runtime scripts. NOTE: the root-level profiler.py / regression.py that the
# WINDOWS installer ships (DmsSetupScript.nsh; the Windows test-harness entry
# points read <install>/profiler.py) are deliberately NOT packaged here: the
# linux flavor of the tst harness (full.py) always uses its own bundled copies
# of the report scripts, and nothing else reads them from the install prefix.
install -m 644 "${SRC}/RewriteExpr.lsp" "${DST}/"

# Typed standard prelude (WP4.5): functions auto-imported by every config via
# %exeDir%/prelude.dms — as REQUIRED next to the binaries as RewriteExpr.lsp.
install -m 644 "${SRC}/prelude.dms" "${DST}/"

# Linux-side performance sampler (issue #1104). When the Windows-side
# profiler.py spots a `wsl --` invocation it splices run_with_sampler.sh
# in front of the GeoDmsRun command; the wrapper then forks
# linux_sampler.py against the exact GeoDmsRun PID so the Bokeh series
# show real CPU/memory/IO from inside the WSL VM instead of zeros.
# These two files are REQUIRED by the .l regression harness — their absence
# from the installed .deb is exactly what broke every 20.7.0.l test (rc=127).
PROFILER_SRC="${REPO_ROOT}/profiler"
install -m 755 -D "${PROFILER_SRC}/run_with_sampler.sh" "${DST}/profiler/run_with_sampler.sh"
install -m 755 -D "${PROFILER_SRC}/linux_sampler.py"    "${DST}/profiler/linux_sampler.py"

# Geographic data — required at runtime by GDAL and PROJ.
cp -r "${SRC}/gdaldata"   "${DST}/"
cp -r "${SRC}/proj4data"  "${DST}/"
# The vcpkg share/ trees these are deployed from carry CMake package-config
# machinery (top-level *.cmake plus whole helper dirs like gdaldata/3.20/,
# packages/, thirdparty/); build-system metadata, never read at runtime —
# drop it by pattern and sweep the then-empty dirs.
find "${DST}/gdaldata" "${DST}/proj4data" -type f \
     \( -name '*.cmake' -o -name '*.cmake.in' -o -name '*.props.in' \) -delete
find "${DST}/gdaldata" "${DST}/proj4data" -type d -empty -delete

# DMS library scripts (referenced by user configurations at runtime) and the
# two tiny demo configurations in examples/ (end-user content, also shipped
# by the Windows installer — kept for flavor parity, ~8 KiB).
cp -r "${SRC}/library"    "${DST}/"
cp -r "${SRC}/examples"   "${DST}/"

# Fonts (misc/fonts/dms*.ttf — used by the GUI renderer)
cp -r "${SRC}/misc"       "${DST}/"

# Qt plugins
for plugin_dir in platforms imageformats xcbglintegrations iconengines tls networkinformation generic styles; do
    if [[ -d "${SRC}/${plugin_dir}" ]]; then
        cp -r "${SRC}/${plugin_dir}" "${DST}/"
    fi
done

# ---------------------------------------------------------------------------
# Self-contained loader paths: point every bundled ELF's rpath at its own
# directory ($ORIGIN) so libDm*.so and the bundled libQt6*.so are found no
# matter how the binary is launched. The GUI test harness runs GeoDmsGuiQt
# directly (not via the geodms launcher), so LD_LIBRARY_PATH is not set there.
# --force-rpath uses DT_RPATH, which (unlike DT_RUNPATH) also covers transitive
# deps (e.g. libQt6Gui -> libicuuc). This also clears the build-tree rpath leak
# (#1134): the installed binaries otherwise point back at build/.../bin.
# ---------------------------------------------------------------------------
if command -v patchelf &>/dev/null; then
    echo "Setting \$ORIGIN rpath on bundled binaries (self-contained)..."
    for _f in "${DST}/GeoDmsGuiQt" "${DST}/GeoDmsRun" "${DST}"/*.so*; do
        [[ -f "${_f}" ]] && patchelf --force-rpath --set-rpath '$ORIGIN' "${_f}" || true
    done
    # plugins live one level down; their Qt deps are in the parent dir
    for _f in "${DST}"/*/*.so; do
        [[ -f "${_f}" ]] && patchelf --force-rpath --set-rpath '$ORIGIN/..' "${_f}" || true
    done
else
    echo "  WARNING: patchelf not found -- the bundled Qt will only be found via the"
    echo "           geodms launcher's LD_LIBRARY_PATH; direct GeoDmsGuiQt invocations"
    echo "           (the test harness) may fail. Install it:  sudo apt-get install patchelf"
fi

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
cat > "${APPS_DIR}/geodms-${GeoDmsVersion}.${GeoDmsFlavor}.desktop" <<EOF
[Desktop Entry]
Name=GeoDMS ${GeoDmsVersion}.${GeoDmsFlavor}
Comment=Geographic Data & Model Software
Exec=${INSTALL_PREFIX}/geodms
Icon=${INSTALL_PREFIX}/misc/fonts/dms.ttf
Terminal=false
Type=Application
Categories=Science;Education;
EOF

# ---------------------------------------------------------------------------
# Normalize permissions. The sources live on the Windows drvfs mount, where
# every file reports mode 0777; the cp -r'd trees (gdaldata, library, plugins,
# ...) inherit that, which used to make the whole dpkg -i result world-
# writable. The stage is on ext4 here, so chmod actually sticks.
# ---------------------------------------------------------------------------
find "${STAGE}" -type d -exec chmod 755 {} +
find "${STAGE}" -type f -exec chmod 644 {} +
chmod 755 "${DST}/GeoDmsGuiQt" "${DST}/GeoDmsRun" "${DST}/geodms" \
          "${DST}/profiler/run_with_sampler.sh" "${DST}/profiler/linux_sampler.py" \
          "${DST}"/*.so* "${DST}"/*/*.so

# ---------------------------------------------------------------------------
# Create tarball (in the native tmpdir; published to distr/ after verification)
#
# VERIFY.md is a tarball-only extra: it explains how to verify the tarball's
# signature, so it belongs next to the extracted content. It is removed again
# before the .deb is built — the old .deb installed it as /VERIFY.md at the
# filesystem root.
# ---------------------------------------------------------------------------
sed "s/<ver>/${GeoDmsVersion}/g" "${SCRIPT_DIR}/VERIFY-LINUX.md" \
    > "${STAGE}/VERIFY.md"

TARBALL_TMP="${PKG_TMP}/${PKG_NAME}.tar.gz"
echo "Creating tarball ${PKG_NAME}.tar.gz..."
tar --owner=0 --group=0 --numeric-owner -czf "${TARBALL_TMP}" -C "${PKG_TMP}" "${PKG_NAME}"

rm "${STAGE}/VERIFY.md"

# ---------------------------------------------------------------------------
# Create .deb package (if dpkg-deb is available) — from the SAME stage,
# by adding DEBIAN/ control metadata in place. dpkg-deb's data archive
# excludes the DEBIAN/ dir itself, so the payload equals the tarball's.
# ---------------------------------------------------------------------------
DEB_TMP_FILE="${PKG_TMP}/${PKG_NAME}.deb"
HAVE_DEB=0
if command -v dpkg-deb &>/dev/null; then
    echo "Creating .deb package..."

    DEBIAN_DIR="${STAGE}/DEBIAN"
    mkdir -p "${DEBIAN_DIR}"
    INSTALLED_SIZE=$(du -sk "${DST}" | cut -f1)

    cat > "${DEBIAN_DIR}/control" <<EOF
Package: geodms
Version: ${GeoDmsVersion}
Architecture: amd64
Maintainer: Object Vision B.V. <info@objectvision.nl>
Installed-Size: ${INSTALLED_SIZE}
Depends: libxcb-xinerama0, libxcb-icccm4, libxcb-image0, libxcb-keysyms1, libxcb-randr0, libxcb-render-util0, libxcb-xkb1, libxkbcommon-x11-0, python3, python3-psutil
Description: GeoDMS ${GeoDmsVersion} -- Geographic Data & Model Software
 GeoDMS is a software environment for the specification and calculation
 of geographic data models. This package contains the GUI and runtime.
EOF

    # --root-owner-group: payload owned root:root instead of the build user
    dpkg-deb --build --root-owner-group "${STAGE}" "${DEB_TMP_FILE}"
    rm -rf "${DEBIAN_DIR}"
    HAVE_DEB=1
else
    echo "  dpkg-deb not found — skipping .deb creation (tarball only)"
fi

# ---------------------------------------------------------------------------
# Verify the packages BEFORE publishing/signing.
#
# 1. The .deb payload file list must equal the .tar.gz file list; the only
#    permitted difference is the tarball-only VERIFY.md. Guards against any
#    future re-divergence of the two assembly paths.
# 2. The critical runtime files must be present, and the data dirs must be
#    non-trivially populated — a truncated drvfs/9p directory read during
#    staging (cp -r exits 0 on it!) would otherwise ship silently.
# ---------------------------------------------------------------------------
echo "Verifying package contents..."
VERIFY_FAILED=0

# Normalized file list of the tarball: strip the <pkg-name>/ wrapper dir and
# trailing / on directory entries.
TAR_LIST="${PKG_TMP}/tar.lst"
tar -tzf "${TARBALL_TMP}" | sed -e "s|^${PKG_NAME}/||" -e 's|/$||' | grep -v '^$' | sort > "${TAR_LIST}"

if [[ "${HAVE_DEB}" -eq 1 ]]; then
    # Normalized file list of the .deb data archive: paths are ./-rooted.
    DEB_LIST="${PKG_TMP}/deb.lst"
    dpkg-deb -c "${DEB_TMP_FILE}" | awk '{print $6}' | sed -e 's|^\./||' -e 's|/$||' | grep -v '^$' | sort > "${DEB_LIST}"

    if ! diff -u <(grep -Fxv 'VERIFY.md' "${TAR_LIST}") "${DEB_LIST}" > "${PKG_TMP}/payload.diff"; then
        echo "ERROR: .deb payload differs from .tar.gz payload:"
        cat "${PKG_TMP}/payload.diff"
        VERIFY_FAILED=1
    fi
fi

# Critical runtime files (paths relative to the package root). Update this
# list consciously when a component is added/renamed — a mismatch fails the
# build, which is the point: better a loud packaging failure than a .deb
# that installs but cannot run or cannot be regression-tested.
PAYLOAD_PREFIX="${INSTALL_PREFIX#/}"
CRITICAL_FILES=(
    GeoDmsRun
    GeoDmsGuiQt
    geodms
    qt.conf
    RewriteExpr.lsp
    prelude.dms
    libDmRtc.so libDmStx.so libDmStg.so libDmClc.so libDmGeo.so libDmShv.so
    libQt6Core.so.6 libQt6Gui.so.6 libQt6Widgets.so.6
    platforms/libqxcb.so
    profiler/run_with_sampler.sh
    profiler/linux_sampler.py
    proj4data/proj.db
    library/Units.dms
    misc/fonts/dms.ttf
)
for _f in "${CRITICAL_FILES[@]}"; do
    if ! grep -Fxq "${PAYLOAD_PREFIX}/${_f}" "${TAR_LIST}"; then
        echo "ERROR: critical file missing from package: ${_f}"
        VERIFY_FAILED=1
    fi
done

# Data dirs that must be non-trivially populated (minimum entry counts are
# deliberately below the current values — gdaldata ~230, proj4data ~30 — to
# tolerate upstream variation while still catching a truncated copy).
for _spec in gdaldata:150 proj4data:20 library:5 imageformats:3 misc:2; do
    _dir="${_spec%%:*}"; _min="${_spec##*:}"
    _n=$(grep -c "^${PAYLOAD_PREFIX}/${_dir}/" "${TAR_LIST}" || true)
    if [[ "${_n}" -lt "${_min}" ]]; then
        echo "ERROR: ${_dir}/ holds only ${_n} entries (expected >= ${_min}) — truncated staging copy?"
        VERIFY_FAILED=1
    fi
done

if [[ "${VERIFY_FAILED}" -ne 0 ]]; then
    echo "*** Package verification FAILED — nothing published to ${DISTR}. ***"
    exit 1
fi
echo "  package contents verified: .deb == .tar.gz payload, all critical files present"

# ---------------------------------------------------------------------------
# Publish to distr/ — and prove the copies over the 9p mount are intact by
# comparing checksums against the native artifacts (a loaded /mnt/c transfer
# must not be trusted blindly; that is what broke 20.7.0).
# ---------------------------------------------------------------------------
TARBALL="${DISTR}/${PKG_NAME}.tar.gz"
DEB="${DISTR}/${PKG_NAME}.deb"

publish_checked() {  # publish_checked <native-file> <distr-file>
    cp -f "$1" "$2"
    local sha_native sha_copy
    sha_native=$(sha256sum "$1" | awk '{print $1}')
    sha_copy=$(sha256sum "$2" | awk '{print $1}')
    if [[ "${sha_native}" != "${sha_copy}" ]]; then
        echo "ERROR: copy to $2 is corrupt (sha256 mismatch after transfer to /mnt)"
        exit 1
    fi
    echo "  -> $2"
}

echo "Publishing packages to ${DISTR}..."
publish_checked "${TARBALL_TMP}" "${TARBALL}"
if [[ "${HAVE_DEB}" -eq 1 ]]; then
    publish_checked "${DEB_TMP_FILE}" "${DEB}"
fi

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

    echo ""
    echo "  Recipients verify with (fetch root CA independently from GlobalSign):"
    echo "    curl -fsSL http://secure.globalsign.com/cacert/codesigningrootr45.crt \\"
    echo "      | openssl x509 -inform DER -out GlobalSign-CodeSigning-Root-R45.pem"
    echo "    # verify fingerprint: 7B:9D:55:3E:1C:92:CB:6E:88:03:E1:37:F4:F2:87:D4:..."
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
# Summary
# ---------------------------------------------------------------------------
echo ""
echo "Done. Packages in ${DISTR}/"
ls -lh "${DISTR}/${PKG_NAME}".* 2>/dev/null
