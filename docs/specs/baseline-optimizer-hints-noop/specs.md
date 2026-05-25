# Baseline Optimizer Hints No-Op

## Summary

This phase admits MySQL optimizer-hint comments as comments for the currently
supported statement subset. MyLite does not implement optimizer behavior in this
slice; valid hint comments must not change statement results, affected-row
counts, warning counts, catalog descriptors, physical SQLite schema, or storage
format state.

The compatibility authority is the MySQL 8.4 Reference Manual optimizer-hints
page (<https://dev.mysql.com/doc/refman/8.4/en/optimizer-hints.html>) and
observed MySQL 8.4.9 runtime behavior. The implementation remains independently
authored. No MySQL grammar or implementation source is copied.

## MySQL 8.4.9 Observations

Observed against `mysql:8.4.9` in the local `mylite-mysql-849` runtime:

- `SELECT /*+ MAX_EXECUTION_TIME(1000) */ id, v FROM t WHERE id = 1` returns the
  same row as the statement without the hint; following diagnostics report
  `ROW_COUNT() = -1`, `@@warning_count = 0`, and `@@error_count = 0`.
- `SELECT /*+ SET_VAR(sort_buffer_size=262144) */ COUNT(*) FROM t` returns the
  same count and zero warnings.
- `INSERT /*+ SET_VAR(sort_buffer_size=262144) */ INTO t VALUES (...)`,
  `REPLACE /*+ SET_VAR(sort_buffer_size=262144) */ INTO t VALUES (...)`,
  `UPDATE /*+ SET_VAR(sort_buffer_size=262144) */ t SET ...`, and
  `DELETE /*+ SET_VAR(sort_buffer_size=262144) */ FROM t WHERE ...` execute
  normally and report the same affected-row counts as the unhinted statements.
- `SELECT /*+ NO_SUCH_HINT() */ 1` succeeds and produces warning
  `1064 / HY000` with an optimizer-hint syntax message.
- `SELECT 1 /*+ NO_SUCH_HINT() */` succeeds with zero warnings; in that position
  the comment is not a recognized optimizer-hint comment for the query block.

## Supported Scope

Supported:

- Block comments whose first three bytes are `/*+`, ending at the next `*/`.
- Hint comments in currently supported `SELECT`, `INSERT`, `REPLACE`, `UPDATE`,
  and `DELETE` statements where MySQL accepts valid optimizer hints.
- Regular comment treatment for hint-shaped comments in non-hint positions.
- Existing statement semantics, diagnostics, result metadata, affected rows, and
  warning counts for supported statements.

Deferred:

- Parsing the hint payload into an optimizer-hint AST.
- Applying optimizer behavior, cost changes, join-order changes, range access
  changes, execution-time limits, resource groups, or `SET_VAR` execution
  effects.
- MySQL-compatible invalid-hint warnings for hint comments in recognized hint
  positions.
- Statement or query-block hint scoping, table/index/function hint resolution,
  duplicate/conflicting hint diagnostics, and warning text parity.
- Hint support for statements MyLite does not otherwise support.

## Syntax

The lexer recognizes three comment families:

```text
ordinary_comment ::= "#" until_line_end
ordinary_comment ::= "--" mysql_comment_space until_line_end
ordinary_comment ::= "/*" comment_body "*/"
version_comment  ::= "/*!" comment_body "*/"
hint_comment     ::= "/*+" comment_body "*/"
```

For this slice, `hint_comment` is consumed before Lemon parser input in the same
comment-skipping path as ordinary and version comments. The grammar surface is
therefore equivalent to admitting optional comments between tokens:

```text
select_statement  ::= SELECT comment* select_tail
insert_statement  ::= INSERT comment* insert_tail
replace_statement ::= REPLACE comment* replace_tail
update_statement  ::= UPDATE comment* update_tail
delete_statement  ::= DELETE comment* delete_tail
```

The snippet describes MyLite's intended surface; the implemented parser does not
receive comment tokens and therefore does not create AST nodes for hints.

## Semantics

Valid hint comments are no-ops for the current MyLite statement subset:

- The public API receives the same `mylite_result` shape as the unhinted
  statement.
- `SELECT` returns the same rows, result-column labels, affected rows, warning
  count, and `ROW_COUNT()` state as the unhinted statement.
- `INSERT`, `REPLACE`, `UPDATE`, and `DELETE` preserve the existing
  descriptor-driven planning and execution paths, including affected-row counts,
  duplicate checks, default handling, conversions, cascades currently supported
  by the statement, and warning counts.
- Hint comments do not mutate session variables. In particular,
  `SET_VAR(...)` hint text is not interpreted.
- Hint comments do not read or mutate MyLite catalog descriptors, descriptor
  caches, catalog generation, or SQLite schema generation.
- Hint comments do not change physical SQLite SQL generation, identifier
  quoting, parameter binding, row storage, file-format preamble handling, VFS
  behavior, or SQLite fork behavior.

Because MyLite does not parse hint payloads in this slice, unsupported or
invalid hint text in a recognized hint position currently follows the same
comment no-op path instead of producing MySQL's optimizer-hint warning. This is
a documented partial-compatibility gap rather than a supported guarantee.

## Architecture

- Public API: unchanged. `mylite_execute()` receives SQL text and returns
  existing result objects.
- Lexer: identifies `/*+ ... */` as a distinct hint-comment token and reports
  unterminated block comments through the existing lexer error path.
- Parser/AST: comments are skipped before Lemon parser input. No AST node,
  semantic payload, or planner contract is added for optimizer hints.
- Analyzer/planner/runtime: unchanged for hinted supported statements because
  the AST is identical to the unhinted statement.
- Catalog: unchanged. Hints are not descriptor metadata and must not become
  durable catalog rows.
- Result builder and diagnostics: unchanged except that tests lock in zero
  warnings for valid no-op hints in supported statements.
- Storage/VFS/SQLite: unchanged. This slice uses existing MyLite wrapper and
  parser behavior; no public SQLite extension API, SQLite translation change, or
  SQLite fork hook is needed.

## Diagnostics

Supported diagnostics remain the diagnostics of the underlying unhinted
statement:

- Syntax errors outside comments are reported through the existing parser path.
- Unterminated `/*+` comments use the existing unterminated-comment lexer
  diagnostic.
- Runtime errors from the underlying statement, such as unknown tables, unknown
  columns, duplicate keys, conversion failures, and allocation failures, are
  unchanged.

Deferred diagnostics:

- MySQL warning `1064` for invalid optimizer-hint payloads in recognized
  positions.
- MySQL warnings for duplicate, conflicting, inapplicable, unknown-table,
  unknown-index, unknown-variable, or non-hintable `SET_VAR` hint content.

## Performance

The runtime cost is limited to existing comment scanning in the lexer. No
additional AST allocation, catalog lookup, planner work, materialization, or
SQLite statement generation is introduced. Supported hinted statements stay on
the same execution path as the equivalent unhinted statement.

## Tests

MySQL expectation coverage records:

- Valid `SELECT` optimizer hints returning rows and zero warnings.
- Valid `INSERT`, `REPLACE`, `UPDATE`, and `DELETE` hints preserving affected
  rows and zero warnings.
- Hint-shaped trailing comments outside recognized hint positions producing no
  MySQL warning.
- Invalid hint payloads in recognized positions producing a MySQL warning, while
  remaining deferred for MyLite.

MyLite C coverage records:

- Parser acceptance of hint comments in supported statement positions.
- Runtime no-op behavior for scalar `SELECT`, table-backed `SELECT`, `INSERT`,
  `REPLACE`, `UPDATE`, and `DELETE`.
- Public result affected-row, row-count, and warning-count behavior.
