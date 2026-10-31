import assert from "node:assert/strict";
import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { after, before } from "node:test";
import { createTreeSitter, root } from "../../scripts/tree-sitter.js";
import { assertCstSourceContract, parseCst, sourceEndPoint } from "./cst.js";

const parseTimeoutMs = 60_000;

let runner;
let library;

before(() => {
  runner = createTreeSitter();
  library = join(
    runner.directory,
    process.platform === "win32" ? "parser.dll" : "parser.so",
  );
  const result = runner.run(["build", "--output", library, root]);
  assert.ifError(result.error);
  assert.equal(result.status, 0, result.stdout + result.stderr);
});

after(() => runner?.close());

export function parse(input) {
  return parseSequence([input])[0];
}

export function parseSequence(inputs) {
  const sources = inputs.map((input) =>
    Array.isArray(input) ? Buffer.from(input) : input,
  );
  const paths = sources.map((source, index) => {
    const path = join(runner.directory, `source-${index}.sh`);
    writeFileSync(path, source);
    return path;
  });
  const result = runner.run(
    [
      "parse",
      "--lib-path",
      library,
      "--lang-name",
      "diagnostic_sh",
      "--cst",
      "--json-summary",
      "--encoding",
      "utf8",
      ...paths,
    ],
    { timeout: parseTimeoutMs },
  );
  assert.ifError(result.error);
  assert.equal(result.signal, null, result.stderr);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const start = result.stdout.indexOf('{\n  "parse_summaries":');
  assert.notEqual(start, -1, result.stdout + result.stderr);
  const { parse_summaries: summaries } = JSON.parse(result.stdout.slice(start));
  assert.equal(summaries.length, sources.length);
  const outputs = [];
  for (const line of result.stdout.slice(0, start).split("\n")) {
    if (/^[0-9]+:[0-9]+ +- +[0-9]+:[0-9]+ +program$/.test(line))
      outputs.push("");
    if (outputs.length) outputs[outputs.length - 1] += `${line}\n`;
  }
  assert.equal(outputs.length, sources.length);
  return sources.map((source, index) => {
    assert.equal(summaries[index].successful, true);
    assert.equal(summaries[index].bytes, Buffer.byteLength(source));
    assert.deepEqual(summaries[index].end, sourceEndPoint(source));
    const actual = parseCst(outputs[index], source);
    assert.equal(actual.hasError, false);
    assert.deepEqual(actual.native, []);
    assertCstSourceContract(actual.tree, source);
    return actual;
  });
}
