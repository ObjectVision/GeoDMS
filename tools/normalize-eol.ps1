#Requires -Version 5.1
<#
.SYNOPSIS
    One-time working-tree line-ending repair for a Windows clone of GeoDMS.

.DESCRIPTION
    Since GeoDMS 20.18.0 every text blob in this repository is stored with LF, and
    .gitattributes carries `* text=auto`, so a Windows checkout is handed CRLF and
    git normalises again on commit. A clone made before that commit keeps whatever
    its files happened to contain -- including the 45 files that had drifted into a
    mix of CRLF and LF inside one file, which git could not repair on its own.

    This repair is cosmetic. git cleans the working copy on `git add`, so a mixed
    file already commits as clean LF and produces no diff noise. What the script
    fixes is the working tree itself, so that what you edit and what a fresh clone
    hands out are the same bytes.

    Only files whose bytes differ from what git would check out are rewritten, which
    keeps the rebuild that follows as small as possible. Be aware that one hot header
    among them -- rtc\dll\src\act\Actor.h or rtc\dll\src\cpc\Types.h -- still pulls a
    large part of the tree into that rebuild.

.PARAMETER Scan
    Report what would change and exit. Nothing is written.

.PARAMETER Force
    Skip the confirmation prompt.

.EXAMPLE
    batch\NormalizeCRLF.bat /scan
#>
[CmdletBinding()]
param(
    [switch] $Scan,
    [switch] $Force
)

$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')

function Fail([string] $Message) {
    Write-Host ''
    Write-Host "*** $Message" -ForegroundColor Red
    exit 1
}

# Classify one `git ls-files --eol` row: what the working tree has against what a
# checkout would produce. Returns $null for rows that are never converted.
function Get-EolMismatch([string] $Row) {
    $tab = $Row.IndexOf([char]9)
    if ($tab -lt 0) { return }
    $flags = $Row.Substring(0, $tab)
    $path = $Row.Substring($tab + 1)

    # Only rows that report an actual working-tree line ending are candidates. This
    # drops binaries (w/-text), files without a single line ending (w/none) and the
    # vcpkg submodule, whose gitlink row carries an empty w/ field.
    if ($flags -notmatch 'w/(lf|crlf|mixed)') { return }

    $want = if ($flags -match 'eol=lf') { 'lf' } else { 'crlf' }
    if ($flags -match 'w/mixed') { $have = 'mixed' }
    elseif ($flags -match 'w/crlf') { $have = 'crlf' }
    else { $have = 'lf' }

    if ($have -eq $want) { return }
    [pscustomobject]@{ Path = $path; Have = $have; Want = $want }
}

# --- preconditions ---------------------------------------------------------

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Fail 'git is not on PATH.'
}

$null = git rev-parse --is-inside-work-tree 2>$null
if ($LASTEXITCODE -ne 0) { Fail "$(Get-Location) is not a git working tree." }

if (-not (Test-Path .gitattributes) -or
    -not (Select-String -Path .gitattributes -Pattern '^\*\s+text=auto' -Quiet)) {
    Fail ('.gitattributes has no "* text=auto" rule. Pull the normalisation commit ' +
          'first -- without it this script has nothing to normalise towards.')
}

# The script assumes a checkout hands out CRLF, which is what `* text=auto` does on
# Windows unless this clone deliberately asks for something else.
$autocrlf = git config --get core.autocrlf
$coreEol = git config --get core.eol
if ($autocrlf -eq 'input' -or $coreEol -eq 'lf') {
    Write-Host ''
    Write-Host ("This clone is configured for LF working trees " +
                "(core.autocrlf=$autocrlf, core.eol=$coreEol).") -ForegroundColor Yellow
    Write-Host 'That is a deliberate choice and this script would fight it. Nothing done.'
    exit 0
}

# The repair overwrites files, so it must not run over uncommitted work.
#
# `git diff`, not `git status`. status decides by comparing stat data, and for
# exactly the files this script exists to repair -- the ones whose bytes on disk are
# not the checkout form -- the size never matches the blob, so status calls every
# single one of them modified while `git diff` is empty and the hashes are identical.
# git diff runs the content through the same clean filter a commit would use, which
# is the question actually being asked here: is there work to lose?
#
# Submodules are excluded. A vcpkg checkout sitting on a different commit than the one
# recorded here shows up as a modification, and it blocked a run on a build server for
# no reason: this script never touches a submodule, because the gitlink row from
# `git ls-files --eol` carries no working-tree line ending and is skipped.
$dirty = @(@(git diff --name-only --ignore-submodules=all) +
           @(git diff --cached --name-only --ignore-submodules=all) | Select-Object -Unique)
if ($dirty.Count -gt 0 -and -not $Scan) {
    Write-Host ''
    Write-Host 'The working tree has uncommitted changes to tracked files:' -ForegroundColor Yellow
    $dirty | ForEach-Object { Write-Host "    $_" }
    Fail 'Commit or stash these first -- the repair rewrites files in place.'
}

# --- detect ----------------------------------------------------------------

# `git ls-files --eol` reports, per file, the line endings in the index (i/), in the
# working tree (w/) and the attributes that apply, followed by a TAB and the path.
$mismatched = @(git -c core.quotePath=false ls-files --eol | ForEach-Object { Get-EolMismatch $_ })

if ($mismatched.Count -eq 0) {
    Write-Host ''
    Write-Host 'Working tree already matches what git would check out. Nothing to do.' -ForegroundColor Green
    exit 0
}

Write-Host ''
Write-Host "$($mismatched.Count) file(s) differ from what a checkout would produce:"
Write-Host ''
foreach ($group in ($mismatched | Group-Object Have | Sort-Object Name)) {
    Write-Host ("  {0,-5} on disk : {1}" -f $group.Name, $group.Count)
}
Write-Host ''
$mismatched | Sort-Object Path | ForEach-Object {
    Write-Host ("    {0,-5} -> {1,-4}  {2}" -f $_.Have, $_.Want, $_.Path)
}

if ($Scan) {
    Write-Host ''
    Write-Host 'Scan only -- nothing written. Run without /scan to repair.'
    exit 0
}

# --- repair ----------------------------------------------------------------

Write-Host ''
Write-Host 'These files will be rewritten from the index. Their timestamps change, so' -ForegroundColor Yellow
Write-Host 'the next build recompiles whatever depends on them.' -ForegroundColor Yellow

if (-not $Force) {
    Write-Host ''
    $answer = Read-Host 'Proceed? [y/N]'
    if ($answer -notmatch '^(y|yes)$') {
        Write-Host 'Cancelled. Nothing written.'
        exit 1
    }
}

# The rewrite happens here rather than through `git checkout-index -f`, which looks
# like the obvious tool but silently does nothing: checkout_entry compares the index
# stat cache against the file first and returns early when it matches, and after a
# renormalisation it always matches -- the clean filter makes these files count as
# unmodified. Forcing git's hand would mean deleting each file before checking it
# out, which leaves a window where the file simply is not there.
#
# Rewriting the bytes is equivalent for the rules this repository uses (text plus an
# eol of crlf or lf, nothing else) and is idempotent. Bytes are round-tripped through
# ISO-8859-1 so that a file which is not valid UTF-8 -- library\Operator.dms holds
# such bytes on purpose -- survives untouched apart from its line endings.
# One file must not abort the run. Whatever fails is collected and reported at the end,
# because stopping halfway leaves the files already rewritten with a stale index stat,
# and that is the state that makes git status -- and Visual Studio reading it -- claim
# hundreds of modified files. Reaching the settle step below matters more than failing
# fast.
$latin1 = [System.Text.Encoding]::GetEncoding(28591)
$failures = @()
foreach ($item in $mismatched) {
    try {
        $full = [System.IO.Path]::GetFullPath((Join-Path $PWD.Path $item.Path))
        $info = New-Object System.IO.FileInfo $full
        # A read-only bit would otherwise throw on the write. Put it back afterwards.
        $wasReadOnly = $info.IsReadOnly
        if ($wasReadOnly) { $info.IsReadOnly = $false }
        $body = $latin1.GetString([System.IO.File]::ReadAllBytes($full))
        $body = $body.Replace("`r`n", "`n")
        if ($item.Want -eq 'crlf') { $body = $body.Replace("`n", "`r`n") }
        [System.IO.File]::WriteAllBytes($full, $latin1.GetBytes($body))
        if ($wasReadOnly) { $info.IsReadOnly = $true }
    } catch {
        $failures += "$($item.Path) -- $($_.Exception.Message)"
    }
}
$paths = @($mismatched | ForEach-Object { $_.Path })

# Settle the index stat cache, or every repaired file shows up as modified in
# `git status` from here on -- and in everything that reads it, which is how a Visual
# Studio session came to report 522 changed files after a run of this script. The
# rewrite changed each file's size and timestamp while its content still cleans to the
# same blob, and status decides on stat data alone. Neither `git update-index --refresh`
# nor `--really-refresh` clears that; the latter reports every one as "needs update" and
# leaves it that way. `git add` does, staging nothing because the blobs are identical.
#
# The repaired paths by name, with -f, in chunks. Not `git add -u` across the tree:
# that would also stage a submodule whose checked-out commit differs from the recorded
# one, which is none of this script's business. And -f because git refuses an explicitly
# named path that matches an ignore rule even when the path is tracked -- .gitignore
# carries /.claude while .claude/settings.json is tracked regardless, and that one
# rejection failed a whole batch. Every path here came from `git ls-files`, so it is
# tracked already and -f cannot pull anything new in. Chunked for the command line limit.
for ($i = 0; $i -lt $paths.Count; $i += 100) {
    $chunk = $paths[$i .. [Math]::Min($i + 99, $paths.Count - 1)]
    & git add -f -- @chunk
    if ($LASTEXITCODE -ne 0) { Fail "git add failed on the chunk starting at $i." }
}

$staged = @(git diff --cached --name-only --ignore-submodules=all)
if ($staged.Count -gt 0) {
    Write-Host ''
    Write-Host 'The rewrite left staged changes, which it must not:' -ForegroundColor Red
    $staged | ForEach-Object { Write-Host "    $_" }
    Fail 'Inspect with `git diff --cached` before doing anything else.'
}

if ($failures.Count -gt 0) {
    Write-Host ''
    Write-Host "$($failures.Count) of $($paths.Count) file(s) could not be rewritten:" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host "    $_" }
    Write-Host ''
    Write-Host 'Everything that did succeed is rewritten and the index is settled, so the'
    Write-Host 'tree is consistent as far as it got. Release whatever holds these files --'
    Write-Host 'an open editor, a running build, a virus scanner -- and run the script again;'
    Write-Host 'it only touches what still differs.'
    exit 1
}

# --- verify ----------------------------------------------------------------

$left = @(git -c core.quotePath=false ls-files --eol | ForEach-Object { Get-EolMismatch $_ })

# The tree was clean going in, so it must still be clean: git cleans the rewritten
# files back to LF and they have to match their blobs again. Anything reported here
# would mean the rewrite changed more than line endings. Content comparison again, and
# submodules excluded again, for the same reasons as the check above.
$stillDirty = @(git diff --name-only --ignore-submodules=all)
if ($stillDirty.Count -gt 0) {
    Write-Host ''
    Write-Host 'The rewrite changed more than line endings:' -ForegroundColor Red
    $stillDirty | ForEach-Object { Write-Host "    $_" }
    Fail 'Recover with `git checkout -- .` and report this.'
}

Write-Host ''
if ($left.Count -eq 0) {
    Write-Host "Rewrote $($paths.Count) file(s). Working tree now matches the index." -ForegroundColor Green
} else {
    Write-Host "Rewrote $($paths.Count) file(s), but $($left.Count) still mismatch:" -ForegroundColor Red
    $left | ForEach-Object { Write-Host "    $($_.Have) $($_.Path)" }
    exit 1
}

Write-Host ''
Write-Host 'One thing this cannot reach: commits made before the normalisation, on a'
Write-Host 'local branch, still carry CRLF blobs and would drag them back in on a merge.'
Write-Host 'On such a branch, run `git add --renormalize .` once and commit that.'
exit 0
