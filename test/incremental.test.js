import assert from "node:assert/strict";
import { test } from "node:test";
import { diagnosticStructure } from "./support/cst.js";
import { parse, parseSequence } from "./support/parser.js";

test("fresh parses retain diagnostic ownership and ranges after normal, damaged and unfinished sources", () => {
  const sources = [
    "cat <<EOF\n$(cat > )\nEOF\n",
    `echo \${x:bad}\n`,
    "echo ok\n",
    "if true; then echo '\n",
    "",
    "cat > ) @; echo ok\n",
    "cat <<EOF\n",
    `echo \${x:bad}\n`,
  ];
  const actual = parseSequence(sources);
  for (const [index, source] of sources.entries()) {
    const expected = parse(source);
    assert.deepEqual(actual[index].issues, expected.issues, `input ${index}`);
    assert.equal(
      diagnosticStructure(actual[index].tree),
      diagnosticStructure(expected.tree),
      `input ${index}`,
    );
  }
});
