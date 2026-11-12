# What the toetser also checks in GeoDMS

The toetser (a reviewing subagent, see the personal skill `rapport-voorleggen`) reads this file when it reviews a report about work in this repository. Each point is a claim a report tends to make without the evidence for it. AGENTS.md holds the policy and `.claude/skills/` the recipes; this file only says what to verify.

- A build that the report relies on really ran. The mtime of `bin\Release\x64\GeoDmsRun.exe` (and of the DLL the edit lives in) is later than the edit, because msbuild exits 0 on an up-to-date no-op and a link that found its DLLs locked can be skipped without an error (`geodms-build`).
- A test result was read, not inferred from an exit code: the `TOTAL=` line and the failing cases of `testcases/run_testcases.bat` and `run_xml_roundtrip.bat`, the `.out` of a case that the report discusses, and for a GeoDmsRun log the `[E]` lines. A run that exits 3 is a failed assertion: the battery counts it as `ASSERT` and fails it, also for a `_neg` case that expects an error.
- A new regression case was shown to fail on the code before the fix, and the report says why it failed there (`geodms-debug`).
- The binaries that were tested contain the change: the run used `bin\<Config>\x64` from this tree after the build, not an installed engine or a kept copy in `C:\LocalData\GeoDMS_engine`, unless the report says which one and why.
- A timing names the machine and the build, and a slowdown or speedup rests on an interleaved A/B against a kept build, not on two rounds at different times (`geodms-perf`).
- A change a modeller can notice has its wiki page updated in `C:\dev\GeoDMS.wiki` in the same session, committed there and not pushed (`geodms-wiki`). A report that calls such work done while the wiki is untouched is not done.
- A change that makes a configuration fail that used to run, or that adds a notation, is named in the release notes of the coming version (`doc/release-notes/release-notes-<ver>.md`) when that draft exists; otherwise the report says it still has to go there (`geodms-wiki`, `geodms-release`).
- A commit that lands, finishes, drops or reverts work planned in a document under the repo root, `doc/` or `doc/development/` also moves that document's status line (AGENTS.md, plan documents).
- A commit that touches `.claude/skills/` stages `.agents/skills/` with it, and `batch\SyncSkills.bat /check` exits 0.
- A commit contains only the session's own files and hunks, its subject starts with the issue number, and `git diff --stat` shows no doubled line counts from line endings (`geodms-commit`). Nothing is pushed.
- Text meant for an issue was shown to the user as a draft and not posted, unless the user asked for posting in the session (`geodms-issues`).
- No work was moved to a git worktree (AGENTS.md). A new session is only the handover that the actielijst hook proposes after the second compaction, in this same checkout.
