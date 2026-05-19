## Summary

When a configuration writes attributes whose identifier contains a non-ASCII character (e.g. Greek lower-case beta `Î²`), the GeoDMS engine creates the corresponding `.dmsdata` file fine on the **first** run, but on **subsequent** runs fails with `FileSystem Error: Write permission denied` â€” *only for the non-ASCII-named files*. ASCII-named sibling attributes in the same `.fss` folder overwrite without issue.

This reproduces against both the **cmake-Release** and **msbuild-Release** 20.0.0 builds on Windows 11 (NTFS), so it is not build-system specific. It looks like a Unicode-path handling regression on the open-for-truncate / overwrite code path of the FSS writer.

## Repro

Use any cfg whose write target contains a Greek-letter attribute. The currently in-tree case is `RSopen_RegressieTest_v2025H2_wLB`:

```
prj_snapshots/RSopen_RegressieTest_v2025H2_wLB/cfg/main/WritePrivData.dms
```

defines (around line 62/67):

```dms
attribute<UInt32> banen : IntegrityCheck = "LiggenErPuntenBinnenStudyArea";
```

and the calculation chain via `Templates/.../Trends/Write_Betas_Objecten` writes attributes called `Objecten`, `Objecten_Î²`, `BrutoOpp`, `BrutoOpp_Î²` into:

```
%localDataProjDir%/BaseData/Vastgoed/Verblijfsrecreatie/Provincie/Betas_Objecten_Nederland.fss
```

### Steps

1. **Empty the target `.fss` folder**:
   ```
   del /s /q "C:\LocalData\regression\RSopen_RegressieTest_v2025\BaseData\Vastgoed\Verblijfsrecreatie\Provincie\Betas_Objecten_Nederland.fss\*"
   ```

2. **Run** `python full.py -version local` (or `local-msbuild-release`). The test runs to completion, all four `.dmsdata` files are created, including `Objecten_Î².dmsdata` and `BrutoOpp_Î².dmsdata`. âœ…

3. **Run again** â€” without clearing the folder. The test now fails with:
   ```
   ErrorLevel up to 1 due to failure: FileSystem Error: Write permission for
   'C:/LocalData/regression/RSopen_RegressieTest_v2025/BaseData/Vastgoed/
    Verblijfsrecreatie/Provincie/Betas_Objecten_Nederland.fss/Objecten_Î².dmsdata' denied
   ```
   The ASCII-named siblings (`Objecten.dmsdata`, `BrutoOpp.dmsdata`) overwrite cleanly. Only the `_Î²` files fail.

## Verification that the OS allows the write

The same files are writable from outside the GeoDMS engine:

```
$ cd "C:/.../Betas_Objecten_Nederland.fss"
$ mv Objecten_Î².dmsdata x.tmp && mv x.tmp Objecten_Î².dmsdata    # rename round-trip OK
$ dd if=/dev/zero of=Objecten_Î².dmsdata bs=52 count=1            # write OK
```

NTFS attributes are plain Archive (no read-only, no DENY ACE):

```
Mode: -a----   Attributes: Archive
```

No process holds an open handle (verified with `Get-Process` filtered for `GeoDms*`).

## Hypothesis

The FSS writer's overwrite path likely uses an ANSI / system-codepage path API (`CreateFileA` / `fopen`) somewhere, while the create-fresh path uses the wide-char API (`CreateFileW` / `_wfopen`). Greek `Î²` (U+03B2) cannot round-trip through the system codepage (CP1252 on a Western Windows install), so the open-existing call resolves to a different (or invalid) path and Windows returns ERROR_ACCESS_DENIED.

Worth grepping in `stg/dll/src/` (FSS storage manager) and the related `Storage*` files for any ANSI path API or implicit `char*`-from-`std::filesystem::path` conversion that could narrow a wide path.

## Environment

- GeoDMS branch: `refactor_linux_gui` @ `3f13e1e5` (rtc: portable address-mode detection + vcpkg-style DMS_PLATFORM)
- Windows 11 Pro 26200, NTFS
- System codepage: CP1252 (Western European)
- Both binaries reproduce:
  - cmake Release: `build/windows-x64-release/bin/GeoDmsRun.exe`
  - msbuild Release: `bin/Release/x64/GeoDmsRun.exe`

## Workaround

Delete the offending `_Î².dmsdata` files before re-running. Acceptable for CI but not for interactive use.
