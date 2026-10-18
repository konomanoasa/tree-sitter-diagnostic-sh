import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { before, test } from "node:test";
import { root, treeSitterExecutable } from "../scripts/tree-sitter.js";

const measurePeakRss = ["darwin", "linux"].includes(process.platform);
const directory = join(root, "build/performance");
const cli = treeSitterExecutable();
const library = join(
  directory,
  process.platform === "win32" ? "parser.dll" : "parser.so",
);
const commandCounts = [128, 1_024];
const bodyLengths = [65_536, 1_048_576];
const nestingDepths = [32, 256];
const measurementSamples = 5;
const warmupSamples = 1;
const parseTimeoutMs = 60_000;
const normalDurationLimitMs = 1_000;
const scalingDurationSlackMs = 100;
const scalingMemorySlackBytes = 32 * 1024 * 1024;
const scalingTolerance = 2;
const env = {
  ...process.env,
  LC_ALL: "C",
  NO_COLOR: "1",
  TREE_SITTER_DIR: directory,
  TREE_SITTER_LIBDIR: directory,
  ...(process.platform === "darwin"
    ? { CC: "/opt/homebrew/opt/llvm/bin/clang" }
    : {}),
};

function run(command, args) {
  const result = spawnSync(command, args, {
    cwd: root,
    env,
    encoding: "utf8",
    timeout: parseTimeoutMs,
    maxBuffer: 1024 * 1024,
  });
  assert.ifError(result.error);
  assert.equal(result.signal, null, result.stderr);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  return result;
}

function median(values) {
  return values.toSorted((a, b) => a - b)[Math.floor(values.length / 2)];
}

before(() => {
  mkdirSync(directory, { recursive: true });
  writeFileSync(join(directory, "config.json"), '{"parser-directories":[]}\n');
  run(cli, ["build", "--output", library, root]);
});

const cases = [];
for (const count of commandCounts) {
  for (const [name, unit] of [
    [
      "normal-commands",
      'for i in a b; do if test "$i" = a; then printf "%s\\n" "犬"; fi; done\n',
    ],
    ["damaged-commands", `cat > ) @; echo \${x:bad}; echo ok\n`],
  ]) {
    cases.push({ name: `${name}-${count}`, source: unit.repeat(count) });
  }
}
for (const length of bodyLengths) {
  for (const [name, opening, body, closing] of [
    ["normal-word", "echo ", "aa", "\n"],
    ["normal-quote", 'echo "', "a ", '"\n'],
    ["unfinished-quote", 'echo "', "a ", ""],
    ["opaque-region", "cat > ) ", "a ", "; echo ok\n"],
    ["here-document", "cat <<EOF\n", "a ", "\nEOF\n"],
  ]) {
    cases.push({
      name: `${name}-${length}`,
      source: opening + body.repeat(length / 2) + closing,
    });
  }
}
for (const depth of nestingDepths) {
  for (const [name, center] of [
    ["normal-compounds", "echo ok; "],
    ["damaged-compounds", "cat > ) @; echo ok; "],
  ]) {
    cases.push({
      name: `${name}-${depth}`,
      source: `${"if true; then ".repeat(depth)}${center}${"fi; ".repeat(depth)}\n`,
    });
  }
  cases.push({
    name: `command-substitutions-${depth}`,
    source: `echo ${"$(echo ".repeat(depth)}ok${")".repeat(depth)}\n`,
  });
}

const references = new Map();

for (const { name, source } of cases) {
  test(`diagnostic_sh: fresh parse performance of ${name}`, (context) => {
    const path = join(directory, `${name}.txt`);
    writeFileSync(path, source);
    const durations = [];
    const rss = [];
    for (
      let sample = 0;
      sample < measurementSamples + warmupSamples;
      sample++
    ) {
      const args = [
        "parse",
        "--lib-path",
        library,
        "--lang-name",
        "diagnostic_sh",
        "--encoding",
        "utf8",
        "--quiet",
        "--json-summary",
        path,
      ];
      const result = measurePeakRss
        ? run("/usr/bin/time", [
            ...(process.platform === "darwin"
              ? ["-l"]
              : ["-f", "MAX_RSS_KIB=%M"]),
            cli,
            ...args,
          ])
        : run(cli, args);
      const start = result.stdout.indexOf('{\n  "parse_summaries":');
      assert.notEqual(start, -1, result.stdout);
      const { parse_summaries: summaries } = JSON.parse(
        result.stdout.slice(start),
      );
      assert.equal(summaries.length, 1);
      const [summary] = summaries;
      assert.equal(summary.successful, true, name);
      assert.equal(summary.bytes, Buffer.byteLength(source), name);
      const lines = source.split("\n");
      assert.deepEqual(
        summary.end,
        {
          row: lines.length - 1,
          column: Buffer.byteLength(lines.at(-1)),
        },
        name,
      );
      if (sample < warmupSamples) continue;
      durations.push(
        summary.duration.secs * 1000 + summary.duration.nanos / 1e6,
      );
      if (measurePeakRss) {
        const memory =
          process.platform === "darwin"
            ? /([0-9]+) +maximum resident set size/.exec(result.stderr)
            : /MAX_RSS_KIB=([0-9]+)/.exec(result.stderr);
        assert.ok(memory, result.stderr);
        rss.push(
          Number(memory[1]) * (process.platform === "darwin" ? 1 : 1024),
        );
      }
    }
    const result = {
      name,
      bytes: Buffer.byteLength(source),
      medianMs: median(durations),
      maxMs: Math.max(...durations),
      peakRssBytes: measurePeakRss ? Math.max(...rss) : null,
    };
    assert.ok(Number.isFinite(result.medianMs), name);
    if (result.peakRssBytes !== null) assert.ok(result.peakRssBytes > 0, name);
    const family = name.slice(0, name.lastIndexOf("-"));
    const limited =
      family.endsWith("compounds") || family === "command-substitutions";
    if (!limited) {
      const reference = references.get(family);
      if (reference) {
        const ratio = result.bytes / reference.bytes;
        assert.ok(
          result.medianMs <=
            reference.medianMs * ratio * scalingTolerance +
              scalingDurationSlackMs,
          `${name}: parse time grew excessively: ${JSON.stringify({ reference, result })}`,
        );
        if (result.peakRssBytes !== null)
          assert.ok(
            result.peakRssBytes <=
              reference.peakRssBytes * ratio * scalingTolerance +
                scalingMemorySlackBytes,
            `${name}: memory use grew excessively: ${JSON.stringify({ reference, result })}`,
          );
      } else {
        references.set(family, result);
      }
      if (family.endsWith("commands")) {
        assert.ok(
          result.medianMs < normalDurationLimitMs,
          `${name}: ${result.medianMs} ms`,
        );
      }
    }
    context.diagnostic(JSON.stringify(result));
  });
}
