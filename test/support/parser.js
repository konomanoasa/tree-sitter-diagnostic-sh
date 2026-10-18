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
  const source = Array.isArray(input) ? Buffer.from(input) : input;
  const path = join(runner.directory, "source.sh");
  writeFileSync(path, source);
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
      path,
    ],
    { timeout: parseTimeoutMs },
  );
  assert.ifError(result.error);
  assert.equal(result.signal, null, result.stderr);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const start = result.stdout.indexOf('{\n  "parse_summaries":');
  assert.notEqual(start, -1, result.stdout + result.stderr);
  const { parse_summaries: summaries } = JSON.parse(result.stdout.slice(start));
  assert.equal(summaries.length, 1);
  assert.equal(summaries[0].bytes, Buffer.byteLength(source));
  assert.deepEqual(summaries[0].end, sourceEndPoint(source));
  const actual = parseCst(result.stdout.slice(0, start), source);
  assert.equal(actual.hasError, false);
  assert.deepEqual(actual.native, []);
  assertCstSourceContract(actual.tree, source);
  return actual;
}
