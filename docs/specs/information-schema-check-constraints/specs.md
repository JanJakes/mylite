# INFORMATION_SCHEMA.CHECK_CONSTRAINTS

## Scope

This feature adds the first executable MyLite slice for:

- `SELECT * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS`

MyLite records CHECK constraints accepted by supported `CREATE TABLE` and
CHECK-only `ALTER TABLE` statements in a CHECK catalog, exposes them through the
MySQL-compatible
`INFORMATION_SCHEMA.CHECK_CONSTRAINTS` read-only system view, and enforces the
cataloged expression subset covered by the dedicated
[CHECK constraints spec](../check-constraints/specs.md).

Wildcard selection remains the baseline row-shape requirement for
`INFORMATION_SCHEMA.CHECK_CONSTRAINTS`. Broader projections, filters, aliases,
ordering, limits, and aggregates are handled by the composable
information-schema system-view path where the corresponding `SELECT` feature is
implemented.

## Sources

- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` `CHECK_CONSTRAINTS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-check-constraints-table.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA` table reference:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html
- MySQL 8.4 Reference Manual, constraint `INFORMATION_SCHEMA` tables:
  https://dev.mysql.com/doc/refman/8.4/en/constraint-information-schema.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 Behavior Summary

`INFORMATION_SCHEMA.CHECK_CONSTRAINTS` reports CHECK constraints defined for
visible schemas. The table has four columns:

1. `CONSTRAINT_CATALOG`
2. `CONSTRAINT_SCHEMA`
3. `CONSTRAINT_NAME`
4. `CHECK_CLAUSE`

Observed metadata for an empty result set:

- `CONSTRAINT_CATALOG`: `VAR_STRING`, `latin1_swedish_ci`, length `64`, flags
  `NOT_NULL`, `UNIQUE_KEY`, `BINARY`, `NO_DEFAULT_VALUE`, `PART_KEY`
- `CONSTRAINT_SCHEMA`: `VAR_STRING`, `latin1_swedish_ci`, length `64`, flags
  `NOT_NULL`, `BINARY`, `NO_DEFAULT_VALUE`, `PART_KEY`
- `CONSTRAINT_NAME`: `VAR_STRING`, `latin1_swedish_ci`, length `64`, flags
  `NOT_NULL`, `NO_DEFAULT_VALUE`, `PART_KEY`
- `CHECK_CLAUSE`: `BLOB`, `latin1_swedish_ci`, length `4294967295`, flags
  `NOT_NULL`, `BLOB`, `BINARY`, `NO_DEFAULT_VALUE`

Runtime probes:

- A schema containing a normal table without CHECK constraints returned zero
  rows for `CHECK_CONSTRAINTS`.
- `SHOW FULL TABLES FROM information_schema LIKE 'check_constraints'` returned
  `CHECK_CONSTRAINTS`, `SYSTEM VIEW`.
- A table with `a INT CHECK (a > 0)` and
  `CONSTRAINT chk_b CHECK (b < 10) NOT ENFORCED` returned two
  `CHECK_CONSTRAINTS` rows. MySQL generated the unnamed constraint as
  `checks_probe_chk_1`; `CHECK_CLAUSE` rendered as ``(`a` > 0)`` for the
  inline constraint and ``(`b` < 10)`` for the named constraint. The matching
  `INFORMATION_SCHEMA.TABLE_CONSTRAINTS` rows used `CONSTRAINT_TYPE='CHECK'`
  and `ENFORCED='YES'` or `ENFORCED='NO'`.

## MyLite Behavior

Supported executable query:

```sql
SELECT * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS
```

The schema and table identifiers are resolved case-insensitively, including
quoted qualified forms such as:

```sql
SELECT * FROM `information_schema`.`CHECK_CONSTRAINTS`
```

The result columns are exactly:

1. `CONSTRAINT_CATALOG`
2. `CONSTRAINT_SCHEMA`
3. `CONSTRAINT_NAME`
4. `CHECK_CLAUSE`

The query returns rows for CHECK constraints recorded by supported
`CREATE TABLE` and CHECK-only `ALTER TABLE` statements. `CONSTRAINT_CATALOG` is
`def`, `CONSTRAINT_SCHEMA` is the table schema, `CONSTRAINT_NAME` is the
explicit constraint name or the generated `<table>_chk_<n>` name, and
`CHECK_CLAUSE` is MySQL-shaped expression text for the supported simple
expression forms.

`INFORMATION_SCHEMA.TABLES` exposes `CHECK_CONSTRAINTS` with
`TABLE_SCHEMA='information_schema'`, `TABLE_NAME='CHECK_CONSTRAINTS'`, and
`TABLE_TYPE='SYSTEM VIEW'`. The existing `SHOW TABLES FROM information_schema`
and `SHOW FULL TABLES FROM information_schema` inventory also exposes
`CHECK_CONSTRAINTS`; `SHOW FULL TABLES FROM information_schema LIKE
'check_constraints'` returns `CHECK_CONSTRAINTS`, `SYSTEM VIEW`.

### Composable Query Shapes

The following forms are covered by the shared system-view `SELECT` path after
the composable information-schema update:

- `SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS`
- `SELECT DISTINCT * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS`
- `SELECT ALL * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS`
- `SELECT * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS WHERE CONSTRAINT_NAME = 'chk'`
- `SELECT * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS ORDER BY CONSTRAINT_NAME`
- `SELECT * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS LIMIT 1`
- `SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS`
- `SELECT * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS AS cc`
- `SELECT * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS cc`
- `SELECT INFORMATION_SCHEMA.CHECK_CONSTRAINTS.* FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS`
- joins involving `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`

Unqualified `SELECT * FROM CHECK_CONSTRAINTS` is not part of this feature.

## Grammar

No new SELECT grammar is needed. The existing qualified table-reference grammar
must continue accepting ordinary, mixed-case, and quoted identifiers for this
information-schema table. The supported surface can be described with this
MyLite-authored Lemon-style snippet:

```lemon
information_schema_wildcard_select ::= SELECT STAR FROM qualified_table_name.

qualified_table_name ::= identifier DOT identifier.
```

Runtime validation narrows the accepted parsed statement to
`information_schema.check_constraints`, an unqualified wildcard projection, no
explicit duplicate modifier, no alias, and no additional SELECT clauses.

## Storage And Runtime

This feature adds `__mylite_check_constraint_catalog` and the temporary-table
counterpart used by temporary CHECK metadata, enforcement, and table-drop
cleanup. Runtime lowering reads the persistent catalog for
`INFORMATION_SCHEMA.CHECK_CONSTRAINTS`; temporary CHECK rows are used by the
temporary-table DML enforcement path.

The CHECK catalog is the shared source for
`INFORMATION_SCHEMA.CHECK_CONSTRAINTS` and CHECK rows in
`INFORMATION_SCHEMA.TABLE_CONSTRAINTS`.

## Tests

Parser coverage:

- `SELECT * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS`
- lower-case and mixed-case qualified names
- quoted qualified name
- projection and `WHERE` forms parse successfully for runtime rejection

Runtime coverage:

- empty database returns zero rows with the exact four uppercase column names
- creating a normal table without CHECK constraints still returns zero rows
- `CREATE TABLE` with inline and table-level CHECK constraints creates
  `CHECK_CONSTRAINTS` rows with MySQL 8.4.9-verified generated names,
  check-clause text, and enforcement flags
- `ALTER TABLE ... ADD CHECK` creates `CHECK_CONSTRAINTS` rows with generated
  names and check-clause text
- `ALTER TABLE ... DROP CHECK` removes CHECK metadata
- lower-case, mixed-case, and quoted table references execute
- `INFORMATION_SCHEMA.TABLES` exposes the `CHECK_CONSTRAINTS` system-view row
- `SHOW TABLES FROM information_schema LIKE 'check_constraints'` exposes the
  system view
- `SHOW FULL TABLES FROM information_schema LIKE 'check_constraints'` returns
  `CHECK_CONSTRAINTS`, `SYSTEM VIEW`
- composable projections, `DISTINCT`/`ALL`, `WHERE`, `ORDER BY`, `LIMIT`,
  `COUNT(*)`, aliases, and qualified wildcard forms are covered by the shared
  system-view SELECT path
- enforced `CREATE TABLE` CHECK constraints reject invalid covered DML rows
  with error or warning 3819
- unsupported `ALTER TABLE ... ADD CHECK ...` DDL must not create
  `CHECK_CONSTRAINTS` rows

## Known Gaps

- `ALTER TABLE ... ADD/DROP/ALTER CHECK` remains unsupported and does not
  mutate CHECK metadata.
- Full MySQL CHECK expression semantics and DDL-time expression validation are
  tracked by the dedicated [CHECK constraints spec](../check-constraints/specs.md).
- Privilege filtering and exact MySQL field metadata remain deferred. General
  projection, filtering, ordering, limiting, alias, and aggregate behavior is
  covered by the composable information-schema SELECT path.
