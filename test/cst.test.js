import assert from "node:assert/strict";
import { test } from "node:test";
import nodeTypes from "../src/node-types.json" with { type: "json" };
import {
  assertCstSourceContract,
  diagnosticStructure,
  parseCst,
} from "./support/cst.js";
import cases from "./support/parser-cases.json" with { type: "json" };

test("CST decoding retains UTF-8 ranges, direct fields and issue fragments", () => {
  const source = "é)\n";
  const output = [
    "0:0 - 1:0   program",
    "0:0 - 0:2     source `é`",
    "0:2 - 1:0     unowned_issue: syntax_issue",
    "0:2 - 0:3       fault: unmatched_parenthesis `)`",
    "0:3 - 1:0       fragment: recovery_fragment",
    "1:0 - 1:0       boundary: recovery_resume",
  ].join("\n");
  const actual = parseCst(output, source);
  assert.deepEqual(actual, {
    hasError: false,
    issues: [
      {
        owner: null,
        range: [2, 4],
        fault: { kind: "unmatched_parenthesis", range: [2, 3] },
        boundary: { kind: "recovery_resume", range: [4, 4] },
        fragments: [[3, 4]],
      },
    ],
    native: [],
    tree: {
      kind: "program",
      field: null,
      range: [0, 4],
      children: [
        { kind: "source", field: null, range: [0, 2], children: [] },
        {
          kind: "syntax_issue",
          field: "unowned_issue",
          range: [2, 4],
          children: [
            {
              kind: "unmatched_parenthesis",
              field: "fault",
              range: [2, 3],
              children: [],
            },
            {
              kind: "recovery_fragment",
              field: "fragment",
              range: [3, 4],
              children: [],
            },
            {
              kind: "recovery_resume",
              field: "boundary",
              range: [4, 4],
              children: [],
            },
          ],
        },
      ],
    },
  });
  assertCstSourceContract(actual.tree, source);
});

test("CST decoding distinguishes recovery markers from displayed source", () => {
  const source = "•\nx";
  const output = [
    "0:0 - 1:1   program",
    "0:0 - 1:1     source `•",
    "1:0 - 1:1       `x`",
  ].join("\n");
  const normal = parseCst(output, source);
  assert.equal(normal.hasError, false);
  assert.deepEqual(normal.native, []);
  assert.deepEqual(normal.tree.children, [
    { kind: "source", field: null, range: [0, 5], children: [] },
  ]);
  const recovery = parseCst(
    output
      .replace("   program", "  •program")
      .replace("     source", "    •ERROR"),
    source,
  );
  assert.equal(recovery.hasError, true);
  assert.deepEqual(recovery.native, [{ kind: "ERROR", range: [0, 5] }]);
  assert.equal(recovery.tree.children[0].kind, "ERROR");
});

test("CST decoding rejects missing roots and out-of-source byte positions", () => {
  for (const output of ["", "0:0 - 1:0   program", "0:0 - 0:3   program"])
    assert.throws(() => parseCst(output, "é"), assert.AssertionError);
});

test("source contracts reject incomplete coverage, invented owners and malformed issues", () => {
  const source = "cat >";
  const output = [
    "0:0 - 0:5   program",
    "0:0 - 0:4     source `cat `",
    "0:4 - 0:5     io_redirect",
    "0:4 - 0:5       source `>`",
    "0:5 - 0:5       issue: syntax_issue",
    "0:5 - 0:5         fault: redirection_target_gap",
    "0:5 - 0:5         boundary: recovery_eof",
  ].join("\n");
  const { tree } = parseCst(output, source);
  assertCstSourceContract(tree, source);
  for (const changed of [
    output.replace("0:0 - 0:5   program", "0:0 - 0:4   program"),
    output.replace("0:0 - 0:4", "0:1 - 0:4"),
    output.replace("0:0 - 0:4", "0:0 - 0:5"),
    output.replace("0:4 - 0:5       source", "0:5 - 0:4       source"),
    output.replace("issue: syntax_issue", "unowned_issue: syntax_issue"),
    output.replace(
      "fault: redirection_target_gap",
      "fragment: recovery_fragment",
    ),
    output.replace("redirection_target_gap", "invalid_redirection_target"),
    output.replace("0:5 - 0:5         boundary", "0:4 - 0:4         boundary"),
  ])
    assert.throws(
      () => assertCstSourceContract(parseCst(changed, source).tree, source),
      assert.AssertionError,
    );
});

test("diagnostic structure ignores source partitioning and retains owners, fields and faults", () => {
  const source = "cat >";
  const output = [
    "0:0 - 0:5   program",
    "0:0 - 0:5     list",
    "0:0 - 0:4       source `cat `",
    "0:4 - 0:5       io_redirect",
    "0:4 - 0:5         source `>`",
    "0:5 - 0:5         issue: syntax_issue",
    "0:5 - 0:5           fault: redirection_target_gap",
    "0:5 - 0:5           boundary: recovery_eof",
  ].join("\n");
  const expected =
    "(program (list (io_redirect issue: (syntax_issue fault: (redirection_target_gap) boundary: (recovery_eof)))))";
  for (const partitioned of [
    output,
    output.replace(
      "0:0 - 0:4       source `cat `",
      "0:0 - 0:3       source `cat`\n0:3 - 0:4       source ` `",
    ),
  ]) {
    const { tree } = parseCst(partitioned, source);
    assertCstSourceContract(tree, source);
    assert.equal(diagnosticStructure(tree), expected);
  }
  for (const changed of [
    output.replace("     list", "     compound_list"),
    output.replace("       io_redirect", "       word"),
    output.replace("issue: syntax_issue", "unowned_issue: syntax_issue"),
    output.replace(
      "fault: redirection_target_gap",
      "fragment: redirection_target_gap",
    ),
    output.replace("redirection_target_gap", "here_document_delimiter_gap"),
    output.replace("recovery_eof", "recovery_resume"),
  ])
    assert.notEqual(
      diagnosticStructure(parseCst(changed, source).tree),
      expected,
    );
});

test("every public fault has an explicit CST expectation", () => {
  const faults = nodeTypes.find((node) => node.type === "syntax_issue")?.fields
    ?.fault?.types;
  assert.ok(faults);
  const covered = new Set(
    cases.flatMap((fixture) =>
      (fixture.expected ?? []).flatMap((issue) =>
        issue.fault ? [issue.fault.kind] : [],
      ),
    ),
  );
  assert.deepEqual([...covered].sort(), faults.map((node) => node.type).sort());
});
