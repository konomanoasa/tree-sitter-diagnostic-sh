#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/scanner.c"

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))

static const char normal_here_documents[] =
  "cat <<-'END' <<EOF\n\t'$'\n\tEND\n"
  "${x:-$(printf '%s' \"$((1 + $n))\")}\nEOF\n";

#ifdef TREE_SITTER_REUSE_ALLOCATOR
static size_t live_allocations;
static size_t allocation_count;
static size_t failing_allocation;

static bool allocation_fails(void) {
  return ++allocation_count == failing_allocation;
}

static void *test_malloc(size_t size) {
  if (allocation_fails()) return NULL;
  void *result = malloc(size);
  if (result) live_allocations++;
  return result;
}

static void *test_calloc(size_t count, size_t size) {
  if (allocation_fails()) return NULL;
  void *result = calloc(count, size);
  if (result) live_allocations++;
  return result;
}

static void *test_realloc(void *pointer, size_t size) {
  if (!pointer) return test_malloc(size);
  if (allocation_fails()) return NULL;
  return realloc(pointer, size);
}

static void test_free(void *pointer) {
  if (pointer) {
    assert(live_allocations > 0);
    live_allocations--;
  }
  free(pointer);
}

void *(*ts_current_malloc)(size_t) = test_malloc;
void *(*ts_current_calloc)(size_t, size_t) = test_calloc;
void *(*ts_current_realloc)(void *, size_t) = test_realloc;
void (*ts_current_free)(void *) = test_free;
#endif

struct MockLexer {
  TSLexer lexer;
  const char *source;
  size_t position;
  size_t end;
  size_t length;
};

static void mock_advance(TSLexer *lexer, bool skip) {
  (void)skip;
  struct MockLexer *mock = (struct MockLexer *)lexer;
  if (mock->position < mock->length) mock->position++;
  lexer->lookahead = mock->position < mock->length
    ? (unsigned char)mock->source[mock->position]
    : 0;
}

static void mock_mark_end(TSLexer *lexer) {
  struct MockLexer *mock = (struct MockLexer *)lexer;
  mock->end = mock->position;
}

static bool mock_eof(const TSLexer *lexer) {
  const struct MockLexer *mock = (const struct MockLexer *)lexer;
  return mock->position == mock->length;
}

static struct MockLexer mock_input(const char *source) {
  return (
    struct MockLexer
  ){.lexer =
      {.lookahead = (unsigned char)source[0],
        .advance = mock_advance,
        .mark_end = mock_mark_end,
        .eof = mock_eof},
    .source = source,
    .length = strlen(source)};
}

static void expect_event(
  struct Scanner *scanner,
  struct MockLexer *mock,
  struct Event expected
) {
  bool valid[TOKEN_TYPE_COUNT] = {0};
  valid[expected.kind] = true;
  assert(tree_sitter_diagnostic_sh_external_scanner_scan(
    scanner,
    &mock->lexer,
    valid
  ));
  assert(mock->lexer.result_symbol == expected.kind);
  assert(mock->end == expected.end);
}

static void
expect_events(const char *source, const struct Event *expected, size_t count) {
  struct Scanner *scanner = tree_sitter_diagnostic_sh_external_scanner_create();
  assert(scanner);
  struct MockLexer mock = mock_input(source);
  expect_event(scanner, &mock, (struct Event){START, 0});
  mock = mock_input(source);
  for (size_t i = 0; i < count; i++) {
    char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
    unsigned length =
      tree_sitter_diagnostic_sh_external_scanner_serialize(scanner, state);
    assert(length > 0 && length <= sizeof(state));
    struct MockLexer saved = mock;
    expect_event(scanner, &mock, expected[i]);
    tree_sitter_diagnostic_sh_external_scanner_deserialize(
      scanner,
      state,
      length
    );
    mock = saved;
    expect_event(scanner, &mock, expected[i]);
  }
  tree_sitter_diagnostic_sh_external_scanner_destroy(scanner);
}

static void test_parameter_issue_events(void) {
  const struct Event expected[] = {
    {BEGIN_LIST, 0},
    {BEGIN_AND_OR, 0},
    {BEGIN_PIPELINE, 0},
    {BEGIN_SIMPLE_COMMAND, 0},
    {SOURCE, 5},
    {BEGIN_WORD, 5},
    {BEGIN_PARAMETER_EXPANSION, 5},
    {SOURCE, 9},
    {BEGIN_ISSUE, 9},
    {PARAMETER_OPERATOR_ERROR, 10},
    {RECOVERY_FRAGMENT, 12},
    {RECOVERY_RESUME, 12},
    {SOURCE, 13},
    {END_OWNER, 13},
    {END_OWNER, 13},
    {END_OWNER, 13},
    {END_OWNER, 13},
    {END_OWNER, 13},
    {END_OWNER, 13},
    {SOURCE, 14},
    {FINISH, 14}
  };
  expect_events("echo ${x:bad}\n", expected, ARRAY_LENGTH(expected));
}

static void test_empty_source(void) {
  const struct Event expected[] = {{FINISH, 0}};
  expect_events("", expected, ARRAY_LENGTH(expected));
}

static void test_normal_here_documents(void) {
  const struct Event expected[] = {
    {SOURCE, sizeof(normal_here_documents) - 1},
    {FINISH, sizeof(normal_here_documents) - 1}
  };
  expect_events(normal_here_documents, expected, ARRAY_LENGTH(expected));
}

static void test_arithmetic_queue_preserves_position_priority_and_fifo(void) {
  struct ArithmeticVertex vertices[] = {
    {64, SIZE_MAX},
    {0, SIZE_MAX},
    {4096, SIZE_MAX},
    {63, SIZE_MAX},
    {4095, SIZE_MAX},
    {4097, SIZE_MAX},
    {65, SIZE_MAX},
    {1, SIZE_MAX},
  };
  size_t vertex_ids[4098];
  for (size_t i = 0; i < ARRAY_LENGTH(vertex_ids); i++)
    vertex_ids[i] = SIZE_MAX;
  for (size_t i = 0; i < ARRAY_LENGTH(vertices); i++)
    vertex_ids[vertices[i].position] = i;
  struct ArithmeticGraph graph = {
    .vertices = vertices,
    .vertex_count = ARRAY_LENGTH(vertices),
    .vertex_ids = vertex_ids,
    .length = ARRAY_LENGTH(vertex_ids) - 1,
  };
  size_t heads[ARRAY_LENGTH(vertices) * ARITHMETIC_SYMBOL_COUNT];
  for (size_t i = 0; i < ARRAY_LENGTH(heads); i++) heads[i] = SIZE_MAX;
  struct ArithmeticChart chart = {.graph = &graph, .heads = heads};
  assert(arithmetic_queue_init(&chart));
  const size_t inputs[][2] = {
    {0, 0},
    {1, 0},
    {2, 0},
    {3, 0},
    {0, 1},
    {4, 0},
    {5, 0},
    {6, 0},
    {7, 0},
  };
  for (size_t i = 0; i < ARRAY_LENGTH(inputs); i++)
    assert(arithmetic_add_item(&chart, inputs[i][0], inputs[i][1], 0, 0));
  assert(arithmetic_next(&chart) == 6);
  assert(arithmetic_add_item(&chart, 5, 1, 0, 0));
  const size_t expected[] = {9, 2, 5, 7, 0, 4, 3, 8, 1};
  for (size_t i = 0; i < ARRAY_LENGTH(expected); i++)
    assert(arithmetic_next(&chart) == expected[i]);
  for (size_t i = 0; i < graph.vertex_count; i++)
    assert(chart.pending[i].first == SIZE_MAX);
  assert(chart.ready[chart.ready_offsets[chart.ready_levels - 1]] == 0);
  ts_free(chart.items);
  ts_free(chart.hash);
  ts_free(chart.pending);
  ts_free(chart.ready);
}

static void test_unowned_regions_choose_the_deepest_later_boundary_owner(void) {
  struct Owner owners[] = {
    {.start = 0, .end = 8, .first = 0, .limit = 2},
    {.start = 0, .end = 4, .parent = 0, .first = 2, .limit = 3},
    {.start = 4, .end = 8, .parent = 0, .first = 3, .limit = 4},
    {.start = 1, .end = 4, .parent = 1},
    {.start = 4, .end = 7, .parent = 2},
  };
  struct Plan plan = {.owners = owners, .owner_count = ARRAY_LENGTH(owners)};
  const struct PublicChild children[] = {
    {0, 1, 0, 4, false},
    {0, 2, 4, 8, false},
    {1, 3, 1, 4, false},
    {2, 4, 4, 7, false},
  };
  const size_t depths[] = {1, 2, 2, 3, 3};
  size_t stack[ARRAY_LENGTH(owners)];
  const size_t cases[][3] = {{4, 4, 4}, {3, 4, 3}, {4, 5, 4}, {0, 8, 0}};
  for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
    struct Issue issue = {
      .start = cases[i][0],
      .end = cases[i][1],
      .recovery = {cases[i][1], true},
    };
    assert(
      place_unowned(&plan, children, depths, stack, &issue) == cases[i][2]
    );
  }
}

static void test_overlapping_issues_ignore_recording_order(void) {
  const struct {
    enum TokenType owner;
    struct Issue first, second, expected;
  } cases[] = {
    {BEGIN_FOR_CLAUSE,
      {INVALID_FOR_NAME, 4, 5, 0, 1, {8, true}},
      {UNSPECIFIED_EXPANSION_START, 4, 6, 4, 2, {6, true}},
      {INVALID_FOR_NAME, 4, 5, 0, 1, {8, true}}},
    {BEGIN_FOR_CLAUSE,
      {INVALID_FOR_NAME, 4, 5, 0, 1, {8, true}},
      {INVALID_ENCODING, 4, 5, 4, 0, {5, true}},
      {INVALID_ENCODING, 4, 5, 4, 0, {5, true}}},
    {BEGIN_FOR_CLAUSE,
      {INVALID_FOR_NAME, 4, 5, 0, 1, {8, true}},
      {NUL_CHARACTER, 4, 5, 4, 0, {5, true}},
      {NUL_CHARACTER, 4, 5, 4, 0, {5, true}}},
    {BEGIN_FUNCTION_DEFINITION,
      {INVALID_FUNCTION_BODY, 4, 8, 0, 1, {19, false}},
      {INVALID_ENCODING, 6, 7, 6, 0, {7, true}},
      {INVALID_ENCODING, 6, 7, 6, 0, {7, true}}},
    {BEGIN_FOR_CLAUSE,
      {INVALID_FOR_HEADER, 4, 8, 0, 1, {10, true}},
      {NUL_CHARACTER, 6, 7, 6, 0, {7, true}},
      {NUL_CHARACTER, 6, 7, 6, 0, {7, true}}},
    {BEGIN_FUNCTION_DEFINITION,
      {INVALID_FUNCTION_BODY, 5, 8, 0, 1, {19, false}},
      {INVALID_ENCODING, 4, 6, 4, 0, {6, true}},
      {INVALID_ENCODING, 4, 6, 4, 0, {6, true}}},
    {BEGIN_PARAMETER_EXPANSION,
      {PARAMETER_OPERATOR_ERROR, 4, 5, 0, 1, {8, true}},
      {NUL_CHARACTER, 5, 6, 5, 0, {6, true}},
      {PARAMETER_OPERATOR_ERROR, 4, 5, 0, 1, {8, true}}},
    {BEGIN_PIPELINE,
      {COMMAND_GAP, 4, 4, 0, 1, {8, true}},
      {UNEXPECTED_PIPE, 4, 5, 4, 0, {19, false}},
      {COMMAND_GAP, 4, 4, 0, 1, {8, true}}},
    {BEGIN_PARAMETER_EXPANSION,
      {PARAMETER_OPERATOR_ERROR, 4, 5, 0, 1, {19, false}},
      {UNSPECIFIED_PARAMETER_QUOTE, 4, 5, 0, 1, {8, true}},
      {PARAMETER_OPERATOR_ERROR, 4, 5, 0, 1, {19, false}}},
    {BEGIN_PARAMETER_EXPANSION,
      {PARAMETER_OPERATOR_ERROR, 4, 5, 0, 1, {8, true}},
      {PARAMETER_OPERATOR_GAP, 4, 4, 0, 1, {19, false}},
      {PARAMETER_OPERATOR_ERROR, 4, 5, 0, 1, {8, true}}},
    {BEGIN_PARAMETER_EXPANSION,
      {PARAMETER_OPERATOR_ERROR, 4, 5, 0, 1, {8, true}},
      {PARAMETER_OPERATOR_ERROR, 4, 5, 0, 1, {19, false}},
      {PARAMETER_OPERATOR_ERROR, 4, 5, 0, 1, {8, true}}},
    {BEGIN_PARAMETER_EXPANSION,
      {PARAMETER_OPERATOR_ERROR, 4, 5, 0, 1, {19, false}},
      {PARAMETER_OPERATOR_ERROR, 4, 5, 0, 1, {19, true}},
      {PARAMETER_OPERATOR_ERROR, 4, 5, 0, 1, {19, true}}},
    {BEGIN_FUNCTION_DEFINITION,
      {PARENTHESIS_GAP, 4, 4, 0, 1, {8, true}},
      {INVALID_FUNCTION_BODY, 4, 5, 0, 1, {19, false}},
      {PARENTHESIS_GAP, 4, 4, 0, 1, {8, true}}},
    {BEGIN_CASE_ITEM,
      {CASE_PATTERN_GAP, 4, 4, 0, 1, {8, true}},
      {CASE_PATTERN_SEPARATOR_ERROR, 4, 5, 0, 1, {19, false}},
      {CASE_PATTERN_GAP, 4, 4, 0, 1, {8, true}}},
  };
  for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
    for (size_t first = 0; first < 2; first++) {
      struct Owner owners[] = {
        {.kind = START, .end = 19},
        {.kind = cases[i].owner, .end = 18},
        {.kind = BEGIN_WORD, .start = 4, .end = 6, .parent = 1},
      };
      struct Issue issues[] = {
        first ? cases[i].second : cases[i].first,
        first ? cases[i].first : cases[i].second,
      };
      struct Plan plan = {
        .owners = owners,
        .owner_count = ARRAY_LENGTH(owners),
        .issues = issues,
        .issue_count = ARRAY_LENGTH(issues),
      };
      assert(select_regions(&plan, 19) == 1);
      const struct Issue *actual = &plan.issues[0];
      const struct Issue *expected = &cases[i].expected;
      assert(actual->kind == expected->kind);
      assert(actual->start == expected->start);
      assert(actual->end == expected->end);
      assert(actual->context_start == expected->context_start);
      assert(actual->recovery.position == expected->recovery.position);
      assert(actual->recovery.complete == expected->recovery.complete);
      assert(actual->owner == expected->owner);
    }
  }
}

static void test_reported_faults_ignore_recording_order(void) {
  const struct {
    const char *source;
    enum TokenType kind, owner;
    size_t owner_start, start, end;
    struct Recovery recovery;
  } cases[] = {
    {"if", COMMAND_GAP, BEGIN_IF_CLAUSE, 0, 2, 2, {2, false}},
    {"if fi", COMMAND_GAP, BEGIN_IF_CLAUSE, 0, 3, 3, {3, true}},
    {"if :", THEN_GAP, BEGIN_IF_CLAUSE, 0, 4, 4, {4, false}},
    {"while", COMMAND_GAP, BEGIN_WHILE_CLAUSE, 0, 5, 5, {5, false}},
    {"echo `$( \"`",
      UNDEFINED_BACKQUOTE_SUBSTITUTION,
      BEGIN_BACKQUOTE_SUBSTITUTION,
      5,
      10,
      11,
      {11, true}},
    {"echo `$( # `",
      UNDEFINED_BACKQUOTE_SUBSTITUTION,
      BEGIN_BACKQUOTE_SUBSTITUTION,
      5,
      11,
      12,
      {12, true}},
    {"echo `$( cat <<E\n`",
      UNDEFINED_BACKQUOTE_SUBSTITUTION,
      BEGIN_BACKQUOTE_SUBSTITUTION,
      5,
      17,
      18,
      {18, true}},
    {"echo `cat <<E \"`",
      UNDEFINED_BACKQUOTE_QUOTE,
      BEGIN_BACKQUOTE_SUBSTITUTION,
      5,
      15,
      16,
      {16, true}},
  };
  for (size_t c = 0; c < ARRAY_LENGTH(cases); c++) {
    struct MockLexer mock = mock_input(cases[c].source);
    struct Text source = {0};
    assert(read_source(&mock.lexer, &source));
    struct Plan plan = {0};
    struct Rejected rejected = {0};
    struct Parser parser = {
      .input = {source.units, source.count, 0, source.count, NULL, &rejected},
      .plan = &plan,
    };
    owner_begin(&parser, START, 0);
    plan.owners[0].end = source.count;
    decode_faults(&parser, &source);
    parse(&parser);
    clear_parser(&parser);
    assert(!plan.failed);
    size_t count = plan.issue_count;
    assert(count > 1);
    struct Issue *issues = ts_malloc(count * sizeof(*issues));
    struct Owner *owners = ts_malloc(plan.owner_count * sizeof(*owners));
    assert(issues && owners);
    memcpy(issues, plan.issues, count * sizeof(*issues));
    memcpy(owners, plan.owners, plan.owner_count * sizeof(*owners));
    for (size_t reverse = 0; reverse < 2; reverse++) {
      for (size_t rotation = 0; rotation < count; rotation++) {
        for (size_t i = 0; i < count; i++) {
          size_t index = (i + rotation) % count;
          plan.issues[i] = issues[reverse ? count - 1 - index : index];
        }
        memcpy(plan.owners, owners, plan.owner_count * sizeof(*owners));
        plan.issue_count = count;
        assert(select_regions(&plan, source.count) == 1);
        const struct Issue *actual = &plan.issues[0];
        assert(actual->kind == cases[c].kind);
        assert(actual->start == cases[c].start);
        assert(actual->end == cases[c].end);
        assert(actual->recovery.position == cases[c].recovery.position);
        assert(actual->recovery.complete == cases[c].recovery.complete);
        assert(plan.owners[actual->owner].kind == cases[c].owner);
        assert(plan.owners[actual->owner].start == cases[c].owner_start);
      }
    }
    ts_free(issues);
    ts_free(owners);
    ts_free(plan.issues);
    ts_free(plan.owners);
    ts_free(plan.extensions);
    ts_free(rejected.bits);
    ts_free(source.units);
  }
}

static void test_eof_gaps_follow_recovery_and_ownership(void) {
  enum { NO_PRECEDING_ISSUE, RECOVERED_AT_EOF, UNRECOVERED_AT_EOF };
  const unsigned recoveries[] = {
    NO_PRECEDING_ISSUE,
    RECOVERED_AT_EOF,
    UNRECOVERED_AT_EOF,
  };
  const struct {
    struct Issue first, second;
    enum TokenType expected;
    size_t owner;
  } cases[] = {
    {{HERE_DOCUMENT_ENDING_GAP, 30, 30, 0, 0, {30, false}},
      {SUBSTITUTION_ENDING_GAP, 30, 30, 0, 1, {30, false}},
      SUBSTITUTION_ENDING_GAP,
      1},
    {{HERE_DOCUMENT_ENDING_GAP, 30, 30, 0, 0, {30, false}},
      {HERE_DOCUMENT_ENDING_GAP, 30, 30, 0, 2, {30, false}},
      HERE_DOCUMENT_ENDING_GAP,
      2},
    {{SUBSTITUTION_ENDING_GAP, 30, 30, 0, 1, {30, false}},
      {HERE_DOCUMENT_ENDING_GAP, 30, 30, 0, 2, {30, false}},
      HERE_DOCUMENT_ENDING_GAP,
      2},
    {{HERE_DOCUMENT_ENDING_GAP, 30, 30, 0, 2, {30, false}},
      {PARAMETER_ENDING_GAP, 30, 30, 0, 3, {30, false}},
      PARAMETER_ENDING_GAP,
      3},
  };
  for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
    for (size_t first = 0; first < 2; first++) {
      for (size_t r = 0; r < ARRAY_LENGTH(recoveries); r++) {
        unsigned recovery = recoveries[r];
        struct Owner owners[] = {
          {.kind = START, .end = 30},
          {.kind = BEGIN_COMMAND_SUBSTITUTION, .start = 5, .end = 30},
          {.kind = BEGIN_HERE_DOCUMENT_BODY,
            .start = 15,
            .end = 30,
            .parent = 1},
          {.kind = BEGIN_PARAMETER_EXPANSION,
            .start = 20,
            .end = 30,
            .parent = 2},
        };
        struct Issue issues[] = {
          {INVALID_ENCODING, 28, 30, 0, 0, {30, recovery == RECOVERED_AT_EOF}},
          first ? cases[i].second : cases[i].first,
          first ? cases[i].first : cases[i].second,
        };
        size_t skip = recovery == NO_PRECEDING_ISSUE;
        struct Plan plan = {
          .owners = owners,
          .owner_count = ARRAY_LENGTH(owners),
          .issues = issues + skip,
          .issue_count = ARRAY_LENGTH(issues) - skip,
        };
        assert(
          select_regions(&plan, 30) == (recovery == RECOVERED_AT_EOF ? 2 : 1)
        );
        if (recovery != NO_PRECEDING_ISSUE) {
          assert(plan.issues[0].kind == INVALID_ENCODING);
          assert(
            plan.issues[0].recovery.complete == (recovery == RECOVERED_AT_EOF)
          );
        }
        if (recovery == UNRECOVERED_AT_EOF) continue;
        const struct Issue *gap = &plan.issues[recovery == RECOVERED_AT_EOF];
        assert(gap->kind == cases[i].expected);
        assert(gap->owner == cases[i].owner);
        assert(gap->start == 30);
        assert(gap->end == 30);
        assert(gap->recovery.position == 30);
        assert(!gap->recovery.complete);
      }
    }
  }
}

#ifdef TREE_SITTER_REUSE_ALLOCATOR
static void expect_allocation_failures(const char *source) {
  allocation_count = 0;
  failing_allocation = 0;
  struct Scanner *scanner = tree_sitter_diagnostic_sh_external_scanner_create();
  assert(scanner);
  struct MockLexer mock = mock_input(source);
  assert(prepare(scanner, &mock.lexer));
  const size_t allocation_points = allocation_count;
  tree_sitter_diagnostic_sh_external_scanner_destroy(scanner);
  assert(live_allocations == 0);
  const char retry_source[] = "echo ok\n";
  const struct Event retry_events[] = {
    {SOURCE, sizeof(retry_source) - 1},
    {FINISH, sizeof(retry_source) - 1}
  };
  for (size_t failure = 1; failure <= allocation_points; failure++) {
    allocation_count = 0;
    failing_allocation = failure;
    scanner = tree_sitter_diagnostic_sh_external_scanner_create();
    if (scanner) {
      mock = mock_input(source);
      assert(!prepare(scanner, &mock.lexer));
      assert(!scanner->started);
      char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
      assert(
        tree_sitter_diagnostic_sh_external_scanner_serialize(scanner, state) ==
        0
      );
      failing_allocation = 0;
      mock = mock_input(retry_source);
      assert(prepare(scanner, &mock.lexer));
      assert(scanner->count == ARRAY_LENGTH(retry_events));
      for (size_t i = 0; i < ARRAY_LENGTH(retry_events); i++) {
        assert(scanner->events[i].kind == retry_events[i].kind);
        assert(scanner->events[i].end == retry_events[i].end);
      }
    }
    failing_allocation = 0;
    tree_sitter_diagnostic_sh_external_scanner_destroy(scanner);
    assert(live_allocations == 0);
  }
}

static void test_allocation_failures(void) {
  enum { ALLOCATION_STRESS_COMMANDS = 32 };
  const char unit[] = "cat > ) @; echo ${x:bad}\n";
  char repeated[ALLOCATION_STRESS_COMMANDS * (sizeof(unit) - 1) + 1];
  for (size_t i = 0; i < ALLOCATION_STRESS_COMMANDS; i++)
    memcpy(repeated + i * (sizeof(unit) - 1), unit, sizeof(unit) - 1);
  repeated[sizeof(repeated) - 1] = '\0';
  const char *sources[] = {
    normal_here_documents,
    repeated,
    "for i bad (cat <<EOF\ndo\nEOF\n); do :; done\n",
    "echo ${x:$(cat <<EOF\n}\nEOF\n)}; echo ok\n",
    "echo $(cat > (cat <<EOF\nbody\nEOF\n)); cat >\n",
    "case x in &a) :;; esac; echo ${#name:-x} ${#+}\n",
    "cat <<'' <<''\n",
    "cat <<A > >out\nbody\nA\n!(cat > )\n",
    "case x in & esac; for > done; f(bad) word\n",
    "echo $'\\c@' $'\\c\\q'\n",
    "cat <<EOF \"a\nb\"\nbody\nEOF\n",
    "cat <<$'\\q'\nbody\nq\n",
    "cat <<$(printf $'a\\'b')\"tail\"\n${x:bad}\n$(printf a'b)tail\ncat >\n",
    "cat <<\"`printf \\\"ab\\\"`\"\nbody\n`printf \"ab\"`\ncat >\n",
    "echo `cat <<A <<$'\\q' <<F\nA\nq\nF\n`",
    "echo $'\\x123' ${x:@}\n",
    "echo $((1 + $x *",
    "echo $((a ? b = 1 : (c)))\n",
    "echo $((($x) + (a ? b = 1 : c)))\n",
    "echo $((($x ? 2) + $y))\n",
    "echo $((1 + (a = 1) + $x))\n",
    "echo $(($x; done",
    "echo $((1 + )",
    "echo `echo $(( ${x%\"a`",
    "echo \"${x:-$%\"bad\"}\"",
    "cat <<EOF\n",
    "cat <<A <<B\n$%\nA\n$%\nB\n",
    "{ ${x:bad}) }",
    "echo `echo $(echo x`",
    "echo `cat <<EOF\n${x:-bad`",
    "cat <<$(echo 'END')\nbody\n$(echo END)\necho $((1 + $n))\n"
  };
  for (size_t i = 0; i < ARRAY_LENGTH(sources); i++)
    expect_allocation_failures(sources[i]);
}
#endif

static void test_nesting_limit(void) {
  const struct {
    const char *prefix, *opening, *body, *closing, *suffix;
    size_t enclosing_depth, levels_per_unit;
  } contexts[] = {
    {"echo ", "$(", "x", ")", "\n", 0, 1},
    {"echo ", "${x:-", "x", "}", "\n", 0, 1},
    {"echo ", "$((", "1", "))", "\n", 0, 1},
    {"", "( ", "echo x", " )", "\n", 0, 1},
    {"", "{ ", "echo x;", " };", "\n", 0, 1},
    {"echo \"", "${x:-", "x", "}", "\"\n", 1, 1},
    {"echo `echo ", "${x:-", "x", "}", "`\n", 1, 1},
    {"cat <<EOF\n", "${x:-", "x", "}", "\nEOF\n", 0, 1},
    {"echo ", "$(( , ", "x++", " ))", "\n", 0, 2},
  };
  for (size_t context = 0; context < ARRAY_LENGTH(contexts); context++) {
    size_t levels = contexts[context].levels_per_unit;
    size_t limit = (NESTING_LIMIT - contexts[context].enclosing_depth) / levels;
    for (size_t count = limit - 1; count <= limit + 1; count++) {
      size_t depth = count * levels + contexts[context].enclosing_depth;
      size_t length = strlen(contexts[context].prefix) +
        count *
          (strlen(contexts[context].opening) +
            strlen(contexts[context].closing)) +
        strlen(contexts[context].body) +
        strlen(contexts[context].suffix);
      char *source = malloc(length + 1);
      assert(source);
      char *cursor = source;
      const char *parts[] = {
        contexts[context].prefix,
        contexts[context].opening,
        contexts[context].body,
        contexts[context].closing,
        contexts[context].suffix
      };
      const size_t repeats[] = {1, count, 1, count, 1};
      for (size_t part = 0; part < ARRAY_LENGTH(parts); part++) {
        size_t bytes = strlen(parts[part]);
        for (size_t i = 0; i < repeats[part]; i++) {
          memcpy(cursor, parts[part], bytes);
          cursor += bytes;
        }
      }
      *cursor = '\0';
      assert((size_t)(cursor - source) == length);

      struct Scanner *scanner =
        tree_sitter_diagnostic_sh_external_scanner_create();
      assert(scanner);
      struct MockLexer mock = mock_input(source);
      bool expected = depth <= NESTING_LIMIT;
      assert(prepare(scanner, &mock.lexer) == expected);
      assert(scanner->started == expected);
      if (expected) {
        assert(scanner->count == 2);
        assert(scanner->events[0].kind == SOURCE);
        assert(scanner->events[0].end == length);
        assert(scanner->events[1].kind == FINISH);
        assert(scanner->events[1].end == length);
      } else {
        char state[TREE_SITTER_SERIALIZATION_BUFFER_SIZE];
        assert(
          tree_sitter_diagnostic_sh_external_scanner_serialize(
            scanner,
            state
          ) == 0
        );
        bool valid[TOKEN_TYPE_COUNT] = {0};
        valid[FINISH] = true;
        assert(!tree_sitter_diagnostic_sh_external_scanner_scan(
          scanner,
          &mock.lexer,
          valid
        ));
      }
      mock = mock_input("echo ok\n");
      assert(prepare(scanner, &mock.lexer));
      assert(scanner->count == 2);
      assert(scanner->events[0].kind == SOURCE);
      assert(scanner->events[0].end == strlen("echo ok\n"));
      assert(scanner->events[1].kind == FINISH);
      assert(scanner->events[1].end == strlen("echo ok\n"));
      tree_sitter_diagnostic_sh_external_scanner_destroy(scanner);
      free(source);
    }
  }
}

static void test_deep_else_chain(void) {
  enum { ELSE_CHAIN_LENGTH = 100000 };
  const char head[] = "if :; then :;", link[] = " elif :; then :;",
             tail[] = " else ${x:bad}; fi\n";
  size_t length = sizeof(head) -
    1 +
    ELSE_CHAIN_LENGTH * (sizeof(link) - 1) +
    sizeof(tail) -
    1;
  char *source = malloc(length + 1);
  assert(source);
  char *cursor = source;
  memcpy(cursor, head, sizeof(head) - 1);
  cursor += sizeof(head) - 1;
  for (size_t i = 0; i < ELSE_CHAIN_LENGTH; i++) {
    memcpy(cursor, link, sizeof(link) - 1);
    cursor += sizeof(link) - 1;
  }
  memcpy(cursor, tail, sizeof(tail));
  struct Scanner *scanner = tree_sitter_diagnostic_sh_external_scanner_create();
  assert(scanner);
  struct MockLexer mock = mock_input(source);
  assert(prepare(scanner, &mock.lexer));
  size_t else_parts = 0, issues = 0;
  for (size_t i = 0; i < scanner->count; i++) {
    else_parts += scanner->events[i].kind == BEGIN_ELSE_PART;
    issues += scanner->events[i].kind == BEGIN_ISSUE;
  }
  assert(else_parts == ELSE_CHAIN_LENGTH + 1);
  assert(issues == 1);
  assert(scanner->events[scanner->count - 1].kind == FINISH);
  assert(scanner->events[scanner->count - 1].end == length);
  tree_sitter_diagnostic_sh_external_scanner_destroy(scanner);
  free(source);
}

static void test_delimiter_spelling_and_body_quoting(void) {
  const struct {
    const char *word;
    const char *delimiter;
    bool quoted;
  } cases[] = {
    {"'END'", "END", true},
    {"E\\ND", "END", true},
    {"END\\", "END\\", false},
    {"$'a\\x62'", "ab", true},
    {"$'a\\'b'", "a'b", true},
    {"$'a\\047b'", "a'b", true},
    {"${x:-'ab'}", "${x:-'ab'}", false},
    {"${x#'ab'}", "${x#'ab'}", false},
    {"\"${x:-\"ab\"}\"", "${x:-ab}", true},
    {"\"${x:-a\\}b}\"", "${x:-a\\}b}", true},
    {"$(printf 'ab')", "$(printf 'ab')", false},
    {"$(printf a\\qb)", "$(printf a\\qb)", false},
    {"$(printf 'ab')\"tail\"", "$(printf ab)tail", true},
    {"$(printf $'a\\x62')", "$(printf 'ab')", false},
    {"$(printf $'a\\'b')", "$(printf 'a'\\''b')", false},
    {"\"$(printf $'a\\x62')\"", "$(printf 'ab')", true},
    {"\"$(printf 'ab')\"", "$(printf 'ab')", true},
    {"\"$(printf \"ab\")\"", "$(printf ab)", true},
    {"\"$(printf '\"')\"", "$(printf ')\"", true},
    {"\"${x:-\"$(printf 'ab')\"}\"", "${x:-$(printf ab)}", true},
    {"\"$(printf 'a')\"$(printf 'b')", "$(printf 'a')$(printf b)", true},
    {"`printf 'ab'`", "`printf 'ab'`", false},
    {"`printf \\`x\\`#tag END`", "`printf \\`x\\`#tag END`", false},
    {"\"`printf \\\"ab\\\"`\"", "`printf \"ab\"`", true},
    {"\"`printf $'ab'`\"", "`printf $'ab'`", true},
    {"`printf a\\\nb`", "`printf ab`", false},
    {"$((1+${x:-'2'}))", "$((1+${x:-'2'}))", false},
    {"\"$((1+$(printf '2')))\"", "$((1+$(printf '2')))", true},
    {"$((printf 'ab'); :)\"tail\"", "$((printf ab); :)tail", true},
  };
  for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
    struct MockLexer mock = mock_input(cases[i].word);
    struct Text source = {0};
    assert(read_source(&mock.lexer, &source));
    struct Plan plan = {.source_end = source.count};
    struct Rejected rejected = {0};
    struct Parser parser = {
      .input = {source.units, source.count, 0, source.count, NULL, &rejected},
      .plan = &plan,
    };
    owner_begin(&parser, START, 0);
    assert(redirect_operand(&parser, HERE));
    assert(!plan.failed && !plan.issue_count);
    assert(parser.document_count == 1);
    const struct Document *here = &parser.documents[0];
    assert(here->quoted == cases[i].quoted);
    assert(here->delimiter.count == strlen(cases[i].delimiter));
    for (size_t j = 0; j < here->delimiter.count; j++)
      assert(
        here->delimiter.units[j].value == (unsigned char)cases[i].delimiter[j]
      );
    clear_parser(&parser);
    ts_free(rejected.bits);
    ts_free(plan.owners);
    ts_free(plan.issues);
    ts_free(plan.extensions);
    ts_free(source.units);
  }
}

int main(void) {
  test_delimiter_spelling_and_body_quoting();
  test_parameter_issue_events();
  test_empty_source();
  test_normal_here_documents();
  test_arithmetic_queue_preserves_position_priority_and_fifo();
  test_unowned_regions_choose_the_deepest_later_boundary_owner();
  test_overlapping_issues_ignore_recording_order();
  test_reported_faults_ignore_recording_order();
  test_eof_gaps_follow_recovery_and_ownership();
  test_nesting_limit();
  test_deep_else_chain();
#ifdef TREE_SITTER_REUSE_ALLOCATOR
  test_allocation_failures();
  assert(live_allocations == 0);
#endif
  puts("Scanner spans, state restoration, and allocation cleanup passed.");
  return 0;
}
