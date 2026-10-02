# Per-case guard for the testcases runners (run_testcases.ps1, run_xml_roundtrip.ps1,
# run_roundtrip.ps1), which dot-source this file. Not a case and not a runner itself.
#
# Every GeoDmsRun invocation of a runner goes through Invoke-GuardedRun, which gives it two limits:
#
#   - a commit limit per process, through a Windows job object (JOB_OBJECT_LIMIT_PROCESS_MEMORY).
#     An allocation that would take the process past it fails inside the process, which then ends
#     the way it ends on any failed allocation ("Memory Error: allocation failed", exit 1);
#   - a wall-clock limit, after which the job, and so the process and anything it started, is
#     terminated.
#
# Why: a case that runs away on the exe under test used to take memory until the allocator failed,
# which on a shared machine means the whole machine's commit, with every other session and build on
# it. testcases\geo_spatial_index_unit_box.dms (#1289) takes about 3 GB per second on any build
# before the fix; the battery is run against such builds when bisecting or when testing a kept
# build. The limits are generous on purpose (8 GB and 300 s by default, the runners' -MaxCommitGB
# and -TimeoutSec; 0 switches one off): no case of this offline and cheap battery comes near them,
# so a case that reaches one is a failure, and the runners report it as LIMIT(commit) or LIMIT(time).
# The case's own output file gets a closing line that says which limit stopped it.
#
# The process is created suspended, put in the job and only then resumed, so it never runs a
# moment outside the job. stdout and stderr both go to one file, unconverted, as cmd's
# '> file 2>&1' writes them; stdin is NUL. The job is created with KILL_ON_JOB_CLOSE, so an
# interrupted runner leaves no GeoDmsRun behind.
#
# Windows PowerShell 5.1 compiles the C# below with the .NET Framework compiler, so it stays
# within C# 5 (no string interpolation, no 'out var').

if (-not ('GeoDmsTestcases.CaseGuard' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace GeoDmsTestcases
{
    public class GuardResult
    {
        public int    ExitCode;
        public string Limit = "";  // "", "commit", "time" or "commit,time"
        public string Note  = "";  // what the limit did, in words; also appended to the output file
        public ulong  PeakCommit;  // bytes, the highest commit charge of the process
        public double Seconds;
    }

    public static class CaseGuard
    {
        [StructLayout(LayoutKind.Sequential)]
        struct BasicLimitInformation
        {
            public long PerProcessUserTimeLimit, PerJobUserTimeLimit;
            public uint LimitFlags;
            public UIntPtr MinimumWorkingSetSize, MaximumWorkingSetSize;
            public uint ActiveProcessLimit;
            public UIntPtr Affinity;
            public uint PriorityClass, SchedulingClass;
        }
        [StructLayout(LayoutKind.Sequential)]
        struct IoCounters { public ulong ReadOps, WriteOps, OtherOps, ReadBytes, WriteBytes, OtherBytes; }
        [StructLayout(LayoutKind.Sequential)]
        struct ExtendedLimitInformation
        {
            public BasicLimitInformation Basic;
            public IoCounters Io;
            public UIntPtr ProcessMemoryLimit, JobMemoryLimit, PeakProcessMemoryUsed, PeakJobMemoryUsed;
        }
        [StructLayout(LayoutKind.Sequential)]
        struct AssociateCompletionPort { public IntPtr CompletionKey, CompletionPort; }
        [StructLayout(LayoutKind.Sequential)]
        struct SecurityAttributes { public int nLength; public IntPtr lpSecurityDescriptor; public bool bInheritHandle; }
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        struct StartupInfo
        {
            public int cb;
            public string lpReserved, lpDesktop, lpTitle;
            public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
            public short wShowWindow, cbReserved2;
            public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError;
        }
        [StructLayout(LayoutKind.Sequential)]
        struct ProcessInformation { public IntPtr hProcess, hThread; public int dwProcessId, dwThreadId; }

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        static extern IntPtr CreateJobObjectW(IntPtr attributes, string name);
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool SetInformationJobObject(IntPtr job, int infoClass, ref ExtendedLimitInformation info, int size);
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool SetInformationJobObject(IntPtr job, int infoClass, ref AssociateCompletionPort info, int size);
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool QueryInformationJobObject(IntPtr job, int infoClass, out ExtendedLimitInformation info, int size, IntPtr returned);
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool TerminateJobObject(IntPtr job, uint exitCode);
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern IntPtr CreateIoCompletionPort(IntPtr file, IntPtr existingPort, UIntPtr key, uint threads);
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool GetQueuedCompletionStatus(IntPtr port, out uint bytes, out UIntPtr key, out IntPtr overlapped, uint milliseconds);
        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        static extern IntPtr CreateFileW(string name, uint access, uint share, ref SecurityAttributes sa, uint disposition, uint flags, IntPtr template);
        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        static extern bool CreateProcessW(string application, StringBuilder commandLine, IntPtr processAttributes, IntPtr threadAttributes,
            bool inheritHandles, uint flags, IntPtr environment, string currentDirectory, ref StartupInfo si, out ProcessInformation pi);
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern uint ResumeThread(IntPtr thread);
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool TerminateProcess(IntPtr process, uint exitCode);
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool GetExitCodeProcess(IntPtr process, out uint exitCode);
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool CloseHandle(IntPtr handle);

        const int  JobObjectAssociateCompletionPortInformation = 7;
        const int  JobObjectExtendedLimitInformation           = 9;
        const uint JOB_OBJECT_LIMIT_PROCESS_MEMORY     = 0x100;
        const uint JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE  = 0x2000;
        const uint JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO  = 4;
        const uint JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT = 9;
        const uint CREATE_SUSPENDED      = 0x4;
        const int  STARTF_USESTDHANDLES  = 0x100;
        const uint GENERIC_READ  = 0x80000000, GENERIC_WRITE = 0x40000000;
        const uint FILE_SHARE_READ = 1, FILE_SHARE_WRITE = 2;
        const uint CREATE_ALWAYS = 2, OPEN_EXISTING = 3;
        const uint WAIT_TIMEOUT = 0x102, INFINITE = 0xFFFFFFFF;
        public const uint TimeoutExitCode = 1460; // ERROR_TIMEOUT, the exit code of a process killed at the wall-clock limit
        static readonly IntPtr INVALID_HANDLE_VALUE = new IntPtr(-1);

        // One argument as the C runtime's argv parsing reads it back.
        static string Quote(string arg)
        {
            if (arg.Length > 0 && arg.IndexOfAny(new char[] { ' ', '\t', '\n', '\v', '"' }) < 0)
                return arg;
            StringBuilder sb = new StringBuilder("\"");
            int backslashes = 0;
            foreach (char c in arg)
            {
                if (c == '\\') { ++backslashes; continue; }
                sb.Append('\\', c == '"' ? 2 * backslashes + 1 : backslashes);
                backslashes = 0;
                sb.Append(c);
            }
            sb.Append('\\', 2 * backslashes);
            sb.Append('"');
            return sb.ToString();
        }

        static string Gb(double bytes) { return (bytes / (1024.0 * 1024 * 1024)).ToString("0.##", CultureInfo.InvariantCulture); }

        public static GuardResult Run(string exe, string[] args, string outFile, string workDir, ulong maxCommitBytes, uint timeoutMs)
        {
            StringBuilder commandLine = new StringBuilder(Quote(exe));
            foreach (string a in args) { commandLine.Append(' ').Append(Quote(a)); }

            GuardResult result = new GuardResult();
            IntPtr job = IntPtr.Zero, port = IntPtr.Zero, output = INVALID_HANDLE_VALUE, input = INVALID_HANDLE_VALUE;
            ProcessInformation pi = new ProcessInformation();
            try
            {
                job = CreateJobObjectW(IntPtr.Zero, null);
                if (job == IntPtr.Zero) throw new Win32Exception();
                ExtendedLimitInformation limits = new ExtendedLimitInformation();
                limits.Basic.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                if (maxCommitBytes > 0)
                {
                    limits.Basic.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
                    limits.ProcessMemoryLimit = new UIntPtr(maxCommitBytes);
                }
                if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, ref limits, Marshal.SizeOf(typeof(ExtendedLimitInformation))))
                    throw new Win32Exception();

                // the job reports a process that runs into its commit limit on this port
                port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, IntPtr.Zero, UIntPtr.Zero, 1);
                if (port == IntPtr.Zero) throw new Win32Exception();
                AssociateCompletionPort association = new AssociateCompletionPort();
                association.CompletionKey  = job;
                association.CompletionPort = port;
                if (!SetInformationJobObject(job, JobObjectAssociateCompletionPortInformation, ref association, Marshal.SizeOf(typeof(AssociateCompletionPort))))
                    throw new Win32Exception();

                SecurityAttributes inheritable = new SecurityAttributes();
                inheritable.nLength = Marshal.SizeOf(typeof(SecurityAttributes));
                inheritable.bInheritHandle = true;
                output = CreateFileW(outFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, ref inheritable, CREATE_ALWAYS, 0, IntPtr.Zero);
                if (output == INVALID_HANDLE_VALUE) throw new Win32Exception(Marshal.GetLastWin32Error(), "cannot write " + outFile);
                input = CreateFileW("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, ref inheritable, OPEN_EXISTING, 0, IntPtr.Zero);
                if (input == INVALID_HANDLE_VALUE) throw new Win32Exception();

                StartupInfo si = new StartupInfo();
                si.cb = Marshal.SizeOf(typeof(StartupInfo));
                si.dwFlags = STARTF_USESTDHANDLES;
                si.hStdInput = input;
                si.hStdOutput = output;
                si.hStdError = output;
                Stopwatch clock = Stopwatch.StartNew();
                if (!CreateProcessW(exe, commandLine, IntPtr.Zero, IntPtr.Zero, true, CREATE_SUSPENDED, IntPtr.Zero, workDir, ref si, out pi))
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "cannot start " + exe);
                if (!AssignProcessToJobObject(job, pi.hProcess))
                {
                    int error = Marshal.GetLastWin32Error();
                    TerminateProcess(pi.hProcess, 1);
                    throw new Win32Exception(error, "cannot put GeoDmsRun in a job object");
                }
                ResumeThread(pi.hThread);

                bool timedOut = WaitForSingleObject(pi.hProcess, timeoutMs > 0 ? timeoutMs : INFINITE) == WAIT_TIMEOUT;
                if (timedOut)
                {
                    TerminateJobObject(job, TimeoutExitCode);
                    WaitForSingleObject(pi.hProcess, INFINITE);
                }
                result.Seconds = clock.Elapsed.TotalSeconds;
                uint exitCode;
                GetExitCodeProcess(pi.hProcess, out exitCode);
                result.ExitCode = unchecked((int)exitCode); // as $LASTEXITCODE shows an NTSTATUS: 0xC0000005 is -1073741819

                // The memory limit message was queued before the exit; ACTIVE_PROCESS_ZERO comes last.
                bool hitCommit = false;
                uint message; UIntPtr key; IntPtr overlapped;
                while (GetQueuedCompletionStatus(port, out message, out key, out overlapped, 1000))
                {
                    if (message == JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT) hitCommit = true;
                    if (message == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO) break;
                }
                ExtendedLimitInformation used;
                if (QueryInformationJobObject(job, JobObjectExtendedLimitInformation, out used, Marshal.SizeOf(typeof(ExtendedLimitInformation)), IntPtr.Zero))
                    result.PeakCommit = used.PeakProcessMemoryUsed.ToUInt64();
                // delivery of job messages is not guaranteed; a peak at the limit says the same
                if (maxCommitBytes > 0 && result.PeakCommit >= maxCommitBytes) hitCommit = true;

                if (hitCommit)
                {
                    result.Limit = "commit";
                    result.Note = "commit limit of " + Gb(maxCommitBytes) + " GB per process reached (peak commit " + Gb(result.PeakCommit) + " GB)";
                }
                if (timedOut)
                {
                    result.Limit += (hitCommit ? "," : "") + "time";
                    result.Note += (hitCommit ? "; " : "") + "wall-clock limit of " + (timeoutMs / 1000.0).ToString("0.###", CultureInfo.InvariantCulture) + " s reached, process killed";
                }
                if (result.Limit.Length > 0)
                {
                    result.Note += "; exit " + result.ExitCode + " after " + result.Seconds.ToString("0.0", CultureInfo.InvariantCulture) + " s";
                    CloseHandle(output);
                    output = INVALID_HANDLE_VALUE;
                    File.AppendAllText(outFile, Environment.NewLine + "*** testcases guard: " + result.Note + Environment.NewLine);
                }
            }
            finally
            {
                if (pi.hThread  != IntPtr.Zero) CloseHandle(pi.hThread);
                if (pi.hProcess != IntPtr.Zero) CloseHandle(pi.hProcess);
                if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
                if (input  != INVALID_HANDLE_VALUE) CloseHandle(input);
                if (port   != IntPtr.Zero) CloseHandle(port);
                if (job    != IntPtr.Zero) CloseHandle(job); // KILL_ON_JOB_CLOSE: ends whatever the case left running
            }
            return result;
        }
    }
}
'@
}

# The highest peak commit and the longest run over all invocations of this runner, for the
# summary line, so that whoever tunes the limits sees how far the battery stays below them.
$script:GuardPeak = [pscustomobject]@{ Bytes = [uint64]0; Case = '' }
$script:GuardLongest = [pscustomobject]@{ Seconds = 0.0; Case = '' }

# Runs $Exe with $Arguments under the limits, all of its output to $OutFile, in the current
# directory as '& $Exe' would. Returns the GeoDmsTestcases.GuardResult; .Limit is empty unless a
# limit stopped the run.
function Invoke-GuardedRun([string]$Exe, [string[]]$Arguments, [string]$OutFile, [double]$MaxCommitGB, [double]$TimeoutSec, [string]$Case) {
    $workDir = (Get-Location -PSProvider FileSystem).ProviderPath
    $r = [GeoDmsTestcases.CaseGuard]::Run($Exe, $Arguments, $OutFile, $workDir, [uint64]($MaxCommitGB * 1GB), [uint32]($TimeoutSec * 1000))
    if ($r.PeakCommit -gt $script:GuardPeak.Bytes) { $script:GuardPeak.Bytes = $r.PeakCommit; $script:GuardPeak.Case = $Case }
    if ($r.Seconds -gt $script:GuardLongest.Seconds) { $script:GuardLongest.Seconds = $r.Seconds; $script:GuardLongest.Case = $Case }
    $r
}

function Get-GuardSummary([double]$MaxCommitGB, [double]$TimeoutSec) {
    $commit = if ($MaxCommitGB -gt 0) { "$MaxCommitGB GB" } else { 'none' }
    $time   = if ($TimeoutSec -gt 0) { "$TimeoutSec s" } else { 'none' }
    "GUARD per GeoDmsRun: commit limit $commit, wall-clock limit $time;" +
    " highest peak commit $([math]::Round($script:GuardPeak.Bytes / 1GB, 2)) GB ($($script:GuardPeak.Case))," +
    " longest run $([math]::Round($script:GuardLongest.Seconds, 1)) s ($($script:GuardLongest.Case))"
}
