import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import {
  existsSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { join } from "node:path";
import { test } from "node:test";
import {
  copyFiles,
  createTreeSitter,
  packageName,
  root,
} from "../scripts/tree-sitter.js";

const referencePlatform = !process.env.CI || process.platform === "darwin";

const commandTimeoutMs = 60_000;
const distributionTimeoutMs = 120_000;
const commandOutputLimitBytes = 64 * 1024 * 1024;

function temporaryDirectory(suffix) {
  const cache = join(root, "node_modules", ".cache");
  mkdirSync(cache, { recursive: true });
  return mkdtempSync(join(cache, `${packageName}-${suffix}-`));
}

test("generated checks reject stale and missing token headers without rewriting them", {
  skip: !referencePlatform,
}, () => {
  const directory = temporaryDirectory("generated-check");
  try {
    copyFiles(
      ["grammar.js", "package.json", "scripts", "src", "tree-sitter.json"],
      directory,
    );
    function check() {
      const result = spawnSync(
        process.execPath,
        ["scripts/check-generated.js"],
        {
          cwd: directory,
          encoding: "utf8",
          timeout: commandTimeoutMs,
          killSignal: "SIGKILL",
        },
      );
      assert.ifError(result.error);
      return result;
    }

    const clean = check();
    assert.equal(clean.status, 0, clean.stdout + clean.stderr);
    const header = join(directory, "src", "tokens.h");
    const changed = `${readFileSync(header, "utf8")}\n`;
    writeFileSync(header, changed);
    const stale = check();
    assert.equal(stale.status, 1, stale.stdout + stale.stderr);
    assert.ok(stale.stderr.includes(join("src", "tokens.h")), stale.stderr);
    assert.equal(readFileSync(header, "utf8"), changed);

    rmSync(header);
    const missing = check();
    assert.equal(missing.status, 1, missing.stdout + missing.stderr);
    assert.ok(missing.stderr.includes(join("src", "tokens.h")), missing.stderr);
    assert.equal(existsSync(header), false);
  } finally {
    rmSync(directory, { recursive: true, force: true });
  }
});

test("npm metadata matches the grammar and license", {
  skip: !referencePlatform,
}, () => {
  const { metadata, bindings } = JSON.parse(
    readFileSync(join(root, "tree-sitter.json"), "utf8"),
  );
  const pkg = JSON.parse(readFileSync(join(root, "package.json"), "utf8"));
  for (const key of ["version", "license", "description"])
    assert.equal(pkg[key], metadata[key]);
  assert.equal(pkg.repository, `git+${metadata.links.repository}.git`);
  assert.ok(
    readFileSync(join(root, "LICENSE"), "utf8").startsWith(
      `${pkg.license} License`,
    ),
  );
  assert.equal(bindings.python, true);
  assert.equal(bindings.rust, true);
});

function run(command, arguments_, cwd) {
  const result = spawnSync(command, arguments_, {
    cwd,
    encoding: "utf8",
    timeout: distributionTimeoutMs,
    maxBuffer: commandOutputLimitBytes,
    killSignal: "SIGKILL",
  });
  assert.ifError(result.error);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  return result.stdout;
}

test("npm archives regenerate and build a parser that reports a missing fi", () => {
  const directory = temporaryDirectory("distribution");
  const runner = createTreeSitter();
  try {
    const npm = process.env.npm_execpath;
    assert.ok(npm, "Run distribution checks through npm test.");
    const [archive] = Object.values(
      JSON.parse(
        run(
          process.execPath,
          [
            npm,
            "pack",
            "--json",
            "--ignore-scripts",
            "--pack-destination",
            directory,
          ],
          root,
        ),
      ),
    );
    run("tar", ["-xf", archive.filename], directory);
    const source = join(directory, "package");
    const generated = join(directory, "generated");
    const generation = runner.run(
      [
        "generate",
        join(source, "grammar.js"),
        "--abi",
        "latest",
        "--output",
        generated,
      ],
      { cwd: source, timeout: commandTimeoutMs },
    );
    assert.ifError(generation.error);
    assert.equal(generation.status, 0, generation.stdout + generation.stderr);
    if (referencePlatform)
      for (const file of ["grammar.json", "node-types.json", "parser.c"]) {
        assert.deepEqual(
          readFileSync(join(generated, file)),
          readFileSync(join(source, "src", file)),
          file,
        );
      }

    const library = join(
      directory,
      `diagnostic_sh.${process.platform === "win32" ? "dll" : "so"}`,
    );
    const built = runner.run(["build", source, "--output", library], {
      cwd: source,
      timeout: commandTimeoutMs,
    });
    assert.ifError(built.error);
    assert.equal(built.status, 0, built.stdout + built.stderr);

    const input = join(directory, "missing-fi.sh");
    writeFileSync(input, "if true; then :;\n");
    const parsed = runner.run(
      ["parse", "--lib-path", library, "--lang-name", "diagnostic_sh", input],
      { cwd: source, timeout: commandTimeoutMs },
    );
    assert.ifError(parsed.error);
    assert.equal(parsed.status, 0, parsed.stdout + parsed.stderr);
    assert.match(parsed.stdout, /\(fi_gap /);
  } finally {
    runner.close();
    rmSync(directory, { recursive: true, force: true });
  }
});
