#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum TokenType {
  START,
  FINISH,
  BEGIN_LIST,
  BEGIN_COMPOUND_LIST,
  BEGIN_AND_OR,
  BEGIN_PIPELINE,
  BEGIN_SIMPLE_COMMAND,
  BEGIN_WORD,
  BEGIN_SUBSHELL,
  BEGIN_BRACE_GROUP,
  BEGIN_IF_CLAUSE,
  BEGIN_ELSE_PART,
  BEGIN_FOR_CLAUSE,
  BEGIN_WHILE_CLAUSE,
  BEGIN_UNTIL_CLAUSE,
  BEGIN_DO_GROUP,
  BEGIN_CASE_CLAUSE,
  BEGIN_CASE_ITEM,
  BEGIN_FUNCTION_DEFINITION,
  BEGIN_IO_REDIRECT,
  BEGIN_HERE_DOCUMENT_BODY,
  BEGIN_SINGLE_QUOTED,
  BEGIN_DOUBLE_QUOTED,
  BEGIN_DOLLAR_SINGLE_QUOTED,
  BEGIN_ESCAPED_CHARACTER,
  BEGIN_DOLLAR_SINGLE_QUOTE_ESCAPE,
  BEGIN_PARAMETER_EXPANSION,
  BEGIN_ARITHMETIC_EXPANSION,
  BEGIN_COMMAND_SUBSTITUTION,
  BEGIN_BACKQUOTE_SUBSTITUTION,
  END_OWNER,
  BEGIN_ISSUE,
  BEGIN_UNOWNED_ISSUE,
  SOURCE,
  INVALID_ENCODING,
  NUL_CHARACTER,
  UNMATCHED_PARENTHESIS,
  UNMATCHED_BRACE,
  UNEXPECTED_THEN,
  UNEXPECTED_ELIF,
  UNEXPECTED_ELSE,
  UNMATCHED_FI,
  UNEXPECTED_DO,
  UNMATCHED_DONE,
  UNMATCHED_ESAC,
  UNEXPECTED_IN,
  UNEXPECTED_SEMICOLON,
  UNEXPECTED_AMPERSAND,
  UNEXPECTED_PIPE,
  UNEXPECTED_AND_IF,
  UNEXPECTED_OR_IF,
  UNEXPECTED_CASE_SEPARATOR,
  UNEXPECTED_CASE_FALLTHROUGH,
  UNEXPECTED_NEGATION,
  COMMAND_GAP,
  SEPARATOR_GAP,
  UNSPECIFIED_NEGATION_SPACING,
  RESERVED_COLON_WORD,
  REDIRECTION_TARGET_GAP,
  INVALID_REDIRECTION_TARGET,
  HERE_DOCUMENT_DELIMITER_GAP,
  INVALID_HERE_DOCUMENT_DELIMITER,
  HERE_DOCUMENT_ENDING_GAP,
  UNSPECIFIED_HERE_DOCUMENT_START,
  UNSPECIFIED_HERE_DOCUMENT_NEWLINE,
  FOR_NAME_GAP,
  INVALID_FOR_NAME,
  FOR_SEPARATOR_GAP,
  INVALID_FOR_HEADER,
  INVALID_FOR_SEPARATOR,
  THEN_GAP,
  FI_GAP,
  DO_GAP,
  INVALID_DO,
  DONE_GAP,
  IN_GAP,
  ESAC_GAP,
  CASE_WORD_GAP,
  INVALID_CASE_WORD,
  INVALID_IN,
  CASE_PATTERN_GAP,
  INVALID_CASE_PATTERN,
  CASE_PATTERN_SEPARATOR_ERROR,
  PARENTHESIS_GAP,
  BRACE_GAP,
  FUNCTION_BODY_GAP,
  INVALID_FUNCTION_BODY,
  FUNCTION_PARAMETER_ERROR,
  SINGLE_QUOTE_GAP,
  DOUBLE_QUOTE_GAP,
  DOLLAR_QUOTE_GAP,
  INCOMPLETE_ESCAPE,
  UNSPECIFIED_DOLLAR_QUOTE_ESCAPE,
  UNSPECIFIED_EMPTY_HEX_ESCAPE,
  UNSPECIFIED_CONTROL_ESCAPE,
  UNSPECIFIED_HEX_ESCAPE,
  UNSPECIFIED_OCTAL_ESCAPE,
  PARAMETER_NAME_GAP,
  INVALID_PARAMETER_NAME,
  PARAMETER_OPERATOR_GAP,
  PARAMETER_OPERATOR_ERROR,
  PARAMETER_LENGTH_SUFFIX_ERROR,
  PARAMETER_WORD_GAP,
  PARAMETER_ENDING_GAP,
  UNSPECIFIED_PARAMETER_QUOTE,
  UNSPECIFIED_EXPANSION_START,
  ARITHMETIC_ENDING_GAP,
  SUBSTITUTION_ENDING_GAP,
  BACKQUOTE_GAP,
  UNDEFINED_BACKQUOTE_QUOTE,
  UNDEFINED_BACKQUOTE_COMMENT,
  UNDEFINED_BACKQUOTE_HERE_DOCUMENT,
  UNDEFINED_BACKQUOTE_SUBSTITUTION,
  RECOVERY_FRAGMENT,
  RECOVERY_RESUME,
  RECOVERY_EOF,
  TOKEN_TYPE_COUNT,
};

enum { INVALID_CHARACTER = 0x110000 };

struct Unit {
  int32_t value;
  size_t start, end;
};
struct Text {
  struct Unit *units;
  size_t count, capacity;
};
struct Change {
  size_t start, end;
  int32_t value;
};
struct Capture {
  struct Change *changes;
  size_t count, capacity;
  bool quoted, failed, newlines_only;
  struct Unit newline;
  size_t uncertain;
  struct Capture *parent;
};
struct Rejected {
  uint8_t *bits;
};
struct Input {
  struct Unit *units;
  size_t count, position, end;
  struct Capture *capture;
  struct Rejected *rejected;
};
struct Recovery {
  size_t position;
  bool complete;
};
struct Issue {
  enum TokenType kind;
  size_t start, end, context_start, owner;
  struct Recovery recovery;
};
struct Event {
  enum TokenType kind;
  size_t end;
};
struct Owner {
  enum TokenType kind;
  size_t start, end, parent;
  size_t first, limit;
  bool retained;
};
// Defer region extension until publication so rejected probes can discard it.
struct Extension {
  size_t position, resume;
};
struct Plan {
  struct Issue *issues;
  size_t issue_count, issue_capacity, decode_issues;
  struct Extension *extensions;
  size_t extension_count, extension_capacity;
  struct Owner *owners;
  size_t owner_count, owner_capacity;
  size_t depth;
  bool failed;
};

enum Lexeme {
  END,
  WORD,
  NEWLINE,
  SEMICOLON,
  AMPERSAND,
  PIPE,
  AND,
  OR,
  LEFT,
  RIGHT,
  DSEMI,
  SEMI_AND,
  REDIRECT,
  HERE,
  HERE_STRIP,
  IO_NUMBER
};
struct Token {
  enum Lexeme kind;
  size_t start, end;
  struct Text text;
  struct Unit name_fault;
  bool plain, name;
  size_t owner;
};
struct Document {
  struct Text delimiter;
  bool quoted, strip;
  size_t uncertain;
};
struct Parser {
  struct Input input;
  struct Plan *plan;
  struct Token token;
  bool ready, opaque, quoted_delimiter;
  size_t owner, consumed, consumed_start;
  struct Document *documents;
  size_t document_count, document_capacity;
  uint32_t stops;
  bool resumed;
  size_t backquote_end;
  int32_t closer;
};
struct Scanner {
  struct Event *events;
  size_t count, capacity, cursor, position;
  bool started;
};

static inline void *
diagnostic_grow(void *data, size_t *capacity, size_t count, size_t width) {
  if (count <= *capacity) return data;
  if (width == 0 || count > SIZE_MAX / width) return NULL;
  size_t maximum = SIZE_MAX / width;
  size_t next = *capacity ? *capacity : (maximum < 16 ? maximum : 16);
  while (next < count) next = next > maximum / 2 ? maximum : next * 2;
  void *grown = ts_realloc(data, next * width);
  if (grown) *capacity = next;
  return grown;
}

enum ArithmeticValidation {
  ARITHMETIC_VALIDATION_INVALID,
  ARITHMETIC_VALIDATION_VALID,
  ARITHMETIC_VALIDATION_RESOURCE_FAILURE,
};

enum ArithmeticSymbol {
  ARITHMETIC_NAME,
  ARITHMETIC_NUMBER,
  ARITHMETIC_LEFT,
  ARITHMETIC_RIGHT,
  ARITHMETIC_UNARY_OPERATOR,
  ARITHMETIC_BINARY_OPERATOR,
  ARITHMETIC_ASSIGNMENT_OPERATOR,
  ARITHMETIC_QUESTION,
  ARITHMETIC_COLON,
  ARITHMETIC_TOKEN_COUNT,
  ARITHMETIC_START = ARITHMETIC_TOKEN_COUNT,
  ARITHMETIC_ASSIGNMENT,
  ARITHMETIC_CONDITIONAL,
  ARITHMETIC_BINARY,
  ARITHMETIC_UNARY,
  ARITHMETIC_PRIMARY,
  ARITHMETIC_LVALUE,
  ARITHMETIC_SYMBOL_COUNT,
};

enum ArithmeticLexState {
  LEX_START,
  LEX_NAME,
  LEX_ZERO,
  LEX_OCTAL,
  LEX_DECIMAL,
  LEX_HEX_PREFIX,
  LEX_HEX,
  LEX_PLUS,
  LEX_MINUS,
  LEX_BANG,
  LEX_TILDE,
  LEX_STAR,
  LEX_SLASH,
  LEX_PERCENT,
  LEX_EQUALS,
  LEX_LESS,
  LEX_GREATER,
  LEX_AMPERSAND,
  LEX_PIPE,
  LEX_CARET,
  LEX_BINARY,
  LEX_LEFT_SHIFT,
  LEX_RIGHT_SHIFT,
  LEX_ASSIGNMENT,
  LEX_LEFT,
  LEX_RIGHT,
  LEX_QUESTION,
  LEX_COLON,
};

enum {
  LEX_STOP = -1,
  LEX_INVALID = -2,
};

struct ArithmeticVertex {
  size_t position;
  size_t edge;
};

struct ArithmeticEdge {
  size_t target;
  size_t next;
  uint16_t tokens;
};

struct ArithmeticLexItem {
  size_t position;
  uint8_t state;
};

struct ArithmeticGraph {
  const int32_t *source;
  size_t length;
  size_t required_end;
  struct ArithmeticVertex *vertices;
  size_t vertex_count;
  size_t vertex_capacity;
  struct ArithmeticEdge *edges;
  size_t edge_count;
  size_t edge_capacity;
  size_t *vertex_ids;
  uint64_t *visited;
  uint16_t *emitted;
  size_t *endpoints;
  size_t endpoint_count;
  size_t endpoint_capacity;
  struct ArithmeticLexItem *pending;
  size_t pending_count;
  size_t pending_capacity;
  bool valid;
};

static bool arithmetic_whitespace(int32_t c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f';
}

static bool is_decimal_digit(int32_t c) {
  return c >= '0' && c <= '9';
}

static bool is_hexadecimal_digit(int32_t c) {
  return is_decimal_digit(c) ||
    (c >= 'A' && c <= 'F') ||
    (c >= 'a' && c <= 'f');
}

static bool is_name_start_character(int32_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static bool is_name_character(int32_t c) {
  return is_name_start_character(c) || is_decimal_digit(c);
}

static int arithmetic_lex(uint8_t state, int32_t c) {
  bool name = is_name_start_character(c);
  bool digit = is_decimal_digit(c);
  switch (state) {
    case LEX_START:
      if (arithmetic_whitespace(c)) return LEX_START;
      if (name) return LEX_NAME;
      if (digit) return c == '0' ? LEX_ZERO : LEX_DECIMAL;
      switch (c) {
        case '+':
          return LEX_PLUS;
        case '-':
          return LEX_MINUS;
        case '!':
          return LEX_BANG;
        case '~':
          return LEX_TILDE;
        case '*':
          return LEX_STAR;
        case '/':
          return LEX_SLASH;
        case '%':
          return LEX_PERCENT;
        case '=':
          return LEX_EQUALS;
        case '<':
          return LEX_LESS;
        case '>':
          return LEX_GREATER;
        case '&':
          return LEX_AMPERSAND;
        case '|':
          return LEX_PIPE;
        case '^':
          return LEX_CARET;
        case '(':
          return LEX_LEFT;
        case ')':
          return LEX_RIGHT;
        case '?':
          return LEX_QUESTION;
        case ':':
          return LEX_COLON;
        default:
          return LEX_INVALID;
      }
    case LEX_NAME:
      return name || digit ? LEX_NAME : LEX_STOP;
    case LEX_ZERO:
      if (c == 'x' || c == 'X') return LEX_HEX_PREFIX;
      if (digit && c <= '7') return LEX_OCTAL;
      return name || digit ? LEX_INVALID : LEX_STOP;
    case LEX_OCTAL:
      if (digit && c <= '7') return LEX_OCTAL;
      return name || digit ? LEX_INVALID : LEX_STOP;
    case LEX_DECIMAL:
      if (digit) return LEX_DECIMAL;
      return name ? LEX_INVALID : LEX_STOP;
    case LEX_HEX_PREFIX:
      return is_hexadecimal_digit(c) ? LEX_HEX : LEX_INVALID;
    case LEX_HEX:
      if (is_hexadecimal_digit(c)) return LEX_HEX;
      return name || digit ? LEX_INVALID : LEX_STOP;
    case LEX_PLUS:
    case LEX_MINUS:
      if (c == (state == LEX_PLUS ? '+' : '-')) return LEX_INVALID;
      return c == '=' ? LEX_ASSIGNMENT : LEX_STOP;
    case LEX_BANG:
    case LEX_EQUALS:
      return c == '=' ? LEX_BINARY : LEX_STOP;
    case LEX_STAR:
    case LEX_SLASH:
    case LEX_PERCENT:
    case LEX_CARET:
    case LEX_LEFT_SHIFT:
    case LEX_RIGHT_SHIFT:
      return c == '=' ? LEX_ASSIGNMENT : LEX_STOP;
    case LEX_LESS:
    case LEX_GREATER:
      if (c == '=') return LEX_BINARY;
      if (c == (state == LEX_LESS ? '<' : '>'))
        return state == LEX_LESS ? LEX_LEFT_SHIFT : LEX_RIGHT_SHIFT;
      return LEX_STOP;
    case LEX_AMPERSAND:
    case LEX_PIPE:
      if (c == '=') return LEX_ASSIGNMENT;
      return c == (state == LEX_AMPERSAND ? '&' : '|') ? LEX_BINARY : LEX_STOP;
    default:
      return LEX_STOP;
  }
}

static const uint16_t arithmetic_tokens[] = {
  [LEX_NAME] = 1u << ARITHMETIC_NAME,
  [LEX_ZERO] = 1u << ARITHMETIC_NUMBER,
  [LEX_OCTAL] = 1u << ARITHMETIC_NUMBER,
  [LEX_DECIMAL] = 1u << ARITHMETIC_NUMBER,
  [LEX_HEX] = 1u << ARITHMETIC_NUMBER,
  [LEX_PLUS] =
    (1u << ARITHMETIC_UNARY_OPERATOR) | (1u << ARITHMETIC_BINARY_OPERATOR),
  [LEX_MINUS] =
    (1u << ARITHMETIC_UNARY_OPERATOR) | (1u << ARITHMETIC_BINARY_OPERATOR),
  [LEX_BANG] = 1u << ARITHMETIC_UNARY_OPERATOR,
  [LEX_TILDE] = 1u << ARITHMETIC_UNARY_OPERATOR,
  [LEX_EQUALS] = 1u << ARITHMETIC_ASSIGNMENT_OPERATOR,
  [LEX_ASSIGNMENT] = 1u << ARITHMETIC_ASSIGNMENT_OPERATOR,
  [LEX_STAR] = 1u << ARITHMETIC_BINARY_OPERATOR,
  [LEX_SLASH] = 1u << ARITHMETIC_BINARY_OPERATOR,
  [LEX_PERCENT] = 1u << ARITHMETIC_BINARY_OPERATOR,
  [LEX_LESS] = 1u << ARITHMETIC_BINARY_OPERATOR,
  [LEX_GREATER] = 1u << ARITHMETIC_BINARY_OPERATOR,
  [LEX_AMPERSAND] = 1u << ARITHMETIC_BINARY_OPERATOR,
  [LEX_PIPE] = 1u << ARITHMETIC_BINARY_OPERATOR,
  [LEX_CARET] = 1u << ARITHMETIC_BINARY_OPERATOR,
  [LEX_BINARY] = 1u << ARITHMETIC_BINARY_OPERATOR,
  [LEX_LEFT_SHIFT] = 1u << ARITHMETIC_BINARY_OPERATOR,
  [LEX_RIGHT_SHIFT] = 1u << ARITHMETIC_BINARY_OPERATOR,
  [LEX_LEFT] = 1u << ARITHMETIC_LEFT,
  [LEX_RIGHT] = 1u << ARITHMETIC_RIGHT,
  [LEX_QUESTION] = 1u << ARITHMETIC_QUESTION,
  [LEX_COLON] = 1u << ARITHMETIC_COLON,
};

static size_t
arithmetic_boundary(const struct ArithmeticGraph *graph, size_t position) {
  while (
    position < graph->length && arithmetic_whitespace(graph->source[position])
  )
    position++;
  return position;
}

static size_t
arithmetic_after_hole(const struct ArithmeticGraph *graph, size_t position) {
  do {
    position++;
  } while (position < graph->length && graph->source[position] == -1);
  return position;
}

static bool
arithmetic_vertex(struct ArithmeticGraph *graph, size_t position, size_t *id) {
  position = arithmetic_boundary(graph, position);
  if (graph->vertex_ids[position] == SIZE_MAX) {
    struct ArithmeticVertex *grown = diagnostic_grow(
      graph->vertices,
      &graph->vertex_capacity,
      graph->vertex_count + 1,
      sizeof(*grown)
    );
    if (!grown) return false;
    graph->vertices = grown;
    graph->vertex_ids[position] = graph->vertex_count;
    graph->vertices[graph->vertex_count++] =
      (struct ArithmeticVertex){position, SIZE_MAX};
  }
  *id = graph->vertex_ids[position];
  return true;
}

static bool arithmetic_edge(
  struct ArithmeticGraph *graph,
  size_t origin,
  size_t position,
  uint16_t tokens
) {
  size_t target;
  if (!arithmetic_vertex(graph, position, &target)) return false;
  struct ArithmeticEdge *grown = diagnostic_grow(
    graph->edges,
    &graph->edge_capacity,
    graph->edge_count + 1,
    sizeof(*grown)
  );
  if (!grown) return false;
  graph->edges = grown;
  graph->edges[graph->edge_count] = (struct ArithmeticEdge){
    target,
    graph->vertices[origin].edge,
    tokens,
  };
  graph->vertices[origin].edge = graph->edge_count++;
  return true;
}

static bool arithmetic_enqueue(
  struct ArithmeticGraph *graph,
  size_t position,
  uint8_t state
) {
  if (state == LEX_START) position = arithmetic_boundary(graph, position);
  uint64_t bit = UINT64_C(1) << state;
  if (graph->visited[position] & bit) return true;
  struct ArithmeticLexItem *grown = diagnostic_grow(
    graph->pending,
    &graph->pending_capacity,
    graph->pending_count + 1,
    sizeof(*grown)
  );
  if (!grown) return false;
  graph->pending = grown;
  graph->visited[position] |= bit;
  graph->pending[graph->pending_count++] =
    (struct ArithmeticLexItem){position, state};
  return true;
}

static bool arithmetic_emit(
  struct ArithmeticGraph *graph,
  size_t origin,
  size_t position,
  uint8_t state
) {
  uint16_t tokens = arithmetic_tokens[state];
  if (!tokens) return true;
  position = arithmetic_boundary(graph, position);
  if (
    !origin &&
    position >= graph->required_end &&
    (tokens & ((1u << ARITHMETIC_NAME) | (1u << ARITHMETIC_NUMBER)))
  ) {
    graph->valid = true;
    return true;
  }
  if (!graph->emitted[position]) {
    size_t *grown = diagnostic_grow(
      graph->endpoints,
      &graph->endpoint_capacity,
      graph->endpoint_count + 1,
      sizeof(*grown)
    );
    if (!grown) return false;
    graph->endpoints = grown;
    graph->endpoints[graph->endpoint_count++] = position;
  }
  graph->emitted[position] |= tokens;
  return true;
}

static bool arithmetic_step(
  struct ArithmeticGraph *graph,
  size_t origin,
  struct ArithmeticLexItem item
) {
  if (item.position == graph->length)
    return arithmetic_emit(graph, origin, item.position, item.state);
  if (graph->source[item.position] == -1) {
    if (!arithmetic_emit(graph, origin, item.position, item.state))
      return false;
    if (
      item.state != LEX_START &&
      !arithmetic_enqueue(
        graph,
        arithmetic_after_hole(graph, item.position),
        item.state
      )
    )
      return false;
    for (int32_t c = 1; c < 128; c++) {
      int next = arithmetic_lex(item.state, c);
      if (next >= 0 && !arithmetic_enqueue(graph, item.position, (uint8_t)next))
        return false;
    }
    return true;
  }
  int next = arithmetic_lex(item.state, graph->source[item.position]);
  if (next == LEX_STOP)
    return arithmetic_emit(graph, origin, item.position, item.state);
  return next < 0 ||
    arithmetic_enqueue(graph, item.position + 1, (uint8_t)next);
}

static bool arithmetic_build_graph(struct ArithmeticGraph *graph) {
  size_t initial;
  if (!arithmetic_vertex(graph, 0, &initial)) return false;
  for (size_t origin = 0; origin < graph->vertex_count; origin++) {
    size_t position = graph->vertices[origin].position;
    if (
      position < graph->length &&
      graph->source[position] == -1 &&
      !arithmetic_edge(graph, origin, arithmetic_after_hole(graph, position), 0)
    ) {
      return false;
    }
    graph->pending_count = 0;
    graph->endpoint_count = 0;
    if (!arithmetic_enqueue(graph, position, LEX_START)) return false;
    for (size_t i = 0; i < graph->pending_count; i++) {
      if (!arithmetic_step(graph, origin, graph->pending[i])) return false;
      if (graph->valid) return true;
    }
    // Clearing only the visited positions keeps each origin's lexing linear.
    for (size_t i = 0; i < graph->pending_count; i++)
      graph->visited[graph->pending[i].position] = 0;
    for (size_t i = 0; i < graph->endpoint_count; i++) {
      size_t endpoint = graph->endpoints[i];
      uint16_t tokens = graph->emitted[endpoint];
      graph->emitted[endpoint] = 0;
      if (!arithmetic_edge(graph, origin, endpoint, tokens)) return false;
    }
  }
  return true;
}

struct ArithmeticRule {
  uint8_t left;
  uint8_t length;
  uint8_t right[5];
};

static const struct ArithmeticRule arithmetic_rules[] = {
  {ARITHMETIC_START, 1, {ARITHMETIC_ASSIGNMENT}},
  {ARITHMETIC_ASSIGNMENT, 1, {ARITHMETIC_CONDITIONAL}},
  {ARITHMETIC_ASSIGNMENT,
    3,
    {ARITHMETIC_LVALUE, ARITHMETIC_ASSIGNMENT_OPERATOR, ARITHMETIC_ASSIGNMENT}},
  {ARITHMETIC_CONDITIONAL, 1, {ARITHMETIC_BINARY}},
  {ARITHMETIC_CONDITIONAL,
    5,
    {ARITHMETIC_BINARY,
      ARITHMETIC_QUESTION,
      ARITHMETIC_ASSIGNMENT,
      ARITHMETIC_COLON,
      ARITHMETIC_CONDITIONAL}},
  {ARITHMETIC_BINARY, 1, {ARITHMETIC_UNARY}},
  {ARITHMETIC_BINARY,
    3,
    {ARITHMETIC_UNARY, ARITHMETIC_BINARY_OPERATOR, ARITHMETIC_BINARY}},
  {ARITHMETIC_UNARY, 1, {ARITHMETIC_PRIMARY}},
  {ARITHMETIC_UNARY, 2, {ARITHMETIC_UNARY_OPERATOR, ARITHMETIC_UNARY}},
  {ARITHMETIC_PRIMARY, 1, {ARITHMETIC_NUMBER}},
  {ARITHMETIC_PRIMARY, 1, {ARITHMETIC_NAME}},
  {ARITHMETIC_PRIMARY,
    3,
    {ARITHMETIC_LEFT, ARITHMETIC_ASSIGNMENT, ARITHMETIC_RIGHT}},
  {ARITHMETIC_LVALUE, 1, {ARITHMETIC_NAME}},
  {ARITHMETIC_LVALUE,
    3,
    {ARITHMETIC_LEFT, ARITHMETIC_LVALUE, ARITHMETIC_RIGHT}},
};

enum {
  ARITHMETIC_RULE_COUNT = sizeof(arithmetic_rules) / sizeof(*arithmetic_rules)
};

// Only assignment needs lvalue tracking; operator results are non-lvalues.
enum ArithmeticOperator {
  ARITHMETIC_OP_LEFT,
  ARITHMETIC_OP_QUESTION,
  ARITHMETIC_OP_ASSIGN,
  ARITHMETIC_OP_CONDITIONAL,
  ARITHMETIC_OP_BINARY,
  ARITHMETIC_OP_UNARY,
};

static unsigned arithmetic_precedence(enum ArithmeticOperator operation) {
  switch (operation) {
    case ARITHMETIC_OP_ASSIGN:
      return 1;
    case ARITHMETIC_OP_QUESTION:
    case ARITHMETIC_OP_CONDITIONAL:
      return 2;
    case ARITHMETIC_OP_BINARY:
      return 3;
    case ARITHMETIC_OP_UNARY:
      return 4;
    default:
      return 0;
  }
}

static bool arithmetic_reduce(
  enum ArithmeticOperator operation,
  uint8_t *values,
  size_t *count
) {
  size_t operands = operation == ARITHMETIC_OP_UNARY ? 1
    : operation == ARITHMETIC_OP_CONDITIONAL         ? 3
                                                     : 2;
  if (
    operation == ARITHMETIC_OP_LEFT ||
    operation == ARITHMETIC_OP_QUESTION ||
    *count < operands
  )
    return false;
  *count -= operands;
  if (operation == ARITHMETIC_OP_ASSIGN && !values[*count]) return false;
  values[(*count)++] = false;
  return true;
}

static enum ArithmeticValidation
arithmetic_literal(const int32_t *source, size_t length) {
  uint8_t *values = ts_malloc((length + 1) * sizeof(*values));
  uint8_t *operations = ts_malloc((length + 1) * sizeof(*operations));
  enum ArithmeticValidation result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
  if (!values || !operations) goto finish;
  result = ARITHMETIC_VALIDATION_INVALID;
  size_t value_count = 0, operation_count = 0, position = 0;
  bool operand = true;
  while (position < length) {
    if (arithmetic_whitespace(source[position])) {
      position++;
      continue;
    }
    int state = LEX_START;
    while (position < length) {
      int next = arithmetic_lex((uint8_t)state, source[position]);
      if (next == LEX_INVALID) goto finish;
      if (next == LEX_STOP) break;
      state = next;
      position++;
    }
    uint16_t token = arithmetic_tokens[state];
    if (!token) goto finish;
    if (operand) {
      if (token & (1u << ARITHMETIC_UNARY_OPERATOR))
        operations[operation_count++] = ARITHMETIC_OP_UNARY;
      else if (token & (1u << ARITHMETIC_LEFT))
        operations[operation_count++] = ARITHMETIC_OP_LEFT;
      else if (token & ((1u << ARITHMETIC_NAME) | (1u << ARITHMETIC_NUMBER))) {
        values[value_count++] = (token & (1u << ARITHMETIC_NAME)) != 0;
        operand = false;
      } else goto finish;
      continue;
    }
    if (token & ((1u << ARITHMETIC_RIGHT) | (1u << ARITHMETIC_COLON))) {
      enum ArithmeticOperator stop = token & (1u << ARITHMETIC_RIGHT)
        ? ARITHMETIC_OP_LEFT
        : ARITHMETIC_OP_QUESTION;
      while (operation_count && operations[operation_count - 1] != stop) {
        if (!arithmetic_reduce(
              operations[--operation_count],
              values,
              &value_count
            ))
          goto finish;
      }
      if (!operation_count) goto finish;
      if (stop == ARITHMETIC_OP_LEFT) operation_count--;
      else {
        operations[operation_count - 1] = ARITHMETIC_OP_CONDITIONAL;
        operand = true;
      }
      continue;
    }
    enum ArithmeticOperator operation;
    if (token & (1u << ARITHMETIC_ASSIGNMENT_OPERATOR))
      operation = ARITHMETIC_OP_ASSIGN;
    else if (token & (1u << ARITHMETIC_QUESTION))
      operation = ARITHMETIC_OP_QUESTION;
    else if (token & (1u << ARITHMETIC_BINARY_OPERATOR))
      operation = ARITHMETIC_OP_BINARY;
    else goto finish;
    while (operation_count) {
      enum ArithmeticOperator previous = operations[operation_count - 1];
      if (previous == ARITHMETIC_OP_LEFT || previous == ARITHMETIC_OP_QUESTION)
        break;
      if (
        arithmetic_precedence(previous) < arithmetic_precedence(operation) ||
        (arithmetic_precedence(previous) == arithmetic_precedence(operation) &&
          operation != ARITHMETIC_OP_BINARY)
      )
        break;
      operation_count--;
      if (!arithmetic_reduce(previous, values, &value_count)) goto finish;
    }
    operations[operation_count++] = operation;
    operand = true;
  }
  if (operand) goto finish;
  while (operation_count)
    if (!arithmetic_reduce(operations[--operation_count], values, &value_count))
      goto finish;
  if (value_count == 1) result = ARITHMETIC_VALIDATION_VALID;
finish:
  ts_free(values);
  ts_free(operations);
  return result;
}

// Holes leave the outer stack unknown; only later punctuation constrains it.
// Check each outermost hole-free group once.
static enum ArithmeticValidation
arithmetic_check_structure(const int32_t *source, size_t length) {
  struct ArithmeticGroup {
    size_t start, end;
  };
  struct ArithmeticGroup *groups = NULL;
  size_t *stack = NULL;
  size_t group_count = 0, group_capacity = 0, depth = 0, capacity = 0;
  bool unknown = false;
  enum ArithmeticValidation result = ARITHMETIC_VALIDATION_VALID;
  for (size_t i = 0; i < length; i++) {
    int32_t c = source[i];
    if (c == -1) {
      depth = 0;
      unknown = true;
    } else if (c == '(' || c == '?') {
      size_t *grown =
        diagnostic_grow(stack, &capacity, depth + 1, sizeof(*grown));
      if (!grown) {
        result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
        goto finish;
      }
      stack = grown;
      stack[depth++] = i;
    } else if (c == ')' || c == ':') {
      if (!depth) {
        if (unknown) continue;
        result = ARITHMETIC_VALIDATION_INVALID;
        goto finish;
      }
      size_t start = stack[--depth];
      if (source[start] != (c == ')' ? '(' : '?')) {
        result = ARITHMETIC_VALIDATION_INVALID;
        goto finish;
      }
      if (c != ')') continue;
      while (group_count && groups[group_count - 1].start > start)
        group_count--;
      struct ArithmeticGroup *grown = diagnostic_grow(
        groups,
        &group_capacity,
        group_count + 1,
        sizeof(*grown)
      );
      if (!grown) {
        result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
        goto finish;
      }
      groups = grown;
      groups[group_count++] = (struct ArithmeticGroup){start, i + 1};
    }
  }
  if (depth) {
    result = ARITHMETIC_VALIDATION_INVALID;
    goto finish;
  }
  for (size_t i = 0; i < group_count; i++) {
    result = arithmetic_literal(
      source + groups[i].start,
      groups[i].end - groups[i].start
    );
    if (result != ARITHMETIC_VALIDATION_VALID) break;
  }
finish:
  ts_free(groups);
  ts_free(stack);
  return result;
}

static uint8_t
arithmetic_symbol(const struct ArithmeticRule *rule, size_t dot) {
  return rule->right[rule->length - dot - 1];
}

struct ArithmeticProjection {
  uint8_t *visited;
  size_t *pending;
  size_t count;
  size_t capacity;
};

static bool arithmetic_project_enqueue(
  struct ArithmeticProjection *projection,
  size_t state
) {
  if (projection->visited[state]) return true;
  size_t *grown = diagnostic_grow(
    projection->pending,
    &projection->capacity,
    projection->count + 1,
    sizeof(*grown)
  );
  if (!grown) return false;
  projection->pending = grown;
  projection->visited[state] = 1;
  projection->pending[projection->count++] = state;
  return true;
}

static enum ArithmeticValidation
arithmetic_project_graph(const struct ArithmeticGraph *graph) {
  enum {
    maximum_states = ARITHMETIC_RULE_COUNT *
      (sizeof(arithmetic_rules[0].right) + 1),
  };
  size_t starts[ARITHMETIC_RULE_COUNT];
  uint8_t rules[maximum_states];
  uint8_t dots[maximum_states];
  size_t state_count = 0;
  for (size_t rule = 0; rule < ARITHMETIC_RULE_COUNT; rule++) {
    starts[rule] = state_count;
    for (uint8_t dot = 0; dot <= arithmetic_rules[rule].length; dot++) {
      rules[state_count] = (uint8_t)rule;
      dots[state_count++] = dot;
    }
  }
  struct ArithmeticProjection projection = {0};
  enum ArithmeticValidation result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
  if (graph->vertex_count > SIZE_MAX / state_count) return result;
  projection.visited = ts_calloc(graph->vertex_count * state_count, 1);
  if (!projection.visited || !arithmetic_project_enqueue(&projection, 0))
    goto finish;
  // Ignoring the return stack gives a regular superset of the full grammar.
  for (size_t i = 0; i < projection.count; i++) {
    size_t vertex = projection.pending[i] / state_count;
    size_t state = projection.pending[i] % state_count;
    const struct ArithmeticRule *rule = &arithmetic_rules[rules[state]];
    uint8_t dot = dots[state];
    if (
      state == arithmetic_rules[0].length &&
      graph->vertices[vertex].position >= graph->required_end
    ) {
      result = ARITHMETIC_VALIDATION_VALID;
      goto finish;
    }
    bool complete = dot == rule->length;
    uint8_t symbol =
      complete ? ARITHMETIC_SYMBOL_COUNT : arithmetic_symbol(rule, dot);
    for (
      size_t edge = graph->vertices[vertex].edge; edge != SIZE_MAX;
      edge = graph->edges[edge].next
    ) {
      struct ArithmeticEdge transition = graph->edges[edge];
      if (!transition.tokens) {
        if (!arithmetic_project_enqueue(
              &projection,
              transition.target * state_count + state
            )) {
          goto finish;
        }
      } else if (
        symbol < ARITHMETIC_TOKEN_COUNT &&
        (transition.tokens & (1u << symbol)) &&
        !arithmetic_project_enqueue(
          &projection,
          transition.target * state_count + state + 1
        )
      ) {
        goto finish;
      }
    }
    for (size_t candidate = 0; candidate < ARITHMETIC_RULE_COUNT; candidate++) {
      const struct ArithmeticRule *target = &arithmetic_rules[candidate];
      if (complete) {
        for (size_t position = 0; position < target->length; position++) {
          if (
            arithmetic_symbol(target, position) == rule->left &&
            !arithmetic_project_enqueue(
              &projection,
              vertex * state_count + starts[candidate] + position + 1
            )
          ) {
            goto finish;
          }
        }
      } else if (
        symbol == target->left &&
        !arithmetic_project_enqueue(
          &projection,
          vertex * state_count + starts[candidate]
        )
      ) {
        goto finish;
      }
    }
  }
  result = ARITHMETIC_VALIDATION_INVALID;
finish:
  ts_free(projection.visited);
  ts_free(projection.pending);
  return result;
}

struct ArithmeticItem {
  size_t vertex;
  size_t origin;
  size_t next;
  size_t pending_next;
  uint8_t rule;
  uint8_t dot;
};

struct ArithmeticCompletion {
  size_t vertex;
  size_t origin;
  size_t next;
  uint8_t symbol;
};

struct ArithmeticQueue {
  size_t first, last;
};

struct ArithmeticChart {
  const struct ArithmeticGraph *graph;
  struct ArithmeticItem *items;
  size_t item_count;
  size_t item_capacity;
  size_t *heads;
  size_t *hash;
  size_t hash_capacity;
  struct ArithmeticCompletion *completions;
  size_t completion_count;
  size_t completion_capacity;
  size_t *completion_hash;
  size_t completion_hash_capacity;
  size_t *completed;
  struct ArithmeticQueue *pending;
  uint64_t *ready;
  size_t ready_offsets[(sizeof(size_t) * CHAR_BIT + 5) / 6];
  size_t ready_levels;
};

static bool arithmetic_queue_init(struct ArithmeticChart *chart) {
  size_t vertices = chart->graph->vertex_count;
  if (vertices > SIZE_MAX / sizeof(*chart->pending)) return false;
  chart->pending = ts_malloc(vertices * sizeof(*chart->pending));
  if (!chart->pending) return false;
  for (size_t i = 0; i < vertices; i++)
    chart->pending[i] = (struct ArithmeticQueue){SIZE_MAX, SIZE_MAX};
  size_t count = chart->graph->length / 64 + 1, total = 0;
  while (true) {
    chart->ready_offsets[chart->ready_levels++] = total;
    if (count > SIZE_MAX - total) return false;
    total += count;
    if (count == 1) break;
    count = count / 64 + (count % 64 != 0);
  }
  if (total > SIZE_MAX / sizeof(*chart->ready)) return false;
  chart->ready = ts_calloc(total, sizeof(*chart->ready));
  return chart->ready != NULL;
}

static void
arithmetic_ready(struct ArithmeticChart *chart, size_t vertex, bool pending) {
  size_t position = chart->graph->vertices[vertex].position;
  for (size_t level = 0; level < chart->ready_levels; level++) {
    uint64_t *word = &chart->ready[chart->ready_offsets[level] + position / 64];
    uint64_t before = *word, bit = UINT64_C(1) << (position % 64);
    *word = pending ? *word | bit : *word & ~bit;
    if ((before != 0) == (*word != 0)) break;
    position /= 64;
  }
}

static size_t arithmetic_next(struct ArithmeticChart *chart) {
  size_t position = 0;
  for (size_t level = chart->ready_levels; level > 0; level--) {
    uint64_t word = chart->ready[chart->ready_offsets[level - 1] + position];
    size_t bit = 0;
    for (unsigned shift = 32; shift; shift /= 2) {
      if (word >> shift) {
        word >>= shift;
        bit += shift;
      }
    }
    position = position * 64 + bit;
  }
  size_t vertex = chart->graph->vertex_ids[position];
  struct ArithmeticQueue *queue = &chart->pending[vertex];
  size_t result = queue->first;
  queue->first = chart->items[result].pending_next;
  if (queue->first == SIZE_MAX) arithmetic_ready(chart, vertex, false);
  return result;
}

static size_t
arithmetic_hash_key(size_t vertex, size_t origin, uint8_t rule, uint8_t dot) {
  uint64_t hash = (uint64_t)vertex * UINT64_C(0x9e3779b97f4a7c15);
  hash ^= (uint64_t)origin * UINT64_C(0xbf58476d1ce4e5b9);
  hash ^= (((uint64_t)rule << 8) | dot) * UINT64_C(0x94d049bb133111eb);
  hash = (hash ^ (hash >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  hash = (hash ^ (hash >> 27)) * UINT64_C(0x94d049bb133111eb);
  return (size_t)(hash ^ (hash >> 31));
}

static size_t
arithmetic_item_key(const struct ArithmeticChart *chart, size_t i) {
  const struct ArithmeticItem *item = &chart->items[i];
  return arithmetic_hash_key(item->vertex, item->origin, item->rule, item->dot);
}

static size_t
arithmetic_completion_key(const struct ArithmeticChart *chart, size_t i) {
  const struct ArithmeticCompletion *completion = &chart->completions[i];
  return arithmetic_hash_key(
    completion->vertex,
    completion->origin,
    completion->symbol,
    0
  );
}

static bool arithmetic_rehash(
  size_t **hash,
  size_t *capacity,
  size_t count,
  size_t (*key)(const struct ArithmeticChart *, size_t),
  const struct ArithmeticChart *chart
) {
  if (count < *capacity / 2) return true;
  size_t next = !*capacity ? 256 : *capacity * 2;
  if (next < *capacity || next > SIZE_MAX / sizeof(size_t)) return false;
  size_t *grown = ts_calloc(next, sizeof(*grown));
  if (!grown) return false;
  for (size_t i = 0; i < count; i++) {
    size_t slot = key(chart, i) & (next - 1);
    while (grown[slot]) slot = (slot + 1) & (next - 1);
    grown[slot] = i + 1;
  }
  ts_free(*hash);
  *hash = grown;
  *capacity = next;
  return true;
}

static bool arithmetic_add_item(
  struct ArithmeticChart *chart,
  size_t vertex,
  size_t origin,
  uint8_t rule,
  uint8_t dot
) {
  if (!arithmetic_rehash(
        &chart->hash,
        &chart->hash_capacity,
        chart->item_count,
        arithmetic_item_key,
        chart
      )) {
    return false;
  }
  const struct ArithmeticRule *production = &arithmetic_rules[rule];
  uint8_t symbol = dot < production->length ? arithmetic_symbol(production, dot)
                                            : ARITHMETIC_SYMBOL_COUNT;
  size_t head = vertex * ARITHMETIC_SYMBOL_COUNT + symbol;
  bool waiting =
    symbol >= ARITHMETIC_TOKEN_COUNT && symbol < ARITHMETIC_SYMBOL_COUNT;
  struct ArithmeticItem item = {
    vertex,
    origin,
    waiting ? chart->heads[head] : SIZE_MAX,
    SIZE_MAX,
    rule,
    dot
  };
  size_t slot =
    arithmetic_hash_key(vertex, origin, rule, dot) & (chart->hash_capacity - 1);
  while (chart->hash[slot]) {
    struct ArithmeticItem existing = chart->items[chart->hash[slot] - 1];
    if (
      existing.vertex == vertex &&
      existing.origin == origin &&
      existing.rule == rule &&
      existing.dot == dot
    ) {
      return true;
    }
    slot = (slot + 1) & (chart->hash_capacity - 1);
  }
  struct ArithmeticItem *items = diagnostic_grow(
    chart->items,
    &chart->item_capacity,
    chart->item_count + 1,
    sizeof(*items)
  );
  if (!items) return false;
  chart->items = items;
  size_t index = chart->item_count++;
  chart->items[index] = item;
  if (waiting) chart->heads[head] = index;
  chart->hash[slot] = index + 1;
  struct ArithmeticQueue *queue = &chart->pending[vertex];
  if (queue->first == SIZE_MAX) {
    queue->first = index;
    arithmetic_ready(chart, vertex, true);
  } else {
    chart->items[queue->last].pending_next = index;
  }
  queue->last = index;
  return true;
}

static bool
arithmetic_complete(struct ArithmeticChart *chart, struct ArithmeticItem item) {
  uint8_t symbol = arithmetic_rules[item.rule].left;
  size_t head = item.origin * ARITHMETIC_SYMBOL_COUNT + symbol;
  if (!arithmetic_rehash(
        &chart->completion_hash,
        &chart->completion_hash_capacity,
        chart->completion_count,
        arithmetic_completion_key,
        chart
      )) {
    return false;
  }
  size_t slot = arithmetic_hash_key(item.vertex, item.origin, symbol, 0) &
    (chart->completion_hash_capacity - 1);
  while (chart->completion_hash[slot]) {
    struct ArithmeticCompletion completion =
      chart->completions[chart->completion_hash[slot] - 1];
    if (
      completion.vertex == item.vertex &&
      completion.origin == item.origin &&
      completion.symbol == symbol
    ) {
      return true;
    }
    slot = (slot + 1) & (chart->completion_hash_capacity - 1);
  }
  struct ArithmeticCompletion *grown = diagnostic_grow(
    chart->completions,
    &chart->completion_capacity,
    chart->completion_count + 1,
    sizeof(*grown)
  );
  if (!grown) return false;
  chart->completions = grown;
  chart->completions[chart->completion_count] = (struct ArithmeticCompletion){
    item.vertex,
    item.origin,
    chart->completed[head],
    symbol
  };
  chart->completed[head] = chart->completion_count;
  chart->completion_hash[slot] = ++chart->completion_count;
  for (size_t i = chart->heads[head]; i != SIZE_MAX;) {
    struct ArithmeticItem parent = chart->items[i];
    i = parent.next;
    if (!arithmetic_add_item(
          chart,
          item.vertex,
          parent.origin,
          parent.rule,
          (uint8_t)(parent.dot + 1)
        )) {
      return false;
    }
  }
  return true;
}

static enum ArithmeticValidation
arithmetic_parse_graph(const struct ArithmeticGraph *graph) {
  struct ArithmeticChart chart = {.graph = graph};
  enum ArithmeticValidation result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
  if (graph->vertex_count > SIZE_MAX / sizeof(size_t) / ARITHMETIC_SYMBOL_COUNT)
    return result;
  chart.heads = ts_malloc(
    graph->vertex_count * ARITHMETIC_SYMBOL_COUNT * sizeof(*chart.heads)
  );
  chart.completed = ts_malloc(
    graph->vertex_count * ARITHMETIC_SYMBOL_COUNT * sizeof(*chart.completed)
  );
  if (!chart.heads || !chart.completed || !arithmetic_queue_init(&chart))
    goto finish;
  for (size_t i = 0; i < graph->vertex_count * ARITHMETIC_SYMBOL_COUNT; i++) {
    chart.heads[i] = SIZE_MAX;
    chart.completed[i] = SIZE_MAX;
  }
  if (!arithmetic_add_item(&chart, 0, 0, 0, 0)) goto finish;
  while (chart.ready[chart.ready_offsets[chart.ready_levels - 1]]) {
    struct ArithmeticItem item = chart.items[arithmetic_next(&chart)];
    const struct ArithmeticRule *rule = &arithmetic_rules[item.rule];
    bool complete = item.dot == rule->length;
    if (
      complete &&
      rule->left == ARITHMETIC_START &&
      !item.origin &&
      graph->vertices[item.vertex].position >= graph->required_end
    ) {
      result = ARITHMETIC_VALIDATION_VALID;
      goto finish;
    }
    uint8_t symbol =
      complete ? ARITHMETIC_SYMBOL_COUNT : arithmetic_symbol(rule, item.dot);
    for (
      size_t edge = graph->vertices[item.vertex].edge; edge != SIZE_MAX;
      edge = graph->edges[edge].next
    ) {
      struct ArithmeticEdge transition = graph->edges[edge];
      if (!transition.tokens) {
        if (!arithmetic_add_item(
              &chart,
              transition.target,
              item.origin,
              item.rule,
              item.dot
            )) {
          goto finish;
        }
      } else if (
        symbol < ARITHMETIC_TOKEN_COUNT &&
        (transition.tokens & (1u << symbol)) &&
        !arithmetic_add_item(
          &chart,
          transition.target,
          item.origin,
          item.rule,
          (uint8_t)(item.dot + 1)
        )
      ) {
        goto finish;
      }
    }
    if (complete) {
      if (!arithmetic_complete(&chart, item)) goto finish;
    } else if (symbol >= ARITHMETIC_TOKEN_COUNT) {
      for (
        size_t candidate = 0; candidate < ARITHMETIC_RULE_COUNT; candidate++
      ) {
        if (
          arithmetic_rules[candidate].left == symbol &&
          !arithmetic_add_item(
            &chart,
            item.vertex,
            item.vertex,
            (uint8_t)candidate,
            0
          )
        ) {
          goto finish;
        }
      }
      size_t head = item.vertex * ARITHMETIC_SYMBOL_COUNT + symbol;
      for (size_t completion = chart.completed[head]; completion != SIZE_MAX;) {
        struct ArithmeticCompletion matched = chart.completions[completion];
        completion = matched.next;
        if (!arithmetic_add_item(
              &chart,
              matched.vertex,
              item.origin,
              item.rule,
              (uint8_t)(item.dot + 1)
            )) {
          goto finish;
        }
      }
    }
  }
  result = ARITHMETIC_VALIDATION_INVALID;
finish:
  ts_free(chart.items);
  ts_free(chart.heads);
  ts_free(chart.hash);
  ts_free(chart.completions);
  ts_free(chart.completion_hash);
  ts_free(chart.completed);
  ts_free(chart.pending);
  ts_free(chart.ready);
  return result;
}

static size_t arithmetic_reverse_vertex(size_t vertex, size_t endpoint) {
  return !vertex ? endpoint : vertex == endpoint ? 0 : vertex;
}

static enum ArithmeticValidation
arithmetic_reverse_graph(struct ArithmeticGraph *graph) {
  size_t endpoint = graph->vertex_ids[graph->length];
  if (endpoint == SIZE_MAX) return ARITHMETIC_VALIDATION_INVALID;
  struct ArithmeticVertex *vertices =
    ts_malloc(graph->vertex_count * sizeof(*vertices));
  if (!vertices) return ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
  for (size_t i = 0; i < graph->vertex_count; i++) {
    size_t reversed = arithmetic_reverse_vertex(i, endpoint);
    vertices[reversed] = (struct ArithmeticVertex){
      graph->length - graph->vertices[i].position,
      SIZE_MAX
    };
  }
  for (size_t origin = 0; origin < graph->vertex_count; origin++) {
    size_t target = arithmetic_reverse_vertex(origin, endpoint);
    for (size_t edge = graph->vertices[origin].edge; edge != SIZE_MAX;) {
      struct ArithmeticEdge transition = graph->edges[edge];
      size_t reversed = arithmetic_reverse_vertex(transition.target, endpoint);
      graph->edges[edge] = (struct ArithmeticEdge){
        target,
        vertices[reversed].edge,
        transition.tokens
      };
      vertices[reversed].edge = edge;
      edge = transition.next;
    }
  }
  graph->required_end = vertices[endpoint].position;
  ts_free(graph->vertices);
  graph->vertices = vertices;
  graph->vertex_capacity = graph->vertex_count;
  for (size_t i = 0; i < graph->vertex_count; i++)
    graph->vertex_ids[vertices[i].position] = i;
  return ARITHMETIC_VALIDATION_VALID;
}

static inline enum ArithmeticValidation
validate_arithmetic(const int32_t *source, size_t length) {
  struct ArithmeticGraph graph = {.source = source, .length = length};
  enum ArithmeticValidation result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
  if (
    length == SIZE_MAX ||
    length + 1 > SIZE_MAX / sizeof(uint64_t) ||
    length + 1 > SIZE_MAX / sizeof(size_t)
  ) {
    return result;
  }
  bool dynamic = false;
  for (size_t i = 0; i < length; i++) dynamic |= source[i] == -1;
  if (!dynamic) return arithmetic_literal(source, length);
  result = arithmetic_check_structure(source, length);
  if (result != ARITHMETIC_VALIDATION_VALID) return result;
  result = ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
  graph.vertex_ids = ts_malloc((length + 1) * sizeof(*graph.vertex_ids));
  graph.visited = ts_calloc(length + 1, sizeof(*graph.visited));
  graph.emitted = ts_calloc(length + 1, sizeof(*graph.emitted));
  if (!graph.vertex_ids || !graph.visited || !graph.emitted) goto finish;
  for (size_t i = 0; i <= length; i++) {
    graph.vertex_ids[i] = SIZE_MAX;
    if (i < length && source[i] != -1 && !arithmetic_whitespace(source[i]))
      graph.required_end = i + 1;
  }
  if (!arithmetic_build_graph(&graph)) goto finish;
  if (graph.valid) {
    result = ARITHMETIC_VALIDATION_VALID;
    goto finish;
  }
  // Reading backward exposes assignment constraints before ambiguous prefixes.
  result = arithmetic_reverse_graph(&graph);
  if (result != ARITHMETIC_VALIDATION_VALID) goto finish;
  result = arithmetic_project_graph(&graph);
  if (result == ARITHMETIC_VALIDATION_VALID)
    result = arithmetic_parse_graph(&graph);
finish:
  ts_free(graph.vertices);
  ts_free(graph.edges);
  ts_free(graph.vertex_ids);
  ts_free(graph.visited);
  ts_free(graph.emitted);
  ts_free(graph.endpoints);
  ts_free(graph.pending);
  return result;
}

static bool append(struct Text *text, struct Unit unit) {
  struct Unit *grown = diagnostic_grow(
    text->units,
    &text->capacity,
    text->count + 1,
    sizeof(*grown)
  );
  if (!grown) return false;
  text->units = grown;
  text->units[text->count++] = unit;
  return true;
}

static int32_t raw(const struct Input *input, size_t index) {
  return index < input->count ? input->units[index].value : -1;
}

static size_t logical_position(const struct Input *input) {
  size_t index = input->position;
  while (raw(input, index) == '\\' && raw(input, index + 1) == '\n') index += 2;
  return index;
}

static int32_t peek(const struct Input *input) {
  return raw(input, logical_position(input));
}

static size_t offset(const struct Input *input) {
  size_t index = logical_position(input);
  return index < input->count ? input->units[index].start : input->end;
}

static size_t consumed_offset(const struct Input *input) {
  return input->position ? input->units[input->position - 1].end : 0;
}

static bool capturing_delimiter(const struct Input *input) {
  for (
    const struct Capture *capture = input->capture; capture;
    capture = capture->parent
  )
    if (!capture->newlines_only) return true;
  return false;
}

static void capture_change(
  struct Input *input,
  struct Unit unit,
  int32_t value,
  bool quoted
) {
  for (
    struct Capture *capture = input->capture; capture; capture = capture->parent
  ) {
    if (capture->newlines_only) continue;
    struct Change *grown = diagnostic_grow(
      capture->changes,
      &capture->capacity,
      capture->count + 1,
      sizeof(*grown)
    );
    if (!grown) {
      capture->failed = true;
      continue;
    }
    capture->changes = grown;
    capture->changes[capture->count++] =
      (struct Change){unit.start, unit.end, value};
    capture->quoted |= quoted;
  }
}

static void capture_uncertain(struct Input *input, size_t position) {
  for (
    struct Capture *capture = input->capture; capture; capture = capture->parent
  )
    if (
      !capture->newlines_only &&
      (!capture->uncertain || position < capture->uncertain - 1)
    )
      capture->uncertain = position + 1;
}

static void capture_newline(struct Input *input, struct Unit unit) {
  if (unit.value == '\n')
    for (
      struct Capture *capture = input->capture; capture;
      capture = capture->parent
    )
      if (!capture->newline.end) capture->newline = unit;
}

static struct Unit read_raw(struct Input *input) {
  if (input->position >= input->count)
    return (struct Unit){-1, input->end, input->end};
  struct Unit unit = input->units[input->position++];
  capture_newline(input, unit);
  return unit;
}

static struct Unit pop(struct Input *input) {
  size_t position = logical_position(input);
  if (position > input->position)
    capture_change(
      input,
      (
        struct Unit
      ){0, input->units[input->position].start, input->units[position - 1].end},
      -1,
      false
    );
  input->position = position;
  return read_raw(input);
}

static struct Unit escaped_unit(struct Input *input, struct Unit slash) {
  struct Unit escaped = input->units[input->position++];
  escaped.start = slash.start;
  capture_change(input, escaped, escaped.value, true);
  return escaped;
}

static bool blank(int32_t c) {
  return c == ' ' || c == '\t';
}
static bool separator(int32_t c) {
  return c < 0 ||
    blank(c) ||
    c == '\n' ||
    c == ';' ||
    c == '&' ||
    c == '|' ||
    c == '(' ||
    c == ')' ||
    c == '<' ||
    c == '>';
}

static bool undecodable(int32_t c) {
  return c == 0 || c == INVALID_CHARACTER;
}

static bool expansion_escape(int32_t c) {
  return c == '$' || c == '`' || c == '\\';
}

enum { NESTING_LIMIT = 512 };

static bool nest(struct Parser *parser) {
  struct Plan *plan = parser->plan;
  if (plan->depth >= NESTING_LIMIT) {
    plan->failed = true;
    return false;
  }
  plan->depth++;
  return true;
}

static void unnest(struct Parser *parser) {
  parser->plan->depth--;
}

static size_t
owner_begin(struct Parser *parser, enum TokenType kind, size_t start) {
  struct Plan *plan = parser->plan;
  if (parser->opaque || plan->failed) return 0;
  struct Owner *grown = diagnostic_grow(
    plan->owners,
    &plan->owner_capacity,
    plan->owner_count + 1,
    sizeof(*grown)
  );
  if (!grown) {
    plan->failed = true;
    return 0;
  }
  plan->owners = grown;
  size_t id = plan->owner_count++;
  plan->owners[id] = (struct Owner){
    .kind = kind,
    .start = start,
    .end = start,
    .parent = parser->owner
  };
  parser->owner = id;
  return id;
}

static void owner_end(struct Parser *parser, size_t id, size_t end) {
  if (!id) return;
  struct Owner *owner = &parser->plan->owners[id];
  if (end > owner->end) owner->end = end;
  parser->owner = owner->parent;
}

#define OWNER_BIT(kind) (UINT64_C(1) << ((kind) - BEGIN_LIST))

static const uint64_t fault_owners[TOKEN_TYPE_COUNT] = {
  [COMMAND_GAP] = OWNER_BIT(BEGIN_PIPELINE) |
    OWNER_BIT(BEGIN_AND_OR) |
    OWNER_BIT(BEGIN_SUBSHELL) |
    OWNER_BIT(BEGIN_BRACE_GROUP) |
    OWNER_BIT(BEGIN_IF_CLAUSE) |
    OWNER_BIT(BEGIN_ELSE_PART) |
    OWNER_BIT(BEGIN_WHILE_CLAUSE) |
    OWNER_BIT(BEGIN_UNTIL_CLAUSE) |
    OWNER_BIT(BEGIN_DO_GROUP),
  [SEPARATOR_GAP] = OWNER_BIT(BEGIN_LIST) | OWNER_BIT(BEGIN_COMPOUND_LIST),
  [REDIRECTION_TARGET_GAP] = OWNER_BIT(BEGIN_IO_REDIRECT),
  [INVALID_REDIRECTION_TARGET] = OWNER_BIT(BEGIN_IO_REDIRECT),
  [HERE_DOCUMENT_DELIMITER_GAP] = OWNER_BIT(BEGIN_IO_REDIRECT),
  [INVALID_HERE_DOCUMENT_DELIMITER] = OWNER_BIT(BEGIN_IO_REDIRECT),
  [HERE_DOCUMENT_ENDING_GAP] = OWNER_BIT(BEGIN_HERE_DOCUMENT_BODY),
  [FOR_NAME_GAP] = OWNER_BIT(BEGIN_FOR_CLAUSE),
  [INVALID_FOR_NAME] = OWNER_BIT(BEGIN_FOR_CLAUSE),
  [FOR_SEPARATOR_GAP] = OWNER_BIT(BEGIN_FOR_CLAUSE),
  [INVALID_FOR_HEADER] = OWNER_BIT(BEGIN_FOR_CLAUSE),
  [INVALID_FOR_SEPARATOR] = OWNER_BIT(BEGIN_FOR_CLAUSE),
  [INVALID_DO] = OWNER_BIT(BEGIN_FOR_CLAUSE),
  [THEN_GAP] = OWNER_BIT(BEGIN_IF_CLAUSE) | OWNER_BIT(BEGIN_ELSE_PART),
  [FI_GAP] = OWNER_BIT(BEGIN_IF_CLAUSE),
  [DO_GAP] = OWNER_BIT(BEGIN_FOR_CLAUSE) |
    OWNER_BIT(BEGIN_WHILE_CLAUSE) |
    OWNER_BIT(BEGIN_UNTIL_CLAUSE),
  [DONE_GAP] = OWNER_BIT(BEGIN_DO_GROUP),
  [IN_GAP] = OWNER_BIT(BEGIN_CASE_CLAUSE),
  [ESAC_GAP] = OWNER_BIT(BEGIN_CASE_CLAUSE),
  [CASE_WORD_GAP] = OWNER_BIT(BEGIN_CASE_CLAUSE),
  [INVALID_CASE_WORD] = OWNER_BIT(BEGIN_CASE_CLAUSE),
  [INVALID_IN] = OWNER_BIT(BEGIN_CASE_CLAUSE),
  [CASE_PATTERN_GAP] =
    OWNER_BIT(BEGIN_CASE_ITEM) | OWNER_BIT(BEGIN_CASE_CLAUSE),
  [INVALID_CASE_PATTERN] =
    OWNER_BIT(BEGIN_CASE_ITEM) | OWNER_BIT(BEGIN_CASE_CLAUSE),
  [CASE_PATTERN_SEPARATOR_ERROR] =
    OWNER_BIT(BEGIN_CASE_ITEM) | OWNER_BIT(BEGIN_CASE_CLAUSE),
  [PARENTHESIS_GAP] = OWNER_BIT(BEGIN_SUBSHELL) |
    OWNER_BIT(BEGIN_CASE_ITEM) |
    OWNER_BIT(BEGIN_FUNCTION_DEFINITION),
  [BRACE_GAP] = OWNER_BIT(BEGIN_BRACE_GROUP),
  [FUNCTION_BODY_GAP] = OWNER_BIT(BEGIN_FUNCTION_DEFINITION),
  [INVALID_FUNCTION_BODY] = OWNER_BIT(BEGIN_FUNCTION_DEFINITION),
  [FUNCTION_PARAMETER_ERROR] = OWNER_BIT(BEGIN_FUNCTION_DEFINITION),
  [SINGLE_QUOTE_GAP] = OWNER_BIT(BEGIN_SINGLE_QUOTED),
  [DOUBLE_QUOTE_GAP] = OWNER_BIT(BEGIN_DOUBLE_QUOTED),
  [DOLLAR_QUOTE_GAP] = OWNER_BIT(BEGIN_DOLLAR_SINGLE_QUOTED),
  [INCOMPLETE_ESCAPE] = OWNER_BIT(BEGIN_ESCAPED_CHARACTER) |
    OWNER_BIT(BEGIN_DOLLAR_SINGLE_QUOTE_ESCAPE),
  [PARAMETER_NAME_GAP] = OWNER_BIT(BEGIN_PARAMETER_EXPANSION),
  [INVALID_PARAMETER_NAME] = OWNER_BIT(BEGIN_PARAMETER_EXPANSION),
  [PARAMETER_OPERATOR_GAP] = OWNER_BIT(BEGIN_PARAMETER_EXPANSION),
  [PARAMETER_OPERATOR_ERROR] = OWNER_BIT(BEGIN_PARAMETER_EXPANSION),
  [PARAMETER_LENGTH_SUFFIX_ERROR] = OWNER_BIT(BEGIN_PARAMETER_EXPANSION),
  [PARAMETER_WORD_GAP] = OWNER_BIT(BEGIN_PARAMETER_EXPANSION),
  [PARAMETER_ENDING_GAP] = OWNER_BIT(BEGIN_PARAMETER_EXPANSION),
  [UNSPECIFIED_PARAMETER_QUOTE] = OWNER_BIT(BEGIN_PARAMETER_EXPANSION),
  [SUBSTITUTION_ENDING_GAP] = OWNER_BIT(BEGIN_COMMAND_SUBSTITUTION),
  [BACKQUOTE_GAP] = OWNER_BIT(BEGIN_BACKQUOTE_SUBSTITUTION),
  [UNDEFINED_BACKQUOTE_QUOTE] = OWNER_BIT(BEGIN_BACKQUOTE_SUBSTITUTION),
  [UNDEFINED_BACKQUOTE_COMMENT] = OWNER_BIT(BEGIN_BACKQUOTE_SUBSTITUTION),
  [UNDEFINED_BACKQUOTE_HERE_DOCUMENT] = OWNER_BIT(BEGIN_BACKQUOTE_SUBSTITUTION),
  [UNDEFINED_BACKQUOTE_SUBSTITUTION] = OWNER_BIT(BEGIN_BACKQUOTE_SUBSTITUTION),
  [ARITHMETIC_ENDING_GAP] = OWNER_BIT(BEGIN_ARITHMETIC_EXPANSION),
  [UNSPECIFIED_DOLLAR_QUOTE_ESCAPE] =
    OWNER_BIT(BEGIN_DOLLAR_SINGLE_QUOTE_ESCAPE),
  [UNSPECIFIED_EMPTY_HEX_ESCAPE] = OWNER_BIT(BEGIN_DOLLAR_SINGLE_QUOTE_ESCAPE),
  [UNSPECIFIED_CONTROL_ESCAPE] = OWNER_BIT(BEGIN_DOLLAR_SINGLE_QUOTE_ESCAPE),
  [UNSPECIFIED_HEX_ESCAPE] = OWNER_BIT(BEGIN_DOLLAR_SINGLE_QUOTE_ESCAPE),
  [UNSPECIFIED_OCTAL_ESCAPE] = OWNER_BIT(BEGIN_DOLLAR_SINGLE_QUOTE_ESCAPE),
  [UNSPECIFIED_EXPANSION_START] = OWNER_BIT(BEGIN_WORD) |
    OWNER_BIT(BEGIN_DOUBLE_QUOTED) |
    OWNER_BIT(BEGIN_PARAMETER_EXPANSION) |
    OWNER_BIT(BEGIN_ARITHMETIC_EXPANSION) |
    OWNER_BIT(BEGIN_HERE_DOCUMENT_BODY),
  [UNSPECIFIED_HERE_DOCUMENT_START] = OWNER_BIT(BEGIN_COMMAND_SUBSTITUTION) |
    OWNER_BIT(BEGIN_BACKQUOTE_SUBSTITUTION),
  [UNSPECIFIED_HERE_DOCUMENT_NEWLINE] = OWNER_BIT(BEGIN_WORD),
  [RESERVED_COLON_WORD] = OWNER_BIT(BEGIN_WORD),
  [UNSPECIFIED_NEGATION_SPACING] = OWNER_BIT(BEGIN_PIPELINE),
};

static size_t fault_owner(const struct Parser *parser, enum TokenType kind) {
  uint64_t allowed = fault_owners[kind];
  for (
    size_t id = parser->owner; allowed && id;
    id = parser->plan->owners[id].parent
  )
    if (allowed & OWNER_BIT(parser->plan->owners[id].kind)) return id;
  return 0;
}

struct Fault {
  enum TokenType kind;
  size_t at, end, owner;
};

static void owned_issue(
  struct Parser *parser,
  struct Fault fault,
  struct Recovery recovery,
  size_t context_start
) {
  if (parser->opaque || parser->plan->failed) return;
  struct Plan *plan = parser->plan;
  struct Issue *grown = diagnostic_grow(
    plan->issues,
    &plan->issue_capacity,
    plan->issue_count + 1,
    sizeof(*grown)
  );
  if (!grown) {
    plan->failed = true;
    return;
  }
  plan->issues = grown;
  plan->issues[plan->issue_count++] = (struct Issue){
    fault.kind,
    fault.at,
    fault.end,
    context_start,
    fault.owner,
    recovery
  };
}

// Backquote faults rank by the lexical context they interrupt, not by owner.
static size_t fault_context(const struct Parser *parser, struct Fault fault) {
  const struct Owner *owners = parser->plan->owners;
  switch (fault.kind) {
    case UNDEFINED_BACKQUOTE_QUOTE:
    case UNDEFINED_BACKQUOTE_SUBSTITUTION:
      return owners[parser->owner].start;
    case UNDEFINED_BACKQUOTE_HERE_DOCUMENT:
      return owners[parser->owner].kind == BEGIN_HERE_DOCUMENT_BODY
        ? owners[parser->owner].start
        : SIZE_MAX;
    case UNSPECIFIED_HERE_DOCUMENT_START:
      return SIZE_MAX;
    default:
      return fault.owner ? owners[fault.owner].start : fault.at;
  }
}

static void
report(struct Parser *parser, struct Fault fault, struct Recovery recovery) {
  owned_issue(parser, fault, recovery, fault_context(parser, fault));
}

static void issue(
  struct Parser *parser,
  enum TokenType kind,
  size_t start,
  size_t end,
  struct Recovery recovery
) {
  if (parser->opaque || parser->plan->failed) return;
  struct Fault fault = {kind, start, end, fault_owner(parser, kind)};
  report(parser, fault, recovery);
}

static void
extend_region(struct Parser *parser, size_t position, size_t resume) {
  struct Plan *plan = parser->plan;
  if (plan->failed) return;
  struct Extension *grown = diagnostic_grow(
    plan->extensions,
    &plan->extension_capacity,
    plan->extension_count + 1,
    sizeof(*grown)
  );
  if (!grown) {
    plan->failed = true;
    return;
  }
  plan->extensions = grown;
  plan->extensions[plan->extension_count++] =
    (struct Extension){position, resume};
}

static bool backquote_cut(struct Parser *parser, enum TokenType kind) {
  size_t cut = parser->backquote_end;
  if (cut)
    issue(parser, kind, parser->input.end, cut, (struct Recovery){cut, true});
  return cut != 0;
}

static void input_end_gap(struct Parser *parser, enum TokenType gap) {
  size_t end = parser->input.end;
  issue(parser, gap, end, end, (struct Recovery){end, false});
}

static void
unclosed(struct Parser *parser, enum TokenType cut, enum TokenType gap) {
  if (!backquote_cut(parser, cut)) input_end_gap(parser, gap);
}

static void
text_push(struct Parser *parser, struct Text *text, struct Unit unit) {
  if (!append(text, unit)) parser->plan->failed = true;
}

static struct Parser
child_parser(const struct Parser *parent, struct Input input) {
  return (
    struct Parser
  ){.input = input,
    .plan = parent->plan,
    .opaque = parent->opaque,
    .quoted_delimiter = parent->quoted_delimiter,
    .owner = parent->owner,
    .backquote_end = parent->backquote_end};
}

enum ExpansionContext { UNQUOTED, DOUBLE_QUOTED, ARITHMETIC, HERE_BODY };

static void expansion(
  struct Parser *parser,
  struct Text *text,
  enum ExpansionContext context
);
static void backquote(struct Parser *parser, struct Text *text, bool quoted);
static void substitution(struct Parser *parser);
static void
parse_contents(struct Parser *parser, struct Input input, size_t backquote_end);

static int32_t peek_quoted(const struct Input *input, bool literal) {
  return literal ? raw(input, input->position) : peek(input);
}

static struct Unit pop_quoted(struct Input *input, bool literal) {
  return literal ? read_raw(input) : pop(input);
}

static bool double_quote_escape(
  struct Parser *parser,
  struct Text *text,
  struct Unit slash
) {
  struct Input *input = &parser->input;
  int32_t next = raw(input, input->position);
  if (!expansion_escape(next) && next != '"') return false;
  text_push(parser, text, escaped_unit(input, slash));
  return true;
}

static void quoted(struct Parser *parser, struct Text *text, int32_t quote) {
  if (!nest(parser)) return;
  struct Input *input = &parser->input;
  bool literal = quote == '\'';
  size_t owner = owner_begin(
    parser,
    literal ? BEGIN_SINGLE_QUOTED : BEGIN_DOUBLE_QUOTED,
    offset(input)
  );
  int32_t parent_closer = parser->closer;
  bool quoted_delimiter = parser->quoted_delimiter;
  parser->closer = quote;
  if (!literal && capturing_delimiter(input)) parser->quoted_delimiter = true;
  struct Unit opening = pop(input);
  bool preserve = literal && parser->quoted_delimiter;
  if (!preserve) capture_change(input, opening, -1, true);
  while (!parser->plan->failed) {
    int32_t c = peek_quoted(input, literal);
    if (c < 0) {
      unclosed(
        parser,
        UNDEFINED_BACKQUOTE_QUOTE,
        literal ? SINGLE_QUOTE_GAP : DOUBLE_QUOTE_GAP
      );
      break;
    }
    if (c == quote) {
      struct Unit closing = pop_quoted(input, literal);
      if (!preserve) capture_change(input, closing, -1, true);
      break;
    }
    if (!literal && (c == '$' || c == '`')) {
      if (c == '$') expansion(parser, text, DOUBLE_QUOTED);
      else backquote(parser, text, true);
      continue;
    }
    struct Unit unit = pop_quoted(input, literal);
    if (literal || c != '\\' || !double_quote_escape(parser, text, unit))
      text_push(parser, text, unit);
  }
  parser->closer = parent_closer;
  parser->quoted_delimiter = quoted_delimiter;
  owner_end(parser, owner, consumed_offset(input));
  unnest(parser);
}

static bool special_parameter(int32_t c) {
  return c == '@' ||
    c == '*' ||
    c == '#' ||
    c == '?' ||
    c == '-' ||
    c == '$' ||
    c == '!';
}

static bool parameter_name(struct Input *input, bool braced) {
  int32_t c = peek(input);
  if (is_name_start_character(c)) {
    do {
      pop(input);
    } while (is_name_character(peek(input)));
  } else if (is_decimal_digit(c) && braced) {
    do {
      pop(input);
    } while (is_decimal_digit(peek(input)));
  } else if (is_decimal_digit(c) || special_parameter(c)) pop(input);
  else return false;
  return true;
}

struct ParameterHead {
  struct Fault fault;
  bool operand, pattern;
};

static bool length_operator(const struct Input *input) {
  struct Input probe = *input;
  probe.capture = NULL;
  pop(&probe);
  int32_t next = peek(&probe);
  return parameter_name(&probe, true) &&
    ((next != '-' && next != '?' && next != '#') ||
      peek(&probe) == '}' ||
      peek(&probe) < 0);
}

static void parameter_operator(
  struct Parser *parser,
  struct ParameterHead *head,
  bool length,
  bool hash
) {
  struct Input *input = &parser->input;
  head->fault.at = offset(input);
  struct Unit operator = input->units[logical_position(input)];
  head->fault.end = operator.end;
  int32_t c = operator.value;
  head->pattern = c == '#' || c == '%';
  if (!length && c == ':') {
    pop(input);
    c = peek(input);
    head->fault.at = offset(input);
    head->fault.end =
      c < 0 ? head->fault.at : input->units[logical_position(input)].end;
  }
  if (length) head->fault.kind = PARAMETER_LENGTH_SUFFIX_ERROR;
  else if (operator.value == ':' && (c < 0 || c == '}')) {
    head->fault.kind = PARAMETER_OPERATOR_GAP;
    head->fault.end = head->fault.at;
  } else if (!(head->pattern || c == '-' || c == '+' || c == '?' || c == '='))
    head->fault.kind = PARAMETER_OPERATOR_ERROR;
  else {
    head->operand = true;
    pop(input);
    if (head->pattern) {
      if (peek(input) == c) pop(input);
    } else if (
      hash && operator.value != ':' && (peek(input) < 0 || peek(input) == '}')
    ) {
      head->fault.kind = PARAMETER_WORD_GAP;
      head->fault.at = head->fault.end = offset(input);
    }
  }
}

static struct ParameterHead parameter_head(struct Parser *parser) {
  struct Input *input = &parser->input;
  struct ParameterHead head =
    {{TOKEN_TYPE_COUNT, offset(input), 0, 0}, false, false};
  head.fault.end = head.fault.at;
  bool hash = peek(input) == '#';
  bool length = hash && length_operator(input);
  if (length) pop(input);
  if (!parameter_name(input, true)) {
    head.fault.kind = peek(input) < 0 || peek(input) == '}'
      ? PARAMETER_NAME_GAP
      : INVALID_PARAMETER_NAME;
    if (head.fault.kind == INVALID_PARAMETER_NAME)
      head.fault.end = input->units[logical_position(input)].end;
    return head;
  }
  if (peek(input) != '}' && peek(input) >= 0)
    parameter_operator(parser, &head, length, hash);
  return head;
}

static void parameter_escape(
  struct Parser *parser,
  bool double_quoted,
  struct Fault *fault
) {
  struct Input *input = &parser->input;
  struct Unit slash = pop(input);
  size_t owner = double_quoted
    ? 0
    : owner_begin(parser, BEGIN_ESCAPED_CHARACTER, slash.start);
  int32_t next = raw(input, input->position);
  if (next < 0) {
    if (!double_quoted && fault->kind == TOKEN_TYPE_COUNT)
      *fault = (struct Fault){INCOMPLETE_ESCAPE, slash.start, slash.end, owner};
  } else if (
    !double_quoted || expansion_escape(next) || next == '"' || next == '}'
  )
    escaped_unit(input, slash);
  else input->position++;
  owner_end(parser, owner, consumed_offset(input));
}

static void
parameter_word(struct Parser *parser, bool double_quoted, struct Fault *fault) {
  struct Input *input = &parser->input;
  struct Text ignored = {0};
  while (!parser->plan->failed && peek(input) >= 0 && peek(input) != '}') {
    int32_t c = peek(input);
    if (c == '$')
      expansion(parser, &ignored, double_quoted ? DOUBLE_QUOTED : UNQUOTED);
    else if (c == '`') backquote(parser, &ignored, double_quoted);
    else if (c == '"' || (c == '\'' && !double_quoted)) {
      if (
        c == '"' &&
        double_quoted &&
        fault->kind == TOKEN_TYPE_COUNT &&
        !capturing_delimiter(input)
      ) {
        *fault = (struct Fault){
          UNSPECIFIED_PARAMETER_QUOTE,
          offset(input),
          input->units[logical_position(input)].end,
          0
        };
        parser->opaque = true;
      }
      quoted(parser, &ignored, c);
    } else if (c == '\\') parameter_escape(parser, double_quoted, fault);
    else pop(input);
  }
  ts_free(ignored.units);
}

static void parameter(struct Parser *parser, bool double_quoted) {
  struct Input *input = &parser->input;
  pop(input);
  int32_t parent_closer = parser->closer;
  parser->closer = '}';
  struct ParameterHead head = parameter_head(parser);
  struct Fault fault = head.fault;
  if (head.operand && head.pattern) double_quoted = false;
  bool opaque = parser->opaque;
  if (fault.kind != TOKEN_TYPE_COUNT) parser->opaque = true;
  size_t word_owner = 0;
  if (
    head.operand &&
    fault.kind == TOKEN_TYPE_COUNT &&
    peek(input) >= 0 &&
    peek(input) != '}'
  )
    word_owner = owner_begin(parser, BEGIN_WORD, offset(input));
  parameter_word(parser, double_quoted, &fault);
  parser->opaque = opaque;
  owner_end(parser, word_owner, consumed_offset(input));
  size_t boundary = offset(input);
  if (fault.kind != TOKEN_TYPE_COUNT) {
    if (!fault.owner) fault.owner = fault_owner(parser, fault.kind);
    report(parser, fault, (struct Recovery){boundary, peek(input) == '}'});
  }
  if (peek(input) == '}') pop(input);
  else if (fault.kind == TOKEN_TYPE_COUNT)
    input_end_gap(parser, PARAMETER_ENDING_GAP);
  parser->closer = parent_closer;
}

static bool undecidable_delimiter_fault(enum TokenType kind) {
  switch (kind) {
    case UNSPECIFIED_DOLLAR_QUOTE_ESCAPE:
    case UNSPECIFIED_EMPTY_HEX_ESCAPE:
    case UNSPECIFIED_CONTROL_ESCAPE:
    case UNSPECIFIED_HEX_ESCAPE:
    case UNSPECIFIED_OCTAL_ESCAPE:
      return true;
    default:
      return false;
  }
}

static int hex_value(int32_t c) {
  if (is_decimal_digit(c)) return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static enum TokenType hex_escape(struct Parser *parser, struct Unit *unit) {
  struct Input *input = &parser->input;
  uint8_t value = 0;
  size_t digits = 0;
  while (hex_value(raw(input, input->position)) >= 0) {
    value = (uint8_t)((value << 4) | hex_value(read_raw(input).value));
    digits++;
  }
  unit->value = -(int32_t)value - 2;
  int32_t next = raw(input, input->position);
  if (!digits && !undecodable(next))
    return next < 0 ? INCOMPLETE_ESCAPE : UNSPECIFIED_EMPTY_HEX_ESCAPE;
  return digits > 2 ? UNSPECIFIED_HEX_ESCAPE : TOKEN_TYPE_COUNT;
}

static enum TokenType
octal_escape(struct Parser *parser, struct Unit *unit, int32_t first) {
  struct Input *input = &parser->input;
  unsigned value = (unsigned)(first - '0');
  for (
    size_t i = 1; i < 3 &&
    raw(input, input->position) >= '0' &&
    raw(input, input->position) <= '7';
    i++
  )
    value = (value << 3) | (unsigned)(read_raw(input).value - '0');
  unit->value = -(int32_t)(uint8_t)value - 2;
  return value > UINT8_MAX ? UNSPECIFIED_OCTAL_ESCAPE : TOKEN_TYPE_COUNT;
}

static enum TokenType
control_escape(struct Parser *parser, struct Unit *unit, size_t *end) {
  struct Input *input = &parser->input;
  int32_t c = raw(input, input->position);
  int32_t next = raw(input, input->position + 1);
  if (
    (c >= 'A' && c <= 'Z') ||
    (c >= 'a' && c <= 'z') ||
    c == '[' ||
    c == ']' ||
    c == '^' ||
    c == '_' ||
    c == '?'
  ) {
    read_raw(input);
    unit->value = c == '?' ? 127 : c & 31;
    return TOKEN_TYPE_COUNT;
  }
  if (c == '\\' && next == '\\') {
    read_raw(input);
    read_raw(input);
    unit->value = 28;
    return TOKEN_TYPE_COUNT;
  }
  if (c < 0 || (c == '\\' && next < 0)) {
    if (c == '\\') read_raw(input);
    return INCOMPLETE_ESCAPE;
  }
  if (undecodable(c) || (c == '\\' && undecodable(next)))
    return TOKEN_TYPE_COUNT;
  if (c != '\'') {
    read_raw(input);
    if (c == '\\' && next != '\'') read_raw(input);
    *end = consumed_offset(input);
  }
  return UNSPECIFIED_CONTROL_ESCAPE;
}

static struct Fault
dollar_quote_escape(struct Parser *parser, struct Unit *unit) {
  struct Input *input = &parser->input;
  size_t owner =
    owner_begin(parser, BEGIN_DOLLAR_SINGLE_QUOTE_ESCAPE, unit->start);
  enum TokenType fault = TOKEN_TYPE_COUNT;
  size_t end = 0;
  int32_t c = raw(input, input->position);
  if (c < 0) fault = INCOMPLETE_ESCAPE;
  else {
    read_raw(input);
    unit->value = c;
    const char *keys = "abefnrtv\"'\\";
    const int32_t values[] = {7, 8, 27, 12, 10, 13, 9, 11, '"', '\'', '\\'};
    bool known = false;
    for (size_t i = 0; keys[i]; i++)
      if (c == keys[i]) {
        unit->value = values[i];
        known = true;
        break;
      }
    if (c == 'x') fault = hex_escape(parser, unit);
    else if (c >= '0' && c <= '7') fault = octal_escape(parser, unit, c);
    else if (c == 'c') fault = control_escape(parser, unit, &end);
    else if (!known && !undecodable(c)) fault = UNSPECIFIED_DOLLAR_QUOTE_ESCAPE;
    unit->end = input->units[input->position - 1].end;
    capture_change(input, *unit, unit->value, true);
  }
  if (undecidable_delimiter_fault(fault)) capture_uncertain(input, unit->start);
  if (!end) end = unit->end;
  owner_end(parser, owner, end);
  return (struct Fault){fault, unit->start, end, owner};
}

static void dollar_quote(struct Parser *parser, struct Text *text) {
  struct Input *input = &parser->input;
  struct Unit opening = pop(input);
  capture_change(input, opening, -1, true);
  bool closed = false;
  while (!parser->plan->failed && input->position < input->count) {
    struct Unit unit = read_raw(input);
    if (unit.value == '\'') {
      capture_change(input, unit, -1, true);
      closed = true;
      break;
    }
    if (unit.value == '\\') {
      struct Fault escape = dollar_quote_escape(parser, &unit);
      if (escape.kind != TOKEN_TYPE_COUNT)
        report(
          parser,
          escape,
          (struct Recovery){escape.end, escape.kind != INCOMPLETE_ESCAPE}
        );
    }
    text_push(parser, text, unit);
  }
  if (!closed) unclosed(parser, UNDEFINED_BACKQUOTE_QUOTE, DOLLAR_QUOTE_GAP);
}

struct BackquoteInput {
  struct Input *input;
  bool quoted, literal_next;
};

static struct Unit backquote_next(struct BackquoteInput *reader) {
  struct Input *input = reader->input;
  while (input->position < input->count) {
    struct Unit unit = input->units[input->position++];
    if (!reader->quoted || reader->literal_next) {
      reader->literal_next = false;
      return unit;
    }
    if (unit.value == '\\') {
      if (raw(input, input->position) == '\n') {
        unit.end = input->units[input->position++].end;
        capture_change(input, unit, -1, false);
        continue;
      }
      reader->literal_next = true;
    }
    return unit;
  }
  return (struct Unit){-1, input->end, input->end};
}

static size_t backquote_body(
  struct Parser *parser,
  bool double_quoted,
  struct Text *content,
  size_t *end
) {
  struct Input *input = &parser->input;
  struct BackquoteInput reader = {.input = input, .quoted = double_quoted};
  while (!parser->plan->failed && input->position < input->count) {
    struct Unit unit = backquote_next(&reader);
    if (unit.value < 0) break;
    if (unit.value == '`') {
      *end = unit.start;
      return unit.end;
    }
    if (unit.value == '\\') {
      struct Unit decoded = backquote_next(&reader);
      int32_t next = decoded.value;
      if (expansion_escape(next) || (double_quoted && next == '"')) {
        decoded.start = unit.start;
        unit = decoded;
        capture_change(input, unit, unit.value, true);
      } else {
        text_push(parser, content, unit);
        if (next >= 0) text_push(parser, content, decoded);
        continue;
      }
    }
    text_push(parser, content, unit);
  }
  return 0;
}

static void backquote_contents(
  struct Parser *parser,
  struct Text *text,
  bool double_quoted
) {
  struct Input *input = &parser->input;
  struct Unit opening = pop(input);
  struct Text content = {0};
  size_t end = input->end;
  size_t closed = backquote_body(parser, double_quoted, &content, &end);
  struct Rejected rejected = {0};
  parse_contents(
    parser,
    (
      struct Input
    ){content.units, content.count, 0, end, input->capture, &rejected},
    closed
  );
  ts_free(rejected.bits);
  ts_free(content.units);
  text_push(parser, text, (struct Unit){-1, opening.start, offset(input)});
}

static void
backquote(struct Parser *parser, struct Text *text, bool double_quoted) {
  if (!nest(parser)) return;
  size_t owner =
    owner_begin(parser, BEGIN_BACKQUOTE_SUBSTITUTION, offset(&parser->input));
  backquote_contents(parser, text, double_quoted);
  owner_end(parser, owner, consumed_offset(&parser->input));
  unnest(parser);
}

// Cache rejected `$((` probes to avoid exponential work during nested fallback.
static bool rejected_arithmetic(const struct Input *input, size_t at) {
  const uint8_t *bits = input->rejected->bits;
  return bits && (bits[at / 8] >> (at % 8)) & 1;
}

static void reject_arithmetic(struct Parser *parser, size_t at) {
  struct Rejected *rejected = parser->input.rejected;
  if (!rejected->bits)
    rejected->bits = ts_calloc(parser->input.count / 8 + 1, 1);
  if (!rejected->bits) parser->plan->failed = true;
  else rejected->bits[at / 8] |= (uint8_t)(1u << (at % 8));
}

struct PlanMark {
  size_t issues, owners, extensions;
};

static struct PlanMark plan_mark(const struct Plan *plan) {
  return (
    struct PlanMark
  ){plan->issue_count, plan->owner_count, plan->extension_count};
}

static void plan_restore(struct Plan *plan, struct PlanMark mark) {
  plan->issue_count = mark.issues;
  plan->owner_count = mark.owners;
  plan->extension_count = mark.extensions;
}

static struct Text
arithmetic_content(struct Parser *probe, bool *ending, bool *closed) {
  struct Input *input = &probe->input;
  struct Text content = {0};
  size_t depth = 0;
  pop(input);
  while (!probe->plan->failed && peek(input) >= 0) {
    int32_t c = peek(input);
    if (c == ')' && depth == 0) {
      *ending = true;
      pop(input);
      if (peek(input) == ')') {
        pop(input);
        *closed = true;
      }
      break;
    }
    if (c == '$') expansion(probe, &content, ARITHMETIC);
    else if (c == '`') backquote(probe, &content, true);
    else {
      struct Unit unit = pop(input);
      if (c == '(') depth++;
      else if (c == ')' && depth) depth--;
      if (c == '\\' && expansion_escape(raw(input, input->position)))
        unit = escaped_unit(input, unit);
      text_push(probe, &content, unit);
    }
  }
  return content;
}

static bool
arithmetic_valid(struct Plan *plan, const struct Text *content, bool open) {
  int32_t *values = content->count < SIZE_MAX / sizeof(*values)
    ? ts_malloc((content->count + 1) * sizeof(*values))
    : NULL;
  if (!values) {
    plan->failed = true;
    return false;
  }
  for (size_t i = 0; i < content->count; i++)
    values[i] = content->units[i].value;
  size_t length = content->count;
  // An EOF continuation is an unconstrained suffix, like an expansion hole.
  if (open) values[length++] = -1;
  enum ArithmeticValidation validation = validate_arithmetic(values, length);
  ts_free(values);
  plan->failed |= validation == ARITHMETIC_VALIDATION_RESOURCE_FAILURE;
  return validation == ARITHMETIC_VALIDATION_VALID;
}

static void
replay_capture(struct Parser *parser, const struct Capture *capture) {
  if (capture->newline.end) capture_newline(&parser->input, capture->newline);
  if (capture->uncertain)
    capture_uncertain(&parser->input, capture->uncertain - 1);
  for (size_t i = 0; i < capture->count; i++) {
    struct Change change = capture->changes[i];
    capture_change(
      &parser->input,
      (struct Unit){0, change.start, change.end},
      change.value,
      capture->quoted
    );
  }
}

static bool arithmetic(struct Parser *parser) {
  size_t at = logical_position(&parser->input);
  if (rejected_arithmetic(&parser->input, at)) return false;
  struct Parser probe = child_parser(parser, parser->input);
  struct Capture capture = {
    .newlines_only = !capturing_delimiter(&parser->input)
  };
  if (parser->input.capture) probe.input.capture = &capture;
  struct Plan *plan = parser->plan;
  struct PlanMark mark = plan_mark(plan);
  bool ending = false, closed = false;
  struct Text content = arithmetic_content(&probe, &ending, &closed);
  bool incomplete = !closed && peek(&probe.input) < 0;
  bool accepted = (closed || incomplete) &&
    !plan->failed &&
    arithmetic_valid(plan, &content, incomplete && !ending);
  ts_free(content.units);
  if (accepted && !plan->failed) {
    replay_capture(parser, &capture);
    probe.input.capture = parser->input.capture;
    parser->input = probe.input;
    if (!closed) input_end_gap(parser, ARITHMETIC_ENDING_GAP);
  }
  plan->failed |= capture.failed;
  ts_free(capture.changes);
  if (!accepted) {
    plan_restore(plan, mark);
    if (!plan->failed) reject_arithmetic(parser, at);
  }
  return accepted;
}

static bool
literal_dollar(const struct Parser *parser, enum ExpansionContext context) {
  const struct Input *input = &parser->input;
  int32_t c = peek(input);
  if (
    c < 0 ||
    undecodable(c) ||
    blank(c) ||
    c == '\n' ||
    c == parser->closer ||
    c == '\'' ||
    c == '`'
  )
    return true;
  if (c == '"') return context == UNQUOTED || context == DOUBLE_QUOTED;
  if (c == '\\') {
    if (context == UNQUOTED) return true;
    int32_t next = raw(input, logical_position(input) + 1);
    return expansion_escape(next) ||
      (context == DOUBLE_QUOTED &&
        (next == '"' || (parser->closer == '}' && next == '}')));
  }
  return (context == UNQUOTED && !parser->closer && separator(c)) ||
    (context == ARITHMETIC && c == ')');
}

static void dollar_literal(
  struct Parser *parser,
  struct Text *text,
  struct Unit dollar,
  enum ExpansionContext context
) {
  struct Input *input = &parser->input;
  if (!literal_dollar(parser, context)) {
    size_t end = input->units[logical_position(input)].end;
    issue(
      parser,
      UNSPECIFIED_EXPANSION_START,
      dollar.start,
      end,
      (struct Recovery){end, true}
    );
  }
  text_push(parser, text, dollar);
}

static void expansion_contents(
  struct Parser *parser,
  struct Text *text,
  enum ExpansionContext context
) {
  struct Input *input = &parser->input;
  struct Unit dollar = pop(input);
  int32_t c = peek(input);
  bool double_quoted = context != UNQUOTED;
  size_t owner = 0;
  if (c == '{') {
    owner = owner_begin(parser, BEGIN_PARAMETER_EXPANSION, dollar.start);
    parameter(parser, double_quoted);
  } else if (c == '(') {
    pop(input);
    owner = owner_begin(parser, BEGIN_ARITHMETIC_EXPANSION, dollar.start);
    if (peek(input) != '(' || !arithmetic(parser)) {
      if (owner) parser->plan->owners[owner].kind = BEGIN_COMMAND_SUBSTITUTION;
      substitution(parser);
    }
  } else if (c == '\'' && !double_quoted) {
    owner = owner_begin(parser, BEGIN_DOLLAR_SINGLE_QUOTED, dollar.start);
    capture_change(input, dollar, -1, true);
    dollar_quote(parser, text);
    owner_end(parser, owner, consumed_offset(input));
    return;
  } else if (!parameter_name(input, false)) {
    dollar_literal(parser, text, dollar, context);
    return;
  }
  owner_end(parser, owner, consumed_offset(input));
  text_push(parser, text, (struct Unit){-1, dollar.start, offset(input)});
}

static void expansion(
  struct Parser *parser,
  struct Text *text,
  enum ExpansionContext context
) {
  if (!nest(parser)) return;
  expansion_contents(parser, text, context);
  unnest(parser);
}

static void escaped_character(struct Parser *parser, struct Token *token) {
  struct Input *input = &parser->input;
  struct Unit slash = pop(input);
  size_t owner = owner_begin(parser, BEGIN_ESCAPED_CHARACTER, slash.start);
  if (raw(input, input->position) >= 0)
    text_push(parser, &token->text, escaped_unit(input, slash));
  else if (parser->backquote_end) text_push(parser, &token->text, slash);
  else
    issue(
      parser,
      INCOMPLETE_ESCAPE,
      slash.start,
      slash.end,
      (struct Recovery){input->end, false}
    );
  owner_end(parser, owner, consumed_offset(input));
}

static bool digits_only(const struct Text *text) {
  for (size_t i = 0; i < text->count; i++)
    if (!is_decimal_digit(text->units[i].value)) return false;
  return text->count > 0;
}

static struct Token word(struct Parser *parser) {
  struct Input *input = &parser->input;
  struct Token token = {.kind = WORD, .start = offset(input), .plain = true};
  token.owner = owner_begin(parser, BEGIN_WORD, token.start);
  bool prefix_name = true;
  size_t count = 0;
  for (; !parser->plan->failed && !separator(peek(input)); count++) {
    int32_t c = peek(input);
    bool name_character =
      count ? is_name_character(c) : is_name_start_character(c);
    if (!token.name_fault.end && !name_character)
      token.name_fault = input->units[logical_position(input)];
    if (c == '$') expansion(parser, &token.text, UNQUOTED);
    else if (c == '`') backquote(parser, &token.text, false);
    else if (c == '\'' || c == '"') quoted(parser, &token.text, c);
    else if (c == '\\') escaped_character(parser, &token);
    else {
      prefix_name = prefix_name && name_character;
      text_push(parser, &token.text, pop(input));
      continue;
    }
    token.plain = false;
    prefix_name = false;
  }
  token.end = consumed_offset(input);
  token.name = token.plain && prefix_name && count > 0;
  if (
    token.plain &&
    digits_only(&token.text) &&
    (peek(input) == '<' || peek(input) == '>')
  )
    token.kind = IO_NUMBER;
  owner_end(parser, token.owner, token.end);
  return token;
}

static void comment(struct Parser *parser) {
  struct Input *input = &parser->input;
  size_t start = offset(input);
  input->position = logical_position(input);
  while (
    raw(input, input->position) >= 0 && raw(input, input->position) != '\n'
  )
    input->position++;
  if (input->position == input->count && parser->backquote_end)
    owned_issue(
      parser,
      (
        struct Fault
      ){UNDEFINED_BACKQUOTE_COMMENT,
        input->end,
        parser->backquote_end,
        fault_owner(parser, UNDEFINED_BACKQUOTE_COMMENT)},
      (struct Recovery){parser->backquote_end, true},
      start
    );
}

static enum Lexeme operator_kind(struct Parser *parser, int32_t c) {
  struct Input *input = &parser->input;
  pop(input);
  int32_t next = peek(input);
  switch (c) {
    case '\n':
      return NEWLINE;
    case '(':
      return LEFT;
    case ')':
      return RIGHT;
    case ';':
      if (next != ';' && next != '&') return SEMICOLON;
      pop(input);
      return next == ';' ? DSEMI : SEMI_AND;
    case '&':
      if (next != '&') return AMPERSAND;
      pop(input);
      return AND;
    case '|':
      if (next != '|') return PIPE;
      pop(input);
      return OR;
    case '<':
      if (next == '<') {
        pop(input);
        if (peek(input) != '-') return HERE;
        pop(input);
        return HERE_STRIP;
      }
      if (next == '&' || next == '>') pop(input);
      return REDIRECT;
    default:
      if (next == '&' || next == '>' || next == '|') pop(input);
      return REDIRECT;
  }
}

static struct Token lex(struct Parser *parser) {
  struct Input *input = &parser->input;
  while (blank(peek(input))) pop(input);
  if (peek(input) == '#') comment(parser);
  struct Token token = {.start = offset(input)};
  int32_t c = peek(input);
  if (c < 0) {
    // Trailing line continuations must not survive as here-document text.
    pop(input);
    token.kind = END;
    token.end = token.start;
    return token;
  }
  if (!separator(c)) return word(parser);
  token.kind = operator_kind(parser, c);
  token.end = consumed_offset(input);
  return token;
}

static void saved_token_newline(struct Parser *parser, struct Unit newline) {
  if (!newline.end || parser->token.kind == NEWLINE) return;
  report(
    parser,
    (
      struct Fault
    ){UNSPECIFIED_HERE_DOCUMENT_NEWLINE,
      newline.start,
      newline.end,
      parser->token.owner},
    (struct Recovery){offset(&parser->input), true}
  );
}

static void lookahead(struct Parser *parser) {
  struct Capture capture = {
    .parent = parser->input.capture,
    .newlines_only = true
  };
  bool pending = parser->document_count > 0;
  if (pending) parser->input.capture = &capture;
  parser->token = lex(parser);
  parser->input.capture = capture.parent;
  if (pending) saved_token_newline(parser, capture.newline);
  parser->ready = true;
}

static const struct Token *look(struct Parser *parser) {
  if (!parser->ready) lookahead(parser);
  return &parser->token;
}

static struct Fault token_fault(
  struct Parser *parser,
  bool missing,
  enum TokenType gap,
  enum TokenType invalid
) {
  const struct Token *token = look(parser);
  struct Fault fault = {missing ? gap : invalid, token->start, token->end, 0};
  if (missing) fault.end = fault.at;
  fault.owner = fault.kind == RESERVED_COLON_WORD
    ? token->owner
    : fault_owner(parser, fault.kind);
  return fault;
}

static void encode(struct Parser *parser, struct Text *text, struct Unit unit) {
  uint32_t c = (uint32_t)unit.value;
  uint8_t bytes[4];
  size_t count;
  if (unit.value <= -2) {
    bytes[0] = (uint8_t)(-unit.value - 2);
    count = 1;
  } else if (c < 0x80) {
    bytes[0] = (uint8_t)c;
    count = 1;
  } else if (c < 0x800) {
    bytes[0] = (uint8_t)(0xc0 | (c >> 6));
    bytes[1] = (uint8_t)(0x80 | (c & 63));
    count = 2;
  } else if (c < 0x10000) {
    bytes[0] = (uint8_t)(0xe0 | (c >> 12));
    bytes[1] = (uint8_t)(0x80 | ((c >> 6) & 63));
    bytes[2] = (uint8_t)(0x80 | (c & 63));
    count = 3;
  } else {
    bytes[0] = (uint8_t)(0xf0 | (c >> 18));
    bytes[1] = (uint8_t)(0x80 | ((c >> 12) & 63));
    bytes[2] = (uint8_t)(0x80 | ((c >> 6) & 63));
    bytes[3] = (uint8_t)(0x80 | (c & 63));
    count = 4;
  }
  for (size_t i = 0; i < count; i++)
    text_push(parser, text, (struct Unit){bytes[i], unit.start, unit.end});
}

static size_t unit_after(
  const struct Unit *units,
  size_t low,
  size_t high,
  size_t offset,
  bool ends
) {
  while (low < high) {
    size_t middle = low + (high - low) / 2;
    if ((ends ? units[middle].end : units[middle].start) <= offset)
      low = middle + 1;
    else high = middle;
  }
  return low;
}

// Changes nest or are disjoint; later changes take precedence.
static struct Text captured_delimiter(
  struct Parser *parser,
  const struct Capture *capture,
  size_t start,
  size_t end
) {
  const size_t removed = SIZE_MAX;
  const struct Unit *units = parser->input.units;
  struct Text bytes = {0};
  size_t *slots = ts_calloc(end - start + 1, sizeof(*slots));
  if (!slots) {
    parser->plan->failed = true;
    return bytes;
  }
  for (size_t i = 0; i < capture->count; i++) {
    struct Change change = capture->changes[i];
    size_t first = unit_after(units, start, end, change.start, true);
    size_t last = unit_after(units, first, end, change.end - 1, false);
    if (first == last) continue;
    for (size_t j = first; j < last; j++) slots[j - start] = removed;
    if (change.value != -1) slots[first - start] = i + 1;
  }
  for (size_t j = start; j < end && !parser->plan->failed; j++) {
    size_t slot = slots[j - start];
    if (!slot) encode(parser, &bytes, units[j]);
    else if (slot != removed) {
      struct Change change = capture->changes[slot - 1];
      encode(
        parser,
        &bytes,
        (struct Unit){change.value, change.start, change.end}
      );
    }
  }
  ts_free(slots);
  return bytes;
}

static void here_body(struct Parser *parser, struct Text content, size_t end) {
  struct Rejected rejected = {0};
  struct Parser body = child_parser(
    parser,
    (
      struct Input
    ){content.units, content.count, 0, end, parser->input.capture, &rejected}
  );
  body.backquote_end = 0;
  struct Text ignored = {0};
  while (!parser->plan->failed && body.input.position < body.input.count) {
    int32_t c = raw(&body.input, body.input.position);
    if (c == '$') expansion(&body, &ignored, HERE_BODY);
    else if (c == '`') backquote(&body, &ignored, true);
    else {
      body.input.position++;
      int32_t next = raw(&body.input, body.input.position);
      if (c == '\\' && expansion_escape(next)) body.input.position++;
    }
  }
  ts_free(ignored.units);
  ts_free(rejected.bits);
}

static void here_document_line(
  struct Parser *parser,
  const struct Document *here,
  struct Text *line
) {
  struct Input *input = &parser->input;
  while (input->position < input->count && !parser->plan->failed) {
    struct Unit unit = input->units[input->position++];
    if (!here->quoted && unit.value == '\\') {
      int32_t next = raw(input, input->position);
      if (next == '\n') {
        unit.end = input->units[input->position++].end;
        capture_change(input, unit, -1, false);
        continue;
      }
      text_push(parser, line, unit);
      if (next == '\\')
        text_push(parser, line, input->units[input->position++]);
      continue;
    }
    text_push(parser, line, unit);
    if (unit.value == '\n') break;
  }
}

static bool here_document_end_line(
  struct Parser *parser,
  const struct Document *here,
  const struct Text *line,
  size_t *first,
  struct Text *bytes
) {
  *first = 0;
  if (here->strip)
    while (*first < line->count && line->units[*first].value == '\t')
      (*first)++;
  bytes->count = 0;
  for (size_t i = *first; i < line->count && line->units[i].value != '\n'; i++)
    encode(parser, bytes, line->units[i]);
  bool found = bytes->count == here->delimiter.count &&
    line->count &&
    line->units[line->count - 1].value == '\n';
  for (size_t i = 0; found && i < bytes->count; i++)
    found &= bytes->units[i].value == here->delimiter.units[i].value;
  return found;
}

static bool here_document_lines(
  struct Parser *parser,
  const struct Document *here,
  struct Text *body,
  bool *at_line_start,
  size_t *boundary
) {
  struct Input *input = &parser->input;
  struct Text line = {0}, bytes = {0};
  bool found = false;
  while (!found && input->position < input->count && !parser->plan->failed) {
    size_t line_start = input->units[input->position].start;
    line.count = 0;
    here_document_line(parser, here, &line);
    size_t first;
    found = here_document_end_line(parser, here, &line, &first, &bytes);
    *at_line_start = line.count && line.units[line.count - 1].value == '\n';
    if (found) *boundary = line_start;
    else
      for (size_t i = first; i < line.count; i++)
        text_push(parser, body, line.units[i]);
  }
  ts_free(line.units);
  ts_free(bytes.units);
  return found;
}

static bool here_document(
  struct Parser *parser,
  const struct Document *here,
  bool at_line_start,
  size_t body_start
) {
  struct Input *input = &parser->input;
  size_t owner = at_line_start
    ? owner_begin(parser, BEGIN_HERE_DOCUMENT_BODY, body_start)
    : 0;
  struct Text body = {0};
  size_t boundary = input->end;
  bool found =
    here_document_lines(parser, here, &body, &at_line_start, &boundary);
  if (!here->quoted) here_body(parser, body, boundary);
  ts_free(body.units);
  // An unstarted body has no owner, so its gap must not climb to an outer body.
  if (!found && !backquote_cut(parser, UNDEFINED_BACKQUOTE_HERE_DOCUMENT))
    report(
      parser,
      (struct Fault){HERE_DOCUMENT_ENDING_GAP, input->end, input->end, owner},
      (struct Recovery){input->end, false}
    );
  owner_end(parser, owner, consumed_offset(input));
  return at_line_start;
}

static void documents(struct Parser *parser, bool at_line_start) {
  struct Input *input = &parser->input;
  size_t body_start = at_line_start ? consumed_offset(input) : input->end;
  for (size_t document = 0; document < parser->document_count; document++) {
    struct Document *here = &parser->documents[document];
    if (here->uncertain) {
      extend_region(parser, here->uncertain - 1, input->end);
      input->position = input->count;
      break;
    }
    at_line_start = here_document(parser, here, at_line_start, body_start);
    body_start = consumed_offset(input);
  }
  for (size_t document = 0; document < parser->document_count; document++)
    ts_free(parser->documents[document].delimiter.units);
  parser->document_count = 0;
}

static void take(struct Parser *parser) {
  enum Lexeme kind = look(parser)->kind;
  if (parser->token.owner)
    parser->plan->owners[parser->token.owner].parent = parser->owner;
  parser->consumed = parser->token.end;
  parser->consumed_start = parser->token.start;
  ts_free(parser->token.text.units);
  parser->token = (struct Token){0};
  parser->ready = false;
  if (kind == NEWLINE && parser->document_count) documents(parser, true);
}

static bool list_boundary(enum Lexeme kind) {
  return kind == END ||
    kind == NEWLINE ||
    kind == SEMICOLON ||
    kind == AMPERSAND ||
    kind == PIPE ||
    kind == AND ||
    kind == OR;
}

enum Stop {
  STOP_RIGHT = 1u << 0,
  STOP_BRACE = 1u << 1,
  STOP_THEN = 1u << 2,
  STOP_ELIF = 1u << 3,
  STOP_ELSE = 1u << 4,
  STOP_FI = 1u << 5,
  STOP_DO = 1u << 6,
  STOP_DONE = 1u << 7,
  STOP_ESAC = 1u << 8,
  STOP_CASE_END = 1u << 9,
  STOP_IN = 1u << 10,
};

static bool spelling(const struct Token *token, const char *value) {
  if (token->kind != WORD || !token->plain) return false;
  size_t i = 0;
  for (; i < token->text.count && value[i]; i++)
    if (token->text.units[i].value != (unsigned char)value[i]) return false;
  return i == token->text.count && !value[i];
}

static bool reserved_colon(const struct Token *token) {
  return token->kind == WORD &&
    token->plain &&
    token->text.count > 1 &&
    token->text.units[token->text.count - 1].value == ':';
}

static const struct {
  const char *word;
  uint32_t stop;
  enum TokenType unexpected;
} keywords[] = {
  {"}", STOP_BRACE, UNMATCHED_BRACE},
  {"then", STOP_THEN, UNEXPECTED_THEN},
  {"elif", STOP_ELIF, UNEXPECTED_ELIF},
  {"else", STOP_ELSE, UNEXPECTED_ELSE},
  {"fi", STOP_FI, UNMATCHED_FI},
  {"do", STOP_DO, UNEXPECTED_DO},
  {"done", STOP_DONE, UNMATCHED_DONE},
  {"esac", STOP_ESAC, UNMATCHED_ESAC},
  {"in", STOP_IN, UNEXPECTED_IN},
};

static uint32_t stop(const struct Token *token) {
  if (token->kind == RIGHT) return STOP_RIGHT;
  if (token->kind == DSEMI || token->kind == SEMI_AND) return STOP_CASE_END;
  for (size_t i = 0; i < sizeof(keywords) / sizeof(*keywords); i++)
    if (spelling(token, keywords[i].word)) return keywords[i].stop;
  return 0;
}

static enum TokenType unexpected_fault(const struct Token *token) {
  switch (token->kind) {
    case RIGHT:
      return UNMATCHED_PARENTHESIS;
    case SEMICOLON:
      return UNEXPECTED_SEMICOLON;
    case AMPERSAND:
      return UNEXPECTED_AMPERSAND;
    case PIPE:
      return UNEXPECTED_PIPE;
    case AND:
      return UNEXPECTED_AND_IF;
    case OR:
      return UNEXPECTED_OR_IF;
    case DSEMI:
      return UNEXPECTED_CASE_SEPARATOR;
    case SEMI_AND:
      return UNEXPECTED_CASE_FALLTHROUGH;
    case WORD:
      if (reserved_colon(token)) return RESERVED_COLON_WORD;
      if (spelling(token, "!")) return UNEXPECTED_NEGATION;
      for (size_t i = 0; i < sizeof(keywords) / sizeof(*keywords); i++)
        if (spelling(token, keywords[i].word)) return keywords[i].unexpected;
      return TOKEN_TYPE_COUNT;
    default:
      return TOKEN_TYPE_COUNT;
  }
}

static const struct {
  const char *word;
  enum TokenType kind;
} starters[] = {
  {"{", BEGIN_BRACE_GROUP},
  {"if", BEGIN_IF_CLAUSE},
  {"while", BEGIN_WHILE_CLAUSE},
  {"until", BEGIN_UNTIL_CLAUSE},
  {"for", BEGIN_FOR_CLAUSE},
  {"case", BEGIN_CASE_CLAUSE},
};

static enum TokenType compound_kind(const struct Token *token) {
  if (token->kind == LEFT) return BEGIN_SUBSHELL;
  for (size_t i = 0; i < sizeof(starters) / sizeof(*starters); i++)
    if (spelling(token, starters[i].word)) return starters[i].kind;
  return TOKEN_TYPE_COUNT;
}

static bool compound_start(const struct Token *token) {
  return compound_kind(token) != TOKEN_TYPE_COUNT;
}

static bool redirect_kind(enum Lexeme kind) {
  return kind == IO_NUMBER ||
    kind == REDIRECT ||
    kind == HERE ||
    kind == HERE_STRIP;
}

static void compound(struct Parser *parser);
static void compound_command(struct Parser *parser);

static bool command_start(const struct Token *token) {
  return compound_start(token) ||
    redirect_kind(token->kind) ||
    (token->kind == WORD && !stop(token));
}

static bool simple_command_start(const struct Token *token) {
  return (token->kind == WORD || redirect_kind(token->kind)) &&
    !compound_start(token) &&
    !stop(token) &&
    !spelling(token, "!") &&
    !reserved_colon(token);
}

static bool stop_or_end(struct Parser *parser, uint32_t stops) {
  const struct Token *token = look(parser);
  return token->kind == END || (stop(token) & (stops | parser->stops));
}

// Decode faults precede word faults and are sorted by start.
static size_t uncertain_position(
  const struct Plan *plan,
  const struct Capture *capture,
  size_t start,
  size_t end
) {
  size_t low = 0, high = plan->decode_issues;
  while (low < high) {
    size_t middle = low + (high - low) / 2;
    if (plan->issues[middle].start < start) low = middle + 1;
    else high = middle;
  }
  size_t uncertain = capture->uncertain;
  if (
    low < plan->decode_issues &&
    plan->issues[low].start < end &&
    (!uncertain || plan->issues[low].start < uncertain - 1)
  )
    uncertain = plan->issues[low].start + 1;
  return uncertain;
}

static bool declare_here_document(
  struct Parser *parser,
  bool strip,
  const struct Capture *capture,
  size_t begin,
  const struct Token *target
) {
  struct Document *grown = diagnostic_grow(
    parser->documents,
    &parser->document_capacity,
    parser->document_count + 1,
    sizeof(*grown)
  );
  if (!grown) {
    parser->plan->failed = true;
    return false;
  }
  parser->documents = grown;
  begin = unit_after(
    parser->input.units,
    begin,
    parser->input.position,
    target->start,
    true
  );
  parser->documents[parser->document_count++] = (struct Document){
    .delimiter =
      captured_delimiter(parser, capture, begin, parser->input.position),
    .quoted = capture->quoted,
    .strip = strip,
    .uncertain =
      uncertain_position(parser->plan, capture, target->start, target->end)
  };
  return true;
}

enum RecoveryBoundary {
  RECOVER_CONNECTORS = 1u << 0,
  RECOVER_REDIRECTIONS = 1u << 1,
};

static bool redirect_operand(struct Parser *parser, enum Lexeme kind) {
  bool here = kind == HERE || kind == HERE_STRIP;
  struct Capture capture = {.parent = parser->input.capture};
  size_t begin = parser->input.position;
  bool quoted_delimiter = parser->quoted_delimiter;
  if (here) {
    parser->input.capture = &capture;
    parser->quoted_delimiter = false;
  }
  const struct Token *target = look(parser);
  parser->quoted_delimiter = quoted_delimiter;
  parser->input.capture = capture.parent;
  parser->plan->failed |= capture.failed;
  bool word = target->kind == WORD;
  if (
    word &&
    (!here ||
      declare_here_document(
        parser,
        kind == HERE_STRIP,
        &capture,
        begin,
        target
      ))
  )
    take(parser);
  ts_free(capture.changes);
  return word;
}

static enum Lexeme redirect_operator(struct Parser *parser) {
  if (look(parser)->kind == IO_NUMBER) take(parser);
  enum Lexeme kind = look(parser)->kind;
  take(parser);
  return kind;
}

static void skip(struct Parser *parser) {
  if (redirect_kind(look(parser)->kind))
    redirect_operand(parser, redirect_operator(parser));
  else take(parser);
}

static struct Recovery resume_at(struct Parser *parser) {
  const struct Token *token = look(parser);
  return (struct Recovery){token->start, token->kind != END};
}

static bool recovery_boundary(struct Parser *parser, unsigned boundaries) {
  const struct Token *token = look(parser);
  if (list_boundary(token->kind))
    return (boundaries & RECOVER_CONNECTORS) ||
      (token->kind != PIPE && token->kind != AND && token->kind != OR);
  return stop_or_end(parser, 0) ||
    compound_start(token) ||
    ((boundaries & RECOVER_REDIRECTIONS) && redirect_kind(token->kind));
}

static struct Recovery recover(struct Parser *parser, unsigned boundaries) {
  bool opaque = parser->opaque;
  parser->opaque = true;
  while (!parser->plan->failed && !recovery_boundary(parser, boundaries))
    skip(parser);
  parser->opaque = opaque;
  parser->resumed =
    !((boundaries & RECOVER_REDIRECTIONS) && redirect_kind(look(parser)->kind));
  return resume_at(parser);
}

static void invalid_target(struct Parser *parser, enum Lexeme kind) {
  bool here = kind == HERE || kind == HERE_STRIP;
  const struct Token *target = look(parser);
  bool missing = list_boundary(target->kind) ||
    redirect_kind(target->kind) ||
    stop_or_end(parser, 0);
  struct Fault fault = token_fault(
    parser,
    missing,
    here ? HERE_DOCUMENT_DELIMITER_GAP : REDIRECTION_TARGET_GAP,
    here ? INVALID_HERE_DOCUMENT_DELIMITER : INVALID_REDIRECTION_TARGET
  );
  if (!missing) {
    bool opaque = parser->opaque;
    parser->opaque = true;
    if (target->kind == LEFT) compound_command(parser);
    else take(parser);
    parser->opaque = opaque;
  }
  report(
    parser,
    fault,
    recover(parser, RECOVER_CONNECTORS | RECOVER_REDIRECTIONS)
  );
}

static void redirect_contents(struct Parser *parser) {
  enum Lexeme kind = redirect_operator(parser);
  if (!redirect_operand(parser, kind)) invalid_target(parser, kind);
}

static void redirect(struct Parser *parser) {
  size_t owner = owner_begin(parser, BEGIN_IO_REDIRECT, look(parser)->start);
  redirect_contents(parser);
  owner_end(parser, owner, parser->consumed);
}

static void linebreak(struct Parser *parser) {
  while (!parser->plan->failed && look(parser)->kind == NEWLINE) take(parser);
}

static struct Recovery seek(struct Parser *parser, uint32_t stops) {
  bool opaque = parser->opaque;
  parser->opaque = true;
  while (!parser->plan->failed && !stop_or_end(parser, stops))
    if (compound_start(look(parser))) compound(parser);
    else skip(parser);
  parser->opaque = opaque;
  parser->resumed = true;
  return resume_at(parser);
}

static void report_token(
  struct Parser *parser,
  bool missing,
  enum TokenType gap,
  enum TokenType invalid,
  uint32_t stops
) {
  struct Fault fault = token_fault(parser, missing, gap, invalid);
  report(parser, fault, seek(parser, stops));
}

static enum TokenType reserved_or(struct Parser *parser, enum TokenType kind) {
  return reserved_colon(look(parser)) ? RESERVED_COLON_WORD : kind;
}

static bool take_stop(struct Parser *parser, uint32_t wanted) {
  if (!(stop(look(parser)) & wanted)) return false;
  take(parser);
  parser->resumed = false;
  return true;
}

static bool expect_stop(
  struct Parser *parser,
  uint32_t wanted,
  uint32_t alternatives,
  enum TokenType gap,
  enum TokenType invalid
) {
  if (!(stop(look(parser)) & wanted))
    report_token(
      parser,
      invalid == TOKEN_TYPE_COUNT || stop_or_end(parser, alternatives),
      gap,
      invalid,
      wanted | alternatives
    );
  return take_stop(parser, wanted);
}

static bool commands(
  struct Parser *parser,
  enum TokenType list,
  uint32_t stops,
  bool required
);
static void command(struct Parser *parser);

static void else_part(struct Parser *parser) {
  owner_begin(parser, BEGIN_ELSE_PART, look(parser)->start);
  take(parser);
}

static void conditional(struct Parser *parser) {
  size_t owner = parser->owner;
  take(parser);
  for (;;) {
    commands(
      parser,
      BEGIN_COMPOUND_LIST,
      STOP_THEN | STOP_ELIF | STOP_ELSE | STOP_FI,
      true
    );
    if (
      expect_stop(
        parser,
        STOP_THEN,
        STOP_ELIF | STOP_ELSE | STOP_FI,
        THEN_GAP,
        TOKEN_TYPE_COUNT
      )
    )
      commands(
        parser,
        BEGIN_COMPOUND_LIST,
        STOP_ELIF | STOP_ELSE | STOP_FI,
        true
      );
    if (!(stop(look(parser)) & STOP_ELIF)) break;
    else_part(parser);
  }
  if (stop(look(parser)) & STOP_ELSE) {
    else_part(parser);
    commands(parser, BEGIN_COMPOUND_LIST, STOP_FI, true);
  }
  while (parser->owner != owner)
    owner_end(parser, parser->owner, parser->consumed);
  expect_stop(parser, STOP_FI, 0, FI_GAP, TOKEN_TYPE_COUNT);
}

static void loop_body(struct Parser *parser, enum TokenType invalid) {
  if (expect_stop(parser, STOP_DO, STOP_DONE, DO_GAP, invalid)) {
    size_t owner = owner_begin(parser, BEGIN_DO_GROUP, parser->consumed_start);
    commands(parser, BEGIN_COMPOUND_LIST, STOP_DONE, true);
    expect_stop(parser, STOP_DONE, 0, DONE_GAP, TOKEN_TYPE_COUNT);
    owner_end(parser, owner, parser->consumed);
  } else take_stop(parser, STOP_DONE);
}

static bool for_name(struct Parser *parser) {
  const struct Token *token = look(parser);
  if (token->name) {
    take(parser);
    return false;
  }
  bool missing = token->kind == NEWLINE ||
    token->kind == SEMICOLON ||
    stop_or_end(parser, 0);
  struct Fault fault =
    token_fault(parser, missing, FOR_NAME_GAP, INVALID_FOR_NAME);
  if (token->name_fault.end) {
    fault.at = token->name_fault.start;
    fault.end = missing ? fault.at : token->name_fault.end;
  }
  report(parser, fault, seek(parser, STOP_IN | STOP_DO | STOP_DONE));
  return true;
}

static bool for_words(struct Parser *parser) {
  take(parser);
  while (!parser->plan->failed && look(parser)->kind == WORD) take(parser);
  if (look(parser)->kind == SEMICOLON || look(parser)->kind == NEWLINE)
    return false;
  report_token(
    parser,
    stop_or_end(parser, 0),
    FOR_SEPARATOR_GAP,
    INVALID_FOR_SEPARATOR,
    STOP_DO | STOP_DONE
  );
  return true;
}

static void for_clause(struct Parser *parser) {
  take(parser);
  bool damaged = for_name(parser);
  bool newline = look(parser)->kind == NEWLINE;
  linebreak(parser);
  if (stop(look(parser)) & STOP_IN) {
    damaged = for_words(parser);
    newline = false;
  } else if (
    look(parser)->kind != SEMICOLON && !stop_or_end(parser, STOP_DO | STOP_DONE)
  ) {
    report_token(
      parser,
      false,
      TOKEN_TYPE_COUNT,
      reserved_or(parser, INVALID_FOR_HEADER),
      STOP_DO | STOP_DONE
    );
    damaged = true;
  }
  if (!newline && look(parser)->kind == SEMICOLON) take(parser);
  linebreak(parser);
  if (damaged && !(stop(look(parser)) & STOP_DO)) {
    take_stop(parser, STOP_DONE);
    return;
  }
  loop_body(parser, reserved_or(parser, INVALID_DO));
}

static bool case_header(struct Parser *parser, struct Fault fault) {
  report(parser, fault, seek(parser, STOP_IN | STOP_ESAC));
  if (stop(look(parser)) & STOP_IN) return true;
  take_stop(parser, STOP_ESAC);
  return false;
}

static bool case_patterns(struct Parser *parser, size_t *item) {
  for (;;) {
    if (look(parser)->kind == WORD) {
      if (!*item)
        *item = owner_begin(parser, BEGIN_CASE_ITEM, look(parser)->start);
      take(parser);
    } else if (look(parser)->kind == PIPE) {
      size_t at = look(parser)->start;
      issue(parser, CASE_PATTERN_GAP, at, at, (struct Recovery){at, true});
    } else {
      uint32_t stops = STOP_RIGHT | STOP_CASE_END | STOP_ESAC;
      report_token(
        parser,
        stop_or_end(parser, stops),
        CASE_PATTERN_GAP,
        INVALID_CASE_PATTERN,
        stops
      );
      return true;
    }
    if (look(parser)->kind != PIPE) return false;
    take(parser);
  }
}

static bool case_item(struct Parser *parser) {
  size_t item = 0;
  if (look(parser)->kind == LEFT) {
    item = owner_begin(parser, BEGIN_CASE_ITEM, look(parser)->start);
    take(parser);
  }
  bool closed;
  if (case_patterns(parser, &item)) closed = take_stop(parser, STOP_RIGHT);
  else
    closed = expect_stop(
      parser,
      STOP_RIGHT,
      STOP_CASE_END | STOP_ESAC,
      PARENTHESIS_GAP,
      CASE_PATTERN_SEPARATOR_ERROR
    );
  if (closed)
    commands(parser, BEGIN_COMPOUND_LIST, STOP_CASE_END | STOP_ESAC, false);
  bool terminated =
    look(parser)->kind == DSEMI || look(parser)->kind == SEMI_AND;
  if (terminated) take(parser);
  owner_end(parser, item, parser->consumed);
  if (terminated) linebreak(parser);
  else if (closed)
    expect_stop(parser, STOP_ESAC, 0, ESAC_GAP, TOKEN_TYPE_COUNT);
  else take_stop(parser, STOP_ESAC);
  return terminated;
}

static void orphan_case_separator(struct Parser *parser) {
  struct Fault fault = token_fault(
    parser,
    false,
    TOKEN_TYPE_COUNT,
    unexpected_fault(look(parser))
  );
  take(parser);
  report(parser, fault, (struct Recovery){parser->consumed, true});
  linebreak(parser);
}

static bool case_item_start(struct Parser *parser) {
  const struct Token *token = look(parser);
  return token->kind != END &&
    !(stop(token) & STOP_ESAC) &&
    (token->kind == WORD ||
      (stop(token) & STOP_CASE_END) ||
      !(stop(token) & parser->stops));
}

static void case_clause(struct Parser *parser) {
  take(parser);
  if (look(parser)->kind == WORD) take(parser);
  else {
    bool missing = look(parser)->kind == NEWLINE || stop_or_end(parser, 0);
    struct Fault fault =
      token_fault(parser, missing, CASE_WORD_GAP, INVALID_CASE_WORD);
    if (!case_header(parser, fault)) return;
  }
  linebreak(parser);
  if (reserved_colon(look(parser))) {
    struct Fault fault =
      token_fault(parser, false, TOKEN_TYPE_COUNT, RESERVED_COLON_WORD);
    if (!case_header(parser, fault)) return;
  }
  if (!expect_stop(parser, STOP_IN, STOP_ESAC, IN_GAP, INVALID_IN)) {
    take_stop(parser, STOP_ESAC);
    return;
  }
  linebreak(parser);
  while (!parser->plan->failed && case_item_start(parser))
    if (stop(look(parser)) & STOP_CASE_END) orphan_case_separator(parser);
    else if (!case_item(parser)) return;
  expect_stop(parser, STOP_ESAC, 0, ESAC_GAP, TOKEN_TYPE_COUNT);
}

static void compound_command(struct Parser *parser) {
  if (!nest(parser)) return;
  const struct Token *token = look(parser);
  enum TokenType kind = compound_kind(token);
  size_t owner = owner_begin(parser, kind, token->start);
  switch (kind) {
    case BEGIN_SUBSHELL:
      take(parser);
      commands(parser, BEGIN_COMPOUND_LIST, STOP_RIGHT, true);
      expect_stop(parser, STOP_RIGHT, 0, PARENTHESIS_GAP, TOKEN_TYPE_COUNT);
      break;
    case BEGIN_BRACE_GROUP:
      take(parser);
      commands(parser, BEGIN_COMPOUND_LIST, STOP_BRACE, true);
      expect_stop(parser, STOP_BRACE, 0, BRACE_GAP, TOKEN_TYPE_COUNT);
      break;
    case BEGIN_IF_CLAUSE:
      conditional(parser);
      break;
    case BEGIN_FOR_CLAUSE:
      for_clause(parser);
      break;
    case BEGIN_CASE_CLAUSE:
      case_clause(parser);
      break;
    default:
      take(parser);
      commands(parser, BEGIN_COMPOUND_LIST, STOP_DO | STOP_DONE, true);
      loop_body(parser, TOKEN_TYPE_COUNT);
  }
  owner_end(parser, owner, parser->consumed);
  unnest(parser);
}

static void compound(struct Parser *parser) {
  compound_command(parser);
  while (!parser->plan->failed && redirect_kind(look(parser)->kind))
    redirect(parser);
}

static void unexpected(struct Parser *parser) {
  enum TokenType kind = unexpected_fault(look(parser));
  if (kind == TOKEN_TYPE_COUNT) {
    parser->plan->failed = true;
    return;
  }
  struct Fault fault = token_fault(parser, false, TOKEN_TYPE_COUNT, kind);
  bool separator =
    look(parser)->kind == SEMICOLON || look(parser)->kind == AMPERSAND;
  take(parser);
  struct Recovery recovery =
    separator ? (struct Recovery){fault.end, true} : recover(parser, 0);
  parser->resumed = true;
  report(parser, fault, recovery);
}

static void function_definition(struct Parser *parser) {
  if (parser->owner && !parser->opaque)
    parser->plan->owners[parser->owner].kind = BEGIN_FUNCTION_DEFINITION;
  take(parser);
  bool damaged = false;
  if (look(parser)->kind != RIGHT && !stop_or_end(parser, 0)) {
    report_token(
      parser,
      false,
      TOKEN_TYPE_COUNT,
      FUNCTION_PARAMETER_ERROR,
      STOP_RIGHT
    );
    damaged = true;
  }
  bool closed = damaged
    ? take_stop(parser, STOP_RIGHT)
    : expect_stop(parser, STOP_RIGHT, 0, PARENTHESIS_GAP, TOKEN_TYPE_COUNT);
  if (!closed) return;
  linebreak(parser);
  if (compound_start(look(parser))) {
    compound(parser);
    return;
  }
  bool missing = list_boundary(look(parser)->kind) || stop_or_end(parser, 0);
  struct Fault fault = token_fault(
    parser,
    missing,
    FUNCTION_BODY_GAP,
    reserved_or(parser, INVALID_FUNCTION_BODY)
  );
  report(parser, fault, recover(parser, RECOVER_CONNECTORS));
}

static void command_contents(struct Parser *parser, bool simple) {
  if (compound_start(look(parser))) {
    compound(parser);
    return;
  }
  if (!simple) {
    unexpected(parser);
    return;
  }
  if (look(parser)->kind == WORD) {
    bool name = look(parser)->name;
    take(parser);
    if (name && look(parser)->kind == LEFT) {
      function_definition(parser);
      return;
    }
  } else redirect(parser);
  while (!parser->plan->failed && !parser->resumed)
    if (look(parser)->kind == WORD) take(parser);
    else if (redirect_kind(look(parser)->kind)) redirect(parser);
    else break;
}

static void command(struct Parser *parser) {
  const struct Token *token = look(parser);
  bool simple = simple_command_start(token);
  size_t owner =
    simple ? owner_begin(parser, BEGIN_SIMPLE_COMMAND, token->start) : 0;
  command_contents(parser, simple);
  owner_end(parser, owner, parser->consumed);
}

static bool command_boundary(struct Parser *parser) {
  return list_boundary(look(parser)->kind) || stop_or_end(parser, 0);
}

static void missing_command(struct Parser *parser) {
  size_t at = look(parser)->start;
  issue(parser, COMMAND_GAP, at, at, recover(parser, 0));
}

static void negation(struct Parser *parser) {
  size_t start = look(parser)->start;
  size_t bang_end = look(parser)->text.units[0].end;
  take(parser);
  size_t next = offset(&parser->input);
  if (look(parser)->kind == LEFT && look(parser)->start == next)
    issue(
      parser,
      UNSPECIFIED_NEGATION_SPACING,
      start,
      bang_end,
      (struct Recovery){look(parser)->start, true}
    );
}

static void pipeline_contents(struct Parser *parser) {
  if (spelling(look(parser), "!")) negation(parser);
  if (command_boundary(parser)) {
    missing_command(parser);
    return;
  }
  command(parser);
  while (!parser->plan->failed && look(parser)->kind == PIPE) {
    take(parser);
    linebreak(parser);
    if (command_boundary(parser)) {
      missing_command(parser);
      return;
    }
    parser->resumed = false;
    command(parser);
  }
}

static void pipeline(struct Parser *parser) {
  size_t owner = 0;
  if (command_start(look(parser)))
    owner = owner_begin(parser, BEGIN_PIPELINE, look(parser)->start);
  pipeline_contents(parser);
  owner_end(parser, owner, parser->consumed);
}

static void and_or(struct Parser *parser) {
  size_t owner = 0;
  if (command_start(look(parser)))
    owner = owner_begin(parser, BEGIN_AND_OR, look(parser)->start);
  parser->resumed = false;
  if (list_boundary(look(parser)->kind)) unexpected(parser);
  else {
    pipeline(parser);
    while (
      !parser->plan->failed &&
      (look(parser)->kind == AND || look(parser)->kind == OR)
    ) {
      take(parser);
      linebreak(parser);
      parser->resumed = false;
      pipeline(parser);
    }
  }
  owner_end(parser, owner, parser->consumed);
}

static void command_separation(struct Parser *parser) {
  const struct Token *token = look(parser);
  if (command_boundary(parser) || parser->resumed) return;
  if (compound_start(token) || (token->kind == WORD && !stop(token)))
    issue(
      parser,
      SEPARATOR_GAP,
      token->start,
      token->start,
      (struct Recovery){token->start, true}
    );
  else unexpected(parser);
}

static bool commands(
  struct Parser *parser,
  enum TokenType list,
  uint32_t stops,
  bool required
) {
  uint32_t parent = parser->stops;
  parser->stops |= stops;
  bool any = false;
  size_t owner = 0;
  linebreak(parser);
  while (!parser->plan->failed && !stop_or_end(parser, 0)) {
    any = true;
    if (!owner && command_start(look(parser)))
      owner = owner_begin(parser, list, look(parser)->start);
    and_or(parser);
    command_separation(parser);
    if (stop_or_end(parser, 0)) break;
    enum Lexeme kind = look(parser)->kind;
    if (kind == SEMICOLON || kind == AMPERSAND) take(parser);
    if (look(parser)->kind == NEWLINE) {
      if (list == BEGIN_LIST) {
        owner_end(parser, owner, parser->consumed);
        owner = 0;
      }
      linebreak(parser);
    }
  }
  owner_end(parser, owner, parser->consumed);
  if (required && !any) {
    size_t at = look(parser)->start;
    issue(parser, COMMAND_GAP, at, at, resume_at(parser));
  }
  parser->stops = parent;
  return any;
}

static void parse(struct Parser *parser) {
  commands(parser, BEGIN_LIST, 0, false);
  if (parser->document_count) documents(parser, false);
}

static void clear_parser(struct Parser *parser) {
  ts_free(parser->token.text.units);
  for (size_t i = 0; i < parser->document_count; i++)
    ts_free(parser->documents[i].delimiter.units);
  ts_free(parser->documents);
}

static void parse_contents(
  struct Parser *parser,
  struct Input input,
  size_t backquote_end
) {
  struct Parser child = child_parser(parser, input);
  child.backquote_end = backquote_end;
  commands(&child, BEGIN_LIST, 0, false);
  if (!backquote_end) input_end_gap(&child, BACKQUOTE_GAP);
  else if (child.document_count)
    issue(
      &child,
      UNSPECIFIED_HERE_DOCUMENT_START,
      input.end,
      backquote_end,
      (struct Recovery){backquote_end, true}
    );
  if (child.document_count && !child.input.capture && !backquote_end)
    documents(&child, false);
  clear_parser(&child);
}

static void substitution(struct Parser *parser) {
  struct Parser child = child_parser(parser, parser->input);
  commands(&child, BEGIN_LIST, STOP_RIGHT, false);
  bool closed = look(&child)->kind == RIGHT;
  if (closed) {
    if (child.document_count)
      issue(
        &child,
        UNSPECIFIED_HERE_DOCUMENT_START,
        child.token.start,
        child.token.end,
        (struct Recovery){child.token.end, true}
      );
    take(&child);
  } else
    unclosed(&child, UNDEFINED_BACKQUOTE_SUBSTITUTION, SUBSTITUTION_ENDING_GAP);
  if (child.document_count && !child.input.capture && !closed)
    documents(&child, false);
  parser->input = child.input;
  clear_parser(&child);
}

static int compare_issues(const void *left, const void *right) {
  const struct Issue *a = left, *b = right;
  return a->start < b->start ? -1 : a->start != b->start;
}

static int compare_extensions(const void *left, const void *right) {
  const struct Extension *a = left, *b = right;
  return a->position < b->position ? -1 : a->position != b->position;
}

static bool event(struct Scanner *scanner, enum TokenType kind, size_t end) {
  struct Event *grown = diagnostic_grow(
    scanner->events,
    &scanner->capacity,
    scanner->count + 1,
    sizeof(*grown)
  );
  if (!grown) return false;
  scanner->events = grown;
  scanner->events[scanner->count++] = (struct Event){kind, end};
  return true;
}

struct PublicChild {
  size_t parent, index, start, end;
  bool issue;
};

static int compare_children(const void *left, const void *right) {
  const struct PublicChild *a = left, *b = right;
  if (a->parent != b->parent) return a->parent < b->parent ? -1 : 1;
  if (a->start != b->start) return a->start < b->start ? -1 : 1;
  if (a->end != b->end) return a->end < b->end ? -1 : 1;
  if (a->issue != b->issue) return a->issue ? -1 : 1;
  return a->index < b->index ? -1 : a->index != b->index;
}

static void arrange_children(
  struct Plan *plan,
  struct PublicChild *children,
  size_t count
) {
  if (count > 1) qsort(children, count, sizeof(*children), compare_children);
  for (size_t i = 0; i < plan->owner_count; i++)
    plan->owners[i].first = plan->owners[i].limit = 0;
  for (size_t i = 0; i < count; i++) {
    struct Owner *parent = &plan->owners[children[i].parent];
    if (!i || children[i - 1].parent != children[i].parent) parent->first = i;
    parent->limit = i + 1;
  }
}

struct PublishFrame {
  size_t id, child, position;
};

static struct PublishFrame publish_frame(const struct Plan *plan, size_t id) {
  return (
    struct PublishFrame
  ){id, plan->owners[id].first, plan->owners[id].start};
}

static bool end_prefixes(
  struct Plan *plan,
  struct PublicChild *children,
  struct PublishFrame *frames,
  size_t end
) {
  if (plan->owners[0].end > end) plan->owners[0].end = end;
  frames[0] = publish_frame(plan, 0);
  for (size_t depth = 0;;) {
    struct PublishFrame *frame = &frames[depth];
    struct Owner *owner = &plan->owners[frame->id];
    if (frame->child == owner->limit) {
      if (!depth) return true;
      struct PublishFrame *parent = &frames[--depth];
      struct PublicChild *child = &children[parent->child++];
      child->end = owner->end;
      if (child->end > plan->owners[parent->id].end) return false;
      parent->position = child->end;
      continue;
    }
    struct PublicChild *child = &children[frame->child];
    if (child->start < frame->position) return false;
    if (child->issue) {
      if (child->end > owner->end) return false;
      frame->position = child->end;
      frame->child++;
      continue;
    }
    size_t boundary = owner->end;
    if (
      frame->child + 1 < owner->limit &&
      children[frame->child + 1].issue &&
      children[frame->child + 1].start < boundary
    )
      boundary = children[frame->child + 1].start;
    if (child->start > boundary) return false;
    struct Owner *next = &plan->owners[child->index];
    if (next->end > boundary) next->end = boundary;
    frames[++depth] = publish_frame(plan, child->index);
  }
}

static bool recovery_eof(struct Recovery recovery, size_t end) {
  return recovery.position == end && !recovery.complete;
}

static bool
emit_issue(struct Scanner *scanner, const struct Issue *issue, size_t end) {
  return event(
           scanner,
           issue->owner ? BEGIN_ISSUE : BEGIN_UNOWNED_ISSUE,
           issue->start
         ) &&
    event(scanner, issue->kind, issue->end) &&
    (issue->recovery.position <= issue->end ||
      event(scanner, RECOVERY_FRAGMENT, issue->recovery.position)) &&
    event(
      scanner,
      recovery_eof(issue->recovery, end) ? RECOVERY_EOF : RECOVERY_RESUME,
      issue->recovery.position
    );
}

static bool emit_owner(
  struct Scanner *scanner,
  const struct Plan *plan,
  const struct PublicChild *children,
  struct PublishFrame *frames,
  size_t end
) {
  frames[0] = publish_frame(plan, 0);
  for (size_t depth = 0;;) {
    struct PublishFrame *frame = &frames[depth];
    const struct Owner *owner = &plan->owners[frame->id];
    if (frame->child == owner->limit) {
      if (
        (frame->position < owner->end && !event(scanner, SOURCE, owner->end)) ||
        !event(scanner, frame->id ? END_OWNER : FINISH, owner->end)
      )
        return false;
      if (!depth) return true;
      struct PublishFrame *parent = &frames[--depth];
      parent->position = children[parent->child++].end;
      continue;
    }
    const struct PublicChild *child = &children[frame->child];
    if (child->start < frame->position || child->end > owner->end) return false;
    if (child->start > frame->position && !event(scanner, SOURCE, child->start))
      return false;
    if (child->issue) {
      if (!emit_issue(scanner, &plan->issues[child->index], end)) return false;
      frame->position = child->end;
      frame->child++;
      continue;
    }
    const struct Owner *next = &plan->owners[child->index];
    if (!event(scanner, next->kind, next->start)) return false;
    frames[++depth] = publish_frame(plan, child->index);
  }
}

static bool cut_by_region(
  const struct Plan *plan,
  size_t owner,
  size_t region,
  size_t start,
  size_t resume
) {
  size_t child = 0;
  for (size_t id = owner; id; child = id, id = plan->owners[id].parent) {
    if (id == region) return child && plan->owners[child].start < resume;
    if (plan->owners[id].start > start && plan->owners[id].start < resume)
      return true;
  }
  return false;
}

static size_t owner_depths(const struct Plan *plan, size_t *depths) {
  size_t deepest = 1;
  depths[0] = 1;
  for (size_t i = 1; i < plan->owner_count; i++) {
    size_t known = i, length = 0;
    while (!depths[known]) {
      known = plan->owners[known].parent;
      length++;
    }
    size_t depth = depths[known] + length;
    if (depth > deepest) deepest = depth;
    for (size_t id = i; id != known; id = plan->owners[id].parent)
      depths[id] = depth--;
  }
  return deepest;
}

static bool later_child(const struct Plan *plan, size_t a, size_t b) {
  const struct Owner *x = &plan->owners[a], *y = &plan->owners[b];
  if (x->parent != y->parent) return x->parent > y->parent;
  if (x->start != y->start) return x->start > y->start;
  if (x->end != y->end) return x->end > y->end;
  return a > b;
}

// Sibling starts and ends are ordered, allowing backward search to stop early.
static size_t place_unowned(
  const struct Plan *plan,
  const struct PublicChild *children,
  const size_t *depths,
  size_t *stack,
  const struct Issue *issue
) {
  size_t best = 0, best_depth = 0, count = 0;
  stack[count++] = 0;
  while (count) {
    const struct Owner *owner = &plan->owners[stack[--count]];
    size_t low = owner->first, high = owner->limit;
    while (low < high) {
      size_t middle = low + (high - low) / 2;
      if (children[middle].start <= issue->start) low = middle + 1;
      else high = middle;
    }
    for (size_t i = low; i > owner->first;) {
      const struct PublicChild *child = &children[--i];
      if (child->issue) continue;
      if (plan->owners[child->index].end < issue->recovery.position) break;
      size_t depth = depths[child->index] - 1;
      if (
        depth > best_depth ||
        (depth == best_depth && later_child(plan, child->index, best))
      ) {
        best = child->index;
        best_depth = depth;
      }
      stack[count++] = child->index;
    }
  }
  return best;
}

static unsigned requirement_order(enum TokenType kind) {
  switch (kind) {
    case INVALID_FOR_HEADER:
    case FOR_SEPARATOR_GAP:
    case INVALID_FOR_SEPARATOR:
    case THEN_GAP:
    case IN_GAP:
    case INVALID_IN:
    case PARAMETER_OPERATOR_GAP:
    case PARAMETER_OPERATOR_ERROR:
    case PARAMETER_LENGTH_SUFFIX_ERROR:
      return 1;
    case DO_GAP:
    case INVALID_DO:
    case CASE_PATTERN_GAP:
    case INVALID_CASE_PATTERN:
    case PARAMETER_WORD_GAP:
    case UNSPECIFIED_PARAMETER_QUOTE:
      return 2;
    case FI_GAP:
    case DONE_GAP:
    case ESAC_GAP:
    case CASE_PATTERN_SEPARATOR_ERROR:
    case PARENTHESIS_GAP:
    case BRACE_GAP:
    case SINGLE_QUOTE_GAP:
    case DOUBLE_QUOTE_GAP:
    case DOLLAR_QUOTE_GAP:
    case PARAMETER_ENDING_GAP:
    case ARITHMETIC_ENDING_GAP:
    case SUBSTITUTION_ENDING_GAP:
    case BACKQUOTE_GAP:
    case HERE_DOCUMENT_ENDING_GAP:
      return 3;
    case UNSPECIFIED_HERE_DOCUMENT_START:
    case FUNCTION_BODY_GAP:
    case INVALID_FUNCTION_BODY:
      return 4;
    default:
      return 0;
  }
}

static bool ancestor(const struct Plan *plan, size_t outer, size_t inner) {
  for (size_t id = plan->owners[inner].parent; id; id = plan->owners[id].parent)
    if (id == outer) return true;
  return false;
}

static bool input_fault(enum TokenType kind) {
  return kind == INVALID_ENCODING || kind == NUL_CHARACTER;
}

static bool replaces_issue(
  const struct Plan *plan,
  const struct Issue *candidate,
  const struct Issue *current
) {
  bool current_input = input_fault(current->kind);
  bool candidate_input = input_fault(candidate->kind);
  if (current_input != candidate_input) return candidate_input;
  if (!!candidate->owner != !!current->owner) return candidate->owner != 0;
  if (candidate->owner && current->owner) {
    if (ancestor(plan, candidate->owner, current->owner))
      return candidate->start != candidate->end;
    if (ancestor(plan, current->owner, candidate->owner))
      return current->start == current->end;
  }
  if (candidate->context_start != current->context_start)
    return candidate->context_start < current->context_start;
  unsigned candidate_order = requirement_order(candidate->kind);
  unsigned current_order = requirement_order(current->kind);
  if (candidate_order != current_order) return candidate_order < current_order;
  bool candidate_gap = candidate->start == candidate->end;
  bool current_gap = current->start == current->end;
  if (candidate_gap != current_gap) return !candidate_gap;
  if (candidate->end != current->end) return candidate->end < current->end;
  if (candidate->recovery.position != current->recovery.position)
    return candidate->recovery.position < current->recovery.position;
  return candidate->recovery.complete && !current->recovery.complete;
}

// A completed recovery leaves its boundary available to independent faults.
static bool covered(
  const struct Plan *plan,
  const struct Issue *candidate,
  const struct Issue *previous,
  size_t end
) {
  if (!previous) return false;
  size_t position = previous->recovery.position;
  if (candidate->start < position) return true;
  if (candidate->start != position) return false;
  return recovery_eof(previous->recovery, end) ||
    cut_by_region(
      plan,
      candidate->owner,
      previous->owner,
      previous->start,
      position
    );
}

static size_t select_regions(struct Plan *plan, size_t end) {
  if (plan->issue_count > 1)
    qsort(
      plan->issues,
      plan->issue_count,
      sizeof(*plan->issues),
      compare_issues
    );
  if (plan->extension_count > 1)
    qsort(
      plan->extensions,
      plan->extension_count,
      sizeof(*plan->extensions),
      compare_extensions
    );
  size_t count = 0, input = 0, extension = 0;
  for (size_t i = 0; i < plan->issue_count;) {
    if (input < i) input = i;
    while (input < plan->issue_count && !input_fault(plan->issues[input].kind))
      input++;
    size_t start = plan->issues[i].start;
    struct Issue current = {0};
    bool found = false;
    for (; i < plan->issue_count && plan->issues[i].start == start; i++) {
      const struct Issue *candidate = &plan->issues[i];
      if (
        !input_fault(candidate->kind) &&
        input < plan->issue_count &&
        plan->issues[input].start < candidate->end
      )
        continue;
      if (
        covered(plan, candidate, count ? &plan->issues[count - 1] : NULL, end)
      )
        continue;
      if (!found || replaces_issue(plan, candidate, &current)) {
        current = *candidate;
        found = true;
      }
    }
    if (!found) continue;
    for (; extension < plan->extension_count; extension++) {
      size_t position = plan->extensions[extension].position;
      size_t resume = plan->extensions[extension].resume;
      if (position > current.start && position >= current.recovery.position)
        break;
      if (position >= current.start && resume >= current.recovery.position)
        current.recovery = (struct Recovery){resume, false};
    }
    plan->issues[count++] = current;
    if (
      current.owner &&
      plan->owners[current.owner].end < current.recovery.position
    )
      plan->owners[current.owner].end = current.recovery.position;
    for (
      size_t owner = current.owner; owner && !plan->owners[owner].retained;
      owner = plan->owners[owner].parent
    )
      plan->owners[owner].retained = true;
  }
  return count;
}

static void extend_ancestors(struct Plan *plan) {
  for (size_t i = 1; i < plan->owner_count; i++) {
    if (!plan->owners[i].retained) continue;
    size_t end = plan->owners[i].end;
    for (
      size_t parent = plan->owners[i].parent;
      parent && plan->owners[parent].end < end;
      parent = plan->owners[parent].parent
    )
      plan->owners[parent].end = end;
  }
}

static size_t
collect_children(const struct Plan *plan, struct PublicChild *children) {
  size_t length = 0;
  for (size_t i = 1; i < plan->owner_count; i++) {
    const struct Owner *owner = &plan->owners[i];
    if (owner->retained)
      children[length++] =
        (struct PublicChild){owner->parent, i, owner->start, owner->end, false};
  }
  for (size_t i = 0; i < plan->issue_count; i++) {
    const struct Issue *issue = &plan->issues[i];
    if (issue->owner)
      children[length++] = (struct PublicChild){
        issue->owner,
        i,
        issue->start,
        issue->recovery.position,
        true
      };
  }
  return length;
}

static bool append_unowned(
  const struct Plan *plan,
  struct PublicChild *children,
  size_t *length,
  const size_t *depths
) {
  size_t *stack = ts_malloc(plan->owner_count * sizeof(*stack));
  if (!stack) return false;
  for (size_t i = 0; i < plan->issue_count; i++) {
    const struct Issue *issue = &plan->issues[i];
    if (issue->owner) continue;
    children[(*length)++] = (struct PublicChild){
      place_unowned(plan, children, depths, stack, issue),
      i,
      issue->start,
      issue->recovery.position,
      true
    };
  }
  ts_free(stack);
  return true;
}

static bool publish(struct Scanner *scanner, struct Plan *plan, size_t end) {
  if (plan->failed) return false;
  plan->issue_count = select_regions(plan, end);
  extend_ancestors(plan);
  size_t count = plan->issue_count;
  if (
    plan->owner_count > SIZE_MAX - count ||
    plan->owner_count + count > SIZE_MAX / sizeof(struct PublicChild)
  )
    return false;
  struct PublicChild *children =
    ts_malloc((plan->owner_count + count) * sizeof(*children));
  size_t *depths = ts_calloc(plan->owner_count, sizeof(*depths));
  struct PublishFrame *frames = children && depths
    ? ts_malloc(owner_depths(plan, depths) * sizeof(*frames))
    : NULL;
  bool success = frames != NULL;
  if (success) {
    size_t length = collect_children(plan, children);
    arrange_children(plan, children, length);
    success = end_prefixes(plan, children, frames, end) &&
      append_unowned(plan, children, &length, depths);
    if (success) {
      arrange_children(plan, children, length);
      // An unowned region can extend beyond the child containing its start.
      success = end_prefixes(plan, children, frames, end) &&
        emit_owner(scanner, plan, children, frames, end);
    }
  }
  ts_free(frames);
  ts_free(depths);
  ts_free(children);
  return success;
}

static bool read_source(TSLexer *lexer, struct Text *source) {
  lexer->mark_end(lexer);
  while (!lexer->eof(lexer)) {
    size_t position = source->count;
    if (!append(
          source,
          (
            struct Unit
          ){lexer->lookahead < 0 ? INVALID_CHARACTER : lexer->lookahead,
            position,
            position + 1}
        ))
      return false;
    lexer->advance(lexer, false);
  }
  return true;
}

static void decode_faults(struct Parser *parser, const struct Text *source) {
  for (size_t i = 0; i < source->count && !parser->plan->failed; i++) {
    int32_t value = source->units[i].value;
    if (value != INVALID_CHARACTER && value != 0) continue;
    size_t start = i;
    while (i + 1 < source->count && source->units[i + 1].value == value) i++;
    issue(
      parser,
      value == 0 ? NUL_CHARACTER : INVALID_ENCODING,
      start,
      i + 1,
      (struct Recovery){i + 1, true}
    );
  }
  parser->plan->decode_issues = parser->plan->issue_count;
}

static bool prepare(struct Scanner *scanner, TSLexer *lexer) {
  struct Text source = {0};
  bool success = read_source(lexer, &source);
  struct Plan plan = {.failed = !success};
  struct Rejected rejected = {0};
  struct Parser parser = {
    .input = {source.units, source.count, 0, source.count, NULL, &rejected},
    .plan = &plan
  };
  owner_begin(&parser, START, 0);
  if (!plan.failed) plan.owners[0].end = source.count;
  decode_faults(&parser, &source);
  if (success) parse(&parser);
  clear_parser(&parser);
  ts_free(rejected.bits);
  scanner->count = 0;
  plan.failed |= !publish(scanner, &plan, source.count);
  ts_free(plan.owners);
  ts_free(plan.issues);
  ts_free(plan.extensions);
  ts_free(source.units);
  scanner->cursor = scanner->position = 0;
  scanner->started = !plan.failed;
  return !plan.failed;
}

void *tree_sitter_diagnostic_sh_external_scanner_create(void) {
  return ts_calloc(1, sizeof(struct Scanner));
}

void tree_sitter_diagnostic_sh_external_scanner_destroy(void *payload) {
  struct Scanner *scanner = payload;
  if (scanner) {
    ts_free(scanner->events);
    ts_free(scanner);
  }
}

unsigned tree_sitter_diagnostic_sh_external_scanner_serialize(
  void *payload,
  char *buffer
) {
  const struct Scanner *scanner = payload;
  if (!scanner || !scanner->started) return 0;
  memcpy(buffer, &scanner->cursor, sizeof(scanner->cursor));
  memcpy(
    buffer + sizeof(scanner->cursor),
    &scanner->position,
    sizeof(scanner->position)
  );
  return (unsigned)(sizeof(scanner->cursor) + sizeof(scanner->position));
}

void tree_sitter_diagnostic_sh_external_scanner_deserialize(
  void *payload,
  const char *buffer,
  unsigned length
) {
  struct Scanner *scanner = payload;
  if (!scanner) return;
  scanner->started =
    length == sizeof(scanner->cursor) + sizeof(scanner->position);
  scanner->cursor = scanner->position = 0;
  if (scanner->started) {
    memcpy(&scanner->cursor, buffer, sizeof(scanner->cursor));
    memcpy(
      &scanner->position,
      buffer + sizeof(scanner->cursor),
      sizeof(scanner->position)
    );
    if (scanner->cursor > scanner->count) scanner->started = false;
  }
}

bool tree_sitter_diagnostic_sh_external_scanner_scan(
  void *payload,
  TSLexer *lexer,
  const bool *valid
) {
  struct Scanner *scanner = payload;
  if (!scanner || (valid[START] && valid[FINISH])) return false;
  if (valid[START]) {
    if (!prepare(scanner, lexer)) return false;
    lexer->result_symbol = START;
    return true;
  }
  if (!scanner->started || scanner->cursor >= scanner->count) return false;
  const struct Event *next = &scanner->events[scanner->cursor];
  if (!valid[next->kind] || next->end < scanner->position) return false;
  while (scanner->position < next->end) {
    if (lexer->eof(lexer)) return false;
    lexer->advance(lexer, false);
    scanner->position++;
  }
  lexer->mark_end(lexer);
  lexer->result_symbol = (TSSymbol)next->kind;
  scanner->cursor++;
  return true;
}
