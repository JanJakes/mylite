# Native Prepared-Statement Binding

## Purpose

This feature replaces value-to-SQL interpolation in MyLite's public statement
and PHP APIs with typed parameter binding. Parameter bytes remain data under
every SQL mode. A prepared statement parses its SQL once, records parameter
slots, binds typed values independently, and can be executed repeatedly without
reconstructing SQL from those values.

The SQL-level `PREPARE`/`EXECUTE` implementation shares the same parameter
representation after this work. Its existing lifecycle and diagnostics remain
MySQL-compatible, but its execution no longer renders user variables as SQL
literals.

## Authorities

- MySQL 8.4 Reference Manual, prepared statements:
  <https://dev.mysql.com/doc/refman/8.4/en/sql-prepared-statements.html>
- MySQL 8.4 C API Developer Guide, prepared statement interface:
  <https://dev.mysql.com/doc/c-api/8.4/en/c-api-prepared-statement-interface.html>
- MySQL 8.4 C API Developer Guide, bind data structures and types:
  <https://dev.mysql.com/doc/c-api/8.4/en/c-api-prepared-statement-data-structures.html>
  and
  <https://dev.mysql.com/doc/c-api/8.4/en/c-api-prepared-statement-type-codes.html>
- MySQL 8.4 C API Developer Guide, `mysql_stmt_bind_param()`:
  <https://dev.mysql.com/doc/c-api/8.4/en/mysql-stmt-bind-param.html>
- MySQL 8.4.9 runtime expectations in
  `packages/libmylite/tests/mysql_native_prepared_statement_binding_expectations.sh`
  and
  `packages/libmylite/tests/mysql_baseline_sql_prepared_statements_expectations.sh`.

The design and grammar below are independently authored from the public
documentation, observed MySQL 8.4.9 behavior, and existing MyLite architecture.

## MySQL 8.4.9 observations

The recorded runtime probes establish the following requirements:

- Parameter markers are recognized only in preparable SQL, not ordinary direct
  SQL.
- A marker is a value expression. It cannot replace a table, column, keyword,
  operator, or complete statement.
- Marker-like bytes in strings, quoted identifiers, and comments are not
  parameters.
- The number of values supplied at execution must equal the marker count.
- Bound text containing quotes, comment delimiters, or SQL operators remains a
  single value under `NO_BACKSLASH_ESCAPES`.
- Character values and binary values are distinct. Binary parameters preserve
  embedded NUL bytes.
- NULL, signed/unsigned integers, fixed/approximate numeric values, character
  strings, binary strings, and temporal values retain their input type for
  MySQL conversion rules.
- LIMIT and OFFSET accept parameters in prepared statements.
- A statement can be executed repeatedly with different values and input
  types.
- Relevant table/view metadata changes cause automatic reprepare before the
  next execution.
- Prepare-time syntax and object-resolution failures are distinct from
  execute-time conversion, constraint, and data errors.
- Prepared SQL contains exactly one preparable statement.

## Public C API

The existing opaque `mylite_stmt` becomes the prepared and cursor handle. The
following operations are added:

```c
int mylite_prepare_buffered(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_stmt **out_stmt
);
size_t mylite_stmt_parameter_count(const mylite_stmt *stmt);
int mylite_stmt_bind_null(mylite_stmt *stmt, size_t index);
int mylite_stmt_bind_int64(mylite_stmt *stmt, size_t index, int64_t value);
int mylite_stmt_bind_uint64(mylite_stmt *stmt, size_t index, uint64_t value);
int mylite_stmt_bind_double(mylite_stmt *stmt, size_t index, double value);
int mylite_stmt_bind_text(
    mylite_stmt *stmt,
    size_t index,
    const char *value,
    size_t value_size
);
int mylite_stmt_bind_blob(
    mylite_stmt *stmt,
    size_t index,
    const void *value,
    size_t value_size
);
int mylite_stmt_clear_bindings(mylite_stmt *stmt);
int mylite_stmt_reset(mylite_stmt *stmt);
int64_t mylite_stmt_affected_rows(const mylite_stmt *stmt);
uint64_t mylite_stmt_insert_id(const mylite_stmt *stmt);
```

Indexes are zero-based in the C ABI. PHP adapters translate their user-facing
index conventions at the boundary.

`mylite_prepare()` borrows SQL only for the call and owns the parsed/analyzed
representation on success. A statement owns copies of text/blob bindings.
Rebinding replaces the previous value. A zero-length text/blob may use a NULL
input pointer; a nonzero length requires a non-NULL pointer. Binding an index
outside the marker count, binding while a cursor row is active, or stepping
with any unbound marker returns `MYLITE_MISUSE` or a statement diagnostic as
specified by the operation.

`mylite_stmt_reset()` ends the current execution, preserves bindings, and makes
the statement executable again. `mylite_stmt_clear_bindings()` clears values
without changing the prepared plan. Schema generation or relevant session
semantic changes invalidate analysis and trigger controlled reprepare on the
next execution.

`mylite_stmt_step()` starts execution lazily and then returns `MYLITE_ROW` for
result rows or `MYLITE_DONE` after completion. Non-row statements return
`MYLITE_DONE` after one execution; affected rows and insert ID remain available
until reset or the next execution.

`mylite_prepare_buffered()` has the same binding and execution contract as
`mylite_prepare()`, but materializes the complete result when execution begins,
before `mylite_stmt_step()` returns the first row. This releases the
connection's read transaction and publishes statement status so adapters with
buffered-result semantics can issue another command while rows remain unread.
The retained AST and bindings are still reused on later executions.

## PHP adapters

### Core MyLite extension

`Connection::prepare()` calls `mylite_prepare()` immediately and therefore
reports prepare-time diagnostics before returning a Statement. `bindValue()`
maps PHP NULL, integer, float, string, and binary bytes to native bind calls.
`execute()` binds its optional array, resets an earlier execution, steps the
native handle, and materializes rows without reconstructing SQL.

### mysqli replacement

`mysqli_prepare()` owns a native statement. `bind_param()` keeps PHP's
by-reference behavior: current variable values are read and bound immediately
before each execution according to the type string. `execute(array)` binds the
array values directly. SELECT execution uses the native cursor/result path;
DML status comes from the same native statement completion record.

### PDO

The driver advertises positional placeholders and binds through the native
handle. It preserves the distinction between text and binary parameter types,
honors by-value versus by-reference parameter behavior, and uses the native
buffered statement path rather than an emulated active query string. PDO's own
placeholder parser rewrites named markers to positional slots without rendering
values.

## Parser and AST

The lexer already emits `MYLITE_SQL_TOKEN_PARAMETER`. Prepared parsing maps it
to a dedicated token only when the parse configuration enables parameters.
Direct SQL retains its current syntax error.

Intended MyLite Lemon syntax:

```lemon
prepared_literal(A) ::= PARAMETER(T). {
    A = mylite_sql_parser_make_parameter(state, T);
}

literal(A) ::= prepared_literal(B). {
    A = B;
}
```

Specialized expression nonterminals that admit MySQL parameter values also
accept `prepared_literal`. Identifier, option-name, keyword, operator, and DDL
definition-name nonterminals do not.

Each parameter AST node stores its zero-based slot index and absolute source
span. Parameter indexes follow lexical order across nested expressions,
subqueries, compounds, and executable comments. Marker counting and parsing
are one operation; independent rescans cannot disagree.

## Analysis and execution

The analyzer records for every slot:

- its lexical index and source span;
- inferred MySQL type family and unsigned/binary attributes where context
  determines them;
- collation/coercibility requirements;
- each plan location that consumes the slot;
- whether execution-time type changes require reanalysis.

The lowered plan emits SQLite placeholders and a binding descriptor in the same
walk. A preparation invariant compares the descriptor count/order with
`sqlite3_bind_parameter_count()`. Internal literal parameters and public input
parameters use distinct descriptor kinds so their numbering cannot drift.

At execution, public values are converted through MyLite's existing MySQL type
rules and bound with SQLite's typed bind APIs. Values are never escaped or
inserted into SQL text. MyLite-only operators consume the same typed value
objects directly.

The first implementation may conservatively reanalyze when a parameter's input
type family changes. Reanalysis must reuse the parsed AST, be generation-safe,
and preserve prepare/execute diagnostics. It must not fall back to interpolation.

## Diagnostics

- Invalid API pointers or indexes return `MYLITE_MISUSE`.
- Allocation failure returns `MYLITE_NOMEM` and leaves prior bindings intact.
- Missing bindings fail execution without starting or partially executing the
  statement.
- Prepare-time syntax/object errors are connection and statement diagnostics
  from prepare.
- Execute-time conversion, range, constraint, and storage errors are statement
  diagnostics and update connection diagnostics according to the unified
  completion contract.
- Automatic reprepare failure reports the new metadata/schema diagnostic and
  leaves the statement resettable or explicitly invalid.
- Parameter data never changes the statement text in diagnostics or statement
  digest output.

## SQL modes, character sets, and binary values

SQL modes affect parsing of the SQL template but never parsing of a bound
value. `NO_BACKSLASH_ESCAPES`, ANSI_QUOTES, and quote/comment syntax therefore
cannot make parameter bytes executable.

Text bindings are bytes in the connection's client character set and receive
character-string collation semantics. Blob bindings use binary semantics and
preserve every byte, including NUL. A later character-set expansion may add an
explicit bind encoding, but it must not infer text by scanning bytes.

## Storage and SQLite integration

This feature uses public SQLite preparation, bind, reset, and clear-binding
APIs. It does not alter the `.mylite` preamble, catalog format, shifted VFS, or
SQLite fork. Schema-generation invalidation remains MyLite-owned.

## Performance requirements

- Repeated execution of an unchanged statement must not normalize, lex, or
  parse its SQL again.
- Rebinding fixed-size scalar values performs no allocation.
- Text/blob bindings allocate only when ownership requires growth.
- Cached SQLite programs are reset and bindings cleared between executions.
- Benchmarks compare prepared repeated SELECT/DML against literal execution and
  direct SQLite binding, reporting parse/plan counts, allocations, median, and
  p95 latency.

## Test plan

### Core C tests

- all bind types, zero-length values, embedded NULs, large values, rebinding,
  clearing, reset, repeated execution, and invalid indexes;
- marker recognition in strings, identifiers, comments, executable comments,
  predicates, projections, DML values/assignments, LIMIT/OFFSET, subqueries,
  compounds, functions, and unsupported identifier positions;
- missing/extra bindings, prepare-time syntax/object errors, execute-time
  conversion/constraint errors, schema invalidation, rollback, and close order;
- generated placeholder/binding descriptor count and order assertions;
- ASan/UBSan allocation/lifetime coverage and failpoint rollback.

### PHP tests

- the original quote/comment injection payload under
  `NO_BACKSLASH_ESCAPES` through core PHP, mysqli, and PDO;
- NULL, signed/unsigned integer, float, text, binary/NUL, by-reference updates,
  execute arrays, repeated execution, DML status, SELECT metadata, errors, and
  destruction order;
- proof that values containing SQL syntax cannot change row count, schema, or
  subsequent diagnostics.

### MySQL and application verification

- run both prepared-statement MySQL 8.4.9 expectation scripts;
- compare metadata, diagnostics, row counts, insert IDs, and side effects;
- run WordPress, Drupal, Laravel, Doctrine, and MediaWiki baselines;
- add a repeated prepared-query performance scenario to the benchmark suite.

## Compatibility status

Core PHP, mysqli, PDO, and SQL-level `PREPARE`/`EXECUTE` use this API. Release
qualification, sanitizer coverage, and ABI/size review are tracked separately
and remain required before closing the broader remediation chapter.

## Implementation measurements

The July 2026 Release measurements on the shared development host show a
prepared WordPress option SELECT at a 39-44 microsecond p50 versus 45-51
microseconds for literal execution. The equivalent autocommit UPDATE remains
journal dominated: prepared and literal p50 values both fall near 205-226
microseconds. These are directional local baselines, not CI thresholds.

Profiling 10,000 retained SELECT executions records zero normalization calls
and zero parse calls after prepare. A focused materialized-DML profile enforces
the same invariant and verifies that lazy step execution, SQLite work, and
completion are included in `cursor_step_ns`.

Parameterized cursor SELECTs also retain their analyzed plan and lowered SQL
when parameters occupy direct comparison, BETWEEN, or IN-list value positions,
every binding keeps the same input type, and the catalog and SQLite schema
generations are unchanged. The cache key also includes the SQL mode, time-zone
offset, last insert ID, and `sql_auto_is_null` state currently consumed by
SELECT analysis. Planned predicate values refer to statement-owned typed slots,
so rebinding changes data without rebuilding SQL. A changed input type triggers
conservative reanalysis. Parameters in projections, function options, LIMIT,
OFFSET, and other positions that can influence plan structure or result metadata
remain deliberately uncached until those plan fields carry explicit runtime
parameter descriptors.
