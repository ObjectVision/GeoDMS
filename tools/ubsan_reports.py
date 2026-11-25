#!/usr/bin/env python3
"""Lists the reports of gcc's undefined-behaviour checks that a test round wrote (#1385).

    python3 ubsan_reports.py <report folder> [--root <source folder>] [--frames <n>]

batch/TestLinuxUbsan.sh runs the tests on a build of the preset linux-x64-ubsan with UBSAN_OPTIONS
log_path=<report folder>/<suite>/ubsan, so every process that makes a report writes it to a file
<suite>/ubsan.<pid> of its own, or <suite>/ubsan.<case>.<pid> for a case of the testcases battery.
A process reports each place in the code once; this script lists each place once, with the number
of processes that reported it, the suites and cases they ran, the message of the first report and
its stack. The paths in it are relative to the source folder (--root, the repository by default).

A report is a line '<file>:<line>:<column>: runtime error: <message>', or a line
'==<pid>==ERROR: UndefinedBehaviorSanitizer: <what>' for a signal that ended the process, followed by
the frames of its stack. The libubsan of gcc 13 writes no summary line after a runtime error, so the
message is what names the check; a line 'SUMMARY: UndefinedBehaviorSanitizer: <check> ...', which
other versions write, ends the report and names the check.

Writes the list to stdout. Exit code 0 when the folder has no report, 2 when it has one or more.
"""

import argparse
import collections
import pathlib
import re
import sys

RUNTIME_ERROR = re.compile(r"^(?P<loc>\S+?:\d+(?::\d+)?): runtime error: (?P<msg>.*)$")
DEADLY = re.compile(r"^==\d+==ERROR: UndefinedBehaviorSanitizer: (?P<msg>.*)$")
FRAME = re.compile(r"^\s+#(?P<n>\d+) (?:0x[0-9a-f]+ in )?(?P<rest>.*)$")
SUMMARY = re.compile(r"^SUMMARY: UndefinedBehaviorSanitizer: (?P<check>\S+)(?: (?P<loc>\S+))?")
REPORT_FILE = re.compile(r"^ubsan(?:\.(?P<case>.+))?\.(?P<pid>\d+)$")


def parse_reports(text):
    """Yields (location, message, check, frames) for each report in the text of one report file."""
    current = None
    for line in text.splitlines():
        m = RUNTIME_ERROR.match(line)
        d = None if m else DEADLY.match(line)
        if m or d:
            if current:
                yield current
            current = {"loc": m.group("loc") if m else None, "msg": (m or d).group("msg"),
                       "check": None, "frames": []}
            continue
        if current is None:
            continue
        f = FRAME.match(line)
        if f:
            current["frames"].append(f.group("rest"))
            continue
        s = SUMMARY.match(line)
        if s:
            current["check"] = s.group("check")
            if current["loc"] is None and s.group("loc"):
                current["loc"] = s.group("loc")
            yield current
            current = None
    if current:
        yield current


def main():
    parser = argparse.ArgumentParser(description="Lists the reports of gcc's undefined-behaviour checks.")
    parser.add_argument("folder", type=pathlib.Path)
    parser.add_argument("--root", default=str(pathlib.Path(__file__).resolve().parent.parent),
                        help="the source folder, removed from the start of every path")
    parser.add_argument("--frames", type=int, default=12, help="the frames of the stack that are listed")
    parser.add_argument("--origins", type=int, default=10,
                        help="the tests and cases that are named for each place; the others are counted")
    args = parser.parse_args()

    root = args.root.rstrip("/") + "/"

    def short(text):
        return text.replace(root, "") if text else text

    places = collections.OrderedDict()
    files = sorted(p for p in args.folder.rglob("ubsan*") if p.is_file())
    for path in files:
        name = REPORT_FILE.match(path.name)
        suite = path.parent.relative_to(args.folder).as_posix()
        origin = suite + (" " + name.group("case") if name and name.group("case") else "")
        pid = name.group("pid") if name else path.name
        for r in parse_reports(path.read_text(encoding="utf-8", errors="replace")):
            key = (short(r["loc"]) or "(no location)", r["check"] or "")
            place = places.setdefault(key, {"first": r, "file": path, "pids": set(), "origins": collections.Counter()})
            place["pids"].add((suite, pid))
            place["origins"][origin] += 1

    if not places:
        print(f"No report of gcc's undefined-behaviour checks in {args.folder} ({len(files)} report files).")
        return 0

    def processes(n):
        return f"{n} process" + ("" if n == 1 else "es")

    n_proc = len({p for place in places.values() for p in place["pids"]})
    print(f"{len(places)} place{'' if len(places) == 1 else 's'} in the code reported, by {processes(n_proc)}, in {args.folder}")
    for (loc, check), place in sorted(places.items()):
        r = place["first"]
        named = sorted(place["origins"].items())
        origins = ", ".join(f"{o} ({n})" if n > 1 else o for o, n in named[:args.origins])
        if len(named) > args.origins:
            origins += f" and {len(named) - args.origins} more"
        print()
        print(f"{loc}  {check or 'runtime error'}  ({processes(len(place['pids']))})")
        print(f"  {short(r['msg'])}")
        print(f"  reported by: {origins}")
        print(f"  first report: {place['file'].relative_to(args.folder).as_posix()}")
        for i, frame in enumerate(r["frames"][:args.frames]):
            print(f"    #{i} {short(frame)}")
        if len(r["frames"]) > args.frames:
            print(f"    ... {len(r['frames']) - args.frames} more frames")
    return 2


if __name__ == "__main__":
    sys.exit(main())
