#!/usr/bin/env bash
# Report, BEFORE a build starts, how many vcpkg ports would be rebuilt.
#
# Linux counterpart of tools/vcpkg-drift-check.ps1; see that file for the full rationale.
# In short: a port's vcpkg ABI hash covers the compiler, the triplet, the portfile/baseline
# AND vcpkg's own pinned tools (cmake, ninja, ...). Any of those moving invalidates the
# binary cache, and the first sign of it is a build that spends hours recompiling boost/gdal
# instead of minutes compiling GeoDMS. `vcpkg install --dry-run` answers that in seconds.
#
# Advisory only: a full rebuild is the CORRECT outcome of a baseline bump or a toolchain
# change. The point is to make the cost visible up front instead of a surprise.
#
# Exit 0 - at most $THRESHOLD ports pending (normal)
# Exit 1 - more than $THRESHOLD pending; the caller decides
# Exit 2 - the query could not run (vcpkg not bootstrapped yet); callers should warn only
set -uo pipefail

REPO_ROOT="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
TRIPLET="${2:-x64-linux}"
THRESHOLD="${3:-5}"
INSTALL_ROOT="${4:-$REPO_ROOT/build/linux-x64-release/vcpkg_installed}"
LIST_MAX=20

VCPKG="$REPO_ROOT/vcpkg/vcpkg"
if [ ! -x "$VCPKG" ]; then
    echo "vcpkg-drift-check: $VCPKG not found or not executable (fresh checkout?) - skipping."
    exit 2
fi

# VCPKG_BINARY_SOURCES / VCPKG_DOWNLOADS are inherited from the caller on purpose: querying
# a different cache or tool root than the build will use would answer the wrong question.
out=$("$VCPKG" install --dry-run \
        --triplet "$TRIPLET" \
        --host-triplet="$TRIPLET" \
        --vcpkg-root "$REPO_ROOT/vcpkg" \
        --x-manifest-root="$REPO_ROOT" \
        --x-install-root="$INSTALL_ROOT" 2>&1)
rc=$?
if [ $rc -ne 0 ]; then
    echo "vcpkg-drift-check: dry-run failed (exit $rc) - skipping the check."
    echo "$out" | tail -n 5 | sed 's/^/    /'
    exit 2
fi

# vcpkg prints an indented list under each header; the pending section is absent entirely
# when there is nothing to do. A leading '*' marks an INDIRECT dependency, not a pending
# build, so section membership is what classifies, never the asterisk.
pending=$(printf '%s\n' "$out" | awk '
    /will be built and installed|will be rebuilt|will be removed/ { inp=1; next }
    /are already installed/                                      { inp=0; next }
    /^[^[:space:]]/                                              { inp=0; next }
    /^[[:space:]]*$/                                             { inp=0; next }
    inp && match($0, /[^[:space:]*][^[:space:]:]*:/) { s=substr($0, RSTART, RLENGTH-1); print s }
')
n=$(printf '%s' "$pending" | grep -c . || true)

if [ "$n" -le "$THRESHOLD" ]; then
    if [ "$n" -eq 0 ]; then echo "vcpkg-drift-check: cache is current, no ports to build."
    else                    echo "vcpkg-drift-check: $n port(s) to build - normal."
    fi
    exit 0
fi

echo
echo '***********************************************************************'
echo "*** vcpkg would (re)build $n ports before this build can link."
echo '***********************************************************************'
echo 'This is what an ABI-hash change looks like. The usual causes, in order:'
echo '  - the vcpkg submodule moved (new baseline => new portfiles AND new pinned'
echo '    tool versions; cmake/ninja versions are ABI inputs for every port)'
echo "  - the gcc/toolchain in this WSL distro changed (apt upgrade)"
echo '  - this build resolves a different vcpkg / downloads root / binary cache'
echo '    than the one that populated vc_archives'
echo
echo "Ports (first $LIST_MAX):"
printf '%s\n' "$pending" | head -n "$LIST_MAX" | sed 's/^/    /'
[ "$n" -gt "$LIST_MAX" ] && echo "    ... and $((n - LIST_MAX)) more"
echo
exit 1
