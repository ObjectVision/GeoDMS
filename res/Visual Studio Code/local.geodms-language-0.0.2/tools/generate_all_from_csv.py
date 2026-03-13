#!/usr/bin/env python3
import json
import re
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
XML_PATH = ROOT / "GeoDMS_npp_def.xml"
CSV_PATH = ROOT / "data" / "operators.csv"
GRAMMAR_PATH = ROOT / "syntaxes" / "geodms.tmLanguage.json"
EXTENSION_PATH = ROOT / "extension.js"

def split_words(s: str):
    return [w for w in re.split(r"\s+", s.strip()) if w]

def load_udl(xml_path: Path):
    tree = ET.parse(xml_path)
    root = tree.getroot()
    kws = {}
    for kw in root.iter("Keywords"):
        kws[kw.attrib.get("name")] = (kw.text or "").strip()

    properties = [w for w in split_words(kws.get("Keywords3", "")) if w not in ('":="', ":=")]
    types = split_words(kws.get("Keywords6", ""))
    primary = split_words(kws.get("Keywords1", ""))
    secondary = split_words(kws.get("Keywords2", ""))
    unit_words = split_words(kws.get("Keywords4", ""))
    return {
        "properties": properties,
        "types": types,
        "primary": primary,
        "secondary": secondary,
        "unit_words": unit_words,
    }

def load_csv_items(csv_path: Path):
    txt = csv_path.read_text(encoding="utf-8-sig")
    items = [line.strip() for line in txt.splitlines() if line.strip()]
    return list(dict.fromkeys(items))

def classify_functions(items, types, properties, keywords):
    type_set = {x.lower() for x in types}
    property_set = {x.lower() for x in properties}
    keyword_set = {x.lower() for x in keywords}
    boolean_set = {"true", "false"}

    out = []
    for item in items:
        low = item.lower()
        if item == "?":
            out.append(item)
            continue
        if low in type_set or low in property_set or low in keyword_set or low in boolean_set:
            continue
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", item):
            out.append(item)
    return list(dict.fromkeys(out))

def alt(words, case_insensitive=False):
    words = [w for w in words if w]
    words = sorted(set(words), key=lambda s: (-len(s), s.lower()))
    body = "|".join(re.escape(w) for w in words)
    if case_insensitive:
        return f"(?i:{body})"
    return body

def write_grammar(path: Path, data):
    properties = data["properties"]
    types = data["types"]
    primary = data["primary"]
    secondary = data["secondary"]
    unit_words = data["unit_words"]
    functions = data["functions"]

    keyword_primary_pat = alt(primary, case_insensitive=True)
    keyword_secondary_pat = alt(secondary, case_insensitive=True)
    unit_pat = alt(unit_words, case_insensitive=True)
    prop_pat = alt(properties, case_insensitive=True)
    type_pat = alt(types, case_insensitive=True)
    fn_pat = alt(functions)

    grammar = {
      "name": "GeoDMS",
      "scopeName": "source.geodms",
      "patterns": [
        {"include": "#comments"},
        {"include": "#doubleQuoted"},
        {"include": "#singleQuoted"},
        {"include": "#numbers"},
        {"include": "#declarations"},
        {"include": "#declaredNames"},
        {"include": "#properties"},
        {"include": "#types"},
        {"include": "#booleans"},
        {"include": "#functionCalls"},
        {"include": "#operators"},
        {"include": "#genericAngles"},
        {"include": "#otherBrackets"}
      ],
      "repository": {
        "comments": {
          "patterns": [
            {"name": "comment.line.double-slash.geodms", "match": "//.*$"},
            {"name": "comment.block.geodms", "begin": "/\\*", "end": "\\*/"}
          ]
        },
        "doubleQuoted": {
          "patterns": [{"name": "string.quoted.double.geodms", "begin": "\"", "end": "\""}]
        },
        "singleQuoted": {
          "patterns": [{"name": "string.quoted.single.geodms", "begin": "'", "end": "'"}]
        },
        "numbers": {
          "patterns": [{"name": "constant.numeric.geodms", "match": "\\b\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?\\b"}]
        },
        "declarations": {
          "patterns": [
            {"name": "keyword.control.declaration.primary.geodms", "match": f"\\b{keyword_primary_pat}\\b"},
            {"name": "keyword.control.declaration.secondary.geodms", "match": f"\\b{keyword_secondary_pat}\\b"},
            {"name": "keyword.control.declaration.unit.geodms", "match": f"\\b{unit_pat}\\b"}
          ]
        },
        "declaredNames": {
          "patterns": [
            {"name": "entity.name.identifier.geodms", "match": "(?<=\\b(?i:Attribute)\\s*<[^>\\n]+>\\s+)[A-Za-z_][A-Za-z0-9_]*"},
            {"name": "entity.name.identifier.geodms", "match": "(?<=\\b(?i:Parameter)\\s*<[^>\\n]+>\\s+)[A-Za-z_][A-Za-z0-9_]*"},
            {"name": "entity.name.identifier.geodms", "match": "(?<=\\b(?i:Container|Template|Unit)\\s+)[A-Za-z_][A-Za-z0-9_]*"}
          ]
        },
        "properties": {
          "patterns": [
            {"name": "support.type.property.geodms", "match": f"\\b{prop_pat}\\b(?=\\s*[:=,])"}
          ]
        },
        "types": {
          "patterns": [
            {"name": "storage.type.geodms", "match": f"\\b{type_pat}\\b"}
          ]
        },
        "booleans": {
          "patterns": [{"name": "constant.language.boolean.geodms", "match": "\\b(?:TRUE|FALSE|true|false|True|False)\\b"}]
        },
        "functionCalls": {
          "patterns": [
            {"name": "entity.name.function.geodms", "match": f"\\b(?:{fn_pat})\\("}
          ]
        },
        "operators": {
          "patterns": [
            {"name": "keyword.operator.assignment.geodms", "match": ":="},
            {"name": "keyword.operator.logical.geodms", "match": "\\|\\||&&|!"},
            {"name": "keyword.operator.arithmetic.geodms", "match": "\\+|-|\\*|%|\\^|#"},
            {"name": "keyword.operator.division.geodms", "match": "(?<=\\s)/(?!/)(?=\\s)"},
            {"name": "keyword.operator.comparison.geodms", "match": "<=|>=|<>|<|>|="},
            {"name": "keyword.operator.ternary.geodms", "match": "\\?|:"},
            {"name": "punctuation.separator.comma.geodms", "match": ","}
          ]
        },
        "genericAngles": {
          "patterns": [
            {"name": "keyword.operator.bracket.geodms", "match": "(?<=\\b(?:Attribute|Parameter|Unit)\\s)<|>"}
          ]
        },
        "otherBrackets": {
          "patterns": [
            {"name": "keyword.operator.bracket.geodms", "match": "[\\[\\]{}]"}
          ]
        }
      }
    }

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(grammar, ensure_ascii=False, indent=2), encoding="utf-8")

def write_extension(path: Path, data):
    properties_js = json.dumps(data["properties"], ensure_ascii=False, indent=2)
    types_js = json.dumps(data["types"], ensure_ascii=False, indent=2)
    functions_js = json.dumps(data["functions"], ensure_ascii=False, indent=2)
    primary = data["primary"]
    secondary = data["secondary"]
    unit_words = data["unit_words"]
    keywords = list(dict.fromkeys([w.lower() for w in primary + secondary + unit_words]))
    keywords_js = json.dumps(keywords, ensure_ascii=False, indent=2)

    content = """const vscode = require(\"vscode\");

const PROPERTIES = __PROPERTIES__;

const TYPES = __TYPES__;

const KEYWORDS = __KEYWORDS__;

const FUNCTIONS = __FUNCTIONS__;

function item(label, kind, insertText, detail) {
  const x = new vscode.CompletionItem(label, kind);
  x.insertText = insertText;
  x.detail = detail;
  return x;
}

function fnItem(name) {
  if (name === \"?\") {
    const x = item(\"?\", vscode.CompletionItemKind.Operator, \"?\", \"GeoDMS operator\");
    x.sortText = \"2000_?\";
    return x;
  }
  const x = item(
    name,
    vscode.CompletionItemKind.Function,
    new vscode.SnippetString(`${name}($1)`),
    \"GeoDMS function/operator\"
  );
  x.sortText = `2000_${name}`;
  return x;
}

function propItem(name) {
  const x = item(
    name,
    vscode.CompletionItemKind.Property,
    new vscode.SnippetString(`${name} = $1`),
    \"GeoDMS property\"
  );
  x.sortText = `3000_${name}`;
  return x;
}

function typeItem(name) {
  const x = item(name, vscode.CompletionItemKind.TypeParameter, name, \"GeoDMS type\");
  x.sortText = `1000_${name}`;
  return x;
}

function keywordItem(name) {
  const x = item(name, vscode.CompletionItemKind.Keyword, name, \"GeoDMS keyword\");
  x.sortText = `4000_${name}`;
  return x;
}

function snippetItem(label, snippet, detail) {
  const x = item(label, vscode.CompletionItemKind.Snippet, new vscode.SnippetString(snippet), detail);
  x.sortText = \"0000\";
  return x;
}

function activate(context) {
  const provider = vscode.languages.registerCompletionItemProvider(
    \"geodms\",
    {
      provideCompletionItems(document, position) {
        const line = document.lineAt(position.line).text;
        const left = line.slice(0, position.character);

        if (/^\\s*unit\\s*$/i.test(left)) {
          return [
            snippetItem(
              \"unit<type> name\",
              \"unit<${1:UInt32}> ${2:name}\",
              \"GeoDMS unit snippet\"
            )
          ];
        }

        if (/^\\s*attribute\\s*$/i.test(left)) {
          return [
            snippetItem(
              \"attribute<Domain> Name (ValuesUnit) :=\",
              \"attribute<${1:Domain}> ${2:Name} (${3:ValuesUnit}) :=\\n\\t${0:expr};\",
              \"GeoDMS attribute snippet\"
            )
          ];
        }

        if (/^\\s*parameter\\s*$/i.test(left)) {
          return [
            snippetItem(
              \"parameter<Domain> Name :=\",
              \"parameter<${1:Domain}> ${2:Name} :=\\n\\t${0:value};\",
              \"GeoDMS parameter snippet\"
            )
          ];
        }

        if (/^\\s*(container|template)\\s*$/i.test(left)) {
          return [
            snippetItem(
              \"container Name :=\",
              \"container ${1:Name} :=\\n{\\n\\t$0\\n}\",
              \"GeoDMS container snippet\"
            )
          ];
        }

        if (/\\b(?:unit|attribute|parameter)\\s*<[^>]*$/i.test(left)) {
          return TYPES.map(typeItem);
        }

        if (/:=\\s*$/.test(left)) {
          return [
            ...FUNCTIONS.map(fnItem),
            ...PROPERTIES.map(propItem)
          ];
        }

        return [
          ...KEYWORDS.map(keywordItem),
          ...TYPES.map(typeItem),
          ...FUNCTIONS.map(fnItem),
          ...PROPERTIES.map(propItem)
        ];
      }
    },
    "<",
    ":",
    "=",
    " ",
    "_"
  );

  context.subscriptions.push(provider);
}

function deactivate() {}

module.exports = { activate, deactivate };
"""
    content = content.replace("__PROPERTIES__", properties_js)
    content = content.replace("__TYPES__", types_js)
    content = content.replace("__KEYWORDS__", keywords_js)
    content = content.replace("__FUNCTIONS__", functions_js)
    path.write_text(content, encoding="utf-8")

def main():
    udl = load_udl(XML_PATH)
    csv_items = load_csv_items(CSV_PATH)
    keywords = udl["primary"] + udl["secondary"] + udl["unit_words"]
    functions = classify_functions(csv_items, udl["types"], udl["properties"], keywords)

    data = {
        **udl,
        "functions": functions
    }

    write_grammar(GRAMMAR_PATH, data)
    write_extension(EXTENSION_PATH, data)
    print(f"Generated {GRAMMAR_PATH}")
    print(f"Generated {EXTENSION_PATH}")
    print(f"CSV items: {len(csv_items)}")
    print(f"Callable functions/operators: {len(functions)}")

if __name__ == "__main__":
    main()
