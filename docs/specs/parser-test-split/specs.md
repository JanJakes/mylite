# Parser test split

## Scope

`packages/libmylite/tests/parser_test.c` has grown into a very large parser
test executable. This refactor preserves parser coverage while splitting the
test surface into smaller category executables that share one parser test
support module.

## Goals

- Reduce the size of individual parser test source files.
- Reduce the single-file cost of clang-tidy on parser tests.
- Keep parser coverage equivalent to the original monolithic test.
- Keep shared parser AST assertions in one support module.
- Use CTest names that still group naturally under `libmylite.parser`.

## Non-goals

- No parser behavior changes.
- No grammar, AST, lexer, runtime, or compatibility behavior changes.
- No removal or weakening of parser assertions.
- No generated test source or external test harness dependency.

## Module boundaries

The split keeps parser test helpers in:

- `parser_test_support.h`: shared parser test constants, parse-mode type, and
  assertion helper declarations.
- `parser_test_support.c`: shared parse wrappers and AST assertion helpers.

Parser coverage is split into category executables:

- `parser_core_test.c`: empty scripts, `USE`, basic select lists, literal and
  identifier basics, comments.
- `parser_builtin_string_json_test.c`: identity, string, regular expression,
  JSON, and related scalar built-ins.
- `parser_builtin_temporal_numeric_test.c`: temporal, numeric, binary, UUID,
  and math built-ins.
- `parser_expression_aggregate_test.c`: expression statements, session scalar
  functions, and aggregate function syntax.
- `parser_ddl_table_test.c`: schema/table lifecycle, table options, table
  maintenance, temporary tables, and table/view creation syntax.
- `parser_types_test.c`: data type grammar and temporal current-time
  statement forms.
- `parser_constraints_indexes_test.c`: primary key, foreign key, index, check,
  and related `ALTER TABLE` syntax.
- `parser_show_test.c`: `SHOW` statement grammar.
- `parser_select_test.c`: SELECT clauses, compound selects, table/value
  statements, joins, aliases, and predicates.
- `parser_dml_control_test.c`: INSERT/REPLACE/LOAD DATA/DELETE/UPDATE,
  prepared statements, transaction control, and table locks.
- `parser_errors_test.c`: syntax and lexer error coverage.

## Compatibility requirements

- Every test function from the original monolithic parser test must appear in
  exactly one split category.
- Every split test executable must return failure if any contained assertion
  fails.
- Parse helper behavior, expected parse statuses, AST traversal, and assertion
  text comparisons must remain equivalent to the original monolith.
- `ctest -R '^libmylite\.parser'` must run the full split parser test set.

## Review checklist

- Shared helpers have a clear `parser_test_` prefix because they are no longer
  file-local statics.
- Category files contain only test functions and their local runner.
- CMake registers every parser category executable with the shared support
  source.
- Focused parser CTest covers all split parser tests.
- Focused clang-tidy runs on parser test sources before the full workflow.
