const vscode = require("vscode");
const fs = require("fs");
const path = require("path");

const PROPERTIES = [
  "cdf",
  "descr",
  "dialogdata",
  "dialogtype",
  "disablestorage",
  "explicitsuppliers",
  "expr",
  "freedata",
  "integritycheck",
  "ishidden",
  "keepdata",
  "label",
  "source",
  "sqlstring",
  "storagename",
  "storagereadonly",
  "storagetype",
  "storedata",
  "syncmode",
  "url",
  "using",
  "nrofrows"
];
const TYPES = [
  "UInt8",
  "UInt16",
  "UInt32",
  "UInt64",
  "Int8",
  "Int16",
  "Int32",
  "Int64",
  "Bool",
  "UInt2",
  "UInt4",
  "Float32",
  "Float64",
  "String",
  "SPoint",
  "IPoint",
  "WPoint",
  "UPoint",
  "DPoint",
  "FPoint",
  "UInt64Seq",
  "UInt32Seq",
  "UInt16Seq",
  "UInt8Seq",
  "Int64Seq",
  "Int32Seq",
  "Int16Seq",
  "Int8Seq",
  "Float32Seq",
  "Float64Seq",
  "SPolygon",
  "IPolygon",
  "WPolygon",
  "UPolygon",
  "FPolygon",
  "DPolygon",
  "Void"
];
const KEYWORDS = [
  "container",
  "attribute",
  "parameter",
  "unit",
  "template"
];

function unique(arr) {
  return [...new Set(arr)];
}

function readOperatorsFromCsv(context) {
  try {
    const csvPath = path.join(context.extensionPath, "data", "operators.csv");
    if (!fs.existsSync(csvPath)) {
      return [];
    }
    const txt = fs.readFileSync(csvPath, "utf8");
    return unique(
      txt
        .split(/\r?\n/)
        .map(s => s.trim())
        .filter(Boolean)
    );
  } catch (err) {
    console.error("GeoDMS: failed to read operators.csv", err);
    return [];
  }
}

function classifyFunctions(csvItems) {
  const typeSet = new Set(TYPES.map(x => x.toLowerCase()));
  const keywordSet = new Set(KEYWORDS.map(x => x.toLowerCase()));
  const propertySet = new Set(PROPERTIES.map(x => x.toLowerCase()));
  const booleanSet = new Set(["true", "false"]);

  return csvItems.filter(item => {
    const low = item.toLowerCase();
    if (item === "?") return true;
    if (typeSet.has(low)) return false;
    if (keywordSet.has(low)) return false;
    if (propertySet.has(low)) return false;
    if (booleanSet.has(low)) return false;
    return /^[A-Za-z_][A-Za-z0-9_]*$/.test(item);
  });
}

function item(label, kind, insertText, detail) {
  const x = new vscode.CompletionItem(label, kind);
  x.insertText = insertText;
  x.detail = detail;
  return x;
}

function fnItem(name) {
  if (name === "?") {
    const x = item("?", vscode.CompletionItemKind.Operator, "?", "GeoDMS operator");
    x.sortText = "2000_?";
    return x;
  }
  const x = item(
    name,
    vscode.CompletionItemKind.Function,
    new vscode.SnippetString(`${name}($1)`),
    "GeoDMS function/operator"
  );
  x.sortText = `2000_${name}`;
  return x;
}

function propItem(name) {
  const x = item(
    name,
    vscode.CompletionItemKind.Property,
    new vscode.SnippetString(`${name} = $1`),
    "GeoDMS property"
  );
  x.sortText = `3000_${name}`;
  return x;
}

function typeItem(name) {
  const x = item(name, vscode.CompletionItemKind.TypeParameter, name, "GeoDMS type");
  x.sortText = `1000_${name}`;
  return x;
}

function keywordItem(name) {
  const x = item(name, vscode.CompletionItemKind.Keyword, name, "GeoDMS keyword");
  x.sortText = `4000_${name}`;
  return x;
}

function snippetItem(label, snippet, detail) {
  const x = item(label, vscode.CompletionItemKind.Snippet, new vscode.SnippetString(snippet), detail);
  x.sortText = "0000";
  return x;
}

function activate(context) {
  const csvItems = readOperatorsFromCsv(context);
  const FUNCTIONS = classifyFunctions(csvItems);

  console.log(`GeoDMS: loaded ${csvItems.length} CSV items, ${FUNCTIONS.length} callable functions/operators`);

  const provider = vscode.languages.registerCompletionItemProvider(
    "geodms",
    {
      provideCompletionItems(document, position) {
        const line = document.lineAt(position.line).text;
        const left = line.slice(0, position.character);

        if (/^\s*unit\s*$/i.test(left)) {
          return [
            snippetItem(
              "unit<type> name",
              "unit<${1:UInt32}> ${2:name}",
              "GeoDMS unit snippet"
            )
          ];
        }

        if (/^\s*attribute\s*$/i.test(left)) {
          return [
            snippetItem(
              "attribute<Domain> Name (ValuesUnit) :=",
              "attribute<${1:Domain}> ${2:Name} (${3:ValuesUnit}) :=\n\t${0:expr};",
              "GeoDMS attribute snippet"
            )
          ];
        }

        if (/^\s*parameter\s*$/i.test(left)) {
          return [
            snippetItem(
              "parameter<Domain> Name :=",
              "parameter<${1:Domain}> ${2:Name} :=\n\t${0:value};",
              "GeoDMS parameter snippet"
            )
          ];
        }

        if (/^\s*(container|template)\s*$/i.test(left)) {
          return [
            snippetItem(
              "container Name :=",
              "container ${1:Name} :=\n{\n\t$0\n}",
              "GeoDMS container snippet"
            )
          ];
        }

        if (/\b(?:unit|attribute|parameter)\s*<[^>]*$/i.test(left)) {
          return TYPES.map(typeItem);
        }

        if (/:=\s*$/.test(left)) {
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
