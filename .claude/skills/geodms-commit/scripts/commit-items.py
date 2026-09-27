"""Commit a round of work one item per commit, each on a temporary index; the shared index and the
working copy are not touched. See the section "Many items, one commit each" of ../SKILL.md.

    python commit-items.py --repo <repo> --plan <plan.py> [--branch <name>] [--verified "<text>"]
                           [--from NN] [--dry]

The plan defines ITEMS, a list of (message_file, whole_files, partial_files) and optionally a fourth
element {path: text} for a file whose committed content is given instead of taken from the working
copy. message_file is relative to the plan's folder; paths are relative to the repository.
partial_files maps a path to key strings: the zero-context hunks of `git diff -U0 HEAD -- path` that
contain one of them are committed, renumbered so that each lands where it stands in the working copy;
the hunks a key misses are listed, so read that list in the --dry run: a change that spans two hunks
needs a key for each. --from NN starts at the item whose message file name begins with NN. The word
VERIFIED in a message is replaced by --verified.
"""
import argparse, importlib.util, os, re, subprocess, sys, tempfile


def git(repo, *args, input=None, env=None):
    r = subprocess.run(['git', '-C', repo, *args], input=input, capture_output=True, env=env)
    if r.returncode:
        raise RuntimeError('git %s failed: %s' % (' '.join(args), r.stderr.decode(errors='replace')))
    return r.stdout


HUNK_HEADER = re.compile(r'^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@')


def renumbered(hunks):
    """Give each hunk the new-side start it has when only these hunks are applied.

    `git apply --unidiff-zero` has no context to find a hunk by, so it looks from the new-side line
    number on; for a pure insertion, whose preimage is empty, that number is the position. Taken
    from the full diff it counts the lines of every hunk before it, also the ones left out, and
    the insertion lands that many lines too low (a release-note bullet in the next section)."""
    out, delta = [], 0
    for h in hunks:
        m = HUNK_HEADER.match(h)
        a, b = int(m.group(1)), int(m.group(2) or 1)
        d = int(m.group(4) or 1)
        c = a + delta + (1 if b == 0 else 0) - (1 if d == 0 else 0)  # a count of 0 names the line before
        out.append('@@ -%s +%d%s @@' % (m.group(1) + (',' + m.group(2) if m.group(2) else ''), c,
                                         ',' + m.group(4) if m.group(4) else '') + h[m.end():])
        delta += d - b
    return out


def selected_hunks(repo, path, keys):
    d = git(repo, 'diff', '-U0', 'HEAD', '--', path).decode('utf-8', 'surrogateescape')
    parts = re.split(r'(?m)^(?=@@ )', d)
    sel = [h for h in parts[1:] if any(k in h for k in keys)]
    if not sel:
        raise RuntimeError('no hunk of %s contains any of %s' % (path, keys))
    left = [h for h in parts[1:] if h not in sel]
    if left:
        # a change split over several hunks is committed half when a key misses one of them
        print('  %s: %d of %d hunks left for a later item:' % (path, len(left), len(parts) - 1))
        for h in left:
            lines = h.splitlines()
            print('    %s  %s' % (lines[0], next((l for l in lines[1:] if l.strip('+- ')), '')[:100]))
    return parts[0] + ''.join(renumbered(sel))


def commit_item(repo, branch, idx, msgfile, whole, partial, content, verified, dry):
    head = git(repo, 'rev-parse', 'HEAD').decode().strip()  # read once: parent, base tree and guard
    env = dict(os.environ, GIT_INDEX_FILE=idx)
    git(repo, 'read-tree', head, env=env)
    for p in whole:
        sha = git(repo, 'hash-object', '-w', '--', p).decode().strip()  # through the clean filter
        git(repo, 'update-index', '--add', '--cacheinfo', '100644,%s,%s' % (sha, p), env=env)
    for p, text in content.items():
        sha = git(repo, 'hash-object', '-w', '--stdin', '--path=' + p, input=text.encode('utf-8')).decode().strip()
        git(repo, 'update-index', '--add', '--cacheinfo', '100644,%s,%s' % (sha, p), env=env)
    for p, keys in partial.items():
        patch = selected_hunks(repo, p, keys)
        git(repo, 'apply', '--cached', '--unidiff-zero', '-', input=patch.encode('utf-8', 'surrogateescape'), env=env)
    tree = git(repo, 'write-tree', env=env).decode().strip()
    msg = open(msgfile, encoding='utf-8').read()
    if verified is not None:
        msg = msg.replace('VERIFIED', verified)
    print('==', os.path.basename(msgfile), '::', msg.splitlines()[0])
    print(git(repo, 'diff', '--stat', head, tree).decode())
    if dry:
        return
    new = git(repo, 'commit-tree', tree, '-p', head, '-F', '-', input=msg.encode('utf-8')).decode().strip()
    git(repo, 'update-ref', 'refs/heads/' + branch, new, head)  # refuses when HEAD moved meanwhile
    print('committed', new[:10])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo', required=True)
    ap.add_argument('--plan', required=True)
    ap.add_argument('--branch')
    ap.add_argument('--verified')
    ap.add_argument('--from', dest='start')
    ap.add_argument('--dry', action='store_true')
    a = ap.parse_args()

    spec = importlib.util.spec_from_file_location('plan', a.plan)
    plan = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(plan)
    plandir = os.path.dirname(os.path.abspath(a.plan))
    branch = a.branch or git(a.repo, 'symbolic-ref', '--short', 'HEAD').decode().strip()
    if git(a.repo, 'diff', '--cached', '--name-only').strip():
        sys.exit('the shared index holds staged changes; commit or unstage them first')

    idx = os.path.join(tempfile.mkdtemp(prefix='commit-items-'), 'index')
    started = a.start is None
    touched = []
    for item in plan.ITEMS:
        msgfile, whole, partial = item[:3]
        content = item[3] if len(item) > 3 else {}
        if not started:
            started = os.path.basename(msgfile).startswith(a.start)
            if not started:
                continue
        commit_item(a.repo, branch, idx, os.path.join(plandir, msgfile), whole, partial, content, a.verified, a.dry)
        touched += list(whole) + list(partial) + list(content)
    if not a.dry and touched:
        # the shared index back on the new HEAD for these paths only: another session may have staged others
        git(a.repo, 'reset', '-q', '--', *sorted(set(touched)))
        print('left in the working copy:')
        print(git(a.repo, 'diff', '--name-only').decode() or '(nothing)')


if __name__ == '__main__':
    main()
