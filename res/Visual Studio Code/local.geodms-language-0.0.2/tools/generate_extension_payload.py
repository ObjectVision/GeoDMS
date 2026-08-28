from pathlib import Path
import json
import re

PROPERTIES = [
  "cdf","descr","dialogdata","dialogtype","disablestorage","explicitsuppliers","expr","freedata",
  "integritycheck","ishidden","keepdata","label","source","sqlstring","storagename","storagereadonly",
  "storagetype","storedata","syncmode","url","using","nrofrows"
]
TYPES = [
  "uint8","uint16","uint32","uint64","int8","int16","int32","int64","bool","uint2","uint4","float32",
  "float64","string","spoint","ipoint","wpoint","upoint","dpoint","fpoint","uint64seq","uint32seq",
  "uint16seq","uint8seq","int64seq","int32seq","int16seq","int8seq","float32seq","float64seq",
  "spolygon","ipolygon","wpolygon","upolygon","fpolygon","dpolygon","void"
]
KEYWORDS = ["container","attribute","parameter","unit","template"]

EXTENSION_JS_TEMPLATE = Path("extension.js.template").read_text(encoding="utf-8")

def unique(arr):
    return list(dict.fromkeys(arr))

def read_csv_items(csv_path: Path):
    return unique([line.strip() for line in csv_path.read_text(encoding="utf-8-sig").splitlines() if line.strip()])

def classify_functions(csv_items):
    type_set = {x.lower() for x in TYPES}
    keyword_set = {x.lower() for x in KEYWORDS}
    property_set = {x.lower() for x in PROPERTIES}
    boolean_set = {"true", "false"}
    out = []
    for item in csv_items:
        low = item.lower()
        if item == "?":
            out.append(item)
            continue
        if low in type_set or low in keyword_set or low in property_set or low in boolean_set:
            continue
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", item):
            out.append(item)
    return unique(out)

def main():
    csv_path = Path("data/operators.csv")
    csv_items = read_csv_items(csv_path)
    functions = classify_functions(csv_items)
    payload = {
        "PROPERTIES": PROPERTIES,
        "TYPES": TYPES,
        "KEYWORDS": KEYWORDS,
        "FUNCTIONS": functions
    }
    Path("data/functions.generated.json").write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"Generated function payload with {len(functions)} callable names.")

if __name__ == "__main__":
    main()
