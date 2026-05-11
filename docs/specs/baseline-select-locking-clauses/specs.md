# Baseline SELECT Locking Clauses

## Summary

This slice adds a deliberately small MySQL-compatible `SELECT` locking-clause
surface on top of the existing scalar, descriptor-backed, aggregate, grouped,
`INSERT ... SELECT`, and `REPLACE ... SELECT` paths.

The supported clauses are parsed, represented in the AST, and executed as
embedded no-ops:

- `FOR UPDATE`
- `FOR SHARE`
- `LOCK IN SHARE MODE`

MyLite does not yet implement explicit transactions, row locks, gap locks,
metadata locks, privilege checks, replication warnings, or blocking lock
behavior. The clauses therefore do not change generated SQLite SQL, row
selection, diagnostics, affected rows, `ROW_COUNT()`, `FOUND_ROWS()`, or
warning count for successful statements. They exist so MySQL-oriented clients
that emit simple locking reads can continue through the currently supported
query paths.

## Sources

Compatibility is based on:

- the official MySQL 8.4 `SELECT` statement reference, especially the locking
  clause placement and supported clause families:
  `https://dev.mysql.com/doc/refman/8.4/en/select.html`;
- the official MySQL 8.4 InnoDB locking reads reference:
  `https://dev.mysql.com/doc/refman/8.4/en/innodb-locking-reads.html`;
- observed MySQL 8.4.9 runtime behavior captured in
  `packages/libmylite/tests/mysql_baseline_select_locking_clauses_expectations.sh`.

No MySQL or SQLite implementation source is used.

## Ownership Boundaries

- Public API: no ABI or public header changes.
- Statement context and diagnostics: successful locking-clause statements reuse
  the existing result and diagnostics behavior for their underlying statement;
  unsupported forms use existing parse or unsupported-statement diagnostics.
- Lexer/parser/AST: owns recognition and preservation of the admitted locking
  clause kind. It does not interpret table descriptors or lower to SQLite.
- Runtime/analyzer/planner: validates where locking clauses are allowed for the
  current MyLite subset and otherwise ignores admitted clause kinds.
- Catalog: descriptors remain authoritative for table, column, aggregate,
  `WHERE`, `ORDER BY`, and `LIMIT` resolution. Locking clauses do not mutate
  catalog rows, descriptor versions, catalog generation, or SQLite schema
  generation.
- Result builder: returns the same row result or DML result shape the underlying
  statement would return without the locking clause.
- Storage/VFS and SQLite physical storage: no file-format, VFS, preamble,
  SQLite payload, physical table, generated SQL, transaction, or SQLite fork
  changes. This slice is a MyLite-side parser/runtime no-op.

## Syntax

The admitted MyLite grammar subset is independent and intentionally smaller
than MySQL's full query-expression grammar:

```lemon
select_statement ::=
    SELECT select_modifiers select_item_list select_locking_clause_opt
  | SELECT select_modifiers select_item_list FROM DUAL select_locking_clause_opt
  | SELECT select_modifiers select_item_list FROM table_name table_alias_opt
        where_clause_opt group_clause_opt having_clause_opt order_clause_opt
        limit_clause_opt select_locking_clause_opt
  | SELECT select_modifiers STAR select_locking_clause_opt
  | SELECT select_modifiers STAR FROM DUAL select_locking_clause_opt
  | SELECT select_modifiers STAR FROM table_name table_alias_opt
        where_clause_opt group_clause_opt having_clause_opt order_clause_opt
        limit_clause_opt select_locking_clause_opt.

select_locking_clause_opt ::= .
select_locking_clause_opt ::= FOR UPDATE.
select_locking_clause_opt ::= FOR SHARE.
select_locking_clause_opt ::= LOCK IN SHARE MODE.
```

The clause appears after the currently supported `ORDER BY` and `LIMIT` tails.
MyLite does not support `SELECT ... INTO`, so no additional `INTO` placement is
introduced.

## Supported Statements

The admitted simple locking clauses are supported as no-ops on:

- no-source scalar/session-value `SELECT`;
- `SELECT ... FROM DUAL`;
- descriptor-backed table column-list and wildcard `SELECT`;
- limited `SELECT DISTINCT` / `DISTINCTROW`;
- existing one-item aggregate and grouped aggregate `SELECT` forms;
- existing `SQL_CALC_FOUND_ROWS` descriptor-backed table forms;
- existing `INSERT ... SELECT` and `REPLACE ... SELECT` source reads.

`CREATE TABLE ... SELECT ... FOR UPDATE|SHARE|LOCK IN SHARE MODE` remains
rejected. MySQL 8.4.9 rejects this shape with error `1746`, SQLSTATE `HY000`,
and a message saying the source table cannot be updated while the target table
is being created. MyLite should use that diagnostic when it can identify the
single source descriptor table and the target table does not already exist;
otherwise it preserves normal target-exists handling, including
`CREATE TABLE IF NOT EXISTS ... SELECT` no-op behavior for an existing target.
It may fall back to a deterministic unsupported-statement diagnostic for
malformed or unsupported source shapes.

## Semantics

For supported statements:

- returned rows, row order, distinctness, aggregate values, grouped rows,
  `WHERE`, `ORDER BY`, `LIMIT`, and `OFFSET` behavior are identical to the same
  statement without the locking clause;
- no additional warnings or notes are produced;
- `ROW_COUNT()` behavior follows the underlying statement (`-1` after
  successful `SELECT`; existing affected-row behavior after DML);
- `FOUND_ROWS()` behavior is unchanged for existing `SQL_CALC_FOUND_ROWS`
  paths;
- DML source locking clauses do not affect target insert/replace behavior,
  defaults, nullability, range checking, or statement atomicity.

The no-op choice is explicitly a current embedded compatibility baseline. It is
not a claim that MyLite has MySQL row-locking semantics.

## Unsupported Forms

This slice does not admit:

- `FOR UPDATE OF table_name`;
- `FOR SHARE OF table_name`;
- `NOWAIT`;
- `SKIP LOCKED`;
- multiple locking clauses in one query block;
- locking clauses before `WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`, or `LIMIT`;
- locking clauses in `CREATE TABLE ... SELECT`;
- joins, subqueries, CTEs, set operations, `TABLE`, `VALUES`, locking of
  derived tables, or nested query blocks;
- transaction wait behavior, lock conflict errors, privileges, grant-table
  exceptions, statement-based replication warnings, or isolation-level
  interactions.

MySQL 8.4.9 accepts `NOWAIT`, `SKIP LOCKED`, and `OF table_name`; MyLite
intentionally leaves them unsupported until it has explicit transaction and
locking semantics. MySQL also returns error `3569` for multiple locking clauses
that apply to the same table; MyLite may reject repeated locking clauses as a
syntax error for this narrow slice.

## Diagnostics

Expected diagnostics:

| Condition | Diagnostic |
| --- | --- |
| Syntax errors or unsupported clause placement | Existing parser syntax error `1064`, SQLSTATE `42000` |
| `NOWAIT`, `SKIP LOCKED`, `OF table_name` | Deterministic syntax or unsupported diagnostic |
| Multiple locking clauses | Deterministic syntax or unsupported diagnostic; MySQL exact `3569` may be deferred |
| `CREATE TABLE ... SELECT ... locking_clause` with one descriptor source table | Error `1746`, SQLSTATE `HY000`, message naming the source and target tables |
| `CREATE TABLE ... SELECT ... locking_clause` with an existing target table | Existing target diagnostics or `IF NOT EXISTS` no-op note take precedence |
| Unknown schema/table/column in the underlying statement | Existing descriptor-driven diagnostics |
| Unsupported object kind once non-base descriptors exist | Existing unsupported-object diagnostic |
| Allocation failure | Existing `MYLITE_NOMEM` / out-of-memory diagnostic |
| Physical SQLite failure | Existing SQLite execution diagnostic for the underlying statement |

Successful admitted locking clauses produce `warning_count == 0` unless the
underlying statement already produces warnings, such as `SQL_NO_CACHE`,
`SQL_CALC_FOUND_ROWS`, `FOUND_ROWS()`, or deprecated `&&` / `||` predicates.

## Implementation Notes

- Add a small AST payload field or equivalent selector for the locking-clause
  kind. Keep this internal to the SQL AST.
- Do not add public result or handle fields.
- Do not include locking-clause text in generated SQLite SQL.
- Do not materialize extra rows in memory. The existing descriptor-built SQLite
  plans should run exactly as before.
- Reject `CREATE TABLE ... SELECT` locking clauses before creating the target
  descriptor or physical table, after target existence / `IF NOT EXISTS`
  precedence has been preserved.
- `INSERT ... SELECT` and `REPLACE ... SELECT` may ignore admitted source
  locking clauses after source planning succeeds.
- Preserve zero-initialized cleanup behavior for any touched statement/planner
  objects.

## Tests

Add MySQL-runtime expectation coverage for:

- simple `FOR UPDATE`, `FOR SHARE`, and `LOCK IN SHARE MODE` table selects;
- no-source and `DUAL` scalar selects with locking clauses;
- one-item aggregate, grouped aggregate, distinct, and `SQL_CALC_FOUND_ROWS`
  table selects with locking clauses;
- `INSERT ... SELECT ... FOR UPDATE` and
  `REPLACE ... SELECT ... FOR SHARE`;
- `CREATE TABLE ... SELECT ... FOR UPDATE` rejection;
- MySQL-accepted but MyLite-deferred `NOWAIT`, `SKIP LOCKED`, and `OF table`;
- multiple locking clauses and misplaced clauses.

Add fast C tests under `packages/libmylite/tests/`, preferably a new
`runtime_select_locking_clauses` test binary if clearer than extending existing
tests. Cover:

- parser acceptance and AST preservation for all admitted clauses;
- successful no-op behavior over scalar, `DUAL`, descriptor table, aggregate,
  grouped, distinct, `SQL_CALC_FOUND_ROWS`, insert-select, and replace-select
  paths;
- no extra warnings for admitted clauses;
- no result-set shape changes;
- `CREATE TABLE ... SELECT ... FOR UPDATE` diagnostic and no target table
  creation side effect;
- unsupported `NOWAIT`, `SKIP LOCKED`, `OF`, repeated clauses, and misplaced
  clauses;
- existing parser/runtime lifecycle tests still pass.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md` locking-clause row from unsupported to limited/partial;
- `docs/compatibility/sql-query-expressions.md` locking-clause row;
- `docs/compatibility/sql-table-dml.md` `INSERT ... SELECT` and
  `REPLACE ... SELECT` notes only for the exact admitted source locking-clause
  subset.
