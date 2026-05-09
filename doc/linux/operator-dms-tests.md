# Operator.dms unit tests on Linux

The DMS `Operator` test suite (`tst/Operator/cfg/Operator.dms`) is the
backbone unit-test set, exercised by `t010_operator_test` in the regression
runner and by the `Test*Unit` shell scripts. Several fixes were needed to
make `unit_linux.sh` produce the same green result as the Windows
`TestReleaseUnit.bat`.

## Fixes applied

### `min/test_string` (raw `\xFF` bytes in source)

**Problem:** Source contained literal `\xFF\xFF\xFF\xFF` bytes for the
"`MaxValue`" string. UTF-8-aware editors (and many SCMs) corrupt those
bytes on save.

**Fix:** Replaced the literals with the DMS hex-escape syntax
`'\xFF\xFF\xFF\xFF'`. Two locations in `tst/Operator/cfg/Operator.dms`:
the `minCityName` array (Region tiled variant) and the `minCityName`
parameter (empty domain variant). The cfg file is now fully
UTF-8-editor-safe — no raw non-UTF-8 bytes for `MaxValue`.

### `from_utf` (glibc `iconv` ASCII translit)

**Problem:** `rtc/dll/src/utl/encodes.cpp` used
`iconv_open("ASCII//TRANSLIT", "UTF-8")` to drop diacritics. glibc's
TRANSLIT mappings differ from libiconv's — produced different output on
Linux than on Windows.

**Fix:** Replaced the `iconv` call with a hard-coded `strip_to_ascii()`
table covering Latin-1 Supplement (U+00C0–U+00FF) and `Œ`/`œ` (U+0152–
U+0153). Removed `#include <iconv.h>` and `#include <cerrno>` — no
longer needed.

### DMS `\xHH` hex-escape parsing

**Problem:** The `\xHH` escape was supported only in some quote contexts.

**Fix:** Added uniform `\xHH` parsing to both single- and double-quoted
string literals. Touched `rtc/dll/src/utl/quotes.cpp` — 8 functions plus
a local `hexDigitVal` helper. After this change `'\xFF\xFF\xFF\xFF'` and
`"\xFF\xFF\xFF\xFF"` both parse to four bytes of `0xFF`.

The output side (`_SingleQuoteMiddle` / `_DoubleQuoteMiddle`) still emits
raw bytes for non-ASCII — the `\xHH` parsing is unidirectional (read-only)
for now. Symmetric quoting on output is a future cleanup.

## Intentional non-UTF-8 bytes still present

`tst/Operator/cfg/Operator.dms` still contains a literal `\xa9` (`©` —
U+00A9) byte. **Do not auto-convert** this on save: it is used as test
input by the `URL_encode` and `HTML_encode` unit tests, where the
expected output specifically encodes the raw 0xa9 byte. Editors that
re-save the file as UTF-8 will turn the single byte into `0xc2 0xa9`
and break those tests.

## Tests still excluded on Linux (display-only)

| Test | Reason |
|---|---|
| `CloseGUIIssue1.dms` | `ShapeType` not yet supported in the SHP driver on Linux |
| `DPGeneral_*` | GUI tests — gated on `$DISPLAY` being set |

## Quick-test invocation

```sh
cd /mnt/c/dev/GeoDMS_2026
./TestLinuxReleaseUnit.sh
# or for the debug build:
./TestLinuxDebugUnit.sh
```

Both scripts run under WSL2 and produce a results file at
`$ResultDir/unit/operator/operator.txt`.
