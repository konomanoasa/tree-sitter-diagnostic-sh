import assert from "node:assert/strict";

function lineOffsets(source) {
  const offsets = [0];
  for (const [index, byte] of Buffer.from(source).entries())
    if (byte === 10) offsets.push(index + 1);
  return offsets;
}

function sourceEndPoint(source) {
  const offsets = lineOffsets(source);
  return {
    row: offsets.length - 1,
    column: Buffer.byteLength(source) - offsets[offsets.length - 1],
  };
}

function parseCst(output, source) {
  const offsets = lineOffsets(source);
  const byteLength = Buffer.byteLength(source);
  function byteOffset(row, column) {
    const start = offsets[row];
    assert.notEqual(start, undefined, `row exceeds source: ${row}`);
    const end = row + 1 < offsets.length ? offsets[row + 1] - 1 : byteLength;
    assert.ok(column <= end - start, `column exceeds source: ${row}:${column}`);
    return start + column;
  }
  const native = [];
  const issues = [];
  const stack = [];
  let tree;
  let hasError = false;
  for (const line of output.split("\n")) {
    const match = /^([0-9]+):([0-9]+) +- +([0-9]+):([0-9]+)( +)([^\n]*)$/.exec(
      line,
    );
    if (!match) continue;
    const recovery = match[6].startsWith("•");
    const content = recovery ? match[6].slice(1) : match[6];
    if (content.startsWith("`")) continue;
    const label = content.replace(/[ ]+`[^\n]*$/, "").trim();
    if (!label) continue;
    const range = [
      byteOffset(Number(match[1]), Number(match[2])),
      byteOffset(Number(match[3]), Number(match[4])),
    ];
    const rangeWidth = line.indexOf("-") - 1;
    const indentation =
      match[5].length -
      Math.max(0, rangeWidth - `${match[3]}:${match[4]}`.length) +
      Number(recovery);
    while (stack.length && stack.at(-1).indentation >= indentation) stack.pop();
    const parent = stack.at(-1)?.node;
    const separator = label.indexOf(": ");
    const field = separator < 0 ? null : label.slice(0, separator);
    const kind = separator < 0 ? label : label.slice(separator + 2);
    const node = { kind, field, range, children: [] };
    if (parent) parent.children.push(node);
    else {
      assert.equal(tree, undefined, "multiple CST roots");
      tree = node;
      hasError = recovery;
    }
    stack.push({ indentation, node });
    if (kind === "syntax_issue")
      issues.push({
        range,
        owner:
          field === "issue" ? { kind: parent.kind, range: parent.range } : null,
        fault: undefined,
        boundary: undefined,
        fragments: [],
      });
    else if (field === "fault") issues.at(-1).fault = { kind, range };
    else if (field === "boundary") issues.at(-1).boundary = { kind, range };
    else if (field === "fragment") issues.at(-1).fragments.push(range);
    else if (kind === "ERROR" || kind.startsWith("MISSING"))
      native.push({ kind, range });
  }
  assert.ok(tree, "CST has no root");
  return { hasError, issues, native, tree };
}

function assertCstSourceContract(tree, source) {
  assert.equal(tree.kind, "program");
  assert.equal(tree.field, null);
  assert.equal(tree.range[1], Buffer.byteLength(source));
  const leaves = [];
  function visit(node) {
    assert.ok(node.range[0] >= 0 && node.range[0] <= node.range[1]);
    let previous = node.range[0];
    let owned = node.kind === "syntax_issue" && node.field === "issue";
    for (const child of node.children) {
      assert.ok(previous <= child.range[0]);
      assert.ok(child.range[1] <= node.range[1]);
      previous = child.range[1];
      owned = visit(child) || owned;
    }
    if (
      node.children.length &&
      node.kind !== "program" &&
      node.kind !== "syntax_issue"
    )
      assert.equal(owned, true, `unnecessary normal owner: ${node.kind}`);
    if (node.kind === "syntax_issue") {
      assert.ok(node.field === "issue" || node.field === "unowned_issue");
      const faults = node.children.filter((child) => child.field === "fault");
      const boundaries = node.children.filter(
        (child) => child.field === "boundary",
      );
      assert.equal(faults.length, 1);
      assert.equal(boundaries.length, 1);
      assert.equal(node.range[0], faults[0].range[0]);
      assert.deepEqual(boundaries[0].range, [node.range[1], node.range[1]]);
      assert.ok(node.children.every((child) => child.children.length === 0));
      if (faults[0].kind.endsWith("_gap"))
        assert.equal(faults[0].range[0], faults[0].range[1]);
      else assert.ok(faults[0].range[0] < faults[0].range[1]);
    }
    if (!node.children.length && node.kind !== "program") leaves.push(node);
    return owned;
  }
  visit(tree);
  const bytes = Buffer.from(source);
  const bom = Buffer.from("\uFEFF");
  let position = bytes.subarray(0, bom.length).equals(bom) ? bom.length : 0;
  for (const leaf of leaves) {
    if (leaf.range[0] === leaf.range[1]) continue;
    assert.equal(leaf.range[0], position);
    position = leaf.range[1];
  }
  assert.equal(position, bytes.length);
}

function diagnosticStructure(node) {
  const children = node.children
    .filter((child) => child.kind !== "source")
    .map(diagnosticStructure);
  const field = node.field ? `${node.field}: ` : "";
  return `${field}(${node.kind}${children.length ? ` ${children.join(" ")}` : ""})`;
}

export {
  assertCstSourceContract,
  diagnosticStructure,
  parseCst,
  sourceEndPoint,
};
