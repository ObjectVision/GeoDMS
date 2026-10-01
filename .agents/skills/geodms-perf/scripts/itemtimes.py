# Compare the "} Updating::[[item]] (x secs)" lines of two GeoDMS logs and list the items that differ most.
# usage: python itemtimes.py <log A> <log B> [<n>]   (an item requested on the command line is logged; its subitems are not)
import sys, re
pat = re.compile(rb'\} Updating::\[\[([^\]]+)\]\] \(([0-9.]+) secs\)')
def load(p):
    d = {}
    for m in pat.finditer(open(p, 'rb').read()):
        k = m.group(1).decode('utf-8', 'replace'); v = float(m.group(2))
        d[k] = max(d.get(k, 0.0), v)
    return d
a, b = load(sys.argv[1]), load(sys.argv[2])
rows = [(b.get(k, 0) - a.get(k, 0), k, a.get(k), b.get(k)) for k in set(a) | set(b)]
rows.sort(reverse=True)
n = int(sys.argv[3]) if len(sys.argv) > 3 else 25
print(f"{'delta':>8} {'A':>8} {'B':>8}  item")
for d, k, x, y in rows[:n]:
    print(f"{d:8.1f} {x if x is not None else -1:8.1f} {y if y is not None else -1:8.1f}  {k}")
print('...')
for d, k, x, y in rows[-8:]:
    print(f"{d:8.1f} {x if x is not None else -1:8.1f} {y if y is not None else -1:8.1f}  {k}")
