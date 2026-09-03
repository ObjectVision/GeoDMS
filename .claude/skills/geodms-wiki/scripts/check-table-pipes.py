"""Find [[label|Page-Name]] wiki-links whose pipe is unescaped inside a table cell.

A markdown table splits a row on every unescaped '|'. A piped wiki-link such as
[[label|Page-Name]] written inside a table cell (a line starting with '|') breaks that
row at the link's own pipe unless it is written [[label\\|Page-Name]] instead. This has
bitten Spatial-joins-and-allocation.md three times in one page; the fix is always the
same escape, so check for it before treating a page with tables as done.

Usage: python3 check-table-pipes.py <page.md> [<page.md> ...]
Exit code is nonzero (and each hit is printed as "<file>:<line>: <link>") when an
unescaped pipe is found inside a table row; silent and exit 0 otherwise.
"""

import re
import sys

LINK_RE = re.compile(r"\[\[([^\]]*)\]\]")
UNESCAPED_PIPE_RE = re.compile(r"(?<!\\)\|")


def find_hits(path):
    hits = []
    with open(path, encoding="utf-8") as f:
        for lineno, line in enumerate(f, 1):
            if not line.startswith("|"):
                continue
            for m in LINK_RE.finditer(line):
                if UNESCAPED_PIPE_RE.search(m.group(1)):
                    hits.append((lineno, m.group(0)))
    return hits


def main(argv):
    any_hits = False
    for path in argv[1:]:
        for lineno, link in find_hits(path):
            print(f"{path}:{lineno}: {link}")
            any_hits = True
    return 1 if any_hits else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
