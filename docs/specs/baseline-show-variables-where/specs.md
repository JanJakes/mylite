# Baseline SHOW VARIABLES WHERE

## Summary

This phase extends the existing runtime-owned `SHOW VARIABLES` statement with a
limited `WHERE` filter over the statement's output columns:

```sql
SHOW [GLOBAL | SESSION | LOCAL] VARIABLES WHERE predicate
```

It also adds the common GTID variables that WordPress-oriented test harnesses
probe: `gtid_executed`, `gtid_mode`, `gtid_owned`, and `gtid_purged`.

The statement remains entirely MyLite-owned metadata. It does not query SQLite,
does not expose the full MySQL variable catalog, and does not add mutable GTID
state, replication, Performance Schema, privilege checks, or arbitrary
expression evaluation.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - `SHOW VARIABLES`: <https://dev.mysql.com/doc/refman/8.4/en/show-variables.html>
  - server system variables: <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_show_variables_where_expectations.sh`.

Runtime probes against MySQL 8.4.9 establish these expectations for this
slice:

- `SHOW VARIABLES WHERE Variable_name = 'autocommit'` returns the
  `autocommit` row;
- comparisons against `Variable_name` and `Value` are case-insensitive for the
  ASCII values in this fixed registry;
- backticked output column names such as `` `Variable_name` `` are accepted;
- qualified output column names such as `variables.Variable_name` are rejected
  as unknown columns in the `WHERE` clause;
- `WHERE` can use `AND`, `OR`, parentheses, `NOT`, comparisons, null-safe
  equality, `LIKE`, `NOT LIKE`, `IN`, `NOT IN`, `IS NULL`, and `IS NOT NULL`;
- `SHOW VARIABLES LIKE 'a%' WHERE ...`, `ORDER BY`, and `LIMIT` remain syntax
  errors for this statement;
- successful supported `WHERE` filters leave warning count `0`, error count
  `0`, and make `ROW_COUNT()` return `-1`;
- comparing output strings to numeric literals succeeds in MySQL but produces a
  warning per compared row. MyLite deliberately defers that warning-producing
  conversion surface in this slice and rejects non-string filter literals
  deterministically.

## Ownership Boundaries

- Public API: no ABI or public-header change. `mylite_execute()` returns a
  normal row result through the existing result handle.
- Statement context: successful `SHOW VARIABLES WHERE` is a result-producing
  statement. It reports affected rows `0`, warning count `0`, and updates the
  previous row count to `-1`.
- Lexer/parser/AST: the parser extends the existing `SHOW VARIABLES` node to
  accept either a string-only `LIKE` filter or a `WHERE` clause. The `WHERE`
  clause reuses the current MyLite predicate AST, but runtime admits only the
  subset specified here.
- Runtime/analyzer: runtime resolves the optional scope, iterates the fixed
  system-variable registry, builds candidate `Variable_name` and `Value` text
  pairs, evaluates the admitted predicate subset, and appends matching rows.
- Catalog: not involved. System variables are runtime metadata, not schema or
  table descriptors.
- Result builder: result rows are built directly with the existing result API.
- Storage/VFS/file format: no file reads beyond normal handle state, no writes,
  and no `.mylite` preamble change.
- SQLite physical storage: no generated SQLite SQL and no SQLite fork patch.

## Syntax

The independent MyLite subset is:

```ebnf
show_variables_statement:
    SHOW show_variables_scope_opt VARIABLES show_variables_filter_opt

show_variables_scope_opt:
    empty
  | GLOBAL
  | SESSION
  | LOCAL

show_variables_filter_opt:
    empty
  | LIKE string_literal
  | WHERE show_variables_predicate
```

`LIKE` and `WHERE` are mutually exclusive. The grammar intentionally excludes
`FULL`, `FROM`, `IN` schema clauses, `ORDER BY`, `LIMIT`, and `SHOW VARIABLES`
filters over hidden metadata.

### MyLite Lemon-Syntax Snippet

```lemon
show_variables_statement(A) ::=
    SHOW(S) show_variables_scope_opt(O) VARIABLES(V)
    show_variables_filter_opt(F). {
    A = mylite_sql_parser_make_show_variables_statement(state, S, O, V, F);
}

show_variables_filter_opt(A) ::= . {
    A = NULL;
}
show_variables_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}
show_variables_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## WHERE Predicate Subset

The `WHERE` predicate is evaluated against the visible `SHOW VARIABLES` result
row. The only admitted columns are the two output columns:

- `Variable_name`
- `Value`

Column names are resolved ASCII case-insensitively. Backtick quoting does not
change the name. Qualified column references are not admitted and are reported
with MySQL-compatible unknown-column diagnostics for this statement.

The admitted predicate subset is:

- `column = string_literal`
- `column <=> string_literal`
- `column <> string_literal`
- `column != string_literal`
- `column < string_literal`
- `column <= string_literal`
- `column > string_literal`
- `column >= string_literal`
- `column <=> NULL`, which matches no rows because these output columns are
  never SQL `NULL`
- `column LIKE string_literal`
- `column NOT LIKE string_literal`
- `column IN (string_literal_or_NULL [, ...])`
- `column NOT IN (string_literal_or_NULL [, ...])`
- `column IS NULL`
- `column IS NOT NULL`
- parenthesized predicates
- `NOT predicate`
- `predicate AND predicate`
- `predicate OR predicate`

String comparisons use MySQL-compatible case-insensitive ordering for the
ASCII names and values in this fixed registry. `LIKE` uses the existing
baseline SHOW pattern matcher: `%` matches any byte sequence, `_` matches one
byte, and backslash escapes the next byte. Matching is ASCII case-insensitive.

`IN` and `NOT IN` follow SQL null propagation for this limited row model.
Because `Variable_name` and `Value` are never SQL `NULL`, `IN (NULL, 'x')`
matches only rows equal to `'x'`, while `NOT IN (NULL, 'x')` matches no rows
that are not equal to `'x'`.

The following are intentionally outside this slice and must fail
deterministically instead of being approximated:

- numeric, decimal, float, hex, bit, boolean, national-string, introducer, or
  parameter literals in `WHERE`;
- column-to-column comparisons;
- functions such as `LOWER(Variable_name)`;
- `BETWEEN`, `REGEXP`, `RLIKE`, `XOR`, `IS TRUE`, `IS FALSE`, and arbitrary
  expression predicates;
- subqueries and CTEs;
- `ORDER BY`, `LIMIT`, and `LIKE ... WHERE` combinations.

## GTID Variable Rows

This phase adds fixed GTID placeholder variables because applications commonly
probe them when adapting to MySQL server capabilities.

| Variable | Default/session/LOCAL SHOW visibility | GLOBAL SHOW visibility | SHOW value | Scalar read scope |
| --- | --- | --- | --- | --- |
| `gtid_executed` | yes | yes | empty string | default and `GLOBAL`; `SESSION`/`LOCAL` raise global-variable diagnostic |
| `gtid_mode` | yes | yes | `OFF` | default and `GLOBAL`; `SESSION`/`LOCAL` raise global-variable diagnostic |
| `gtid_owned` | yes | yes | empty string | default, `GLOBAL`, `SESSION`, and `LOCAL` |
| `gtid_purged` | yes | yes | empty string | default and `GLOBAL`; `SESSION`/`LOCAL` raise global-variable diagnostic |

The scalar values match the embedded no-GTID baseline. `gtid_mode` returns
`OFF`; the other GTID variables return empty strings. GTID variables are
read-only in this phase. MyLite does not implement binary logging, GTID sets,
replication channels, `SET GLOBAL gtid_purged`, GTID consistency enforcement,
or privilege semantics.

`lower_case_table_names` is also part of the fixed registry after the
baseline-lower-case-table-names-system-variable slice. It has default,
session/local `SHOW`, and global `SHOW` visibility, displays `0`, permits
default/global scalar reads, and rejects session/local scalar reads as a global
variable. The `SHOW VARIABLES WHERE` predicate evaluates over this row like any
other fixed text row; it does not change MyLite's current case-sensitive
catalog name-resolution behavior.

## Scope Semantics

No explicit scope, `SESSION`, and `LOCAL` use the existing session-visible
`SHOW VARIABLES` behavior. `GLOBAL` includes only rows visible through global
introspection. The `WHERE` predicate is evaluated after scope filtering, so
session-only rows such as `warning_count` are unavailable to
`SHOW GLOBAL VARIABLES WHERE ...`.

## Result Semantics

Successful statements return a row result with columns:

```text
Variable_name
Value
```

Rows contain text values only. Output order remains the existing supported
registry order, bytewise lowercase variable-name order. Filtering does not
change ordering.

Successful statements report:

- affected rows `0`;
- warning count `0`;
- no stored warnings for the supported in-range subset;
- `ROW_COUNT()` as `-1` for the following statement.

The statement does not change fixed system-variable state, session character
set state, diagnostics state beyond normal statement-boundary cleanup, catalog
state, SQLite schema generation, or storage contents.

## Diagnostics

- Unknown `WHERE` columns use MySQL-compatible `1054` / `42S22` diagnostics
  with the clause name `where clause`.
- Qualified `WHERE` columns are treated as unknown columns in the `WHERE`
  clause for this statement.
- Unsupported predicate shapes use MyLite's existing unsupported-syntax
  diagnostic policy, currently `1064` / `42000`.
- Unsupported non-string or non-`NULL` predicate literals use a deterministic
  unsupported predicate diagnostic rather than MySQL's warning-producing
  runtime conversion behavior.
- Unsupported statement clauses such as `ORDER BY`, `LIMIT`, and
  `LIKE ... WHERE` remain parser syntax errors.
- Allocation failures use the existing `MYLITE_NOMEM` / out-of-memory
  diagnostic policy.
- Physical SQLite failures are not expected because this statement generates no
  SQLite SQL. Any unexpected SQLite involvement is a bug in this phase.

## Performance

The implementation remains O(number of supported variables) over a small
static registry. It does not materialize a temporary table, does not invoke the
SQLite SQL engine, and does not allocate per-row expression trees at execution
time. Runtime evaluates the already parsed predicate directly against each
visible candidate row.

## Tests

Fast C tests cover:

- parsing `SHOW VARIABLES WHERE ...` and rejecting unsupported clauses;
- equality, case-insensitive comparisons, backticked output columns,
  comparisons, null-safe equality, `LIKE`, `NOT LIKE`, `IN`, `NOT IN`,
  `IS NULL`, `IS NOT NULL`, `AND`, `OR`, `NOT`, and parentheses;
- filtering by `Variable_name` and `Value`;
- default, `GLOBAL`, `SESSION`, and `LOCAL` scope interactions;
- GTID variable rows and scalar reads;
- unknown and qualified output columns;
- deterministic rejection of unsupported predicate shapes and non-string
  literals;
- result metadata, warning count, affected rows, absence of storage changes,
  and `ROW_COUNT()` behavior;
- independent handles and file-backed handles remain unaffected.

The MySQL expectation script records the MySQL 8.4.9 behavior used for this
slice and must pass before the implementation is treated as complete.
