import re, json, xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
xml_path = ROOT / "GeoDMS_npp_def.xml"
csv_path = ROOT / "data" / "operators.csv"
out_path = ROOT / "syntaxes" / "geodms.tmLanguage.json"

def split_words(s):
    return [w for w in re.split(r"\s+", s.strip()) if w]

tree = ET.parse(xml_path)
root = tree.getroot()
kws = {kw.attrib.get("name"): (kw.text or "").strip() for kw in root.iter("Keywords")}

kw1 = split_words(kws.get("Keywords1", ""))
kw2 = split_words(kws.get("Keywords2", ""))
kw3 = split_words(kws.get("Keywords3", ""))
kw4 = split_words(kws.get("Keywords4", ""))
kw6 = split_words(kws.get("Keywords6", ""))

with open(csv_path, "r", encoding="utf-8-sig") as f:
    csv_items = [ln.strip() for ln in f if ln.strip()]

properties = [w for w in kw3 if w and w != '":="']
types = kw6
keywords_primary = [w for w in kw1 if w.lower() in ["container", "template"]]
keywords_secondary = [w for w in kw2 if w.lower() in ["attribute", "parameter"]]
keywords_unit = [w for w in kw4 if w.lower() in ["unit"]]

def regex_alt(words, ci=False):
    words = sorted(set(words), key=lambda x: (-len(x), x.lower()))
    pat = "|".join(re.escape(w) for w in words)
    return f"(?i:{pat})" if ci else pat

callables = []
excluded = {x.lower() for x in properties + types + kw1 + kw2 + kw4}
for x in csv_items:
    if x in ["TRUE", "FALSE", "true", "false", "True", "False"]:
        continue
    if x.lower() in excluded:
        continue
    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", x):
        callables.append(x)

func_pat = regex_alt(callables)
prop_pat = regex_alt(properties, ci=True)
type_pat = regex_alt(types)
prim_pat = regex_alt(keywords_primary, ci=True)
sec_pat = regex_alt(keywords_secondary, ci=True)
unit_pat = regex_alt(keywords_unit, ci=True)
bool_pat = regex_alt(["TRUE", "FALSE", "true", "false", "True", "False"])

grammar = {
  "name":"GeoDMS",
  "scopeName":"source.geodms",
  "patterns":[
    {"include":"#comments"},
    {"include":"#doubleQuoted"},
    {"include":"#singleQuoted"},
    {"include":"#numbers"},
    {"include":"#declarations"},
    {"include":"#declaredNames"},
    {"include":"#properties"},
    {"include":"#types"},
    {"include":"#booleans"},
    {"include":"#functionCalls"},
    {"include":"#operators"},
    {"include":"#genericAngles"},
    {"include":"#otherBrackets"}
  ],
  "repository":{
    "comments":{"patterns":[
      {"name":"comment.line.double-slash.geodms","match":"//.*$"},
      {"name":"comment.block.geodms","begin":"/\\*","end":"\\*/"}
    ]},
    "doubleQuoted":{"patterns":[
      {"name":"string.quoted.double.geodms","begin":"\"","end":"\""}
    ]},
    "singleQuoted":{"patterns":[
      {"name":"string.quoted.single.geodms","begin":"'","end":"'"}
    ]},
    "numbers":{"patterns":[
      {"name":"constant.numeric.geodms","match":"\\b\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?\\b"}
    ]},
    "declarations":{"patterns":[
      {"name":"keyword.control.declaration.primary.geodms","match":f"\\b{prim_pat}\\b"},
      {"name":"keyword.control.declaration.secondary.geodms","match":f"\\b{sec_pat}\\b"},
      {"name":"keyword.control.declaration.unit.geodms","match":f"\\b{unit_pat}\\b"}
    ]},
    "declaredNames":{"patterns":[
      {"name":"entity.name.identifier.geodms","match":"(?<=\\b(?i:Attribute)\\s*<[^>\\n]+>\\s+)[A-Za-z_][A-Za-z0-9_]*"},
      {"name":"entity.name.identifier.geodms","match":"(?<=\\b(?i:Parameter)\\s*<[^>\\n]+>\\s+)[A-Za-z_][A-Za-z0-9_]*"},
      {"name":"entity.name.identifier.geodms","match":"(?<=\\b(?i:Container|Template|Unit)\\s+)[A-Za-z_][A-Za-z0-9_]*"}
    ]},
    "properties":{"patterns":[
      {"name":"support.type.property.geodms","match":f"\\b{prop_pat}\\b(?=\\s*[:=,])"}
    ]},
    "types":{"patterns":[
      {"name":"storage.type.geodms","match":f"\\b{type_pat}\\b"}
    ]},
    "booleans":{"patterns":[
      {"name":"constant.language.boolean.geodms","match":f"\\b{bool_pat}\\b"}
    ]},
    "functionCalls":{"patterns":[
      {"name":"entity.name.function.geodms","match":f"\\b(?:{func_pat})\\("}
    ]},
    "operators":{"patterns":[
      {"name":"keyword.operator.assignment.geodms","match":":="},
      {"name":"keyword.operator.logical.geodms","match":"\\|\\||&&|!"},
      {"name":"keyword.operator.arithmetic.geodms","match":"\\+|-|\\*|%|\\^|#"},
      {"name":"keyword.operator.division.geodms","match":"(?<=\\s)/(?!/)(?=\\s)"},
      {"name":"keyword.operator.comparison.geodms","match":"<=|>=|<>|<|>|="},
      {"name":"keyword.operator.ternary.geodms","match":"\\?|:"},
      {"name":"punctuation.separator.comma.geodms","match":","},
      {"name":"keyword.operator.path.geodms","match":"(?<=\\b(?:for_each_[A-Za-z0-9_]*|bg_[A-Za-z0-9_]*|bp_[A-Za-z0-9_]*|cgal_[A-Za-z0-9_]*|geos_[A-Za-z0-9_]*)\\()\\s/"}
    ]},
    "genericAngles":{"patterns":[
      {"name":"keyword.operator.bracket.geodms","match":"(?<=\\b(?:Attribute|Parameter|Unit)\\s)<|>"}
    ]},
    "otherBrackets":{"patterns":[
      {"name":"keyword.operator.bracket.geodms","match":"[\\[\\]{}]"}
    ]}
  }
}

out_path.write_text(json.dumps(grammar, indent=2), encoding="utf-8")
print(f"Written {out_path}")
