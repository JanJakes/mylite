# Baseline SQL Prepared Statements

This slice adds the first SQL-level prepared statement lifecycle on top of the
existing `mylite_execute()` path: `PREPARE`, `EXECUTE`, and
`DEALLOCATE PREPARE` / `DROP PREPARE`. The target is application bootstrap and
WordPress-style compatibility where SQL text is prepared inside a session and
then executed with user-variable values.

This is not the binary prepared-statement protocol, a general parameter system,
or a new query planner. Prepared statements are handle-local runtime state.
After marker substitution, execution reuses the same parser, analyzer, catalog,
descriptor, result, statement-context, storage, and SQLite paths as ordinary
SQL text.

## Sources

- MySQL 8.4 Reference Manual, prepared statements:
  <https://dev.mysql.com/doc/refman/8.4/en/sql-prepared-statements.html>
- MySQL 8.4 Reference Manual, `PREPARE`:
  <https://dev.mysql.com/doc/refman/8.4/en/prepare.html>
- MySQL 8.4 Reference Manual, `EXECUTE`:
  <https://dev.mysql.com/doc/refman/8.4/en/execute.html>
- MySQL 8.4 Reference Manual, `DEALLOCATE PREPARE`:
  <https://dev.mysql.com/doc/refman/8.4/en/deallocate-prepare.html>
- MySQL 8.4.9 runtime probes in
  `packages/libmylite/tests/mysql_baseline_sql_prepared_statements_expectations.sh`
- MyLite SQLite source snapshot notes: `third_party/sqlite/README.md`

The MyLite grammar and implementation are independently authored from the
official documentation, observed MySQL 8.4.9 behavior, and existing MyLite
patterns.

## MySQL 8.4.9 Observations

Runtime probes verified these behaviors for the supported slice:

- Statement names are case-insensitive: `PREPARE stmt ...` can be executed as
  `EXECUTE STMT`.
- Prepared statements are session-local and disappear when deallocated.
- `DROP PREPARE name` is accepted as a synonym for `DEALLOCATE PREPARE name`.
- `PREPARE` source text may be a string literal or a user variable containing
  SQL text.
- A `NULL`, uninitialized, or numeric user-variable source is converted to SQL
  text for parsing and fails with a normal syntax error near that text.
- The prepared source must contain exactly one statement. A second statement
  after a semicolon fails at prepare time.
- If a statement with the same name already exists, MySQL implicitly removes it
  before preparing the replacement. If the replacement fails, the old statement
  is gone.
- `?` markers are legal only as value placeholders in preparable SQL text.
  Direct SQL such as `SELECT ?` remains a syntax error.
- `EXECUTE ... USING` accepts user variables only. Constants in the `USING`
  list are syntax errors.
- The `USING` variable count must exactly match the marker count; otherwise
  MySQL reports `1210 / HY000`.
- Unknown statement execution/deallocation reports `1243 / HY000`.
- Preparing `PREPARE`, `EXECUTE`, or `DEALLOCATE PREPARE` as the inner
  statement reports `1295 / HY000`.
- Successful `PREPARE` and deallocation report `ROW_COUNT() == 0`.
- Successful `EXECUTE` reports the executed statement's row-count semantics:
  `SELECT` yields `ROW_COUNT() == -1`; non-query statements use the underlying
  affected-row count.
- SQL syntax is interpreted using the SQL-mode lexer rules in effect at
  `PREPARE`. Later `sql_mode` changes do not reinterpret quoted tokens,
  `PIPES_AS_CONCAT`, or backslash escapes in the prepared source.
- The default database, connection character set, and connection collation are
  captured at `PREPARE`. Unqualified objects, `DATABASE()`, string-literal
  metadata, and literal collation semantics use that context during execution,
  after which the connection's current default database is restored.
- User-variable parameter values and ordinary session-variable reads remain
  execute-time. For example, `@@session.sql_mode` and
  `@@session.collation_connection` report their current values even when the
  prepared source and literals use their captured prepare-time context.
- Runtime validation modes remain execute-time. Changing strict data-validation
  modes after `PREPARE` affects a later DML execution without reparsing the
  prepared source under the new syntax modes.

## Supported Surface

### Statements

Supported lifecycle forms:

```sql
PREPARE stmt_name FROM 'sql text'
PREPARE stmt_name FROM @source
EXECUTE stmt_name
EXECUTE stmt_name USING @a
EXECUTE stmt_name USING @a, @b
DEALLOCATE PREPARE stmt_name
DROP PREPARE stmt_name
```

`stmt_name` is a MySQL identifier or quoted identifier resolved
case-insensitively after MyLite's existing identifier decoding. Prepared
statements are stored only on the current `mylite_db` handle. They are not
durable, visible through metadata tables, or shared across independent handles.

Supported source text is either an ordinary string literal decoded with the
current session SQL-mode string rules or an existing user variable. String
sources preserve the source bytes after decoding; user-variable string sources
use the stored user-variable value. Other user-variable values are converted to
their visible text before parsing and therefore fail or succeed according to the
same parser path MySQL exposes.

### Parameter Markers

The prepared SQL text may contain `?` markers where the current MyLite parser
can validate the statement after replacing markers with `NULL`. Markers inside
string literals, quoted identifiers, comments, or hint comments are ordinary
text and do not count as parameter markers.

`EXECUTE ... USING` binds only user variables. Uninitialized variables and
`NULL` variables bind as SQL `NULL`. Integer and boolean user variables bind as
decimal integer literals. Fixed-decimal user variables bind as decimal source
text. String user variables bind as single-quoted string literals with
MyLite-owned escaping. The bound SQL is then parsed and dispatched through the
same internal statement execution path as direct SQL text, so descriptor
resolution, catalog authority, SQLite identifier quoting, parameter binding
inside physical plans, result metadata, diagnostics, warning counts, and
affected-row behavior remain owned by existing statement implementations.

This slice deliberately does not admit parameter markers outside prepared SQL,
binary protocol markers, marker metadata, server-side parameter type inference,
or identifier/keyword/table-name markers.

### Unsupported Prepared Content

The inner prepared statement may be any currently supported single MyLite SQL
statement after marker replacement, except `PREPARE`, `EXECUTE`, and
`DEALLOCATE PREPARE` / `DROP PREPARE`, which return the verified unsupported
prepared-command diagnostic.

Unsupported inner SQL keeps the same parse or runtime diagnostic it has when
executed directly. This slice does not add new support for arbitrary
expressions, subqueries, joins, metadata tables, defaults, triggers, cascades,
privileges, or SQLite pass-through.

## Grammar

The MyLite Lemon grammar is extended with independent SQL-level prepared
statement productions:

```lemon
statement ::= prepare_statement.
statement ::= execute_statement.
statement ::= deallocate_prepare_statement.

prepare_statement ::= PREPARE identifier FROM prepare_source.
prepare_source ::= STRING.
prepare_source ::= USER_VARIABLE.

execute_statement ::= EXECUTE identifier execute_using_opt.
execute_using_opt ::= .
execute_using_opt ::= USING execute_using_list.
execute_using_list ::= USER_VARIABLE.
execute_using_list ::= execute_using_list COMMA USER_VARIABLE.

deallocate_prepare_statement ::= DEALLOCATE PREPARE identifier.
deallocate_prepare_statement ::= DROP PREPARE identifier.
```

`PARAMETER` tokens remain unmapped in ordinary parser input. They are handled
only by the prepared-statement runtime scanner while validating and expanding a
prepared SQL string.

## Architecture

- Public API: no ABI or public-header change. SQL prepared statements are used
  through `mylite_execute()`.
- Statement context: outer `PREPARE`, `EXECUTE`, and deallocation statements
  use normal statement-context diagnostics and row-count snapshots. `EXECUTE`
  creates an inner statement context for the expanded SQL and exposes the inner
  result object without recursively entering the public API.
- Lexer/parser/AST: parser adds lifecycle AST nodes but does not make `?`
  valid in direct SQL.
- Runtime session state: owns a growable vector of prepared statement entries
  containing source SQL, marker count, lexer modes, default database, and
  connection character-set/collation context, plus a small user-variable
  value-kind tag needed for safe marker rendering. Entries are zero-init safe
  and freed on close.
- Analyzer/planner: no separate prepared-statement planner is introduced.
  Prepare-time validation parses a marker-normalized SQL string and stores the
  original source plus immutable prepare context. Execute-time expansion uses
  the captured lexer mode, and the existing analyzer/planner resolves the
  expanded SQL under statement-effective schema and collation accessors without
  changing observable session-variable values.
- Catalog/storage/VFS: no catalog rows, descriptor versions, generation
  counters, SQLite schema text, `.mylite` preamble, or shifted SQLite payload
  invariants change.
- SQLite physical execution: no SQLite fork patch is needed. This is MyLite
  wrapper/session behavior using public SQLite APIs already used by existing
  statement execution.
- Result builder: successful `PREPARE` and deallocation return non-row result
  objects with affected rows `0`; successful `EXECUTE` returns the exact result
  object produced by the inner statement.

## Diagnostics

The slice covers these diagnostics:

- existing syntax errors for malformed lifecycle statements, direct `?`, and
  non-user-variable `USING` arguments;
- existing string-literal diagnostics for unsupported prepared source strings;
- existing user-variable name diagnostics for source and `USING` variables;
- `1064 / 42000` parse errors for invalid source SQL, multiple source
  statements, invalid marker placement, `NULL` source text, and numeric source
  text;
- `1210 / HY000` for missing, too few, or too many `USING` variables;
- `1243 / HY000` for unknown statement handlers during `EXECUTE` or
  deallocation;
- `1295 / HY000` for preparing prepared-statement lifecycle commands;
- existing diagnostics from the executed statement after marker expansion;
- allocation failures as `MYLITE_NOMEM`;
- public API misuse remains unchanged.

Supported successful statements emit no warnings on their own. Warnings from
the executed inner statement are preserved by the normal result and diagnostics
snapshot path.

## Test Plan

- Add MySQL 8.4.9 expectation probes for successful lifecycle, row counts,
  warning/error counts, statement-name case folding, replacement failure,
  unknown handlers, `DROP PREPARE`, source string and user-variable forms,
  marker count mismatches, invalid `USING` constants, direct `?` syntax errors,
  invalid marker locations, nested prepared commands, semicolon/multiple
  statements, uninitialized/`NULL` variables, integer parameters,
  fixed-decimal parameters, string parameters, quoted strings with
  apostrophes/backslashes, and DML side effects.
- Add parser tests for all admitted lifecycle grammar and rejected malformed
  forms.
- Add runtime C tests for session-local lifecycle, independent handles,
  successful scalar `SELECT`, descriptor-backed `SELECT`, `INSERT`, `UPDATE`,
  `DELETE`, deallocation, replacement failure, reopen nonpersistence, and
  zero-initialized cleanup.
- Run the new CTest entries, related parser/runtime user-variable tests, the
  MySQL expectation script, and `cmake --workflow --preset check`.

## Known Gaps

- No binary protocol prepared statements.
- No marker metadata, prepared statement status counters, performance-schema
  rows, `max_prepared_stmt_count` enforcement, privilege semantics, or prepared
  statement instrumentation.
- No direct SQL parameters outside `PREPARE`/`EXECUTE`.
- No persistent prepared statements.
- No prepared lifecycle commands inside prepared SQL.
- No additional SQL statement compatibility beyond whatever the expanded SQL
  already supports directly.
- No typed SQL-level parameter descriptors; marker values are still rendered as
  SQL literals before parsing, subject to the documented value-kind and binary
  limitations.
