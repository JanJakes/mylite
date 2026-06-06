# Baseline Joined SELECT SQL_CALC_FOUND_ROWS

## Summary

This phase extends the existing deprecated `SQL_CALC_FOUND_ROWS` and
`FOUND_ROWS()` slice to the current descriptor-backed two-source joined
`SELECT` envelope.

Supported statements are the already admitted two-source joined selects with
`SQL_CALC_FOUND_ROWS`, including the current descriptor `DISTINCT` projection
subset:

```sql
SELECT SQL_CALC_FOUND_ROWS select_list
FROM left_table [alias] JOIN right_table [alias] ON left_col = right_col
[WHERE ...] [ORDER BY ...] [LIMIT ...]

SELECT SQL_CALC_FOUND_ROWS select_list
FROM left_table [alias] LEFT [OUTER] JOIN right_table [alias] ON left_col = right_col
[WHERE ...] [ORDER BY ...] [LIMIT ...]

SELECT SQL_CALC_FOUND_ROWS select_list
FROM left_table [alias], right_table [alias]
[WHERE ...] [ORDER BY ...] [LIMIT ...]
```

The row result remains the normal limited joined result. For non-grouped
joined results, the connection-local found-row state is updated to the count
after source joining and `WHERE`, but before `LIMIT` / `OFFSET`. For the later
grouped joined envelope, the state is updated to the group count before
`LIMIT` / `OFFSET`. The implementation stays descriptor-driven and does not
add new join kinds, expression support, distinct found-row behavior, or
arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - `FOUND_ROWS()` information function:
    <https://dev.mysql.com/doc/refman/8.4/en/information-functions.html>
  - `SELECT` statement and `SQL_CALC_FOUND_ROWS` modifier:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- Observed MySQL 8.4.9 runtime behavior, captured in
  `packages/libmylite/tests/mysql_baseline_found_rows_function_expectations.sh`.

The MySQL 8.4.9 probe for this phase used:

```sql
SELECT VERSION();
DROP DATABASE IF EXISTS mylite_joined_calc_probe;
CREATE DATABASE mylite_joined_calc_probe;
USE mylite_joined_calc_probe;
CREATE TABLE lefts(id INT NOT NULL, k INT NULL, v INT NULL);
CREATE TABLE rights(id INT NOT NULL, k INT NULL, w INT NULL);
INSERT INTO lefts VALUES (1,10,100),(2,20,200),(3,NULL,300);
INSERT INTO rights VALUES (7,10,700),(8,10,800),(9,NULL,900);
SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id
FROM lefts JOIN rights ON lefts.k = rights.k
ORDER BY rights.id LIMIT 1;
SHOW WARNINGS;
SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id
FROM lefts, rights
WHERE lefts.k = rights.k
ORDER BY rights.id LIMIT 0;
SHOW WARNINGS;
SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id
FROM lefts, rights
ORDER BY lefts.id, rights.id LIMIT 2;
SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id
FROM lefts LEFT JOIN rights ON lefts.k = rights.k
ORDER BY lefts.id, rights.id LIMIT 2;
SHOW WARNINGS;
SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
SELECT lefts.id, rights.id
FROM lefts JOIN rights ON lefts.k = rights.k
ORDER BY rights.id LIMIT 1, 1;
SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
DROP DATABASE mylite_joined_calc_probe;
```

Observed expectations:

- inner joined `SQL_CALC_FOUND_ROWS ... LIMIT 1` returns the limited row and
  sets `FOUND_ROWS()` to `2`;
- comma joined `SQL_CALC_FOUND_ROWS ... WHERE ... LIMIT 0` returns no visible
  rows and sets `FOUND_ROWS()` to `2`;
- cartesian comma joined `SQL_CALC_FOUND_ROWS ... LIMIT 2` returns the limited
  joined rows and sets `FOUND_ROWS()` to `9`;
- left joined `SQL_CALC_FOUND_ROWS ... LIMIT 2` returns the first two joined
  rows and sets `FOUND_ROWS()` to `4`, including the left-side rows that have
  no matching right row;
- a joined `SELECT` without `SQL_CALC_FOUND_ROWS` keeps the existing visible
  `LIMIT` envelope accounting: `LIMIT 1, 1` over two matched rows leaves
  `FOUND_ROWS()` as `2`;
- each successful `SQL_CALC_FOUND_ROWS` joined result records MySQL warning
  `1287`; a subsequent `FOUND_ROWS()` scalar select records its own warning
  and leaves `ROW_COUNT()` at `-1`.

## Ownership Boundaries

- Public API: unchanged. Joined `SELECT` returns through the existing
  `mylite_result` row-result conventions.
- Statement context: unchanged. Successful selects publish the result object's
  found-row count into the connection-local state during statement completion.
- Parser/AST: no new tokens or AST fields are needed. The parser already stores
  the independent `SQL_CALC_FOUND_ROWS` select flag on joined select statements.
- Analyzer/planner: remove the current joined-select rejection and keep all
  existing joined-source, projection, predicate, ordering, and limit planning
  rules. Joined `DISTINCT SQL_CALC_FOUND_ROWS` keeps the existing joined
  distinct projection rules and counts distinct projected rows through the
  found-rows count path.
- Catalog: table and column descriptors remain authoritative. The feature does
  not read SQLite schema text for logical names and does not mutate descriptors,
  catalog generation, table status, or schema generation.
- Result builder: row data, column labels, warning counts, and found-row state
  use the existing result builder. Successful `SQL_CALC_FOUND_ROWS` adds the
  same deprecation warning as the single-table slice.
- Storage/VFS/file format: no file format or VFS change. The `.mylite` preamble
  and shifted SQLite payload invariants are unaffected.
- SQLite: use public prepare/bind/step/finalize APIs. No SQLite fork patch or
  optional SQLite `UPDATE`/`DELETE` extension syntax is involved.

## Syntax

This phase does not widen joined `SELECT` grammar beyond admitting an already
parsed select modifier on the existing joined-source envelope.

```ebnf
joined_sql_calc_select:
    SELECT SQL_CALC_FOUND_ROWS joined_select_body
  | SELECT ALL SQL_CALC_FOUND_ROWS joined_select_body
  | SELECT existing_supported_noop_modifiers SQL_CALC_FOUND_ROWS joined_select_body

joined_select_body:
    select_item_list FROM joined_source joined_select_clauses_opt

joined_source:
    table_factor join_operator table_factor ON equality_join_condition
  | table_factor "," table_factor

joined_select_clauses_opt:
    existing optional WHERE, ORDER BY, LIMIT clauses admitted by joined SELECT
```

### MyLite Lemon-Syntax Snippet

The intended grammar shape is the existing select-modifier production feeding
the existing joined-source production:

```lemon
select_statement(A) ::=
    SELECT(T) select_modifier_list_opt(M) select_item_list(P)
    FROM(F) joined_table_reference(J) select_optional_clauses_opt(C). {
    A = mylite_sql_parser_make_select_statement(state, T, P, J, C);
    mylite_sql_parser_apply_select_modifiers(state, A, M);
}

select_modifier(M) ::= SQL_CALC_FOUND_ROWS(T). {
    M = mylite_sql_parser_make_select_modifier(
        state, T, MYLITE_SQL_AST_SELECT_MODIFIER_SQL_CALC_FOUND_ROWS);
}
```

The snippet describes MyLite's intended parser shape independently. It is not
copied from MySQL grammar.

## Semantics

For supported joined selects, `SQL_CALC_FOUND_ROWS` counts the rows produced by
the planned joined source and optional `WHERE` predicate before `LIMIT` /
`OFFSET`. `ORDER BY` is allowed when the current joined-select planner supports
it, but ordering does not change the found-row count.

For inner joins and comma joins, the count is the number of matching joined
rows. For left outer joins, unmatched left-side rows count as one row each when
they survive the optional `WHERE` predicate, matching MySQL's observed behavior.
For joined `DISTINCT SQL_CALC_FOUND_ROWS`, the count is the number of distinct
projected result rows before `LIMIT`, not the number of raw joined rows.

The visible row set is still produced by the ordinary joined `SELECT` plan and
then limited by the existing `LIMIT` / `OFFSET` handling. Without
`SQL_CALC_FOUND_ROWS`, joined `SELECT` continues to update `FOUND_ROWS()` from
the existing visible `LIMIT` envelope rules.

The feature relies on the existing descriptor-built physical SQL shape:

```sql
SELECT COUNT(*)
FROM "_mylite_user_table_<left_id>" AS "_mylite_s0"
[JOIN|LEFT JOIN] "_mylite_user_table_<right_id>" AS "_mylite_s1"
  [ON "_mylite_s0"."left_col" = "_mylite_s1"."right_col"]
[WHERE descriptor-built predicates with bound values]
```

Physical table and column identifiers are quoted. Predicate values remain bound
parameters. No user SQL literal is interpolated into generated SQLite SQL.

## Diagnostics

This phase should not introduce new public error shapes except removing the
previous MyLite-specific rejection for joined `SQL_CALC_FOUND_ROWS`.

Existing diagnostics remain authoritative:

- syntax errors and unsupported joined syntax use the current parser/planner
  diagnostics;
- unknown databases, tables, columns, ambiguous columns, unsupported join
  predicates, unsupported `WHERE`, unsupported `ORDER BY`, unsupported `LIMIT`,
  unsupported `DISTINCT`, unsupported grouped/aggregate-only
  `SQL_CALC_FOUND_ROWS`, CTAS, `INSERT ... SELECT`, `REPLACE ... SELECT`,
  `UNION`, `TABLE`, CTEs, and subqueries keep their existing errors;
- physical SQLite failures are mapped through existing runtime diagnostics;
- allocation failures keep existing `MYLITE_NOMEM` handling.

Successful supported joined `SQL_CALC_FOUND_ROWS` statements record warning
`1287` with the existing SQL_CALC deprecation message and no additional
warnings. `FOUND_ROWS()` scalar reads keep their own existing deprecation
warning behavior.

## Performance

The implementation does not materialize joined rows in MyLite to count them.
It performs the normal joined select for visible rows and, when
`SQL_CALC_FOUND_ROWS` is present or an offset envelope needs total matching
rows, executes a descriptor-built SQLite `COUNT(*)` over the same joined source
and predicate. This duplicates the source scan only for the deprecated modifier
or existing offset accounting. It keeps filtering and joining inside SQLite and
does not add MyLite-side sorting, joining, or row buffering.

## Tests

Extend the existing MySQL expectation script and runtime C test:

- MySQL 8.4.9 expectation artifact for inner, comma, and left joined
  `SQL_CALC_FOUND_ROWS`;
- limited visible rows plus pre-limit found-row state;
- `LIMIT 0` count behavior;
- left join unmatched-row count behavior;
- ordinary joined select without `SQL_CALC_FOUND_ROWS` still uses the visible
  `LIMIT` envelope;
- warning count and warning rows for `SQL_CALC_FOUND_ROWS` and `FOUND_ROWS()`;
- preservation of existing unsupported forms, including distinct/grouped,
  CTAS, insert-source, replace-source, union, scalar/no-source, and row-scalar
  paths;
- existing joined select and found-rows regression suites still pass.

## Compatibility Notes

This phase supports only the current two-source descriptor-backed joined
`SELECT` surface. It does not claim MySQL's full deprecated found-rows behavior
for distinct joins, grouped joins, aggregate joins, derived tables, subqueries,
CTEs, `UNION`, recursive constructs, locking options, protocol metadata,
statement-based replication, or optimizer interaction.
