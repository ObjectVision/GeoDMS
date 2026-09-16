# vcpkg overlay ports

Ports of the pinned baseline (`vcpkg-configuration.json`: vcpkg `d015e31e`, 2026.05.25) whose
**source download no longer exists**, so a tree without a warm `vc_downloads` cannot provision
the `x64-windows-v145` triplet at all. Each overlay is the smallest change that restores the
download, with a SHA512 that upstream vcpkg pins, so nothing rests on a hash computed here.

Both were found on OVSRV05 on 2026-09-14/16, on a clone whose `vc_downloads` was empty.

| port | baseline | overlay | why |
|---|---|---|---|
| `gmp` | 6.3.0#3 | 6.3.0#4 | the portfile downloads the msys2 package `autoconf2.71-2.71-3-any.pkg.tar.zst` directly; msys2 replaced that build by `-4` and 26 mirrors answer 404. Only the URL and its SHA512 change, both taken from upstream's own gmp port-version 4. The gmp version itself is unchanged. |
| `libaec` | 1.1.6 | 1.1.7#1 | the portfile downloads from `gitlab.dkrz.de`, whose archive endpoint has answered 429 to this machine for many hours on end, on the old and the new project path alike, while the site itself serves fine. Upstream moved the port to the GitHub mirror in commit `9942fb5f` ("[libaec] Switch to GitHub repo"), and that move was made at 1.1.7, so there is no upstream-pinned hash for a GitHub tarball of 1.1.6. This overlay is upstream's port verbatim; the tarball was checked to hash to the SHA512 that commit pins. |

`libaec` is therefore one patch release ahead of the baseline. It is a small entropy-coding
codec that reaches GeoDMS only through hdf5 and netcdf-c, and no GeoDMS code calls it.

Remove a port from this folder, and the `overlay-ports` entry in `vcpkg-configuration.json`
when the folder empties, as soon as the baseline is bumped past the fix it carries.
