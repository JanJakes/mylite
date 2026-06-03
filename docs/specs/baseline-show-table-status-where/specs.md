# Baseline SHOW TABLE STATUS WHERE

## Summary

This phase extends the existing descriptor-driven `SHOW TABLE STATUS` statement
with a limited `WHERE` filter over the statement's displayed output columns:

```sql
SHOW TABLE STATUS [ {FROM | IN} schema_name ] WHERE predicate
```

The statement remains a MyLite-owned metadata query. It does not inspect
SQLite schema text, does not add full InnoDB statistics, and does not implement
views, temporary-table rows, privileges, arbitrary expression evaluation, or a
general `INFORMATION_SCHEMA.TABLES` replacement.

## Compatibility Authority

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `COMPATIBILITY.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite `SHOW TABLE STATUS` and `SHOW VARIABLES WHERE` designs:
  - `docs/specs/baseline-show-table-status-introspection/specs.md`
  - `docs/specs/baseline-show-table-status-metadata/specs.md`
  - `docs/specs/baseline-show-variables-where/specs.md`
- Official MySQL 8.4 documentation:
  - `SHOW TABLE STATUS`:
    <https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html>
  - extensions to `SHOW` statements:
    <https://dev.mysql.com/doc/refman/8.4/en/extended-show.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_show_table_status_where_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against a local `mysql:8.4.9` container establish these
expectations for this slice:

- `SHOW TABLE STATUS` accepts either a `LIKE` filter or a `WHERE` filter after
  the optional schema clause. `LIKE ... WHERE` and `WHERE ... ORDER BY` are
  syntax errors.
- The `WHERE` predicate is evaluated against displayed output column names,
  including backtick-quoted names such as `` `Name` `` and `` `Rows` ``.
- Unquoted `Rows` is a syntax error in MySQL 8.4.9 because it conflicts with a
  keyword in this position; backtick quoting makes it an output-column
  reference.
- Output column names resolve ASCII case-insensitively for this observed
  subset. Qualified references such as `tables.Name` are rejected as unknown
  `WHERE` columns.
- Table-name value comparisons are case-sensitive on the observed Linux
  MySQL 8.4.9 runtime with `lower_case_table_names = 0`; `Name = 'NUMBERS'`
  does not match a table named `numbers`.
- Non-name status text comparisons such as `Engine = 'INNODB'` and `Collation
  = 'UTF8MB4_0900_AI_CI'` match case-insensitively in the observed default
  metadata collation.
- `Auto_increment IS NULL`, `Auto_increment IS NOT NULL`, and
  `Auto_increment <=> NULL` follow normal SQL `NULL` truth behavior over the
  displayed cells.
- Numeric comparisons such as `` `Rows` = 3 `` are accepted by MySQL. MyLite
  admits unsigned integer literals for numeric status columns and continues to
  defer warning-producing and type-coercing `SHOW` predicates over nonnumeric
  columns. Quoted numeric text such as `` `Rows` = '3' `` remains admitted.
- `SUBSTRING()` / `SUBSTR()` / `MID()` expressions over displayed output
  columns are admitted as left operands for the same limited predicate shapes.
  For example, `SUBSTR(Name, 11, 1) = '1'` matches a table named
  `_tmp_table1`. `NULL` position or length arguments produce SQL `NULL`.
- Successful supported `WHERE` filters leave `@@warning_count == 0`,
  `@@error_count == 0`, and make `ROW_COUNT()` return `-1`.
- Unknown output columns report `1054` / `42S22` (`Unknown column ... in 'where
  clause'`).

## Ownership Boundaries

- Public API: no ABI or public-header change. `mylite_execute()` returns the
  existing row-result handle for successful metadata statements.
- Statement context: successful `SHOW TABLE STATUS WHERE` is a result-producing
  statement. It reports affected rows `0`, warning count `0`, and updates the
  previous row count to `-1`.
- Lexer/parser/AST: grammar admission belongs to MyLite's parser. The existing
  predicate AST is reused, but runtime admits only the subset specified here.
- Runtime/analyzer: runtime resolves the schema, rejects reserved schema names,
  iterates descriptor-owned persistent base tables, builds candidate status
  cells, evaluates the admitted predicate subset, and appends matching rows.
- Catalog: descriptors remain authoritative for schema and table names,
  default collation, timestamps, auto-increment metadata, and index metadata.
  This feature reads descriptors but does not mutate catalog rows, descriptor
  versions, descriptor caches, catalog generation, or SQLite schema generation.
- SQLite physical storage: SQLite is used only through existing descriptor-built
  row-count reads against stable physical table names. This feature emits no
  user-derived SQLite SQL and requires no SQLite fork patch.
- Storage/VFS/file format: no `.mylite` preamble or shifted payload behavior
  changes.

## Syntax

The independent MyLite subset is:

```ebnf
show_table_status_statement:
    SHOW TABLE STATUS show_table_status_schema_opt show_table_status_filter_opt

show_table_status_schema_opt:
    empty
  | FROM identifier
  | IN identifier

show_table_status_filter_opt:
    empty
  | LIKE string_literal
  | WHERE show_table_status_predicate
```

`LIKE` and `WHERE` are mutually exclusive. The grammar intentionally excludes
`FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`, `LIKE ... WHERE`, and filter clauses
before the schema clause.

### MyLite Lemon-Syntax Snippet

```lemon
show_table_status_statement(A) ::=
    SHOW(S) TABLE STATUS(T) show_table_status_filter_opt(F). {
    A = mylite_sql_parser_make_show_table_status_statement(state, S, T, NULL, F);
}
show_table_status_statement(A) ::=
    SHOW(S) TABLE STATUS(T) FROM identifier(D) show_table_status_filter_opt(F). {
    A = mylite_sql_parser_make_show_table_status_statement(state, S, T, D, F);
}
show_table_status_statement(A) ::=
    SHOW(S) TABLE STATUS(T) IN identifier(D) show_table_status_filter_opt(F). {
    A = mylite_sql_parser_make_show_table_status_statement(state, S, T, D, F);
}

show_table_status_filter_opt(A) ::= . {
    A = NULL;
}
show_table_status_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}
show_table_status_filter_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}
```

These snippets describe MyLite's admitted subset and are not MySQL grammar
text.

## WHERE Predicate Subset

The filter is evaluated against the current `SHOW TABLE STATUS` output row.
The admitted column names are exactly the displayed column labels:

```text
Name
Engine
Version
Row_format
Rows
Avg_row_length
Data_length
Max_data_length
Index_length
Data_free
Auto_increment
Create_time
Update_time
Check_time
Collation
Checksum
Create_options
Comment
```

Column names are resolved ASCII case-insensitively. Backtick quoting does not
change the name. Qualified column references are reported as unknown columns in
the `WHERE` clause.

The admitted predicate subset is:

- `operand = string_literal_or_unsigned_integer_literal`
- `operand <=> string_literal_or_unsigned_integer_literal`
- `operand <> string_literal_or_unsigned_integer_literal`
- `operand != string_literal_or_unsigned_integer_literal`
- `operand < string_literal_or_unsigned_integer_literal`
- `operand <= string_literal_or_unsigned_integer_literal`
- `operand > string_literal_or_unsigned_integer_literal`
- `operand >= string_literal_or_unsigned_integer_literal`
- `operand <=> NULL`
- `operand LIKE string_literal`
- `operand NOT LIKE string_literal`
- `operand REGEXP string_literal`
- `operand NOT REGEXP string_literal`
- `operand RLIKE string_literal`
- `operand NOT RLIKE string_literal`
- `operand IN (string_literal_or_unsigned_integer_literal_or_NULL [, ...])`
- `operand NOT IN (string_literal_or_unsigned_integer_literal_or_NULL [, ...])`
- `operand IS NULL`
- `operand IS NOT NULL`
- parenthesized predicates
- `NOT predicate`
- `predicate AND predicate`
- `predicate OR predicate`

`operand` is either a displayed output column or one `SUBSTRING()`,
`SUBSTR()`, or `MID()` call whose first argument is a displayed output column.
Substring position and length arguments follow the existing baseline substring
function subset for integer, boolean, unary-signed integer, and `NULL`
literals. Substring operands are string-valued and compare case-sensitively
when their source column uses the `Name` table-name policy; otherwise they use
the same ASCII case-insensitive comparison policy as the source status text.
Unsigned integer predicate literals remain admitted only for direct numeric
status-column operands, not substring operands.

Comparisons for numeric status columns use unsigned decimal conversion for the
admitted string and integer literals, so leading zeroes do not affect equality
or ordering. The current numeric status columns are `Version`, `Rows`,
`Avg_row_length`, `Data_length`, `Max_data_length`, `Index_length`,
`Data_free`, `Auto_increment`, and `Checksum`. Unsigned integer literals are
supported only for these numeric status columns. Numeric comparisons against
nonnumeric columns, signed literals, decimal/float literals, and other MySQL
warning-producing coercions remain outside this slice. String comparisons for
`Name` use the existing case-sensitive catalog table name policy. String
comparisons for the other current descriptor-generated status text use ASCII
case-insensitive comparison.
`LIKE` uses the same case-sensitivity rule as equality for the referenced
column: `%` matches any byte sequence, `_` matches one byte, and backslash
escapes the next byte.

SQL `NULL` status cells follow normal three-valued logic. For example, `Name
<=> NULL` matches no current base-table rows, `Auto_increment <=> NULL` matches
non-auto-increment tables, `Auto_increment IS NOT NULL` matches current
auto-increment tables, and `Auto_increment IN (NULL, '3')` matches the row with
`Auto_increment = '3'`.

The following are intentionally outside this slice and must fail
deterministically rather than being approximated:

- signed integer, decimal, float, hex, bit, boolean, national-string,
  introducer, or parameter literals in `WHERE`;
- unsigned integer literals for nonnumeric output columns;
- warning-producing string/numeric comparison coercions;
- column-to-column comparisons;
- functions other than the admitted one-level `SUBSTRING()` / `SUBSTR()` /
  `MID()` operands, which may fail during parsing before runtime predicate
  validation;
- nested substring expressions, substring calls on literals or non-output
  expressions, nonliteral substring position/length arguments, and substring
  calls on the right side of a predicate;
- `BETWEEN`, `XOR`, `IS TRUE`, `IS FALSE`, and arbitrary expression
  predicates;
- subqueries and CTEs;
- `ORDER BY`, `LIMIT`, and `LIKE ... WHERE` combinations.

## Diagnostics

Supported runtime diagnostics:

- missing default schema: existing `1046` / `3D000`;
- unknown explicit schema: existing `1049` / `42000`;
- reserved `_mylite_*` schema names: existing incorrect-database-name
  diagnostic;
- unknown output column: `1054` / `42S22` with `Unknown column '<name>' in
  'where clause'`;
- qualified output column: the same unknown-column diagnostic using the
  displayed dotted reference;
- non-output expressions where an admitted operand is required: syntax error
  when the parser does not admit the expression shape, otherwise a
  deterministic unsupported MyLite diagnostic;
- non-string and non-`NULL` predicate literals: deterministic unsupported
  MyLite diagnostic;
- unsupported predicate operators and expression forms: deterministic
  unsupported MyLite diagnostics;
- allocation failures: existing out-of-memory diagnostic;
- internal row-count or descriptor read failures: existing runtime diagnostic.

Successful supported statements return an 18-column result set, affected rows
`0`, warning count `0`, and no catalog or file-format mutation.

## Tests

The feature is covered by:

- MySQL 8.4.9 expectation script:
  `packages/libmylite/tests/mysql_baseline_show_table_status_where_expectations.sh`;
- parser tests for `WHERE` admission and rejected clause combinations;
- runtime C tests extending
  `packages/libmylite/tests/runtime_show_table_status_introspection_test.c`.

Runtime coverage must include default-schema and explicit-schema filters,
case-insensitive output-column references, backticked column references,
`LIKE`, baseline `REGEXP`/`RLIKE`, equality, null-safe equality, `IN`/`NOT IN`,
`IS NULL`/`IS NOT NULL`, substring operands over output columns, `AND`/`OR`/`NOT`,
no-match filters, auto-increment `NULL` behavior, unknown columns, qualified
columns, unsupported numeric predicates, unsupported expression forms, no
result-set mutation, row-count/warning behavior, persistence after reopen,
independent handles, and preservation of the existing `.mylite` preamble
invariants.
