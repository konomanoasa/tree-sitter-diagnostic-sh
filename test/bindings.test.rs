use tree_sitter::Parser;

#[test]
fn parses_source_and_exposes_faults() {
  let language = konomanoasa_tree_sitter_diagnostic_sh::LANGUAGE.into();
  let mut parser = Parser::new();
  parser.set_language(&language).unwrap();
  assert_eq!(language.name(), Some("diagnostic_sh"));
  let source = "cat >\n";
  let tree = parser.parse(source, None).unwrap();
  let root = tree.root_node();
  assert_eq!(root.kind(), "program");
  assert!(!root.has_error());
  assert_eq!(root.byte_range(), 0..source.len());
  assert!(root.to_sexp().contains("fault: (redirection_target_gap)"));
  assert!(
    konomanoasa_tree_sitter_diagnostic_sh::NODE_TYPES
      .contains("\"redirection_target_gap\"")
  );
}
