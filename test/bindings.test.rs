use serde_json::{Value, json};
use std::ops::ControlFlow;
use tree_sitter::{Node, ParseOptions, Parser};

fn parser() -> Parser {
  let mut parser = Parser::new();
  parser
    .set_language(&konomanoasa_tree_sitter_diagnostic_sh::LANGUAGE.into())
    .unwrap();
  parser
}

fn cases() -> Vec<Value> {
  serde_json::from_str(include_str!("support/parser-cases.json")).unwrap()
}

fn source(case: &Value) -> Vec<u8> {
  if let Some(source) = case["source"].as_str() {
    source.as_bytes().to_vec()
  } else {
    case["source"]
      .as_array()
      .unwrap()
      .iter()
      .map(|byte| u8::try_from(byte.as_u64().unwrap()).unwrap())
      .collect()
  }
}

fn fingerprint(node: Node<'_>) -> Value {
  json!({"kind": node.kind(), "range": [node.start_byte(), node.end_byte()],
    "named": node.is_named(), "extra": node.is_extra(), "error": node.is_error(),
    "missing": node.is_missing(), "hasError": node.has_error(),
    "children": (0..node.child_count()).map(|index| json!({
      "field": node.field_name_for_child(index), "node": fingerprint(node.child(index).unwrap())
    })).collect::<Vec<_>>()})
}

#[test]
fn direct_fields_match_shared_expectations() {
  let mut parser = parser();
  for case in cases() {
    let source = source(&case);
    let tree = parser.parse(&source, None).unwrap();
    let root = tree.root_node();
    assert_eq!(root.kind(), "program", "{}", case["name"]);
    assert_eq!(root.end_byte(), source.len(), "{}", case["name"]);
    assert!(!root.has_error(), "{}: {}", case["name"], root.to_sexp());
    let mut issues = Vec::new();
    let mut pending = vec![root];
    while let Some(child) = pending.pop() {
      pending.extend(
        (0..child.child_count())
          .rev()
          .map(|index| child.child(index).unwrap()),
      );
      if child.kind() != "syntax_issue" {
        continue;
      }
      let fault = child.child_by_field_name("fault").unwrap();
      let boundary = child.child_by_field_name("boundary").unwrap();
      let parent = child.parent().unwrap();
      let fields: Vec<_> = ["issue", "unowned_issue"]
        .into_iter()
        .filter(|field| {
          parent
            .children_by_field_name(field, &mut parent.walk())
            .any(|node| node == child)
        })
        .collect();
      assert_eq!(fields.len(), 1, "{}", case["name"]);
      let owner = if fields[0] == "issue" {
        assert_ne!(parent.kind(), "program");
        json!({"kind": parent.kind(), "start": parent.start_byte(), "end": parent.end_byte()})
      } else {
        Value::Null
      };
      assert_eq!(fault.parent(), Some(child));
      assert_eq!(boundary.parent(), Some(child));
      assert!(!fault.is_missing() && !boundary.is_missing());
      issues.push(json!({"range": [child.start_byte(), child.end_byte()],
        "owner": owner,
        "fault": {"kind": fault.kind(), "range": [fault.start_byte(), fault.end_byte()]},
        "boundary": {"kind": boundary.kind(), "range": [boundary.start_byte(), boundary.end_byte()]},
        "fragments": child.children_by_field_name("fragment", &mut child.walk()).map(|node| {
          assert_eq!(node.parent(), Some(child));
          assert_eq!(node.kind(), "recovery_fragment");
          assert!(!node.is_missing());
          json!([node.start_byte(), node.end_byte()])
        }).collect::<Vec<_>>()
      }));
    }
    if let Some(expected) = case["expected"].as_array() {
      assert_eq!(issues.len(), expected.len(), "{}", case["name"]);
      for (actual, expected) in issues.iter().zip(expected) {
        for (field, value) in expected.as_object().unwrap() {
          if field == "owner"
            && let Some(owner) = value.as_object()
          {
            for (field, value) in owner {
              assert_eq!(actual["owner"][field], *value, "{}", case["name"]);
            }
          } else {
            assert_eq!(actual[field], *value, "{}", case["name"]);
          }
        }
      }
    }
  }
}

#[test]
fn fresh_parses_on_a_reused_parser_match_new_instances() {
  let mut reused = parser();
  for case in cases() {
    let source = source(&case);
    let tree = reused.parse(&source, None).unwrap();
    let fresh = parser().parse(&source, None).unwrap();
    assert_eq!(
      fingerprint(tree.root_node()),
      fingerprint(fresh.root_node()),
      "{}",
      case["name"]
    );
  }
}

#[test]
fn reset_after_cancellation_matches_a_new_parser() {
  let mut reused = parser();
  const CANCELLATION_COMMANDS: usize = 10_000;
  let source = "cat > ) value; echo ok\n".repeat(CANCELLATION_COMMANDS);
  let mut cancel = |_: &tree_sitter::ParseState| ControlFlow::Break(());
  let result = reused.parse_with_options(
    &mut |offset, _| &source.as_bytes()[offset..],
    None,
    Some(ParseOptions::new().progress_callback(&mut cancel)),
  );
  assert!(result.is_none());
  reused.reset();
  let input = "cat > ; echo ok\n";
  let actual = reused.parse(input, None).unwrap();
  let expected = parser().parse(input, None).unwrap();
  assert_eq!(
    fingerprint(actual.root_node()),
    fingerprint(expected.root_node())
  );
}

#[test]
fn interleaved_instances_keep_their_own_fresh_state() {
  let mut left = parser();
  let mut right = parser();
  for (left_input, right_input) in [
    ("cat <<EOF\n$(cat > )\nEOF\n", "echo ${x:bad}\n"),
    ("echo ok\n", "if true; then echo '\n"),
    ("", "cat > ) @; echo ok\n"),
    ("cat <<EOF\n", "echo \"${x:-$%\"bad\"}\""),
    ("cat <<A <<B\n$%\nA\n$%\nB\n", "echo `echo $(( ${x%\"a`"),
  ] {
    let left_tree = left.parse(left_input, None).unwrap();
    let left_result = fingerprint(left_tree.root_node());
    let right_tree = right.parse(right_input, None).unwrap();
    assert_eq!(fingerprint(left_tree.root_node()), left_result);
    let left_fresh = parser().parse(left_input, None).unwrap();
    let right_fresh = parser().parse(right_input, None).unwrap();
    assert_eq!(left_result, fingerprint(left_fresh.root_node()));
    assert_eq!(
      fingerprint(right_tree.root_node()),
      fingerprint(right_fresh.root_node())
    );
  }
}
