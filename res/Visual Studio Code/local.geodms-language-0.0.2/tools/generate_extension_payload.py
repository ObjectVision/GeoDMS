from pathlib import Path
import json
import re

PROPERTIES = [
  "cdf","descr","dialogdata","dialogtype","disablestorage","explicitsuppliers","expr","freedata",
  "integritycheck","ishidden","keepdata","label","source","sqlstring","storagename","storagereadonly",
  "storagetype","storedata","syncmode","url","using","nrofrows"
]
TYPES = [
  "UInt8","UInt16","UInt32","UInt64","Int8","Int16","Int32","Int64","Bool","UInt2","UInt4","Float32",
  "Float64","String","SPoint","IPoint","WPoint","UPoint","DPoint","FPoint","UInt64Seq","UInt32Seq",
  "UInt16Seq","UInt8Seq","Int64Seq","Int32Seq","Int16Seq","Int8Seq","Float32Seq","Float64Seq",
  "SPolygon","IPolygon","WPolygon","UPolygon","FPolygon","DPolygon","Void"
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
