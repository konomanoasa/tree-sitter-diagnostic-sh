import tokens from "./src/tokens.json" with { type: "json" };

const tokenNames = new Set();
const scannerSymbols = new Set();
for (const { name, scanner, kind } of tokens) {
  if (
    typeof name !== "string" ||
    !/^_?[a-z][a-z0-9_]*$/.test(name) ||
    typeof scanner !== "string" ||
    !/^[A-Z][A-Z0-9_]*$/.test(scanner) ||
    scanner === "TOKEN_TYPE_COUNT" ||
    tokenNames.has(name) ||
    scannerSymbols.has(scanner) ||
    (kind !== undefined && !["owner", "fault"].includes(kind)) ||
    (kind === "owner" && !/^_begin_[a-z][a-z0-9_]*$/.test(name)) ||
    (kind === "fault" && name.startsWith("_"))
  ) {
    throw new Error(`Invalid or duplicate external token ${name}`);
  }
  tokenNames.add(name);
  scannerSymbols.add(scanner);
}

const owners = tokens
  .filter(({ kind }) => kind === "owner")
  .map(({ name }) => name.slice("_begin_".length));
const faults = tokens
  .filter(({ kind }) => kind === "fault")
  .map(({ name }) => name);

function contents($) {
  return repeat(
    choice(
      $.source,
      field("issue", $.syntax_issue),
      field("unowned_issue", alias($._unowned_issue, $.syntax_issue)),
      $._owner,
    ),
  );
}

function issue($) {
  return seq(
    field("fault", choice(...faults.map((kind) => $[kind]))),
    optional(field("fragment", $.recovery_fragment)),
    field("boundary", choice($.recovery_resume, $.recovery_eof)),
  );
}

export default grammar({
  name: "diagnostic_sh",
  externals: ($) => tokens.map(({ name }) => $[name]),
  // Prevents lexer fallback from accepting EOF mid-input.
  extras: ($) => [$._unmatchable],
  rules: {
    program: ($) =>
      seq(
        $._start,
        repeat(
          choice(
            $.source,
            field("unowned_issue", alias($._unowned_issue, $.syntax_issue)),
            $._owner,
          ),
        ),
        $._finish,
      ),
    _owner: ($) => choice(...owners.map((kind) => $[kind])),
    ...Object.fromEntries(
      owners.map((kind) => [
        kind,
        ($) => seq($[`_begin_${kind}`], contents($), $._end_owner),
      ]),
    ),
    syntax_issue: ($) => seq($._begin_issue, issue($)),
    _unowned_issue: ($) => seq($._begin_unowned_issue, issue($)),
    _unmatchable: () => token(seq(/[\s\S]/, /[^\s\S]/)),
  },
});
