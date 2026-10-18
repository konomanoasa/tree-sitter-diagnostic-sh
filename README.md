# tree-sitter-diagnostic-sh

[![CI](https://github.com/konomanoasa/tree-sitter-diagnostic-sh/actions/workflows/ci.yaml/badge.svg)](https://github.com/konomanoasa/tree-sitter-diagnostic-sh/actions/workflows/ci.yaml)
[![crates.io](https://img.shields.io/crates/v/konomanoasa-tree-sitter-diagnostic-sh)](https://crates.io/crates/konomanoasa-tree-sitter-diagnostic-sh)
[![npm](https://img.shields.io/npm/v/@konomanoasa/tree-sitter-diagnostic-sh)](https://www.npmjs.com/package/@konomanoasa/tree-sitter-diagnostic-sh)

[Tree-sitter](https://tree-sitter.github.io/tree-sitter/) grammar for
diagnostics of the POSIX.1-2024 Shell Command Language.

Guarantees cover fresh parsing only.

## Installation

```sh
npm install @konomanoasa/tree-sitter-diagnostic-sh
```

## Development

Development uses Node.js 24.21.0 or later.

```sh
npm install
npm run build
npm test
```

## Specification

[POSIX.1-2024 Shell Command Language](https://pubs.opengroup.org/onlinepubs/9799919799.2024edition/utilities/V3_chap02.html)

## License

[MIT](LICENSE)
