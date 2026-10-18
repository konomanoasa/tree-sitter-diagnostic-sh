import assert from "node:assert/strict";
import { test } from "node:test";
import { diagnosticStructure } from "./support/cst.js";
import { parse } from "./support/parser.js";
import cases from "./support/parser-cases.json" with { type: "json" };

for (const [name, word, expected] of [
  ["single quote", "$'text'", {}],
  ["backquote substitution", "$`echo ok`", {}],
  ["escaped dollar", "$\\$x", {}],
  [
    "double quote",
    '$"text"',
    {
      quotedParameter: ["unspecified_parameter_quote", 1, 2, 7],
      body: ["unspecified_expansion_start", 0, 2, 2],
    },
  ],
  [
    "ordinary backslash",
    "$\\q",
    {
      quoted: ["unspecified_expansion_start", 0, 2, 2],
      quotedParameter: ["unspecified_expansion_start", 0, 2, 2],
      body: ["unspecified_expansion_start", 0, 2, 2],
    },
  ],
  [
    "operator",
    "$;",
    {
      quoted: ["unspecified_expansion_start", 0, 2, 2],
      parameter: ["unspecified_expansion_start", 0, 2, 2],
      quotedParameter: ["unspecified_expansion_start", 0, 2, 2],
      body: ["unspecified_expansion_start", 0, 2, 2],
    },
  ],
  [
    "a line continuation before an ordinary character",
    "$\\\n%",
    {
      unquoted: ["unspecified_expansion_start", 0, 4, 4],
      quoted: ["unspecified_expansion_start", 0, 4, 4],
      parameter: ["unspecified_expansion_start", 0, 4, 4],
      quotedParameter: ["unspecified_expansion_start", 0, 4, 4],
      body: ["unspecified_expansion_start", 0, 4, 4],
    },
  ],
]) {
  for (const [context, prefix, suffix] of [
    ["unquoted", "echo ", "\n"],
    ["quoted", 'echo "', '"\n'],
    ["parameter", "echo ${x:-", "}\n"],
    ["quotedParameter", 'echo "${x:-', '}"\n'],
    ["body", "cat <<EOF\n", "\nEOF\n"],
    ["quotedBody", "cat <<'EOF'\n", "\nEOF\n"],
  ]) {
    test(`a dollar before ${name} follows the ${context} lexical context`, () => {
      const offset = Buffer.byteLength(prefix);
      const fault = expected[context];
      assert.deepEqual(
        parse(`${prefix}${word}${suffix}`).issues.map(
          ({ fault, boundary }) => ({
            fault,
            boundary,
          }),
        ),
        fault
          ? [
              {
                fault: {
                  kind: fault[0],
                  range: [offset + fault[1], offset + fault[2]],
                },
                boundary: {
                  kind: "recovery_resume",
                  range: [offset + fault[3], offset + fault[3]],
                },
              },
            ]
          : [],
      );
    });
  }
}

for (const [context, prefix, fault] of [
  ["double quotes", 'echo "', "double_quote_gap"],
  ["a here-document body", "cat <<EOF\n", "here_document_ending_gap"],
]) {
  test(`a backslash at EOF in ${context} leaves only the enclosing ending missing`, () => {
    for (const tail of ["", "\\"]) {
      const source = prefix + tail;
      const end = Buffer.byteLength(source);
      assert.deepEqual(
        parse(source).issues.map(({ fault }) => fault),
        [{ kind: fault, range: [end, end] }],
      );
    }
  });
}

for (const separator of [";;", ";&"]) {
  for (const [context, prefix, suffix] of [
    ["case clause", "case x in ", " esac\n"],
    ["nested case clause", "case x in a) case y in ", " esac;; esac\n"],
  ]) {
    test(`an orphan ${separator} in a ${context} resumes before the next item`, () => {
      for (const item of ["", " a) :;;"]) {
        const start = Buffer.byteLength(prefix);
        assert.deepEqual(
          parse(`${prefix}${separator}${item}${suffix}`).issues,
          [
            {
              owner: null,
              range: [start, start + 2],
              fault: {
                kind:
                  separator === ";;"
                    ? "unexpected_case_separator"
                    : "unexpected_case_fallthrough",
                range: [start, start + 2],
              },
              boundary: {
                kind: "recovery_resume",
                range: [start + 2, start + 2],
              },
              fragments: [],
            },
          ],
        );
      }
    });
  }
}

for (const operator of [">", "<<"]) {
  for (const opaque of [
    ") ",
    ') "2>hidden" ',
    ") $(echo 2>hidden) ",
    "(x) ",
    "(x >hidden) ",
  ]) {
    test(`${operator} recovers after ${JSON.stringify(opaque)} at an independent redirect`, () => {
      for (const redirect of [">", "2>", "2\\\n>"]) {
        const prefix = `cat ${operator} `;
        const next = Buffer.byteLength(prefix + opaque);
        const source = `${prefix}${opaque}${redirect}out >\n`;
        const gap = Buffer.byteLength(source) - 1;
        const start = Buffer.byteLength(prefix);
        assert.deepEqual(
          parse(source).issues.map(({ owner, fault, boundary }) => ({
            owner: owner && { kind: owner.kind, start: owner.range[0] },
            fault,
            boundary,
          })),
          [
            {
              owner: { kind: "io_redirect", start: 4 },
              fault: {
                kind:
                  operator === ">"
                    ? "invalid_redirection_target"
                    : "invalid_here_document_delimiter",
                range: [start, start + 1],
              },
              boundary: { kind: "recovery_resume", range: [next, next] },
            },
            {
              owner: { kind: "io_redirect", start: gap - 1 },
              fault: { kind: "redirection_target_gap", range: [gap, gap] },
              boundary: { kind: "recovery_resume", range: [gap, gap] },
            },
          ],
        );
      }
    });
  }
}

for (const fixture of cases) {
  test(fixture.name, () => {
    const actual = parse(fixture.source);
    if (fixture.expected) {
      assert.equal(actual.issues.length, fixture.expected.length);
      for (const [index, expected] of fixture.expected.entries()) {
        const { owner, ...issue } = actual.issues[index];
        const values = {
          ...issue,
          owner: owner && {
            kind: owner.kind,
            start: owner.range[0],
            ...(expected.owner && "end" in expected.owner
              ? { end: owner.range[1] }
              : {}),
          },
        };
        assert.deepEqual(
          Object.fromEntries(
            Object.keys(expected).map((key) => [key, values[key]]),
          ),
          expected,
        );
      }
    }
    if (fixture.children) {
      assert.deepEqual(
        actual.tree.children
          .filter((node) => node.kind !== "source")
          .map(({ kind, field, range }) => ({ kind, field, range })),
        fixture.children,
      );
    }
    if (fixture.structure) {
      assert.equal(diagnosticStructure(actual.tree), fixture.structure);
    }
  });
}

for (const [name, word, rejected, delimiter] of [
  ["parameter double quotes", `"\${x:-"ab"}"`, `\${x:-"ab"}`, `\${x:-ab}`],
  [
    "quoted parameter brace",
    `"\${x:-"a}b"}tail"`,
    `\${x:-"a}b"}tail`,
    `\${x:-a}b}tail`,
  ],
  [
    "nested parameter quotes",
    `"\${x:-"\${y:-"ab"}"}"`,
    `\${x:-"\${y:-"ab"}"}`,
    `\${x:-\${y:-ab}}`,
  ],
  [
    "quoted parameter command",
    `"\${x:-"$(printf 'ab')"}"`,
    `\${x:-$(printf ab)}`,
    `\${x:-$(printf 'ab')}`,
  ],
  ["prefix pattern", `"\${x#'ab'}"`, `\${x#ab}`, `\${x#'ab'}`],
  ["long prefix pattern", `"\${x##'ab'}"`, `\${x##ab}`, `\${x##'ab'}`],
  ["suffix pattern", `"\${x%'ab'}"`, `\${x%ab}`, `\${x%'ab'}`],
  ["long suffix pattern", `"\${x%%'ab'}"`, `\${x%%ab}`, `\${x%%'ab'}`],
  [
    "command single quotes",
    `"$(printf 'ab')"`,
    "$(printf ab)",
    "$(printf 'ab')",
  ],
  [
    "command double quotes",
    '"$(printf "ab")"',
    '$(printf "ab")',
    "$(printf ab)",
  ],
  ["quoted parenthesis", `"$(printf ')')"`, "$(printf ))", "$(printf ')')"],
  [
    "nested command",
    `"$(printf $(printf 'ab'))"`,
    "$(printf $(printf ab))",
    "$(printf $(printf 'ab'))",
  ],
  [
    "parameter command",
    `"\${x:-$(printf 'ab')}"`,
    `\${x:-$(printf ab)}`,
    `\${x:-$(printf 'ab')}`,
  ],
  ["backquote", "\"`printf 'ab'`\"", "`printf ab`", "`printf 'ab'`"],
  [
    "arithmetic command",
    `"$((1+$(printf '2')))"`,
    "$((1+$(printf 2)))",
    "$((1+$(printf '2')))",
  ],
  [
    "concatenated word",
    `pre"$(printf 'ab')"post`,
    "pre$(printf ab)post",
    "pre$(printf 'ab')post",
  ],
  ["unquoted pattern", `\${x#'ab'}`, `\${x#'ab'}`, `\${x#ab}`],
  ["unquoted command", "$(printf 'ab')", "$(printf 'ab')", "$(printf ab)"],
  [
    "adjacent quoted and unquoted commands",
    `"$(printf 'a')"$(printf 'b')`,
    "$(printf a)$(printf b)",
    "$(printf 'a')$(printf b)",
  ],
]) {
  test(`here-document delimiter ${name} matches only its quoted spelling`, () => {
    for (const operator of ["<<", "<<-"]) {
      const prefix = `cat ${operator}${word}\n`;
      const indent = operator === "<<-" ? "\t" : "";
      const source = `${prefix}${indent}$%\n${indent}${rejected}\n${indent})\n${indent}${delimiter}\ncat >\n`;
      const redirect = Buffer.byteLength(source) - 2;
      assert.deepEqual(parse(source).issues, [
        {
          owner: { kind: "io_redirect", range: [redirect, redirect + 1] },
          range: [redirect + 1, redirect + 1],
          fault: {
            kind: "redirection_target_gap",
            range: [redirect + 1, redirect + 1],
          },
          boundary: {
            kind: "recovery_resume",
            range: [redirect + 1, redirect + 1],
          },
          fragments: [],
        },
      ]);
      const incomplete = `${prefix}${indent}${rejected}\n`;
      const end = Buffer.byteLength(incomplete);
      assert.deepEqual(parse(incomplete).issues, [
        {
          owner: {
            kind: "here_document_body",
            range: [Buffer.byteLength(prefix), end],
          },
          range: [end, end],
          fault: { kind: "here_document_ending_gap", range: [end, end] },
          boundary: { kind: "recovery_eof", range: [end, end] },
          fragments: [],
        },
      ]);
      assert.deepEqual(
        parse(`${incomplete}${indent}${delimiter}\n`).issues,
        [],
      );
    }
  });
}

for (const [name, prefix, suffix] of [
  ["ordinary word", 'echo "', '"\n'],
  [
    "argument after a here-document declaration",
    'cat <<EOF "',
    '"\nbody\nEOF\n',
  ],
  [
    "arithmetic after a here-document declaration",
    "cat <<EOF $(( ",
    " ))\nbody\nEOF\n",
  ],
  ["here-document body", "cat <<EOF\n", "\nEOF\n"],
]) {
  test(`parameter quotes in ${name} retain their ordinary diagnostic`, () => {
    const start = Buffer.byteLength(prefix);
    const source = `${prefix}\${x:-"ab"}${suffix}`;
    assert.deepEqual(parse(source).issues, [
      {
        owner: { kind: "parameter_expansion", range: [start, start + 10] },
        range: [start + 5, start + 9],
        fault: {
          kind: "unspecified_parameter_quote",
          range: [start + 5, start + 6],
        },
        boundary: {
          kind: "recovery_resume",
          range: [start + 9, start + 9],
        },
        fragments: [[start + 6, start + 9]],
      },
    ]);
  });
}

for (const fixture of [
  {
    name: "an unowned closer truncates the preceding brace-group body",
    source: `{ \${x:bad}) }`,
    parameter: [2, 10],
    fault: [6, 7],
    resume: 9,
    unowned: [10, 12],
    parent: "brace_group",
  },
  {
    name: "an unowned closer truncates the preceding case-item body",
    source: `case x in a) \${x:bad}) ;; esac`,
    parameter: [13, 21],
    fault: [17, 18],
    resume: 20,
    unowned: [21, 23],
    parent: "case_item",
  },
]) {
  test(fixture.name, () => {
    const actual = parse(fixture.source);
    assert.deepEqual(actual.issues, [
      {
        owner: { kind: "parameter_expansion", range: fixture.parameter },
        range: [fixture.fault[0], fixture.resume],
        fault: { kind: "parameter_operator_error", range: fixture.fault },
        boundary: {
          kind: "recovery_resume",
          range: [fixture.resume, fixture.resume],
        },
        fragments: [[fixture.fault[1], fixture.resume]],
      },
      {
        owner: null,
        range: fixture.unowned,
        fault: {
          kind: "unmatched_parenthesis",
          range: [fixture.unowned[0], fixture.unowned[0] + 1],
        },
        boundary: {
          kind: "recovery_resume",
          range: [fixture.unowned[1], fixture.unowned[1]],
        },
        fragments: [[fixture.unowned[0] + 1, fixture.unowned[1]]],
      },
    ]);
    function find(node) {
      if (node.kind === fixture.parent) return node;
      for (const child of node.children) {
        const found = find(child);
        if (found) return found;
      }
    }
    const parent = find(actual.tree);
    assert.ok(parent);
    assert.deepEqual(
      parent.children.find((child) => child.kind === "compound_list").range,
      [fixture.parameter[0], fixture.unowned[0]],
    );
    assert.equal(
      parent.children.find((child) => child.kind === "syntax_issue").field,
      "unowned_issue",
    );
  });
}

for (const operator of [
  "<",
  ">",
  ">>",
  ">|",
  "<&",
  ">&",
  "<>",
  "2>",
  "2\\\n>",
]) {
  for (const [prefix, operand, suffix] of [
    ["while ", "do", "; do :; done\n"],
    ["while :; do ", "done", "; done\n"],
    ["if ", "then", "; then :; fi\n"],
    ["if :; then ", "elif", "; elif :; then :; fi\n"],
    ["if :; then ", "else", "; else :; fi\n"],
    ["if :; then ", "fi", "; fi\n"],
    ["case x in a) ", "esac", ";; esac\n"],
    ["{ ", "}", "; }\n"],
    ["", "if", "\n"],
  ]) {
    test(`recovery consumes ${JSON.stringify(operator)} ${operand} as a redirect operand`, () => {
      const command = `${prefix}thing: ${operator} ${operand}`;
      const start = Buffer.byteLength(prefix);
      const resume = Buffer.byteLength(command);
      assert.deepEqual(
        parse(command + suffix).issues.map(({ fault, boundary }) => ({
          fault,
          boundary,
        })),
        [
          {
            fault: { kind: "reserved_colon_word", range: [start, start + 6] },
            boundary: { kind: "recovery_resume", range: [resume, resume] },
          },
        ],
      );
    });
  }
}

for (const operator of [">", "<<"]) {
  for (const [name, prefix, suffix] of [
    ["semicolon", "while ", "; do :; done\n"],
    ["newline", "while ", "\ndo :; done\n"],
    ["parenthesis", "(", ")\n"],
    ["case separator", "case x in a) ", ";; esac\n"],
    ["case fallthrough", "case x in a) ", ";& b) :;; esac\n"],
    ["EOF", "", ""],
  ]) {
    test(`recovery leaves ${name} after ${operator} outside the redirect operand`, () => {
      const command = `${prefix}thing: ${operator}`;
      const start = Buffer.byteLength(prefix);
      const resume = Buffer.byteLength(command);
      assert.deepEqual(
        parse(command + suffix).issues.map(({ fault, boundary }) => ({
          fault,
          boundary,
        })),
        [
          {
            fault: { kind: "reserved_colon_word", range: [start, start + 6] },
            boundary: {
              kind: suffix ? "recovery_resume" : "recovery_eof",
              range: [resume, resume],
            },
          },
        ],
      );
    });
  }
}

for (const [name, prefix, recovery] of [
  ["one pending body", "cat <<E; for x ", `\n\${x:bad}\nE\n`],
  ["a quoted body", "cat <<'E'; for x ", `\n\${x:bad}\ndo\nE\n`],
  ["a tab-stripped body", "cat <<-E; for x ", `\n\t\${x:bad}\n\tdo\n\tE\n`],
  [
    "multiple pending bodies",
    "cat <<A <<'B'; for x ",
    `\n\${x:bad}\ndo\nA\n\${y:bad}\ndo\nB\n`,
  ],
  ["a delimiter named do", "for x ", ` <<do\ndone\n\${x:bad}\ndo\n`],
  ["a delimiter named done", "for x ", ` <<done\ndo\n\${x:bad}\ndone\n`],
  ["a quoted delimiter named do", "for x ", ` <<'do'\ndone\n\${x:bad}\ndo\n`],
  [
    "a tab-stripped delimiter named do",
    "for x ",
    ` 2<<-do\n\tdone\n\t\${x:bad}\n\tdo\n`,
  ],
  [
    "multiple delimiters named after keywords",
    "for x ",
    ` <<do <<done\n\${x:bad}\ndo\n\${y:bad}\ndone\n`,
  ],
  [
    "a delimiter with a line continuation",
    "for x ",
    ` <<d\\\no\ndone\n\${x:bad}\ndo\n`,
  ],
]) {
  test(`header recovery crosses ${name} before diagnosing the resumed loop body`, () => {
    const header = `${prefix}+${recovery}`;
    const source = `${header}do cat >; done\n`;
    const start = Buffer.byteLength(prefix);
    const resume = Buffer.byteLength(header);
    const gap = resume + 8;
    assert.deepEqual(parse(source).issues, [
      {
        owner: {
          kind: "for_clause",
          range: [prefix.indexOf("for"), Buffer.byteLength(source) - 1],
        },
        range: [start, resume],
        fault: { kind: "invalid_for_header", range: [start, start + 1] },
        boundary: { kind: "recovery_resume", range: [resume, resume] },
        fragments: [[start + 1, resume]],
      },
      {
        owner: { kind: "io_redirect", range: [gap - 1, gap] },
        range: [gap, gap],
        fault: { kind: "redirection_target_gap", range: [gap, gap] },
        boundary: { kind: "recovery_resume", range: [gap, gap] },
        fragments: [],
      },
    ]);
  });
}

for (const [name, source] of [
  ["before a here-document body", "for x + <<do"],
  ["inside a here-document body", `for x + <<do\n\${x:bad}\ndone\n`],
  ["inside a brace group", "for x + { :;"],
  ["inside a for loop", "for x + for y; do :;"],
  ["inside a conditional", "for x + if :; then :;"],
]) {
  test(`header recovery stays opaque at EOF ${name}`, () => {
    const end = Buffer.byteLength(source);
    assert.deepEqual(parse(source).issues, [
      {
        owner: { kind: "for_clause", range: [0, end] },
        range: [6, end],
        fault: { kind: "invalid_for_header", range: [6, 7] },
        boundary: { kind: "recovery_eof", range: [end, end] },
        fragments: [[7, end]],
      },
    ]);
  });
}

for (const [name, compound] of [
  ["a subshell", "( : )"],
  ["a brace group", "{ :; }"],
  ["a for loop", "for y; do :; done"],
  ["a while loop", "while :; do :; done"],
  ["an until loop", "until :; do :; done"],
  ["a conditional", "if :; then :; elif :; then :; else :; fi"],
  ["a case clause", "case x in do) :;; esac"],
  ["nested compounds", "{ for y; do if :; then :; fi; done; }"],
  ["an inner fault", `for y; do echo \${x:bad}; done`],
  ["a here-document", "for y; do cat <<E\ndo\ndone\nE\ndone"],
  ["a quoted keyword", "'for'"],
  ["a keyword filename", ">for"],
  ["a keyword delimiter", "<<for\nfor\n"],
]) {
  for (const [context, opening, closing] of [
    ["top level", "", "\n"],
    ["a brace group", "{ ", "; }\n"],
    ["a loop body", "while :; do ", "; done\n"],
  ]) {
    test(`header recovery in ${context} skips ${name} before resuming`, () => {
      const header = `${opening}for x + ${compound}; `;
      const body = "do cat >; done";
      const start = Buffer.byteLength(opening);
      const resume = Buffer.byteLength(header);
      const gap = resume + 8;
      assert.deepEqual(parse(header + body + closing).issues, [
        {
          owner: {
            kind: "for_clause",
            range: [start, resume + Buffer.byteLength(body)],
          },
          range: [start + 6, resume],
          fault: { kind: "invalid_for_header", range: [start + 6, start + 7] },
          boundary: { kind: "recovery_resume", range: [resume, resume] },
          fragments: [[start + 7, resume]],
        },
        {
          owner: { kind: "io_redirect", range: [gap - 1, gap] },
          range: [gap, gap],
          fault: { kind: "redirection_target_gap", range: [gap, gap] },
          boundary: { kind: "recovery_resume", range: [gap, gap] },
          fragments: [],
        },
      ]);
    });
  }
}

for (const [name, opening, outer, ending] of [
  [
    "command substitution",
    "echo $(",
    "command_substitution",
    "substitution_ending_gap",
  ],
  [
    "backquote substitution",
    "echo `",
    "backquote_substitution",
    "backquote_gap",
  ],
]) {
  for (const [body, owner, start, fault] of [
    ["cat <<E", outer, 5, ending],
    [
      "cat <<E <<F\nE",
      "here_document_body",
      opening.length + 12,
      "here_document_ending_gap",
    ],
    [
      "cat <<E <<F\nE\n",
      "here_document_body",
      opening.length + 14,
      "here_document_ending_gap",
    ],
    [
      "cat <<E\nbody",
      "here_document_body",
      opening.length + 8,
      "here_document_ending_gap",
    ],
    [
      "cat <<E\n${x",
      "parameter_expansion",
      opening.length + 8,
      "parameter_ending_gap",
    ],
    [
      "cat <<E\n$(cat <<F",
      "command_substitution",
      opening.length + 8,
      "substitution_ending_gap",
    ],
  ]) {
    test(`EOF in ${name} after ${JSON.stringify(body)} selects the innermost started owner`, () => {
      const source = opening + body;
      const end = Buffer.byteLength(source);
      assert.deepEqual(parse(source).issues, [
        {
          owner: { kind: owner, range: [start, end] },
          range: [end, end],
          fault: { kind: fault, range: [end, end] },
          boundary: { kind: "recovery_eof", range: [end, end] },
          fragments: [],
        },
      ]);
    });
  }
}

for (const context of [
  {
    name: "a word",
    prefix: "echo ",
    lexicalOwner: "word",
    lexicalStart: 5,
    gapOwner: null,
    gapStart: 0,
    gap: null,
  },
  {
    name: "double quotes",
    prefix: 'echo "',
    lexicalOwner: "double_quoted",
    lexicalStart: 5,
    gapOwner: "double_quoted",
    gapStart: 5,
    gap: "double_quote_gap",
  },
  {
    name: "a parameter word",
    prefix: "echo ${x:-",
    lexicalOwner: "word",
    lexicalStart: 10,
    gapOwner: "parameter_expansion",
    gapStart: 5,
    gap: "parameter_ending_gap",
  },
  {
    name: "a command substitution",
    prefix: "echo $(echo ",
    lexicalOwner: "word",
    lexicalStart: 12,
    gapOwner: "command_substitution",
    gapStart: 5,
    gap: "substitution_ending_gap",
  },
  {
    name: "a backquote substitution",
    prefix: "echo `echo ",
    lexicalOwner: "word",
    lexicalStart: 11,
    gapOwner: "backquote_substitution",
    gapStart: 5,
    gap: "backquote_gap",
  },
  {
    name: "a here-document body",
    prefix: "cat <<E\n",
    lexicalOwner: "here_document_body",
    lexicalStart: 8,
    gapOwner: "here_document_body",
    gapStart: 8,
    gap: "here_document_ending_gap",
  },
  {
    name: "a here-document body inside an unfinished substitution",
    prefix: "$(cat <<E\n",
    lexicalOwner: "here_document_body",
    lexicalStart: 10,
    gapOwner: "here_document_body",
    gapStart: 10,
    gap: "here_document_ending_gap",
  },
  {
    name: "a parameter inside a here-document body",
    prefix: "cat <<E\n${x:-",
    lexicalOwner: "word",
    lexicalStart: 13,
    gapOwner: "parameter_expansion",
    gapStart: 8,
    gap: "parameter_ending_gap",
  },
]) {
  for (const { name, tail, kind, owned } of [
    {
      name: "an invalid expansion start",
      tail: [36, 37],
      kind: "unspecified_expansion_start",
      owned: true,
    },
    {
      name: "an invalid UTF-8 byte",
      tail: [255],
      kind: "invalid_encoding",
      owned: false,
    },
    {
      name: "a truncated UTF-8 sequence",
      tail: [226, 130],
      kind: "invalid_encoding",
      owned: false,
    },
    { name: "a NUL", tail: [0], kind: "nul_character", owned: false },
  ]) {
    test(`${name} at EOF in ${context.name} completes recovery before any independent gap`, () => {
      const source = [...Buffer.from(context.prefix), ...tail];
      const start = Buffer.byteLength(context.prefix);
      const end = source.length;
      const expected = [
        {
          owner: owned
            ? { kind: context.lexicalOwner, range: [context.lexicalStart, end] }
            : null,
          range: [start, end],
          fault: { kind, range: [start, end] },
          boundary: { kind: "recovery_resume", range: [end, end] },
          fragments: [],
        },
      ];
      if (context.gap) {
        expected.push({
          owner: { kind: context.gapOwner, range: [context.gapStart, end] },
          range: [end, end],
          fault: { kind: context.gap, range: [end, end] },
          boundary: { kind: "recovery_eof", range: [end, end] },
          fragments: [],
        });
      }
      assert.deepEqual(parse(source).issues, expected);
    });
  }
}

for (const [name, tail, kind] of [
  ["an invalid UTF-8 byte", [255], "invalid_encoding"],
  ["a NUL", [0], "nul_character"],
]) {
  for (const suffix of ["", "\nbody\n"]) {
    test(`${name} in a here-document delimiter remains unrecovered with ${JSON.stringify(suffix)}`, () => {
      const source = [
        ...Buffer.from("cat <<"),
        ...tail,
        ...Buffer.from(suffix),
      ];
      const end = source.length;
      assert.deepEqual(parse(source).issues, [
        {
          owner: null,
          range: [6, end],
          fault: { kind, range: [6, 7] },
          boundary: { kind: "recovery_eof", range: [end, end] },
          fragments: suffix ? [[7, end]] : [],
        },
      ]);
    });
  }
}

const interactions = [
  {
    name: "parameter operator",
    damaged: `echo \${x:bad}`,
    normal: `echo \${x:-bad}`,
    issues: [
      {
        range: [9, 12],
        owner: {
          kind: "parameter_expansion",
          range: [5, 13],
        },
        fault: {
          kind: "parameter_operator_error",
          range: [9, 10],
        },
        boundary: {
          kind: "recovery_resume",
          range: [12, 12],
        },
        fragments: [[10, 12]],
      },
    ],
  },
  {
    name: "redirection target",
    damaged: "cat > ; echo ok",
    normal: "cat > out; echo ok",
    issues: [
      {
        range: [6, 6],
        owner: {
          kind: "io_redirect",
          range: [4, 6],
        },
        fault: {
          kind: "redirection_target_gap",
          range: [6, 6],
        },
        boundary: {
          kind: "recovery_resume",
          range: [6, 6],
        },
        fragments: [],
      },
    ],
  },
  {
    name: "case pattern",
    damaged: "case x in (|a) :;; esac",
    normal: "case x in (a) :;; esac",
    issues: [
      {
        range: [11, 11],
        owner: {
          kind: "case_item",
          range: [10, 18],
        },
        fault: {
          kind: "case_pattern_gap",
          range: [11, 11],
        },
        boundary: {
          kind: "recovery_resume",
          range: [11, 11],
        },
        fragments: [],
      },
    ],
  },
  {
    name: "loop name",
    damaged: "for 1x; do :; done",
    normal: "for ix; do :; done",
    issues: [
      {
        range: [4, 8],
        owner: {
          kind: "for_clause",
          range: [0, 18],
        },
        fault: {
          kind: "invalid_for_name",
          range: [4, 5],
        },
        boundary: {
          kind: "recovery_resume",
          range: [8, 8],
        },
        fragments: [[5, 8]],
      },
    ],
  },
  {
    name: "loop name with a word fault at the same start",
    damaged: "for $%; do :; done",
    normal: "for ix; do :; done",
    issues: [
      {
        range: [4, 8],
        owner: { kind: "for_clause", range: [0, 18] },
        fault: { kind: "invalid_for_name", range: [4, 5] },
        boundary: { kind: "recovery_resume", range: [8, 8] },
        fragments: [[5, 8]],
      },
    ],
  },
  {
    name: "dollar quote escape",
    damaged: "echo $'\\qtail'",
    normal: "echo $'\\ntail'",
    issues: [
      {
        range: [7, 13],
        owner: {
          kind: "dollar_single_quote_escape",
          range: [7, 13],
        },
        fault: {
          kind: "unspecified_dollar_quote_escape",
          range: [7, 9],
        },
        boundary: {
          kind: "recovery_resume",
          range: [13, 13],
        },
        fragments: [[9, 13]],
      },
    ],
  },
  {
    name: "negation and inner redirection",
    damaged: "!(cat > )",
    normal: "! (cat > out)",
    issues: [
      {
        range: [0, 1],
        owner: {
          kind: "pipeline",
          range: [0, 9],
        },
        fault: {
          kind: "unspecified_negation_spacing",
          range: [0, 1],
        },
        boundary: {
          kind: "recovery_resume",
          range: [1, 1],
        },
        fragments: [],
      },
      {
        range: [8, 8],
        owner: {
          kind: "io_redirect",
          range: [6, 8],
        },
        fault: {
          kind: "redirection_target_gap",
          range: [8, 8],
        },
        boundary: {
          kind: "recovery_resume",
          range: [8, 8],
        },
        fragments: [],
      },
    ],
  },
  {
    name: "required parameter word",
    damaged: `echo \${#+}`,
    normal: `echo \${#+x}`,
    issues: [
      {
        range: [9, 9],
        owner: {
          kind: "parameter_expansion",
          range: [5, 10],
        },
        fault: {
          kind: "parameter_word_gap",
          range: [9, 9],
        },
        boundary: {
          kind: "recovery_resume",
          range: [9, 9],
        },
        fragments: [],
      },
    ],
  },
  {
    name: "reserved colon word",
    damaged: "thing: ; echo ok",
    normal: "thing ; echo ok",
    issues: [
      {
        range: [0, 7],
        owner: {
          kind: "word",
          range: [0, 7],
        },
        fault: {
          kind: "reserved_colon_word",
          range: [0, 6],
        },
        boundary: {
          kind: "recovery_resume",
          range: [7, 7],
        },
        fragments: [[6, 7]],
      },
    ],
  },
];

const contexts = [
  {
    name: "brace group",
    prefix: "{ ",
    suffix: "; }\n",
  },
  {
    name: "subshell",
    prefix: "( ",
    suffix: " )\n",
  },
  {
    name: "if body",
    prefix: "if true; then ",
    suffix: "; fi\n",
  },
  {
    name: "for body",
    prefix: "for x in a b; do ",
    suffix: "; done\n",
  },
  {
    name: "case body",
    prefix: "case x in x) ",
    suffix: ";; esac\n",
  },
  {
    name: "function body",
    prefix: "f() { ",
    suffix: "; }\n",
  },
  {
    name: "command substitution",
    prefix: 'printf "%s" "$( ',
    suffix: ' )"\n',
  },
  {
    name: "backquote substitution",
    prefix: 'printf "%s" ` ',
    suffix: " `\n",
  },
  {
    name: "here-document substitution",
    prefix: "cat <<EOF\n$( ",
    suffix: " )\nEOF\n",
  },
  {
    name: "UTF-8 prefix",
    prefix: "printf 'あ'; ",
    suffix: "\n",
  },
];

for (const context of contexts) {
  for (const fixture of interactions) {
    test(`${context.name}: ${fixture.name}: owned damage`, () => {
      const source = context.prefix + fixture.damaged + context.suffix;
      const actual = parse(source);
      const offset = Buffer.byteLength(context.prefix);
      const shifted = (range) => range.map((position) => position + offset);
      const expected = fixture.issues.map((issue) => ({
        range: shifted(issue.range),
        owner: { ...issue.owner, range: shifted(issue.owner.range) },
        fault: { ...issue.fault, range: shifted(issue.fault.range) },
        boundary: { ...issue.boundary, range: shifted(issue.boundary.range) },
        fragments: issue.fragments.map(shifted),
      }));
      assert.deepEqual(actual.issues, expected);
    });
    test(`${context.name}: ${fixture.name}: normal counterpart`, () => {
      const source = context.prefix + fixture.normal + context.suffix;
      const actual = parse(source);
      assert.deepEqual(actual.issues, []);
    });
  }
}
