const commandCounts = [128, 512, 1024];
const bodyLengths = [65536, 262144, 1048576];
const nestingDepths = [32, 128, 256];

export const workloads = [];
for (const count of commandCounts) {
  for (const [name, unit] of [
    [
      "normal-commands",
      'for i in a b; do if test "$i" = a; then printf "%s\\n" "犬"; fi; done\n',
    ],
    ["damaged-commands", `cat > ) @; echo \${x:bad}; echo ok\n`],
  ]) {
    workloads.push({ name: `${name}-${count}`, source: unit.repeat(count) });
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
    workloads.push({
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
    workloads.push({
      name: `${name}-${depth}`,
      source: `${"if true; then ".repeat(depth)}${center}${"fi; ".repeat(depth)}\n`,
    });
  }
  workloads.push({
    name: `command-substitutions-${depth}`,
    source: `echo ${"$(echo ".repeat(depth)}ok${")".repeat(depth)}\n`,
  });
}
