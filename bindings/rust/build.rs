fn main() {
  println!("cargo::rerun-if-changed=src");
  let mut build = cc::Build::new();
  build.std("c17");
  if build.get_compiler().is_like_msvc() {
    build.flag("-utf-8");
  }
  build
    .include("src")
    .files(["src/parser.c", "src/scanner.c"])
    .compile("tree-sitter-diagnostic-sh");
}
