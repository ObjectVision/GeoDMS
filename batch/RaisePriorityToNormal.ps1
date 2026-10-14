# Keeps the cmd.exe that runs the calling batch script, and everything it starts, at Normal priority.
#
# Build.bat and the batch\BuildSignAndCreateSetup*.bat scripts call this first thing. Two things
# lower the priority of a build, and on 2026-10-14, with a model run at BelowNormal on 26 of the 32
# cores of the machine, each made a setup seem to hang:
#
# - A shell opened in the Terminal of Visual Studio is started by its ServiceHub host (DevHub.exe),
#   which runs at BelowNormal, and everything started from that shell inherits it: the build
#   competed with the model on equal terms.
# - vcpkg starts its own children at Idle, whatever its own priority. vcpkg runs three times in a
#   setup: in tools\vcpkg-drift-check.ps1, from the CMake toolchain when a configure re-runs, and
#   from msbuild when ports must be (re)installed. Each time the vcvarsall.bat with which it
#   identifies the compiler waited 10 to 20 minutes for CPU, at under half a second of CPU used.
#
# So this script raises that cmd.exe to Normal when it runs lower, and starts a hidden watcher that,
# for as long as that cmd.exe lives, raises every process below it that runs at Idle or BelowNormal
# to Normal, once a second. A second call for the same cmd.exe, as when Build.bat calls the flavour
# scripts, finds the watcher running and starts no other. It never lowers a priority and never fails
# the caller.
#
# Usage, from a batch script:  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0RaisePriorityToNormal.ps1"
# The parent of that PowerShell is the cmd.exe that runs the batch script; a script reached by
# `call` runs in that same cmd.exe.
param(
    [int]  $WatchParent = 0,   # internal: run as the watcher of this cmd.exe
    [long] $WatchStart  = 0    # internal: its start time, so a reused process id is not watched
)

if ($WatchParent) {
    # The watcher. Ends when the watched cmd.exe has ended.
    while ($true) {
        $cmd = Get-Process -Id $WatchParent -ErrorAction SilentlyContinue
        if (-not $cmd -or $cmd.StartTime.ToFileTimeUtc() -ne $WatchStart) { exit 0 }
        try {
            $byParent = Get-CimInstance Win32_Process -Property ProcessId, ParentProcessId |
                Group-Object ParentProcessId -AsHashTable -AsString
            $queue = New-Object System.Collections.Generic.Queue[int]
            $queue.Enqueue($WatchParent)
            while ($queue.Count) {
                $id = $queue.Dequeue()
                if (-not $byParent.ContainsKey("$id")) { continue }
                foreach ($c in $byParent["$id"]) {
                    $queue.Enqueue([int] $c.ProcessId)
                    $p = Get-Process -Id $c.ProcessId -ErrorAction SilentlyContinue
                    if ($p -and ($p.PriorityClass -eq 'Idle' -or $p.PriorityClass -eq 'BelowNormal')) {
                        try { $p.PriorityClass = 'Normal' } catch {}
                    }
                }
            }
        } catch {}
        Start-Sleep -Seconds 1
    }
}

try {
    # This PowerShell first: the watcher it starts inherits its priority.
    $self = Get-Process -Id $PID
    if ($self.PriorityClass -eq 'Idle' -or $self.PriorityClass -eq 'BelowNormal') { $self.PriorityClass = 'Normal' }

    $parentId = (Get-CimInstance Win32_Process -Filter "ProcessId=$PID").ParentProcessId
    $parent = Get-Process -Id $parentId -ErrorAction Stop
    $before = $parent.PriorityClass
    if ($before -eq 'Idle' -or $before -eq 'BelowNormal') {
        $parent.PriorityClass = 'Normal'
        Write-Host "Priority of this build (cmd $parentId) raised from $before to Normal."
    }

    $start = $parent.StartTime.ToFileTimeUtc()
    $watching = @(Get-CimInstance Win32_Process -Filter "Name='powershell.exe'" |
        Where-Object { $_.CommandLine -match 'RaisePriorityToNormal\.ps1' -and $_.CommandLine -match "-WatchParent $parentId\b" })
    if ($watching.Count -eq 0) {
        # A window of its own, hidden: the watcher must not hold the console or a pipe of the build open.
        Start-Process powershell -WindowStyle Hidden -ArgumentList @(
            '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"",
            '-WatchParent', "$parentId", '-WatchStart', "$start") | Out-Null
        Write-Host "Processes of this build (cmd $parentId) are kept at Normal priority while it runs."
    }
} catch {
    Write-Host "Could not check the priority of this build: $($_.Exception.Message)"
}
exit 0
