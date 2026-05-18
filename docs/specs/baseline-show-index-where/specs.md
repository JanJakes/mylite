# Baseline SHOW INDEX WHERE

## Summary

This phase extends descriptor-driven `SHOW INDEX`, `SHOW INDEXES`, and
`SHOW KEYS` with a limited `WHERE` filter over the displayed result columns:

```sql
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} table_name [ {FROM | IN} schema_name ]
WHERE predicate
```

The statement remains MyLite-owned metadata. It reads durable table, column,
and index descriptors, builds the same result rows as the existing `SHOW INDEX`
surface, and evaluates the admitted predicate subset in MyLite runtime code.
It does not query SQLite schema text, SQLite PRAGMAs, or storage-engine
statistics, and it does not add any new index kinds.

## Compatibility Authority

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `COMPATIBILITY.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite `SHOW INDEX`, `SHOW VARIABLES WHERE`, and
  `SHOW TABLE STATUS WHERE` designs:
  - `docs/specs/baseline-show-index-empty-introspection/specs.md`
  - `docs/specs/baseline-show-variables-where/specs.md`
  - `docs/specs/baseline-show-table-status-where/specs.md`
- Official MySQL 8.4 documentation:
  - `SHOW INDEX`: <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
  - extensions to `SHOW` statements:
    <https://dev.mysql.com/doc/refman/8.4/en/extended-show.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_show_index_where_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against a local MySQL 8.4.9 runtime establish these expectations
for this slice:

- `SHOW INDEX`, `SHOW INDEXES`, and `SHOW KEYS` accept a trailing `WHERE`
  clause after the existing table and optional schema clauses.
- The `WHERE` predicate is evaluated against displayed output column names.
  Backtick-quoted output column names such as `` `Column_name` `` are accepted.
- Qualified output column references such as `indexes.Key_name` are rejected as
  unknown columns in the `WHERE` clause.
- Output column name resolution is ASCII case-insensitive for the observed
  subset.
- Text comparisons over current output cells are ASCII case-insensitive in the
  observed default metadata collation, including `Table`, `Key_name`,
  `Column_name`, `Index_type`, and `Visible`.
- SQL `NULL` output cells such as `Sub_part`, `Packed`, and `Expression`
  follow normal three-valued logic. `Expression <=> NULL` matches current
  nonfunctional key parts; `Expression IS NOT NULL` matches none.
- Numeric output columns such as `Non_unique`, `Seq_in_index`, and
  `Cardinality` accept numeric literals in MySQL. MyLite deliberately defers
  warning-producing and type-coercing `SHOW` predicates in this slice; quoted
  unsigned decimal string literals are admitted.
- MySQL accepts broader predicates such as `REGEXP`; MyLite defers them with a
  deterministic unsupported diagnostic for this slice.
- Successful supported filters leave `@@warning_count == 0`,
  `@@error_count == 0`, and make `ROW_COUNT()` return `-1`.
- Unknown output columns report `1054` / `42S22`
  (`Unknown column ... in 'where clause'`).

## Ownership Boundaries

- Public API: no ABI or public-header change. `mylite_execute()` returns the
  existing row-result handle for successful metadata statements.
- Statement context: successful `SHOW INDEX WHERE` is a result-producing
  statement. It reports affected rows `0`, warning count `0`, and updates the
  previous row count to `-1`.
- Lexer/parser/AST: grammar admission belongs to MyLite's parser. The existing
  predicate AST is reused, but runtime admits only the subset specified here.
- Runtime/analyzer: runtime resolves the target schema/table, rejects reserved
  names and unsupported object kinds, loads descriptor-owned indexes and key
  parts, builds candidate `SHOW INDEX` cells, evaluates the admitted predicate
  subset, and appends matching rows.
- Catalog: descriptors remain authoritative for schema, table, column, and
  index metadata. This feature reads descriptors but does not mutate catalog
  rows, descriptor versions, descriptor caches, catalog generation, or SQLite
  schema generation.
- SQLite physical storage: no generated SQLite SQL is added for this feature.
  The existing index descriptors may correspond to physical SQLite indexes, but
  `SHOW INDEX WHERE` does not inspect SQLite metadata.
- Storage/VFS/file format: no `.mylite` preamble or shifted payload behavior
  changes.

## Syntax

The independent MyLite subset is:

```ebnf
show_index_statement:
    SHOW show_index_keyword show_index_table_keyword table_name show_index_schema_opt
        show_index_filter_opt

show_index_keyword:
    INDEX
  | INDEXES
  | KEYS

show_index_table_keyword:
    FROM
  | IN

show_index_schema_opt:
    empty
  | FROM identifier
  | IN identifier

show_index_filter_opt:
    empty
  | WHERE show_index_predicate
```

`LIKE` remains unsupported for `SHOW INDEX`. `SHOW EXTENDED INDEX`, `ORDER BY`,
`LIMIT`, and filter clauses before the optional schema clause remain outside
this slice.

### MyLite Lemon-Syntax Snippet

```lemon
show_index_statement(A) ::=
    SHOW(S) show_index_keyword show_index_table_keyword table_name(T)
    show_index_filter_opt(F). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, NULL, F);
}
show_index_statement(A) ::=
    SHOW(S) show_index_keyword show_index_table_keyword table_name(T)
    show_index_schema_keyword identifier(D) show_index_filter_opt(F). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, D, F);
}

show_index_table_keyword ::= FROM.
show_index_table_keyword ::= IN.
show_index_schema_keyword ::= FROM.
show_index_schema_keyword ::= IN.

show_index_filter_opt(A) ::= . {
    A = NULL;
}
show_index_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}
```

These snippets describe MyLite's admitted subset and are not MySQL grammar
text.

## WHERE Predicate Subset

The filter is evaluated against the current `SHOW INDEX` output row. The
admitted column names are exactly the displayed column labels:

```text
Table
Non_unique
Key_name
Seq_in_index
Column_name
Collation
Cardinality
Sub_part
Packed
Null
Index_type
Comment
Index_comment
Visible
Expression
```

Column names are resolved ASCII case-insensitively. Backtick quoting does not
change the name. Qualified column references are reported as unknown columns in
the `WHERE` clause.

The admitted predicate subset is:

- `column = string_literal`
- `column <=> string_literal`
- `column <> string_literal`
- `column != string_literal`
- `column < string_literal`
- `column <= string_literal`
- `column > string_literal`
- `column >= string_literal`
- `column <=> NULL`
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

Comparisons for numeric output columns use unsigned decimal string conversion
for admitted string literals. The current numeric output columns are
`Non_unique`, `Seq_in_index`, `Cardinality`, and `Sub_part` when non-`NULL`.
Leading zeroes do not affect numeric equality or ordering. Non-decimal numeric
metadata strings remain outside this slice because MySQL accepts them through
broader expression coercion.

Text comparisons and `LIKE` matching use ASCII case-insensitive comparison for
current descriptor-generated `SHOW INDEX` cells. `LIKE` uses `%`, `_`, and
backslash escaping through the existing SHOW pattern matcher.

SQL `NULL` output cells follow normal three-valued logic. For example,
`Expression <=> NULL` matches current nonfunctional key parts, `Sub_part IS
NULL` matches full key parts, and `Sub_part IN (NULL, '3')` matches only rows
whose displayed `Sub_part` equals `3`.

The following are intentionally outside this slice and must fail
deterministically rather than being approximated:

- numeric, decimal, float, hex, bit, boolean, national-string, introducer, or
  parameter literals in `WHERE`;
- warning-producing string/numeric comparison coercions;
- column-to-column comparisons;
- functions such as `LOWER(Key_name)`;
- `BETWEEN`, `REGEXP`, `RLIKE`, `XOR`, `IS TRUE`, `IS FALSE`, and arbitrary
  expression predicates;
- subqueries and CTEs;
- `SHOW EXTENDED INDEX`, `LIKE`, `ORDER BY`, and `LIMIT`.

## Diagnostics

Supported runtime diagnostics:

- missing default schema: existing `1046` / `3D000`;
- unknown explicit schema: existing `1049` / `42000`;
- reserved `_mylite_*` schema and table names: existing catalog-name
  diagnostics;
- unknown table: existing `1146` / `42S02`;
- unsupported object kind: existing descriptor-resolution diagnostic;
- unknown output column: `1054` / `42S22` with
  `Unknown column '<name>' in 'where clause'`;
- qualified output column: the same unknown-column diagnostic using the
  displayed dotted reference;
- non-output expressions where a column is required: syntax error when the
  parser does not admit the expression shape, otherwise a deterministic
  unsupported MyLite diagnostic;
- non-string and non-`NULL` predicate literals: deterministic unsupported
  MyLite diagnostic;
- unsupported predicate operators and expression forms: deterministic
  unsupported MyLite diagnostics;
- allocation failures: existing out-of-memory diagnostic.

Successful supported statements return the existing 15-column result set,
affected rows `0`, warning count `0`, and no catalog or file-format mutation.

## Tests

The feature is covered by:

- MySQL 8.4.9 expectation script:
  `packages/libmylite/tests/mysql_baseline_show_index_where_expectations.sh`;
- parser tests extending `packages/libmylite/tests/parser_test.c`;
- runtime C tests extending
  `packages/libmylite/tests/runtime_show_index_empty_introspection_test.c`.

Runtime coverage must include schema-qualified and unqualified targets, primary
and secondary key rows, prefix key rows, fulltext rows, equality, `LIKE`,
null-safe equality, `IN`/`NOT IN`, `IS NULL`/`IS NOT NULL`, numeric string
comparisons, `AND`/`OR`/`NOT`, no-match filters, unknown columns, qualified
columns, unsupported numeric predicates, unsupported `REGEXP`, result metadata,
warning count, `ROW_COUNT()` behavior, catalog-generation preservation,
persistence after reopen, independent handles, and preservation of the existing
`.mylite` preamble invariants.
