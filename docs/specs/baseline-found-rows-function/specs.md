# Baseline FOUND_ROWS Function

## Summary

This phase adds a narrow, deprecated-compatibility slice for:

```sql
SELECT FOUND_ROWS()
SELECT SQL_CALC_FOUND_ROWS select_list FROM table_name ... [LIMIT ...]
```

`FOUND_ROWS()` returns connection-local row-count state associated with the
most recent successful `SELECT` surface MyLite supports. `SQL_CALC_FOUND_ROWS`
updates that state to the number of rows the supported descriptor-backed
`SELECT` would have produced before applying `LIMIT`.

This is not a general optimizer feature. The implementation remains
descriptor-driven and MyLite-owned. Its initial scope did not add `UNION`,
joins, subqueries, full SELECT modifiers, full distinct/grouped found-row
behavior, protocol metadata, statement-based replication behavior, or arbitrary
SQLite pass-through. The joined-select subset is extended separately by
`docs/specs/baseline-joined-select-sql-calc-found-rows/specs.md`.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - `FOUND_ROWS()` information function:
    <https://dev.mysql.com/doc/refman/8.4/en/information-functions.html>
  - `SELECT` statement and `SQL_CALC_FOUND_ROWS` modifier:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_found_rows_function_expectations.sh`.

The MySQL 8.4.9 probes establish these expectations for the admitted subset:

- `SQL_CALC_FOUND_ROWS` and `FOUND_ROWS()` both emit deprecation warning `1287`;
- `FOUND_ROWS()` emits one warning per invocation in the statement;
- `FOUND_ROWS()` accepts zero arguments; any argument count is error `1582`;
- `FOUND_ROWS()` is case-insensitive and accepts whitespace before `(`;
- without a preceding `SELECT`, `SELECT FOUND_ROWS()` returns `1` because the
  current one-row scalar select itself is the relevant successful select result;
- after `SELECT SQL_CALC_FOUND_ROWS ... LIMIT n`, `FOUND_ROWS()` returns the
  row count before `LIMIT`;
- after a successful `SELECT` without `SQL_CALC_FOUND_ROWS`, `FOUND_ROWS()`
  returns the number of rows MySQL considered for client return up to the
  `LIMIT` envelope: for `LIMIT row_count`, the returned row count; for
  `LIMIT offset, row_count` or `LIMIT row_count OFFSET offset`, the smaller of
  the matched row count and `offset + returned row count`;
- a successful `SELECT FOUND_ROWS()` then becomes the most recent successful
  select and updates subsequent `FOUND_ROWS()` reads to `1`;
- `@@warning_count` in the same statement as `FOUND_ROWS()` reports the
  resulting statement warning count, including the `FOUND_ROWS()` deprecation
  warning;
- non-`SELECT` statements do not update found-row state;
- `ROW_COUNT()` after result-set statements remains `-1`.

## Ownership Boundaries

- Public API: no ABI change. `mylite_execute()` continues to return ordinary
  result objects through the existing result API.
- Statement context: at statement begin, snapshot the prior found-row value for
  scalar `FOUND_ROWS()` evaluation. Statement completion updates the connection
  found-row state only for successful supported `SELECT` statements.
- Parser/AST: add a zero-argument `FOUND_ROWS()` scalar function node and an
  independent `SELECT` flag for `SQL_CALC_FOUND_ROWS`. The flag must not reuse
  the existing distinct/default enum because MySQL permits `SQL_CALC_FOUND_ROWS`
  as a separate select option.
- Runtime/planner: resolve and execute supported descriptor-backed selects as
  usual. When the select has `SQL_CALC_FOUND_ROWS`, compute the pre-limit row
  count from MyLite-owned planning metadata and descriptor-built SQLite SQL.
- Catalog: descriptors remain authoritative for selected, predicate, ordering,
  distinct, and grouped columns. This phase does not mutate catalog rows,
  descriptor versions, or catalog generation.
- Result builder: result rows and warning counts use the existing result API.
  The implementation may add an internal result found-row field if that is the
  cleanest way to transfer select-specific row accounting to statement
  completion; this is not public ABI.
- Storage/VFS/file format: no file format or VFS changes. File-backed data
  remains in the shifted SQLite payload with the MyLite preamble preserved.
- SQLite: use public SQLite prepare/bind/step/finalize APIs. Do not fork SQLite
  for this slice.

## Syntax

The admitted scalar function subset is:

```ebnf
found_rows_function:
    FOUND_ROWS "(" ")"
```

The admitted select modifier subset is deliberately small:

```ebnf
select_statement:
    SELECT SQL_CALC_FOUND_ROWS table_select_body
  | SELECT ALL SQL_CALC_FOUND_ROWS table_select_body
  | existing_supported_select_statement

table_select_body:
    select_item_list FROM table_name table_alias_opt
      where_clause_opt order_clause_opt limit_clause_opt
  | "*" FROM table_name table_alias_opt
      where_clause_opt order_clause_opt limit_clause_opt
```

The first implementation supports `SQL_CALC_FOUND_ROWS` only for the existing
non-distinct descriptor-backed base-table `SELECT` column-list and wildcard
paths with optional source alias, baseline `WHERE`, one-column `ORDER BY`, and
existing `LIMIT` / `OFFSET` forms. It does not admit or execute
`SQL_CALC_FOUND_ROWS` on scalar/no-source selects, `FROM DUAL`, `DISTINCT`,
`DISTINCTROW`, grouped aggregates, aggregate-only selects, `CREATE TABLE ...
SELECT`, `INSERT ... SELECT`, `REPLACE ... SELECT`, `UNION`, `TABLE`,
subqueries, CTEs, lock clauses, or other select modifiers in this phase. The
later joined-select slice admits the current two-source joined `SELECT`
envelope without changing this single-table design.

### MyLite Lemon-Syntax Snippet

```lemon
expression(A) ::= FOUND_ROWS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_nullary_function(
        state, T, MYLITE_SQL_AST_FOUND_ROWS_FUNCTION, R);
}

select_statement(A) ::=
    SELECT(T) SQL_CALC_FOUND_ROWS select_item_list(B) FROM(F) table_name(N)
    table_alias_opt(AL) where_clause_opt(W) order_clause_opt(O) limit_clause_opt(L). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_table(state, F, N, AL),
        W, NULL, NULL, O, L);
    mylite_sql_ast_node_set_select_calc_found_rows(A, 1);
}

select_statement(A) ::=
    SELECT(T) SQL_CALC_FOUND_ROWS STAR(S) FROM(F) table_name(N)
    table_alias_opt(AL) where_clause_opt(W) order_clause_opt(O) limit_clause_opt(L). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_table(state, F, N, AL),
        W, NULL, NULL, O, L);
    mylite_sql_ast_node_set_select_calc_found_rows(A, 1);
}
```

The implementation may factor these productions through local parser helpers
to avoid duplicating the existing select grammar. The snippet expresses the
admitted MyLite grammar and is independently authored; it is not MySQL grammar.

`FOUND_ROWS` and `SQL_CALC_FOUND_ROWS` must remain unavailable as bare
unquoted identifiers only where MyLite's existing lexer classifies them as
reserved function/modifier syntax. No broader keyword reservation changes are
part of this phase.

## Semantics

### `FOUND_ROWS()`

`FOUND_ROWS()` is a no-source/`FROM DUAL` scalar function in the same scalar
projection domain as `ROW_COUNT()` and `LAST_INSERT_ID()`. It returns the
connection-local found-row value captured at statement start, formatted as an
unsigned decimal text value.

The current `SELECT FOUND_ROWS()` statement itself is still a successful select.
After completion, it updates the connection found-row value to `1`, because the
statement returned one row. This matches MySQL's observed transient behavior.

Each invocation appends MySQL warning `1287`:

```text
FOUND_ROWS() is deprecated and will be removed in a future release. Consider using COUNT(*) instead.
```

The warning count of the result equals the number of `FOUND_ROWS()` calls in
the statement plus any other warnings emitted by evaluated scalar expressions.
`@@warning_count` in the same statement reports the resulting warning count
observed by MySQL for the supported scalar-select surface, including
`FOUND_ROWS()` deprecation warnings.

Unsupported forms:

- arguments to `FOUND_ROWS()`;
- bare `FOUND_ROWS` without parentheses;
- table-backed `FOUND_ROWS()` evaluation;
- `FOUND_ROWS()` in DML assignments, predicates, defaults, generated values, or
  arbitrary expression positions not already admitted by scalar SELECT / `DO`.

### `SQL_CALC_FOUND_ROWS`

For admitted descriptor-backed selects, `SQL_CALC_FOUND_ROWS` computes how many
rows the statement would have returned after descriptor selection, source
resolution, `WHERE`, `DISTINCT` handling if later admitted, grouping if later
admitted, and ordering, but before `LIMIT` / `OFFSET`.

For this phase, only non-distinct non-grouped descriptor-backed table selects
are admitted. Therefore the pre-limit found count is the number of rows that
match the selected table source and optional `WHERE` predicate. `ORDER BY`
does not change the found count.

The regular result set still applies `LIMIT` and `OFFSET`. Successful
`SQL_CALC_FOUND_ROWS` statements append MySQL warning `1287`:

```text
SQL_CALC_FOUND_ROWS is deprecated and will be removed in a future release. Consider using two separate queries instead.
```

`ROW_COUNT()` after the result-set statement remains `-1`.

### Successful SELECT Without `SQL_CALC_FOUND_ROWS`

For supported successful selects without `SQL_CALC_FOUND_ROWS`, MyLite updates
the connection found-row value using the visible result envelope:

- no `LIMIT`: result row count;
- `LIMIT row_count`: result row count;
- `LIMIT row_count OFFSET offset` and `LIMIT offset, row_count`: `offset`
  plus result row count, capped naturally by the matched result envelope.

The implementation should derive this from MyLite's parsed limit values and
the actual result row count, not by inspecting SQLite SQL text.

Non-`SELECT` statements leave found-row state unchanged.

## Diagnostics

Supported success cases report MySQL-compatible warnings:

- `SQL_CALC_FOUND_ROWS` statement warning: code `1287`, SQLSTATE `HY000`;
- each `FOUND_ROWS()` invocation warning: code `1287`, SQLSTATE `HY000`;
- no additional warnings for in-range descriptor-backed selects.

Required errors and deterministic diagnostics:

- `FOUND_ROWS()` argument count: MySQL error `1582`, SQLSTATE `42000`;
- unsupported scalar placement: existing scalar/table-backed unsupported
  diagnostics;
- unsupported `SQL_CALC_FOUND_ROWS` select shape: deterministic
  MyLite-specific unsupported-feature diagnostic unless the parser rejects the
  syntax first;
- syntax errors for malformed `SQL_CALC_FOUND_ROWS` placement;
- existing selected-schema, unknown-schema, unknown-table, reserved-name,
  unknown-column, unsupported object-kind, predicate, order, and limit
  diagnostics are preserved for descriptor-backed selects.

Failed statements do not define new found-row behavior; MySQL documents
behavior after failed `SELECT` as undefined, so MyLite keeps the current
failed-statement policy and does not promise a found-row update.

## Performance

For ordinary selects without `SQL_CALC_FOUND_ROWS`, found-row tracking should
use the result row count and parsed limit envelope already available to MyLite.
It must not materialize additional rows. When MySQL-compatible capping for
positive-offset empty envelopes requires knowing the matched row count, MyLite
may run the same descriptor-built `SELECT COUNT(*)` shape used by
`SQL_CALC_FOUND_ROWS`; it must still avoid reading full rows into MyLite
memory.

For admitted `SQL_CALC_FOUND_ROWS`, the baseline may run a second
descriptor-built `SELECT COUNT(*)` over the same table source and `WHERE`
predicate before or after the visible result query. This stays close to
SQLite's optimized path and avoids materializing the full result in MyLite
memory. It must use stable physical table names, quoted identifiers, prepared
statements, and bound predicate values through existing descriptor-driven
planning helpers. It must not remove the visible query's `LIMIT` or read all
rows into MyLite just to count them.

`DISTINCT`, grouping, and `UNION` found-row behavior are deferred partly
because pre-limit row accounting there must count result rows after duplicate
elimination or grouping, not base rows.

## Tests

Add fast plain C tests, preferably `runtime_found_rows_function_test.c`, plus
parser coverage for the function and admitted modifier forms. The tests must
cover:

- zero-argument `FOUND_ROWS()` in no-source and `FROM DUAL` scalar selects;
- case-insensitive function names and whitespace before `(`;
- warning `1287` per `FOUND_ROWS()` invocation and exact warning text;
- `FOUND_ROWS()` after ordinary descriptor-backed `SELECT` with no `LIMIT`,
  `LIMIT row_count`, `LIMIT 0`, `LIMIT offset,row_count`, and
  `LIMIT row_count OFFSET offset`;
- `FOUND_ROWS()` after `SELECT SQL_CALC_FOUND_ROWS ...` with no `LIMIT`,
  `LIMIT row_count`, `LIMIT 0`, and filtered `WHERE`;
- `SQL_CALC_FOUND_ROWS` warning `1287`, result warning count, `SHOW WARNINGS`,
  and `ROW_COUNT() == -1`;
- successful `FOUND_ROWS()` updating subsequent found-row state to `1`;
- non-`SELECT` statements preserving found-row state;
- schema-qualified and unqualified table resolution through existing selected
  schema policy;
- unknown schema/table/column and unsupported object-kind diagnostics inherited
  from descriptor-backed SELECT;
- unsupported modifier shapes: scalar selects, `FROM DUAL`, `DISTINCT`,
  grouped aggregates, aggregate-only selects, `CREATE TABLE ... SELECT`,
  `INSERT ... SELECT`, `REPLACE ... SELECT`, other select modifiers, joins,
  `UNION`, subqueries, parameters, and lock clauses;
- file-backed reopen behavior and independent handle state;
- no catalog generation, descriptor, SQLite schema generation, file preamble,
  or VFS mutation caused by found-row tracking.

Run:

1. `packages/libmylite/tests/mysql_baseline_found_rows_function_expectations.sh`
2. focused parser/runtime CTest entries
3. `cmake --build --preset dev`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-system.md`, and
`docs/compatibility/sql-query-expressions.md` only for the exact limited
`FOUND_ROWS()` and `SQL_CALC_FOUND_ROWS` subset. Do not claim full SELECT
modifier support, full distinct/grouped found-row behavior, `UNION`, protocol
metadata, replication behavior, `CLIENT_FOUND_ROWS`, joins, subqueries, or
general expression support.
